#include "tlalpowa_openmap.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_set>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>
#endif

namespace tlalpowa::openmap {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr std::uintmax_t kMaximumResponseBytes = 8u * 1024u * 1024u;
constexpr std::uintmax_t kMaximumCacheBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaximumCacheFiles = 24u;

struct QuantizedRequest {
    std::string key;
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    int zoom = 0;
    fs::path cache_dir;
};

struct SharedState {
    std::mutex mutex;
    std::shared_ptr<const Snapshot> snapshot = std::make_shared<const Snapshot>();
    std::string requested_key;
    std::chrono::steady_clock::time_point last_launch{};
    std::atomic_bool loading{false};
};

SharedState& shared_state() {
    static SharedState* state = new SharedState();
    return *state;
}

std::string trim(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) {
        return !is_space(static_cast<unsigned char>(c));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) {
        return !is_space(static_cast<unsigned char>(c));
    }).base(), value.end());
    return value;
}

std::string percent_encode(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 2u);
    for (unsigned char c : value) {
        const bool unreserved =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0x0f]);
            out.push_back(hex[c & 0x0f]);
        }
    }
    return out;
}

std::string element_name(const json& tags) {
    for (const char* key : {"name:es", "name", "official_name", "short_name"}) {
        const auto found = tags.find(key);
        if (found != tags.end() && found->is_string()) {
            const std::string value = trim(found->get<std::string>());
            if (!value.empty()) return value;
        }
    }
    return {};
}

std::string tag_string(const json& tags, const char* key) {
    const auto found = tags.find(key);
    return found != tags.end() && found->is_string() ? trim(found->get<std::string>()) : std::string{};
}

int parse_integer_tag(std::string value) {
    value = trim(std::move(value));
    std::string digits;
    digits.reserve(value.size());
    for (char c : value) {
        if (c >= '0' && c <= '9') digits.push_back(c);
    }
    if (digits.empty()) return 0;
    try {
        const long long parsed = std::stoll(digits);
        return static_cast<int>(std::clamp<long long>(parsed, 0, std::numeric_limits<int>::max()));
    } catch (...) {
        return 0;
    }
}

int parse_elevation_tag(std::string value) {
    value = trim(std::move(value));
    if (value.empty()) return 0;
    for (char& c : value) if (c == ',') c = '.';
    try {
        const double parsed = std::stod(value);
        if (!std::isfinite(parsed)) return 0;
        return static_cast<int>(std::lround(std::clamp(parsed, -500.0, 9000.0)));
    } catch (...) {
        return 0;
    }
}

int place_rank(const std::string& kind, int population, const json& tags) {
    if (kind == "city" || tag_string(tags, "capital") == "yes" || population >= 500000) return 1;
    if (kind == "town" || population >= 50000) return 2;
    if (kind == "village" || kind == "suburb" || population >= 5000) return 3;
    return 4;
}

bool points_near(const std::pair<double, double>& a, const std::pair<double, double>& b) {
    return std::fabs(a.first - b.first) <= 1.0e-6 && std::fabs(a.second - b.second) <= 1.0e-6;
}

std::vector<std::vector<std::pair<double, double>>> relation_outer_rings(const json& element) {
    std::vector<std::vector<std::pair<double, double>>> segments;
    const auto members = element.find("members");
    if (members == element.end() || !members->is_array()) return segments;
    for (const json& member : *members) {
        if (!member.is_object()) continue;
        const std::string role = member.value("role", std::string{});
        if (role == "inner" || (role != "outer" && !role.empty())) continue;
        const auto geometry = member.find("geometry");
        if (geometry == member.end() || !geometry->is_array()) continue;
        std::vector<std::pair<double, double>> segment;
        segment.reserve(geometry->size());
        for (const json& point : *geometry) {
            if (!point.is_object()) continue;
            const auto lon = point.find("lon");
            const auto lat = point.find("lat");
            if (lon == point.end() || lat == point.end() || !lon->is_number() || !lat->is_number()) continue;
            const double x = lon->get<double>();
            const double y = lat->get<double>();
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            if (segment.empty() || !points_near(segment.back(), {x, y})) segment.emplace_back(x, y);
        }
        if (segment.size() >= 2u) segments.push_back(std::move(segment));
    }

    std::vector<std::vector<std::pair<double, double>>> rings;
    while (!segments.empty()) {
        std::vector<std::pair<double, double>> ring = std::move(segments.back());
        segments.pop_back();
        bool progressed = true;
        while (!segments.empty() && progressed && !points_near(ring.front(), ring.back())) {
            progressed = false;
            for (std::size_t i = 0; i < segments.size(); ++i) {
                auto& candidate = segments[i];
                if (points_near(ring.back(), candidate.front())) {
                    ring.insert(ring.end(), candidate.begin() + 1, candidate.end());
                } else if (points_near(ring.back(), candidate.back())) {
                    ring.insert(ring.end(), std::next(candidate.rbegin()), candidate.rend());
                } else if (points_near(ring.front(), candidate.back())) {
                    ring.insert(ring.begin(), candidate.begin(), candidate.end() - 1);
                } else if (points_near(ring.front(), candidate.front())) {
                    std::reverse(candidate.begin(), candidate.end());
                    ring.insert(ring.begin(), candidate.begin(), candidate.end() - 1);
                } else {
                    continue;
                }
                segments.erase(segments.begin() + static_cast<std::ptrdiff_t>(i));
                progressed = true;
                break;
            }
        }
        if (ring.size() >= 4u && points_near(ring.front(), ring.back())) ring.pop_back();
        if (ring.size() >= 3u) rings.push_back(std::move(ring));
    }
    return rings;
}

