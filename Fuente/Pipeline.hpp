



#pragma once
#include "Dashboard.hpp"
#include "PdfExtractor.hpp"



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
