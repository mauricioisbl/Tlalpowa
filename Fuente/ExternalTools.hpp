


#pragma once
#include "TextUtils.hpp"



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