void simplify_ring(std::vector<std::pair<double, double>>& ring, double tolerance) {
    if (ring.size() < 8u || tolerance <= 0.0) return;
    const double tolerance_sq = tolerance * tolerance;
    std::vector<std::pair<double, double>> simplified;
    simplified.reserve(ring.size());
    simplified.push_back(ring.front());
    for (std::size_t i = 1; i + 1 < ring.size(); ++i) {
        const auto& previous = simplified.back();
        const auto& current = ring[i];
        const double dx = current.first - previous.first;
        const double dy = current.second - previous.second;
        if (dx * dx + dy * dy >= tolerance_sq) simplified.push_back(current);
    }
    if (!points_near(simplified.back(), ring.back())) simplified.push_back(ring.back());
    if (simplified.size() >= 3u) ring.swap(simplified);
}

bool point_in_ring(double lon, double lat, const std::vector<std::pair<double, double>>& ring) {
    if (ring.size() < 3u) return false;
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1u; i < ring.size(); j = i++) {
        const auto& a = ring[i];
        const auto& b = ring[j];
        const bool crosses = ((a.second > lat) != (b.second > lat)) &&
            (lon < (b.first - a.first) * (lat - a.second) /
                    ((b.second - a.second) == 0.0 ? 1.0e-20 : (b.second - a.second)) + a.first);
        if (crosses) inside = !inside;
    }
    return inside;
}

void assign_boundary_parents(std::vector<BoundaryRecord>& boundaries) {
    for (BoundaryRecord& municipality : boundaries) {
        if (municipality.admin_level <= 4 || !municipality.admin1_id.empty()) continue;
        for (const BoundaryRecord& state : boundaries) {
            if (state.admin_level > 4 || state.admin1_id.empty()) continue;
            bool contains = false;
            for (const auto& ring : state.rings) {
                if (point_in_ring(municipality.center_lon, municipality.center_lat, ring)) {
                    contains = true;
                    break;
                }
            }
            if (contains) {
                municipality.admin1_id = state.admin1_id;
                break;
            }
        }
    }
}

std::string boundary_ref(const json& tags) {
    for (const char* key : {"ref:INEGI", "inegi", "INEGI", "ref"}) {
        const std::string value = tag_string(tags, key);
        if (!value.empty()) return value;
    }
    return {};
}

bool element_point(const json& element, double& lon, double& lat) {
    const auto lon_it = element.find("lon");
    const auto lat_it = element.find("lat");
    if (lon_it != element.end() && lat_it != element.end() && lon_it->is_number() && lat_it->is_number()) {
        lon = lon_it->get<double>();
        lat = lat_it->get<double>();
    } else {
        const auto center = element.find("center");
        if (center != element.end() && center->is_object()) {
            const auto center_lon = center->find("lon");
            const auto center_lat = center->find("lat");
            if (center_lon == center->end() || center_lat == center->end() ||
                !center_lon->is_number() || !center_lat->is_number()) return false;
            lon = center_lon->get<double>();
            lat = center_lat->get<double>();
        } else {
            const auto bounds = element.find("bounds");
            if (bounds == element.end() || !bounds->is_object()) return false;
            const auto min_lon = bounds->find("minlon");
            const auto max_lon = bounds->find("maxlon");
            const auto min_lat = bounds->find("minlat");
            const auto max_lat = bounds->find("maxlat");
            if (min_lon == bounds->end() || max_lon == bounds->end() ||
                min_lat == bounds->end() || max_lat == bounds->end() ||
                !min_lon->is_number() || !max_lon->is_number() ||
                !min_lat->is_number() || !max_lat->is_number()) return false;
            lon = (min_lon->get<double>() + max_lon->get<double>()) * 0.5;
            lat = (min_lat->get<double>() + max_lat->get<double>()) * 0.5;
        }
    }
    return std::isfinite(lon) && std::isfinite(lat) &&
           lon >= -180.0 && lon <= 180.0 && lat >= -90.0 && lat <= 90.0;
}

