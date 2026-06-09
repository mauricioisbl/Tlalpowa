



#pragma once
#include "ExternalTools.hpp"
#include "Config.hpp"



namespace epi {




class PdfTextExtractor {
public:


    PdfTextExtractor(const ExternalTools& tools, Logger& log) : tools_(tools), log_(log) {}



    PdfDocument extract(const fs::path& pdf, const fs::path& work_dir) const;
private:
    const ExternalTools& tools_;

    Logger& log_;



    [[nodiscard]] std::vector<PageText> parse_bbox_layout(const fs::path& html, int page_number_offset = 0) const;


    [[nodiscard]] std::vector<PageText> parse_plain_layout_text(const fs::path& text_file, int page_number_offset = 0) const;


    [[nodiscard]] static std::pair<int,int> infer_year_week(const fs::path& pdf, const std::vector<PageText>& pages);
};

}
