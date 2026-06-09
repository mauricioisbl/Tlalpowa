/* Núcleo visible de importación: descarga, detección y materialización de fuentes externas.
   La implementación C caliente RUOA/PEMBU/RAMA/REDMA vive ahora dentro del núcleo importador. */
#include "importador.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>
#ifndef SECURITY_FLAG_IGNORE_REVOCATION
#define SECURITY_FLAG_IGNORE_REVOCATION 0x00000080
#endif
#endif

namespace fs = std::filesystem;

namespace ImportRuoa {

namespace {

struct RuoaStationDef {
    const char* id;
    const char* label;
    const char* folder;
};

const std::vector<RuoaStationDef>& estaciones_def() {
    static const std::vector<RuoaStationDef> estaciones = {
        {"cca",  "CCA",   "CCA"},
        {"enp1", "ENP1",  "ENP1"},
        {"enp2", "ENP2",  "ENP2"},
        {"enp3", "ENP3",  "ENP3"},
        {"enp4", "ENP4",  "ENP4"},
        {"enp5", "ENP5",  "ENP5"},
        {"enp6", "ENP6",  "ENP6"},
        {"enp7", "ENP7",  "ENP7"},
        {"enp8", "ENP8",  "ENP8"},
        {"enp9", "ENP9",  "ENP9"},
        {"ccha", "CCH-A", "CCHA"},
        {"ccho", "CCH-O", "CCHO"},
        {"cchs", "CCH-S", "CCHS"},
        {"cchv", "CCH-V", "CCHV"},
        {"cchn", "CCH-N", "CCHN"}
    };
    return estaciones;
}

std::string two_digits(int v) {
    std::ostringstream os;
    os << std::setw(2) << std::setfill('0') << std::clamp(v, 1, 12);
    return os.str();
}

std::string trim_ascii(std::string s) {
    auto is_sp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && is_sp(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_sp(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string lower_ascii(std::string s) {
    for (char& c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u >= 'A' && u <= 'Z') c = static_cast<char>(u + 32u);
    }
    return s;
}

std::string percent_encode_utf8(std::string_view s) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3u);
    for (unsigned char c : s) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                           c == '.' || c == '~';
        if (plain) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4u]);
            out.push_back(hex[c & 15u]);
        }
    }
    return out;
}

bool string_looks_like_email(const std::string& s) {
    const auto at = s.find('@');
    return at != std::string::npos && at > 0 && s.find('.', at) != std::string::npos;
}

std::string infer_ruoa_email(const RuoaCredentials& cred) {
    if (string_looks_like_email(cred.correo)) return trim_ascii(cred.correo);
    if (string_looks_like_email(cred.usuario)) return trim_ascii(cred.usuario);
    return {};
}

std::string getenv_text(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    size_t value_len = 0u;
    if (!name || _dupenv_s(&value, &value_len, name) != 0 || !value) return {};
    std::string out = trim_ascii(std::string(value));
    std::free(value);
    return out;
#else
    const char* v = std::getenv(name);
    return v ? trim_ascii(std::string(v)) : std::string{};
#endif
}

std::string infer_ruoa_public_name(const RuoaCredentials& cred) {
    const std::string explicit_name = trim_ascii(cred.nombre_publico);
    if (!explicit_name.empty() && !string_looks_like_email(explicit_name)) return explicit_name;

    const std::string env_name = getenv_text("TLALPOWA_RUOA_DISPLAY_NAME");
    if (!env_name.empty() && !string_looks_like_email(env_name)) return env_name;

    const std::string e = lower_ascii(infer_ruoa_email(cred));
    if (!cred.usuario.empty() && !string_looks_like_email(cred.usuario)) return trim_ascii(cred.usuario);
    if (!e.empty()) {
        std::string stem = e.substr(0, e.find('@'));
        std::replace(stem.begin(), stem.end(), '.', ' ');
        std::replace(stem.begin(), stem.end(), '_', ' ');
        std::replace(stem.begin(), stem.end(), '-', ' ');
        return trim_ascii(stem.empty() ? "tlalpowa" : stem);
    }
    return "tlalpowa";
}

bool read_head_lower(const fs::path& p, std::string& out, std::size_t max_bytes = 16384) {
    out.clear();
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::string buf(max_bytes, '\0');
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    buf.resize(static_cast<std::size_t>(std::max<std::streamsize>(0, in.gcount())));
    out.reserve(buf.size());
    for (unsigned char c : buf) {
        if (c >= 'A' && c <= 'Z') out.push_back(static_cast<char>(c + 32u));
        else out.push_back(static_cast<char>(c));
    }
    return true;
}

