#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tlalpowa::openmap {

struct PlaceRecord {
    std::string id;
    std::string name;
    std::string kind;
    std::string admin1;
    std::string admin2;
    std::string source_url;
    double lon = 0.0;
    double lat = 0.0;
    int population = 0;
    int elevation_m = 0;
    int rank = 4;
};

struct WaterRecord {
    std::string id;
    std::string name;
    std::string kind;
    std::string source_url;
    double lon = 0.0;
    double lat = 0.0;
};

struct BoundaryRecord {
    std::string id;
    std::string name;
    std::string admin1_id;
    std::string admin2_id;
    std::string source_url;
    int admin_level = 0;
    double center_lon = 0.0;
    double center_lat = 0.0;
    std::vector<std::vector<std::pair<double, double>>> rings;
};

struct Snapshot {
    std::string key;
    std::vector<PlaceRecord> places;
    std::vector<WaterRecord> waters;
    std::vector<BoundaryRecord> boundaries;
    bool from_cache = false;
};

struct Request {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    int zoom = 0;
    std::filesystem::path cache_dir;
};

void request(const Request& request);
std::shared_ptr<const Snapshot> current_snapshot();
bool loading();

}  // namespace tlalpowa::openmap
