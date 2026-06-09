#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TLALPOWA_HOTDATA_CORE_ANY 0u
#define TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY 1u
#define TLALPOWA_HOTDATA_CORE_METEOROLOGY 2u
#define TLALPOWA_HOTDATA_CORE_CONTAMINANT 3u
#define TLALPOWA_HOTDATA_CORE_OTHER 255u

typedef struct TlalpowaHotDataConfig {
    uint64_t max_total_touch_bytes;
    uint32_t max_depth;
    uint32_t max_ixiptlah_files;
    uint32_t max_payload_bytes_per_record;
    uint32_t enable_3d_touch;
    uint32_t probe_bytes_per_record;
    uint32_t progressive_neighbor_records;
    uint32_t neighbor_bytes_per_record;
    uint32_t keep_runtime_index;
    uint64_t max_runtime_cache_bytes;
    uint32_t runtime_cache_lines;
    uint32_t startup_gate_records_per_core;
    uint32_t startup_gate_bytes_per_record;
    uint32_t startup_gate_category_limit;
} TlalpowaHotDataConfig;

typedef struct TlalpowaHotDataStats {
    uint64_t files_seen;
    uint64_t ixiptlah_files;
    uint64_t ixiptlah_records;
    uint64_t ixiptlah_directories;
    uint64_t touched_bytes;
    uint64_t latest_temporal_key;
    uint64_t latest_epidemiology_key;
    uint64_t latest_atmosphere_key;
    uint64_t latest_3d_bytes;
    uint64_t failed_files;
    uint64_t indexed_records;
    uint64_t record_probe_bytes;
    uint64_t mapped_files;
    uint64_t latest_contaminant_key;
    uint64_t latest_meteorology_key;
    uint64_t progressive_records_touched;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cache_bytes;
    uint64_t binary_searches;
    uint64_t prepared_hits;
    uint64_t prepared_bytes;
    uint64_t exact_window_hits;
    uint64_t retained_mapped_file_bytes;
    uint64_t startup_gate_hits;
    uint64_t startup_gate_bytes;
    uint64_t startup_gate_exact_hits;
    uint64_t startup_gate_categories;
    uint64_t startup_gate_expected_hits;
} TlalpowaHotDataStats;

typedef struct TlalpowaHotDataHit {
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t stored_size;
    uint64_t layer_hash;
    uint32_t type;
    uint32_t schema;
    uint32_t core_group;
    uint32_t file_index;
    char path[4096];
} TlalpowaHotDataHit;

TlalpowaHotDataConfig tlalpowa_hotdata_default_config(void);
int tlalpowa_hotdata_prewarm_root(const char* root_utf8,
                                  const TlalpowaHotDataConfig* config,
                                  TlalpowaHotDataStats* stats);

int tlalpowa_hotdata_prefetch_temporal(uint32_t core_group,
                                       uint64_t temporal_key,
                                       uint32_t neighbor_records,
                                       uint32_t bytes_per_record,
                                       TlalpowaHotDataStats* stats);

int tlalpowa_hotdata_find_nearest(uint32_t core_group,
                                  uint64_t temporal_key,
                                  TlalpowaHotDataHit* hit);

uint32_t tlalpowa_hotdata_collect_window(uint32_t core_group,
                                         uint64_t temporal_key,
                                         uint32_t max_hits,
                                         TlalpowaHotDataHit* hits);

uint64_t tlalpowa_hotdata_read_hit(const TlalpowaHotDataHit* hit,
                                   void* out_buffer,
                                   uint64_t out_capacity,
                                   uint64_t payload_relative_offset);

/*
Contrato fijo de hot data:
- Bienvenida: no usa la fecha civil actual. Espera los ultimos DIEZ registros
  IXIPTLAH realmente disponibles por cada categoria fisica encontrada
  (nucleo/tipo/esquema/capa), dentro del limite de seguridad configurado.
  Esa es la hotdata inicial; el fade solo debe iniciar cuando, ademas, la
  primera fecha visible ya fue preparada para evitar espera posterior.
- Despues de abrir la interfaz: cada temporal_key solicitado sirve primero, de
  forma sincronica y prioritaria, la fecha/hora activa o su vecino fisico mas
  cercano; solo despues precalienta vecinos cronologicos adelante/atras del mas
  cercano al mas lejano.
- Nunca sustituye payloads por resumenes, sidecars ni agregados.
*/
uint32_t tlalpowa_hotdata_prepare_active_temporal_view(uint32_t core_group,
                                                       uint64_t temporal_key,
                                                       uint32_t active_hits,
                                                       uint32_t active_bytes_per_hit,
                                                       uint32_t neighbor_hits,
                                                       uint32_t neighbor_bytes_per_hit,
                                                       TlalpowaHotDataHit* hits,
                                                       TlalpowaHotDataStats* stats);

uint32_t tlalpowa_hotdata_prepare_temporal_view(uint32_t core_group,
                                                uint64_t temporal_key,
                                                uint32_t max_hits,
                                                uint32_t bytes_per_hit,
                                                TlalpowaHotDataHit* hits,
                                                TlalpowaHotDataStats* stats);

void tlalpowa_hotdata_release_runtime_index(void);

#ifdef __cplusplus
}
#endif
