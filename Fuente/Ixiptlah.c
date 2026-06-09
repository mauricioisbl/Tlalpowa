/* Núcleo visible de datos: lectura, escritura, representación y exportación IXIPTLAH.
   Se compila como C++ por integración histórica con STL, pero la frontera C queda en core.c. */
#include "Ixiptlah.h"

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