std::string osm_element_id(const json& element) {
    const std::string type = element.value("type", std::string{"node"});
    const std::int64_t id = element.value("id", std::int64_t{0});
    return type + "/" + std::to_string(id);
}

std::string osm_element_url(const std::string& id) {
    return "https://www.openstreetmap.org/" + id;
}

std::shared_ptr<Snapshot> parse_overpass(const std::string& text, const std::string& key) {
    const json root = json::parse(text, nullptr, false);
    if (!root.is_object()) return {};
    const auto elements = root.find("elements");
    if (elements == root.end() || !elements->is_array()) return {};

    auto snapshot = std::make_shared<Snapshot>();
    snapshot->key = key;
    std::unordered_set<std::string> place_ids;
    std::unordered_set<std::string> water_ids;
    std::unordered_set<std::string> boundary_ids;

    for (const json& element : *elements) {
        if (!element.is_object()) continue;
        const auto tags_it = element.find("tags");
        if (tags_it == element.end() || !tags_it->is_object()) continue;
        const json& tags = *tags_it;
        const std::string name = element_name(tags);
        if (name.empty()) continue;

        double lon = 0.0;
        double lat = 0.0;
        if (!element_point(element, lon, lat)) continue;

        const std::string id = osm_element_id(element);
        const std::string boundary = tag_string(tags, "boundary");
        const int admin_level = parse_integer_tag(tag_string(tags, "admin_level"));
        if (boundary == "administrative" && admin_level > 0) {
            if (!boundary_ids.insert(id).second) continue;
            auto rings = relation_outer_rings(element);
            if (rings.empty()) continue;
            BoundaryRecord area;
            area.id = "osm:" + id;
            area.name = name;
            area.admin_level = admin_level;
            area.center_lon = lon;
            area.center_lat = lat;
            for (auto& ring : rings) simplify_ring(ring, admin_level <= 4 ? 0.0008 : 0.00018);
            area.rings = std::move(rings);
            area.source_url = osm_element_url(id);
            const std::string ref = boundary_ref(tags);
            if (admin_level <= 4) {
                const std::string iso = tag_string(tags, "ISO3166-2");
                area.admin1_id = !iso.empty() ? iso : ref;
            } else {
                area.admin2_id = ref;
                if (ref.size() >= 2u && std::isdigit(static_cast<unsigned char>(ref[0])) &&
                    std::isdigit(static_cast<unsigned char>(ref[1]))) {
                    area.admin1_id = ref.substr(0, 2);
                } else {
                    area.admin1_id = tag_string(tags, "is_in:state");
                }
            }
            snapshot->boundaries.push_back(std::move(area));
            continue;
        }

        const std::string place_kind = tag_string(tags, "place");
        if (!place_kind.empty()) {
            if (!place_ids.insert(id).second) continue;
            PlaceRecord place;
            place.id = "osm:" + id;
            place.name = name;
            place.kind = place_kind;
            place.lon = lon;
            place.lat = lat;
            place.population = parse_integer_tag(tag_string(tags, "population"));
            place.elevation_m = parse_elevation_tag(tag_string(tags, "ele"));
            place.rank = place_rank(place_kind, place.population, tags);
            place.admin1 = tag_string(tags, "is_in:state");
            place.admin2 = tag_string(tags, "is_in:municipality");
            place.source_url = osm_element_url(id);
            snapshot->places.push_back(std::move(place));
            continue;
        }

        const std::string natural = tag_string(tags, "natural");
        const std::string water = tag_string(tags, "water");
        const std::string waterway = tag_string(tags, "waterway");
        if (natural == "water" || !water.empty() || !waterway.empty()) {
            if (!water_ids.insert(id).second) continue;
            WaterRecord body;
            body.id = "osm:" + id;
            body.name = name;
            body.kind = !water.empty() ? water : (!waterway.empty() ? waterway : natural);
            body.lon = lon;
            body.lat = lat;
            body.source_url = osm_element_url(id);
            snapshot->waters.push_back(std::move(body));
        }
    }

    std::stable_sort(snapshot->places.begin(), snapshot->places.end(), [](const PlaceRecord& a, const PlaceRecord& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.population != b.population) return a.population > b.population;
        return a.name < b.name;
    });
    std::stable_sort(snapshot->waters.begin(), snapshot->waters.end(), [](const WaterRecord& a, const WaterRecord& b) {
        return a.name < b.name;
    });
    std::stable_sort(snapshot->boundaries.begin(), snapshot->boundaries.end(), [](const BoundaryRecord& a, const BoundaryRecord& b) {
        if (a.admin_level != b.admin_level) return a.admin_level < b.admin_level;
        return a.name < b.name;
    });
    assign_boundary_parents(snapshot->boundaries);
    if (snapshot->places.size() > 1200u) snapshot->places.resize(1200u);
    if (snapshot->waters.size() > 400u) snapshot->waters.resize(400u);
    if (snapshot->boundaries.size() > 600u) snapshot->boundaries.resize(600u);
    return snapshot;
}

