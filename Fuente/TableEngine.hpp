


#pragma once
#include "Config.hpp"



namespace epi {




class TableEngine {
public:

    explicit TableEngine(const Config& config) : config_(config) {}



    [[nodiscard]] std::vector<TableCandidate> reconstruct_page(const PdfDocument& doc, const PageText& page) const;
private:
    const Config& config_;


    struct Line { int index=-1; double y=0; Rect box; std::vector<Token> tokens; std::string text; std::string norm; };


    [[nodiscard]] std::vector<Line> make_lines(const PageText& page) const;

    [[nodiscard]] std::vector<RowBand> detect_rows(const std::vector<Line>& lines) const;

    [[nodiscard]] std::vector<std::vector<RowBand>> detect_row_blocks(const std::vector<Line>& lines) const;

    [[nodiscard]] std::vector<Token> numeric_tokens_in_rows(const PageText& page, const std::vector<RowBand>& rows) const;


    [[nodiscard]] std::vector<ColumnBand> cluster_columns(const std::vector<Token>& nums, const PageText& page, const std::vector<RowBand>& rows) const;

    void assign_cells(TableCandidate& t, const std::vector<Token>& nums) const;



    void infer_headers(TableCandidate& t, const PdfDocument& doc, const PageText& page) const;



    void validate_and_materialize(TableCandidate& t, const PdfDocument& doc) const;

    [[nodiscard]] static std::string infer_period(const std::string& header_norm, int ordinal_mod);

    [[nodiscard]] static std::string infer_sex(const std::string& header_norm);

    [[nodiscard]] static std::string make_column_key(const ColumnBand& c);
};

}