std::string path_utf8_local(const fs::path& p) {
#if defined(__cpp_char8_t)
    const auto u = p.u8string();
    return std::string(reinterpret_cast<const char*>(u.data()), u.size());
#else
    return p.u8string();
#endif
}

std::uintmax_t file_size_or_zero_local(const fs::path& p) {
    std::error_code ec;
    const auto n = fs::file_size(p, ec);
    return ec ? 0u : n;
}


fs::path ruoa_project_root_from_any_path(fs::path p) {
    std::error_code ec;
    if (!p.empty() && !fs::is_directory(p, ec)) p = p.parent_path();
    for (fs::path cur = p; !cur.empty(); cur = cur.parent_path()) {
        if (fs::exists(cur / "Fuente", ec) || fs::exists(cur / "Tlalpowa.exe", ec) || fs::exists(cur / "Compilar_Tlalpowa.cmd", ec)) return cur;
        if (cur == cur.parent_path()) break;
    }
    if (!p.empty()) {
        fs::path cur = p;
        for (int i = 0; i < 4 && !cur.empty(); ++i) cur = cur.parent_path();
        if (!cur.empty()) return cur;
    }
    return fs::current_path(ec);
}

std::string ruoa_log_stamp_local() {
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv) == 0) return "0000-00-00T00:00:00";
    return std::string(buf);
}

void ruoa_base_log_from_path(const fs::path& any_path, const std::string& message) {
    if (message.empty()) return;
    try {
        const fs::path base = ruoa_project_root_from_any_path(any_path);
        std::ofstream out(base / "Tlalpowa.log", std::ios::binary | std::ios::app);
        if (!out) return;
        out << ruoa_log_stamp_local() << " | RUOA_PEMBU | " << message << "\n";
    } catch (...) {}
}

int ruoa_parse_int_env_local(const char* name, int fallback, int lo, int hi) {
    const std::string v = getenv_text(name);
    if (v.empty()) return fallback;
    char* end = nullptr;
    long x = std::strtol(v.c_str(), &end, 10);
    if (!end || *end != '\0') return fallback;
    return std::clamp(static_cast<int>(x), lo, hi);
}

struct RuoaPeriodLimit { int anio; int mes; };

RuoaPeriodLimit ruoa_latest_probable_published_period() {
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    int year = tmv.tm_year + 1900;
    int month = tmv.tm_mon + 1;
    int current_excluded_year = year;
    int current_excluded_month = month - 1;
    while (current_excluded_month < 1) { current_excluded_month += 12; --current_excluded_year; }

    const int env_y = ruoa_parse_int_env_local("TLALPOWA_RUOA_MAX_YEAR", 0, 1997, 2300);
    const int env_m = ruoa_parse_int_env_local("TLALPOWA_RUOA_MAX_MONTH", 0, 1, 12);
    if (env_y > 0 && env_m > 0) {
        /* Nunca permitir mes actual ni meses futuros aunque el entorno pida un
           limite demasiado nuevo. Esta barrera evita martillar PEMBU con
           periodos que casi nunca estan publicados todavia. */
        if (env_y > current_excluded_year || (env_y == current_excluded_year && env_m > current_excluded_month)) {
            return {current_excluded_year, current_excluded_month};
        }
        return {env_y, env_m};
    }

    /* PEMBU no publica necesariamente el mes en curso. Se evita martillar meses
       futuros/no publicados; por defecto se usa rezago de dos meses, que en
       junio de 2026 inicia exactamente en Pembu_cca_2026_04.csv. */
    const int lag = ruoa_parse_int_env_local("TLALPOWA_RUOA_PUBLICATION_LAG_MONTHS", 2, 1, 11);
    month -= lag;
    while (month < 1) { month += 12; --year; }
    if (year > current_excluded_year || (year == current_excluded_year && month > current_excluded_month)) {
        year = current_excluded_year;
        month = current_excluded_month;
    }
    return {year, std::clamp(month, 1, 12)};
}

std::vector<std::pair<int,int>> ruoa_build_period_schedule(int year_oldest, int year_newest, RuoaPeriodLimit limit) {
    std::vector<std::pair<int,int>> periods;
    for (int anio = year_newest; anio >= year_oldest; --anio) {
        if (anio > limit.anio) continue;
        int start_month = (anio == limit.anio) ? limit.mes : 12;
        if (start_month < 1) continue;
        for (int mes = std::clamp(start_month, 1, 12); mes >= 1; --mes) periods.emplace_back(anio, mes);
    }
    return periods;
}

std::string ruoa_short_file_head(const fs::path& p) {
    std::string h;
    if (!read_head_lower(p, h, 256)) return {};
    std::string out;
    out.reserve(std::min<std::size_t>(h.size(), 200));
    for (unsigned char c : h) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        if (c < 32 || c == 127) continue;
        out.push_back(static_cast<char>(c));
        if (out.size() >= 200) break;
    }
    return out;
}

