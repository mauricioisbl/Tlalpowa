#pragma once
// TLALPOWA: cabecera interna central.
// Contrato C++ interno de una sola cabecera; la ABI visible permanece fuera de ImGui/GLFW.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cmath>
#include <csignal>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <exception>
#include <future>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <system_error>
#include <thread>
#include <tuple>
#include <nlohmann/json.hpp>


// ===== EpiTypes.hpp =====
namespace epi {



namespace fs = std::filesystem;




struct Rect {

    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;


    [[nodiscard]] double width() const noexcept { return x1 - x0; }

    [[nodiscard]] double height() const noexcept { return y1 - y0; }

    [[nodiscard]] double cx() const noexcept { return (x0 + x1) * 0.5; }

    [[nodiscard]] double cy() const noexcept { return (y0 + y1) * 0.5; }

    [[nodiscard]] bool valid() const noexcept { return x1 >= x0 && y1 >= y0; }


    void include(const Rect& r) noexcept {

        if (!valid() || (x0 == 0.0 && x1 == 0.0 && y0 == 0.0 && y1 == 0.0)) {
            *this = r;

            return;
        }

        x0 = std::min(x0, r.x0);

        y0 = std::min(y0, r.y0);



        x1 = std::max(x1, r.x1);

        y1 = std::max(y1, r.y1);
    }
};




struct Token {
    int page = 0;
    std::string text;
    std::string norm;
    Rect box;
};




struct PageText {
    int page = 0;
    double width = 0.0;
    double height = 0.0;

    std::vector<Token> tokens;
};





struct PdfDocument {


    fs::path pdf_path;


    std::string file_name;
    std::string stable_id;

    int bulletin_year = 0;

    int bulletin_week = 0;
    int source_page_count = 0;
    int first_extracted_page = 1;
    int last_extracted_page = 0;

    std::vector<PageText> pages;
};




struct Jurisdiction {
    std::string id;
    std::string canonical;

    std::vector<std::string> aliases;

    std::vector<std::string> aliases_norm;
};




struct Disease {

    std::string id;
    std::string canonical;

    std::string group;

    std::vector<std::string> cie10;

    std::vector<std::string> cie10_norm;

    std::vector<std::string> aliases;

    std::vector<std::string> aliases_norm;
};




struct RowBand {
    std::string jurisdiction_id;
    std::string jurisdiction;

    Rect label_box;
    double y_mid = 0.0;
    double y0 = 0.0;
    double y1 = 0.0;

    int line_index = -1;
};




struct ColumnBand {
    int index = -1;
    double x_mid = 0.0;

    double x0 = 0.0;
    double x1 = 0.0;
    std::string period = "unknown";
    std::string sex = "total";
    std::string disease_id = "unknown";
    std::string disease = "unknown";
    std::string cie10 = "";

    std::string source_year = "";
    std::string header_text = "";

    std::string role = "";

    std::string expected_role = "";
    std::string group_layout_note = "";
    int group_index = -1;
    Rect header_box;
    Rect disease_box;
    Rect cie10_box;
    double header_confidence = 0.0;
};





struct ParsedValue {
    std::string raw;
    std::optional<int64_t> value;
    Rect box;
};




struct Observation {


    std::string pdf_file;
    std::string pdf_id;
    int page = 0;

    int bulletin_year = 0;

    int bulletin_week = 0;
    std::string table_id;
    std::string table_title;
    std::string disease_id;
    std::string disease;
    std::string cie10;
    std::string jurisdiction_id;
    std::string jurisdiction;

    std::string source_year;
    std::string period;
    std::string sex;
    std::string raw_value;

    int64_t value = 0;
    double confidence = 0.0;
    Rect cell_box;
    std::string validation_rule;
};




struct QuarantineItem {


    std::string pdf_file;
    std::string pdf_id;
    int page = 0;
    std::string table_id;

    std::string column_key;
    std::string reason;
    std::string detail;
};




struct TableCandidate {
    std::string table_id;

    std::string table_title;
    int page = 0;
    Rect page_box;

    std::vector<RowBand> rows;

    std::vector<ColumnBand> columns;


    std::map<std::pair<std::string, int>, ParsedValue> cells;

    std::set<std::pair<std::string, int>> duplicate_cells;

    std::vector<Observation> accepted;

    std::vector<QuarantineItem> quarantine;
};




struct PipelineStats {

    int pdf_total = 0;
    int pdf_done = 0;
    int pages_total = 0;
    int pages_done = 0;
    int detail_total = 0;

    int detail_done = 0;

