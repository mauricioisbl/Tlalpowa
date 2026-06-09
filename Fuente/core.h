




#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif



int ozmvm_json_string_span(const char* s, size_t n, const char* key,
                           size_t* out_begin, size_t* out_end, int* out_has_escape);



int64_t ozmvm_json_i64(const char* s, size_t n, const char* key);

int ozmvm_attr_double_span(const char* s, size_t begin, size_t end, const char* attr, double* out);


int ozmvm_first_year_20xx(const char* s, size_t n);


int ozmvm_week_after_marker(const char* s, size_t n, const char* marker);


int ozmvm_week_before_del_year(const char* s, size_t n);

int ozmvm_parse_numeric_date_time_components(const char* s, size_t n, int out_parts5[5]);

int ozmvm_first_epi_year_1900_2099(const char* s, size_t n);

int ozmvm_spanish_epi_week_words_to_int(const char* s, size_t n);

int ozmvm_epi_week_contextual(const char* s, size_t n);

int ozmvm_html_next_href(const char* s, size_t n, size_t start,
                         size_t* out_begin, size_t* out_end, size_t* out_next);

size_t ozmvm_strip_internal_hash_fragments_copy(const char* s, size_t n, char* out, size_t out_cap);

int ozmvm_cdmx_bulletin_year_week_from_key(const char* s, size_t n, int* out_year, int* out_week);

int ozmvm_cdmx_zip_year_from_key(const char* s, size_t n);

int ozmvm_edomex_bulletin_year_week_from_url(const char* s, size_t n, int* out_year, int* out_week);

size_t ozmvm_metro_route_key_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_strip_metro_route_prefix_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_compact_route_name_copy(const char* s, size_t n, int mode, char* out, size_t out_cap);

int ozmvm_icon_year_range_from_stem(const char* s, size_t n, int* out_from, int* out_to);

int ozmvm_column_header_role_hint(const char* s, size_t n);

int ozmvm_year_week_label(const char* s, size_t n, int* out_year, int* out_week);

int ozmvm_xml_next_tag_span(const char* s, size_t n, size_t start, const char* tag,
                            size_t* out_begin, size_t* out_end, size_t* out_next);

int ozmvm_xml_value_i32_after_key(const char* s, size_t n, const char* key,
                                  int fallback, int min_value, int max_value);



typedef struct OzmvmTsvEpiFields {

    size_t entity_begin;
    size_t entity_end;

    int year;

    int epi_week;
    int page;
    size_t disease_begin;
    size_t disease_end;
    size_t cie10_begin;
    size_t cie10_end;

    size_t jurisdiction_begin;
    size_t jurisdiction_end;
    size_t period_begin;
    size_t period_end;

    size_t sex_begin;
    size_t sex_end;
    int64_t value;

} OzmvmTsvEpiFields;




int ozmvm_tsv_epi_parse10(const char* s, size_t n, OzmvmTsvEpiFields* out);


typedef struct OzmvmI64Stats {
    size_t n;
    int64_t sum;
    int64_t min_value;
    int64_t max_value;
    double mean;
    double sd;
    double cv;
} OzmvmI64Stats;



typedef struct OzmvmLinearFitD {
    double slope;
    double intercept;
    double index;
    double rmse;
    double mae;
    double mean_x;
    double mean_y;
    double pearson;

    double r2;
    int valid;
} OzmvmLinearFitD;

int ozmvm_i64_stats_strided(const void* base, size_t n, size_t stride,
                            size_t value_offset, OzmvmI64Stats* out);

int ozmvm_indexed_i64_ols_strided(const void* base, size_t n, size_t stride,
                                  size_t value_offset, double* out_slope, double* out_intercept);
double ozmvm_indexed_i64_theil_sen_strided(const void* base, size_t n, size_t stride,
                                           size_t value_offset, size_t cap, double* out_intercept);

double ozmvm_xy_pearson_strided(const void* base, size_t n, size_t stride,
                                size_t x_offset, size_t y_offset);
