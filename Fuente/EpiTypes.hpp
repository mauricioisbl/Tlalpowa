


#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>



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