    double progress = 0.0;
    int pages_with_tables = 0;
    int tables_detected = 0;
    int64_t observations_accepted = 0;
    int64_t quarantine_items = 0;

    std::string current_pdf;
    int current_page = 0;
    std::string status = "listo";
};




struct ExtractionPreview {


    std::string pdf_file;

    fs::path page_image;

    int pdf_index = 0;
    int pdf_total = 0;
    int page = 0;
    double page_width = 0.0;
    double page_height = 0.0;

    std::vector<Token> tokens;

    std::vector<RowBand> rows;

    std::vector<ColumnBand> columns;

    std::vector<Observation> accepted;

    std::vector<QuarantineItem> quarantine;
    std::string status;
};




struct AppOptions {

    fs::path input_dir;




    std::vector<fs::path> explicit_pdfs;

    fs::path output_dir;


    fs::path config_dir;

    fs::path log_dir;

    fs::path runtime_dir;


    fs::path pdftotext;


    fs::path pdftoppm;

    fs::path tesseract;


    fs::path geojson;

    bool dashboard = true;
    bool render_pages = true;
    bool stop_on_error = false;
    bool resume = true;

    int limit_pdfs = 0;
    int skip_front_pages = 0;
    int skip_back_pages = 0;

    int max_pages_per_pdf = 0;
    int render_dpi = 32;



    std::function<void(const PipelineStats&)> progress_callback;

    std::function<void(const ExtractionPreview&)> preview_callback;

    std::function<bool()> pause_requested;


    std::function<bool()> cancel_requested;
};




[[nodiscard]] std::string rect_to_json(const Rect& r);


[[nodiscard]] std::string rect_to_csv(const Rect& r);

}

// ===== TextUtils.hpp =====
namespace epi {



[[nodiscard]] std::string trim(std::string s);


[[nodiscard]] std::string lower_ascii(std::string s);

[[nodiscard]] std::string clean_user_path_string(std::string s);

[[nodiscard]] fs::path clean_user_path(const fs::path& p);

[[nodiscard]] fs::path resolve_existing_path_relaxed(const fs::path& p);

[[nodiscard]] std::string strip_accents_utf8(std::string s);


[[nodiscard]] std::string normalize_key(std::string s);

[[nodiscard]] std::string html_unescape(std::string s);


[[nodiscard]] std::string json_escape(const std::string& s);


[[nodiscard]] std::string csv_escape(const std::string& s);


[[nodiscard]] bool contains_norm(const std::string& haystack_norm, const std::string& needle_norm);

[[nodiscard]] bool is_numeric_token(const std::string& s);


[[nodiscard]] std::optional<int64_t> parse_epi_int(const std::string& s);

[[nodiscard]] std::string safe_filename(std::string s);

[[nodiscard]] std::string simple_hash_hex(const std::string& s);

[[nodiscard]] std::string read_text_file(const fs::path& p);

void write_text_file(const fs::path& p, const std::string& content);


void ensure_dir(const fs::path& p);



[[nodiscard]] uintmax_t file_size_or_zero(const fs::path& p);


bool copy_file_overwrite(const fs::path& source, const fs::path& destination, std::error_code& ec);



[[nodiscard]] std::vector<fs::path> list_pdfs_recursive(const fs::path& root);

[[nodiscard]] std::string now_utc_iso();

[[nodiscard]] std::wstring widen_utf8(const std::string& s);

[[nodiscard]] std::string narrow_utf8(const std::wstring& s);


[[nodiscard]] std::string path_utf8(const fs::path& p);

[[nodiscard]] fs::path executable_dir();

[[nodiscard]] fs::path project_root();

[[nodiscard]] fs::path config_root();

[[nodiscard]] fs::path internal_data_root();


[[nodiscard]] fs::path external_data_root();



[[nodiscard]] std::string getenv_utf8_or_empty(const char* name);

[[nodiscard]] fs::path getenv_path_utf8(const char* name);

[[nodiscard]] std::wstring getenv_wstring_or_empty(const wchar_t* name);



class Logger {
public:


    explicit Logger(fs::path log_file);

    void info(const std::string& m);

    void warn(const std::string& m);


    void error(const std::string& m);
private:


    std::mutex mu_;


    std::ofstream out_;

    void line(const std::string& level, const std::string& m);
};

}

// ===== Config.hpp =====
namespace epi {



class Config {
public:


    void load(const fs::path& config_dir);


    [[nodiscard]] const std::vector<Jurisdiction>& jurisdictions() const noexcept { return jurisdictions_; }

    [[nodiscard]] const std::vector<Disease>& diseases() const noexcept { return diseases_; }


    [[nodiscard]] std::optional<Jurisdiction> match_jurisdiction_line(const std::string& normalized_line) const;