std::vector<std::int64_t> boundary_relation_ids(const std::string& text, double center_lon, double center_lat) {
    const json root = json::parse(text, nullptr, false);
    if (!root.is_object()) return {};
    const auto elements = root.find("elements");
    if (elements == root.end() || !elements->is_array()) return {};
    struct Candidate {
        std::int64_t id = 0;
        double distance = 0.0;
        int admin_level = 0;
    };
    std::vector<Candidate> candidates;
    for (const json& element : *elements) {
        if (!element.is_object() || element.value("type", std::string{}) != "relation") continue;
        const std::int64_t id = element.value("id", std::int64_t{0});
        if (id <= 0) continue;
        double lon = center_lon;
        double lat = center_lat;
        (void)element_point(element, lon, lat);
        int admin_level = 0;
        const auto tags = element.find("tags");
        if (tags != element.end() && tags->is_object()) {
            admin_level = parse_integer_tag(tag_string(*tags, "admin_level"));
        }
        const double dx = (lon - center_lon) * std::cos(center_lat * 3.14159265358979323846 / 180.0);
        const double dy = lat - center_lat;
        candidates.push_back({id, dx * dx + dy * dy, admin_level});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distance < b.distance;
    });
    std::vector<std::int64_t> ids;
    ids.reserve(std::min<std::size_t>(48u, candidates.size()));
    for (const Candidate& candidate : candidates) {
        if (candidate.admin_level <= 4 && ids.size() < 8u) ids.push_back(candidate.id);
    }
    for (const Candidate& candidate : candidates) {
        if (candidate.admin_level > 4 && ids.size() < 48u) ids.push_back(candidate.id);
    }
    return ids;
}

std::string boundary_geometry_query(const std::vector<std::int64_t>& ids) {
    std::ostringstream query;
    query << "[out:json][timeout:25][maxsize:25165824];(";
    for (const std::int64_t id : ids) query << "rel(" << id << ");";
    query << ");out body center geom qt;";
    return query.str();
}

json compact_json(const Snapshot& snapshot) {
    json root;
    root["schema"] = "tlalpowa.openmap.v5";
    root["key"] = snapshot.key;
    root["places"] = json::array();
    root["waters"] = json::array();
    root["boundaries"] = json::array();
    for (const PlaceRecord& place : snapshot.places) {
        root["places"].push_back({
            {"id", place.id}, {"name", place.name}, {"kind", place.kind},
            {"lon", place.lon}, {"lat", place.lat}, {"population", place.population},
            {"elevation_m", place.elevation_m}, {"rank", place.rank},
            {"admin1", place.admin1}, {"admin2", place.admin2}, {"source_url", place.source_url}
        });
    }
    for (const WaterRecord& water : snapshot.waters) {
        root["waters"].push_back({
            {"id", water.id}, {"name", water.name}, {"kind", water.kind},
            {"lon", water.lon}, {"lat", water.lat}, {"source_url", water.source_url}
        });
    }
    for (const BoundaryRecord& boundary : snapshot.boundaries) {
        json rings = json::array();
        for (const auto& ring : boundary.rings) {
            json points = json::array();
            for (const auto& point : ring) points.push_back({point.first, point.second});
            rings.push_back(std::move(points));
        }
        root["boundaries"].push_back({
            {"id", boundary.id}, {"name", boundary.name}, {"admin_level", boundary.admin_level},
            {"admin1_id", boundary.admin1_id}, {"admin2_id", boundary.admin2_id},
            {"center_lon", boundary.center_lon}, {"center_lat", boundary.center_lat},
            {"source_url", boundary.source_url}, {"rings", std::move(rings)}
        });
    }
    return root;
}

