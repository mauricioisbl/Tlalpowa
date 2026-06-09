
#pragma once
#include "TextUtils.hpp"

#include <functional>
#include <cstdint>
#include <vector>



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