    [[nodiscard]] std::optional<Disease> match_disease_text(const std::string& normalized_text) const;

    [[nodiscard]] std::optional<Disease> match_disease_cie10(const std::string& cie10_text) const;


    [[nodiscard]] std::string default_table_title() const { return "tabla_epidemiologica"; }
private:


    std::vector<Jurisdiction> jurisdictions_;

    std::vector<Disease> diseases_;

    void load_builtin();


    void load_jurisdictions_tsv(const fs::path& p);


    void load_diseases_tsv(const fs::path& p);



    void normalize_catalog_entries();
};

}

// ===== ExternalTools.hpp =====
namespace epi {





struct ProcessResult {
    int exit_code = -1;
    std::string command_for_log;
    std::string captured_output;
};





class ExternalTools {
public:


    explicit ExternalTools(AppOptions options) : options_(std::move(options)) {}


    [[nodiscard]] const AppOptions& options() const noexcept { return options_; }


    void validate() const;



    [[nodiscard]] int pdf_page_count(const fs::path& pdf) const;


    ProcessResult run_pdftotext_bbox(const fs::path& pdf, const fs::path& out_html, int first_page = 1, int last_page = 0) const;





    ProcessResult run_pdftotext_layout(const fs::path& pdf, const fs::path& out_text, int first_page = 1, int last_page = 0) const;





    ProcessResult run_pdftoppm_page_png(const fs::path& pdf, int page, const fs::path& out_prefix_no_ext) const;





    ProcessResult run_pdftoppm_pages_png(const fs::path& pdf, int first_page, int last_page, const fs::path& out_prefix_no_ext) const;
private:
    AppOptions options_;


    [[nodiscard]] fs::path resolve_tool_path(const fs::path& configured, const std::string& exe_name) const;


    [[nodiscard]] ProcessResult run_command(const std::vector<std::string>& args) const;


    [[nodiscard]] ProcessResult run_command_timed(const std::vector<std::string>& args, int timeout_ms) const;

    [[nodiscard]] static std::string quote_for_log(const std::string& s);


    [[nodiscard]] fs::path external_work_root() const;


    [[nodiscard]] fs::path stage_pdf_ascii(const fs::path& pdf, const std::string& purpose) const;
};

}

// ===== ImportRuoa.hpp =====
namespace ImportRuoa {

struct RuoaCredentials {
    std::string usuario;
    std::string password;
    // Nombre y correo que el formulario PEMBU envia realmente al endpoint.
    // Si estan vacios se infieren de usuario/password sin persistirlos.
    std::string nombre_publico;
    std::string correo;
};

struct RuoaDownloadOptions {
    std::filesystem::path destino_raiz;
    int anio_mas_antiguo = 1997;
    int anio_mas_reciente = 2026;
    int pausa_ms_entre_csv = 1000;
    int intentos_por_csv = 3;
    std::uintmax_t csv_min_bytes = 64;
    bool conservar_csv_valido = true;
    bool estrictamente_reciente_a_antiguo = true;
    // En modo transversal se itera por ronda temporal: año reciente -> antiguo,
    // mes diciembre -> enero y, dentro de cada mes, todas las estaciones.
    bool transversal_por_mes = true;
    std::atomic_bool* cancelar = nullptr;
};

struct RuoaStationProgress {
    std::string estacion;
    std::string etiqueta;
    int completados = 0;
    int total = 0;
    int descargados = 0;
    int omitidos_validos = 0;
    int fallidos = 0;
};

struct RuoaProgress {
    int completados = 0;
    int total = 0;
    int descargados = 0;
    int utilizables = 0;
    int omitidos_validos = 0;
    int fallidos = 0;
    int anio = 0;
    int mes = 0;
    std::string estacion;
    std::string etiqueta;
    std::filesystem::path destino;
    std::string fase;
    std::vector<RuoaStationProgress> estaciones;
};

using RuoaProgressCallback = std::function<void(const RuoaProgress&)>;

bool descargar_csvs_pembu(const RuoaCredentials& credenciales,
                          const RuoaDownloadOptions& opciones,
                          nlohmann::json& auditoria,
                          RuoaProgressCallback progreso = {});

const std::vector<std::string>& estaciones_pembu();

}  // namespace ImportRuoa

