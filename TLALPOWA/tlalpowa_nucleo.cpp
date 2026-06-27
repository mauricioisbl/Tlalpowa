// TLALPOWA: unidad central fusionada de núcleo C++/STL.
// No cambia contratos públicos; reduce archivos compilados y tránsito de cabeceras.
#include "tlalpowa_core.h"
#include "core.h"


// ===== Nucleos/Config.impl =====
#line 1 "Nucleos/Config.impl"






namespace epi {





static std::vector<std::string> split(const std::string& s, char sep) {

    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);

    while (std::getline(is, cur, sep)) {

        cur = trim(cur);

        if (!cur.empty()) out.push_back(cur);
    }

    return out;
}

static bool loose_alias_noise_word(const std::string& word) {
    if (word.size() < 4) return true;
    return word == "para" || word == "como" || word == "otros" || word == "otras" ||
        word == "enfermedad" || word == "enfermedades" || word == "infecciosas" ||
        word == "infecciosos" || word == "transmisibles" || word == "notificados" ||
        word == "jurisdiccion" || word == "sanitaria" || word == "aparato";
}

static std::vector<std::string> loose_alias_words(const std::string& norm) {
    std::vector<std::string> words;
    std::istringstream is(norm);
    std::string word;
    while (is >> word) {
        if (loose_alias_noise_word(word)) continue;
        if (std::find(words.begin(), words.end(), word) == words.end()) words.push_back(word);
    }
    return words;
}

static bool loose_alias_match(const std::string& text_norm, const std::string& alias_norm, size_t& score) {
    const auto words = loose_alias_words(alias_norm);
    if (words.size() < 2) return false;
    size_t local_score = 0;
    for (const auto& word : words) {
        if (!contains_norm(text_norm, word)) return false;
        local_score += word.size();
    }
    score = local_score;
    return true;
}





static std::vector<std::string> split_tsv_preserve_empty(const std::string& s) {

    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);


    while (std::getline(is, cur, '\t')) out.push_back(trim(cur));

    if (!s.empty() && s.back() == '\t') out.push_back("");

    return out;
}





static std::string normalize_cie10_for_match(std::string s) {
    std::string out;

    out.reserve(s.size());

    for (unsigned char ch : s) {


        if (std::isspace(ch)) continue;

        if (ch == ';' || ch == ',' || ch == ':' || ch == '(' || ch == ')' || ch == '[' || ch == ']') continue;

        out.push_back(static_cast<char>(std::toupper(ch)));
    }

    for (size_t i = 1; i < out.size(); ++i) {

        if (out[i] == 'O') out[i] = '0';
    }

    while (!out.empty() && (out.back() == '.' || out.back() == '-')) out.pop_back();

    return out;
}




static std::vector<std::string> split_cie10_values(const std::string& s) {

    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);

    while (std::getline(is, cur, '|')) {


        cur = normalize_cie10_for_match(cur);

        if (!cur.empty()) out.push_back(cur);
    }

    return out;
}



static bool cie10_base_triplet(const std::string& code, char& letter, int& number) {


    const std::string c = normalize_cie10_for_match(code);

    if (c.size() < 3 || !std::isalpha(static_cast<unsigned char>(c[0])) ||
        !std::isdigit(static_cast<unsigned char>(c[1])) || !std::isdigit(static_cast<unsigned char>(c[2]))) return false;
    letter = static_cast<char>(std::toupper(static_cast<unsigned char>(c[0])));
    number = (c[1] - '0') * 10 + (c[2] - '0');

    return true;
}





static bool cie10_code_matches_configured(const std::string& observed_raw, const std::string& configured_raw) {


    const std::string observed = normalize_cie10_for_match(observed_raw);


    const std::string configured = normalize_cie10_for_match(configured_raw);

    if (observed.empty() || configured.empty()) return false;

    if (observed == configured) return true;

    if (observed.rfind(configured + ".", 0) == 0) return true;

    const size_t dash = configured.find('-');

    if (dash != std::string::npos) {
        const std::string lo = configured.substr(0, dash);
        std::string hi = configured.substr(dash + 1);

        if (!lo.empty() && !hi.empty() && std::isdigit(static_cast<unsigned char>(hi[0]))) hi = lo.substr(0, 1) + hi;
        char ol = 0, ll = 0, hl = 0;
        int on = -1, ln = -1, hn = -1;

        if (!cie10_base_triplet(observed, ol, on) || !cie10_base_triplet(lo, ll, ln) || !cie10_base_triplet(hi, hl, hn)) return false;

        if (ll == hl && ol == ll) return on >= ln && on <= hn;

        if (ol < ll || ol > hl) return false;

        if (ol == ll && on < ln) return false;

        if (ol == hl && on > hn) return false;

        return true;
    }

    return false;
}




void Config::load(const fs::path& config_dir) {
    load_builtin();


    const auto jp = config_dir / "jurisdictions.tsv";

    const auto dp = config_dir / "diseases.tsv";

    const auto hp = config_dir / "diseases_historial.tsv";

    if (fs::exists(jp)) load_jurisdictions_tsv(jp);

    if (fs::exists(dp)) load_diseases_tsv(dp);

    if (fs::exists(hp)) load_diseases_tsv(hp);


    normalize_catalog_entries();
}




void Config::load_builtin() {
    jurisdictions_ = {
        {"gustavo_a_madero","Gustavo A. Madero",{"Gustavo A. Madero","Gustavo A Madero","GAM"}},
        {"azcapotzalco","Azcapotzalco",{"Azcapotzalco"}},
        {"iztacalco","Iztacalco",{"Iztacalco"}},

        {"coyoacan","Coyoacán",{"Coyoacán","Coyoacan"}},
        {"alvaro_obregon","Álvaro Obregón",{"Álvaro Obregón","Alvaro Obregon","Alvaro Obregón"}},
        {"magdalena_contreras","Magdalena Contreras",{"Magdalena Contreras","La Magdalena Contreras"}},
        {"cuajimalpa","Cuajimalpa",{"Cuajimalpa","Cuajimalpa de Morelos"}},
        {"tlalpan","Tlalpan",{"Tlalpan"}},
        {"iztapalapa","Iztapalapa",{"Iztapalapa"}},
        {"xochimilco","Xochimilco",{"Xochimilco"}},
        {"milpa_alta","Milpa Alta",{"Milpa Alta"}},
        {"tlahuac","Tláhuac",{"Tláhuac","Tlahuac"}},
        {"miguel_hidalgo","Miguel Hidalgo",{"Miguel Hidalgo"}},

        {"benito_juarez","Benito Juárez",{"Benito Juárez","Benito Juarez"}},

        {"cuauhtemoc","Cuauhtémoc",{"Cuauhtémoc","Cuauhtemoc"}},
        {"venustiano_carranza","Venustiano Carranza",{"Venustiano Carranza"}},
        {"total","Total",{"Total"}},
        {"edomex_atlacomulco","Atlacomulco",{"ATLACOMULCO","Atlacomulco"}},
        {"edomex_ixtlahuaca","Ixtlahuaca",{"IXTLAHUACA","Ixtlahuaca"}},
        {"edomex_jilotepec","Jilotepec",{"JILOTEPEC","Jilotepec"}},
        {"edomex_tenango_del_valle","Tenango del Valle",{"TENANGO DEL VALLE","Tenango del Valle"}},
        {"edomex_toluca","Toluca",{"TOLUCA","Toluca"}},
        {"edomex_xonacatlan","Xonacatlán",{"XONACATLAN","Xonacatlán","Xonacatlan"}},
        {"edomex_tejupilco","Tejupilco",{"TEJUPILCO","Tejupilco"}},
        {"edomex_tenancingo","Tenancingo",{"TENANCINGO","Tenancingo"}},

        {"edomex_valle_de_bravo","Valle de Bravo",{"VALLE DE BRAVO","Valle de Bravo"}},
        {"edomex_atizapan","Atizapán",{"ATIZAPAN","Atizapán","Atizapan"}},
        {"edomex_cuautitlan","Cuautitlán",{"CUAUTITLAN","Cuautitlán","Cuautitlan"}},
        {"edomex_naucalpan","Naucalpan",{"NAUCALPAN","Naucalpan"}},
        {"edomex_teotihuacan","Teotihuacán",{"TEOTIHUACAN","Teotihuacán","Teotihuacan"}},
        {"edomex_tlalnepantla","Tlalnepantla",{"TLALNEPANTLA","Tlalnepantla"}},
        {"edomex_zumpango","Zumpango",{"ZUMPANGO","Zumpango"}},
        {"edomex_amecameca","Amecameca",{"AMECAMECA","Amecameca"}},
        {"edomex_ecatepec","Ecatepec",{"ECATEPEC","Ecatepec"}},
        {"edomex_nezahualcoyotl","Nezahualcóyotl",{"NEZAHUALCOYOTL","Nezahualcóyotl","Nezahualcoyotl"}},

        {"edomex_texcoco","Texcoco",{"TEXCOCO","Texcoco"}},

        {"edomex_total","Edo. Méx.",{"EDO. MEX.","Edo. Mex.","Edo. Méx.","Estado de México","Estado de Mexico"}}
    };
    diseases_ = {
        {"neumococo_invasiva","Enfermedad invasiva por neumococo","prevenibles_vacunacion",{"A40.3","G00.1","J13"},{"Enfermedad Invasiva por Neumococo","Enfermedad invasiva por neumococo"}},
        {"rotavirus_enteritis","Enteritis debida a rotavirus","prevenibles_vacunacion",{"A08.0"},{"Enteritis debida a Rotavirus","Enteritis debida a rotavirus","rotavirus"}},
        {"hepatitis_a","Hepatitis vírica A","prevenibles_vacunacion",{"B15"},{"Hepatitis vírica A","Hepatitis virica A","Hepatitis A"}},
        {"hepatitis_b","Hepatitis vírica B","prevenibles_vacunacion",{"B16"},{"Hepatitis vírica B","Hepatitis virica B","Hepatitis B"}},
        {"vph","Infección por virus del papiloma humano","transmision_sexual",{"B97.7"},{"Infección por virus del papiloma humano","Infeccion por virus del papiloma humano","papiloma humano"}},
        {"haemophilus_influenzae_invasiva","Infecciones invasivas por Haemophilus influenzae","prevenibles_vacunacion",{"A41.3","G00.0","J14"},{"Infecciones invasivas por Haemophilus Influenzae","haemophilus influenzae"}},
        {"influenza","Influenza","respiratorias",{"J09","J10","J11"},{"Influenza"}},
        {"meningitis_meningococica","Meningitis meningocócica","prevenibles_vacunacion",{"A39.0"},{"Meningitis meningocócica","Meningitis meningococica"}},

        {"meningitis_tuberculosa","Meningitis tuberculosa","prevenibles_vacunacion",{"A17.0"},{"Meningitis tuberculosa"}},
        {"iras","Infecciones respiratorias agudas","respiratorias",{"J00-J06"},{"Infecciones respiratorias agudas","IRAS"}},
        {"neumonias_bronconeumonias","Neumonías y bronconeumonías","respiratorias",{"J12-J18"},{"Neumonías y bronconeumonías","Neumonias y bronconeumonias"}},
        {"covid_19","Covid-19","respiratorias",{"U07.1","U07.2"},{"Covid - 19","Covid-19","COVID 19"}},
        {"asma","Asma y estado asmático","no_transmisibles",{"J45","J46"},{"Asma y estado asmático","Asma y estado asmatico"}},
        {"hipertension","Hipertensión arterial","no_transmisibles",{"I10-I15"},{"Hipertensión arterial","Hipertension arterial"}},
        {"diabetes_tipo_2","Diabetes mellitus no insulinodependiente (Tipo II)","no_transmisibles",{"E11"},{"Diabetes mellitus no insulinodependiente","Tipo II"}},
        {"efectos_calor_luz","Efectos del calor y de la luz","no_transmisibles",{"T67"},{"Efectos del Calor y de La Luz","Efectos del calor y de la luz"}},
        {"toxoplasmosis","Toxoplasmosis","otras_transmisibles",{"B58"},{"Toxoplasmosis"}}
    };
}



void Config::load_jurisdictions_tsv(const fs::path& p) {

    std::vector<Jurisdiction> loaded;


    std::istringstream ss(read_text_file(p));
    std::string line;

    while (std::getline(ss, line)) {


        if (trim(line).empty() || line[0] == '#') continue;


        auto cols = split_tsv_preserve_empty(line);

        if (cols.size() < 2) continue;
        Jurisdiction j;
        j.id = cols[0];
        j.canonical = cols[1];


        if (cols.size() >= 3) j.aliases = split(cols[2], '|');

        j.aliases.push_back(j.canonical);

        loaded.push_back(j);
    }

    if (!loaded.empty()) jurisdictions_ = loaded;
}



void Config::load_diseases_tsv(const fs::path& p) {

    std::vector<Disease> loaded;

    std::istringstream ss(read_text_file(p));
    std::string line;

    while (std::getline(ss, line)) {


        if (trim(line).empty() || line[0] == '#') continue;


        auto cols = split_tsv_preserve_empty(line);

        if (cols.size() < 2) continue;
        Disease d;

        d.id = cols[0];
        d.canonical = cols[1];
        d.group = cols.size() > 2 ? cols[2] : "unknown";


        d.cie10 = cols.size() > 3 ? split(cols[3], '|') : std::vector<std::string>{};


        d.aliases = cols.size() > 4 ? split(cols[4], '|') : std::vector<std::string>{};

        d.aliases.push_back(d.canonical);

        loaded.push_back(d);
    }

    if (!loaded.empty()) {

        for (const auto& d : loaded) {
            auto it = std::find_if(diseases_.begin(), diseases_.end(), [&](const Disease& current) { return current.id == d.id; });

            if (it == diseases_.end()) diseases_.push_back(d);
            else {
                const auto merge_unique = [](std::vector<std::string>& out, const std::string& value) {
                    const std::string cleaned = trim(value);
                    if (cleaned.empty()) return;
                    if (std::find(out.begin(), out.end(), cleaned) == out.end()) out.push_back(cleaned);
                };
                if (!d.canonical.empty()) it->canonical = d.canonical;
                if (!d.group.empty()) it->group = d.group;
                for (const auto& code : d.cie10) merge_unique(it->cie10, code);
                for (const auto& alias : d.aliases) merge_unique(it->aliases, alias);
            }
        }
    }
}





void Config::normalize_catalog_entries() {

    auto add_unique = [](std::vector<std::string>& out, std::string value) {


        value = trim(std::move(value));

        if (value.empty()) return;

        if (std::find(out.begin(), out.end(), value) == out.end()) out.push_back(std::move(value));
    };

    for (auto& j : jurisdictions_) {
        add_unique(j.aliases, j.canonical);

        j.aliases_norm.clear();

        j.aliases_norm.reserve(j.aliases.size());


        for (const auto& alias : j.aliases) add_unique(j.aliases_norm, normalize_key(alias));
    }


    for (auto& d : diseases_) {

        add_unique(d.aliases, d.canonical);

        d.aliases_norm.clear();

        d.aliases_norm.reserve(d.aliases.size());


        for (const auto& alias : d.aliases) add_unique(d.aliases_norm, normalize_key(alias));

        d.cie10_norm.clear();

        d.cie10_norm.reserve(d.cie10.size());


        for (const auto& code : d.cie10) add_unique(d.cie10_norm, normalize_cie10_for_match(code));
    }
}





std::optional<Jurisdiction> Config::match_jurisdiction_line(const std::string& normalized_line) const {

    for (const auto& j : jurisdictions_) {


        for (const auto& a : j.aliases_norm) if (contains_norm(normalized_line, a)) return j;
    }

    return std::nullopt;
}





std::optional<Disease> Config::match_disease_text(const std::string& normalized_text) const {
    std::optional<Disease> best;
    size_t best_len = 0;

    for (const auto& d : diseases_) {

        for (const auto& a : d.aliases_norm) {


            if (a.size() > best_len && contains_norm(normalized_text, a)) {
                best = d;

                best_len = a.size();
            }
        }
    }
    for (const auto& d : diseases_) {
        for (const auto& a : d.aliases_norm) {
            size_t loose_score = 0;
            if (loose_alias_match(normalized_text, a, loose_score) && loose_score > best_len) {
                best = d;
                best_len = loose_score;
            }
        }
    }

    return best;
}




std::optional<Disease> Config::match_disease_cie10(const std::string& cie10_text) const {

    const auto observed_codes = split_cie10_values(cie10_text);

    if (observed_codes.empty()) return std::nullopt;



    for (const auto& observed : observed_codes) {

        for (const auto& d : diseases_) {

            for (const auto& configured : d.cie10_norm) {


                const std::string observed_norm = normalize_cie10_for_match(observed);
                const std::string& configured_norm = configured;
                char ol = 0, cl = 0;
                int on = -1, cn = -1;
                const bool same_base = observed_norm.find('-') == std::string::npos && configured_norm.find('-') == std::string::npos &&
                    cie10_base_triplet(observed_norm, ol, on) && cie10_base_triplet(configured_norm, cl, cn) && ol == cl && on == cn;

                if (observed_norm == configured_norm || observed_norm.rfind(configured_norm + ".", 0) == 0 || same_base) {

                    return d;
                }
            }
        }
    }



    for (const auto& observed : observed_codes) {

        for (const auto& d : diseases_) {

            for (const auto& configured : d.cie10_norm) {

                if (cie10_code_matches_configured(observed, configured)) return d;
            }
        }
    }

    return std::nullopt;
}

}

// ===== Nucleos/Dashboard.impl =====
#line 1 "Nucleos/Dashboard.impl"


namespace epi {


void Dashboard::ensure() {
    ensure_dir(root_);
}


void Dashboard::push_page(DashboardPage page, const PipelineStats& stats) {
    ensure();
    last_.push_back(std::move(page));
    /* Ráfaga documental: la UI debe ver la sucesión real de páginas útiles, no
       una muestra de dos hojas. La cola se mantiene acotada y lineal para que
       el dashboard siga siendo telemetría ligera, sin crecer con el boletín. */
    while (last_.size() > 48) last_.erase(last_.begin());
    write(stats);
}


void Dashboard::write(const PipelineStats& stats) {
    const fs::path ix = index_path();
    ensure_dir(ix.parent_path());

    /*
       Vista previa nativa: se conserva la telemetría de inspección en IXIPTLAH,
       sin generar un runtime documental paralelo. El payload es lineal para que
       el lector pueda saltarlo por tipo/esquema o reconstruir el último estado
       con lecturas secuenciales, sin árboles de marcado ni hojas de estilo.
    */
    (void)ixiptlah_write_single_record_atomic(ix, IxiptlahRecordType::LivePreview, 2, [&](std::ostream& out) {
        if (!ixiptlah_write_string(out, now_utc_iso()) ||
            !ixiptlah_write_value(out, stats.pdf_total) ||
            !ixiptlah_write_value(out, stats.pdf_done) ||
            !ixiptlah_write_value(out, stats.pages_total) ||
            !ixiptlah_write_value(out, stats.pages_done) ||
            !ixiptlah_write_value(out, stats.detail_total) ||
            !ixiptlah_write_value(out, stats.detail_done) ||
            !ixiptlah_write_value(out, stats.progress) ||
            !ixiptlah_write_value(out, stats.pages_with_tables) ||
            !ixiptlah_write_value(out, stats.tables_detected) ||
            !ixiptlah_write_value(out, stats.observations_accepted) ||
            !ixiptlah_write_value(out, stats.quarantine_items) ||
            !ixiptlah_write_string(out, stats.current_pdf) ||
            !ixiptlah_write_value(out, stats.current_page) ||
            !ixiptlah_write_string(out, stats.status)) return false;

        const std::uint32_t page_count = static_cast<std::uint32_t>(std::min<std::size_t>(last_.size(), 48u));
        if (!ixiptlah_write_value(out, page_count)) return false;

        for (std::uint32_t i = 0; i < page_count; ++i) {
            const DashboardPage& page = last_[static_cast<std::size_t>(i)];
            const std::uint32_t table_count = static_cast<std::uint32_t>(std::min<std::size_t>(page.tables.size(), 4096u));
            if (!ixiptlah_write_string(out, page.pdf_file) ||
                !ixiptlah_write_value(out, page.page) ||
                !ixiptlah_write_value(out, page.page_width) ||
                !ixiptlah_write_value(out, page.page_height) ||
                !ixiptlah_write_string(out, path_utf8(page.image_path)) ||
                !ixiptlah_write_value(out, table_count)) return false;

            for (std::uint32_t j = 0; j < table_count; ++j) {
                const TableCandidate& table = page.tables[static_cast<std::size_t>(j)];
                const std::uint32_t row_count = static_cast<std::uint32_t>(std::min<std::size_t>(table.rows.size(), 1000000u));
                const std::uint32_t column_count = static_cast<std::uint32_t>(std::min<std::size_t>(table.columns.size(), 1000000u));
                const std::uint32_t accepted_count = static_cast<std::uint32_t>(std::min<std::size_t>(table.accepted.size(), 1000000u));
                const std::uint32_t quarantine_count = static_cast<std::uint32_t>(std::min<std::size_t>(table.quarantine.size(), 1000000u));
                if (!ixiptlah_write_string(out, table.table_id) ||
                    !ixiptlah_write_value(out, row_count) ||
                    !ixiptlah_write_value(out, column_count) ||
                    !ixiptlah_write_value(out, accepted_count) ||
                    !ixiptlah_write_value(out, quarantine_count)) return false;
            }
        }

        return true;
    });
}

}

// ===== Nucleos/EpiTypes.impl =====
#line 1 "Nucleos/EpiTypes.impl"






namespace epi {


namespace {

template <typename Fn>
std::string rect_c_format(Fn fn) {
    char stack[160];
    size_t n = fn(stack, sizeof(stack));
    if (n < sizeof(stack)) return std::string(stack, n);
    std::string out(n + 1, '\0');
    n = fn(out.data(), out.size());
    out.resize(n);
    return out;
}

}



std::string rect_to_json(const Rect& r) {
    return rect_c_format([&](char* out, size_t cap) {
        return ozmvm_rect_json_copy(r.x0, r.y0, r.x1, r.y1, out, cap);
    });
}




std::string rect_to_csv(const Rect& r) {
    return rect_c_format([&](char* out, size_t cap) {
        return ozmvm_rect_csv_copy(r.x0, r.y0, r.x1, r.y1, out, cap);
    });
}

}

// ===== Nucleos/ExternalTools.impl =====
#line 1 "Nucleos/ExternalTools.impl"




#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <thread>
#include <mutex>
#include <atomic>



namespace epi {



namespace {




bool executable_exists(const fs::path& p) {

    if (p.empty()) return false;
    std::error_code ec;

    if (fs::is_regular_file(p, ec) && !ec) return true;
#ifdef _WIN32

    const DWORD attr = GetFileAttributesW(p.wstring().c_str());


    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else

    return false;
#endif
}




int env_int_clamped(const char* name, int fallback, int lo, int hi) {

    const std::string raw = getenv_utf8_or_empty(name);

    if (raw.empty()) return std::clamp(fallback, lo, hi);
    try { return std::clamp(std::stoi(raw), lo, hi); } catch (...) { return std::clamp(fallback, lo, hi); }
}



int adaptive_compute_worker_budget() {
    const unsigned hw_raw = std::thread::hardware_concurrency();
    const int hw = static_cast<int>(hw_raw == 0 ? 2u : hw_raw);
    int budget = std::clamp((hw + 1) / 2, 1, 8);
#ifdef _WIN32
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        constexpr std::uint64_t gib = 1024ull * 1024ull * 1024ull;
        int memory_budget = 8;
        if (memory.ullTotalPhys <= 4ull * gib) memory_budget = 1;
        else if (memory.ullTotalPhys <= 8ull * gib) memory_budget = 3;
        else if (memory.ullTotalPhys <= 16ull * gib) memory_budget = 5;
        if (memory.ullAvailPhys < gib) memory_budget = 1;
        else if (memory.ullAvailPhys < 2ull * gib) memory_budget = std::min(memory_budget, 2);
        else if (memory.dwMemoryLoad >= 85) memory_budget = std::min(memory_budget, 2);
        budget = std::min(budget, memory_budget);
    }
#endif
    return std::max(1, budget);
}



bool env_flag_enabled(const char* name) {
    const std::string raw = getenv_utf8_or_empty(name);

    if (raw.empty()) return false;

    const std::string v = lower_ascii(trim(raw));

    return v == "1" || v == "true" || v == "on" || v == "yes" || v == "si";
}





int poppler_process_budget() {
    const int fallback = adaptive_compute_worker_budget();
    return env_int_clamped("TLALPOWA_POPPLER_MAX_PROCESSES", fallback, 1, fallback);
}





class ExternalProcessSlot {
public:


    explicit ExternalProcessSlot(int max_active) : max_active_(std::max(1, max_active)) {
        int observed = active_.load();

        for (;;) {

            observed = active_.load();

            if (observed < max_active_ && active_.compare_exchange_weak(observed, observed + 1)) break;


            std::this_thread::sleep_for(std::chrono::milliseconds(18));
        }
    }

    ~ExternalProcessSlot() { active_.fetch_sub(1); }
    ExternalProcessSlot(const ExternalProcessSlot&) = delete;
    ExternalProcessSlot& operator=(const ExternalProcessSlot&) = delete;
private:
    int max_active_ = 1;


    static std::atomic<int> active_;
};



std::atomic<int> ExternalProcessSlot::active_{0};



std::string preview_render_extension() {
    const std::string raw = getenv_utf8_or_empty("TLALPOWA_PREVIEW_FORMAT");

    const std::string v = raw.empty() ? std::string{} : lower_ascii(trim(raw));

    return v == "png" ? ".png" : ".jpg";
}



bool preview_render_jpeg() {

    return preview_render_extension() == ".jpg";
}





std::vector<fs::path> split_path_env() {

    std::vector<fs::path> out;

    const std::string text = getenv_utf8_or_empty("PATH");

    if (text.empty()) return out;
#ifdef _WIN32
    const char sep = ';';

#else
    const char sep = ':';
#endif
    std::stringstream ss(text);
    std::string part;

    while (std::getline(ss, part, sep)) {

        part = trim(part);

        if (!part.empty()) out.emplace_back(widen_utf8(part));
    }

    return out;
}



std::vector<fs::path> system_dependency_roots() {

    std::vector<fs::path> roots;

    auto add_root = [&](const fs::path& root) {

        if (root.empty()) return;


        const fs::path normalized = root.lexically_normal();


        const auto key = path_utf8(normalized);

        for (const auto& existing : roots) {

            if (path_utf8(existing.lexically_normal()) == key) return;
        }


        roots.push_back(normalized);
    };



    add_root(executable_dir() / "core" / "Dependencias");
    add_root(executable_dir() / "Dependencias");
    const std::string deps_root = getenv_utf8_or_empty("TLALPOWA_DEPS_ROOT");

    if (!deps_root.empty()) {

        const std::string cleaned = trim(deps_root);


        if (!cleaned.empty()) add_root(fs::path(widen_utf8(cleaned)));
    }
#ifdef _WIN32

    add_root(fs::path(L"C:/ProgramData/Miausoft/deps"));

    add_root(fs::path(L"C:/ProgramData/Tlalpowa/Dependencias"));



    add_root(fs::path(L"C:/ProgramData/Observatorio-ZMVM/Dependencias"));
#endif

    return roots;
}



fs::path system_dependency_root() {

    const auto roots = system_dependency_roots();

    return roots.empty() ? fs::path{} : roots.front();
}

}





fs::path ExternalTools::resolve_tool_path(const fs::path& configured, const std::string& exe_name) const {
#ifdef _WIN32
    const std::string bare = exe_name;
#else
    std::string bare = exe_name;


    if (bare.size() > 4 && lower_ascii(bare.substr(bare.size() - 4)) == ".exe") bare.resize(bare.size() - 4);
#endif

    std::vector<fs::path> candidates;

    if (!configured.empty()) candidates.push_back(configured);

    if (!configured.empty() && !configured.has_parent_path()) {


        for (const auto& d : split_path_env()) candidates.push_back(d / configured);
    }

    for (const auto& deps_root : system_dependency_roots()) {

        candidates.push_back(deps_root / "poppler" / "Library" / "bin" / exe_name);


        candidates.push_back(deps_root / "poppler" / "bin" / exe_name);

        candidates.push_back(deps_root / "bin" / exe_name);
    }


    for (const auto& d : split_path_env()) {

        candidates.push_back(d / bare);
#ifdef _WIN32

        candidates.push_back(d / exe_name);
#endif
    }


    for (const auto& c : candidates) {

        if (executable_exists(c)) return c;
    }

    return configured.empty() ? fs::path(widen_utf8(bare)) : configured;
}





void ExternalTools::validate() const {


    const fs::path pdftotext = resolve_tool_path(options_.pdftotext, "pdftotext.exe");


    if (!executable_exists(pdftotext)) {


        throw std::runtime_error("No se encontro pdftotext.exe para extraer boletines PDF. Busque en TLALPOWA_DEPS_ROOT, C:/ProgramData/Tlalpowa/Dependencias, C:/ProgramData/Observatorio-ZMVM/Dependencias y PATH. Ruta intentada: " + path_utf8(pdftotext));
    }


}




fs::path ExternalTools::external_work_root() const {


    return fs::temp_directory_path() / "Tlalpowa" / "external_work";
}





fs::path ExternalTools::stage_pdf_ascii(const fs::path& pdf, const std::string& purpose) const {


    const std::string id = simple_hash_hex(path_utf8(pdf) + "|" + purpose + "|" + std::to_string(file_size_or_zero(pdf)));


    const fs::path dir = external_work_root() / safe_filename(id);
    ensure_dir(dir);


    const fs::path staged = dir / "input.pdf";
    std::error_code ec;




    static std::mutex stage_mu;


    std::lock_guard<std::mutex> lk(stage_mu);


    if (!fs::exists(staged) || file_size_or_zero(staged) != file_size_or_zero(pdf)) {


        const fs::path tmp = dir / ("input_" + simple_hash_hex(path_utf8(pdf) + now_utc_iso()) + ".tmp");


        copy_file_overwrite(pdf, tmp, ec);


        if (ec) throw std::runtime_error("No se pudo preparar copia ASCII temporal para Poppler: " + ec.message());
        fs::rename(tmp, staged, ec);

        if (ec) {


            copy_file_overwrite(tmp, staged, ec);
            std::error_code rm_ec;
            fs::remove(tmp, rm_ec);
        }


        if (ec) throw std::runtime_error("No se pudo publicar copia ASCII temporal para Poppler: " + ec.message());
    }

    return staged;
}





int ExternalTools::pdf_page_count(const fs::path& pdf) const {


    const fs::path pdfinfo = resolve_tool_path(fs::path{}, "pdfinfo.exe");


    if (!executable_exists(pdfinfo)) return 0;


    const fs::path staged_pdf = stage_pdf_ascii(pdf, "shared");


    std::vector<std::string> args = {path_utf8(pdfinfo), path_utf8(staged_pdf)};




    const int timeout_ms = env_int_clamped("TLALPOWA_PDFINFO_TIMEOUT_MS", 2500, 600, 20000);

    auto pr = run_command_timed(args, timeout_ms);

    if (pr.exit_code != 0 || pr.captured_output.empty()) return 0;

    std::istringstream in(pr.captured_output);

    std::string line;

    while (std::getline(in, line)) {


        std::string norm = normalize_key(line);

        if (norm.find("pages:") == 0 || norm.find("paginas:") == 0) {
            int pages = 0;
            bool in_digits = false;
            for (const unsigned char ch : line) {
                if (ch >= static_cast<unsigned char>('0') && ch <= static_cast<unsigned char>('9')) {
                    in_digits = true;
                    pages = pages * 10 + static_cast<int>(ch - static_cast<unsigned char>('0'));
                    if (pages > 1000000) return 0;
                } else if (in_digits) {
                    break;
                }
            }
            if (in_digits) return std::max(0, pages);
        }
    }

    return 0;
}





ProcessResult ExternalTools::run_pdftotext_bbox(const fs::path& pdf, const fs::path& out_html, int first_page, int last_page) const {

    ensure_dir(out_html.parent_path());


    const fs::path staged_pdf = stage_pdf_ascii(pdf, "shared");


    const std::string tmp_id = simple_hash_hex(path_utf8(pdf) + "|bbox|" + std::to_string(first_page) + "|" + std::to_string(last_page) + "|" + path_utf8(out_html));


    const fs::path tmp_html = staged_pdf.parent_path() / ("bbox_f" + std::to_string(std::max(1, first_page)) + "_l" + std::to_string(std::max(0, last_page)) + "_" + tmp_id + ".html");


    const fs::path pdftotext = resolve_tool_path(options_.pdftotext, "pdftotext.exe");
    std::error_code ec;
    fs::remove(tmp_html, ec);


    first_page = std::max(1, first_page);

    last_page = std::max(0, last_page);



    const int timeout_ms = env_int_clamped("TLALPOWA_BBOX_PROCESS_TIMEOUT_MS", 18000, 2500, 180000);


    auto run_mode = [&](const std::string& mode) {

        std::vector<std::string> args = {


            path_utf8(pdftotext),
            "-enc", "UTF-8"
        };

        if (first_page > 1) {

            args.push_back("-f");

            args.push_back(std::to_string(first_page));
        }

        if (last_page >= first_page) {

            args.push_back("-l");

            args.push_back(std::to_string(last_page));
        }

        args.push_back(mode);


        args.push_back(path_utf8(staged_pdf));

        args.push_back(path_utf8(tmp_html));

        return run_command_timed(args, timeout_ms);
    };


    auto pr = run_mode("-bbox-layout");


    if (pr.exit_code != 0 || !fs::exists(tmp_html) || file_size_or_zero(tmp_html) == 0) {


        ProcessResult first = pr;
        fs::remove(tmp_html, ec);

        pr = run_mode("-bbox");

        if (pr.exit_code != 0 && !first.captured_output.empty()) {

            pr.captured_output = "Primer intento -bbox-layout:\n" + first.captured_output + "\nSegundo intento -bbox:\n" + pr.captured_output;
        }
    }

    if (pr.exit_code == 0 && fs::exists(tmp_html)) {


        fs::remove(out_html, ec);
        ec.clear();
        fs::rename(tmp_html, out_html, ec);
        if (ec) {
            ec.clear();
            copy_file_overwrite(tmp_html, out_html, ec);
            std::error_code cleanup_ec;
            fs::remove(tmp_html, cleanup_ec);
        }

        if (ec) {
            pr.exit_code = 9001;


            pr.captured_output += "\nNo se pudo copiar XHTML temporal a salida real: " + ec.message();
        }

    } else if (pr.exit_code != 0 && !pr.captured_output.empty()) {
        pr.command_for_log += "\nSTDERR/STDOUT: " + pr.captured_output;
    }

    return pr;
}





ProcessResult ExternalTools::run_pdftotext_layout(const fs::path& pdf, const fs::path& out_text, int first_page, int last_page) const {

    ensure_dir(out_text.parent_path());


    const fs::path staged_pdf = stage_pdf_ascii(pdf, "shared");


    const std::string tmp_id = simple_hash_hex(path_utf8(pdf) + "|layout|" + std::to_string(first_page) + "|" + std::to_string(last_page) + "|" + path_utf8(out_text));


    const fs::path tmp_text = staged_pdf.parent_path() / ("layout_f" + std::to_string(std::max(1, first_page)) + "_l" + std::to_string(std::max(0, last_page)) + "_" + tmp_id + ".txt");


    const fs::path pdftotext = resolve_tool_path(options_.pdftotext, "pdftotext.exe");
    std::error_code ec;
    fs::remove(tmp_text, ec);

    first_page = std::max(1, first_page);
    last_page = std::max(0, last_page);


    const int timeout_ms = env_int_clamped("TLALPOWA_LAYOUT_PROCESS_TIMEOUT_MS", 22000, 2500, 240000);


    auto run_mode = [&](const std::string& mode) {


        std::vector<std::string> args = { path_utf8(pdftotext), "-enc", "UTF-8" };

        if (first_page > 1) { args.push_back("-f"); args.push_back(std::to_string(first_page)); }

        if (last_page >= first_page) { args.push_back("-l"); args.push_back(std::to_string(last_page)); }

        if (!mode.empty()) args.push_back(mode);


        args.push_back(path_utf8(staged_pdf));

        args.push_back(path_utf8(tmp_text));


        return run_command_timed(args, timeout_ms);
    };


    ProcessResult pr = run_mode("-layout");


    if (pr.exit_code != 0 || !fs::exists(tmp_text) || file_size_or_zero(tmp_text) == 0) {
        ProcessResult first = pr;
        fs::remove(tmp_text, ec);
        pr = run_mode("-raw");

        if (pr.exit_code != 0 && !first.captured_output.empty()) {

            pr.captured_output = "Primer intento -layout:\n" + first.captured_output + "\nSegundo intento -raw:\n" + pr.captured_output;
        }
    }

    if (pr.exit_code == 0 && fs::exists(tmp_text)) {


        copy_file_overwrite(tmp_text, out_text, ec);

        if (ec) {
            pr.exit_code = 9003;

            pr.captured_output += "\nNo se pudo copiar TXT temporal a salida real: " + ec.message();
        }

    } else if (pr.exit_code != 0 && !pr.captured_output.empty()) {
        pr.command_for_log += "\nSTDERR/STDOUT: " + pr.captured_output;
    }

    return pr;
}





ProcessResult ExternalTools::run_pdftoppm_pages_png(const fs::path& pdf, int first_page, int last_page, const fs::path& out_prefix_no_ext) const {

    ensure_dir(out_prefix_no_ext.parent_path());
    first_page = std::max(1, first_page);
    last_page = std::max(first_page, last_page);



    const fs::path staged_pdf = stage_pdf_ascii(pdf, "shared");

    const int scale_to = env_int_clamped("TLALPOWA_RENDER_SCALE_TO", 430, 240, 1400);


    const int timeout_ms = env_int_clamped("TLALPOWA_RENDER_PROCESS_TIMEOUT_MS", 4200, 1000, 30000);




    const fs::path tmp_prefix = staged_pdf.parent_path() /

        ("preview_batch_f" + std::to_string(first_page) + "_l" + std::to_string(last_page));

    std::error_code ec;

    const std::string tmp_name = path_utf8(tmp_prefix.filename());


    for (const auto& e : fs::directory_iterator(staged_pdf.parent_path(), ec)) {

        if (ec) break;

        const std::string fn = path_utf8(e.path().filename());

        if (fn.find(tmp_name) == 0 && (e.path().extension() == ".png" || e.path().extension() == ".jpg" || e.path().extension() == ".jpeg")) {

            fs::remove(e.path(), ec);
        }
    }



    auto normalize_outputs = [&](const fs::path& prefix) -> int {

        int copied = 0;

        const std::string prefix_name = path_utf8(prefix.filename());

        auto parse_page_suffix = [](const std::string& suffix) -> int {
            if (suffix.size() < 6u || suffix[0] != '-') return 0;
            size_t i = 1u;
            int page = 0;
            bool saw_digit = false;
            while (i < suffix.size() && suffix[i] >= '0' && suffix[i] <= '9') {
                saw_digit = true;
                page = page * 10 + static_cast<int>(suffix[i] - '0');
                if (page > 1000000) return 0;
                ++i;
            }
            if (!saw_digit || i >= suffix.size() || suffix[i] != '.') return 0;
            const char* e = suffix.c_str() + i + 1u;
            const size_t en = suffix.size() - i - 1u;
            auto lo = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; };
            if (en == 3u && lo(e[0]) == 'p' && lo(e[1]) == 'n' && lo(e[2]) == 'g') return page;
            if (en == 3u && lo(e[0]) == 'j' && lo(e[1]) == 'p' && lo(e[2]) == 'g') return page;
            if (en == 4u && lo(e[0]) == 'j' && lo(e[1]) == 'p' && lo(e[2]) == 'e' && lo(e[3]) == 'g') return page;
            return 0;
        };
        std::error_code ec2;

        for (const auto& e : fs::directory_iterator(prefix.parent_path(), ec2)) {

            if (ec2) break;

            const std::string fn = path_utf8(e.path().filename());

            if (fn.find(prefix_name) != 0) continue;
            const std::string suffix = fn.substr(prefix_name.size());
            int out_page = parse_page_suffix(suffix);



            if (out_page > 0 && out_page < first_page && first_page + out_page - 1 <= last_page) {
                out_page = first_page + out_page - 1;
            }

            if (out_page <= 0 && first_page == last_page) out_page = first_page;

            if (out_page < first_page || out_page > last_page) continue;

            fs::path dest = out_prefix_no_ext;
            dest += "-" + std::to_string(out_page) + preview_render_extension();


            copy_file_overwrite(e.path(), dest, ec2);

            if (!ec2) ++copied;
        }

        return copied;
    };


    ProcessResult last_error{9002, "renderizador de PDF no disponible", ""};




    const fs::path pdftocairo = resolve_tool_path(fs::path{}, "pdftocairo.exe");


    if (executable_exists(pdftocairo) && !env_flag_enabled("TLALPOWA_RENDER_DISABLE_CAIRO")) {

        std::vector<std::string> args = {


            path_utf8(pdftocairo),
            preview_render_jpeg() ? "-jpeg" : "-png",
            "-antialias", env_flag_enabled("TLALPOWA_RENDER_HIGH_AA") ? "best" : "none",

            "-scale-to", std::to_string(scale_to),
            "-f", std::to_string(first_page),
            "-l", std::to_string(last_page),


            path_utf8(staged_pdf),


            path_utf8(tmp_prefix)
        };

        if (preview_render_jpeg()) {

            args.insert(args.begin() + 2, "-jpegopt");


            args.insert(args.begin() + 3, "quality=" + std::to_string(env_int_clamped("TLALPOWA_RENDER_JPEG_QUALITY", 38, 25, 92)) + ",optimize=n,progressive=n");
        }

        if (env_flag_enabled("TLALPOWA_RENDER_GRAY")) args.insert(args.begin() + 2, "-gray");

        auto pr = run_command_timed(args, timeout_ms);


        const int copied = normalize_outputs(tmp_prefix);

        if (copied > 0) {

            if (pr.exit_code != 0) pr.command_for_log += " [salida parcial util copiada pese a codigo " + std::to_string(pr.exit_code) + "]";
            pr.exit_code = 0;

            return pr;
        }
        last_error = pr;
    }



    const fs::path pdftoppm = resolve_tool_path(options_.pdftoppm, "pdftoppm.exe");


    if (executable_exists(pdftoppm)) {

        std::vector<std::string> args = {


            path_utf8(pdftoppm),
            preview_render_jpeg() ? "-jpeg" : "-png",
            "-aa", env_flag_enabled("TLALPOWA_RENDER_HIGH_AA") ? "yes" : "no",
            "-aaVector", env_flag_enabled("TLALPOWA_RENDER_HIGH_AA") ? "yes" : "no",

            "-scale-to", std::to_string(scale_to),
            "-f", std::to_string(first_page),
            "-l", std::to_string(last_page),


            path_utf8(staged_pdf),

            path_utf8(tmp_prefix)
        };

        if (preview_render_jpeg()) {

            args.insert(args.begin() + 2, "-jpegopt");


            args.insert(args.begin() + 3, "quality=" + std::to_string(env_int_clamped("TLALPOWA_RENDER_JPEG_QUALITY", 38, 25, 92)) + ",optimize=n,progressive=n");
        }

        if (env_flag_enabled("TLALPOWA_RENDER_GRAY")) args.insert(args.begin() + 2, "-gray");

        auto pr = run_command_timed(args, timeout_ms);


        int copied = normalize_outputs(tmp_prefix);

        if (copied > 0) {

            if (pr.exit_code != 0) pr.command_for_log += " [salida parcial util copiada pese a codigo " + std::to_string(pr.exit_code) + "]";
            pr.exit_code = 0;

            return pr;
        }

        if (pr.exit_code != 0) {

            std::vector<std::string> args_dpi = {


                path_utf8(pdftoppm),
                preview_render_jpeg() ? "-jpeg" : "-png",
                "-aa", env_flag_enabled("TLALPOWA_RENDER_HIGH_AA") ? "yes" : "no",
                "-aaVector", env_flag_enabled("TLALPOWA_RENDER_HIGH_AA") ? "yes" : "no",
                "-r", std::to_string(std::clamp(options_.render_dpi, 32, 220)),
                "-f", std::to_string(first_page),
                "-l", std::to_string(last_page),


                path_utf8(staged_pdf),


                path_utf8(tmp_prefix)
            };

            if (preview_render_jpeg()) {

                args_dpi.insert(args_dpi.begin() + 2, "-jpegopt");


                args_dpi.insert(args_dpi.begin() + 3, "quality=" + std::to_string(env_int_clamped("TLALPOWA_RENDER_JPEG_QUALITY", 38, 25, 92)) + ",optimize=n,progressive=n");
            }

            if (env_flag_enabled("TLALPOWA_RENDER_GRAY")) args_dpi.insert(args_dpi.begin() + 2, "-gray");

            const ProcessResult first_try = pr;

            pr = run_command_timed(args_dpi, timeout_ms);


            copied = normalize_outputs(tmp_prefix);

            if (copied > 0) {

                if (pr.exit_code != 0) pr.command_for_log += " [salida parcial util copiada pese a codigo " + std::to_string(pr.exit_code) + "]";
                pr.exit_code = 0;

                return pr;
            }

            if (pr.exit_code != 0 && !first_try.captured_output.empty()) {

                pr.captured_output = "Primer intento -scale-to:\n" + first_try.captured_output + "\nSegundo intento -r:\n" + pr.captured_output;
            }
        }
        last_error = pr;
    }


    if (!last_error.captured_output.empty()) last_error.command_for_log += "\nSTDERR/STDOUT: " + last_error.captured_output;

    return last_error;
}





ProcessResult ExternalTools::run_pdftoppm_page_png(const fs::path& pdf, int page, const fs::path& out_prefix_no_ext) const {




    return run_pdftoppm_pages_png(pdf, page, page, out_prefix_no_ext);
}




std::string ExternalTools::quote_for_log(const std::string& s) {

    if (s.find_first_of(" \t\"'&()[]{}áéíóúÁÉÍÓÚñÑ") == std::string::npos) return s;
    std::string out = "\"";


    for (char c : s) {

        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";

    return out;
}




ProcessResult ExternalTools::run_command(const std::vector<std::string>& args) const {

    return run_command_timed(args, 0);
}





ProcessResult ExternalTools::run_command_timed(const std::vector<std::string>& args, int timeout_ms) const {

    if (args.empty()) return {-1, "", ""};



    ExternalProcessSlot process_slot(poppler_process_budget());
    std::ostringstream log;

    for (size_t i = 0; i < args.size(); ++i) {

        if (i) log << ' ';
        log << quote_for_log(args[i]);
    }
    const std::string command_log = log.str();
#ifdef _WIN32
    std::wstring cmdline;

    for (size_t i = 0; i < args.size(); ++i) {

        if (i) cmdline += L" ";
        std::wstring w = widen_utf8(args[i]);
        cmdline += L"\"";

        for (wchar_t c : w) {


            if (c == L'\"') cmdline += L"\\\"";
            else cmdline += c;
        }
        cmdline += L"\"";
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;

    HANDLE writePipe = nullptr;
    std::string captured;

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {

        return {static_cast<int>(GetLastError()), command_log + " [CreatePipe failed]", ""};
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};

    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    si.hStdOutput = writePipe;

    si.hStdError = writePipe;

    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);


    std::vector<wchar_t> mutable_cmd(cmdline.begin(), cmdline.end());

    mutable_cmd.push_back(L'\0');

    DWORD priority_flags = CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS;

    if (env_flag_enabled("TLALPOWA_POPPLER_HIGH_PRIORITY")) priority_flags = CREATE_NO_WINDOW | HIGH_PRIORITY_CLASS;

    BOOL ok = CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, priority_flags, nullptr, nullptr, &si, &pi);

    CloseHandle(writePipe);

    if (!ok) {
        const DWORD err = GetLastError();

        CloseHandle(readPipe);


        return {static_cast<int>(err), command_log + " [CreateProcessW failed]", ""};
    }


    const auto t0 = std::chrono::steady_clock::now();

    bool timed_out = false;
    char buffer[4096];

    for (;;) {



        if (timeout_ms > 0 &&

            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() >= timeout_ms) {

            timed_out = true;

            TerminateProcess(pi.hProcess, 1460);
            WaitForSingleObject(pi.hProcess, 500);
            break;
        }

        DWORD available = 0;

        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {

            DWORD read = 0;

            if (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &read, nullptr) && read > 0) {


                captured.append(buffer, buffer + read);



                if (captured.size() > 65536) captured.erase(0, captured.size() - 65536);
            }
        } else {
            DWORD code = STILL_ACTIVE;

            GetExitCodeProcess(pi.hProcess, &code);

            if (code != STILL_ACTIVE) break;
            Sleep(12);
        }
    }
    DWORD code = 0;

    GetExitCodeProcess(pi.hProcess, &code);


    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CloseHandle(readPipe);

    if (timed_out) {

        return {1460, command_log + " [timeout " + std::to_string(timeout_ms) + " ms; proceso terminado para no bloquear importacion]", captured};
    }

    return {static_cast<int>(code), command_log, captured};
#else
    int code = std::system(command_log.c_str());

    return {code, command_log, ""};
#endif
}

}

// ===== Nucleos/AtmosphereModel.impl =====
#line 1 "Nucleos/AtmosphereModel.impl"






#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <unordered_map>

#include <nlohmann/json.hpp>



namespace epi {


namespace {




std::string extension_norm(const fs::path& p) {


    return lower_ascii(path_utf8(p.extension()));
}



fs::path atmosphere_runtime_dir() {

    if (auto p = getenv_path_utf8("LOCALAPPDATA"); !p.empty()) {
        return p / "MiausoftSuite" / "Tlalpowa" / "runtime" / "atmosfera";
    }
    return project_root() / "core" / "Runtime" / "atmosfera";
}




std::string iso_ymd_string_atmosphere(int y, int m, int d) {
    std::ostringstream os;

    os << std::setw(4) << std::setfill('0') << y << '-'

       << std::setw(2) << std::setfill('0') << m << '-'

       << std::setw(2) << std::setfill('0') << d;


    return os.str();
}



int year_from_any_text(const std::string& text) {

    for (size_t i = 0; i + 3 < text.size(); ++i) {

        if (!std::isdigit(static_cast<unsigned char>(text[i])) ||
            !std::isdigit(static_cast<unsigned char>(text[i + 1])) ||
            !std::isdigit(static_cast<unsigned char>(text[i + 2])) ||
            !std::isdigit(static_cast<unsigned char>(text[i + 3]))) continue;
        const int y = std::stoi(text.substr(i, 4));

        if (y >= 1800 && y <= 2300) return y;
    }

    return 0;
}




bool is_supported_source(const fs::path& p) {
    const std::string ext = extension_norm(p);




    return ext == ".csv" || ext == ".tsv" || ext == ".txt" || ext == ".json" || ext == ".jsonl" || ext == ".geojson" ||
           ext == ".nc" || ext == ".nc4" || ext == ".cdf" ||
           ext == ".hdf" || ext == ".h5" || ext == ".hdf5" || ext == ".he5" || ext == ".he4" ||
           ext == ".grib" || ext == ".grib2" || ext == ".grb" || ext == ".grb2" ||
           ext == ".tif" || ext == ".tiff" || ext == ".geotiff" ||

           ext == ".shp" || ext == ".shx" || ext == ".dbf" || ext == ".prj" || ext == ".cpg" ||

           ext == ".gpkg" || ext == ".gdb" || ext == ".kml" || ext == ".kmz" ||
           ext == ".zip" || ext == ".xlsx" || ext == ".xls" || ext == ".ods" ||
           ext == ".parquet" || ext == ".feather" || ext == ".pbf" || ext == ".pdf";
}

bool is_atmosphere_inventory_only_source(const fs::path& p) {
    const std::string name = normalize_key(path_utf8(p.filename()));
    const std::string ext = extension_norm(p);

    if (contains_norm(name, " coverage") || contains_norm(name, " catalog") ||
        contains_norm(name, " granules ") || contains_norm(name, " manifest") ||
        contains_norm(name, " audit")) {
        return ext == ".csv" || ext == ".tsv" || ext == ".txt" ||
               ext == ".json" || ext == ".jsonl" || ext == ".geojson";
    }
    return false;
}

bool is_satellite_coverage_table(const fs::path& p) {
    const std::string ext = extension_norm(p);
    if (ext != ".csv" && ext != ".tsv" && ext != ".txt") return false;

    const std::string name = normalize_key(path_utf8(p.filename()));
    if (!contains_norm(name, "coverage")) return false;

    const std::string full = normalize_key(path_utf8(p));
    return contains_norm(full, "satelital") || contains_norm(full, "satellite") ||
           contains_norm(full, "sentinel") || contains_norm(full, "tropomi") ||
           contains_norm(full, "modis") || contains_norm(full, "maiac") ||
           contains_norm(full, "aura") || contains_norm(full, "omi") ||
           contains_norm(full, "goes");
}





std::vector<std::string> split_guess_delimited(const std::string& line, char delimiter) {

    std::vector<std::string> cols;
    std::string cur;
    bool quoted = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (quoted) {

            if (c == '"') {

                if (i + 1 < line.size() && line[i + 1] == '"') { cur.push_back('"'); ++i; }
                else quoted = false;

            } else cur.push_back(c);
        } else {

            if (c == '"') quoted = true;


            else if (c == delimiter) { cols.push_back(trim(cur)); cur.clear(); }

            else cur.push_back(c);
        }
    }


    cols.push_back(trim(cur));

    return cols;
}



char infer_delimiter(const std::string& line) {

    const std::array<char, 4> candidates{',', '\t', ';', '|'};
    char best = ',';
    size_t best_count = 0;

    for (const char c : candidates) {
        const size_t n = static_cast<size_t>(std::count(line.begin(), line.end(), c));

        if (n > best_count) { best_count = n; best = c; }
    }

    return best;
}



std::string delimiter_name(char c) {

    if (c == '\t') return "tab";

    if (c == ';') return "semicolon";

    if (c == '|') return "pipe";

    return "comma";
}


bool env_flag_enabled_atmosphere(const char* name, bool fallback = false) {
    const std::string raw = getenv_utf8_or_empty(name);

    if (raw.empty()) return fallback;


    std::string v = lower_ascii(trim(raw));

    return v == "1" || v == "true" || v == "yes" || v == "si" || v == "on";
}



int env_int_clamped_atmosphere(const char* name, int fallback, int lo, int hi) {
    const std::string raw = getenv_utf8_or_empty(name);

    if (raw.empty()) return fallback;
    try {


        return std::clamp(std::stoi(trim(raw)), lo, hi);

    } catch (...) {

        return fallback;
    }
}




std::string classify_provider(const std::string& norm_path) {

    if (contains_norm(norm_path, "rama") || contains_norm(norm_path, "simat")) return "RAMA/SIMAT";

    if (contains_norm(norm_path, "redmet")) return "REDMET";

    if (contains_norm(norm_path, "redma")) return "REDMA";

    if (contains_norm(norm_path, "ruoa")) return "RUOA";

    if (contains_norm(norm_path, "pembu")) return "PEMBU";

    if (contains_norm(norm_path, "sentinel 5p") || contains_norm(norm_path, "sentinel5p") ||

        contains_norm(norm_path, "tropomi") || contains_norm(norm_path, "s5p")) return "Copernicus Sentinel-5P/TROPOMI";

    if (contains_norm(norm_path, "omi") || contains_norm(norm_path, "aura") ||

        contains_norm(norm_path, "omno2") || contains_norm(norm_path, "omaer") ||

        contains_norm(norm_path, "omto3")) return "NASA Aura/OMI";


    if (contains_norm(norm_path, "modis") || contains_norm(norm_path, "maiac") || contains_norm(norm_path, "mcd19")) return "NASA MODIS/MAIAC";

    if (contains_norm(norm_path, "goes") || contains_norm(norm_path, "abi")) return "NOAA GOES-R/ABI";

    if (contains_norm(norm_path, "viirs")) return "NOAA/NASA VIIRS";

    if (contains_norm(norm_path, "era5") || contains_norm(norm_path, "copernicus")) return "Copernicus ERA5";

    if (contains_norm(norm_path, "srtm") || contains_norm(norm_path, "dem") || contains_norm(norm_path, "relieve")) return "DEM/relieve";

    return "desconocido";
}



std::string classify_kind(const std::string& norm_path) {



    if (contains_norm(norm_path, "satelital") || contains_norm(norm_path, "satellite") ||

        contains_norm(norm_path, "sentinel") || contains_norm(norm_path, "sentinel5p") ||

        contains_norm(norm_path, "tropomi") || contains_norm(norm_path, "s5p") ||

        contains_norm(norm_path, "omi") || contains_norm(norm_path, "aura") ||

        contains_norm(norm_path, "modis") || contains_norm(norm_path, "maiac") ||

        contains_norm(norm_path, "mcd19") || contains_norm(norm_path, "goes") ||

        contains_norm(norm_path, "abi") || contains_norm(norm_path, "viirs") ||

        contains_norm(norm_path, "aod") || contains_norm(norm_path, "hdf") ||

        contains_norm(norm_path, "netcdf") || contains_norm(norm_path, "geotiff")) return "satelital";

    if (contains_norm(norm_path, "contamin") || contains_norm(norm_path, "pm25") || contains_norm(norm_path, "pm2 5") ||

        contains_norm(norm_path, "pm10") || contains_norm(norm_path, "ozono") || contains_norm(norm_path, "o3") ||


        contains_norm(norm_path, "no2") || contains_norm(norm_path, "so2") || contains_norm(norm_path, "co ")) return "contaminante";

    if (contains_norm(norm_path, "meteor") || contains_norm(norm_path, "temperatura") || contains_norm(norm_path, "viento") ||

        contains_norm(norm_path, "humedad") || contains_norm(norm_path, "radiacion") || contains_norm(norm_path, "precipit")) return "meteorologico";

    if (contains_norm(norm_path, "era5") || contains_norm(norm_path, "reanalis")) return "reanalisis";

    if (contains_norm(norm_path, "emision") || contains_norm(norm_path, "inventario")) return "emisiones";

    if (contains_norm(norm_path, "relieve") || contains_norm(norm_path, "srtm") || contains_norm(norm_path, "dem")) return "orografia";

    return "desconocido";
}




std::string infer_variable_hint(const std::string& text) {


    const std::string n = normalize_key(text);

    if (contains_norm(n, "pm2 5") || contains_norm(n, "pm25")) return "PM2.5";

    if (contains_norm(n, "pm10")) return "PM10";

    if (contains_norm(n, "ozono") || contains_norm(n, " o3") || n == "o3") return "O3";

    if (contains_norm(n, "no2")) return "NO2";

    if (contains_norm(n, "so2")) return "SO2";

    if (contains_norm(n, " monoxido") || contains_norm(n, " co") || n == "co") return "CO";

    if (contains_norm(n, "temperatura") || contains_norm(n, " tmp") || contains_norm(n, "temp")) return "temperatura";

    if (contains_norm(n, "viento") || contains_norm(n, "vel viento") || contains_norm(n, "dir viento")) return "viento";

    if (contains_norm(n, "radiacion") || contains_norm(n, "uv") || contains_norm(n, "solar")) return "radiacion_solar";


    if (contains_norm(n, "humedad")) return "humedad_relativa";

    if (contains_norm(n, "presion")) return "presion";

    if (contains_norm(n, "precipit")) return "precipitacion";

    if (contains_norm(n, "aod") || contains_norm(n, "aerosol") || contains_norm(n, "aerosoles")) return "AOD/aerosoles";

    if (contains_norm(n, "tropomi") || contains_norm(n, "sentinel") || contains_norm(n, "s5p")) return "columna_satelital_atmosferica";

    if (contains_norm(n, "omi") || contains_norm(n, "aura")) return "columna_satelital_omi";

    if (contains_norm(n, "goes") || contains_norm(n, "abi")) return "raster_satelital_auxiliar";

    return "desconocida";
}




std::string temporal_hint_from_name(const std::string& text) {
    std::smatch m;


    static const std::regex hint_re(R"(((?:19|20)[0-9]{2})[-_ ]?([0-1][0-9])?[-_ ]?([0-3][0-9])?)");
    if (std::regex_search(text, m, hint_re)) {

        return m[0].str();
    }

    return {};
}



std::string canonical_date_from_temporal_hint(const std::string& hint) {




    const std::string n = normalize_key(hint);

    std::smatch m;


    static const std::regex ymd_re(R"(((?:19|20)[0-9]{2})[^0-9]?([0-1][0-9])[^0-9]?([0-3][0-9]))");
    static const std::regex ym_re(R"(((?:19|20)[0-9]{2})[^0-9]?([0-1][0-9]))");
    static const std::regex y_re(R"(((?:19|20)[0-9]{2}))");

    if (std::regex_search(n, m, ymd_re)) {
        int y = 0, mo = 1, d = 1;
        try {
            y = std::stoi(m[1].str());
            mo = std::stoi(m[2].str());
            d = std::stoi(m[3].str());
        } catch (...) { return {}; }

        if (y < 1800 || y > 2300 || mo < 1 || mo > 12 || d < 1 || d > 31) return {};

        return iso_ymd_string_atmosphere(y, mo, d);
    }


    if (std::regex_search(n, m, ym_re)) {
        int y = 0, mo = 1;
        try {
            y = std::stoi(m[1].str());
            mo = std::stoi(m[2].str());
        } catch (...) { return {}; }

        if (y < 1800 || y > 2300 || mo < 1 || mo > 12) return {};

        return iso_ymd_string_atmosphere(y, mo, 1);
    }


    if (std::regex_search(n, m, y_re)) {
        int y = 0;

        try { y = std::stoi(m[1].str()); } catch (...) { return {}; }


        if (y < 1800 || y > 2300) return {};

        return iso_ymd_string_atmosphere(y, 1, 1);
    }

    return {};
}




long long file_time_tick(const fs::path& p) {
    std::error_code ec;

    const auto ft = fs::last_write_time(p, ec);

    if (ec) return 0;

    return static_cast<long long>(ft.time_since_epoch().count());
}





std::string compact_columns_json(const std::vector<std::string>& cols) {
    std::ostringstream out;
    out << '[';

    for (size_t i = 0; i < cols.size(); ++i) {

        if (i) out << ',';


        out << '"' << json_escape(cols[i]) << '"';
    }
    out << ']';

    return out.str();
}



struct StationMeta {
    std::string id;

    std::string name;

    double lon = 0.0;

    double lat = 0.0;
    double alt = 0.0;
    std::string entity_code;
    std::string municipality_code;
    std::string municipality_name;
};


std::string uppercase_code3(std::string value, const std::string& fallback = "ATM") {


    value = trim(value);
    std::string out;

    out.reserve(3);

    for (char c : value) {
        const unsigned char uc = static_cast<unsigned char>(c);

        if (std::isalnum(uc)) out.push_back(static_cast<char>(std::toupper(uc)));

        if (out.size() == 3) break;
    }
    std::string fb = fallback;

    for (char& c : fb) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    while (out.size() < 3 && out.size() < fb.size()) out.push_back(fb[out.size()]);


    while (out.size() < 3) out.push_back('X');

    return out;
}



void apply_station_territory_hint(StationMeta& s) {

    struct T { const char* id; const char* ent; const char* mun; const char* name; };
    static const T hints[] = {
        {"ACO","MEX","ACO","Acolman"},{"AJU","CMX","TLP","Tlalpan"},{"AJM","CMX","TLP","Tlalpan"},{"ARA","CMX","GAM","Gustavo A. Madero"},
        {"ATI","MEX","ATZ","Atizapán de Zaragoza"},{"AZC","CMX","AZC","Azcapotzalco"},{"BJU","CMX","BJU","Benito Juárez"},{"CAM","CMX","AZC","Azcapotzalco"},
        {"CCA","CMX","COY","Coyoacán"},{"CES","CMX","IZP","Iztapalapa"},{"CFE","CMX","MIH","Miguel Hidalgo"},{"CHO","MEX","CHA","Chalco"},
        {"COR","CMX","TLH","Tláhuac"},{"COY","CMX","COY","Coyoacán"},{"CUA","CMX","CUA","Cuajimalpa"},{"CUI","CMX","AZC","Azcapotzalco"},
        {"CUT","MEX","CUT","Cuautitlán"},{"DIC","CMX","TLP","Tlalpan"},{"EAJ","CMX","TLP","Tlalpan"},{"EDL","CMX","CUA","Cuajimalpa"},

        {"FAC","MEX","NAU","Naucalpan"},{"FAN","CMX","TLP","Tlalpan"},{"GAM","CMX","GAM","Gustavo A. Madero"},{"HAN","CMX","VCA","Venustiano Carranza"},
        {"HGM","CMX","CUH","Cuauhtémoc"},{"IBM","CMX","MIH","Miguel Hidalgo"},{"IMP","CMX","GAM","Gustavo A. Madero"},{"INN","MEX","OCO","Ocoyoacac"},
        {"IZT","CMX","IZC","Iztacalco"},{"LAA","CMX","GAM","Gustavo A. Madero"},{"LAG","CMX","CUH","Cuauhtémoc"},{"LLA","MEX","ECA","Ecatepec"},
        {"LOM","CMX","MIH","Miguel Hidalgo"},{"LPR","MEX","TLA","Tlalnepantla"},{"LVI","CMX","GAM","Gustavo A. Madero"},{"MCM","CMX","CUH","Cuauhtémoc"},
        {"MER","CMX","VCA","Venustiano Carranza"},{"MGH","CMX","MIH","Miguel Hidalgo"},{"MIN","CMX","CUH","Cuauhtémoc"},{"MON","MEX","TEX","Texcoco"},
        {"MPA","CMX","MPA","Milpa Alta"},{"NET","MEX","NEZ","Nezahualcóyotl"},{"NEZ","MEX","NEZ","Nezahualcóyotl"},{"PED","CMX","COY","Coyoacán"},
        {"PER","MEX","NEZ","Nezahualcóyotl"},{"PLA","CMX","AOB","Álvaro Obregón"},{"POT","CMX","BJU","Benito Juárez"},{"SAG","MEX","ECA","Ecatepec"},
        {"SFE","CMX","CUA","Cuajimalpa"},{"SHA","CMX","MIH","Miguel Hidalgo"},{"SJA","CMX","GAM","Gustavo A. Madero"},{"SNT","CMX","MCO","Magdalena Contreras"},

        {"SUR","CMX","COY","Coyoacán"},{"TAC","CMX","MIH","Miguel Hidalgo"},{"TAH","CMX","TLH","Tláhuac"},{"TAX","CMX","COY","Coyoacán"},
        {"TEC","CMX","GAM","Gustavo A. Madero"},{"TLA","MEX","TLA","Tlalnepantla"},{"TLI","MEX","TLT","Tultitlán"},{"TPN","CMX","TLP","Tlalpan"},
        {"UAX","CMX","XOC","Xochimilco"},{"UIZ","CMX","IZP","Iztapalapa"},{"UNM","CMX","UNM","Unidad móvil"},{"VAL","CMX","AZC","Azcapotzalco"},

        {"VIF","MEX","COA","Coacalco"},{"XAL","MEX","ECA","Ecatepec"},{"XCH","CMX","XOC","Xochimilco"},{"FAR","MEX","NEZ","Nezahualcóyotl"},
        {"SAC","CMX","IZP","Iztapalapa"}
    };
    const std::string id = uppercase_code3(s.id);

    for (const auto& h : hints) {

        if (id == h.id) {
            s.entity_code = h.ent;
            s.municipality_code = h.mun;
            s.municipality_name = h.name;

            return;
        }
    }

    s.entity_code = s.lon < -99.35 || s.lon > -98.95 || s.lat > 19.60 ? "MEX" : "CMX";
    s.municipality_code = id;
    s.municipality_name = s.name;
}



void seed_builtin_station_catalog(std::map<std::string, StationMeta>& stations) {

    struct R { const char* id; const char* name; double lon; double lat; double alt; };

    static const R rows[] = {

        {"ACO","Acolman",-98.912003,19.635501,2198},{"AJU","Ajusco",-99.162611,19.154286,2942},{"AJM","Ajusco Medio",-99.207744,19.272161,2548},
        {"ARA","Aragón",-99.074549,19.470218,2200},{"ATI","Atizapan",-99.254133,19.576963,2341},{"AZC","Azcapotzalco",-99.198657,19.487728,2279},
        {"BJU","Benito Juárez",-99.159596,19.370464,2249},{"CAM","Camarones",-99.169794,19.468404,2233},{"CCA","Centro de Indexacións de la Atmósfera",-99.176111,19.326111,2294},

        {"CES","Cerro de la Estrella",-99.074678,19.334731,2219},{"CFE","Museo Tecnológico de la CFE",-99.194279,19.414393,2287},{"CHO","Chalco",-98.886088,19.266948,2253},
        {"COR","CORENA",-99.02604,19.265346,2242},{"COY","Coyoacán",-99.157101,19.350258,2260},{"CUA","Cuajimalpa",-99.291705,19.365313,2704},
        {"CUI","Cuitláhuac",-99.165849,19.469859,2255},{"CUT","Cuautitlán",-99.198602,19.722186,2263},{"DIC","Diconsa",-99.185774,19.298819,2305},
        {"EAJ","Ecoguardas Ajusco",-99.203971,19.271222,2584},{"EDL","Exconv. Desierto Leones",-99.310635,19.313357,2980},{"FAC","FES Acatlán",-99.243524,19.482473,2299},
        {"FAN","Felipe Ángeles",-99.17492,19.299126,2279},{"GAM","Gustavo A. Madero",-99.094517,19.4827,2227},{"HAN","Hangares",-99.083623,19.420518,2235},
        {"HGM","Hospital General de México",-99.152207,19.411617,2234},{"IBM","Legaria",-99.21536,19.443319,2314},{"IMP","Inst. Mexicano del Petróleo",-99.147294,19.487561,2250},
        {"INN","Investigaciones Nucleares",-99.38052,19.291968,3082},{"IZT","Iztacalco",-99.117641,19.384413,2238},{"LAA","Lab. de Analisis Ambiental",-99.147312,19.483781,2255},
        {"LAG","Lagunilla",-99.135183,19.44242,2223},{"LLA","Los Laureles",-99.039644,19.578792,2230},{"LOM","Lomas",-99.242062,19.403,2434},
        {"LPR","La Presa",-99.11772,19.534727,2302},{"LVI","La Villa",-99.117749,19.46789,2228},{"MCM","Museo de la Cd. de México",-99.131924,19.429071,2237},
        {"MER","Merced",-99.119594,19.42461,2245},{"MGH","Mguel Hidalgo",-99.20266,19.40405,2327},{"MIN","Metro Insurgentes",-99.162885,19.42144,2231},
        {"MON","Montecillo",-98.902853,19.460415,2252},{"MPA","Milpa Alta",-98.990189,19.1769,2594},{"NET","Netzahualcoyotl",-99.026119,19.42115,2230},

        {"NEZ","Nezahualcóyotl",-99.028212,19.393734,2235},{"PED","Pedregal",-99.204136,19.325146,2326},{"PER","La Perla",-98.991858,19.38286,2237},

        {"PLA","Plateros",-99.200109,19.365869,2345},{"POT","Portales",-99.145766,19.376494,2237},{"SAG","San Agustín",-99.030324,19.532968,2241},
        {"SFE","Santa fe",-99.262865,19.357357,2599},{"SHA","Secretaría de Hacienda",-99.207868,19.446203,2272},{"SJA","San Juan Aragón",-99.086095,19.452592,2258},
        {"SNT","San Nicolas Totolapan",-99.256462,19.250385,2946},{"SUR","Santa Ursula",-99.149994,19.31448,2279},{"TAC","Tacuba",-99.202455,19.453907,2275},

        {"TAH","Tlahuac",-99.010564,19.246459,2297},{"TAX","Taxqueña",-99.123204,19.335689,2242},{"TEC","Cerro del Tepeyac",-99.114229,19.487227,2265},
        {"TLA","Tlalnepantla",-99.204597,19.529077,2311},{"TLI","Tultitlán",-99.177173,19.602542,2313},{"TPN","Tlalpan",-99.184177,19.257041,2522},
        {"UAX","UAM Xochimilco",-99.103629,19.304441,2246},{"UIZ","UAM Iztapalapa",-99.07388,19.360794,2221},{"UNM","Unidad Movil",-99.147137,19.482238,0},
        {"VAL","Vallejo",-99.165702,19.522437,2248},{"VIF","Villa de las Flores",-99.09659,19.658223,2242},{"XAL","Xalostoc",-99.0824,19.525995,2160},
        {"XCH","Xochimilco",-99.118252,19.267066,2243},{"FAR","FES Aragón",-99.046176,19.473692,2230},{"SAC","Santiago Acahualtepec",-99.009381,19.34561,2293}
    };

    for (const auto& r : rows) {

        StationMeta s;

        s.id = r.id; s.name = r.name; s.lon = r.lon; s.lat = r.lat; s.alt = r.alt;
        apply_station_territory_hint(s);
        stations[s.id] = std::move(s);
    }
}




struct CloudAccumulator {
    StationMeta station;
    std::string pollutant;
    std::string unit;

    int year = 0;


    int month = 0;
    double sum = 0.0;
    double min_v = std::numeric_limits<double>::infinity();
    double max_v = -std::numeric_limits<double>::infinity();
    int64_t count = 0;



void add(double v) {
        sum += v;
        min_v = std::min(min_v, v);
        max_v = std::max(max_v, v);
        ++count;
    }
};








std::string month_start_date(int year, int month) {
    std::ostringstream os;

    os << std::setw(4) << std::setfill('0') << year << '-'


       << std::setw(2) << std::setfill('0') << std::clamp(month, 1, 12) << "-01";

    return os.str();
}



struct PackedAtmosphereBatchBuilder {

    int year = 0;


    int month = 0;

    std::vector<TemporalAtmospherePackedStation> stations;

    std::vector<std::string> pollutants;

    std::vector<std::string> units;


    std::vector<TemporalAtmospherePackedSample> samples;

    std::map<std::string, std::uint32_t> station_index;

    std::map<std::string, std::uint32_t> pollutant_index;

    std::map<std::string, std::uint32_t> unit_index;
};


std::uint32_t intern_packed_string(std::vector<std::string>& values,

                                   std::map<std::string, std::uint32_t>& index,

                                   const std::string& value) {
    const auto it = index.find(value);

    if (it != index.end()) return it->second;
    const std::uint32_t id = static_cast<std::uint32_t>(values.size());

    values.push_back(value);
    index[value] = id;

    return id;
}



std::uint32_t intern_packed_station(PackedAtmosphereBatchBuilder& batch, const StationMeta& station) {
    const std::string id = uppercase_code3(station.id);
    const auto it = batch.station_index.find(id);

    if (it != batch.station_index.end()) return it->second;
    const std::uint32_t idx = static_cast<std::uint32_t>(batch.stations.size());


    TemporalAtmospherePackedStation packed;
    packed.id = id;
    packed.name = station.name;

    packed.lon = station.lon;

    packed.lat = station.lat;
    packed.alt = station.alt;

    batch.stations.push_back(std::move(packed));
    batch.station_index[id] = idx;

    return idx;
}


void add_packed_atmosphere_sample(std::map<std::string, PackedAtmosphereBatchBuilder>& batches,

                                  int year,


                                  int month,
                                  int day,

                                  int hour,

                                  int minute,
                                  const StationMeta& station,
                                  const std::string& pollutant,
                                  const std::string& unit,

                                  double value) {


    if (year <= 0 || month < 1 || month > 12 || day < 1 || day > 31) return;


    const std::string key = std::to_string(year) + "|" + std::to_string(month);

    auto& batch = batches[key];

    if (batch.samples.empty()) {

        batch.year = year;


        batch.month = month;

        batch.samples.reserve(4096);
    }
    const std::uint32_t station_idx = intern_packed_station(batch, station);
    const std::uint32_t pollutant_idx = intern_packed_string(batch.pollutants, batch.pollutant_index, pollutant);
    const std::uint32_t unit_idx = intern_packed_string(batch.units, batch.unit_index, unit);



    TemporalAtmospherePackedSample sample;
    sample.day = static_cast<std::uint8_t>(std::clamp(day, 1, 31));

    sample.hour = static_cast<std::uint8_t>(std::clamp(hour, 0, 23));

    sample.minute = static_cast<std::uint8_t>(std::clamp(minute, 0, 59));
    sample.station_index = station_idx;
    sample.pollutant_index = pollutant_idx;
    sample.unit_index = unit_idx;

    sample.value = value;

    batch.samples.push_back(sample);
}







void push_unique_existing_dir(std::vector<fs::path>& roots, const fs::path& p) {

    if (p.empty()) return;
    std::error_code ec;

    if (!fs::exists(p, ec) || ec || !fs::is_directory(p, ec) || ec) return;


    const std::string key = normalize_key(path_utf8(fs::weakly_canonical(p, ec)));

    for (const auto& existing : roots) {
        std::error_code existing_ec;


        if (normalize_key(path_utf8(fs::weakly_canonical(existing, existing_ec))) == key) return;
    }

    roots.push_back(p);
}



bool looks_like_atmospheric_dir(const fs::path& p) {


    const std::string n = normalize_key(path_utf8(p.filename()));

    return contains_norm(n, "meteorolog") || contains_norm(n, "contaminante") ||

           contains_norm(n, "rama") || contains_norm(n, "simat") ||
           contains_norm(n, "redmet") || contains_norm(n, "redma") || contains_norm(n, "ruoa") || contains_norm(n, "pembu");
}




std::vector<fs::path> atmospheric_scan_roots(const fs::path& source_root) {

    std::vector<fs::path> roots;
    std::error_code ec;


    if (source_root.empty() || !fs::exists(source_root, ec) || ec) return roots;

    const fs::path contaminantes = source_root / L"CONTAMINANTES";

    const fs::path meteorologicos = source_root / L"METEOROL\u00d3GICOS";

    const fs::path meteorologicos_ascii = source_root / L"METEOROLOGICOS";
    /* La raíz elegida también puede contener los CSV mensuales oficiales
       directamente. Mantenerla en el conjunto desde el inicio evita que RAMA
       local quede invisible cuando además existen subcarpetas auxiliares. */
    push_unique_existing_dir(roots, source_root);
    push_unique_existing_dir(roots, contaminantes);
    push_unique_existing_dir(roots, source_root / L"Contaminantes");

    push_unique_existing_dir(roots, meteorologicos);
    push_unique_existing_dir(roots, meteorologicos_ascii);
    push_unique_existing_dir(roots, source_root / L"Meteorologicos");
    push_unique_existing_dir(roots, source_root / L"REDMET");
    push_unique_existing_dir(roots, source_root / L"REDMA");
    push_unique_existing_dir(roots, source_root / L"RUOA");
    push_unique_existing_dir(roots, source_root / L"PEMBU");

    if (roots.empty()) {
        push_unique_existing_dir(roots, source_root);

        if (looks_like_atmospheric_dir(source_root)) {

            const fs::path parent = source_root.parent_path();
            push_unique_existing_dir(roots, parent / L"CONTAMINANTES");
            push_unique_existing_dir(roots, parent / L"Contaminantes");
            push_unique_existing_dir(roots, parent / L"METEOROL\u00d3GICOS");
            push_unique_existing_dir(roots, parent / L"METEOROLOGICOS");

            push_unique_existing_dir(roots, parent / L"Meteorologicos");
            push_unique_existing_dir(roots, parent / L"REDMET");
            push_unique_existing_dir(roots, parent / L"REDMA");
            push_unique_existing_dir(roots, parent / L"RUOA");
            push_unique_existing_dir(roots, parent / L"PEMBU");
        }
    }


    return roots;
}




std::map<std::string, StationMeta> load_station_catalogs_for_cloud(const fs::path& source_root) {

    std::map<std::string, StationMeta> stations;
    seed_builtin_station_catalog(stations);

    for (const auto& root : atmospheric_scan_roots(source_root)) {


        const fs::path p = root / "cat_estacion.csv";

        if (!fs::exists(p)) continue;


        std::ifstream in(p, std::ios::binary);
        std::string line;

        while (std::getline(in, line)) {

            const auto cols = split_guess_delimited(line, ',');


            if (cols.size() < 4 || cols[0].empty() || cols[0] == "cve_estac" || contains_norm(normalize_key(cols[0]), "catalogo")) continue;
            try {
                StationMeta s;
                s.id = uppercase_code3(cols[0]);
                s.name = cols.size() > 1 ? cols[1] : cols[0];

                s.lon = std::stod(cols[2]);

                s.lat = std::stod(cols[3]);


                if (cols.size() > 4 && !cols[4].empty()) s.alt = std::stod(cols[4]);

                if (s.lon < -103.0 || s.lon > -96.0 || s.lat < 17.0 || s.lat > 22.0) continue;
                apply_station_territory_hint(s);
                stations[s.id] = std::move(s);
            } catch (...) {}
        }
    }

    return stations;
}



std::map<std::string, std::string> load_unit_catalogs_for_cloud(const fs::path& source_root) {

    std::map<std::string, std::string> units;

    for (const auto& root : atmospheric_scan_roots(source_root)) {


        const fs::path p = root / "cat_unidades.csv";

        if (!fs::exists(p)) continue;


        std::ifstream in(p, std::ios::binary);
        std::string line;

        while (std::getline(in, line)) {

            const auto cols = split_guess_delimited(line, ',');

            if (cols.size() < 2) continue;

            const std::string id = trim(cols[0]);


            const std::string key = normalize_key(id);


            if (id.empty() || key == "id unidad" || key == "catalogo") continue;


            units[id] = trim(cols[1]);
        }
    }

    return units;
}



std::string canonical_parameter_id(const std::string& value) {


    const std::string n = normalize_key(value);

    if (n == "pm2 5" || n == "pm25") return "pm25";

    if (n == "pm10") return "pm10";

    if (n == "pmco" || n == "pm10 2 5") return "pmco";

    if (n == "o3" || n == "ozono") return "o3";

    if (n == "co" || contains_norm(n, "monoxido de carbono")) return "co";

    if (n == "no2" || contains_norm(n, "dioxido de nitrogeno")) return "no2";

    if (n == "no") return "no";

    if (n == "nox" || n == "no x") return "nox";

    if (n == "so2" || contains_norm(n, "dioxido de azufre")) return "so2";

    if (n == "h2s" || contains_norm(n, "acido sulfhidrico")) return "h2s";

    if (n == "hcho" || contains_norm(n, "formaldehido") || contains_norm(n, "formaldehyde")) return "hcho";

    if (n == "nh3" || contains_norm(n, "amoniaco") || contains_norm(n, "ammonia")) return "nh3";

    if (n == "ben" || n == "benzene" || n == "benceno") return "ben";


    if (n == "tol" || n == "toluene" || n == "tolueno") return "tol";

    if (n == "xyl" || n == "xylene" || n == "xylenes" || n == "xileno" || n == "xilenos") return "xyl";

    if (n == "ch4" || n == "methane" || n == "metano") return "ch4";

    if (n == "co2" || contains_norm(n, "dioxido de carbono") || contains_norm(n, "carbon dioxide")) return "co2";

    if (n == "so4" || n == "sulfate" || n == "sulfato") return "so4";

    if (n == "no3" || n == "no3a" || n == "nitrate" || n == "nitrato") return "no3a";

    if (n == "pb" || n == "lead" || n == "plomo") return "pb";

    if (n == "cd" || n == "cadmium" || n == "cadmio") return "cd";

    if (n == "as" || n == "arsenic" || n == "arsenico") return "as";

    if (n == "ni" || n == "nickel" || n == "niquel") return "ni";

    if (n == "hg" || n == "mercury" || n == "mercurio") return "hg";

    if (n == "cr" || n == "chromium" || n == "cromo") return "cr";

    if (n == "bc" || n == "black carbon" || contains_norm(n, "carbono negro")) return "bc";

    if (n == "ec" || contains_norm(n, "carbono elemental")) return "ec";

    if (n == "oc" || contains_norm(n, "carbono organico")) return "oc";

    if (n == "aod" || contains_norm(n, "aerosol optical depth") || contains_norm(n, "profundidad optica")) return "aod";

    if (n == "uvaerosolindex" || n == "aai" || contains_norm(n, "aerosol index")) return "uvai";

    if (n == "frp" || contains_norm(n, "fire radiative power")) return "frp";

    if (n == "fire_temperature" || contains_norm(n, "fire temperature") || contains_norm(n, "temperatura subpixel de fuego")) return "fire_temperature";

    if (n == "fire_area" || contains_norm(n, "fire area") || contains_norm(n, "area subpixel de fuego")) return "fire_area";


    if (n == "brightness" || n == "bright ti4" || n == "bright_ti4" || n == "bright ti5" || n == "bright_ti5" ||
        n == "bright t31" || n == "bright_t31") return "brightness";

    if (n == "cloud_fraction" || contains_norm(n, "cloud fraction") || contains_norm(n, "fraccion nubosa")) return "cloud_fraction";

    if (n == "cloud_pressure" || contains_norm(n, "cloud pressure") || contains_norm(n, "presion de nube")) return "cloud_pressure";

    if (n == "cloud_top_temperature" || contains_norm(n, "cloud top temperature") || contains_norm(n, "temperatura de tope de nube")) return "cloud_top_temperature";

    if (n == "lst" || contains_norm(n, "land surface temperature") || contains_norm(n, "temperatura superficial")) return "lst";

    if (n == "ndvi") return "ndvi";

    if (n == "evi") return "evi";

    if (n == "albedo") return "albedo";

    if (n == "pblh" || contains_norm(n, "boundary layer") || contains_norm(n, "capa limite")) return "pblh";

    if (n == "tmp" || n == "temp" || n == "temperatura" || contains_norm(n, "temperatura")) return "tmp";

    if (n == "rh" || n == "hum rel" || n == "hum_rel" || contains_norm(n, "humedad")) return "rh";

    if (n == "wsp" || n == "rapidez v sostenido" || n == "rapidez_v_sostenido" || contains_norm(n, "velocidad del viento")) return "wsp";

    if (n == "wdr" || n == "dir v sostenido" || n == "dir_v_sostenido" || contains_norm(n, "direccion del viento")) return "wdr";

    if (n == "u10" || n == "componente zonal del viento") return "u10";

    if (n == "v10" || n == "v1" || n == "componente meridional del viento") return "v10";

    if (n == "wgst" || n == "rapidez rachas" || n == "rapidez_rachas" || n == "wind gust" || n == "gust speed") return "wgst";

    if (n == "wdr_gust" || n == "dir rachas" || n == "dir_rachas" || n == "gust direction") return "wdr_gust";

    if (n == "pba" || n == "pa" || n == "presion bar" || n == "presion_bar" || contains_norm(n, "presion")) return "pa";

    if (n == "pb" || contains_norm(n, "plomo")) return "pb";

    if (n == "gr" || contains_norm(n, "radiacion global")) return "gr";

    if (n == "uva") return "uva";

    if (n == "uvb") return "uvb";

    // Alias RUOA/PEMBU/Davis Vantage: los CSV del puente PEMBU pueden llegar
    // con encabezados en ingles compactos o con espacios; estos mapeos evitan
    // degradar la importacion a columnas desconocidas.
    if (n == "timestamp" || n == "date time" || n == "datetime") return "timestamp";
    if (n == "out temp" || n == "outside temp" || n == "temp out" || n == "temperature" || n == "temperature out" || n == "air temperature") return "tmp";
    if (n == "hi temp" || n == "high temp" || n == "temp hi" || n == "max temp" || n == "temperature max") return "tmp";
    if (n == "low temp" || n == "lo temp" || n == "temp low" || n == "min temp" || n == "temperature min") return "tmp";
    if (n == "out hum" || n == "outside hum" || n == "humidity" || n == "relative humidity" || n == "hum out") return "rh";
    if (n == "wind speed" || n == "wind spd" || n == "windspeed" || n == "10minavgwindspeed") return "wsp";
    if (n == "wind dir" || n == "wind direction" || n == "winddir" || n == "hi wind dir") return "wdr";
    if (n == "bar" || n == "barometer" || n == "barometric pressure" || n == "pressure") return "pa";
    if (n == "rain" || n == "rainfall" || n == "rain rate" || n == "rainrate" || n == "precipitation" || n == "precipitacion" || n == "precipitación") return "pp";
    if (n == "flow" || n == "caudal" || n == "gasto" || n == "flujo" || n == "discharge" || n == "streamflow" || n == "escurrimiento" || n == "aforo") return "flow";
    if (n == "water level" || n == "nivel agua" || n == "nivel del agua" || n == "nivel" || n == "stage" || n == "tirante" || n == "cota" || n == "elevacion" || n == "elevación") return "water_level";
    if (n == "storage" || n == "almacenamiento" || n == "volumen" || n == "volumen almacenado" || n == "embalse" || n == "presa volumen") return "storage";
    if (n == "water temperature" || n == "temperatura agua" || n == "temperatura del agua" || n == "temp agua") return "water_temp";
    if (n == "ph" || n == "p h" || n == "potencial hidrogeno" || n == "potencial hidrógeno") return "ph";
    if (n == "conductividad" || n == "conductividad electrica" || n == "conductividad eléctrica" || n == "conductivity" || n == "specific conductance") return "conductivity";
    if (n == "turbidez" || n == "turbidity" || n == "ntu") return "turbidity";
    if (n == "oxigeno disuelto" || n == "oxígeno disuelto" || n == "dissolved oxygen" || n == "od" || n == "do") return "dissolved_oxygen";
    if (n == "dbo" || n == "bod" || n == "demanda bioquimica de oxigeno" || n == "demanda bioquímica de oxígeno") return "bod";
    if (n == "dqo" || n == "cod" || n == "demanda quimica de oxigeno" || n == "demanda química de oxígeno") return "cod";
    if (n == "tds" || n == "solidos disueltos totales" || n == "sólidos disueltos totales" || n == "total dissolved solids") return "tds";
    if (n == "tss" || n == "sst" || n == "solidos suspendidos totales" || n == "sólidos suspendidos totales" || n == "total suspended solids") return "tss";
    if (n == "solar rad" || n == "solarrad" || n == "solar radiation" || n == "radiacion solar" || n == "rad solar" || n == "rad_solar") return "gr";
    if (n == "uv" || n == "uv index" || n == "uvindex" || n == "indice uv" || n == "indice_uv") return "uv";
    if (n == "dosis uv" || n == "dosis_uv") return "uv_dose";
    if (n == "pm 2 5" || n == "pm2 5 ug m3" || n == "pm2 5_atm" || n == "pm2 5 cf1") return "pm25";

    return lower_ascii(trim(value));
}



bool is_known_atmosphere_parameter(const std::string& value) {

    static const std::set<std::string> ids = {

        "co", "no", "no2", "nox", "o3", "pm10", "pm25", "pmco", "so2",
        "h2s", "hcho", "nh3", "ben", "tol", "xyl", "ch4", "co2", "aod", "uvai",
        "frp", "brightness", "cloud_fraction", "cloud_pressure", "cloud_top_temperature", "lst", "ndvi", "evi", "albedo",
        "tmp", "tmax", "tmin", "rh", "pa", "wsp", "wdr", "wgst", "wdr_gust", "u10", "v10", "gr", "uv", "uva", "uvb", "uvc", "pp", "pblh",
        "flow", "water_level", "storage", "water_temp", "ph", "conductivity", "turbidity", "dissolved_oxygen", "bod", "cod", "tds", "tss"
    };

    return ids.count(canonical_parameter_id(value)) > 0;
}



bool atmosphere_parameter_is_meteorological_key(const std::string& pollutant) {
    const std::string p = canonical_parameter_id(pollutant);
    static const std::set<std::string> ids = {
        "tmp", "tmax", "tmin", "rh", "pa", "wsp", "wdr", "wgst", "wdr_gust", "u10", "v10", "gr", "uv", "uva", "uvb", "uvc", "pp", "pblh"
    };
    return ids.count(p) != 0;
}

bool atmosphere_source_accepts_parameter(const std::string& source_path, const std::string& pollutant) {
    const std::string n = normalize_key(source_path);
    if (contains_norm(n, "conagua") || contains_norm(n, "bandas") || contains_norm(n, "renameca") ||
        contains_norm(n, "hidro") || contains_norm(n, "agua") || contains_norm(n, "presa")) return true;
    const bool meteo = atmosphere_parameter_is_meteorological_key(pollutant);
    if (contains_norm(n, "ruoa") || contains_norm(n, "pembu")) return meteo;
    if (contains_norm(n, "redma") || contains_norm(n, "redmet") || contains_norm(n, "meteorolog") || contains_norm(n, "clima")) return meteo;
    if (contains_norm(n, "rama") || contains_norm(n, "simat") || contains_norm(n, "contaminante")) return !meteo;
    return true;
}

bool atmosphere_forced_family_accepts_parameter(int forced_source_family, const std::string& pollutant) {
    if (forced_source_family <= 0) return true;
    const bool meteo = atmosphere_parameter_is_meteorological_key(pollutant);
    if (forced_source_family == TLAL_ATMOS_SOURCE_RAMA) return !meteo;
    if (forced_source_family == TLAL_ATMOS_SOURCE_REDMA || forced_source_family == TLAL_ATMOS_SOURCE_RUOA) return meteo;
    return true;
}

std::string default_unit_for_parameter(const std::string& pollutant) {

    const std::string p = canonical_parameter_id(pollutant);

    if (p == "co") return "ppm";

    if (p == "pm10" || p == "pm25" || p == "pmco" || p == "pb" || p == "cd" || p == "as" || p == "ni" || p == "hg" || p == "cr" || p == "bc" || p == "ec" || p == "oc" || p == "so4" || p == "no3a") return "ug/m3";

    if (p == "aod" || p == "uvai" || p == "uv" || p == "ndvi" || p == "evi" || p == "albedo") return "indice";

    if (p == "pp") return "mm";

    if (p == "flow") return "m3/s";
    if (p == "water_level") return "m";
    if (p == "storage") return "hm3";
    if (p == "water_temp") return "C";
    if (p == "ph") return "pH";
    if (p == "conductivity") return "uS/cm";
    if (p == "turbidity") return "NTU";
    if (p == "dissolved_oxygen" || p == "bod" || p == "cod" || p == "tds" || p == "tss") return "mg/L";

    if (p == "uv_dose") return "mJ/cm2";

    if (p == "frp") return "MW";

    if (p == "fire_area") return "m2";

    if (p == "cloud_fraction") return "1";

    if (p == "cloud_pressure") return "hPa";

    if (p == "brightness" || p == "fire_temperature" || p == "cloud_top_temperature" || p == "lst") return "K";

    if (p == "pblh") return "m";

    if (p == "tmp") return "C";

    if (p == "rh") return "%";

    if (p == "pa") return "hPa";

    if (p == "wsp" || p == "wgst") return "m/s";

    if (p == "wdr" || p == "wdr_gust") return "grados";


    if (p == "gr" || p == "uva" || p == "uvb") return "W/m2";

    if (p == "o3" || p == "no" || p == "no2" || p == "nox" || p == "so2" || p == "h2s" || p == "hcho" || p == "nh3" || p == "ben" || p == "tol" || p == "xyl" || p == "ch4" || p == "co2") return "ppb";

    return {};
}



std::string recode_unit_code(std::string unit, const std::string& pollutant) {

    unit = trim(unit);

    if (unit == "1") return "ppb";

    if (unit == "2") return "ug/m3";

    if (unit == "3") return "m/s";

    if (unit == "4") return "grados";

    if (unit == "5") return "C";

    if (unit == "6") return "%";

    if (unit == "7") return "W/m2";

    if (unit == "9") return "mmHg";

    if (unit == "15") return "ppm";

    if (unit == "17") return "mW/cm2";

    if (unit.empty()) return default_unit_for_parameter(pollutant);

    return unit;
}



StationMeta aggregate_station_for_source(const std::string& source_path) {


    const std::string n = normalize_key(source_path);
    StationMeta s;

    struct RuoaStation { const char* code; const char* name; double lon; double lat; double alt; const char* ent; const char* mun; const char* mun_name; };
    static const RuoaStation ruoa_stations[] = {
        {"cca", "RUOA PEMBU CCA", -99.176111, 19.326111, 2294, "CMX", "COY", "Coyoacan"},
        {"enp1", "RUOA PEMBU ENP1", -99.1039, 19.2533, 2240, "CMX", "XOC", "Xochimilco"},
        {"enp2", "RUOA PEMBU ENP2", -99.0960, 19.3860, 2240, "CMX", "IZC", "Iztacalco"},
        {"enp3", "RUOA PEMBU ENP3", -99.0980, 19.4830, 2240, "CMX", "GAM", "Gustavo A. Madero"},
        {"enp4", "RUOA PEMBU ENP4", -99.1900, 19.4040, 2240, "CMX", "MIH", "Miguel Hidalgo"},
        {"enp5", "RUOA PEMBU ENP5", -99.1670, 19.2840, 2280, "CMX", "TLP", "Tlalpan"},
        {"enp6", "RUOA PEMBU ENP6", -99.1620, 19.3450, 2240, "CMX", "COY", "Coyoacan"},
        {"enp7", "RUOA PEMBU ENP7", -99.1250, 19.3920, 2240, "CMX", "IZC", "Iztacalco"},
        {"enp8", "RUOA PEMBU ENP8", -99.1860, 19.3710, 2240, "CMX", "AOB", "Alvaro Obregon"},
        {"enp9", "RUOA PEMBU ENP9", -99.1290, 19.4940, 2240, "CMX", "GAM", "Gustavo A. Madero"},
        {"ccha", "RUOA PEMBU CCH Azcapotzalco", -99.1880, 19.5020, 2240, "CMX", "AZC", "Azcapotzalco"},
        {"ccho", "RUOA PEMBU CCH Oriente", -99.0450, 19.3820, 2240, "CMX", "IZP", "Iztapalapa"},
        {"cchs", "RUOA PEMBU CCH Sur", -99.1950, 19.3150, 2290, "CMX", "COY", "Coyoacan"},
        {"cchv", "RUOA PEMBU CCH Vallejo", -99.1530, 19.4860, 2240, "CMX", "GAM", "Gustavo A. Madero"},
        {"cchn", "RUOA PEMBU CCH Naucalpan", -99.2310, 19.4750, 2300, "MEX", "NAU", "Naucalpan"}
    };

    for (const auto& r : ruoa_stations) {
        const std::string key = std::string("pembu ") + r.code;
        const std::string key2 = std::string("ruoa pembu ") + r.code;
        const std::string key3 = std::string("/") + r.code + "/";
        if (contains_norm(n, key) || contains_norm(n, key2) || n.find(key3) != std::string::npos || contains_norm(n, std::string("pembu_") + r.code)) {
            std::string sid = r.code;
            if (sid == "cca") sid = "CCA";
            else if (sid.size() == 4 && sid.rfind("enp", 0) == 0) sid = std::string("E0") + sid.substr(3, 1);
            else if (sid == "ccha") sid = "CHA";
            else if (sid == "ccho") sid = "CHO";
            else if (sid == "cchs") sid = "CHS";
            else if (sid == "cchv") sid = "CHV";
            else if (sid == "cchn") sid = "CHN";
            s.id = uppercase_code3(sid);
            s.name = r.name;
            s.lon = r.lon;
            s.lat = r.lat;
            s.alt = r.alt;
            s.entity_code = r.ent;
            s.municipality_code = r.mun;
            s.municipality_name = r.mun_name;
            return s;
        }
    }

    if (contains_norm(n, "meteorolog") || contains_norm(n, "redma")) {
        s.id = "MET";
        s.name = "REDMA promedio ZMVM";
    } else {
        s.id = "RMA";
        s.name = "RAMA promedio ZMVM";
    }

    s.lon = -99.1332;

    s.lat = 19.4326;
    s.alt = 2240.0;
    s.entity_code = "CMX";

    s.municipality_code = "CUH";
    s.municipality_name = "Cuauhtemoc";

    return s;
}





struct ParsedAtmosDate {

    int year = 0;


    int month = 0;
    int day = 0;

    int hour = 0;

    int minute = 0;
    int second = 0;

    std::string date;

    std::string hour_text;
    std::string key;
};



int expand_two_digit_year(int y) {

    if (y >= 100) return y;

    return y >= 70 ? 1900 + y : 2000 + y;
}




ParsedAtmosDate parsed_atmos_date_from_stamp(const TlalAtmStamp& st) {
    ParsedAtmosDate out;
    out.year = st.year;
    out.month = st.month;
    out.day = st.day;
    out.hour = st.hour;
    out.minute = st.minute;
    out.second = st.second;

    char dbuf[16];
    char hbuf[16];
    std::snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d", out.year, out.month, out.day);
    if (out.minute != 0 || out.second != 0) {
        if (out.second != 0) std::snprintf(hbuf, sizeof(hbuf), "%02d:%02d:%02d", out.hour, out.minute, out.second);
        else std::snprintf(hbuf, sizeof(hbuf), "%02d:%02d", out.hour, out.minute);
    } else {
        std::snprintf(hbuf, sizeof(hbuf), "%02d", out.hour);
    }
    out.date = dbuf;
    out.hour_text = hbuf;
    out.key = out.date + " " + out.hour_text;
    return out;
}

std::optional<ParsedAtmosDate> parse_atmospheric_datetime_parts(const std::string& raw_date, const std::string& raw_time) {
    TlalAtmStamp st;
    const char* tp = raw_time.empty() ? nullptr : raw_time.data();
    const size_t tn = raw_time.empty() ? 0u : raw_time.size();
    if (!tlal_atm_parse_stamp(raw_date.data(), raw_date.size(), tp, tn, &st)) return std::nullopt;
    return parsed_atmos_date_from_stamp(st);
}

std::optional<ParsedAtmosDate> parse_atmospheric_datetime(const std::string& raw) {
    return parse_atmospheric_datetime_parts(raw, std::string{});
}



void add_render_accumulator_sample(std::map<std::string, CloudAccumulator>& render_accumulators,
                                   const ParsedAtmosDate& dt,
                                   const StationMeta& station,
                                   const std::string& pollutant,
                                   const std::string& unit,
                                   double value) {
    if (dt.year <= 0 || dt.month < 1 || dt.month > 12) return;
    const std::string station_id_key = uppercase_code3(station.id);
    const std::string key = std::to_string(dt.year) + "|" + std::to_string(dt.month) + "|" +
                            station_id_key + "|" + pollutant + "|" + unit;
    auto& acc = render_accumulators[key];
    if (acc.count == 0) {
        acc.station = station;
        acc.station.id = station_id_key;
        acc.pollutant = pollutant;
        acc.unit = unit;
        acc.year = dt.year;
        acc.month = dt.month;
    }
    acc.add(value);
}



struct AtmosFastParseContext {
    StationMeta default_station;
    const std::map<std::string, StationMeta>* stations = nullptr;
    std::map<std::string, CloudAccumulator>* render_accumulators = nullptr;
    std::map<std::string, PackedAtmosphereBatchBuilder>* packed_batches = nullptr;
    int* visible_rows = nullptr;
    int64_t* rows_in_file = nullptr;
    const AtmosphereFoundationOptions* options = nullptr;
    int processed_files = 0;
    int total_files = 0;
    std::string current_file;
    std::uint64_t* import_errors_total = nullptr;
    std::uint64_t* external_errors_total = nullptr;

    // Cache microresidente por archivo: RAMA/REDMET repite miles de veces las
    // mismas claves de estación. Evita búsquedas logarítmicas y copias de
    // metadatos por cada celda sin persistir estado ni tocar IXIPTLAH.
    std::string hot_station_id;
    StationMeta hot_station;
    std::unordered_map<std::string, StationMeta> station_cache;
};

StationMeta station_for_fast_row(const TlalAtmosCsvRow* row, AtmosFastParseContext* ctx) {
    StationMeta station = ctx ? ctx->default_station : StationMeta{};
    const char* raw_id = row ? row->station_id : nullptr;
    if (row && row->has_coordinates) {
        station.id = raw_id && raw_id[0]
            ? uppercase_code3(raw_id, station.id.empty() ? "SAT" : station.id)
            : station.id;
        if (station.id.empty()) station.id = "SAT";
        station.name = raw_id && raw_id[0] ? raw_id : "Punto satelital";
        station.lat = row->latitude;
        station.lon = row->longitude;
        station.alt = 0.0;
        apply_station_territory_hint(station);
        return station;
    }
    if (raw_id && raw_id[0]) {
        const std::string id = uppercase_code3(raw_id, station.id.empty() ? "ATM" : station.id);
        if (ctx) {
            if (id == ctx->hot_station_id) return ctx->hot_station;
            const auto cache_it = ctx->station_cache.find(id);
            if (cache_it != ctx->station_cache.end()) {
                ctx->hot_station_id = id;
                ctx->hot_station = cache_it->second;
                return ctx->hot_station;
            }
        }
        if (ctx && ctx->stations) {
            const auto it = ctx->stations->find(id);
            if (it != ctx->stations->end()) {
                station = it->second;
                if (ctx->station_cache.size() < 4096u) ctx->station_cache.emplace(id, station);
                ctx->hot_station_id = id;
                ctx->hot_station = station;
                return station;
            }
        }
        station.id = id;
        if (station.name.empty() || station.name == "RAMA promedio ZMVM" || station.name == "REDMA promedio ZMVM") {
            station.name = "Estación atmosférica " + id;
        }
        apply_station_territory_hint(station);
        if (ctx) {
            if (ctx->station_cache.size() < 4096u) ctx->station_cache.emplace(id, station);
            ctx->hot_station_id = id;
            ctx->hot_station = station;
        }
    }
    return station;
}

int atmos_fast_parse_callback(const TlalAtmosCsvRow* row, void* user) {
    if (!row || !user) return 0;
    auto* ctx = static_cast<AtmosFastParseContext*>(user);
    if (!ctx->render_accumulators || !ctx->packed_batches || !ctx->visible_rows || !ctx->rows_in_file) return 0;
    const ParsedAtmosDate dt = parsed_atmos_date_from_stamp(row->stamp);
    const std::string pollutant = row->parameter_id ? row->parameter_id : "";
    std::string unit = row->unit ? row->unit : default_unit_for_parameter(pollutant);
    if (unit.empty()) unit = default_unit_for_parameter(pollutant);
    unit = recode_unit_code(unit, pollutant);
    if (pollutant.empty() || !std::isfinite(row->value)) return 1;
    const StationMeta station = station_for_fast_row(row, ctx);
    add_render_accumulator_sample(*ctx->render_accumulators, dt, station, pollutant, unit, row->value);
    add_packed_atmosphere_sample(*ctx->packed_batches, dt.year, dt.month, dt.day, dt.hour, dt.minute, station, pollutant, unit, row->value);
    ++(*ctx->visible_rows);
    ++(*ctx->rows_in_file);
    if (((*ctx->visible_rows) % 8192) == 0) temporal_flush_append_streams_if_due(600);
    return 1;
}
int atmos_fast_parse_progress_callback(uint64_t physical_lines,
                                       uint64_t data_lines,
                                       uint64_t emitted_measurements,
                                       uint64_t bytes_done,
                                       uint64_t bytes_total,
                                       void* user) {
    auto* ctx = static_cast<AtmosFastParseContext*>(user);
    if (!ctx || !ctx->options || !ctx->options->detail_progress_callback) return 1;
    const int64_t rows = ctx->rows_in_file ? *ctx->rows_in_file : static_cast<int64_t>(emitted_measurements);
    std::string phase = "IXv1 leyendo ";
    phase += ctx->current_file.empty() ? "CSV atmosferico" : ctx->current_file;
    phase += " · ";
    phase += std::to_string(static_cast<unsigned long long>(physical_lines));
    phase += " lineas fisicas · ";
    phase += std::to_string(static_cast<unsigned long long>(emitted_measurements));
    phase += " mediciones";
    (void)data_lines;
    try {
        ctx->options->detail_progress_callback(ctx->processed_files, ctx->total_files, rows,
                                               bytes_done, bytes_total, ctx->current_file, phase,
                                               ctx->import_errors_total ? *ctx->import_errors_total : 0u,
                                               ctx->external_errors_total ? *ctx->external_errors_total : 0u);
    } catch (...) {
        return 1;
    }
    return 1;
}



int year_from_row_date(const std::string& s) {


    const auto parsed = parse_atmospheric_datetime(s);


    return parsed ? parsed->year : 0;
}





std::optional<double> parse_cloud_value(const std::string& s) {
    const std::string t = trim(s);


    if (t.empty() || t == "NA" || t == "NaN" || t == "null") return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);

    if (end == t.c_str() || !std::isfinite(v)) return std::nullopt;

    return v;
}



int year_hint_from_filename(const fs::path& p) {

    const std::string n = path_utf8(p.filename());

    for (size_t i = 0; i + 3 < n.size(); ++i) {

        if ((n[i] == '1' || n[i] == '2') &&
            std::isdigit(static_cast<unsigned char>(n[i + 1])) &&
            std::isdigit(static_cast<unsigned char>(n[i + 2])) &&

            std::isdigit(static_cast<unsigned char>(n[i + 3]))) {
            const int y = (n[i] - '0') * 1000 + (n[i + 1] - '0') * 100 + (n[i + 2] - '0') * 10 + (n[i + 3] - '0');

            if (y >= 1980 && y <= 2100) return y;
        }
    }

    return 0;
}



std::vector<fs::path> select_cloud_source_files(const AtmosphereFoundationOptions& options) {

    std::vector<fs::path> files;
    std::error_code ec;
    std::set<std::string> seen_paths;


    for (const auto& root : atmospheric_scan_roots(options.source_root)) {

        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {

            if (ec) { ec.clear(); continue; }
            std::error_code item_ec;

            if (!it->is_regular_file(item_ec) || item_ec) continue;


            const std::string ext = lower_ascii(path_utf8(it->path().extension()));


            if (ext != ".csv" && ext != ".tsv" && ext != ".txt") continue;


            const std::string name = normalize_key(path_utf8(it->path().filename()));


            const std::string full = normalize_key(path_utf8(it->path()));

            if (name.rfind("cat ", 0) == 0 || contains_norm(name, "cat estacion") ||
                contains_norm(name, "cat parametros") || contains_norm(name, "cat unidades")) continue;

            /* La carpeta seleccionada por el usuario ya es el contrato de origen.
               No exigir que cada CSV lleve RAMA/REDMET/RUOA en el nombre: muchos
               paquetes oficiales vienen como 2024.csv, datos.csv o subcarpetas
               neutras. Los catálogos ya se excluyeron arriba; el parser valida
               columnas antes de escribir IXIPTLAH, por lo que aceptar el archivo
               aquí desbloquea RAMA local sin contaminar el registro. */
            const std::string stable = path_utf8(fs::weakly_canonical(it->path(), item_ec));
            if (!seen_paths.insert(stable.empty() ? path_utf8(it->path()) : stable).second) continue;
            files.push_back(it->path());
        }
    }

    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {

        const int ya = year_hint_from_filename(a);

        const int yb = year_hint_from_filename(b);

        if (ya != yb) return ya < yb;


        return path_utf8(a.filename()) < path_utf8(b.filename());
    });

    return files;
}




struct TerritoryHourAccumulator {
    std::string entity_code;
    std::string municipality_code;
    std::string municipality_name;
    std::string pollutant;

    std::string unit;
    double sum = 0.0;
    int64_t count = 0;
    void add(double v) { sum += v; ++count; }
};



void flush_territory_hour(const AtmosphereFoundationOptions& options,

                          const std::string& date,

                          const std::string& hour,

                          int year,


                          std::map<std::string, TerritoryHourAccumulator>& acc) {

    for (const auto& [_, a] : acc) {


        if (a.count <= 0) continue;
        temporal_append_atmosphere_territory_average(options.output_root,

                                                     date,

                                                     hour,

                                                     year,
                                                     a.entity_code,
                                                     a.municipality_code,
                                                     a.municipality_name,
                                                     a.pollutant,
                                                     "promedio_horario_alcaldia_municipio",
                                                     a.sum / static_cast<double>(a.count),

                                                     a.unit,
                                                     a.count);
    }

    acc.clear();
}





std::optional<double> json_number_field(const nlohmann::json& obj, std::initializer_list<const char*> keys) {

    if (!obj.is_object()) return std::nullopt;

    for (const char* key : keys) {

        if (!obj.contains(key)) continue;

        const auto& v = obj[key];

        if (v.is_number()) return v.get<double>();

        if (v.is_string()) {


            if (auto parsed = parse_cloud_value(v.get<std::string>())) return parsed;
        }
    }

    return std::nullopt;
}




std::string json_string_field(const nlohmann::json& obj, std::initializer_list<const char*> keys) {

    if (!obj.is_object()) return std::string();

    for (const char* key : keys) {

        if (!obj.contains(key)) continue;
        const auto& v = obj[key];

        if (v.is_string()) return v.get<std::string>();

        if (v.is_number_integer() || v.is_number_unsigned()) return std::to_string(v.get<long long>());

        if (v.is_number_float()) {
            std::ostringstream os;

            os << std::setprecision(12) << v.get<double>();

            return os.str();
        }
    }

    return std::string();
}




bool json_first_coordinate_lonlat(const nlohmann::json& coordinates, double& lon, double& lat) {


    if (!coordinates.is_array() || coordinates.empty()) return false;

    if (coordinates.size() >= 2 && coordinates[0].is_number() && coordinates[1].is_number()) {

        lon = coordinates[0].get<double>();

        lat = coordinates[1].get<double>();

        return std::isfinite(lon) && std::isfinite(lat);
    }

    for (const auto& child : coordinates) {


        if (json_first_coordinate_lonlat(child, lon, lat)) return true;
    }

    return false;
}




bool json_geometry_lonlat(const nlohmann::json& obj, double& lon, double& lat) {



    if (!obj.is_object()) return false;


    if (auto la = json_number_field(obj, {"latitude", "latitud", "lat", "LAT", "Lat"})) {


        if (auto lo = json_number_field(obj, {"longitude", "longitud", "lon", "lng", "LON", "Lon", "x"})) {

            lat = *la;

            lon = *lo;

            return std::isfinite(lat) && std::isfinite(lon);
        }
    }


    const nlohmann::json* geom = nullptr;

    if (obj.contains("geometry") && obj["geometry"].is_object()) geom = &obj["geometry"];

    else if (obj.contains("geometria") && obj["geometria"].is_object()) geom = &obj["geometria"];


    else if (obj.contains("coordinates") && obj["coordinates"].is_array()) geom = &obj;


    if (geom && geom->contains("coordinates")) return json_first_coordinate_lonlat((*geom)["coordinates"], lon, lat);

    return false;
}




std::vector<std::pair<std::string, double>> json_atmosphere_metrics(const nlohmann::json& props) {

    std::vector<std::pair<std::string, double>> out;

    if (!props.is_object()) return out;

    for (auto it = props.begin(); it != props.end(); ++it) {

        const std::string key = canonical_parameter_id(it.key());

        if (!is_known_atmosphere_parameter(key)) continue;

        std::optional<double> value;

        if (it.value().is_number()) value = it.value().get<double>();


        else if (it.value().is_string()) value = parse_cloud_value(it.value().get<std::string>());

        if (value && std::isfinite(*value)) out.push_back({key, *value});
    }

    return out;
}



int write_light_atmospheric_cloud(const AtmosphereFoundationOptions& options, const std::vector<fs::path>& only_files = {}) {

    if (options.output_root.empty()) return 0;
    const auto stations = load_station_catalogs_for_cloud(options.source_root);
    const auto units = load_unit_catalogs_for_cloud(options.source_root);




    const bool persist_legacy_string_records = env_flag_enabled_atmosphere("TLALPOWA_ATMOS_LEGACY_STRING_IXIPTLAH", false);

    std::vector<fs::path> files = only_files.empty() ? select_cloud_source_files(options) : only_files;

    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {

        const int ya = year_hint_from_filename(a);

        const int yb = year_hint_from_filename(b);

        if (ya != yb) return ya > yb;

        return path_utf8(a) > path_utf8(b);
    });

    files.erase(std::unique(files.begin(), files.end()), files.end());


    int visible_rows = 0;
    int render_points = 0;


    int processed_files = 0;
    std::uint64_t import_errors_total = 0;
    std::uint64_t external_errors_total = 0;
    const int total_files = static_cast<int>(files.size());
    if (options.detail_progress_callback) {
        try {
            options.detail_progress_callback(0, total_files, 0, 0, 0, std::string{},
                                             "IXv1 preparando lectura atmosferica directa",
                                             import_errors_total, external_errors_total);
        } catch (...) {}
    }

    for (const auto& file : files) {


        const std::string source_path = path_utf8(file);
        const std::string source_file = path_utf8(file.filename());
        const std::uint64_t source_bytes = static_cast<std::uint64_t>(file_size_or_zero(file));
        if (options.detail_progress_callback) {
            try {
                options.detail_progress_callback(processed_files, total_files, 0, 0, source_bytes, source_file,
                                                 "IXv1 abriendo " + source_file,
                                                 import_errors_total, external_errors_total);
            } catch (...) {}
        }

        std::ifstream in(file, std::ios::binary);

        if (!in) {
            ++external_errors_total;
            if (options.detail_progress_callback) {
                try {
                    options.detail_progress_callback(processed_files, total_files, 0, 0, source_bytes, source_file,
                                                     "IXv1 no pudo abrir " + source_file,
                                                     import_errors_total, external_errors_total);
                } catch (...) {}
            }
            continue;
        }


        const std::string source_id = simple_hash_hex(source_path + "|" + std::to_string(file_size_or_zero(file)) + "|" + std::to_string(file_time_tick(file)));

        const auto source_accepts_pollutant = [&](const std::string& pollutant) -> bool {
            if (options.forced_source_family > 0) return atmosphere_forced_family_accepts_parameter(options.forced_source_family, pollutant);
            return atmosphere_source_accepts_parameter(source_path, pollutant);
        };


        const std::string domain = classify_kind(normalize_key(source_path));
        std::string line;
        bool header_seen = false;
        bool wide_table = false;

        bool geospatial_table = false;

        bool geospatial_long_table = false;
        char delim = ',';

        int date_i = -1, time_i = -1, station_i = -1, param_i = -1, value_i = -1, unit_i = -1, lat_i = -1, lon_i = -1;

        std::vector<std::pair<int, std::string>> wide_parameters;

        std::string current_hour_key;

        std::string current_date;

        std::string current_hour;

        int current_year = 0;

        std::map<std::string, TerritoryHourAccumulator> territory_hour;

        std::map<std::string, PackedAtmosphereBatchBuilder> packed_batches;

        std::map<std::string, CloudAccumulator> render_accumulators;


        const auto add_render_sample = [&](const ParsedAtmosDate& dt,
                                           const StationMeta& station,
                                           const std::string& pollutant,
                                           const std::string& unit,
                                           double value) {
            add_render_accumulator_sample(render_accumulators, dt, station, pollutant, unit, value);
        };

        int64_t rows_in_file = 0;


        bool non_tabular_materialized = false;


        const std::string file_ext = extension_norm(file);


        if (file_ext == ".json" || file_ext == ".jsonl" || file_ext == ".geojson") {




            const std::uintmax_t max_json_bytes = static_cast<std::uintmax_t>(std::max(1024, env_int_clamped_atmosphere("TLALPOWA_ATMOS_JSON_VISUAL_MAX_BYTES", 32 * 1024 * 1024, 1024 * 1024, 512 * 1024 * 1024)));


            if (file_size_or_zero(file) <= max_json_bytes) {

                const std::string body = read_text_file(file);


                auto parsed_json = nlohmann::json::parse(body, nullptr, false);




                if (parsed_json.is_discarded() && file_ext == ".jsonl") {


                    nlohmann::json arr = nlohmann::json::array();


                    std::istringstream jsonl_in(body);


                    std::string jsonl_line;


                    std::size_t accepted_jsonl = 0;


                    while (std::getline(jsonl_in, jsonl_line)) {


                        jsonl_line = trim(jsonl_line);


                        if (jsonl_line.empty()) continue;


                        auto one = nlohmann::json::parse(jsonl_line, nullptr, false);

                        if (!one.is_discarded()) {

                            arr.push_back(std::move(one));


                            if (++accepted_jsonl >= static_cast<std::size_t>(env_int_clamped_atmosphere("TLALPOWA_ATMOS_JSONL_VISUAL_MAX_RECORDS", 250000, 1000, 2000000))) break;
                        }
                    }


                    if (!arr.empty()) parsed_json = std::move(arr);
                }


                if (!parsed_json.is_discarded()) {


                    std::function<void(const nlohmann::json&)> visit_object;


                    visit_object = [&](const nlohmann::json& obj) {

                        if (obj.is_array()) {

                            for (const auto& child : obj) visit_object(child);

                            return;
                        }

                        if (!obj.is_object()) return;

                        if (obj.contains("features") && obj["features"].is_array()) {

                            for (const auto& f : obj["features"]) visit_object(f);

                            return;
                        }


                        const nlohmann::json* props = &obj;

                        if (obj.contains("properties") && obj["properties"].is_object()) props = &obj["properties"];

                        double lon = 0.0, lat = 0.0;


                        if (!json_geometry_lonlat(obj, lon, lat) && !json_geometry_lonlat(*props, lon, lat)) return;

                        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return;


                        std::string dt_text = json_string_field(*props, {"datetime", "date", "fecha", "acq_date", "start_date", "time_start", "timestamp", "fecha_hora"});


                        const std::string time_text = json_string_field(*props, {"time", "hora", "acq_time"});


                        if (!time_text.empty() && dt_text.find(':') == std::string::npos) dt_text += " " + time_text;

                        if (dt_text.empty()) {

                            const int y = year_from_any_text(source_path);

                            if (y > 0) dt_text = std::to_string(y) + "-01-01 00:00";
                        }


                        const auto dt = parse_atmospheric_datetime(dt_text);

                        if (!dt) return;


                        const auto metrics = json_atmosphere_metrics(*props);

                        if (metrics.empty()) return;
                        StationMeta station;

                        const std::string coord_key = simple_hash_hex(std::to_string(lat) + "," + std::to_string(lon)).substr(0, 8);
                        station.id = "J" + coord_key.substr(0, 5);


                        station.name = "Punto JSON/GeoJSON " + coord_key;

                        station.lon = lon;

                        station.lat = lat;
                        station.alt = 0.0;
                        apply_station_territory_hint(station);

                        for (const auto& [pollutant, value] : metrics) {

                            const std::string unit = default_unit_for_parameter(pollutant);

                            if (!source_accepts_pollutant(pollutant)) continue;

                            add_render_sample(*dt, station, pollutant, unit, value);


                            add_packed_atmosphere_sample(packed_batches, dt->year, dt->month, dt->day, dt->hour, dt->minute, station, pollutant, unit, value);

                            ++visible_rows;

                            ++rows_in_file;
                        }
                    };


                    visit_object(parsed_json);


                    non_tabular_materialized = rows_in_file > 0;
                }
            }
        }


        /* Ruta crítica RAMA/REDMA/RUOA: todo el barrido tabular se resuelve en C puro
           sobre una vista contigua del archivo; C++ sólo consume filas ya tipadas. */
        if (!non_tabular_materialized && (file_ext == ".csv" || file_ext == ".tsv" || file_ext == ".txt")) {
            AtmosFastParseContext fast_ctx;
            fast_ctx.default_station = aggregate_station_for_source(source_path);
            fast_ctx.stations = &stations;
            fast_ctx.render_accumulators = &render_accumulators;
            fast_ctx.packed_batches = &packed_batches;
            fast_ctx.visible_rows = &visible_rows;
            fast_ctx.rows_in_file = &rows_in_file;
            fast_ctx.options = &options;
            fast_ctx.processed_files = processed_files;
            fast_ctx.total_files = total_files;
            fast_ctx.current_file = source_file;
            fast_ctx.import_errors_total = &import_errors_total;
            fast_ctx.external_errors_total = &external_errors_total;
            TlalAtmosCsvParseStats fast_stats;
            const int fast_ok = tlal_atmos_csv_parse_file_utf8_for_family_progress(source_path.c_str(),
                                                                                    options.forced_source_family,
                                                                                    atmos_fast_parse_callback,
                                                                                    &fast_ctx,
                                                                                    atmos_fast_parse_progress_callback,
                                                                                    &fast_ctx,
                                                                                    &fast_stats);
            if (fast_ok) import_errors_total += fast_stats.rejected_cells;
            if (fast_ok && fast_stats.header_found && fast_stats.emitted_measurements > 0) {
                /* El parser C sólo cierra la ruta cuando realmente emitió filas.
                   Si una cabecera fue reconocida pero quedó en cero por catálogo,
                   unidad o frontera de red, la ruta lenta intenta recuperar datos
                   sin duplicar mediciones válidas. */
                non_tabular_materialized = true;
            } else if (!fast_ok || (fast_stats.header_found && fast_stats.data_lines > 0 && fast_stats.emitted_measurements == 0)) {
                ++import_errors_total;
            }
        }



        if (!non_tabular_materialized) while (std::getline(in, line)) {


            if (trim(line).empty()) continue;

            if (!header_seen) {


                const std::string n = normalize_key(line);

                const bool has_date = contains_norm(n, "date") || contains_norm(n, "fecha") || contains_norm(n, "timestamp") || contains_norm(n, "time stamp");

                const bool has_station = contains_norm(n, "id station") || contains_norm(n, "cve station") || contains_norm(n, "cve estac") || contains_norm(n, "estacion");

                const bool has_parameter = contains_norm(n, "id parameter") || contains_norm(n, "cve parameter") || contains_norm(n, "parametro") || contains_norm(n, "cve param");

                if (has_date && has_station && has_parameter) {
                    delim = infer_delimiter(line);

                    const auto h = split_guess_delimited(line, delim);

                    for (int i = 0; i < static_cast<int>(h.size()); ++i) {


                        const std::string k = normalize_key(h[static_cast<size_t>(i)]);


                        if (k == "date" || k == "fecha" || k == "timestamp" || k == "time stamp" || k == "date time" || k == "datetime") date_i = i;

                        else if (k == "time" || k == "hora" || k == "hr" || k == "hour") time_i = i;

                        else if (k == "id station" || k == "cve station" || k == "cve estac" || k == "estacion") station_i = i;

                        else if (k == "id parameter" || k == "cve parameter" || k == "parametro" || k == "cve param") param_i = i;

                        else if (k == "valor" || k == "value") value_i = i;

                        else if (k == "unit" || k == "unidad") unit_i = i;
                    }

                    header_seen = date_i >= 0 && station_i >= 0 && param_i >= 0 && value_i >= 0;

                } else if (has_date) {
                    delim = infer_delimiter(line);

                    const auto h = split_guess_delimited(line, delim);

                    for (int i = 0; i < static_cast<int>(h.size()); ++i) {


                        const std::string k = normalize_key(h[static_cast<size_t>(i)]);

                        if (k == "date" || k == "fecha" || k == "fecha hora" || k == "fecha_hora" || k == "timestamp" || k == "time stamp" || k == "date time" || k == "datetime" || k == "acq date" || k == "acq_date") {

                            date_i = i;
                            continue;
                        }

                        if (k == "time" || k == "hora" || k == "acq time" || k == "acq_time") {

                            time_i = i;
                            continue;
                        }

                        if (k == "latitude" || k == "latitud" || k == "lat") {


                            lat_i = i;
                            continue;
                        }

                        if (k == "longitude" || k == "longitud" || k == "lon" || k == "lng" || k == "long") {

                            lon_i = i;

                            continue;
                        }

                        if (k == "parameter" || k == "param" || k == "pollutant" || k == "variable" || k == "contaminante" || k == "parametro" || k == "id parameter" || k == "cve parameter" || k == "cve param") {
                            param_i = i;
                            continue;
                        }

                        if (k == "value" || k == "valor" || k == "measurement" || k == "medicion" || k == "mean" || k == "concentration" || k == "concentracion") {
                            value_i = i;
                            continue;
                        }

                        if (k == "unit" || k == "units" || k == "unidad" || k == "unidades") {

                            unit_i = i;
                            continue;
                        }

                        if (!is_known_atmosphere_parameter(k)) continue;

                        wide_parameters.push_back({i, canonical_parameter_id(k)});
                    }



                    geospatial_long_table = date_i >= 0 && lat_i >= 0 && lon_i >= 0 && param_i >= 0 && value_i >= 0;

                    geospatial_table = !geospatial_long_table && date_i >= 0 && lat_i >= 0 && lon_i >= 0 && !wide_parameters.empty();


                    wide_table = !geospatial_long_table && !geospatial_table && date_i >= 0 && !wide_parameters.empty();

                    header_seen = wide_table || geospatial_table || geospatial_long_table;
                }

                continue;
            }

            const auto cols = split_guess_delimited(line, delim);

            if (geospatial_long_table) {

                const int need = std::max(std::max(date_i, time_i), std::max(std::max(lat_i, lon_i), std::max(param_i, value_i)));

                if (need < 0 || static_cast<int>(cols.size()) <= need) continue;

                std::string dt_text = cols[static_cast<size_t>(date_i)];

                if (time_i >= 0 && static_cast<int>(cols.size()) > time_i) dt_text += " " + cols[static_cast<size_t>(time_i)];


                const auto dt = parse_atmospheric_datetime(dt_text);

                if (!dt) continue;


                const auto lat_v = parse_cloud_value(cols[static_cast<size_t>(lat_i)]);


                const auto lon_v = parse_cloud_value(cols[static_cast<size_t>(lon_i)]);

                if (!lat_v || !lon_v || *lat_v < -90.0 || *lat_v > 90.0 || *lon_v < -180.0 || *lon_v > 180.0) continue;

                const std::string pollutant = canonical_parameter_id(cols[static_cast<size_t>(param_i)]);

                if (!is_known_atmosphere_parameter(pollutant)) continue;


                const auto value = parse_cloud_value(cols[static_cast<size_t>(value_i)]);

                if (!value) continue;

                std::string unit = unit_i >= 0 && static_cast<int>(cols.size()) > unit_i ? trim(cols[static_cast<size_t>(unit_i)]) : std::string{};
                unit = recode_unit_code(unit, pollutant);
                StationMeta station;

                const std::string coord_key = simple_hash_hex(std::to_string(*lat_v) + "," + std::to_string(*lon_v)).substr(0, 8);
                station.id = "L" + coord_key.substr(0, 5);

                station.name = "Punto geoespacial largo " + coord_key;

                station.lon = *lon_v;

                station.lat = *lat_v;
                station.alt = 0.0;
                apply_station_territory_hint(station);
                if (!source_accepts_pollutant(pollutant)) continue;

                add_render_sample(*dt, station, pollutant, unit, *value);


                add_packed_atmosphere_sample(packed_batches, dt->year, dt->month, dt->day, dt->hour, dt->minute, station, pollutant, unit, *value);

                ++visible_rows;

                ++rows_in_file;


                if ((visible_rows % 5000) == 0) temporal_flush_append_streams_if_due(800);
                continue;
            }

            if (geospatial_table) {

                if (date_i < 0 || lat_i < 0 || lon_i < 0 ||


                    static_cast<int>(cols.size()) <= std::max(date_i, std::max(lat_i, lon_i))) continue;

                std::string dt_text = cols[static_cast<size_t>(date_i)];

                if (time_i >= 0 && static_cast<int>(cols.size()) > time_i) dt_text += " " + cols[static_cast<size_t>(time_i)];


                const auto dt = parse_atmospheric_datetime(dt_text);

                if (!dt) continue;


                const auto lat_v = parse_cloud_value(cols[static_cast<size_t>(lat_i)]);


                const auto lon_v = parse_cloud_value(cols[static_cast<size_t>(lon_i)]);

                if (!lat_v || !lon_v || *lat_v < -90.0 || *lat_v > 90.0 || *lon_v < -180.0 || *lon_v > 180.0) continue;

                StationMeta station;

                const std::string coord_key = simple_hash_hex(std::to_string(*lat_v) + "," + std::to_string(*lon_v)).substr(0, 8);
                station.id = "G" + coord_key.substr(0, 5);
                station.name = "Punto geoespacial " + coord_key;

                station.lon = *lon_v;

                station.lat = *lat_v;
                station.alt = 0.0;
                apply_station_territory_hint(station);

                for (const auto& [value_col, pollutant] : wide_parameters) {

                    if (value_col < 0 || static_cast<int>(cols.size()) <= value_col) continue;


                    const auto value = parse_cloud_value(cols[static_cast<size_t>(value_col)]);

                    if (!value) continue;

                    const std::string unit = default_unit_for_parameter(pollutant);
                    if (!source_accepts_pollutant(pollutant)) continue;

                    add_render_sample(*dt, station, pollutant, unit, *value);


                    add_packed_atmosphere_sample(packed_batches, dt->year, dt->month, dt->day, dt->hour, dt->minute, station, pollutant, unit, *value);

                    ++visible_rows;

                    ++rows_in_file;
                }


                if ((visible_rows % 5000) == 0) temporal_flush_append_streams_if_due(800);
                continue;
            }

            if (wide_table) {

                if (date_i < 0 || static_cast<int>(cols.size()) <= date_i) continue;


                const std::string row_time = (time_i >= 0 && static_cast<int>(cols.size()) > time_i) ? cols[static_cast<size_t>(time_i)] : std::string{};
                const auto dt = parse_atmospheric_datetime_parts(cols[static_cast<size_t>(date_i)], row_time);

                if (!dt) continue;

                const StationMeta station = aggregate_station_for_source(source_path);

                for (const auto& [value_col, pollutant] : wide_parameters) {

                    if (value_col < 0 || static_cast<int>(cols.size()) <= value_col) continue;


                    const auto value = parse_cloud_value(cols[static_cast<size_t>(value_col)]);

                    if (!value) continue;


                    const std::string unit = default_unit_for_parameter(pollutant);
                    if (!source_accepts_pollutant(pollutant)) continue;

                    add_render_sample(*dt, station, pollutant, unit, *value);


                    add_packed_atmosphere_sample(packed_batches, dt->year, dt->month, dt->day, dt->hour, dt->minute, station, pollutant, unit, *value);

                    if (persist_legacy_string_records) {
                        temporal_append_atmosphere_measurement(options.output_root,

                                                               dt->date,

                                                               dt->hour_text,

                                                               dt->year,
                                                               domain.empty() ? "atmosfera" : domain,
                                                               source_id,

                                                               source_file,

                                                               source_path,
                                                               pollutant,
                                                               station.id,

                                                               station.name,

                                                               station.lon,

                                                               station.lat,
                                                               station.alt,

                                                               "promedio_red",
                                                               *value,
                                                               unit);
                    }

                    ++visible_rows;

                    ++rows_in_file;
                }


                if ((visible_rows % 5000) == 0) temporal_flush_append_streams_if_due(800);

                continue;
            }

            const int need = std::max(std::max(date_i, station_i), std::max(param_i, value_i));

            if (static_cast<int>(cols.size()) <= need || stations.empty()) continue;
            const std::string station_id = uppercase_code3(cols[static_cast<size_t>(station_i)]);
            const auto sit = stations.find(station_id);

            if (sit == stations.end()) continue;


            const auto value = parse_cloud_value(cols[static_cast<size_t>(value_i)]);

            if (!value) continue;


            const std::string row_time = (time_i >= 0 && static_cast<int>(cols.size()) > time_i) ? cols[static_cast<size_t>(time_i)] : std::string{};
            const auto dt = parse_atmospheric_datetime_parts(cols[static_cast<size_t>(date_i)], row_time);

            if (!dt) continue;

            const std::string pollutant = canonical_parameter_id(cols[static_cast<size_t>(param_i)]);

            if (pollutant.empty()) continue;

            std::string unit = unit_i >= 0 && static_cast<int>(cols.size()) > unit_i ? trim(cols[static_cast<size_t>(unit_i)]) : std::string{};


            if (const auto uit = units.find(unit); uit != units.end()) unit = uit->second;
            unit = recode_unit_code(unit, pollutant);

            if (!source_accepts_pollutant(pollutant)) continue;

            add_render_sample(*dt, sit->second, pollutant, unit, *value);


            add_packed_atmosphere_sample(packed_batches, dt->year, dt->month, dt->day, dt->hour, dt->minute, sit->second, pollutant, unit, *value);

            if (persist_legacy_string_records) {
                temporal_append_atmosphere_measurement(options.output_root,

                                                       dt->date,

                                                       dt->hour_text,

                                                       dt->year,
                                                       domain.empty() ? "atmosfera" : domain,
                                                       source_id,

                                                       source_file,

                                                       source_path,
                                                       pollutant,
                                                       station_id,
                                                       sit->second.name,

                                                       sit->second.lon,

                                                       sit->second.lat,

                                                       sit->second.alt,

                                                       "medicion_horaria_estacion",
                                                       *value,
                                                       unit);
            }

            ++visible_rows;

            ++rows_in_file;


            if (current_hour_key.empty()) {

                current_hour_key = dt->key;

                current_date = dt->date;

                current_hour = dt->hour_text;

                current_year = dt->year;

            } else if (dt->key != current_hour_key) {


                if (persist_legacy_string_records) flush_territory_hour(options, current_date, current_hour, current_year, territory_hour);

                current_hour_key = dt->key;

                current_date = dt->date;

                current_hour = dt->hour_text;

                current_year = dt->year;
            }

            if (persist_legacy_string_records && !sit->second.municipality_code.empty()) {
                const std::string territory_key = sit->second.entity_code + "|" + sit->second.municipality_code + "|" + pollutant + "|" + unit;


                auto& ta = territory_hour[territory_key];

                if (ta.count == 0) {
                    ta.entity_code = sit->second.entity_code;

                    ta.municipality_code = sit->second.municipality_code;
                    ta.municipality_name = sit->second.municipality_name.empty() ? sit->second.name : sit->second.municipality_name;
                    ta.pollutant = pollutant;
                    ta.unit = unit;
                }
                ta.add(*value);
            }


            if ((visible_rows % 5000) == 0) temporal_flush_append_streams_if_due(800);
        }


        if (persist_legacy_string_records && !current_hour_key.empty()) flush_territory_hour(options, current_date, current_hour, current_year, territory_hour);

        for (const auto& [_, acc] : render_accumulators) {

            if (acc.count <= 0 || !std::isfinite(acc.sum) ||
                !std::isfinite(acc.min_v) || !std::isfinite(acc.max_v)) continue;
            temporal_append_atmosphere_render_summary(options.output_root,


                                                      month_start_date(acc.year, acc.month),
                                                      "",

                                                      acc.year,
                                                      acc.pollutant,

                                                      acc.station.id,
                                                      acc.station.name,

                                                      acc.station.lon,

                                                      acc.station.lat,

                                                      acc.station.alt,
                                                      acc.sum / static_cast<double>(acc.count),
                                                      acc.min_v,
                                                      acc.max_v,
                                                      acc.unit,
                                                      acc.count);
            ++render_points;
        }

        for (const auto& [_, batch] : packed_batches) {
            temporal_append_atmosphere_measurement_batch(options.output_root,
                                                        source_id,

                                                        source_file,

                                                        source_path,
                                                        domain.empty() ? "atmosfera" : domain,

                                                        batch.year,


                                                        batch.month,
                                                        batch.stations,
                                                        batch.pollutants,
                                                        batch.units,
                                                        batch.samples);
        }

        if (rows_in_file <= 0 && !non_tabular_materialized) ++import_errors_total;

        temporal_flush_append_streams_if_due(250);


        ++processed_files;

        if (options.detail_progress_callback) {
            try {
                options.detail_progress_callback(processed_files, total_files, rows_in_file, source_bytes, source_bytes, source_file,
                                                 "IXv1 cerrado " + source_file + " · " + std::to_string(rows_in_file) + " filas",
                                                 import_errors_total, external_errors_total);
            } catch (...) {}
        }


        if (options.progress_callback) {
            const int py = year_from_any_text(source_path);
            std::string phase = "IXv1 ";
            if (py > 0) phase += std::to_string(py) + " ";
            phase += source_file + " " + std::to_string(rows_in_file) + " filas";
            // Mensaje final ultrabreve: sólo describe el archivo/año/líneas ya
            // volcadas al IXIPTLAH elemental; no toca el formato del progreso.
            try { options.progress_callback(processed_files, render_points, phase); } catch (...) {}
        }

        (void)rows_in_file;
    }


    temporal_flush_append_streams();


    if (options.detail_progress_callback) {
        try {
            options.detail_progress_callback(processed_files, total_files, 0, 0, 0, std::string{},
                                             "IXv1 cerrando indices temporales",
                                             import_errors_total, external_errors_total);
        } catch (...) {}
    }

    if (options.progress_callback) {
        try { options.progress_callback(processed_files, render_points, "IXv1 listo datos directos"); } catch (...) {}
    }

    return render_points;

}}




std::set<std::string> AtmosphericReconstructionEngine::load_checkpoint_ids(const fs::path& checkpoint) {

    std::set<std::string> ids;

    if (!fs::exists(checkpoint)) return ids;



    ixiptlah_read_records(checkpoint, [&](IxiptlahRecordType type, std::uint32_t schema, std::istream& in) {


        if (type != IxiptlahRecordType::ProcessedPdf || schema != 1) return true;
        std::string id;
        std::string processed_utc;

        std::string path;


        if (!ixiptlah_read_string(in, id) ||


            !ixiptlah_read_string(in, processed_utc) ||


            !ixiptlah_read_string(in, path)) return false;

        if (!id.empty()) ids.insert(id);

        return true;
    });

    return ids;
}



void AtmosphericReconstructionEngine::append_checkpoint(const fs::path& checkpoint, const SourceFile& source) {


    (void)ixiptlah_append_record(checkpoint, IxiptlahRecordType::ProcessedPdf, 1, [&](std::ostream& out) {


        return ixiptlah_write_string(out, source.file_id) &&


               ixiptlah_write_string(out, now_utc_iso()) &&


               ixiptlah_write_string(out, path_utf8(source.path));
    });

    return;
}




AtmosphericReconstructionEngine::SourceFile AtmosphericReconstructionEngine::describe_source(const fs::path& path, const AtmosphereFoundationOptions& options) {
    (void)options;

    SourceFile s;

    std::error_code canonical_ec;

    const fs::path canonical_path = fs::weakly_canonical(path, canonical_ec);

    s.path = canonical_ec ? path : canonical_path;


    s.bytes = file_size_or_zero(path);

    s.mtime_tick = file_time_tick(path);

    const std::string path_text = path_utf8(s.path);


    const std::string norm = normalize_key(path_text);

    s.file_id = simple_hash_hex(path_text + "|" + std::to_string(s.bytes) + "|" + std::to_string(s.mtime_tick));


    s.kind = options.forced_domain.empty() ? classify_kind(norm) : normalize_key(options.forced_domain);
    s.provider = classify_provider(norm);

    if (!options.forced_provider.empty() && (s.provider.empty() || s.provider == "desconocido")) {

        s.provider = options.forced_provider;
    }

    s.temporal_hint = temporal_hint_from_name(path_text);


    s.variable_hint = infer_variable_hint(path_text);



    const std::string ext = extension_norm(path);


    if (ext == ".csv" || ext == ".txt") {


        std::ifstream in(path, std::ios::binary);
        std::string line;
        int lines = 0;

        std::vector<std::string> fallback_cols;
        std::string fallback_delimiter;

        while (in && lines < options.sample_lines_per_file && std::getline(in, line)) {
            ++lines;

            line = trim(line);

            if (line.empty()) continue;


            const std::string norm_line = normalize_key(line);

            if (contains_norm(norm_line, "simat") || contains_norm(norm_line, "aire df")) s.provider = "RAMA/SIMAT";
            const char delimiter = infer_delimiter(line);

            auto cols = split_guess_delimited(line, delimiter);

            if (cols.size() < 2) continue;

            if (fallback_cols.empty()) {

                fallback_cols = cols;

                fallback_delimiter = delimiter_name(delimiter);
            }

            const bool has_date = contains_norm(norm_line, "date") || contains_norm(norm_line, "fecha");
            const bool has_station = contains_norm(norm_line, "id station") || contains_norm(norm_line, "cve station") || contains_norm(norm_line, "cve estac") || contains_norm(norm_line, "estacion");

            const bool has_parameter = contains_norm(norm_line, "id parameter") || contains_norm(norm_line, "cve parameter") || contains_norm(norm_line, "parametro") || contains_norm(norm_line, "cve param");

            if (has_date && has_station && has_parameter) {
                s.delimiter = delimiter_name(delimiter);

                s.columns = std::move(cols);
                break;
            }
        }

        if (s.columns.empty()) {

            s.columns = std::move(fallback_cols);
            s.delimiter = fallback_delimiter;
        }

        if (!s.columns.empty()) {

            std::string joined;


            for (const auto& c : s.columns) joined += " " + c;
            const std::string variable_from_header = infer_variable_hint(joined);

            if (s.variable_hint == "desconocida" && variable_from_header != "desconocida") s.variable_hint = variable_from_header;
        }
    }

    return s;
}





std::vector<AtmosphericReconstructionEngine::SourceFile> AtmosphericReconstructionEngine::discover_sources(const AtmosphereFoundationOptions& options) {

    std::vector<SourceFile> rows;

    if (options.source_root.empty() || !fs::exists(options.source_root)) return rows;
    std::error_code ec;

    std::set<std::string> seen_paths;
    for (const auto& root : atmospheric_scan_roots(options.source_root)) {

        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {

            if (ec) { ec.clear(); continue; }

            if (options.max_files > 0 && static_cast<int>(rows.size()) >= options.max_files) break;
            std::error_code item_ec;

            if (!it->is_regular_file(item_ec) || item_ec) continue;

            if (!is_supported_source(it->path())) continue;

            const std::string stable = path_utf8(fs::weakly_canonical(it->path(), item_ec));
            if (!seen_paths.insert(stable.empty() ? path_utf8(it->path()) : stable).second) continue;

            rows.push_back(describe_source(it->path(), options));
        }

        if (options.max_files > 0 && static_cast<int>(rows.size()) >= options.max_files) break;
    }


    std::sort(rows.begin(), rows.end(), [](const SourceFile& a, const SourceFile& b) {

        if (a.kind != b.kind) return a.kind < b.kind;

        return path_utf8(a.path) < path_utf8(b.path);
    });

    return rows;
}





void AtmosphericReconstructionEngine::append_manifest_rows(const AtmosphereFoundationOptions& options, const std::vector<SourceFile>& rows) {
    ensure_dir(options.output_root);

    for (const auto& r : rows) {

        const int year = year_from_any_text(r.temporal_hint + " " + path_utf8(r.path));
        std::ostringstream payload;
        payload << "provider=" << r.provider
                << ";delimiter=" << r.delimiter

                << ";bytes=" << static_cast<unsigned long long>(r.bytes)

                << ";mtime_tick=" << r.mtime_tick

                << ";columns=";

        for (size_t i = 0; i < r.columns.size(); ++i) {

            if (i) payload << '|';

            payload << r.columns[i];
        }

        std::map<std::string, std::string> fields;




        const std::string forced_domain_norm = normalize_key(options.forced_domain);

        if (forced_domain_norm == "satelital" || r.kind == "satelital") {

            fields["record_type"] = "atmosfera_satelital_fuente";
        } else if (!forced_domain_norm.empty() &&

                   forced_domain_norm != "atmosfera" &&
                   forced_domain_norm != "meteorologico" &&

                   forced_domain_norm != "contaminante") {

            fields["record_type"] = "catalogo_fuente_datos";
        } else {

            fields["record_type"] = "atmosfera_fuente";
        }

        const std::string canonical_date = canonical_date_from_temporal_hint(r.temporal_hint + " " + path_utf8(r.path));

        fields["date"] = !canonical_date.empty() ? canonical_date : (year > 0 ? (std::to_string(year) + "-01-01") : "0000-01-01");

        fields["hour"] = "00";

        fields["year"] = std::to_string(year);

        fields["domain"] = r.kind.empty() ? "atmosfera" : r.kind;

        fields["provider"] = r.provider;

        fields["source_id"] = r.file_id;

        fields["source_file"] = path_utf8(r.path.filename());

        fields["source_path"] = path_utf8(r.path);

        fields["metric"] = r.variable_hint;

        fields["value_text"] = r.temporal_hint;

        fields["payload"] = payload.str();


        temporal_append_source_inventory_record(options.output_root, fields);
    }
}




void AtmosphericReconstructionEngine::write_model_base(const fs::path& out) {
    (void)out;

    return;
}




AtmosphereFoundationReport AtmosphericReconstructionEngine::prepare_foundation(const AtmosphereFoundationOptions& options) {

    AtmosphereFoundationReport report;

    if (options.output_root.empty()) return report;
    ensure_dir(options.output_root);


    report.manifest_jsonl = atmosphere_runtime_dir() / "atmosfera_fuentes.ixiptlah";


    report.stac_items_jsonl = atmosphere_runtime_dir() / "atmosfera_stac.ixiptlah";


    report.model_base_json = atmosphere_runtime_dir() / "modelo_base_atmosferico.ixiptlah";


    report.cloud_jsonl = atmosphere_runtime_dir() / "atmosfera_nube.ixiptlah";



    const std::string checkpoint_scope = path_utf8(options.output_root) + "|" + path_utf8(options.source_root) + "|ixiptlah_atmos_raw_measurement_batch_v4_visual_textual_longtable";
    const std::string output_key = simple_hash_hex(checkpoint_scope).substr(0, 12);


    const fs::path checkpoint = atmosphere_runtime_dir() / ("checkpoints_atmosfera_" + output_key + ".ixiptlah");

    const auto done = options.resume ? load_checkpoint_ids(checkpoint) : std::set<std::string>{};

    auto rows = discover_sources(options);

    if (options.year_start > 0 || options.year_end > 0) {

        const int ys = options.year_start > 0 ? options.year_start : options.year_end;

        const int ye = options.year_end > 0 ? options.year_end : options.year_start;
        const int lo = std::min(ys, ye);
        const int hi = std::max(ys, ye);

        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const SourceFile& r) {

            const int y = year_from_any_text(r.temporal_hint + " " + path_utf8(r.path));

            return y > 0 && (y < lo || y > hi);

        }), rows.end());
    }

    report.discovered_files = static_cast<int>(rows.size());
    if (options.detail_progress_callback) {
        try {
            options.detail_progress_callback(0, report.discovered_files, 0, 0, 0, std::string{},
                                             "IXv1 detecto " + std::to_string(report.discovered_files) + " archivos atmosfericos",
                                             0u, 0u);
        } catch (...) {}
    }


    std::vector<SourceFile> new_rows;

    for (const auto& r : rows) {


        if (done.count(r.file_id) > 0) { ++report.skipped_by_checkpoint; continue; }

        new_rows.push_back(r);
    }

    append_manifest_rows(options, new_rows);
    if (options.detail_progress_callback) {
        try {
            options.detail_progress_callback(0, static_cast<int>(new_rows.size()), 0, 0, 0, std::string{},
                                             "IXv1 manifesto tecnico escrito; iniciando parser RAMA/REDMET",
                                             0u, 0u);
        } catch (...) {}
    }

    std::vector<fs::path> visualizable_rows;

    visualizable_rows.reserve(new_rows.size());

    for (const auto& r : new_rows) {

        const std::string ext = extension_norm(r.path);

        if ((!is_atmosphere_inventory_only_source(r.path) || is_satellite_coverage_table(r.path)) &&
            (ext == ".csv" || ext == ".tsv" || ext == ".txt" ||
             ext == ".json" || ext == ".jsonl" || ext == ".geojson")) {

            visualizable_rows.push_back(r.path);
        }
    }


    write_model_base(report.model_base_json);

    if (!options.inventory_only && !visualizable_rows.empty()) {

        report.cloud_points = write_light_atmospheric_cloud(options, visualizable_rows);

    } else if (!options.inventory_only && !options.resume && new_rows.empty()) {

        report.cloud_points = write_light_atmospheric_cloud(options);
    }

    for (const auto& r : new_rows) append_checkpoint(checkpoint, r);

    temporal_close_append_streams();

    if (options.detail_progress_callback) {
        try {
            options.detail_progress_callback(static_cast<int>(new_rows.size()), static_cast<int>(new_rows.size()), 0, 0, 0, std::string{},
                                             "IXv1 compactando indices mensuales",
                                             0u, 0u);
        } catch (...) {}
    }


    temporal_rebuild_all_json_indexes(options.output_root);

    report.indexed_files = static_cast<int>(new_rows.size());


    report.sampled_files = static_cast<int>(new_rows.size());

    return report;
}

}

// ===== Nucleos/GuiLauncher.impl =====
#line 1 "Nucleos/GuiLauncher.impl"






namespace epi {

int run_interactive_launcher();




int run_gui_launcher() {

    return run_interactive_launcher();
}

}

// ===== Nucleos/Launcher.impl =====
#line 1 "Nucleos/Launcher.impl"




#include <cstdlib>

#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif



namespace epi {



namespace {




fs::path first_existing_path(std::initializer_list<fs::path> paths) {

    for (const auto& p : paths) {
        std::error_code ec;

        if (!p.empty() && fs::exists(p, ec) && !ec) return p;
    }


    return paths.size() > 0 ? *paths.begin() : fs::path{};
}



std::vector<fs::path> launcher_dependency_roots() {

    std::vector<fs::path> roots;

    auto add_root = [&](const fs::path& root) {

        if (root.empty()) return;


        const fs::path normalized = root.lexically_normal();


        const std::string key = path_utf8(normalized);

        for (const auto& existing : roots) {

            if (path_utf8(existing.lexically_normal()) == key) return;
        }


        roots.push_back(normalized);
    };

    add_root(executable_dir() / "core" / "Dependencias");
    add_root(executable_dir() / "Dependencias");
    const std::string deps_root = getenv_utf8_or_empty("TLALPOWA_DEPS_ROOT");

    if (!deps_root.empty()) {

        const std::string cleaned = trim(deps_root);


        if (!cleaned.empty()) add_root(fs::path(widen_utf8(cleaned)));
    }
#ifdef _WIN32

    add_root(fs::path(L"C:/ProgramData/Miausoft/deps"));

    add_root(fs::path(L"C:/ProgramData/Tlalpowa/Dependencias"));

    add_root(fs::path(L"C:/ProgramData/Observatorio-ZMVM/Dependencias"));
#endif

    return roots;
}



fs::path launcher_dependency_root() {
    const auto roots = launcher_dependency_roots();

    return roots.empty() ? fs::path{} : roots.front();
}




fs::path cdmx_pdf_root(const fs::path& root) {

    return first_existing_path({


        root / "Descargas" / L"BOLETINES EPIDEMIOL\u00d3GICOS SEMANALES",


        root / L". DATOS CIUDAD DE M\u00c9XICO" / L"SALUD" / L"BOLETINES EPIDEMIOL\u00d3GICOS SEMANALES",


        root / L".DATOS CIUDAD DE M\u00c9XICO" / L"SALUD" / L"BOLETINES EPIDEMIOL\u00d3GICOS SEMANALES",


        root / L"BOLETINES EPIDEMIOL\u00d3GICOS SEMANALES"
    });
}




AppOptions make_default_options(bool full_run) {

    const fs::path root = project_root();

    AppOptions o;
    o.config_dir = config_root();

    o.input_dir = cdmx_pdf_root(root);
    o.output_dir = internal_data_root();


    o.runtime_dir = root / "Build" / "runtime" / "pipeline_epidemiologia";


    o.geojson = o.config_dir / "zmvm.geojson";
    o.log_dir = root;
    o.dashboard = false;
    o.render_pages = true;
    o.stop_on_error = false;

    o.render_dpi = 32;


    o.limit_pdfs = full_run ? 0 : 2;
    o.max_pages_per_pdf = full_run ? 0 : 24;

    std::vector<fs::path> deps_candidates;


    for (const auto& deps_root : launcher_dependency_roots()) deps_candidates.push_back(deps_root / "dependencies.local.json");

    fs::path deps_path;

    for (const auto& p : deps_candidates) {
        std::error_code ec;

        if (fs::exists(p, ec) && !ec) {

            deps_path = p;
            break;
        }
    }

    const std::string deps = fs::exists(deps_path) ? read_text_file(deps_path) : std::string{};

    auto read_path = [&](const char* key) -> fs::path {

        const std::regex rx("\\\"" + std::string(key) + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"");
        std::smatch m;


        if (!std::regex_search(deps, m, rx)) return {};
        std::string value = m[1].str();
        std::replace(value.begin(), value.end(), '\\', '/');

        return fs::path(widen_utf8(value));
    };


    o.pdftotext = read_path("pdftotext");


    o.pdftoppm = read_path("pdftoppm");

    for (const auto& deps_root : launcher_dependency_roots()) {


        if (o.pdftotext.empty() || !fs::exists(o.pdftotext)) {


            const fs::path candidate = deps_root / "poppler" / "Library" / "bin" / "pdftotext.exe";


            if (fs::exists(candidate)) o.pdftotext = candidate;
        }


        if (o.pdftoppm.empty() || !fs::exists(o.pdftoppm)) {


            const fs::path candidate = deps_root / "poppler" / "Library" / "bin" / "pdftoppm.exe";


            if (fs::exists(candidate)) o.pdftoppm = candidate;
        }
    }


    return o;
}




void open_path_default(const fs::path& p) {
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)p;
#endif
}

}




int run_interactive_launcher() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::cout << "Tlalpowa\n";


    std::cout << "1) Prueba rapida\n2) Procesamiento completo\n0) Salir\n\nOpcion: ";
    std::string s;
    std::getline(std::cin, s);

    s = trim(s);

    if (s == "0") return 0;
    const bool full = s == "2";
    AppOptions o = make_default_options(full);

    if (!fs::exists(o.config_dir)) throw std::runtime_error("No existe Fuente\\Tlalpowa plano junto al ejecutable/proyecto.");


    if (!fs::exists(o.input_dir)) throw std::runtime_error("No existe el directorio relativo de boletines epidemiologicos.");


    if (!fs::exists(o.pdftotext) || !fs::exists(o.pdftoppm)) throw std::runtime_error("No se encontro Poppler en las dependencias del sistema.");
    Pipeline p(o);

    const int code = p.run();

    return code;
}

}

// ===== Nucleos/main.impl =====
#line 1 "Nucleos/main.impl"





#include <csignal>
#include <cstdint>
#include <exception>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif


using namespace epi;



namespace epi {
int run_interactive_launcher();
int run_gui_launcher();


}

#ifdef _WIN32


static fs::path exe_log_dir() {
    std::wstring buffer(32768, L'\0');

    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

    if (len > 0 && len < buffer.size()) {

        buffer.resize(len);

        return fs::path(buffer).parent_path();
    }

    return fs::current_path();
}
#else


static fs::path exe_log_dir() {

    return fs::current_path();
}
#endif





static void append_crash_log(const std::string& message) noexcept {
    try {
        ensure_dir(exe_log_dir());


        std::ofstream out(exe_log_dir() / "Tlalpowa.log", std::ios::binary | std::ios::app);
        out << now_utc_iso() << " " << message << "\n";
    } catch (...) {}
}

static std::atomic<const char*> g_tlalpowa_crash_phase{"arranque"};
static thread_local const char* g_tlalpowa_thread_crash_phase = "arranque";

extern "C" void tlalpowa_set_crash_phase(const char* phase) noexcept {
    const char* safe_phase = phase ? phase : "desconocido";
    g_tlalpowa_thread_crash_phase = safe_phase;
    g_tlalpowa_crash_phase.store(safe_phase, std::memory_order_relaxed);
}

static const char* tlalpowa_current_crash_phase() noexcept {
    return g_tlalpowa_thread_crash_phase ? g_tlalpowa_thread_crash_phase :
        g_tlalpowa_crash_phase.load(std::memory_order_relaxed);
}

extern "C" void tlalpowa_log_failure_detail(const char* context, const char* detail) noexcept {
    std::ostringstream os;
    os << "[FALLA] contexto=" << (context ? context : "desconocido")
       << " fase=" << tlalpowa_current_crash_phase();
    if (detail && *detail) os << " detalle=" << detail;
#ifdef _WIN32
    os << " pid=" << GetCurrentProcessId()
       << " tid=" << GetCurrentThreadId()
       << " uptime_ms=" << GetTickCount64();
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        os << " memoria_carga=" << memory.dwMemoryLoad
           << "% ram_disp_mb=" << (memory.ullAvailPhys / (1024ull * 1024ull))
           << " ram_total_mb=" << (memory.ullTotalPhys / (1024ull * 1024ull));
    }
#endif
    append_crash_log(os.str());
}




static void signal_handler(int sig) {
    append_crash_log(std::string("senal capturada: ") + std::to_string(sig) +
                     " fase=" + tlalpowa_current_crash_phase());
    std::_Exit(128 + sig);
}




static void install_crash_guards() {

    static bool installed = false;

    if (installed) return;

    installed = true;
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    ULONG stack_guarantee = 128u * 1024u;
    SetThreadStackGuarantee(&stack_guarantee);
#endif


    std::set_terminate([]() {
        append_crash_log(std::string("std::terminate capturado; salida controlada fase=") +
                         tlalpowa_current_crash_phase());
        std::_Exit(3);
    });
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGILL, signal_handler);
    std::signal(SIGSEGV, signal_handler);
#ifdef _WIN32


    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {

        DWORD code = e && e->ExceptionRecord ? e->ExceptionRecord->ExceptionCode : 0;
        const auto address = e && e->ExceptionRecord ?
            reinterpret_cast<std::uintptr_t>(e->ExceptionRecord->ExceptionAddress) : 0u;
        const auto flags = e && e->ExceptionRecord ? e->ExceptionRecord->ExceptionFlags : 0u;
        std::ostringstream os;
        os << "excepcion Win32 no controlada: 0x" << std::hex << code
           << " direccion=0x" << address
           << " banderas=0x" << flags
           << " fase=" << tlalpowa_current_crash_phase();
        os << " pid=" << std::dec << GetCurrentProcessId()
           << " tid=" << GetCurrentThreadId()
           << " uptime_ms=" << GetTickCount64();
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) {
            os << " memoria_carga=" << memory.dwMemoryLoad
               << "% ram_disp_mb=" << (memory.ullAvailPhys / (1024ull * 1024ull))
               << " ram_total_mb=" << (memory.ullTotalPhys / (1024ull * 1024ull));
        }
        ULONG_PTR stack_low = 0, stack_high = 0;
        GetCurrentThreadStackLimits(&stack_low, &stack_high);
        os << " stack_low=0x" << std::hex << static_cast<std::uintptr_t>(stack_low)
           << " stack_high=0x" << static_cast<std::uintptr_t>(stack_high);
        if (address != 0u) {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
                char module_path[MAX_PATH]{};
                if (GetModuleFileNameA(reinterpret_cast<HMODULE>(mbi.AllocationBase), module_path, MAX_PATH) > 0) {
                    os << " modulo=" << module_path
                       << " modulo_base=0x" << reinterpret_cast<std::uintptr_t>(mbi.AllocationBase)
                       << " rva=0x" << (address - reinterpret_cast<std::uintptr_t>(mbi.AllocationBase));
                }
            }
        }
        if (e && e->ExceptionRecord) {
            os << " parametros=" << std::dec << e->ExceptionRecord->NumberParameters;
            for (DWORD i = 0; i < e->ExceptionRecord->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i) {
                os << " p" << i << "=0x" << std::hex
                   << static_cast<std::uintptr_t>(e->ExceptionRecord->ExceptionInformation[i]);
            }
        }
        void* frames[48]{};
        USHORT frame_count = CaptureStackBackTrace(0, static_cast<DWORD>(sizeof(frames) / sizeof(frames[0])), frames, nullptr);
        os << " stack_frames=" << std::dec << frame_count;
        for (USHORT i = 0; i < frame_count; ++i) {
            os << " f" << i << "=0x" << std::hex << reinterpret_cast<std::uintptr_t>(frames[i]);
        }
        append_crash_log(os.str());

        return EXCEPTION_EXECUTE_HANDLER;

    });
#endif
}




/* raise_process_priority: eleva prioridad del proceso. */
static void raise_process_priority() noexcept {
    try {
#ifdef _WIN32

        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#else
        setpriority(PRIO_PROCESS, 0, -4);
#endif
    } catch (...) {}
}



static void print_help() {
    std::cout <<
R"HELP(Tlalpowa

Uso:
  "Tlalpowa.exe" run --input <dir> --output <dir> --config <dir> --pdftotext <exe> --pdftoppm <exe> [--tesseract <exe>] [--geojson <file>] [--runtime <dir>] [--no-dashboard] [--no-resume|--reprocess-epidemiology] [--limit-pdfs N] [--skip-front-pages N] [--skip-back-pages N] [--max-pages N]
  "Tlalpowa.exe" atmosphere --input <dir> [--output <dir>] [--no-resume] [--max-files N]
  "Tlalpowa.exe" atmosphere-web --source meteorologia|contaminantes --years AAAA-AAAA [--no-overwrite]
  "Tlalpowa.exe" epi-web --source cdmx|edomex|all [--years AAAA-AAAA]
  "Tlalpowa.exe" external-smoke --input <dir> --output <dir> [--years AAAA-AAAA] [--inventory-only]
  "Tlalpowa.exe" satellite-web --source sentinel5p|omi|modis|goes --output <dir> [--years AAAA-AAAA]
  "Tlalpowa.exe" epi-audit --input <dir> [--top N]
  "Tlalpowa.exe" selftest
  "Tlalpowa.exe" launcher

Contrato de seguridad:
  1. Nunca usa coordenadas manuales.
  2. Toda celda procede de intersección fila-columna reconstruida desde cajas PDF.
  3. Lee encabezados fusionados por bloques Sem/Acum/M/F/anio anterior y guarda CIE-10 cuando existe.
  4. Total se usa solo como corroboracion o imputacion unica; no elimina datos aceptables.
  5. Las incidencias son Sem; M/F y anio anterior se conservan como acumulados.

)HELP";
}



static std::string arg_value(int& i, int argc, char** argv) {

    if (i + 1 >= argc) throw std::runtime_error(std::string("Falta valor para ") + argv[i]);

    return argv[++i];
}



static fs::path arg_path(int& i, int argc, char** argv) {

    const std::string value = clean_user_path_string(arg_value(i, argc, argv));
#ifdef _WIN32

    return fs::path(widen_utf8(value));
#else


    return fs::path(value);
#endif
}




/* parse_options: decodifica entrada. */
static AppOptions parse_options(int argc, char** argv) {
    AppOptions o;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--input") o.input_dir = arg_path(i, argc, argv);

        else if (a == "--output") o.output_dir = arg_path(i, argc, argv);

        else if (a == "--config") o.config_dir = arg_path(i, argc, argv);

        else if (a == "--runtime") o.runtime_dir = arg_path(i, argc, argv);


        else if (a == "--pdftotext") o.pdftotext = arg_path(i, argc, argv);


        else if (a == "--pdftoppm") o.pdftoppm = arg_path(i, argc, argv);

        else if (a == "--tesseract") o.tesseract = arg_path(i, argc, argv);


        else if (a == "--geojson") o.geojson = arg_path(i, argc, argv);


        else if (a == "--limit-pdfs") o.limit_pdfs = std::stoi(arg_value(i, argc, argv));

        else if (a == "--skip-front-pages") o.skip_front_pages = std::max(0, std::stoi(arg_value(i, argc, argv)));

        else if (a == "--skip-back-pages") o.skip_back_pages = std::max(0, std::stoi(arg_value(i, argc, argv)));


        else if (a == "--max-pages") o.max_pages_per_pdf = std::stoi(arg_value(i, argc, argv));

        else if (a == "--dpi") o.render_dpi = std::stoi(arg_value(i, argc, argv));

        else if (a == "--no-dashboard") o.dashboard = false;

        else if (a == "--no-render") o.render_pages = false;

        else if (a == "--no-resume" || a == "--reprocess-epidemiology" || a == "--rewrite-epidemiology" || a == "--reset-epidemiology") o.resume = false;


        else if (a == "--stop-on-error") o.stop_on_error = true;

        else if (a == "--help" || a == "-h") { print_help(); std::exit(0); }

        else throw std::runtime_error("Argumento desconocido: " + a);
    }

    return o;
}




static int selftest() {

    try {


        if (!parse_epi_int("1,234").has_value() || *parse_epi_int("1,234") != 1234) throw std::runtime_error("parse 1,234");


        if (!parse_epi_int("-").has_value() || *parse_epi_int("-") != 0) throw std::runtime_error("parse hyphen");


        if (normalize_key("Álvaro Obregón") != "alvaro obregon") throw std::runtime_error("normalize accents");
        Config cfg;
        cfg.load(config_root());


        if (!cfg.match_jurisdiction_line(normalize_key("Álvaro Obregón - - 1"))) throw std::runtime_error("jurisdiction match");


        if (!cfg.match_disease_text(normalize_key("Hepatitis vírica A B15"))) throw std::runtime_error("disease match");

        const fs::path mesh_path = fs::temp_directory_path() / "tlalpowa_selftest_mesh.t3d";
        std::vector<unsigned char> mesh_bytes(64u + 36u, 0u);
        std::memcpy(mesh_bytes.data(), "TLP3D002", 8u);
        const auto put_u32 = [&](std::size_t offset, std::uint32_t value) {
            mesh_bytes[offset + 0u] = static_cast<unsigned char>(value);
            mesh_bytes[offset + 1u] = static_cast<unsigned char>(value >> 8u);
            mesh_bytes[offset + 2u] = static_cast<unsigned char>(value >> 16u);
            mesh_bytes[offset + 3u] = static_cast<unsigned char>(value >> 24u);
        };
        const auto put_f32 = [&](std::size_t offset, float value) {
            std::uint32_t bits = 0u;
            std::memcpy(&bits, &value, sizeof(bits));
            put_u32(offset, bits);
        };
        put_u32(8u, 3u);
        put_u32(12u, 1u);
        put_f32(16u, -99.15f);
        put_f32(20u, 19.42f);
        put_f32(24u, -99.14f);
        put_f32(28u, 19.43f);
        put_f32(32u, 0.0f);
        put_f32(36u, 10.0f);
        put_u32(40u, 12u);
        put_u32(48u, 1u);
        put_u32(52u, 1u);
        put_f32(56u, 1.0f);
        for (std::size_t vertex = 0u; vertex < 3u; ++vertex) {
            const std::size_t offset = 64u + vertex * 12u;
            mesh_bytes[offset + 0u] = static_cast<unsigned char>(vertex * 127u);
            mesh_bytes[offset + 2u] = static_cast<unsigned char>(vertex * 127u);
            mesh_bytes[offset + 4u] = static_cast<unsigned char>(vertex == 2u ? 255u : 0u);
            mesh_bytes[offset + 8u] = 127u;
            mesh_bytes[offset + 9u] = 180u;
            mesh_bytes[offset + 10u] = 140u;
            mesh_bytes[offset + 11u] = 110u;
        }
        {
            std::ofstream mesh_file(mesh_path, std::ios::binary | std::ios::trunc);
            mesh_file.write(reinterpret_cast<const char*>(mesh_bytes.data()),
                            static_cast<std::streamsize>(mesh_bytes.size()));
            if (!mesh_file) throw std::runtime_error("temporary native mesh write");
        }

        TlalHistoricalNativeMesh mesh{};
        if (!tlal_historical_native_mesh_load_utf8(path_utf8(mesh_path).c_str(), &mesh)) {
            std::error_code remove_ec;
            fs::remove(mesh_path, remove_ec);
            throw std::runtime_error("tenochtitlan native mesh load");
        }
        const bool mesh_valid = mesh.format_version == 2u &&
                                mesh.vertex_count == 3u &&
                                mesh.triangle_count == 1u &&
                                mesh.source_triangle_count == 1u &&
                                mesh.vertices != nullptr &&
                                mesh.lon_max > mesh.lon_min &&
                                mesh.lat_max > mesh.lat_min &&
                                mesh.height_max_m > mesh.height_min_m;
        tlal_historical_native_mesh_release(&mesh);
        {
            std::error_code remove_ec;
            fs::remove(mesh_path, remove_ec);
        }
        if (!mesh_valid) throw std::runtime_error("tenochtitlan native mesh integrity");
        if (!tlalpowa_tlalpowa3d_regeoref_selftest()) {
            throw std::runtime_error("tlalpowa3d fast regeoreference");
        }

        std::cout << "SELFTEST OK\n";

        return 0;

    } catch (const std::exception& e) {

        std::cerr << "SELFTEST FAIL: " << e.what() << "\n";

        return 3;
    }
}



static int run_atmosphere_prepare_cli(int argc, char** argv) {


    fs::path input = external_data_root();

    fs::path output = internal_data_root();
    bool resume = true;


    int max_files = 2000;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--input") input = arg_path(i, argc, argv);

        else if (a == "--output") output = arg_path(i, argc, argv);

        else if (a == "--no-resume" || a == "--reprocess-epidemiology" || a == "--rewrite-epidemiology" || a == "--reset-epidemiology") resume = false;

        else if (a == "--max-files") max_files = std::max(1, std::stoi(arg_value(i, argc, argv)));

        else if (a == "--help" || a == "-h") { print_help(); return 0; }

        else throw std::runtime_error("Argumento atmosphere desconocido: " + a);
    }
    AtmosphereFoundationOptions options;
    options.source_root = input;
    options.output_root = output;
    options.resume = resume;

    options.max_files = max_files;

    options.sample_lines_per_file = 40;
    const AtmosphereFoundationReport report = AtmosphericReconstructionEngine::prepare_foundation(options);
    std::cout << "ATMOSPHERE OK\n"

              << "input=" << path_utf8(input) << "\n"

              << "output=" << path_utf8(output) << "\n"

              << "discovered_files=" << report.discovered_files << "\n"

              << "indexed_files=" << report.indexed_files << "\n"
              << "skipped_by_checkpoint=" << report.skipped_by_checkpoint << "\n"


              << "visible_rows=" << report.cloud_points << "\n"

              << "output_folder=" << path_utf8(output) << "\n";

    return (report.discovered_files > 0 || report.indexed_files > 0 || report.skipped_by_checkpoint > 0) ? 0 : 4;
}



static int run_atmosphere_web_cli(int argc, char** argv) {
    int source = 0;

    int year_start = 2026;

    int year_end = 2026;

    bool overwrite = true;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--source") {


            const std::string value = normalize_key(arg_value(i, argc, argv));

            if (value == "contaminantes" || value == "contaminante" || value == "rama") source = 1;

            else if (value == "meteorologia" || value == "meteorologico" || value == "redma") source = 0;

            else throw std::runtime_error("Fuente atmosferica desconocida: " + value);

        } else if (a == "--years") {
            const std::string value = arg_value(i, argc, argv);
            const size_t dash = value.find('-');

            if (dash == std::string::npos) {

                year_start = year_end = std::stoi(value);
            } else {

                year_start = std::stoi(value.substr(0, dash));


                year_end = std::stoi(value.substr(dash + 1));
            }

        } else if (a == "--year-start") {

            year_start = std::stoi(arg_value(i, argc, argv));

        } else if (a == "--year-end") {

            year_end = std::stoi(arg_value(i, argc, argv));

        } else if (a == "--no-overwrite") {

            overwrite = false;

        } else if (a == "--overwrite") {

            overwrite = true;

        } else if (a == "--help" || a == "-h") {
            print_help();

            return 0;
        } else {

            throw std::runtime_error("Argumento atmosphere-web desconocido: " + a);
        }
    }


    return epi::run_atmosphere_web_import_cli(source, year_start, year_end, overwrite);
}

static void parse_cli_year_range(const std::string& value, int& year_start, int& year_end) {
    const size_t dash = value.find('-');
    if (dash == std::string::npos) {
        year_start = year_end = std::stoi(value);
    } else {
        year_start = std::stoi(value.substr(0, dash));
        year_end = std::stoi(value.substr(dash + 1));
    }
}

static int run_epi_web_cli(int argc, char** argv) {
    bool cdmx = true;
    bool edomex = true;
    int year_start = 2019;
    int year_end = 2026;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--source") {
            const std::string source = normalize_key(arg_value(i, argc, argv));
            if (source == "cdmx" || source.find("ciudad") != std::string::npos) {
                cdmx = true;
                edomex = false;
            } else if (source == "edomex" || source.find("estado") != std::string::npos) {
                cdmx = false;
                edomex = true;
            } else if (source == "all" || source == "todo" || source == "todos") {
                cdmx = true;
                edomex = true;
            } else {
                throw std::runtime_error("Fuente epi-web desconocida: " + source);
            }
        } else if (a == "--years") {
            parse_cli_year_range(arg_value(i, argc, argv), year_start, year_end);
        } else if (a == "--year-start") {
            year_start = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--year-end") {
            year_end = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        } else {
            throw std::runtime_error("Argumento epi-web desconocido: " + a);
        }
    }
    return epi::run_epidemiology_web_download_cli(cdmx, edomex, year_start, year_end);
}

static int run_external_smoke_cli(int argc, char** argv) {
    fs::path input;
    fs::path output;
    int year_start = 2026;
    int year_end = 2026;
    bool inventory_only = false;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--input") {
            input = arg_path(i, argc, argv);
        } else if (a == "--output") {
            output = arg_path(i, argc, argv);
        } else if (a == "--years") {
            parse_cli_year_range(arg_value(i, argc, argv), year_start, year_end);
        } else if (a == "--year-start") {
            year_start = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--year-end") {
            year_end = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--inventory-only") {
            inventory_only = true;
        } else if (a == "--materialize") {
            inventory_only = false;
        } else if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        } else {
            throw std::runtime_error("Argumento external-smoke desconocido: " + a);
        }
    }
    if (input.empty()) throw std::runtime_error("Uso: Tlalpowa.exe external-smoke --input <dir> --output <dir> [--years AAAA-AAAA]");
    if (output.empty()) output = fs::current_path() / "Build" / "test_imports" / "external_smoke_out";
    return epi::run_external_import_smoke_cli(input, output, year_start, year_end, inventory_only);
}

static int satellite_source_from_cli_value(const std::string& value) {
    const std::string key = normalize_key(value);
    if (key == "0" || key.find("sentinel") != std::string::npos || key.find("tropomi") != std::string::npos) return 0;
    if (key == "1" || key == "omi" || key.find("aura") != std::string::npos) return 1;
    if (key == "2" || key.find("modis") != std::string::npos || key.find("maiac") != std::string::npos) return 2;
    if (key == "3" || key.find("goes") != std::string::npos || key.find("abi") != std::string::npos) return 3;
    throw std::runtime_error("Fuente satelital desconocida: " + value);
}

static int run_satellite_web_cli(int argc, char** argv) {
    int source = 2;
    fs::path output;
    int year_start = 2026;
    int year_end = 2026;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--source") {
            source = satellite_source_from_cli_value(arg_value(i, argc, argv));
        } else if (a == "--output") {
            output = arg_path(i, argc, argv);
        } else if (a == "--years") {
            parse_cli_year_range(arg_value(i, argc, argv), year_start, year_end);
        } else if (a == "--year-start") {
            year_start = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--year-end") {
            year_end = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        } else {
            throw std::runtime_error("Argumento satellite-web desconocido: " + a);
        }
    }
    return epi::run_satellite_web_import_cli(source, output, year_start, year_end);
}

static int run_epi_audit_cli(int argc, char** argv) {
    fs::path input;
    fs::path report_path;
    int top = 30;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--input") {
            input = arg_path(i, argc, argv);
        } else if (a == "--report") {
            report_path = arg_path(i, argc, argv);
        } else if (a == "--top") {
            top = std::max(1, std::stoi(arg_value(i, argc, argv)));
        } else if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        } else {
            throw std::runtime_error("Argumento epi-audit desconocido: " + a);
        }
    }
    if (input.empty()) throw std::runtime_error("Uso: Tlalpowa.exe epi-audit --input <dir> [--top N]");

    int64_t records = 0;
    int64_t missing_disease = 0;
    int64_t missing_cie10 = 0;
    int64_t synthetic_disease = 0;
    int64_t total_value = 0;
    std::map<std::string, int64_t> by_disease;
    std::map<std::string, int64_t> by_cie10;
    std::map<std::string, int64_t> by_period;
    std::map<std::string, int64_t> by_sex;
    std::map<int, int64_t> by_year;
    std::set<std::string> jurisdictions;
    std::set<int> years;
    std::set<int> weeks;

    epi::temporal_read_epidemiology_records(input, [&](const epi::TemporalEpidemiologyRecord& rec) {
        ++records;
        total_value += rec.value;
        const std::string disease = trim(rec.disease);
        const std::string cie10 = trim(rec.cie10);
        if (disease.empty()) ++missing_disease;
        else {
            by_disease[disease] += rec.value;
            const std::string disease_norm = normalize_key(disease);
            if (disease_norm.rfind("enfermedad no catalogada", 0) == 0 ||
                disease_norm.rfind("auto_", 0) == 0 ||
                disease_norm.find("cuadro_detectado") != std::string::npos) {
                ++synthetic_disease;
            }
        }
        if (cie10.empty()) ++missing_cie10;
        else by_cie10[cie10] += rec.value;
        by_period[rec.period.empty() ? "(vacio)" : rec.period] += 1;
        by_sex[rec.sex.empty() ? "(vacio)" : rec.sex] += 1;
        if (rec.year > 0) by_year[rec.year] += 1;
        if (!rec.jurisdiction.empty()) jurisdictions.insert(rec.jurisdiction);
        if (rec.year > 0) years.insert(rec.year);
        if (rec.epi_week > 0) weeks.insert(rec.epi_week);
        return true;
    });

    std::vector<std::pair<std::string, int64_t>> disease_rank(by_disease.begin(), by_disease.end());
    std::sort(disease_rank.begin(), disease_rank.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::ostringstream audit;
    audit << "EPI AUDIT\n"
          << "input=" << path_utf8(input) << "\n"
          << "records=" << records << "\n"
          << "total_value=" << total_value << "\n"
          << "unique_diseases=" << by_disease.size() << "\n"
          << "unique_cie10=" << by_cie10.size() << "\n"
          << "jurisdictions=" << jurisdictions.size() << "\n"
          << "years=" << years.size() << "\n"
          << "weeks=" << weeks.size() << "\n"
          << "missing_disease=" << missing_disease << "\n"
          << "missing_cie10=" << missing_cie10 << "\n"
          << "synthetic_disease=" << synthetic_disease << "\n";
    audit << "periods:\n";
    for (const auto& kv : by_period) audit << "  " << kv.first << "=" << kv.second << "\n";
    audit << "sexes:\n";
    for (const auto& kv : by_sex) audit << "  " << kv.first << "=" << kv.second << "\n";
    audit << "records_by_year:\n";
    for (const auto& kv : by_year) audit << "  " << kv.first << "=" << kv.second << "\n";
    audit << "top_diseases:\n";
    const int limit = std::min(top, static_cast<int>(disease_rank.size()));
    for (int i = 0; i < limit; ++i) {
        audit << "  " << disease_rank[static_cast<size_t>(i)].first
              << "=" << disease_rank[static_cast<size_t>(i)].second << "\n";
    }
    const std::string audit_text = audit.str();
    std::cout << audit_text;
    if (report_path.empty()) report_path = fs::current_path() / "Build" / "epi_audit_last.txt";
    try {
        ensure_dir(report_path.parent_path());
        std::ofstream out(report_path, std::ios::binary | std::ios::trunc);
        out << audit_text;
        out.flush();
    } catch (...) {
    }
    if (records == 0) return 31;
    if (missing_disease > 0) return 32;
    if (synthetic_disease > 0) return 33;
    return 0;
}




static int run_ixiptlah_purge_epi_file_cli(int argc, char** argv) {


    if (argc < 3) throw std::runtime_error("Uso: Tlalpowa.exe ixiptlah-purge-epi-file <archivo.ixiptlah>");

    fs::path file = fs::path(widen_utf8(clean_user_path_string(argv[2])));


    const IxiptlahRewriteStats stats = ixiptlah_rewrite_without_records(file,


        [](IxiptlahRecordType type, std::uint32_t schema) {


            return schema == 1 &&


                (type == IxiptlahRecordType::EpidemiologyObservation ||


                 type == IxiptlahRecordType::MonthlyEpidemiologyBatch);
        });


    std::cout << "IXIPTLAH_PURGE_EPI_FILE\n"

              << "file=" << path_utf8(file) << "\n"
              << "kept=" << stats.kept << "\n"
              << "removed=" << stats.removed << "\n"

              << "unreadable=" << stats.unreadable << "\n"
              << "rewritten=" << (stats.rewritten ? 1 : 0) << "\n";

    return stats.rewritten ? 0 : 2;
}




extern "C" int tlalpowa_execute_command(TlalpowaCommand command, int argc, char** argv) {
    install_crash_guards();

    raise_process_priority();
    try {
        switch (command) {
        case TLALPOWA_COMMAND_DEFAULT: {
            const int modern = epi::run_tlalpowa_app();
            if (modern == 0) return 0;
            return epi::run_gui_launcher();
        }
        case TLALPOWA_COMMAND_SELFTEST:
            return selftest();
        case TLALPOWA_COMMAND_ATMOSPHERE:
            return run_atmosphere_prepare_cli(argc, argv);
        case TLALPOWA_COMMAND_ATMOSPHERE_WEB:
            return run_atmosphere_web_cli(argc, argv);
        case TLALPOWA_COMMAND_EPI_WEB:
            return run_epi_web_cli(argc, argv);
        case TLALPOWA_COMMAND_EXTERNAL_SMOKE:
            return run_external_smoke_cli(argc, argv);
        case TLALPOWA_COMMAND_SATELLITE_WEB:
            return run_satellite_web_cli(argc, argv);
        case TLALPOWA_COMMAND_EPI_AUDIT:
            return run_epi_audit_cli(argc, argv);
        case TLALPOWA_COMMAND_IXIPTLAH_PURGE_EPI_FILE:
            return run_ixiptlah_purge_epi_file_cli(argc, argv);
        case TLALPOWA_COMMAND_LAUNCHER:
            return epi::run_interactive_launcher();
        case TLALPOWA_COMMAND_GUI:
            return epi::run_gui_launcher();
        case TLALPOWA_COMMAND_APP:
            return epi::run_tlalpowa_app();
        case TLALPOWA_COMMAND_RUN: {
#ifdef _WIN32
            SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
            auto options = parse_options(argc, argv);
            Pipeline p(options);
            return p.run();
        }
        case TLALPOWA_COMMAND_UNKNOWN:
        default:
            print_help();
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";

        return 2;
    }
}

// ===== Nucleos/PdfExtractor.impl =====
#line 1 "Nucleos/PdfExtractor.impl"








#include <atomic>
#include <cstring>
#include <future>

#include <mutex>
#include <thread>



namespace epi {



namespace {





int env_int_clamped_pdf(const char* name, int fallback, int lo, int hi) {
    const std::string raw = getenv_utf8_or_empty(name);


    if (raw.empty()) return std::clamp(fallback, lo, hi);
    try { return std::clamp(std::stoi(raw), lo, hi); } catch (...) { return std::clamp(fallback, lo, hi); }
}

bool keep_bbox_debug_html_pdf() {
    static const bool keep =
        env_int_clamped_pdf("TLALPOWA_KEEP_BBOX_HTML", 0, 0, 1) != 0 ||
        env_int_clamped_pdf("TLALPOWA_DEBUG_BBOX_HTML", 0, 0, 1) != 0;
    return keep;
}

void cleanup_bbox_debug_html_pdf(const fs::path& html) {
    if (keep_bbox_debug_html_pdf()) return;
    std::error_code ec;
    fs::remove(html, ec);
}





PageText placeholder_page(int page_no) {
    PageText p;
    p.page = page_no;

    p.width = 612.0;


    p.height = 792.0;


    return p;
}




size_t count_page_tokens_pdf(const std::vector<PageText>& pages) {
    size_t n = 0;


    for (const auto& pg : pages) n += pg.tokens.size();


    return n;
}

}





PdfDocument PdfTextExtractor::extract(const fs::path& pdf, const fs::path& work_dir) const {
    PdfDocument doc;


    doc.pdf_path = pdf;


    doc.file_name = path_utf8(pdf.filename());


    doc.stable_id = simple_hash_hex(path_utf8(pdf) + "|" + std::to_string(file_size_or_zero(pdf)));
    const int total_pages = tools_.pdf_page_count(pdf);
    const int configured_front_skip = std::max(0, tools_.options().skip_front_pages);

    const int configured_back_skip = std::max(0, tools_.options().skip_back_pages);



    const bool total_pages_known = total_pages > 0;
    int first_page = total_pages_known ? configured_front_skip + 1 : 1;
    int last_page = total_pages_known ? total_pages - configured_back_skip : 0;

    doc.source_page_count = total_pages;
    doc.first_extracted_page = first_page;


    doc.last_extracted_page = last_page;



    if (!total_pages_known) {


        log_.warn("No se obtuvo conteo pdfinfo para " + path_utf8(pdf.filename()) + "; se enviara el PDF completo a pdftotext y el filtrado se hara despues, evitando rangos vacios.");
    }




    if (total_pages_known && first_page > last_page) {
        log_.warn("El boletin tiene " + std::to_string(total_pages) + " paginas; el salto configurado lo dejaria vacio. Se procesara desde la pagina 1 hasta la penultima disponible.");
        first_page = 1;

        last_page = std::max(1, total_pages - configured_back_skip);
        doc.first_extracted_page = first_page;
        doc.last_extracted_page = last_page;
    }




    if (last_page >= first_page && total_pages > 0) {




        const int chunk_pages = env_int_clamped_pdf("TLALPOWA_BBOX_CHUNK_PAGES", 6, 1, 24);


        const int auto_workers = adaptive_compute_worker_budget();
        const int bbox_workers = env_int_clamped_pdf(
            "TLALPOWA_BBOX_WORKERS", auto_workers, 1, auto_workers);


        struct ChunkJob { int first = 0; int last = 0; };


        struct ChunkResult { int first = 0; int last = 0; std::vector<PageText> pages; std::string error; };


        std::vector<ChunkJob> jobs;


        for (int f = first_page; f <= last_page; f += chunk_pages) jobs.push_back({f, std::min(last_page, f + chunk_pages - 1)});


        std::vector<ChunkResult> results(jobs.size());


        std::atomic<size_t> next_job{0};


        std::vector<std::thread> workers;


        workers.reserve(static_cast<size_t>(bbox_workers));


        for (int wi = 0; wi < bbox_workers; ++wi) {


            workers.emplace_back([&, wi]() {
                (void)wi;


                for (;;) {
                    const size_t k = next_job.fetch_add(1);


                    if (k >= jobs.size()) break;
                    const int f = jobs[k].first;
                    const int l = jobs[k].last;

                    ChunkResult cr;


                    cr.first = f;
                    cr.last = l;


                    const fs::path html = work_dir / (safe_filename(doc.stable_id + "_bbox_f" + std::to_string(f) + "_l" + std::to_string(l)) + ".html");


                    auto parse_or_throw = [&](int pf, int pl, const fs::path& target_html) -> std::vector<PageText> {


                        const bool cached_bbox_ready = fs::exists(target_html) && file_size_or_zero(target_html) > 256;


                        if (!cached_bbox_ready) {


                            auto pr = tools_.run_pdftotext_bbox(pdf, target_html, pf, pl);


                            if (pr.exit_code != 0) throw std::runtime_error("pdftotext fragmento " + std::to_string(pf) + "-" + std::to_string(pl) + " codigo " + std::to_string(pr.exit_code) + ": " + pr.command_for_log);
                        }


                        auto parsed_pages = parse_bbox_layout(target_html, pf - 1);
                        cleanup_bbox_debug_html_pdf(target_html);
                        return parsed_pages;
                    };


                    try {


                        cr.pages = parse_or_throw(f, l, html);


                    } catch (const std::exception& e) {
                        cr.error = e.what();




                        if (f < l) {


                            std::vector<std::string> page_errors;


                            for (int pno = f; pno <= l; ++pno) {


                                const fs::path page_html = work_dir / (safe_filename(doc.stable_id + "_bbox_p" + std::to_string(pno)) + ".html");
                                try {


                                    auto one = parse_or_throw(pno, pno, page_html);


                                    for (auto& recovered_page : one) cr.pages.push_back(std::move(recovered_page));


                                } catch (const std::exception& pe) {


                                    page_errors.push_back(std::to_string(pno) + ":" + pe.what());
                                }
                            }


                            if (!page_errors.empty()) {
                                cr.error += " | recuperacion pagina-a-pagina con fallos=" + std::to_string(page_errors.size());
                            }
                        }
                    }




                    std::map<int, PageText> by_page;


                    for (auto& pg : cr.pages) by_page[pg.page] = std::move(pg);


                    cr.pages.clear();


                    for (int pno = f; pno <= l; ++pno) {
                        auto it = by_page.find(pno);


                        if (it != by_page.end()) cr.pages.push_back(std::move(it->second));


                        else cr.pages.push_back(placeholder_page(pno));
                    }
                    results[k] = std::move(cr);
                }

            });
        }


        for (auto& w : workers) if (w.joinable()) w.join();


        for (const auto& r : results) {


            if (!r.error.empty()) log_.warn("Fragmento bbox degradado " + std::to_string(r.first) + "-" + std::to_string(r.last) + ": " + r.error);


            for (const auto& pg : r.pages) doc.pages.push_back(pg);
        }
        std::sort(doc.pages.begin(), doc.pages.end(), [](const PageText& a, const PageText& b){ return a.page < b.page; });

    } else {



        std::string full_error;


        const fs::path html = work_dir / (safe_filename(doc.stable_id + "_bbox_full_no_pdfinfo") + ".html");
        try {


            const bool cached_bbox_ready = fs::exists(html) && file_size_or_zero(html) > 256;


            if (cached_bbox_ready) {


                log_.info("XHTML bbox completo reutilizado desde cache de trabajo: " + path_utf8(html));
            } else {


                auto pr = tools_.run_pdftotext_bbox(pdf, html, 1, 0);


                if (pr.exit_code != 0) {


                    throw std::runtime_error("pdftotext completo codigo " + std::to_string(pr.exit_code) + ": " + pr.command_for_log);
                }
            }


            doc.pages = parse_bbox_layout(html, 0);
            cleanup_bbox_debug_html_pdf(html);


        } catch (const std::exception& e) {
            full_error = e.what();


            doc.pages.clear();
        }



        if (doc.pages.empty() || count_page_tokens_pdf(doc.pages) == 0) {


            if (!full_error.empty()) {


                log_.warn("pdftotext completo sin pdfinfo no produjo paginas utiles para " + path_utf8(pdf.filename()) + ": " + full_error + "; se activara sondeo por pagina.");
            } else {


                log_.warn("pdftotext completo sin pdfinfo produjo XHTML sin tokens para " + path_utf8(pdf.filename()) + "; se activara sondeo por pagina.");
            }



            std::vector<PageText> probed_pages;
            const int probe_max = env_int_clamped_pdf("TLALPOWA_UNKNOWN_PDF_PROBE_MAX_PAGES", 90, 8, 260);
            const int miss_limit = env_int_clamped_pdf("TLALPOWA_UNKNOWN_PDF_PROBE_CONSECUTIVE_MISSES", 4, 2, 12);

            int consecutive_misses = 0;


            for (int pno = 1; pno <= probe_max; ++pno) {


                const fs::path page_html = work_dir / (safe_filename(doc.stable_id + "_bbox_probe_p" + std::to_string(pno)) + ".html");
                try {


                    const bool cached_page_ready = fs::exists(page_html) && file_size_or_zero(page_html) > 128;


                    if (!cached_page_ready) {


                        auto pr = tools_.run_pdftotext_bbox(pdf, page_html, pno, pno);


                        if (pr.exit_code != 0) throw std::runtime_error("codigo " + std::to_string(pr.exit_code) + ": " + pr.command_for_log);
                    }


                    auto one = parse_bbox_layout(page_html, pno - 1);
                    cleanup_bbox_debug_html_pdf(page_html);


                    if (one.empty()) {
                        ++consecutive_misses;


                    } else {
                        consecutive_misses = 0;


                        for (auto& pg : one) {


                            if (pg.page <= 0) pg.page = pno;


                            probed_pages.push_back(std::move(pg));
                        }
                    }


                } catch (const std::exception& e) {
                    ++consecutive_misses;


                    if (pno <= 3) log_.warn("Sondeo bbox pagina " + std::to_string(pno) + " fallo en " + path_utf8(pdf.filename()) + ": " + e.what());
                }


                if (!probed_pages.empty() && consecutive_misses >= miss_limit) break;
            }


            if (!probed_pages.empty()) {
                std::sort(probed_pages.begin(), probed_pages.end(), [](const PageText& a, const PageText& b){ return a.page < b.page; });
                doc.pages = std::move(probed_pages);

                doc.last_extracted_page = doc.pages.empty() ? 0 : doc.pages.back().page;


                log_.info("Sondeo por pagina recupero " + std::to_string(doc.pages.size()) + " paginas bbox para " + path_utf8(pdf.filename()));


            } else if (!full_error.empty()) {


                log_.warn("pdftotext bbox no produjo paginas parseables ni en modo completo ni por sondeo para " + path_utf8(pdf.filename()) + ": " + full_error + "; se intentara respaldo textual -layout antes de declarar el PDF sin paginas.");
            }
        }
    }



    if (total_pages_known && !doc.pages.empty() && count_page_tokens_pdf(doc.pages) == 0) {



        try {


            const fs::path full_html = work_dir / (safe_filename(doc.stable_id + "_bbox_full_retry_known_pages") + ".html");


            if (!fs::exists(full_html) || file_size_or_zero(full_html) <= 256) {


                auto pr = tools_.run_pdftotext_bbox(pdf, full_html, 1, 0);


                if (pr.exit_code != 0) throw std::runtime_error("codigo " + std::to_string(pr.exit_code) + ": " + pr.command_for_log);
            }


            auto full_pages = parse_bbox_layout(full_html, 0);
            cleanup_bbox_debug_html_pdf(full_html);


            if (!full_pages.empty() && count_page_tokens_pdf(full_pages) > 0) {
                doc.pages = std::move(full_pages);


                doc.first_extracted_page = 1;
                doc.last_extracted_page = doc.pages.empty() ? 0 : doc.pages.back().page;


                log_.info("Pasada bbox completa rescato tokens despues de fragmentos vacios: " + path_utf8(pdf.filename()));
            }


        } catch (const std::exception& e) {


            log_.warn("Pasada bbox completa de rescate fallo en " + path_utf8(pdf.filename()) + ": " + e.what());
        }
    }



    if (doc.pages.empty() || count_page_tokens_pdf(doc.pages) == 0) {



        try {
            const int layout_first = total_pages_known ? std::max(1, first_page) : 1;


            const int layout_last = (total_pages_known && last_page >= layout_first) ? last_page : 0;


            const fs::path layout_txt = work_dir / (safe_filename(doc.stable_id + "_layout_f" + std::to_string(layout_first) + "_l" + std::to_string(layout_last)) + ".txt");


            if (!fs::exists(layout_txt) || file_size_or_zero(layout_txt) == 0) {
                auto pr = tools_.run_pdftotext_layout(pdf, layout_txt, layout_first, layout_last);


                if (pr.exit_code != 0) throw std::runtime_error("pdftotext -layout codigo " + std::to_string(pr.exit_code) + ": " + pr.command_for_log);
            }


            auto layout_pages = parse_plain_layout_text(layout_txt, layout_first - 1);


            if (!layout_pages.empty() && count_page_tokens_pdf(layout_pages) > 0) {
                doc.pages = std::move(layout_pages);
                doc.first_extracted_page = doc.pages.front().page;

                doc.last_extracted_page = doc.pages.back().page;


                log_.warn("Respaldo textual -layout activo: " + path_utf8(pdf.filename()) + " entro al conversor con " + std::to_string(doc.pages.size()) + " paginas sinteticas por monoespaciado.");
            } else {


                log_.warn("Respaldo textual -layout tampoco produjo tokens para " + path_utf8(pdf.filename()));
            }


        } catch (const std::exception& e) {


            log_.warn("Respaldo textual -layout fallo en " + path_utf8(pdf.filename()) + ": " + e.what());
        }
    }



    if (configured_front_skip > 0 || configured_back_skip > 0) {
        log_.info("Filtro Poppler aplicado: primeras " + std::to_string(configured_front_skip) + " paginas omitidas" +
                  (total_pages > 0 ? ", ultima(s) " + std::to_string(configured_back_skip) + " omitida(s), total=" + std::to_string(total_pages) : ", sin pdfinfo para contar total; la ultima se omitira en el pipeline si fue necesario"));
    }


    auto [year, week] = infer_year_week(pdf, doc.pages);


    doc.bulletin_year = year;


    doc.bulletin_week = week;


    return doc;
}





static std::optional<double> attr_double_range(const std::string& s, size_t begin, size_t end, const char* name) {
    double value = 0.0;


    if (ozmvm_attr_double_span(s.data(), begin, end, name, &value)) return value;


    return std::nullopt;
}





std::vector<PageText> PdfTextExtractor::parse_bbox_layout(const fs::path& html, int page_number_offset) const {


    const std::string s = read_text_file(html);


    std::vector<PageText> pages;
    size_t pos = 0;
    int page_no = 0;


    while (true) {
        const size_t p0 = s.find("<page", pos);


        if (p0 == std::string::npos) break;
        const size_t tag_end = s.find('>', p0);


        if (tag_end == std::string::npos) break;
        const size_t p1 = s.find("</page>", tag_end);


        if (p1 == std::string::npos) break;
        ++page_no;
        const int logical_page = page_number_offset + page_no;

        PageText page;
        page.page = logical_page;
        page.width = attr_double_range(s, p0, tag_end, "width").value_or(0.0);

        page.height = attr_double_range(s, p0, tag_end, "height").value_or(0.0);
        size_t wpos = tag_end + 1;


        while (true) {
            const size_t w0 = s.find("<word", wpos);


            if (w0 == std::string::npos || w0 >= p1) break;


            const size_t wtag_end = s.find('>', w0);


            if (wtag_end == std::string::npos || wtag_end >= p1) break;
            const size_t w1 = s.find("</word>", wtag_end);


            if (w1 == std::string::npos || w1 > p1) break;
            Token t;
            t.page = logical_page;

            auto x0 = attr_double_range(s, w0, wtag_end, "xMin");
            auto y0 = attr_double_range(s, w0, wtag_end, "yMin");
            auto x1 = attr_double_range(s, w0, wtag_end, "xMax");

            auto y1 = attr_double_range(s, w0, wtag_end, "yMax");


            if (x0 && y0 && x1 && y1) {


                t.box = {*x0, *y0, *x1, *y1};
                t.text = html_unescape(s.substr(wtag_end + 1, w1 - wtag_end - 1));
                t.text = trim(t.text);


                if (!t.text.empty()) {


                    t.norm = normalize_key(t.text);


                    page.tokens.push_back(std::move(t));
                }
            }

            wpos = w1 + 7;
        }


        std::sort(page.tokens.begin(), page.tokens.end(), [](const Token& a, const Token& b) {


            if (std::abs(a.box.cy() - b.box.cy()) > 2.0) return a.box.cy() < b.box.cy();


            return a.box.cx() < b.box.cx();
        });


        pages.push_back(std::move(page));
        pos = p1 + 7;
    }


    if (pages.empty()) log_.warn("El XHTML de pdftotext no contiene páginas parseables: " + path_utf8(html));


    return pages;
}





std::vector<PageText> PdfTextExtractor::parse_plain_layout_text(const fs::path& text_file, int page_number_offset) const {


    const std::string raw = read_text_file(text_file);


    std::vector<PageText> pages;


    if (raw.empty()) return pages;




    std::vector<std::string> page_chunks;
    std::string cur;


    cur.reserve(raw.size());


    for (char ch : raw) {


        if (ch == '\f') {


            page_chunks.push_back(cur);


            cur.clear();


        } else if (ch != '\r') {


            cur.push_back(ch);
        }
    }


    page_chunks.push_back(cur);

    const double left = 24.0;

    const double top = 24.0;
    const double char_w = 4.85;
    const double line_h = 10.75;

    int logical_index = 0;


    for (const std::string& chunk : page_chunks) {
        ++logical_index;
        PageText page;


        page.page = page_number_offset + logical_index;


        std::vector<std::string> lines;
        std::string line;
        std::istringstream in(chunk);

        size_t max_cols = 0;


        while (std::getline(in, line)) {


            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            max_cols = std::max(max_cols, line.size());


            lines.push_back(line);
        }
        page.width = std::max(612.0, left * 2.0 + static_cast<double>(std::max<size_t>(1, max_cols)) * char_w);


        page.height = std::max(792.0, top * 2.0 + static_cast<double>(std::max<size_t>(1, lines.size())) * line_h);


        for (size_t r = 0; r < lines.size(); ++r) {
            const std::string& ln = lines[r];
            size_t c = 0;


            while (c < ln.size()) {


                while (c < ln.size() && std::isspace(static_cast<unsigned char>(ln[c]))) ++c;
                const size_t start = c;


                while (c < ln.size() && !std::isspace(static_cast<unsigned char>(ln[c]))) ++c;


                if (c <= start) continue;
                std::string txt = trim(html_unescape(ln.substr(start, c - start)));


                if (txt.empty()) continue;


                Token t;
                t.page = page.page;
                t.text = std::move(txt);


                t.norm = normalize_key(t.text);
                const double x0 = left + static_cast<double>(start) * char_w;
                const double x1 = left + static_cast<double>(c) * char_w;

                const double y0 = top + static_cast<double>(r) * line_h;
                t.box = {x0, y0, x1, y0 + line_h * 0.92};


                page.tokens.push_back(std::move(t));
            }
        }


        if (!page.tokens.empty()) {


            std::sort(page.tokens.begin(), page.tokens.end(), [](const Token& a, const Token& b) {


                if (std::abs(a.box.cy() - b.box.cy()) > 2.0) return a.box.cy() < b.box.cy();


                return a.box.cx() < b.box.cx();
            });


            pages.push_back(std::move(page));
        }
    }


    if (pages.empty()) log_.warn("El TXT layout de pdftotext no contiene paginas con tokens: " + path_utf8(text_file));


    return pages;
}




static int first_year_20xx_pdf(const std::string& text) {


    return ozmvm_first_year_20xx(text.data(), text.size());
}




static bool valid_epi_week_pdf(int week) {


    return week >= 1 && week <= 53;
}





static int week_from_compact_or_spaced_marker_pdf(const std::string& text) {


    const std::string norm = normalize_key(text);


    if (norm.empty()) return 0;



    auto regex_week = [&](const std::regex& re) -> int {
        std::smatch m;


        if (!std::regex_search(norm, m, re) || m.size() < 2) return 0;
        try {
            const int w = std::stoi(m[1].str());


            return valid_epi_week_pdf(w) ? w : 0;
        } catch (...) { return 0; }
    };



    static const std::regex se_re(R"((?:^| )se ?0*([1-9]|[1-4][0-9]|5[0-3])(?: |$))", std::regex::icase);
    static const std::regex semana_re(R"((?:^| )(?:sem|semana|numero|num|nro|no) ?0*([1-9]|[1-4][0-9]|5[0-3])(?: |$))", std::regex::icase);
    static const std::regex spaced_se_re(R"((?:^| )s e ?0*([1-9]|[1-4][0-9]|5[0-3])(?: |$))", std::regex::icase);

    if (int w = regex_week(se_re)) return w;


    if (int w = regex_week(semana_re)) return w;


    if (int w = regex_week(spaced_se_re)) return w;


    return 0;
}




static int number_immediately_after_marker(const std::string& text, const char* marker) {


    return ozmvm_week_after_marker(text.data(), text.size(), marker);
}




static int week_before_del_year(const std::string& text) {


    return ozmvm_week_before_del_year(text.data(), text.size());
}





std::pair<int,int> PdfTextExtractor::infer_year_week(const fs::path& pdf, const std::vector<PageText>& pages) {


    const std::string filename_text = path_utf8(pdf.filename());
    const std::string parent_text = path_utf8(pdf.parent_path().filename());
    const std::string filename_norm = normalize_key(filename_text);
    const std::string parent_norm = normalize_key(parent_text);
    const int filename_year = first_year_20xx_pdf(filename_norm);
    const int parent_year = first_year_20xx_pdf(parent_norm);
    int filename_week = week_from_compact_or_spaced_marker_pdf(filename_text);
    if (filename_week == 0) filename_week = number_immediately_after_marker(filename_norm, "semana");
    if (filename_week == 0) filename_week = number_immediately_after_marker(filename_norm, "semanal");
    if (filename_week == 0) filename_week = number_immediately_after_marker(filename_norm, "sem");
    if (filename_week == 0) filename_week = number_immediately_after_marker(filename_norm, "se");
    if (filename_week == 0) filename_week = week_before_del_year(filename_norm);
    std::string all = filename_text + " " + parent_text + " ";


    for (size_t i = 0; i < std::min<size_t>(pages.size(), 15); ++i) {


        for (const auto& t : pages[i].tokens) all += t.text + " ";
    }


    const std::string norm = normalize_key(all);


    int year = 0;


    int week = 0;


    year = filename_year > 0 ? filename_year : (parent_year > 0 ? parent_year : first_year_20xx_pdf(norm));


    week = valid_epi_week_pdf(filename_week) ? filename_week : week_from_compact_or_spaced_marker_pdf(norm);


    if (week == 0) week = number_immediately_after_marker(norm, "semana");


    if (week == 0) {


        std::string file_text = path_utf8(pdf.parent_path().filename()) + " " + path_utf8(pdf.filename());


        std::string file = normalize_key(file_text);


        week = week_from_compact_or_spaced_marker_pdf(file_text);


        if (week == 0) week = number_immediately_after_marker(file, "semana");


        if (week == 0) week = number_immediately_after_marker(file, "semanal");


        if (week == 0) week = number_immediately_after_marker(file, "sem");


        if (week == 0) week = number_immediately_after_marker(file, "se");


        if (week == 0) week = number_immediately_after_marker(file, "numero");


        if (week == 0) week = number_immediately_after_marker(file, "num");


        if (week == 0) week = number_immediately_after_marker(file, "nro");


        if (week == 0) week = number_immediately_after_marker(file, "no");


        if (week == 0) week = week_before_del_year(file);
    }


    if (!valid_epi_week_pdf(week)) week = 0;


    return {year, week};
}

}

// ===== Nucleos/Pipeline.impl =====
#line 1 "Nucleos/Pipeline.impl"








#include <chrono>
#include <thread>
#include <atomic>

#include <cstdlib>
#include <memory>
#include <future>

#include <vector>
#include <system_error>



namespace epi {



namespace {





int env_int_clamped_pipeline(const char* name, int fallback, int lo, int hi) {
    const std::string raw = getenv_utf8_or_empty(name);


    if (raw.empty()) return std::clamp(fallback, lo, hi);
    try { return std::clamp(std::stoi(raw), lo, hi); } catch (...) { return std::clamp(fallback, lo, hi); }
}




bool env_flag_enabled_pipeline(const char* name) {
    const std::string raw = getenv_utf8_or_empty(name);


    if (raw.empty()) return false;
    const std::string v = lower_ascii(trim(raw));


    return v == "1" || v == "true" || v == "on" || v == "yes" || v == "si";
}




fs::path pipeline_runtime_dir(const AppOptions& options) {


    if (!options.runtime_dir.empty()) return options.runtime_dir;


    fs::path base = options.output_dir.empty() ? project_root() : options.output_dir.parent_path();


    if (base.empty()) base = project_root();


    return base / "Build" / "runtime" / "pipeline_epidemiologia";
}





fs::path pipeline_log_file(const AppOptions& options) {


    if (!options.log_dir.empty()) return options.log_dir / "Tlalpowa.log";


    return pipeline_runtime_dir(options) / "pipeline.log";
}





std::vector<fs::path> resolve_effective_pdf_queue_for_pipeline(const AppOptions& options, Logger& log) {


    std::vector<fs::path> pdfs;


    if (!options.explicit_pdfs.empty()) {


        pdfs.reserve(options.explicit_pdfs.size());


        for (const auto& raw : options.explicit_pdfs) {
            try {


                const fs::path p = resolve_existing_path_relaxed(raw);


                if (p.empty()) continue;


                if (file_size_or_zero(p) == 0) {


                    log.warn("PDF explícito ignorado por tamaño cero o inaccesible: " + path_utf8(p));


                    continue;
                }


                pdfs.push_back(p);


            } catch (const std::exception& e) {
                log.warn(std::string("PDF explícito ignorado por excepción: ") + e.what());


            } catch (...) {
                log.warn("PDF explícito ignorado por excepción desconocida.");
            }
        }

        log.info("Cola explícita recibida desde UI: " + std::to_string(options.explicit_pdfs.size()) + " candidato(s), " + std::to_string(pdfs.size()) + " utilizable(s).");
    }


    if (pdfs.empty()) {
        pdfs = list_pdfs_recursive(options.input_dir);
        log.info("Cola PDF enumerada desde carpeta: " + std::to_string(pdfs.size()) + " documento(s).");
    }


    std::sort(pdfs.begin(), pdfs.end(), [](const fs::path& a, const fs::path& b) {


        return path_utf8(a.lexically_normal()) < path_utf8(b.lexically_normal());
    });


    pdfs.erase(std::unique(pdfs.begin(), pdfs.end(), [](const fs::path& a, const fs::path& b) {


        return normalize_key(path_utf8(a.lexically_normal())) == normalize_key(path_utf8(b.lexically_normal()));
    }), pdfs.end());


    return pdfs;
}





void update_detail_progress(PipelineStats& stats, int done, int total) {
    stats.detail_total = std::max(1, total);
    stats.detail_done = std::clamp(done, 0, stats.detail_total);

    stats.progress = static_cast<double>(stats.detail_done) / static_cast<double>(stats.detail_total);
}




fs::path processed_manifest(const AppOptions& options) {


    return pipeline_runtime_dir(options) / "processed_pdfs.ixiptlah";
}




fs::path processed_pages_manifest(const AppOptions& options) {


    return pipeline_runtime_dir(options) / "processed_pages.ixiptlah";
}




fs::path live_preview_manifest(const AppOptions& options) {


    return pipeline_runtime_dir(options) / "live_preview.ixiptlah";
}





fs::path preview_prefix_from_options(const AppOptions& options, const std::string& stable_id) {


    return pipeline_runtime_dir(options) / ("preview_" + safe_filename(stable_id));
}





std::string preview_image_extension_from_env() {



    const std::string raw = getenv_utf8_or_empty("TLALPOWA_PREVIEW_FORMAT");
    const std::string v = raw.empty() ? std::string{} : lower_ascii(trim(raw));


    if (v == "png") return ".png";


    return ".jpg";
}





std::string preview_raster_mode_from_env() {
    const std::string raw = getenv_utf8_or_empty("TLALPOWA_PREVIEW_RASTER_MODE");




    if (raw.empty()) return "live";


    return lower_ascii(trim(raw));
}




fs::path preview_image_path_from_options(const AppOptions& options, const std::string& stable_id, int page) {


    fs::path p = preview_prefix_from_options(options, stable_id);
    p += "-" + std::to_string(page) + preview_image_extension_from_env();


    return p;
}




bool preview_raster_enabled_by_env() {
    const std::string v = preview_raster_mode_from_env();


    if (v == "0" || v == "false" || v == "off" || v == "no" || v == "vector" || v == "bbox" || v == "direct") return false;


    return v == "1" || v == "true" || v == "on" || v == "yes" || v == "si" ||
           v == "jpg" || v == "jpeg" || v == "png" || v == "auto" || v == "live" ||
           v == "live20" || v == "prefetch20" || v == "bulk";
}




bool preview_bulk_prefetch_enabled_by_env() {
    const std::string v = preview_raster_mode_from_env();




    return v == "1" || v == "true" || v == "on" || v == "yes" || v == "si" ||


           v == "jpg" || v == "jpeg" || v == "png" || v == "auto" || v == "live" ||
           v == "live20" || v == "prefetch20" || v == "bulk" ||
           env_flag_enabled_pipeline("TLALPOWA_PREVIEW_BULK_PREFETCH");
}





void prune_live_preview_rasters(const fs::path& output_dir) {



    const int keep = env_int_clamped_pipeline("TLALPOWA_PREVIEW_KEEP_RASTERS", 48, 2, 512);
    std::error_code ec;


    if (output_dir.empty() || !fs::exists(output_dir, ec) || ec) return;


    struct Item { fs::path p; fs::file_time_type t; std::uintmax_t size = 0; };


    std::vector<Item> items;


    for (fs::directory_iterator it(output_dir, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {


        std::error_code iec;


        if (!it->is_regular_file(iec) || iec) { ec.clear(); continue; }


        const std::string name = path_utf8(it->path().filename());


        const std::string ext = lower_ascii(path_utf8(it->path().extension()));


        if (name.rfind("preview_", 0) == 0 && (ext == ".jpg" || ext == ".jpeg" || ext == ".png")) {


            items.push_back({it->path(), fs::last_write_time(it->path(), iec), it->file_size(iec)});
        }


        ec.clear();
    }


    if (static_cast<int>(items.size()) <= keep) return;


    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {


        if (a.t != b.t) return a.t > b.t;


        return a.size > b.size;
    });


    for (size_t i = static_cast<size_t>(keep); i < items.size(); ++i) {
        std::error_code rec;
        fs::remove(items[i].p, rec);
    }
}




fs::path legacy_preview_png_path_from_options(const AppOptions& options, const std::string& stable_id, int page) {


    fs::path p = preview_prefix_from_options(options, stable_id);
    p += "-" + std::to_string(page) + ".png";


    return p;
}



void schedule_detached_preview_render(AppOptions options,


                                      fs::path pdf,
                                      std::string stable_id,
                                      int first_page,

                                      int last_page,


                                      bool single_page_priority) {


    if (!options.render_pages || !preview_raster_enabled_by_env() || first_page <= 0 || last_page < first_page) return;


    if (!single_page_priority && !preview_bulk_prefetch_enabled_by_env()) return;


    static std::atomic<int> active_preview_jobs{0};


    static std::mutex scheduled_mu;


    static std::set<std::string> scheduled_ranges;
    const int max_preview_jobs = env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_ACTIVE_JOBS", 1, 1, 12);
    const std::string schedule_key = stable_id + ":" + std::to_string(first_page) + ":" + std::to_string(last_page) + ":" + preview_image_extension_from_env();


    if (!single_page_priority) {


        std::lock_guard<std::mutex> lk(scheduled_mu);


        if (scheduled_ranges.find(schedule_key) != scheduled_ranges.end()) return;


        scheduled_ranges.insert(schedule_key);


        if (scheduled_ranges.size() > 4096) scheduled_ranges.clear();
    }




    std::thread([options = std::move(options), pdf = std::move(pdf), stable_id = std::move(stable_id), first_page, last_page, single_page_priority, max_preview_jobs]() mutable {




        int observed = active_preview_jobs.load();


        while (true) {
            observed = active_preview_jobs.load();


            if (observed < max_preview_jobs && active_preview_jobs.compare_exchange_weak(observed, observed + 1)) break;


            std::this_thread::sleep_for(std::chrono::milliseconds(single_page_priority ? 12 : 35));
        }


        struct JobGuard { std::atomic<int>& n; ~JobGuard(){ n.fetch_sub(1); } } guard{active_preview_jobs};
        try {
            ExternalTools tools(options);


            const fs::path prefix = preview_prefix_from_options(options, stable_id);




            if (!single_page_priority) {


                const int mid = first_page + (last_page - first_page) / 2;


                auto ready = [&](int pg) {


                    return fs::exists(preview_image_path_from_options(options, stable_id, pg)) ||


                           fs::exists(legacy_preview_png_path_from_options(options, stable_id, pg));
                };


                if (ready(first_page) && ready(mid) && ready(last_page)) return;
            }
            (void)tools.run_pdftoppm_pages_png(pdf, first_page, last_page, prefix);


            prune_live_preview_rasters(pipeline_runtime_dir(options));


        } catch (...) {

        }


    }).detach();
}




void prune_raster_preview_files_if_vector_mode(const AppOptions& options) {


    if (preview_raster_enabled_by_env()) { prune_live_preview_rasters(pipeline_runtime_dir(options)); return; }


    const fs::path runtime = pipeline_runtime_dir(options);
    std::error_code ec;


    if (runtime.empty() || !fs::exists(runtime, ec) || ec) return;
    int removed = 0;


    for (fs::directory_iterator it(runtime, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
        std::error_code item_ec;


        if (!it->is_regular_file(item_ec) || item_ec) continue;


        const std::string name = path_utf8(it->path().filename());


        const std::string ext = lower_ascii(path_utf8(it->path().extension()));


        if (name.rfind("preview_", 0) == 0 && (ext == ".jpg" || ext == ".jpeg" || ext == ".png")) {


            fs::remove(it->path(), item_ec);


            if (!item_ec && ++removed >= env_int_clamped_pipeline("TLALPOWA_PREVIEW_PRUNE_MAX_FILES", 25000, 100, 200000)) break;
        }
    }
}




int prune_token_audit_files(const fs::path& output_dir) {
    std::error_code ec;


    if (output_dir.empty() || !fs::exists(output_dir, ec) || ec) return 0;


    int removed = 0;


    for (fs::directory_iterator it(output_dir, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
        std::error_code item_ec;


        if (!it->is_regular_file(item_ec) || item_ec) continue;


        const std::string name = path_utf8(it->path().filename());


        const std::string ext = lower_ascii(path_utf8(it->path().extension()));


        if (name.rfind("tokens_", 0) == 0 && ext == ".jsonl") {


            fs::remove(it->path(), item_ec);


            if (!item_ec) ++removed;
        }
    }


    return removed;
}




fs::path run_state_file(const AppOptions& options) {


    return pipeline_runtime_dir(options) / "run_state.ixiptlah";
}




bool write_rect_payload(std::ostream& out, const Rect& r) {


    return ixiptlah_write_value(out, r.x0) &&


           ixiptlah_write_value(out, r.y0) &&


           ixiptlah_write_value(out, r.x1) &&


           ixiptlah_write_value(out, r.y1);
}




bool write_extraction_preview_payload(std::ostream& out, const ExtractionPreview& preview) {


    auto write_i32 = [&](int v) { return ixiptlah_write_value(out, v); };


    auto write_i64 = [&](int64_t v) { return ixiptlah_write_value(out, v); };


    auto write_f64 = [&](double v) { return ixiptlah_write_value(out, v); };


    auto write_string = [&](const std::string& s) { return ixiptlah_write_string(out, s); };


    auto write_count = [&](size_t n) {


        if (n > 1000000u) n = 1000000u;
        const std::uint32_t v = static_cast<std::uint32_t>(n);


        return ixiptlah_write_value(out, v);
    };



    if (!write_string(preview.pdf_file) ||


        !write_string(path_utf8(preview.page_image)) ||


        !write_i32(preview.pdf_index) ||


        !write_i32(preview.pdf_total) ||


        !write_i32(preview.page) ||


        !write_f64(preview.page_width) ||


        !write_f64(preview.page_height) ||


        !write_string(preview.status)) return false;



    if (!write_count(preview.tokens.size())) return false;


    for (const auto& t : preview.tokens) {


        if (!write_i32(t.page) ||


            !write_string(t.text) ||


            !write_string(t.norm) ||


            !write_rect_payload(out, t.box)) return false;
    }



    if (!write_count(preview.rows.size())) return false;


    for (const auto& r : preview.rows) {


        if (!write_string(r.jurisdiction_id) ||


            !write_string(r.jurisdiction) ||


            !write_rect_payload(out, r.label_box) ||


            !write_f64(r.y_mid) ||


            !write_f64(r.y0) ||


            !write_f64(r.y1) ||


            !write_i32(r.line_index)) return false;
    }



    if (!write_count(preview.columns.size())) return false;


    for (const auto& c : preview.columns) {


        if (!write_i32(c.index) ||


            !write_f64(c.x_mid) ||


            !write_f64(c.x0) ||


            !write_f64(c.x1) ||


            !write_string(c.period) ||


            !write_string(c.sex) ||


            !write_string(c.disease_id) ||


            !write_string(c.disease) ||


            !write_string(c.cie10) ||


            !write_string(c.source_year) ||


            !write_string(c.header_text) ||


            !write_string(c.role) ||


            !write_string(c.expected_role) ||


            !write_string(c.group_layout_note) ||


            !write_i32(c.group_index) ||


            !write_rect_payload(out, c.header_box) ||


            !write_rect_payload(out, c.disease_box) ||


            !write_rect_payload(out, c.cie10_box) ||


            !write_f64(c.header_confidence)) return false;
    }



    if (!write_count(preview.accepted.size())) return false;


    for (const auto& o : preview.accepted) {


        if (!write_string(o.jurisdiction) ||


            !write_string(o.disease_id) ||


            !write_string(o.disease) ||


            !write_string(o.cie10) ||


            !write_string(o.period) ||


            !write_string(o.sex) ||


            !write_i64(o.value)) return false;
    }



    if (!write_count(preview.quarantine.size())) return false;


    for (const auto& q : preview.quarantine) {


        if (!write_string(q.reason) || !write_string(q.detail)) return false;
    }


    return true;
}



void write_run_state(const AppOptions& options,
                     const std::string& status,
                     const PipelineStats& stats,

                     const std::string& detail = {}) {


    if (options.output_dir.empty()) return;


    const fs::path final_path = run_state_file(options);


    (void)ixiptlah_write_single_record_atomic(final_path, IxiptlahRecordType::RunState, 1, [&](std::ostream& out) {


        return ixiptlah_write_string(out, status) &&


               ixiptlah_write_string(out, now_utc_iso()) &&


               ixiptlah_write_value(out, stats.pdf_done) &&


               ixiptlah_write_value(out, stats.pdf_total) &&


               ixiptlah_write_value(out, stats.pages_done) &&


               ixiptlah_write_value(out, stats.pages_total) &&


               ixiptlah_write_value(out, stats.detail_done) &&


               ixiptlah_write_value(out, stats.detail_total) &&


               ixiptlah_write_value(out, stats.progress) &&


               ixiptlah_write_value(out, stats.tables_detected) &&


               ixiptlah_write_value(out, stats.observations_accepted) &&


               ixiptlah_write_value(out, stats.quarantine_items) &&


               ixiptlah_write_string(out, path_utf8(options.input_dir)) &&


               ixiptlah_write_string(out, path_utf8(options.output_dir)) &&


               ixiptlah_write_string(out, detail);
    });
}




std::string rect_json_field(const Rect& r) {


    return rect_to_json(r);
}



void append_json_field(std::ostringstream& os, const std::string& key, const std::string& value, bool comma = true) {


    os << '"' << key << "\":\"" << json_escape(value) << '"';


    if (comma) os << ',';
}



void append_json_field(std::ostringstream& os, const std::string& key, int value, bool comma = true) {
    os << '"' << key << "\":" << value;


    if (comma) os << ',';
}



void append_json_field(std::ostringstream& os, const std::string& key, int64_t value, bool comma = true) {
    os << '"' << key << "\":" << value;


    if (comma) os << ',';
}



void append_json_field(std::ostringstream& os, const std::string& key, double value, bool comma = true) {


    os << '"' << key << "\":" << std::fixed << std::setprecision(4) << value;


    if (comma) os << ',';
}





void write_live_preview(const AppOptions& options, const ExtractionPreview& preview) {


    if (options.output_dir.empty()) return;


    const fs::path final_path = live_preview_manifest(options);


    (void)ixiptlah_write_single_record_atomic(final_path, IxiptlahRecordType::LivePreview, 1, [&](std::ostream& out) {


        return write_extraction_preview_payload(out, preview);


    });
}




std::string page_stable_id(const std::string& pdf_id, int page) {


    return pdf_id + "|p" + std::to_string(page);
}





struct RuntimeTsvSink {


    std::ofstream out;


    std::chrono::steady_clock::time_point last_flush = std::chrono::steady_clock::now() - std::chrono::seconds(5);
};




std::mutex& runtime_tsv_mu() {


    static std::mutex mu;


    return mu;
}




std::unordered_map<std::string, std::unique_ptr<RuntimeTsvSink>>& runtime_tsv_sinks() {


    static std::unordered_map<std::string, std::unique_ptr<RuntimeTsvSink>> sinks;


    return sinks;
}



void append_runtime_tsv_line(const fs::path& path,
                             const std::string& header,
                             const std::string& line,

                             bool force_flush = false) {


    if (path.empty()) return;


    ensure_dir(path.parent_path());


    const std::string key = path_utf8(path);


    std::lock_guard<std::mutex> lk(runtime_tsv_mu());


    auto& sinks = runtime_tsv_sinks();
    auto it = sinks.find(key);


    if (it == sinks.end()) {


        const bool needs_header = !fs::exists(path) || file_size_or_zero(path) == 0;


        auto sink = std::make_unique<RuntimeTsvSink>();


        sink->out.open(path, std::ios::binary | std::ios::app);


        if (!sink->out) return;


        if (needs_header) sink->out << header << '\n';


        it = sinks.emplace(key, std::move(sink)).first;
    }
    it->second->out << line << '\n';


    const int min_ms = env_int_clamped_pipeline("TLALPOWA_RUNTIME_TSV_FLUSH_MS", 180, 50, 2000);


    const auto now = std::chrono::steady_clock::now();


    if (force_flush || std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->last_flush).count() >= min_ms) {
        it->second->out.flush();
        it->second->last_flush = now;
    }
}




bool page_has_epidemiological_table_signal(const PageText& page) {




    int signal = 0;


    for (const auto& t : page.tokens) {
        const std::string& n = t.norm;


        if (n.empty()) continue;


        if (contains_norm(n, "cuadro") || contains_norm(n, "enfermedad") || contains_norm(n, "diagnostico") ||
            contains_norm(n, "cie") || contains_norm(n, "sem") || contains_norm(n, "acum") ||


            contains_norm(n, "alcaldia") || contains_norm(n, "delegacion") || contains_norm(n, "jurisdiccion")) {


            if (++signal >= 2) return true;
        }


        if (contains_norm(n, "azcapotzalco") || contains_norm(n, "coyoacan") || contains_norm(n, "iztapalapa") ||


            contains_norm(n, "tlalpan") || contains_norm(n, "xochimilco") || contains_norm(n, "cuauhtemoc")) {


            if (++signal >= 2) return true;
        }
    }


    return false;
}




std::set<std::string> load_completed_page_ids(const AppOptions& options, const std::string& pdf_id) {


    std::set<std::string> ids;


    const fs::path p = processed_pages_manifest(options);


    if (!fs::exists(p)) return ids;


    ixiptlah_read_records(p, [&](IxiptlahRecordType type, std::uint32_t schema, std::istream& in) {


        if (type != IxiptlahRecordType::ProcessedPage || schema != 2) return true;


        std::string id;
        int page = 0;
        std::string status;

        int64_t accepted = 0;
        int64_t quarantine = 0;
        std::string processed_utc;


        std::string pdf_path;


        if (!ixiptlah_read_string(in, id) ||


            !ixiptlah_read_value(in, page) ||


            !ixiptlah_read_string(in, status) ||


            !ixiptlah_read_value(in, accepted) ||


            !ixiptlah_read_value(in, quarantine) ||


            !ixiptlah_read_string(in, processed_utc) ||


            !ixiptlah_read_string(in, pdf_path)) {



            return true;
        }


        if (id != pdf_id) return true;
        const bool skip_empty = env_flag_enabled_pipeline("TLALPOWA_RESUME_SKIP_EMPTY_PAGES");


        if ((skip_empty && (status == "empty" || status == "empty_no_epi_signal")) ||


            (status == "ok" && accepted > 0)) {


            ids.insert(page_stable_id(pdf_id, page));
        }


        return true;
    });


    return ids;


    return ids;
}




void append_processed_page_id(const AppOptions& options, const PdfDocument& doc, const PageText& page, const std::vector<TableCandidate>& tables) {


    const fs::path p = processed_pages_manifest(options);
    size_t accepted = 0;
    size_t quarantine = 0;


    for (const auto& t : tables) {


        accepted += t.accepted.size();
        quarantine += t.quarantine.size();
    }

    std::string status;


    if (quarantine > 0) status = "partial_quarantine";


    else if (accepted > 0) status = "ok";
    else status = page_has_epidemiological_table_signal(page) ? "empty_revisable" : "empty_no_epi_signal";


    (void)ixiptlah_append_record(p, IxiptlahRecordType::ProcessedPage, 2, [&](std::ostream& out) {
        const int64_t accepted_i = static_cast<int64_t>(accepted);
        const int64_t quarantine_i = static_cast<int64_t>(quarantine);


        return ixiptlah_write_string(out, doc.stable_id) &&


               ixiptlah_write_value(out, page.page) &&


               ixiptlah_write_string(out, status) &&


               ixiptlah_write_value(out, accepted_i) &&


               ixiptlah_write_value(out, quarantine_i) &&


               ixiptlah_write_string(out, now_utc_iso()) &&


               ixiptlah_write_string(out, path_utf8(doc.pdf_path));
    });


    ixiptlah_flush_all();


    return;
}




struct PdfResumeEvidence {
    int pages_seen = 0;
    int ok_pages = 0;

    int empty_pages = 0;
    int quarantine_pages = 0;
    int64_t accepted_total = 0;
};




std::map<std::string, PdfResumeEvidence> load_pdf_resume_evidence(const AppOptions& options) {


    std::map<std::string, PdfResumeEvidence> evidence;


    const fs::path p = processed_pages_manifest(options);


    if (!fs::exists(p)) return evidence;


    ixiptlah_read_records(p, [&](IxiptlahRecordType type, std::uint32_t schema, std::istream& in) {


        if (type != IxiptlahRecordType::ProcessedPage || schema != 2) return true;
        std::string id;
        int page = 0;

        std::string status;
        int64_t accepted = 0;
        int64_t quarantine = 0;


        std::string processed_utc;


        std::string pdf_path;


        if (!ixiptlah_read_string(in, id) ||


            !ixiptlah_read_value(in, page) ||


            !ixiptlah_read_string(in, status) ||


            !ixiptlah_read_value(in, accepted) ||


            !ixiptlah_read_value(in, quarantine) ||


            !ixiptlah_read_string(in, processed_utc) ||


            !ixiptlah_read_string(in, pdf_path)) return true;


        if (id.empty() || page <= 0) return true;
        auto& e = evidence[id];


        ++e.pages_seen;
        e.accepted_total += std::max<int64_t>(0, accepted);


        if (accepted > 0 && status == "ok") ++e.ok_pages;


        else if (accepted <= 0 && quarantine <= 0 && (status == "empty" || status == "empty_no_epi_signal")) ++e.empty_pages;


        if (quarantine > 0 || status == "partial_quarantine" || status == "empty_revisable") ++e.quarantine_pages;


        return true;
    });


    return evidence;
}




bool pdf_has_strong_resume_evidence(const PdfResumeEvidence& e) {




    if (e.ok_pages > 0 && e.accepted_total > 0) return true;


    if (env_flag_enabled_pipeline("TLALPOWA_ALLOW_EMPTY_PDF_COMPLETION")) {


        return e.pages_seen >= 3 && e.quarantine_pages == 0;
    }


    return false;
}




std::set<std::string> load_processed_ids(const AppOptions& options) {


    std::set<std::string> ids;


    const fs::path p = processed_manifest(options);


    if (!fs::exists(p)) return ids;
    const auto evidence = load_pdf_resume_evidence(options);


    ixiptlah_read_records(p, [&](IxiptlahRecordType type, std::uint32_t schema, std::istream& in) {


        if (type != IxiptlahRecordType::ProcessedPdf || schema != 1) return true;
        std::string id;
        std::string processed_utc;


        std::string pdf_path;


        if (!ixiptlah_read_string(in, id) ||


            !ixiptlah_read_string(in, processed_utc) ||


            !ixiptlah_read_string(in, pdf_path)) return true;


        if (id.empty()) return true;
        const auto it = evidence.find(id);


        if (it != evidence.end() && pdf_has_strong_resume_evidence(it->second)) ids.insert(id);


        return true;


    });


    return ids;
}




void append_processed_id(const AppOptions& options, const fs::path& pdf, const std::string& id) {


    const fs::path p = processed_manifest(options);


    (void)ixiptlah_append_record(p, IxiptlahRecordType::ProcessedPdf, 1, [&](std::ostream& out) {


        return ixiptlah_write_string(out, id) &&


               ixiptlah_write_string(out, now_utc_iso()) &&


               ixiptlah_write_string(out, path_utf8(pdf));
    });


    ixiptlah_flush_all();


    return;
}




std::string pdf_stable_id(const fs::path& pdf) {


    return simple_hash_hex(path_utf8(pdf) + "|" + std::to_string(file_size_or_zero(pdf)));
}




struct PipelineCancelled : std::runtime_error {


    PipelineCancelled() : std::runtime_error("cancelado por el usuario") {}
};

}


Pipeline::Pipeline(AppOptions options)
    : options_(std::move(options)),


      log_(pipeline_log_file(options_)),
      tools_(options_),
      extractor_(tools_, log_),

      table_engine_(config_),
      output_(options_.output_dir),



      dashboard_(pipeline_runtime_dir(options_)) {}





int Pipeline::run() {
    try {


        ensure_dir(options_.output_dir);


        prune_raster_preview_files_if_vector_mode(options_);


        ensure_dir(pipeline_runtime_dir(options_));
        stats_.status = "iniciando";


        write_run_state(options_, "running", stats_, "arranque del importador");


        if (options_.progress_callback) options_.progress_callback(stats_);
        log_.info("Iniciando Tlalpowa");


        log_.info("Entrada: " + path_utf8(options_.input_dir));


        log_.info("Salida: " + path_utf8(options_.output_dir));


        if (!env_flag_enabled_pipeline("TLALPOWA_WRITE_TOKEN_AUDIT")) {


            const int removed_token_audits = prune_token_audit_files(pipeline_runtime_dir(options_));


            if (removed_token_audits > 0) {


                log_.info("Volcados de coordenadas tokens_*.jsonl retirados: " + std::to_string(removed_token_audits));
            }
        }

        stats_.status = "enumerando_pdf";


        if (options_.progress_callback) options_.progress_callback(stats_);
        auto pdfs = resolve_effective_pdf_queue_for_pipeline(options_, log_);


        if (options_.limit_pdfs > 0 && static_cast<int>(pdfs.size()) > options_.limit_pdfs) pdfs.resize(static_cast<size_t>(options_.limit_pdfs));
        stats_.pdf_total = static_cast<int>(pdfs.size());


        update_detail_progress(stats_, 0, std::max(1, stats_.pdf_total * 100));
        stats_.status = "pdf_detectados";


        if (options_.progress_callback) options_.progress_callback(stats_);
        log_.info("PDF detectados: " + std::to_string(stats_.pdf_total));


        if (pdfs.empty()) {




            throw std::runtime_error("No se detectaron PDF importables en la entrada del pipeline: " + path_utf8(options_.input_dir));
        }


        config_.load(options_.config_dir);
        stats_.status = "validando_poppler";


        if (options_.progress_callback) options_.progress_callback(stats_);


        tools_.validate();


        stats_.status = "abriendo_salida_ixiptlah";


        if (options_.progress_callback) options_.progress_callback(stats_);


        output_.open(options_.resume);
        stats_.observations_accepted = static_cast<int64_t>(output_.observation_count());
        stats_.quarantine_items = static_cast<int64_t>(output_.quarantine_count());


        if (!options_.resume) {


            const fs::path p = processed_manifest(options_);


            const fs::path pages = processed_pages_manifest(options_);


            const fs::path live = live_preview_manifest(options_);


            ixiptlah_close_all();


            ensure_dir(p.parent_path());
            std::error_code ec;
            fs::remove(p, ec);


            ec.clear();
            fs::remove(pages, ec);


            ec.clear();
            fs::remove(live, ec);
        }


        if (options_.dashboard) dashboard_.ensure();


        std::set<std::string> processed = options_.resume ? load_processed_ids(options_) : std::set<std::string>{};


        if (options_.resume && processed.empty() && !pdfs.empty()) {
            log_.warn("Reanudacion solicitada, pero ningun PDF tiene evidencia fuerte de paginas aceptadas; se procesara la cola completa para no repetir el falso PDF 0/N.");
        }


        struct PdfQueueItem {


            fs::path pdf;
            int index = 0;
            std::string id;
        };


        std::vector<PdfQueueItem> pending;


        pending.reserve(pdfs.size());
        int i = 0;


        for (const auto& pdf : pdfs) {
            honor_controls();
            ++i;

            const std::string id = pdf_stable_id(pdf);


            if (processed.count(id) > 0) {
                ++stats_.pdf_done;


                stats_.current_pdf = path_utf8(pdf.filename());
                stats_.current_page = 0;
                stats_.status = "omitido_reanudacion";


                update_detail_progress(stats_, i * 100, std::max(1, stats_.pdf_total * 100));


                log_.info("[" + std::to_string(i) + "/" + std::to_string(stats_.pdf_total) + "] omitido por reanudacion: " + path_utf8(pdf.filename()));


                if (options_.progress_callback) options_.progress_callback(stats_);
                continue;
            }


            pending.push_back({pdf, i, id});
        }



        struct PrefetchedPdf {


            PdfQueueItem item;


            std::future<PdfDocument> future;
            bool active = false;
        };

        std::vector<PrefetchedPdf> prefetched;
        const int prefetch_budget = std::max(1, std::min(3, adaptive_compute_worker_budget()));
        const int pdf_prefetch_window = env_int_clamped_pipeline(
            "TLALPOWA_PDF_PREFETCH_WINDOW", std::min(2, prefetch_budget), 1, prefetch_budget);

        auto already_prefetched = [&](const std::string& id) {
            for (const auto& p : prefetched) {
                if (p.active && p.item.id == id) return true;
            }
            return false;
        };

        auto launch_prefetch = [&](const PdfQueueItem& item, bool foreground_hint) {
            if (already_prefetched(item.id)) return;
            PrefetchedPdf p;
            p.item = item;

            p.active = true;

            if (foreground_hint) {
                stats_.current_pdf = path_utf8(item.pdf.filename());
                stats_.current_page = 0;
                stats_.status = "extrayendo_texto_pdf";

                update_detail_progress(stats_, std::max(0, item.index - 1) * 100 + 4, std::max(1, stats_.pdf_total * 100));

                write_run_state(options_, "running", stats_, "abriendo PDF " + path_utf8(item.pdf.filename()) + "; prelectura de hasta " + std::to_string(pdf_prefetch_window) + " boletines");

                if (options_.progress_callback) options_.progress_callback(stats_);
            } else {
                log_.info("Prefetch PDF +" + std::to_string(static_cast<int>(prefetched.size()) + 1) + ": " + path_utf8(item.pdf.filename()));
            }

            p.future = std::async(std::launch::async, [this, pdf = item.pdf]() {


                return extract_pdf_document(pdf);
            });

            prefetched.push_back(std::move(p));
        };

        auto fill_pdf_prefetch_window = [&](size_t current_pos) {
            if (pending.empty()) return;
            for (size_t j = current_pos; j < pending.size() && static_cast<int>(prefetched.size()) < pdf_prefetch_window; ++j) {
                launch_prefetch(pending[j], j == current_pos);
            }
        };

        auto take_prefetched = [&](const PdfQueueItem& item) -> std::unique_ptr<PdfDocument> {
            for (auto it = prefetched.begin(); it != prefetched.end(); ++it) {
                if (!it->active || it->item.id != item.id) continue;
                auto fut = std::move(it->future);
                prefetched.erase(it);
                return std::make_unique<PdfDocument>(fut.get());
            }
            return nullptr;
        };

        // Ventana de PDF: el documento actual y dos siguientes quedan abriéndose
        // en segundo plano. Esto conserva una sola pasada analítica; sólo adelanta
        // I/O y Poppler para que al cerrar una sopa de letras no haya pausa antes
        // del siguiente boletín.
        fill_pdf_prefetch_window(0);

        for (size_t k = 0; k < pending.size(); ++k) {
            honor_controls();
            const PdfQueueItem item = pending[k];

            bool completed_without_retryable_quarantine = false;
            try {
                fill_pdf_prefetch_window(k);

                PdfDocument doc;


                if (auto pref = take_prefetched(item)) {
                    doc = std::move(*pref);

                } else {
                    stats_.current_pdf = path_utf8(item.pdf.filename());
                    stats_.current_page = 0;
                    stats_.status = "extrayendo_texto_pdf_sin_prefetch";
                    update_detail_progress(stats_, std::max(0, item.index - 1) * 100 + 4, std::max(1, stats_.pdf_total * 100));
                    if (options_.progress_callback) options_.progress_callback(stats_);
                    doc = extract_pdf_document(item.pdf);
                }




                fill_pdf_prefetch_window(k + 1);
                completed_without_retryable_quarantine = process_pdf_document(item.pdf, item.index, std::move(doc));


            } catch (const PipelineCancelled&) {


                throw;


            } catch (const std::exception& e) {


                log_.error("Fallo procesando PDF " + path_utf8(item.pdf.filename()) + ": " + e.what());


                if (options_.stop_on_error) throw;


                fill_pdf_prefetch_window(k + 1);
                continue;
            }


            if (completed_without_retryable_quarantine) {
                append_processed_id(options_, item.pdf, item.id);
            } else {


                log_.warn("PDF con paginas en cuarentena reintentable; no se marca como completamente cerrado: " + path_utf8(item.pdf.filename()));
            }
        }

        // Cierre estrictamente terminal: la lectura pesada termina en el bucle de boletines.
        // No se reconstruyen índices ni se dispara una recarga visual aquí; hacerlo después
        // de rozar 100 % duplicaba el recorrido perceptible y reiniciaba la barra de progreso.
        output_.finalize();
        write_master_summaries();


        update_detail_progress(stats_, stats_.detail_total, stats_.detail_total);
        stats_.status = "terminado";


        write_run_state(options_, "complete", stats_, "corrida cerrada en una sola pasada; buffers IXIPTLAH flusheados");


        if (options_.progress_callback) options_.progress_callback(stats_);
        log_.info("Terminado. Observaciones aceptadas=" + std::to_string(stats_.observations_accepted) + ", cuarentena=" + std::to_string(stats_.quarantine_items));


        return 0;


    } catch (const PipelineCancelled&) {
        stats_.status = "cancelado";


        write_run_state(options_, "canceled", stats_, "cancelacion solicitada; los datos parciales quedan marcados como no autoritativos");


        if (options_.progress_callback) options_.progress_callback(stats_);
        log_.warn("Ejecucion cancelada por el usuario. Las salidas parciales se conservan para auditoria, pero la UI las marcara como incompletas al reiniciar.");


        try {
            output_.finalize();
            output_.flush_streams();


            write_master_summaries();


            write_run_state(options_, "canceled", stats_, "salidas parciales flusheadas; requiere reanudacion para regenerar derivados completos");


        } catch (const std::exception& e) {
            log_.warn(std::string("No se pudieron cerrar salidas parciales tras cancelar: ") + e.what());
        }


        return 130;


    } catch (const std::exception& e) {


        write_run_state(options_, "error", stats_, e.what());


        log_.error(std::string("Ejecución abortada: ") + e.what());


        return 2;
    }
}





void Pipeline::honor_controls() {


    if (options_.cancel_requested && options_.cancel_requested()) throw PipelineCancelled();
    bool logged_pause = false;


    while (options_.pause_requested && options_.pause_requested()) {


        if (options_.cancel_requested && options_.cancel_requested()) throw PipelineCancelled();
        stats_.status = "pausado";


        if (options_.progress_callback) options_.progress_callback(stats_);


        if (!logged_pause) {
            log_.info("Procesamiento pausado por el usuario.");
            logged_pause = true;
        }


        std::this_thread::sleep_for(std::chrono::milliseconds(180));
    }


    if (logged_pause) log_.info("Procesamiento reanudado por el usuario.");
}




fs::path Pipeline::preview_prefix(const PdfDocument& doc) const {




    return preview_prefix_from_options(options_, doc.stable_id);
}




fs::path Pipeline::preview_image_path(const PdfDocument& doc, int page) const {


    return preview_image_path_from_options(options_, doc.stable_id, page);
}




void Pipeline::render_preview_batch(const fs::path& pdf, const PdfDocument& doc, int first_page, int last_page) const {
    schedule_detached_preview_render(options_, pdf, doc.stable_id, first_page, last_page, false);
}




fs::path Pipeline::render_page(const fs::path& pdf, const PdfDocument& doc, const PageText& page) const {


    if (!options_.render_pages || !preview_raster_enabled_by_env()) return {};


    const fs::path expected = preview_image_path(doc, page.page);


    const fs::path legacy_png = legacy_preview_png_path_from_options(options_, doc.stable_id, page.page);


    if (fs::exists(expected)) return expected;


    if (fs::exists(legacy_png)) return legacy_png;



    const int wait_ms = env_int_clamped_pipeline("TLALPOWA_RASTER_COMMIT_WAIT_MS", 4, 0, 90);


    const auto t0 = std::chrono::steady_clock::now();


    while (wait_ms > 0 && !fs::exists(expected) && !fs::exists(legacy_png)) {


        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() >= wait_ms) break;


        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }


    if (fs::exists(expected)) return expected;


    if (fs::exists(legacy_png)) return legacy_png;




    if (!preview_bulk_prefetch_enabled_by_env()) {
        schedule_detached_preview_render(options_, pdf, doc.stable_id, page.page, page.page, true);
    }


    return expected;
}





PdfDocument Pipeline::extract_pdf_document(const fs::path& pdf) {
    const std::string id = pdf_stable_id(pdf);




    fs::path base_for_tmp = pipeline_runtime_dir(options_);


    const fs::path work = base_for_tmp / "importador_tmp" / safe_filename(id);


    return extractor_.extract(pdf, work);
}




bool Pipeline::process_pdf(const fs::path& pdf, int index) {
    auto doc = extract_pdf_document(pdf);


    return process_pdf_document(pdf, index, std::move(doc));
}





bool Pipeline::process_pdf_document(const fs::path& pdf, int index, PdfDocument doc) {


    log_.info("[" + std::to_string(index) + "/" + std::to_string(stats_.pdf_total) + "] " + path_utf8(pdf.filename()));


    stats_.current_pdf = path_utf8(pdf.filename());
    stats_.current_page = 0;
    stats_.status = "procesando_pdf";

    write_run_state(options_, "running", stats_, "procesando sopa de letras " + stats_.current_pdf);

    const int detail_base = std::max(0, index - 1) * 100;


    update_detail_progress(stats_, detail_base + 20, std::max(1, stats_.pdf_total * 100));


    if (options_.progress_callback) options_.progress_callback(stats_);
    const std::string id = pdf_stable_id(pdf);


    honor_controls();


    log_.info("Documento parseado: paginas=" + std::to_string(doc.pages.size()) + ", anio=" + std::to_string(doc.bulletin_year) + ", semana=" + std::to_string(doc.bulletin_week));


    if (env_flag_enabled_pipeline("TLALPOWA_WRITE_TOKEN_AUDIT")) {


        output_.write_tokens_jsonl(doc);
    }


    const auto completed_pages = options_.resume ? load_completed_page_ids(options_, id) : std::set<std::string>{};


    if (!completed_pages.empty()) {
        log_.info("Reanudacion granular: " + std::to_string(completed_pages.size()) + " paginas ya cerradas se omitiran; paginas con cuarentena quedan reintentables.");
    }

    bool pdf_has_retryable_quarantine = false;


    auto last_live_excel = std::chrono::steady_clock::now() - std::chrono::seconds(30);


    size_t last_live_excel_observations = output_.observation_count();
    const int64_t pdf_observations_start = stats_.observations_accepted;
    const int skip_front_pages = std::max(0, options_.skip_front_pages);

    const int skip_back_pages = std::max(0, options_.skip_back_pages);


    int max_parsed_page = 0;


    for (const auto& page : doc.pages) max_parsed_page = std::max(max_parsed_page, page.page);


    const int fallback_last_allowed_page = (doc.source_page_count > 0 || skip_back_pages == 0) ? std::numeric_limits<int>::max() : std::max(0, max_parsed_page - skip_back_pages);



    auto page_is_usable = [&](const PageText& page) {



        if (page.page <= skip_front_pages) return false;



        if (page.page > fallback_last_allowed_page) return false;


        return true;
    };


    int processable_pages = 0;


    for (const auto& page : doc.pages) {


        if (!page_is_usable(page)) continue;


        if (options_.max_pages_per_pdf > 0 && processable_pages >= options_.max_pages_per_pdf) break;
        ++processable_pages;
    }

    log_.info("Saltando portadas/indices: primeras " + std::to_string(skip_front_pages) +
              " paginas fuera de Poppler; ultima(s) " + std::to_string(skip_back_pages) +
              (doc.source_page_count > 0 ? " fuera de Poppler." : " fuera del motor tabular por falta de pdfinfo."));



    std::vector<int> preview_pages;


    preview_pages.reserve(static_cast<size_t>(std::max(0, processable_pages)));
    int preview_seen = 0;


    for (const auto& ptxt : doc.pages) {


        if (!page_is_usable(ptxt)) continue;


        if (options_.max_pages_per_pdf > 0 && preview_seen >= options_.max_pages_per_pdf) break;
        ++preview_seen;


        preview_pages.push_back(ptxt.page);
    }




    if (options_.render_pages && preview_raster_enabled_by_env() && !preview_pages.empty()) {
        const int batch_pages = env_int_clamped_pipeline("TLALPOWA_PREVIEW_BATCH_PAGES", 20, 1, 32);
        const int max_prefetch_batches = env_int_clamped_pipeline("TLALPOWA_PREVIEW_PREFETCH_BATCHES", 1, 1, 64);


        int launched_batches = 0;


        for (size_t i = 0; i < preview_pages.size() && launched_batches < max_prefetch_batches; ) {
            const int first = preview_pages[i];
            int last = first;

            size_t j = i + 1;


            while (j < preview_pages.size() && j - i < static_cast<size_t>(batch_pages) && preview_pages[j] == last + 1) {
                last = preview_pages[j];
                ++j;
            }

            render_preview_batch(pdf, doc, first, last);
            ++launched_batches;


            i = j;
        }
    }



    std::vector<const PageText*> ordered_pages;


    ordered_pages.reserve(static_cast<size_t>(std::max(0, processable_pages)));
    int ordered_seen = 0;


    for (const auto& page : doc.pages) {


        if (!page_is_usable(page)) continue;


        if (options_.max_pages_per_pdf > 0 && ordered_seen >= options_.max_pages_per_pdf) break;
        ++ordered_seen;


        ordered_pages.push_back(&page);
    }


    stats_.pages_total += static_cast<int>(ordered_pages.size());



    if (ordered_pages.empty()) {



        PageText synthetic;
        synthetic.page = std::max(1, doc.first_extracted_page);
        synthetic.width = 612.0;

        synthetic.height = 792.0;

        QuarantineItem q;


        q.pdf_file = doc.file_name;
        q.pdf_id = doc.stable_id;
        q.page = synthetic.page;


        q.table_id = "pdf_extract";


        q.column_key = "document";


        q.reason = "pdf_without_parseable_pages";


        q.detail = "pdftotext no produjo paginas bbox parseables; el PDF se conserva reintentable y no se marca como completamente procesado";



        TableCandidate tq;
        tq.table_id = "pdf_extract";


        tq.table_title = "PDF sin paginas bbox parseables";
        tq.page = synthetic.page;
        tq.page_box = {0.0, 0.0, synthetic.width, synthetic.height};


        tq.quarantine.push_back(q);
        output_.append_table(tq);


        ++stats_.pages_done;
        ++stats_.quarantine_items;
        stats_.current_page = synthetic.page;


        stats_.status = "pdf_sin_paginas_bbox_reintentable";


        append_processed_page_id(options_, doc, synthetic, std::vector<TableCandidate>{tq});
        ExtractionPreview preview;


        preview.pdf_file = doc.file_name;
        preview.pdf_index = index;
        preview.pdf_total = stats_.pdf_total;

        preview.page = synthetic.page;
        preview.page_width = synthetic.width;


        preview.page_height = synthetic.height;
        preview.quarantine = tq.quarantine;


        preview.status = "PDF validado pero sin páginas bbox; queda reintentable, no cerrado como exitoso";


        write_live_preview(options_, preview);


        if (options_.preview_callback) options_.preview_callback(preview);


        write_run_state(options_, "running", stats_, "pdf sin paginas bbox parseables; requiere reintento");


        if (options_.progress_callback) options_.progress_callback(stats_);
        ++stats_.pdf_done;


        update_detail_progress(stats_, index * 100, std::max(1, stats_.pdf_total * 100));


        if (options_.progress_callback) options_.progress_callback(stats_);


        return false;
    }




    struct PageTableResult {
        const PageText* page = nullptr;


        std::vector<TableCandidate> tables;
        std::string error;
    };



    struct PageTableJob {
        const PageText* page = nullptr;


        std::future<PageTableResult> future;
        bool launched = false;
    };



    const int auto_table_workers = adaptive_compute_worker_budget();
    const int table_workers = env_int_clamped_pipeline(
        "TLALPOWA_TABLE_WORKERS", auto_table_workers, 1, auto_table_workers);


    std::vector<PageTableJob> page_jobs(ordered_pages.size());


    for (size_t pos = 0; pos < ordered_pages.size(); ++pos) page_jobs[pos].page = ordered_pages[pos];



    auto needs_processing = [&](size_t pos) -> bool {


        if (pos >= ordered_pages.size()) return false;
        const PageText& ptxt = *ordered_pages[pos];


        return completed_pages.count(page_stable_id(id, ptxt.page)) == 0;
    };



    auto launch_page_job = [&](size_t pos) {


        if (pos >= page_jobs.size() || page_jobs[pos].launched || !needs_processing(pos)) return;
        const PageText* page_ptr = ordered_pages[pos];
        page_jobs[pos].launched = true;


        page_jobs[pos].future = std::async(std::launch::async, [this, &doc, page_ptr]() -> PageTableResult {
            PageTableResult r;
            r.page = page_ptr;

            try {
                r.tables = table_engine_.reconstruct_page(doc, *page_ptr);


            } catch (const std::exception& e) {
                r.error = e.what();


            } catch (...) {


                r.error = "excepcion desconocida en reconstruccion tabular";
            }


            return r;
        });
    };


    size_t next_launch = 0;


    auto fill_table_window = [&](size_t commit_pos) {
        const size_t limit = std::min(ordered_pages.size(), commit_pos + static_cast<size_t>(table_workers) + 1);


        while (next_launch < limit) {
            launch_page_job(next_launch);
            ++next_launch;
        }




        while (next_launch < ordered_pages.size()) {
            size_t active_or_pending = 0;


            for (size_t k = commit_pos; k < next_launch; ++k) {


                if (needs_processing(k)) ++active_or_pending;
            }


            if (active_or_pending >= static_cast<size_t>(table_workers)) break;
            launch_page_job(next_launch);
            ++next_launch;
        }
    };



    int processed_pages = 0;


    auto last_live_preview_emit = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    int emitted_live_previews = 0;
    const int live_preview_min_ms = env_int_clamped_pipeline("TLALPOWA_LIVE_PREVIEW_MIN_MS", 0, 0, 2000);

    const int live_preview_force_every_pages = env_int_clamped_pipeline("TLALPOWA_LIVE_PREVIEW_FORCE_EVERY_PAGES", 1, 1, 500);
    const bool live_preview_continuous_pages = !env_flag_enabled_pipeline("TLALPOWA_LIVE_PREVIEW_THROTTLED");


    for (size_t pos = 0; pos < ordered_pages.size(); ++pos) {
        honor_controls();
        fill_table_window(pos);

        const PageText& page = *ordered_pages[pos];
        const std::string page_id = page_stable_id(id, page.page);


        if (completed_pages.count(page_id) > 0) {


            ++processed_pages;
            stats_.current_page = page.page;
            stats_.status = "pagina_omitida_reanudacion";


            update_detail_progress(stats_, detail_base + 20 + static_cast<int>((75.0 * processed_pages) / std::max(1, processable_pages)), std::max(1, stats_.pdf_total * 100));
            log_.info("Pagina " + std::to_string(page.page) + " omitida por checkpoint granular seguro.");


            if (options_.progress_callback) options_.progress_callback(stats_);
            continue;
        }



        if (!page_jobs[pos].launched) launch_page_job(pos);


        while (page_jobs[pos].future.wait_for(std::chrono::milliseconds(35)) != std::future_status::ready) {
            honor_controls();


            stats_.current_page = page.page;
            stats_.status = "reconstruyendo_tabla_ordenada";


            if (options_.progress_callback) options_.progress_callback(stats_);
        }
        PageTableResult result = page_jobs[pos].future.get();

        fill_table_window(pos + 1);

        ++processed_pages;

        ++stats_.pages_done;
        stats_.current_page = page.page;
        stats_.status = "cerrando_pagina_validada";


        if (options_.render_pages && preview_raster_enabled_by_env()) {




            int last = page.page;
            const int look = env_int_clamped_pipeline("TLALPOWA_PREVIEW_VISIBLE_LOOKAHEAD", 20, 0, 20);
            const bool refresh_preview_window = look > 0 && (processed_pages == 1 || processed_pages % std::max(1, look) == 0);


            if (refresh_preview_window) {


                for (int k = 1; k <= look && pos + static_cast<size_t>(k) < ordered_pages.size(); ++k) {


                    const int candidate = ordered_pages[pos + static_cast<size_t>(k)]->page;


                    if (candidate == last + 1) last = candidate;
                    else break;
                }

                render_preview_batch(pdf, doc, page.page, last);
            }
        }

        auto tables = std::move(result.tables);


        if (!result.error.empty()) {
            pdf_has_retryable_quarantine = true;
            log_.warn("Pagina " + std::to_string(page.page) + " con reconstruccion tabular fallida y reintentable: " + result.error);
        }


        if (!tables.empty()) ++stats_.pages_with_tables;


        for (const auto& t : tables) {
            ++stats_.tables_detected;
            stats_.observations_accepted += static_cast<int64_t>(t.accepted.size());

            stats_.quarantine_items += static_cast<int64_t>(t.quarantine.size());


            if (!t.quarantine.empty()) pdf_has_retryable_quarantine = true;
            output_.append_table(t);
        }

        const int page_total = std::max(1, processable_pages);


        update_detail_progress(stats_, detail_base + 20 + static_cast<int>((75.0 * processed_pages) / page_total), std::max(1, stats_.pdf_total * 100));
        log_.info("Pagina " + std::to_string(page.page) + ": tokens=" + std::to_string(page.tokens.size()) +
                  ", tablas=" + std::to_string(tables.size()) +

                  ", aceptadas_total=" + std::to_string(stats_.observations_accepted) +
                  ", cuarentena_total=" + std::to_string(stats_.quarantine_items));


        fs::path image;
        const bool should_render_preview = options_.render_pages && preview_raster_enabled_by_env();


        const auto live_preview_now = std::chrono::steady_clock::now();
        bool emit_live_preview = options_.preview_callback || options_.render_pages || !page.tokens.empty();


        if (emit_live_preview) {
            const bool first_pages = emitted_live_previews < 2;
            const bool last_page = processed_pages >= processable_pages;


            const bool timed = std::chrono::duration_cast<std::chrono::milliseconds>(live_preview_now - last_live_preview_emit).count() >= live_preview_min_ms;
            const bool counted = (processed_pages % live_preview_force_every_pages) == 0;


            emit_live_preview = live_preview_continuous_pages || first_pages || last_page || timed || counted || !result.error.empty();
        }


        if (emit_live_preview) {


            if (should_render_preview) image = render_page(pdf, doc, page);


            last_live_preview_emit = live_preview_now;
            ++emitted_live_previews;
            ExtractionPreview preview;


            preview.pdf_file = doc.file_name;
            preview.page_image = image;
            preview.pdf_index = index;

            preview.pdf_total = stats_.pdf_total;
            preview.page = page.page;
            preview.page_width = page.width;

            preview.page_height = page.height;



            const size_t kMaxPreviewTokens = static_cast<size_t>(env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_TOKENS", 900, 120, 5000));


            if (page.tokens.size() <= kMaxPreviewTokens) {
                preview.tokens = page.tokens;
            } else {


                preview.tokens.reserve(kMaxPreviewTokens);
                const double step = static_cast<double>(page.tokens.size()) / static_cast<double>(kMaxPreviewTokens);


                for (size_t n = 0; n < kMaxPreviewTokens; ++n) {


                    preview.tokens.push_back(page.tokens[std::min(page.tokens.size() - 1, static_cast<size_t>(n * step))]);
                }
            }


            if (!result.error.empty()) preview.status = "Reconstruccion reintentable: " + result.error;
            else preview.status = tables.empty() ? "Sin tabla reconstruible en esta pagina" : "Tabla reconstruida y validada";




            // Captura viva hiperrobusta por ronda: una página de boletín puede traer
            // varias enfermedades en paralelo. No basta dividir cupos por tabla y
            // recorrer en serie, porque muchas tablas pequeñas o una tabla inicial
            // masiva podrían agotar el cupo global antes de que la UI vea las demás.
            // Se toman filas/columnas/observaciones/cuarentena por rondas, con cota
            // local y cota global; así la previsualización conserva evidencia de
            // todas las tablas sin tocar los IXIPTLAH persistentes epidemiológicos
            // ni atmosféricos, que siguen escribiéndose por su propio contrato.
            const size_t kMaxPreviewRows = static_cast<size_t>(env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_ROWS", 144, 16, 768));


            const size_t kMaxPreviewColumns = static_cast<size_t>(env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_COLUMNS", 240, 8, 1024));
            const size_t kMaxPreviewAccepted = static_cast<size_t>(env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_ACCEPTED", 2304, 64, 16384));
            const size_t kMaxPreviewQuarantine = static_cast<size_t>(env_int_clamped_pipeline("TLALPOWA_PREVIEW_MAX_QUARANTINE", 224, 8, 1024));

            const size_t table_divisor = std::max<size_t>(1u, tables.size());
            const size_t per_table_rows = std::max<size_t>(18u, kMaxPreviewRows / table_divisor + 1u);
            const size_t per_table_columns = std::max<size_t>(12u, kMaxPreviewColumns / table_divisor + 1u);
            const size_t per_table_accepted = std::max<size_t>(112u, kMaxPreviewAccepted / table_divisor + 1u);
            const size_t per_table_quarantine = std::max<size_t>(12u, kMaxPreviewQuarantine / table_divisor + 1u);

            auto round_robin_copy = [&](auto member, auto& dst, size_t global_cap, size_t local_cap) {
                if (tables.empty() || global_cap == 0 || local_cap == 0) return;
                std::vector<size_t> cursor(tables.size(), 0u);
                std::vector<size_t> copied(tables.size(), 0u);
                bool progressed = true;
                while (dst.size() < global_cap && progressed) {
                    progressed = false;
                    for (size_t ti = 0; ti < tables.size() && dst.size() < global_cap; ++ti) {
                        const auto& src = tables[ti].*member;
                        if (cursor[ti] >= src.size() || copied[ti] >= local_cap) continue;
                        dst.push_back(src[cursor[ti]++]);
                        ++copied[ti];
                        progressed = true;
                    }
                }
            };

            round_robin_copy(&TableCandidate::rows, preview.rows, kMaxPreviewRows, per_table_rows);
            round_robin_copy(&TableCandidate::columns, preview.columns, kMaxPreviewColumns, per_table_columns);
            round_robin_copy(&TableCandidate::accepted, preview.accepted, kMaxPreviewAccepted, per_table_accepted);
            round_robin_copy(&TableCandidate::quarantine, preview.quarantine, kMaxPreviewQuarantine, per_table_quarantine);

            if (result.error.empty() && !tables.empty()) {
                preview.status = "Tablas=" + std::to_string(tables.size()) +
                                 " | vista=" + std::to_string(preview.accepted.size()) +
                                 " valores | cuarentena=" + std::to_string(preview.quarantine.size());
            }


            write_live_preview(options_, preview);


            if (options_.preview_callback) options_.preview_callback(preview);
        }



        append_processed_page_id(options_, doc, page, tables);


        const auto live_now = std::chrono::steady_clock::now();
        const bool new_observations_for_excel = output_.observation_count() != last_live_excel_observations;
        const int live_every_pages = env_int_clamped_pipeline("TLALPOWA_LIVE_EXPORT_EVERY_N_PAGES", 75, 5, 1000);
        const int live_every_seconds = env_int_clamped_pipeline("TLALPOWA_LIVE_EXPORT_EVERY_SECONDS", 75, 10, 3600);
        const bool build_live_excel = new_observations_for_excel &&
            (stats_.pages_done <= 2 || stats_.pages_done % live_every_pages == 0 ||

             std::chrono::duration_cast<std::chrono::seconds>(live_now - last_live_excel).count() >= live_every_seconds);

        if (build_live_excel) {
            output_.flush_live_outputs(options_.config_dir, true);
            last_live_excel = live_now;
            last_live_excel_observations = output_.observation_count();
        } else {
            output_.flush_streams();
        }

        if (stats_.pages_done <= 3 || stats_.pages_done % 8 == 0) {

            write_run_state(options_, "running", stats_, "procesando documento actual");
        }

        if (options_.progress_callback) options_.progress_callback(stats_);

        if (options_.dashboard) {
            /* Dashboard secuencial: cada página útil cerrada entra a la ráfaga.
               Los skips de portada/última ya fueron resueltos por page_is_usable;
               aquí no se vuelve a diezmar la evidencia visual. */
            if (image.empty() && should_render_preview) image = render_page(pdf, doc, page);
            if (!image.empty()) dashboard_.push_page({doc.file_name, page.page, page.width, page.height, image, tables}, stats_);
        }
        std::cout << "\rPDF " << stats_.pdf_done + 1 << "/" << stats_.pdf_total
                  << " | pagina util " << processed_pages << "/" << processable_pages
                  << " | aceptadas " << stats_.observations_accepted

                  << " | cuarentena " << stats_.quarantine_items << "        " << std::flush;
    }
    std::cout << std::endl;

    ++stats_.pdf_done;

    update_detail_progress(stats_, index * 100, std::max(1, stats_.pdf_total * 100));

    if (options_.progress_callback) options_.progress_callback(stats_);
    const int64_t pdf_observations = stats_.observations_accepted - pdf_observations_start;
    (void)pdf_observations;



    return !pdf_has_retryable_quarantine;
}




void Pipeline::write_master_summaries() {

    ensure_dir(pipeline_runtime_dir(options_));
    std::ostringstream md;
    md << "# Resumen de corrida\n\n";

    md << "Generado UTC: " << now_utc_iso() << "\n\n";
    md << "- PDF procesados: " << stats_.pdf_done << "/" << stats_.pdf_total << "\n";
    md << "- Páginas procesadas: " << stats_.pages_done << "\n";
    md << "- Páginas con tablas: " << stats_.pages_with_tables << "\n";
    md << "- Tablas detectadas: " << stats_.tables_detected << "\n";
    md << "- Observaciones aceptadas: " << stats_.observations_accepted << "\n";

    md << "- Elementos en cuarentena: " << stats_.quarantine_items << "\n\n";

    md << "La base validada se concentra en contenedores mensuales nativos `.ixiptlah` directamente dentro de `Datos`, con registros epidemiologicos, atmosfericos, contaminantes y productos de grafica preparados para carga directa.\n";

    md << "Los manifiestos de progreso y previsualizacion viven como `.ixiptlah` en `Build/runtime`, fuera de Datos.\n";

    write_text_file(pipeline_runtime_dir(options_) / "RUN_SUMMARY.md", md.str());
}

}

// ===== Nucleos/TableEngine.impl =====
#line 1 "Nucleos/TableEngine.impl"






#include <cstring>
#include <initializer_list>



namespace epi {



namespace {



struct HeaderYear {

    int year = 0;
    double x = 0.0;
};




struct HeaderGroup {
    int first_col = 0;
    int last_col = 0;
    double x0 = 0.0;
    double x1 = 0.0;

    std::string label;
    std::string norm;
    std::string disease_id;
    std::string disease;
    std::string cie10;

    Rect label_box;
    Rect cie10_box;
    double confidence = 0.0;

    std::vector<HeaderYear> years;
};



struct HeaderSpan {

    std::string text;
    Rect box;
    bool found = false;
};



struct ColumnGroupRange {
    int first_col = 0;
    int last_col = 0;
};




double median_value(std::vector<double> values) {

    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());

    const size_t mid = values.size() / 2;

    if (values.size() % 2 == 1) return values[mid];

    return (values[mid - 1] + values[mid]) * 0.5;
}



bool has_norm_word(const std::string& norm, const char* word) {

    const size_t wl = std::strlen(word);

    if (wl == 0) return false;
    size_t p = 0;

    while ((p = norm.find(word, p)) != std::string::npos) {
        const bool left = (p == 0 || norm[p - 1] == ' ');
        const size_t e = p + wl;

        const bool right = (e >= norm.size() || norm[e] == ' ');

        if (left && right) return true;
        p = e;
    }

    return false;
}



bool has_any_norm_word(const std::string& norm, std::initializer_list<const char*> words) {

    for (const char* w : words) if (has_norm_word(norm, w)) return true;

    return false;
}



bool is_norm_sex_only(const std::string& norm) {

    return norm == "m" || norm == "f" || norm == "masc" || norm == "fem" ||
           norm == "masculino" || norm == "femenino";
}



bool is_norm_year_20xx_exact(const std::string& norm) {

    return norm.size() == 4 && norm[0] == '2' && norm[1] == '0' &&

           std::isdigit(static_cast<unsigned char>(norm[2])) &&
           std::isdigit(static_cast<unsigned char>(norm[3]));
}



int first_year_20xx(const std::string& text) {

    for (size_t i = 0; i + 3 < text.size(); ++i) {

        if (text[i] == '2' && text[i + 1] == '0' &&

            std::isdigit(static_cast<unsigned char>(text[i + 2])) &&

            std::isdigit(static_cast<unsigned char>(text[i + 3]))) {

            return 2000 + (text[i + 2] - '0') * 10 + (text[i + 3] - '0');
        }
    }

    return 0;
}




bool is_semantic_sem_header(const std::string& norm) {

    if (norm.empty()) return false;

    const bool has_sem = contains_norm(norm, "sem") || contains_norm(norm, "semana");
    const bool has_acum = contains_norm(norm, "acum") || contains_norm(norm, "acumulado");

    const bool sex_only = is_norm_sex_only(norm);

    return has_sem && !has_acum && !sex_only;
}




std::vector<ColumnGroupRange> fallback_column_groups(int n) {

    std::vector<ColumnGroupRange> groups;

    if (n <= 0) return groups;


    for (int first = 0; first < n; first += 4) {

        groups.push_back({first, std::min(n - 1, first + 3)});
    }

    return groups;
}

std::vector<ColumnGroupRange> fallback_column_groups_from_role_headers(int n, const std::vector<std::string>& local_norms) {
    std::vector<ColumnGroupRange> groups;
    if (n <= 0 || static_cast<int>(local_norms.size()) < n) return groups;

    int sem_headers = 0;
    int acum_headers = 0;
    int sex_headers = 0;
    for (int i = 0; i < n; ++i) {
        const std::string& norm = local_norms[static_cast<size_t>(i)];
        const bool has_sem = contains_norm(norm, "sem") || contains_norm(norm, "semana");
        const bool has_acum = contains_norm(norm, "acum") || contains_norm(norm, "acumulado");
        if (has_sem && !has_acum) ++sem_headers;
        if (has_acum) ++acum_headers;
        if (is_norm_sex_only(norm)) ++sex_headers;
    }

    const int minimum_repeated = std::max(2, n / 5);
    if (sex_headers <= std::max(1, n / 10) && sem_headers >= minimum_repeated && acum_headers >= minimum_repeated) {
        for (int first = 0; first < n; first += 2) groups.push_back({first, std::min(n - 1, first + 1)});
        if (groups.size() >= 2) return groups;
    }

    if (sex_headers == 0 && acum_headers == 0 && sem_headers >= std::max(3, n / 2)) {
        for (int first = 0; first < n; ++first) groups.push_back({first, first});
        if (groups.size() >= 3) return groups;
    }

    groups.clear();
    return groups;
}

std::vector<ColumnGroupRange> edomex_legacy_column_groups(int n, const std::string& header_norm) {
    std::vector<ColumnGroupRange> groups;
    if (n <= 0) return groups;
    const bool has_sem = contains_norm(header_norm, "sem") || contains_norm(header_norm, "semana");
    if (contains_norm(header_norm, "cie 10") || has_norm_word(header_norm, "cie") || has_norm_word(header_norm, "rev")) return groups;
    const bool has_sex = contains_norm(header_norm, "masculino") || contains_norm(header_norm, "femenino") ||
                         contains_norm(header_norm, "hombres") || contains_norm(header_norm, "mujeres");
    if (!has_sem || has_sex) return groups;
    for (int first = 0; first < n; first += 2) groups.push_back({first, std::min(n - 1, first + 1)});
    return groups;
}

std::vector<ColumnGroupRange> edomex_modern_column_groups(int n,
                                                          const std::string& header_norm,
                                                          const std::vector<std::string>& local_norms,
                                                          int doc_year) {
    std::vector<ColumnGroupRange> groups;
    if (n < 4 || doc_year < 2023) return groups;
    if (!contains_norm(header_norm, "cie") && !contains_norm(header_norm, "rev")) return groups;

    int sem_headers = 0;
    int acum_headers = 0;
    int sex_headers = 0;
    for (int i = 0; i < n && i < static_cast<int>(local_norms.size()); ++i) {
        const std::string& norm = local_norms[static_cast<size_t>(i)];
        const bool has_sem = contains_norm(norm, "sem") || contains_norm(norm, "semana");
        const bool has_acum = contains_norm(norm, "acum") || contains_norm(norm, "acumulado");
        if (has_sem) ++sem_headers;
        if (has_acum) ++acum_headers;
        if (is_norm_sex_only(norm) ||
            has_any_norm_word(norm, {"m", "f", "masc", "fem", "masculino", "femenino"})) ++sex_headers;
    }

    const int disease_groups = std::max(1, n / 4);
    const bool header_has_sex = has_any_norm_word(header_norm, {"m", "f", "masc", "fem", "masculino", "femenino"});
    const bool role_shape_matches =
        sem_headers >= std::max(1, disease_groups / 2) &&
        acum_headers >= std::max(2, disease_groups) &&
        (sex_headers >= std::max(1, disease_groups / 2) || header_has_sex);
    if (!role_shape_matches) return groups;

    for (int first = 0; first < n; first += 4) groups.push_back({first, std::min(n - 1, first + 3)});
    return groups;
}

bool is_non_epidemiological_column_group(const std::string& norm) {
    if (norm.empty()) return false;
    if (contains_norm(norm, "totales por jurisdiccion") || contains_norm(norm, "totales por")) return true;
    if (contains_norm(norm, "total edo") || contains_norm(norm, "total estado")) return true;
    if (contains_norm(norm, "suma por institucion") || contains_norm(norm, "por institucion")) return true;
    if (contains_norm(norm, "institucion") || contains_norm(norm, "instituciones")) return true;
    if (contains_norm(norm, "institu ciones") || contains_norm(norm, "otras institu")) return true;
    if (contains_norm(norm, "acum desde la sem") || contains_norm(norm, "acumulado desde la sem")) return true;
    if (contains_norm(norm, "acum desde") || contains_norm(norm, "acumulado desde")) return true;
    if (contains_norm(norm, "i s e m") || contains_norm(norm, "i s s s t e")) return true;
    if (contains_norm(norm, "i s s s") || contains_norm(norm, "i m s s")) return true;
    if (contains_norm(norm, "isem") || contains_norm(norm, "imss") || contains_norm(norm, "issste")) return true;
    if (contains_norm(norm, "isse") || contains_norm(norm, "my m")) return true;
    if (contains_norm(norm, "difem") || contains_norm(norm, "pemex") || contains_norm(norm, "sedena")) return true;
    if (contains_norm(norm, "oportunidades") || contains_norm(norm, "bienestar")) return true;
    if (contains_norm(norm, "otras instituciones") || contains_norm(norm, "otras institu")) return true;
    return false;
}

bool contains_any_norm(const std::string& norm, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (needle && contains_norm(norm, needle)) return true;
    }
    return false;
}

int norm_word_count(const std::string& norm) {
    int count = 0;
    bool in_word = false;
    for (unsigned char ch : norm) {
        const bool word = std::isalnum(ch) != 0;
        if (word && !in_word) ++count;
        in_word = word;
    }
    return count;
}

bool is_document_admin_noise_norm(const std::string& norm) {
    if (norm.empty()) return true;
    if (contains_any_norm(norm, {
        "boletin epidemiologico",
        "ciudad de mexico",
        "salud publica",
        "servicios de salud",
        "secretaria de salud",
        "gobierno de la ciudad",
        "direccion de epidemiologia",
        "subdireccion de analisis",
        "subdireccion de sistemas",
        "medicina preventiva",
        "medina preventiva",
        "epidemiologia y medina",
        "preventiva epidemiologica",
        "mexico preventiva",
        "analisis e informacion",
        "informacion epidemiologica",
        "vigilancia epidemiologica",
        "sistema unico",
        "semana epidemiologica",
        "casos por jurisdiccion",
        "fuente sinave",
        "jurisdiccion sanitaria"
    })) return true;
    return false;
}

bool has_epidemiology_label_signal_norm(const std::string& norm) {
    if (norm.empty()) return false;
    return contains_any_norm(norm, {
        "absceso", "accidente", "alacran", "alcohol", "alzheimer", "amebiasis",
        "anencefalia", "anorexia", "asma", "bocio", "brucelosis", "bulimia",
        "candidiasis", "chancro", "chikungunya", "cirrosis", "cisticercosis",
        "colera", "conjuntivitis", "covid", "dengue", "depresion", "desnutricion",
        "diabetes", "difteria", "displasia", "encefalocele", "enfermedad",
        "enteritis", "erisipela", "escabiosis", "faringitis", "fiebre",
        "gastritis", "giardiasis", "gonococica", "hepatitis", "hiperplasia",
        "hipertension", "infeccion", "infecciones", "influenza", "insuficiencia",
        "intoxicacion", "lepra", "leptospirosis",
        "linfogranuloma", "malaria", "meningitis", "mordedura", "neoplasia",
        "neumonia", "neumonias", "obesidad", "oncocercosis", "otitis",
        "paludismo", "papiloma", "paralisis", "parkinson", "parotiditis",
        "quemadura", "rabia", "rickettsiosis", "rotavirus", "rubeola",
        "salmonelosis", "sarampion", "shigelosis", "sida", "sifilis",
        "sindrome", "tetanos", "tifo", "tos ferina", "toxoplasmosis",
        "tricomoniasis", "tripanosomiasis", "tuberculosis", "tumor",
        "ulcera", "ulceras", "varicela", "vih", "violencia", "vulvovaginitis", "zika"
    });
}

bool looks_like_mixed_section_title_as_disease_norm(const std::string& norm) {
    if (norm.empty()) return true;
    if (contains_any_norm(norm, {
        "casos nuevos notificados",
        "nuevos notificados",
        "notificados de enfermedades",
        "notificados de otras enfermedades",
        "casos por jurisdiccion",
        "casos por de",
        "enfermedades bajo vigilancia",
        "enfermedades de transmision",
        "enfermedades transmisibles",
        "enfs trasmitidas",
        "enfermedades trasmitidas",
        "enfermedades no transmisibles",
        "enfer inf del ap",
        "enfermedades infecciosas",
        "parasitaria del aparato",
        "parasitarias del aparato",
        "aparato digestivo",
        "aparato respiratorio",
        "displasias y tumor",
        "displasias defectos",
        "tumor malig cuello",
        "prevenibles por virus",
        "por vacunacion a",
        "accidentes violencia",
        "otras enfermedades pinto",
        "totales por jurisdiccion",
        "acum desde la sem",
        "suma por institucion",
        "por institucion",
        "de otras enfermedades"
    })) return true;

    if (norm == "y infecciones" || norm == "neumonia y" ||
        norm == "nuevos notificados de" ||
        norm == "nuevos notificados de enfermedades" ||
        norm == "notificados de enfermedades" ||
        norm == "de otras enfermedades" ||
        norm == "otras enfermedades no" ||
        norm == "no defectos" ||
        norm == "z o o n o s i s" ||
        norm == "no defectos al" ||
        norm == "intox alimen amibiasis" ||
        norm == "displ cervic y severa y" ||
        norm == "maligno displasia cervic leve y") return true;

    const bool too_many_signals =
        (contains_norm(norm, "faringitis") && contains_norm(norm, "neumonia")) ||
        (contains_norm(norm, "cerebro") && contains_norm(norm, "gingivitis") && contains_norm(norm, "asma")) ||
        (contains_norm(norm, "diabetes") && contains_norm(norm, "enfermedad no")) ||
        (contains_norm(norm, "dengue") && contains_norm(norm, "paludismo") && contains_norm(norm, "transmitidas")) ||
        (contains_norm(norm, "peaton") && contains_norm(norm, "alzheimer") && contains_norm(norm, "accid")) ||
        (contains_norm(norm, "desnutri") && norm_word_count(norm) <= 4);
    if (too_many_signals) return true;

    if (norm_word_count(norm) >= 7 && contains_norm(norm, "enfermedad") &&
        (contains_norm(norm, "por") || contains_norm(norm, "de"))) return true;
    return false;
}

bool is_generic_or_unusable_disease_label_norm(const std::string& norm) {
    if (norm.empty()) return true;
    if (is_document_admin_noise_norm(norm)) return true;
    if (looks_like_mixed_section_title_as_disease_norm(norm)) return true;
    if (norm.size() <= 3) return true;
    if (norm == "mexico" || norm == "ciudad" || norm == "salud" || norm == "publica" ||
        norm == "analisis" || norm == "informacion" || norm == "epidemiologica" ||
        norm == "epidemiologico" || norm == "medicina" || norm == "preventiva" ||
        norm == "enfermedad" || norm == "enfermedades" ||
        norm == "de enfermedades" || norm == "por de enfermedades" ||
        norm == "por de otras enfermedades" || norm == "de otras enfermedades" ||
        norm == "epidemiologica del" || norm == "epidemiologico del" ||
        norm == "semana epidemiologica del" || norm == "de mexico" ||
        norm == "especiales de vigilancia" || norm == "transmisibles por vector" ||
        norm == "subdireccion" || norm == "direccion" || norm == "sistemas" ||
        norm == "sistema" || norm == "vigilancia" || norm == "fuente" ||
        norm == "total" || norm == "grupo" || norm == "tipo" || norm == "vector" ||
        norm == "sexual" || norm == "respiratorio" || norm == "digestivo" ||
        norm == "transmisibles" || norm == "transmision" || norm == "del" ||
        norm == "de" || norm == "la" || norm == "las" || norm == "los" ||
        norm == "el" || norm == "en" || norm == "por" || norm == "con" ||
        norm == "para" || norm == "sin" || norm == "y" || norm == "o" ||
        norm == "por no") return true;
    if (contains_any_norm(norm, {
        "prevenibles por vacunacion",
        "bajo vigilancia sindromatica",
        "salud mental",
        "neurologicas y de salud",
        "enfermedades neurologicas",
        "enfermedades infecciosas y",
        "de enfermedades infecciosas",
        "epidemiologica del",
        "epidemiologico del",
        "especiales de vigilancia",
        "enfermedades transmisibles por vector",
        "y neoplasias",
        "y defectos",
        "y accidentes",
        "interes local"
    })) return true;
    if (contains_norm(norm, "eventos supuestamente") && !contains_norm(norm, "asociados")) return true;
    if (norm_word_count(norm) <= 1 && !has_epidemiology_label_signal_norm(norm)) return true;
    return false;
}



std::vector<ColumnGroupRange> infer_column_groups_from_geometry_and_headers(const std::vector<ColumnBand>& cols,


                                                                             const std::vector<std::string>& local_norms) {

    std::vector<ColumnGroupRange> groups;
    const int n = static_cast<int>(cols.size());

    if (n <= 0) return groups;

    if (n <= 4) return {{0, n - 1}};

    const auto role_groups_preferred = fallback_column_groups_from_role_headers(n, local_norms);
    if (!role_groups_preferred.empty()) return role_groups_preferred;


    std::vector<double> gaps;

    gaps.reserve(static_cast<size_t>(n - 1));

    for (int i = 1; i < n; ++i) {
        const double gap = std::max(0.0, cols[i].x_mid - cols[i - 1].x_mid);

        if (gap > 0.01) gaps.push_back(gap);
    }
    const double med = median_value(gaps);
    const double threshold = std::max(med * 1.55, med + 13.0);


    std::set<int> starts;

    starts.insert(0);


    for (int i = 1; i < n; ++i) {
        const double gap = std::max(0.0, cols[i].x_mid - cols[i - 1].x_mid);

        if (med > 0.0 && gap >= threshold) starts.insert(i);

        if (i < static_cast<int>(local_norms.size()) && is_semantic_sem_header(local_norms[static_cast<size_t>(i)])) starts.insert(i);
    }


    std::vector<int> ordered(starts.begin(), starts.end());

    std::sort(ordered.begin(), ordered.end());

    for (size_t k = 0; k < ordered.size(); ++k) {
        const int first = ordered[k];
        const int last = (k + 1 < ordered.size()) ? ordered[k + 1] - 1 : n - 1;

        if (last >= first) groups.push_back({first, last});
    }

    bool plausible = !groups.empty();
    int covered = 0;

    for (const auto& g : groups) {
        covered += (g.last_col - g.first_col + 1);
        const int sz = g.last_col - g.first_col + 1;

        if (sz <= 0 || sz > 5) plausible = false;
    }

    if (!plausible || covered != n || (groups.size() == 1 && n > 4)) {

        const auto role_groups = fallback_column_groups_from_role_headers(n, local_norms);
        if (!role_groups.empty()) return role_groups;
        return fallback_column_groups(n);
    }


    return groups;
}



int group_index_for_column(const std::vector<ColumnGroupRange>& groups, int col_index) {

    for (int i = 0; i < static_cast<int>(groups.size()); ++i) {

        if (col_index >= groups[static_cast<size_t>(i)].first_col && col_index <= groups[static_cast<size_t>(i)].last_col) return i;
    }

    return groups.empty() ? -1 : 0;
}



std::string role_key_from_column(const ColumnBand& col) {

    if (col.period == "Sem") return "sem_total";

    if (col.period == "Acum" && col.sex == "M") return "acum_m";

    if (col.period == "Acum" && col.sex == "F") return "acum_f";

    if (col.period == "Acum" && col.sex == "total" && col.role == "previous_year_accumulated") return "acum_total_previous";

    if (col.period == "Acum" && col.sex == "total") return "acum_total";

    return "unknown";
}



std::string friendly_missing_role(const std::string& key) {

    if (key == "sem_total") return "Sem";

    if (key == "acum_m") return "Acum_M";

    if (key == "acum_f") return "Acum_F";

    if (key == "acum_total_previous") return "Acum_total_anio_previo";

    if (key == "acum_total") return "Acum_total";


    return key;
}




std::string join_missing_roles(const std::vector<std::string>& missing) {
    std::ostringstream os;

    for (size_t i = 0; i < missing.size(); ++i) {

        if (i) os << '|';

        os << friendly_missing_role(missing[i]);
    }

    return os.str();
}



bool rect_has_area(const Rect& r) {

    return r.width() > 0.01 && r.height() > 0.01;
}


int nearest_year(const std::vector<HeaderYear>& years, double x, int fallback);




std::string join_pipe(const std::vector<std::string>& values) {
    std::ostringstream os;
    bool first = true;

    for (const auto& v : values) {


        if (trim(v).empty()) continue;

        if (!first) os << '|';
        first = false;

        os << trim(v);
    }


    return os.str();
}




bool norm_looks_like_cie10_fragment(const std::string& norm) {
    if (norm.empty()) return false;
    std::string compact;
    compact.reserve(norm.size());
    for (unsigned char ch : norm) {
        if (std::isspace(ch)) continue;
        compact.push_back(static_cast<char>(std::toupper(ch)));
    }
    if (compact.size() < 3) return false;
    const auto digit_or_o = [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == 'O';
    };
    return std::isalpha(static_cast<unsigned char>(compact[0])) && digit_or_o(compact[1]) && digit_or_o(compact[2]);
}

bool looks_like_header_noise(const std::string& norm) {
    if (norm.empty()) return true;
    if (is_numeric_token(norm)) return true;
    if (is_norm_year_20xx_exact(norm)) return true;
    if (norm == "sem" || norm == "semana" || norm == "acum" || norm == "acumulado") return true;
    if (norm == "m" || norm == "f") return true;
    if (contains_norm(norm, "cie 10") || norm == "cie" || norm == "rev" || norm == "excepto") return true;
    if (norm_looks_like_cie10_fragment(norm)) return true;
    if (contains_norm(norm, "jurisdiccion") || contains_norm(norm, "sanitaria")) return true;
    if (norm == "total" || norm == "fuente") return true;
    return false;
}

bool looks_like_table_title_line(const std::string& norm) {
    if (norm.empty()) return true;
    if (is_document_admin_noise_norm(norm)) return true;
    if (is_generic_or_unusable_disease_label_norm(norm) && !has_epidemiology_label_signal_norm(norm)) return true;
    if (contains_norm(norm, "vigilancia epidemiologica")) return true;
    if (contains_norm(norm, "casos por jurisdiccion")) return true;
    if (contains_norm(norm, "hasta la") && contains_norm(norm, "epidemiologica")) return true;
    if (contains_norm(norm, "enfermedades transmitidas por") && contains_norm(norm, "epidemiologica")) return true;
    if (contains_norm(norm, "cuadro")) return true;
    if (contains_norm(norm, "jurisdiccion") || contains_norm(norm, "sanitaria")) return true;
    if (contains_norm(norm, "servicios de salud") || contains_norm(norm, "direccion de epidemiologia")) return true;
    if (contains_norm(norm, "subdireccion de sistemas")) return true;
    if (contains_norm(norm, "infecciosas") && contains_norm(norm, "parasitarias")) return true;
    if (contains_norm(norm, "aparato") && (contains_norm(norm, "digestivo") || contains_norm(norm, "respiratorio"))) return true;
    if (contains_norm(norm, "transmision sexual")) return true;
    if (contains_norm(norm, "transmisibles") && (contains_norm(norm, "no transmisibles") || norm == "transmisibles")) return true;
    if (norm == "desnutricion" || norm == "neoplasias" || norm == "displasias" || norm == "intoxicaciones") return true;
    if (contains_norm(norm, "defectos") && contains_norm(norm, "nacimiento")) return true;
    if (contains_norm(norm, "paras ap digestiv") || contains_norm(norm, "paras digestiva")) return true;
    if (contains_norm(norm, "enfermedades") &&
        (contains_norm(norm, "transmisibles") || contains_norm(norm, "prevenibles") ||
         contains_norm(norm, "digestivo") || contains_norm(norm, "respiratorio") ||
         contains_norm(norm, "vector") || contains_norm(norm, "zoonosis") ||
         contains_norm(norm, "exantematicas") || contains_norm(norm, "sindromatica") ||
         contains_norm(norm, "no transmisibles") || contains_norm(norm, "nutricion") ||
         contains_norm(norm, "neoplasias") || contains_norm(norm, "defectos") ||
         contains_norm(norm, "accidentes") || contains_norm(norm, "interes local"))) return true;
    return false;
}

std::string cleaned_disease_label_text(const std::string& raw) {
    std::istringstream is(raw);
    std::string tok;
    std::ostringstream os;
    bool first = true;
    while (is >> tok) {
        const std::string n = normalize_key(tok);
        if (n.empty()) continue;
        if (looks_like_header_noise(n)) continue;
        if (contains_norm(n, "cie 10") || n == "cie" || n == "rev" || n == "excepto") continue;
        if (!first) os << ' ';
        first = false;
        os << tok;
    }
    return trim(os.str());
}

std::string strip_parenthetical_disease_noise(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    int depth = 0;
    for (unsigned char ch : raw) {
        if (ch == '(' || ch == '[' || ch == '{') {
            ++depth;
            out.push_back(' ');
            continue;
        }
        if (depth > 0) {
            if (ch == ')' || ch == ']' || ch == '}') --depth;
            continue;
        }
        if (ch == ',' || ch == ';' || ch == ':') out.push_back(' ');
        else out.push_back(static_cast<char>(ch));
    }
    return trim(out);
}

bool is_leading_epidemiology_residue_word(const std::string& word) {
    return word == "del" || word == "de" || word == "la" || word == "las" ||
        word == "los" || word == "el" || word == "y" || word == "con" ||
        word == "por" || word == "para" || word == "en";
}

std::string disease_label_match_norm(const std::string& raw) {
    const std::string stripped = strip_parenthetical_disease_noise(raw);
    std::istringstream is(normalize_key(stripped));
    std::string tok;
    std::vector<std::string> words;
    while (is >> tok) {
        if (tok.empty()) continue;
        if (tok == "cie" || tok == "rev" || tok == "excepto") continue;
        if (norm_looks_like_cie10_fragment(tok)) continue;
        words.push_back(tok);
    }

    size_t start = 0;
    while (start < words.size()) {
        const std::string& word = words[start];
        if (is_leading_epidemiology_residue_word(word)) {
            ++start;
            continue;
        }
        if (word == "mal" && start + 1 < words.size() && words[start + 1] == "intoxicacion") {
            ++start;
            continue;
        }
        break;
    }

    std::ostringstream os;
    for (size_t i = start; i < words.size(); ++i) {
        if (i > start) os << ' ';
        os << words[i];
    }
    return trim(os.str());
}





HeaderSpan collect_header_span(const std::vector<Token>& toks, double x0, double x1, double y0, double y1, bool disease_only) {

    std::vector<Token> parts;

    for (const auto& t : toks) {

        if (t.box.cx() < x0 || t.box.cx() > x1 || t.box.cy() < y0 || t.box.cy() > y1) continue;

        if (disease_only && looks_like_header_noise(t.norm)) continue;

        parts.push_back(t);
    }

    std::sort(parts.begin(), parts.end(), [](const Token& a, const Token& b) {


        if (std::abs(a.box.cy() - b.box.cy()) > 4.0) return a.box.cy() < b.box.cy();

        return a.box.x0 < b.box.x0;
    });
    HeaderSpan span;
    std::ostringstream os;

    for (size_t i = 0; i < parts.size(); ++i) {

        if (i) os << ' ';
        os << parts[i].text;

        if (!span.found) { span.box = parts[i].box; span.found = true; }
        else span.box.include(parts[i].box);
    }

    span.text = trim(os.str());

    if (span.text.empty()) span.found = false;

    return span;
}



std::string collect_header_text(const std::vector<Token>& toks, double x0, double x1, double y0, double y1, bool disease_only) {

    return collect_header_span(toks, x0, x1, y0, y1, disease_only).text;
}

HeaderSpan collect_cie10_span(const std::vector<Token>& toks, double x0, double x1, double y0, double y1);

struct HeaderLineCandidate {
    std::vector<Token> tokens;
    Rect box;
    double y = 0.0;
    std::string text;
    std::string norm;
};

std::vector<HeaderLineCandidate> collect_semantic_header_lines(const std::vector<Token>& toks,
                                                               double x0,
                                                               double x1,
                                                               double y0,
                                                               double y1) {
    std::vector<Token> filtered;
    filtered.reserve(toks.size());
    for (const auto& t : toks) {
        if (t.box.cx() < x0 || t.box.cx() > x1 || t.box.cy() < y0 || t.box.cy() > y1) continue;
        if (looks_like_header_noise(t.norm)) continue;
        filtered.push_back(t);
    }
    std::sort(filtered.begin(), filtered.end(), [](const Token& a, const Token& b) {
        if (std::abs(a.box.cy() - b.box.cy()) > 4.5) return a.box.cy() < b.box.cy();
        return a.box.x0 < b.box.x0;
    });

    std::vector<HeaderLineCandidate> lines;
    for (const auto& t : filtered) {
        if (lines.empty() || std::abs(lines.back().y - t.box.cy()) > 5.2) {
            HeaderLineCandidate line;
            line.y = t.box.cy();
            line.box = t.box;
            line.tokens.push_back(t);
            lines.push_back(std::move(line));
        } else {
            auto& line = lines.back();
            const double n = static_cast<double>(line.tokens.size());
            line.y = (line.y * n + t.box.cy()) / (n + 1.0);
            line.box.include(t.box);
            line.tokens.push_back(t);
        }
    }

    for (auto& line : lines) {
        std::sort(line.tokens.begin(), line.tokens.end(), [](const Token& a, const Token& b) { return a.box.x0 < b.box.x0; });
        std::ostringstream os;
        for (size_t i = 0; i < line.tokens.size(); ++i) {
            if (i) os << ' ';
            os << line.tokens[i].text;
        }
        line.text = cleaned_disease_label_text(os.str());
        line.norm = normalize_key(line.text);
    }

    lines.erase(std::remove_if(lines.begin(), lines.end(), [](const HeaderLineCandidate& line) {
        if (line.text.empty() || line.norm.empty()) return true;
        if (looks_like_table_title_line(line.norm)) return true;
        return false;
    }), lines.end());
    return lines;
}

HeaderSpan collect_disease_label_span(const std::vector<Token>& toks,
                                      double x0,
                                      double x1,
                                      double y0,
                                      double y1,
                                      double table_top) {
    const HeaderSpan cie_span = collect_cie10_span(toks, x0, x1, y0, y1);
    double label_ceiling = y1;
    if (cie_span.found) label_ceiling = std::min(label_ceiling, std::max(y0, cie_span.box.y0 - 2.0));
    if (table_top > 0.0) label_ceiling = std::min(label_ceiling, std::max(y0, table_top - 30.0));

    auto lines = collect_semantic_header_lines(toks, x0, x1, y0, label_ceiling);
    if (lines.empty()) return {};

    std::sort(lines.begin(), lines.end(), [](const HeaderLineCandidate& a, const HeaderLineCandidate& b) {
        return a.y < b.y;
    });

    int last = static_cast<int>(lines.size()) - 1;
    while (last >= 0 && lines[static_cast<size_t>(last)].box.y1 > label_ceiling + 0.5) --last;
    if (last < 0) return {};

    int first = last;
    size_t combined_chars = lines[static_cast<size_t>(last)].text.size();
    while (first > 0) {
        const auto& prev = lines[static_cast<size_t>(first - 1)];
        const auto& cur = lines[static_cast<size_t>(first)];
        const double vertical_gap = std::max(0.0, cur.box.y0 - prev.box.y1);
        if (vertical_gap > 18.0) break;
        if (combined_chars + prev.text.size() + 1 > 135) break;
        if ((last - (first - 1) + 1) > 4) break;
        --first;
        combined_chars += prev.text.size() + 1;
    }

    HeaderSpan out;
    std::ostringstream os;
    for (int i = first; i <= last; ++i) {
        const auto& line = lines[static_cast<size_t>(i)];
        if (!line.text.empty()) {
            if (!out.found) { out.box = line.box; out.found = true; }
            else out.box.include(line.box);
            if (os.tellp() > 0) os << ' ';
            os << line.text;
        }
    }
    out.text = cleaned_disease_label_text(os.str());
    if (out.text.empty()) out.found = false;
    return out;
}





HeaderSpan collect_cie10_span(const std::vector<Token>& toks, double x0, double x1, double y0, double y1) {

    std::vector<Token> parts;

    auto token_has_cie10 = [](const std::string& raw) {

        std::string upper = raw;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        for (size_t i = 0; i < upper.size(); ++i) {

            if (!std::isalpha(static_cast<unsigned char>(upper[i]))) continue;

            if (i > 0 && std::isalnum(static_cast<unsigned char>(upper[i - 1]))) continue;
            size_t j = i + 1;

            while (j < upper.size() && std::isspace(static_cast<unsigned char>(upper[j]))) ++j;

            if (j + 1 < upper.size() &&
                (std::isdigit(static_cast<unsigned char>(upper[j])) || upper[j] == 'O') &&
                (std::isdigit(static_cast<unsigned char>(upper[j + 1])) || upper[j + 1] == 'O')) return true;
        }

        return false;
    };

    for (const auto& t : toks) {

        if (t.box.cx() < x0 || t.box.cx() > x1 || t.box.cy() < y0 || t.box.cy() > y1) continue;

        if (!token_has_cie10(t.text) && !contains_norm(t.norm, "cie")) continue;

        parts.push_back(t);
    }

    std::sort(parts.begin(), parts.end(), [](const Token& a, const Token& b) {

        if (std::abs(a.box.cy() - b.box.cy()) > 4.0) return a.box.cy() < b.box.cy();

        return a.box.x0 < b.box.x0;

    });
    HeaderSpan span;
    std::ostringstream os;

    for (size_t i = 0; i < parts.size(); ++i) {

        if (i) os << ' ';
        os << parts[i].text;

        if (!span.found) { span.box = parts[i].box; span.found = true; }
        else span.box.include(parts[i].box);
    }

    span.text = trim(os.str());

    if (span.text.empty()) span.found = false;

    return span;
}




std::string infer_role_from_header(const std::string& header_norm, int pos_in_group, int group_size, int doc_year, int& source_year, const HeaderGroup* group, double x_mid) {

    (void)doc_year;
    std::string sex_hint = "total";

    if (has_any_norm_word(header_norm, {"m", "masc", "masculino", "masculinos", "hombres", "varones"})) sex_hint = "M";

    else if (has_any_norm_word(header_norm, {"f", "fem", "femenino", "femeninos", "mujeres"})) sex_hint = "F";

    const bool has_sem = contains_norm(header_norm, "sem") || contains_norm(header_norm, "semana");

    const bool has_acum = contains_norm(header_norm, "acum") || contains_norm(header_norm, "acumulado");




    if (group_size >= 4) {

        if (pos_in_group == 0) return "Sem|total|weekly_incidence";

        if (pos_in_group == 1) return "Acum|M|current_year_accumulated_male";

        if (pos_in_group == 2) return "Acum|F|current_year_accumulated_female";

        return "Acum|total|previous_year_accumulated";
    }

    if (group_size == 3) {

        if (pos_in_group == 0) return "Sem|total|weekly_incidence";

        if (pos_in_group == 1) return "Acum|M|current_year_accumulated_male";

        return "Acum|F|current_year_accumulated_female";
    }



    if (group_size == 2 && sex_hint == "total") {
        if (pos_in_group == 0) return "Sem|total|weekly_incidence";
        return "Acum|total|accumulated";
    }
    if (has_sem && (pos_in_group == 0 || sex_hint == "total" || !has_acum)) return "Sem|total|weekly_incidence";

    if (has_acum && group_size <= 2 && pos_in_group >= 1) return "Acum|total|accumulated";

    if (sex_hint == "M") return "Acum|M|current_year_accumulated_male";

    if (sex_hint == "F") return "Acum|F|current_year_accumulated_female";



    if (group_size == 2) {

        if (pos_in_group == 0) return "Sem|total|weekly_incidence";

        return "Acum|total|accumulated";
    }

    if (has_sem) return "Sem|total|weekly_incidence";

    if (has_acum) return "Acum|total|accumulated";


    if (source_year == 0 && group) source_year = nearest_year(group->years, x_mid, doc_year);

    return (pos_in_group == 0 ? "Sem|total|weekly_incidence" : "Acum|total|accumulated");
}



void apply_role_triplet(ColumnBand& col, const std::string& triplet) {
    const size_t p1 = triplet.find('|');

    if (p1 == std::string::npos) { col.period = triplet; return; }
    const size_t p2 = triplet.find('|', p1 + 1);
    col.period = triplet.substr(0, p1);

    if (p2 == std::string::npos) {
        col.sex = triplet.substr(p1 + 1);

        return;
    }
    col.sex = triplet.substr(p1 + 1, p2 - p1 - 1);
    col.role = triplet.substr(p2 + 1);
}




std::string compact_cie10_code(std::string raw) {
    std::string code;

    code.reserve(raw.size());

    for (unsigned char ch : raw) {

        if (std::isspace(ch)) continue;

        if (ch == ',' || ch == ';' || ch == ':' || ch == '(' || ch == ')' || ch == '[' || ch == ']') continue;


        code.push_back(static_cast<char>(std::toupper(ch)));
    }

    for (size_t i = 1; i < code.size(); ++i) {


        if (code[i] == 'O') code[i] = '0';
    }

    while (!code.empty() && (code.back() == '.' || code.back() == '-')) code.pop_back();

    return code;
}




std::string extract_cie10_codes(const std::string& text) {
    std::string upper = text;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    std::set<std::string> codes;

    auto skip_spaces = [&](size_t& i) {
        while (i < upper.size() && std::isspace(static_cast<unsigned char>(upper[i]))) ++i;
    };

    auto digit_or_o = [&](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == 'O';
    };

    auto push_digit = [](std::string& code, char c) {
        code.push_back(c == 'O' ? '0' : c);
    };

    auto word_at = [&](size_t i, const char* word) {
        const size_t n = std::strlen(word);
        if (i + n > upper.size()) return false;
        for (size_t k = 0; k < n; ++k) {
            if (upper[i + k] != word[k]) return false;
        }
        const bool left = (i == 0 || !std::isalnum(static_cast<unsigned char>(upper[i - 1])));
        const bool right = (i + n >= upper.size() || !std::isalnum(static_cast<unsigned char>(upper[i + n])));
        return left && right;
    };

    auto parse_tail = [&](size_t& i, char implied_letter, std::string& code) {
        skip_spaces(i);
        if (i >= upper.size()) return false;

        if (std::isalpha(static_cast<unsigned char>(upper[i]))) {
            code.push_back(upper[i++]);
            skip_spaces(i);
        } else {
            code.push_back(implied_letter);
        }

        if (i + 1 >= upper.size() || !digit_or_o(upper[i]) || !digit_or_o(upper[i + 1])) return false;
        push_digit(code, upper[i++]);
        push_digit(code, upper[i++]);
        skip_spaces(i);

        if (i < upper.size() && upper[i] == '.') {
            const size_t dot = i++;
            skip_spaces(i);
            if (i < upper.size() && std::isalnum(static_cast<unsigned char>(upper[i]))) {
                code.push_back('.');
                code.push_back(upper[i++]);
            } else {
                i = dot;
            }
        }

        return true;
    };

    bool excluding = false;

    for (size_t i = 0; i < upper.size(); ++i) {
        if (word_at(i, "CIE")) {
            excluding = false;
        }
        if (word_at(i, "EXCEPTO") || word_at(i, "EXCEP")) {
            excluding = true;
            continue;
        }

        if (!std::isalpha(static_cast<unsigned char>(upper[i]))) continue;
        if (i > 0 && std::isalnum(static_cast<unsigned char>(upper[i - 1]))) continue;

        const char first_letter = upper[i];
        size_t j = i + 1;
        std::string code;
        code.reserve(14);

        if (!parse_tail(j, first_letter, code)) continue;
        size_t k = j;
        skip_spaces(k);

        if (k < upper.size() && upper[k] == '-') {
            ++k;
            std::string hi;
            hi.reserve(7);
            if (parse_tail(k, first_letter, hi)) {
                code.push_back('-');
                code += hi;
                j = k;
            }
        }

        if (!code.empty() && !excluding) codes.insert(compact_cie10_code(code));
        i = j > i ? j - 1 : i;
    }

    std::ostringstream os;
    bool first = true;
    for (const auto& code : codes) {
        if (!first) os << '|';
        first = false;
        os << code;
    }
    return os.str();
}



std::vector<HeaderYear> collect_years(const std::vector<Token>& toks, double x0, double x1, double y0, double y1) {

    std::vector<HeaderYear> years;

    for (const auto& t : toks) {

        if (t.box.cx() < x0 || t.box.cx() > x1 || t.box.cy() < y0 || t.box.cy() > y1) continue;

        const int year = first_year_20xx(t.norm);

        if (year > 0) years.push_back({year, t.box.cx()});
    }

    return years;
}



int nearest_year(const std::vector<HeaderYear>& years, double x, int fallback) {
    double best = std::numeric_limits<double>::max();


    int out = fallback;

    for (const auto& y : years) {
        const double d = std::abs(y.x - x);

        if (d < best) {
            best = d;

            out = y.year;
        }
    }

    return out;
}



int previous_distinct_year(const std::vector<HeaderYear>& years, int current_year) {

    int best = 0;

    for (const auto& y : years) {

        if (current_year > 0 && y.year >= current_year) continue;

        best = std::max(best, y.year);
    }

    return best;
}



std::string synthetic_disease_label(const std::string& label, const std::string& fallback) {

    const std::string clean = trim(label);

    if (!clean.empty()) return clean;

    return fallback;
}



std::string jurisdiction_name_by_id(const Config& config, const std::string& id) {


    for (const auto& j : config.jurisdictions()) {

        if (j.id == id) return j.canonical;
    }

    return id;
}




bool is_total_jurisdiction_id(const std::string& id) {

    return id == "total" || id == "edomex_total";
}



bool is_edomex_jurisdiction_id(const std::string& id) {

    return id.rfind("edomex_", 0) == 0 && !is_total_jurisdiction_id(id);
}



std::set<std::string> cdmx_expected_jurisdictions() {

    return {
        "gustavo_a_madero","azcapotzalco","iztacalco","coyoacan","alvaro_obregon","magdalena_contreras","cuajimalpa","tlalpan",
        "iztapalapa","xochimilco","milpa_alta","tlahuac","miguel_hidalgo","benito_juarez","cuauhtemoc","venustiano_carranza"
    };
}



std::set<std::string> edomex_expected_jurisdictions() {



    return {
        "edomex_atlacomulco","edomex_ixtlahuaca","edomex_jilotepec","edomex_tenango_del_valle","edomex_toluca",

        "edomex_xonacatlan","edomex_tejupilco","edomex_tenancingo","edomex_valle_de_bravo","edomex_atizapan",
        "edomex_cuautitlan","edomex_naucalpan","edomex_teotihuacan","edomex_tlalnepantla","edomex_zumpango",
        "edomex_amecameca","edomex_ecatepec","edomex_nezahualcoyotl","edomex_texcoco"
    };
}



std::set<std::string> expected_jurisdictions_for_rows(const std::vector<RowBand>& rows) {

    const auto cdmx = cdmx_expected_jurisdictions();
    const auto edomex = edomex_expected_jurisdictions();
    int cdmx_hits = 0;
    int edomex_hits = 0;

    std::set<std::string> dynamic;

    for (const auto& r : rows) {

        if (is_total_jurisdiction_id(r.jurisdiction_id)) continue;

        if (cdmx.count(r.jurisdiction_id)) ++cdmx_hits;

        if (edomex.count(r.jurisdiction_id)) ++edomex_hits;

        dynamic.insert(r.jurisdiction_id);
    }

    if (edomex_hits >= 6 && edomex_hits >= cdmx_hits) return edomex;

    if (cdmx_hits >= 6) return cdmx;



    return dynamic;
}

}




std::vector<TableEngine::Line> TableEngine::make_lines(const PageText& page) const {

    std::vector<Token> toks = page.tokens;


    std::sort(toks.begin(), toks.end(), [](const Token& a, const Token& b) {

        if (std::abs(a.box.cy() - b.box.cy()) > 3.0) return a.box.cy() < b.box.cy();

        return a.box.x0 < b.box.x0;

    });

    std::vector<Line> lines;
    const double y_tol = 4.2;

    for (const auto& t : toks) {
        bool placed = false;

        for (auto& line : lines) {

            if (std::abs(line.y - t.box.cy()) <= y_tol) {

                line.tokens.push_back(t);
                line.box.include(t.box);
                line.y = (line.y * (line.tokens.size() - 1) + t.box.cy()) / static_cast<double>(line.tokens.size());
                placed = true;
                break;
            }
        }

        if (!placed) {
            Line l;
            l.index = static_cast<int>(lines.size());

            l.y = t.box.cy();
            l.box = t.box;

            l.tokens.push_back(t);


            lines.push_back(std::move(l));
        }
    }

    for (auto& l : lines) {
        std::sort(l.tokens.begin(), l.tokens.end(), [](const Token& a, const Token& b){ return a.box.x0 < b.box.x0; });
        std::ostringstream text;

        for (size_t i=0; i<l.tokens.size(); ++i) {

            if (i) text << ' ';

            text << l.tokens[i].text;
        }
        l.text = text.str();


        l.norm = normalize_key(l.text);
    }

    std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b){ return a.y < b.y; });

    for (size_t i=0; i<lines.size(); ++i) lines[i].index = static_cast<int>(i);

    return lines;
}




std::vector<RowBand> TableEngine::detect_rows(const std::vector<Line>& lines) const {

    std::vector<RowBand> rows;

    std::set<std::string> seen;

    for (const auto& l : lines) {
        auto j = config_.match_jurisdiction_line(l.norm);

        if (!j) continue;


        if (seen.count(j->id)) continue;

        RowBand r;
        r.jurisdiction_id = j->id;
        r.jurisdiction = j->canonical;
        bool first_label_token = true;
        Rect label;
        bool matched_label_span = false;
        Rect matched_label;
        for (size_t i = 0; i < l.tokens.size() && !matched_label_span; ++i) {
            if (is_numeric_token(l.tokens[i].text)) continue;
            std::string fragment;
            Rect fragment_box;
            bool have_fragment_box = false;
            for (size_t k = i; k < l.tokens.size() && k < i + 4; ++k) {
                if (is_numeric_token(l.tokens[k].text)) break;
                if (!fragment.empty()) fragment.push_back(' ');
                fragment += l.tokens[k].text;
                if (!have_fragment_box) { fragment_box = l.tokens[k].box; have_fragment_box = true; }
                else fragment_box.include(l.tokens[k].box);
                const std::string fragment_norm = normalize_key(fragment);
                for (const auto& alias_norm : j->aliases_norm) {
                    if (!alias_norm.empty() && contains_norm(fragment_norm, alias_norm)) {
                        matched_label = fragment_box;
                        matched_label_span = true;
                        break;
                    }
                }
                if (matched_label_span) break;
            }
        }

        for (const auto& tok : l.tokens) {

            if (is_numeric_token(tok.text)) break;

            if (first_label_token) { label = tok.box; first_label_token = false; }
            else label.include(tok.box);
        }
        r.label_box = matched_label_span ? matched_label : (first_label_token ? l.box : label);
        r.y_mid = l.y;
        r.line_index = l.index;

        rows.push_back(r);

        seen.insert(j->id);
    }

    if (rows.size() < 10) return {};


    std::sort(rows.begin(), rows.end(), [](const RowBand& a, const RowBand& b){ return a.y_mid < b.y_mid; });

    for (size_t i=0; i<rows.size(); ++i) {


        const double prev = (i == 0) ? rows[i].y_mid - 10.0 : rows[i-1].y_mid;

        const double next = (i + 1 == rows.size()) ? rows[i].y_mid + 10.0 : rows[i+1].y_mid;

        rows[i].y0 = (prev + rows[i].y_mid) * 0.5;

        rows[i].y1 = (rows[i].y_mid + next) * 0.5;
    }

    return rows;
}








std::vector<std::vector<RowBand>> TableEngine::detect_row_blocks(const std::vector<Line>& lines) const {

    std::vector<std::vector<RowBand>> blocks;
    std::vector<RowBand> current;
    std::set<std::string> seen_in_block;
    double previous_row_y = -1.0;

    auto finalize_block = [&]() {
        if (current.size() >= 10) {
            std::sort(current.begin(), current.end(), [](const RowBand& a, const RowBand& b){ return a.y_mid < b.y_mid; });
            for (size_t i = 0; i < current.size(); ++i) {
                const double prev = (i == 0) ? current[i].y_mid - 10.0 : current[i - 1].y_mid;
                const double next = (i + 1 == current.size()) ? current[i].y_mid + 10.0 : current[i + 1].y_mid;
                current[i].y0 = (prev + current[i].y_mid) * 0.5;
                current[i].y1 = (current[i].y_mid + next) * 0.5;
            }
            blocks.push_back(current);
        }
        current.clear();
        seen_in_block.clear();
        previous_row_y = -1.0;
    };

    auto row_from_line = [&](const Line& l, const Jurisdiction& j) -> RowBand {
        RowBand r;
        r.jurisdiction_id = j.id;
        r.jurisdiction = j.canonical;

        bool first_label_token = true;
        Rect label;
        bool matched_label_span = false;
        Rect matched_label;

        for (size_t i = 0; i < l.tokens.size() && !matched_label_span; ++i) {
            if (is_numeric_token(l.tokens[i].text)) continue;
            std::string fragment;
            Rect fragment_box;
            bool have_fragment_box = false;
            for (size_t k = i; k < l.tokens.size() && k < i + 4; ++k) {
                if (is_numeric_token(l.tokens[k].text)) break;
                if (!fragment.empty()) fragment.push_back(' ');
                fragment += l.tokens[k].text;
                if (!have_fragment_box) { fragment_box = l.tokens[k].box; have_fragment_box = true; }
                else fragment_box.include(l.tokens[k].box);
                const std::string fragment_norm = normalize_key(fragment);
                for (const auto& alias_norm : j.aliases_norm) {
                    if (!alias_norm.empty() && contains_norm(fragment_norm, alias_norm)) {
                        matched_label = fragment_box;
                        matched_label_span = true;
                        break;
                    }
                }
                if (matched_label_span) break;
            }
        }

        for (const auto& tok : l.tokens) {
            if (is_numeric_token(tok.text)) break;
            if (first_label_token) { label = tok.box; first_label_token = false; }
            else label.include(tok.box);
        }

        r.label_box = matched_label_span ? matched_label : (first_label_token ? l.box : label);
        r.y_mid = l.y;
        r.line_index = l.index;
        return r;
    };

    for (const auto& l : lines) {
        auto j = config_.match_jurisdiction_line(l.norm);
        if (!j) continue;

        const bool is_total = is_total_jurisdiction_id(j->id);
        const bool repeated_inside_table = !is_total && seen_in_block.count(j->id) != 0;
        const double vertical_gap = (previous_row_y < 0.0) ? 0.0 : (l.y - previous_row_y);

        /*
           Un boletín CDMX moderno puede alojar dos o tres cuadros en la misma
           página. El algoritmo anterior imponía unicidad de alcaldía en toda la
           página; eso hacía que sólo sobreviviera el primer cuadro y obligaba a
           reprocesar páginas ya visitadas para descubrir los siguientes. Aquí la
           unidad mínima de inferencia es el bloque jurisdiccional: cuando vuelve
           a aparecer una alcaldía no-total dentro de una secuencia ya madura, se
           cierra el cuadro actual y el siguiente se reconstruye en la misma
           pasada de la página. No se reabre el PDF ni se vuelve a recorrer el
           documento para extraer cuadros hermanos.
        */
        if (!current.empty() && current.size() >= 8 && (repeated_inside_table || vertical_gap > 58.0)) {
            finalize_block();
        }

        if (!current.empty() && !is_total && seen_in_block.count(j->id) != 0) {
            continue;
        }

        RowBand r = row_from_line(l, *j);
        current.push_back(std::move(r));
        if (!is_total) seen_in_block.insert(j->id);
        previous_row_y = l.y;
    }

    finalize_block();

    return blocks;
}



std::vector<Token> TableEngine::numeric_tokens_in_rows(const PageText& page, const std::vector<RowBand>& rows) const {

    std::vector<Token> nums;

    if (rows.empty()) return nums;

    std::vector<double> label_rights;

    for (const auto& r : rows) label_rights.push_back(r.label_box.x1);
    std::sort(label_rights.begin(), label_rights.end());
    const double min_label_right = label_rights.empty() ? 0.0 : label_rights[label_rights.size() / 2];

    for (const auto& t : page.tokens) {

        if (!is_numeric_token(t.text)) continue;

        if (t.box.cx() <= min_label_right + 6.0) continue;

        for (const auto& r : rows) {

            if (t.box.cy() >= r.y0 && t.box.cy() <= r.y1) {

                nums.push_back(t);


                break;
            }
        }
    }

    return nums;
}




std::vector<ColumnBand> TableEngine::cluster_columns(const std::vector<Token>& nums, const PageText&, const std::vector<RowBand>& rows) const {


    struct Cluster {

        std::vector<Token> tokens;
        double xsum = 0.0;
        Rect box;
    };

    std::vector<Cluster> clusters;
    const double x_tol = 7.5;

    std::vector<Token> sorted = nums;

    std::sort(sorted.begin(), sorted.end(), [](const Token& a, const Token& b){ return a.box.cx() < b.box.cx(); });

    for (const auto& n : sorted) {
        bool placed = false;

        for (auto& c : clusters) {
            const double mean = c.xsum / static_cast<double>(c.tokens.size());

            if (std::abs(mean - n.box.cx()) <= x_tol) {

                c.tokens.push_back(n);

                c.xsum += n.box.cx();

                c.box.include(n.box);
                placed = true;
                break;
            }
        }

        if (!placed) {
            Cluster c;

            c.tokens.push_back(n);
            c.xsum = n.box.cx();
            c.box = n.box;

            clusters.push_back(std::move(c));
        }
    }

    std::vector<ColumnBand> cols;

    for (auto& c : clusters) {

        if (c.tokens.size() < std::max<size_t>(6, rows.size()/3)) continue;

        ColumnBand col;
        col.index = static_cast<int>(cols.size());
        col.x_mid = c.xsum / static_cast<double>(c.tokens.size());
        col.x0 = c.box.x0 - 4.0;
        col.x1 = c.box.x1 + 4.0;

        cols.push_back(col);
    }



    std::sort(cols.begin(), cols.end(), [](const ColumnBand& a, const ColumnBand& b){ return a.x_mid < b.x_mid; });

    for (size_t i=0; i<cols.size(); ++i) cols[i].index = static_cast<int>(i);

    if (cols.size() >= 2) {

        for (size_t i=0; i<cols.size(); ++i) {
            const double left = (i == 0) ? cols[i].x0 : (cols[i-1].x_mid + cols[i].x_mid) * 0.5;
            const double right = (i + 1 == cols.size()) ? cols[i].x1 : (cols[i].x_mid + cols[i+1].x_mid) * 0.5;
            cols[i].x0 = left;
            cols[i].x1 = right;
        }
    }

    return cols;
}




void TableEngine::assign_cells(TableCandidate& t, const std::vector<Token>& nums) const {

    for (const auto& n : nums) {

        const RowBand* row = nullptr;

        for (const auto& r : t.rows) {

            if (n.box.cy() >= r.y0 && n.box.cy() <= r.y1) { row = &r; break; }
        }

        if (!row) continue;

        const ColumnBand* col = nullptr;

        for (const auto& c : t.columns) {

            if (n.box.cx() >= c.x0 && n.box.cx() <= c.x1) { col = &c; break; }
        }


        if (!col) continue;

        const auto cell_key = std::make_pair(row->jurisdiction_id, col->index);


        ParsedValue pv;
        pv.raw = n.text;


        pv.value = parse_epi_int(n.text);
        pv.box = n.box;
        auto it = t.cells.find(cell_key);

        if (it != t.cells.end()) {

            t.duplicate_cells.insert(cell_key);
            const bool existing_valid = it->second.value.has_value();

            const bool candidate_valid = pv.value.has_value();

            if ((!existing_valid && candidate_valid) ||

                (existing_valid == candidate_valid && pv.box.width() > it->second.box.width())) {
                it->second = pv;
            }
            continue;
        }
        t.cells[cell_key] = pv;
    }
}



std::string TableEngine::infer_period(const std::string& header_norm, int ordinal_mod) {

    if (header_norm.find("acum") != std::string::npos) return "Acum";


    if (header_norm.find("sem") != std::string::npos) return "Sem";

    if (ordinal_mod == 0) return "Sem";

    return "Acum";
}



std::string TableEngine::infer_sex(const std::string& header_norm) {

    if (has_norm_word(header_norm, "m")) return "M";

    if (has_norm_word(header_norm, "f")) return "F";

    return "total";
}



static std::string fast_cuadro_title_from_norm(const std::string& header_norm) {
    size_t p = header_norm.find("cuadro");

    if (p == std::string::npos) return "Cuadro_detectado";
    p += 6;

    while (p < header_norm.size() && header_norm[p] == ' ') ++p;

    if (p >= header_norm.size() || !std::isdigit(static_cast<unsigned char>(header_norm[p]))) return "Cuadro_detectado";
    std::string n;

    n.reserve(5);

    while (p < header_norm.size() && std::isdigit(static_cast<unsigned char>(header_norm[p])) && n.size() < 2) n.push_back(header_norm[p++]);
    size_t q = p;

    while (q < header_norm.size() && header_norm[q] == ' ') ++q;


    if (q < header_norm.size() && std::isdigit(static_cast<unsigned char>(header_norm[q]))) {

        n.push_back(' ');

        while (q < header_norm.size() && std::isdigit(static_cast<unsigned char>(header_norm[q])) && n.size() < 5) n.push_back(header_norm[q++]);
    }

    return n.empty() ? "Cuadro_detectado" : ("Cuadro " + n);
}





void TableEngine::infer_headers(TableCandidate& t, const PdfDocument& doc, const PageText& page) const {

    if (t.rows.empty()) return;
    const double header_y0 = 0.0;

    const double header_y1 = std::max(0.0, t.rows.front().y0 - 2.0);

    std::vector<Token> header_tokens;

    for (const auto& tok : page.tokens) {

        if (tok.box.cy() >= header_y0 && tok.box.cy() <= header_y1) header_tokens.push_back(tok);
    }
    std::string all_header;

    for (const auto& h : header_tokens) all_header += h.text + " ";


    std::string header_norm = normalize_key(all_header);
    std::string title = fast_cuadro_title_from_norm(header_norm);
    t.table_title = title;

    t.table_id = safe_filename("p" + std::to_string(t.page) + "_" + title);


    const double table_top = t.rows.front().y0;


    const double disease_y0 = std::max(0.0, table_top - 120.0);
    const double disease_y1 = std::max(0.0, table_top - 56.0);
    const double role_y0 = std::max(0.0, table_top - 46.0);


    std::vector<std::string> local_norm_hints;

    local_norm_hints.reserve(t.columns.size());

    for (const auto& col : t.columns) {
        const double wx0 = col.x0 - 22.0;
        const double wx1 = col.x1 + 22.0;
        HeaderSpan local_span = collect_header_span(header_tokens, wx0, wx1, role_y0, header_y1, false);

        if (!local_span.found) local_span = collect_header_span(header_tokens, wx0, wx1, header_y0, header_y1, false);


        local_norm_hints.push_back(normalize_key(local_span.text));
    }

    const auto expected_for_table = expected_jurisdictions_for_rows(t.rows);
    int edomex_row_hits = 0;
    int cdmx_row_hits = 0;
    for (const auto& id : expected_for_table) {
        if (is_edomex_jurisdiction_id(id)) ++edomex_row_hits;
        if (cdmx_expected_jurisdictions().count(id) != 0) ++cdmx_row_hits;
    }
    std::vector<ColumnGroupRange> column_groups;
    bool edomex_legacy_institution_tail = false;
    if (edomex_row_hits >= 8 && edomex_row_hits > cdmx_row_hits) {
        std::string edomex_header_norm = header_norm;
        std::string page_header_text;
        std::string page_all_text;
        for (const auto& tok : page.tokens) {
            page_all_text += tok.text + " ";
            if (tok.box.cy() <= table_top + 2.0) page_header_text += tok.text + " ";
        }
        edomex_header_norm = normalize_key(edomex_header_norm + " " + page_header_text);
        const std::string edomex_page_norm = normalize_key(page_all_text);
        column_groups = edomex_modern_column_groups(static_cast<int>(t.columns.size()), edomex_header_norm, local_norm_hints, doc.bulletin_year);
        if (column_groups.empty()) {
            column_groups = edomex_legacy_column_groups(static_cast<int>(t.columns.size()), edomex_header_norm);
            edomex_legacy_institution_tail = !column_groups.empty() &&
                (is_non_epidemiological_column_group(edomex_header_norm) ||
                 is_non_epidemiological_column_group(edomex_page_norm) ||
                 contains_norm(edomex_header_norm, "por institucion") ||
                 contains_norm(edomex_page_norm, "por institucion") ||
                 contains_norm(edomex_header_norm, "institucion") ||
                 contains_norm(edomex_page_norm, "institucion") ||
                 contains_norm(edomex_header_norm, "institu ciones") ||
                 contains_norm(edomex_page_norm, "institu ciones") ||
                 contains_norm(edomex_header_norm, "totales por jurisdiccion") ||
                 contains_norm(edomex_page_norm, "totales por jurisdiccion") ||
                 contains_norm(edomex_header_norm, "suma por institucion") ||
                 contains_norm(edomex_page_norm, "suma por institucion") ||
                 contains_norm(edomex_header_norm, "total edo") ||
                 contains_norm(edomex_page_norm, "total edo") ||
                 contains_norm(edomex_header_norm, "isem") ||
                 contains_norm(edomex_page_norm, "isem") ||
                 contains_norm(edomex_header_norm, "imss") ||
                 contains_norm(edomex_page_norm, "imss") ||
                 contains_norm(edomex_header_norm, "issste") ||
                 contains_norm(edomex_page_norm, "issste") ||
                 contains_norm(edomex_header_norm, "difem") ||
                 contains_norm(edomex_page_norm, "difem"));
        }
    }
    if (column_groups.empty()) {
        column_groups = infer_column_groups_from_geometry_and_headers(t.columns, local_norm_hints);
    }


    std::vector<HeaderGroup> groups;

    for (const auto& range : column_groups) {
        HeaderGroup g;
        g.first_col = range.first_col;
        g.last_col = range.last_col;

        g.x0 = t.columns[range.first_col].x0 - 24.0;


        g.x1 = t.columns[range.last_col].x1 + 24.0;
        HeaderSpan label_span;
        if (edomex_row_hits >= 8 && edomex_row_hits > cdmx_row_hits) {
            label_span = collect_disease_label_span(header_tokens, g.x0, g.x1, header_y0, header_y1, table_top);
        } else {
            label_span = collect_header_span(header_tokens, g.x0, g.x1, disease_y0, disease_y1, true);
        }
        if (!label_span.found) label_span = collect_header_span(header_tokens, g.x0, g.x1, disease_y0, disease_y1, true);
        if (!label_span.found) label_span = collect_header_span(header_tokens, g.x0, g.x1, header_y0, disease_y1, true);
        g.label = cleaned_disease_label_text(label_span.text);

        if (label_span.found) g.label_box = label_span.box;


        g.norm = normalize_key(g.label);
        HeaderSpan group_all_span = collect_header_span(header_tokens, g.x0, g.x1, header_y0, header_y1, false);
        std::string group_all = group_all_span.text;
        const std::string group_all_norm = normalize_key(group_all);
        const std::string group_match_norm = disease_label_match_norm(group_all);
        const std::string preliminary_cie10 = extract_cie10_codes(group_all);
        const size_t edomex_legacy_tail_index = groups.size();
        const bool edomex_legacy_right_institution_tail =
            edomex_legacy_institution_tail && t.page_box.x0 > 1.0;
        const char* const edomex_legacy_right_tail_diseases[] = {
            "Mordeduras por serpiente",
            "Violencia intrafamiliar",
            "Otras diversas"
        };
        const size_t edomex_legacy_right_tail_disease_count =
            sizeof(edomex_legacy_right_tail_diseases) / sizeof(edomex_legacy_right_tail_diseases[0]);
        if (edomex_legacy_right_institution_tail && edomex_legacy_tail_index < edomex_legacy_right_tail_disease_count) {
            g.label = edomex_legacy_right_tail_diseases[edomex_legacy_tail_index];
            g.norm = normalize_key(g.label);
        }
        const bool edomex_legacy_tail_group =
            edomex_legacy_right_institution_tail && edomex_legacy_tail_index >= edomex_legacy_right_tail_disease_count;
        const std::string label_match_norm = disease_label_match_norm(g.label);
        const bool title_fragment_group =
            (contains_norm(g.norm, "casos por jurisdiccion") ||
             (contains_norm(g.norm, "hasta la") && contains_norm(g.norm, "epidemiologica")) ||
             contains_norm(g.norm, "enfermedades transmitidas por") ||
             (contains_norm(g.norm, "enfermedades transmitidas por") && contains_norm(g.norm, "epidemiologica")));
        const bool has_cie10_evidence = !preliminary_cie10.empty();
        const bool label_unusable = is_generic_or_unusable_disease_label_norm(g.norm);
        const bool all_unusable = is_generic_or_unusable_disease_label_norm(group_all_norm);
        const bool admin_noise_group =
            !has_cie10_evidence &&
            (is_document_admin_noise_norm(g.norm) ||
             (label_unusable && is_document_admin_noise_norm(group_all_norm)));
        const bool unsupported_synthetic_group =
            !has_cie10_evidence &&
            label_unusable &&
            all_unusable;
        if (edomex_legacy_tail_group ||
            (edomex_row_hits >= 8 && label_unusable) ||
            is_non_epidemiological_column_group(g.norm) ||
            is_non_epidemiological_column_group(group_all_norm) ||
            title_fragment_group ||
            admin_noise_group ||
            unsupported_synthetic_group) {
            g.disease_id = "__skip_epidemiology_column__";
            g.disease.clear();
            g.cie10.clear();
            g.confidence = 1.0;
            groups.push_back(std::move(g));
            continue;
        }
        g.cie10 = preliminary_cie10;
        HeaderSpan cie_span = collect_cie10_span(header_tokens, g.x0, g.x1, header_y0, header_y1);

        if (cie_span.found) g.cie10_box = cie_span.box;

        g.years = collect_years(header_tokens, g.x0, g.x1, role_y0, header_y1);
        auto disease = label_unusable ? std::optional<Disease>{} :
            config_.match_disease_text(label_match_norm.empty() ? g.norm : label_match_norm);


        if (!disease && !all_unusable && !group_match_norm.empty()) disease = config_.match_disease_text(group_match_norm);
        if (!disease && !all_unusable) disease = config_.match_disease_text(group_all_norm);

        auto disease_by_cie10 = config_.match_disease_cie10(g.cie10);

        bool disease_from_cie10_only = false;
        if (!disease && disease_by_cie10) {
            disease = disease_by_cie10;
            disease_from_cie10_only = true;
        }
        const bool disease_cie10_agrees = disease && disease_by_cie10 && disease_by_cie10->id == disease->id;

        if (disease) {
            g.disease_id = disease->id;

            g.disease = disease->canonical;
            const std::string configured_cie = join_pipe(disease->cie10);
            g.cie10 = configured_cie.empty() ? g.cie10 : configured_cie;
            g.confidence = disease_from_cie10_only && g.norm.empty() ? 0.93 : (disease_cie10_agrees ? 0.96 : 0.94);
        } else {
            if (edomex_row_hits >= 8 && !has_cie10_evidence) {
                g.disease_id = "__skip_epidemiology_column__";
                g.disease.clear();
                g.cie10.clear();
                g.confidence = 1.0;
                groups.push_back(std::move(g));
                continue;
            }
            const std::string label = synthetic_disease_label(
                strip_parenthetical_disease_noise(g.label),
                "Enfermedad no catalogada " + title + " grupo " + std::to_string(static_cast<int>(groups.size()) + 1));
            const std::string label_norm = normalize_key(label);
            const bool synthetic_has_signal =
                has_epidemiology_label_signal_norm(label_norm) ||
                has_epidemiology_label_signal_norm(group_all_norm);
            if (is_generic_or_unusable_disease_label_norm(label_norm) || !synthetic_has_signal) {
                g.disease_id = "__skip_epidemiology_column__";
                g.disease.clear();
                g.cie10.clear();
                g.confidence = 1.0;
                groups.push_back(std::move(g));
                continue;
            }


            g.disease_id = safe_filename("auto_" + normalize_key(label));

            g.disease = label;
            g.confidence = g.norm.empty() ? 0.56 : 0.68;
        }

        groups.push_back(std::move(g));
    }


    for (auto& col : t.columns) {

        const int group_index = group_index_for_column(column_groups, col.index);
        const HeaderGroup* group = (group_index >= 0 && group_index < static_cast<int>(groups.size())) ? &groups[static_cast<size_t>(group_index)] : nullptr;
        const int first_col = group ? group->first_col : 0;

        const int last_col = group ? group->last_col : static_cast<int>(t.columns.size()) - 1;
        const int group_size = std::max(1, last_col - first_col + 1);

        const int pos_in_group = std::clamp(col.index - first_col, 0, group_size - 1);
        col.group_index = group_index;

        const double wx0 = col.x0 - 22.0;
        const double wx1 = col.x1 + 22.0;
        HeaderSpan local_span = collect_header_span(header_tokens, wx0, wx1, role_y0, header_y1, false);

        if (!local_span.found) local_span = collect_header_span(header_tokens, wx0, wx1, header_y0, header_y1, false);
        const std::string local = local_span.text;


        const std::string local_norm = normalize_key(local);
        col.header_text = trim(local);

        if (local_span.found) col.header_box = local_span.box;
        else col.header_box = {col.x0, role_y0, col.x1, std::max(role_y0 + 1.0, table_top - 1.0)};

        if (group) {
            col.disease_id = group->disease_id;
            col.disease = group->disease;
            col.cie10 = group->cie10;
            col.disease_box = group->label_box;
            col.cie10_box = group->cie10_box;
            col.header_confidence = group->confidence;
        } else {

            col.disease_id = "auto_" + safe_filename(title);

            col.disease = "Enfermedad no catalogada " + title;
            col.header_confidence = 0.54;
        }


        int source_year = first_year_20xx(local_norm);

        const int current_year = doc.bulletin_year > 0 ? doc.bulletin_year : (source_year > 0 ? source_year : 0);

        const int previous_year = group ? previous_distinct_year(group->years, current_year) : 0;


        apply_role_triplet(col, infer_role_from_header(local_norm, pos_in_group, group_size, current_year, source_year, group, col.x_mid));

        col.expected_role = role_key_from_column(col);

        if (col.period == "Sem") {

            source_year = current_year > 0 ? current_year : source_year;

        } else if (col.sex == "M" || col.sex == "F") {

            source_year = current_year > 0 ? current_year : (group ? nearest_year(group->years, col.x_mid, source_year) : source_year);

        } else if (col.role == "previous_year_accumulated") {

            source_year = source_year > 0 ? source_year : (previous_year > 0 ? previous_year : (current_year > 1 ? current_year - 1 : current_year));
        } else {

            if (source_year == 0 && group) source_year = nearest_year(group->years, col.x_mid, current_year);

            if (source_year == 0) source_year = current_year;
        }

        col.source_year = source_year > 0 ? std::to_string(source_year) : "";

        if (col.cie10.empty()) col.cie10 = extract_cie10_codes(local);
    }



    for (int gi = 0; gi < static_cast<int>(column_groups.size()); ++gi) {

        const auto& range = column_groups[static_cast<size_t>(gi)];

        std::set<std::string> present_roles;

        for (int ci = range.first_col; ci <= range.last_col && ci < static_cast<int>(t.columns.size()); ++ci) {

            present_roles.insert(role_key_from_column(t.columns[static_cast<size_t>(ci)]));
        }

        std::vector<std::string> missing;

        for (const std::string& expected : {std::string("sem_total"), std::string("acum_m"), std::string("acum_f"), std::string("acum_total_previous")}) {

            if (present_roles.count(expected) == 0) missing.push_back(expected);
        }

        if (!missing.empty()) {


            const std::string note = "pdf_layout_missing_columns=" + join_missing_roles(missing);

            for (int ci = range.first_col; ci <= range.last_col && ci < static_cast<int>(t.columns.size()); ++ci) {

                t.columns[static_cast<size_t>(ci)].group_layout_note = note;
            }
        }
    }
}



std::string TableEngine::make_column_key(const ColumnBand& c) {

    return c.disease_id + "|" + c.source_year + "|" + c.period + "|" + c.sex + "|col" + std::to_string(c.index);
}





void TableEngine::validate_and_materialize(TableCandidate& t, const PdfDocument& doc) const {

    const std::set<std::string> non_total = expected_jurisdictions_for_rows(t.rows);
    const int expected_present = static_cast<int>(non_total.size());

    if (expected_present <= 0) return;


    for (const auto& col : t.columns) {
        if (col.disease_id == "__skip_epidemiology_column__") continue;

        const auto key = make_column_key(col);
        int present = 0;
        int64_t sum = 0;
        std::optional<int64_t> total;
        bool invalid_numeric = false;
        bool duplicate_cell = false;

        std::set<std::string> present_ids;

        std::set<std::string> missing_ids = non_total;

        for (const auto& r : t.rows) {

            if (t.duplicate_cells.count({r.jurisdiction_id, col.index}) > 0) duplicate_cell = true;

            auto it = t.cells.find({r.jurisdiction_id, col.index});

            if (it == t.cells.end()) continue;

            if (!it->second.value) { invalid_numeric = true; continue; }

            if (is_total_jurisdiction_id(r.jurisdiction_id)) total = *it->second.value;

            else if (non_total.count(r.jurisdiction_id)) {
                sum += *it->second.value;
                present++;


                present_ids.insert(r.jurisdiction_id);

                missing_ids.erase(r.jurisdiction_id);
            }
        }



        std::map<std::string, ParsedValue> values_by_jurisdiction;

        for (const auto& r : t.rows) {

            if (!non_total.count(r.jurisdiction_id)) continue;
            auto it = t.cells.find({r.jurisdiction_id, col.index});

            if (it != t.cells.end() && it->second.value) values_by_jurisdiction[r.jurisdiction_id] = it->second;
        }


        std::string source_year = col.source_year;
        double confidence_penalty = 0.0;

        if (source_year.empty()) {

            source_year = std::to_string(doc.bulletin_year);
            confidence_penalty += 0.10;
        }

        if (doc.bulletin_year <= 0 || doc.bulletin_week <= 0 || doc.bulletin_week > 53) confidence_penalty += 0.18;

        if (col.header_confidence < 0.65) confidence_penalty += 0.10;

        if (invalid_numeric) confidence_penalty += 0.05;

        if (duplicate_cell) confidence_penalty += 0.04;



        auto emit_one = [&](const std::string& jurisdiction_id, const std::string& jurisdiction_name, const ParsedValue& pv,
                            const std::string& validation_rule, double extra_penalty) {


            if (!pv.value) return;
            Observation o;


            o.pdf_file = doc.file_name;
            o.pdf_id = doc.stable_id;

            o.page = t.page;

            o.bulletin_year = doc.bulletin_year;

            o.bulletin_week = doc.bulletin_week;
            o.table_id = t.table_id;
            o.table_title = t.table_title;
            o.disease_id = col.disease_id;
            o.disease = col.disease;
            o.cie10 = col.cie10;
            o.jurisdiction_id = jurisdiction_id;
            o.jurisdiction = jurisdiction_name;

            o.source_year = source_year;

            o.period = col.period;
            o.sex = col.sex;
            o.raw_value = pv.raw;

            o.value = *pv.value;
            o.confidence = std::clamp(0.65 + col.header_confidence * 0.30 + 0.04 - confidence_penalty - extra_penalty, 0.50, 0.99);
            o.cell_box = pv.box;
            o.validation_rule = validation_rule;

            t.accepted.push_back(std::move(o));
        };


        auto emit = [&](const std::string& validation_rule, double extra_penalty) {

            std::set<std::string> emitted;

            for (const auto& r : t.rows) {

                if (!non_total.count(r.jurisdiction_id)) continue;
                auto it = values_by_jurisdiction.find(r.jurisdiction_id);

                if (it == values_by_jurisdiction.end() || !it->second.value) continue;
                emit_one(r.jurisdiction_id, r.jurisdiction, it->second, validation_rule, extra_penalty);

                emitted.insert(r.jurisdiction_id);
            }



            for (const auto& [jurisdiction_id, pv] : values_by_jurisdiction) {

                if (emitted.count(jurisdiction_id) || !non_total.count(jurisdiction_id) || !pv.value) continue;
                emit_one(jurisdiction_id, jurisdiction_name_by_id(config_, jurisdiction_id), pv, validation_rule, extra_penalty);
            }
        };


        if (values_by_jurisdiction.empty()) continue;

        std::string prefix;


        if (invalid_numeric) prefix += "non_numeric_cells_skipped;";

        if (duplicate_cell) prefix += "duplicate_cell_best_token_kept;";

        if (!col.group_layout_note.empty()) prefix += col.group_layout_note + ";";

        if (doc.bulletin_year <= 0 || doc.bulletin_week <= 0 || doc.bulletin_week > 53) prefix += "week_unresolved;";

        if (col.header_confidence < 0.65) prefix += "synthetic_or_low_confidence_header;";


        if (present == expected_present && total && sum == *total) {
            emit(prefix + "sum_" + std::to_string(expected_present) + "_jurisdictions_equals_total", 0.0);
            continue;
        }

        if (present == expected_present - 1 && total && missing_ids.size() == 1 && *total >= sum) {
            const int64_t delta = *total - sum;


            ParsedValue imputed;
            imputed.raw = "[imputed_from_total]";
            imputed.value = delta;
            values_by_jurisdiction[*missing_ids.begin()] = imputed;
            emit(prefix + "sum_" + std::to_string(expected_present - 1) + "_jurisdictions_plus_one_imputed_equals_total", 0.16);
            continue;
        }

        if (present == expected_present && !total) {

            emit(prefix + "observed_" + std::to_string(expected_present) + "_jurisdictions_total_absent_review", 0.20);

            continue;
        }

        if (total && present == expected_present) {
            emit(prefix + "total_mismatch_kept_total_only_as_check;sum=" + std::to_string(sum) + ";total=" + std::to_string(*total), 0.18);
            continue;
        }

        if (total) {

            emit(prefix + "partial_column_kept_total_only_as_check;present=" + std::to_string(present) + ";total=" + std::to_string(*total), 0.28);
        } else {

            emit(prefix + "partial_column_kept_total_absent;present=" + std::to_string(present), 0.30);
        }
    }
}





std::vector<TableCandidate> TableEngine::reconstruct_page(const PdfDocument& doc, const PageText& page) const {

    std::vector<TableCandidate> out;
    const auto build_table = [&](const PageText& source_page, const Rect& source_box, const std::string& id_suffix) -> std::optional<TableCandidate> {
        auto lines = make_lines(source_page);

        auto rows = detect_rows(lines);

        if (rows.size() < 10) return std::nullopt;

        auto nums = numeric_tokens_in_rows(source_page, rows);

        auto cols = cluster_columns(nums, source_page, rows);

        if (cols.size() < 3) return std::nullopt;

        TableCandidate t;
        t.page = source_page.page;

        t.page_box = source_box;

        t.rows = std::move(rows);

        t.columns = std::move(cols);
        assign_cells(t, nums);
        infer_headers(t, doc, source_page);
        if (!id_suffix.empty()) t.table_id += "_" + id_suffix;


        validate_and_materialize(t, doc);
        return t;
    };

    double page_right = page.width;
    for (const auto& tok : page.tokens) page_right = std::max(page_right, tok.box.x1);
    page_right = std::max(page_right, page.width);

    const auto page_lines = make_lines(page);
    const auto row_blocks = detect_row_blocks(page_lines);

    if (row_blocks.size() >= 2) {
        std::vector<TableCandidate> segmented_tables;
        segmented_tables.reserve(row_blocks.size());

        for (size_t bi = 0; bi < row_blocks.size(); ++bi) {
            const auto& block = row_blocks[bi];
            if (block.size() < 10) continue;

            const double previous_bottom = (bi == 0 || row_blocks[bi - 1].empty()) ? 0.0 : row_blocks[bi - 1].back().y1;
            const double next_top = (bi + 1 < row_blocks.size() && !row_blocks[bi + 1].empty()) ? row_blocks[bi + 1].front().y0 : page.height;
            const double header_margin = std::clamp(page.height * 0.16, 72.0, 165.0);
            const double seg_y0 = std::max(0.0, std::max(previous_bottom + 2.0, block.front().y0 - header_margin));
            const double seg_y1 = std::min(page.height, std::max(block.back().y1 + 18.0, std::min(next_top - 2.0, block.back().y1 + 42.0)));
            if (seg_y1 <= seg_y0 + 20.0) continue;

            PageText segment = page;
            segment.tokens.clear();
            segment.tokens.reserve(page.tokens.size() / row_blocks.size() + 96);
            for (const auto& tok : page.tokens) {
                const double cy = tok.box.cy();
                if (cy >= seg_y0 && cy <= seg_y1) segment.tokens.push_back(tok);
            }
            if (segment.tokens.empty()) continue;

            auto table = build_table(segment, {0.0, seg_y0, page_right, seg_y1}, "bloque" + std::to_string(static_cast<int>(bi) + 1));
            if (table && (!table->accepted.empty() || !table->quarantine.empty())) {
                segmented_tables.push_back(std::move(*table));
            }
        }

        size_t segmented_accepted = 0;
        for (const auto& t : segmented_tables) segmented_accepted += t.accepted.size();
        if (segmented_tables.size() >= 2 && segmented_accepted > 0) {
            return segmented_tables;
        }
    }

    const Rect full_box{0, 0, page_right, page.height};
    std::optional<TableCandidate> full_table = build_table(page, full_box, "");

    const std::string pdf_name_norm = normalize_key(doc.file_name + " " + path_utf8(doc.pdf_path));
    const bool edomex_pdf =
        pdf_name_norm.find("edomex") != std::string::npos ||
        (pdf_name_norm.find("estado") != std::string::npos && pdf_name_norm.find("mexico") != std::string::npos);
    if (edomex_pdf && doc.bulletin_year > 0 && doc.bulletin_year < 2023 && page_right > 0.0) {
        auto legacy_split_x = [&]() -> double {
            const auto lines = make_lines(page);
            std::vector<double> label_xs;
            label_xs.reserve(48);
            for (const auto& line : lines) {
                if (line.tokens.empty()) continue;
                for (size_t i = 0; i < line.tokens.size(); ++i) {
                    if (is_numeric_token(line.tokens[i].text)) continue;
                    std::string fragment;
                    Rect label_box;
                    bool have_box = false;
                    for (size_t k = i; k < line.tokens.size() && k < i + 4; ++k) {
                        if (is_numeric_token(line.tokens[k].text)) break;
                        if (!fragment.empty()) fragment.push_back(' ');
                        fragment += line.tokens[k].text;
                        if (!have_box) { label_box = line.tokens[k].box; have_box = true; }
                        else label_box.include(line.tokens[k].box);
                        if (config_.match_jurisdiction_line(normalize_key(fragment))) {
                            label_xs.push_back(label_box.x0);
                            i = k;
                            break;
                        }
                    }
                }
            }
            if (label_xs.size() < 16) return page_right * 0.5;
            std::sort(label_xs.begin(), label_xs.end());
            struct XCluster { double sum = 0.0; int count = 0; };
            std::vector<XCluster> clusters;
            for (double x : label_xs) {
                if (clusters.empty() || std::abs(x - clusters.back().sum / std::max(1, clusters.back().count)) > 28.0) {
                    clusters.push_back({x, 1});
                } else {
                    clusters.back().sum += x;
                    clusters.back().count += 1;
                }
            }
            std::vector<std::pair<double,int>> centers;
            centers.reserve(clusters.size());
            for (const auto& c : clusters) {
                if (c.count >= 8) centers.push_back({c.sum / static_cast<double>(c.count), c.count});
            }
            if (centers.size() < 2) return page.width * 0.5;
            std::sort(centers.begin(), centers.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            double left_label = centers.front().first;
            double right_label = 0.0;
            for (size_t i = 1; i < centers.size(); ++i) {
                if (centers[i].first - left_label > 120.0) {
                    right_label = centers[i].first;
                    break;
                }
            }
            if (right_label <= 0.0) return page.width * 0.5;
            const double gap = right_label - left_label;
            const double left_padding = std::clamp(gap * 0.10, 18.0, 55.0);
            return right_label - left_padding;
        };
        const double mid = legacy_split_x();
        const double gutter = std::max(5.0, std::min(12.0, page.width * 0.006));
        auto crop_half = [&](double x0, double x1) {
            PageText half = page;
            half.tokens.clear();
            half.width = std::max(page.width, x1);
            half.tokens.reserve(page.tokens.size() / 2 + 16);
            for (const auto& tok : page.tokens) {
                const double cx = tok.box.cx();
                if (cx >= x0 && cx <= x1) half.tokens.push_back(tok);
            }
            return half;
        };
        const PageText left = crop_half(0.0, mid + gutter);
        const PageText right = crop_half(mid + gutter, page_right);
        std::vector<TableCandidate> split_tables;
        auto left_table = build_table(left, {0.0, 0.0, mid + gutter, page.height}, "edomex_izq");
        auto right_table = build_table(right, {mid + gutter, 0.0, page_right, page.height}, "edomex_der");
        if (left_table) split_tables.push_back(std::move(*left_table));
        if (right_table) split_tables.push_back(std::move(*right_table));
        size_t split_accepted = 0;
        for (const auto& t : split_tables) {
            split_accepted += t.accepted.size();
        }
        const size_t full_accepted = full_table ? full_table->accepted.size() : 0;
        if (split_tables.size() >= 2 && split_accepted >= std::max<size_t>(full_accepted, 1)) {
            return split_tables;
        }
    }
    if (!full_table) return out;
    TableCandidate t = std::move(*full_table);

    out.push_back(std::move(t));

    return out;
}

}

// ===== Nucleos/TemporalBlocks.impl =====
#line 1 "Nucleos/TemporalBlocks.impl"






#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>

#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <tuple>



namespace epi {


namespace {





fs::path temporal_ixiptlah_root();
bool is_atmosphere_fields(const std::map<std::string, std::string>& fields);
bool temporal_env_truthy(const char* name);
int month_from_iso_date(const std::string& date);
bool temporal_unified_weekly_files_enabled();
bool temporal_unified_weekly_ixiptlah_name_parts(const std::string& stem, int* year, int* month, int* day);
fs::path temporal_unified_ixiptlah_file_for_fields(const fs::path& root, const std::map<std::string, std::string>& fields);
fs::path temporal_weekly_ixiptlah_file_in_root(const fs::path& root, int year, int epi_week);
std::string temporal_week_start_token_from_ymd(int year, int month, int day);

int temporal_year_from_date_or_fields(const std::string& date, const std::string& year_hint) {
    // IXIPTLAH V1: las rutas internas de sharding por década no pueden depender
    // de una resolución externa tardía del enlazador. Esta copia interna mantiene
    // la extracción de año junto a las funciones anónimas que deciden el archivo
    // físico, evitando que MSVC emita LNK2019 por una declaración con vinculación
    // interna sin cuerpo cuando el orden de parseo cambia por refactorización.
    if (date.size() >= 4 && std::isdigit(static_cast<unsigned char>(date[0])) &&
        std::isdigit(static_cast<unsigned char>(date[1])) &&
        std::isdigit(static_cast<unsigned char>(date[2])) &&
        std::isdigit(static_cast<unsigned char>(date[3]))) {
        try { return std::stoi(date.substr(0, 4)); } catch (...) {}
    }
    if (!year_hint.empty()) {
        try { return std::stoi(year_hint); } catch (...) {}
    }
    return 0;
}


enum class TemporalIxiptlahCategory {
    Epidemiological,
    Meteorological,
    AtmosphericContaminants,
    RamaNetwork,
    RedmaNetwork,
    RuoaNetwork,
    Demographic,
    Historical,
    Mobility
};

bool temporal_category_shards_enabled() {
    // IXIPTLAH-DATOS V1: la compatibilidad mensual queda eliminada por diseño.
    // Epidemiología se escribe por año natural para que el visor pueda saltar
    // directo al intervalo semanal solicitado; atmósfera/contaminantes conservan
    // décadas porque sus bloques horarios son más densos y ya portan llave
    // temporal completa en el encabezado V1.
    return true;
}

const char* temporal_ixiptlah_category_stem(TemporalIxiptlahCategory category) {
    switch (category) {
        case TemporalIxiptlahCategory::Epidemiological: return "DATOS_EPIDEMIOLOGICOS";
        case TemporalIxiptlahCategory::Meteorological: return "DATOS_REDMA";
        case TemporalIxiptlahCategory::AtmosphericContaminants: return "DATOS_RAMA";
        case TemporalIxiptlahCategory::RamaNetwork: return "DATOS_RAMA";
        case TemporalIxiptlahCategory::RedmaNetwork: return "DATOS_REDMA";
        case TemporalIxiptlahCategory::RuoaNetwork: return "DATOS_RUOA";
        case TemporalIxiptlahCategory::Demographic: return "DATOS_DEMOGRAFICOS";
        case TemporalIxiptlahCategory::Historical: return "DATOS_HISTORICOS";
        case TemporalIxiptlahCategory::Mobility: return "DATOS_DE_TRANSPORTE_Y_MOVILIDAD";
    }
    return "DATOS_EPIDEMIOLOGICOS";
}

const char* temporal_ixiptlah_decade_stem(TemporalIxiptlahCategory category) {
    switch (category) {
        case TemporalIxiptlahCategory::Epidemiological: return "DatosEpidemiologicos";
        case TemporalIxiptlahCategory::Meteorological: return "DatosRedma";
        case TemporalIxiptlahCategory::AtmosphericContaminants: return "DatosRama";
        case TemporalIxiptlahCategory::RamaNetwork: return "DatosRama";
        case TemporalIxiptlahCategory::RedmaNetwork: return "DatosRedma";
        case TemporalIxiptlahCategory::RuoaNetwork: return "DatosRuoa";
        case TemporalIxiptlahCategory::Demographic: return "DatosDemograficos";
        case TemporalIxiptlahCategory::Historical: return "DatosHistoricos";
        case TemporalIxiptlahCategory::Mobility: return "DatosMovilidad";
    }
    return "DatosEpidemiologicos";
}

int temporal_decade_start_for_year(int year) {
    year = std::clamp(year, 0, 9999);
    return (year / 10) * 10;
}

int temporal_shard_start_for_category_year(TemporalIxiptlahCategory category, int year) {
    year = std::clamp(year, 0, 9999);
    if (category == TemporalIxiptlahCategory::Epidemiological) return year;
    return temporal_decade_start_for_year(year);
}

std::string temporal_decade_ixiptlah_stem(TemporalIxiptlahCategory category, int year) {
    std::ostringstream os;
    os << temporal_ixiptlah_decade_stem(category) << '_'
       << std::setw(4) << std::setfill('0') << temporal_shard_start_for_category_year(category, year);
    return os.str();
}

fs::path temporal_decade_ixiptlah_file_in_root(const fs::path& root, TemporalIxiptlahCategory category, int year) {
    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    return ixiptlah_path(canonical_root, temporal_decade_ixiptlah_stem(category, year));
}

bool temporal_decade_ixiptlah_name_parts(const std::string& stem, TemporalIxiptlahCategory* category, int* decade) {
    const std::string lower = lower_ascii(stem);
    const std::pair<const char*, TemporalIxiptlahCategory> prefixes[] = {
        {"datosepidemiologicos_", TemporalIxiptlahCategory::Epidemiological},
        {"datosrama_", TemporalIxiptlahCategory::RamaNetwork},
        {"datosredma_", TemporalIxiptlahCategory::RedmaNetwork},
        {"datosruoa_", TemporalIxiptlahCategory::RuoaNetwork},
        {"datosdemograficos_", TemporalIxiptlahCategory::Demographic},
        {"datoshistoricos_", TemporalIxiptlahCategory::Historical},
        {"datosmovilidad_", TemporalIxiptlahCategory::Mobility}
    };
    for (const auto& entry : prefixes) {
        const std::string prefix = entry.first;
        if (lower.rfind(prefix, 0) != 0 || lower.size() != prefix.size() + 4u) continue;
        int y = 0;
        for (std::size_t i = prefix.size(); i < lower.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(lower[i]))) return false;
            y = y * 10 + (lower[i] - '0');
        }
        if (y < 0 || y > 9999) return false;
        if (entry.second != TemporalIxiptlahCategory::Epidemiological && (y > 9990 || y % 10 != 0)) return false;
        if (category) *category = entry.second;
        if (decade) *decade = y;
        return true;
    }
    return false;
}

fs::path temporal_category_ixiptlah_file_in_root(const fs::path& root, TemporalIxiptlahCategory category) {
    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    return ixiptlah_path(canonical_root, temporal_ixiptlah_category_stem(category));
}

std::mutex& temporal_element_import_reset_mu() {
    static std::mutex mu;
    return mu;
}

std::set<std::string>& temporal_element_import_reset_seen() {
    static std::set<std::string> seen;
    return seen;
}

std::mutex& temporal_epi_dedupe_cache_mu() {
    static std::mutex mu;
    return mu;
}

struct TemporalEpiDedupeHashSet {
    std::vector<std::uint64_t> buckets;
    std::size_t used = 0;

    static std::uint64_t normalize(std::uint64_t h) {
        return h == 0ull ? 0x9e3779b97f4a7c15ull : h;
    }

    static std::size_t bucket_index(std::uint64_t h, std::size_t mask) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ull;
        h ^= h >> 33;
        return static_cast<std::size_t>(h) & mask;
    }

    void reserve(std::size_t wanted) {
        std::size_t cap = 2048u;
        while (cap < wanted * 2u + 1u) cap <<= 1u;
        if (cap <= buckets.size()) return;
        std::vector<std::uint64_t> old = std::move(buckets);
        buckets.assign(cap, 0ull);
        used = 0;
        for (std::uint64_t h : old) if (h != 0ull) (void)insert_known_nonzero(h);
    }

    std::size_t size() const { return used; }

    bool insert_known_nonzero(std::uint64_t h) {
        const std::size_t mask = buckets.size() - 1u;
        std::size_t i = bucket_index(h, mask);
        for (;;) {
            std::uint64_t& slot = buckets[i];
            if (slot == 0ull) { slot = h; ++used; return true; }
            if (slot == h) return false;
            i = (i + 1u) & mask;
        }
    }

    bool insert(std::uint64_t h) {
        h = normalize(h);
        if (buckets.empty() || (used + 1u) * 10u >= buckets.size() * 7u) reserve(std::max<std::size_t>(used + 1u, 2048u));
        return insert_known_nonzero(h);
    }
};

std::unordered_map<std::string, std::shared_ptr<TemporalEpiDedupeHashSet>>& temporal_epi_dedupe_cache() {
    static std::unordered_map<std::string, std::shared_ptr<TemporalEpiDedupeHashSet>> cache;
    return cache;
}

void temporal_clear_epi_dedupe_cache_for_target(const fs::path& target) {
    if (target.empty()) return;
    std::lock_guard<std::mutex> lock(temporal_epi_dedupe_cache_mu());
    temporal_epi_dedupe_cache().erase(path_utf8(target.lexically_normal()));
}

void temporal_prepare_element_ixiptlah_for_fresh_import(const fs::path& target) {
    if (target.empty() || !temporal_category_shards_enabled()) return;
    if (!temporal_env_truthy("TLALPOWA_IXIPTLAH_ALLOW_ELEMENTAL_TRUNCATE")) return;
    const std::string key = path_utf8(target.lexically_normal());
    std::lock_guard<std::mutex> lock(temporal_element_import_reset_mu());
    auto& seen = temporal_element_import_reset_seen();
    if (!seen.insert(key).second) return;
    // La recomposición destructiva de núcleos elementales queda detrás de un
    // interruptor explícito: la ruta normal de reanudación debe preservar los
    // IXIPTLAH ya aceptados y anexar solo registros nuevos. La limpieza real de
    // una corrida desde cero pertenece al controlador de importación, que elimina
    // la familia DATOS_EPIDEMIOLOGICOS__*.ixiptlah antes de abrir el conversor.
    std::error_code ec;
    fs::remove(target, ec);
    ec.clear();
    fs::remove(fs::path(target.wstring() + L".ixsm"), ec);
    temporal_clear_epi_dedupe_cache_for_target(target);
}

std::string temporal_ixiptlah_element_token(const std::string& raw) {
    std::string n = normalize_key(raw);
    std::string out;
    out.reserve(n.size() + 8);
    bool sep = false;
    for (unsigned char ch : n) {
        if (std::isalnum(ch)) {
            if (sep && !out.empty()) out.push_back('_');
            out.push_back(static_cast<char>(std::toupper(ch)));
            sep = false;
        } else {
            sep = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "ELEMENTO";
    if (out.size() > 96) out.resize(96);
    return out;
}

fs::path temporal_element_ixiptlah_file_in_root(const fs::path& root,
                                                TemporalIxiptlahCategory category,
                                                const std::string& element_key) {
    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    // IXIPTLAH-SM elemento-atómico: un único .ixiptlah por cada elemento del
    // catálogo visible. No se crean subcarpetas ni extensiones analíticas nuevas;
    // la separación ocurre en el nombre del núcleo binario y el dato primario
    // conserva su payload nativo dentro de ese mismo archivo.
    return ixiptlah_path(canonical_root,
                         std::string(temporal_ixiptlah_category_stem(category)) + "__" +
                         temporal_ixiptlah_element_token(element_key));
}

bool atmospheric_key_is_meteorological(const std::string& raw_key) {
    std::string k = lower_ascii(trim(raw_key));
    std::replace(k.begin(), k.end(), '_', ' ');
    static const std::set<std::string> primary = {
        "tmp", "temperatura", "temperature", "temperatura ambiente", "air temperature",
        "rh", "hr", "humedad relativa", "relative humidity",
        "pa", "pba", "presion", "presión", "presion atmosferica", "presión atmosférica", "barometric pressure",
        "pp", "precipitacion", "precipitación", "precipitation",
        "wsp", "velocidad del viento", "wdr", "direccion del viento", "dirección del viento", "wgst", "rapidez de rachas", "wdr_gust", "u10", "v10",
        "uv", "indice uv", "índice uv", "uva", "uv a", "uvb", "uv b", "uvc", "uv c",
        "gr", "radiacion global", "radiación global", "global radiation"
    };
    return primary.count(k) > 0;
}



bool atmospheric_key_is_contaminant(const std::string& raw_key) {
    std::string k = lower_ascii(trim(raw_key));
    std::replace(k.begin(), k.end(), '_', ' ');
    static const std::set<std::string> primary = {
        "o3", "ozono", "pm10", "pm25", "pm2.5", "pm2 5", "pmco", "pm10-2.5",
        "co", "monoxido de carbono", "monóxido de carbono",
        "no", "no2", "nox", "so2", "dioxido de azufre", "dióxido de azufre",
        "h2s", "nh3", "hcho", "ch4", "co2", "ben", "tol", "xyl", "btex",
        "bc", "ec", "oc", "tc", "pb", "cd", "as", "ni", "hg", "cr",
        "so4", "no3a", "aod", "uvai", "inorg aer", "ox",
        "pm25 pm10", "pmco pm10", "no2 nox", "no nox", "no no2",
        "hcho no2", "hcho nox", "oc ec", "ec oc", "ec tc", "oc tc",
        "bc pm25", "ec pm25", "oc pm25", "so4 no3a", "tol ben",
        "xyl ben", "btex ben", "o3 no2", "co no2"
    };
    return primary.count(k) > 0;
}



bool temporal_env_truthy(const char* name) {
    const std::string v = getenv_utf8_or_empty(name);
    if (v.empty()) return false;
    const char c = v.front();
    return c == '1' || c == 's' || c == 'S' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
}

std::string temporal_atmospheric_layer_key(const std::string& raw_key) {
    std::string p = lower_ascii(trim(raw_key));
    std::replace(p.begin(), p.end(), '_', ' ');
    if (p.empty()) return {};

    if (p == "altura de capa limite" || p == "altura de capa límite" || p == "planetary boundary layer height" || p == "pblh") return "pblh";
    if (p == "humedad relativa" || p == "relative humidity" || p == "rh" || p == "hr") return "rh";
    if (p == "precipitacion" || p == "precipitación" || p == "precipitation" || p == "pp") return "pp";
    if (p == "presion atmosferica" || p == "presión atmosférica" || p == "atmospheric pressure" || p == "pa" || p == "pba" || p == "barometric pressure") return "pa";
    if (p == "temperatura" || p == "temperature" || p == "temperatura ambiente" || p == "air temperature" || p == "tmp") return "tmp";
    if (p == "temperatura maxima" || p == "temperatura máxima" || p == "tmax") return "tmax";
    if (p == "temperatura minima" || p == "temperatura mínima" || p == "tmin") return "tmin";
    if (p == "velocidad y direccion del viento" || p == "velocidad y dirección del viento" || p == "velocidad del viento" || p == "wsp") return "wsp";
    if (p == "direccion del viento" || p == "dirección del viento" || p == "wdr") return "wdr";
    if (p == "componente meridional del viento" || p == "v1" || p == "v10") return "v10";
    if (p == "componente zonal del viento" || p == "u10") return "u10";
    if (p == "indice uv" || p == "índice uv" || p == "uv") return "uv";
    if (p == "radiacion uv-a" || p == "radiación uv-a" || p == "uva" || p == "uv a") return "uva";
    if (p == "radiacion uv-b" || p == "radiación uv-b" || p == "uvb" || p == "uv b") return "uvb";
    if (p == "radiacion uv-c" || p == "radiación uv-c" || p == "uvc" || p == "uv c") return "uvc";
    if (p == "radiacion global" || p == "radiación global" || p == "global radiation" || p == "gr") return "gr";
    if (p == "potencia radiativa del fuego" || p == "fire radiative power" || p == "frp") return "frp";
    if (p == "temperatura de brillo satelital" || p == "brightness") return "brightness";
    if (p == "fire temperature" || p == "temperatura subpixel de fuego" || p == "fire_temperature") return "fire_temperature";
    if (p == "fire area" || p == "area subpixel de fuego" || p == "área subpixel de fuego" || p == "fire_area") return "fire_area";
    if (p == "cloud fraction" || p == "fraccion nubosa" || p == "fracción nubosa" || p == "cloud_fraction") return "cloud_fraction";
    if (p == "cloud pressure" || p == "presion de nube" || p == "presión de nube" || p == "cloud_pressure") return "cloud_pressure";
    if (p == "cloud top temperature" || p == "temperatura de tope de nube" || p == "cloud_top_temperature") return "cloud_top_temperature";
    if (p == "uvai" || p == "aerosol index" || p == "indice de aerosoles" || p == "índice de aerosoles") return "uvai";

    if (p == "pm2.5" || p == "pm2 5" || p == "particulas pm2.5" || p == "partículas pm2.5" || p == "pm25") return "pm25";
    if (p == "particulas pm10" || p == "partículas pm10" || p == "pm10") return "pm10";
    if (p == "particulas gruesas pm10-2.5" || p == "partículas gruesas pm10-2.5" || p == "pm10-2.5" || p == "pmco") return "pmco";
    if (p == "monoxido de carbono" || p == "monóxido de carbono" || p == "co") return "co";
    if (p == "dioxido de nitrogeno" || p == "dióxido de nitrógeno" || p == "no2") return "no2";
    if (p == "dioxido de azufre" || p == "dióxido de azufre" || p == "so2") return "so2";
    if (p == "ozono" || p == "o3") return "o3";
    if (p == "oxido nitrico" || p == "óxido nítrico" || p == "no") return "no";
    if (p == "oxidos de nitrogeno" || p == "óxidos de nitrógeno" || p == "nox") return "nox";
    if (p == "sulfuro de hidrogeno" || p == "sulfuro de hidrógeno" || p == "h2s") return "h2s";
    if (p == "benzene" || p == "benceno" || p == "ben") return "ben";
    if (p == "toluene" || p == "tolueno" || p == "tol") return "tol";
    if (p == "xylene" || p == "xylenes" || p == "xileno" || p == "xilenos" || p == "xyl") return "xyl";
    if (p == "methane" || p == "metano" || p == "ch4") return "ch4";
    if (p == "carbon dioxide" || p == "dioxido de carbono" || p == "dióxido de carbono" || p == "co2") return "co2";
    if (p == "formaldehyde" || p == "formaldehido" || p == "formaldehído" || p == "hcho") return "hcho";
    if (p == "ammonia" || p == "amoniaco" || p == "amoníaco" || p == "nh3") return "nh3";
    if (p == "sulfate" || p == "sulfato" || p == "sulfato en aerosol" || p == "so4") return "so4";
    if (p == "nitrate" || p == "nitrato" || p == "nitrato en aerosol" || p == "no3a") return "no3a";
    if (p == "lead" || p == "plomo" || p == "plomo particulado" || p == "pb") return "pb";
    if (p == "cadmium" || p == "cadmio" || p == "cadmio particulado" || p == "cd") return "cd";
    if (p == "arsenic" || p == "arsenico" || p == "arsénico" || p == "arsenico particulado" || p == "arsénico particulado" || p == "as") return "as";
    if (p == "nickel" || p == "niquel" || p == "níquel" || p == "niquel particulado" || p == "níquel particulado" || p == "ni") return "ni";
    if (p == "mercury" || p == "mercurio" || p == "hg") return "hg";
    if (p == "chromium" || p == "cromo" || p == "cromo particulado" || p == "cr") return "cr";
    if (p == "black carbon" || p == "carbono negro" || p == "bc") return "bc";
    if (p == "elemental carbon" || p == "carbono elemental" || p == "ec") return "ec";
    if (p == "organic carbon" || p == "carbono organico" || p == "carbono orgánico" || p == "oc") return "oc";
    if (p == "profundidad optica de aerosoles" || p == "profundidad óptica de aerosoles" || p == "aod") return "aod";

    if (p == "punto de rocio" || p == "punto de rocío" || p == "dew point" || p == "dewpoint") return "dewpoint";
    if (p == "deficit de presion de vapor" || p == "déficit de presión de vapor" || p == "vapor pressure deficit" || p == "vpd") return "vpd";
    if (p == "humedad absoluta" || p == "absolute humidity") return "abs_humidity";
    if (p == "presion de vapor" || p == "presión de vapor" || p == "vapor pressure") return "vapor_pressure";
    if (p == "presion de vapor de saturacion" || p == "presión de vapor de saturación" || p == "saturation vapor pressure") return "sat_vapor_pressure";
    if (p == "bulbo humedo" || p == "bulbo húmedo" || p == "wet bulb") return "wet_bulb";
    if (p == "razon de mezcla" || p == "razón de mezcla" || p == "mixing ratio") return "mixing_ratio";
    if (p == "humedad especifica" || p == "humedad específica" || p == "specific humidity") return "specific_humidity";
    if (p == "densidad del aire" || p == "air density") return "air_density";
    if (p == "temperatura potencial" || p == "potential temperature") return "potential_temperature";
    if (p == "temperatura virtual" || p == "virtual temperature") return "virtual_temperature";

    if (p == "carbono total" || p == "total carbon" || p == "tc") return "tc";
    if (p == "aerosol inorganico" || p == "aerosol inorgánico" || p == "inorganic aerosol") return "inorg_aer";
    if (p == "pm2.5/pm10" || p == "pm25 pm10" || p == "pm25 pm10 ratio") return "pm25_pm10";
    if (p == "pm10-2.5/pm10" || p == "pmco pm10") return "pmco_pm10";
    if (p == "no2/nox" || p == "no2 nox") return "no2_nox";
    if (p == "no/nox" || p == "no nox") return "no_nox";
    if (p == "no/no2" || p == "no no2") return "no_no2";
    if (p == "oc/ec" || p == "oc ec") return "oc_ec";
    if (p == "ec/oc" || p == "ec oc") return "ec_oc";
    if (p == "ec/tc" || p == "ec tc") return "ec_tc";
    if (p == "oc/tc" || p == "oc tc") return "oc_tc";
    if (p == "bc/pm2.5" || p == "bc pm25") return "bc_pm25";
    if (p == "ec/pm2.5" || p == "ec pm25") return "ec_pm25";
    if (p == "oc/pm2.5" || p == "oc pm25") return "oc_pm25";
    if (p == "so4/no3" || p == "so4/no3a" || p == "so4 no3a") return "so4_no3a";
    if (p == "btex") return "btex";
    if (p == "tolueno/benceno" || p == "tol ben" || p == "toluene benzene") return "tol_ben";
    if (p == "xilenos/benceno" || p == "xyl ben" || p == "xylenes benzene") return "xyl_ben";
    if (p == "btex/benceno" || p == "btex ben") return "btex_ben";
    if (p == "o3/no2" || p == "o3 no2") return "o3_no2";
    if (p == "hcho/no2" || p == "hcho no2") return "hcho_no2";
    if (p == "co/no2" || p == "co no2") return "co_no2";
    if (p == "ox" || p == "odd oxygen" || p == "o3+no2") return "ox";
    if (p == "hcho/nox" || p == "hcho nox") return "hcho_nox";

    return normalize_key(p);
}

std::string temporal_atmospheric_layer_key_for_fields(const std::map<std::string, std::string>& fields) {
    const auto get = [&](const char* key) -> std::string {
        const auto it = fields.find(key);
        return it == fields.end() ? std::string{} : it->second;
    };
    std::string key = get("pollutant");
    if (key.empty()) key = get("parameter");
    if (key.empty()) key = get("metric");
    return temporal_atmospheric_layer_key(key);
}

TemporalIxiptlahCategory temporal_atmosphere_source_category(const std::string& source_id,
                                                               const std::string& source_file,
                                                               const std::string& source_path,
                                                               const std::string& domain,
                                                               const std::string& pollutant) {
    const std::string ctx = normalize_key(source_id + " " + source_file + " " + source_path + " " + domain);

    // Separación física estricta: RUOA/PEMBU, REDMA y RAMA no se mezclan por
    // nombres de variable. La categoría decide el núcleo de escritura antes de
    // tocar el payload, de modo que el lector salta redes enteras por archivo.
    if (contains_norm(ctx, "ruoa") || contains_norm(ctx, "pembu") ||
        contains_norm(ctx, "observatorios atmosfericos")) {
        return TemporalIxiptlahCategory::RuoaNetwork;
    }
    if (contains_norm(ctx, "redma") || contains_norm(ctx, "redma") ||
        contains_norm(ctx, "meteorolog") || contains_norm(ctx, "meteorologic") ||
        contains_norm(ctx, "clima")) {
        return TemporalIxiptlahCategory::RedmaNetwork;
    }
    if (contains_norm(ctx, "rama") || contains_norm(ctx, "simat") ||
        contains_norm(ctx, "contamin")) {
        return TemporalIxiptlahCategory::RamaNetwork;
    }
    return atmospheric_key_is_meteorological(pollutant) && !atmospheric_key_is_contaminant(pollutant)
        ? TemporalIxiptlahCategory::RedmaNetwork
        : TemporalIxiptlahCategory::RamaNetwork;
}

TemporalIxiptlahCategory temporal_category_for_atmosphere_fields(const std::map<std::string, std::string>& fields) {
    const auto get = [&](const char* key) -> std::string {
        const auto it = fields.find(key);
        return it == fields.end() ? std::string{} : it->second;
    };
    const std::string pollutant = get("pollutant").empty() ? get("parameter") : get("pollutant");
    return temporal_atmosphere_source_category(get("source_id"), get("source_file"), get("source_path"), get("domain"), pollutant);
}

fs::path temporal_category_ixiptlah_file_for_fields(const fs::path& root, const std::map<std::string, std::string>& fields) {
    if (temporal_unified_weekly_files_enabled()) return temporal_unified_ixiptlah_file_for_fields(root, fields);
    const auto find_field = [&](const char* key) -> std::string {
        const auto it = fields.find(key);
        return it == fields.end() ? std::string{} : it->second;
    };
    int year = temporal_year_from_date_or_fields(find_field("date"), find_field("year"));
    if (year <= 0) year = temporal_year_from_date_or_fields(find_field("fecha"), find_field("año"));
    if (year <= 0) year = 0;

    if (is_atmosphere_fields(fields)) {
        return temporal_decade_ixiptlah_file_in_root(root, temporal_category_for_atmosphere_fields(fields), year);
    }
    return temporal_decade_ixiptlah_file_in_root(root, TemporalIxiptlahCategory::Epidemiological, year);
}

std::vector<fs::path> temporal_primary_category_files(const fs::path& root) {
    const fs::path canonical = root.empty() ? temporal_ixiptlah_root() : root;
    std::vector<fs::path> out;
    out.reserve(8);
    for (TemporalIxiptlahCategory c : {TemporalIxiptlahCategory::Epidemiological,
                                      TemporalIxiptlahCategory::RamaNetwork,
                                      TemporalIxiptlahCategory::RedmaNetwork,
                                      TemporalIxiptlahCategory::RuoaNetwork,
                                      TemporalIxiptlahCategory::Demographic,
                                      TemporalIxiptlahCategory::Historical,
                                      TemporalIxiptlahCategory::Mobility}) {
        out.push_back(temporal_category_ixiptlah_file_in_root(canonical, c));
    }
    return out;
}


long long days_from_civil(int y, unsigned m, unsigned d) noexcept {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;

    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;


    return static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
}




std::tuple<int, unsigned, unsigned> civil_from_days(long long z) noexcept {


    z += 719468;


    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;

    int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;

    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);


    return {y, m, d};
}




int iso_weekday_from_days(long long z) noexcept {
    int w = static_cast<int>((z + 3) % 7);


    if (w < 0) w += 7;


    return w + 1;
}




std::string ymd_string(int y, unsigned m, unsigned d) {
    std::ostringstream os;


    os << std::setw(4) << std::setfill('0') << y << '-'


       << std::setw(2) << std::setfill('0') << m << '-'


       << std::setw(2) << std::setfill('0') << d;


    return os.str();
}




int temporal_week_file_days_in_month(int year, int month) {
    if (month < 1 || month > 12) return 0;
    static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = (month == 2) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    return days[month - 1] + (leap ? 1 : 0);
}




std::string ymd_file_token(int y, unsigned m, unsigned d) {
    std::ostringstream os;
    os << std::setw(4) << std::setfill('0') << std::clamp(y, 0, 9999) << '_'
       << std::setw(2) << std::setfill('0') << std::clamp<unsigned>(m, 1u, 12u) << '_'
       << std::setw(2) << std::setfill('0') << std::clamp<unsigned>(d, 1u, 31u);
    return os.str();
}

bool temporal_unified_weekly_files_enabled() {
    // IXIPTLAH v12: ruta única. El nombre YYYY_MM_DD representa el inicio de la
    // semana interna del mes (01/08/15/22/29). RAMA, REDMET, RUOA/PEMBU y
    // epidemiología escriben aquí; el directorio IXIPTLAH interno separa núcleos
    // por tipo, capa y bucket temporal sin duplicar payload ni romper calendario.
    return true;
}

bool temporal_unified_weekly_ixiptlah_name_parts(const std::string& stem, int* year, int* month, int* day) {
    if (stem.size() != 10u || stem[4] != '_' || stem[7] != '_') return false;
    for (std::size_t i = 0; i < stem.size(); ++i) {
        if (i == 4u || i == 7u) continue;
        if (!std::isdigit(static_cast<unsigned char>(stem[i]))) return false;
    }
    const int y = std::stoi(stem.substr(0, 4));
    const int m = std::stoi(stem.substr(5, 2));
    const int d = std::stoi(stem.substr(8, 2));
    if (y < 0 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    const int dim = temporal_week_file_days_in_month(y, m);
    if (dim <= 0 || d > dim) return false;
    // Sólo se aceptan inicios de bloque semanal mensual; así no se llena Datos
    // con 365 archivos de cero registros ni con fechas ambiguas.
    if (!(d == 1 || d == 8 || d == 15 || d == 22 || d == 29)) return false;
    if (year) *year = y;
    if (month) *month = m;
    if (day) *day = d;
    return true;
}

std::string temporal_week_start_token_from_ymd(int year, int month, int day) {
    year = std::clamp(year, 0, 9999);
    month = std::clamp(month, 1, 12);
    const int dim = temporal_week_file_days_in_month(year, month);
    if (dim <= 0) return ymd_file_token(year, static_cast<unsigned>(month), 1u);
    day = std::clamp(day, 1, dim);
    const int block_day = ((day - 1) / 7) * 7 + 1;
    return ymd_file_token(year, static_cast<unsigned>(month), static_cast<unsigned>(block_day));
}

std::string temporal_week_start_token_from_iso_date(const std::string& date, int fallback_year) {
    int y = fallback_year;
    int m = 1;
    int d = 1;
    if (date.size() >= 10u && std::isdigit(static_cast<unsigned char>(date[0])) &&
        std::isdigit(static_cast<unsigned char>(date[1])) && std::isdigit(static_cast<unsigned char>(date[2])) &&
        std::isdigit(static_cast<unsigned char>(date[3]))) {
        try {
            y = std::stoi(date.substr(0, 4));
            m = std::stoi(date.substr(5, 2));
            d = std::stoi(date.substr(8, 2));
        } catch (...) {}
    }
    if (y <= 0) y = 0;
    return temporal_week_start_token_from_ymd(y, m, d);
}

fs::path temporal_weekly_ixiptlah_file_in_root(const fs::path& root, int year, int epi_week) {
    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    year = std::clamp(year, 0, 9999);
    epi_week = std::clamp(epi_week <= 0 ? 1 : epi_week, 1, 53);
    const long long jan4 = days_from_civil(year, 1u, 4u);
    const int jan4_weekday = iso_weekday_from_days(jan4);
    const long long week1_monday = jan4 - static_cast<long long>(jan4_weekday - 1);
    const auto [y, m, d] = civil_from_days(week1_monday + static_cast<long long>(epi_week - 1) * 7ll);
    return ixiptlah_path(canonical_root, temporal_week_start_token_from_ymd(y, static_cast<int>(m), static_cast<int>(d)));
}

fs::path temporal_weekly_ixiptlah_file_for_date(const fs::path& root, const std::string& date, int fallback_year) {
    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    return ixiptlah_path(canonical_root, temporal_week_start_token_from_iso_date(date, fallback_year));
}

fs::path temporal_unified_ixiptlah_file_for_fields(const fs::path& root, const std::map<std::string, std::string>& fields) {
    const auto find_field = [&](const char* key) -> std::string {
        const auto it = fields.find(key);
        return it == fields.end() ? std::string{} : it->second;
    };
    std::string date = find_field("date");
    if (date.empty()) date = find_field("fecha");
    int year = temporal_year_from_date_or_fields(date, find_field("year"));
    if (year <= 0) year = temporal_year_from_date_or_fields(date, find_field("año"));
    if (!date.empty()) return temporal_weekly_ixiptlah_file_for_date(root, date, year);

    std::string week_text = find_field("epi_week");
    if (week_text.empty()) week_text = find_field("week");
    if (week_text.empty()) week_text = find_field("semana");
    int week = 1;
    try { if (!week_text.empty()) week = std::stoi(week_text); } catch (...) { week = 1; }
    if (year <= 0) year = 0;
    return temporal_weekly_ixiptlah_file_in_root(root, year, week);
}

bool temporal_ixiptlah_file_date_key(const fs::path& p, int& y, int& m, int& d) {
    return temporal_unified_weekly_ixiptlah_name_parts(path_utf8(p.stem()), &y, &m, &d);
}

std::string normalized_extension(const fs::path& p) {


    return lower_ascii(path_utf8(p.extension()));
}




bool lustrum_name_parts(const std::string& stem, int& start, int& end) {


    if (stem.size() != 9 || stem[4] != '-') return false;


    for (size_t i = 0; i < stem.size(); ++i) {


        if (i == 4) continue;


        if (!std::isdigit(static_cast<unsigned char>(stem[i]))) return false;
    }


    try {
        start = std::stoi(stem.substr(0, 4));
        end = std::stoi(stem.substr(5, 4));

    } catch (...) { return false; }


    return end == start + 4 && start % 5 == 0;
}




std::string two_digit_month_token(int month) {


    month = std::clamp(month, 0, 12);
    std::ostringstream os;


    os << std::setw(2) << std::setfill('0') << month;


    return os.str();
}




int month_from_legacy_or_numeric_token(std::string value) {
    value = lower_ascii(trim(value));


    if (value.size() == 2 && std::isdigit(static_cast<unsigned char>(value[0])) && std::isdigit(static_cast<unsigned char>(value[1]))) {
        try {
            const int m = std::stoi(value);


            return (m >= 0 && m <= 12) ? m : -1;
        } catch (...) { return -1; }
    }


    static const std::map<std::string, int> months = {
        {"unk", 0}, {"ene", 1}, {"feb", 2}, {"mar", 3}, {"abr", 4}, {"may", 5},


        {"jun", 6}, {"jul", 7}, {"ago", 8}, {"sep", 9}, {"oct", 10}, {"nov", 11}, {"dic", 12}
    };


    const auto it = months.find(value);


    return it == months.end() ? -1 : it->second;
}




bool monthly_ixiptlah_name_parts(const std::string& stem, int& year, int& month) {


    if (stem.size() != 7 && stem.size() != 8) return false;


    if (stem.size() != 7 && stem[4] != '_') return false;


    if (stem.size() == 7 && stem[4] != '_') return false;


    for (size_t i = 0; i < 4; ++i) {


        if (!std::isdigit(static_cast<unsigned char>(stem[i]))) return false;
    }
    try {


        year = std::stoi(stem.substr(0, 4));
    } catch (...) { return false; }


    month = month_from_legacy_or_numeric_token(stem.substr(5));


    return year >= 0 && year <= 9999 && month >= 0 && month <= 12;
}




fs::path temporal_ixiptlah_root() {


    return internal_data_root();
}




bool temporal_unified_monthly_ixiptlah_enabled() {


    const std::string legacy = getenv_utf8_or_empty("TLALPOWA_IXIPTLAH_LEGACY_SUBROOTS");
    const char c = legacy.empty() ? '\0' : legacy.front();


    return !(c == '1' || c == 's' || c == 'S' || c == 't' || c == 'T' || c == 'y' || c == 'Y');
}




fs::path temporal_monthly_ixiptlah_file_in_root(const fs::path& root, int year, int month) {




    year = std::clamp(year, 0, 9999);


    month = std::clamp(month, 0, 12);
    std::ostringstream os;


    os << std::setw(4) << std::setfill('0') << year << '_' << two_digit_month_token(month);


    const fs::path canonical_root = temporal_unified_monthly_ixiptlah_enabled() ? temporal_ixiptlah_root() : (root.empty() ? temporal_ixiptlah_root() : root);


    return ixiptlah_path(canonical_root, os.str());
}




fs::path temporal_monthly_ixiptlah_file_exact_root(const fs::path& root, int year, int month) {


    year = std::clamp(year, 0, 9999);


    month = std::clamp(month, 0, 12);
    std::ostringstream os;


    os << std::setw(4) << std::setfill('0') << year << '_' << two_digit_month_token(month);


    return ixiptlah_path(root.empty() ? temporal_ixiptlah_root() : root, os.str());
}




fs::path temporal_monthly_ixiptlah_file(int year, int month) {


    return temporal_monthly_ixiptlah_file_in_root(temporal_ixiptlah_root(), year, month);
}




int month_from_iso_date(const std::string& date) {


    if (date.size() >= 7 &&


        std::isdigit(static_cast<unsigned char>(date[0])) &&


        std::isdigit(static_cast<unsigned char>(date[1])) &&


        std::isdigit(static_cast<unsigned char>(date[2])) &&


        std::isdigit(static_cast<unsigned char>(date[3])) &&


        date[4] == '-' &&


        std::isdigit(static_cast<unsigned char>(date[5])) &&


        std::isdigit(static_cast<unsigned char>(date[6]))) {


        try { return std::clamp(std::stoi(date.substr(5, 2)), 1, 12); } catch (...) {}
    }


    return 0;
}




std::pair<int, int> year_month_from_epi_week(int year, int week) {


    if (year <= 0) return {0, 0};


    week = std::clamp(week <= 0 ? 1 : week, 1, 53);


    const long long jan4 = days_from_civil(year, 1, 4);


    const long long week1_monday = jan4 - (iso_weekday_from_days(jan4) - 1);


    const long long target = week1_monday + static_cast<long long>(week - 1) * 7;


    auto [y, m, _] = civil_from_days(target);


    return {std::clamp(y, 0, 9999), static_cast<int>(m)};
}




fs::path temporal_monthly_ixiptlah_file_for_fields(const fs::path& root, const std::map<std::string, std::string>& fields) {


    auto get = [&](const char* key) -> std::string {


        const auto it = fields.find(key);


        return it == fields.end() ? std::string{} : it->second;
    };


    int year = temporal_year_from_date_or_fields(get("date"), get("year"));


    if (year <= 0) year = temporal_year_from_date_or_fields(get("fecha"), get("año"));


    int month = month_from_iso_date(get("date"));


    if (month <= 0) month = month_from_iso_date(get("fecha"));


    if (month <= 0) month = 1;


    return temporal_monthly_ixiptlah_file_in_root(root, year, month);
}




bool annual_entity_name_parts(const std::string& stem, std::string& entity, int& year) {


    if (stem.size() != 8 || stem[3] != ' ') return false;


    for (size_t i = 0; i < 3; ++i) {


        if (!std::isalpha(static_cast<unsigned char>(stem[i]))) return false;
    }


    for (size_t i = 4; i < 8; ++i) {


        if (!std::isdigit(static_cast<unsigned char>(stem[i]))) return false;
    }
    entity = stem.substr(0, 3);


    for (char& c : entity) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));


    try { year = std::stoi(stem.substr(4, 4)); } catch (...) { return false; }


    return year >= 1800 && year <= 2300;
}




bool is_atmosphere_fields(const std::map<std::string, std::string>& fields) {


    const auto it = fields.find("record_type");


    return it != fields.end() && it->second.rfind("atmosfera_", 0) == 0;
}




bool is_source_inventory_fields(const std::map<std::string, std::string>& fields) {


    const auto it = fields.find("record_type");


    if (it == fields.end()) return false;


    const std::string t = normalize_key(it->second);


    return t == "catalogo_fuente_datos" ||
           t == "fuente_datos" ||
           t == "inventario_fuente_datos" ||

           t == "atmosfera_fuente" ||
           t == "atmosfera_satelital_fuente";
}




std::string normalized_entity_code(std::string value) {
    value = trim(value);


    for (char& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));


    if (value.size() == 3 &&
        std::isalpha(static_cast<unsigned char>(value[0])) &&


        std::isalpha(static_cast<unsigned char>(value[1])) &&


        std::isalpha(static_cast<unsigned char>(value[2]))) {


        return value;
    }


    return "CMX";
}




std::string entity_code_from_fields(const std::map<std::string, std::string>& fields) {


    const auto it = fields.find("siglas_entidad");


    return normalized_entity_code(it == fields.end() ? std::string{} : it->second);
}



std::string normalized_three_code(std::string value, const std::string& fallback = "ATM") {


    value = trim(value);
    std::string out;


    out.reserve(3);


    for (char c : value) {
        const unsigned char uc = static_cast<unsigned char>(c);


        if (std::isalnum(uc)) out.push_back(static_cast<char>(std::toupper(uc)));


        if (out.size() == 3) break;
    }


    if (out.size() == 3) return out;
    std::string fb = fallback;


    for (char& c : fb) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));


    while (out.size() < 3 && out.size() < fb.size()) out.push_back(fb[out.size()]);


    while (out.size() < 3) out.push_back('X');


    return out;
}




bool territorial_annual_name_parts(const std::string& stem, std::string& entity, std::string& territory, int& year) {


    if (stem.size() != 12 || stem[3] != '-' || stem[7] != ' ') return false;


    for (size_t i : {size_t(0), size_t(1), size_t(2), size_t(4), size_t(5), size_t(6)}) {


        if (!std::isalpha(static_cast<unsigned char>(stem[i]))) return false;
    }


    for (size_t i = 8; i < 12; ++i) if (!std::isdigit(static_cast<unsigned char>(stem[i]))) return false;
    entity = stem.substr(0, 3);


    territory = stem.substr(4, 3);


    for (char& c : entity) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));


    for (char& c : territory) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));


    try { year = std::stoi(stem.substr(8, 4)); } catch (...) { return false; }


    return year >= 1800 && year <= 2300;
}




std::string atmosphere_label_from_fields(const std::map<std::string, std::string>& fields, int year) {


    const auto get = [&](const char* k) -> std::string {


        const auto it = fields.find(k);


        return it == fields.end() ? std::string{} : it->second;
    };


    const std::string territory = !get("siglas_municipio").empty() ? get("siglas_municipio") : get("siglas_alcaldia");


    if (!territory.empty()) {


        return normalized_three_code(get("siglas_entidad"), "CMX") + "-" + normalized_three_code(territory, "MUN") + " " + std::to_string(year);
    }


    return normalized_three_code(get("station_id"), "ATM") + " " + std::to_string(year);
}




std::string csv_header_for_columns(const std::vector<std::string>& cols) {
    std::ostringstream os;


    for (size_t i = 0; i < cols.size(); ++i) {


        if (i) os << ',';


        os << csv_escape(cols[i]);
    }


    return os.str();
}




std::string tsv_header_for_columns(const std::vector<std::string>& cols) {
    std::ostringstream os;


    for (size_t i = 0; i < cols.size(); ++i) {


        if (i) os << '\t';
        os << cols[i];
    }


    return os.str();
}




std::string double_compact(double v) {


    if (!std::isfinite(v)) return {};
    std::ostringstream os;


    os.setf(std::ios::fmtflags(0), std::ios::floatfield);


    os << std::setprecision(12) << v;


    return os.str();
}




std::string header_value(const std::vector<std::string>& header, const std::vector<std::string>& cols, const std::string& key) {


    for (size_t i = 0; i < header.size() && i < cols.size(); ++i) {


        if (header[i] == key) return cols[i];
    }


    return {};
}




struct JsonHourSummary {


    size_t records = 0;


    std::map<std::string, size_t> record_types;


    std::map<std::string, size_t> domains;
};





struct TemporalAppendSink {


    fs::path path;


    std::ofstream stream;
    uint64_t last_used = 0;
};




std::mutex& temporal_append_mu() {


    static std::mutex mu;


    return mu;
}




std::unordered_map<std::string, std::unique_ptr<TemporalAppendSink>>& temporal_append_sinks() {


    static std::unordered_map<std::string, std::unique_ptr<TemporalAppendSink>> sinks;


    return sinks;
}




uint64_t& temporal_append_tick() {
    static uint64_t tick = 0;


    return tick;
}




std::chrono::steady_clock::time_point& temporal_last_flush_time() {


    static auto t = std::chrono::steady_clock::now() - std::chrono::seconds(5);


    return t;
}




bool write_epidemiology_payload(std::ostream& out, const TemporalEpidemiologyRecord& r) {


    return ixiptlah_write_string(out, normalized_entity_code(r.entity)) &&


           ixiptlah_write_value(out, r.year) &&


           ixiptlah_write_value(out, r.epi_week) &&


           ixiptlah_write_value(out, r.page) &&


           ixiptlah_write_string(out, r.disease) &&


           ixiptlah_write_string(out, r.cie10) &&


           ixiptlah_write_string(out, r.jurisdiction) &&


           ixiptlah_write_string(out, r.period) &&


           ixiptlah_write_string(out, r.sex) &&


           ixiptlah_write_value(out, r.value);
}




bool read_epidemiology_payload(std::istream& in, TemporalEpidemiologyRecord& r) {


    return ixiptlah_read_string(in, r.entity) &&


           ixiptlah_read_value(in, r.year) &&


           ixiptlah_read_value(in, r.epi_week) &&


           ixiptlah_read_value(in, r.page) &&


           ixiptlah_read_string(in, r.disease) &&


           ixiptlah_read_string(in, r.cie10) &&


           ixiptlah_read_string(in, r.jurisdiction) &&


           ixiptlah_read_string(in, r.period) &&


           ixiptlah_read_string(in, r.sex) &&


           ixiptlah_read_value(in, r.value);
}


std::string temporal_epidemiology_element_key(const std::string& disease, const std::string& cie10) {
    std::string d = trim(disease);
    std::string c = trim(cie10);
    if (!c.empty()) return d.empty() ? c : (d + "__" + c);
    return d.empty() ? std::string("ENFERMEDAD") : d;
}

std::uint8_t temporal_epi_week_u8(int week) {
    return static_cast<std::uint8_t>(std::clamp(week, 0, 255));
}

std::uint64_t temporal_epi_time_key(int year, int week) {
    const std::uint64_t y = static_cast<std::uint64_t>(std::clamp(year, 0, 9999));
    const std::uint64_t w = static_cast<std::uint64_t>(std::clamp(week, 0, 53));
    // YYYYWW0000: compatible con orden lexicográfico numérico descendente y sin
    // pagar conversión ISO completa dentro del encabezado de registro.
    return y * 1000000ull + w * 10000ull;
}

bool temporal_epi_record_valid_for_ixiptlah(const TemporalEpidemiologyRecord& r) {
    // Contrato de núcleo EPI: la temporalidad es semanal y toda fila debe poder
    // dirigirse por YYYYWW0000 sin heurísticas posteriores. Los campos textuales
    // mínimos aseguran que la representación preformada conserve enfermedad y
    // jurisdicción sin inventar capas al graficar.
    if (r.year < 1800 || r.year > 2300) return false;
    if (r.epi_week < 1 || r.epi_week > 53) return false;
    if (trim(r.disease).empty() && trim(r.cie10).empty()) return false;
    if (trim(r.jurisdiction).empty()) return false;
    if (r.period.empty()) return false;
    return true;
}

std::uint64_t temporal_atmosphere_time_key(int year, int month, std::uint8_t day, std::uint8_t hour, std::uint8_t minute) {
    const std::uint64_t y = static_cast<std::uint64_t>(std::clamp(year, 0, 9999));
    const std::uint64_t mo = static_cast<std::uint64_t>(std::clamp(month, 0, 12));
    const std::uint64_t d = static_cast<std::uint64_t>(std::clamp<int>(day, 0, 31));
    const std::uint64_t h = static_cast<std::uint64_t>(std::clamp<int>(hour, 0, 23));
    const std::uint64_t mi = static_cast<std::uint64_t>(std::clamp<int>(minute, 0, 59));
    return (((y * 100ull + mo) * 100ull + d) * 100ull + h) * 100ull + mi;
}

std::uint16_t temporal_epi_page_u16(int page) {
    return static_cast<std::uint16_t>(std::clamp(page, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
}

std::int32_t temporal_epi_value_i32(int64_t value) {
    if (value < static_cast<int64_t>(std::numeric_limits<std::int32_t>::min())) return std::numeric_limits<std::int32_t>::min();
    if (value > static_cast<int64_t>(std::numeric_limits<std::int32_t>::max())) return std::numeric_limits<std::int32_t>::max();
    return static_cast<std::int32_t>(value);
}

#pragma pack(push, 1)
struct TemporalEpiV1GeneralRow {
    std::uint16_t entity = 0;
    std::uint8_t week = 0;
    std::uint16_t page = 0;
    std::uint16_t jurisdiction = 0;
    std::uint16_t period = 0;
    std::uint8_t sex = 0;
    std::int32_t value = 0;
};
#pragma pack(pop)
static_assert(sizeof(TemporalEpiV1GeneralRow) == 14, "IXIPTLAH-EPI1 debe permanecer canónicamente compacto");

#pragma pack(push, 1)
struct TemporalEpiV1DistributionRow {
    std::uint16_t entity = 0;
    std::uint8_t week = 0;
    std::uint16_t page = 0;
    std::uint16_t jurisdiction = 0;
    std::uint8_t distribution = 0;
    std::int32_t value = 0;
};
#pragma pack(pop)
static_assert(sizeof(TemporalEpiV1DistributionRow) == 12, "IXIPTLAH-EPI1D debe conservar filas epidemiologicas de 12 bytes");

bool ixiptlah_write_string_table(std::ostream& out, const std::vector<std::string>& values);
bool ixiptlah_read_string_table(std::istream& in, std::vector<std::string>& values, std::uint16_t max_count);
std::uint16_t temporal_epi_table_index(std::vector<std::string>& table,
                                       std::unordered_map<std::string, std::uint16_t>& index,
                                       const std::string& value);

bool temporal_epi_norm_has(const std::string& h, const char* n) {
    return h.find(n) != std::string::npos;
}

bool temporal_epi_norm_is_female(const std::string& s) {
    return s == "f" || s == "fem" || s == "femenino" || s == "mujer" || s == "mujeres" ||
           temporal_epi_norm_has(s, "femen") || temporal_epi_norm_has(s, "female");
}

bool temporal_epi_norm_is_male(const std::string& s) {
    return s == "m" || s == "masc" || s == "masculino" || s == "hombre" || s == "hombres" ||
           temporal_epi_norm_has(s, "mascul") || temporal_epi_norm_has(s, "male");
}

bool temporal_epi_norm_is_total_sex(const std::string& s) {
    return s.empty() || s == "t" || s == "total" || s == "ambos" || s == "ambos_sexos" ||
           s == "todos" || s == "todas" || temporal_epi_norm_has(s, "total");
}

std::uint8_t temporal_epi_distribution_code(const std::string& period, const std::string& sex) {
    const std::string p = normalize_key(period);
    const std::string s = normalize_key(sex);

    // Codec epidemiológico canónico: la vista y el almacenamiento convergen en
    // sólo tres distribuciones humanas para boletines CDMX: Sem => total,
    // Acum/F => acum_F, Acum/M => acum_M. Se retienen códigos heredados para
    // SemDerivada porque pueden existir IXIPTLAH viejos; la escritura nueva del
    // flujo normal cae en 1, 5 o 6 y evita duplicar Periodo/Sexo por fila.
    const bool looks_weekly = p == "sem" || p == "semana" || p == "semanal" ||
                              temporal_epi_norm_has(p, "sem") || temporal_epi_norm_has(p, "weekly");
    const bool looks_accum = p == "acum" || p == "acumulado" || p == "acumulada" ||
                             temporal_epi_norm_has(p, "acum") || temporal_epi_norm_has(p, "accum");
    const bool looks_derived = p == "semderivada" || p == "sem_derivada" || temporal_epi_norm_has(p, "deriv");

    if (looks_derived) {
        if (temporal_epi_norm_is_female(s)) return 2u;
        if (temporal_epi_norm_is_male(s)) return 3u;
        return 4u;
    }
    if (looks_weekly) return 1u;
    if (looks_accum) {
        if (temporal_epi_norm_is_female(s)) return 5u;
        if (temporal_epi_norm_is_male(s)) return 6u;
        if (temporal_epi_norm_is_total_sex(s)) return 7u;
    }
    return 0u;
}

void temporal_epi_distribution_decode(std::uint8_t code, std::string& period, std::string& sex) {
    switch (code) {
        case 1u: period = "Sem"; sex = "total"; break;
        case 2u: period = "SemDerivada"; sex = "F"; break;
        case 3u: period = "SemDerivada"; sex = "M"; break;
        case 4u: period = "SemDerivada"; sex = "total"; break;
        case 5u: period = "Acum"; sex = "F"; break;
        case 6u: period = "Acum"; sex = "M"; break;
        case 7u: period = "Acum"; sex = "total"; break;
        default: period = ""; sex = "ND"; break;
    }
}

bool temporal_epi_write_packed_rows_v1_distribution(std::ostream& out, const std::vector<TemporalEpiV1DistributionRow>& rows) {
    if (rows.empty()) return true;
    const std::uint64_t bytes = static_cast<std::uint64_t>(rows.size()) * sizeof(TemporalEpiV1DistributionRow);
    if (bytes > 512ull * 1024ull * 1024ull) return false;
    out.write(reinterpret_cast<const char*>(rows.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(out);
}

bool temporal_epi_read_packed_rows_v1_distribution(std::istream& in, std::vector<TemporalEpiV1DistributionRow>& rows, std::uint32_t count) {
    if (count > 10000000u) return false;
    rows.assign(count, {});
    if (count == 0) return true;
    const std::uint64_t bytes = static_cast<std::uint64_t>(count) * sizeof(TemporalEpiV1DistributionRow);
    if (bytes > 512ull * 1024ull * 1024ull) return false;
    in.read(reinterpret_cast<char*>(rows.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(in);
}

std::uint64_t temporal_epi_v1_distribution_row_hash(const TemporalEpiV1DistributionRow& r) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= static_cast<unsigned char>((v >> (i * 8)) & 0xffu);
            h *= 1099511628211ull;
        }
    };
    mix(r.entity); mix(r.week); mix(r.page); mix(r.jurisdiction); mix(r.distribution);
    mix(static_cast<std::uint32_t>(r.value));
    return h ? h : 1ull;
}

bool temporal_epi_v1_distribution_row_less(const TemporalEpiV1DistributionRow& a, const TemporalEpiV1DistributionRow& b) {
    if (a.entity != b.entity) return a.entity < b.entity;
    if (a.week != b.week) return a.week < b.week;
    if (a.page != b.page) return a.page < b.page;
    if (a.jurisdiction != b.jurisdiction) return a.jurisdiction < b.jurisdiction;
    if (a.distribution != b.distribution) return a.distribution < b.distribution;
    return a.value < b.value;
}

bool temporal_epi_v1_distribution_decode_row(const TemporalEpiV1DistributionRow& row,
                              const std::vector<std::string>& entities,
                              const std::vector<std::string>& jurisdictions,
                              const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record,
                              const std::string& disease,
                              const std::string& cie10,
                              int year) {
    if (row.entity >= entities.size() || row.jurisdiction >= jurisdictions.size()) return true;
    TemporalEpidemiologyRecord r;
    r.entity = entities[row.entity];
    r.year = year;
    r.epi_week = static_cast<int>(row.week);
    r.page = static_cast<int>(row.page);
    r.disease = disease;
    r.cie10 = cie10;
    r.jurisdiction = jurisdictions[row.jurisdiction];
    temporal_epi_distribution_decode(row.distribution, r.period, r.sex);
    if (r.period.empty()) return true;
    r.value = static_cast<int64_t>(row.value);
    return on_record(r);
}

bool temporal_epi_can_use_distribution_codec(const std::vector<TemporalEpidemiologyRecord>& rows) {
    for (const TemporalEpidemiologyRecord& r : rows) if (temporal_epi_distribution_code(r.period, r.sex) == 0u) return false;
    return !rows.empty();
}

bool temporal_epi_write_distribution_payload_v1(std::ostream& out,
                                                const std::vector<TemporalEpidemiologyRecord>& input_rows,
                                                const std::string& disease,
                                                const std::string& cie10,
                                                int year) {
    std::vector<std::string> entities, jurisdictions;
    std::unordered_map<std::string, std::uint16_t> entity_index, jurisdiction_index;
    entities.reserve(4);
    jurisdictions.reserve(32);
    entity_index.reserve(8);
    jurisdiction_index.reserve(64);

    std::vector<TemporalEpiV1DistributionRow> rows;
    rows.reserve(input_rows.size());
    std::unordered_set<std::uint64_t> row_seen;
    row_seen.reserve(input_rows.size() * 2u + 257u);
    for (const TemporalEpidemiologyRecord& r0 : input_rows) {
        if (r0.year != year || r0.disease != disease || r0.cie10 != cie10) return false;
        if (r0.epi_week < 1 || r0.epi_week > 53 || r0.value == 0) continue;
        const std::uint8_t dist = temporal_epi_distribution_code(r0.period, r0.sex);
        if (dist == 0u) return false;
        TemporalEpiV1DistributionRow r;
        r.entity = temporal_epi_table_index(entities, entity_index, normalized_entity_code(r0.entity));
        r.week = temporal_epi_week_u8(r0.epi_week);
        r.page = temporal_epi_page_u16(r0.page);
        r.jurisdiction = temporal_epi_table_index(jurisdictions, jurisdiction_index, r0.jurisdiction);
        r.distribution = dist;
        r.value = temporal_epi_value_i32(r0.value);
        const std::uint64_t hk = temporal_epi_v1_distribution_row_hash(r);
        if (row_seen.insert(hk).second) rows.push_back(r);
    }
    if (rows.empty() || rows.size() > 10000000ull) return false;
    std::sort(rows.begin(), rows.end(), temporal_epi_v1_distribution_row_less);

    // IXIPTLAH-EPI1D compacta exactamente el patrón de boletines: enfermedad y
    // CIE-10 se escriben una sola vez por núcleo/año; cada fila sólo porta entidad,
    // semana, página, alcaldía indexada, distribución canónica y valor. Así el
    // contrato visual total/acum_F/acum_M no fuerza duplicaciones en disco ni en
    // lectura. El codec vive dentro del dominio epidemiológico; los bloques
    // atmosféricos siguen usando sus propios payloads para no acoplar semánticas.
    if (!ixiptlah_write_string(out, "EPI1D") ||
        !ixiptlah_write_string(out, disease) ||
        !ixiptlah_write_string(out, cie10) ||
        !ixiptlah_write_value(out, year) ||
        !ixiptlah_write_string_table(out, entities) ||
        !ixiptlah_write_string_table(out, jurisdictions)) return false;

    const std::uint32_t count = static_cast<std::uint32_t>(rows.size());
    if (!ixiptlah_write_value(out, count)) return false;
    return temporal_epi_write_packed_rows_v1_distribution(out, rows);
}

bool temporal_epi_read_distribution_payload_v1(std::istream& in,
                                               const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record,
                                               const std::string& disease,
                                               const std::string& cie10,
                                               int year) {
    std::vector<std::string> entities, jurisdictions;
    if (!ixiptlah_read_string_table(in, entities, 4096) || !ixiptlah_read_string_table(in, jurisdictions, 4096)) return false;
    std::uint32_t count = 0;
    if (!ixiptlah_read_value(in, count) || count > 10000000u) return false;

    // Lectura EPI1D en ventanas POD: el importador y el visor no necesitan
    // reservar todo el lote para reconstruir TemporalEpidemiologyRecord. Esto
    // reduce picos de memoria cuando una enfermedad acumula muchos años/semanas
    // y mantiene el mismo contrato de callback/early-stop del lector histórico.
    constexpr std::uint32_t kChunkRows = 8192u;
    std::array<TemporalEpiV1DistributionRow, kChunkRows> chunk{};
    std::uint32_t remaining = count;
    while (remaining > 0) {
        const std::uint32_t n = std::min<std::uint32_t>(remaining, kChunkRows);
        const std::uint64_t bytes = static_cast<std::uint64_t>(n) * sizeof(TemporalEpiV1DistributionRow);
        if (bytes > 512ull * 1024ull * 1024ull) return false;
        in.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(bytes));
        if (!in) return false;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (!temporal_epi_v1_distribution_decode_row(chunk[i], entities, jurisdictions, on_record, disease, cie10, year)) return false;
        }
        remaining -= n;
    }
    return true;
}

bool temporal_epi_write_packed_rows(std::ostream& out, const std::vector<TemporalEpiV1GeneralRow>& rows) {
    if (rows.empty()) return true;
    const std::uint64_t bytes = static_cast<std::uint64_t>(rows.size()) * sizeof(TemporalEpiV1GeneralRow);
    if (bytes > 512ull * 1024ull * 1024ull) return false;
    out.write(reinterpret_cast<const char*>(rows.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(out);
}

bool temporal_epi_read_packed_rows(std::istream& in, std::vector<TemporalEpiV1GeneralRow>& rows, std::uint32_t count) {
    if (count > 10000000u) return false;
    rows.assign(count, {});
    if (count == 0) return true;
    const std::uint64_t bytes = static_cast<std::uint64_t>(count) * sizeof(TemporalEpiV1GeneralRow);
    if (bytes > 512ull * 1024ull * 1024ull) return false;
    in.read(reinterpret_cast<char*>(rows.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(in);
}

std::uint8_t temporal_epi_sex_code(const std::string& sex) {
    const std::string k = normalize_key(sex);
    if (k == "f" || k == "femenino" || k == "mujer" || k == "mujeres") return 1;
    if (k == "m" || k == "masculino" || k == "hombre" || k == "hombres") return 2;
    if (k == "t" || k == "total" || k == "ambos" || k == "ambos sexos") return 3;
    return 0;
}

std::string temporal_epi_sex_text(std::uint8_t code) {
    switch (code) {
        case 1: return "F";
        case 2: return "M";
        case 3: return "Total";
        default: return "ND";
    }
}

bool ixiptlah_write_string_table(std::ostream& out, const std::vector<std::string>& values) {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) return false;
    const std::uint16_t n = static_cast<std::uint16_t>(values.size());
    if (!ixiptlah_write_value(out, n)) return false;
    for (const std::string& v : values) if (!ixiptlah_write_string(out, v)) return false;
    return true;
}

bool ixiptlah_read_string_table(std::istream& in, std::vector<std::string>& values, std::uint16_t max_count = 4096) {
    std::uint16_t n = 0;
    if (!ixiptlah_read_value(in, n) || n > max_count) return false;
    values.assign(n, {});
    for (std::string& v : values) if (!ixiptlah_read_string(in, v)) return false;
    return true;
}

std::uint16_t temporal_epi_table_index(std::vector<std::string>& table, std::unordered_map<std::string, std::uint16_t>& index, const std::string& value) {
    const std::string key = trim(value);
    auto it = index.find(key);
    if (it != index.end()) return it->second;
    if (table.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) return 0;
    const std::uint16_t id = static_cast<std::uint16_t>(table.size());
    table.push_back(key);
    index.emplace(key, id);
    return id;
}

bool write_epidemiology_payload_v1(std::ostream& out, const std::vector<TemporalEpidemiologyRecord>& input_rows) {
    if (input_rows.empty()) return false;

    const std::string disease = input_rows.front().disease;
    const std::string cie10 = input_rows.front().cie10;
    const int year = input_rows.front().year;
    if (year < 1800 || year > 2300) return false;

    std::vector<std::string> entities, jurisdictions, periods;
    std::unordered_map<std::string, std::uint16_t> entity_index, jurisdiction_index, period_index;
    entities.reserve(4);
    jurisdictions.reserve(32);
    periods.reserve(4);
    entity_index.reserve(8);
    jurisdiction_index.reserve(64);
    period_index.reserve(8);

    if (temporal_epi_can_use_distribution_codec(input_rows)) {
        return temporal_epi_write_distribution_payload_v1(out, input_rows, disease, cie10, year);
    }

    std::vector<TemporalEpiV1GeneralRow> rows;
    rows.reserve(input_rows.size());
    for (const TemporalEpidemiologyRecord& r0 : input_rows) {
        // IXIPTLAH-EPI1 no es tolerante: el registro primario es un bloque elemental
        // enfermedad/CIE/año. Mezclar año o enfermedad obligaría a guardar cadenas por
        // fila y destruiría la velocidad de lectura. La llamada superior agrupa antes.
        if (r0.year != year || r0.disease != disease || r0.cie10 != cie10) return false;
        if (r0.epi_week < 1 || r0.epi_week > 53 || r0.value == 0) continue;
        TemporalEpiV1GeneralRow r;
        r.entity = temporal_epi_table_index(entities, entity_index, normalized_entity_code(r0.entity));
        r.week = temporal_epi_week_u8(r0.epi_week);
        r.page = temporal_epi_page_u16(r0.page);
        r.jurisdiction = temporal_epi_table_index(jurisdictions, jurisdiction_index, r0.jurisdiction);
        r.period = temporal_epi_table_index(periods, period_index, r0.period);
        r.sex = temporal_epi_sex_code(r0.sex);
        r.value = temporal_epi_value_i32(r0.value);
        rows.push_back(r);
    }
    if (rows.empty() || rows.size() > 10000000ull) return false;

    // IXIPTLAH-EPI1C: bloque primario columnar-equivalente de lectura directa. Las
    // cadenas de cabecera/diccionarios se internan una vez y las filas se vuelcan como
    // POD empaquetado de 14 bytes. No hay JSON/TSV ni objetos por muestra en disco.
    if (!ixiptlah_write_string(out, "EPI1C") ||
        !ixiptlah_write_string(out, disease) ||
        !ixiptlah_write_string(out, cie10) ||
        !ixiptlah_write_value(out, year) ||
        !ixiptlah_write_string_table(out, entities) ||
        !ixiptlah_write_string_table(out, jurisdictions) ||
        !ixiptlah_write_string_table(out, periods)) return false;

    const std::uint32_t count = static_cast<std::uint32_t>(rows.size());
    if (!ixiptlah_write_value(out, count)) return false;
    return temporal_epi_write_packed_rows(out, rows);
}

bool read_epidemiology_payload_v1(std::istream& in, const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {
    std::string magic, disease, cie10;
    int year = 0;
    std::vector<std::string> entities, jurisdictions, periods;
    if (!ixiptlah_read_string(in, magic)) return false;
    if (magic != "EPI1" && magic != "EPI1C" && magic != "EPI1D") return false;
    if (!ixiptlah_read_string(in, disease) ||
        !ixiptlah_read_string(in, cie10) ||
        !ixiptlah_read_value(in, year)) return false;
    if (year < 1800 || year > 2300) return false;

    if (magic == "EPI1D") {
        return temporal_epi_read_distribution_payload_v1(in, on_record, disease, cie10, year);
    }

    if (!ixiptlah_read_string_table(in, entities) ||
        !ixiptlah_read_string_table(in, jurisdictions) ||
        !ixiptlah_read_string_table(in, periods)) return false;

    std::uint32_t count = 0;
    if (!ixiptlah_read_value(in, count) || count > 10000000u) return false;

    if (magic == "EPI1C") {
        std::vector<TemporalEpiV1GeneralRow> rows;
        if (!temporal_epi_read_packed_rows(in, rows, count)) return false;
        for (const TemporalEpiV1GeneralRow& row : rows) {
            if (row.entity >= entities.size() || row.jurisdiction >= jurisdictions.size() || row.period >= periods.size()) continue;
            TemporalEpidemiologyRecord r;
            r.entity = entities[row.entity];
            r.year = year;
            r.epi_week = static_cast<int>(row.week);
            r.page = static_cast<int>(row.page);
            r.disease = disease;
            r.cie10 = cie10;
            r.jurisdiction = jurisdictions[row.jurisdiction];
            r.period = periods[row.period];
            r.sex = temporal_epi_sex_text(row.sex);
            r.value = static_cast<int64_t>(row.value);
            if (!on_record(r)) return false;
        }
        return true;
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint16_t entity = 0, page = 0, jurisdiction = 0, period = 0;
        std::uint8_t week = 0, sex = 0;
        std::int32_t value = 0;
        if (!ixiptlah_read_value(in, entity) ||
            !ixiptlah_read_value(in, week) ||
            !ixiptlah_read_value(in, page) ||
            !ixiptlah_read_value(in, jurisdiction) ||
            !ixiptlah_read_value(in, period) ||
            !ixiptlah_read_value(in, sex) ||
            !ixiptlah_read_value(in, value)) return false;
        if (entity >= entities.size() || jurisdiction >= jurisdictions.size() || period >= periods.size()) continue;
        TemporalEpidemiologyRecord r;
        r.entity = entities[entity];
        r.year = year;
        r.epi_week = static_cast<int>(week);
        r.page = static_cast<int>(page);
        r.disease = disease;
        r.cie10 = cie10;
        r.jurisdiction = jurisdictions[jurisdiction];
        r.period = periods[period];
        r.sex = temporal_epi_sex_text(sex);
        r.value = static_cast<int64_t>(value);
        if (!on_record(r)) return false;
    }
    return true;
}





std::ofstream* temporal_stream_for_path_locked(const fs::path& path, const std::string& header) {


    if (path.empty()) return nullptr;


    ensure_dir(path.parent_path());


    auto& sinks = temporal_append_sinks();


    const std::string key = path_utf8(path);
    auto it = sinks.find(key);


    if (it != sinks.end()) {
        it->second->last_used = ++temporal_append_tick();


        return it->second->stream ? &it->second->stream : nullptr;
    }


    const bool needs_header = !fs::exists(path) || file_size_or_zero(path) == 0;
    auto sink = std::make_unique<TemporalAppendSink>();


    sink->path = path;
    sink->last_used = ++temporal_append_tick();


    sink->stream.open(path, std::ios::binary | std::ios::app);


    if (!sink->stream) return nullptr;


    if (needs_header) sink->stream << header << '\n';
    auto* stream = &sink->stream;


    sinks.emplace(key, std::move(sink));


    const size_t max_open = static_cast<size_t>(std::max(8, std::min(512, []{
        const std::string v = getenv_utf8_or_empty("TLALPOWA_TEMPORAL_MAX_OPEN_STREAMS");


        if (v.empty()) return 96;
        try { return std::stoi(v); } catch (...) { return 96; }
    }())));


    if (sinks.size() > max_open) {


        auto victim = sinks.end();


        for (auto jt = sinks.begin(); jt != sinks.end(); ++jt) {


            if (jt->first == key) continue;


            if (victim == sinks.end() || jt->second->last_used < victim->second->last_used) victim = jt;
        }


        if (victim != sinks.end()) {
            victim->second->stream.flush();
            victim->second->stream.close();


            sinks.erase(victim);
        }
    }


    return stream;
}




void temporal_append_line_locked(const fs::path& path, const std::string& header, const std::string& line) {


    if (auto* out = temporal_stream_for_path_locked(path, header)) {
        *out << line << '\n';
    }
}





std::string json_count_map(const std::map<std::string, size_t>& values) {
    std::ostringstream os;
    os << '{';

    bool first = true;


    for (const auto& [k, v] : values) {


        if (!first) os << ',';
        first = false;


        os << '"' << json_escape(k) << "\":" << v;
    }
    os << '}';


    return os.str();
}




bool write_string_map(std::ostream& out, const std::map<std::string, std::string>& fields) {


    const std::uint32_t n = static_cast<std::uint32_t>(std::min<size_t>(fields.size(), 1000000));


    if (!ixiptlah_write_value(out, n)) return false;
    std::uint32_t written = 0;


    for (const auto& [k, v] : fields) {


        if (written >= n) break;


        if (!ixiptlah_write_string(out, k) || !ixiptlah_write_string(out, v)) return false;
        ++written;
    }


    return true;
}




bool read_string_map_limited(std::istream& in, std::map<std::string, std::string>& fields) {


    fields.clear();
    std::uint32_t n = 0;


    if (!ixiptlah_read_value(in, n) || n > 1000000u) return false;


    for (std::uint32_t i = 0; i < n; ++i) {
        std::string k, v;


        if (!ixiptlah_read_string(in, k) || !ixiptlah_read_string(in, v)) return false;


        fields[std::move(k)] = std::move(v);
    }


    return true;
}




std::string atmosphere_purge_category_normalized(std::string category) {


    category = normalize_key(category);


    if (category == "meteorologia" || category == "meteorologico" || category == "redma") return "meteorologico";


    if (category == "contaminantes" || category == "contaminante" || category == "rama") return "contaminante";




    if (category == "satelital" || category == "satelite" || category == "satellite" ||
        category == "remote sensing" || category == "teledeteccion" || category == "teledeteccion") return "satelital";


    if (category == "todo" || category == "all" || category == "atmosfera") return "atmosfera";


    return category;
}




bool atmosphere_purge_category_matches_text(const std::string& text, const std::string& category) {


    if (category == "atmosfera") return true;


    const std::string n = normalize_key(text);


    if (category == "meteorologico") {


        return contains_norm(n, "meteorolog") || contains_norm(n, "redma") ||
               contains_norm(n, "temperatura") || contains_norm(n, "humedad") ||


               contains_norm(n, "viento") || contains_norm(n, "radiacion") ||
               contains_norm(n, "presion") || contains_norm(n, "precipit");
    }


    if (category == "contaminante") {


        return contains_norm(n, "contaminante") || contains_norm(n, "rama contamin") ||
               contains_norm(n, "pm10") || contains_norm(n, "pm25") ||
               contains_norm(n, "pm2 5") || contains_norm(n, "ozono") ||
               contains_norm(n, "aerosol") || contains_norm(n, "metal") ||
               contains_norm(n, "carbono") || contains_norm(n, "cov") ||
               contains_norm(n, "gas traza") || contains_norm(n, "invernadero") ||
               contains_norm(n, "o3") || contains_norm(n, "no2") ||
               contains_norm(n, "so2") || contains_norm(n, "nox") ||
               contains_norm(n, "hcho") || contains_norm(n, "nh3") ||
               contains_norm(n, "h2s") || contains_norm(n, "ch4") ||
               contains_norm(n, "co2") || contains_norm(n, "btex") ||
               contains_norm(n, "benceno") || contains_norm(n, "tolueno") ||
               contains_norm(n, "xileno") || contains_norm(n, "monoxido") ||
               contains_norm(n, " co ") || n == "co";
    }


    if (category == "satelital") {


        return contains_norm(n, "satelital") || contains_norm(n, "satellite") ||
               contains_norm(n, "teledeteccion") || contains_norm(n, "remote sensing") ||
               contains_norm(n, "sentinel") || contains_norm(n, "sentinel5p") ||

               contains_norm(n, "tropomi") || contains_norm(n, "s5p") ||
               contains_norm(n, "omi") || contains_norm(n, "aura") ||
               contains_norm(n, "modis") || contains_norm(n, "maiac") ||

               contains_norm(n, "mcd19") || contains_norm(n, "goes") ||
               contains_norm(n, "abi") || contains_norm(n, "viirs") ||
               contains_norm(n, "aod") || contains_norm(n, "hdf") ||

               contains_norm(n, "netcdf") || contains_norm(n, "geotiff") ||


               contains_norm(n, "grib");
    }


    return false;
}




std::string atmosphere_parameter_category(const std::string& raw) {


    const std::string p = normalize_key(raw);


    static const std::set<std::string> meteorological = {
        "tmp", "tmax", "tmin", "rh", "wsp", "wdr", "wgst", "wdr_gust", "u10", "v10", "gr", "uva", "uvb", "uvc", "uv", "pa", "pp", "pblh"
    };


    static const std::set<std::string> pollutants = {
        "o3", "co", "no", "no2", "nox", "so2", "pm10", "pm25", "pm2 5", "pmco",
        "pb", "cd", "as", "ni", "hg", "cr", "bc", "ec", "oc", "tc",
        "h2s", "ben", "tol", "xyl", "btex", "hcho", "nh3", "co2", "ch4",
        "so4", "no3a", "aod", "uvai", "inorg_aer", "ox", "pm25_pm10",
        "pmco_pm10", "no2_nox", "no_nox", "no_no2", "hcho_no2", "hcho_nox",
        "oc_ec", "ec_oc", "ec_tc", "oc_tc", "bc_pm25", "ec_pm25",
        "oc_pm25", "so4_no3a", "tol_ben", "xyl_ben", "btex_ben",
        "o3_no2", "co_no2"
    };


    if (meteorological.count(p) > 0) return "meteorologico";


    if (pollutants.count(p) > 0) return "contaminante";


    if (atmosphere_purge_category_matches_text(p, "meteorologico")) return "meteorologico";


    if (atmosphere_purge_category_matches_text(p, "contaminante")) return "contaminante";


    return {};
}




bool atmosphere_purge_category_matches_parameter(const std::string& parameter, const std::string& category) {


    if (category == "atmosfera") return true;


    return atmosphere_parameter_category(parameter) == category;
}




bool atmosphere_year_filter_active(int year_start, int year_end) {


    return year_start > 0 || year_end > 0;
}




bool atmosphere_year_in_range(int year, int year_start, int year_end) {


    if (!atmosphere_year_filter_active(year_start, year_end)) return true;


    if (year <= 0) return false;


    int lo = year_start > 0 ? year_start : year_end;


    int hi = year_end > 0 ? year_end : year_start;


    if (lo > hi) std::swap(lo, hi);


    return year >= lo && year <= hi;
}




int parse_atmosphere_year_field(const std::string& raw) {
    const std::string value = trim(raw);


    for (size_t i = 0; i + 3 < value.size(); ++i) {


        if (!std::isdigit(static_cast<unsigned char>(value[i])) ||
            !std::isdigit(static_cast<unsigned char>(value[i + 1])) ||
            !std::isdigit(static_cast<unsigned char>(value[i + 2])) ||

            !std::isdigit(static_cast<unsigned char>(value[i + 3]))) continue;
        try {


            const int year = std::stoi(value.substr(i, 4));


            if (year >= 1800 && year <= 2300) return year;
        } catch (...) {}
    }


    return 0;
}




bool ixiptlah_file_matches_year_filter(const fs::path& path, int year_start, int year_end) {


    if (!atmosphere_year_filter_active(year_start, year_end)) return true;


    int year = 0, month = 0;


    if (monthly_ixiptlah_name_parts(path_utf8(path.stem()), year, month)) return atmosphere_year_in_range(year, year_start, year_end);
    int weekly_day = 0;
    if (temporal_unified_weekly_ixiptlah_name_parts(path_utf8(path.stem()), &year, &month, &weekly_day)) {
        return atmosphere_year_in_range(year, year_start, year_end);
    }

    TemporalIxiptlahCategory category = TemporalIxiptlahCategory::Epidemiological;
    int shard = 0;
    if (temporal_decade_ixiptlah_name_parts(path_utf8(path.stem()), &category, &shard)) {
        if (category == TemporalIxiptlahCategory::Epidemiological) return atmosphere_year_in_range(shard, year_start, year_end);
        const int shard_end = std::min(9999, shard + 9);
        int lo = year_start > 0 ? year_start : year_end;
        int hi = year_end > 0 ? year_end : year_start;
        if (lo > hi) std::swap(lo, hi);
        return !(shard_end < lo || shard > hi);
    }

    return false;
}




bool read_batch_domain_for_purge(const std::string& payload, std::string& source_text, std::string& domain_text, int& year) {
    std::istringstream in(payload, std::ios::in | std::ios::binary);


    std::string source_id, source_file, source_path, domain;


    if (!ixiptlah_read_string(in, source_id) ||


        !ixiptlah_read_string(in, source_file) ||


        !ixiptlah_read_string(in, source_path) ||


        !ixiptlah_read_string(in, domain) ||


        !ixiptlah_read_value(in, year)) return false;


    source_text = source_id + " " + source_file + " " + source_path;
    domain_text = domain;


    return true;
}



bool ixiptlah_atmosphere_payload_matches_category(IxiptlahRecordType type,


                                                  std::uint32_t schema,
                                                  const std::string& payload,
                                                  const std::string& category,


                                                  int year_start,



                                                  int year_end) {


    if (schema != 1) return false;


    if (type == IxiptlahRecordType::AtmosphereRenderSummary) {
        std::istringstream in(payload, std::ios::in | std::ios::binary);


        std::string pollutant, station_id, station, unit, date, hour;


        int year = 0;


        if (!ixiptlah_read_string(in, pollutant) ||


            !ixiptlah_read_string(in, station_id) ||


            !ixiptlah_read_string(in, station) ||


            !ixiptlah_read_string(in, unit) ||


            !ixiptlah_read_string(in, date) ||


            !ixiptlah_read_string(in, hour) ||


            !ixiptlah_read_value(in, year)) return false;


        if (!atmosphere_year_in_range(year, year_start, year_end)) return false;


        return category == "atmosfera" || atmosphere_purge_category_matches_parameter(pollutant, category);
    }


    if (type == IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch) {
        std::string source_text, domain_text;


        int year = 0;


        if (!read_batch_domain_for_purge(payload, source_text, domain_text, year)) return false;


        if (!atmosphere_year_in_range(year, year_start, year_end)) return false;


        return category == "atmosfera" ||
               atmosphere_purge_category_matches_text(domain_text, category) ||
               atmosphere_purge_category_matches_text(source_text, category);
    }


    if (type == IxiptlahRecordType::AtmosphereMeasurement ||


        type == IxiptlahRecordType::AtmosphereTerritoryAverage) {
        std::istringstream in(payload, std::ios::in | std::ios::binary);


        std::map<std::string, std::string> fields;


        if (!read_string_map_limited(in, fields)) return false;


        const auto get = [&](const char* key) -> std::string {


            const auto it = fields.find(key);


            return it == fields.end() ? std::string{} : it->second;
        };


        const int year = parse_atmosphere_year_field(get("year").empty() ? get("date") : get("year"));


        if (!atmosphere_year_in_range(year, year_start, year_end)) return false;


        return category == "atmosfera" ||


               atmosphere_purge_category_matches_text(get("domain") + " " + get("source_file") + " " + get("source_path"), category) ||


               atmosphere_purge_category_matches_parameter(get("pollutant"), category);
    }


    if (type == IxiptlahRecordType::MonthlySourceInventory) {
        std::istringstream in(payload, std::ios::in | std::ios::binary);


        std::map<std::string, std::string> fields;


        if (!read_string_map_limited(in, fields)) return false;


        const auto get = [&](const char* key) -> std::string {


            const auto it = fields.find(key);


            return it == fields.end() ? std::string{} : it->second;
        };


        const int year = parse_atmosphere_year_field(get("year").empty() ? get("date") : get("year"));


        if (!atmosphere_year_in_range(year, year_start, year_end)) return false;


        return category == "atmosfera" ||


               atmosphere_purge_category_matches_text(get("domain") + " " + get("provider") + " " + get("source_file") + " " + get("source_path") + " " + get("metric"), category);
    }


    if (category == "atmosfera" &&


        (type == IxiptlahRecordType::MonthlyAtmosphereRenderBatch ||


         type == IxiptlahRecordType::MonthlyAtmosphereTerritoryBatch)) {


        return true;
    }


    return false;
}




std::string read_remaining_payload_bytes(std::istream& in) {
    std::ostringstream bytes(std::ios::out | std::ios::binary);


    bytes << in.rdbuf();


    return bytes.str();
}



void append_ixiptlah_fields_record(const fs::path& root,


                                  IxiptlahRecordType type,



                                  const std::map<std::string, std::string>& fields) {


    if (root.empty()) return;


    const fs::path target = temporal_category_shards_enabled() ? temporal_category_ixiptlah_file_for_fields(root, fields) : temporal_monthly_ixiptlah_file_for_fields(root, fields);
    const bool atmosphere = is_atmosphere_fields(fields);
    const std::string layer_key = atmosphere ? temporal_atmospheric_layer_key_for_fields(fields) : std::string{};
    std::uint64_t temporal_key = 0;
    if (atmosphere) {
        const auto get = [&](const char* key) -> std::string {
            const auto it = fields.find(key);
            return it == fields.end() ? std::string{} : it->second;
        };
        const int year = temporal_year_from_date_or_fields(get("date"), get("year"));
        const int month = month_from_iso_date(get("date"));
        int day = 1;
        const std::string date = get("date");
        if (date.size() >= 10 && date[7] == '-' &&
            std::isdigit(static_cast<unsigned char>(date[8])) && std::isdigit(static_cast<unsigned char>(date[9]))) {
            try { day = std::clamp(std::stoi(date.substr(8, 2)), 1, 31); } catch (...) { day = 1; }
        }
        int hh = 0;
        int mm = 0;
        const std::string hour = get("hour");
        if (hour.size() >= 2 && std::isdigit(static_cast<unsigned char>(hour[0])) && std::isdigit(static_cast<unsigned char>(hour[1]))) {
            try { hh = std::clamp(std::stoi(hour.substr(0, 2)), 0, 23); } catch (...) { hh = 0; }
        }
        const std::size_t colon = hour.find(':');
        if (colon != std::string::npos && colon + 2 < hour.size() &&
            std::isdigit(static_cast<unsigned char>(hour[colon + 1])) && std::isdigit(static_cast<unsigned char>(hour[colon + 2]))) {
            try { mm = std::clamp(std::stoi(hour.substr(colon + 1, 2)), 0, 59); } catch (...) { mm = 0; }
        }
        if (year > 0 && month > 0) {
            temporal_key = temporal_atmosphere_time_key(year, month, static_cast<std::uint8_t>(day), static_cast<std::uint8_t>(hh), static_cast<std::uint8_t>(mm));
        }
    }
    (void)ixiptlah_append_record_tagged_temporal(target, type, 1, layer_key, temporal_key, [&](std::ostream& out) {


        return write_string_map(out, fields);
    });
}

}




int temporal_lustrum_start_for_year(int year) {


    if (year < 0) year = 0;


    return (year / 5) * 5;
}




std::string temporal_lustrum_label_for_year(int year) {


    const int start = temporal_lustrum_start_for_year(year);
    std::ostringstream os;


    os << std::setw(4) << std::setfill('0') << start << '-' << std::setw(4) << std::setfill('0') << (start + 4);


    return os.str();
}




std::string temporal_iso_date_from_epi_week(int year, int week) {


    if (year <= 0) return "0000-01-01";


    if (week <= 0) week = 1;


    if (week > 53) week = 53;


    const long long jan4 = days_from_civil(year, 1, 4);


    const long long week1_monday = jan4 - (iso_weekday_from_days(jan4) - 1);


    const long long target = week1_monday + static_cast<long long>(week - 1) * 7;


    auto [y, m, d] = civil_from_days(target);


    return ymd_string(y, m, d);
}




int temporal_year_from_date_or_fields(const std::string& date, const std::string& year_hint) {


    if (date.size() >= 4 && std::isdigit(static_cast<unsigned char>(date[0])) && std::isdigit(static_cast<unsigned char>(date[1])) &&


        std::isdigit(static_cast<unsigned char>(date[2])) && std::isdigit(static_cast<unsigned char>(date[3]))) {


        try { return std::stoi(date.substr(0, 4)); } catch (...) {}
    }


    if (!year_hint.empty()) {


        try { return std::stoi(year_hint); } catch (...) {}
    }


    return 0;
}




const std::vector<std::string>& temporal_block_columns() {


    static const std::vector<std::string> cols = {


        "siglas_entidad", "año", "semana_epidemiológica", "página_boletín", "enfermedad", "cie10",
        "alcaldía", "periodo", "sexo", "casos"
    };


    return cols;
}




const std::vector<std::string>& temporal_atmosphere_columns() {


    static const std::vector<std::string> cols = {


        "record_type", "date", "hour", "year", "siglas_entidad", "siglas_municipio", "municipio",


        "domain", "source_id", "source_file", "source_path", "pollutant", "station_id", "station",


        "lon", "lat", "alt", "metric", "value", "value_text", "unit", "count", "min", "max", "payload"
    };


    return cols;
}




const std::vector<std::string>& temporal_columns_for_fields(const std::map<std::string, std::string>& fields) {


    if (is_atmosphere_fields(fields)) return temporal_atmosphere_columns();


    return temporal_block_columns();
}




std::string temporal_block_header_line() {
    std::ostringstream os;


    const auto& cols = temporal_block_columns();


    for (size_t i = 0; i < cols.size(); ++i) {


        if (i) os << '\t';


        os << cols[i];
    }


    return os.str();
}




std::string temporal_header_line_for_fields(const std::map<std::string, std::string>& fields) {
    std::ostringstream os;


    const auto& cols = temporal_columns_for_fields(fields);


    for (size_t i = 0; i < cols.size(); ++i) {


        if (i) os << '\t';
        os << cols[i];
    }


    return os.str();
}




std::vector<std::string> split_tsv_lossless(const std::string& line) {


    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(line);


    while (std::getline(is, cur, '\t')) out.push_back(cur);


    if (!line.empty() && line.back() == '\t') out.push_back({});


    return out;
}




std::string tsv_escape_field(std::string value) {


    for (char& c : value) {


        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }


    return value;
}




bool temporal_is_lustrum_data_file(const fs::path& p, const std::string& required_extension) {


    const std::string ext = normalized_extension(p);


    if (!required_extension.empty() && ext != required_extension) return false;


    if (ext != kIxiptlahExtension) return false;


    int decade = 0;
    int y = 0, m = 0, d = 0;
    const std::string stem = path_utf8(p.stem());
    if (temporal_unified_weekly_ixiptlah_name_parts(stem, &y, &m, &d)) return true;
    // Compatibilidad de lectura: los núcleos por categoría/año existentes siguen
    // válidos, pero las escrituras nuevas prefieren YYYY_MM_DD semanal unificado.
    return temporal_decade_ixiptlah_name_parts(stem, nullptr, &decade);
}




std::vector<fs::path> temporal_tsv_files(const fs::path& root) {
    (void)root;


    return {};
}




std::vector<fs::path> temporal_json_files(const fs::path& root) {
    (void)root;


    return {};
}




std::vector<fs::path> temporal_csv_files(const fs::path& root) {


    (void)root;


    return {};
}




fs::path temporal_ixiptlah_file(const fs::path& root) {


    return temporal_category_shards_enabled() ? temporal_decade_ixiptlah_file_in_root(root, TemporalIxiptlahCategory::Epidemiological, 0) : temporal_monthly_ixiptlah_file_in_root(root, 0, 0);
}




std::vector<fs::path> temporal_ixiptlah_files(const fs::path& root) {


    std::vector<fs::path> out;


    std::vector<fs::path> dirs;


    const fs::path canonical = temporal_ixiptlah_root();


    const fs::path requested = root.empty() ? canonical : root;



    if (temporal_category_shards_enabled()) {
        // IXIPTLAH V1 descubre únicamente archivos reales por década natural.
        // No se fabrican rutas de categoría inexistentes: cada stat innecesario
        // cuesta latencia en el arranque y ensucia la ruta de lectura reciente.
    }



    auto add_dir = [&](const fs::path& dir) {


        if (dir.empty()) return;


        const std::string key = path_utf8(dir.lexically_normal());


        for (const auto& existing : dirs) {


            if (path_utf8(existing.lexically_normal()) == key) return;
        }


        dirs.push_back(dir);
    };



    add_dir(requested);



    for (const auto& dir : dirs) {
        std::error_code ec;


        if (!fs::exists(dir, ec) || ec) { ec.clear(); continue; }


        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
            std::error_code item_ec;


            if (!it->is_regular_file(item_ec) || item_ec) { ec.clear(); continue; }


            if (temporal_is_lustrum_data_file(it->path(), kIxiptlahExtension)) out.push_back(it->path());
        }
    }



    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
        int ay = -1, am = -1, ady = -1;
        int by = -1, bm = -1, bdy = -1;
        const bool aw = temporal_ixiptlah_file_date_key(a, ay, am, ady);
        const bool bw = temporal_ixiptlah_file_date_key(b, by, bm, bdy);
        if (aw && bw) {
            if (ay != by) return ay > by;
            if (am != bm) return am > bm;
            if (ady != bdy) return ady > bdy;
        }
        if (aw != bw) return aw;
        int ad = -1, bd = -1;
        TemporalIxiptlahCategory ac = TemporalIxiptlahCategory::Epidemiological;
        TemporalIxiptlahCategory bc = TemporalIxiptlahCategory::Epidemiological;
        const bool ap = temporal_decade_ixiptlah_name_parts(path_utf8(a.stem()), &ac, &ad);
        const bool bp = temporal_decade_ixiptlah_name_parts(path_utf8(b.stem()), &bc, &bd);
        if (ap && bp && ad != bd) return ad > bd;
        if (ap && bp && static_cast<int>(ac) != static_cast<int>(bc)) return static_cast<int>(ac) < static_cast<int>(bc);
        if (ap != bp) return ap;
        return path_utf8(a.filename()) > path_utf8(b.filename());
    });



    out.erase(std::unique(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {


        return path_utf8(a.lexically_normal()) == path_utf8(b.lexically_normal());
    }), out.end());


    return out;
}




bool temporal_has_ixiptlah_records(const fs::path& root) {


    for (const auto& p : temporal_ixiptlah_files(root)) {


        if (fs::exists(p) && file_size_or_zero(p) > 16) return true;
    }


    return false;
}


std::vector<fs::path> temporal_epidemiology_ixiptlah_files(const fs::path& root) {
    struct EpiFileListCacheEntry {
        std::int64_t dir_mtime_ns = 0;
        std::chrono::steady_clock::time_point cached_at{};
        std::vector<fs::path> files;
    };
    static std::mutex cache_mu;
    static std::unordered_map<std::string, EpiFileListCacheEntry> cache;

    const fs::path canonical_root = root.empty() ? temporal_ixiptlah_root() : root;
    const std::string cache_key = path_utf8(canonical_root.lexically_normal());
    const auto now = std::chrono::steady_clock::now();
    std::error_code ec;
    const auto wt = fs::last_write_time(canonical_root, ec);
    const std::int64_t mtime_ns = ec ? 0 : std::chrono::duration_cast<std::chrono::nanoseconds>(wt.time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(cache_mu);
        const auto it = cache.find(cache_key);
        if (it != cache.end() && it->second.dir_mtime_ns == mtime_ns && !it->second.files.empty()) {
            const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.cached_at).count();
            // Lectura viva: varias plataformas no actualizan el mtime del directorio
            // con granularidad suficiente cuando el importador crea IXIPTLAH de forma
            // muy rápida. El TTL corto conserva la ruta caliente, pero obliga a
            // redescubrir enfermedades nuevas sin esperar reinicio ni segunda pasada.
            if (age_ms >= 0 && age_ms < 120) return it->second.files;
        }
    }

    std::vector<fs::path> files = temporal_ixiptlah_files(canonical_root);
    std::vector<fs::path> out;
    out.reserve(files.size());
    for (const fs::path& path : files) {
        const std::string stem = path_utf8(path.stem());
        if (stem.find("REPRESENTACION_PREFORMADA") != std::string::npos ||
            stem.find("RENDER_CACHE") != std::string::npos) {
            continue;
        }
        int wy = 0, wm = 0, wd = 0;
        if (temporal_unified_weekly_ixiptlah_name_parts(stem, &wy, &wm, &wd)) {
            out.push_back(path);
            continue;
        }
        int decade = 0;
        TemporalIxiptlahCategory cat = TemporalIxiptlahCategory::Epidemiological;
        if (temporal_decade_ixiptlah_name_parts(stem, &cat, &decade) && cat == TemporalIxiptlahCategory::Epidemiological) {
            out.push_back(path);
        }
    }
    // IXIPTLAH-EPI: el listado de nombres se cachea por mtime del directorio raíz.
    // Las escrituras append-only modifican archivos ya conocidos sin invalidar el
    // listado; la caché de shard por tamaño/mtime detecta esos cambios. Crear una
    // enfermedad nueva sí modifica el directorio y por eso invalida esta lista.
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
        int ay = -1, am = -1, ady = -1;
        int by = -1, bm = -1, bdy = -1;
        const bool aw = temporal_ixiptlah_file_date_key(a, ay, am, ady);
        const bool bw = temporal_ixiptlah_file_date_key(b, by, bm, bdy);
        if (aw && bw) {
            if (ay != by) return ay > by;
            if (am != bm) return am > bm;
            if (ady != bdy) return ady > bdy;
        }
        if (aw != bw) return aw;
        int ad = -1, bd = -1;
        (void)temporal_decade_ixiptlah_name_parts(path_utf8(a.stem()), nullptr, &ad);
        (void)temporal_decade_ixiptlah_name_parts(path_utf8(b.stem()), nullptr, &bd);
        if (ad != bd) return ad > bd;
        return path_utf8(a.filename()) > path_utf8(b.filename());
    });
    out.erase(std::unique(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
        return path_utf8(a.lexically_normal()) == path_utf8(b.lexically_normal());
    }), out.end());
    {
        std::lock_guard<std::mutex> lock(cache_mu);
        if (cache.size() > 16u) cache.clear();
        cache[cache_key] = EpiFileListCacheEntry{mtime_ns, now, out};
    }
    return out;
}

bool temporal_has_epidemiology_ixiptlah_records(const fs::path& root) {
    for (const auto& p : temporal_epidemiology_ixiptlah_files(root)) {
        if (fs::exists(p) && file_size_or_zero(p) > 16) return true;
    }
    return false;
}



void temporal_read_epidemiology_records(const fs::path& root,



                                        const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {


    for (const auto& path : temporal_epidemiology_ixiptlah_files(root)) {
        bool keep_reading_files = true;
        ixiptlah_read_selected_records(path,
            [](IxiptlahRecordType type, std::uint32_t schema) {
                return type == IxiptlahRecordType::MonthlyEpidemiologyBatch && (schema == 1);
            },
            [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
                keep_reading_files = read_epidemiology_payload_v1(in, on_record);
                return keep_reading_files;
            });
        if (!keep_reading_files) break;
    }
}

void temporal_read_epidemiology_records_from_ixiptlah_file(const fs::path& path,
                                                           const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {
    if (path.empty()) return;
    bool keep_reading_records = true;
    ixiptlah_read_selected_records(path,
        [](IxiptlahRecordType type, std::uint32_t schema) {
            return type == IxiptlahRecordType::MonthlyEpidemiologyBatch && (schema == 1);
        },
        [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
            keep_reading_records = read_epidemiology_payload_v1(in, on_record);
            return keep_reading_records;
        });
}


void temporal_read_epidemiology_records_from_ixiptlah_file_filtered(
    const fs::path& path,
    const std::function<bool(std::uint64_t layer_hash, std::uint64_t temporal_key)>& accept_record,
    const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {
    if (path.empty()) return;
    bool keep_reading_records = true;
    ixiptlah_read_selected_records_tagged_temporal(path,
        [&](IxiptlahRecordType type, std::uint32_t schema, std::uint64_t layer_hash, std::uint64_t temporal_key) {
            if (type != IxiptlahRecordType::MonthlyEpidemiologyBatch || (schema != 1)) return false;
            return !accept_record || accept_record(layer_hash, temporal_key);
        },
        [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
            keep_reading_records = read_epidemiology_payload_v1(in, on_record);
            return keep_reading_records;
        });
}

void temporal_read_epidemiology_records_from_ixiptlah_file_exact(
    const fs::path& path,
    const std::vector<std::uint64_t>& layer_hashes,
    bool include_zero_layer,
    std::uint64_t temporal_begin,
    std::uint64_t temporal_end,
    const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {
    if (path.empty()) return;
    bool keep_reading_records = true;
    for (std::uint32_t schema : {1u}) {
        if (!keep_reading_records) break;
        ixiptlah_read_selected_records_tagged_temporal_exact(path,
            IxiptlahRecordType::MonthlyEpidemiologyBatch, schema, layer_hashes, include_zero_layer, temporal_begin, temporal_end,
            [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
                keep_reading_records = read_epidemiology_payload_v1(in, on_record);
                return keep_reading_records;
            });
    }
}

void temporal_read_epidemiology_records_recent_files(const fs::path& root,
                                                     int max_files,
                                                     const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record) {
    if (max_files <= 0) return;
    std::vector<fs::path> files = temporal_epidemiology_ixiptlah_files(root);
    if (files.empty()) return;
    if (files.size() > static_cast<size_t>(max_files)) files.resize(static_cast<size_t>(max_files));

    for (const auto& path : files) {
        bool keep_reading_files = true;
        ixiptlah_read_selected_records(path,
            [](IxiptlahRecordType type, std::uint32_t schema) {
                return type == IxiptlahRecordType::MonthlyEpidemiologyBatch && (schema == 1);
            },
            [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
                keep_reading_files = read_epidemiology_payload_v1(in, on_record);
                return keep_reading_files;
            });
        if (!keep_reading_files) break;
    }
}


template <class T>
bool temporal_epi_render_write_pod(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(out);
}

bool temporal_epi_render_write_string(std::ostream& out, const std::string& s) {
    if (s.size() > 16u * 1024u * 1024u) return false;
    const std::uint32_t n = static_cast<std::uint32_t>(s.size());
    if (!temporal_epi_render_write_pod(out, n)) return false;
    if (n) out.write(s.data(), static_cast<std::streamsize>(n));
    return static_cast<bool>(out);
}

bool temporal_epi_render_write_map(std::ostream& out, const std::map<std::string, std::string>& m) {
    if (m.size() > 1000000u) return false;
    const std::uint32_t n = static_cast<std::uint32_t>(m.size());
    if (!temporal_epi_render_write_pod(out, n)) return false;
    for (const auto& [k, v] : m) {
        if (!temporal_epi_render_write_string(out, k) || !temporal_epi_render_write_string(out, v)) return false;
    }
    return true;
}

std::string temporal_epi_week_label(int year, int week) {
    if (year <= 0 || week <= 0) return {};
    std::ostringstream os;
    os << year << '-' << std::setw(2) << std::setfill('0') << week;
    return os.str();
}

std::string temporal_epi_metric_from_period_inline(const std::string& period,
                                                   const std::string& source_year,
                                                   const std::string& year,
                                                   const std::string& sex) {
    if (period == "Sem") return "incidencia_semanal";
    if (period == "SemDerivada") return "incidencia_semanal_derivada_sexo";
    if (period == "Acum" && !source_year.empty() && source_year != year) return "acumulado_anio_anterior";
    if (period == "Acum" && (sex == "M" || sex == "F")) return "acumulado_anual_por_sexo";
    if (period == "Acum") return "acumulado_anual";
    return "valor";
}

std::string temporal_epi_infer_group_inline(const std::string& id, const std::string& name, const std::string& cie10) {
    const std::string n = normalize_key(id + " " + name + " " + cie10);
    const auto has = [&](const char* needle) { return n.find(needle) != std::string::npos; };
    if (has("rubeola") || has("tetanos") || has("tos ferina") || has("varicela") || has("parotiditis") || has("vacun")) return "prevenibles_vacunacion";
    if (has("colera") || has("intoxicacion alimentaria") || has("gastro") || has("intestinal") || has("diar") || has("helmint") || has("protozo")) return "digestivas";
    if (has("influenza") || has("neumonia") || has("respiratoria") || has("faringitis") || has("otitis")) return "respiratorias";
    if (has("vih") || has("sifilis") || has("gonoc") || has("genital") || has("venereo") || has("tricomon")) return "transmision_sexual";
    if (has("dengue") || has("zika") || has("chikungunya") || has("paludismo") || has("leishman") || has("oncocerc") || has("oeste del nilo") || has("ricketts")) return "vectores";
    if (has("rabia") || has("brucel") || has("leptosp") || has("teniasis") || has("cisticerc") || has("triquinos")) return "zoonosis";
    if (has("accidente") || has("morded") || has("picadura") || has("arma") || has("quemadura") || has("traumatico") || has("avisp") || has("abej")) return "accidentes";
    if (has("desnutric") || has("obesidad") || has("anorexia") || has("bulimia")) return "nutricion";
    if (has("anencef") || has("encefalocele") || has("bifida") || has("microcef") || has("paladar hendido")) return "defectos_nacer";
    if (has("depresion") || has("alzheimer") || has("parkinson") || has("neurolog")) return "neurologicas_salud_mental";
    if (!cie10.empty()) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(cie10[0])));
        if (c == 'A' || c == 'B') return "otras_enfermedades_transmisibles";
        if (c == 'J') return "respiratorias";
        if (c == 'V' || c == 'W' || c == 'X' || c == 'Y') return "accidentes";
        if (c == 'E' || c == 'C' || c == 'D' || c == 'I' || c == 'K' || c == 'N' || c == 'O' || c == 'Q' || c == 'T') return "no_transmisibles";
    }
    return "sin_categoria";
}

Config& temporal_epi_render_config() {
    static Config cfg;
    static std::once_flag once;
    std::call_once(once, []() {
        const fs::path cwd = fs::current_path();
        const fs::path candidates[] = {cwd / "Fuente" / "Tlalpowa", cwd / "Config", cwd};
        for (const fs::path& c : candidates) {
            std::error_code ec;
            if (fs::exists(c, ec) && !ec) { cfg.load(c); return; }
        }
        cfg.load({});
    });
    return cfg;
}

struct TemporalEpiRenderRow {
    std::string week;
    std::string jurisdiction_id;
    std::string jurisdiction;
    std::string disease_id;
    std::string disease;
    std::string cie10;
    std::string source_year;
    std::string period;
    std::string sex;
    std::string metric;
    int64_t value = 0;
    int week_index = -1;
    bool metric_weekly_incidence = false;
};

std::string temporal_jurisdiction_id_inline(const std::string& raw) {
    std::string id = normalize_key(raw);
    for (char& c : id) if (c == ' ') c = '_';
    if (id == "alvaro_obregon" || id == "azcapotzalco" || id == "benito_juarez" || id == "coyoacan" ||
        id == "cuajimalpa" || id == "cuauhtemoc" || id == "gustavo_a_madero" || id == "iztacalco" ||
        id == "iztapalapa" || id == "magdalena_contreras" || id == "miguel_hidalgo" || id == "milpa_alta" ||
        id == "tlahuac" || id == "tlalpan" || id == "venustiano_carranza" || id == "xochimilco" ||
        id == "total") return id;
    return id.empty() ? "sin_jurisdiccion" : id;
}

void temporal_epi_render_identity(const TemporalEpidemiologyRecord& r,
                                  std::string& disease_id,
                                  std::string& disease_name,
                                  std::string& disease_group) {
    Config& cfg = temporal_epi_render_config();
    std::optional<Disease> matched;
    if (!trim(r.cie10).empty()) matched = cfg.match_disease_cie10(r.cie10);
    if (!matched) matched = cfg.match_disease_text(normalize_key(r.disease));
    if (matched) {
        disease_id = matched->id;
        disease_name = matched->canonical.empty() ? r.disease : matched->canonical;
        disease_group = matched->group.empty() ? temporal_epi_infer_group_inline(disease_id, disease_name, r.cie10) : matched->group;
        return;
    }
    disease_id = normalize_key(r.disease);
    for (char& c : disease_id) if (c == ' ') c = '_';
    if (disease_id.empty()) disease_id = "unknown";
    disease_name = trim(r.disease).empty() ? disease_id : r.disease;
    disease_group = temporal_epi_infer_group_inline(disease_id, disease_name, r.cie10);
}

bool temporal_write_epi_render_snapshot_payload(std::ostream& out, const std::vector<TemporalEpiRenderRow>& rows,
                                                 const std::vector<std::string>& weeks,
                                                 const std::map<std::string, std::string>& diseases,
                                                 const std::map<std::string, std::string>& disease_groups,
                                                 size_t scanned_rows) {
    (void)diseases;
    (void)disease_groups;
    (void)scanned_rows;
    #pragma pack(push, 1)
    struct PackedRow {
        std::uint16_t week = 0;
        std::uint16_t jurisdiction = 0;
        std::uint16_t disease = 0;
        std::uint8_t distribution = 0;
        std::int32_t value = 0;
    };
    #pragma pack(pop)
    static_assert(sizeof(PackedRow) == 11, "EPI render V1 mantiene filas compactas de 11 bytes");

    if (rows.empty() || rows.size() > 10000000u) return false;

    std::vector<std::uint32_t> week_keys;
    std::vector<std::pair<std::string, std::string>> jurisdictions;
    std::vector<std::pair<std::string, std::string>> disease_pairs;
    std::vector<PackedRow> packed;
    week_keys.reserve(std::max<size_t>(weeks.size(), 1u));
    jurisdictions.reserve(64u);
    disease_pairs.reserve(64u);
    packed.reserve(rows.size());

    std::unordered_map<std::uint32_t, std::uint16_t> week_index;
    std::unordered_map<std::string, std::uint16_t> jurisdiction_index;
    std::unordered_map<std::string, std::uint16_t> disease_index;
    week_index.reserve(std::max<size_t>(weeks.size() * 2u + 17u, 32u));
    jurisdiction_index.reserve(128u);
    disease_index.reserve(128u);

    auto week_key_from_label = [](const std::string& label) -> std::uint32_t {
        if (label.size() < 7u) return 0u;
        int y = 0, w = 0;
        try {
            y = std::stoi(label.substr(0, 4));
            w = std::stoi(label.substr(5, 2));
        } catch (...) { return 0u; }
        if (y < 1800 || y > 2300 || w < 1 || w > 53) return 0u;
        return static_cast<std::uint32_t>(y) * 1000000u + static_cast<std::uint32_t>(w) * 10000u;
    };
    auto intern_week = [&](std::uint32_t key) -> std::uint16_t {
        const auto it = week_index.find(key);
        if (it != week_index.end()) return it->second;
        if (key == 0u || week_keys.size() > static_cast<size_t>(std::numeric_limits<std::uint16_t>::max())) return std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t id = static_cast<std::uint16_t>(week_keys.size());
        week_keys.push_back(key);
        week_index.emplace(key, id);
        return id;
    };
    auto intern_pair = [](std::unordered_map<std::string, std::uint16_t>& index,
                          std::vector<std::pair<std::string, std::string>>& values,
                          const std::string& id,
                          const std::string& name) -> std::uint16_t {
        const auto it = index.find(id);
        if (it != index.end()) return it->second;
        if (id.empty() || values.size() > static_cast<size_t>(std::numeric_limits<std::uint16_t>::max())) return std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t pos = static_cast<std::uint16_t>(values.size());
        values.emplace_back(id, name.empty() ? id : name);
        index.emplace(values.back().first, pos);
        return pos;
    };

    for (const std::string& w : weeks) {
        const std::uint32_t key = week_key_from_label(w);
        if (key) (void)intern_week(key);
    }

    for (const TemporalEpiRenderRow& o : rows) {
        const std::uint32_t wk = week_key_from_label(o.week);
        const std::uint8_t dist = temporal_epi_distribution_code(o.period, o.sex);
        if (wk == 0u || o.jurisdiction_id.empty() || o.disease_id.empty() || dist == 0u) continue;
        PackedRow row;
        row.week = intern_week(wk);
        row.jurisdiction = intern_pair(jurisdiction_index, jurisdictions, o.jurisdiction_id, o.jurisdiction);
        row.disease = intern_pair(disease_index, disease_pairs, o.disease_id, o.disease);
        if (row.week == std::numeric_limits<std::uint16_t>::max() ||
            row.jurisdiction == std::numeric_limits<std::uint16_t>::max() ||
            row.disease == std::numeric_limits<std::uint16_t>::max()) return false;
        row.distribution = dist;
        row.value = static_cast<std::int32_t>(std::clamp<std::int64_t>(o.value, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
        packed.push_back(row);
    }

    if (week_keys.empty() || jurisdictions.empty() || disease_pairs.empty() || packed.empty()) return false;
    if (week_keys.size() > 65535u || jurisdictions.size() > 65535u || disease_pairs.size() > 65535u || packed.size() > 10000000u) return false;

    const std::uint16_t wc = static_cast<std::uint16_t>(week_keys.size());
    const std::uint16_t jc = static_cast<std::uint16_t>(jurisdictions.size());
    const std::uint16_t dc = static_cast<std::uint16_t>(disease_pairs.size());
    const std::uint32_t rc = static_cast<std::uint32_t>(packed.size());

    if (!temporal_epi_render_write_string(out, "TLALPOWA_EPI_RENDER_V1") ||
        !temporal_epi_render_write_pod(out, wc) ||
        !temporal_epi_render_write_pod(out, jc) ||
        !temporal_epi_render_write_pod(out, dc) ||
        !temporal_epi_render_write_pod(out, rc)) return false;
    for (const std::uint32_t key : week_keys) if (!temporal_epi_render_write_pod(out, key)) return false;
    for (const auto& j : jurisdictions) {
        if (!temporal_epi_render_write_string(out, j.first) || !temporal_epi_render_write_string(out, j.second)) return false;
    }
    for (const auto& d : disease_pairs) {
        if (!temporal_epi_render_write_string(out, d.first) || !temporal_epi_render_write_string(out, d.second)) return false;
    }
    const std::uint64_t bytes = static_cast<std::uint64_t>(packed.size()) * sizeof(PackedRow);
    if (bytes > 256ull * 1024ull * 1024ull) return false;
    out.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(bytes));
    return static_cast<bool>(out);
}

bool temporal_embed_epidemiology_render_snapshot_inline(const fs::path& target) {
    if (target.empty()) return false;
    std::vector<TemporalEpiRenderRow> rows;
    std::vector<std::string> weeks;
    std::map<std::string, std::string> diseases;
    std::map<std::string, std::string> disease_groups;
    std::unordered_set<std::string> dedupe;
    rows.reserve(4096);
    weeks.reserve(384);
    dedupe.reserve(8192);
    size_t scanned = 0;
    std::uint64_t snapshot_temporal_key = 0ull;
    bool snapshot_temporal_mixed = false;

    temporal_read_epidemiology_records_from_ixiptlah_file(target, [&](const TemporalEpidemiologyRecord& r) {
        ++scanned;
        const std::uint64_t row_temporal_key = temporal_epi_time_key(r.year, r.epi_week);
        if (snapshot_temporal_key == 0ull) snapshot_temporal_key = row_temporal_key;
        else if (snapshot_temporal_key != row_temporal_key) snapshot_temporal_mixed = true;
        TemporalEpiRenderRow o;
        o.week = temporal_epi_week_label(r.year, r.epi_week);
        std::string disease_group;
        temporal_epi_render_identity(r, o.disease_id, o.disease, disease_group);
        if (o.disease_id.empty() || o.week.empty()) return true;
        o.cie10 = r.cie10;
        o.jurisdiction_id = temporal_jurisdiction_id_inline(r.jurisdiction);
        o.jurisdiction = r.jurisdiction.empty() ? o.jurisdiction_id : r.jurisdiction;
        o.source_year = std::to_string(r.year);
        o.period = r.period;
        o.sex = r.sex;
        o.metric = temporal_epi_metric_from_period_inline(o.period, o.source_year, o.source_year, o.sex);
        o.value = r.value;
        if (o.jurisdiction_id.empty()) return true;
        std::string key;
        key.reserve(o.week.size() + o.jurisdiction_id.size() + o.disease_id.size() + o.period.size() + o.sex.size() + 48u);
        key += o.week; key.push_back('\x1f'); key += o.jurisdiction_id; key.push_back('\x1f');
        key += o.disease_id; key.push_back('\x1f'); key += o.cie10; key.push_back('\x1f');
        key += o.period; key.push_back('\x1f'); key += o.sex; key.push_back('\x1f'); key += std::to_string(o.value);
        if (!dedupe.insert(std::move(key)).second) return true;
        diseases[o.disease_id] = o.disease;
        disease_groups[o.disease_id] = disease_group.empty() ? "sin_categoria" : disease_group;
        weeks.push_back(o.week);
        rows.push_back(std::move(o));
        return true;
    });
    if (rows.empty()) return false;
    std::sort(weeks.begin(), weeks.end());
    weeks.erase(std::unique(weeks.begin(), weeks.end()), weeks.end());
    std::unordered_map<std::string, int> week_to_index;
    week_to_index.reserve(weeks.size() * 2u + 1u);
    for (int i = 0; i < static_cast<int>(weeks.size()); ++i) week_to_index[weeks[static_cast<size_t>(i)]] = i;
    for (TemporalEpiRenderRow& r : rows) {
        const auto it = week_to_index.find(r.week);
        r.week_index = it == week_to_index.end() ? -1 : it->second;
        r.metric_weekly_incidence = (r.metric == "incidencia_semanal");
    }
    ixiptlah_close_all();
    (void)ixiptlah_rewrite_without_records(target, [](IxiptlahRecordType type, std::uint32_t) {
        return type == IxiptlahRecordType::EpidemiologyRenderSnapshot;
    });
    const auto writer = [&](std::ostream& out) {
        return temporal_write_epi_render_snapshot_payload(out, rows, weeks, diseases, disease_groups, scanned);
    };
    if (snapshot_temporal_mixed) snapshot_temporal_key = 0ull;
    if (ixiptlah_append_record_tagged_temporal(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, 0ull, snapshot_temporal_key, writer)) return true;
    ixiptlah_close_all();
    return ixiptlah_append_record_tagged_temporal(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, 0ull, snapshot_temporal_key, writer);
}



std::uint64_t temporal_epi_fnv1a_append(std::uint64_t h, const char* data, std::size_t n) {
    constexpr std::uint64_t prime = 1099511628211ull;
    if (!data && n) return h;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<unsigned char>(data[i]);
        h *= prime;
    }
    return h;
}

std::uint64_t temporal_epi_fnv1a_append_string(std::uint64_t h, const std::string& s) {
    h = temporal_epi_fnv1a_append(h, s.data(), s.size());
    const unsigned char sep = 0x1fu;
    return temporal_epi_fnv1a_append(h, reinterpret_cast<const char*>(&sep), 1);
}

std::uint64_t temporal_epi_exact_key_hash(const TemporalEpidemiologyRecord& r) {
    // Deduplicación IXIPTLAH en 64 bits: conserva semántica exacta de la clave
    // previa, pero evita retener millones de cadenas heap-residentes durante una
    // corrida larga. El hash se usa sólo como índice antirrepetición operativo;
    // los datos primarios siguen escritos completos en EPI1.
    std::uint64_t h = 1469598103934665603ull;
    h = temporal_epi_fnv1a_append_string(h, normalized_entity_code(r.entity));
    h = temporal_epi_fnv1a_append_string(h, std::to_string(r.year));
    h = temporal_epi_fnv1a_append_string(h, std::to_string(r.epi_week));
    h = temporal_epi_fnv1a_append_string(h, std::to_string(r.page));
    h = temporal_epi_fnv1a_append_string(h, trim(r.disease));
    h = temporal_epi_fnv1a_append_string(h, trim(r.cie10));
    h = temporal_epi_fnv1a_append_string(h, trim(r.jurisdiction));
    h = temporal_epi_fnv1a_append_string(h, trim(r.period));
    h = temporal_epi_fnv1a_append_string(h, trim(r.sex));
    h = temporal_epi_fnv1a_append_string(h, std::to_string(r.value));
    return h ? h : 1ull;
}

std::string temporal_epi_exact_key_string(const TemporalEpidemiologyRecord& r) {
    // Clave exacta estable para deduplicación intra-corrida y reanudación: evita
    // reescanear el IXIPTLAH completo por cada página, que era el origen del
    // enlentecimiento acumulativo. El separador 0x1F no aparece en los campos
    // sanitizados y mantiene comparación byte a byte, sin depender de std::hash ABI.
    std::string k;
    k.reserve(normalized_entity_code(r.entity).size() + trim(r.disease).size() +
              trim(r.cie10).size() + trim(r.jurisdiction).size() +
              trim(r.period).size() + trim(r.sex).size() + 96u);
    k += normalized_entity_code(r.entity); k.push_back('\x1f');
    k += std::to_string(r.year); k.push_back('\x1f');
    k += std::to_string(r.epi_week); k.push_back('\x1f');
    k += std::to_string(r.page); k.push_back('\x1f');
    k += trim(r.disease); k.push_back('\x1f');
    k += trim(r.cie10); k.push_back('\x1f');
    k += trim(r.jurisdiction); k.push_back('\x1f');
    k += trim(r.period); k.push_back('\x1f');
    k += trim(r.sex); k.push_back('\x1f');
    k += std::to_string(r.value);
    return k;
}

void temporal_load_existing_epidemiology_keys(const fs::path& target,
                                            TemporalEpiDedupeHashSet& keys,
                                            const std::vector<TemporalEpidemiologyRecord>* scope_rows = nullptr) {
    if (target.empty()) return;
    std::error_code ec;
    if (!fs::exists(target, ec) || ec || file_size_or_zero(target) <= 16) return;

    auto consume_payload = [&](std::istream& in) {
        return read_epidemiology_payload_v1(in, [&](const TemporalEpidemiologyRecord& r) {
            keys.insert(temporal_epi_exact_key_hash(r));
            return true;
        });
    };

    if (scope_rows && !scope_rows->empty()) {
        std::vector<std::uint64_t> layer_hashes;
        layer_hashes.reserve(scope_rows->size());
        std::uint64_t begin_key = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t end_key = 0;
        for (const TemporalEpidemiologyRecord& r : *scope_rows) {
            layer_hashes.push_back(ixiptlah_layer_hash(temporal_epidemiology_element_key(r.disease, r.cie10)));
            const std::uint64_t k = temporal_epi_time_key(r.year, r.epi_week);
            if (k != 0ull) { begin_key = std::min(begin_key, k); end_key = std::max<std::uint64_t>(end_key, k + 1ull); }
        }
        std::sort(layer_hashes.begin(), layer_hashes.end());
        layer_hashes.erase(std::unique(layer_hashes.begin(), layer_hashes.end()), layer_hashes.end());
        if (!layer_hashes.empty() && begin_key != std::numeric_limits<std::uint64_t>::max() && end_key > begin_key) {
            bool read_any = false;
            for (std::uint32_t schema : {1u}) {
                ixiptlah_read_selected_records_tagged_temporal_exact(target,
                    IxiptlahRecordType::MonthlyEpidemiologyBatch, schema,
                    layer_hashes, false, begin_key, end_key,
                    [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
                        read_any = true;
                        return consume_payload(in);
                    });
            }
            if (read_any) return;
        }
    }

    ixiptlah_read_selected_records(target,
        [](IxiptlahRecordType type, std::uint32_t schema) {
            return type == IxiptlahRecordType::MonthlyEpidemiologyBatch && (schema == 1);
        },
        [&](IxiptlahRecordType, std::uint32_t, std::istream& in) {
            return consume_payload(in);
        });
}

std::vector<TemporalEpidemiologyRecord> temporal_filter_new_epidemiology_rows(
    const fs::path& target,
    const std::vector<TemporalEpidemiologyRecord>& rows) {

    if (rows.empty()) return {};

    std::vector<TemporalEpidemiologyRecord> filtered;
    filtered.reserve(rows.size());

    const std::string target_key = path_utf8(target.lexically_normal());
    std::shared_ptr<TemporalEpiDedupeHashSet> slot;
    bool must_load_existing = false;

    {
        std::lock_guard<std::mutex> lock(temporal_epi_dedupe_cache_mu());
        auto& existing = temporal_epi_dedupe_cache()[target_key];
        if (existing) {
            slot = existing;
        } else {
            must_load_existing = true;
        }
    }

    if (must_load_existing) {
        auto loaded = std::make_shared<TemporalEpiDedupeHashSet>();
        loaded->reserve(std::max<size_t>(rows.size() * 2u + 257u, 2048u));
        // Carga histórica fuera del mutex global: si el shard ya creció mucho,
        // escanear sus EPI1/EPI1C no debe congelar todos los demás escritores ni
        // el visor vivo. Sólo la instalación final y las inserciones calientes
        // quedan protegidas.
        temporal_load_existing_epidemiology_keys(target, *loaded, &rows);
        loaded->reserve(loaded->size() + rows.size() * 2u + 257u);

        std::lock_guard<std::mutex> lock(temporal_epi_dedupe_cache_mu());
        auto& installed = temporal_epi_dedupe_cache()[target_key];
        if (!installed) {
            installed = std::move(loaded);
            // Cota dura contra corridas largas y miles de shards: el caché es una
            // ventana de aceleración, no almacenamiento canónico. Si se excede, se
            // libera memoria y cada enfermedad se rehidrata bajo demanda.
            if (temporal_epi_dedupe_cache().size() > 512u) {
                const auto keep = installed;
                temporal_epi_dedupe_cache().clear();
                temporal_epi_dedupe_cache()[target_key] = keep;
            }
        }
        slot = installed;
    }

    if (!slot) return filtered;

    {
        std::lock_guard<std::mutex> lock(temporal_epi_dedupe_cache_mu());
        slot->reserve(slot->size() + rows.size() * 2u + 257u);
        for (const TemporalEpidemiologyRecord& r : rows) {
            if (r.value == 0) continue;
            if (slot->insert(temporal_epi_exact_key_hash(r))) filtered.push_back(r);
        }
    }
    return filtered;
}

bool temporal_append_epidemiology_render_delta_inline(const fs::path& target,
                                                      const std::vector<TemporalEpidemiologyRecord>& records) {
    if (target.empty() || records.empty()) return false;

    #pragma pack(push, 1)
    struct PackedRow {
        std::uint16_t week = 0;
        std::uint16_t jurisdiction = 0;
        std::uint16_t disease = 0;
        std::uint8_t distribution = 0;
        std::int32_t value = 0;
    };
    #pragma pack(pop)
    static_assert(sizeof(PackedRow) == 11, "EPI delta V1 mantiene filas compactas de 11 bytes");

    std::vector<std::uint32_t> week_keys;
    std::vector<std::pair<std::string, std::string>> jurisdictions;
    std::vector<std::pair<std::string, std::string>> disease_pairs;
    std::vector<PackedRow> packed;
    week_keys.reserve(4u);
    jurisdictions.reserve(32u);
    disease_pairs.reserve(4u);
    packed.reserve(records.size());

    std::unordered_map<std::uint32_t, std::uint16_t> week_index;
    std::unordered_map<std::string, std::uint16_t> jurisdiction_index;
    std::unordered_map<std::string, std::uint16_t> disease_index;
    week_index.reserve(8u);
    jurisdiction_index.reserve(64u);
    disease_index.reserve(8u);

    auto intern_week = [&](std::uint32_t key) -> std::uint16_t {
        const auto it = week_index.find(key);
        if (it != week_index.end()) return it->second;
        if (key == 0u || week_keys.size() > static_cast<size_t>(std::numeric_limits<std::uint16_t>::max())) return std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t pos = static_cast<std::uint16_t>(week_keys.size());
        week_keys.push_back(key);
        week_index.emplace(key, pos);
        return pos;
    };
    auto intern_pair = [](std::unordered_map<std::string, std::uint16_t>& index,
                          std::vector<std::pair<std::string, std::string>>& values,
                          const std::string& id,
                          const std::string& name) -> std::uint16_t {
        const auto it = index.find(id);
        if (it != index.end()) return it->second;
        if (id.empty() || values.size() > static_cast<size_t>(std::numeric_limits<std::uint16_t>::max())) return std::numeric_limits<std::uint16_t>::max();
        const std::uint16_t pos = static_cast<std::uint16_t>(values.size());
        values.emplace_back(id, name.empty() ? id : name);
        index.emplace(values.back().first, pos);
        return pos;
    };

    std::string snapshot_layer_key;
    bool snapshot_layer_mixed = false;
    std::uint64_t snapshot_temporal_key = 0ull;
    for (const TemporalEpidemiologyRecord& r : records) {
        if (r.value == 0 || r.year < 1800 || r.year > 2300 || r.epi_week < 1 || r.epi_week > 53) continue;
        const std::uint8_t dist = temporal_epi_distribution_code(r.period, r.sex);
        if (dist == 0u) continue;
        std::string disease_id, disease_name, disease_group;
        temporal_epi_render_identity(r, disease_id, disease_name, disease_group);
        (void)disease_group;
        if (disease_id.empty()) continue;
        const std::string jur_id = temporal_jurisdiction_id_inline(r.jurisdiction);
        if (jur_id.empty()) continue;
        const std::uint64_t row_temporal_key64 = temporal_epi_time_key(r.year, r.epi_week);
        const std::uint32_t row_temporal_key32 = static_cast<std::uint32_t>(row_temporal_key64);
        if (snapshot_temporal_key == 0ull) snapshot_temporal_key = row_temporal_key64;
        else if (snapshot_temporal_key != row_temporal_key64) snapshot_temporal_key = 0ull;
        if (snapshot_layer_key.empty()) snapshot_layer_key = disease_id;
        else if (snapshot_layer_key != disease_id) snapshot_layer_mixed = true;

        PackedRow row;
        row.week = intern_week(row_temporal_key32);
        row.jurisdiction = intern_pair(jurisdiction_index, jurisdictions, jur_id, r.jurisdiction.empty() ? jur_id : r.jurisdiction);
        row.disease = intern_pair(disease_index, disease_pairs, disease_id, disease_name);
        if (row.week == std::numeric_limits<std::uint16_t>::max() ||
            row.jurisdiction == std::numeric_limits<std::uint16_t>::max() ||
            row.disease == std::numeric_limits<std::uint16_t>::max()) return false;
        row.distribution = dist;
        row.value = static_cast<std::int32_t>(std::clamp<std::int64_t>(r.value, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
        packed.push_back(row);
    }
    if (week_keys.empty() || jurisdictions.empty() || disease_pairs.empty() || packed.empty()) return false;
    if (week_keys.size() > 65535u || jurisdictions.size() > 65535u || disease_pairs.size() > 65535u || packed.size() > 10000000u) return false;

    const auto writer = [&](std::ostream& out) {
        const std::uint16_t wc = static_cast<std::uint16_t>(week_keys.size());
        const std::uint16_t jc = static_cast<std::uint16_t>(jurisdictions.size());
        const std::uint16_t dc = static_cast<std::uint16_t>(disease_pairs.size());
        const std::uint32_t rc = static_cast<std::uint32_t>(packed.size());
        if (!temporal_epi_render_write_string(out, "TLALPOWA_EPI_DELTA_V1") ||
            !temporal_epi_render_write_pod(out, wc) ||
            !temporal_epi_render_write_pod(out, jc) ||
            !temporal_epi_render_write_pod(out, dc) ||
            !temporal_epi_render_write_pod(out, rc)) return false;
        for (const std::uint32_t key : week_keys) if (!temporal_epi_render_write_pod(out, key)) return false;
        for (const auto& j : jurisdictions) {
            if (!temporal_epi_render_write_string(out, j.first) || !temporal_epi_render_write_string(out, j.second)) return false;
        }
        for (const auto& d : disease_pairs) {
            if (!temporal_epi_render_write_string(out, d.first) || !temporal_epi_render_write_string(out, d.second)) return false;
        }
        const std::uint64_t bytes = static_cast<std::uint64_t>(packed.size()) * sizeof(PackedRow);
        if (bytes > 256ull * 1024ull * 1024ull) return false;
        out.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(bytes));
        return static_cast<bool>(out);
    };
    if (snapshot_layer_mixed) snapshot_layer_key.clear();
    if (!snapshot_layer_key.empty()) {
        if (ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, snapshot_layer_key, snapshot_temporal_key, writer)) return true;
        ixiptlah_close_all();
        return ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, snapshot_layer_key, snapshot_temporal_key, writer);
    }
    if (ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, 0ull, snapshot_temporal_key, writer)) return true;
    ixiptlah_close_all();
    return ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::EpidemiologyRenderSnapshot, 1, 0ull, snapshot_temporal_key, writer);
}




void temporal_append_epidemiology_record(const fs::path& root,
                                        const std::string& entity,


                                        int year,


                                        int epi_week,


                                        int page,
                                        const std::string& disease,
                                        const std::string& cie10,

                                        const std::string& jurisdiction,
                                        const std::string& period,
                                        const std::string& sex,

                                        int64_t value) {


    if (root.empty()) return;


    const std::string ent = normalized_entity_code(entity);


    const fs::path target = temporal_unified_weekly_files_enabled()
        ? temporal_weekly_ixiptlah_file_in_root(root, year, epi_week)
        : temporal_decade_ixiptlah_file_in_root(root, TemporalIxiptlahCategory::Epidemiological, year);
    temporal_prepare_element_ixiptlah_for_fresh_import(target);


    TemporalEpidemiologyRecord r;
    r.entity = ent;


    r.year = year;


    r.epi_week = epi_week;
    r.page = page;
    r.disease = disease;

    r.cie10 = cie10;
    r.jurisdiction = jurisdiction;
    r.period = period;

    r.sex = sex;
    r.value = value;

    if (!temporal_epi_record_valid_for_ixiptlah(r)) return;

    std::vector<TemporalEpidemiologyRecord> one{r};
    one = temporal_filter_new_epidemiology_rows(target, one);
    if (one.empty()) return;

    auto write_payload = [&](std::ostream& out) {
        return write_epidemiology_payload_v1(out, one);
    };


    const std::string layer_key = temporal_epidemiology_element_key(disease, cie10);
    const std::uint64_t temporal_key = temporal_epi_time_key(year, epi_week);
    if (!ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::MonthlyEpidemiologyBatch, 1, layer_key, temporal_key, write_payload)) {




        ixiptlah_close_all();


        if (!ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::MonthlyEpidemiologyBatch, 1, layer_key, temporal_key, write_payload)) {


            throw std::runtime_error("No se pudo escribir observacion epidemiologica ixiptlah: " + path_utf8(target));
        }
    }
    // Representación viva append-only: la fila recién aceptada se escribe como
    // delta preformado dentro del mismo IXIPTLAH, sin reabrir ni reescribir el
    // historial de la enfermedad.
    if (!temporal_append_epidemiology_render_delta_inline(target, one)) ixiptlah_flush_all();
}



void temporal_append_epidemiology_records_batch_impl(const fs::path& root,


                                                     const std::vector<TemporalEpidemiologyRecord>& records,


                                                     bool exact_root) {


    (void)exact_root;
    if (root.empty() || records.empty()) return;

    struct EpiBatchKey {
        fs::path target;
        int year = 0;
        int week = 0;
        std::string layer_key;

        bool operator<(const EpiBatchKey& other) const {
            if (target != other.target) return target < other.target;
            if (year != other.year) return year < other.year;
            if (week != other.week) return week < other.week;
            return layer_key < other.layer_key;
        }
    };
    std::map<EpiBatchKey, std::vector<TemporalEpidemiologyRecord>> by_layer_year_week;
    std::set<fs::path> modified_targets;
    for (TemporalEpidemiologyRecord r : records) {
        r.entity = normalized_entity_code(r.entity);
        if (r.value == 0) continue;
        if (!temporal_epi_record_valid_for_ixiptlah(r)) continue;
        const fs::path target = temporal_unified_weekly_files_enabled()
            ? temporal_weekly_ixiptlah_file_in_root(root, r.year, r.epi_week)
            : temporal_decade_ixiptlah_file_in_root(root, TemporalIxiptlahCategory::Epidemiological, r.year);
        const std::string layer_key = temporal_epidemiology_element_key(r.disease, r.cie10);
        by_layer_year_week[{target, r.year, r.epi_week, layer_key}].push_back(std::move(r));
    }

    for (const auto& [key, rows] : by_layer_year_week) {
        const fs::path& target = key.target;
        if (rows.empty()) continue;
        temporal_prepare_element_ixiptlah_for_fresh_import(target);
        const std::vector<TemporalEpidemiologyRecord> unique_rows = temporal_filter_new_epidemiology_rows(target, rows);
        if (unique_rows.empty()) continue;
        size_t offset = 0;
        while (offset < unique_rows.size()) {
            const size_t n = std::min<size_t>(unique_rows.size() - offset, 250000u);
            std::vector<TemporalEpidemiologyRecord> view;
            view.reserve(n);
            for (size_t i = 0; i < n; ++i) view.push_back(unique_rows[offset + i]);
            auto write_payload = [&](std::ostream& out) {
                // Payload compacto por semana/capa: una gráfica semanal no reagrupa
                // meses ni reabre enfermedades ajenas; el encabezado V1 salta payload.
                return write_epidemiology_payload_v1(out, view);
            };
            const std::uint64_t temporal_key = temporal_epi_time_key(key.year, key.week);
            if (!ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::MonthlyEpidemiologyBatch, 1, key.layer_key, temporal_key, write_payload)) {
                ixiptlah_close_all();
                if (!ixiptlah_append_record_tagged_temporal_deferred_flush(target, IxiptlahRecordType::MonthlyEpidemiologyBatch, 1, key.layer_key, temporal_key, write_payload)) {
                    throw std::runtime_error("No se pudo escribir lote epidemiologico ixiptlah V1: " + path_utf8(target));
                }
            }
            if (!temporal_append_epidemiology_render_delta_inline(target, view)) ixiptlah_flush_all();
            modified_targets.insert(target);
            offset += n;
        }
    }

    // Cada lote anterior ya dejó un delta de representación append-only junto a su EPI1.
    // Reescribir snapshots completos aquí reintroduciría una segunda pasada por enfermedad
    // y un coste O(n²) conforme crecen los boletines; queda prohibido en importación viva.
    (void)modified_targets;
}



void temporal_append_epidemiology_records_batch(const fs::path& root,



                                                const std::vector<TemporalEpidemiologyRecord>& records) {


    temporal_append_epidemiology_records_batch_impl(root, records, false);
}



void temporal_append_epidemiology_records_batch_exact_root(const fs::path& root,



                                                           const std::vector<TemporalEpidemiologyRecord>& records) {




    temporal_append_epidemiology_records_batch_impl(root, records, true);
}




void temporal_append_source_inventory_record(const fs::path& root, const std::map<std::string, std::string>& fields) {


    if (root.empty()) return;




    append_ixiptlah_fields_record(root, IxiptlahRecordType::MonthlySourceInventory, fields);
}




void temporal_append_record(const fs::path& root, const std::map<std::string, std::string>& fields) {


    if (root.empty()) return;


    if (is_source_inventory_fields(fields)) {


        temporal_append_source_inventory_record(root, fields);


        return;
    }


    const bool atmosphere = is_atmosphere_fields(fields);
    if (!atmosphere) {
        auto get = [&](const char* key) -> std::string {
            const auto it = fields.find(key);
            return it == fields.end() ? std::string{} : trim(it->second);
        };
        std::string v = get("value");
        if (v.empty()) v = get("casos");
        if (v.empty()) v = get("count");
        bool numeric_zero = !v.empty();
        bool seen_digit = false;
        bool seen_decimal = false;
        for (char c : v) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (c == '+' || c == '-' || c == ',' || std::isspace(uc)) continue;
            if (c == '.') {
                if (seen_decimal) { numeric_zero = false; break; }
                seen_decimal = true;
                continue;
            }
            if (!std::isdigit(uc)) { numeric_zero = false; break; }
            if (c != '0') numeric_zero = false;
            seen_digit = true;
        }
        // Epidemiología no debe poblar semanas con ceros explícitos, incluso si
        // llegan como 0, 0.0, 000 o con separadores: un cero de boletín significa
        // ausencia de fila representable, no un punto arrastrable por historial.
        if (seen_digit && numeric_zero) return;
    }


    append_ixiptlah_fields_record(root,


        atmosphere ? IxiptlahRecordType::AtmosphereMeasurement : IxiptlahRecordType::EpidemiologyObservation,


        fields);
}



void temporal_append_atmosphere_measurement(const fs::path& root,


                                           const std::string& date,


                                           const std::string& hour,


                                           int year,
                                           const std::string& domain,
                                           const std::string& source_id,


                                           const std::string& source_file,


                                           const std::string& source_path,
                                           const std::string& pollutant,


                                           const std::string& station_id,
                                           const std::string& station,


                                           double lon,


                                           double lat,
                                           double alt,
                                           const std::string& metric,

                                           double value,
                                           const std::string& unit) {


    if (root.empty() || year <= 0) return;


    const std::string lon_s = double_compact(lon);


    const std::string lat_s = double_compact(lat);


    const std::string alt_s = double_compact(alt);
    const std::string value_s = double_compact(value);


    append_ixiptlah_fields_record(root, IxiptlahRecordType::AtmosphereMeasurement, {


        {"record_type", "atmosfera_medicion"}, {"date", date}, {"hour", hour}, {"year", std::to_string(year)},


        {"domain", domain}, {"source_id", source_id}, {"source_file", source_file}, {"source_path", source_path},


        {"pollutant", pollutant}, {"station_id", normalized_three_code(station_id, "ATM")}, {"station", station},


        {"lon", lon_s}, {"lat", lat_s}, {"alt", alt_s}, {"metric", metric}, {"value", value_s},
        {"value_text", value_s}, {"unit", unit}, {"count", "1"}, {"min", value_s}, {"max", value_s}
    });
}




double tlalpowa_nan() { return std::numeric_limits<double>::quiet_NaN(); }

bool temporal_unit_key_same(const std::string& a, const std::string& b) {
    return normalize_key(a) == normalize_key(b);
}

double temporal_temperature_c(double value, const std::string& unit) {
    const std::string u = normalize_key(unit);
    if (u.empty() || u == "c" || u == "celsius" || u == "deg c" || u == "degree c" || u == "degrees c" || u == "°c") return value;
    if (u == "k" || u == "kelvin") return value - 273.15;
    if (u == "f" || u == "fahrenheit" || u == "deg f" || u == "°f") return (value - 32.0) * 5.0 / 9.0;
    return tlalpowa_nan();
}

double temporal_rh_percent(double value, const std::string& unit) {
    const std::string u = normalize_key(unit);
    if (u.empty() || u == "%" || u == "percent" || u == "porcentaje") return std::clamp(value, 0.0, 100.0);
    if ((u == "fraction" || u == "fraccion" || u == "fracción") && value >= 0.0 && value <= 1.0) return value * 100.0;
    return tlalpowa_nan();
}

double temporal_wind_speed_ms(double value, const std::string& unit) {
    const std::string u = normalize_key(unit);
    if (u.empty() || u == "m s" || u == "m/s" || u == "mps" || u == "ms-1" || u == "m s-1") return value;
    if (u == "km h" || u == "km/h" || u == "kph") return value / 3.6;
    if (u == "mph") return value * 0.44704;
    if (u == "kn" || u == "kt" || u == "knot" || u == "knots") return value * 0.514444;
    return tlalpowa_nan();
}

double temporal_pressure_hpa(double value, const std::string& unit) {
    const std::string u = normalize_key(unit);
    if (u.empty() || u == "hpa" || u == "mb" || u == "mbar") return value;
    if (u == "pa") return value / 100.0;
    if (u == "kpa") return value * 10.0;
    if (u == "mmhg" || u == "torr") return value * 1.333223684;
    return tlalpowa_nan();
}

double temporal_direction_degrees(double value, const std::string& unit) {
    const std::string u = normalize_key(unit);
    if (u.empty() || u == "deg" || u == "degree" || u == "degrees" || u == "grados" || u == "°") {
        double d = std::fmod(value, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    }
    if (u == "rad" || u == "radian" || u == "radians") {
        double d = std::fmod(value * 180.0 / 3.14159265358979323846, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    }
    return tlalpowa_nan();
}

struct TemporalDerivedValue {
    double value = 0.0;
    std::string unit;
};

struct TemporalPrimitiveValue {
    double value = 0.0;
    std::string unit;
    std::uint32_t station_index = 0;
    std::uint8_t day = 1;
    std::uint8_t hour = 0;
    std::uint8_t minute = 0;
};

std::string temporal_sample_instant_key(const TemporalAtmospherePackedSample& sample) {
    return std::to_string(sample.station_index) + "|" + std::to_string(static_cast<int>(sample.day)) + "|" +
           std::to_string(static_cast<int>(sample.hour)) + "|" + std::to_string(static_cast<int>(sample.minute));
}

bool temporal_same_positive_unit(const TemporalPrimitiveValue& a, const TemporalPrimitiveValue& b) {
    return std::isfinite(a.value) && std::isfinite(b.value) && temporal_unit_key_same(a.unit, b.unit);
}

bool temporal_add_if_absent(std::map<std::string, TemporalDerivedValue>& out,
                            const std::string& key,
                            double value,
                            std::string unit) {
    if (key.empty() || !std::isfinite(value)) return false;
    if (out.find(key) != out.end()) return false;
    out[key] = TemporalDerivedValue{value, std::move(unit)};
    return true;
}

void temporal_materialize_secondary_values_for_instant(const std::map<std::string, TemporalPrimitiveValue>& v,
                                                       std::map<std::string, TemporalDerivedValue>& out) {
    // Sólo se materializan secundarios físicos o químicos defensibles desde
    // primarios simultáneos. Prohibido añadir razones oportunistas por mera
    // disponibilidad de columnas; no se interpolan satélites contra estaciones.
    auto has = [&](const char* key) { return v.find(key) != v.end() && std::isfinite(v.find(key)->second.value); };
    auto get = [&](const char* key) -> const TemporalPrimitiveValue* {
        const auto it = v.find(key);
        return it == v.end() ? nullptr : &it->second;
    };
    struct RatioValue { double value = 0.0; std::string unit; bool ok = false; };
    auto get_any = [&](const char* key) -> RatioValue {
        if (const auto* x = get(key)) return RatioValue{x->value, x->unit, std::isfinite(x->value)};
        const auto it = out.find(key);
        if (it != out.end()) return RatioValue{it->second.value, it->second.unit, std::isfinite(it->second.value)};
        return {};
    };
    auto add_ratio = [&](const char* dst, const char* a_key, const char* b_key) {
        const RatioValue a = get_any(a_key);
        const RatioValue b = get_any(b_key);
        if (!a.ok || !b.ok || !temporal_unit_key_same(a.unit, b.unit) || std::fabs(b.value) <= 1.0e-12) return;
        temporal_add_if_absent(out, dst, a.value / b.value, "ratio");
    };
    auto add_ratio_unitful = [&](const char* dst, const char* a_key, const char* b_key) {
        const RatioValue a = get_any(a_key);
        const RatioValue b = get_any(b_key);
        if (!a.ok || !b.ok || std::fabs(b.value) <= 1.0e-12) return;
        std::string unit = a.unit.empty() ? "1" : a.unit;
        unit += "/";
        unit += b.unit.empty() ? "1" : b.unit;
        temporal_add_if_absent(out, dst, a.value / b.value, unit);
    };
    (void)add_ratio_unitful;
    auto add_sum_same_unit = [&](const char* dst, const std::initializer_list<const char*> keys) {
        std::string unit;
        double sum = 0.0;
        bool ok = true, any = false;
        for (const char* k : keys) {
            const auto* x = get(k);
            if (!x) { ok = false; break; }
            if (!any) { unit = x->unit; any = true; }
            else if (!temporal_unit_key_same(unit, x->unit)) { ok = false; break; }
            sum += x->value;
        }
        if (ok && any) temporal_add_if_absent(out, dst, sum, unit);
    };

    if (has("tmp") && has("rh")) {
        const auto* t0 = get("tmp"); const auto* rh0 = get("rh");
        const double tc = temporal_temperature_c(t0->value, t0->unit);
        const double rh = temporal_rh_percent(rh0->value, rh0->unit);
        if (std::isfinite(tc) && std::isfinite(rh) && tc > -90.0 && tc < 70.0 && rh >= 0.0 && rh <= 100.0) {
            const double es_hpa = 6.112 * std::exp((17.67 * tc) / (tc + 243.5));
            const double e_hpa = es_hpa * rh / 100.0;
            const double ln_arg = e_hpa / 6.112;
            if (ln_arg > 0.0) {
                const double g = std::log(ln_arg);
                temporal_add_if_absent(out, "dewpoint", 243.5 * g / (17.67 - g), "C");
            }
            temporal_add_if_absent(out, "sat_vapor_pressure", es_hpa, "hPa");
            temporal_add_if_absent(out, "vapor_pressure", e_hpa, "hPa");
            temporal_add_if_absent(out, "vpd", std::max(0.0, (es_hpa - e_hpa) / 10.0), "kPa");
            temporal_add_if_absent(out, "abs_humidity", 216.7 * e_hpa / (tc + 273.15), "g/m3");
            const double wet = tc * std::atan(0.151977 * std::sqrt(rh + 8.313659)) +
                               std::atan(tc + rh) - std::atan(rh - 1.676331) +
                               0.00391838 * std::pow(rh, 1.5) * std::atan(0.023101 * rh) - 4.686035;
            temporal_add_if_absent(out, "wet_bulb", wet, "C");
        }
    }

    if (has("tmp") && has("rh") && has("pa")) {
        const double tc = temporal_temperature_c(get("tmp")->value, get("tmp")->unit);
        const double rh = temporal_rh_percent(get("rh")->value, get("rh")->unit);
        const double p_hpa = temporal_pressure_hpa(get("pa")->value, get("pa")->unit);
        if (std::isfinite(tc) && std::isfinite(rh) && std::isfinite(p_hpa) && p_hpa > 100.0) {
            const double es_hpa = 6.112 * std::exp((17.67 * tc) / (tc + 243.5));
            const double e_hpa = std::min(p_hpa * 0.99, es_hpa * rh / 100.0);
            temporal_add_if_absent(out, "mixing_ratio", 621.97 * e_hpa / std::max(1.0e-9, p_hpa - e_hpa), "g/kg");
            temporal_add_if_absent(out, "specific_humidity", 1000.0 * 0.622 * e_hpa / std::max(1.0e-9, p_hpa - 0.378 * e_hpa), "g/kg");
            const double tk = tc + 273.15;
            const double air_density = (p_hpa * 100.0) / (287.05 * tk);
            temporal_add_if_absent(out, "air_density", air_density, "kg/m3");
            temporal_add_if_absent(out, "potential_temperature", tk * std::pow(1000.0 / p_hpa, 0.286), "K");
            const RatioValue q = get_any("specific_humidity");
            if (q.ok) {
                const double q_kgkg = q.unit == "g/kg" ? q.value / 1000.0 : q.value;
                if (q_kgkg >= 0.0 && q_kgkg < 0.2) temporal_add_if_absent(out, "virtual_temperature", tk * (1.0 + 0.61 * q_kgkg), "K");
            }
        }
    }

    if (has("u10") && has("v10")) {
        const auto* u = get("u10"); const auto* vv = get("v10");
        if (temporal_unit_key_same(u->unit, vv->unit)) {
            const double wsp = std::sqrt(u->value * u->value + vv->value * vv->value);
            double wdr = std::atan2(-u->value, -vv->value) * 180.0 / 3.14159265358979323846;
            wdr = std::fmod(wdr + 360.0, 360.0);
            temporal_add_if_absent(out, "wsp", wsp, u->unit.empty() ? "m/s" : u->unit);
            temporal_add_if_absent(out, "wdr", wdr, "deg");
        }
    }
    if (has("wsp") && has("wdr")) {
        const double wsp = temporal_wind_speed_ms(get("wsp")->value, get("wsp")->unit);
        const double wdr = temporal_direction_degrees(get("wdr")->value, get("wdr")->unit);
        if (std::isfinite(wsp) && std::isfinite(wdr)) {
            const double rad = wdr * 3.14159265358979323846 / 180.0;
            temporal_add_if_absent(out, "u10", -wsp * std::sin(rad), "m/s");
            temporal_add_if_absent(out, "v10", -wsp * std::cos(rad), "m/s");
        }
    }

    if (has("pm10") && has("pm25") && temporal_same_positive_unit(*get("pm10"), *get("pm25"))) {
        temporal_add_if_absent(out, "pmco", std::max(0.0, get("pm10")->value - get("pm25")->value), get("pm10")->unit);
    }
    add_sum_same_unit("nox", {"no", "no2"});
    add_sum_same_unit("tc", {"ec", "oc"});
    add_sum_same_unit("inorg_aer", {"so4", "no3a"});
    if (has("ben") && has("tol") && has("xyl") && temporal_unit_key_same(get("ben")->unit, get("tol")->unit) && temporal_unit_key_same(get("ben")->unit, get("xyl")->unit)) {
        temporal_add_if_absent(out, "btex", get("ben")->value + get("tol")->value + get("xyl")->value, get("ben")->unit);
    }
    add_ratio("pm25_pm10", "pm25", "pm10");
    add_ratio("pmco_pm10", "pmco", "pm10");
    add_ratio("no2_nox", "no2", "nox");
    add_ratio("no_nox", "no", "nox");
    add_ratio("no_no2", "no", "no2");
    add_ratio("oc_ec", "oc", "ec");
    add_ratio("ec_tc", "ec", "tc");
    add_ratio("oc_tc", "oc", "tc");
    add_ratio("bc_pm25", "bc", "pm25");
    add_ratio("ec_pm25", "ec", "pm25");
    add_ratio("oc_pm25", "oc", "pm25");
    add_ratio("so4_no3a", "so4", "no3a");
    add_ratio("tol_ben", "tol", "ben");
    add_ratio("xyl_ben", "xyl", "ben");
    add_ratio("btex_ben", "btex", "ben");
    add_ratio("o3_no2", "o3", "no2");
    add_ratio("hcho_no2", "hcho", "no2");
    add_ratio("co_no2", "co", "no2");
    add_sum_same_unit("ox", {"o3", "no2"});
    add_ratio("hcho_nox", "hcho", "nox");
    add_ratio("ec_oc", "ec", "oc");
}

void temporal_materialize_atmosphere_secondaries(std::vector<std::string>& pollutants,
                                                 std::vector<std::string>& units,
                                                 std::vector<TemporalAtmospherePackedSample>& samples,
                                                 std::uint32_t station_count) {
    if (pollutants.empty() || units.empty() || samples.empty()) return;
    auto ensure_pollutant = [&](const std::string& key) -> std::uint32_t {
        const std::string canonical = temporal_atmospheric_layer_key(key);
        for (std::uint32_t i = 0; i < pollutants.size(); ++i) {
            if (temporal_atmospheric_layer_key(pollutants[i]) == canonical) return i;
        }
        if (pollutants.size() >= std::numeric_limits<std::uint32_t>::max()) return 0;
        pollutants.push_back(canonical);
        return static_cast<std::uint32_t>(pollutants.size() - 1);
    };
    auto ensure_unit = [&](const std::string& unit) -> std::uint32_t {
        for (std::uint32_t i = 0; i < units.size(); ++i) if (temporal_unit_key_same(units[i], unit)) return i;
        if (units.size() >= std::numeric_limits<std::uint32_t>::max()) return 0;
        units.push_back(unit);
        return static_cast<std::uint32_t>(units.size() - 1);
    };

    std::map<std::string, std::map<std::string, TemporalPrimitiveValue>> by_instant;
    for (const auto& sample : samples) {
        if (sample.station_index >= station_count || sample.pollutant_index >= pollutants.size() || sample.unit_index >= units.size()) continue;
        if (!std::isfinite(sample.value)) continue;
        const std::string key = temporal_atmospheric_layer_key(pollutants[sample.pollutant_index]);
        if (key.empty()) continue;
        by_instant[temporal_sample_instant_key(sample)][key] = TemporalPrimitiveValue{sample.value, units[sample.unit_index], sample.station_index, sample.day, sample.hour, sample.minute};
    }

    std::vector<TemporalAtmospherePackedSample> derived;
    derived.reserve(std::min<std::size_t>(samples.size(), 4096));
    for (const auto& [instant_key, values] : by_instant) {
        if (values.empty()) continue;
        const TemporalPrimitiveValue& base = values.begin()->second;
        std::map<std::string, TemporalDerivedValue> out;
        temporal_materialize_secondary_values_for_instant(values, out);
        for (const auto& [key, val] : out) {
            if (values.find(key) != values.end()) continue;
            TemporalAtmospherePackedSample s;
            s.day = base.day;
            s.hour = base.hour;
            s.minute = base.minute;
            s.station_index = base.station_index;
            s.pollutant_index = ensure_pollutant(key);
            s.unit_index = ensure_unit(val.unit);
            s.value = val.value;
            derived.push_back(s);
        }
    }
    samples.insert(samples.end(), derived.begin(), derived.end());
}


struct TemporalAtmosphereAggregateAcc {
    double sum = 0.0;
    double min_value = std::numeric_limits<double>::infinity();
    double max_value = -std::numeric_limits<double>::infinity();
    std::uint64_t count = 0;

    void add(double v) {
        if (!std::isfinite(v)) return;
        sum += v;
        min_value = std::min(min_value, v);
        max_value = std::max(max_value, v);
        ++count;
    }

    double mean() const { return count == 0 ? tlalpowa_nan() : sum / static_cast<double>(count); }
};

void temporal_append_atmosphere_layer_preformed_records(const fs::path& target,
                                                        const std::string& layer_key,
                                                        const std::string& pollutant,
                                                        const std::string& source_id,
                                                        const std::string& source_file,
                                                        const std::string& source_path,
                                                        const std::string& domain,
                                                        int year,
                                                        int month,
                                                        const std::vector<TemporalAtmospherePackedStation>& stations,
                                                        const std::vector<std::string>& units,
                                                        const std::vector<TemporalAtmospherePackedSample>& samples)
{
    if (target.empty() || layer_key.empty() || year <= 0 || month < 1 || month > 12 ||
        stations.empty() || units.empty() || samples.empty()) return;

    const int dim = temporal_week_file_days_in_month(year, month);
    if (dim <= 0 || dim > 31) return;

    struct LocalAcc {
        double sum = 0.0;
        double min_value = std::numeric_limits<double>::infinity();
        double max_value = -std::numeric_limits<double>::infinity();
        std::uint32_t count = 0;
        std::uint8_t day = 0;
        std::uint32_t station = 0;
        std::uint32_t unit = 0;

        void add(double value) {
            if (!std::isfinite(value)) return;
            sum += value;
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
            if (count != std::numeric_limits<std::uint32_t>::max()) ++count;
        }
    };

    std::vector<std::uint32_t> station_remap(stations.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<std::uint32_t> unit_remap(units.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<TemporalAtmospherePackedStation> local_stations;
    std::vector<std::string> local_units;
    local_stations.reserve(std::min<std::size_t>(stations.size(), samples.size()));
    local_units.reserve(std::min<std::size_t>(units.size(), 8u));

    std::unordered_map<std::uint64_t, std::size_t> slot;
    slot.reserve(std::min<std::size_t>(samples.size(), 65536u));
    std::vector<LocalAcc> accs;
    accs.reserve(std::min<std::size_t>(samples.size(), 4096u));
    std::uint64_t temporal_key = 0;

    for (const TemporalAtmospherePackedSample& sample : samples) {
        if (sample.station_index >= stations.size() || sample.unit_index >= units.size()) continue;
        if (sample.day < 1 || sample.day > dim || !std::isfinite(sample.value)) continue;

        std::uint32_t local_station = station_remap[static_cast<std::size_t>(sample.station_index)];
        if (local_station == std::numeric_limits<std::uint32_t>::max()) {
            if (local_stations.size() >= std::numeric_limits<std::uint32_t>::max()) continue;
            local_station = static_cast<std::uint32_t>(local_stations.size());
            station_remap[static_cast<std::size_t>(sample.station_index)] = local_station;
            local_stations.push_back(stations[static_cast<std::size_t>(sample.station_index)]);
        }

        std::uint32_t local_unit = unit_remap[static_cast<std::size_t>(sample.unit_index)];
        if (local_unit == std::numeric_limits<std::uint32_t>::max()) {
            if (local_units.size() >= std::numeric_limits<std::uint32_t>::max()) continue;
            local_unit = static_cast<std::uint32_t>(local_units.size());
            unit_remap[static_cast<std::size_t>(sample.unit_index)] = local_unit;
            local_units.push_back(units[static_cast<std::size_t>(sample.unit_index)]);
        }

        const std::uint64_t key = (static_cast<std::uint64_t>(sample.day) << 56) |
                                  (static_cast<std::uint64_t>(local_station) << 20) |
                                  static_cast<std::uint64_t>(local_unit & 0xFFFFFu);
        auto it = slot.find(key);
        if (it == slot.end()) {
            LocalAcc acc;
            acc.day = sample.day;
            acc.station = local_station;
            acc.unit = local_unit;
            acc.add(sample.value);
            const std::size_t idx = accs.size();
            accs.push_back(acc);
            slot.emplace(key, idx);
        } else {
            accs[it->second].add(sample.value);
        }
        temporal_key = std::max<std::uint64_t>(temporal_key,
            temporal_atmosphere_time_key(year, month, sample.day, 23u, 59u));
    }

    if (local_stations.empty() || local_units.empty() || accs.empty()) return;
    std::stable_sort(accs.begin(), accs.end(), [](const LocalAcc& a, const LocalAcc& b) {
        if (a.day != b.day) return a.day < b.day;
        if (a.station != b.station) return a.station < b.station;
        return a.unit < b.unit;
    });

    const std::uint32_t station_count = static_cast<std::uint32_t>(local_stations.size());
    const std::uint32_t unit_count = static_cast<std::uint32_t>(local_units.size());
    const std::uint64_t row_count = static_cast<std::uint64_t>(accs.size());

    // IXIPTLAH-GRAF schema 1: registro unitario, denso y preformado por capa.
    // El gráfico anual/diario lee medias ya agrupadas por día-estación-unidad y
    // evita rehidratar cada medición horaria cuando sólo necesita una curva.
    (void)ixiptlah_append_record_tagged_temporal_deferred_flush(target,
                                                 IxiptlahRecordType::AtmosphereGraphDailyStationBatch, 1,
                                                 layer_key, temporal_key,
                                                 [&](std::ostream& out) {
        if (!ixiptlah_write_string(out, source_id) ||
            !ixiptlah_write_string(out, source_file) ||
            !ixiptlah_write_string(out, source_path) ||
            !ixiptlah_write_string(out, domain) ||
            !ixiptlah_write_string(out, pollutant) ||
            !ixiptlah_write_value(out, year) ||
            !ixiptlah_write_value(out, month) ||
            !ixiptlah_write_value(out, station_count)) return false;

        for (std::uint32_t i = 0; i < station_count; ++i) {
            const auto& st = local_stations[static_cast<std::size_t>(i)];
            if (!ixiptlah_write_string(out, normalized_three_code(st.id, "ATM")) ||
                !ixiptlah_write_string(out, st.name) ||
                !ixiptlah_write_value(out, st.lon) ||
                !ixiptlah_write_value(out, st.lat) ||
                !ixiptlah_write_value(out, st.alt)) return false;
        }

        if (!ixiptlah_write_value(out, unit_count)) return false;
        for (std::uint32_t i = 0; i < unit_count; ++i) {
            if (!ixiptlah_write_string(out, local_units[static_cast<std::size_t>(i)])) return false;
        }

        if (!ixiptlah_write_value(out, row_count)) return false;
        for (const LocalAcc& acc : accs) {
            if (acc.count == 0 || acc.station >= station_count || acc.unit >= unit_count) return false;
            const float mean32 = static_cast<float>(acc.sum / static_cast<double>(acc.count));
            const float min32 = static_cast<float>(acc.min_value);
            const float max32 = static_cast<float>(acc.max_value);
            if (!ixiptlah_write_value(out, acc.day) ||
                !ixiptlah_write_value(out, acc.station) ||
                !ixiptlah_write_value(out, acc.unit) ||
                !ixiptlah_write_value(out, mean32) ||
                !ixiptlah_write_value(out, min32) ||
                !ixiptlah_write_value(out, max32) ||
                !ixiptlah_write_value(out, acc.count)) return false;
        }
        return true;
    });
}

void temporal_append_atmosphere_layer_hourly_preformed_records(const fs::path& target,
                                                              const std::string& layer_key,
                                                              const std::string& pollutant,
                                                              const std::string& source_id,
                                                              const std::string& source_file,
                                                              const std::string& source_path,
                                                              const std::string& domain,
                                                              int year,
                                                              int month,
                                                              const std::vector<TemporalAtmospherePackedStation>& stations,
                                                              const std::vector<std::string>& units,
                                                              const std::vector<TemporalAtmospherePackedSample>& samples) {
    if (target.empty() || layer_key.empty() || year <= 0 || month < 1 || month > 12 ||
        stations.empty() || units.empty() || samples.empty()) return;

    const int dim = temporal_week_file_days_in_month(year, month);
    if (dim <= 0 || dim > 31) return;

    struct LocalRow {
        std::uint8_t day = 0;
        std::uint16_t minute_of_day = 0;
        std::uint32_t station = 0;
        std::uint32_t unit = 0;
        float value = 0.0f;
    };

    std::vector<LocalRow> rows;
    rows.reserve(samples.size());
    std::uint64_t temporal_key = 0;
    bool rows_already_ordered = true;
    bool have_previous_row = false;
    LocalRow previous_row;
    const auto row_less = [](const LocalRow& a, const LocalRow& b) {
        if (a.day != b.day) return a.day < b.day;
        if (a.minute_of_day != b.minute_of_day) return a.minute_of_day < b.minute_of_day;
        if (a.station != b.station) return a.station < b.station;
        return a.unit < b.unit;
    };
    for (const TemporalAtmospherePackedSample& s : samples) {
        if (s.station_index >= stations.size() || s.unit_index >= units.size()) continue;
        if (s.day < 1 || s.day > dim || s.hour > 23u || s.minute > 59u || !std::isfinite(s.value)) continue;
        if (s.station_index > std::numeric_limits<std::uint32_t>::max() || s.unit_index > std::numeric_limits<std::uint32_t>::max()) continue;
        LocalRow r;
        r.day = s.day;
        r.minute_of_day = static_cast<std::uint16_t>(static_cast<unsigned>(s.hour) * 60u + static_cast<unsigned>(s.minute));
        r.station = s.station_index;
        r.unit = s.unit_index;
        r.value = static_cast<float>(s.value);
        if (have_previous_row && row_less(r, previous_row)) rows_already_ordered = false;
        previous_row = r;
        have_previous_row = true;
        rows.push_back(r);
        temporal_key = std::max<std::uint64_t>(temporal_key,
            temporal_atmosphere_time_key(year, month, s.day, s.hour, s.minute));
    }
    if (rows.empty()) return;

    if (!rows_already_ordered && rows.size() > 1u) {
        std::stable_sort(rows.begin(), rows.end(), row_less);
    }

    const std::uint32_t station_count = static_cast<std::uint32_t>(stations.size());
    const std::uint32_t unit_count = static_cast<std::uint32_t>(units.size());
    const std::uint64_t row_count = static_cast<std::uint64_t>(rows.size());

    // IXIPTLAH-HORA schema 1: serie puntual ya segregada por red/capa. La UI de
    // minuto latcheado lee sólo día+minuto+estación+valor; no abre el lote crudo
    // salvo ausencia del bloque preformado. El payload duplica bytes a propósito
    // para cambiar disco secuencial barato por respuesta gráfica inmediata.
    (void)ixiptlah_append_record_tagged_temporal_deferred_flush(target,
        IxiptlahRecordType::AtmosphereGraphHourlyStationBatch, 1, layer_key, temporal_key,
        [&](std::ostream& out) {
            if (!ixiptlah_write_string(out, source_id) ||
                !ixiptlah_write_string(out, source_file) ||
                !ixiptlah_write_string(out, source_path) ||
                !ixiptlah_write_string(out, domain) ||
                !ixiptlah_write_string(out, pollutant) ||
                !ixiptlah_write_value(out, year) ||
                !ixiptlah_write_value(out, month) ||
                !ixiptlah_write_value(out, station_count)) return false;

            for (std::uint32_t i = 0; i < station_count; ++i) {
                const auto& st = stations[static_cast<std::size_t>(i)];
                if (!ixiptlah_write_string(out, normalized_three_code(st.id, "ATM")) ||
                    !ixiptlah_write_string(out, st.name) ||
                    !ixiptlah_write_value(out, st.lon) ||
                    !ixiptlah_write_value(out, st.lat) ||
                    !ixiptlah_write_value(out, st.alt)) return false;
            }

            if (!ixiptlah_write_value(out, unit_count)) return false;
            for (std::uint32_t i = 0; i < unit_count; ++i) {
                if (!ixiptlah_write_string(out, units[static_cast<std::size_t>(i)])) return false;
            }

            if (!ixiptlah_write_value(out, row_count)) return false;
            for (const LocalRow& r : rows) {
                if (!ixiptlah_write_value(out, r.day) ||
                    !ixiptlah_write_value(out, r.minute_of_day) ||
                    !ixiptlah_write_value(out, r.station) ||
                    !ixiptlah_write_value(out, r.unit) ||
                    !ixiptlah_write_value(out, r.value)) return false;
            }
            return true;
        });
}

void temporal_append_atmosphere_layer_weekly_preformed_records(const fs::path& target,
                                                              const std::string& layer_key,
                                                              const std::string& pollutant,
                                                              const std::string& source_id,
                                                              const std::string& source_file,
                                                              const std::string& source_path,
                                                              const std::string& domain,
                                                              int year,
                                                              int month,
                                                              const std::vector<TemporalAtmospherePackedStation>& stations,
                                                              const std::vector<std::string>& units,
                                                              const std::vector<TemporalAtmospherePackedSample>& samples) {
    if (target.empty() || layer_key.empty() || year <= 0 || month < 1 || month > 12 ||
        stations.empty() || units.empty() || samples.empty()) return;

    const int dim = temporal_week_file_days_in_month(year, month);
    if (dim <= 0 || dim > 31) return;

    struct LocalRow {
        std::uint8_t day = 0;
        std::uint16_t minute_of_day = 0;
        std::uint16_t station = 0;
        std::uint8_t unit = 0;
        float value = 0.0f;
    };

    struct WeekBucket {
        std::uint8_t start_day = 0;
        std::uint64_t temporal_key = 0;
        std::vector<LocalRow> rows;
        std::vector<TemporalAtmospherePackedStation> local_stations;
        std::vector<std::string> local_units;
        std::vector<std::uint16_t> station_remap;
        std::vector<std::uint8_t> unit_remap;
        bool ordered = true;
        bool have_previous = false;
        LocalRow previous;
    };

    const auto row_less = [](const LocalRow& a, const LocalRow& b) {
        if (a.day != b.day) return a.day < b.day;
        if (a.minute_of_day != b.minute_of_day) return a.minute_of_day < b.minute_of_day;
        if (a.station != b.station) return a.station < b.station;
        return a.unit < b.unit;
    };

    if (stations.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) ||
        units.size() > static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max())) {
        return;
    }

    WeekBucket weeks[6];
    for (int i = 0; i < 6; ++i) {
        weeks[i].start_day = static_cast<std::uint8_t>(i * 7 + 1);
        weeks[i].station_remap.assign(stations.size(), std::numeric_limits<std::uint16_t>::max());
        weeks[i].unit_remap.assign(units.size(), std::numeric_limits<std::uint8_t>::max());
        weeks[i].rows.reserve(samples.size() / 6u + 16u);
    }

    for (const TemporalAtmospherePackedSample& s : samples) {
        if (s.station_index >= stations.size() || s.unit_index >= units.size()) continue;
        if (s.day < 1 || s.day > dim || s.hour > 23u || s.minute > 59u || !std::isfinite(s.value)) continue;

        const int week_index = std::clamp<int>((static_cast<int>(s.day) - 1) / 7, 0, 5);
        WeekBucket& week = weeks[week_index];

        std::uint16_t local_station = week.station_remap[static_cast<std::size_t>(s.station_index)];
        if (local_station == std::numeric_limits<std::uint16_t>::max()) {
            if (week.local_stations.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) continue;
            local_station = static_cast<std::uint16_t>(week.local_stations.size());
            week.station_remap[static_cast<std::size_t>(s.station_index)] = local_station;
            week.local_stations.push_back(stations[static_cast<std::size_t>(s.station_index)]);
        }

        std::uint8_t local_unit = week.unit_remap[static_cast<std::size_t>(s.unit_index)];
        if (local_unit == std::numeric_limits<std::uint8_t>::max()) {
            if (week.local_units.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max())) continue;
            local_unit = static_cast<std::uint8_t>(week.local_units.size());
            week.unit_remap[static_cast<std::size_t>(s.unit_index)] = local_unit;
            week.local_units.push_back(units[static_cast<std::size_t>(s.unit_index)]);
        }

        LocalRow r;
        r.day = s.day;
        r.minute_of_day = static_cast<std::uint16_t>(static_cast<unsigned>(s.hour) * 60u + static_cast<unsigned>(s.minute));
        r.station = local_station;
        r.unit = local_unit;
        r.value = static_cast<float>(s.value);
        if (week.have_previous && row_less(r, week.previous)) week.ordered = false;
        week.previous = r;
        week.have_previous = true;
        week.rows.push_back(r);
        week.temporal_key = std::max<std::uint64_t>(week.temporal_key,
            temporal_atmosphere_time_key(year, month, s.day, s.hour, s.minute));
    }

    for (WeekBucket& week : weeks) {
        if (week.rows.empty() || week.local_stations.empty() || week.local_units.empty()) continue;
        if (!week.ordered && week.rows.size() > 1u) std::stable_sort(week.rows.begin(), week.rows.end(), row_less);

        const std::uint32_t station_count = static_cast<std::uint32_t>(week.local_stations.size());
        const std::uint32_t unit_count = static_cast<std::uint32_t>(week.local_units.size());
        const std::uint64_t row_count = static_cast<std::uint64_t>(week.rows.size());
        const std::uint8_t safe_start_day = static_cast<std::uint8_t>(
            std::clamp<int>(static_cast<int>(week.start_day), 1, dim));
        const std::uint64_t week_begin_key = temporal_atmosphere_time_key(year, month, safe_start_day, 0u, 0u);

        // IXIPTLAH-SEMANA schema 1: bloque caliente de representación semanal.
        // Sólo se escriben estaciones y unidades efectivamente presentes en esa
        // semana. El payload queda más pequeño que el mensual y el lector proyecta
        // directamente día/minuto/estación/unidad/valor, sin reconstruir catálogos.
        (void)ixiptlah_append_record_tagged_temporal_deferred_flush(target,
            IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch, 1, layer_key, week_begin_key,
            [&](std::ostream& out) {
                if (!ixiptlah_write_string(out, source_id) ||
                    !ixiptlah_write_string(out, source_file) ||
                    !ixiptlah_write_string(out, source_path) ||
                    !ixiptlah_write_string(out, domain) ||
                    !ixiptlah_write_string(out, pollutant) ||
                    !ixiptlah_write_value(out, year) ||
                    !ixiptlah_write_value(out, month) ||
                    !ixiptlah_write_value(out, week.start_day) ||
                    !ixiptlah_write_value(out, station_count)) return false;

                for (std::uint32_t i = 0; i < station_count; ++i) {
                    const auto& st = week.local_stations[static_cast<std::size_t>(i)];
                    if (!ixiptlah_write_string(out, normalized_three_code(st.id, "ATM")) ||
                        !ixiptlah_write_string(out, st.name) ||
                        !ixiptlah_write_value(out, st.lon) ||
                        !ixiptlah_write_value(out, st.lat) ||
                        !ixiptlah_write_value(out, st.alt)) return false;
                }

                if (!ixiptlah_write_value(out, unit_count)) return false;
                for (std::uint32_t i = 0; i < unit_count; ++i) {
                    if (!ixiptlah_write_string(out, week.local_units[static_cast<std::size_t>(i)])) return false;
                }

                if (!ixiptlah_write_value(out, row_count)) return false;
                for (const LocalRow& r : week.rows) {
                    if (r.station >= station_count || r.unit >= unit_count) return false;
                    if (!ixiptlah_write_value(out, r.day) ||
                        !ixiptlah_write_value(out, r.minute_of_day) ||
                        !ixiptlah_write_value(out, r.station) ||
                        !ixiptlah_write_value(out, r.unit) ||
                        !ixiptlah_write_value(out, r.value)) return false;
                }
                return true;
            });
    }
}

int temporal_days_in_month_fast(int year, int month) {
    if (month < 1 || month > 12) return 0;
    static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = days[month - 1];
    const bool leap = (month == 2) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    return d + (leap ? 1 : 0);
}

bool temporal_atmosphere_sample_calendar_valid(const TemporalAtmospherePackedSample& sample, int year, int month) {
    const int dim = temporal_week_file_days_in_month(year, month);
    return dim > 0 && sample.day >= 1 && sample.day <= dim && sample.hour <= 23 && sample.minute <= 59;
}

void temporal_append_atmosphere_measurement_batch(const fs::path& root,


                                                  const std::string& source_id,


                                                  const std::string& source_file,


                                                  const std::string& source_path,
                                                  const std::string& domain,


                                                  int year,


                                                  int month,


                                                  const std::vector<TemporalAtmospherePackedStation>& stations,


                                                  const std::vector<std::string>& pollutants,


                                                  const std::vector<std::string>& units,


                                                  const std::vector<TemporalAtmospherePackedSample>& samples) {


    if (root.empty() || year <= 0 || month < 1 || month > 12 || samples.empty()) return;


    const std::uint32_t station_count = static_cast<std::uint32_t>(std::min<std::size_t>(stations.size(), std::numeric_limits<std::uint32_t>::max()));
    std::vector<std::string> pollutants_local = pollutants;
    std::vector<std::string> units_local = units;
    std::vector<TemporalAtmospherePackedSample> samples_local = samples;

    // Catálogo primario estricto: se preservan sólo variables ofrecidas por
    // epidemiología, RAMA, REDMET, RUOA o PEMBU. No se materializan razones,
    // gases inferidos, índices satelitales ni derivados meteorológicos.
    (void)station_count;

    const std::uint32_t pollutant_count = static_cast<std::uint32_t>(std::min<std::size_t>(pollutants_local.size(), std::numeric_limits<std::uint32_t>::max()));
    const std::uint32_t unit_count = static_cast<std::uint32_t>(std::min<std::size_t>(units_local.size(), std::numeric_limits<std::uint32_t>::max()));

    if (station_count == 0 || pollutant_count == 0 || unit_count == 0) return;

    std::vector<std::uint64_t> counts_by_pollutant(static_cast<std::size_t>(pollutant_count), 0ull);
    for (const auto& sample : samples_local) {
        if (sample.station_index < station_count && sample.pollutant_index < pollutant_count &&
            sample.unit_index < unit_count && temporal_atmosphere_sample_calendar_valid(sample, year, month) && std::isfinite(sample.value)) {
            ++counts_by_pollutant[static_cast<std::size_t>(sample.pollutant_index)];
        }
    }

    const bool disable_split = false;
    if (disable_split) {
        const TemporalIxiptlahCategory batch_category = [&]() {
            const std::string d = normalize_key(domain + " " + source_id + " " + source_path);
            bool has_meteorological = contains_norm(d, "meteor") || contains_norm(d, "redma");
            bool has_contaminant = contains_norm(d, "contamin") || contains_norm(d, "rama");
            for (const std::string& pollutant : pollutants_local) {
                has_meteorological = has_meteorological || atmospheric_key_is_meteorological(pollutant);
                has_contaminant = has_contaminant || atmospheric_key_is_contaminant(pollutant);
            }
            return (has_meteorological && !has_contaminant)
                ? TemporalIxiptlahCategory::Meteorological
                : TemporalIxiptlahCategory::AtmosphericContaminants;
        }();

        const fs::path target = temporal_category_shards_enabled()
            ? temporal_category_ixiptlah_file_in_root(root, batch_category)
            : temporal_monthly_ixiptlah_file_in_root(root, year, month);
        const std::uint64_t sample_count = static_cast<std::uint64_t>(samples_local.size());

        (void)ixiptlah_append_record(target,
                                     IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch, 1,
                                     [&](std::ostream& out) {
            if (!ixiptlah_write_string(out, source_id) ||
                !ixiptlah_write_string(out, source_file) ||
                !ixiptlah_write_string(out, source_path) ||
                !ixiptlah_write_string(out, domain) ||
                !ixiptlah_write_value(out, year) ||
                !ixiptlah_write_value(out, month) ||
                !ixiptlah_write_value(out, station_count)) return false;
            for (std::uint32_t i = 0; i < station_count; ++i) {
                const auto& st = stations[static_cast<std::size_t>(i)];
                if (!ixiptlah_write_string(out, normalized_three_code(st.id, "ATM")) ||
                    !ixiptlah_write_string(out, st.name) ||
                    !ixiptlah_write_value(out, st.lon) ||
                    !ixiptlah_write_value(out, st.lat) ||
                    !ixiptlah_write_value(out, st.alt)) return false;
            }
            if (!ixiptlah_write_value(out, pollutant_count)) return false;
            for (std::uint32_t i = 0; i < pollutant_count; ++i) if (!ixiptlah_write_string(out, pollutants_local[static_cast<std::size_t>(i)])) return false;
            if (!ixiptlah_write_value(out, unit_count)) return false;
            for (std::uint32_t i = 0; i < unit_count; ++i) if (!ixiptlah_write_string(out, units_local[static_cast<std::size_t>(i)])) return false;
            if (!ixiptlah_write_value(out, sample_count)) return false;
            for (const auto& sample : samples_local) {
                if (sample.station_index >= station_count || sample.pollutant_index >= pollutant_count || sample.unit_index >= unit_count) return false;
                if (!ixiptlah_write_value(out, sample.day) ||
                    !ixiptlah_write_value(out, sample.hour) ||
                    !ixiptlah_write_value(out, sample.minute) ||
                    !ixiptlah_write_value(out, sample.station_index) ||
                    !ixiptlah_write_value(out, sample.pollutant_index) ||
                    !ixiptlah_write_value(out, sample.unit_index) ||
                    !ixiptlah_write_value(out, sample.value)) return false;
            }
            return true;
        });
        return;
    }

    // IXIPTLAH v12 semanal unificado: RAMA, REDMET y RUOA/PEMBU viven en el
    // mismo núcleo físico de su semana. La separación científica queda dentro
    // del encabezado/tag/capa; la UI abre un solo archivo y descarta capas antes
    // de tocar payload, eliminando barridos por red o década.
    for (std::uint32_t pollutant_i = 0; pollutant_i < pollutant_count; ++pollutant_i) {
        const std::uint64_t global_sample_count = counts_by_pollutant[static_cast<std::size_t>(pollutant_i)];
        if (global_sample_count == 0) continue;

        const std::string pollutant = pollutants_local[static_cast<std::size_t>(pollutant_i)];
        const std::string layer_key = temporal_atmospheric_layer_key(pollutant);
        if (layer_key.empty() || layer_key == "desconocida" || layer_key == "unknown") {
            // No crear records basura para variables vacías o no identificadas:
            // la década se conserva densa sólo con capas científicamente válidas.
            continue;
        }
        const TemporalIxiptlahCategory layer_category = temporal_atmosphere_source_category(source_id, source_file, source_path, domain, pollutant);

        const fs::path legacy_target = temporal_monthly_ixiptlah_file_in_root(root, year, month);
        (void)layer_category;

        std::vector<std::uint32_t> station_remap(static_cast<std::size_t>(station_count), std::numeric_limits<std::uint32_t>::max());
        std::vector<std::uint32_t> unit_remap(static_cast<std::size_t>(unit_count), std::numeric_limits<std::uint32_t>::max());
        std::vector<TemporalAtmospherePackedStation> local_stations;
        std::vector<std::string> local_units;
        local_stations.reserve(std::min<std::uint64_t>(global_sample_count, station_count));
        local_units.reserve(std::min<std::uint64_t>(global_sample_count, unit_count));

        std::uint64_t layer_sample_count = 0;
        std::uint64_t layer_temporal_key = 0;
        std::vector<const TemporalAtmospherePackedSample*> layer_samples;
        layer_samples.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(global_sample_count, 1024ull * 1024ull)));
        for (const auto& sample : samples_local) {
            if (sample.station_index >= station_count || sample.pollutant_index >= pollutant_count || sample.unit_index >= unit_count) continue;
            if (sample.pollutant_index != pollutant_i || !std::isfinite(sample.value)) continue;
            if (!temporal_atmosphere_sample_calendar_valid(sample, year, month)) continue;

            std::uint32_t local_station = station_remap[static_cast<std::size_t>(sample.station_index)];
            if (local_station == std::numeric_limits<std::uint32_t>::max()) {
                if (local_stations.size() >= std::numeric_limits<std::uint32_t>::max()) continue;
                local_station = static_cast<std::uint32_t>(local_stations.size());
                station_remap[static_cast<std::size_t>(sample.station_index)] = local_station;
                local_stations.push_back(stations[static_cast<std::size_t>(sample.station_index)]);
            }

            std::uint32_t local_unit = unit_remap[static_cast<std::size_t>(sample.unit_index)];
            if (local_unit == std::numeric_limits<std::uint32_t>::max()) {
                if (local_units.size() >= std::numeric_limits<std::uint32_t>::max()) continue;
                local_unit = static_cast<std::uint32_t>(local_units.size());
                unit_remap[static_cast<std::size_t>(sample.unit_index)] = local_unit;
                local_units.push_back(units_local[static_cast<std::size_t>(sample.unit_index)]);
            }

            layer_temporal_key = std::max<std::uint64_t>(
                layer_temporal_key,
                temporal_atmosphere_time_key(year, month, sample.day, sample.hour, sample.minute));
            layer_samples.push_back(&sample);
            ++layer_sample_count;
        }

        if (local_stations.empty() || local_units.empty() || layer_sample_count == 0 || layer_samples.empty()) continue;
        std::stable_sort(layer_samples.begin(), layer_samples.end(), [](const TemporalAtmospherePackedSample* a, const TemporalAtmospherePackedSample* b) {
            if (!a || !b) return b != nullptr;
            if (a->day != b->day) return a->day < b->day;
            if (a->hour != b->hour) return a->hour < b->hour;
            if (a->minute != b->minute) return a->minute < b->minute;
            if (a->station_index != b->station_index) return a->station_index < b->station_index;
            if (a->unit_index != b->unit_index) return a->unit_index < b->unit_index;
            return a->value < b->value;
        });
        layer_sample_count = static_cast<std::uint64_t>(layer_samples.size());

        if (local_stations.empty() || local_units.empty() || layer_sample_count == 0) continue;

        const std::uint32_t local_station_count = static_cast<std::uint32_t>(local_stations.size());
        const std::uint32_t local_unit_count = static_cast<std::uint32_t>(local_units.size());
        if (local_station_count > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()) ||
            local_unit_count > static_cast<std::uint32_t>(std::numeric_limits<std::uint8_t>::max())) {
            // IXIPTLAH-ATM V1 no rellena semanas vacías ni retrocede a schema 2:
            // una fuente excesiva debe partirse antes de escribir para conservar
            // payloads compactos y lectura directa en la ruta caliente.
            continue;
        }
        const std::uint32_t schema_version = 1u;

        std::array<std::vector<const TemporalAtmospherePackedSample*>, 6> samples_by_week;
        for (const TemporalAtmospherePackedSample* sample_ptr : layer_samples) {
            if (!sample_ptr) continue;
            const int week_index = std::clamp<int>((static_cast<int>(sample_ptr->day) - 1) / 7, 0, 5);
            samples_by_week[static_cast<std::size_t>(week_index)].push_back(sample_ptr);
        }

        for (std::size_t week_index = 0; week_index < samples_by_week.size(); ++week_index) {
            const std::vector<const TemporalAtmospherePackedSample*>& write_samples = samples_by_week[week_index];
            if (write_samples.empty()) continue;
            const unsigned week_file_start_day = static_cast<unsigned>(week_index * 7u + 1u);
            const fs::path target = temporal_unified_weekly_files_enabled()
                ? ixiptlah_path(root, temporal_week_start_token_from_ymd(year, month, static_cast<int>(week_file_start_day)))
                : legacy_target;
            temporal_prepare_element_ixiptlah_for_fresh_import(target);

            // El lote crudo vive físicamente en el IXIPTLAH semanal; su llave
            // temporal debe representar el inicio del bloque, no el último dato.
            // Así la consulta semanal encuentra el payload por índice directo y
            // no cae a buckets de capa aunque el último registro sea tardío.
            const std::uint64_t write_temporal_key = temporal_atmosphere_time_key(
                year, month, static_cast<std::uint8_t>(std::clamp<unsigned>(week_file_start_day, 1u, 31u)), 0u, 0u);
            const std::uint64_t write_sample_count = static_cast<std::uint64_t>(write_samples.size());

            const bool measurement_ok = ixiptlah_append_record_tagged_temporal_deferred_flush(target,
                                                         IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch, schema_version,
                                                         layer_key, write_temporal_key,
                                                         [&](std::ostream& out) {
                if (!ixiptlah_write_string(out, source_id) ||
                    !ixiptlah_write_string(out, source_file) ||
                    !ixiptlah_write_string(out, source_path) ||
                    !ixiptlah_write_string(out, domain) ||
                    !ixiptlah_write_value(out, year) ||
                    !ixiptlah_write_value(out, month) ||
                    !ixiptlah_write_value(out, local_station_count)) return false;

                for (std::uint32_t i = 0; i < local_station_count; ++i) {
                    const auto& st = local_stations[static_cast<std::size_t>(i)];
                    if (!ixiptlah_write_string(out, normalized_three_code(st.id, "ATM")) ||
                        !ixiptlah_write_string(out, st.name) ||
                        !ixiptlah_write_value(out, st.lon) ||
                        !ixiptlah_write_value(out, st.lat) ||
                        !ixiptlah_write_value(out, st.alt)) return false;
                }

                const std::uint32_t one_pollutant = 1;
                if (!ixiptlah_write_value(out, one_pollutant) || !ixiptlah_write_string(out, pollutant)) return false;

                if (!ixiptlah_write_value(out, local_unit_count)) return false;
                for (std::uint32_t i = 0; i < local_unit_count; ++i) {
                    if (!ixiptlah_write_string(out, local_units[static_cast<std::size_t>(i)])) return false;
                }

                if (!ixiptlah_write_value(out, write_sample_count)) return false;
                std::uint64_t written_samples = 0;
                for (const TemporalAtmospherePackedSample* sample_ptr : write_samples) {
                    if (!sample_ptr) continue;
                    const TemporalAtmospherePackedSample& sample = *sample_ptr;
                    if (sample.station_index >= station_count || sample.pollutant_index >= pollutant_count || sample.unit_index >= unit_count) continue;
                    if (sample.pollutant_index != pollutant_i || !std::isfinite(sample.value)) continue;
                    if (!temporal_atmosphere_sample_calendar_valid(sample, year, month)) continue;

                    const std::uint32_t local_station = station_remap[static_cast<std::size_t>(sample.station_index)];
                    const std::uint32_t local_unit = unit_remap[static_cast<std::size_t>(sample.unit_index)];
                    if (local_station >= local_station_count || local_unit >= local_unit_count) return false;

                    const std::uint16_t minute_of_day = static_cast<std::uint16_t>(static_cast<unsigned>(sample.hour) * 60u + static_cast<unsigned>(sample.minute));
                    const float value32 = static_cast<float>(sample.value);

                    // IXIPTLAH-ATM V1: cada archivo semanal queda ya ordenado por
                    // día/minuto/estación; la UI sólo proyecta bytes preformados y no
                    // fabrica perfiles horarios ni rellena semanas con cero registros.
                    const std::uint16_t station16 = static_cast<std::uint16_t>(local_station);
                    const std::uint8_t unit8 = static_cast<std::uint8_t>(local_unit);
                    if (!ixiptlah_write_value(out, sample.day) ||
                        !ixiptlah_write_value(out, minute_of_day) ||
                        !ixiptlah_write_value(out, station16) ||
                        !ixiptlah_write_value(out, unit8) ||
                        !ixiptlah_write_value(out, value32)) return false;
                    ++written_samples;
                }

                return written_samples == write_sample_count;
            });

            if (measurement_ok) {
                std::vector<TemporalAtmospherePackedSample> preformed_samples;
                preformed_samples.reserve(write_samples.size());
                for (const TemporalAtmospherePackedSample* sample_ptr : write_samples) {
                    if (!sample_ptr) continue;
                    const TemporalAtmospherePackedSample& sample = *sample_ptr;
                    if (sample.station_index >= station_count || sample.unit_index >= unit_count || sample.pollutant_index != pollutant_i) continue;
                    const std::uint32_t local_station = station_remap[static_cast<std::size_t>(sample.station_index)];
                    const std::uint32_t local_unit = unit_remap[static_cast<std::size_t>(sample.unit_index)];
                    if (local_station >= local_station_count || local_unit >= local_unit_count) continue;
                    TemporalAtmospherePackedSample compact = sample;
                    compact.station_index = local_station;
                    compact.unit_index = local_unit;
                    compact.pollutant_index = 0u;
                    preformed_samples.push_back(compact);
                }
                temporal_append_atmosphere_layer_preformed_records(target, layer_key, pollutant, source_id, source_file,
                                                                   source_path, domain, year, month, local_stations,
                                                                   local_units, preformed_samples);
                temporal_append_atmosphere_layer_hourly_preformed_records(target, layer_key, pollutant, source_id, source_file,
                                                                         source_path, domain, year, month, local_stations,
                                                                         local_units, preformed_samples);
                temporal_append_atmosphere_layer_weekly_preformed_records(target, layer_key, pollutant, source_id, source_file,
                                                                         source_path, domain, year, month, local_stations,
                                                                         local_units, preformed_samples);
            }
        }

    }
}




void temporal_append_atmosphere_render_summary(const fs::path& root,


                                               const std::string& date,


                                               const std::string& hour,


                                               int year,
                                               const std::string& pollutant,
                                               const std::string& station_id,

                                               const std::string& station,


                                               double lon,


                                               double lat,


                                               double alt,
                                               double mean,
                                               double min_value,

                                               double max_value,
                                               const std::string& unit,
                                               int64_t count)
{
    if (root.empty() || trim(pollutant).empty() || trim(station_id).empty()) return;
    int y = year;
    if (date.size() >= 4u && std::isdigit(static_cast<unsigned char>(date[0])) &&
        std::isdigit(static_cast<unsigned char>(date[1])) &&
        std::isdigit(static_cast<unsigned char>(date[2])) &&
        std::isdigit(static_cast<unsigned char>(date[3]))) {
        try { y = std::stoi(date.substr(0, 4)); } catch (...) {}
    }
    int m = month_from_iso_date(date);
    int d = 1;
    if (date.size() >= 10u && std::isdigit(static_cast<unsigned char>(date[8])) &&
        std::isdigit(static_cast<unsigned char>(date[9]))) {
        try { d = std::clamp(std::stoi(date.substr(8, 2)), 1, 31); } catch (...) { d = 1; }
    }
    int hh = 0;
    int mm = 0;
    if (hour.size() >= 2u && std::isdigit(static_cast<unsigned char>(hour[0])) &&
        std::isdigit(static_cast<unsigned char>(hour[1]))) {
        try { hh = std::clamp(std::stoi(hour.substr(0, 2)), 0, 23); } catch (...) { hh = 0; }
    }
    const std::size_t colon = hour.find(':');
    if (colon != std::string::npos && colon + 2u < hour.size() &&
        std::isdigit(static_cast<unsigned char>(hour[colon + 1])) &&
        std::isdigit(static_cast<unsigned char>(hour[colon + 2]))) {
        try { mm = std::clamp(std::stoi(hour.substr(colon + 1, 2)), 0, 59); } catch (...) { mm = 0; }
    }
    if (y <= 0 || m < 1 || m > 12) return;

    const fs::path target = ixiptlah_path(root, temporal_week_start_token_from_ymd(y, m, d));
    const std::string layer_key = std::string("ATM:") + normalize_key(pollutant);
    const std::uint64_t temporal_key = temporal_atmosphere_time_key(
        y, m, static_cast<std::uint8_t>(std::clamp(d, 1, 31)),
        static_cast<std::uint8_t>(hh), static_cast<std::uint8_t>(mm));

    // IXIPTLAH V1 guarda el punto crudo y este resumen preformado por estación.
    // El resumen no sustituye la muestra: sólo permite al mapa resolver etiquetas
    // y rangos visuales con un payload pequeño antes de caer al lote horario.
    (void)ixiptlah_append_record_tagged_temporal_deferred_flush(target,
        IxiptlahRecordType::AtmosphereRenderSummary, 1, layer_key, temporal_key,
        [&](std::ostream& out) {
            const std::string sid = normalized_three_code(station_id, "ATM");
            const std::string sname = trim(station).empty() ? sid : trim(station);
            const std::string iso_date = date.empty() ? ymd_file_token(y, static_cast<unsigned>(m), static_cast<unsigned>(std::clamp(d, 1, 31))) : date;
            const std::string hhmm = hour.empty() ? (std::string(hh < 10 ? "0" : "") + std::to_string(hh) + ":" + (mm < 10 ? "0" : "") + std::to_string(mm)) : hour;
            const float mean32 = static_cast<float>(mean);
            const float min32 = static_cast<float>(min_value);
            const float max32 = static_cast<float>(max_value);
            const std::int64_t count64 = count < 0 ? 0 : count;
            return ixiptlah_write_string(out, trim(pollutant)) &&
                   ixiptlah_write_string(out, sid) &&
                   ixiptlah_write_string(out, sname) &&
                   ixiptlah_write_string(out, trim(unit)) &&
                   ixiptlah_write_string(out, iso_date) &&
                   ixiptlah_write_string(out, hhmm) &&
                   ixiptlah_write_value(out, y) &&
                   ixiptlah_write_value(out, lon) &&
                   ixiptlah_write_value(out, lat) &&
                   ixiptlah_write_value(out, alt) &&
                   ixiptlah_write_value(out, mean32) &&
                   ixiptlah_write_value(out, min32) &&
                   ixiptlah_write_value(out, max32) &&
                   ixiptlah_write_value(out, count64);
        });
}



void temporal_append_atmosphere_territory_average(const fs::path& root,


                                                  const std::string& date,


                                                  const std::string& hour,


                                                  int year,
                                                  const std::string& entity_code,
                                                  const std::string& territory_code,

                                                  const std::string& territory_name,
                                                  const std::string& pollutant,
                                                  const std::string& metric,

                                                  double value,
                                                  const std::string& unit,
                                                  int64_t count) {
    if (root.empty() || trim(pollutant).empty() || trim(territory_code).empty()) return;
    std::map<std::string, std::string> fields;
    fields["date"] = date;
    fields["hour"] = hour;
    fields["year"] = std::to_string(year);
    fields["entity_code"] = entity_code;
    fields["territory_code"] = territory_code;
    fields["territory_name"] = territory_name;
    fields["pollutant"] = pollutant;
    fields["metric"] = metric;
    fields["value"] = std::to_string(value);
    fields["unit"] = unit;
    fields["count"] = std::to_string(count < 0 ? 0 : count);
    fields["domain"] = "territorio_atmosferico_preformado";

    // Promedio territorial V1: persistido como registro preformado pequeño. La
    // muestra por estación continúa siendo la verdad puntual; este bloque evita
    // reagrupar territorios en cada movimiento del navegador histórico.
    append_ixiptlah_fields_record(root, IxiptlahRecordType::AtmosphereTerritoryAverage, fields);
}




void temporal_flush_append_streams() {


    std::lock_guard<std::mutex> lk(temporal_append_mu());


    for (auto& [_, sink] : temporal_append_sinks()) {


        if (sink && sink->stream) sink->stream.flush();
    }


    ixiptlah_flush_all();


    temporal_last_flush_time() = std::chrono::steady_clock::now();
}




void temporal_flush_append_streams_if_due(int min_interval_ms) {


    if (min_interval_ms < 0) min_interval_ms = 0;


    const auto now = std::chrono::steady_clock::now();


    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - temporal_last_flush_time()).count() < min_interval_ms) return;


    temporal_flush_append_streams();
}




void temporal_close_append_streams() {


    std::lock_guard<std::mutex> lk(temporal_append_mu());
    auto& sinks = temporal_append_sinks();


    for (auto& [_, sink] : sinks) {


        if (sink && sink->stream) {
            sink->stream.flush();
            sink->stream.close();
        }
    }


    sinks.clear();


    ixiptlah_close_all();


    temporal_last_flush_time() = std::chrono::steady_clock::now();
}




int temporal_purge_atmosphere_category(const fs::path& root, const std::string& category, int year_start, int year_end) {

    const std::string wanted = atmosphere_purge_category_normalized(category);

    if (wanted.empty()) return 0;
    temporal_close_append_streams();

    int removed_records = 0;

    for (const auto& path : temporal_ixiptlah_files(root)) {
        std::error_code ec;

        if (!fs::exists(path, ec) || ec || file_size_or_zero(path) <= 16) continue;

        if (!ixiptlah_file_matches_year_filter(path, year_start, year_end)) continue;

        const fs::path tmp = fs::path(path.wstring() + L".rewrite");
        fs::remove(tmp, ec);
        int kept_here = 0;
        int removed_here = 0;

        bool read_ok = true;

        ixiptlah_read_records(path, [&](IxiptlahRecordType type, std::uint32_t schema, std::istream& in) {

            const std::string payload = read_remaining_payload_bytes(in);

            const bool remove = ixiptlah_atmosphere_payload_matches_category(type, schema, payload, wanted, year_start, year_end);


            if (remove) {

                ++removed_here;

                return true;
            }

            const bool ok = ixiptlah_append_record(tmp, type, schema, [&](std::ostream& out) {

                if (!payload.empty()) out.write(payload.data(), static_cast<std::streamsize>(payload.size()));

                return static_cast<bool>(out);
            });

            if (!ok) {

                read_ok = false;

                return false;
            }

            ++kept_here;

            return true;
        });

        ixiptlah_close_all();

        if (!read_ok || removed_here == 0) {
            fs::remove(tmp, ec);
            continue;
        }

        if (kept_here == 0) {


            fs::remove(path, ec);
            fs::remove(tmp, ec);

        } else {

            fs::remove(path, ec);

            ec.clear();

            fs::rename(tmp, path, ec);

            if (ec) fs::remove(tmp, ec);
        }

        removed_records += removed_here;
    }

    return removed_records;
}



int temporal_purge_epidemiology_records(const fs::path& root, int year_start, int year_end) {

    temporal_close_append_streams();

    int removed_records = 0;

    for (const auto& path : temporal_ixiptlah_files(root)) {
        std::error_code ec;

        if (!fs::exists(path, ec) || ec || file_size_or_zero(path) <= 16) continue;

        if (!ixiptlah_file_matches_year_filter(path, year_start, year_end)) continue;



        const IxiptlahRewriteStats stats = ixiptlah_rewrite_without_records(path,

            [](IxiptlahRecordType type, std::uint32_t schema) {


                if (type == IxiptlahRecordType::EpidemiologyRenderSnapshot) return true;
                if (type == IxiptlahRecordType::EpidemiologyObservation) return true;
                if (type == IxiptlahRecordType::EpidemiologyQuarantine) return true;
                return schema == 1 && type == IxiptlahRecordType::MonthlyEpidemiologyBatch;

            });

        if (stats.removed > 0 && !stats.rewritten) {

            throw std::runtime_error("No se pudo reescribir el nucleo epidemiologico ixiptlah sin tocar otros nucleos: " + path_utf8(path));
        }

        if (stats.rewritten) {

            temporal_clear_epi_dedupe_cache_for_target(path);
            removed_records += static_cast<int>(std::min<std::uint64_t>(
                stats.removed, static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
        }
    }

    return removed_records;
}



void temporal_rebuild_json_index_for_tsv(const fs::path& tsv_path) {

    (void)tsv_path;
    temporal_flush_append_streams();

    return;
}



void temporal_rebuild_all_json_indexes(const fs::path& root) {
    (void)root;

    temporal_flush_append_streams();
}

}

// ===== Nucleos/TextUtils.impl =====
#line 1 "Nucleos/TextUtils.impl"



#include <chrono>
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <locale>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif



namespace epi {


template <typename Fn>
static std::string c_text_transform(const std::string& s, size_t reserve_extra, Fn fn) {
    size_t cap = std::max<size_t>(16, s.size() + reserve_extra);
    std::string out(cap, '\0');
    size_t n = fn(out.data(), out.size());
    if (n >= out.size()) {
        out.assign(n + 1, '\0');
        n = fn(out.data(), out.size());
    }
    out.resize(n);
    return out;
}




std::string trim(std::string s) {
    size_t b = 0;
    size_t e = 0;
    ozmvm_trim_ascii_span(s.data(), s.size(), &b, &e);
    return s.substr(b, e - b);
}




std::string lower_ascii(std::string s) {
    return c_text_transform(s, 1, [&](char* out, size_t cap) {
        return ozmvm_lower_ascii_copy(s.data(), s.size(), out, cap);
    });
}



std::string clean_user_path_string(std::string s) {
#ifdef _WIN32
    constexpr int windows_paths = 1;
#else
    constexpr int windows_paths = 0;
#endif
    return c_text_transform(s, 1, [&](char* out, size_t cap) {
        return ozmvm_clean_user_path_copy(s.data(), s.size(), out, cap, windows_paths);
    });
}



fs::path clean_user_path(const fs::path& p) {

    const std::string text = clean_user_path_string(path_utf8(p));
#ifdef _WIN32

    return fs::path(widen_utf8(text));

#else

    return fs::path(text);
#endif
}



static void replace_all(std::string& s, const std::string& a, const std::string& b) {

    if (a.empty()) return;

    size_t pos = 0;

    while ((pos = s.find(a, pos)) != std::string::npos) {
        s.replace(pos, a.size(), b);
        pos += b.size();
    }
}



static void append_utf8(std::string& out, int code) {

    if (code < 0) return;

    if (code <= 0x7F) {

        out.push_back(static_cast<char>(code));

    } else if (code <= 0x7FF) {


        out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));

        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));

    } else if (code <= 0xFFFF) {

        out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));

        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));

        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));

    } else if (code <= 0x10FFFF) {

        out.push_back(static_cast<char>(0xF0 | ((code >> 18) & 0x07)));

        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));

        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));

        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}




std::string strip_accents_utf8(std::string s) {
    return c_text_transform(s, 1, [&](char* out, size_t cap) {
        return ozmvm_strip_accents_utf8_copy(s.data(), s.size(), out, cap);
    });

    const std::pair<const char*, const char*> repl[] = {
        {"á","a"},{"Á","A"},{"é","e"},{"É","E"},{"í","i"},{"Í","I"},{"ó","o"},{"Ó","O"},{"ú","u"},{"Ú","U"},

        {"á","a"},{"Á","A"},{"é","e"},{"É","E"},{"í","i"},{"Í","I"},{"ó","o"},{"Ó","O"},{"ú","u"},{"Ú","U"},
        {"ü","u"},{"Ü","U"},{"ñ","n"},{"Ñ","N"},{"à","a"},{"è","e"},{"ì","i"},{"ò","o"},{"ù","u"},
        {"ü","u"},{"Ü","U"},{"ñ","n"},{"Ñ","N"},{"à","a"},{"è","e"},{"ì","i"},{"ò","o"},{"ù","u"}
    };

    for (const auto& [a,b] : repl) replace_all(s, a, b);

    return s;
}





std::string normalize_key(std::string s) {
    return c_text_transform(s, 1, [&](char* out, size_t cap) {
        return ozmvm_normalize_key_utf8_copy(s.data(), s.size(), out, cap);
    });

    s = html_unescape(s);
    s = strip_accents_utf8(s);

    s = lower_ascii(s);

    for (char& c : s) {
        const unsigned char u = static_cast<unsigned char>(c);

        if (!(std::isalnum(u))) c = ' ';
    }
    std::string out;
    bool prev_space = false;


    for (char c : s) {

        if (c == ' ') {

            if (!prev_space) out.push_back(' ');
            prev_space = true;
        } else {

            out.push_back(c);
            prev_space = false;
        }
    }


    return trim(out);
}




std::string html_unescape(std::string s) {
    return c_text_transform(s, 8, [&](char* out, size_t cap) {
        return ozmvm_html_unescape_copy(s.data(), s.size(), out, cap);
    });

    replace_all(s, "&amp;", "&");
    replace_all(s, "&lt;", "<");
    replace_all(s, "&gt;", ">");
    replace_all(s, "&quot;", "\"");
    replace_all(s, "&#39;", "'");

    replace_all(s, "&apos;", "'");
    std::string result;

    result.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {

        if (s[i] == '&' && i + 3 < s.size() && s[i + 1] == '#') {

            size_t j = i + 2;
            int code = 0;
            bool digit = false;

            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) {
                digit = true;
                code = code * 10 + (s[j] - '0');
                ++j;
            }

            if (digit && j < s.size() && s[j] == ';') {
                append_utf8(result, code);

                i = j;

                continue;
            }
        }

        result.push_back(s[i]);
    }

    return result;
}





std::string json_escape(const std::string& s) {
    return c_text_transform(s, 8, [&](char* out, size_t cap) {
        return ozmvm_json_escape_copy(s.data(), s.size(), out, cap);
    });

    std::ostringstream o;

    for (unsigned char c : s) {
        switch (c) {
            case '\\': o << "\\\\"; break;
            case '"': o << "\\\""; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;

            case '\t': o << "\\t"; break;
            default:

                if (c < 0x20) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                else o << static_cast<char>(c);
        }
    }

    return o.str();
}





std::string csv_escape(const std::string& s) {
    return c_text_transform(s, 4, [&](char* out, size_t cap) {
        return ozmvm_csv_escape_copy(s.data(), s.size(), out, cap);
    });

    bool need = s.find_first_of(",\n\r\"") != std::string::npos;

    if (!need) return s;
    std::string out = "\"";

    for (char c : s) {

        if (c == '"') out += "\"\"";
        else out += c;
    }

    out += '"';

    return out;
}




bool contains_norm(const std::string& haystack_norm, const std::string& needle_norm) {

    if (needle_norm.empty()) return false;

    return haystack_norm.find(needle_norm) != std::string::npos;
}



bool is_numeric_token(const std::string& s) {
    return ozmvm_is_numeric_token(s.data(), s.size()) != 0;

    const std::string t = trim(s);

    if (t.empty()) return false;

    if (t == "-" || t == "–" || t == "—") return true;

    bool digit = false;

    for (char c : t) {

        if (std::isdigit(static_cast<unsigned char>(c))) digit = true;

        else if (c == ',' || c == '.' || c == ' ') continue;
        else return false;
    }

    return digit;
}





std::optional<int64_t> parse_epi_int(const std::string& s) {
    int64_t value = 0;
    if (ozmvm_parse_epi_i64_token(s.data(), s.size(), &value)) return value;
    return std::nullopt;

    std::string t = trim(s);

    if (t == "-" || t == "–" || t == "—") return int64_t{0};

    std::string digits;

    for (char c : t) {

        if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);

        else if (c == ',' || c == '.' || c == ' ') continue;
        else return std::nullopt;
    }

    if (digits.empty()) return std::nullopt;

    try { return std::stoll(digits); } catch (...) { return std::nullopt; }
}




std::string safe_filename(std::string s) {
    return c_text_transform(s, 16, [&](char* out, size_t cap) {
        return ozmvm_safe_filename_copy(s.data(), s.size(), out, cap);
    });

    s = strip_accents_utf8(s);

    for (char& c : s) {
        unsigned char u = static_cast<unsigned char>(c);

        if (!(std::isalnum(u) || c == '-' || c == '_' || c == '.')) c = '_';
    }


    if (s.size() > 120) s = s.substr(0, 120);


    return trim(s.empty() ? std::string("unnamed") : s);
}



std::string simple_hash_hex(const std::string& s) {
    char hex[17];
    ozmvm_fnv1a64_hex(s.data(), s.size(), hex);
    return std::string(hex, 16);

    uint64_t h = 1469598103934665603ull;

    for (unsigned char c : s) {
        h ^= c;

        h *= 1099511628211ull;
    }
    std::ostringstream os;

    os << std::hex << std::setw(16) << std::setfill('0') << h;

    return os.str();
}



std::string read_text_file(const fs::path& p) {


    std::ifstream in(p, std::ios::binary);

    if (!in) throw std::runtime_error("No se pudo abrir archivo: " + path_utf8(p));
    std::ostringstream ss;

    ss << in.rdbuf();

    std::string text = ss.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}



void write_text_file(const fs::path& p, const std::string& content) {

    ensure_dir(p.parent_path());

    fs::path tmp = p;
    tmp += ".tmp";
    {


        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);

        if (!out) throw std::runtime_error("No se pudo escribir archivo temporal: " + path_utf8(tmp));
        out << content;
        out.close();

        if (!out) throw std::runtime_error("No se pudo cerrar archivo temporal: " + path_utf8(tmp));
    }
    std::error_code ec;

    fs::rename(tmp, p, ec);

    if (ec) {

        fs::remove(p, ec);

        ec.clear();

        fs::rename(tmp, p, ec);
    }


    if (ec) throw std::runtime_error("No se pudo publicar archivo atomico: " + path_utf8(p) + " (" + ec.message() + ")");
}



void ensure_dir(const fs::path& p) {

    if (!p.empty()) fs::create_directories(p);
}

#ifdef _WIN32


static std::wstring win32_long_path(const fs::path& p) {
    std::error_code ec;

    fs::path abs = p;

    if (!abs.is_absolute()) {

        const fs::path tmp = fs::absolute(abs, ec);

        if (!ec) abs = tmp;
    }
    std::wstring s = abs.wstring();


    std::replace(s.begin(), s.end(), L'/', L'\\');

    if (s.rfind(L"\\\\?\\", 0) == 0) return s;

    if (s.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + s.substr(2);

    return L"\\\\?\\" + s;
}
#endif




uintmax_t file_size_or_zero(const fs::path& p) {
    std::error_code ec;


    const auto n = fs::file_size(p, ec);

    if (!ec) return n;
#ifdef _WIN32

    WIN32_FILE_ATTRIBUTE_DATA data{};

    if (GetFileAttributesExW(win32_long_path(p).c_str(), GetFileExInfoStandard, &data)) {

        const uint64_t high = static_cast<uint64_t>(data.nFileSizeHigh);

        const uint64_t low = static_cast<uint64_t>(data.nFileSizeLow);


        return static_cast<uintmax_t>((high << 32) | low);
    }
#endif

    return 0;
}




bool copy_file_overwrite(const fs::path& source, const fs::path& destination, std::error_code& ec) {

    ensure_dir(destination.parent_path());
#ifdef _WIN32

    if (CopyFileW(win32_long_path(source).c_str(), win32_long_path(destination).c_str(), FALSE)) {

        ec.clear();

        return true;
    }
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());

    return false;
#else


    return fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
#endif
}





static bool path_exists_directory_relaxed(const fs::path& p) {
    std::error_code ec;

    if (fs::is_directory(p, ec) && !ec) return true;
#ifdef _WIN32

    const DWORD attr = GetFileAttributesW(win32_long_path(p).c_str());

    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else

    return false;
#endif
}



fs::path resolve_existing_path_relaxed(const fs::path& raw) {

    const fs::path p = clean_user_path(raw);
    std::error_code ec;

    if (fs::exists(p, ec) && !ec) return p;


    const fs::path parent = p.parent_path();


    if (parent.empty() || parent == p) return p;


    fs::path resolved_parent = resolve_existing_path_relaxed(parent);

    if (!path_exists_directory_relaxed(resolved_parent)) return p;



    const std::string wanted = normalize_key(path_utf8(p.filename()));

    if (wanted.empty()) return p;


    try {

        for (const auto& e : fs::directory_iterator(resolved_parent, fs::directory_options::skip_permission_denied)) {


            if (normalize_key(path_utf8(e.path().filename())) == wanted) return e.path();
        }
    } catch (...) {}

    return p;
}




namespace {




bool file_has_pdf_signature_light(const fs::path& p) {


    if (file_size_or_zero(p) < 256) return false;


    std::ifstream in(p, std::ios::binary);


    if (!in) return false;
    char head[8]{};

    in.read(head, sizeof(head));
    const std::streamsize got = in.gcount();

    if (got < 5) return false;

    return head[0] == '%' && head[1] == 'P' && head[2] == 'D' && head[3] == 'F' && head[4] == '-';
}




bool path_has_pdf_extension(const fs::path& p) {


    return lower_ascii(path_utf8(p.extension())) == ".pdf";
}




bool file_is_acceptable_pdf_document(const fs::path& p) {




    if (path_has_pdf_extension(p)) return true;


    const std::string ext = lower_ascii(path_utf8(p.extension()));


    if (!ext.empty() && ext != ".tmp" && ext != ".download" && ext != ".candidate") return false;


    return file_has_pdf_signature_light(p);
}

}




std::vector<fs::path> list_pdfs_recursive(const fs::path& raw_root) {


    std::vector<fs::path> out;

    const fs::path root = resolve_existing_path_relaxed(raw_root);
#ifdef _WIN32

    const std::wstring root_long = win32_long_path(root);

    DWORD root_attr = GetFileAttributesW(root_long.c_str());

    if (root_attr == INVALID_FILE_ATTRIBUTES || (root_attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {

        throw std::runtime_error("No existe el directorio de entrada: " + path_utf8(root));
    }


    std::function<void(const fs::path&)> walk = [&](const fs::path& dir) {
        WIN32_FIND_DATAW data{};

        const std::wstring pattern = win32_long_path(dir / L"*");

        HANDLE h = FindFirstFileW(pattern.c_str(), &data);

        if (h == INVALID_HANDLE_VALUE) return;

        do {

            const std::wstring name(data.cFileName);


            if (name == L"." || name == L"..") continue;

            const fs::path child = dir / name;

            const DWORD attr = data.dwFileAttributes;

            if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {


                if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0) walk(child);


            } else if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0 && file_is_acceptable_pdf_document(child)) {

                out.push_back(child);
            }

        } while (FindNextFileW(h, &data));
        FindClose(h);
    };
    walk(root);
#else

    if (!fs::exists(root)) throw std::runtime_error("No existe el directorio de entrada: " + path_utf8(root));
    std::error_code ec;

    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {


        if (ec) { ec.clear(); continue; }
        const auto& e = *it;
        std::error_code item_ec;

        if (!e.is_regular_file(item_ec) || item_ec) continue;


        if (file_is_acceptable_pdf_document(e.path())) out.push_back(e.path());
    }
#endif


    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) { return path_utf8(a) < path_utf8(b); });


    out.erase(std::unique(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) { return path_utf8(a) == path_utf8(b); }), out.end());

    return out;
}



std::string now_utc_iso() {


    using namespace std::chrono;
    const auto now = system_clock::now();

    const auto tt = system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32

    gmtime_s(&tm, &tt);
#else

    gmtime_r(&tt, &tm);
#endif
    std::ostringstream os;

    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    return os.str();
}




std::wstring widen_utf8(const std::string& s) {
#ifdef _WIN32

    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring ws(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), len);


    return ws;
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;

    return conv.from_bytes(s);
#endif
}



std::string narrow_utf8(const std::wstring& s) {
#ifdef _WIN32

    if (s.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);

    return out;
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;


    return conv.to_bytes(s);
#endif
}



std::string path_utf8(const fs::path& p) {
#if defined(__cpp_char8_t)
    auto u = p.u8string();

    return std::string(reinterpret_cast<const char*>(u.data()), u.size());
#else

    return p.u8string();
#endif
}



std::string getenv_utf8_or_empty(const char* name) {

    if (!name || !*name) return {};
#ifdef _WIN32
    char* raw = nullptr;
    size_t len = 0;

    const errno_t rc = _dupenv_s(&raw, &len, name);

    if (rc != 0 || !raw) {

        if (raw) std::free(raw);

        return {};
    }
    const size_t n = (len > 0 && raw[len - 1] == '\0') ? len - 1 : std::strlen(raw);
    std::string value(raw, n);
    std::free(raw);

    return value;
#else
    const char* value = std::getenv(name);

    return (value && *value) ? std::string(value) : std::string();
#endif
}



std::wstring getenv_wstring_or_empty(const wchar_t* name) {

    if (!name || !*name) return {};

#ifdef _WIN32
    wchar_t* raw = nullptr;

    size_t len = 0;
    const errno_t rc = _wdupenv_s(&raw, &len, name);

    if (rc != 0 || !raw) {

        if (raw) std::free(raw);

        return {};
    }
    const size_t n = (len > 0 && raw[len - 1] == L'\0') ? len - 1 : std::wcslen(raw);
    std::wstring value(raw, n);
    std::free(raw);

    return value;
#else
    (void)name;

    return {};

#endif
}



fs::path getenv_path_utf8(const char* name) {
    const std::string value = getenv_utf8_or_empty(name);

    if (value.empty()) return {};
#ifdef _WIN32

    return fs::path(widen_utf8(value));
#else

    return fs::path(value);
#endif
}



fs::path executable_dir() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');

    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

    if (len > 0 && len < buffer.size()) {


        buffer.resize(len);

        return fs::path(buffer).parent_path();
    }
#endif
    std::error_code ec;

    fs::path cwd = fs::current_path(ec);

    return ec ? fs::path(".") : cwd;
}



static fs::path env_path_utf8(const char* name) {

    return getenv_path_utf8(name);
}



static bool looks_like_project_root(const fs::path& p) {

    return fs::exists(p / "tlalpowa" / "tlalpowa_datos.json") ||

           fs::exists(p / "TLALPOWA" / "tlalpowa_datos.json") ||

           fs::exists(p / "Fuente" / "Tlalpowa" / "tlalpowa_datos.json") ||

           fs::exists(p / "Fuente" / "Tlalpowa" / "diseases.tsv") ||

           fs::exists(p / "compilepushpull.cmd") ||

           fs::exists(p / "rutas_directorio.txt") ||

           fs::exists(p / "config" / "tlalpowa_datos.json") ||

           fs::exists(p / "config" / "diseases.tsv");
}




fs::path project_root() {

    if (auto p = env_path_utf8("OBSERVATORIO_ZMVM_ROOT"); !p.empty()) return p;


    if (auto p = env_path_utf8("CDMX_EPIATMOS_ROOT"); !p.empty()) return p;

    std::vector<fs::path> starts;
    std::error_code ec;

    starts.push_back(fs::current_path(ec));

    starts.push_back(executable_dir());

    for (const auto& start : starts) {

        if (start.empty()) continue;

        for (fs::path cur = start; !cur.empty(); cur = cur.parent_path()) {

            if (looks_like_project_root(cur)) return cur;

            if (cur == cur.parent_path()) break;
        }
    }

    return executable_dir();
}



fs::path first_existing_project_path(std::initializer_list<fs::path> paths) {

    for (const auto& p : paths) {
        std::error_code ec;


        if (!p.empty() && fs::exists(p, ec) && !ec) return p;
    }


    return paths.size() > 0 ? *paths.begin() : fs::path{};
}



static bool consolidated_relative_path_ok(const std::string& name) {

    if (name.empty() || name.size() > 240) return false;
    if (name.find(':') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    if (!fs::path(name).is_relative()) return false;
    for (unsigned char c : name) {
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}



static std::vector<unsigned char> consolidated_base64_decode(const std::string& text) {

    signed char table[256];
    for (int i = 0; i < 256; ++i) table[i] = -1;
    for (int i = 'A'; i <= 'Z'; ++i) table[i] = static_cast<signed char>(i - 'A');
    for (int i = 'a'; i <= 'z'; ++i) table[i] = static_cast<signed char>(26 + i - 'a');
    for (int i = '0'; i <= '9'; ++i) table[i] = static_cast<signed char>(52 + i - '0');
    table[static_cast<unsigned char>('+')] = 62;
    table[static_cast<unsigned char>('/')] = 63;

    std::vector<unsigned char> out;
    out.reserve((text.size() * 3u) / 4u + 3u);
    int value = 0;
    int bits = -8;
    for (unsigned char c : text) {
        if (c == '=' || c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
        const signed char d = table[c];
        if (d < 0) return {};
        value = (value << 6) | d;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<unsigned char>((value >> bits) & 0xff));
            bits -= 8;
        }
    }
    return out;
}



static void write_binary_file_checked(const fs::path& p, const std::vector<unsigned char>& bytes) {

    ensure_dir(p.parent_path());
    fs::path tmp = p;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("No se pudo escribir archivo temporal: " + path_utf8(tmp));
        if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out) throw std::runtime_error("No se pudo cerrar archivo temporal: " + path_utf8(tmp));
    }
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) {
        fs::remove(p, ec);
        ec.clear();
        fs::rename(tmp, p, ec);
    }
    if (ec) throw std::runtime_error("No se pudo publicar archivo atomico: " + path_utf8(p) + " (" + ec.message() + ")");
}



static fs::path consolidated_bundle_path() {

    const fs::path root = project_root();
    const auto candidates = std::array{
        root / "tlalpowa" / "tlalpowa_datos.json",
        root / "TLALPOWA" / "tlalpowa_datos.json",
        root / "Fuente" / "Tlalpowa" / "tlalpowa_datos.json",
        root / "config" / "tlalpowa_datos.json",
        executable_dir() / "tlalpowa" / "tlalpowa_datos.json",
        executable_dir() / "TLALPOWA" / "tlalpowa_datos.json",
        executable_dir() / "Fuente" / "Tlalpowa" / "tlalpowa_datos.json",
        executable_dir() / "config" / "tlalpowa_datos.json"
    };
    for (const fs::path& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && !ec && fs::is_regular_file(p, ec)) return p;
    }
    return {};
}



static fs::path consolidated_runtime_config_root() {

    if (auto p = env_path_utf8("LOCALAPPDATA"); !p.empty()) {
        return p / "MiausoftSuite" / "Tlalpowa" / "runtime" / "config_consolidada";
    }
    return project_root() / "datos" / ".runtime" / "config_consolidada";
}



static bool consolidated_required_files_exist(const fs::path& root) {

    std::error_code ec;
    return fs::exists(root / "diseases.tsv", ec) && !ec &&
           fs::exists(root / "jurisdictions.tsv", ec) && !ec &&
           fs::exists(root / "tlalpowa_territorial.json", ec) && !ec &&
           fs::exists(root / "zmvm.geojson", ec) && !ec &&
           fs::exists(root / "metro_cronologia_red.json", ec) && !ec &&
           fs::exists(root / "movilidad_red_integrada_base.jsonl", ec) && !ec;
}



static bool materialize_consolidated_config() {

    static std::mutex mutex;
    static bool checked = false;
    static bool ok = false;
    std::lock_guard<std::mutex> lock(mutex);
    if (checked) return ok && consolidated_required_files_exist(consolidated_runtime_config_root());
    checked = true;

    const fs::path bundle_path = consolidated_bundle_path();
    if (bundle_path.empty()) return false;

    try {
        const auto bundle = nlohmann::json::parse(read_text_file(bundle_path), nullptr, false);
        if (!bundle.is_object() || bundle.value("schema", std::string{}) != "tlalpowa.datos.consolidados.v1") return false;
        if (!bundle.contains("archivos") || !bundle["archivos"].is_array()) return false;

        const std::string bundle_id = bundle.value("id", std::string{});
        const fs::path root = consolidated_runtime_config_root();
        const fs::path manifest = root / "_tlalpowa_datos_manifest.json";
        if (!bundle_id.empty() && consolidated_required_files_exist(root)) {
            try {
                const auto current = nlohmann::json::parse(read_text_file(manifest), nullptr, false);
                if (current.is_object() && current.value("id", std::string{}) == bundle_id) { ok = true; return true; }
            } catch (...) {}
        }

        std::error_code ec;
        fs::remove_all(root, ec);
        ensure_dir(root);
        int written = 0;
        for (const auto& item : bundle["archivos"]) {
            if (!item.is_object()) return false;
            const std::string rel = item.value("ruta", std::string{});
            const std::string encoding = item.value("encoding", std::string{});
            const std::string content = item.value("contenido", std::string{});
            if (!consolidated_relative_path_ok(rel)) return false;
            const fs::path dst = root / fs::path(rel);
            if (encoding == "base64") {
                std::vector<unsigned char> bytes = consolidated_base64_decode(content);
                if (bytes.empty() && !content.empty()) return false;
                write_binary_file_checked(dst, bytes);
            } else if (encoding == "utf8") {
                write_text_file(dst, content);
            } else {
                return false;
            }
            ++written;
        }

        nlohmann::json m;
        m["id"] = bundle_id;
        m["archivos"] = written;
        m["fuente"] = path_utf8(bundle_path);
        write_text_file(manifest, m.dump(2) + "\n");
        ok = consolidated_required_files_exist(root);
        return ok;
    } catch (...) {
        return false;
    }
}



static fs::path legacy_or_bundle_config_root(const fs::path& base) {

    if (base.empty()) return {};
    std::error_code ec;
    if (fs::exists(base / "tlalpowa_datos.json", ec) && !ec) return base;
    if (fs::exists(base / "diseases.tsv", ec) && !ec) return base;
    if (fs::exists(base / "zmvm.geojson", ec) && !ec) return base;
    return {};
}



fs::path config_root() {

    if (materialize_consolidated_config()) return consolidated_runtime_config_root();

    const fs::path root = project_root();
    const auto candidates = std::array{
        root / "tlalpowa",
        root / "TLALPOWA",
        root / "Fuente" / "Tlalpowa",
        root / "config",
        executable_dir() / "tlalpowa",
        executable_dir() / "TLALPOWA",
        executable_dir() / "Fuente" / "Tlalpowa",
        executable_dir() / "config"
    };

    for (const fs::path& p : candidates) {
        const fs::path usable = legacy_or_bundle_config_root(p);
        if (!usable.empty()) return usable;
    }

    return root / "tlalpowa";
}



fs::path internal_data_root() {

    const fs::path root = project_root();
    const fs::path nuevo = root / "datos";
    if (fs::exists(nuevo) || !fs::exists(root / "TLALPOWA" / "Datos")) return nuevo;
    return root / "TLALPOWA" / "Datos";
}




fs::path external_data_root() {

    const fs::path root = project_root();
    const fs::path nuevo = root / "descargas";
    if (fs::exists(nuevo) || !fs::exists(root / "TLALPOWA" / "Descargas")) return nuevo;
    return root / "TLALPOWA" / "Descargas";
}





Logger::Logger(fs::path log_file) {

    ensure_dir(log_file.parent_path());

    out_.open(log_file, std::ios::binary | std::ios::app);
}


void Logger::info(const std::string& m) { line("INFO", m); }

void Logger::warn(const std::string& m) { line("WARN", m); }

void Logger::error(const std::string& m) { line("ERROR", m); }




void Logger::line(const std::string& level, const std::string& m) {


    std::lock_guard<std::mutex> lock(mu_);
    const std::string s = now_utc_iso() + " [" + level + "] " + m;
    std::cout << s << std::endl;

    if (out_) out_ << s << '\n';
}

}

// ===== Nucleos/Writers.impl =====
#line 1 "Nucleos/Writers.impl"


#include <chrono>



namespace epi {




OutputStore::OutputStore(fs::path root) : root_(std::move(root)), write_root_(root_) {}



namespace {



constexpr size_t kEpidemiologyImportFlushRows = 384;
constexpr size_t kExpectedCdmxObservationKeys = 131072;
constexpr size_t kEpidemiologySessionKeyCap = 131072;


int epidemiology_live_flush_interval_ms() {
    const std::string raw = getenv_utf8_or_empty("TLALPOWA_EPI_LIVE_FLUSH_MS");
    if (!raw.empty()) {
        try {
            const int v = std::stoi(raw);
            if (v >= 0 && v <= 5000) return v;
        } catch (...) {
        }
    }
    return 90;
}

constexpr size_t kEpidemiologyLiveFlushRows = 896;

void epidemiology_session_key_insert_bounded(std::unordered_set<std::uint64_t>& keys, std::uint64_t key) {
    // El antirrepetición pesado vive en IXIPTLAH y se alimenta por shard; el
    // OutputStore sólo retiene una ventana caliente intra-sesión para cortar
    // duplicados inmediatos sin reservar millones de celdas hash residentes.
    if (keys.size() >= kEpidemiologySessionKeyCap) {
        keys.clear();
        keys.reserve(kExpectedCdmxObservationKeys);
    }
    keys.insert(key);
}




bool is_epidemiology_ixiptlah_record(IxiptlahRecordType type, std::uint32_t schema) {


    return type == IxiptlahRecordType::MonthlyEpidemiologyBatch && schema == 1;
}




std::vector<fs::path> monthly_ixiptlah_files_in_exact_dir(const fs::path& dir) {


    std::vector<fs::path> out;
    std::error_code ec;


    if (!fs::exists(dir, ec) || ec) return out;


    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
        std::error_code item_ec;


        if (it->is_regular_file(item_ec) && !item_ec && lower_ascii(path_utf8(it->path().extension())) == kIxiptlahExtension) {


            out.push_back(it->path());
        }
    }


    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {


        return path_utf8(a.filename()) < path_utf8(b.filename());


    });


    return out;
}




bool replace_file_with_ready_file(const fs::path& ready, const fs::path& target) {
    std::error_code ec;


    ensure_dir(target.parent_path());


    fs::rename(ready, target, ec);


    if (!ec) return true;


    ec.clear();


    fs::copy_file(ready, target, fs::copy_options::overwrite_existing, ec);


    if (!ec) {


        std::error_code cleanup_ec;


        fs::remove(ready, cleanup_ec);


        return true;
    }


    ec.clear();
    fs::remove(target, ec);


    if (ec) return false;


    ec.clear();


    fs::rename(ready, target, ec);


    return !ec;
}




std::string entity_code_from_text(const std::string& text) {


    const std::string n = normalize_key(text);


    if (n == "hgo" || n.find("hidalgo") != std::string::npos || n.find(" hgo ") != std::string::npos) return "HGO";


    if (n.find("estado de mexico") != std::string::npos || n.find("edomex") != std::string::npos) return "MEX";


    if (n == "mex") return "MEX";


    if (n == "cmx" || n.find("ciudad de mexico") != std::string::npos || n.find("cdmx") != std::string::npos || n.find("cmx") != std::string::npos) return "CMX";


    return "CMX";
}




std::string entity_code_for_observation(const Observation& o) {


    if (o.pdf_id.size() == 3) return entity_code_from_text(" " + o.pdf_id + " ");


    return entity_code_from_text(o.pdf_file + " " + o.table_id + " " + o.pdf_id);
}




std::string observation_key(const Observation& o) {


    const std::string disease_key = o.disease_id.empty() ? normalize_key(o.disease) : o.disease_id;


    const std::string jurisdiction_key = o.jurisdiction_id.empty() ? normalize_key(o.jurisdiction) : o.jurisdiction_id;


    return entity_code_for_observation(o) + "|" + std::to_string(o.bulletin_year) + "|" + std::to_string(o.bulletin_week) + "|" +
           std::to_string(o.page) + "|" + disease_key + "|" + jurisdiction_key + "|" +
           o.period + "|" + o.sex + "|" + std::to_string(o.value);
}




std::string quarantine_key(const QuarantineItem& q) {


    return q.pdf_id + "|" + std::to_string(q.page) + "|" + q.table_id + "|" +


           q.column_key + "|" + q.reason + "|" + q.detail;
}





std::uint64_t fnv1a64(const std::string& s) noexcept {
    std::uint64_t h = 1469598103934665603ull;


    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }


    return h ? h : 1ull;
}




std::uint64_t observation_key_hash(const Observation& o) {


    return fnv1a64(observation_key(o));
}




std::uint64_t quarantine_key_hash(const QuarantineItem& q) {


    return fnv1a64(quarantine_key(q));
}




std::string observation_metric(const Observation& o) {


    if (o.period == "Sem") return "incidencia_semanal";


    if (o.period == "SemDerivada") return "incidencia_semanal_derivada_sexo";


    if (o.period == "Acum" && o.source_year != std::to_string(o.bulletin_year)) return "acumulado_anio_anterior";


    if (o.period == "Acum" && (o.sex == "M" || o.sex == "F")) return "acumulado_anual_por_sexo";


    if (o.period == "Acum") return "acumulado_anual";


    return "valor";
}




int year_from_source_text(const std::string& text) {


    for (size_t i = 0; i + 3 < text.size(); ++i) {


        if (!std::isdigit(static_cast<unsigned char>(text[i])) ||
            !std::isdigit(static_cast<unsigned char>(text[i + 1])) ||
            !std::isdigit(static_cast<unsigned char>(text[i + 2])) ||

            !std::isdigit(static_cast<unsigned char>(text[i + 3]))) continue;
        const int y = std::stoi(text.substr(i, 4));


        if (y >= 1800 && y <= 2300) return y;
    }


    return 0;
}




/* TLALPOWA-FUSION: se retiró el header_value local de Writers.impl.
   En unidad fusionada duplicaba la rutina interna homónima de TemporalBlocks.impl;
   además no tenía llamadas dentro de Writers.impl. */

std::string compact_id_from_name(const std::string& name) {


    std::string key = normalize_key(name);


    for (char& c : key) {


        if (c == ' ') c = '_';
    }


    return key;
}




std::string normalized_hour(const std::string& hour) {


    if (hour.size() >= 2 && std::isdigit(static_cast<unsigned char>(hour[0])) && std::isdigit(static_cast<unsigned char>(hour[1]))) return hour.substr(0, 2);


    return "00";
}




void remove_legacy_generated_products(const fs::path& root) {
    std::error_code ec;


    if (!fs::exists(root, ec) || ec) return;


    const std::set<std::string> legacy_names = {


        "observations.csv", "observations.jsonl", "observaciones.csv", "observaciones.jsonl",


        "quarantine.csv", "tables.jsonl", "cdmx_epiatmos.sqlite.sql", "cdmx_epiatmos.sql",


        "resultados.json", "observaciones_completas.json", "DatosEpidemiologicosZMVM.json",


        "web_observaciones.jsonl", "web_observaciones_ligero.jsonl", "web_observaciones_ligero_core.tsv",


        "observaciones_en_vivo.jsonl", "observaciones_en_vivo_ligero.jsonl", "web_manifest.json",


        "semanas.json", "web_semanas.json", "web_zmvm.geojson", "HistorialEpidemiologicoCDMX_generado.csv",


        "observaciones_derivadas_sexo.jsonl", "atmosfera_archivos.json", "web_atmosfera_archivos.json"
    };


    const std::set<std::string> legacy_dirs = {"accepted", "base", "web", "progress", "quarantine", "decadas", "_last_good"};


    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {


        if (ec) { ec.clear(); continue; }


        const std::string name = path_utf8(it->path().filename());
        std::error_code item_ec;


        if (it->is_directory(item_ec) && !item_ec && legacy_dirs.count(name)) {


            fs::remove_all(it->path(), ec);


            ec.clear();
            continue;
        }


        if (it->is_regular_file(item_ec) && !item_ec) {


            const std::string ext = lower_ascii(path_utf8(it->path().extension()));


            if (ext == ".tsv" || ext == ".csv" || ext == ".json" || ext == ".jsonl" ||


                legacy_names.count(name) || (name.rfind("tokens_", 0) == 0 && ext == ".jsonl") ||


                (name.rfind("web_observaciones_", 0) == 0 && (ext == ".jsonl" || ext == ".json" || ext == ".tsv"))) {


                fs::remove(it->path(), ec);


                ec.clear();
            }
        }
    }
}

}




void OutputStore::open(bool resume) {
    ensure_dir(root_);
    remove_legacy_generated_products(root_);


    epidemiology_batch_buffer_.clear();
    staged_epidemiology_rebuild_ = false;


    epidemiology_rebuild_root_.clear();


    write_root_ = root_;




    observation_keys_.clear();
    observation_keys_.reserve(kExpectedCdmxObservationKeys);


    if (!resume) {



        staged_epidemiology_rebuild_ = false;
        epidemiology_rebuild_root_ = root_ / "_epidemiology_rebuild_tmp";
        std::error_code ec;

        fs::remove_all(epidemiology_rebuild_root_, ec);


        ec.clear();


        epidemiology_rebuild_root_.clear();


        write_root_ = root_;
        temporal_close_append_streams();
        (void)temporal_purge_epidemiology_records(root_);


        for (const auto& p : temporal_tsv_files(root_)) fs::remove(p, ec);


        for (const auto& p : temporal_csv_files(root_)) fs::remove(p, ec);


        for (const auto& p : temporal_json_files(root_)) fs::remove(p, ec);
    } else {
        // IXIPTLAH-SM v9 evita el barrido de reanudación: leer todos los shards
        // epidemiológicos sólo para poblar una tabla hash de salida duplicaba RAM
        // y convertía una importación larga en O(histórico + nuevo). La dedupe
        // exacta contra disco ocurre dentro de temporal_filter_new_epidemiology_rows().
        load_existing_observations();
        load_existing_quarantine();
    }
    opened_ = true;
}




void OutputStore::load_existing_observations() {
    observation_count_ = 0;
    observation_keys_.clear();
    observation_keys_.reserve(kExpectedCdmxObservationKeys);
    // No se rehidratan claves históricas en RAM. El lector IXIPTLAH ya deduplica
    // contra cada archivo elemental al anexar, y cargar millones de claves aquí
    // atrasaba la captura antes incluso de procesar la primera página nueva.
}




void OutputStore::load_existing_quarantine() {
    quarantine_count_ = 0;


    quarantine_keys_.clear();
}




void OutputStore::write_observation(const Observation& o) {
    const std::uint64_t key = observation_key_hash(o);


    if (observation_keys_.find(key) != observation_keys_.end()) return;


    TemporalEpidemiologyRecord r;
    r.entity = entity_code_for_observation(o);


    r.year = o.bulletin_year;


    r.epi_week = o.bulletin_week;
    r.page = o.page;
    r.disease = o.disease;

    r.cie10 = o.cie10;
    r.jurisdiction = o.jurisdiction;
    r.period = o.period;

    r.sex = o.sex.empty() ? (o.period == "Sem" ? "total" : o.sex) : o.sex;
    r.value = o.value;


    epidemiology_batch_buffer_.push_back(std::move(r));


    if (epidemiology_batch_buffer_.size() >= kEpidemiologyImportFlushRows) {
        flush_epidemiology_batch_buffer();
    }


    epidemiology_session_key_insert_bounded(observation_keys_, key);
    ++observation_count_;
}




void OutputStore::write_quarantine(const QuarantineItem& q) {
    const std::uint64_t key = quarantine_key_hash(q);


    if (quarantine_keys_.find(key) != quarantine_keys_.end()) return;




    quarantine_keys_.insert(key);
    ++quarantine_count_;
}




void OutputStore::write_tokens_jsonl(const PdfDocument&) {

}




void OutputStore::append_table(const TableCandidate& t) {


    if (!opened_) throw std::runtime_error("OutputStore no abierto.");


    epidemiology_batch_buffer_.reserve(std::min<size_t>(


        kEpidemiologyImportFlushRows + t.accepted.size(), kEpidemiologyImportFlushRows * 2));


    for (const auto& o : t.accepted) {
        const std::uint64_t key = observation_key_hash(o);


        if (observation_keys_.find(key) != observation_keys_.end()) continue;


        TemporalEpidemiologyRecord r;
        r.entity = entity_code_for_observation(o);


        r.year = o.bulletin_year;


        r.epi_week = o.bulletin_week;
        r.page = o.page;
        r.disease = o.disease;

        r.cie10 = o.cie10;
        r.jurisdiction = o.jurisdiction;
        r.period = o.period;

        r.sex = o.sex.empty() ? (o.period == "Sem" ? "total" : o.sex) : o.sex;
        r.value = o.value;


        epidemiology_batch_buffer_.push_back(std::move(r));


        epidemiology_session_key_insert_bounded(observation_keys_, key);
        ++observation_count_;
    }




    if (!t.accepted.empty() && !epidemiology_batch_buffer_.empty()) {
        const auto now = std::chrono::steady_clock::now();
        const int live_ms = epidemiology_live_flush_interval_ms();
        const bool first_live_flush = last_epidemiology_live_flush_.time_since_epoch().count() == 0;
        const bool enough_rows = epidemiology_batch_buffer_.size() >= kEpidemiologyLiveFlushRows;
        const bool enough_time = first_live_flush || live_ms == 0 ||
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_epidemiology_live_flush_).count() >= live_ms;
        if (enough_rows || enough_time) {
            // Publicación viva amortizada: el primer bloque aparece inmediatamente;
            // los siguientes se compactan por ventana corta para evitar tormentas de
            // append/flush que saturen disco, heap e índices IXIPTLAH en boletines largos.
            flush_epidemiology_batch_buffer();
            temporal_flush_append_streams_if_due(120);
            last_epidemiology_live_flush_ = now;
        }
    }


    for (const auto& q : t.quarantine) write_quarantine(q);
    const std::string force_flush = getenv_utf8_or_empty("TLALPOWA_FLUSH_EVERY_TABLE");


    if (!force_flush.empty() && force_flush != "0") {
        flush_epidemiology_batch_buffer();
        temporal_flush_append_streams();
    }
}




void OutputStore::flush_epidemiology_batch_buffer() {


    if (epidemiology_batch_buffer_.empty()) return;




    std::vector<TemporalEpidemiologyRecord> batch;
    batch.swap(epidemiology_batch_buffer_);


    if (staged_epidemiology_rebuild_) {


        temporal_append_epidemiology_records_batch_exact_root(write_root_, batch);
    } else {


        temporal_append_epidemiology_records_batch(root_, batch);
    }
}




void OutputStore::commit_epidemiology_rebuild() {


    if (!staged_epidemiology_rebuild_ || epidemiology_rebuild_root_.empty()) return;



    temporal_close_append_streams();


    const std::vector<fs::path> staged_files = monthly_ixiptlah_files_in_exact_dir(epidemiology_rebuild_root_);


    for (const fs::path& staged_file : staged_files) {


        const fs::path target = root_ / staged_file.filename();


        const fs::path ready = fs::path(target.wstring() + L".epi_ready");
        std::error_code ec;


        fs::remove(ready, ec);


        ec.clear();


        if (fs::exists(target, ec) && !ec && file_size_or_zero(target) > 0) {


            fs::copy_file(target, ready, fs::copy_options::overwrite_existing, ec);


            if (ec) throw std::runtime_error("No se pudo preparar fusion ixiptlah: " + path_utf8(target));


            const IxiptlahRewriteStats purge = ixiptlah_rewrite_without_records(ready, is_epidemiology_ixiptlah_record);


            if (purge.removed > 0 && !purge.rewritten) {


                throw std::runtime_error("No se pudo limpiar epidemiologia en fusion ixiptlah: " + path_utf8(target));
            }
        }


        const IxiptlahCopyStats copied = ixiptlah_append_selected_records_raw(ready, staged_file, is_epidemiology_ixiptlah_record);


        if (copied.copied == 0 || copied.unreadable > 0 || !copied.target_touched) {


            throw std::runtime_error("No se pudo copiar el nucleo epidemiologico nuevo hacia ixiptlah: " + path_utf8(target));
        }


        ixiptlah_close_all();


        if (!replace_file_with_ready_file(ready, target)) {


            throw std::runtime_error("No se pudo publicar fusion ixiptlah: " + path_utf8(target));
        }
    }

    std::error_code ec;
    fs::remove_all(epidemiology_rebuild_root_, ec);
    staged_epidemiology_rebuild_ = false;


    epidemiology_rebuild_root_.clear();


    write_root_ = root_;
}




void OutputStore::flush_streams() {


    // Boletines epidemiológicos: los aceptados se vuelcan a IXIPTLAH conforme
    // aparecen. El buffer sólo amortigua micro-lotes; nunca debe retener una
    // corrida completa ni depender de finalize() para existir en Datos.
    flush_epidemiology_batch_buffer();
    temporal_flush_append_streams_if_due(350);
}




void OutputStore::flush_live_outputs(const fs::path&, bool) {
    // Vacía únicamente buffers ya materializados. No indexa ni reabre IXIPTLAH:
    // esta función puede llamarse desde UI sin convertir el cierre en segunda pasada.
    flush_epidemiology_batch_buffer();
    flush_streams();
}



void OutputStore::write_derived_outputs(const fs::path&) {
    // Derivados obsoletos deliberadamente inertes: la ruta canónica es IXIPTLAH
    // preformado, append-only y consumible sin JSON intermedio.
    flush_epidemiology_batch_buffer();
}



void OutputStore::finalize() {
    // Cierre O(1) respecto al número de boletines ya leídos: solo se vuelcan
    // lotes pendientes y se cierran streams; ningún archivo histórico se recorre otra vez.
    flush_epidemiology_batch_buffer();
    commit_epidemiology_rebuild();
    temporal_flush_append_streams();
    temporal_close_append_streams();
}



void OutputStore::write_obs_csv_header() {}



void OutputStore::write_quarantine_csv_header() {}



void OutputStore::write_sql_header() {}



void OutputStore::write_results_json() const {}



void OutputStore::write_master_csv() const {}



void OutputStore::write_derived_sex_incidence() const {}



void OutputStore::write_atmospheric_inventory() const {}

}

// ===== Ixiptlah.c =====
#line 1 "Ixiptlah.c"
/* Núcleo visible de datos: lectura, escritura, representación y exportación IXIPTLAH.
   Se compila como C++ por integración histórica con STL, pero la frontera C queda en core.c. */

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <memory>
#include <streambuf>
#include <thread>
#include <unordered_map>
#include <unordered_set>



namespace epi {


namespace {



constexpr char kIxiptlahMagic[8] = {'I','X','I','P','T','L','A','H'};


constexpr std::uint32_t kIxiptlahFileVersionLegacy = 0;

// IXIPTLAH V1 es el único formato operativo desde este corte: encabezado
// autosuficiente con capa/tiempo y payload directo. La lectura rechaza versiones
// previas para no pagar ramas retrocompatibles ni sostener núcleos obsoletos.
constexpr std::uint32_t kIxiptlahFileVersion = 1;
constexpr std::uint32_t kIxiptlahFrameSync = 0x00001000u;
constexpr std::uint64_t kMaxPayloadBytes = 512ull * 1024ull * 1024ull;
constexpr std::uint32_t kCodecRaw = 0;
constexpr std::uint32_t kCodecIxLz = 1;
constexpr std::uint32_t kCodecIxLzBlocks = 2;
constexpr std::uint32_t kIxiptlahBlockCodecRaw = 0;
constexpr std::uint32_t kIxiptlahBlockCodecIxLz = 1;
constexpr std::uint32_t kIxiptlahCompressionBlockBytes = 256u * 1024u;
constexpr std::uint64_t kIxiptlahHardFileTargetBytes = 90ull * 1024ull * 1024ull;

// IXIPTLAH-SM V1 usa, por omisión, un único .ixiptlah por elemento del
// catálogo. El índice lateral histórico queda dormido salvo activación manual:
// no debe aparecer ningún tipo nuevo de archivo durante una importación normal.
constexpr char kIxiptlahSmIndexMagic[8] = {'I','X','S','M','I','D','X','1'};
constexpr std::uint32_t kIxiptlahSmIndexVersion = 1;
constexpr std::uint32_t kIxiptlahSmIndexEntryBytes = 48;

// Directorio terminal embebido IXIPTLAH V1. No es un sidecar: vive al final
// del mismo archivo y permite cargar offsets/capas/tiempos con un unico seek al
// EOF. Si el archivo se abre para append, el directorio se recorta antes de
// escribir nuevos registros y se vuelve a emitir al cerrar.
constexpr char kIxiptlahDirMagic[8] = {'I','X','D','I','R','V','1','A'};
constexpr char kIxiptlahDirEndMagic[8] = {'I','X','D','I','R','E','N','D'};
constexpr std::uint32_t kIxiptlahDirVersion = 1;
constexpr std::uint32_t kIxiptlahDirEntryBytes = 104;
constexpr std::uint64_t kIxiptlahDirHeaderBytes = 24ull;
constexpr std::uint64_t kIxiptlahDirTrailerBytes = 40ull;





struct IxiptlahRecordEnvelope {


    IxiptlahRecordType type = IxiptlahRecordType::EpidemiologyObservation;


    std::uint32_t schema = 0;

    std::uint64_t stored_size = 0;
    std::uint64_t raw_size = 0;

    std::uint32_t codec = kCodecRaw;
    std::uint64_t layer_hash = 0;
    std::uint64_t temporal_key = 0;
};




struct IxiptlahIndexedRecord {


    IxiptlahRecordType type = IxiptlahRecordType::EpidemiologyObservation;


    std::uint32_t schema = 0;
    std::uint64_t payload_offset = 0;
    std::uint64_t stored_size = 0;
    std::uint64_t raw_size = 0;
    std::uint32_t codec = kCodecRaw;
    std::uint64_t layer_hash = 0;
    std::uint64_t temporal_key = 0;

    // Direcciones preformadas V1 dentro del directorio terminal. No duplican
    // payload: guardan sólo cubetas temporales ya normalizadas para que cada
    // consulta entre por enteros directos, sin reinterpretar fechas ni cadenas.
    std::uint64_t narrow_bucket = 0;
    std::uint64_t hour_bucket = 0;
    std::uint64_t week_bucket = 0;
    std::uint64_t wide_bucket = 0;
    std::uint32_t core_group = 0;
    std::uint32_t quality_flags = 0;
};




struct IxiptlahFileIndex;


struct IxiptlahSink {

    fs::path path;


    std::ofstream stream;
    std::vector<char> io_buffer;


    std::uint32_t version = kIxiptlahFileVersion;
    std::uint64_t last_used = 0;

    // Índice vivo del archivo abierto. Evita que una lectura inmediata tras
    // importar RAMA/REDMA/RUOA tenga que escanear todo el núcleo de década; el
    // flush se paga una sola vez al consultar, no por cada registro capturado.
    std::vector<IxiptlahIndexedRecord> live_records;
    bool live_index_valid = true;

    // Snapshot residente del directorio vivo ya bucketizado. Una lectura durante
    // importacion no debe reconstruir buckets por cada clic: si no hubo nuevos
    // append, se reutiliza el mismo índice inmutable y la ruta queda en enteros.
    std::uint64_t live_revision = 0;
    std::uint64_t live_snapshot_revision = std::numeric_limits<std::uint64_t>::max();
    std::shared_ptr<const IxiptlahFileIndex> live_snapshot;
};




struct IxiptlahSmIndexHeader {
    std::uint32_t version = 0;
    std::uint32_t entry_size = 0;
    std::uint64_t ix_file_size = 0;
    std::int64_t ix_mtime_ns = 0;
    std::uint64_t record_count = 0;
};

struct IxiptlahSmIndexEntry {
    IxiptlahRecordType type = IxiptlahRecordType::EpidemiologyObservation;
    std::uint32_t schema = 0;
    std::uint64_t layer_hash = 0;
    std::uint64_t payload_offset = 0;
    std::uint64_t stored_size = 0;
    std::uint64_t raw_size = 0;
    std::uint32_t codec = kCodecRaw;
    std::uint32_t flags = 0;
};




struct IxiptlahFileIndex {


    std::uint64_t file_size = 0;

    std::int64_t mtime_ns = 0;

    std::uint32_t file_version = 0;


    std::vector<IxiptlahIndexedRecord> records;

    // Buckets residentes type+schema+layer -> índices físicos. El payload no se
    // duplica: sólo se evita recorrer todo el directorio cuando la vista ya sabe
    // qué red/capa/mes pidió. Se reconstruye desde el directorio embebido, índice
    // vivo o escaneo lineal y queda acotado por el mismo cache de IxiptlahFileIndex.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_buckets;
    // Buckets type+schema+layer+bucket_temporal. El bucket temporal es
    // temporal_key/10000: YYYYMMDD para atmósfera y YYYYWW para epidemiología.
    // Permite servir una semana sin revisar toda la capa dentro del shard anual
    // o decenal; el payload sigue intacto y el filtro final verifica límites.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_time_buckets;
    // Buckets type+schema+layer+hora. Sólo se llenan para llaves atmosféricas
    // YYYYMMDDHHMM; sirven al visor puntual sin abrir buckets diarios completos.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_hour_buckets;
    // Buckets type+schema+layer+semana interna mensual. La unidad física actual
    // es el archivo AAAA_MM_DD.ixiptlah; este índice evita caer a capa completa
    // cuando el bloque consultado es precisamente una semana.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_week_buckets;
    // Buckets type+schema+layer+mes/logro epidemiologico. Para atmosfericos el
    // bucket es YYYYMM; para epidemiologia, YYYY. Las vistas anuales y mensuales
    // evitan recorrer toda la capa cuando el rango supera pocos dias.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_month_buckets;
    // Núcleos internos: no cambian el formato físico ni duplican payload. Separan
    // familias lógicas para que una consulta epidemiológica no toque buffers
    // atmosféricos y viceversa cuando el IXIPTLAH semanal contiene todo el bloque.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fast_core_buckets;
    bool valid = false;
};





std::mutex& ixiptlah_mu() {


    static std::mutex mu;

    return mu;
}




std::unordered_map<std::string, std::unique_ptr<IxiptlahSink>>& ixiptlah_sinks() {


    static std::unordered_map<std::string, std::unique_ptr<IxiptlahSink>> sinks;

    return sinks;
}




std::uint64_t& ixiptlah_tick() {
    static std::uint64_t tick = 0;

    return tick;
}




std::mutex& ixiptlah_index_mu() {


    static std::mutex mu;

    return mu;
}




std::unordered_map<std::string, IxiptlahFileIndex>& ixiptlah_index_cache() {


    static std::unordered_map<std::string, IxiptlahFileIndex> cache;

    return cache;
}


std::unordered_map<std::string, std::shared_ptr<const IxiptlahFileIndex>>& ixiptlah_index_shared_cache() {

    static std::unordered_map<std::string, std::shared_ptr<const IxiptlahFileIndex>> cache;

    return cache;
}


bool env_truthy_ix(const char* name);
bool ixiptlah_raw_type_is_known(std::uint32_t raw_type);
void ixiptlah_fill_preformed_address_fields(IxiptlahIndexedRecord& rec);
bool ixiptlah_read_file_header(std::istream& in, std::uint32_t& version);
bool ixiptlah_scan_file_index(const fs::path& path, IxiptlahFileIndex& index);
std::shared_ptr<const IxiptlahFileIndex> ixiptlah_live_index_for_open_sink(const fs::path& path);
void ixiptlah_invalidate_index_cache(const fs::path& path);
void ixiptlah_payload_cache_erase_path(const fs::path& path);
void ixiptlah_configure_hot_read_buffer(std::ifstream& in);
std::ifstream ixiptlah_open_binary_input(const fs::path& path);




std::int64_t ixiptlah_mtime_ns(const fs::path& path) {
    std::error_code ec;


    const auto t = fs::last_write_time(path, ec);

    if (ec) return 0;

    return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
}


fs::path ixiptlah_sm_index_path(const fs::path& path) {
    if (path.empty()) return {};
    return fs::path(path.wstring() + L".ixsm");
}

bool ixiptlah_sm_index_disabled() {
    // Política V1: el usuario pidió estrictamente un .ixiptlah por elemento y
    // ningún tipo nuevo de archivo. El sidecar histórico queda apagado salvo
    // activación manual; la lectura por elemento mantiene bajo el coste de escaneo.
    if (env_truthy_ix("TLALPOWA_IXIPTLAHSM_ENABLE_INDEX")) return false;
    return true;
}

std::uint64_t ixiptlah_record_header_bytes_for_version(std::uint32_t file_version) {
    (void)file_version;
    // V1: frame_sync(4) + type(4) + schema(4) + stored/raw(16) +
    // codec/reserved(8) + layer_hash(8) + temporal_key(8). Tamaño fijo para
    // saltos O(1) desde el índice residente sin interpretar el payload.
    return 52ull;
}

bool ixiptlah_dir_write_entry(std::ostream& out, const IxiptlahIndexedRecord& rec) {
    IxiptlahIndexedRecord packed = rec;
    ixiptlah_fill_preformed_address_fields(packed);
    const std::uint32_t raw_type = static_cast<std::uint32_t>(packed.type);
    const std::uint32_t flags = packed.quality_flags;
    const std::uint64_t reserved = 0;
    return ixiptlah_write_value(out, raw_type) &&
           ixiptlah_write_value(out, packed.schema) &&
           ixiptlah_write_value(out, packed.payload_offset) &&
           ixiptlah_write_value(out, packed.stored_size) &&
           ixiptlah_write_value(out, packed.raw_size) &&
           ixiptlah_write_value(out, packed.codec) &&
           ixiptlah_write_value(out, flags) &&
           ixiptlah_write_value(out, packed.layer_hash) &&
           ixiptlah_write_value(out, packed.temporal_key) &&
           ixiptlah_write_value(out, reserved) &&
           ixiptlah_write_value(out, packed.narrow_bucket) &&
           ixiptlah_write_value(out, packed.hour_bucket) &&
           ixiptlah_write_value(out, packed.week_bucket) &&
           ixiptlah_write_value(out, packed.wide_bucket) &&
           ixiptlah_write_value(out, packed.core_group) &&
           ixiptlah_write_value(out, packed.quality_flags);
}

std::uint64_t ixiptlah_fast_bucket_key(IxiptlahRecordType type, std::uint32_t schema, std::uint64_t layer_hash) {
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= static_cast<unsigned char>((v >> (i * 8)) & 0xffu);
            h *= 1099511628211ull;
        }
    };
    mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(type)));
    mix(static_cast<std::uint64_t>(schema));
    mix(layer_hash);
    return h == 0 ? 1 : h;
}

std::uint64_t ixiptlah_fast_time_bucket_key(IxiptlahRecordType type,
                                           std::uint32_t schema,
                                           std::uint64_t layer_hash,
                                           std::uint64_t temporal_bucket) {
    std::uint64_t h = ixiptlah_fast_bucket_key(type, schema, layer_hash);
    std::uint64_t v = temporal_bucket;
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<unsigned char>((v >> (i * 8)) & 0xffu);
        h *= 1099511628211ull;
    }
    return h == 0 ? 1 : h;
}

bool ixiptlah_type_is_epidemiology_temporal(IxiptlahRecordType type) {
    switch (type) {
        case IxiptlahRecordType::EpidemiologyObservation:
        case IxiptlahRecordType::EpidemiologyQuarantine:
        case IxiptlahRecordType::MonthlyEpidemiologyBatch:
        case IxiptlahRecordType::EpidemiologyRenderSnapshot:
            return true;
        default:
            return false;
    }
}


bool ixiptlah_type_is_atmosphere_temporal(IxiptlahRecordType type) {
    switch (type) {
        case IxiptlahRecordType::AtmosphereMeasurement:
        case IxiptlahRecordType::AtmosphereTerritoryAverage:
        case IxiptlahRecordType::AtmosphereRenderSummary:
        case IxiptlahRecordType::AtmosphereGraphLayerCatalog:
        case IxiptlahRecordType::AtmosphereGraphDailyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphHourlyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch:
        case IxiptlahRecordType::MonthlyAtmosphereRenderBatch:
        case IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch:
        case IxiptlahRecordType::MonthlyAtmosphereTerritoryBatch:
        case IxiptlahRecordType::MonthlySourceInventory:
            return true;
        default:
            return false;
    }
}

bool ixiptlah_atmosphere_temporal_parts(std::uint64_t temporal_key,
                                        int& year,
                                        int& month,
                                        int& day,
                                        int& hour,
                                        int& minute) {
    if (temporal_key == 0ull) return false;
    minute = static_cast<int>(temporal_key % 100ull); temporal_key /= 100ull;
    hour = static_cast<int>(temporal_key % 100ull); temporal_key /= 100ull;
    day = static_cast<int>(temporal_key % 100ull); temporal_key /= 100ull;
    month = static_cast<int>(temporal_key % 100ull); temporal_key /= 100ull;
    year = static_cast<int>(temporal_key);
    return year >= 0 && year <= 9999 && month >= 1 && month <= 12 &&
           day >= 1 && day <= 31 && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

std::uint64_t ixiptlah_fast_hour_bucket_from_temporal_key(IxiptlahRecordType type, std::uint64_t temporal_key) {
    if (!ixiptlah_type_is_atmosphere_temporal(type) || temporal_key == 0ull) return 0ull;
    int y = 0, m = 0, d = 0, h = 0, mi = 0;
    if (!ixiptlah_atmosphere_temporal_parts(temporal_key, y, m, d, h, mi)) return 0ull;
    (void)mi;
    return (((static_cast<std::uint64_t>(y) * 100ull + static_cast<std::uint64_t>(m)) * 100ull +
             static_cast<std::uint64_t>(d)) * 100ull + static_cast<std::uint64_t>(h));
}

std::uint64_t ixiptlah_fast_week_bucket_from_temporal_key(IxiptlahRecordType type, std::uint64_t temporal_key) {
    if (temporal_key == 0ull) return 0ull;
    if (ixiptlah_type_is_epidemiology_temporal(type)) return temporal_key >= 1000000ull ? temporal_key / 10000ull : temporal_key;
    if (!ixiptlah_type_is_atmosphere_temporal(type)) return 0ull;
    int y = 0, m = 0, d = 0, h = 0, mi = 0;
    if (!ixiptlah_atmosphere_temporal_parts(temporal_key, y, m, d, h, mi)) return 0ull;
    (void)h; (void)mi;
    const int week_start_day = ((std::clamp(d, 1, 31) - 1) / 7) * 7 + 1;
    return (static_cast<std::uint64_t>(y) * 10000ull) +
           (static_cast<std::uint64_t>(m) * 100ull) +
           static_cast<std::uint64_t>(week_start_day);
}

std::uint64_t ixiptlah_fast_narrow_bucket_from_temporal_key(IxiptlahRecordType type, std::uint64_t temporal_key) {
    if (temporal_key == 0ull) return 0ull;
    // Epidemiologia es semanal: la llave operativa es YYYYWW. No debe dividirse
    // como una fecha atmosferica porque colapsaria muchas semanas en un bucket
    // falso y obligaria a barrer capas completas justo al navegar por calendario.
    if (ixiptlah_type_is_epidemiology_temporal(type)) return temporal_key >= 1000000ull ? temporal_key / 10000ull : temporal_key;
    // Atmosfera/contaminantes usan YYYYMMDDHHMM; el bucket estrecho es dia.
    return temporal_key / 10000ull;
}

std::uint64_t ixiptlah_fast_wide_bucket_from_temporal_key(IxiptlahRecordType type, std::uint64_t temporal_key) {
    if (temporal_key == 0ull) return 0ull;
    // Epidemiologia semanal: el bucket amplio es anio, no mes inexistente.
    if (ixiptlah_type_is_epidemiology_temporal(type)) return temporal_key >= 1000000ull ? temporal_key / 1000000ull : temporal_key / 100ull;
    // Atmosfera/contaminantes: bucket mensual YYYYMM.
    return temporal_key / 1000000ull;
}

std::uint64_t ixiptlah_fast_day_bucket_from_temporal_key(std::uint64_t temporal_key) {
    return temporal_key == 0ull ? 0ull : temporal_key / 10000ull;
}

std::uint64_t ixiptlah_fast_month_bucket_from_temporal_key(std::uint64_t temporal_key) {
    return temporal_key == 0ull ? 0ull : temporal_key / 1000000ull;
}

enum class IxiptlahCoreGroup : std::uint32_t {
    Epidemiological = 1u,
    Meteorological = 2u,
    Contaminant = 3u,
    Other = 255u
};

IxiptlahCoreGroup ixiptlah_core_group_for_record_type(IxiptlahRecordType type) {
    switch (type) {
        case IxiptlahRecordType::EpidemiologyObservation:
        case IxiptlahRecordType::EpidemiologyQuarantine:
        case IxiptlahRecordType::MonthlyEpidemiologyBatch:
        case IxiptlahRecordType::EpidemiologyRenderSnapshot:
            return IxiptlahCoreGroup::Epidemiological;
        case IxiptlahRecordType::AtmosphereMeasurement:
        case IxiptlahRecordType::AtmosphereTerritoryAverage:
        case IxiptlahRecordType::AtmosphereRenderSummary:
        case IxiptlahRecordType::AtmosphereGraphLayerCatalog:
        case IxiptlahRecordType::AtmosphereGraphDailyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphHourlyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch:
        case IxiptlahRecordType::MonthlyAtmosphereRenderBatch:
        case IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch:
        case IxiptlahRecordType::MonthlyAtmosphereTerritoryBatch:
            // RAMA/REDMET/RUOA/PEMBU conservan su granularidad real por hora/minuto
            // en los buckets temporales. En este índice genérico de bajo nivel la
            // red concreta se distingue por layer_hash; el núcleo físico compartido
            // evita mezclarlo con epidemiología durante lecturas de calendario.
            return IxiptlahCoreGroup::Contaminant;
        case IxiptlahRecordType::MonthlySourceInventory:
            return IxiptlahCoreGroup::Meteorological;
        default:
            return IxiptlahCoreGroup::Other;
    }
}

std::uint32_t ixiptlah_core_group_u32_for_record_type(IxiptlahRecordType type) {
    return static_cast<std::uint32_t>(ixiptlah_core_group_for_record_type(type));
}

void ixiptlah_fill_preformed_address_fields(IxiptlahIndexedRecord& rec) {
    rec.narrow_bucket = ixiptlah_fast_narrow_bucket_from_temporal_key(rec.type, rec.temporal_key);
    rec.hour_bucket = ixiptlah_fast_hour_bucket_from_temporal_key(rec.type, rec.temporal_key);
    rec.week_bucket = ixiptlah_fast_week_bucket_from_temporal_key(rec.type, rec.temporal_key);
    rec.wide_bucket = ixiptlah_fast_wide_bucket_from_temporal_key(rec.type, rec.temporal_key);
    rec.core_group = ixiptlah_core_group_u32_for_record_type(rec.type);
    rec.quality_flags = 0u;
}

std::uint64_t ixiptlah_fast_core_bucket_key(IxiptlahCoreGroup core,
                                            IxiptlahRecordType type,
                                            std::uint32_t schema,
                                            std::uint64_t layer_hash) {
    const std::uint64_t core_bits = static_cast<std::uint64_t>(core) & 0xffull;
    return ixiptlah_fast_time_bucket_key(type, schema, layer_hash, core_bits);
}

void ixiptlah_build_fast_buckets(IxiptlahFileIndex& index) {
    index.fast_buckets.clear();
    index.fast_time_buckets.clear();
    index.fast_hour_buckets.clear();
    index.fast_week_buckets.clear();
    index.fast_month_buckets.clear();
    index.fast_core_buckets.clear();
    if (index.records.empty()) return;
    index.fast_buckets.reserve(std::min<std::size_t>(index.records.size(), 8192u));
    index.fast_time_buckets.reserve(std::min<std::size_t>(index.records.size(), 16384u));
    index.fast_hour_buckets.reserve(std::min<std::size_t>(index.records.size(), 16384u));
    index.fast_week_buckets.reserve(std::min<std::size_t>(index.records.size(), 8192u));
    index.fast_month_buckets.reserve(std::min<std::size_t>(index.records.size(), 8192u));
    index.fast_core_buckets.reserve(std::min<std::size_t>(index.records.size(), 4096u));
    for (std::size_t i = 0; i < index.records.size(); ++i) {
        if (i > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) break;
        const IxiptlahIndexedRecord& rec = index.records[i];
        const std::uint32_t idx = static_cast<std::uint32_t>(i);
        const std::uint64_t exact_key = ixiptlah_fast_bucket_key(rec.type, rec.schema, rec.layer_hash);
        const std::uint64_t type_schema_key = ixiptlah_fast_bucket_key(rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max());
        index.fast_buckets[exact_key].push_back(idx);
        index.fast_buckets[type_schema_key].push_back(idx);

        const IxiptlahCoreGroup core_group = ixiptlah_core_group_for_record_type(rec.type);
        if (core_group != IxiptlahCoreGroup::Other) {
            index.fast_core_buckets[ixiptlah_fast_core_bucket_key(core_group, rec.type, rec.schema, rec.layer_hash)].push_back(idx);
            index.fast_core_buckets[ixiptlah_fast_core_bucket_key(core_group, rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max())].push_back(idx);
        }

        const std::uint64_t temporal_bucket = rec.narrow_bucket != 0ull ? rec.narrow_bucket : ixiptlah_fast_narrow_bucket_from_temporal_key(rec.type, rec.temporal_key);
        if (temporal_bucket != 0ull) {
            // Dos entradas temporales: una exacta por capa y otra por type+schema.
            // La segunda sirve catálogos o selecciones “todas” sin visitar el índice
            // completo; la verificación final mantiene seguridad ante colisiones.
            index.fast_time_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, rec.layer_hash, temporal_bucket)].push_back(idx);
            index.fast_time_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max(), temporal_bucket)].push_back(idx);
        }
        const std::uint64_t hour_bucket = rec.hour_bucket != 0ull ? rec.hour_bucket : ixiptlah_fast_hour_bucket_from_temporal_key(rec.type, rec.temporal_key);
        if (hour_bucket != 0ull) {
            index.fast_hour_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, rec.layer_hash, hour_bucket)].push_back(idx);
            index.fast_hour_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max(), hour_bucket)].push_back(idx);
        }
        const std::uint64_t week_bucket = rec.week_bucket != 0ull ? rec.week_bucket : ixiptlah_fast_week_bucket_from_temporal_key(rec.type, rec.temporal_key);
        if (week_bucket != 0ull) {
            index.fast_week_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, rec.layer_hash, week_bucket)].push_back(idx);
            index.fast_week_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max(), week_bucket)].push_back(idx);
        }
        const std::uint64_t month_bucket = rec.wide_bucket != 0ull ? rec.wide_bucket : ixiptlah_fast_wide_bucket_from_temporal_key(rec.type, rec.temporal_key);
        if (month_bucket != 0ull) {
            // Bucket mensual/anual: acelera gráficas de año y mes sin duplicar payload.
            index.fast_month_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, rec.layer_hash, month_bucket)].push_back(idx);
            index.fast_month_buckets[ixiptlah_fast_time_bucket_key(rec.type, rec.schema, std::numeric_limits<std::uint64_t>::max(), month_bucket)].push_back(idx);
        }
    }
}

bool ixiptlah_dir_read_entry(std::istream& in, std::uint32_t entry_size, IxiptlahIndexedRecord& rec) {
    std::uint32_t raw_type = 0;
    std::uint32_t flags = 0;
    std::uint64_t reserved = 0;
    rec = {};
    if (entry_size != 64u && entry_size != kIxiptlahDirEntryBytes) return false;
    if (!ixiptlah_read_value(in, raw_type) || !ixiptlah_raw_type_is_known(raw_type)) return false;
    rec.type = static_cast<IxiptlahRecordType>(raw_type);
    if (!ixiptlah_read_value(in, rec.schema) ||
        !ixiptlah_read_value(in, rec.payload_offset) ||
        !ixiptlah_read_value(in, rec.stored_size) ||
        !ixiptlah_read_value(in, rec.raw_size) ||
        !ixiptlah_read_value(in, rec.codec) ||
        !ixiptlah_read_value(in, flags) ||
        !ixiptlah_read_value(in, rec.layer_hash) ||
        !ixiptlah_read_value(in, rec.temporal_key) ||
        !ixiptlah_read_value(in, reserved)) return false;
    if (entry_size >= kIxiptlahDirEntryBytes) {
        if (!ixiptlah_read_value(in, rec.narrow_bucket) ||
            !ixiptlah_read_value(in, rec.hour_bucket) ||
            !ixiptlah_read_value(in, rec.week_bucket) ||
            !ixiptlah_read_value(in, rec.wide_bucket) ||
            !ixiptlah_read_value(in, rec.core_group) ||
            !ixiptlah_read_value(in, rec.quality_flags)) return false;
    } else {
        rec.quality_flags = flags;
        ixiptlah_fill_preformed_address_fields(rec);
    }
    (void)reserved;
    if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) return false;
    if (rec.codec == kCodecRaw && rec.stored_size != rec.raw_size) return false;
    if (rec.codec != kCodecRaw && rec.codec != kCodecIxLz && rec.codec != kCodecIxLzBlocks) return false;
    if (rec.narrow_bucket == 0ull && rec.hour_bucket == 0ull && rec.week_bucket == 0ull && rec.wide_bucket == 0ull) {
        ixiptlah_fill_preformed_address_fields(rec);
    }
    return true;
}

bool ixiptlah_read_embedded_directory_info(const fs::path& path,
                                           std::uint64_t& directory_offset,
                                           std::uint64_t& record_count,
                                           std::uint64_t& directory_end,
                                           std::uint32_t* directory_entry_size = nullptr) {
    directory_offset = 0;
    record_count = 0;
    directory_end = 0;
    const std::uint64_t size = file_size_or_zero(path);
    if (size < sizeof(kIxiptlahMagic) + sizeof(std::uint32_t) + kIxiptlahDirTrailerBytes) return false;

    auto in = ixiptlah_open_binary_input(path);
    if (!in) return false;
    in.seekg(static_cast<std::streamoff>(size - kIxiptlahDirTrailerBytes), std::ios::beg);
    char magic[8] = {};
    std::uint32_t version = 0, entry_size = 0;
    std::uint64_t count = 0, offset = 0;
    std::uint64_t reserved = 0;
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kIxiptlahDirEndMagic, sizeof(kIxiptlahDirEndMagic)) != 0) return false;
    if (!ixiptlah_read_value(in, version) || !ixiptlah_read_value(in, entry_size) ||
        !ixiptlah_read_value(in, count) || !ixiptlah_read_value(in, offset) ||
        !ixiptlah_read_value(in, reserved)) return false;
    if (version != kIxiptlahDirVersion || (entry_size != 64u && entry_size != kIxiptlahDirEntryBytes)) return false;
    if (offset < sizeof(kIxiptlahMagic) + sizeof(std::uint32_t) || offset >= size - kIxiptlahDirTrailerBytes) return false;
    if (count > (size / 4ull + 16ull)) return false;
    const std::uint64_t entries_bytes = count * static_cast<std::uint64_t>(entry_size);
    if (entry_size == 0u || entries_bytes / entry_size != count) return false;
    const std::uint64_t expected_end = offset + kIxiptlahDirHeaderBytes + entries_bytes;
    if (expected_end != size - kIxiptlahDirTrailerBytes) return false;
    directory_offset = offset;
    record_count = count;
    directory_end = expected_end;
    if (directory_entry_size) *directory_entry_size = entry_size;
    (void)reserved;
    return true;
}

bool ixiptlah_read_embedded_directory(const fs::path& path, IxiptlahFileIndex& index) {
    index = {};
    std::uint64_t directory_offset = 0, record_count = 0, directory_end = 0;
    std::uint32_t directory_entry_size = 0;
    if (!ixiptlah_read_embedded_directory_info(path, directory_offset, record_count, directory_end, &directory_entry_size)) return false;

    auto in = ixiptlah_open_binary_input(path);
    std::uint32_t version = 0;
    if (!in || !ixiptlah_read_file_header(in, version)) return false;

    in.seekg(static_cast<std::streamoff>(directory_offset), std::ios::beg);
    char magic[8] = {};
    std::uint32_t dir_version = 0, entry_size = 0;
    std::uint64_t count = 0;
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kIxiptlahDirMagic, sizeof(kIxiptlahDirMagic)) != 0) return false;
    if (!ixiptlah_read_value(in, dir_version) || !ixiptlah_read_value(in, entry_size) || !ixiptlah_read_value(in, count)) return false;
    if (dir_version != kIxiptlahDirVersion || entry_size != directory_entry_size || count != record_count) return false;

    const std::uint64_t size = file_size_or_zero(path);
    index.records.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(record_count, 4ull * 1024ull * 1024ull)));
    for (std::uint64_t i = 0; i < record_count; ++i) {
        IxiptlahIndexedRecord rec;
        if (!ixiptlah_dir_read_entry(in, directory_entry_size, rec)) return false;
        if (rec.payload_offset > directory_offset || rec.stored_size > directory_offset - rec.payload_offset) return false;
        if (rec.payload_offset > size || rec.stored_size > size - rec.payload_offset) return false;
        index.records.push_back(rec);
    }

    index.file_size = size;
    index.mtime_ns = ixiptlah_mtime_ns(path);
    index.file_version = version;
    ixiptlah_build_fast_buckets(index);
    index.valid = true;
    (void)directory_end;
    return true;
}

bool ixiptlah_strip_embedded_directory(const fs::path& path) {
    std::uint64_t directory_offset = 0, record_count = 0, directory_end = 0;
    if (!ixiptlah_read_embedded_directory_info(path, directory_offset, record_count, directory_end)) return true;
    std::error_code ec;
    fs::resize_file(path, directory_offset, ec);
    (void)record_count; (void)directory_end;
    return !ec;
}

bool ixiptlah_embed_terminal_directory(const fs::path& path) {
    if (path.empty() || !fs::exists(path) || file_size_or_zero(path) <= sizeof(kIxiptlahMagic) + sizeof(std::uint32_t)) return true;
    if (!ixiptlah_strip_embedded_directory(path)) return false;

    IxiptlahFileIndex scanned;
    if (!ixiptlah_scan_file_index(path, scanned) || !scanned.valid) return false;

    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    const std::uint64_t directory_offset = file_size_or_zero(path);
    out.write(kIxiptlahDirMagic, sizeof(kIxiptlahDirMagic));
    if (!ixiptlah_write_value(out, kIxiptlahDirVersion) ||
        !ixiptlah_write_value(out, kIxiptlahDirEntryBytes) ||
        !ixiptlah_write_value(out, static_cast<std::uint64_t>(scanned.records.size()))) return false;
    for (const IxiptlahIndexedRecord& rec : scanned.records) {
        if (!ixiptlah_dir_write_entry(out, rec)) return false;
    }
    const std::uint64_t reserved = 0;
    out.write(kIxiptlahDirEndMagic, sizeof(kIxiptlahDirEndMagic));
    if (!ixiptlah_write_value(out, kIxiptlahDirVersion) ||
        !ixiptlah_write_value(out, kIxiptlahDirEntryBytes) ||
        !ixiptlah_write_value(out, static_cast<std::uint64_t>(scanned.records.size())) ||
        !ixiptlah_write_value(out, directory_offset) ||
        !ixiptlah_write_value(out, reserved)) return false;
    out.flush();
    ixiptlah_invalidate_index_cache(path);
    return static_cast<bool>(out);
}


bool ixiptlah_embed_terminal_directory_from_records(const fs::path& path,
                                                    const std::vector<IxiptlahIndexedRecord>& records) {
    if (path.empty() || !fs::exists(path) || file_size_or_zero(path) <= sizeof(kIxiptlahMagic) + sizeof(std::uint32_t)) return true;
    if (!ixiptlah_strip_embedded_directory(path)) return false;

    const std::uint64_t directory_offset = file_size_or_zero(path);
    if (directory_offset <= sizeof(kIxiptlahMagic) + sizeof(std::uint32_t)) return true;
    if (records.size() > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() / kIxiptlahDirEntryBytes)) return false;

    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    out.write(kIxiptlahDirMagic, sizeof(kIxiptlahDirMagic));
    if (!ixiptlah_write_value(out, kIxiptlahDirVersion) ||
        !ixiptlah_write_value(out, kIxiptlahDirEntryBytes) ||
        !ixiptlah_write_value(out, static_cast<std::uint64_t>(records.size()))) return false;

    for (const IxiptlahIndexedRecord& rec : records) {
        if (rec.payload_offset > directory_offset || rec.stored_size > directory_offset - rec.payload_offset) return false;
        if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) return false;
        if (!ixiptlah_dir_write_entry(out, rec)) return false;
    }

    const std::uint64_t reserved = 0;
    out.write(kIxiptlahDirEndMagic, sizeof(kIxiptlahDirEndMagic));
    if (!ixiptlah_write_value(out, kIxiptlahDirVersion) ||
        !ixiptlah_write_value(out, kIxiptlahDirEntryBytes) ||
        !ixiptlah_write_value(out, static_cast<std::uint64_t>(records.size())) ||
        !ixiptlah_write_value(out, directory_offset) ||
        !ixiptlah_write_value(out, reserved)) return false;
    out.flush();
    ixiptlah_invalidate_index_cache(path);
    return static_cast<bool>(out);
}

bool ixiptlah_sm_write_header(std::ostream& out, const IxiptlahSmIndexHeader& h) {
    out.write(kIxiptlahSmIndexMagic, sizeof(kIxiptlahSmIndexMagic));
    return ixiptlah_write_value(out, h.version) &&
           ixiptlah_write_value(out, h.entry_size) &&
           ixiptlah_write_value(out, h.ix_file_size) &&
           ixiptlah_write_value(out, h.ix_mtime_ns) &&
           ixiptlah_write_value(out, h.record_count);
}

bool ixiptlah_sm_read_header(std::istream& in, IxiptlahSmIndexHeader& h) {
    char magic[8] = {};
    h = {};
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kIxiptlahSmIndexMagic, sizeof(kIxiptlahSmIndexMagic)) != 0) return false;
    if (!ixiptlah_read_value(in, h.version) ||
        !ixiptlah_read_value(in, h.entry_size) ||
        !ixiptlah_read_value(in, h.ix_file_size) ||
        !ixiptlah_read_value(in, h.ix_mtime_ns) ||
        !ixiptlah_read_value(in, h.record_count)) return false;
    return h.version == kIxiptlahSmIndexVersion && h.entry_size == kIxiptlahSmIndexEntryBytes;
}

bool ixiptlah_sm_write_entry(std::ostream& out, const IxiptlahSmIndexEntry& e) {
    const std::uint32_t raw_type = static_cast<std::uint32_t>(e.type);
    return ixiptlah_write_value(out, raw_type) &&
           ixiptlah_write_value(out, e.schema) &&
           ixiptlah_write_value(out, e.layer_hash) &&
           ixiptlah_write_value(out, e.payload_offset) &&
           ixiptlah_write_value(out, e.stored_size) &&
           ixiptlah_write_value(out, e.raw_size) &&
           ixiptlah_write_value(out, e.codec) &&
           ixiptlah_write_value(out, e.flags);
}

bool ixiptlah_sm_read_entry(std::istream& in, IxiptlahSmIndexEntry& e) {
    std::uint32_t raw_type = 0;
    e = {};
    if (!ixiptlah_read_value(in, raw_type) || !ixiptlah_raw_type_is_known(raw_type)) return false;
    e.type = static_cast<IxiptlahRecordType>(raw_type);
    if (!ixiptlah_read_value(in, e.schema) ||
        !ixiptlah_read_value(in, e.layer_hash) ||
        !ixiptlah_read_value(in, e.payload_offset) ||
        !ixiptlah_read_value(in, e.stored_size) ||
        !ixiptlah_read_value(in, e.raw_size) ||
        !ixiptlah_read_value(in, e.codec) ||
        !ixiptlah_read_value(in, e.flags)) return false;
    if (e.stored_size > kMaxPayloadBytes || e.raw_size > kMaxPayloadBytes) return false;
    if (e.codec == kCodecRaw && e.stored_size != e.raw_size) return false;
    return true;
}

bool ixiptlah_sm_read_header_from_file(const fs::path& index_path, IxiptlahSmIndexHeader& h) {
    auto in = ixiptlah_open_binary_input(index_path);
    return in && ixiptlah_sm_read_header(in, h);
}

bool ixiptlah_sm_index_append_entry(const fs::path& path, const IxiptlahIndexedRecord& rec, std::uint64_t layer_hash) {
    if (ixiptlah_sm_index_disabled() || path.empty() || rec.payload_offset == 0) return false;

    const fs::path index_path = ixiptlah_sm_index_path(path);
    ensure_dir(index_path.parent_path());

    IxiptlahSmIndexHeader h;
    bool have = ixiptlah_sm_read_header_from_file(index_path, h);
    if (!have) {
        const std::uint64_t rec_header = ixiptlah_record_header_bytes_for_version(kIxiptlahFileVersion);
        const std::uint64_t record_start = rec.payload_offset >= rec_header ? rec.payload_offset - rec_header : 0ull;
        const std::uint64_t file_header = sizeof(kIxiptlahMagic) + sizeof(std::uint32_t);
        if (record_start > file_header) {
            // No fabricar un índice parcial para archivos antiguos con registros
            // previos: sería rápido pero semánticamente falso. En ese caso el
            // lector cae al escaneo compatible hasta que se reimporte o reescriba.
            return false;
        }
        std::ofstream create(index_path, std::ios::binary | std::ios::trunc);
        h.version = kIxiptlahSmIndexVersion;
        h.entry_size = kIxiptlahSmIndexEntryBytes;
        h.ix_file_size = 0;
        h.ix_mtime_ns = 0;
        h.record_count = 0;
        if (!create || !ixiptlah_sm_write_header(create, h)) return false;
    }

    {
        std::ofstream out(index_path, std::ios::binary | std::ios::app);
        if (!out) return false;
        IxiptlahSmIndexEntry e;
        e.type = rec.type;
        e.schema = rec.schema;
        (void)layer_hash;
        e.layer_hash = rec.layer_hash;
        e.payload_offset = rec.payload_offset;
        e.stored_size = rec.stored_size;
        e.raw_size = rec.raw_size;
        e.codec = rec.codec;
        e.flags = 0;
        if (!ixiptlah_sm_write_entry(out, e)) return false;
    }

    ++h.record_count;
    h.ix_file_size = static_cast<std::uint64_t>(file_size_or_zero(path));
    h.ix_mtime_ns = ixiptlah_mtime_ns(path);

    std::fstream header_out(index_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!header_out) return false;
    header_out.seekp(0, std::ios::beg);
    return ixiptlah_sm_write_header(header_out, h);
}




void ixiptlah_invalidate_index_cache(const fs::path& path) {


    std::lock_guard<std::mutex> lock(ixiptlah_index_mu());


    const std::string key = path_utf8(path);
    ixiptlah_index_cache().erase(key);
    ixiptlah_index_shared_cache().erase(key);

    // Todo cambio físico invalida payloads residentes asociados al archivo. La
    // clave incluye tamaño/mtime, pero purgar aquí evita conservar bytes obsoletos
    // durante sesiones largas y reduce presión de RAM tras reescrituras.
    ixiptlah_payload_cache_erase_path(path);
}


bool env_truthy_ix(const char* name) {
    const std::string v = getenv_utf8_or_empty(name);

    if (v.empty()) return false;
    const char c = v.front();

    return c == '1' || c == 's' || c == 'S' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
}


std::uint64_t env_u64_ix(const char* name, std::uint64_t fallback, std::uint64_t floor_value, std::uint64_t ceiling_value) {
    const std::string raw = trim(getenv_utf8_or_empty(name));
    if (raw.empty()) return fallback;
    try {
        size_t used = 0;
        const std::uint64_t parsed = static_cast<std::uint64_t>(std::stoull(raw, &used, 10));
        if (used == 0) return fallback;
        return std::clamp(parsed, floor_value, ceiling_value);
    } catch (...) {
        return fallback;
    }
}


std::uint64_t ixiptlah_memory_spool_limit_bytes() {
    static const std::uint64_t value = env_u64_ix(
        "TLALPOWA_IXIPTLAH_MEM_SPOOL_BYTES",
        2ull * 1024ull * 1024ull,
        256ull * 1024ull,
        64ull * 1024ull * 1024ull);
    return value;
}


std::uint64_t ixiptlah_compression_limit_bytes() {
    static const std::uint64_t value = env_u64_ix(
        "TLALPOWA_IXIPTLAH_MAX_COMPRESS_BYTES",
        128ull * 1024ull * 1024ull,
        0ull,
        512ull * 1024ull * 1024ull);
    return value;
}






std::uint32_t ixiptlah_default_write_version() {
    return kIxiptlahFileVersion;
}



bool ixiptlah_write_file_header(std::ostream& out, std::uint32_t version = kIxiptlahFileVersion) {


    out.write(kIxiptlahMagic, sizeof(kIxiptlahMagic));


    return ixiptlah_write_value(out, version);
}





bool ixiptlah_raw_type_is_known(std::uint32_t raw_type) {
    switch (static_cast<IxiptlahRecordType>(raw_type)) {
        case IxiptlahRecordType::EpidemiologyObservation:
        case IxiptlahRecordType::EpidemiologyQuarantine:
        case IxiptlahRecordType::AtmosphereMeasurement:
        case IxiptlahRecordType::AtmosphereTerritoryAverage:
        case IxiptlahRecordType::AtmosphereRenderSummary:
        case IxiptlahRecordType::AtmosphereGraphLayerCatalog:
        case IxiptlahRecordType::AtmosphereGraphDailyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphHourlyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch:
        case IxiptlahRecordType::ProcessedPdf:
        case IxiptlahRecordType::ProcessedPage:
        case IxiptlahRecordType::RunState:
        case IxiptlahRecordType::LivePreview:
        case IxiptlahRecordType::MonthlyDictionary:
        case IxiptlahRecordType::MonthlyEpidemiologyBatch:
        case IxiptlahRecordType::MonthlyAtmosphereRenderBatch:
        case IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch:
        case IxiptlahRecordType::MonthlyAtmosphereTerritoryBatch:
        case IxiptlahRecordType::MonthlySourceInventory:
        case IxiptlahRecordType::EpidemiologyRenderSnapshot:
            return true;
    }
    return false;
}

bool ixiptlah_file_version_supported(std::uint32_t version) {
    return version == kIxiptlahFileVersion;
}

bool ixiptlah_write_frame_sync_if_needed(std::ostream& out, std::uint32_t file_version) {
    // Marcador fijo de baja entropía: permite resincronizar archivos grandes sin
    // escanear ni copiar payloads; no forma parte de contenedores ajenos a V1.
    if (file_version < kIxiptlahFileVersion) return true;
    return ixiptlah_write_value(out, kIxiptlahFrameSync);
}

bool ixiptlah_read_file_header(std::istream& in, std::uint32_t& version) {
    char magic[8] = {};
    version = 0;

    in.read(magic, sizeof(magic));


    if (!in || std::memcmp(magic, kIxiptlahMagic, sizeof(kIxiptlahMagic)) != 0) return false;


    if (!ixiptlah_read_value(in, version)) return false;


    return ixiptlah_file_version_supported(version);
}




std::uint32_t ixiptlah_existing_version_or_default(const fs::path& path, bool needs_header) {


    if (needs_header) return ixiptlah_default_write_version();


    auto in = ixiptlah_open_binary_input(path);
    std::uint32_t version = 0;


    if (in && ixiptlah_read_file_header(in, version)) return version;


    return ixiptlah_default_write_version();
}




std::string ix_lz_compress(const std::string& input) {
    const size_t n = input.size();

    if (n < 48) return {};
    std::string out;

    out.reserve(n);

    thread_local std::vector<int> last_pos(1u << 16, -1);
    thread_local std::vector<std::uint32_t> last_epoch(1u << 16, 0u);
    thread_local std::uint32_t epoch = 1u;
    ++epoch;
    if (epoch == 0u) {
        std::fill(last_epoch.begin(), last_epoch.end(), 0u);
        epoch = 1u;
    }
    size_t i = 0;


    while (i < n) {
        const size_t control_pos = out.size();

        out.push_back('\0');
        unsigned char control = 0;

        for (int bit = 0; bit < 8 && i < n; ++bit) {
            size_t best_len = 0;
            size_t best_dist = 0;

            if (i + 4 <= n) {
                const unsigned char a = static_cast<unsigned char>(input[i]);

                const unsigned char b = static_cast<unsigned char>(input[i + 1]);
                const unsigned char c = static_cast<unsigned char>(input[i + 2]);
                const std::uint32_t h = ((static_cast<std::uint32_t>(a) * 251u) ^
                                         (static_cast<std::uint32_t>(b) * 911u) ^
                                         (static_cast<std::uint32_t>(c) * 3571u)) & 0xffffu;
                const int prev = (last_epoch[h] == epoch) ? last_pos[h] : -1;
                last_pos[h] = static_cast<int>(i);
                last_epoch[h] = epoch;

                if (prev >= 0 && i > static_cast<size_t>(prev)) {
                    const size_t dist = i - static_cast<size_t>(prev);


                    if (dist <= 65535) {
                        size_t len = 0;

                        while (i + len < n && static_cast<size_t>(prev) + len < i &&

                               input[static_cast<size_t>(prev) + len] == input[i + len] && len < 255 + 4) {
                            ++len;
                        }

                        if (len >= 4) {
                            best_len = len;
                            best_dist = dist;
                        }
                    }
                }
            }

            if (best_len >= 4) {
                control |= static_cast<unsigned char>(1u << bit);

                out.push_back(static_cast<char>(best_dist & 0xffu));

                out.push_back(static_cast<char>((best_dist >> 8) & 0xffu));

                out.push_back(static_cast<char>(best_len - 4));
                const size_t end = std::min(n, i + best_len);

                for (size_t j = i + 1; j + 2 < end; ++j) {
                    const unsigned char a = static_cast<unsigned char>(input[j]);
                    const unsigned char b = static_cast<unsigned char>(input[j + 1]);
                    const unsigned char c = static_cast<unsigned char>(input[j + 2]);

                    const std::uint32_t h = ((static_cast<std::uint32_t>(a) * 251u) ^

                                             (static_cast<std::uint32_t>(b) * 911u) ^
                                             (static_cast<std::uint32_t>(c) * 3571u)) & 0xffffu;
                    last_pos[h] = static_cast<int>(j);
                    last_epoch[h] = epoch;
                }
                i += best_len;
            } else {

                out.push_back(input[i++]);
            }
        }
        out[control_pos] = static_cast<char>(control);

        if (out.size() >= n) return {};
    }

    return out;
}



bool ix_lz_decompress(const std::string& input, std::uint64_t raw_size, std::string& output) {

    if (raw_size > kMaxPayloadBytes) return false;

    output.clear();

    output.reserve(static_cast<size_t>(raw_size));
    size_t i = 0;

    while (i < input.size() && output.size() < raw_size) {
        const unsigned char control = static_cast<unsigned char>(input[i++]);

        for (int bit = 0; bit < 8 && output.size() < raw_size; ++bit) {


            if (control & static_cast<unsigned char>(1u << bit)) {

                if (i + 3 > input.size()) return false;
                const std::uint32_t dist = static_cast<unsigned char>(input[i]) |
                    (static_cast<std::uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8);
                const std::uint32_t len = static_cast<std::uint32_t>(static_cast<unsigned char>(input[i + 2])) + 4u;
                i += 3;

                if (dist == 0 || dist > output.size()) return false;
                const size_t start = output.size() - dist;

                for (std::uint32_t j = 0; j < len; ++j) {

                    if (output.size() >= raw_size) break;

                    output.push_back(output[start + j]);
                }
            } else {

                if (i >= input.size()) return false;

                output.push_back(input[i++]);
            }
        }
    }

    return output.size() == raw_size;
}




std::string ixiptlah_payload_for_storage(const std::string& bytes, std::uint32_t& codec, std::uint32_t file_version) {
    codec = kCodecRaw;

    if (file_version >= kIxiptlahFileVersion && !env_truthy_ix("TLALPOWA_IXIPTLAH_V1_COMPRESS")) return bytes;

    if (bytes.size() < 48 ||
        bytes.size() > ixiptlah_compression_limit_bytes() ||
        env_truthy_ix("TLALPOWA_IXIPTLAH_DISABLE_COMPRESSION")) return bytes;
    std::string packed = ix_lz_compress(bytes);

    if (!packed.empty() && packed.size() + 8 < bytes.size()) {
        codec = kCodecIxLz;

        return packed;
    }

    return bytes;
}




bool ixiptlah_decode_payload(std::string stored, const IxiptlahRecordEnvelope& env, std::string& decoded) {

    if (env.codec == kCodecRaw) {

        if (env.raw_size != env.stored_size) return false;

        decoded = std::move(stored);

        return true;
    }

    if (env.codec == kCodecIxLz) {

        return ix_lz_decompress(stored, env.raw_size, decoded);
    }

    if (env.codec == kCodecIxLzBlocks) {
        decoded.clear();
        decoded.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(env.raw_size, kMaxPayloadBytes)));
        std::istringstream blocks(stored, std::ios::in | std::ios::binary);
        std::uint64_t produced = 0;
        while (produced < env.raw_size && blocks) {
            std::uint32_t raw_n = 0, stored_n = 0, block_codec = 0;
            if (!ixiptlah_read_value(blocks, raw_n) || !ixiptlah_read_value(blocks, stored_n) || !ixiptlah_read_value(blocks, block_codec)) return false;
            if (raw_n == 0 || raw_n > kIxiptlahCompressionBlockBytes || stored_n > kIxiptlahCompressionBlockBytes + 64u) return false;
            std::string block(stored_n, '\0');
            if (stored_n > 0) blocks.read(block.data(), static_cast<std::streamsize>(stored_n));
            if (!blocks) return false;
            std::string raw;
            if (block_codec == kIxiptlahBlockCodecRaw) raw = std::move(block);
            else if (block_codec == kIxiptlahBlockCodecIxLz) {
                if (!ix_lz_decompress(block, raw_n, raw)) return false;
            } else return false;
            if (raw.size() != raw_n || raw_n > env.raw_size - produced) return false;
            decoded.append(raw);
            produced += raw_n;
        }
        return produced == env.raw_size;
    }

    return false;
}




bool ixiptlah_read_record_envelope(std::istream& in, std::uint32_t file_version, IxiptlahRecordEnvelope& env) {
    std::uint32_t raw_type = 0;
    env = {};


    if (!ixiptlah_read_value(in, raw_type)) return false;

    if (!ixiptlah_raw_type_is_known(raw_type)) {
        if (raw_type != kIxiptlahFrameSync) return false;
        if (!ixiptlah_read_value(in, raw_type) || !ixiptlah_raw_type_is_known(raw_type)) return false;
    }


    if (!ixiptlah_read_value(in, env.schema)) return false;


    if (!ixiptlah_read_value(in, env.stored_size)) return false;


    env.type = static_cast<IxiptlahRecordType>(raw_type);


    if (file_version == kIxiptlahFileVersionLegacy) {
        env.raw_size = env.stored_size;
        env.codec = kCodecRaw;
    } else {


        if (!ixiptlah_read_value(in, env.raw_size)) return false;


        if (!ixiptlah_read_value(in, env.codec)) return false;
        std::uint32_t reserved = 0;


        if (!ixiptlah_read_value(in, reserved)) return false;
        (void)reserved;

        if (!ixiptlah_read_value(in, env.layer_hash)) return false;
        if (!ixiptlah_read_value(in, env.temporal_key)) return false;
    }

    if (env.stored_size > kMaxPayloadBytes || env.raw_size > kMaxPayloadBytes) return false;

    if (env.codec == kCodecRaw && env.stored_size != env.raw_size) return false;
    if (env.codec != kCodecRaw && env.codec != kCodecIxLz && env.codec != kCodecIxLzBlocks) return false;

    return true;
}




bool ixiptlah_scan_file_index(const fs::path& path, IxiptlahFileIndex& index) {
    index = {};

    if (ixiptlah_read_embedded_directory(path, index)) return true;

    const std::uint64_t size = file_size_or_zero(path);

    if (size == 0) return false;


    auto in = ixiptlah_open_binary_input(path);
    std::uint32_t version = 0;


    if (!in || !ixiptlah_read_file_header(in, version)) return false;


    for (;;) {


        IxiptlahRecordEnvelope env;


        if (!ixiptlah_read_record_envelope(in, version, env)) break;
        const auto payload_pos = in.tellg();

        if (payload_pos < std::streampos(0)) break;

        if (env.stored_size > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) break;
        const auto payload_offset = static_cast<std::uint64_t>(payload_pos);

        if (payload_offset > size) break;

        if (env.stored_size > size - payload_offset) break;



        IxiptlahIndexedRecord rec;
        rec.type = env.type;


        rec.schema = env.schema;
        rec.payload_offset = payload_offset;
        rec.stored_size = env.stored_size;
        rec.raw_size = env.raw_size;
        rec.codec = env.codec;
        rec.layer_hash = env.layer_hash;
        rec.temporal_key = env.temporal_key;
        ixiptlah_fill_preformed_address_fields(rec);

        index.records.push_back(rec);

        in.seekg(static_cast<std::streamoff>(env.stored_size), std::ios::cur);

        if (!in) break;

        if (static_cast<std::uint64_t>(in.tellg()) > size) break;
    }



    index.file_size = size;


    index.mtime_ns = ixiptlah_mtime_ns(path);

    index.file_version = version;
    ixiptlah_build_fast_buckets(index);
    index.valid = true;

    return true;
}





std::shared_ptr<const IxiptlahFileIndex> ixiptlah_live_index_for_open_sink(const fs::path& path) {
    if (path.empty()) return {};

    const std::string key = path_utf8(path);
    IxiptlahFileIndex live;
    std::uint64_t revision = 0;
    std::uint64_t file_size = 0;
    std::int64_t mtime_ns = 0;

    {
        std::lock_guard<std::mutex> lock(ixiptlah_mu());
        auto it = ixiptlah_sinks().find(key);
        if (it == ixiptlah_sinks().end() || !it->second || !it->second->stream || !it->second->live_index_valid) {
            return {};
        }

        // Publica bytes pendientes exactamente en la frontera de lectura. La
        // captura masiva mantiene escritura diferida; la UI, en cambio, ve un
        // índice completo sin depender de cerrar el proceso ni de reescanear.
        it->second->stream.flush();
        if (!it->second->stream) return {};

        file_size = file_size_or_zero(path);
        mtime_ns = ixiptlah_mtime_ns(path);
        revision = it->second->live_revision;

        if (it->second->live_snapshot &&
            it->second->live_snapshot_revision == revision &&
            it->second->live_snapshot->valid &&
            it->second->live_snapshot->file_size == file_size &&
            it->second->live_snapshot->mtime_ns == mtime_ns) {
            return it->second->live_snapshot;
        }

        live.file_version = it->second->version;
        live.records = it->second->live_records;
    }

    live.file_size = file_size;
    live.mtime_ns = mtime_ns;
    ixiptlah_build_fast_buckets(live);
    live.valid = true;

    auto shared = std::make_shared<const IxiptlahFileIndex>(std::move(live));

    {
        std::lock_guard<std::mutex> lock(ixiptlah_mu());
        auto it = ixiptlah_sinks().find(key);
        if (it != ixiptlah_sinks().end() && it->second && it->second->live_revision == revision) {
            it->second->live_snapshot = shared;
            it->second->live_snapshot_revision = revision;
        }
    }

    return shared;
}

std::shared_ptr<const IxiptlahFileIndex> ixiptlah_index_for_path_shared(const fs::path& path) {

    if (auto live = ixiptlah_live_index_for_open_sink(path)) return live;

    const std::string key = path_utf8(path);
    const std::uint64_t size = file_size_or_zero(path);
    const std::int64_t mtime = ixiptlah_mtime_ns(path);

    {
        std::lock_guard<std::mutex> lock(ixiptlah_index_mu());
        const auto it = ixiptlah_index_shared_cache().find(key);
        if (it != ixiptlah_index_shared_cache().end() && it->second && it->second->valid &&
            it->second->file_size == size && it->second->mtime_ns == mtime) {
            return it->second;
        }
    }

    IxiptlahFileIndex fresh;
    ixiptlah_scan_file_index(path, fresh);

    auto shared = std::make_shared<const IxiptlahFileIndex>(std::move(fresh));
    {
        std::lock_guard<std::mutex> lock(ixiptlah_index_mu());
        auto& shared_slot = ixiptlah_index_shared_cache()[key];
        shared_slot = shared;

        // Índice residente estrictamente acotado: conserva el directorio compacto
        // de los núcleos calientes sin permitir que una sesión larga duplique miles
        // de vectores. El payload nunca entra aquí; sólo offsets, capa y tiempo.
        if (ixiptlah_index_shared_cache().size() > 4096u) {
            ixiptlah_index_shared_cache().clear();
            ixiptlah_index_cache().clear();
            ixiptlah_index_shared_cache()[key] = shared;
        }
    }

    return shared;
}


IxiptlahFileIndex ixiptlah_index_for_path(const fs::path& path) {
    auto shared = ixiptlah_index_for_path_shared(path);
    return shared ? *shared : IxiptlahFileIndex{};
}


bool ixiptlah_write_record_with_meta(std::ostream& out,
                                    std::uint32_t file_version,
                                    IxiptlahRecordType type,
                                    std::uint32_t schema_version,
                                    std::uint64_t layer_hash,
                                    std::uint64_t temporal_key,
                                    const std::string& bytes) {
    const std::uint32_t raw_type = static_cast<std::uint32_t>(type);

    if (!ixiptlah_write_frame_sync_if_needed(out, file_version)) return false;

    std::uint32_t codec = kCodecRaw;
    std::string stored = ixiptlah_payload_for_storage(bytes, codec, file_version);
    const std::uint64_t stored_size = static_cast<std::uint64_t>(stored.size());
    const std::uint64_t raw_size = static_cast<std::uint64_t>(bytes.size());
    const std::uint32_t reserved = 0;

    if (!ixiptlah_write_value(out, raw_type) ||
        !ixiptlah_write_value(out, schema_version) ||
        !ixiptlah_write_value(out, stored_size) ||
        !ixiptlah_write_value(out, raw_size) ||
        !ixiptlah_write_value(out, codec) ||
        !ixiptlah_write_value(out, reserved) ||
        !ixiptlah_write_value(out, layer_hash) ||
        !ixiptlah_write_value(out, temporal_key)) return false;

    if (stored_size > 0) out.write(stored.data(), static_cast<std::streamsize>(stored.size()));
    return static_cast<bool>(out);
}

bool ixiptlah_write_record(std::ostream& out,
                           std::uint32_t file_version,
                           IxiptlahRecordType type,
                           std::uint32_t schema_version,
                           const std::string& bytes) {
    return ixiptlah_write_record_with_meta(out, file_version, type, schema_version, 0ull, 0ull, bytes);
}




[[maybe_unused]] bool ixiptlah_write_indexed_record_raw(std::ostream& out, const IxiptlahIndexedRecord& rec, const std::string& stored) {

    if (stored.size() != rec.stored_size) return false;
    const std::uint32_t raw_type = static_cast<std::uint32_t>(rec.type);

    const std::uint32_t reserved = 0;


    if (!ixiptlah_write_value(out, raw_type) ||


        !ixiptlah_write_value(out, rec.schema) ||


        !ixiptlah_write_value(out, rec.stored_size) ||


        !ixiptlah_write_value(out, rec.raw_size) ||


        !ixiptlah_write_value(out, rec.codec) ||


        !ixiptlah_write_value(out, reserved) ||
        !ixiptlah_write_value(out, rec.layer_hash) ||
        !ixiptlah_write_value(out, rec.temporal_key)) return false;

    if (!stored.empty()) out.write(stored.data(), static_cast<std::streamsize>(stored.size()));

    return static_cast<bool>(out);
}


bool ixiptlah_write_indexed_record_header_raw(std::ostream& out, const IxiptlahIndexedRecord& rec, std::uint32_t target_file_version = kIxiptlahFileVersion) {
    const std::uint32_t raw_type = static_cast<std::uint32_t>(rec.type);
    const std::uint32_t reserved = 0;

    return ixiptlah_write_frame_sync_if_needed(out, target_file_version) &&
           ixiptlah_write_value(out, raw_type) &&
           ixiptlah_write_value(out, rec.schema) &&
           ixiptlah_write_value(out, rec.stored_size) &&
           ixiptlah_write_value(out, rec.raw_size) &&
           ixiptlah_write_value(out, rec.codec) &&
           ixiptlah_write_value(out, reserved) &&
           ixiptlah_write_value(out, rec.layer_hash) &&
           ixiptlah_write_value(out, rec.temporal_key);
}


bool ixiptlah_copy_exact_bytes(std::istream& in, std::ostream& out, std::uint64_t bytes) {
    std::array<char, 256u * 1024u> buffer{};

    while (bytes > 0) {
        const std::size_t step = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, buffer.size()));
        in.read(buffer.data(), static_cast<std::streamsize>(step));
        const std::streamsize got = in.gcount();

        if (got <= 0) return false;
        out.write(buffer.data(), got);

        if (!out) return false;
        bytes -= static_cast<std::uint64_t>(got);

        if (static_cast<std::size_t>(got) != step) return false;
    }

    return static_cast<bool>(out);
}


bool ixiptlah_write_indexed_record_raw_stream(std::istream& in, std::ostream& out, const IxiptlahIndexedRecord& rec, std::uint32_t target_file_version = kIxiptlahFileVersion) {
    if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) return false;
    if (!ixiptlah_write_indexed_record_header_raw(out, rec, target_file_version)) return false;
    return ixiptlah_copy_exact_bytes(in, out, rec.stored_size);
}


fs::path ixiptlah_temp_path_near(const fs::path& owner, const char* suffix) {
    const fs::path dir = owner.has_parent_path() ? owner.parent_path() : fs::temp_directory_path();
    std::ostringstream name;
    name << "." << path_utf8(owner.filename()) << "."
         << std::chrono::steady_clock::now().time_since_epoch().count() << "."
         << std::hash<std::thread::id>{}(std::this_thread::get_id()) << suffix;
    return dir / name.str();
}


class IxiptlahPayloadSpool final : public std::streambuf {
public:
    explicit IxiptlahPayloadSpool(const fs::path& owner_path)
        : spill_limit_(ixiptlah_memory_spool_limit_bytes()),
          tmp_path_(ixiptlah_temp_path_near(owner_path, ".payload.tmp")) {
        memory_.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(spill_limit_, 64ull * 1024ull)));
    }

    ~IxiptlahPayloadSpool() override {
        close_and_remove();
    }

    IxiptlahPayloadSpool(const IxiptlahPayloadSpool&) = delete;
    IxiptlahPayloadSpool& operator=(const IxiptlahPayloadSpool&) = delete;

    std::uint64_t size() const noexcept { return total_; }
    bool ok() const noexcept { return !failed_; }

    bool seal() {
        if (file_.is_open()) file_.flush();
        return ok() && (!file_.is_open() || static_cast<bool>(file_));
    }

    bool read_all_to_string(std::string& out) {
        if (!seal()) return false;
        if (total_ > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return false;

        out.clear();
        out.reserve(static_cast<std::size_t>(total_));

        if (!spilled_) {
            out.assign(memory_.begin(), memory_.end());
            return true;
        }

        std::ifstream in(tmp_path_, std::ios::binary);
        if (!in) return false;
        out.assign(static_cast<std::size_t>(total_), '\0');
        if (!out.empty()) in.read(out.data(), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(in) || (total_ == 0);
    }

    bool copy_to(std::ostream& out) {
        if (!seal()) return false;

        if (!spilled_) {
            if (!memory_.empty()) out.write(memory_.data(), static_cast<std::streamsize>(memory_.size()));
            return static_cast<bool>(out);
        }

        std::ifstream in(tmp_path_, std::ios::binary);
        if (!in) return false;
        return ixiptlah_copy_exact_bytes(in, out, total_);
    }

    bool for_each_chunk(const std::function<bool(const char*, std::size_t)>& visitor,
                        std::size_t chunk_size = kIxiptlahCompressionBlockBytes) {
        if (!visitor || !seal()) return false;
        chunk_size = std::clamp<std::size_t>(chunk_size, 4096u, kIxiptlahCompressionBlockBytes);

        if (!spilled_) {
            std::size_t pos = 0;
            while (pos < memory_.size()) {
                const std::size_t step = std::min<std::size_t>(chunk_size, memory_.size() - pos);
                if (!visitor(memory_.data() + pos, step)) return false;
                pos += step;
            }
            return true;
        }

        std::ifstream in(tmp_path_, std::ios::binary);
        if (!in) return false;
        std::vector<char> buffer(chunk_size);
        std::uint64_t left = total_;
        while (left > 0) {
            const std::size_t step = static_cast<std::size_t>(std::min<std::uint64_t>(left, buffer.size()));
            in.read(buffer.data(), static_cast<std::streamsize>(step));
            const std::streamsize got = in.gcount();
            if (got <= 0) return false;
            if (!visitor(buffer.data(), static_cast<std::size_t>(got))) return false;
            left -= static_cast<std::uint64_t>(got);
            if (static_cast<std::size_t>(got) != step) return false;
        }
        return true;
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        const char c = traits_type::to_char_type(ch);
        return append_bytes(&c, 1) ? ch : traits_type::eof();
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override {
        if (!s || count <= 0) return 0;
        const auto n = static_cast<std::size_t>(count);
        return append_bytes(s, n) ? count : 0;
    }

    int sync() override {
        return seal() ? 0 : -1;
    }

private:
    bool spill_to_file() {
        if (spilled_) return file_.is_open() && static_cast<bool>(file_);

        ensure_dir(tmp_path_.parent_path());
        file_.open(tmp_path_, std::ios::binary | std::ios::trunc);
        if (!file_) {
            failed_ = true;
            return false;
        }

        if (!memory_.empty()) file_.write(memory_.data(), static_cast<std::streamsize>(memory_.size()));
        if (!file_) {
            failed_ = true;
            return false;
        }

        memory_.clear();
        memory_.shrink_to_fit();
        spilled_ = true;
        return true;
    }

    bool append_bytes(const char* data, std::size_t n) {
        if (failed_) return false;
        if (n == 0) return true;
        if (total_ > kMaxPayloadBytes || n > kMaxPayloadBytes - total_) {
            failed_ = true;
            return false;
        }

        if (!spilled_ && total_ + static_cast<std::uint64_t>(n) <= spill_limit_) {
            memory_.insert(memory_.end(), data, data + n);
            total_ += static_cast<std::uint64_t>(n);
            return true;
        }

        if (!spill_to_file()) return false;
        file_.write(data, static_cast<std::streamsize>(n));
        if (!file_) {
            failed_ = true;
            return false;
        }
        total_ += static_cast<std::uint64_t>(n);
        return true;
    }

    void close_and_remove() {
        if (file_.is_open()) file_.close();
        if (!tmp_path_.empty()) {
            std::error_code ec;
            fs::remove(tmp_path_, ec);
        }
    }

    std::uint64_t spill_limit_ = 0;
    std::uint64_t total_ = 0;
    bool spilled_ = false;
    bool failed_ = false;
    fs::path tmp_path_;
    std::vector<char> memory_;
    std::ofstream file_;
};


bool ixiptlah_write_block_header(std::ostream& out, std::uint32_t raw_n, std::uint32_t stored_n, std::uint32_t block_codec) {
    return ixiptlah_write_value(out, raw_n) &&
           ixiptlah_write_value(out, stored_n) &&
           ixiptlah_write_value(out, block_codec);
}

bool ixiptlah_build_block_lz_payload(IxiptlahPayloadSpool& raw_spool,
                                     const fs::path& owner_path,
                                     IxiptlahPayloadSpool& compressed_spool) {
    if (!raw_spool.seal() || raw_spool.size() == 0 || raw_spool.size() > kMaxPayloadBytes) return false;
    std::ostream out(&compressed_spool);
    std::uint64_t raw_total = 0;
    bool ok = raw_spool.for_each_chunk([&](const char* data, std::size_t n) -> bool {
        if (!data || n == 0 || n > kIxiptlahCompressionBlockBytes) return false;
        raw_total += static_cast<std::uint64_t>(n);
        std::string raw(data, data + n);
        std::string packed = ix_lz_compress(raw);
        const bool use_packed = !packed.empty() && packed.size() + 12u < raw.size();
        const std::uint32_t raw_n = static_cast<std::uint32_t>(raw.size());
        const std::uint32_t stored_n = static_cast<std::uint32_t>(use_packed ? packed.size() : raw.size());
        const std::uint32_t block_codec = use_packed ? kIxiptlahBlockCodecIxLz : kIxiptlahBlockCodecRaw;
        if (!ixiptlah_write_block_header(out, raw_n, stored_n, block_codec)) return false;
        if (stored_n > 0) {
            const std::string& bytes = use_packed ? packed : raw;
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(out);
    });
    out.flush();
    (void)owner_path;
    return ok && out && compressed_spool.ok() && compressed_spool.seal() && raw_total == raw_spool.size();
}

bool ixiptlah_write_record_block_lz_payload(std::ostream& out,
                                            std::uint32_t file_version,
                                            IxiptlahRecordType type,
                                            std::uint32_t schema_version,
                                            std::uint64_t layer_hash,
                                            std::uint64_t temporal_key,
                                            std::uint64_t raw_payload_size,
                                            IxiptlahPayloadSpool& compressed_spool) {
    if (!compressed_spool.seal()) return false;
    const std::uint64_t stored_size = compressed_spool.size();
    const std::uint32_t raw_type = static_cast<std::uint32_t>(type);
    const std::uint32_t reserved = 0;
    if (!ixiptlah_write_frame_sync_if_needed(out, file_version)) return false;
    if (!ixiptlah_write_value(out, raw_type) ||
        !ixiptlah_write_value(out, schema_version) ||
        !ixiptlah_write_value(out, stored_size) ||
        !ixiptlah_write_value(out, raw_payload_size) ||
        !ixiptlah_write_value(out, kCodecIxLzBlocks) ||
        !ixiptlah_write_value(out, reserved) ||
        !ixiptlah_write_value(out, layer_hash) ||
        !ixiptlah_write_value(out, temporal_key)) return false;
    return compressed_spool.copy_to(out);
}


bool ixiptlah_write_record_raw_payload(std::ostream& out,
                                       std::uint32_t file_version,
                                       IxiptlahRecordType type,
                                       std::uint32_t schema_version,
                                       std::uint64_t layer_hash,
                                       std::uint64_t temporal_key,
                                       std::uint64_t payload_size,
                                       IxiptlahPayloadSpool& spool) {
    const std::uint32_t raw_type = static_cast<std::uint32_t>(type);

    if (!ixiptlah_write_frame_sync_if_needed(out, file_version)) return false;

    if (file_version == kIxiptlahFileVersionLegacy) {
        if (!ixiptlah_write_value(out, raw_type) ||
            !ixiptlah_write_value(out, schema_version) ||
            !ixiptlah_write_value(out, payload_size)) return false;
        return spool.copy_to(out);
    }

    const std::uint32_t codec = kCodecRaw;
    const std::uint32_t reserved = 0;
    if (!ixiptlah_write_value(out, raw_type) ||
        !ixiptlah_write_value(out, schema_version) ||
        !ixiptlah_write_value(out, payload_size) ||
        !ixiptlah_write_value(out, payload_size) ||
        !ixiptlah_write_value(out, codec) ||
        !ixiptlah_write_value(out, reserved) ||
        !ixiptlah_write_value(out, layer_hash) ||
        !ixiptlah_write_value(out, temporal_key)) return false;

    return spool.copy_to(out);
}


bool ixiptlah_write_spooled_record(std::ostream& out,
                                   std::uint32_t file_version,
                                   IxiptlahRecordType type,
                                   std::uint32_t schema_version,
                                   std::uint64_t layer_hash,
                                   std::uint64_t temporal_key,
                                   IxiptlahPayloadSpool& spool,
                                   IxiptlahIndexedRecord* written_record = nullptr) {
    if (written_record) *written_record = {};
    if (!spool.seal()) return false;
    const std::uint64_t payload_size = spool.size();
    if (payload_size > kMaxPayloadBytes) return false;

    const auto mark_raw_record = [&](std::streampos start_pos) {
        if (!written_record || start_pos < std::streampos(0)) return;
        written_record->type = type;
        written_record->schema = schema_version;
        const auto start_off = static_cast<std::streamoff>(start_pos);
        written_record->payload_offset = static_cast<std::uint64_t>(start_off) + ixiptlah_record_header_bytes_for_version(file_version);
        written_record->stored_size = payload_size;
        written_record->raw_size = payload_size;
        written_record->codec = kCodecRaw;
        written_record->layer_hash = layer_hash;
        written_record->temporal_key = temporal_key;
        ixiptlah_fill_preformed_address_fields(*written_record);
    };

    const std::uint64_t epi_raw_cutoff = env_u64_ix("TLALPOWA_IXIPTLAH_EPI_RAW_MAX_BYTES",
                                                     256ull * 1024ull,
                                                     0ull,
                                                     8ull * 1024ull * 1024ull);
    const bool epidemiology_small_hot_record =
        (type == IxiptlahRecordType::EpidemiologyRenderSnapshot) ||
        (type == IxiptlahRecordType::MonthlyEpidemiologyBatch && payload_size <= epi_raw_cutoff);
    const std::uint64_t graph_raw_cutoff = env_u64_ix("TLALPOWA_IXIPTLAH_GRAPH_RAW_MAX_BYTES",
                                                      32ull * 1024ull * 1024ull,
                                                      0ull,
                                                      256ull * 1024ull * 1024ull);
    const bool atmosphere_graph_hot_record =
        payload_size <= graph_raw_cutoff &&
        (type == IxiptlahRecordType::AtmosphereGraphLayerCatalog ||
         type == IxiptlahRecordType::AtmosphereGraphDailyStationBatch ||
         type == IxiptlahRecordType::AtmosphereGraphHourlyStationBatch ||
         type == IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch ||
         type == IxiptlahRecordType::AtmosphereRenderSummary ||
         type == IxiptlahRecordType::MonthlyAtmosphereRenderBatch);
    const bool compression_disabled = env_truthy_ix("TLALPOWA_IXIPTLAH_DISABLE_COMPRESSION") ||
                                      env_truthy_ix("TLALPOWA_IXIPTLAH_FORCE_RAW") ||
                                      env_truthy_ix("TLALPOWA_IXIPTLAH_V1_RAW") ||
                                      epidemiology_small_hot_record ||
                                      atmosphere_graph_hot_record;

    if (file_version >= kIxiptlahFileVersion && !compression_disabled && payload_size >= 1024u) {
        // IXIPTLAH V1: la compresión por bloques es la ruta normal. No se
        // lee el payload entero en RAM; se comprime en ventanas pequeñas y el
        // lector descomprime una ventana a la vez. El encabezado ya porta capa
        // y tiempo, por eso puede saltarse el payload de capas apagadas.
        // El spool comprimido se materializa en archivo temporal si rebasa el
        // límite de memoria; nunca exige sostener el payload completo residente.
        IxiptlahPayloadSpool packed_spool(fs::path("ixiptlah_compressed_block"));
        if (payload_size <= ixiptlah_compression_limit_bytes() &&
            ixiptlah_build_block_lz_payload(spool, fs::path{}, packed_spool) &&
            packed_spool.size() + 1024u < payload_size) {
            const std::streampos start_pos = out.tellp();
            const bool ok = ixiptlah_write_record_block_lz_payload(out, file_version, type, schema_version, layer_hash, temporal_key, payload_size, packed_spool);
            if (ok && written_record && start_pos >= std::streampos(0)) {
                written_record->type = type;
                written_record->schema = schema_version;
                written_record->payload_offset = static_cast<std::uint64_t>(static_cast<std::streamoff>(start_pos)) + ixiptlah_record_header_bytes_for_version(file_version);
                written_record->stored_size = packed_spool.size();
                written_record->raw_size = payload_size;
                written_record->codec = kCodecIxLzBlocks;
                written_record->layer_hash = layer_hash;
                written_record->temporal_key = temporal_key;
                ixiptlah_fill_preformed_address_fields(*written_record);
            }
            return ok;
        }
    }

    if (!compression_disabled && payload_size <= ixiptlah_compression_limit_bytes() && file_version < kIxiptlahFileVersion) {
        std::string bytes;
        if (spool.read_all_to_string(bytes)) return ixiptlah_write_record_with_meta(out, file_version, type, schema_version, layer_hash, temporal_key, bytes);
    }

    const std::streampos start_pos = out.tellp();
    const bool ok = ixiptlah_write_record_raw_payload(out, file_version, type, schema_version, layer_hash, temporal_key, payload_size, spool);
    if (ok) mark_raw_record(start_pos);
    return ok;
}


class IxiptlahLimitedInputStreamBuf final : public std::streambuf {
public:
    IxiptlahLimitedInputStreamBuf(std::istream& source, std::uint64_t bytes_left)
        : source_(source), remaining_(bytes_left) {
        setg(buffer_.data(), buffer_.data(), buffer_.data());
    }

protected:
    int_type underflow() override {
        if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
        if (remaining_ == 0 || !source_) return traits_type::eof();

        const std::size_t step = static_cast<std::size_t>(std::min<std::uint64_t>(remaining_, buffer_.size()));
        source_.read(buffer_.data(), static_cast<std::streamsize>(step));
        const std::streamsize got = source_.gcount();
        if (got <= 0) return traits_type::eof();

        remaining_ -= static_cast<std::uint64_t>(got);
        setg(buffer_.data(), buffer_.data(), buffer_.data() + got);
        return traits_type::to_int_type(*gptr());
    }

    std::streamsize xsgetn(char* dst, std::streamsize count) override {
        if (!dst || count <= 0) return 0;
        std::streamsize copied = 0;

        while (count > 0 && gptr() < egptr()) {
            const std::streamsize buffered = static_cast<std::streamsize>(egptr() - gptr());
            const std::streamsize step = std::min(count, buffered);
            std::memcpy(dst + copied, gptr(), static_cast<std::size_t>(step));
            gbump(static_cast<int>(step));
            copied += step;
            count -= step;
        }

        while (count > 0 && remaining_ > 0 && source_) {
            const std::streamsize step = static_cast<std::streamsize>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(count), remaining_));
            source_.read(dst + copied, step);
            const std::streamsize got = source_.gcount();
            if (got <= 0) break;
            remaining_ -= static_cast<std::uint64_t>(got);
            copied += got;
            count -= got;
            if (got != step) break;
        }

        return copied;
    }

private:
    std::istream& source_;
    std::uint64_t remaining_ = 0;
    std::array<char, 64u * 1024u> buffer_{};
};


class IxiptlahLimitedInputStream final : public std::istream {
public:
    IxiptlahLimitedInputStream(std::istream& source, std::uint64_t bytes_left)
        : std::istream(nullptr), buf_(source, bytes_left) {
        rdbuf(&buf_);
    }

private:
    IxiptlahLimitedInputStreamBuf buf_;
};



class IxiptlahMemoryInputStreamBuf final : public std::streambuf {
public:
    explicit IxiptlahMemoryInputStreamBuf(std::shared_ptr<const std::string> bytes)
        : bytes_(std::move(bytes)) {
        char* base = (bytes_ && !bytes_->empty()) ? const_cast<char*>(bytes_->data()) : nullptr;
        setg(base, base, base ? base + static_cast<std::ptrdiff_t>(bytes_->size()) : base);
    }

private:
    std::shared_ptr<const std::string> bytes_;
};

class IxiptlahMemoryInputStream final : public std::istream {
public:
    explicit IxiptlahMemoryInputStream(std::shared_ptr<const std::string> bytes)
        : std::istream(nullptr), buf_(std::move(bytes)) {
        rdbuf(&buf_);
    }
private:
    IxiptlahMemoryInputStreamBuf buf_;
};

struct IxiptlahPayloadCacheEntry {
    std::shared_ptr<const std::string> bytes;
    std::uint64_t size = 0;
};

std::mutex& ixiptlah_payload_cache_mu() {
    static std::mutex mu;
    return mu;
}

std::unordered_map<std::string, IxiptlahPayloadCacheEntry>& ixiptlah_payload_cache() {
    static std::unordered_map<std::string, IxiptlahPayloadCacheEntry> cache;
    return cache;
}

std::deque<std::string>& ixiptlah_payload_cache_lru() {
    static std::deque<std::string> lru;
    return lru;
}

std::uint64_t& ixiptlah_payload_cache_bytes() {
    static std::uint64_t total = 0;
    return total;
}

void ixiptlah_payload_cache_erase_path(const fs::path& path) {
    if (path.empty()) return;
    const std::string prefix = path_utf8(path.lexically_normal()) + "|";
    std::lock_guard<std::mutex> lock(ixiptlah_payload_cache_mu());
    auto& cache = ixiptlah_payload_cache();
    auto& lru = ixiptlah_payload_cache_lru();
    auto& total = ixiptlah_payload_cache_bytes();
    for (auto it = cache.begin(); it != cache.end(); ) {
        if (it->first.rfind(prefix, 0) == 0) {
            total = (total >= it->second.size) ? (total - it->second.size) : 0ull;
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
    lru.erase(std::remove_if(lru.begin(), lru.end(), [&](const std::string& key) {
        return key.rfind(prefix, 0) == 0;
    }), lru.end());
}


bool ixiptlah_hot_payload_type(IxiptlahRecordType type) {
    switch (type) {
        case IxiptlahRecordType::EpidemiologyObservation:
        case IxiptlahRecordType::EpidemiologyQuarantine:
        case IxiptlahRecordType::AtmosphereMeasurement:
        case IxiptlahRecordType::AtmosphereTerritoryAverage:
        case IxiptlahRecordType::AtmosphereGraphLayerCatalog:
        case IxiptlahRecordType::AtmosphereGraphDailyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphHourlyStationBatch:
        case IxiptlahRecordType::AtmosphereGraphWeeklyStationBatch:
        case IxiptlahRecordType::MonthlyAtmosphereMeasurementBatch:
        case IxiptlahRecordType::MonthlyAtmosphereRenderBatch:
        case IxiptlahRecordType::MonthlyAtmosphereTerritoryBatch:
        case IxiptlahRecordType::EpidemiologyRenderSnapshot:
        case IxiptlahRecordType::MonthlyEpidemiologyBatch:
            return true;
        default:
            return false;
    }
}

std::uint64_t ixiptlah_payload_cache_budget_bytes() {
    return env_u64_ix("TLALPOWA_IXIPTLAH_PAYLOAD_CACHE_BYTES",
                      384ull * 1024ull * 1024ull,
                      0ull,
                      4096ull * 1024ull * 1024ull);
}

std::uint64_t ixiptlah_payload_cache_record_limit_bytes() {
    return env_u64_ix("TLALPOWA_IXIPTLAH_PAYLOAD_CACHE_RECORD_BYTES",
                      96ull * 1024ull * 1024ull,
                      0ull,
                      768ull * 1024ull * 1024ull);
}

std::string ixiptlah_payload_cache_key(const fs::path& path,
                                       std::uint64_t file_size,
                                       std::int64_t mtime_ns,
                                       const IxiptlahIndexedRecord& rec) {
    std::string key = path_utf8(path.lexically_normal());
    key.push_back('|'); key += std::to_string(file_size);
    key.push_back('|'); key += std::to_string(mtime_ns);
    key.push_back('|'); key += std::to_string(static_cast<std::uint32_t>(rec.type));
    key.push_back('|'); key += std::to_string(rec.schema);
    key.push_back('|'); key += std::to_string(rec.layer_hash);
    key.push_back('|'); key += std::to_string(rec.temporal_key);
    key.push_back('|'); key += std::to_string(rec.payload_offset);
    key.push_back('|'); key += std::to_string(rec.stored_size);
    key.push_back('|'); key += std::to_string(rec.raw_size);
    key.push_back('|'); key += std::to_string(rec.codec);
    return key;
}

void ixiptlah_payload_cache_touch_locked(const std::string& key) {
    auto& lru = ixiptlah_payload_cache_lru();
    lru.erase(std::remove(lru.begin(), lru.end(), key), lru.end());
    lru.push_back(key);
}

std::shared_ptr<const std::string> ixiptlah_payload_cache_get(const std::string& key) {
    if (key.empty() || ixiptlah_payload_cache_budget_bytes() == 0ull) return {};
    std::lock_guard<std::mutex> lock(ixiptlah_payload_cache_mu());
    auto& cache = ixiptlah_payload_cache();
    auto it = cache.find(key);
    if (it == cache.end() || !it->second.bytes) return {};
    ixiptlah_payload_cache_touch_locked(key);
    return it->second.bytes;
}

void ixiptlah_payload_cache_put(const std::string& key, std::shared_ptr<const std::string> bytes) {
    if (key.empty() || !bytes) return;
    const std::uint64_t budget = ixiptlah_payload_cache_budget_bytes();
    const std::uint64_t record_limit = ixiptlah_payload_cache_record_limit_bytes();
    const std::uint64_t n = static_cast<std::uint64_t>(bytes->size());
    if (budget == 0ull || record_limit == 0ull || n == 0ull || n > record_limit || n > budget) return;

    std::lock_guard<std::mutex> lock(ixiptlah_payload_cache_mu());
    auto& cache = ixiptlah_payload_cache();
    auto& total = ixiptlah_payload_cache_bytes();
    auto it = cache.find(key);
    if (it != cache.end()) {
        total = (total >= it->second.size) ? (total - it->second.size) : 0ull;
        it->second.bytes = std::move(bytes);
        it->second.size = n;
    } else {
        cache.emplace(key, IxiptlahPayloadCacheEntry{std::move(bytes), n});
    }
    total += n;
    ixiptlah_payload_cache_touch_locked(key);

    auto& lru = ixiptlah_payload_cache_lru();
    while (total > budget && !lru.empty()) {
        const std::string evict = lru.front();
        lru.pop_front();
        auto eit = cache.find(evict);
        if (eit == cache.end()) continue;
        total = (total >= eit->second.size) ? (total - eit->second.size) : 0ull;
        cache.erase(eit);
    }
}

bool ixiptlah_payload_should_cache(const IxiptlahIndexedRecord& rec) {
    if (rec.raw_size == 0ull || rec.raw_size > kMaxPayloadBytes) return false;
    if (rec.stored_size > kMaxPayloadBytes) return false;
    if (!ixiptlah_hot_payload_type(rec.type)) return false;
    if (rec.codec != kCodecRaw && rec.codec != kCodecIxLz && rec.codec != kCodecIxLzBlocks) return false;
    return rec.raw_size <= ixiptlah_payload_cache_record_limit_bytes();
}

class IxiptlahBlockLzInputStreamBuf final : public std::streambuf {
public:
    IxiptlahBlockLzInputStreamBuf(std::istream& source, std::uint64_t stored_bytes, std::uint64_t raw_bytes)
        : source_(source), stored_remaining_(stored_bytes), raw_remaining_(raw_bytes) {
        setg(nullptr, nullptr, nullptr);
    }

protected:
    int_type underflow() override {
        if (gptr() && gptr() < egptr()) return traits_type::to_int_type(*gptr());
        if (!fill_next_block()) return traits_type::eof();
        return traits_type::to_int_type(*gptr());
    }

    std::streamsize xsgetn(char* dst, std::streamsize count) override {
        if (!dst || count <= 0) return 0;
        std::streamsize copied = 0;
        while (count > 0) {
            if (!gptr() || gptr() >= egptr()) {
                if (!fill_next_block()) break;
            }
            const std::streamsize avail = static_cast<std::streamsize>(egptr() - gptr());
            const std::streamsize step = std::min(count, avail);
            std::memcpy(dst + copied, gptr(), static_cast<std::size_t>(step));
            gbump(static_cast<int>(step));
            copied += step;
            count -= step;
        }
        return copied;
    }

private:
    bool fill_next_block() {
        decoded_.clear();
        if (stored_remaining_ == 0 || raw_remaining_ == 0 || !source_) return false;
        std::uint32_t raw_n = 0, stored_n = 0, block_codec = 0;
        if (stored_remaining_ < 12u) return false;
        if (!ixiptlah_read_value(source_, raw_n) || !ixiptlah_read_value(source_, stored_n) || !ixiptlah_read_value(source_, block_codec)) return false;
        stored_remaining_ -= 12u;
        if (raw_n == 0 || raw_n > kIxiptlahCompressionBlockBytes || raw_n > raw_remaining_) return false;
        if (stored_n > stored_remaining_ || stored_n > kIxiptlahCompressionBlockBytes + 64u) return false;
        std::string stored(stored_n, '\0');
        if (stored_n > 0) source_.read(stored.data(), static_cast<std::streamsize>(stored_n));
        if (!source_) return false;
        stored_remaining_ -= stored_n;
        if (block_codec == kIxiptlahBlockCodecRaw) {
            if (stored.size() != raw_n) return false;
            decoded_ = std::move(stored);
        } else if (block_codec == kIxiptlahBlockCodecIxLz) {
            if (!ix_lz_decompress(stored, raw_n, decoded_)) return false;
        } else {
            return false;
        }
        if (decoded_.size() != raw_n) return false;
        raw_remaining_ -= raw_n;
        setg(decoded_.data(), decoded_.data(), decoded_.data() + decoded_.size());
        return true;
    }

    std::istream& source_;
    std::uint64_t stored_remaining_ = 0;
    std::uint64_t raw_remaining_ = 0;
    std::string decoded_;
};

class IxiptlahBlockLzInputStream final : public std::istream {
public:
    IxiptlahBlockLzInputStream(std::istream& source, std::uint64_t stored_bytes, std::uint64_t raw_bytes)
        : std::istream(nullptr), buf_(source, stored_bytes, raw_bytes) {
        rdbuf(&buf_);
    }
private:
    IxiptlahBlockLzInputStreamBuf buf_;
};


void ixiptlah_configure_hot_read_buffer(std::ifstream& in) {
    // Buffer por hilo: la lectura exacta IXIPTLAH hace muchos seeks cortos sobre
    // payloads compactos. Un bloque residente evita que cada salto pague el
    // tamaño conservador del streambuf estándar sin cambiar el formato físico.
    static thread_local std::vector<char> buffer;
    const std::size_t wanted = static_cast<std::size_t>(env_u64_ix(
        "TLALPOWA_IXIPTLAH_READ_BUFFER_BYTES",
        2ull * 1024ull * 1024ull,
        64ull * 1024ull,
        16ull * 1024ull * 1024ull));
    if (buffer.size() != wanted) {
        try { buffer.assign(wanted, 0); } catch (...) { buffer.clear(); }
    }
    if (!buffer.empty()) in.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

std::ifstream ixiptlah_open_binary_input(const fs::path& path) {
    std::ifstream in;
    ixiptlah_configure_hot_read_buffer(in);
    in.open(path, std::ios::binary);
    return in;
}


bool ixiptlah_dispatch_payload(std::ifstream& in,
                               const fs::path& path,
                               std::uint64_t file_size,
                               std::int64_t mtime_ns,
                               const IxiptlahIndexedRecord& rec,
                               const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload,
                               bool& keep_going) {
    keep_going = true;
    if (!read_payload) return false;
    if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) return false;
    if (rec.codec == kCodecRaw && rec.raw_size != rec.stored_size) return false;
    if (rec.codec != kCodecRaw && rec.codec != kCodecIxLz && rec.codec != kCodecIxLzBlocks) return false;

    const bool cacheable = ixiptlah_payload_should_cache(rec);
    const std::string cache_key = cacheable ? ixiptlah_payload_cache_key(path, file_size, mtime_ns, rec) : std::string{};

    try {
        if (cacheable) {
            if (auto cached = ixiptlah_payload_cache_get(cache_key)) {
                IxiptlahMemoryInputStream payload_in(cached);
                keep_going = read_payload(rec.type, rec.schema, payload_in);
                return true;
            }
        }

        in.clear();
        in.seekg(static_cast<std::streamoff>(rec.payload_offset), std::ios::beg);
        if (!in) return false;

        if (rec.codec == kCodecRaw) {
            if (cacheable) {
                std::string bytes(static_cast<std::size_t>(rec.stored_size), '\0');
                if (rec.stored_size > 0) in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                if (!in) return false;
                auto cached_bytes = std::make_shared<const std::string>(std::move(bytes));
                ixiptlah_payload_cache_put(cache_key, cached_bytes);
                IxiptlahMemoryInputStream payload_in(cached_bytes);
                keep_going = read_payload(rec.type, rec.schema, payload_in);
                return true;
            }

            IxiptlahLimitedInputStream payload_in(in, rec.stored_size);
            keep_going = read_payload(rec.type, rec.schema, payload_in);
            return true;
        }

        if (cacheable) {
            std::string stored(static_cast<std::size_t>(rec.stored_size), '\0');
            if (rec.stored_size > 0) in.read(stored.data(), static_cast<std::streamsize>(stored.size()));
            if (!in) return false;

            IxiptlahRecordEnvelope env;
            env.type = rec.type;
            env.schema = rec.schema;
            env.stored_size = rec.stored_size;
            env.raw_size = rec.raw_size;
            env.codec = rec.codec;
            env.layer_hash = rec.layer_hash;
            env.temporal_key = rec.temporal_key;

            std::string decoded;
            if (!ixiptlah_decode_payload(std::move(stored), env, decoded)) return false;
            if (decoded.size() != rec.raw_size) return false;
            auto cached_bytes = std::make_shared<const std::string>(std::move(decoded));
            ixiptlah_payload_cache_put(cache_key, cached_bytes);
            IxiptlahMemoryInputStream payload_in(cached_bytes);
            keep_going = read_payload(rec.type, rec.schema, payload_in);
            return true;
        }

        if (rec.codec == kCodecIxLzBlocks) {
            IxiptlahBlockLzInputStream payload_in(in, rec.stored_size, rec.raw_size);
            keep_going = read_payload(rec.type, rec.schema, payload_in);
            return true;
        }

        std::string stored(static_cast<std::size_t>(rec.stored_size), '\0');
        if (rec.stored_size > 0) in.read(stored.data(), static_cast<std::streamsize>(stored.size()));
        if (!in) return false;

        IxiptlahRecordEnvelope env;
        env.type = rec.type;
        env.schema = rec.schema;
        env.stored_size = rec.stored_size;
        env.raw_size = rec.raw_size;
        env.codec = rec.codec;
        env.layer_hash = rec.layer_hash;
        env.temporal_key = rec.temporal_key;
        std::string decoded;

        if (!ixiptlah_decode_payload(std::move(stored), env, decoded)) return false;
        std::istringstream payload_in(decoded, std::ios::in | std::ios::binary);
        keep_going = read_payload(rec.type, rec.schema, payload_in);
        return true;
    } catch (const std::bad_alloc&) {
        keep_going = false;
        return false;
    } catch (const std::exception&) {
        keep_going = true;
        return false;
    } catch (...) {
        keep_going = true;
        return false;
    }
}


// Ruta de lectura caliente con apertura perezosa. En consultas repetidas, el
// payload ya decodificado puede estar residente: no se abre el archivo, no se
// hace seek y no se toca disco. Si falta la entrada en caché, se abre una sola
// vez por consulta exacta y se reutiliza el descriptor para todos los offsets.
bool ixiptlah_dispatch_payload_lazy(std::unique_ptr<std::ifstream>& lazy_in,
                                    const fs::path& path,
                                    std::uint64_t file_size,
                                    std::int64_t mtime_ns,
                                    const IxiptlahIndexedRecord& rec,
                                    const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload,
                                    bool& keep_going) {
    keep_going = true;
    if (!read_payload) return false;
    if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) return false;
    if (rec.codec == kCodecRaw && rec.raw_size != rec.stored_size) return false;
    if (rec.codec != kCodecRaw && rec.codec != kCodecIxLz && rec.codec != kCodecIxLzBlocks) return false;

    const bool cacheable = ixiptlah_payload_should_cache(rec);
    const std::string cache_key = cacheable ? ixiptlah_payload_cache_key(path, file_size, mtime_ns, rec) : std::string{};
    if (cacheable) {
        if (auto cached = ixiptlah_payload_cache_get(cache_key)) {
            try {
                IxiptlahMemoryInputStream payload_in(cached);
                keep_going = read_payload(rec.type, rec.schema, payload_in);
                return true;
            } catch (const std::bad_alloc&) {
                keep_going = false;
                return false;
            } catch (...) {
                keep_going = true;
                return false;
            }
        }
    }

    if (!lazy_in) {
        lazy_in = std::make_unique<std::ifstream>();
        ixiptlah_configure_hot_read_buffer(*lazy_in);
        lazy_in->open(path, std::ios::binary);
        if (!lazy_in || !(*lazy_in)) return false;
    }
    return ixiptlah_dispatch_payload(*lazy_in, path, file_size, mtime_ns, rec, read_payload, keep_going);
}






bool ixiptlah_upgrade_file_to_current_version_locked(const fs::path& path) {
    if (path.empty() || !fs::exists(path) || file_size_or_zero(path) == 0) return true;

    {
        auto probe = ixiptlah_open_binary_input(path);
        std::uint32_t version = 0;
        if (!probe || !ixiptlah_read_file_header(probe, version)) return false;
        if (version >= kIxiptlahFileVersion) return true;
        if (env_truthy_ix("TLALPOWA_IXIPTLAH_DISABLE_AUTO_UPGRADE")) return true;
    }

    IxiptlahFileIndex index;
    if (!ixiptlah_scan_file_index(path, index) || !index.valid) return false;

    auto in = ixiptlah_open_binary_input(path);
    if (!in) return false;

    const fs::path tmp = ixiptlah_temp_path_near(path, ".upgrade.tmp");
    std::error_code ec;
    fs::remove(tmp, ec);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out || !ixiptlah_write_file_header(out, kIxiptlahFileVersion)) return false;

        for (const IxiptlahIndexedRecord& rec : index.records) {
            in.clear();
            in.seekg(static_cast<std::streamoff>(rec.payload_offset), std::ios::beg);
            if (!in || !ixiptlah_write_indexed_record_raw_stream(in, out, rec, kIxiptlahFileVersion)) {
                fs::remove(tmp, ec);
                return false;
            }
        }
        if (!out) {
            fs::remove(tmp, ec);
            return false;
        }
    }

    in.close();
    fs::rename(tmp, path, ec);
    if (ec) {
        ec.clear();
        fs::copy_file(tmp, path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            fs::remove(tmp, ec);
            return false;
        }
        fs::remove(tmp, ec);
    }

    ixiptlah_invalidate_index_cache(path);
    return true;
}


IxiptlahSink* ixiptlah_sink_for_path_locked(const fs::path& path) {

    if (path.empty()) return nullptr;

    ensure_dir(path.parent_path());


    auto& sinks = ixiptlah_sinks();

    const std::string key = path_utf8(path);
    auto it = sinks.find(key);

    if (it != sinks.end()) {


        it->second->last_used = ++ixiptlah_tick();

        return it->second->stream ? it->second.get() : nullptr;
    }


    const bool needs_header = !fs::exists(path) || file_size_or_zero(path) == 0;

    IxiptlahFileIndex resident_index;
    bool resident_index_valid = false;
    if (!needs_header) {
        // Antes de recortar el directorio terminal, se hidrata su contenido en RAM.
        // Así el archivo abierto para append conserva sus offsets sin volver a
        // barrer décadas completas cuando la UI pide una gráfica justo después.
        resident_index_valid = (ixiptlah_read_embedded_directory(path, resident_index) ||
                                ixiptlah_scan_file_index(path, resident_index)) &&
                               resident_index.valid;
        if (!ixiptlah_strip_embedded_directory(path)) return nullptr;
    }

    if (!needs_header) {
        auto existing = ixiptlah_open_binary_input(path);
        std::uint32_t existing_version = 0;
        if (!existing || !ixiptlah_read_file_header(existing, existing_version) || existing_version != kIxiptlahFileVersion) {
            // Sin retrocompatibilidad: un núcleo viejo se reemplaza por un V1
            // elemental. La importación es la fuente de verdad, no el archivo mixto.
            existing.close();
            resident_index = {};
            resident_index_valid = false;
            std::error_code ec;
            fs::remove(path, ec);
        }
    }


    const bool needs_header_after_version_check = !fs::exists(path) || file_size_or_zero(path) == 0;
    const std::uint32_t version = ixiptlah_default_write_version();


    auto sink = std::make_unique<IxiptlahSink>();

    sink->path = path;
    sink->version = version;
    if (resident_index_valid) {
        sink->live_records = std::move(resident_index.records);
        sink->live_index_valid = true;
        sink->live_revision = 1;
    }


    sink->last_used = ++ixiptlah_tick();

    const std::uint64_t buffer_bytes = env_u64_ix("TLALPOWA_IXIPTLAH_WRITE_BUFFER_BYTES",
                                                   1024ull * 1024ull,
                                                   64ull * 1024ull,
                                                   16ull * 1024ull * 1024ull);
    sink->io_buffer.resize(static_cast<std::size_t>(buffer_bytes));
    if (!sink->io_buffer.empty()) {
        sink->stream.rdbuf()->pubsetbuf(sink->io_buffer.data(), static_cast<std::streamsize>(sink->io_buffer.size()));
    }

    sink->stream.open(path, std::ios::binary | std::ios::app);

    if (!sink->stream) return nullptr;


    if (needs_header_after_version_check && !ixiptlah_write_file_header(sink->stream, version)) return nullptr;
    auto* out_sink = sink.get();

    sinks.emplace(key, std::move(sink));
    constexpr size_t kMaxOpenSinks = 96;

    if (sinks.size() > kMaxOpenSinks) {
        auto victim = sinks.end();

        for (auto jt = sinks.begin(); jt != sinks.end(); ++jt) {

            if (jt->first == key) continue;

            if (victim == sinks.end() || jt->second->last_used < victim->second->last_used) victim = jt;
        }

        if (victim != sinks.end()) {

            const fs::path victim_path = victim->second->path;
            const std::vector<IxiptlahIndexedRecord> victim_records = victim->second->live_records;
            victim->second->stream.flush();
            victim->second->stream.close();

            sinks.erase(victim);
            if (!victim_path.empty() && !victim_records.empty()) {
                (void)ixiptlah_embed_terminal_directory_from_records(victim_path, victim_records);
            } else if (!victim_path.empty()) {
                (void)ixiptlah_embed_terminal_directory(victim_path);
            }
        }
    }

    return out_sink;
}

}




fs::path ixiptlah_path(const fs::path& root, const std::string& stem) {


    return root / (stem + kIxiptlahExtension);
}


std::uint64_t ixiptlah_layer_hash(const std::string& layer_key) {
    // FNV-1a 64-bit deliberadamente estable: no depende de std::hash ni de ABI.
    // La capa vacía produce 0 para conservar compatibilidad con IXIPTLAH previo.
    const std::string key = normalize_key(layer_key);
    if (key.empty()) return 0;
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : key) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h == 0 ? 1 : h;
}




bool ixiptlah_write_string(std::ostream& out, const std::string& value) {

    if (value.size() > static_cast<size_t>(std::numeric_limits<std::uint32_t>::max())) return false;

    const std::uint32_t n = static_cast<std::uint32_t>(value.size());


    if (!ixiptlah_write_value(out, n)) return false;

    if (n > 0) out.write(value.data(), static_cast<std::streamsize>(n));

    return static_cast<bool>(out);
}




bool ixiptlah_read_string(std::istream& in, std::string& value) {
    std::uint32_t n = 0;


    if (!ixiptlah_read_value(in, n)) return false;

    if (n > 64u * 1024u * 1024u) return false;
    value.assign(n, '\0');

    if (n > 0) in.read(value.data(), static_cast<std::streamsize>(n));


    return static_cast<bool>(in);
}



bool ixiptlah_append_record_tagged_impl(const fs::path& path,

                                  IxiptlahRecordType type,

                                  std::uint32_t schema_version,

                                  std::uint64_t layer_hash,

                                  std::uint64_t temporal_key,

                                  const std::function<bool(std::ostream&)>& write_payload,
                                  bool flush_after) {
    if (path.empty()) return false;

    IxiptlahPayloadSpool spool(path);
    std::ostream payload(&spool);
    if (!write_payload(payload)) return false;
    payload.flush();
    if (!payload || !spool.ok() || !spool.seal()) return false;


    std::lock_guard<std::mutex> lock(ixiptlah_mu());


    IxiptlahSink* sink = ixiptlah_sink_for_path_locked(path);

    if (!sink || !sink->stream) return false;


    IxiptlahIndexedRecord written;
    const bool ok = ixiptlah_write_spooled_record(sink->stream, sink->version, type, schema_version, layer_hash, temporal_key, spool, &written);

    if (ok) {

        if (written.payload_offset != 0 && sink->live_index_valid) {
            sink->live_records.push_back(written);
            ++sink->live_revision;
            sink->live_snapshot.reset();
            sink->live_snapshot_revision = std::numeric_limits<std::uint64_t>::max();
        }

        // V1 mantiene núcleos por década y encabezados ricos: la ruta caliente
        // puede diferir flush y conservar el índice vivo como verdad inmediata.
        // No se invalida el cache global por cada registro; el sink abierto sombrea
        // cualquier índice anterior y el cierre emite directorio terminal + purge.
        if (flush_after) {
            sink->stream.flush();
            if (!sink->stream) return false;
        }

        if (sink->version >= kIxiptlahFileVersion && written.payload_offset != 0) {
            (void)ixiptlah_sm_index_append_entry(path, written, layer_hash);
        }

    }

    return ok;
}


bool ixiptlah_append_record_tagged(const fs::path& path,

                                  IxiptlahRecordType type,

                                  std::uint32_t schema_version,

                                  std::uint64_t layer_hash,

                                  const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_impl(path, type, schema_version, layer_hash, 0ull, write_payload, true);
}


bool ixiptlah_append_record_tagged(const fs::path& path,

                                  IxiptlahRecordType type,

                                  std::uint32_t schema_version,

                                  const std::string& layer_key,

                                  const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged(path, type, schema_version, ixiptlah_layer_hash(layer_key), write_payload);
}

bool ixiptlah_append_record_tagged_temporal(const fs::path& path,
                                           IxiptlahRecordType type,
                                           std::uint32_t schema_version,
                                           std::uint64_t layer_hash,
                                           std::uint64_t temporal_key,
                                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_impl(path, type, schema_version, layer_hash, temporal_key, write_payload, true);
}

bool ixiptlah_append_record_tagged_temporal(const fs::path& path,
                                           IxiptlahRecordType type,
                                           std::uint32_t schema_version,
                                           const std::string& layer_key,
                                           std::uint64_t temporal_key,
                                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_temporal(path, type, schema_version, ixiptlah_layer_hash(layer_key), temporal_key, write_payload);
}

bool ixiptlah_append_record_tagged_temporal_deferred_flush(const fs::path& path,
                                           IxiptlahRecordType type,
                                           std::uint32_t schema_version,
                                           std::uint64_t layer_hash,
                                           std::uint64_t temporal_key,
                                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_impl(path, type, schema_version, layer_hash, temporal_key, write_payload, false);
}

bool ixiptlah_append_record_tagged_temporal_deferred_flush(const fs::path& path,
                                           IxiptlahRecordType type,
                                           std::uint32_t schema_version,
                                           const std::string& layer_key,
                                           std::uint64_t temporal_key,
                                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_temporal_deferred_flush(path, type, schema_version, ixiptlah_layer_hash(layer_key), temporal_key, write_payload);
}

bool ixiptlah_append_record(const fs::path& path,


                           IxiptlahRecordType type,


                           std::uint32_t schema_version,


                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged(path, type, schema_version, 0ull, write_payload);
}

bool ixiptlah_append_record_deferred_flush(const fs::path& path,


                           IxiptlahRecordType type,


                           std::uint32_t schema_version,


                           const std::function<bool(std::ostream&)>& write_payload) {
    return ixiptlah_append_record_tagged_impl(path, type, schema_version, 0ull, 0ull, write_payload, false);
}



bool ixiptlah_write_single_record_atomic(const fs::path& path,


                                        IxiptlahRecordType type,


                                        std::uint32_t schema_version,


                                        const std::function<bool(std::ostream&)>& write_payload) {

    if (path.empty()) return false;

    IxiptlahPayloadSpool spool(path);
    std::ostream payload(&spool);
    if (!write_payload(payload)) return false;
    payload.flush();
    if (!payload || !spool.ok() || !spool.seal()) return false;

    ensure_dir(path.parent_path());

    const fs::path tmp_path = fs::path(path.wstring() + L".tmp");


    const std::uint32_t version = ixiptlah_default_write_version();
    {


        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);


        if (!out || !ixiptlah_write_file_header(out, version)) return false;


        if (!ixiptlah_write_spooled_record(out, version, type, schema_version, 0ull, 0ull, spool)) return false;

        if (!out) return false;
    }


    std::lock_guard<std::mutex> lock(ixiptlah_mu());


    auto& sinks = ixiptlah_sinks();

    const std::string key = path_utf8(path);

    if (auto it = sinks.find(key); it != sinks.end()) {


        if (it->second && it->second->stream) {
            it->second->stream.flush();
            it->second->stream.close();
        }

        sinks.erase(it);
    }
    std::error_code ec;


    fs::rename(tmp_path, path, ec);

    if (ec) {

        fs::remove(path, ec);

        ec.clear();


        fs::rename(tmp_path, path, ec);

        if (ec) {

            fs::remove(tmp_path, ec);

            return false;
        }
    }


    ixiptlah_invalidate_index_cache(path);

    return true;
}



bool ixiptlah_stream_records_from_sm_index(const fs::path& path,
                                         const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t)>& accept_record,
                                         const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload,
                                         bool& used_index) {
    used_index = false;
    if (ixiptlah_sm_index_disabled() || path.empty() || !read_payload) return false;

    const fs::path index_path = ixiptlah_sm_index_path(path);
    auto idx = ixiptlah_open_binary_input(index_path);
    if (!idx) return false;

    IxiptlahSmIndexHeader h;
    if (!ixiptlah_sm_read_header(idx, h)) return false;

    const std::uint64_t current_size = static_cast<std::uint64_t>(file_size_or_zero(path));
    const std::int64_t current_mtime = ixiptlah_mtime_ns(path);
    if (h.ix_file_size != current_size || h.ix_mtime_ns != current_mtime) return false;
    if (h.record_count > (current_size / 4ull + 16ull)) return false;

    auto data = ixiptlah_open_binary_input(path);
    if (!data) return false;

    used_index = true;
    for (std::uint64_t i = 0; i < h.record_count; ++i) {
        IxiptlahSmIndexEntry e;
        if (!ixiptlah_sm_read_entry(idx, e)) break;

        bool accepted = true;
        try { accepted = !accept_record || accept_record(e.type, e.schema, e.layer_hash); } catch (...) { accepted = false; }
        if (!accepted) continue;

        IxiptlahIndexedRecord rec;
        rec.type = e.type;
        rec.schema = e.schema;
        rec.payload_offset = e.payload_offset;
        rec.stored_size = e.stored_size;
        rec.raw_size = e.raw_size;
        rec.codec = e.codec;

        if (rec.payload_offset > current_size || rec.stored_size > current_size - rec.payload_offset) break;

        bool keep_going = true;
        if (ixiptlah_dispatch_payload(data, path, current_size, current_mtime, rec, read_payload, keep_going) && !keep_going) break;
    }

    return true;
}


std::vector<IxiptlahRecordManifestEntry> ixiptlah_record_manifest(const fs::path& path) {
    std::vector<IxiptlahRecordManifestEntry> out;
    const auto index = ixiptlah_index_for_path_shared(path);
    if (!index || !index->valid) return out;
    out.reserve(index->records.size());
    for (const IxiptlahIndexedRecord& rec : index->records) {
        IxiptlahRecordManifestEntry e;
        e.type = rec.type;
        e.schema = rec.schema;
        e.stored_size = rec.stored_size;
        e.raw_size = rec.raw_size;
        e.layer_hash = rec.layer_hash;
        e.temporal_key = rec.temporal_key;
        out.push_back(e);
    }
    return out;
}


void ixiptlah_read_records(const fs::path& path,



                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {

    bool used_sm_index = false;
    if (ixiptlah_stream_records_from_sm_index(path, {}, read_payload, used_sm_index) && used_sm_index) return;


    const auto index = ixiptlah_index_for_path_shared(path);


    if (!index || !index->valid) return;


    auto in = ixiptlah_open_binary_input(path);

    if (!in) return;


    for (const IxiptlahIndexedRecord& rec : index->records) {
        bool keep_going = true;

        if (ixiptlah_dispatch_payload(in, path, index->file_size, index->mtime_ns, rec, read_payload, keep_going) && !keep_going) break;
    }
}



void ixiptlah_read_selected_records(const fs::path& path,


                                   const std::function<bool(IxiptlahRecordType, std::uint32_t)>& accept_record,



                                   const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {

    bool used_sm_index = false;
    const auto accept_layerless = [&](IxiptlahRecordType type, std::uint32_t schema, std::uint64_t) {
        return !accept_record || accept_record(type, schema);
    };
    if (ixiptlah_stream_records_from_sm_index(path, accept_layerless, read_payload, used_sm_index) && used_sm_index) return;


    const auto index = ixiptlah_index_for_path_shared(path);

    if (!index || !index->valid) return;


    auto in = ixiptlah_open_binary_input(path);

    if (!in) return;


    for (const IxiptlahIndexedRecord& rec : index->records) {
        bool accepted = true;


        try { accepted = !accept_record || accept_record(rec.type, rec.schema); } catch (...) { accepted = false; }

        if (!accepted) continue;

        bool keep_going = true;
        if (ixiptlah_dispatch_payload(in, path, index->file_size, index->mtime_ns, rec, read_payload, keep_going) && !keep_going) break;
    }
}


void ixiptlah_read_selected_records_tagged(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {
    ixiptlah_read_selected_records_tagged_temporal(path,
        [&](IxiptlahRecordType type, std::uint32_t schema, std::uint64_t layer_hash, std::uint64_t) {
            return !accept_record || accept_record(type, schema, layer_hash);
        },
        read_payload);
}

void ixiptlah_read_selected_records_tagged_temporal(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {
    const auto index = ixiptlah_index_for_path_shared(path);
    if (!index || !index->valid) return;

    auto in = ixiptlah_open_binary_input(path);
    if (!in) return;

    std::vector<const IxiptlahIndexedRecord*> ordered;
    ordered.reserve(index->records.size());

    const auto record_desc_precedes = [](const IxiptlahIndexedRecord* a, const IxiptlahIndexedRecord* b) {
        if (!a || !b) return b != nullptr;
        if (a->temporal_key != b->temporal_key) return a->temporal_key > b->temporal_key;
        return a->payload_offset > b->payload_offset;
    };
    bool already_descending = true;
    const IxiptlahIndexedRecord* previous = nullptr;

    // Prefiltro sobre encabezado V1 antes de ordenar o tocar payload: el costo
    // dominante queda reducido a comparaciones de enteros cache-friendly. Las
    // capas apagadas, semanas fuera de ventana y esquemas ajenos nunca entran al
    // vector caliente ni fuerzan seek/descompresion.
    for (const IxiptlahIndexedRecord& rec : index->records) {
        bool accepted = true;
        try {
            accepted = !accept_record || accept_record(rec.type, rec.schema, rec.layer_hash, rec.temporal_key);
        } catch (...) {
            accepted = false;
        }
        if (!accepted) continue;
        if (previous && !record_desc_precedes(previous, &rec)) already_descending = false;
        previous = &rec;
        ordered.push_back(&rec);
    }

    // Los núcleos nuevos ya suelen venir en orden físico útil. Sólo se ordena el
    // subconjunto aceptado cuando el directorio muestra inversión real; evitar un
    // sort O(n log n) en cada clic conserva el camino caliente en comparaciones y
    // seeks estrictamente necesarios.
    if (!already_descending && ordered.size() > 1u) {
        std::stable_sort(ordered.begin(), ordered.end(), record_desc_precedes);
    }

    for (const IxiptlahIndexedRecord* ptr : ordered) {
        if (!ptr) continue;
        const IxiptlahIndexedRecord& rec = *ptr;
        bool keep_going = true;
        if (ixiptlah_dispatch_payload(in, path, index->file_size, index->mtime_ns, rec, read_payload, keep_going) && !keep_going) break;
    }
}

void ixiptlah_read_selected_records_tagged_temporal_physical(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {
    const auto index = ixiptlah_index_for_path_shared(path);
    if (!index || !index->valid) return;

    auto in = ixiptlah_open_binary_input(path);
    if (!in) return;

    // Ruta física append-only: no hay vector temporal ni sort. El encabezado ya
    // descarta capa/semana/esquema; el payload sólo se abre en registros que aún
    // pueden alterar la representación visible. Esto protege la semántica de
    // snapshot completo seguido por deltas, que depende del orden de escritura.
    for (const IxiptlahIndexedRecord& rec : index->records) {
        bool accepted = true;
        try {
            accepted = !accept_record || accept_record(rec.type, rec.schema, rec.layer_hash, rec.temporal_key);
        } catch (...) {
            accepted = false;
        }
        if (!accepted) continue;

        bool keep_going = true;
        if (ixiptlah_dispatch_payload(in, path, index->file_size, index->mtime_ns, rec, read_payload, keep_going) && !keep_going) break;
    }
}



void ixiptlah_read_selected_records_tagged_temporal_exact(const fs::path& path,

                                          IxiptlahRecordType type,

                                          std::uint32_t schema,

                                          const std::vector<std::uint64_t>& layer_hashes,

                                          bool include_zero_layer,

                                          std::uint64_t temporal_begin,

                                          std::uint64_t temporal_end,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload) {
    if (path.empty() || !read_payload) return;
    const auto index = ixiptlah_index_for_path_shared(path);
    if (!index || !index->valid) return;

    std::unique_ptr<std::ifstream> lazy_in;
    const auto dispatch_exact_payload = [&](const IxiptlahIndexedRecord& rec, bool& keep_going) -> bool {
        return ixiptlah_dispatch_payload_lazy(lazy_in, path, index->file_size, index->mtime_ns, rec, read_payload, keep_going);
    };

    if (ixiptlah_type_is_epidemiology_temporal(type)) {
        if (temporal_begin > 0ull && temporal_begin < 1000000ull) temporal_begin *= 10000ull;
        if (temporal_end > 0ull && temporal_end < 1000000ull) temporal_end *= 10000ull;
    }

    /* Contrato puntual V1: los llamadores de semana/hora/minuto entregan
       begin == end para consultar una llave exacta. El filtro interno usa
       intervalo semiabierto [begin,end), por eso una llave puntual debe
       ensancharse un solo tick sintactico sin tocar el valor temporal real.
       Sin esta normalizacion, YYYYWW0000 consultaba un intervalo vacio y la
       capa epidemiologica parecia no existir aunque el IXIPTLAH tuviera datos. */
    if (temporal_begin != 0ull && temporal_begin == temporal_end) {
        if (temporal_end != std::numeric_limits<std::uint64_t>::max()) ++temporal_end;
    }

    const bool filter_time = temporal_begin != 0ull || temporal_end != 0ull;
    const auto time_ok = [&](std::uint64_t key) -> bool {
        if (!filter_time || key == 0ull) return true;
        if (temporal_begin != 0ull && key < temporal_begin) return false;
        if (temporal_end != 0ull && key >= temporal_end) return false;
        return true;
    };

    const std::uint64_t first_temporal_bucket = ixiptlah_fast_narrow_bucket_from_temporal_key(type, temporal_begin);
    const std::uint64_t last_temporal_bucket = temporal_end > 0ull ? ixiptlah_fast_narrow_bucket_from_temporal_key(type, temporal_end - 1ull) : first_temporal_bucket;
    const bool small_temporal_bucket_window = filter_time && first_temporal_bucket != 0ull &&
        last_temporal_bucket >= first_temporal_bucket &&
        last_temporal_bucket - first_temporal_bucket <= env_u64_ix("TLALPOWA_IXIPTLAH_EXACT_TEMPORAL_BUCKET_SPAN", 100ull, 1ull, 3660ull);
    const std::uint64_t first_month_bucket = ixiptlah_fast_wide_bucket_from_temporal_key(type, temporal_begin);
    const std::uint64_t last_month_bucket = temporal_end > 0ull ? ixiptlah_fast_wide_bucket_from_temporal_key(type, temporal_end - 1ull) : first_month_bucket;
    const bool small_month_bucket_window = filter_time && first_month_bucket != 0ull &&
        last_month_bucket >= first_month_bucket &&
        last_month_bucket - first_month_bucket <= env_u64_ix("TLALPOWA_IXIPTLAH_EXACT_MONTH_BUCKET_SPAN", 120ull, 1ull, 2400ull);

    std::vector<std::uint64_t> query_layers;
    query_layers.reserve(layer_hashes.size() + (include_zero_layer && !layer_hashes.empty() ? 1u : 0u));
    if (include_zero_layer && !layer_hashes.empty()) query_layers.push_back(0ull);
    for (std::uint64_t h : layer_hashes) {
        if (h == 0ull) { if (!include_zero_layer) query_layers.push_back(0ull); continue; }
        query_layers.push_back(h);
    }
    if (!query_layers.empty()) {
        std::sort(query_layers.begin(), query_layers.end());
        query_layers.erase(std::unique(query_layers.begin(), query_layers.end()), query_layers.end());
    }

    const std::uint64_t first_hour_bucket = ixiptlah_fast_hour_bucket_from_temporal_key(type, temporal_begin);
    const std::uint64_t last_hour_bucket = temporal_end > 0ull ? ixiptlah_fast_hour_bucket_from_temporal_key(type, temporal_end - 1ull) : first_hour_bucket;
    const bool same_day_hour_window = filter_time && first_hour_bucket != 0ull &&
        last_hour_bucket >= first_hour_bucket &&
        ixiptlah_fast_day_bucket_from_temporal_key(temporal_begin) == ixiptlah_fast_day_bucket_from_temporal_key(temporal_end > 0ull ? temporal_end - 1ull : temporal_begin) &&
        last_hour_bucket - first_hour_bucket <= env_u64_ix("TLALPOWA_IXIPTLAH_EXACT_HOUR_BUCKET_SPAN", 24ull, 1ull, 48ull);

    const std::uint64_t first_week_bucket = ixiptlah_fast_week_bucket_from_temporal_key(type, temporal_begin);
    const std::uint64_t last_week_bucket = temporal_end > 0ull ? ixiptlah_fast_week_bucket_from_temporal_key(type, temporal_end - 1ull) : first_week_bucket;
    const bool single_week_bucket_window = filter_time && first_week_bucket != 0ull && first_week_bucket == last_week_bucket;

    const auto visit_temporal_bucket_map = [&](const std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>& buckets,
                                               std::uint64_t first_bucket,
                                               std::uint64_t last_bucket,
                                               bool contiguous_range,
                                               bool exact_temporal_address) -> bool {
        if (buckets.empty() || first_bucket == 0ull || last_bucket == 0ull || last_bucket < first_bucket) return false;
        bool probed_address = false;
        bool visited_any = false;
        bool stop = false;
        std::unordered_set<std::uint64_t> visited_query_layers;
        if (!query_layers.empty()) visited_query_layers.reserve(query_layers.size() + 1u);
        const auto visit_bucket = [&](std::uint64_t layer_key_for_bucket, std::uint64_t bucket) {
            if (stop) return;
            probed_address = true;
            const std::uint64_t bucket_key = ixiptlah_fast_time_bucket_key(type, schema, layer_key_for_bucket, bucket);
            auto it = buckets.find(bucket_key);
            if (it == buckets.end()) return;
            bool delivered_for_layer = false;
            for (std::uint32_t idx : it->second) {
                if (idx >= index->records.size()) continue;
                const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
                if (rec.type != type || rec.schema != schema) continue;
                if (!query_layers.empty() && !std::binary_search(query_layers.begin(), query_layers.end(), rec.layer_hash)) continue;
                if (!time_ok(rec.temporal_key)) continue;
                visited_any = true;
                delivered_for_layer = true;
                bool keep_going = true;
                if (dispatch_exact_payload(rec, keep_going) && !keep_going) { stop = true; break; }
            }
            if (delivered_for_layer && !query_layers.empty() && layer_key_for_bucket != std::numeric_limits<std::uint64_t>::max()) {
                visited_query_layers.insert(layer_key_for_bucket);
            }
        };

        const auto visit_bucket_span_for_layer = [&](std::uint64_t layer_key_for_bucket) {
            if (contiguous_range) {
                for (std::uint64_t b = first_bucket; b <= last_bucket && !stop; ++b) {
                    visit_bucket(layer_key_for_bucket, b);
                    if (b == std::numeric_limits<std::uint64_t>::max()) break;
                }
            } else {
                visit_bucket(layer_key_for_bucket, first_bucket);
            }
        };

        if (query_layers.empty()) {
            visit_bucket_span_for_layer(std::numeric_limits<std::uint64_t>::max());
        } else {
            for (std::uint64_t h : query_layers) {
                visit_bucket_span_for_layer(h);
                if (stop) break;
            }
        }
        if (stop) return true;
        if (exact_temporal_address && probed_address) {
            // Un bucket horario/semanal puntual es una dirección cerrada: si el
            // índice no contiene cierta capa, la respuesta correcta es vacío, no
            // barrer el archivo entero. Esto evita latencias de consulta puntual.
            return true;
        }
        bool covered = visited_any;
        if (covered && !query_layers.empty()) {
            for (std::uint64_t h : query_layers) {
                if (h != 0ull && visited_query_layers.find(h) == visited_query_layers.end()) {
                    covered = false;
                    break;
                }
            }
        }
        return covered;
    };

    // Ruta más estrecha: una consulta puntual/hora dentro del mismo día no debe
    // abrir todos los registros diarios de esa capa. Si el bucket horario no
    // cubre todo, se cae a día/semana/capa sin perder registros.
    if (same_day_hour_window && visit_temporal_bucket_map(index->fast_hour_buckets, first_hour_bucket, last_hour_bucket, true, true)) return;

    // Archivo semanal unificado: para RAMA/REDMET/RUOA/epidemiología el caso
    // normal es que todo el intervalo caiga en una sola semana física. Este
    // bucket salta directo a esos offsets y evita barridos de día o capa.
    if (single_week_bucket_window && visit_temporal_bucket_map(index->fast_week_buckets, first_week_bucket, last_week_bucket, false, true)) return;

    if (small_temporal_bucket_window && !index->fast_time_buckets.empty()) {
        bool visited_any = false;
        bool stop = false;
        std::unordered_set<std::uint64_t> visited_query_layers;
        if (!query_layers.empty()) visited_query_layers.reserve(query_layers.size() + 1u);
        const auto visit_time_bucket = [&](std::uint64_t layer_key_for_bucket, std::uint64_t temporal_bucket) {
            if (stop) return;
            const std::uint64_t bucket_key = ixiptlah_fast_time_bucket_key(type, schema, layer_key_for_bucket, temporal_bucket);
            auto it = index->fast_time_buckets.find(bucket_key);
            if (it == index->fast_time_buckets.end()) return;
            bool delivered_for_layer = false;
            for (std::uint32_t idx : it->second) {
                if (idx >= index->records.size()) continue;
                const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
                if (rec.type != type || rec.schema != schema) continue;
                if (!query_layers.empty() && !std::binary_search(query_layers.begin(), query_layers.end(), rec.layer_hash)) continue;
                if (!time_ok(rec.temporal_key)) continue;
                visited_any = true;
                delivered_for_layer = true;
                bool keep_going = true;
                if (dispatch_exact_payload(rec, keep_going) && !keep_going) { stop = true; break; }
            }
            if (delivered_for_layer && !query_layers.empty() && layer_key_for_bucket != std::numeric_limits<std::uint64_t>::max()) {
                visited_query_layers.insert(layer_key_for_bucket);
            }
        };

        if (query_layers.empty()) {
            for (std::uint64_t b = first_temporal_bucket; b <= last_temporal_bucket && !stop; ++b) {
                visit_time_bucket(std::numeric_limits<std::uint64_t>::max(), b);
                if (b == std::numeric_limits<std::uint64_t>::max()) break;
            }
        } else {
            for (std::uint64_t h : query_layers) {
                for (std::uint64_t b = first_temporal_bucket; b <= last_temporal_bucket && !stop; ++b) {
                    visit_time_bucket(h, b);
                    if (b == std::numeric_limits<std::uint64_t>::max()) break;
                }
                if (stop) break;
            }
        }
        if (stop) return;
        bool temporal_bucket_covered = visited_any;
        if (temporal_bucket_covered && !query_layers.empty()) {
            for (std::uint64_t h : query_layers) {
                if (h != 0ull && visited_query_layers.find(h) == visited_query_layers.end()) {
                    temporal_bucket_covered = false;
                    break;
                }
            }
        }
        if (temporal_bucket_covered) return;
    }

    if (small_month_bucket_window && !index->fast_month_buckets.empty()) {
        bool visited_any = false;
        bool stop = false;
        std::unordered_set<std::uint64_t> visited_query_layers;
        if (!query_layers.empty()) visited_query_layers.reserve(query_layers.size() + 1u);
        const auto visit_month_bucket = [&](std::uint64_t layer_key_for_bucket, std::uint64_t month_bucket) {
            if (stop) return;
            const std::uint64_t bucket_key = ixiptlah_fast_time_bucket_key(type, schema, layer_key_for_bucket, month_bucket);
            auto it = index->fast_month_buckets.find(bucket_key);
            if (it == index->fast_month_buckets.end()) return;
            bool delivered_for_layer = false;
            for (std::uint32_t idx : it->second) {
                if (idx >= index->records.size()) continue;
                const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
                if (rec.type != type || rec.schema != schema) continue;
                if (!query_layers.empty() && !std::binary_search(query_layers.begin(), query_layers.end(), rec.layer_hash)) continue;
                if (!time_ok(rec.temporal_key)) continue;
                visited_any = true;
                delivered_for_layer = true;
                bool keep_going = true;
                if (dispatch_exact_payload(rec, keep_going) && !keep_going) { stop = true; break; }
            }
            if (delivered_for_layer && !query_layers.empty() && layer_key_for_bucket != std::numeric_limits<std::uint64_t>::max()) {
                visited_query_layers.insert(layer_key_for_bucket);
            }
        };

        if (query_layers.empty()) {
            for (std::uint64_t b = first_month_bucket; b <= last_month_bucket && !stop; ++b) {
                visit_month_bucket(std::numeric_limits<std::uint64_t>::max(), b);
                if (b == std::numeric_limits<std::uint64_t>::max()) break;
            }
        } else {
            for (std::uint64_t h : query_layers) {
                for (std::uint64_t b = first_month_bucket; b <= last_month_bucket && !stop; ++b) {
                    visit_month_bucket(h, b);
                    if (b == std::numeric_limits<std::uint64_t>::max()) break;
                }
                if (stop) break;
            }
        }
        if (stop) return;
        bool month_bucket_covered = visited_any;
        if (month_bucket_covered && !query_layers.empty()) {
            for (std::uint64_t h : query_layers) {
                if (h != 0ull && visited_query_layers.find(h) == visited_query_layers.end()) {
                    month_bucket_covered = false;
                    break;
                }
            }
        }
        if (month_bucket_covered) return;
    }

    if (!query_layers.empty() && !index->fast_buckets.empty()) {
        // Ruta exacta caliente: los buckets exactos type+schema+layer no se
        // solapan entre sí. Por tanto no se copia, ordena ni uniquifica el
        // conjunto salvo que se fuerce el modo estable por diagnóstico. En mapas
        // y semanas la agregación es conmutativa; el coste queda en enteros + seek.
        const bool force_stable_merge = env_truthy_ix("TLALPOWA_IXIPTLAH_EXACT_STABLE_MERGE");
        if (!force_stable_merge) {
            bool visited_any = false;
            bool stop = false;
            for (std::uint64_t h : query_layers) {
                const std::uint64_t bucket_key = ixiptlah_fast_bucket_key(type, schema, h);
                auto it = index->fast_buckets.find(bucket_key);
                if (it == index->fast_buckets.end()) continue;
                visited_any = true;
                for (std::uint32_t idx : it->second) {
                    if (idx >= index->records.size()) continue;
                    const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
                    if (!time_ok(rec.temporal_key)) continue;
                    bool keep_going = true;
                    if (dispatch_exact_payload(rec, keep_going) && !keep_going) { stop = true; break; }
                }
                if (stop) break;
            }
            if (visited_any) return;
        }

        std::vector<std::uint32_t> physical_indices;
        std::size_t reserve_n = 0;
        for (std::uint64_t h : query_layers) {
            const std::uint64_t bucket_key = ixiptlah_fast_bucket_key(type, schema, h);
            auto it = index->fast_buckets.find(bucket_key);
            if (it != index->fast_buckets.end()) reserve_n += it->second.size();
        }
        physical_indices.reserve(reserve_n);
        for (std::uint64_t h : query_layers) {
            const std::uint64_t bucket_key = ixiptlah_fast_bucket_key(type, schema, h);
            auto it = index->fast_buckets.find(bucket_key);
            if (it == index->fast_buckets.end()) continue;
            for (std::uint32_t idx : it->second) physical_indices.push_back(idx);
        }
        if (physical_indices.empty()) return;
        std::sort(physical_indices.begin(), physical_indices.end());
        physical_indices.erase(std::unique(physical_indices.begin(), physical_indices.end()), physical_indices.end());

        for (std::uint32_t idx : physical_indices) {
            if (idx >= index->records.size()) continue;
            const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
            if (!time_ok(rec.temporal_key)) continue;
            bool keep_going = true;
            if (dispatch_exact_payload(rec, keep_going) && !keep_going) break;
        }
        return;
    }

    if (query_layers.empty() && !index->fast_buckets.empty()) {
        const std::uint64_t type_schema_key = ixiptlah_fast_bucket_key(type, schema, std::numeric_limits<std::uint64_t>::max());
        auto it = index->fast_buckets.find(type_schema_key);
        if (it == index->fast_buckets.end()) return;
        for (std::uint32_t idx : it->second) {
            if (idx >= index->records.size()) continue;
            const IxiptlahIndexedRecord& rec = index->records[static_cast<std::size_t>(idx)];
            if (!time_ok(rec.temporal_key)) continue;
            bool keep_going = true;
            if (dispatch_exact_payload(rec, keep_going) && !keep_going) break;
        }
        return;
    }

    // Último resguardo para índices heredados sin buckets residentes. El filtro
    // de tiempo sigue siendo puro encabezado; ningún payload ajeno se toca.
    for (const IxiptlahIndexedRecord& rec : index->records) {
        if (rec.type != type || rec.schema != schema) continue;
        if (!time_ok(rec.temporal_key)) continue;
        bool keep_going = true;
        if (dispatch_exact_payload(rec, keep_going) && !keep_going) break;
    }
}


IxiptlahRewriteStats ixiptlah_rewrite_without_records(

    const fs::path& path,



    const std::function<bool(IxiptlahRecordType, std::uint32_t)>& drop_record) {


    IxiptlahRewriteStats stats;


    const IxiptlahFileIndex index = ixiptlah_index_for_path(path);


    if (!index.valid) return stats;


    auto in = ixiptlah_open_binary_input(path);

    if (!in) return stats;


    const fs::path tmp = fs::path(path.wstring() + L".rewrite");
    std::error_code ec;
    fs::remove(tmp, ec);
    {


        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);


        if (!out || !ixiptlah_write_file_header(out, kIxiptlahFileVersion)) return stats;


        for (const IxiptlahIndexedRecord& rec : index.records) {

            bool remove = false;


            try { remove = drop_record && drop_record(rec.type, rec.schema); } catch (...) { remove = false; }

            if (remove) {
                ++stats.removed;
                continue;
            }

            if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) {

                ++stats.unreadable;
                continue;
            }

            in.clear();


            in.seekg(static_cast<std::streamoff>(rec.payload_offset), std::ios::beg);

            if (!in || !ixiptlah_write_indexed_record_raw_stream(in, out, rec)) {

                ++stats.unreadable;
                continue;
            }

            ++stats.kept;
        }

        if (!out) return stats;
    }


    in.close();

    {


        std::lock_guard<std::mutex> lock(ixiptlah_mu());


        auto& sinks = ixiptlah_sinks();

        const std::string key = path_utf8(path);

        if (auto it = sinks.find(key); it != sinks.end()) {

            if (it->second && it->second->stream) {


                it->second->stream.flush();
                it->second->stream.close();
            }

            sinks.erase(it);
        }
    }


    fs::rename(tmp, path, ec);

    if (ec) {



        ec.clear();


        fs::copy_file(tmp, path, fs::copy_options::overwrite_existing, ec);

        if (!ec) {

            std::error_code cleanup_ec;
            fs::remove(tmp, cleanup_ec);
        } else {

            ec.clear();

            fs::remove(path, ec);

            if (!ec) {

                ec.clear();


                fs::rename(tmp, path, ec);
            }

            if (ec) {
                fs::remove(tmp, ec);

                return stats;
            }
        }
    }

    stats.rewritten = true;


    ixiptlah_invalidate_index_cache(path);

    return stats;
}



IxiptlahCopyStats ixiptlah_append_selected_records_raw(

    const fs::path& dst,

    const fs::path& src,



    const std::function<bool(IxiptlahRecordType, std::uint32_t)>& accept_record) {


    IxiptlahCopyStats stats;

    if (dst.empty() || src.empty()) return stats;


    const IxiptlahFileIndex index = ixiptlah_index_for_path(src);

    if (!index.valid) return stats;


    auto in = ixiptlah_open_binary_input(src);

    if (!in) return stats;



    for (const IxiptlahIndexedRecord& rec : index.records) {
        bool accepted = true;


        try { accepted = !accept_record || accept_record(rec.type, rec.schema); } catch (...) { accepted = false; }

        if (!accepted) continue;

        if (rec.stored_size > kMaxPayloadBytes || rec.raw_size > kMaxPayloadBytes) {


            ++stats.unreadable;
            continue;
        }

        in.clear();
        in.seekg(static_cast<std::streamoff>(rec.payload_offset), std::ios::beg);

        if (!in) {

            ++stats.unreadable;
            continue;
        }
        {


            std::lock_guard<std::mutex> lock(ixiptlah_mu());


            IxiptlahSink* sink = ixiptlah_sink_for_path_locked(dst);


            if (!sink || !sink->stream || sink->version != kIxiptlahFileVersion ||


                !ixiptlah_write_indexed_record_raw_stream(in, sink->stream, rec)) {

                ++stats.unreadable;

                continue;
            }

            sink->stream.flush();

            if (!sink->stream) {

                ++stats.unreadable;
                continue;
            }
        }
        ++stats.copied;
        stats.target_touched = true;
    }


    if (stats.target_touched) ixiptlah_invalidate_index_cache(dst);

    return stats;
}




void ixiptlah_flush_all() {


    std::lock_guard<std::mutex> lock(ixiptlah_mu());


    for (auto& [_, sink] : ixiptlah_sinks()) {

        if (sink && sink->stream) {

            sink->stream.flush();
        }
    }
}




void ixiptlah_close_all() {


    std::vector<std::pair<fs::path, std::vector<IxiptlahIndexedRecord>>> closed_paths;
    {
        std::lock_guard<std::mutex> lock(ixiptlah_mu());


        for (auto& [_, sink] : ixiptlah_sinks()) {

            if (sink && sink->stream) {

                sink->stream.flush();
                sink->stream.close();
                if (!sink->path.empty()) closed_paths.emplace_back(sink->path, sink->live_records);
            }
        }


        ixiptlah_sinks().clear();
    }

    for (const auto& closed : closed_paths) {
        if (!closed.second.empty()) {
            (void)ixiptlah_embed_terminal_directory_from_records(closed.first, closed.second);
        } else {
            (void)ixiptlah_embed_terminal_directory(closed.first);
        }
    }
}

}

// ===== importador.c =====
#line 1 "importador.c"
/* Núcleo visible de importación: descarga, detección y materialización de fuentes externas.
   La implementación C caliente RUOA/PEMBU/RAMA/REDMA vive ahora dentro del núcleo importador. */

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
