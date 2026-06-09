


#pragma once
#include "EpiTypes.hpp"



namespace epi {



[[nodiscard]] std::string trim(std::string s);


[[nodiscard]] std::string lower_ascii(std::string s);

[[nodiscard]] std::string clean_user_path_string(std::string s);

[[nodiscard]] fs::path clean_user_path(const fs::path& p);

[[nodiscard]] fs::path resolve_existing_path_relaxed(const fs::path& p);

[[nodiscard]] std::string strip_accents_utf8(std::string s);


[[nodiscard]] std::string normalize_key(std::string s);

[[nodiscard]] std::string html_unescape(std::string s);


[[nodiscard]] std::string json_escape(const std::string& s);


[[nodiscard]] std::string csv_escape(const std::string& s);


[[nodiscard]] bool contains_norm(const std::string& haystack_norm, const std::string& needle_norm);

[[nodiscard]] bool is_numeric_token(const std::string& s);


[[nodiscard]] std::optional<int64_t> parse_epi_int(const std::string& s);

[[nodiscard]] std::string safe_filename(std::string s);

[[nodiscard]] std::string simple_hash_hex(const std::string& s);

[[nodiscard]] std::string read_text_file(const fs::path& p);

void write_text_file(const fs::path& p, const std::string& content);


void ensure_dir(const fs::path& p);



[[nodiscard]] uintmax_t file_size_or_zero(const fs::path& p);


bool copy_file_overwrite(const fs::path& source, const fs::path& destination, std::error_code& ec);



[[nodiscard]] std::vector<fs::path> list_pdfs_recursive(const fs::path& root);

[[nodiscard]] std::string now_utc_iso();

[[nodiscard]] std::wstring widen_utf8(const std::string& s);

[[nodiscard]] std::string narrow_utf8(const std::wstring& s);


[[nodiscard]] std::string path_utf8(const fs::path& p);

[[nodiscard]] fs::path executable_dir();

[[nodiscard]] fs::path project_root();

[[nodiscard]] fs::path config_root();

[[nodiscard]] fs::path internal_data_root();


[[nodiscard]] fs::path external_data_root();



[[nodiscard]] std::string getenv_utf8_or_empty(const char* name);

[[nodiscard]] fs::path getenv_path_utf8(const char* name);

[[nodiscard]] std::wstring getenv_wstring_or_empty(const wchar_t* name);



class Logger {
public:


    explicit Logger(fs::path log_file);

    void info(const std::string& m);

    void warn(const std::string& m);


    void error(const std::string& m);
private:


    std::mutex mu_;


    std::ofstream out_;

    void line(const std::string& level, const std::string& m);
};

}