// ===== Ixiptlah.hpp =====
namespace epi {



constexpr const char* kIxiptlahExtension = ".ixiptlah";




enum class IxiptlahRecordType : std::uint32_t {
    EpidemiologyObservation = 1,
    EpidemiologyQuarantine = 2,
    AtmosphereMeasurement = 10,
    AtmosphereTerritoryAverage = 11,
    // IXIPTLAH V1 sin retrocompatibilidad mensual: muestras directas y, cuando
    // una vista lo exige, registros preformados compactos para gráfica inmediata.
    AtmosphereRenderSummary = 12,
    AtmosphereGraphLayerCatalog = 13,
    AtmosphereGraphDailyStationBatch = 14,
    AtmosphereGraphHourlyStationBatch = 15,
    AtmosphereGraphWeeklyStationBatch = 16,

    ProcessedPdf = 20,


    ProcessedPage = 21,
    RunState = 30,
    LivePreview = 31,




    MonthlyDictionary = 100,


    MonthlyEpidemiologyBatch = 101,


    MonthlyAtmosphereRenderBatch = 102,


    MonthlyAtmosphereMeasurementBatch = 103,


    MonthlyAtmosphereTerritoryBatch = 104,




    MonthlySourceInventory = 105,

    // Snapshot IXIPTLAH preformado para representación epidemiológica: catálogo,
    // semanas y filas normalizadas listas para dibujo. No sustituye las muestras
    // EPI2 primarias; sólo evita rehidratar 136 núcleos en cada clic de UI.
    EpidemiologyRenderSnapshot = 106
};




fs::path ixiptlah_path(const fs::path& root, const std::string& stem);



template <typename T>



bool ixiptlah_write_value(std::ostream& out, const T& value) {


    out.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));

    return static_cast<bool>(out);
}


template <typename T>



bool ixiptlah_read_value(std::istream& in, T& value) {


    in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));

    return static_cast<bool>(in);
}




bool ixiptlah_write_string(std::ostream& out, const std::string& value);



bool ixiptlah_read_string(std::istream& in, std::string& value);



bool ixiptlah_append_record(const fs::path& path,


                           IxiptlahRecordType type,


                           std::uint32_t schema_version,


                           const std::function<bool(std::ostream&)>& write_payload);

// Variante interna de alta frecuencia: anexa el registro sin forzar flush físico
// inmediato. El siguiente registro normal o ixiptlah_flush_all() publicará el
// bloque completo. Se usa para EPI2 seguido por su delta de representación en el
// mismo archivo, reduciendo fsync/flush doble sin perder escritura append-only.
bool ixiptlah_append_record_deferred_flush(const fs::path& path,


                           IxiptlahRecordType type,


                           std::uint32_t schema_version,


                           const std::function<bool(std::ostream&)>& write_payload);



// Huella semántica estable de capa IXIPTLAH-SM. No guarda punteros ni cadenas
// residentes: sólo un entero FNV-1a que permite saltar registros no seleccionados
// desde el índice lateral sin abrir payloads ni calcular catálogos en runtime.
std::uint64_t ixiptlah_layer_hash(const std::string& layer_key);

bool ixiptlah_append_record_tagged(const fs::path& path,

                                  IxiptlahRecordType type,

                                  std::uint32_t schema_version,

                                  std::uint64_t layer_hash,

                                  const std::function<bool(std::ostream&)>& write_payload);

bool ixiptlah_append_record_tagged(const fs::path& path,

                                  IxiptlahRecordType type,

                                  std::uint32_t schema_version,

                                  const std::string& layer_key,

                                  const std::function<bool(std::ostream&)>& write_payload);

// IXIPTLAH V1 fija la identidad de capa y la llave temporal directamente en el
// encabezado binario del registro. El lector puede descartar millones de filas no
// seleccionadas sin abrir el payload ni depender de sidecars, mmaps no portables o
// rutas retrocompatibles.
bool ixiptlah_append_record_tagged_temporal(const fs::path& path,

                                           IxiptlahRecordType type,

                                           std::uint32_t schema_version,

                                           std::uint64_t layer_hash,

                                           std::uint64_t temporal_key,

                                           const std::function<bool(std::ostream&)>& write_payload);

bool ixiptlah_append_record_tagged_temporal(const fs::path& path,

                                           IxiptlahRecordType type,

                                           std::uint32_t schema_version,

                                           const std::string& layer_key,

                                           std::uint64_t temporal_key,

                                           const std::function<bool(std::ostream&)>& write_payload);

// Variante de captura masiva: conserva el registro append-only, pero no fuerza
// flush por registro. La importación RAMA/REDMA/RUOA llama a flush periódico o
// cierre explícito, evitando pagar latencia de disco dos veces por capa/mes.
bool ixiptlah_append_record_tagged_temporal_deferred_flush(const fs::path& path,

                                           IxiptlahRecordType type,

                                           std::uint32_t schema_version,

                                           std::uint64_t layer_hash,

                                           std::uint64_t temporal_key,

                                           const std::function<bool(std::ostream&)>& write_payload);

