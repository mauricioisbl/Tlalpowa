


#pragma once
#include "Writers.hpp"



namespace epi {




struct DashboardPage {


    std::string pdf_file;
    int page = 0;
    double page_width = 0.0;
    double page_height = 0.0;

    fs::path image_path;

    std::vector<TableCandidate> tables;
};




class Dashboard {
public:


    explicit Dashboard(fs::path root) : root_(std::move(root)) {}


    void ensure();

    void push_page(DashboardPage page, const PipelineStats& stats);


    [[nodiscard]] fs::path index_path() const { return root_ / "dashboard_live_preview.ixiptlah"; }
private:

    fs::path root_;

    std::vector<DashboardPage> last_;


    void write(const PipelineStats& stats);
};

}
