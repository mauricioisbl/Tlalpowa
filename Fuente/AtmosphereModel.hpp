


#pragma once
#include "TextUtils.hpp"
#include <cstdint>



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