std::shared_ptr<Snapshot> parse_compact(const std::string& text, const std::string& expected_key) {
    const json root = json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", std::string{}) != "tlalpowa.openmap.v5") return {};
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->key = root.value("key", expected_key);
    snapshot->from_cache = true;
    const auto places = root.find("places");
    if (places != root.end() && places->is_array()) {
        for (const json& item : *places) {
            if (!item.is_object()) continue;
            PlaceRecord place;
            place.id = item.value("id", std::string{});
            place.name = item.value("name", std::string{});
            place.kind = item.value("kind", std::string{});
            place.lon = item.value("lon", 0.0);
            place.lat = item.value("lat", 0.0);
            place.population = item.value("population", 0);
            place.elevation_m = item.value("elevation_m", 0);
            place.rank = std::clamp(item.value("rank", 4), 1, 4);
            place.admin1 = item.value("admin1", std::string{});
            place.admin2 = item.value("admin2", std::string{});
            place.source_url = item.value("source_url", std::string{});
            if (!place.name.empty() && std::isfinite(place.lon) && std::isfinite(place.lat)) {
                snapshot->places.push_back(std::move(place));
            }
        }
    }
    const auto waters = root.find("waters");
    if (waters != root.end() && waters->is_array()) {
        for (const json& item : *waters) {
            if (!item.is_object()) continue;
            WaterRecord water;
            water.id = item.value("id", std::string{});
            water.name = item.value("name", std::string{});
            water.kind = item.value("kind", std::string{});
            water.lon = item.value("lon", 0.0);
            water.lat = item.value("lat", 0.0);
            water.source_url = item.value("source_url", std::string{});
            if (!water.name.empty() && std::isfinite(water.lon) && std::isfinite(water.lat)) {
                snapshot->waters.push_back(std::move(water));
            }
        }
    }
    const auto boundaries = root.find("boundaries");
    if (boundaries != root.end() && boundaries->is_array()) {
        for (const json& item : *boundaries) {
            if (!item.is_object()) continue;
            BoundaryRecord boundary;
            boundary.id = item.value("id", std::string{});
            boundary.name = item.value("name", std::string{});
            boundary.admin_level = item.value("admin_level", 0);
            boundary.admin1_id = item.value("admin1_id", std::string{});
            boundary.admin2_id = item.value("admin2_id", std::string{});
            boundary.center_lon = item.value("center_lon", 0.0);
            boundary.center_lat = item.value("center_lat", 0.0);
            boundary.source_url = item.value("source_url", std::string{});
            const auto rings = item.find("rings");
            if (rings != item.end() && rings->is_array()) {
                for (const json& ring_json : *rings) {
                    if (!ring_json.is_array()) continue;
                    std::vector<std::pair<double, double>> ring;
                    ring.reserve(ring_json.size());
                    for (const json& point : ring_json) {
                        if (!point.is_array() || point.size() < 2u || !point[0].is_number() || !point[1].is_number()) continue;
                        ring.emplace_back(point[0].get<double>(), point[1].get<double>());
                    }
                    if (ring.size() >= 3u) boundary.rings.push_back(std::move(ring));
                }
            }
            if (!boundary.name.empty() && !boundary.rings.empty()) snapshot->boundaries.push_back(std::move(boundary));
        }
    }
    return snapshot;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool write_compact_cache(const fs::path& path, const Snapshot& snapshot) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << compact_json(snapshot).dump();
        if (!out) return false;
    }
    fs::rename(temporary, path, ec);
    if (!ec) return true;
    ec.clear();
    fs::remove(path, ec);
    ec.clear();
    fs::rename(temporary, path, ec);
    if (ec) fs::remove(temporary, ec);
    return !ec;
}