bool ixiptlah_append_record_tagged_temporal_deferred_flush(const fs::path& path,

                                           IxiptlahRecordType type,

                                           std::uint32_t schema_version,

                                           const std::string& layer_key,

                                           std::uint64_t temporal_key,

                                           const std::function<bool(std::ostream&)>& write_payload);


bool ixiptlah_write_single_record_atomic(const fs::path& path,


                                        IxiptlahRecordType type,


                                        std::uint32_t schema_version,


                                        const std::function<bool(std::ostream&)>& write_payload);




struct IxiptlahRewriteStats {
    std::uint64_t kept = 0;
    std::uint64_t removed = 0;

    std::uint64_t unreadable = 0;
    bool rewritten = false;
};




struct IxiptlahCopyStats {
    std::uint64_t copied = 0;

    std::uint64_t unreadable = 0;
    bool target_touched = false;
};



IxiptlahRewriteStats ixiptlah_rewrite_without_records(

    const fs::path& path,



    const std::function<bool(IxiptlahRecordType, std::uint32_t)>& drop_record);


IxiptlahCopyStats ixiptlah_append_selected_records_raw(

    const fs::path& dst,

    const fs::path& src,



    const std::function<bool(IxiptlahRecordType, std::uint32_t)>& accept_record);




struct IxiptlahRecordManifestEntry {
    IxiptlahRecordType type = IxiptlahRecordType::EpidemiologyObservation;
    std::uint32_t schema = 0;
    std::uint64_t stored_size = 0;
    std::uint64_t raw_size = 0;
    std::uint64_t layer_hash = 0;
    std::uint64_t temporal_key = 0;
};

[[nodiscard]] std::vector<IxiptlahRecordManifestEntry> ixiptlah_record_manifest(const fs::path& path);

void ixiptlah_read_records(const fs::path& path,



                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);


void ixiptlah_read_selected_records(const fs::path& path,


                                   const std::function<bool(IxiptlahRecordType, std::uint32_t)>& accept_record,



                                   const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);


void ixiptlah_read_selected_records_tagged(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);

// Variante temporal V1: entrega al predicado la llave YYYYMMDDHHmm del
// encabezado antes de abrir el payload. Es la ruta obligatoria para datos
// atmosfericos por decada; impide que un dia/hora de 2024 lea perfiles horarios
// de 1987, 1994 o 2012 solo porque comparten minuto del dia.
void ixiptlah_read_selected_records_tagged_temporal(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);

// Variante física estricta: conserva el orden append-only del archivo después del
// prefiltro de encabezado. Los snapshots completos y sus deltas dependen de esa
// secuencia; ordenar por fecha rompería la semántica de sustitución+delta.
void ixiptlah_read_selected_records_tagged_temporal_physical(const fs::path& path,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::uint64_t, std::uint64_t)>& accept_record,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);

// Consulta exacta de ruta caliente: type+schema+capa+intervalo temporal. Evita
// predicados generales y usa buckets internos del directorio IXIPTLAH cuando la
// vista ya conoce qué red/capa/mes necesita. La lectura mantiene orden físico.
void ixiptlah_read_selected_records_tagged_temporal_exact(const fs::path& path,

                                          IxiptlahRecordType type,

                                          std::uint32_t schema,

                                          const std::vector<std::uint64_t>& layer_hashes,

                                          bool include_zero_layer,

                                          std::uint64_t temporal_begin,

                                          std::uint64_t temporal_end,

                                          const std::function<bool(IxiptlahRecordType, std::uint32_t, std::istream&)>& read_payload);





void ixiptlah_flush_all();



void ixiptlah_close_all();

}

// ===== AtmosphereModel.hpp =====
namespace epi {




struct AtmosphereFoundationOptions {

    fs::path source_root;

    fs::path output_root;
    bool resume = true;

    int max_files = 400;

    int sample_lines_per_file = 20;


    int live_flush_every_files = 1;

    int year_start = 0;

    int year_end = 0;
    std::string forced_domain;

    std::string forced_provider;
    bool inventory_only = false;

    /* 0=autodetectar por ruta; 1=RAMA contaminantes; 2=REDMET/REDMA meteorologia; 3=RUOA/PEMBU.
       La importacion no debe depender del nombre fisico de la carpeta elegida:
       la UI ya conoce la red seleccionada y debe imponer esa frontera al parser. */
    int forced_source_family = 0;



    std::function<void(int files_done, int rows_written, const std::string& phase)> progress_callback;