bool parece_csv_ruoa_valido(const fs::path& p, std::uintmax_t min_bytes) {
    if (file_size_or_zero_local(p) < min_bytes) return false;
    std::string h;
    if (!read_head_lower(p, h)) return false;
    if (h.find("<html") != std::string::npos || h.find("<!doctype") != std::string::npos ||
        h.find("bad gateway") != std::string::npos || h.find("certificate verify failed") != std::string::npos ||
        h.find("usuario o e-mail") != std::string::npos || h.find("contraseña") != std::string::npos ||
        h.find("restricted content") != std::string::npos || h.find("wordpress") != std::string::npos) return false;
    const bool has_header = h.find("fecha_hora") != std::string::npos || h.find("fecha hora") != std::string::npos;
    const bool has_pembu = h.find("programa de estaciones meteorologicas") != std::string::npos ||
                           h.find("programa de estaciones meteorológicas") != std::string::npos ||
                           h.find("www.ruoa.unam.mx/pembu") != std::string::npos;
    const bool has_vars = h.find("temp") != std::string::npos &&
                          (h.find("hum_rel") != std::string::npos || h.find("hum rel") != std::string::npos) &&
                          (h.find("presion") != std::string::npos || h.find("presión") != std::string::npos);
    return (has_header && has_vars) || (has_pembu && has_header);
}

bool promover_atomico(const fs::path& tmp, const fs::path& final_path) {
    std::error_code ec;
    fs::create_directories(final_path.parent_path(), ec);
    fs::path bak = final_path;
    bak += L".prev";
    fs::remove(bak, ec);
    if (fs::exists(final_path, ec)) {
        fs::rename(final_path, bak, ec);
        if (ec) {
            ec.clear();
            fs::remove(final_path, ec);
        }
    }
    ec.clear();
    fs::rename(tmp, final_path, ec);
    if (ec) {
        ec.clear();
        fs::copy_file(tmp, final_path, fs::copy_options::overwrite_existing, ec);
        if (!ec) fs::remove(tmp, ec);
    }
    const bool ok = fs::exists(final_path, ec) && !ec;
    if (ok) fs::remove(bak, ec);
    else if (fs::exists(bak, ec)) {
        ec.clear();
        fs::rename(bak, final_path, ec);
    }
    return ok;
}

std::string ruoa_redact_endpoint_url(std::string url) {
    auto redact = [&](const char* key) {
        const std::string k = std::string(key) + "=";
        const std::size_t b = url.find(k);
        if (b == std::string::npos) return;
        const std::size_t v = b + k.size();
        const std::size_t e = url.find('&', v);
        url.replace(v, e == std::string::npos ? std::string::npos : e - v, "<redacted>");
    };
    redact("user");
    redact("email");
    return url;
}

std::string construir_url_pembu(const std::string& estacion, int anio, int mes, const RuoaCredentials& cred) {
    const std::string mm = two_digits(mes);
    const std::string nombre = infer_ruoa_public_name(cred);
    const std::string correo = infer_ruoa_email(cred);
    std::string url = "https://www.ruoa.unam.mx:54151/pembu_rd?id=" + percent_encode_utf8(estacion) +
                      "&anio=" + std::to_string(anio) +
                      "&mes=" + percent_encode_utf8(mm) +
                      "&user=" + percent_encode_utf8(nombre) +
                      "&email=" + percent_encode_utf8(correo.empty() ? nombre : correo);
    return url;
}