bool cache_is_fresh(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec || !fs::is_regular_file(path, ec)) return false;
    const auto age = fs::file_time_type::clock::now() - fs::last_write_time(path, ec);
    return !ec && age <= std::chrono::hours(24 * 14);
}

void prune_cache(const fs::path& root) {
    std::error_code ec;
    if (!fs::exists(root, ec) || ec) return;
    struct Entry {
        fs::path path;
        std::uintmax_t size = 0;
        fs::file_time_type time{};
    };
    std::vector<Entry> entries;
    std::uintmax_t total = 0;
    for (const auto& item : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec) || item.path().extension() != ".json") continue;
        const std::uintmax_t size = item.file_size(ec);
        const auto time = item.last_write_time(ec);
        if (ec) { ec.clear(); continue; }
        entries.push_back({item.path(), size, time});
        total += size;
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.time < b.time; });
    std::size_t remaining = entries.size();
    for (const Entry& entry : entries) {
        if (remaining <= kMaximumCacheFiles && total <= kMaximumCacheBytes) break;
        fs::remove(entry.path, ec);
        ec.clear();
        total = total >= entry.size ? total - entry.size : 0;
        if (remaining > 0) --remaining;
    }
}

#ifdef _WIN32
std::wstring widen_utf8(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), count);
    return out;
}