    /* Pulso fino para UI: no modifica IXIPTLAH ni fuerza dependencias. Permite
       reportar avance dentro de CSV grandes RAMA/REDMET sin esperar a cerrar
       el archivo completo. */
    std::function<void(int files_done, int total_files, std::int64_t current_rows,
                       std::uint64_t bytes_done, std::uint64_t bytes_total,
                       const std::string& current_file, const std::string& phase,
                       std::uint64_t import_errors, std::uint64_t external_errors)> detail_progress_callback;
};




struct AtmosphereFoundationReport {

    int discovered_files = 0;

    int indexed_files = 0;

    int skipped_by_checkpoint = 0;

    int sampled_files = 0;
    int cloud_points = 0;


    fs::path manifest_jsonl;


    fs::path stac_items_jsonl;


    fs::path model_base_json;


    fs::path cloud_jsonl;
};




class AtmosphericReconstructionEngine {


public:

    [[nodiscard]] static AtmosphereFoundationReport prepare_foundation(const AtmosphereFoundationOptions& options);

private:


    struct SourceFile {

        fs::path path;

        std::string file_id;
        std::string kind;
        std::string provider;
        std::string temporal_hint;

        std::string variable_hint;
        std::string delimiter;

        std::vector<std::string> columns;
        uintmax_t bytes = 0;

        long long mtime_tick = 0;
    };




    [[nodiscard]] static std::vector<SourceFile> discover_sources(const AtmosphereFoundationOptions& options);

    [[nodiscard]] static SourceFile describe_source(const fs::path& path, const AtmosphereFoundationOptions& options);

    [[nodiscard]] static std::set<std::string> load_checkpoint_ids(const fs::path& checkpoint);

    static void append_checkpoint(const fs::path& checkpoint, const SourceFile& source);


    static void write_model_base(const fs::path& out);


    static void append_manifest_rows(const AtmosphereFoundationOptions& options, const std::vector<SourceFile>& rows);
};

}

// ===== TableEngine.hpp =====
namespace epi {




class TableEngine {
public:

    explicit TableEngine(const Config& config) : config_(config) {}



    [[nodiscard]] std::vector<TableCandidate> reconstruct_page(const PdfDocument& doc, const PageText& page) const;
private:
    const Config& config_;


    struct Line { int index=-1; double y=0; Rect box; std::vector<Token> tokens; std::string text; std::string norm; };


    [[nodiscard]] std::vector<Line> make_lines(const PageText& page) const;

    [[nodiscard]] std::vector<RowBand> detect_rows(const std::vector<Line>& lines) const;

    [[nodiscard]] std::vector<std::vector<RowBand>> detect_row_blocks(const std::vector<Line>& lines) const;

    [[nodiscard]] std::vector<Token> numeric_tokens_in_rows(const PageText& page, const std::vector<RowBand>& rows) const;


    [[nodiscard]] std::vector<ColumnBand> cluster_columns(const std::vector<Token>& nums, const PageText& page, const std::vector<RowBand>& rows) const;

    void assign_cells(TableCandidate& t, const std::vector<Token>& nums) const;



    void infer_headers(TableCandidate& t, const PdfDocument& doc, const PageText& page) const;



    void validate_and_materialize(TableCandidate& t, const PdfDocument& doc) const;

    [[nodiscard]] static std::string infer_period(const std::string& header_norm, int ordinal_mod);

    [[nodiscard]] static std::string infer_sex(const std::string& header_norm);

    [[nodiscard]] static std::string make_column_key(const ColumnBand& c);
};

}

// ===== TemporalBlocks.hpp =====
namespace epi {




struct TemporalEpidemiologyRecord {
    std::string entity;

    int year = 0;

    int epi_week = 0;
    int page = 0;
    std::string disease;
    std::string cie10;

    std::string jurisdiction;

    std::string period;
    std::string sex;
    int64_t value = 0;
};



struct TemporalAtmospherePackedStation {
    std::string id;
    std::string name;

    double lon = 0.0;

    double lat = 0.0;

    double alt = 0.0;
};




struct TemporalAtmospherePackedSample {
    std::uint8_t day = 1;

    std::uint8_t hour = 0;

    std::uint8_t minute = 0;

    std::uint32_t station_index = 0;
    std::uint32_t pollutant_index = 0;
    std::uint32_t unit_index = 0;