#ifdef _WIN32
struct InternetHandle {
    HINTERNET h = nullptr;
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET v) : h(v) {}
    ~InternetHandle() { if (h) InternetCloseHandle(h); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    InternetHandle(InternetHandle&& other) noexcept : h(other.h) { other.h = nullptr; }
    InternetHandle& operator=(InternetHandle&& other) noexcept {
        if (this != &other) {
            if (h) InternetCloseHandle(h);
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }
    explicit operator bool() const { return h != nullptr; }
};

std::wstring widen_utf8_local(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring(s.begin(), s.end());
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

bool descargar_wininet_ruoa(const std::string& url, const fs::path& tmp_path, long& http_status, std::string& error, std::atomic_bool* cancelar) {
    http_status = 0;
    error.clear();
    if (cancelar && cancelar->load()) { error = "cancelado antes de iniciar"; return false; }

    URL_COMPONENTSW uc{};
    wchar_t scheme[16]{};
    wchar_t host[256]{};
    wchar_t path[4096]{};
    wchar_t extra[4096]{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszScheme = scheme;
    uc.dwSchemeLength = static_cast<DWORD>(std::size(scheme));
    uc.lpszHostName = host;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(host));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));
    std::wstring wurl = widen_utf8_local(url);
    if (!InternetCrackUrlW(wurl.c_str(), 0, ICU_ESCAPE, &uc)) {
        error = "InternetCrackUrlW fallo";
        return false;
    }

    std::wstring path_and_query(path, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo && uc.dwExtraInfoLength > 0) path_and_query.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    const INTERNET_PORT port = uc.nPort ? uc.nPort : INTERNET_DEFAULT_HTTPS_PORT;

    InternetHandle inet(InternetOpenW(L"Tlalpowa-RUOA/2026 respectful-csv-bridge", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0));
    if (!inet) { error = "InternetOpenW fallo"; return false; }

    DWORD timeout = 30000;
    InternetSetOptionW(inet.h, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(inet.h, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(inet.h, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    if (cancelar && cancelar->load()) { error = "cancelado antes de conectar"; return false; }
    InternetHandle conn(InternetConnectW(inet.h, std::wstring(host, uc.dwHostNameLength).c_str(), port,
                                         nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0));
    if (!conn) { error = "InternetConnectW fallo"; return false; }

    const wchar_t* accept[] = { L"text/csv", L"text/plain", L"*/*", nullptr };
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE |
                  INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_SECURE |
                  INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    InternetHandle req(HttpOpenRequestW(conn.h, L"GET", path_and_query.c_str(), L"HTTP/1.1",
                                        L"https://ruoa.unam.mx/pembu/descargas_pembu/", accept, flags, 0));
    if (!req) { error = "HttpOpenRequestW fallo"; return false; }

    DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                           SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                           SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                           SECURITY_FLAG_IGNORE_REVOCATION;
    InternetSetOptionW(req.h, INTERNET_OPTION_SECURITY_FLAGS, &security_flags, sizeof(security_flags));

    const std::wstring headers = L"Accept: text/csv,text/plain,*/*\r\n"
                                 L"Cache-Control: no-cache\r\n"
                                 L"Pragma: no-cache\r\n"
                                 L"Referer: https://ruoa.unam.mx/pembu/descargas_pembu/\r\n";

    if (cancelar && cancelar->load()) { error = "cancelado antes de solicitar"; return false; }
    if (!HttpSendRequestW(req.h, headers.c_str(), static_cast<DWORD>(headers.size()), nullptr, 0)) {
        const DWORD gle = GetLastError();
        error = "HttpSendRequestW fallo codigo=" + std::to_string(static_cast<unsigned long>(gle));
        return false;
    }

    DWORD status = 0;
    DWORD status_len = sizeof(status);
    if (HttpQueryInfoW(req.h, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &status_len, nullptr)) {
        http_status = static_cast<long>(status);
    }
    if (http_status >= 400) {
        error = "HTTP " + std::to_string(http_status);
        return false;
    }

    std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
    if (!out) { error = "no se pudo abrir archivo temporal"; return false; }

    char buf[65536];
    DWORD got = 0;
    std::uintmax_t total = 0;
    while (true) {
        if (cancelar && cancelar->load()) { error = "cancelado durante lectura"; return false; }
        if (!InternetReadFile(req.h, buf, static_cast<DWORD>(sizeof(buf)), &got)) {
            error = "InternetReadFile fallo";
            return false;
        }
        if (got == 0) break;
        out.write(buf, static_cast<std::streamsize>(got));
        total += got;
        if (!out || total > (512ull * 1024ull * 1024ull)) {
            error = "flujo RUOA excedio limite de seguridad";
            return false;
        }
    }
    out.close();
    return total > 0;
}
#else
bool descargar_wininet_ruoa(const std::string&, const fs::path&, long& http_status, std::string& error, std::atomic_bool*) {
    http_status = 0;
    error = "RUOA WinINet disponible solo en Windows";
    return false;
}
#endif

struct DownloadOneResult {
    bool usable = false;
    bool downloaded = false;
    bool skipped_existing = false;
    bool failed = false;
    bool cancelled = false;
};

void cooperative_delay(int ms, std::atomic_bool* cancelar) {
    const int total = std::max(0, ms);
    int elapsed = 0;
    while (elapsed < total) {
        if (cancelar && cancelar->load()) return;
        const int slice = std::min(50, total - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        elapsed += slice;
    }
}

DownloadOneResult descargar_un_csv(TlalRuoaSession* session,
                                   const std::string& url,
                                   const fs::path& final_path,
                                   const RuoaDownloadOptions& opt,
                                   nlohmann::json& row) {
    DownloadOneResult result;
    row["url"] = ruoa_redact_endpoint_url(url);
    row["target"] = path_utf8_local(final_path);
    row["started"] = true;
    row["attempts"] = nlohmann::json::array();

    std::error_code ec;
    fs::create_directories(final_path.parent_path(), ec);

    if (opt.conservar_csv_valido && parece_csv_ruoa_valido(final_path, opt.csv_min_bytes)) {
        row["ok"] = true;
        row["result"] = "existente_valido";
        row["skipped_existing_valid_csv"] = true;
        row["final_size_bytes"] = file_size_or_zero_local(final_path);
        result.usable = true;
        result.skipped_existing = true;
        ruoa_base_log_from_path(final_path, std::string("CSV_SKIP_NO_REQUEST | target=") + path_utf8_local(final_path.filename()) + " | motivo=existente_valido");
        return result;
    }

    ruoa_base_log_from_path(final_path, std::string("CSV_START | target=") + path_utf8_local(final_path.filename()) + " | url_host=www.ruoa.unam.mx:54151 | path=/pembu_rd");

    const fs::path tmp = fs::path(final_path.wstring() + L".download");
    fs::remove(tmp, ec);

    const int intentos = std::clamp(opt.intentos_por_csv, 1, 8);
    for (int i = 1; i <= intentos; ++i) {
        if (opt.cancelar && opt.cancelar->load()) {
            row["ok"] = false;
            row["cancelled"] = true;
            row["result"] = "cancelado";
            result.cancelled = true;
            result.failed = true;
            return result;
        }
        nlohmann::json attempt;
        attempt["n"] = i;
        TlalRuoaDownloadReport dl_report;
        const int cancel_snapshot = (opt.cancelar && opt.cancelar->load()) ? 1 : 0;
        ruoa_base_log_from_path(final_path, std::string("CSV_ATTEMPT_BEGIN | n=") + std::to_string(i) + " | tmp=" + path_utf8_local(tmp.filename()));
        const bool got = tlal_ruoa_session_download_utf8(session,
                                                        url.c_str(),
                                                        path_utf8_local(tmp).c_str(),
                                                        &cancel_snapshot,
                                                        &dl_report) != 0;
        attempt["http_status"] = dl_report.http_status;
        attempt["downloaded_bytes"] = static_cast<std::uintmax_t>(dl_report.bytes_written);
        attempt["transport_ok"] = got;
        attempt["stage"] = dl_report.stage;
        if (dl_report.message[0]) attempt["message"] = dl_report.message;

        const bool valid = got && parece_csv_ruoa_valido(tmp, opt.csv_min_bytes);
        attempt["csv_valid"] = valid;
        if (!valid && fs::exists(tmp, ec)) attempt["head"] = ruoa_short_file_head(tmp);
        row["attempts"].push_back(attempt);
        ruoa_base_log_from_path(final_path,
            std::string("CSV_ATTEMPT_END | n=") + std::to_string(i) +
            " | transport=" + (got ? "ok" : "fail") +
            " | http=" + std::to_string(dl_report.http_status) +
            " | bytes=" + std::to_string(static_cast<std::uintmax_t>(dl_report.bytes_written)) +
            " | stage=" + dl_report.stage +
            " | valid=" + (valid ? "yes" : "no") +
            (dl_report.message[0] ? (std::string(" | msg=") + dl_report.message) : std::string()) +
            ((!valid && fs::exists(tmp, ec)) ? (std::string(" | head=") + ruoa_short_file_head(tmp)) : std::string()));

        if (valid && promover_atomico(tmp, final_path) && parece_csv_ruoa_valido(final_path, opt.csv_min_bytes)) {
            row["ok"] = true;
            row["result"] = "descargado";
            row["final_size_bytes"] = file_size_or_zero_local(final_path);
            result.usable = true;
            result.downloaded = true;
            ruoa_base_log_from_path(final_path, std::string("CSV_OK | promoted=") + path_utf8_local(final_path));
            return result;
        }

        fs::remove(tmp, ec);
        if (i < intentos) cooperative_delay(250 * i, opt.cancelar);
    }

    row["ok"] = false;
    row["result"] = "fallo";
    ruoa_base_log_from_path(final_path, std::string("CSV_FAIL | target=") + path_utf8_local(final_path));
    row["final_size_bytes"] = file_size_or_zero_local(final_path);
    result.failed = true;
    return result;
}

} // namespace

const std::vector<std::string>& estaciones_pembu() {
    static const std::vector<std::string> ids = [] {
        std::vector<std::string> out;
        for (const auto& e : estaciones_def()) out.emplace_back(e.id);
        return out;
    }();
    return ids;
}

bool descargar_csvs_pembu(const RuoaCredentials& credenciales,
                          const RuoaDownloadOptions& opciones,
                          nlohmann::json& auditoria,
                          RuoaProgressCallback progreso) {
    RuoaDownloadOptions opt = opciones;
    if (opt.destino_raiz.empty()) return false;
    if (opt.anio_mas_reciente < opt.anio_mas_antiguo) std::swap(opt.anio_mas_reciente, opt.anio_mas_antiguo);
    opt.anio_mas_antiguo = std::clamp(opt.anio_mas_antiguo, 1997, 2300);
    opt.anio_mas_reciente = std::clamp(opt.anio_mas_reciente, opt.anio_mas_antiguo, 2300);
    opt.pausa_ms_entre_csv = std::max(1000, opt.pausa_ms_entre_csv);
    opt.csv_min_bytes = std::max<std::uintmax_t>(16, opt.csv_min_bytes);

    const auto& estaciones = estaciones_def();
    const RuoaPeriodLimit published_limit = ruoa_latest_probable_published_period();
    if (opt.anio_mas_reciente > published_limit.anio) opt.anio_mas_reciente = published_limit.anio;
    if (opt.anio_mas_antiguo > opt.anio_mas_reciente) opt.anio_mas_antiguo = opt.anio_mas_reciente;
    std::vector<std::pair<int,int>> period_schedule = ruoa_build_period_schedule(opt.anio_mas_antiguo, opt.anio_mas_reciente, published_limit);
    if (period_schedule.empty()) {
        auditoria = nlohmann::json::object();
        auditoria["dataset"] = "RUOA_UNAM_PEMBU";
        auditoria["ok"] = false;
        auditoria["error"] = "sin_periodos_publicados_en_rango";
        auditoria["published_limit_year"] = published_limit.anio;
        auditoria["published_limit_month"] = two_digits(published_limit.mes);
        ruoa_base_log_from_path(opt.destino_raiz, std::string("STOP | sin periodos publicados en rango | latest_period=") + std::to_string(published_limit.anio) + "-" + two_digits(published_limit.mes));
        return false;
    }
    const int station_total = static_cast<int>(period_schedule.size());
    const int total = static_cast<int>(estaciones.size()) * station_total;

    std::vector<RuoaStationProgress> station_progress;
    station_progress.reserve(estaciones.size());
    for (const auto& e : estaciones) {
        RuoaStationProgress sp;
        sp.estacion = e.id;
        sp.etiqueta = e.label;
        sp.total = station_total;
        station_progress.push_back(std::move(sp));
    }

    auditoria = nlohmann::json::object();
    auditoria["dataset"] = "RUOA_UNAM_PEMBU";
    auditoria["policy"] = "descarga transversal por mes: años recientes primero, mes publicado mas reciente a enero, todas las estaciones por ronda, pausa minima de 1 segundo por CSV, escritura atomica y validacion anti-HTML";
    auditoria["root"] = path_utf8_local(opt.destino_raiz);
    auditoria["year_newest"] = opt.anio_mas_reciente;
    auditoria["year_oldest"] = opt.anio_mas_antiguo;
    auditoria["delay_ms_between_csv"] = opt.pausa_ms_entre_csv;
    auditoria["published_limit_year"] = published_limit.anio;
    auditoria["published_limit_month"] = two_digits(published_limit.mes);
    auditoria["scheduled_months_per_station"] = station_total;
    auditoria["endpoint_user"] = infer_ruoa_public_name(credenciales);
    auditoria["endpoint_email_present"] = !infer_ruoa_email(credenciales).empty();
    auditoria["stations"] = nlohmann::json::array();
    for (const auto& e : estaciones) auditoria["stations"].push_back({{"id", e.id}, {"label", e.label}, {"folder", e.folder}});
    auditoria["rows"] = nlohmann::json::array();
    ruoa_base_log_from_path(opt.destino_raiz,
        std::string("START | root=") + path_utf8_local(opt.destino_raiz) +
        " | years=" + std::to_string(opt.anio_mas_reciente) + ".." + std::to_string(opt.anio_mas_antiguo) +
        " | latest_period=" + std::to_string(published_limit.anio) + "-" + two_digits(published_limit.mes) +
        " | periods_per_station=" + std::to_string(station_total) +
        " | total_csv=" + std::to_string(total));

    TlalRuoaSession* ruoa_session = tlal_ruoa_session_create();
    if (!ruoa_session) {
        ruoa_base_log_from_path(opt.destino_raiz, "SESSION_FAIL | no se pudo inicializar sesion HTTP RUOA");
        auditoria["login_ok"] = false;
        auditoria["login_stage"] = "session_create";
        auditoria["login_message"] = "No se pudo inicializar sesion WinINet RUOA.";
        return false;
    }
    struct RuoaSessionGuard { TlalRuoaSession* s; ~RuoaSessionGuard() { tlal_ruoa_session_destroy(s); } } ruoa_guard{ruoa_session};
    TlalRuoaLoginReport login_report;
    ruoa_base_log_from_path(opt.destino_raiz, "LOGIN_BEGIN | solicitando pagina de login y validando sesion RUOA");
    const int login_ok = tlal_ruoa_session_login(ruoa_session,
                                                 trim_ascii(credenciales.usuario).c_str(),
                                                 credenciales.password.c_str(),
                                                 &login_report);
    auditoria["login_ok"] = login_ok != 0;
    auditoria["login_stage"] = login_report.stage;
    auditoria["login_http_status"] = login_report.http_status;
    auditoria["login_message"] = login_report.message;
    ruoa_base_log_from_path(opt.destino_raiz,
        std::string("LOGIN_END | ok=") + (login_ok ? "yes" : "no") +
        " | stage=" + login_report.stage +
        " | http=" + std::to_string(login_report.http_status) +
        (login_report.message[0] ? (std::string(" | msg=") + login_report.message) : std::string()));
    if (!login_ok) return false;

    if (progreso) {
        RuoaProgress p;
        p.completados = 0;
        p.total = total;
        p.descargados = 0;
        p.utilizables = 0;
        p.omitidos_validos = 0;
        p.fallidos = 0;
        p.anio = published_limit.anio;
        p.mes = published_limit.mes;
        p.estacion = "RUOA";
        p.etiqueta = "RUOA";
        p.destino = opt.destino_raiz;
        p.fase = "sesion_validada";
        p.estaciones = station_progress;
        progreso(p);
    }

    RuoaCredentials endpoint_cred = credenciales;
    const char* page_user = tlal_ruoa_session_endpoint_user_utf8(ruoa_session);
    const char* page_email = tlal_ruoa_session_endpoint_email_utf8(ruoa_session);
    if (page_user && page_user[0]) endpoint_cred.nombre_publico = page_user;
    if (page_email && page_email[0]) endpoint_cred.correo = page_email;
    auditoria["endpoint_user"] = infer_ruoa_public_name(endpoint_cred);
    auditoria["endpoint_email_present"] = !infer_ruoa_email(endpoint_cred).empty();
    ruoa_base_log_from_path(opt.destino_raiz,
        std::string("IDENTITY | endpoint_user_present=") + (!infer_ruoa_public_name(endpoint_cred).empty() ? "yes" : "no") +
        " | endpoint_email_present=" + (!infer_ruoa_email(endpoint_cred).empty() ? "yes" : "no"));

    int done = 0;
    int downloaded_count = 0;
    int usable_count = 0;
    int skipped = 0;
    int failed = 0;

    auto emit = [&](const char* fase, int anio, int mes, const RuoaStationDef& estacion, const fs::path& dst) {
        if (!progreso) return;
        RuoaProgress p;
        p.completados = done;
        p.total = total;
        p.descargados = downloaded_count;
        p.utilizables = usable_count;
        p.omitidos_validos = skipped;
        p.fallidos = failed;
        p.anio = anio;
        p.mes = mes;
        p.estacion = estacion.id;
        p.etiqueta = estacion.label;
        p.destino = dst;
        p.fase = fase ? fase : "";
        p.estaciones = station_progress;
        progreso(p);
    };

    const auto process_one = [&](std::size_t station_i, int anio, int mes) -> bool {
        const RuoaStationDef& estacion = estaciones[station_i];
        const fs::path estacion_dir = opt.destino_raiz / estacion.folder;
        std::error_code ec;
        fs::create_directories(estacion_dir, ec);
        const std::string mm = two_digits(mes);
        const fs::path target = estacion_dir / ("Pembu_" + std::string(estacion.id) + "_" + std::to_string(anio) + "_" + mm + ".csv");

        /* Corte temprano: si el CSV mensual ya existe y pasa la validacion
           estricta, no se construye URL efectiva ni se toca el servidor. Esta
           ruta protege a RUOA de peticiones redundantes y mantiene el avance
           local consistente. */
        if (opt.conservar_csv_valido && parece_csv_ruoa_valido(target, opt.csv_min_bytes)) {
            nlohmann::json row;
            row["station"] = estacion.id;
            row["station_label"] = estacion.label;
            row["year"] = anio;
            row["month"] = mm;
            row["target"] = path_utf8_local(target);
            row["ok"] = true;
            row["result"] = "existente_valido";
            row["skipped_existing_valid_csv"] = true;
            row["server_request"] = false;
            row["final_size_bytes"] = file_size_or_zero_local(target);
            row["valid_after"] = true;
            auditoria["rows"].push_back(row);

            ++done;
            auto& sp = station_progress[station_i];
            ++sp.completados;
            ++skipped;
            ++usable_count;
            ++sp.omitidos_validos;
            ruoa_base_log_from_path(target, std::string("CSV_SKIP_NO_REQUEST | target=") + path_utf8_local(target.filename()) + " | motivo=existente_valido_temprano");
            emit("existente", anio, mes, estacion, target);
            return true;
        }

        const std::string url = construir_url_pembu(estacion.id, anio, mes, endpoint_cred);

        emit("descargando", anio, mes, estacion, target);
        nlohmann::json row;
        row["station"] = estacion.id;
        row["station_label"] = estacion.label;
        row["year"] = anio;
        row["month"] = mm;
        DownloadOneResult one = descargar_un_csv(ruoa_session, url, target, opt, row);
        const bool valid_after = parece_csv_ruoa_valido(target, opt.csv_min_bytes);
        row["valid_after"] = valid_after;
        auditoria["rows"].push_back(row);

        ++done;
        auto& sp = station_progress[station_i];
        ++sp.completados;
        if (one.downloaded && valid_after) { ++downloaded_count; ++usable_count; ++sp.descargados; }
        else if (one.skipped_existing && valid_after) { ++skipped; ++usable_count; ++sp.omitidos_validos; }
        else { ++failed; ++sp.fallidos; }

        emit(valid_after ? (one.skipped_existing ? "existente" : "validado") : (one.cancelled ? "cancelado" : "fallo"), anio, mes, estacion, target);
        cooperative_delay(opt.pausa_ms_entre_csv, opt.cancelar);
        return !one.cancelled;
    };

    if (opt.transversal_por_mes) {
        for (const auto& periodo : period_schedule) {
            const int anio = periodo.first;
            const int mes = periodo.second;
            ruoa_base_log_from_path(opt.destino_raiz, std::string("PERIOD_BEGIN | anio=") + std::to_string(anio) + " | mes=" + two_digits(mes) + " | estaciones=" + std::to_string(estaciones.size()));
            for (std::size_t station_i = 0; station_i < estaciones.size(); ++station_i) {
                    if (opt.cancelar && opt.cancelar->load()) {
                        auditoria["cancelled"] = true;
                        auditoria["completed"] = done;
                        auditoria["downloaded"] = downloaded_count;
                        auditoria["primary_ok"] = usable_count;
                        auditoria["skipped_existing_valid"] = skipped;
                        auditoria["failed"] = failed;
                        return false;
                    }
                    if (!process_one(station_i, anio, mes)) {
                        auditoria["cancelled"] = true;
                        auditoria["completed"] = done;
                        auditoria["downloaded"] = downloaded_count;
                        auditoria["primary_ok"] = usable_count;
                        auditoria["skipped_existing_valid"] = skipped;
                        auditoria["failed"] = failed;
                        return false;
                    }
            }
            ruoa_base_log_from_path(opt.destino_raiz, std::string("PERIOD_END | anio=") + std::to_string(anio) + " | mes=" + two_digits(mes));
        }
    } else {
        for (std::size_t station_i = 0; station_i < estaciones.size(); ++station_i) {
            for (const auto& periodo : period_schedule) {
                const int anio = periodo.first;
                const int mes = periodo.second;
                    if (opt.cancelar && opt.cancelar->load()) {
                        auditoria["cancelled"] = true;
                        auditoria["completed"] = done;
                        auditoria["downloaded"] = downloaded_count;
                        auditoria["primary_ok"] = usable_count;
                        auditoria["skipped_existing_valid"] = skipped;
                        auditoria["failed"] = failed;
                        return false;
                    }
                    if (!process_one(station_i, anio, mes)) {
                        auditoria["cancelled"] = true;
                        auditoria["completed"] = done;
                        auditoria["downloaded"] = downloaded_count;
                        auditoria["primary_ok"] = usable_count;
                        auditoria["skipped_existing_valid"] = skipped;
                        auditoria["failed"] = failed;
                        return false;
                    }
                }
            }
        }

    auditoria["completed"] = done;
    auditoria["downloaded"] = downloaded_count;
    auditoria["primary_ok"] = usable_count;
    auditoria["skipped_existing_valid"] = skipped;
    auditoria["failed"] = failed;
    auditoria["complete"] = failed == 0 && usable_count > 0;
    ruoa_base_log_from_path(opt.destino_raiz,
        std::string("FINAL | completed=") + std::to_string(done) +
        " | downloaded=" + std::to_string(downloaded_count) +
        " | usable=" + std::to_string(usable_count) +
        " | existing=" + std::to_string(skipped) +
        " | failed=" + std::to_string(failed));
    return usable_count > 0 && !(opt.cancelar && opt.cancelar->load());
}

}  // namespace ImportRuoa
