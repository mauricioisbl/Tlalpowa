#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TlalRuoaSession TlalRuoaSession;

typedef struct TlalRuoaLoginReport {
    int attempted;
    int ok;
    long http_status;
    char stage[64];
    char message[512];
} TlalRuoaLoginReport;

typedef struct TlalRuoaDownloadReport {
    int ok;
    int cancelled;
    long http_status;
    uint64_t bytes_written;
    char stage[64];
    char message[512];
} TlalRuoaDownloadReport;

typedef struct TlalPembuRow {
    TlalAtmStamp stamp;
    const char* parameter_id;
    const char* unit;
    double value;
} TlalPembuRow;

typedef struct TlalPembuParseStats {
    uint64_t physical_lines;
    uint64_t data_lines;
    uint64_t emitted_measurements;
    uint64_t rejected_cells;
    int header_found;
    int delimiter;
} TlalPembuParseStats;

typedef enum TlalAtmosSourceFamily {
    TLAL_ATMOS_SOURCE_UNKNOWN = 0,
    TLAL_ATMOS_SOURCE_RAMA = 1,
    TLAL_ATMOS_SOURCE_REDMA = 2,
    TLAL_ATMOS_SOURCE_RUOA = 3
} TlalAtmosSourceFamily;

typedef struct TlalAtmosCsvRow {
    TlalAtmStamp stamp;
    const char* station_id;
    const char* parameter_id;
    const char* unit;
    double value;
    double latitude;
    double longitude;
    int has_coordinates;
} TlalAtmosCsvRow;

typedef struct TlalAtmosCsvParseStats {
    uint64_t physical_lines;
    uint64_t data_lines;
    uint64_t emitted_measurements;
    uint64_t rejected_cells;
    uint64_t hour_resolution_rows;
    uint64_t minute_resolution_rows;
    uint64_t parameter_wide_columns;
    uint64_t station_wide_columns;
    int header_found;
    int delimiter;
    int format_flags;
    int source_family;
} TlalAtmosCsvParseStats;

typedef int (*TlalPembuRowFn)(const TlalPembuRow* row, void* user);
typedef int (*TlalAtmosCsvRowFn)(const TlalAtmosCsvRow* row, void* user);

/* Pulso de lectura estrictamente observacional: no altera filas, no conserva
 * punteros internos y permite a la UI respirar durante CSV grandes. */
typedef int (*TlalAtmosCsvProgressFn)(uint64_t physical_lines,
                                      uint64_t data_lines,
                                      uint64_t emitted_measurements,
                                      uint64_t bytes_done,
                                      uint64_t bytes_total,
                                      void* user);

TlalRuoaSession* tlal_ruoa_session_create(void);
void tlal_ruoa_session_destroy(TlalRuoaSession* session);

int tlal_ruoa_session_login(TlalRuoaSession* session,
                            const char* username_utf8,
                            const char* password_utf8,
                            TlalRuoaLoginReport* report);

const char* tlal_ruoa_session_endpoint_user_utf8(const TlalRuoaSession* session);
const char* tlal_ruoa_session_endpoint_email_utf8(const TlalRuoaSession* session);

int tlal_ruoa_session_download_utf8(TlalRuoaSession* session,
                                    const char* url_utf8,
                                    const char* target_tmp_path_utf8,
                                    const volatile int* cancel_flag,
                                    TlalRuoaDownloadReport* report);

int tlal_ruoa_pembu_csv_valid_utf8(const char* path_utf8, uint64_t min_bytes);

int tlal_pembu_csv_parse_file_utf8(const char* path_utf8,
                                   TlalPembuRowFn callback,
                                   void* user,
                                   TlalPembuParseStats* stats);

int tlal_atmos_csv_parse_file_utf8(const char* path_utf8,
                                   TlalAtmosCsvRowFn callback,
                                   void* user,
                                   TlalAtmosCsvParseStats* stats);

int tlal_atmos_csv_parse_file_utf8_for_family(const char* path_utf8,
                                              int forced_source_family,
                                              TlalAtmosCsvRowFn callback,
                                              void* user,
                                              TlalAtmosCsvParseStats* stats);

int tlal_atmos_csv_parse_file_utf8_for_family_progress(const char* path_utf8,
                                                       int forced_source_family,
                                                       TlalAtmosCsvRowFn callback,
                                                       void* user,
                                                       TlalAtmosCsvProgressFn progress_callback,
                                                       void* progress_user,
                                                       TlalAtmosCsvParseStats* stats);

#ifdef __cplusplus
}
#endif