    double value = 0.0;
};



[[nodiscard]] int temporal_lustrum_start_for_year(int year);

[[nodiscard]] std::string temporal_lustrum_label_for_year(int year);

[[nodiscard]] std::string temporal_iso_date_from_epi_week(int year, int week);

[[nodiscard]] int temporal_year_from_date_or_fields(const std::string& date, const std::string& year_hint);

[[nodiscard]] const std::vector<std::string>& temporal_block_columns();

[[nodiscard]] std::string temporal_block_header_line();


[[nodiscard]] std::vector<std::string> split_tsv_lossless(const std::string& line);

[[nodiscard]] std::string tsv_escape_field(std::string value);

[[nodiscard]] bool temporal_is_lustrum_data_file(const fs::path& p, const std::string& required_extension = {});

[[nodiscard]] std::vector<fs::path> temporal_tsv_files(const fs::path& root);


[[nodiscard]] std::vector<fs::path> temporal_csv_files(const fs::path& root);


[[nodiscard]] std::vector<fs::path> temporal_json_files(const fs::path& root);


[[nodiscard]] fs::path temporal_ixiptlah_file(const fs::path& root);


[[nodiscard]] std::vector<fs::path> temporal_ixiptlah_files(const fs::path& root);

[[nodiscard]] std::vector<fs::path> temporal_epidemiology_ixiptlah_files(const fs::path& root);


[[nodiscard]] bool temporal_has_ixiptlah_records(const fs::path& root);

[[nodiscard]] bool temporal_has_epidemiology_ixiptlah_records(const fs::path& root);
void temporal_read_epidemiology_records(const fs::path& root,



                                        const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record);

void temporal_read_epidemiology_records_recent_files(const fs::path& root,
                                                     int max_files,
                                                     const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record);

void temporal_read_epidemiology_records_from_ixiptlah_file(const fs::path& path,
                                                           const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record);

void temporal_read_epidemiology_records_from_ixiptlah_file_filtered(
    const fs::path& path,
    const std::function<bool(std::uint64_t layer_hash, std::uint64_t temporal_key)>& accept_record,
    const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record);

void temporal_read_epidemiology_records_from_ixiptlah_file_exact(
    const fs::path& path,
    const std::vector<std::uint64_t>& layer_hashes,
    bool include_zero_layer,
    std::uint64_t temporal_begin,
    std::uint64_t temporal_end,
    const std::function<bool(const TemporalEpidemiologyRecord&)>& on_record);


void temporal_append_record(const fs::path& root, const std::map<std::string, std::string>& fields);


void temporal_append_source_inventory_record(const fs::path& root,

                                             const std::map<std::string, std::string>& fields);


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
                                           const std::string& unit);

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

                                                  const std::vector<TemporalAtmospherePackedSample>& samples);

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
                                               int64_t count);

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
                                                  int64_t count);


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
                                        int64_t value);


void temporal_append_epidemiology_records_batch(const fs::path& root,


                                                const std::vector<TemporalEpidemiologyRecord>& records);


void temporal_append_epidemiology_records_batch_exact_root(const fs::path& root,


                                                           const std::vector<TemporalEpidemiologyRecord>& records);


void temporal_flush_append_streams();


void temporal_flush_append_streams_if_due(int min_interval_ms);

void temporal_close_append_streams();



[[nodiscard]] int temporal_purge_atmosphere_category(const fs::path& root, const std::string& category, int year_start = 0, int year_end = 0);


[[nodiscard]] int temporal_purge_epidemiology_records(const fs::path& root, int year_start = 0, int year_end = 0);


void temporal_rebuild_json_index_for_tsv(const fs::path& tsv_path);



void temporal_rebuild_all_json_indexes(const fs::path& root);

}

// ===== Writers.hpp =====
namespace epi {




class OutputStore {
public:


    explicit OutputStore(fs::path root);

    void open(bool resume = true);



    void write_tokens_jsonl(const PdfDocument& doc);


    void append_table(const TableCandidate& t);



    void flush_live_outputs(const fs::path& config_dir, bool build_xlsx = false);


    void flush_streams();



    void write_derived_outputs(const fs::path& geojson_path);


    [[nodiscard]] size_t observation_count() const noexcept { return observation_count_; }

    [[nodiscard]] size_t quarantine_count() const noexcept { return quarantine_count_; }

    void finalize();


    [[nodiscard]] const fs::path& root() const noexcept { return root_; }
private:

    fs::path root_;

    fs::path write_root_;


    fs::path epidemiology_rebuild_root_;


    std::ofstream obs_csv_;


    std::ofstream obs_jsonl_;


    std::ofstream base_obs_csv_;


    std::ofstream base_obs_jsonl_;


    std::ofstream quarantine_csv_;


    std::ofstream sql_;


    std::ofstream sql_spanish_;


    std::ofstream table_audit_jsonl_;

    std::vector<Observation> observations_;