bool download_text(const std::string& url, std::string& out) {
    HINTERNET internet = InternetOpenW(
        L"Tlalpowa/2026 lightweight-openstreetmap-overpass",
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) return false;

    const DWORD connect_timeout = 4500;
    const DWORD receive_timeout = 15000;
    InternetSetOptionW(internet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&connect_timeout, sizeof(connect_timeout));
    InternetSetOptionW(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&receive_timeout, sizeof(receive_timeout));
    const std::wstring wide_url = widen_utf8(url);
    const wchar_t* headers = L"Accept: application/json\r\n";
    HINTERNET file = InternetOpenUrlW(
        internet, wide_url.c_str(), headers, static_cast<DWORD>(-1),
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI |
        INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_SECURE, 0);
    if (!file) {
        InternetCloseHandle(internet);
        return false;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    HttpQueryInfoW(file, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &status_size, nullptr);
    bool ok = status >= 200 && status < 300;
    std::string buffer;
    if (ok) {
        char chunk[16384];
        for (;;) {
            DWORD read = 0;
            if (!InternetReadFile(file, chunk, sizeof(chunk), &read)) { ok = false; break; }
            if (read == 0) break;
            if (buffer.size() + read > kMaximumResponseBytes) { ok = false; break; }
            buffer.append(chunk, chunk + read);
        }
    }
    InternetCloseHandle(file);
    InternetCloseHandle(internet);
    if (!ok || buffer.size() < 32u) return false;
    out = std::move(buffer);
    return true;
}
#else
bool download_text(const std::string&, std::string&) {
    return false;
}
#endif

QuantizedRequest quantize(const Request& request) {
    QuantizedRequest out;
    out.zoom = std::clamp(request.zoom, 0, 19);
    out.cache_dir = request.cache_dir;
    const double center_lon = std::clamp((request.min_lon + request.max_lon) * 0.5, -179.5, 179.5);
    const double center_lat = std::clamp((request.min_lat + request.max_lat) * 0.5, -84.0, 84.0);
    double span_lon = 2.4;
    double span_lat = 1.8;
    int band = 0;
    if (out.zoom >= 16) {
        span_lon = 0.18; span_lat = 0.15; band = 3;
    } else if (out.zoom >= 14) {
        span_lon = 0.48; span_lat = 0.38; band = 2;
    } else if (out.zoom >= 11) {
        span_lon = 1.15; span_lat = 0.90; band = 1;
    }
    const double visible_lon = std::max(0.01, std::fabs(request.max_lon - request.min_lon) * 1.15);
    const double visible_lat = std::max(0.01, std::fabs(request.max_lat - request.min_lat) * 1.15);
    span_lon = std::clamp(std::max(span_lon, visible_lon), span_lon, band == 0 ? 3.2 : span_lon * 1.75);
    span_lat = std::clamp(std::max(span_lat, visible_lat), span_lat, band == 0 ? 2.4 : span_lat * 1.75);
    const double step_lon = span_lon * 0.55;
    const double step_lat = span_lat * 0.55;
    const long long qlon = static_cast<long long>(std::llround(center_lon / step_lon));
    const long long qlat = static_cast<long long>(std::llround(center_lat / step_lat));
    const double quantized_lon = static_cast<double>(qlon) * step_lon;
    const double quantized_lat = static_cast<double>(qlat) * step_lat;
    out.min_lon = std::clamp(quantized_lon - span_lon * 0.5, -180.0, 180.0);
    out.max_lon = std::clamp(quantized_lon + span_lon * 0.5, -180.0, 180.0);
    out.min_lat = std::clamp(quantized_lat - span_lat * 0.5, -85.0, 85.0);
    out.max_lat = std::clamp(quantized_lat + span_lat * 0.5, -85.0, 85.0);
    out.key = "b" + std::to_string(band) + "_" + std::to_string(qlon) + "_" + std::to_string(qlat);
    return out;
}

std::string overpass_query(const QuantizedRequest& request) {
    const char* places = request.zoom >= 14
        ? "^(city|town|village|hamlet|suburb|neighbourhood|quarter|locality)$"
        : (request.zoom >= 11 ? "^(city|town|village|suburb)$" : "^(city|town)$");
    std::ostringstream bbox;
    bbox << std::fixed << std::setprecision(6)
         << request.min_lat << "," << request.min_lon << ","
         << request.max_lat << "," << request.max_lon;
    std::ostringstream query;
    query << "[out:json][timeout:15][maxsize:8388608];";
    query << "node[\"place\"~\"" << places << "\"](" << bbox.str() << ");";
    query << "out body qt 1800;";
    return query.str();
}

std::string water_query(const QuantizedRequest& request, bool waterways) {
    const double center_lon = (request.min_lon + request.max_lon) * 0.5;
    const double center_lat = (request.min_lat + request.max_lat) * 0.5;
    const double half_lon = std::min(0.30, std::max(0.08, (request.max_lon - request.min_lon) * 0.5));
    const double half_lat = std::min(0.25, std::max(0.07, (request.max_lat - request.min_lat) * 0.5));
    std::ostringstream bbox;
    bbox << std::fixed << std::setprecision(6)
         << center_lat - half_lat << "," << center_lon - half_lon << ","
         << center_lat + half_lat << "," << center_lon + half_lon;
    std::ostringstream query;
    query << "[out:json][timeout:15][maxsize:8388608];";
    if (waterways) {
        query << "way[\"waterway\"~\"^(river|canal)$\"][\"name\"](" << bbox.str() << ");";
    } else {
        query << "way[\"natural\"=\"water\"][\"name\"](" << bbox.str() << ");";
    }
    query << "out center tags qt 300;";
    return query.str();
}

std::string boundary_index_query(const QuantizedRequest& request) {
    std::ostringstream bbox;
    bbox << std::fixed << std::setprecision(6)
         << request.min_lat << "," << request.min_lon << ","
         << request.max_lat << "," << request.max_lon;
    std::ostringstream query;
    query << "[out:json][timeout:15][maxsize:4194304];"
          << "relation[\"boundary\"=\"administrative\"][\"admin_level\"~\"^(4|6)$\"]("
          << bbox.str() << ");out tags center qt;";
    return query.str();
}

void publish(std::shared_ptr<Snapshot> snapshot) {
    if (!snapshot) return;
    SharedState& state = shared_state();
    std::lock_guard<std::mutex> lock(state.mutex);
    auto merged = std::make_shared<Snapshot>();
    merged->key = snapshot->key;
    merged->from_cache = snapshot->from_cache;
    if (state.snapshot) *merged = *state.snapshot;
    merged->key = snapshot->key;
    merged->from_cache = snapshot->from_cache;
    const auto merge_by_id = [](auto& target, const auto& incoming, std::size_t cap) {
        for (const auto& item : incoming) {
            auto found = std::find_if(target.begin(), target.end(), [&](const auto& old) { return old.id == item.id; });
            if (found == target.end()) target.push_back(item);
            else *found = item;
        }
        if (target.size() > cap) target.erase(target.begin(), target.begin() + static_cast<std::ptrdiff_t>(target.size() - cap));
    };
    merge_by_id(merged->places, snapshot->places, 2400u);
    merge_by_id(merged->waters, snapshot->waters, 800u);
    merge_by_id(merged->boundaries, snapshot->boundaries, 1000u);
    state.snapshot = std::move(merged);
}

void load_request(QuantizedRequest request) {
    SharedState& state = shared_state();
    const fs::path cache_path = request.cache_dir / (request.key + ".json");
    std::shared_ptr<Snapshot> stale;
    if (fs::exists(cache_path)) stale = parse_compact(read_file(cache_path), request.key);
    if (stale && cache_is_fresh(cache_path)) {
        publish(stale);
        state.loading.store(false);
        return;
    }

    const std::array<const char*, 2> endpoints = {
        "https://overpass-api.de/api/interpreter?data=",
        "https://overpass.kumi.systems/api/interpreter?data="
    };
    std::shared_ptr<Snapshot> fresh;
    for (const char* endpoint : endpoints) {
        std::string response;
        if (!download_text(std::string(endpoint) + percent_encode(overpass_query(request)), response)) continue;
        fresh = parse_overpass(response, request.key);
        if (fresh) break;
    }
    if (fresh) {
        for (const bool waterways : {false, true}) {
            for (const char* endpoint : endpoints) {
                std::string water_response;
                if (!download_text(std::string(endpoint) + percent_encode(water_query(request, waterways)), water_response)) continue;
                const std::shared_ptr<Snapshot> water = parse_overpass(water_response, request.key);
                if (water) {
                    fresh->waters.insert(fresh->waters.end(),
                        std::make_move_iterator(water->waters.begin()),
                        std::make_move_iterator(water->waters.end()));
                    break;
                }
            }
        }
        std::stable_sort(fresh->waters.begin(), fresh->waters.end(), [](const WaterRecord& a, const WaterRecord& b) {
            return a.id < b.id;
        });
        fresh->waters.erase(std::unique(fresh->waters.begin(), fresh->waters.end(), [](const WaterRecord& a, const WaterRecord& b) {
            return a.id == b.id;
        }), fresh->waters.end());
        const double center_lon = (request.min_lon + request.max_lon) * 0.5;
        const double center_lat = (request.min_lat + request.max_lat) * 0.5;
        for (const char* endpoint : endpoints) {
            std::string index_response;
            if (!download_text(std::string(endpoint) + percent_encode(boundary_index_query(request)), index_response)) continue;
            const std::vector<std::int64_t> ids = boundary_relation_ids(index_response, center_lon, center_lat);
            if (ids.empty()) break;
            std::vector<BoundaryRecord> loaded_boundaries;
            constexpr std::size_t kBoundaryBatch = 16u;
            for (std::size_t offset = 0; offset < ids.size(); offset += kBoundaryBatch) {
                const std::size_t end = std::min(ids.size(), offset + kBoundaryBatch);
                const std::vector<std::int64_t> batch(ids.begin() + static_cast<std::ptrdiff_t>(offset),
                                                       ids.begin() + static_cast<std::ptrdiff_t>(end));
                std::string geometry_response;
                if (!download_text(std::string(endpoint) + percent_encode(boundary_geometry_query(batch)), geometry_response)) continue;
                const std::shared_ptr<Snapshot> boundaries = parse_overpass(geometry_response, request.key);
                if (boundaries) {
                    loaded_boundaries.insert(loaded_boundaries.end(),
                        std::make_move_iterator(boundaries->boundaries.begin()),
                        std::make_move_iterator(boundaries->boundaries.end()));
                }
            }
            if (!loaded_boundaries.empty()) {
                assign_boundary_parents(loaded_boundaries);
                fresh->boundaries = std::move(loaded_boundaries);
                break;
            }
        }
    }
    if (fresh) {
        fresh->from_cache = false;
        write_compact_cache(cache_path, *fresh);
        prune_cache(request.cache_dir);
        publish(std::move(fresh));
    } else if (stale) {
        publish(std::move(stale));
    }
    state.loading.store(false);
}

}  // namespace

void request(const Request& request_value) {
    if (request_value.cache_dir.empty()) return;
    const QuantizedRequest request = quantize(request_value);
    SharedState& state = shared_state();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.requested_key == request.key || state.loading.load()) return;
        const auto now = std::chrono::steady_clock::now();
        if (state.last_launch.time_since_epoch().count() != 0 &&
            now - state.last_launch < std::chrono::seconds(3)) return;
        state.requested_key = request.key;
        state.last_launch = now;
        state.loading.store(true);
    }
    std::thread([request]() { load_request(request); }).detach();
}

std::shared_ptr<const Snapshot> current_snapshot() {
    SharedState& state = shared_state();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.snapshot;
}

bool loading() {
    return shared_state().loading.load();
}

}  // namespace tlalpowa::openmap