int ozmvm_xy_ols_strided(const void* base, size_t n, size_t stride,
                         size_t x_offset, size_t y_offset, const double* weights,
                         double* out_slope, double* out_intercept);
int ozmvm_xy_fit_metrics_strided(const void* base, size_t n, size_t stride,
                                 size_t x_offset, size_t y_offset,
                                 double slope, double intercept, OzmvmLinearFitD* out);

double ozmvm_aggregate_double_values(double* values, size_t n, int mode);


size_t ozmvm_mobility_station_icon_key_inplace(char* s, size_t n);


int ozmvm_lag_to_days(int value, int unit);

int ozmvm_lag_value_from_days(int days, int unit);

size_t ozmvm_trim_ascii_span(const char* s, size_t n, size_t* out_begin, size_t* out_end);

size_t ozmvm_lower_ascii_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_clean_user_path_copy(const char* s, size_t n, char* out, size_t out_cap, int windows_paths);

size_t ozmvm_html_unescape_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_strip_accents_utf8_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_normalize_key_utf8_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_safe_filename_copy(const char* s, size_t n, char* out, size_t out_cap);

int ozmvm_is_numeric_token(const char* s, size_t n);

int ozmvm_parse_epi_i64_token(const char* s, size_t n, int64_t* out);

size_t ozmvm_json_escape_copy(const char* s, size_t n, char* out, size_t out_cap);

size_t ozmvm_csv_escape_copy(const char* s, size_t n, char* out, size_t out_cap);

void ozmvm_fnv1a64_hex(const char* s, size_t n, char out_hex16[17]);

uint64_t ozmvm_fnv1a64_update(uint64_t h, const char* s, size_t n);

uint64_t ozmvm_fnv1a64_u64(const char* s, size_t n);

size_t ozmvm_key_fragment_copy(const char* s, size_t n, char* out, size_t out_cap, size_t max_len, const char* fallback);

size_t ozmvm_rect_json_copy(double x0, double y0, double x1, double y1, char* out, size_t out_cap);

size_t ozmvm_rect_csv_copy(double x0, double y0, double x1, double y1, char* out, size_t out_cap);

uint32_t ozmvm_png_crc32_bytes(const unsigned char* data, size_t n);

uint32_t ozmvm_png_adler32_bytes(const unsigned char* data, size_t n);

size_t ozmvm_png_rgba_uncompressed_size(int w, int h);

