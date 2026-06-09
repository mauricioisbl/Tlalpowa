


#pragma once
#include "TextUtils.hpp"



namespace epi {



class Config {
public:


    void load(const fs::path& config_dir);


    [[nodiscard]] const std::vector<Jurisdiction>& jurisdictions() const noexcept { return jurisdictions_; }

    [[nodiscard]] const std::vector<Disease>& diseases() const noexcept { return diseases_; }


    [[nodiscard]] std::optional<Jurisdiction> match_jurisdiction_line(const std::string& normalized_line) const;


    [[nodiscard]] std::optional<Disease> match_disease_text(const std::string& normalized_text) const;

    [[nodiscard]] std::optional<Disease> match_disease_cie10(const std::string& cie10_text) const;


    [[nodiscard]] std::string default_table_title() const { return "tabla_epidemiologica"; }
private:


    std::vector<Jurisdiction> jurisdictions_;

    std::vector<Disease> diseases_;

    void load_builtin();


    void load_jurisdictions_tsv(const fs::path& p);


    void load_diseases_tsv(const fs::path& p);



    void normalize_catalog_entries();
};

}
