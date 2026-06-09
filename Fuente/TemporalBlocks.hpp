
#pragma once
#include "TextUtils.hpp"

#include "Ixiptlah.hpp"

#include <cstdint>
#include <vector>



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