    std::vector<QuarantineItem> quarantines_;


    std::vector<TemporalEpidemiologyRecord> epidemiology_batch_buffer_;

    std::unordered_set<std::uint64_t> observation_keys_;

    std::unordered_set<std::uint64_t> quarantine_keys_;
    std::chrono::steady_clock::time_point last_epidemiology_live_flush_{};
    size_t observation_count_ = 0;
    size_t quarantine_count_ = 0;
    bool opened_ = false;

    bool staged_epidemiology_rebuild_ = false;

    void load_existing_observations();


    void load_existing_quarantine();



    void write_obs_csv_header();



    void write_quarantine_csv_header();


    void write_sql_header();


    void write_observation(const Observation& o);


    void write_quarantine(const QuarantineItem& q);


    void flush_epidemiology_batch_buffer();


    void commit_epidemiology_rebuild();



    void write_results_json() const;



    void write_master_csv() const;


    void write_derived_sex_incidence() const;


    void write_atmospheric_inventory() const;
};

}

// ===== Dashboard.hpp =====
namespace epi {




struct DashboardPage {


    std::string pdf_file;
    int page = 0;
    double page_width = 0.0;
    double page_height = 0.0;

    fs::path image_path;

    std::vector<TableCandidate> tables;
};




class Dashboard {
public:


    explicit Dashboard(fs::path root) : root_(std::move(root)) {}


    void ensure();

    void push_page(DashboardPage page, const PipelineStats& stats);


    [[nodiscard]] fs::path index_path() const { return root_ / "dashboard_live_preview.ixiptlah"; }
private:

    fs::path root_;

    std::vector<DashboardPage> last_;


    void write(const PipelineStats& stats);
};

}

// ===== PdfExtractor.hpp =====
namespace epi {




class PdfTextExtractor {
public:


    PdfTextExtractor(const ExternalTools& tools, Logger& log) : tools_(tools), log_(log) {}



    PdfDocument extract(const fs::path& pdf, const fs::path& work_dir) const;
private:
    const ExternalTools& tools_;

    Logger& log_;



    [[nodiscard]] std::vector<PageText> parse_bbox_layout(const fs::path& html, int page_number_offset = 0) const;


    [[nodiscard]] std::vector<PageText> parse_plain_layout_text(const fs::path& text_file, int page_number_offset = 0) const;


    [[nodiscard]] static std::pair<int,int> infer_year_week(const fs::path& pdf, const std::vector<PageText>& pages);
};

}

// ===== Pipeline.hpp =====
namespace epi {





class Pipeline {
public:


    explicit Pipeline(AppOptions options);


    int run();
private:
    AppOptions options_;

    Logger log_;
    Config config_;


    ExternalTools tools_;
    PdfTextExtractor extractor_;
    TableEngine table_engine_;

    OutputStore output_;
    Dashboard dashboard_;
    PipelineStats stats_;


    void honor_controls();



    [[nodiscard]] PdfDocument extract_pdf_document(const fs::path& pdf);


    [[nodiscard]] bool process_pdf(const fs::path& pdf, int index);


    [[nodiscard]] bool process_pdf_document(const fs::path& pdf, int index, PdfDocument doc);


    [[nodiscard]] fs::path preview_prefix(const PdfDocument& doc) const;


    [[nodiscard]] fs::path preview_image_path(const PdfDocument& doc, int page) const;


    [[nodiscard]] fs::path render_page(const fs::path& pdf, const PdfDocument& doc, const PageText& page) const;


    void render_preview_batch(const fs::path& pdf, const PdfDocument& doc, int first_page, int last_page) const;



    void write_master_summaries();
};

}


// ===== Contrato C++ visible fusionado desde Tlalpowa.h =====
namespace epi {

/* Contrato público del núcleo visible de interfaz.  Este encabezado debe
   permanecer pequeño: la UI puede crecer internamente, pero otros núcleos sólo
   deben conocer estas entradas estables y no depender de tipos ImGui/GLFW. */
int run_tlalpowa_app();
bool tlalpowa_tlalpowa3d_regeoref_selftest();
int run_atmosphere_web_import_cli(int source, int year_start, int year_end, bool overwrite_category);
int run_external_import_smoke_cli(const std::filesystem::path& source_root,
                                 const std::filesystem::path& output_root,
                                 int year_start,
                                 int year_end,
                                 bool inventory_only);
int run_satellite_web_import_cli(int source,
                                 const std::filesystem::path& output_root,
                                 int year_start,
                                 int year_end);
int run_epidemiology_web_download_cli(bool cdmx, bool edomex, int year_start, int year_end);

}  // namespace epi