size_t ozmvm_png_rgba_uncompressed_encode(int w, int h, const unsigned char* rgba_top_down,
                                          unsigned char* out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif



/* Núcleo epidemiológico C: selección, hash, rango anual y cronología IXIPTLAH sin heap. */
const char* ozmvm_epi_all_selection_sentinel(void);
int ozmvm_epi_is_all_selection_key(const char* s, size_t n);
uint64_t ozmvm_fnv1a64_update_field(uint64_t h, const char* s, size_t n);
int ozmvm_ascii_year_prefix4(const char* s, size_t n);
int ozmvm_epi_year_filter_active_i32(int year_start, int year_end);
int ozmvm_epi_year_in_range_i32(int year, int year_start, int year_end);
int ozmvm_epi_year_week_from_label(const char* s, size_t n, int* out_year, int* out_week);
int ozmvm_epi_week_block_key_from_label(const char* s, size_t n);
size_t ozmvm_epi_week_label_from_year_epi_or_date_copy(const char* year_s, size_t year_n,
                                                         const char* epi_s, size_t epi_n,
                                                         const char* date_s, size_t date_n,
                                                         char* out, size_t out_cap);
int ozmvm_ixiptlah_epi_year_from_stem(const char* stem, size_t n);
int ozmvm_ixiptlah_epi_chronology_key_from_stem(const char* stem, size_t n);
const char* ozmvm_epi_metric_from_period(const char* period, size_t period_n,
                                         const char* source_year, size_t source_year_n,
                                         const char* year, size_t year_n,
                                         const char* sex, size_t sex_n);
const char* ozmvm_epi_period_from_code(uint8_t code);
const char* ozmvm_epi_sex_from_code(uint8_t code);




/* Tonal: núcleo calendárico C puro para cronología, fecha civil y tonalli.
   Nombre corto, sin referencia territorial; las rutas C++ sólo lo envuelven para ImGui. */
typedef struct TonalStamp {
    int number;
    int sign_index;
    const char* sign_name;
} TonalStamp;

int64_t tonal_days_from_civil(int y, unsigned m, unsigned d);
void tonal_civil_from_days(int64_t z, int* y, unsigned* m, unsigned* d);
int64_t tonal_div_floor_i64(int64_t a, int64_t b);
int64_t tonal_mod_pos_i64(int64_t a, int64_t b);
unsigned tonal_days_in_month(int year, unsigned month);
int tonal_is_leap_year(int year);
int tonal_iso_weeks_in_year(int year);
int64_t tonal_hour_from_civil(int start_year, int y, unsigned m, unsigned d, unsigned hour);
void tonal_hour_to_civil(int start_year, int64_t hour, int* y, unsigned* m, unsigned* d, unsigned* h);
size_t tonal_format_hour_copy(int start_year, int64_t hour, char* out, size_t out_cap);
size_t tonal_format_compact_copy(int start_year, int64_t hour, int minute, char* out, size_t out_cap);
size_t tonal_format_long_es_copy(int start_year, int64_t hour, int minute, char* out, size_t out_cap);
size_t tonal_format_week_label_copy(int start_year, int64_t hour, char* out, size_t out_cap);
const char* tonal_month_abbrev_es3(unsigned month);
int tonal_first_numeric_width(const char* raw);
int tonal_parse_hour_text(const char* text, int start_year, int min_year, int max_year, int64_t* out_hour);
int tonal_parse_datetime_text(const char* text, int start_year, int min_year, int max_year,
                              int64_t current_hour, int current_minute, int64_t* out_hour, int* out_minute);
int64_t tonal_hour_for_epi_week(int start_year, int year, int week);
int tonal_stamp_from_civil(int year, unsigned month, unsigned day, TonalStamp* out);
const char* tonal_number_word(int number);
const char* tonal_sign_es(int sign_index);
const char* tonal_sign_name(int sign_index);

/* Amo/Tlac/Tlal: utilidades C puras para bitácora, escala y geografía.
   Nombres cortos sin referencia territorial; buffers externos, sin heap. */
size_t amo_log_hms_copy(char* out, size_t out_cap);
size_t amo_line_clean_copy(const char* s, size_t n, char* out, size_t out_cap);
int64_t tonal_nav_max_hour_now(int start_year, int min_year, int max_year, int future_months, int fallback_year);
int64_t tonal_local_hour_now(int start_year, int min_year, int max_year);
int tonal_local_minute_now(void);
int64_t tonal_clamp_hour(int64_t hour, int64_t lo, int64_t hi);
double tlac_scale_nice_m(double target_m);
size_t tlac_dist_label_copy(double meters, char* out, size_t out_cap);
size_t tlac_i64_group_copy(int64_t value, char* out, size_t out_cap);
size_t tlac_lonlat_label_copy(double lon, double lat, char* out, size_t out_cap);


typedef struct TlalAtmStamp {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} TlalAtmStamp;

int tlal_ix_week_name_parts(const char* stem, size_t n, int* year, int* month, int* day);
int tlal_atm_parse_stamp(const char* date, size_t date_n, const char* time, size_t time_n, TlalAtmStamp* out);

double tlal_lon_px(double lon, int z);
double tlal_lat_px(double lat, int z);
void tlal_px_lonlat(double x, double y, int z, double* out_lon, double* out_lat);
double tlal_dist_m(double lon1, double lat1, double lon2, double lat2);

/* Tlal: presupuesto de mapa en C puro. Sin heap: controla profundidad, ventana
   local de teselas y niebla para evitar streaming territorial innecesario. */
int tlal_lod_desired_z(double scale_den, int center_inside_buffer, int moving, float idle_seconds,
                       int z_overview, int z_300k, int z_90k, int z_30k, int z_15k_idle);
int tlal_lod_step_z(int current_z, int desired_z, int z_min, int z_max, int max_step);
int tlal_tile_margin(float pitch);
size_t tlal_tile_draw_cap(int z, float pitch, float screen_w, float screen_h);
int tlal_stream_budget(int moving, int startup_boost, float app_uptime, float warmup_seconds,
                       float idle_seconds, float deep_idle_seconds, int max_per_frame);
float tlal_fog_strength(float manual, double rh, double pm25, double aod, int moving, float idle_seconds);

/* Tlal: geometría cartográfica métrica y reducción LOD en C puro.
   Las delimitaciones se definen en metros de mundo, no en pixeles fijos: de
   lejos casi desaparecen; a calle recuperan cuerpo real sin trazos inflados. */
double tlal_mpp(double lat_deg, int z, double scale);
float tlal_m_to_px(double meters, double mpp, float min_px, float max_px);
float tlal_boundary_px(double mpp, int has_data, int out_of_focus);
int tlal_boundary_alpha(double mpp, int has_data, int out_of_focus);
int tlal_vertex_stride(double mpp, int source_count);
float tlal_fog_view(float fog, float pitch);
int tlal_atm_mesh_enabled(const char* key, size_t n);
int tlal_atm_station_draw_cap(int z, float pitch);
int tlal_atm_station_halo_alpha(double mpp, int variable_active);

/* Núcleo gráfico C: catálogo, coerción y etiquetas sin asignación dinámica.
tipos proscritos hacia la UI: XY genérico y círculos; la dispersión O3 usa tipo 6. */
typedef struct TlacGal {
    int chart_type;
    int x_domain;
    int x_field;
    int y_domain;
    int y_field;
    int z_domain;
    int z_field;
    int ozone_template;
    const char* title;
    const char* subtitle;
    const char* icon_filename;
} TlacGal;

typedef struct TlacSpec {
    int chart_type;
    int axis_x_domain;
    int axis_x;
    int axis_y_domain;
    int axis_y;
    int axis_z_domain;
    int axis_z;
    int dependency;
    int measurement;
    int exposure_aggregation;
    int use_selected_data;
    int compare_best_lag;
    int auto_best_lag;
    int force_ozone_epi;
    int use_graph_disease_filter;
    int show_fit_line;
    int fit_model;
    int lag_weeks;
    int lag_days;
    int lag_value;
    int lag_unit;
} TlacSpec;

typedef struct TlacToc {
    int item_count;
    float pad;
    float gap;
    float title_h;
    float grid_x;
    float grid_y;
    float grid_w;
    float grid_h;
    float tile_w;
    float tile_h;
} TlacToc;

typedef struct TlacRectI {
    int x0;
    int y0;
    int x1;
    int y1;
    int gl_y;
    int w;
    int h;
} TlacRectI;

size_t tlac_gal_count(void);
const TlacGal* tlac_gal_item(size_t index);
int tlac_tipo_ok(int type);
int tlac_tipo_limpia(int type);
int tlac_combo_count(void);
int tlac_tipo_de_combo(int combo_index);
int tlac_combo_de_tipo(int chart_type);
const char* tlac_combo_label(int combo_index);
const char* tlac_tipo_label(int type);
const char* tlac_eje_dom_label(int domain);
const char* tlac_eje_campo_label(int domain, int field);
void tlac_spec_zero(TlacSpec* spec);
void tlac_spec_limpia(TlacSpec* spec);
int tlac_spec_de_gal(const TlacGal* item, TlacSpec* spec);
size_t tlac_titulo_base_copy(const TlacSpec* spec, char* out, size_t out_cap);
size_t tlac_titulo_copy(const char* title, char* out, size_t out_cap);
size_t tlac_tiempo_compacto_copy(int64_t unix_time, char* out, size_t out_cap);
TlacToc tlac_gal_fila_unica(float width, float height, float text_line_h);
int tlac_lag_unit_clean(int unit);
int tlac_lag_slider_max(int unit);
const char* tlac_lag_unit_label(int unit);
float tlac_oz_legend_height(int disease_count);

typedef struct TlacBoxF {
    float x0;
    float y0;
    float x1;
    float y1;
    float w;
    float h;
} TlacBoxF;

typedef struct TlacSegF {
    float x0;
    float y0;
    float x1;
    float y1;
} TlacSegF;

TlacBoxF tlac_box_xywh(float x, float y, float w, float h);
TlacBoxF tlac_box_xyxy(float x0, float y0, float x1, float y1);
TlacBoxF tlac_tile_icon_box(const TlacBoxF* tile);
void tlac_tile_text_xy(const TlacBoxF* tile, float line_h, float* x, float* y0, float* y1);
int tlac_box_edges(const TlacBoxF* box, TlacSegF out_edges4[4]);

int tlac_px_rect(float x0, float y0, float x1, float y1, float sx, float sy, int fb_w, int fb_h, TlacRectI* out);

typedef struct TlacPlot {
    float x0;
    float y0;
    float x1;
    float y1;
    float w;
    float h;
    float left;
    float right;
    float top;
    float bottom;
    int ok;
} TlacPlot;

TlacPlot tlac_plot_rect(float x0, float y0, float x1, float y1);
float tlac_plot_y_i64(const TlacPlot* p, int64_t value, int64_t max_value);
float tlac_plot_x_index(const TlacPlot* p, int index, int count);
int tlac_hist_bin_count(size_t n);
int tlac_hist_bin_i64(int64_t value, int64_t min_value, int64_t range, int bins);

#define TLAC_LAG_MAX 1825

typedef struct TlacLagScore {
    int lag;
    double score;
} TlacLagScore;

size_t tlac_lag_seed_fill(int max_lag, int* out, size_t out_cap);
int tlac_lag_seen(const unsigned char* seen, size_t seen_n, int lag);
int tlac_lag_mark(unsigned char* seen, size_t seen_n, int lag);
int tlac_lag_next_region(const TlacLagScore* scores, size_t score_n,
                         const unsigned char* seen, size_t seen_n,
                         int* radius_io, int* rank_io, int max_lag);

/* Núcleo atmosférico C: clasificación y semaforización sin contenedores C++ en ruta de cuadro. */
int ozmvm_atm_key_is_meteorological(const char* key, size_t n);
int ozmvm_atm_key_is_contaminant(const char* key, size_t n);
int ozmvm_atm_group_is_meteorological(const char* group, size_t n);
int ozmvm_atm_group_is_contaminant(const char* group, size_t n);
size_t ozmvm_atm_catalog_group_copy(const char* key, size_t n, char* out, size_t out_cap);
size_t ozmvm_atm_sidebar_type_label_copy(const char* key, size_t key_n,
                                          const char* group, size_t group_n,
                                          char* out, size_t out_cap);
double ozmvm_atm_value_for_semaphore(const char* key, size_t key_n,
                                      double value, const char* unit, size_t unit_n);
float ozmvm_atm_meteorological_t(const char* key, size_t key_n, double value);
float ozmvm_atm_meteorological_t_unit(const char* key, size_t key_n,
                                      double value, const char* unit, size_t unit_n);
float ozmvm_atm_measurement_t(const char* key, size_t key_n,
                              double value, const char* unit, size_t unit_n);
double ozmvm_atm_health_ratio(const char* key, size_t key_n,
                               double value, const char* unit, size_t unit_n);
const char* ozmvm_atm_measurement_band_label(float t);

#ifdef __cplusplus
}
#endif
