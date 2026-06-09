


#pragma once
#include "TableEngine.hpp"
#include "TemporalBlocks.hpp"
#include <chrono>



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
