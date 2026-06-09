




#include "core.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



/* oz_is_space: evalúa un predicado. */
static int oz_is_space(char c) {
    unsigned char u = (unsigned char)c;

    return u == ' ' || u == '\t' || u == '\n' || u == '\r' || u == '\f' || u == '\v';
}


/* oz_is_digit: evalúa un predicado. */
static int oz_is_digit(char c) {

    return c >= '0' && c <= '9';
}



static int oz_key_at(const char* s, size_t n, size_t p, const char* key, size_t key_len) {

    if (p == 0 || p + key_len >= n) return 0;

    return s[p - 1] == '"' && s[p + key_len] == '"' && memcmp(s + p, key, key_len) == 0;
}



/* oz_find_key_value_start: localiza datos. */
static int oz_find_key_value_start(const char* s, size_t n, const char* key, size_t* out) {
    const size_t key_len = key ? strlen(key) : 0;

    if (!s || !key || key_len == 0 || key_len + 3 > n) return 0;

    for (size_t p = 1; p + key_len < n; ++p) {

        if (s[p] != key[0] || !oz_key_at(s, n, p, key, key_len)) continue;
        size_t q = p + key_len + 1;

        while (q < n && oz_is_space(s[q])) ++q;

        if (q >= n || s[q] != ':') continue;

        ++q;

        while (q < n && oz_is_space(s[q])) ++q;
        *out = q;

        return 1;
    }

    return 0;
}




int ozmvm_json_string_span(const char* s, size_t n, const char* key,

                           size_t* out_begin, size_t* out_end, int* out_has_escape) {
    size_t p = 0;

    if (!out_begin || !out_end || !oz_find_key_value_start(s, n, key, &p)) return 0;

    if (p >= n || s[p] != '"') return 0;
    ++p;
    const size_t begin = p;
    int escaped = 0;

    for (; p < n; ++p) {
        const char c = s[p];

        if (c == '"') {
            *out_begin = begin;
            *out_end = p;

            if (out_has_escape) *out_has_escape = escaped;

            return 1;
        }

        if (c == '\\' && p + 1 < n) {
            escaped = 1;
            ++p;
        }
    }

    return 0;
}





int64_t ozmvm_json_i64(const char* s, size_t n, const char* key) {
    size_t p = 0;

    if (!oz_find_key_value_start(s, n, key, &p)) return 0;
    int neg = 0;

    if (p < n && s[p] == '-') {
        neg = 1;
        ++p;
    }
    int64_t v = 0;

    int seen = 0;

    while (p < n && oz_is_digit(s[p])) {
        seen = 1;
        const int d = s[p++] - '0';

        if (v <= (INT64_MAX - d) / 10) v = v * 10 + d;
    }

    if (!seen) return 0;

    return neg ? -v : v;
}




int ozmvm_attr_double_span(const char* s, size_t begin, size_t end, const char* attr, double* out) {

    if (!s || !attr || !out || begin >= end) return 0;

    const size_t attr_len = strlen(attr);

    for (size_t p = begin; p + attr_len + 2 < end; ++p) {

        if (s[p] != attr[0] || memcmp(s + p, attr, attr_len) != 0) continue;
        size_t q = p + attr_len;

        if (q + 1 >= end || s[q] != '=' || s[q + 1] != '"') continue;
        q += 2;

        int neg = 0;

        if (q < end && s[q] == '-') {
            neg = 1;
            ++q;
        }

        double value = 0.0;
        int seen = 0;

        while (q < end && oz_is_digit(s[q])) {
            seen = 1;
            value = value * 10.0 + (double)(s[q++] - '0');
        }

        if (q < end && s[q] == '.') {
            ++q;
            double base = 0.1;

            while (q < end && oz_is_digit(s[q])) {
                seen = 1;

                value += (double)(s[q++] - '0') * base;
                base *= 0.1;
            }
        }

        if (!seen) return 0;
        *out = neg ? -value : value;

        return 1;
    }

    return 0;
}



int ozmvm_first_year_20xx(const char* s, size_t n) {

    if (!s || n < 4) return 0;

    for (size_t i = 0; i + 3 < n; ++i) {


        if (s[i] == '2' && s[i + 1] == '0' && oz_is_digit(s[i + 2]) && oz_is_digit(s[i + 3])) {

            return 2000 + (s[i + 2] - '0') * 10 + (s[i + 3] - '0');
        }
    }

    return 0;
}



int ozmvm_week_after_marker(const char* s, size_t n, const char* marker) {

    if (!s || !marker || !*marker) return 0;
    const size_t ml = strlen(marker);

    if (n < ml + 1) return 0;

    for (size_t p = 0; p + ml < n; ++p) {

        if (s[p] != marker[0] || memcmp(s + p, marker, ml) != 0) continue;
        size_t i = p + ml;

        while (i < n && oz_is_space(s[i])) ++i;
        int v = 0, digits = 0;

        while (i < n && oz_is_digit(s[i]) && digits < 2) {
            v = v * 10 + (s[i++] - '0');
            ++digits;
        }

        if (digits > 0 && v >= 1 && v <= 53) return v;
    }

    return 0;
}




int ozmvm_week_before_del_year(const char* s, size_t n) {

    if (!s) return 0;

    for (size_t i = 0; i < n; ++i) {

        if (!oz_is_digit(s[i])) continue;
        int v = 0, digits = 0;

        size_t j = i;

        while (j < n && oz_is_digit(s[j]) && digits < 2) {
            v = v * 10 + (s[j++] - '0');
            ++digits;
        }

        if (digits == 0 || v < 1 || v > 53) {
            i = j;
            continue;
        }

        while (j < n && oz_is_space(s[j])) ++j;

        if (j + 7 <= n && memcmp(s + j, "del ", 4) == 0) j += 4;

        else if (j + 6 <= n && memcmp(s + j, "de ", 3) == 0) j += 3;
        else {
            i = j;
            continue;
        }

        if (j + 4 <= n && ozmvm_first_year_20xx(s + j, 4) > 0) return v;
        i = j;
    }

    return 0;
}

static int oz_parse_u_limited(const char* s, size_t n, size_t* pos, int max_digits, int* out) {
    size_t i = pos ? *pos : 0u;
    int v = 0;
    int digits = 0;
    if (!s || !pos || !out || max_digits <= 0) return 0;
    while (i < n && oz_is_digit(s[i]) && digits < max_digits) {
        v = v * 10 + (s[i] - '0');
        ++i;
        ++digits;
    }
    if (digits == 0) return 0;
    if (i < n && oz_is_digit(s[i])) return 0;
    *pos = i;
    *out = v;
    return 1;
}

static void oz_skip_space_span(const char* s, size_t n, size_t* pos) {
    if (!s || !pos) return;
    while (*pos < n && oz_is_space(s[*pos])) ++(*pos);
}

int ozmvm_parse_numeric_date_time_components(const char* s, size_t n, int out_parts5[5]) {
    size_t i = 0;
    int a = 0, b = 0, c = 0, h = -1, m = -1;
    int saw_tail_space = 0;
    if (!s || !out_parts5) return 0;
    oz_skip_space_span(s, n, &i);
    if (!oz_parse_u_limited(s, n, &i, 4, &a)) return 0;
    oz_skip_space_span(s, n, &i);
    if (i >= n || (s[i] != '-' && s[i] != '/')) return 0;
    ++i;
    oz_skip_space_span(s, n, &i);
    if (!oz_parse_u_limited(s, n, &i, 2, &b)) return 0;
    oz_skip_space_span(s, n, &i);
    if (i >= n || (s[i] != '-' && s[i] != '/')) return 0;
    ++i;
    oz_skip_space_span(s, n, &i);
    if (!oz_parse_u_limited(s, n, &i, 4, &c)) return 0;
    while (i < n && oz_is_space(s[i])) {
        saw_tail_space = 1;
        ++i;
    }
    if (i < n) {
        if (!saw_tail_space) return 0;
        if (!oz_parse_u_limited(s, n, &i, 2, &h)) return 0;
        oz_skip_space_span(s, n, &i);
        if (i < n && s[i] == ':') {
            ++i;
            oz_skip_space_span(s, n, &i);
            if (!oz_parse_u_limited(s, n, &i, 2, &m)) return 0;
            oz_skip_space_span(s, n, &i);
        }
        if (i != n) return 0;
    }
    out_parts5[0] = a;
    out_parts5[1] = b;
    out_parts5[2] = c;
    out_parts5[3] = h;
    out_parts5[4] = m;
    return 1;
}

int ozmvm_first_epi_year_1900_2099(const char* s, size_t n) {
    if (!s || n < 4) return 0;
    for (size_t i = 0; i + 3u < n; ++i) {
        const int prev_digit = i > 0 && oz_is_digit(s[i - 1u]);
        const int next_digit = i + 4u < n && oz_is_digit(s[i + 4u]);
        if (prev_digit || next_digit) continue;
        if ((s[i] == '1' && s[i + 1u] == '9') || (s[i] == '2' && s[i + 1u] == '0')) {
            if (oz_is_digit(s[i + 2u]) && oz_is_digit(s[i + 3u])) {
                return (s[i] - '0') * 1000 + (s[i + 1u] - '0') * 100 +
                       (s[i + 2u] - '0') * 10 + (s[i + 3u] - '0');
            }
        }
    }
    return 0;
}

typedef struct OzWeekWord {
    const char* text;
    int value;
} OzWeekWord;

static int oz_week_phrase_compact_eq(const char* s, size_t n, const char* word) {
    size_t i = 0;
    size_t j = 0;
    if (!s || !word) return 0;
    while (i < n && oz_is_space(s[i])) ++i;
    while (word[j] == ' ') ++j;
    for (;;) {
        while (i < n && oz_is_space(s[i])) ++i;
        while (word[j] == ' ') ++j;
        if (i >= n || word[j] == '\0') break;
        if (s[i] != word[j]) return 0;
        ++i;
        ++j;
    }
    while (i < n && oz_is_space(s[i])) ++i;
    while (word[j] == ' ') ++j;
    return i == n && word[j] == '\0';
}

int ozmvm_spanish_epi_week_words_to_int(const char* s, size_t n) {
    static const OzWeekWord words[] = {
        {"uno",1},{"una",1},{"primer",1},{"primera",1},{"dos",2},{"segundo",2},{"segunda",2},
        {"tres",3},{"tercero",3},{"tercera",3},{"cuatro",4},{"cinco",5},{"seis",6},{"siete",7},
        {"ocho",8},{"nueve",9},{"diez",10},{"once",11},{"doce",12},{"trece",13},{"catorce",14},
        {"quince",15},{"dieciseis",16},{"diecisiete",17},{"dieciocho",18},{"diecinueve",19},
        {"veinte",20},{"veintiuno",21},{"veintiuna",21},{"veintidos",22},{"veintitres",23},
        {"veinticuatro",24},{"veinticinco",25},{"veintiseis",26},{"veintisiete",27},
        {"veintiocho",28},{"veintinueve",29},{"treinta",30},{"treinta y uno",31},{"treinta y una",31},
        {"treinta y dos",32},{"treinta y tres",33},{"treinta y cuatro",34},{"treinta y cinco",35},
        {"treinta y seis",36},{"treinta y siete",37},{"treinta y ocho",38},{"treinta y nueve",39},
        {"cuarenta",40},{"cuarenta y uno",41},{"cuarenta y una",41},{"cuarenta y dos",42},
        {"cuarenta y tres",43},{"cuarenta y cuatro",44},{"cuarenta y cinco",45},{"cuarenta y seis",46},
        {"cuarenta y siete",47},{"cuarenta y ocho",48},{"cuarenta y nueve",49},{"cincuenta",50},
        {"cincuenta y uno",51},{"cincuenta y una",51},{"cincuenta y dos",52},{"cincuenta y tres",53}
    };
    if (!s) return 0;
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (oz_week_phrase_compact_eq(s, n, words[i].text)) return words[i].value;
    }
    return 0;
}

typedef struct OzSpan {
    size_t b;
    size_t e;
} OzSpan;

static int oz_is_alnum_ascii(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int oz_tokenize_ascii(const char* s, size_t n, OzSpan* out, size_t out_cap) {
    size_t count = 0;
    size_t i = 0;
    if (!s || !out || out_cap == 0) return 0;
    while (i < n) {
        while (i < n && !oz_is_alnum_ascii((unsigned char)s[i])) ++i;
        if (i >= n) break;
        const size_t b = i;
        while (i < n && oz_is_alnum_ascii((unsigned char)s[i])) ++i;
        if (count < out_cap) {
            out[count].b = b;
            out[count].e = i;
        }
        ++count;
    }
    return count > out_cap ? (int)out_cap : (int)count;
}

static int oz_span_eq_lit(const char* s, OzSpan t, const char* lit) {
    const size_t n = t.e - t.b;
    return lit && strlen(lit) == n && memcmp(s + t.b, lit, n) == 0;
}

static int oz_span_has_prefix(const char* s, OzSpan t, const char* lit, size_t* suffix_b) {
    const size_t n = t.e - t.b;
    const size_t m = lit ? strlen(lit) : 0u;
    if (m == 0 || n <= m || memcmp(s + t.b, lit, m) != 0) return 0;
    if (suffix_b) *suffix_b = t.b + m;
    return 1;
}

static int oz_week_number_span(const char* s, size_t b, size_t e) {
    int v = 0;
    int significant = 0;
    if (!s || b >= e) return 0;
    for (size_t i = b; i < e; ++i) {
        if (!oz_is_digit(s[i])) return 0;
        if (s[i] == '0' && significant == 0) continue;
        ++significant;
        if (significant > 2) return 0;
        v = v * 10 + (s[i] - '0');
    }
    return v >= 1 && v <= 53 ? v : 0;
}

static int oz_is_epi_semana_marker(const char* s, OzSpan t) {
    return oz_span_eq_lit(s, t, "se") || oz_span_eq_lit(s, t, "sem") || oz_span_eq_lit(s, t, "semana");
}

static int oz_is_epi_number_marker(const char* s, OzSpan t) {
    return oz_span_eq_lit(s, t, "numero") || oz_span_eq_lit(s, t, "num") ||
           oz_span_eq_lit(s, t, "no") || oz_span_eq_lit(s, t, "nro") || oz_span_eq_lit(s, t, "n");
}

static int oz_is_epi_number_marker_after_semana(const char* s, OzSpan t) {
    return oz_span_eq_lit(s, t, "numero") || oz_span_eq_lit(s, t, "num") ||
           oz_span_eq_lit(s, t, "no") || oz_span_eq_lit(s, t, "nro");
}

static int oz_week_words_from_tokens(const char* s, const OzSpan* tok, int nt, int start) {
    if (!s || !tok || start < 0 || start >= nt) return 0;
    for (int len = 1; len <= 4 && start + len <= nt; ++len) {
        const size_t b = tok[start].b;
        const size_t e = tok[start + len - 1].e;
        const int v = ozmvm_spanish_epi_week_words_to_int(s + b, e - b);
        if (v >= 1 && v <= 53) return v;
    }
    return 0;
}

static int oz_compact_semana_week(const char* s, OzSpan t) {
    size_t suffix = 0;
    int v = 0;
    if (oz_span_has_prefix(s, t, "semana", &suffix)) {
        v = oz_week_number_span(s, suffix, t.e);
        if (v) return v;
    }
    if (oz_span_has_prefix(s, t, "sem", &suffix)) {
        v = oz_week_number_span(s, suffix, t.e);
        if (v) return v;
    }
    if (oz_span_has_prefix(s, t, "se", &suffix)) {
        v = oz_week_number_span(s, suffix, t.e);
        if (v) return v;
    }
    return 0;
}

int ozmvm_epi_week_contextual(const char* s, size_t n) {
    OzSpan tok[96];
    const int nt = oz_tokenize_ascii(s, n, tok, sizeof(tok) / sizeof(tok[0]));
    if (!s || nt <= 0) return 0;
    for (int i = 0; i < nt; ++i) {
        int v = oz_compact_semana_week(s, tok[i]);
        if (v) return v;
        if (oz_is_epi_semana_marker(s, tok[i])) {
            int start = i + 1;
            if (start < nt && oz_span_eq_lit(s, tok[start], "epidemiologica")) ++start;
            if (start < nt && oz_is_epi_number_marker_after_semana(s, tok[start])) ++start;
            if (start < nt) {
                v = oz_week_number_span(s, tok[start].b, tok[start].e);
                if (v) return v;
                v = oz_week_words_from_tokens(s, tok, nt, start);
                if (v) return v;
            }
            continue;
        }
        if (oz_is_epi_number_marker(s, tok[i])) {
            const int start = i + 1;
            if (start < nt) {
                v = oz_week_number_span(s, tok[start].b, tok[start].e);
                if (v) return v;
                v = oz_week_words_from_tokens(s, tok, nt, start);
                if (v) return v;
            }
            continue;
        }
        if (oz_span_eq_lit(s, tok[i], "boletin") || oz_span_eq_lit(s, tok[i], "boletines")) {
            int start = i + 1;
            if (start < nt && (oz_span_eq_lit(s, tok[start], "semanal") || oz_span_eq_lit(s, tok[start], "semanales"))) ++start;
            if (start < nt) {
                v = oz_week_number_span(s, tok[start].b, tok[start].e);
                if (v) return v;
            }
        }
    }
    for (int i = 0; i < nt; ++i) {
        const int v = oz_week_number_span(s, tok[i].b, tok[i].e);
        if (v) return v;
    }
    return 0;
}



/* oz_trim_span: sanea valores. */
static void oz_trim_span(const char* s, size_t* b, size_t* e) {

    while (*b < *e && oz_is_space(s[*b])) ++(*b);

    while (*e > *b && oz_is_space(s[*e - 1])) --(*e);
}



/* oz_parse_i32_span: decodifica entrada. */
static int oz_parse_i32_span(const char* s, size_t b, size_t e, int* out) {

    if (!out) return 0;

    oz_trim_span(s, &b, &e);


    if (b >= e) return 0;
    int neg = 0;

    if (s[b] == '-') {
        neg = 1;
        ++b;
    }
    int v = 0;
    int seen = 0;

    for (size_t p = b; p < e; ++p) {

        if (!oz_is_digit(s[p])) return 0;
        seen = 1;
        const int d = s[p] - '0';

        if (v > (INT32_MAX - d) / 10) return 0;
        v = v * 10 + d;
    }

    if (!seen) return 0;
    *out = neg ? -v : v;

    return 1;
}



/* oz_parse_i64_span: decodifica entrada. */
static int oz_parse_i64_span(const char* s, size_t b, size_t e, int64_t* out) {

    if (!out) return 0;


    oz_trim_span(s, &b, &e);

    if (b >= e) return 0;
    int neg = 0;

    if (s[b] == '-') {
        neg = 1;

        ++b;
    }
    int64_t v = 0;
    int seen = 0;

    for (size_t p = b; p < e; ++p) {

        if (!oz_is_digit(s[p])) return 0;

        seen = 1;
        const int d = s[p] - '0';

        if (v > (INT64_MAX - d) / 10) return 0;
        v = v * 10 + d;
    }

    if (!seen) return 0;
    *out = neg ? -v : v;

    return 1;
}




static void oz_set_span_trimmed(const char* s, size_t b, size_t e, size_t* out_b, size_t* out_e) {
    oz_trim_span(s, &b, &e);
    *out_b = b;
    *out_e = e;
}




int ozmvm_tsv_epi_parse10(const char* s, size_t n, OzmvmTsvEpiFields* out) {

    if (!s || !out) return 0;
    memset(out, 0, sizeof(*out));

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) --n;

    size_t b[10] = {0};
    size_t e[10] = {0};

    size_t field = 0;
    size_t start = 0;

    for (size_t p = 0; p <= n; ++p) {


        if (p == n || s[p] == '\t') {

            if (field >= 10) return 0;

            b[field] = start;

            e[field] = p;

            ++field;
            start = p + 1;
        }
    }

    if (field != 10) return 0;


    oz_set_span_trimmed(s, b[0], e[0], &out->entity_begin, &out->entity_end);


    if (!oz_parse_i32_span(s, b[1], e[1], &out->year)) return 0;


    if (!oz_parse_i32_span(s, b[2], e[2], &out->epi_week)) return 0;


    if (!oz_parse_i32_span(s, b[3], e[3], &out->page)) out->page = 0;


    oz_set_span_trimmed(s, b[4], e[4], &out->disease_begin, &out->disease_end);
    oz_set_span_trimmed(s, b[5], e[5], &out->cie10_begin, &out->cie10_end);
    oz_set_span_trimmed(s, b[6], e[6], &out->jurisdiction_begin, &out->jurisdiction_end);

    oz_set_span_trimmed(s, b[7], e[7], &out->period_begin, &out->period_end);
    oz_set_span_trimmed(s, b[8], e[8], &out->sex_begin, &out->sex_end);


    if (!oz_parse_i64_span(s, b[9], e[9], &out->value)) out->value = 0;

    return out->year > 0 && out->epi_week >= 1 && out->epi_week <= 53 &&
           out->disease_begin < out->disease_end &&
           out->jurisdiction_begin < out->jurisdiction_end;
}



static double oz_nan(void) {

    volatile double z = 0.0;


    return z / z;
}



static double oz_clamp01(double v) {

    if (v < 0.0) return 0.0;

    if (v > 1.0) return 1.0;

    return v;
}



static int oz_double_cmp(const void* a, const void* b) {
    const double da = *(const double*)a;
    const double db = *(const double*)b;

    return (da > db) - (da < db);
}


/* oz_read_i64_field: decodifica entrada. */
static int64_t oz_read_i64_field(const void* base, size_t i, size_t stride, size_t offset) {
    int64_t v = 0;
    memcpy(&v, (const unsigned char*)base + i * stride + offset, sizeof(v));

    return v;
}


/* oz_read_double_field: decodifica entrada. */
static double oz_read_double_field(const void* base, size_t i, size_t stride, size_t offset) {
    double v = 0.0;
    memcpy(&v, (const unsigned char*)base + i * stride + offset, sizeof(v));

    return v;
}



static double oz_median_sorted(const double* values, size_t n) {

    if (!values || n == 0) return oz_nan();
    const size_t mid = n / 2;

    return (n & 1u) ? values[mid] : (values[mid - 1] + values[mid]) * 0.5;
}

int ozmvm_i64_stats_strided(const void* base, size_t n, size_t stride,


                            size_t value_offset, OzmvmI64Stats* out) {

    if (!out) return 0;
    memset(out, 0, sizeof(*out));

    if (!base || n == 0 || stride == 0) return 0;

    int64_t min_v = oz_read_i64_field(base, 0, stride, value_offset);
    int64_t max_v = min_v;
    int64_t sum = 0;

    for (size_t i = 0; i < n; ++i) {

        const int64_t v = oz_read_i64_field(base, i, stride, value_offset);

        if (v < min_v) min_v = v;

        if (v > max_v) max_v = v;
        sum += v;
    }
    const double mean = (double)sum / (double)n;
    out->n = n;

    out->sum = sum;
    out->min_value = min_v;
    out->max_value = max_v;
    out->mean = mean;

    if (n >= 2) {
        double ss = 0.0;

        for (size_t i = 0; i < n; ++i) {

            const double d = (double)oz_read_i64_field(base, i, stride, value_offset) - mean;

            ss += d * d;
        }
        out->sd = sqrt(ss / (double)(n - 1));

        if (fabs(mean) > 1.0e-9) out->cv = out->sd / fabs(mean);
    }

    return 1;
}

int ozmvm_indexed_i64_ols_strided(const void* base, size_t n, size_t stride,

                                  size_t value_offset, double* out_slope, double* out_intercept) {

    if (out_slope) *out_slope = oz_nan();

    if (out_intercept) *out_intercept = oz_nan();

    if (!base || n < 2 || stride == 0 || !out_slope || !out_intercept) return 0;
    double sx = 0.0;
    double sy = 0.0;

    for (size_t i = 0; i < n; ++i) {
        sx += (double)i;

        sy += (double)oz_read_i64_field(base, i, stride, value_offset);
    }
    const double mx = sx / (double)n;
    const double my = sy / (double)n;
    double num = 0.0;
    double dx2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double dx = (double)i - mx;

        num += dx * ((double)oz_read_i64_field(base, i, stride, value_offset) - my);

        dx2 += dx * dx;
    }


    if (dx2 <= 0.0) return 0;
    *out_slope = num / dx2;
    *out_intercept = my - (*out_slope) * mx;

    return 1;
}

double ozmvm_indexed_i64_theil_sen_strided(const void* base, size_t n, size_t stride,

                                           size_t value_offset, size_t cap, double* out_intercept) {

    if (out_intercept) *out_intercept = oz_nan();

    if (!base || n < 2 || stride == 0) return oz_nan();

    if (cap < 2 || cap > n) cap = n;
    const size_t sample_n = cap;

    double* xs = (double*)malloc(sample_n * sizeof(double));
    double* ys = (double*)malloc(sample_n * sizeof(double));

    if (!xs || !ys) {
        free(xs);
        free(ys);

        return oz_nan();
    }
    const double step = (double)n / (double)sample_n;

    for (size_t i = 0; i < sample_n; ++i) {
        size_t src = n <= sample_n ? i : (size_t)((double)i * step);

        if (src >= n) src = n - 1;

        xs[i] = (double)src;

        ys[i] = (double)oz_read_i64_field(base, src, stride, value_offset);
    }
    const size_t max_slopes = (sample_n * (sample_n - 1)) / 2;
    double* slopes = (double*)malloc(max_slopes * sizeof(double));


    if (!slopes) {
        free(xs);
        free(ys);

        return oz_nan();
    }
    size_t slope_n = 0;

    for (size_t i = 0; i < sample_n; ++i) {

        for (size_t j = i + 1; j < sample_n; ++j) {
            const double dx = xs[j] - xs[i];

            if (fabs(dx) < 1.0e-12) continue;
            slopes[slope_n++] = (ys[j] - ys[i]) / dx;
        }
    }

    if (slope_n == 0) {
        free(slopes);
        free(xs);
        free(ys);

        return oz_nan();
    }
    qsort(slopes, slope_n, sizeof(double), oz_double_cmp);
    const double slope = oz_median_sorted(slopes, slope_n);

    if (out_intercept && isfinite(slope)) {
        double* intercepts = (double*)malloc(sample_n * sizeof(double));

        if (intercepts) {

            for (size_t i = 0; i < sample_n; ++i) intercepts[i] = ys[i] - slope * xs[i];
            qsort(intercepts, sample_n, sizeof(double), oz_double_cmp);
            *out_intercept = oz_median_sorted(intercepts, sample_n);

            free(intercepts);
        }
    }
    free(slopes);
    free(xs);
    free(ys);

    return slope;
}

double ozmvm_xy_pearson_strided(const void* base, size_t n, size_t stride,

                                size_t x_offset, size_t y_offset) {

    if (!base || n < 2 || stride == 0) return oz_nan();
    double sx = 0.0;
    double sy = 0.0;

    for (size_t i = 0; i < n; ++i) {

        sx += oz_read_double_field(base, i, stride, x_offset);

        sy += oz_read_double_field(base, i, stride, y_offset);
    }
    const double mx = sx / (double)n;
    const double my = sy / (double)n;
    double num = 0.0;
    double dx2 = 0.0;
    double dy2 = 0.0;

    for (size_t i = 0; i < n; ++i) {

        const double dx = oz_read_double_field(base, i, stride, x_offset) - mx;

        const double dy = oz_read_double_field(base, i, stride, y_offset) - my;
        num += dx * dy;

        dx2 += dx * dx;
        dy2 += dy * dy;
    }

    if (dx2 <= 0.0 || dy2 <= 0.0) return oz_nan();

    return num / sqrt(dx2 * dy2);
}

int ozmvm_xy_ols_strided(const void* base, size_t n, size_t stride,
                         size_t x_offset, size_t y_offset, const double* weights,

                         double* out_slope, double* out_intercept) {

    if (out_slope) *out_slope = oz_nan();

    if (out_intercept) *out_intercept = oz_nan();

    if (!base || n < 2 || stride == 0 || !out_slope || !out_intercept) return 0;
    double sw = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double w = weights ? weights[i] : 1.0;

        if (!(w > 0.0) || !isfinite(w)) continue;

        const double x = oz_read_double_field(base, i, stride, x_offset);

        const double y = oz_read_double_field(base, i, stride, y_offset);
        sw += w;
        sx += w * x;

        sy += w * y;
        sxx += w * x * x;
        sxy += w * x * y;
    }
    const double denom = sw * sxx - sx * sx;

    if (sw <= 0.0 || fabs(denom) < 1.0e-12) return 0;
    *out_slope = (sw * sxy - sx * sy) / denom;
    *out_intercept = (sy - (*out_slope) * sx) / sw;

    return 1;
}

int ozmvm_xy_fit_metrics_strided(const void* base, size_t n, size_t stride,
                                 size_t x_offset, size_t y_offset,

                                 double slope, double intercept, OzmvmLinearFitD* out) {

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->slope = slope;
    out->intercept = intercept;
    out->index = oz_nan();
    out->rmse = oz_nan();
    out->mae = oz_nan();

    out->pearson = oz_nan();
    out->r2 = oz_nan();

    if (!base || n < 2 || stride == 0 || !isfinite(slope) || !isfinite(intercept)) return 0;
    double sx = 0.0;
    double sy = 0.0;

    for (size_t i = 0; i < n; ++i) {


        sx += oz_read_double_field(base, i, stride, x_offset);

        sy += oz_read_double_field(base, i, stride, y_offset);
    }
    const double mx = sx / (double)n;
    const double my = sy / (double)n;

    double se = 0.0;
    double ae = 0.0;
    double rss = 0.0;
    double tss = 0.0;

    for (size_t i = 0; i < n; ++i) {

        const double x = oz_read_double_field(base, i, stride, x_offset);

        const double y = oz_read_double_field(base, i, stride, y_offset);
        const double pred = intercept + slope * x;
        const double err = y - pred;
        const double dy = y - my;
        se += err * err;

        ae += fabs(err);
        rss += err * err;
        tss += dy * dy;
    }
    out->mean_x = mx;
    out->mean_y = my;
    out->rmse = sqrt(se / (double)n);
    out->mae = ae / (double)n;

    out->pearson = ozmvm_xy_pearson_strided(base, n, stride, x_offset, y_offset);

    if (tss > 1.0e-12) {
        const double pseudo_r2 = 1.0 - rss / tss;

        out->r2 = pseudo_r2;
        out->index = (slope < 0.0 ? -1.0 : 1.0) * oz_clamp01(fabs(pseudo_r2));
    } else {
        double sp = 0.0;

        for (size_t i = 0; i < n; ++i) {

            const double x = oz_read_double_field(base, i, stride, x_offset);
            sp += intercept + slope * x;
        }
        const double mp = sp / (double)n;
        double num = 0.0;
        double pp2 = 0.0;

        double yy2 = 0.0;

        for (size_t i = 0; i < n; ++i) {

            const double x = oz_read_double_field(base, i, stride, x_offset);

            const double y = oz_read_double_field(base, i, stride, y_offset);
            const double dp = intercept + slope * x - mp;
            const double dy = y - my;
            num += dp * dy;
            pp2 += dp * dp;
            yy2 += dy * dy;
        }


        if (pp2 > 0.0 && yy2 > 0.0) out->index = num / sqrt(pp2 * yy2);
    }
    out->valid = 1;

    return 1;
}



static double oz_mean_range(const double* values, size_t first, size_t last) {

    if (!values || first >= last) return oz_nan();
    double s = 0.0;

    for (size_t i = first; i < last; ++i) s += values[i];

    return s / (double)(last - first);
}



double ozmvm_aggregate_double_values(double* values, size_t n, int mode) {

    if (!values || n == 0) return oz_nan();
    size_t m = 0;

    for (size_t i = 0; i < n; ++i) {

        if (isfinite(values[i])) values[m++] = values[i];
    }

    if (m == 0) return oz_nan();
    qsort(values, m, sizeof(double), oz_double_cmp);

    if (mode < 0) mode = 0;

    if (mode > 5) mode = 5;
    switch (mode) {
        case 1:

            return oz_median_sorted(values, m);
        case 2: {

            if (m <= 4) return oz_mean_range(values, 0, m);

            const size_t trim = m / 5 > 1 ? m / 5 : 1;


            return oz_mean_range(values, trim, m - trim);
        }
        case 3:

            return values[m - 1];
        case 4: {
            const size_t idx = (size_t)floor(0.90 * (double)(m - 1));

            return values[idx < m ? idx : m - 1];
        }
        case 5: {
            int best_bin = (int)lround(values[0]);
            int best_n = 0;
            int cur_bin = best_bin;
            int cur_n = 0;

            for (size_t i = 0; i < m; ++i) {
                const int bin = (int)lround(values[i]);

                if (bin == cur_bin) {
                    ++cur_n;
                } else {

                    if (cur_n > best_n) {
                        best_n = cur_n;
                        best_bin = cur_bin;
                    }
                    cur_bin = bin;
                    cur_n = 1;
                }
            }


            if (cur_n > best_n) best_bin = cur_bin;

            return (double)best_bin;
        }
        default:

            return oz_mean_range(values, 0, m);
    }
}



static size_t oz_cstr_len_cap(const char* s, size_t n) {

    if (!s) return 0;
    size_t len = 0;

    while (len < n && s[len] != '\0') ++len;

    return len;
}



static int oz_suffix_token_line(const char* s, size_t begin, size_t end) {

    if (!s || begin >= end) return 0;

    while (begin < end && s[begin] == ' ') ++begin;

    if (begin >= end) return 0;

    if (end - begin == 1) {
        const char c = s[begin];

        return (c >= '1' && c <= '9') || c == 'a' || c == 'b';
    }

    if (end - begin == 2 && s[begin] == '1' && s[begin + 1] == '2') return 1;

    return 0;
}



static int oz_remove_suffix_literal(char* s, size_t* len, const char* suffix) {

    const size_t slen = suffix ? strlen(suffix) : 0;

    if (!s || !len || slen == 0 || *len < slen) return 0;

    if (memcmp(s + *len - slen, suffix, slen) != 0) return 0;
    *len -= slen;


    while (*len > 0 && s[*len - 1] == ' ') --(*len);
    s[*len] = '\0';

    return 1;
}



static int oz_remove_suffix_line(char* s, size_t* len, const char* marker) {
    const size_t mlen = marker ? strlen(marker) : 0;

    if (!s || !len || mlen == 0 || *len <= mlen) return 0;
    size_t p = *len;

    while (p > 0 && s[p - 1] != ' ') --p;

    if (!oz_suffix_token_line(s, p, *len)) return 0;

    if (p < mlen || memcmp(s + p - mlen, marker, mlen) != 0) return 0;
    *len = p - mlen;

    while (*len > 0 && s[*len - 1] == ' ') --(*len);
    s[*len] = '\0';

    return 1;
}



size_t ozmvm_mobility_station_icon_key_inplace(char* s, size_t n) {

    if (!s || n == 0) return 0;
    size_t len = oz_cstr_len_cap(s, n);

    while (len > 0 && s[len - 1] == ' ') s[--len] = '\0';

    if (len >= 6 && memcmp(s, "metro ", 6) == 0) {
        memmove(s, s + 6, len - 5);
        len -= 6;
    }

    while (len > 0 && s[len - 1] == ' ') s[--len] = '\0';
    oz_remove_suffix_literal(s, &len, " metro");

    oz_remove_suffix_literal(s, &len, " estacion");
    oz_remove_suffix_line(s, &len, " correspondencia linea ");
    oz_remove_suffix_line(s, &len, " linea ");

    return len;
}



int ozmvm_lag_to_days(int value, int unit) {

    if (value < 0) value = 0;
    switch (unit) {
        case 1: return value > 260 ? 1820 : value * 7;
        case 2: return value > 60 ? 1800 : value * 30;
        case 3: return value > 5 ? 1825 : value * 365;
        default: return value > 1825 ? 1825 : value;
    }
}



int ozmvm_lag_value_from_days(int days, int unit) {

    if (days < 0) days = 0;

    if (days > 1825) days = 1825;
    switch (unit) {
        case 1: return (days + 3) / 7;
        case 2: return (days + 15) / 30;
        case 3: return (days + 182) / 365;
        default: return days;
    }
}

size_t ozmvm_trim_ascii_span(const char* s, size_t n, size_t* out_begin, size_t* out_end) {
    size_t b = 0;
    size_t e = n;
    if (!s) {
        if (out_begin) *out_begin = 0;
        if (out_end) *out_end = 0;
        return 0;
    }
    while (b < e && oz_is_space(s[b])) ++b;
    while (e > b && oz_is_space(s[e - 1])) --e;
    if (out_begin) *out_begin = b;
    if (out_end) *out_end = e;
    return e - b;
}

static void oz_out_byte(char* out, size_t out_cap, size_t* used, unsigned char c) {
    if (out && *used + 1 < out_cap) out[*used] = (char)c;
    ++(*used);
}

static void oz_out_zero(char* out, size_t out_cap, size_t used) {
    if (!out || out_cap == 0) return;
    out[used < out_cap ? used : out_cap - 1] = '\0';
}

size_t ozmvm_lower_ascii_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        oz_out_byte(out, out_cap, &used, c);
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

size_t ozmvm_clean_user_path_copy(const char* s, size_t n, char* out, size_t out_cap, int windows_paths) {
    size_t b = 0;
    size_t e = 0;
    size_t used = 0;
    static const char prefix[] = "file:///";
    const size_t prefix_n = sizeof(prefix) - 1;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    ozmvm_trim_ascii_span(s, n, &b, &e);
    if (e >= b + 2 && ((s[b] == '"' && s[e - 1] == '"') || (s[b] == '\'' && s[e - 1] == '\''))) {
        ++b;
        --e;
        while (b < e && oz_is_space(s[b])) ++b;
        while (e > b && oz_is_space(s[e - 1])) --e;
    }
    if (e >= b + prefix_n && memcmp(s + b, prefix, prefix_n) == 0) {
        b += prefix_n;
        if (windows_paths && e >= b + 3 && s[b] == '/' &&
            ((s[b + 1] >= 'A' && s[b + 1] <= 'Z') || (s[b + 1] >= 'a' && s[b + 1] <= 'z')) &&
            s[b + 2] == ':') {
            ++b;
        }
    }
    while (e > b && oz_is_space(s[e - 1])) --e;
    for (size_t i = b; i < e; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (windows_paths && c == '/') c = '\\';
        oz_out_byte(out, out_cap, &used, c);
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

static void oz_append_utf8_code(char* out, size_t out_cap, size_t* used, int code) {
    if (code < 0) return;
    if (code <= 0x7f) {
        oz_out_byte(out, out_cap, used, (unsigned char)code);
    } else if (code <= 0x7ff) {
        oz_out_byte(out, out_cap, used, (unsigned char)(0xc0 | ((code >> 6) & 0x1f)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | (code & 0x3f)));
    } else if (code <= 0xffff) {
        oz_out_byte(out, out_cap, used, (unsigned char)(0xe0 | ((code >> 12) & 0x0f)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | ((code >> 6) & 0x3f)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | (code & 0x3f)));
    } else if (code <= 0x10ffff) {
        oz_out_byte(out, out_cap, used, (unsigned char)(0xf0 | ((code >> 18) & 0x07)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | ((code >> 12) & 0x3f)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | ((code >> 6) & 0x3f)));
        oz_out_byte(out, out_cap, used, (unsigned char)(0x80 | (code & 0x3f)));
    }
}

static int oz_entity_named(const char* s, size_t n, size_t i, const char* name, char value, size_t* consumed) {
    const size_t m = strlen(name);
    if (i + m <= n && memcmp(s + i, name, m) == 0) {
        *consumed = m;
        return (unsigned char)value;
    }
    return -1;
}

static int oz_html_entity_at(const char* s, size_t n, size_t i, size_t* consumed) {
    int v = -1;
    if (i >= n || s[i] != '&') return -1;
    v = oz_entity_named(s, n, i, "&amp;", '&', consumed); if (v >= 0) return v;
    v = oz_entity_named(s, n, i, "&lt;", '<', consumed); if (v >= 0) return v;
    v = oz_entity_named(s, n, i, "&gt;", '>', consumed); if (v >= 0) return v;
    v = oz_entity_named(s, n, i, "&quot;", '"', consumed); if (v >= 0) return v;
    v = oz_entity_named(s, n, i, "&#39;", '\'', consumed); if (v >= 0) return v;
    v = oz_entity_named(s, n, i, "&apos;", '\'', consumed); if (v >= 0) return v;
    if (i + 3 < n && s[i + 1] == '#') {
        size_t j = i + 2;
        int code = 0;
        int digit = 0;
        while (j < n && oz_is_digit(s[j])) {
            digit = 1;
            if (code <= (0x10ffff - (s[j] - '0')) / 10) code = code * 10 + (s[j] - '0');
            ++j;
        }
        if (digit && j < n && s[j] == ';') {
            *consumed = j - i + 1;
            return code;
        }
    }
    return -1;
}

size_t ozmvm_html_unescape_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        size_t consumed = 0;
        const int entity = oz_html_entity_at(s, n, i, &consumed);
        if (entity >= 0) {
            oz_append_utf8_code(out, out_cap, &used, entity);
            i += consumed - 1;
        } else {
            oz_out_byte(out, out_cap, &used, (unsigned char)s[i]);
        }
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

static int oz_codepoint_accent_ascii(int code, char* ascii) {
    switch (code) {
        case 0x00e1: case 0x00e0: *ascii = 'a'; return 1;
        case 0x00c1: *ascii = 'A'; return 1;
        case 0x00e9: case 0x00e8: *ascii = 'e'; return 1;
        case 0x00c9: *ascii = 'E'; return 1;
        case 0x00ed: case 0x00ec: *ascii = 'i'; return 1;
        case 0x00cd: *ascii = 'I'; return 1;
        case 0x00f3: case 0x00f2: *ascii = 'o'; return 1;
        case 0x00d3: *ascii = 'O'; return 1;
        case 0x00fa: case 0x00f9: case 0x00fc: *ascii = 'u'; return 1;
        case 0x00da: case 0x00dc: *ascii = 'U'; return 1;
        case 0x00f1: *ascii = 'n'; return 1;
        case 0x00d1: *ascii = 'N'; return 1;
        default: return 0;
    }
}

static int oz_utf8_accent_ascii_at(const unsigned char* s, size_t n, size_t* consumed, char* ascii) {
    if (n >= 2 && s[0] == 0xc3) {
        int code = 0;
        if ((s[1] & 0xc0) == 0x80) {
            code = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
            if (oz_codepoint_accent_ascii(code, ascii)) {
                *consumed = 2;
                return 1;
            }
        }
    }
    if (n >= 4 && s[0] == 0xc3 && s[1] == 0x83 && s[2] == 0xc2) {
        unsigned char tail = s[3];
        switch (tail) {
            case 0xa1: case 0xa0: *ascii = 'a'; break;
            case 0x81: *ascii = 'A'; break;
            case 0xa9: case 0xa8: *ascii = 'e'; break;
            case 0x89: *ascii = 'E'; break;
            case 0xad: case 0xac: *ascii = 'i'; break;
            case 0x8d: *ascii = 'I'; break;
            case 0xb3: case 0xb2: *ascii = 'o'; break;
            case 0x93: *ascii = 'O'; break;
            case 0xba: case 0xb9: case 0xbc: *ascii = 'u'; break;
            case 0x9a: case 0x9c: *ascii = 'U'; break;
            case 0xb1: *ascii = 'n'; break;
            case 0x91: *ascii = 'N'; break;
            default: return 0;
        }
        *consumed = 4;
        return 1;
    }
    return 0;
}

size_t ozmvm_strip_accents_utf8_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        size_t consumed = 0;
        char ascii = 0;
        if (oz_utf8_accent_ascii_at((const unsigned char*)s + i, n - i, &consumed, &ascii)) {
            oz_out_byte(out, out_cap, &used, (unsigned char)ascii);
            i += consumed - 1;
        } else {
            oz_out_byte(out, out_cap, &used, (unsigned char)s[i]);
        }
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

static void oz_normalized_emit(char* out, size_t out_cap, size_t* used, int* prev_space, unsigned char c) {
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
        if (*used > 0) *prev_space = 1;
        return;
    }
    if (*prev_space && *used > 0) oz_out_byte(out, out_cap, used, ' ');
    *prev_space = 0;
    oz_out_byte(out, out_cap, used, c);
}

size_t ozmvm_normalize_key_utf8_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    int prev_space = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        size_t consumed = 0;
        char ascii = 0;
        int entity = oz_html_entity_at(s, n, i, &consumed);
        if (entity >= 0) {
            if (entity <= 0x7f) {
                oz_normalized_emit(out, out_cap, &used, &prev_space, (unsigned char)entity);
            } else if (oz_codepoint_accent_ascii(entity, &ascii)) {
                oz_normalized_emit(out, out_cap, &used, &prev_space, (unsigned char)ascii);
            } else {
                if (used > 0) prev_space = 1;
            }
            i += consumed - 1;
            continue;
        }
        if (oz_utf8_accent_ascii_at((const unsigned char*)s + i, n - i, &consumed, &ascii)) {
            oz_normalized_emit(out, out_cap, &used, &prev_space, (unsigned char)ascii);
            i += consumed - 1;
            continue;
        }
        oz_normalized_emit(out, out_cap, &used, &prev_space, (unsigned char)s[i]);
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

size_t ozmvm_safe_filename_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) s = "";
    for (size_t i = 0; i < n && used < 120; ++i) {
        size_t consumed = 0;
        char ascii = 0;
        unsigned char c = (unsigned char)s[i];
        if (oz_utf8_accent_ascii_at((const unsigned char*)s + i, n - i, &consumed, &ascii)) {
            c = (unsigned char)ascii;
            i += consumed - 1;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) {
            c = '_';
        }
        oz_out_byte(out, out_cap, &used, c);
    }
    if (used == 0) {
        static const char unnamed[] = "unnamed";
        for (size_t i = 0; i < sizeof(unnamed) - 1; ++i) oz_out_byte(out, out_cap, &used, (unsigned char)unnamed[i]);
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

static int oz_is_utf8_dash_token(const char* s, size_t b, size_t e) {
    return (e == b + 1 && s[b] == '-') ||
           (e == b + 3 && (unsigned char)s[b] == 0xe2 && (unsigned char)s[b + 1] == 0x80 &&
            ((unsigned char)s[b + 2] == 0x93 || (unsigned char)s[b + 2] == 0x94));
}

int ozmvm_parse_epi_i64_token(const char* s, size_t n, int64_t* out) {
    size_t b = 0;
    size_t e = 0;
    int64_t v = 0;
    int seen = 0;
    if (out) *out = 0;
    if (!s || !out) return 0;
    ozmvm_trim_ascii_span(s, n, &b, &e);
    if (b >= e) return 0;
    if (oz_is_utf8_dash_token(s, b, e)) {
        *out = 0;
        return 1;
    }
    for (size_t i = b; i < e; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= '0' && c <= '9') {
            int d = (int)(c - '0');
            seen = 1;
            if (v > (INT64_MAX - d) / 10) return 0;
            v = v * 10 + d;
        } else if (c == ',' || c == '.' || c == ' ') {
            continue;
        } else {
            return 0;
        }
    }
    if (!seen) return 0;
    *out = v;
    return 1;
}

int ozmvm_is_numeric_token(const char* s, size_t n) {
    int64_t v = 0;
    return ozmvm_parse_epi_i64_token(s, n, &v);
}

static void oz_out_ascii_literal(char* out, size_t out_cap, size_t* used, const char* lit) {
    if (!lit) return;
    while (*lit) {
        oz_out_byte(out, out_cap, used, (unsigned char)*lit);
        ++lit;
    }
}

static unsigned char oz_hex_nibble(unsigned v) {
    v &= 0x0f;
    return (unsigned char)(v < 10 ? ('0' + v) : ('a' + (v - 10)));
}

size_t ozmvm_json_escape_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '\\': oz_out_ascii_literal(out, out_cap, &used, "\\\\"); break;
            case '"': oz_out_ascii_literal(out, out_cap, &used, "\\\""); break;
            case '\b': oz_out_ascii_literal(out, out_cap, &used, "\\b"); break;
            case '\f': oz_out_ascii_literal(out, out_cap, &used, "\\f"); break;
            case '\n': oz_out_ascii_literal(out, out_cap, &used, "\\n"); break;
            case '\r': oz_out_ascii_literal(out, out_cap, &used, "\\r"); break;
            case '\t': oz_out_ascii_literal(out, out_cap, &used, "\\t"); break;
            default:
                if (c < 0x20) {
                    oz_out_ascii_literal(out, out_cap, &used, "\\u00");
                    oz_out_byte(out, out_cap, &used, oz_hex_nibble(c >> 4));
                    oz_out_byte(out, out_cap, &used, oz_hex_nibble(c));
                } else {
                    oz_out_byte(out, out_cap, &used, c);
                }
                break;
        }
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

size_t ozmvm_csv_escape_copy(const char* s, size_t n, char* out, size_t out_cap) {
    int need = 0;
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (c == ',' || c == '\n' || c == '\r' || c == '"' || c == '\t') {
            need = 1;
            break;
        }
    }
    if (!need) {
        for (size_t i = 0; i < n; ++i) oz_out_byte(out, out_cap, &used, (unsigned char)s[i]);
        oz_out_zero(out, out_cap, used);
        return used;
    }
    oz_out_byte(out, out_cap, &used, '"');
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') oz_out_byte(out, out_cap, &used, '"');
        oz_out_byte(out, out_cap, &used, c);
    }
    oz_out_byte(out, out_cap, &used, '"');
    oz_out_zero(out, out_cap, used);
    return used;
}

void ozmvm_fnv1a64_hex(const char* s, size_t n, char out_hex16[17]) {
    static const char hex[] = "0123456789abcdef";
    uint64_t h = ozmvm_fnv1a64_u64(s, n);
    if (!out_hex16) return;
    for (int i = 15; i >= 0; --i) {
        out_hex16[i] = hex[h & 0x0full];
        h >>= 4;
    }
    out_hex16[16] = '\0';
}

uint64_t ozmvm_fnv1a64_update(uint64_t h, const char* s, size_t n) {
    if (!s) n = 0;
    for (size_t i = 0; i < n; ++i) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t ozmvm_fnv1a64_u64(const char* s, size_t n) {
    return ozmvm_fnv1a64_update(1469598103934665603ull, s, n);
}

size_t ozmvm_key_fragment_copy(const char* s, size_t n, char* out, size_t out_cap, size_t max_len, const char* fallback) {
    size_t used = 0;
    char stack[256];
    char* tmp = stack;
    size_t tmp_cap = sizeof(stack);
    size_t norm_n = 0;
    if (!fallback || !*fallback) fallback = "grafica";
    if (max_len == 0 || max_len > 512) max_len = 512;
    if (n + 1 > tmp_cap) {
        tmp_cap = n + 1;
        tmp = (char*)malloc(tmp_cap);
        if (!tmp) {
            tmp = stack;
            tmp_cap = sizeof(stack);
        }
    }
    norm_n = ozmvm_normalize_key_utf8_copy(s, n, tmp, tmp_cap);
    if (norm_n == 0) {
        for (const char* p = fallback; *p && used < max_len; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) c = '_';
            oz_out_byte(out, out_cap, &used, c);
        }
    } else {
        for (size_t i = 0; i < norm_n && used < max_len; ++i) {
            unsigned char c = (unsigned char)tmp[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) c = '_';
            oz_out_byte(out, out_cap, &used, c);
        }
    }
    oz_out_zero(out, out_cap, used);
    if (tmp != stack) free(tmp);
    return used;
}

static size_t oz_snprintf_len(int rc) {
    return rc < 0 ? 0u : (size_t)rc;
}

size_t ozmvm_rect_json_copy(double x0, double y0, double x1, double y1, char* out, size_t out_cap) {
    int rc = 0;
    if (out && out_cap > 0) out[0] = '\0';
    rc = snprintf(out, out_cap, "{\"x0\":%.3f,\"y0\":%.3f,\"x1\":%.3f,\"y1\":%.3f}", x0, y0, x1, y1);
    if (out && out_cap > 0) out[out_cap - 1] = '\0';
    return oz_snprintf_len(rc);
}

size_t ozmvm_rect_csv_copy(double x0, double y0, double x1, double y1, char* out, size_t out_cap) {
    int rc = 0;
    if (out && out_cap > 0) out[0] = '\0';
    rc = snprintf(out, out_cap, "%.3f|%.3f|%.3f|%.3f", x0, y0, x1, y1);
    if (out && out_cap > 0) out[out_cap - 1] = '\0';
    return oz_snprintf_len(rc);
}

static int oz_ascii_lower_c(int c) {
    return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

static int oz_ascii_ieq(char a, char b) {
    return oz_ascii_lower_c((unsigned char)a) == oz_ascii_lower_c((unsigned char)b);
}

static int oz_ascii_imatch_lit(const char* s, size_t n, size_t p, const char* lit) {
    size_t i = 0;
    if (!s || !lit) return 0;
    while (lit[i]) {
        if (p + i >= n || !oz_ascii_ieq(s[p + i], lit[i])) return 0;
        ++i;
    }
    return 1;
}

static int oz_span_contains_lit_i(const char* s, size_t n, const char* lit) {
    const size_t m = lit ? strlen(lit) : 0u;
    if (!s || !lit || m == 0 || m > n) return 0;
    for (size_t i = 0; i + m <= n; ++i) {
        if (oz_ascii_imatch_lit(s, n, i, lit)) return 1;
    }
    return 0;
}

static int oz_is_hex_ascii(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int ozmvm_html_next_href(const char* s, size_t n, size_t start,
                         size_t* out_begin, size_t* out_end, size_t* out_next) {
    if (out_begin) *out_begin = 0;
    if (out_end) *out_end = 0;
    if (out_next) *out_next = start;
    if (!s || !out_begin || !out_end || !out_next || start >= n) return 0;
    for (size_t p = start; p + 4u <= n; ++p) {
        size_t i = p + 4u;
        char quote = 0;
        if (!oz_ascii_imatch_lit(s, n, p, "href")) continue;
        while (i < n && oz_is_space(s[i])) ++i;
        if (i >= n || s[i] != '=') continue;
        ++i;
        while (i < n && oz_is_space(s[i])) ++i;
        if (i >= n || (s[i] != '\'' && s[i] != '"')) continue;
        quote = s[i++];
        const size_t b = i;
        while (i < n && s[i] != '\'' && s[i] != '"') ++i;
        if (i >= n) return 0;
        if (i == b) {
            *out_next = i + 1u;
            continue;
        }
        (void)quote;
        *out_begin = b;
        *out_end = i;
        *out_next = i + 1u;
        return 1;
    }
    *out_next = n;
    return 0;
}

static int oz_internal_hash_tag_at(const char* s, size_t n, size_t p, size_t* out_end) {
    size_t i = p + 2u;
    size_t digits = 0;
    if (!s || p + 12u > n || s[p] != '_' || s[p + 1u] != '_') return 0;
    while (i < n && digits < 32u && oz_is_hex_ascii(s[i])) {
        ++i;
        ++digits;
    }
    if (digits < 8u || i + 2u > n || s[i] != '_' || s[i + 1u] != '_') return 0;
    if (out_end) *out_end = i + 2u;
    return 1;
}

static int oz_tmp_hash_suffix_begin(const char* s, size_t n, size_t* out_begin) {
    size_t dot_tmp = 0;
    size_t dot_hash = 0;
    size_t hash_b = 0;
    size_t hash_e = 0;
    const char normalizing[] = "normalizando_";
    const size_t normalizing_n = sizeof(normalizing) - 1u;
    if (!s || n < 13u || !oz_ascii_imatch_lit(s, n, n - 4u, ".tmp")) return 0;
    dot_tmp = n - 4u;
    dot_hash = dot_tmp;
    while (dot_hash > 0 && s[dot_hash - 1u] != '.') --dot_hash;
    if (dot_hash == 0) return 0;
    hash_b = dot_hash;
    if (hash_b + normalizing_n < dot_tmp && memcmp(s + hash_b, normalizing, normalizing_n) == 0) {
        hash_b += normalizing_n;
    }
    hash_e = dot_tmp;
    if (hash_e - hash_b < 8u || hash_e - hash_b > 32u) return 0;
    for (size_t i = hash_b; i < hash_e; ++i) if (!oz_is_hex_ascii(s[i])) return 0;
    if (out_begin) *out_begin = dot_hash - 1u;
    return 1;
}

size_t ozmvm_strip_internal_hash_fragments_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t used = 0;
    size_t end = n;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    if (oz_tmp_hash_suffix_begin(s, n, &end)) {
        /* end already points at the suffix-starting dot. */
    } else {
        end = n;
    }
    for (size_t i = 0; i < end;) {
        size_t tag_end = 0;
        if (oz_internal_hash_tag_at(s, end, i, &tag_end)) {
            oz_out_byte(out, out_cap, &used, '_');
            oz_out_byte(out, out_cap, &used, '_');
            i = tag_end;
            continue;
        }
        oz_out_byte(out, out_cap, &used, (unsigned char)s[i]);
        ++i;
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

int ozmvm_cdmx_bulletin_year_week_from_key(const char* s, size_t n, int* out_year, int* out_week) {
    const int year = ozmvm_first_epi_year_1900_2099(s, n);
    const int week = ozmvm_epi_week_contextual(s, n);
    if (out_year) *out_year = 0;
    if (out_week) *out_week = 0;
    if (year < 2019 || year > 2026 || week < 1 || week > 53) return 0;
    if (out_year) *out_year = year;
    if (out_week) *out_week = week;
    return 1;
}

int ozmvm_cdmx_zip_year_from_key(const char* s, size_t n) {
    const int year = ozmvm_first_epi_year_1900_2099(s, n);
    if (!s || year < 2019 || year > 2026) return 0;
    if (!oz_span_contains_lit_i(s, n, ".zip")) return 0;
    if (!oz_span_contains_lit_i(s, n, "demp boletin")) return 0;
    if (!oz_span_contains_lit_i(s, n, "ciudad de mexico") && !oz_span_contains_lit_i(s, n, "cdmx")) return 0;
    return year;
}

static int oz_parse_edomex_archive_year(const char* s, size_t n) {
    const char needle[] = "/archivos/";
    const size_t needle_n = sizeof(needle) - 1u;
    if (!s || n < needle_n + 5u) return 0;
    for (size_t i = 0; i + needle_n + 5u <= n; ++i) {
        if (!oz_ascii_imatch_lit(s, n, i, needle)) continue;
        const size_t y = i + needle_n;
        if (s[y] == '2' && s[y + 1u] == '0' && oz_is_digit(s[y + 2u]) && oz_is_digit(s[y + 3u]) && s[y + 4u] == '/') {
            return 2000 + (s[y + 2u] - '0') * 10 + (s[y + 3u] - '0');
        }
    }
    return 0;
}

int ozmvm_edomex_bulletin_year_week_from_url(const char* s, size_t n, int* out_year, int* out_week) {
    if (out_year) *out_year = 0;
    if (out_week) *out_week = 0;
    if (!s) return 0;
    for (size_t i = 0; i + 10u <= n; ++i) {
        size_t p = i + 3u;
        size_t d = 0;
        int week = 0;
        int yy = 0;
        int year = 0;
        if (!oz_ascii_imatch_lit(s, n, i, "bol")) continue;
        if (p < n && (s[p] == '-' || s[p] == '_')) ++p;
        while (p + d < n && d < 4u && oz_is_digit(s[p + d])) ++d;
        if ((d != 3u && d != 4u) || p + d + 4u > n || !oz_ascii_imatch_lit(s, n, p + d, ".pdf")) continue;
        if (d == 3u) {
            week = s[p] - '0';
            yy = (s[p + 1u] - '0') * 10 + (s[p + 2u] - '0');
        } else {
            week = (s[p] - '0') * 10 + (s[p + 1u] - '0');
            yy = (s[p + 2u] - '0') * 10 + (s[p + 3u] - '0');
        }
        year = oz_parse_edomex_archive_year(s, n);
        if (year == 0) year = 2000 + yy;
        if (year < 2008 || year > 2026) return 0;
        if (yy != year % 100) return 0;
        if (week < 1 || week > 53) return 0;
        if (out_year) *out_year = year;
        if (out_week) *out_week = week;
        return 1;
    }
    return 0;
}

static uint32_t oz_png_crc32_update(uint32_t c, const unsigned char* data, size_t n) {
    if (!data && n > 0) return c;
    for (size_t i = 0; i < n; ++i) {
        c ^= (uint32_t)data[i];
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
    }
    return c;
}

uint32_t ozmvm_png_crc32_bytes(const unsigned char* data, size_t n) {
    return oz_png_crc32_update(0xffffffffu, data, n) ^ 0xffffffffu;
}

static void oz_png_adler32_update(uint32_t* a, uint32_t* b, const unsigned char* data, size_t n) {
    if (!a || !b || (!data && n > 0)) return;
    while (n > 0) {
        const size_t block = n > 5552u ? 5552u : n;
        for (size_t i = 0; i < block; ++i) {
            *a += data[i];
            *b += *a;
        }
        *a %= 65521u;
        *b %= 65521u;
        data += block;
        n -= block;
    }
}

static void oz_png_adler32_update_zero(uint32_t* a, uint32_t* b) {
    if (!a || !b) return;
    *b += *a;
    if (*b >= 65521u) *b %= 65521u;
}

uint32_t ozmvm_png_adler32_bytes(const unsigned char* data, size_t n) {
    uint32_t a = 1u;
    uint32_t b = 0u;
    oz_png_adler32_update(&a, &b, data, n);
    return (b << 16) | a;
}

static size_t oz_png_uncompressed_blocks(size_t raw_size) {
    return raw_size / 65535u + (raw_size % 65535u ? 1u : 0u);
}

static size_t oz_png_zlib_uncompressed_size(size_t raw_size) {
    const size_t blocks = oz_png_uncompressed_blocks(raw_size);
    if (blocks == 0 || blocks > (SIZE_MAX - 6u) / 5u) return 0;
    const size_t headers = blocks * 5u;
    if (raw_size > SIZE_MAX - 2u - headers - 4u) return 0;
    return 2u + headers + raw_size + 4u;
}

static int oz_png_raw_geometry(int w, int h, size_t* row_bytes, size_t* row_stride, size_t* raw_size) {
    const size_t sw = (size_t)w;
    const size_t sh = (size_t)h;
    if (w <= 0 || h <= 0) return 0;
    if (sw > (SIZE_MAX - 1u) / 4u) return 0;
    *row_bytes = sw * 4u;
    *row_stride = *row_bytes + 1u;
    if (sh > SIZE_MAX / *row_stride) return 0;
    *raw_size = sh * *row_stride;
    return *raw_size > 0;
}

size_t ozmvm_png_rgba_uncompressed_size(int w, int h) {
    size_t row_bytes = 0;
    size_t row_stride = 0;
    size_t raw_size = 0;
    if (!oz_png_raw_geometry(w, h, &row_bytes, &row_stride, &raw_size)) return 0;
    (void)row_bytes;
    (void)row_stride;
    const size_t zlib_size = oz_png_zlib_uncompressed_size(raw_size);
    if (zlib_size == 0 || zlib_size > (size_t)0xffffffffu || zlib_size > SIZE_MAX - 57u) return 0;
    return zlib_size + 57u;
}

static void oz_png_write_be32(unsigned char* out, size_t* used, uint32_t v) {
    out[(*used)++] = (unsigned char)((v >> 24) & 255u);
    out[(*used)++] = (unsigned char)((v >> 16) & 255u);
    out[(*used)++] = (unsigned char)((v >> 8) & 255u);
    out[(*used)++] = (unsigned char)(v & 255u);
}

static void oz_png_write_bytes(unsigned char* out, size_t* used, const unsigned char* data, size_t n) {
    if (n == 0) return;
    memcpy(out + *used, data, n);
    *used += n;
}

static void oz_png_finish_chunk(unsigned char* out, size_t* used, size_t type_pos) {
    oz_png_write_be32(out, used, ozmvm_png_crc32_bytes(out + type_pos, *used - type_pos));
}

static void oz_png_write_zlib_rgba(unsigned char* out, size_t* used, int w, int h,
                                   const unsigned char* rgba_top_down, size_t row_bytes,
                                   size_t row_stride, size_t raw_size) {
    uint32_t adler_a = 1u;
    uint32_t adler_b = 0u;
    size_t remaining = raw_size;
    size_t row = 0;
    size_t row_pos = 0;
    (void)h;

    out[(*used)++] = 0x78u;
    out[(*used)++] = 0x01u;

    while (remaining > 0) {
        const unsigned len = (unsigned)(remaining > 65535u ? 65535u : remaining);
        size_t in_block = 0;
        const int last = remaining == (size_t)len;
        out[(*used)++] = (unsigned char)(last ? 0x01u : 0x00u);
        out[(*used)++] = (unsigned char)(len & 255u);
        out[(*used)++] = (unsigned char)((len >> 8) & 255u);
        {
            const unsigned nlen = (~len) & 0xffffu;
            out[(*used)++] = (unsigned char)(nlen & 255u);
            out[(*used)++] = (unsigned char)((nlen >> 8) & 255u);
        }
        while (in_block < (size_t)len) {
            if (row_pos == 0) {
                out[(*used)++] = 0u;
                oz_png_adler32_update_zero(&adler_a, &adler_b);
                row_pos = 1u;
                ++in_block;
            } else {
                size_t chunk = row_stride - row_pos;
                const unsigned char* src = rgba_top_down + row * row_bytes + (row_pos - 1u);
                if (chunk > (size_t)len - in_block) chunk = (size_t)len - in_block;
                oz_png_write_bytes(out, used, src, chunk);
                oz_png_adler32_update(&adler_a, &adler_b, src, chunk);
                row_pos += chunk;
                in_block += chunk;
                if (row_pos == row_stride) {
                    row_pos = 0;
                    ++row;
                }
            }
        }
        remaining -= len;
    }
    (void)w;
    oz_png_write_be32(out, used, (adler_b << 16) | adler_a);
}

size_t ozmvm_png_rgba_uncompressed_encode(int w, int h, const unsigned char* rgba_top_down,
                                          unsigned char* out, size_t out_cap) {
    static const unsigned char sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    size_t row_bytes = 0;
    size_t row_stride = 0;
    size_t raw_size = 0;
    size_t used = 0;
    size_t type_pos = 0;
    const size_t need = ozmvm_png_rgba_uncompressed_size(w, h);
    const size_t zlib_size = oz_png_raw_geometry(w, h, &row_bytes, &row_stride, &raw_size)
                                 ? oz_png_zlib_uncompressed_size(raw_size)
                                 : 0u;
    if (need == 0 || zlib_size == 0 || !rgba_top_down || !out || out_cap < need) return 0;

    oz_png_write_bytes(out, &used, sig, sizeof(sig));

    oz_png_write_be32(out, &used, 13u);
    type_pos = used;
    oz_png_write_bytes(out, &used, (const unsigned char*)"IHDR", 4u);
    oz_png_write_be32(out, &used, (uint32_t)w);
    oz_png_write_be32(out, &used, (uint32_t)h);
    out[used++] = 8u;
    out[used++] = 6u;
    out[used++] = 0u;
    out[used++] = 0u;
    out[used++] = 0u;
    oz_png_finish_chunk(out, &used, type_pos);

    oz_png_write_be32(out, &used, (uint32_t)zlib_size);
    type_pos = used;
    oz_png_write_bytes(out, &used, (const unsigned char*)"IDAT", 4u);
    oz_png_write_zlib_rgba(out, &used, w, h, rgba_top_down, row_bytes, row_stride, raw_size);
    oz_png_finish_chunk(out, &used, type_pos);

    oz_png_write_be32(out, &used, 0u);
    type_pos = used;
    oz_png_write_bytes(out, &used, (const unsigned char*)"IEND", 4u);
    oz_png_finish_chunk(out, &used, type_pos);

    return used == need ? used : 0u;
}

static size_t oz_copy_span_zero(const char* s, size_t b, size_t e, char* out, size_t out_cap) {
    const size_t n = (s && e >= b) ? (e - b) : 0u;
    const size_t copy_n = out_cap > 0 && n >= out_cap ? out_cap - 1u : n;
    if (out && out_cap > 0 && copy_n > 0) memcpy(out, s + b, copy_n);
    if (out && out_cap > 0) out[copy_n] = '\0';
    return n;
}

static int oz_metro_route_span_value(const char* s, size_t b, size_t e, size_t* out_b, size_t* out_e) {
    const size_t n = e >= b ? e - b : 0u;
    if (!s || n == 0) return 0;
    if (n == 2u && s[b] == '1' && s[b + 1u] == '2') {
        if (out_b) *out_b = b;
        if (out_e) *out_e = e;
        return 1;
    }
    if (n == 1u && ((s[b] >= '1' && s[b] <= '9') || oz_ascii_lower_c((unsigned char)s[b]) == 'a' || oz_ascii_lower_c((unsigned char)s[b]) == 'b')) {
        if (out_b) *out_b = b;
        if (out_e) *out_e = e;
        return 1;
    }
    return 0;
}

static int oz_metro_route_from_compact_token(const char* s, OzSpan t, size_t* out_b, size_t* out_e) {
    size_t suffix = 0;
    if (!s) return 0;
    if (oz_span_has_prefix(s, t, "linea", &suffix) && oz_metro_route_span_value(s, suffix, t.e, out_b, out_e)) return 1;
    if (oz_span_has_prefix(s, t, "metrol", &suffix) && oz_metro_route_span_value(s, suffix, t.e, out_b, out_e)) return 1;
    if (oz_span_has_prefix(s, t, "l", &suffix) && oz_metro_route_span_value(s, suffix, t.e, out_b, out_e)) return 1;
    return 0;
}

size_t ozmvm_metro_route_key_copy(const char* s, size_t n, char* out, size_t out_cap) {
    OzSpan tok[48];
    const int nt = oz_tokenize_ascii(s, n, tok, sizeof(tok) / sizeof(tok[0]));
    size_t rb = 0, re = 0;
    if (out && out_cap > 0) out[0] = '\0';
    if (!s || nt <= 0) return 0;
    for (int i = 0; i < nt; ++i) {
        if (oz_metro_route_from_compact_token(s, tok[i], &rb, &re)) return oz_copy_span_zero(s, rb, re, out, out_cap);
        if (oz_span_eq_lit(s, tok[i], "linea") || oz_span_eq_lit(s, tok[i], "l")) {
            if (i + 1 < nt && oz_metro_route_span_value(s, tok[i + 1].b, tok[i + 1].e, &rb, &re)) {
                return oz_copy_span_zero(s, rb, re, out, out_cap);
            }
        }
        if (oz_span_eq_lit(s, tok[i], "metro") && i + 2 < nt && oz_span_eq_lit(s, tok[i + 1], "l") &&
            oz_metro_route_span_value(s, tok[i + 2].b, tok[i + 2].e, &rb, &re)) {
            return oz_copy_span_zero(s, rb, re, out, out_cap);
        }
    }
    return 0;
}

static void oz_trim_ascii_bounds(const char* s, size_t n, size_t* b, size_t* e) {
    *b = 0;
    *e = s ? n : 0u;
    if (!s) return;
    while (*b < *e && oz_is_space(s[*b])) ++(*b);
    while (*e > *b && oz_is_space(s[*e - 1u])) --(*e);
}

size_t ozmvm_strip_metro_route_prefix_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t b = 0, e = 0, p = 0, rb = 0, re = 0;
    oz_trim_ascii_bounds(s, n, &b, &e);
    p = b;
    if (oz_metro_route_span_value(s, p, p + 2u <= e ? p + 2u : e, &rb, &re) && re < e && oz_is_space(s[re])) {
        p = re;
    } else if (oz_metro_route_span_value(s, p, p + 1u <= e ? p + 1u : e, &rb, &re) && re < e && oz_is_space(s[re])) {
        p = re;
    } else {
        return oz_copy_span_zero(s, 0, n, out, out_cap);
    }
    while (p < e && oz_is_space(s[p])) ++p;
    return oz_copy_span_zero(s, p, n, out, out_cap);
}

size_t ozmvm_compact_route_name_copy(const char* s, size_t n, int mode, char* out, size_t out_cap) {
    size_t used = 0;
    if (!s) {
        oz_out_zero(out, out_cap, 0);
        return 0;
    }
    for (size_t i = 0; i < n;) {
        if (mode == 0) {
            size_t j = i;
            while (j < n && oz_is_space(s[j])) ++j;
            if (j < n && s[j] == '/') {
                size_t k = j + 1u;
                while (k < n && oz_is_space(s[k])) ++k;
                oz_out_byte(out, out_cap, &used, '-');
                i = k;
                continue;
            }
        } else {
            size_t j = i;
            while (j < n && oz_is_space(s[j])) ++j;
            if (j > i && j < n && s[j] == '-') {
                size_t k = j + 1u;
                while (k < n && oz_is_space(s[k])) ++k;
                if (k > j + 1u) {
                    oz_out_byte(out, out_cap, &used, '-');
                    i = k;
                    continue;
                }
            }
        }
        oz_out_byte(out, out_cap, &used, (unsigned char)s[i]);
        ++i;
    }
    oz_out_zero(out, out_cap, used);
    return used;
}

static int oz_parse_4digits_at(const char* s, size_t n, size_t p, int* out) {
    if (!s || !out || p + 4u > n) return 0;
    if (!oz_is_digit(s[p]) || !oz_is_digit(s[p + 1u]) || !oz_is_digit(s[p + 2u]) || !oz_is_digit(s[p + 3u])) return 0;
    *out = (s[p] - '0') * 1000 + (s[p + 1u] - '0') * 100 + (s[p + 2u] - '0') * 10 + (s[p + 3u] - '0');
    return 1;
}

int ozmvm_icon_year_range_from_stem(const char* s, size_t n, int* out_from, int* out_to) {
    int from = 0, to = 0;
    if (out_from) *out_from = 0;
    if (out_to) *out_to = 9999;
    if (!s || n < 12u) return 0;
    if (!oz_parse_4digits_at(s, n, 0, &from)) return 0;
    if (s[4] != '_') return 0;
    if (oz_parse_4digits_at(s, n, 5u, &to)) {
        if (n < 11u || s[9] != '_' || s[10] != '_') return 0;
    } else {
        size_t word_e = 5u;
        while (word_e < n && ((s[word_e] >= 'a' && s[word_e] <= 'z') || (s[word_e] >= 'A' && s[word_e] <= 'Z'))) ++word_e;
        if (word_e + 2u > n || s[word_e] != '_' || s[word_e + 1u] != '_') return 0;
        const size_t word_n = word_e - 5u;
        if ((word_n == 6u && oz_ascii_imatch_lit(s, word_e, 5u, "actual")) ||
            (word_n == 8u && oz_ascii_imatch_lit(s, word_e, 5u, "presente")) ||
            (word_n == 7u && oz_ascii_imatch_lit(s, word_e, 5u, "vigente"))) {
            to = 9999;
        } else {
            return 0;
        }
    }
    if (to < from) {
        const int tmp = from;
        from = to;
        to = tmp;
    }
    if (out_from) *out_from = from;
    if (out_to) *out_to = to;
    return 1;
}

int ozmvm_column_header_role_hint(const char* s, size_t n) {
    OzSpan tok[64];
    const int nt = oz_tokenize_ascii(s, n, tok, sizeof(tok) / sizeof(tok[0]));
    if (!s || nt <= 0) return 0;
    for (int i = 0; i < nt; ++i) {
        if (oz_span_eq_lit(s, tok[i], "sem") || oz_span_eq_lit(s, tok[i], "semana")) return 1;
    }
    for (int i = 0; i < nt; ++i) {
        if (oz_span_eq_lit(s, tok[i], "m") || oz_span_eq_lit(s, tok[i], "masc") ||
            oz_span_eq_lit(s, tok[i], "masculino") || oz_span_eq_lit(s, tok[i], "masculinos") ||
            oz_span_eq_lit(s, tok[i], "hombres") || oz_span_eq_lit(s, tok[i], "varones")) return 2;
    }
    for (int i = 0; i < nt; ++i) {
        if (oz_span_eq_lit(s, tok[i], "f") || oz_span_eq_lit(s, tok[i], "fem") ||
            oz_span_eq_lit(s, tok[i], "femenino") || oz_span_eq_lit(s, tok[i], "femeninos") ||
            oz_span_eq_lit(s, tok[i], "mujeres")) return 3;
    }
    for (int i = 0; i < nt; ++i) {
        if (oz_span_eq_lit(s, tok[i], "acum") || oz_span_eq_lit(s, tok[i], "acumulado") ||
            oz_span_eq_lit(s, tok[i], "total")) return 4;
    }
    return 0;
}

int ozmvm_year_week_label(const char* s, size_t n, int* out_year, int* out_week) {
    const int year = ozmvm_first_epi_year_1900_2099(s, n);
    const int week = ozmvm_epi_week_contextual(s, n);
    if (out_year) *out_year = 0;
    if (out_week) *out_week = 0;
    if (year <= 0 || week <= 0) return 0;
    if (out_year) *out_year = year;
    if (out_week) *out_week = week;
    return 1;
}

static int oz_xml_open_tag_at(const char* s, size_t n, size_t p, const char* tag, size_t tag_n, size_t* out_gt) {
    size_t i = 0;
    if (!s || !tag || p + tag_n + 2u > n || s[p] != '<') return 0;
    if (p + 1u < n && s[p + 1u] == '/') return 0;
    for (i = 0; i < tag_n; ++i) {
        if (p + 1u + i >= n || !oz_ascii_ieq(s[p + 1u + i], tag[i])) return 0;
    }
    {
        const size_t q = p + 1u + tag_n;
        if (q >= n || !(s[q] == '>' || oz_is_space(s[q]) || s[q] == '/')) return 0;
    }
    for (i = p + 1u + tag_n; i < n; ++i) {
        if (s[i] == '>') {
            if (out_gt) *out_gt = i;
            return 1;
        }
    }
    return 0;
}

static int oz_xml_close_tag_at(const char* s, size_t n, size_t p, const char* tag, size_t tag_n) {
    size_t i = 0;
    if (!s || !tag || p + tag_n + 3u > n || s[p] != '<' || s[p + 1u] != '/') return 0;
    for (i = 0; i < tag_n; ++i) {
        if (p + 2u + i >= n || !oz_ascii_ieq(s[p + 2u + i], tag[i])) return 0;
    }
    return p + 2u + tag_n < n && s[p + 2u + tag_n] == '>';
}

int ozmvm_xml_next_tag_span(const char* s, size_t n, size_t start, const char* tag,
                            size_t* out_begin, size_t* out_end, size_t* out_next) {
    const size_t tag_n = tag ? strlen(tag) : 0u;
    if (out_begin) *out_begin = 0;
    if (out_end) *out_end = 0;
    if (out_next) *out_next = start;
    if (!s || !tag || tag_n == 0 || !out_begin || !out_end || !out_next || start >= n) return 0;
    for (size_t p = start; p < n; ++p) {
        size_t gt = 0;
        if (!oz_xml_open_tag_at(s, n, p, tag, tag_n, &gt)) continue;
        const size_t content_b = gt + 1u;
        for (size_t q = content_b; q < n; ++q) {
            if (!oz_xml_close_tag_at(s, n, q, tag, tag_n)) continue;
            *out_begin = content_b;
            *out_end = q;
            *out_next = q + tag_n + 3u;
            return 1;
        }
        return 0;
    }
    *out_next = n;
    return 0;
}

int ozmvm_xml_value_i32_after_key(const char* s, size_t n, const char* key,
                                  int fallback, int min_value, int max_value) {
    const size_t key_n = key ? strlen(key) : 0u;
    if (!s || !key || key_n == 0 || key_n > n) return fallback;
    for (size_t p = 0; p + key_n <= n; ++p) {
        size_t vb = 0, ve = 0, next = 0, i = 0;
        int v = 0;
        int digits = 0;
        if (!oz_ascii_imatch_lit(s, n, p, key)) continue;
        if (!ozmvm_xml_next_tag_span(s, n, p + key_n, "value", &vb, &ve, &next)) return fallback;
        (void)next;
        i = vb;
        while (i < ve && oz_is_space(s[i])) ++i;
        while (i < ve && oz_is_digit(s[i]) && digits < 4) {
            v = v * 10 + (s[i] - '0');
            ++i;
            ++digits;
        }
        if (digits >= 3 && v >= min_value && v <= max_value) return v;
        return fallback;
    }
    return fallback;
}

/* -------------------------------------------------------------------------
   Núcleo atmosférico C puro.

   Las tablas siguientes sustituyen búsquedas std::set/std::map repetidas en
   la ruta caliente de la barra lateral, carga IXIPTLAH y semáforos de mapa.
   El contrato es deliberadamente plano: puntero + longitud, recorrido lineal
   sobre tablas const pequeñas, cero heap, cero excepciones y retorno estable.
   No trasladar esto a contenedores dinámicos; en cuadro interactivo importa
   más la localidad de caché y la ausencia de inicialización perezosa que la
   asintótica teórica de un árbol para universos de <100 claves.
   ------------------------------------------------------------------------- */

typedef struct OzmvmAtomSpan { const char* s; size_t n; } OzmvmAtomSpan;

static int ozmvm_span_eq(const char* a, size_t an, const char* b, size_t bn) {
    return a && b && an == bn && (an == 0 || memcmp(a, b, an) == 0);
}

static int ozmvm_span_eq_lit(const char* a, size_t an, const char* lit) {
    return lit && ozmvm_span_eq(a, an, lit, strlen(lit));
}

static int ozmvm_span_contains_lit(const char* a, size_t an, const char* lit) {
    const size_t ln = lit ? strlen(lit) : 0u;
    if (!a || !lit || ln == 0u || an < ln) return 0;
    for (size_t i = 0; i + ln <= an; ++i) {
        if (memcmp(a + i, lit, ln) == 0) return 1;
    }
    return 0;
}


/* Núcleo epidemiológico C.
   Estas rutinas evitan std::string temporales, excepciones y contenedores dentro
   de rutas de carga/filtrado. La frontera C++ queda reducida a convertir spans
   ya existentes; la decisión semántica vive en tablas y aritmética plana. */
static const char ozmvm_epi_all_sentinel_lit[] = "__TLALPOWA_ALL_EPIDEMIOLOGY__";

const char* ozmvm_epi_all_selection_sentinel(void) {
    return ozmvm_epi_all_sentinel_lit;
}

int ozmvm_epi_is_all_selection_key(const char* s, size_t n) {
    return ozmvm_span_eq_lit(s, n, ozmvm_epi_all_sentinel_lit);
}

uint64_t ozmvm_fnv1a64_update_field(uint64_t h, const char* s, size_t n) {
    static const unsigned char sep = 0x1fu;
    h = ozmvm_fnv1a64_update(h, s, n);
    return ozmvm_fnv1a64_update(h, (const char*)&sep, 1u);
}

int ozmvm_ascii_year_prefix4(const char* s, size_t n) {
    if (!s || n < 4u) return 0;
    int y = 0;
    for (size_t i = 0; i < 4u; ++i) {
        if (!oz_is_digit(s[i])) return 0;
        y = y * 10 + (s[i] - '0');
    }
    return y;
}

int ozmvm_epi_year_filter_active_i32(int year_start, int year_end) {
    return year_start > 0 || year_end > 0;
}

int ozmvm_epi_year_in_range_i32(int year, int year_start, int year_end) {
    if (year_start <= 0 && year_end <= 0) return 1;
    if (year <= 0) return 0;
    int lo = year_start > 0 ? year_start : year_end;
    int hi = year_end > 0 ? year_end : year_start;
    if (lo > hi) { const int tmp = lo; lo = hi; hi = tmp; }
    return year >= lo && year <= hi;
}

int ozmvm_epi_year_week_from_label(const char* s, size_t n, int* out_year, int* out_week) {
    if (!s || !out_year || !out_week) return 0;
    const int y = ozmvm_first_epi_year_1900_2099(s, n);
    if (y < 1900 || y > 2100) return 0;
    const int w = ozmvm_epi_week_contextual(s, n);
    if (w < 1 || w > 53) return 0;
    *out_year = y;
    *out_week = w;
    return 1;
}

static int64_t oz_epi_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2u;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2u ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static void oz_epi_civil_from_days(int64_t z, int* y, unsigned* m, unsigned* d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int yy = (int)yoe + (int)era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned dd = doy - (153u * mp + 2u) / 5u + 1u;
    const unsigned mm = mp + (mp < 10u ? 3u : (unsigned)-9);
    yy += (mm <= 2u);
    *y = yy;
    *m = mm;
    *d = dd;
}

static unsigned oz_epi_days_in_month(int year, unsigned month) {
    static const unsigned base_days[13] = {0u,31u,28u,31u,30u,31u,30u,31u,31u,30u,31u,30u,31u};
    if (month < 1u) month = 1u;
    if (month > 12u) month = 12u;
    if (month != 2u) return base_days[month];
    return ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0)) ? 29u : 28u;
}


static int oz_epi_parse_i32_loose(const char* s, size_t n, int* out) {
    if (!s || !out) return 0;
    size_t i = 0;
    while (i < n && oz_is_space(s[i])) ++i;
    int v = 0;
    int digits = 0;
    while (i < n && oz_is_digit(s[i]) && digits < 9) {
        v = v * 10 + (s[i] - '0');
        ++i;
        ++digits;
    }
    while (i < n && oz_is_space(s[i])) ++i;
    if (digits == 0 || i != n) return 0;
    *out = v;
    return 1;
}

size_t ozmvm_epi_week_label_from_year_epi_or_date_copy(const char* year_s, size_t year_n,
                                                         const char* epi_s, size_t epi_n,
                                                         const char* date_s, size_t date_n,
                                                         char* out, size_t out_cap) {
    if (!out || out_cap == 0u) return 0u;
    out[0] = '\0';
    int y = 0, w = 0;
    if (oz_epi_parse_i32_loose(year_s, year_n, &y) && oz_epi_parse_i32_loose(epi_s, epi_n, &w) && y > 0 && w > 0) {
        const int rc = snprintf(out, out_cap, "%d-%02d", y, w);
        if (rc < 0) { out[0] = '\0'; return 0u; }
        return (size_t)rc < out_cap ? (size_t)rc : out_cap - 1u;
    }
    if (date_s && date_n >= 10u) {
        const size_t m = 10u < out_cap - 1u ? 10u : out_cap - 1u;
        memcpy(out, date_s, m);
        out[m] = '\0';
        return m;
    }
    return 0u;
}

int ozmvm_epi_week_block_key_from_label(const char* s, size_t n) {
    int year = 0;
    int week = 0;
    if (!ozmvm_epi_year_week_from_label(s, n, &year, &week)) return 0;
    if (year < 1800) year = 1800;
    if (year > 2300) year = 2300;
    if (week < 1) week = 1;
    if (week > 53) week = 53;
    int y = year;
    unsigned m = 1u, d = 1u;
    oz_epi_civil_from_days(oz_epi_days_from_civil(year, 1u, 1u) + (int64_t)(week - 1) * 7, &y, &m, &d);
    if (m < 1u) m = 1u;
    if (m > 12u) m = 12u;
    const unsigned max_day = oz_epi_days_in_month(y, m);
    if (d < 1u) d = 1u;
    if (d > max_day) d = max_day;
    const unsigned candidate = (((d - 1u) / 7u) * 7u) + 1u;
    const unsigned block_day = candidate <= max_day ? candidate : max_day;
    return y * 10000 + (int)m * 100 + (int)block_day;
}

static int oz_ascii_lower_eq_lit(const char* s, size_t n, const char* lit) {
    if (!s || !lit) return 0;
    const size_t ln = strlen(lit);
    if (n != ln) return 0;
    for (size_t i = 0; i < ln; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') c = (unsigned char)(c - 'A' + 'a');
        if ((char)c != lit[i]) return 0;
    }
    return 1;
}

static int oz_parse_yyyy_mm_dd_stem(const char* stem, size_t n, int* out_y, int* out_m, int* out_d) {
    if (!stem || n != 10u || stem[4] != '_' || stem[7] != '_') return 0;
    for (size_t i = 0; i < 10u; ++i) {
        if (i == 4u || i == 7u) continue;
        if (!oz_is_digit(stem[i])) return 0;
    }
    const int y = (stem[0]-'0')*1000 + (stem[1]-'0')*100 + (stem[2]-'0')*10 + (stem[3]-'0');
    const int m = (stem[5]-'0')*10 + (stem[6]-'0');
    const int d = (stem[8]-'0')*10 + (stem[9]-'0');
    if (y < 1800 || y > 2300 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    if (out_y) *out_y = y;
    if (out_m) *out_m = m;
    if (out_d) *out_d = d;
    return 1;
}

int ozmvm_ixiptlah_epi_year_from_stem(const char* stem, size_t n) {
    int y = 0, m = 0, d = 0;
    if (oz_parse_yyyy_mm_dd_stem(stem, n, &y, &m, &d)) return y;
    static const char prefix[] = "datosepidemiologicos_";
    const size_t pn = sizeof(prefix) - 1u;
    if (!stem || n != pn + 4u) return 0;
    if (!oz_ascii_lower_eq_lit(stem, pn, prefix)) return 0;
    y = 0;
    for (size_t i = pn; i < n; ++i) {
        if (!oz_is_digit(stem[i])) return 0;
        y = y * 10 + (stem[i] - '0');
    }
    return (y >= 1800 && y <= 2300) ? y : 0;
}

int ozmvm_ixiptlah_epi_chronology_key_from_stem(const char* stem, size_t n) {
    int y = 0, m = 0, d = 0;
    if (oz_parse_yyyy_mm_dd_stem(stem, n, &y, &m, &d)) return y * 10000 + m * 100 + d;
    y = ozmvm_ixiptlah_epi_year_from_stem(stem, n);
    return y > 0 ? y * 10000 + 1231 : 0;
}

const char* ozmvm_epi_metric_from_period(const char* period, size_t period_n,
                                         const char* source_year, size_t source_year_n,
                                         const char* year, size_t year_n,
                                         const char* sex, size_t sex_n) {
    if (ozmvm_span_eq_lit(period, period_n, "Sem")) return "incidencia_semanal";
    if (ozmvm_span_eq_lit(period, period_n, "SemDerivada")) return "incidencia_semanal_derivada_sexo";
    if (ozmvm_span_eq_lit(period, period_n, "Acum")) {
        const int source_differs = source_year && year && source_year_n != 0u &&
            (source_year_n != year_n || memcmp(source_year, year, source_year_n) != 0);
        if (source_differs) return "acumulado_anio_anterior";
        if (ozmvm_span_eq_lit(sex, sex_n, "M") || ozmvm_span_eq_lit(sex, sex_n, "F")) return "acumulado_anual_por_sexo";
        return "acumulado_anual";
    }
    return "valor";
}

const char* ozmvm_epi_period_from_code(uint8_t code) {
    switch (code) {
        case 1u: return "Sem";
        case 2u: return "SemDerivada";
        case 3u: return "Acum";
        default: return "";
    }
}

const char* ozmvm_epi_sex_from_code(uint8_t code) {
    switch (code) {
        case 1u: return "F";
        case 2u: return "M";
        default: return "";
    }
}


static int ozmvm_table_has(const OzmvmAtomSpan* t, size_t count, const char* key, size_t n) {
    if (!key || n == 0u) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (t[i].n == n && memcmp(t[i].s, key, n) == 0) return 1;
    }
    return 0;
}

#define OZMVM_ATOM(x) { x, sizeof(x) - 1u }
#define OZMVM_COUNT(x) (sizeof(x) / sizeof((x)[0]))

static const OzmvmAtomSpan ozmvm_atm_meteorological_keys[] = {
    OZMVM_ATOM("tmp"), OZMVM_ATOM("tmax"), OZMVM_ATOM("tmin"), OZMVM_ATOM("rh"),
    OZMVM_ATOM("wsp"), OZMVM_ATOM("wdr"), OZMVM_ATOM("wgst"), OZMVM_ATOM("wdr_gust"),
    OZMVM_ATOM("u10"), OZMVM_ATOM("v10"), OZMVM_ATOM("gr"), OZMVM_ATOM("uva"),
    OZMVM_ATOM("uvb"), OZMVM_ATOM("uvc"), OZMVM_ATOM("uv"), OZMVM_ATOM("pa"),
    OZMVM_ATOM("pp"), OZMVM_ATOM("pblh")
};

static const OzmvmAtomSpan ozmvm_atm_contaminant_keys[] = {
    OZMVM_ATOM("o3"), OZMVM_ATOM("co"), OZMVM_ATOM("no"), OZMVM_ATOM("no2"),
    OZMVM_ATOM("nox"), OZMVM_ATOM("so2"), OZMVM_ATOM("pm10"), OZMVM_ATOM("pm25"),
    OZMVM_ATOM("pmco"), OZMVM_ATOM("pb"), OZMVM_ATOM("cd"), OZMVM_ATOM("as"),
    OZMVM_ATOM("ni"), OZMVM_ATOM("hg"), OZMVM_ATOM("cr"), OZMVM_ATOM("bc"),
    OZMVM_ATOM("ec"), OZMVM_ATOM("oc")
};

static const OzmvmAtomSpan ozmvm_atm_meteorological_groups[] = {
    OZMVM_ATOM("meteorologico"), OZMVM_ATOM("radiacion")
};

static const OzmvmAtomSpan ozmvm_atm_contaminant_groups[] = {
    OZMVM_ATOM("contaminante_criterio")
};

int ozmvm_atm_key_is_meteorological(const char* key, size_t n) {
    return ozmvm_table_has(ozmvm_atm_meteorological_keys, OZMVM_COUNT(ozmvm_atm_meteorological_keys), key, n);
}

int ozmvm_atm_key_is_contaminant(const char* key, size_t n) {
    return ozmvm_table_has(ozmvm_atm_contaminant_keys, OZMVM_COUNT(ozmvm_atm_contaminant_keys), key, n);
}

static size_t ozmvm_lower_tmp_copy(const char* s, size_t n, char* out, size_t out_cap) {
    if (!out || out_cap == 0u) return 0u;
    if (!s) { out[0] = '\0'; return 0u; }
    const size_t m = (n < out_cap - 1u) ? n : out_cap - 1u;
    for (size_t i = 0; i < m; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') c = (unsigned char)(c - 'A' + 'a');
        out[i] = (char)c;
    }
    out[m] = '\0';
    return m;
}

int ozmvm_atm_group_is_meteorological(const char* group, size_t n) {
    char buf[96];
    const size_t m = ozmvm_lower_tmp_copy(group, n, buf, sizeof(buf));
    return ozmvm_table_has(ozmvm_atm_meteorological_groups, OZMVM_COUNT(ozmvm_atm_meteorological_groups), buf, m);
}

int ozmvm_atm_group_is_contaminant(const char* group, size_t n) {
    char buf[96];
    const size_t m = ozmvm_lower_tmp_copy(group, n, buf, sizeof(buf));
    return ozmvm_table_has(ozmvm_atm_contaminant_groups, OZMVM_COUNT(ozmvm_atm_contaminant_groups), buf, m);
}

static size_t ozmvm_copy_lit(char* out, size_t out_cap, const char* lit) {
    if (!out || out_cap == 0u) return 0u;
    if (!lit) lit = "";
    const size_t n = strlen(lit);
    const size_t m = (n < out_cap - 1u) ? n : out_cap - 1u;
    if (m > 0u) memcpy(out, lit, m);
    out[m] = '\0';
    return m;
}

size_t ozmvm_atm_catalog_group_copy(const char* key, size_t n, char* out, size_t out_cap) {
    static const OzmvmAtomSpan criteria[] = {
        OZMVM_ATOM("o3"), OZMVM_ATOM("pm10"), OZMVM_ATOM("pm25"), OZMVM_ATOM("pmco"),
        OZMVM_ATOM("co"), OZMVM_ATOM("no"), OZMVM_ATOM("no2"), OZMVM_ATOM("nox"),
        OZMVM_ATOM("so2")
    };
    static const OzmvmAtomSpan radiation[] = {
        OZMVM_ATOM("gr"), OZMVM_ATOM("uva"), OZMVM_ATOM("uvb"), OZMVM_ATOM("uvc"), OZMVM_ATOM("uv")
    };

    if (ozmvm_table_has(criteria, OZMVM_COUNT(criteria), key, n)) return ozmvm_copy_lit(out, out_cap, "contaminante_criterio");
    if (ozmvm_table_has(radiation, OZMVM_COUNT(radiation), key, n)) return ozmvm_copy_lit(out, out_cap, "radiacion");
    if (ozmvm_atm_key_is_meteorological(key, n)) return ozmvm_copy_lit(out, out_cap, "meteorologico");
    return ozmvm_copy_lit(out, out_cap, "datos_atmosfericos");
}

size_t ozmvm_atm_sidebar_type_label_copy(const char* key, size_t key_n,
                                          const char* group, size_t group_n,
                                          char* out, size_t out_cap) {
    static const OzmvmAtomSpan particulate_mass[] = {
        OZMVM_ATOM("pm10"), OZMVM_ATOM("pm25"), OZMVM_ATOM("pmco")
    };
    static const OzmvmAtomSpan oxidants[] = { OZMVM_ATOM("o3") };
    static const OzmvmAtomSpan nitrogen[] = {
        OZMVM_ATOM("no"), OZMVM_ATOM("no2"), OZMVM_ATOM("nox")
    };
    static const OzmvmAtomSpan sulfur[] = { OZMVM_ATOM("so2") };
    static const OzmvmAtomSpan carbon_gases[] = { OZMVM_ATOM("co") };
    static const OzmvmAtomSpan wind[] = { OZMVM_ATOM("wsp"), OZMVM_ATOM("wdr"), OZMVM_ATOM("wgst"), OZMVM_ATOM("wdr_gust"), OZMVM_ATOM("u10"), OZMVM_ATOM("v10") };
    static const OzmvmAtomSpan thermodynamic[] = { OZMVM_ATOM("tmp"), OZMVM_ATOM("tmax"), OZMVM_ATOM("tmin"), OZMVM_ATOM("pa") };
    static const OzmvmAtomSpan humidity[] = { OZMVM_ATOM("rh") };
    static const OzmvmAtomSpan hydrology[] = { OZMVM_ATOM("pp") };
    static const OzmvmAtomSpan radiation[] = {
        OZMVM_ATOM("gr"), OZMVM_ATOM("uv"), OZMVM_ATOM("uva"), OZMVM_ATOM("uvb"), OZMVM_ATOM("uvc")
    };

    if (ozmvm_table_has(particulate_mass, OZMVM_COUNT(particulate_mass), key, key_n)) return ozmvm_copy_lit(out, out_cap, "PM");
    if (ozmvm_table_has(oxidants, OZMVM_COUNT(oxidants), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Oxidante");
    if (ozmvm_table_has(nitrogen, OZMVM_COUNT(nitrogen), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Nitrógeno");
    if (ozmvm_table_has(sulfur, OZMVM_COUNT(sulfur), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Azufre");
    if (ozmvm_table_has(carbon_gases, OZMVM_COUNT(carbon_gases), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Gas");
    if (ozmvm_table_has(wind, OZMVM_COUNT(wind), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Viento");
    if (ozmvm_table_has(thermodynamic, OZMVM_COUNT(thermodynamic), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Termo");
    if (ozmvm_table_has(humidity, OZMVM_COUNT(humidity), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Humedad");
    if (ozmvm_table_has(hydrology, OZMVM_COUNT(hydrology), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Agua");
    if (ozmvm_table_has(radiation, OZMVM_COUNT(radiation), key, key_n)) return ozmvm_copy_lit(out, out_cap, "Radiación");

    char g[128];
    const size_t gn = ozmvm_lower_tmp_copy(group, group_n, g, sizeof(g));
    if (ozmvm_span_contains_lit(g, gn, "radi")) return ozmvm_copy_lit(out, out_cap, "Radiación");
    if (ozmvm_span_contains_lit(g, gn, "meteor")) return ozmvm_copy_lit(out, out_cap, "Meteo");
    if (ozmvm_span_contains_lit(g, gn, "contaminante")) return ozmvm_copy_lit(out, out_cap, "Criterio");
    return ozmvm_copy_lit(out, out_cap, "Atmósfera");
}

static double ozmvm_atm_abs(double x) { return x < 0.0 ? -x : x; }
static double ozmvm_atm_max(double a, double b) { return a > b ? a : b; }
static float ozmvm_atm_clampf(double x) {
    if (!isfinite(x)) return -1.0f;
    if (x < 0.0) return 0.0f;
    if (x > 1.0) return 1.0f;
    return (float)x;
}

static int ozmvm_ascii_unit_contains(const char* unit, size_t n, const char* lit) {
    char buf[96];
    const size_t m = ozmvm_lower_tmp_copy(unit, n, buf, sizeof(buf));
    return ozmvm_span_contains_lit(buf, m, lit);
}

double ozmvm_atm_value_for_semaphore(const char* key, size_t key_n,
                                      double value, const char* unit, size_t unit_n) {
    if (!isfinite(value) || !unit || unit_n == 0u) return value;
    if (ozmvm_span_eq_lit(key, key_n, "o3") || ozmvm_span_eq_lit(key, key_n, "no2") ||
        ozmvm_span_eq_lit(key, key_n, "so2") || ozmvm_span_eq_lit(key, key_n, "no") ||
        ozmvm_span_eq_lit(key, key_n, "nox")) {
        if (ozmvm_ascii_unit_contains(unit, unit_n, "ppm") && ozmvm_atm_abs(value) < 10.0) return value * 1000.0;
        return value;
    }
    if (ozmvm_span_eq_lit(key, key_n, "pm10") || ozmvm_span_eq_lit(key, key_n, "pm25")) {
        if ((ozmvm_ascii_unit_contains(unit, unit_n, "mg") || ozmvm_ascii_unit_contains(unit, unit_n, "milig")) && ozmvm_atm_abs(value) < 10.0) return value * 1000.0;
        return value;
    }
    if (ozmvm_span_eq_lit(key, key_n, "co")) {
        if (ozmvm_ascii_unit_contains(unit, unit_n, "ppb")) return value / 1000.0;
        return value;
    }
    return value;
}

static double ozmvm_atm_fraction_to_percent_if_needed(double value) {
    if (!isfinite(value)) return value;
    /* Muchos equipos y APIs exportan proporciones 0..1 para magnitudes que la
       UI interpreta en porcentaje físico. Los ceros reales quedan intactos. */
    if (value >= 0.0 && value <= 1.25) return value * 100.0;
    return value;
}

static double ozmvm_atm_pressure_hpa(double value, const char* unit, size_t unit_n) {
    if (!isfinite(value)) return value;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "pa") && !ozmvm_ascii_unit_contains(unit, unit_n, "hpa") && ozmvm_atm_abs(value) > 2000.0) return value / 100.0;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "kpa")) return value * 10.0;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "mmhg")) return value * 1.3332239;
    if (value > 2000.0) return value / 100.0;
    if (value > 60.0 && value < 130.0) return value * 10.0;
    return value;
}

static double ozmvm_atm_wind_ms(double value, const char* unit, size_t unit_n) {
    if (!isfinite(value)) return value;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "km/h") || ozmvm_ascii_unit_contains(unit, unit_n, "kmh")) return value / 3.6;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "mph")) return value * 0.44704;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "kt") || ozmvm_ascii_unit_contains(unit, unit_n, "knot")) return value * 0.514444;
    return value;
}

static double ozmvm_atm_radiation_wm2(double value, const char* unit, size_t unit_n) {
    if (!isfinite(value)) return value;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "kw")) return value * 1000.0;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "mw/cm2") || ozmvm_ascii_unit_contains(unit, unit_n, "mw/cm^2")) return value * 10.0;
    /* Davis/RUOA a veces deja radiación solar como kW/m2 sin unidad. */
    if (value > 0.0 && value <= 1.6) return value * 1000.0;
    return value;
}

static double ozmvm_atm_precip_mm(double value, const char* unit, size_t unit_n) {
    if (!isfinite(value)) return value;
    if (ozmvm_ascii_unit_contains(unit, unit_n, "inch") || ozmvm_ascii_unit_contains(unit, unit_n, "in/")) return value * 25.4;
    return value;
}

float ozmvm_atm_meteorological_t_unit(const char* key, size_t key_n, double value, const char* unit, size_t unit_n) {
    if (!isfinite(value)) return -1.0f;
    if (ozmvm_span_eq_lit(key, key_n, "tmp") || ozmvm_span_eq_lit(key, key_n, "tmax") || ozmvm_span_eq_lit(key, key_n, "tmin")) return ozmvm_atm_clampf(ozmvm_atm_abs(value - 22.0) / 18.0);
    if (ozmvm_span_eq_lit(key, key_n, "rh")) {
        value = ozmvm_atm_fraction_to_percent_if_needed(value);
        return ozmvm_atm_clampf(ozmvm_atm_max(0.0, ozmvm_atm_abs(value - 50.0) - 10.0) / 40.0);
    }
    if (ozmvm_span_eq_lit(key, key_n, "wsp") || ozmvm_span_eq_lit(key, key_n, "wgst")) return ozmvm_atm_clampf(ozmvm_atm_wind_ms(value, unit, unit_n) / 12.0);
    if (ozmvm_span_eq_lit(key, key_n, "u10") || ozmvm_span_eq_lit(key, key_n, "v10")) return ozmvm_atm_clampf(ozmvm_atm_abs(ozmvm_atm_wind_ms(value, unit, unit_n)) / 12.0);
    if (ozmvm_span_eq_lit(key, key_n, "wdr") || ozmvm_span_eq_lit(key, key_n, "wdr_gust")) return 0.0f;
    if (ozmvm_span_eq_lit(key, key_n, "gr")) return ozmvm_atm_clampf(ozmvm_atm_radiation_wm2(value, unit, unit_n) / 850.0);
    if (ozmvm_span_eq_lit(key, key_n, "uv")) {
        /* RUOA/PEMBU puede usar fracción 0..1 o UVI físico 0..11+. */
        return ozmvm_atm_clampf(value <= 1.25 ? value : value / 11.0);
    }
    if (ozmvm_span_eq_lit(key, key_n, "uva")) return ozmvm_atm_clampf(value <= 1.25 ? value : value / 45.0);
    if (ozmvm_span_eq_lit(key, key_n, "uvb")) return ozmvm_atm_clampf(value <= 1.25 ? value : value / 3.0);
    if (ozmvm_span_eq_lit(key, key_n, "uvc")) return ozmvm_atm_clampf(value <= 1.25 ? value : value / 1.0);
    if (ozmvm_span_eq_lit(key, key_n, "pa")) {
        const double hpa = ozmvm_atm_pressure_hpa(value, unit, unit_n);
        return ozmvm_atm_clampf(ozmvm_atm_abs(hpa - 780.0) / 80.0);
    }
    if (ozmvm_span_eq_lit(key, key_n, "pp")) return ozmvm_atm_clampf(ozmvm_atm_precip_mm(value, unit, unit_n) / 25.0);
    if (ozmvm_span_eq_lit(key, key_n, "pblh")) return ozmvm_atm_clampf(value / 4000.0);
    if (ozmvm_span_eq_lit(key, key_n, "frp")) return ozmvm_atm_clampf(value / 250.0);
    if (ozmvm_span_eq_lit(key, key_n, "brightness")) return ozmvm_atm_clampf((value - 280.0) / 90.0);
    return -1.0f;
}

float ozmvm_atm_meteorological_t(const char* key, size_t key_n, double value) {
    return ozmvm_atm_meteorological_t_unit(key, key_n, value, (const char*)0, 0u);
}

static float ozmvm_atm_breakpoint_t(double value, double b0, double b1, double b2, double b3) {
    if (!isfinite(value)) return -1.0f;
    if (value <= b0) return 0.18f;
    if (value <= b1) return 0.45f + 0.08f * (float)((value - b0) / ozmvm_atm_max(1.0e-9, b1 - b0));
    if (value <= b2) return 0.64f + 0.08f * (float)((value - b1) / ozmvm_atm_max(1.0e-9, b2 - b1));
    if (value <= b3) return 0.82f + 0.08f * (float)((value - b2) / ozmvm_atm_max(1.0e-9, b3 - b2));
    return 1.0f;
}

float ozmvm_atm_measurement_t(const char* key, size_t key_n,
                              double value, const char* unit, size_t unit_n) {
    value = ozmvm_atm_value_for_semaphore(key, key_n, value, unit, unit_n);
    if (ozmvm_span_eq_lit(key, key_n, "pm10")) return ozmvm_atm_breakpoint_t(value, 45.0, 50.0, 132.0, 213.0);
    if (ozmvm_span_eq_lit(key, key_n, "pm25")) return ozmvm_atm_breakpoint_t(value, 15.0, 25.0, 79.0, 130.0);
    if (ozmvm_span_eq_lit(key, key_n, "o3")) return ozmvm_atm_breakpoint_t(value, 58.0, 90.0, 135.0, 175.0);
    if (ozmvm_span_eq_lit(key, key_n, "co")) return ozmvm_atm_breakpoint_t(value, 5.0, 9.0, 12.0, 16.0);
    if (ozmvm_span_eq_lit(key, key_n, "no2") || ozmvm_span_eq_lit(key, key_n, "no") || ozmvm_span_eq_lit(key, key_n, "nox")) return ozmvm_atm_breakpoint_t(value, 53.0, 106.0, 160.0, 213.0);
    if (ozmvm_span_eq_lit(key, key_n, "so2")) return ozmvm_atm_breakpoint_t(value, 35.0, 75.0, 185.0, 304.0);
    return ozmvm_atm_meteorological_t_unit(key, key_n, value, unit, unit_n);
}

double ozmvm_atm_health_ratio(const char* key, size_t key_n,
                               double value, const char* unit, size_t unit_n) {
    const float t = ozmvm_atm_measurement_t(key, key_n, value, unit, unit_n);
    if (t < 0.0f) return -1.0;
    double r = (double)t * 1.35;
    if (r < 0.0) r = 0.0;
    if (r > 1.35) r = 1.35;
    return r;
}

const char* ozmvm_atm_measurement_band_label(float t) {
    if (t < 0.0f) return "Sin escala aplicable";
    if (t < 0.35f) return "Bajo / esperado";
    if (t < 0.58f) return "Moderado";
    if (t < 0.76f) return "Alto";
    if (t < 0.94f) return "Muy alto";
    return "Extremo";
}

#undef OZMVM_ATOM
#undef OZMVM_COUNT

/* -------------------------------------------------------------------------
   Núcleo gráfico C puro: catálogo fijo y coerción semántica de tipos.
   La UI sólo consume punteros const: no hay heap, excepciones ni búsquedas.
   ------------------------------------------------------------------------- */

static const TlacGal TLAC_GAL[] = {
    {6, 1, 1, 0, 0, 3, 4, 1, "Gráfica de dispersión", "incidencia O3", "Grafica_dispersion.png"},
    {2, 3, 0, 0, 0, 4, 0, 0, "Gráfica de líneas", "serie temporal", "Grafica_lineas.png"},
    {4, 3, 0, 0, 0, 4, 0, 0, "Gráfica de área", "acumulado temporal", "Grafica_area.png"},
    {5, 0, 0, 0, 0, 4, 0, 0, "Histograma", "distribución", "Grafica_histograma.png"},
    {1, 4, 3, 0, 0, 4, 0, 0, "Gráfica de barras", "comparación discreta", "grafica_barras.png"}
};

static const int TLAC_TIPOS[] = {6, 2, 4, 5, 1};

size_t tlac_gal_count(void) {
    return sizeof(TLAC_GAL) / sizeof(TLAC_GAL[0]);
}

const TlacGal* tlac_gal_item(size_t index) {
    return index < tlac_gal_count() ? &TLAC_GAL[index] : (const TlacGal*)0;
}

int tlac_tipo_ok(int type) {
    for (size_t i = 0; i < sizeof(TLAC_TIPOS) / sizeof(TLAC_TIPOS[0]); ++i) {
        if (TLAC_TIPOS[i] == type) return 1;
    }
    return 0;
}

int tlac_tipo_limpia(int type) {
    return tlac_tipo_ok(type) ? type : 6;
}

int tlac_combo_count(void) {
    return (int)(sizeof(TLAC_TIPOS) / sizeof(TLAC_TIPOS[0]));
}

int tlac_tipo_de_combo(int combo_index) {
    if (combo_index < 0 || combo_index >= tlac_combo_count()) return 6;
    return TLAC_TIPOS[combo_index];
}

int tlac_combo_de_tipo(int chart_type) {
    chart_type = tlac_tipo_limpia(chart_type);
    for (int i = 0; i < tlac_combo_count(); ++i) {
        if (TLAC_TIPOS[i] == chart_type) return i;
    }
    return 0;
}

const char* tlac_tipo_label(int type) {
    switch (tlac_tipo_limpia(type)) {
        case 1: return "barras";
        case 2: return "líneas";
        case 4: return "área";
        case 5: return "histograma";
        case 6: return "dispersión";
        default: return "dispersión";
    }
}

const char* tlac_combo_label(int combo_index) {
    return tlac_tipo_label(tlac_tipo_de_combo(combo_index));
}

const char* tlac_eje_dom_label(int domain) {
    static const char* domains[] = {"Datos epidemiologicos", "Datos atmosfericos", "Datos demograficos", "Tiempo", "Territorio"};
    if (domain < 0) domain = 0;
    if (domain > 4) domain = 4;
    return domains[domain];
}

const char* tlac_eje_campo_label(int domain, int field) {
    static const char* fields[5][5] = {
        {"Incidencia semanal", "Enfermedad", "Grupo de enfermedad", "Acumulado femenino", "Acumulado masculino"},
        {"Contaminante", "Concentración", "Estación RAMA", "Promedio semanal", "Desfase"},
        {"Población total", "Densidad", "Estructura etaria", "Distribución por sexo", "Índice demográfico"},
        {"Semana", "Año", "Periodo", "Rango temporal", "Desfase"},
        {"Alcaldía", "Municipio", "Entidad", "Coordenada X", "Coordenada Y"}
    };
    if (domain < 0) domain = 0;
    if (domain > 4) domain = 4;
    if (field < 0) field = 0;
    if (field > 4) field = 4;
    return fields[domain][field];
}

static int tlac_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float tlac_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}


int tlac_lag_unit_clean(int unit) {
    return tlac_clampi(unit, 0, 3);
}

int tlac_lag_slider_max(int unit) {
    switch (tlac_lag_unit_clean(unit)) {
        case 1: return 260;
        case 2: return 60;
        case 3: return 5;
        default: return 1825;
    }
}

const char* tlac_lag_unit_label(int unit) {
    switch (tlac_lag_unit_clean(unit)) {
        case 1: return "semanas";
        case 2: return "meses";
        case 3: return "anios";
        default: return "dias";
    }
}

float tlac_oz_legend_height(int disease_count) {
    if (disease_count <= 0) return 0.0f;
    const int rows = (disease_count + 2) / 3;
    return 30.0f + (float)rows * 24.0f;
}


TlacPlot tlac_plot_rect(float x0, float y0, float x1, float y1) {
    TlacPlot p;
    memset(&p, 0, sizeof(p));
    if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1)) return p;
    if (x1 < x0) { const float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { const float t = y0; y0 = y1; y1 = t; }
    const float aw = x1 - x0;
    const float ah = y1 - y0;
    if (aw <= 1.0f || ah <= 1.0f) return p;
    p.left = tlac_clampf(aw * 0.03444185f, 50.0f, 78.0f);
    p.right = tlac_clampf(aw * 0.01315562f, 18.0f, 34.0f);
    p.top = tlac_clampf(ah * 0.05572809f, 40.0f, 64.0f);
    p.bottom = tlac_clampf(ah * 0.05572809f, 44.0f, 68.0f);
    p.x0 = x0 + p.left;
    p.y0 = y0 + p.top;
    p.x1 = x1 - p.right;
    p.y1 = y1 - p.bottom;
    p.w = p.x1 - p.x0;
    p.h = p.y1 - p.y0;
    p.ok = p.w > 16.0f && p.h > 16.0f;
    return p;
}

float tlac_plot_y_i64(const TlacPlot* p, int64_t value, int64_t max_value) {
    if (!p || !p->ok) return 0.0f;
    const float denom = (float)(max_value > 1 ? max_value : 1);
    float t = (float)value / denom;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return p->y1 - t * p->h;
}

float tlac_plot_x_index(const TlacPlot* p, int index, int count) {
    if (!p || !p->ok) return 0.0f;
    const int n = count > 1 ? count : 1;
    return p->x0 + ((float)index + 0.5f) * p->w / (float)n;
}

int tlac_hist_bin_count(size_t n) {
    int b = (int)(sqrt((double)n) + 2.0);
    if (b < 6) b = 6;
    if (b > 18) b = 18;
    return b;
}

int tlac_hist_bin_i64(int64_t value, int64_t min_value, int64_t range, int bins) {
    if (bins <= 1) return 0;
    if (range < 1) range = 1;
    int bin = (int)(((value - min_value) * (int64_t)bins) / (range + 1));
    if (bin < 0) bin = 0;
    if (bin >= bins) bin = bins - 1;
    return bin;
}


TlacBoxF tlac_box_xywh(float x, float y, float w, float h) {
    TlacBoxF b;
    if (!(w > 0.0f)) w = 1.0f;
    if (!(h > 0.0f)) h = 1.0f;
    b.x0 = x;
    b.y0 = y;
    b.x1 = x + w;
    b.y1 = y + h;
    b.w = w;
    b.h = h;
    return b;
}

TlacBoxF tlac_box_xyxy(float x0, float y0, float x1, float y1) {
    TlacBoxF b;
    if (!(x1 > x0)) x1 = x0 + 1.0f;
    if (!(y1 > y0)) y1 = y0 + 1.0f;
    b.x0 = x0;
    b.y0 = y0;
    b.x1 = x1;
    b.y1 = y1;
    b.w = x1 - x0;
    b.h = y1 - y0;
    return b;
}

TlacBoxF tlac_tile_icon_box(const TlacBoxF* tile) {
    if (!tile) return tlac_box_xywh(0.0f, 0.0f, 1.0f, 1.0f);
    return tlac_box_xyxy(tile->x0 + tile->w * 0.05572809f,
                         tile->y0 + tile->h * 0.09016994f,
                         tile->x0 + tile->w * (1.0f - 0.05572809f),
                         tile->y0 + tile->h * 0.50f);
}

void tlac_tile_text_xy(const TlacBoxF* tile, float line_h, float* x, float* y0, float* y1) {
    if (!tile) {
        if (x) *x = 0.0f;
        if (y0) *y0 = 0.0f;
        if (y1) *y1 = line_h > 0.0f ? line_h : 14.0f;
        return;
    }
    if (!(line_h > 0.0f)) line_h = 14.0f;
    if (x) *x = tile->x0 + tile->w * 0.09016994f;
    if (y0) *y0 = tile->y0 + tile->h * 0.58f;
    if (y1) *y1 = tile->y0 + tile->h * 0.58f + line_h * 1.18f;
}

int tlac_box_edges(const TlacBoxF* box, TlacSegF out_edges4[4]) {
    if (!box || !out_edges4) return 0;
    out_edges4[0].x0 = box->x0; out_edges4[0].y0 = box->y0; out_edges4[0].x1 = box->x1; out_edges4[0].y1 = box->y0;
    out_edges4[1].x0 = box->x1; out_edges4[1].y0 = box->y0; out_edges4[1].x1 = box->x1; out_edges4[1].y1 = box->y1;
    out_edges4[2].x0 = box->x1; out_edges4[2].y0 = box->y1; out_edges4[2].x1 = box->x0; out_edges4[2].y1 = box->y1;
    out_edges4[3].x0 = box->x0; out_edges4[3].y0 = box->y1; out_edges4[3].x1 = box->x0; out_edges4[3].y1 = box->y0;
    return 4;
}

int tlac_px_rect(float x0, float y0, float x1, float y1, float sx, float sy, int fb_w, int fb_h, TlacRectI* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1)) return 0;
    if (!isfinite(sx) || sx < 0.1f) sx = 0.1f;
    if (!isfinite(sy) || sy < 0.1f) sy = 0.1f;
    if (fb_w <= 1 || fb_h <= 1) return 0;
    if (x1 < x0) { const float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { const float t = y0; y0 = y1; y1 = t; }
    const int ix0 = tlac_clampi((int)floor((double)(x0 * sx)), 0, fb_w - 1);
    const int ix1 = tlac_clampi((int)ceil((double)(x1 * sx)), 0, fb_w);
    const int iy0 = tlac_clampi((int)floor((double)(y0 * sy)), 0, fb_h - 1);
    const int iy1 = tlac_clampi((int)ceil((double)(y1 * sy)), 0, fb_h);
    out->x0 = ix0;
    out->y0 = iy0;
    out->x1 = ix1;
    out->y1 = iy1;
    out->w = ix1 > ix0 ? ix1 - ix0 : 0;
    out->h = iy1 > iy0 ? iy1 - iy0 : 0;
    out->gl_y = tlac_clampi(fb_h - iy1, 0, fb_h - 1);
    return out->w > 1 && out->h > 1;
}


static int tlac_lag_clamp(int v, int hi) {
    if (hi < 0) hi = 0;
    if (hi > TLAC_LAG_MAX) hi = TLAC_LAG_MAX;
    return v < 0 ? 0 : (v > hi ? hi : v);
}

size_t tlac_lag_seed_fill(int max_lag, int* out, size_t out_cap) {
    if (!out || out_cap == 0) return 0;
    max_lag = tlac_lag_clamp(max_lag, TLAC_LAG_MAX);
    unsigned char seen[TLAC_LAG_MAX + 1];
    memset(seen, 0, sizeof(seen));
    size_t n = 0;
#define TLAC_ADD_LAG_(v_) do { \
        const int lag_ = tlac_lag_clamp((int)(v_), max_lag); \
        if (!seen[lag_] && n < out_cap) { seen[lag_] = 1u; out[n++] = lag_; } \
    } while (0)
    TLAC_ADD_LAG_(0);
    for (int i = 0; i < 72; ++i) TLAC_ADD_LAG_((53 + i * 137) % (max_lag + 1));
    for (int lag = 0; lag <= max_lag; lag += 91) TLAC_ADD_LAG_(lag);
    for (int lag = 0; lag <= max_lag; lag += 28) TLAC_ADD_LAG_(lag);
    for (int lag = 0; lag <= max_lag; lag += 7) TLAC_ADD_LAG_(lag);
#undef TLAC_ADD_LAG_
    return n;
}

int tlac_lag_seen(const unsigned char* seen, size_t seen_n, int lag) {
    if (!seen || lag < 0 || (size_t)lag >= seen_n) return 0;
    return seen[lag] != 0;
}

int tlac_lag_mark(unsigned char* seen, size_t seen_n, int lag) {
    if (!seen || lag < 0 || (size_t)lag >= seen_n) return 0;
    if (seen[lag]) return 0;
    seen[lag] = 1u;
    return 1;
}

static int tlac_lag_score_used(const int* used, size_t used_n, int idx) {
    for (size_t i = 0; i < used_n; ++i) if (used[i] == idx) return 1;
    return 0;
}

int tlac_lag_next_region(const TlacLagScore* scores, size_t score_n,
                         const unsigned char* seen, size_t seen_n,
                         int* radius_io, int* rank_io, int max_lag) {
    (void)rank_io;
    if (!scores || score_n == 0 || !seen || seen_n == 0) return -1;
    max_lag = tlac_lag_clamp(max_lag, TLAC_LAG_MAX);
    int top[8];
    size_t top_n = 0;
    for (size_t pick = 0; pick < 8 && pick < score_n; ++pick) {
        int best = -1;
        for (size_t i = 0; i < score_n; ++i) {
            if (tlac_lag_score_used(top, top_n, (int)i)) continue;
            if (best < 0 || scores[i].score > scores[best].score ||
                (scores[i].score == scores[best].score && scores[i].lag < scores[best].lag)) {
                best = (int)i;
            }
        }
        if (best < 0) break;
        top[top_n++] = best;
    }
    int radius0 = radius_io && *radius_io > 1 ? *radius_io : 1;
    for (int radius = radius0; radius <= 63; ++radius) {
        for (size_t rank = 0; rank < top_n; ++rank) {
            const int center = tlac_lag_clamp(scores[top[rank]].lag, max_lag);
            for (int sign_i = 0; sign_i < 2; ++sign_i) {
                const int sign = sign_i == 0 ? -1 : 1;
                const int lag = tlac_lag_clamp(center + sign * radius, max_lag);
                if (!tlac_lag_seen(seen, seen_n, lag)) {
                    if (radius_io) *radius_io = radius;
                    if (rank_io) *rank_io = (int)rank;
                    return lag;
                }
            }
        }
    }
    return -1;
}


/* Tonal: calendario civil y tonalli en C plano.
   Mantener aquí evita dispersar aritmética cronológica entre draw calls y lambdas C++.
   No usa heap, excepciones ni contenedores; sólo enteros de ancho conocido. */
int64_t tonal_days_from_civil(int y, unsigned m, unsigned d) {
    if (m < 1u) m = 1u;
    if (m > 12u) m = 12u;
    y -= m <= 2u;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned mm = m + (m > 2u ? (unsigned)-3 : 9u);
    const unsigned doy = (153u * mm + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (int64_t)era * 146097LL + (int64_t)doe - 719468LL;
}

void tonal_civil_from_days(int64_t z, int* y, unsigned* m, unsigned* d) {
    if (!y || !m || !d) return;
    z += 719468LL;
    const int64_t era = (z >= 0 ? z : z - 146096LL) / 146097LL;
    const unsigned doe = (unsigned)(z - era * 146097LL);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int yy = (int)yoe + (int)era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned dd = doy - (153u * mp + 2u) / 5u + 1u;
    const unsigned mm = mp + (mp < 10u ? 3u : (unsigned)-9);
    yy += (mm <= 2u);
    *y = yy;
    *m = mm;
    *d = dd;
}

int64_t tonal_div_floor_i64(int64_t a, int64_t b) {
    if (b <= 0) return 0;
    int64_t q = a / b;
    const int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

int64_t tonal_mod_pos_i64(int64_t a, int64_t b) {
    if (b <= 0) return 0;
    int64_t r = a % b;
    return r < 0 ? r + b : r;
}

int tonal_is_leap_year(int year) {
    return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
}

unsigned tonal_days_in_month(int year, unsigned month) {
    static const unsigned days[13] = {0u,31u,28u,31u,30u,31u,30u,31u,31u,30u,31u,30u,31u};
    if (month < 1u) month = 1u;
    if (month > 12u) month = 12u;
    return month == 2u && tonal_is_leap_year(year) ? 29u : days[month];
}

int tonal_iso_weeks_in_year(int year) {
    const int64_t jan1 = tonal_days_from_civil(year, 1u, 1u);
    const int iso_wday = (int)tonal_mod_pos_i64(jan1 + 3LL, 7LL) + 1;
    return (iso_wday == 4 || (iso_wday == 3 && tonal_is_leap_year(year))) ? 53 : 52;
}

int64_t tonal_hour_from_civil(int start_year, int y, unsigned m, unsigned d, unsigned hour) {
    if (hour > 23u) hour = 23u;
    return (tonal_days_from_civil(y, m, d) - tonal_days_from_civil(start_year, 1u, 1u)) * 24LL + (int64_t)hour;
}

void tonal_hour_to_civil(int start_year, int64_t hour, int* y, unsigned* m, unsigned* d, unsigned* h) {
    const int64_t day = tonal_days_from_civil(start_year, 1u, 1u) + tonal_div_floor_i64(hour, 24LL);
    tonal_civil_from_days(day, y, m, d);
    if (h) *h = (unsigned)tonal_mod_pos_i64(hour, 24LL);
}

static size_t tonal_copy_lit(const char* lit, char* out, size_t cap) {
    const size_t n = lit ? strlen(lit) : 0u;
    if (out && cap) {
        const size_t m = n < cap - 1u ? n : cap - 1u;
        if (m) memcpy(out, lit, m);
        out[m] = '\0';
    }
    return n;
}



/* amo_line_clean_copy: compacta una línea de bitácora en una sola pasada.
   Es intencionalmente destructiva respecto a CR/LF/TAB y colapsa blancos: la UI
   no necesita preservar control chars; la ruta queda sin std::remove/find loops. */
size_t amo_line_clean_copy(const char* s, size_t n, char* out, size_t out_cap) {
    size_t w = 0u;
    int pending_space = 0;
    int emitted = 0;
    if (!s) n = 0u;
    for (size_t i = 0u; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = ' ';
        if (c == ' ') {
            if (emitted) pending_space = 1;
            continue;
        }
        if (pending_space) {
            if (out && out_cap && w + 1u < out_cap) out[w] = ' ';
            ++w;
            pending_space = 0;
        }
        if (out && out_cap && w + 1u < out_cap) out[w] = (char)c;
        ++w;
        emitted = 1;
    }
    if (out && out_cap) out[w < out_cap ? w : out_cap - 1u] = '\0';
    return w;
}

size_t amo_log_hms_copy(char* out, size_t out_cap) {
    if (!out || out_cap == 0u) return 8u;
    time_t t = time((time_t*)0);
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
#ifdef _WIN32
    if (localtime_s(&tmv, &t) != 0) {
        return tonal_copy_lit("00:00:00", out, out_cap);
    }
#else
    {
        const struct tm* tmp = localtime(&t);
        if (!tmp) return tonal_copy_lit("00:00:00", out, out_cap);
        tmv = *tmp;
    }
#endif
    const int rc = snprintf(out, out_cap, "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    if (rc < 0) return tonal_copy_lit("00:00:00", out, out_cap);
    return (size_t)rc;
}

static void tonal_now_tm(struct tm* tmv) {
    if (!tmv) return;
    time_t t = time((time_t*)0);
    memset(tmv, 0, sizeof(*tmv));
#ifdef _WIN32
    if (t == (time_t)-1 || localtime_s(tmv, &t) != 0) {
        tmv->tm_year = 2026 - 1900;
        tmv->tm_mon = 0;
        tmv->tm_mday = 1;
        tmv->tm_hour = 0;
        tmv->tm_min = 0;
    }
#else
    if (t == (time_t)-1) {
        tmv->tm_year = 2026 - 1900;
        tmv->tm_mon = 0;
        tmv->tm_mday = 1;
        tmv->tm_hour = 0;
        tmv->tm_min = 0;
    } else {
        const struct tm* tmp = localtime(&t);
        if (tmp) *tmv = *tmp;
    }
#endif
}

int64_t tonal_nav_max_hour_now(int start_year, int min_year, int max_year, int future_months, int fallback_year) {
    const time_t t = time((time_t*)0);
    if (t == (time_t)-1) return tonal_hour_from_civil(start_year, fallback_year, 12u, 31u, 23u);
    const time_t bucket = t / 60;
    static time_t last_bucket = (time_t)-1;
    static int64_t last_hour = 0;
    if (bucket == last_bucket && last_hour != 0) return last_hour;

    struct tm lt;
    tonal_now_tm(&lt);
    int y = lt.tm_year + 1900;
    int mo = lt.tm_mon + 1 + future_months;
    while (mo > 12) { mo -= 12; ++y; }
    while (mo < 1) { mo += 12; --y; }
    if (y < min_year) y = min_year;
    if (y > max_year) y = max_year;
    if (mo < 1) mo = 1;
    if (mo > 12) mo = 12;
    unsigned d = (unsigned)(lt.tm_mday < 1 ? 1 : lt.tm_mday);
    const unsigned dim = tonal_days_in_month(y, (unsigned)mo);
    if (d > dim) d = dim;
    int h = lt.tm_hour;
    if (h < 0) h = 0;
    if (h > 23) h = 23;
    last_hour = tonal_hour_from_civil(start_year, y, (unsigned)mo, d, (unsigned)h);
    last_bucket = bucket;
    return last_hour;
}

int64_t tonal_local_hour_now(int start_year, int min_year, int max_year) {
    struct tm lt;
    tonal_now_tm(&lt);
    int y = lt.tm_year + 1900;
    if (y < min_year) y = min_year;
    if (y > max_year) y = max_year;
    int mo = lt.tm_mon + 1;
    if (mo < 1) mo = 1;
    if (mo > 12) mo = 12;
    unsigned d = (unsigned)(lt.tm_mday < 1 ? 1 : lt.tm_mday);
    const unsigned dim = tonal_days_in_month(y, (unsigned)mo);
    if (d > dim) d = dim;
    int h = lt.tm_hour;
    if (h < 0) h = 0;
    if (h > 23) h = 23;
    return tonal_hour_from_civil(start_year, y, (unsigned)mo, d, (unsigned)h);
}

int tonal_local_minute_now(void) {
    struct tm lt;
    tonal_now_tm(&lt);
    if (lt.tm_min < 0) return 0;
    if (lt.tm_min > 59) return 59;
    return lt.tm_min;
}

int64_t tonal_clamp_hour(int64_t hour, int64_t lo, int64_t hi) {
    if (lo > hi) { const int64_t t = lo; lo = hi; hi = t; }
    if (hour < lo) return lo;
    if (hour > hi) return hi;
    return hour;
}

double tlac_scale_nice_m(double target_m) {
    static const double steps[8] = {1.0, 1.5, 2.0, 2.5, 3.0, 5.0, 7.5, 10.0};
    if (!isfinite(target_m) || target_m <= 0.0) return 1000.0;
    const double mag = pow(10.0, floor(log10(target_m)));
    const double norm = target_m / mag;
    double chosen = 1.0;
    for (size_t i = 0u; i < 8u; ++i) {
        if (norm + 1.0e-9 >= steps[i]) chosen = steps[i];
    }
    return chosen * mag;
}

size_t tlac_dist_label_copy(double meters, char* out, size_t out_cap) {
    if (!out || out_cap == 0u) return 0u;
    int rc = 0;
    if (!isfinite(meters) || meters <= 0.0) {
        return tonal_copy_lit("0 m", out, out_cap);
    }
    if (meters >= 1000.0) {
        const double km = meters / 1000.0;
        const double near_i = fabs(km - floor(km + 0.5));
        if (km >= 10.0 || near_i < 0.05) rc = snprintf(out, out_cap, "%lld km", (long long)floor(km + 0.5));
        else rc = snprintf(out, out_cap, "%.1f km", km);
    } else {
        rc = snprintf(out, out_cap, "%lld m", (long long)floor(meters + 0.5));
    }
    if (rc < 0) return tonal_copy_lit("0 m", out, out_cap);
    return (size_t)rc;
}

size_t tlac_i64_group_copy(int64_t value, char* out, size_t out_cap) {
    char tmp[32];
    char rev[48];
    size_t n = 0u, w = 0u;
    int neg = value < 0;
    uint64_t u = neg ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u && n < sizeof(tmp));
    for (size_t i = 0u; i < n; ++i) {
        if (i && (i % 3u) == 0u) rev[w++] = ',';
        rev[w++] = tmp[i];
    }
    size_t total = w + (neg ? 1u : 0u);
    if (out && out_cap) {
        size_t p = 0u;
        if (neg && p + 1u < out_cap) out[p++] = '-';
        for (size_t i = 0u; i < w; ++i) {
            const char c = rev[w - 1u - i];
            if (p + 1u < out_cap) out[p++] = c;
        }
        out[p < out_cap ? p : out_cap - 1u] = '\0';
    }
    return total;
}

size_t tlac_lonlat_label_copy(double lon, double lat, char* out, size_t out_cap) {
    if (!out || out_cap == 0u) return 0u;
    if (!isfinite(lon)) lon = 0.0;
    if (!isfinite(lat)) lat = 0.0;
    const int rc = snprintf(out, out_cap, "%.4f°%c %.4f°%c",
                            fabs(lat), lat >= 0.0 ? 'N' : 'S',
                            fabs(lon), lon >= 0.0 ? 'E' : 'O');
    if (rc < 0) return tonal_copy_lit("0.0000°N 0.0000°O", out, out_cap);
    return (size_t)rc;
}


int tlal_ix_week_name_parts(const char* stem, size_t n, int* year, int* month, int* day) {
    if (!stem || n != 10u) return 0;
    if (stem[4] != '_' || stem[7] != '_') return 0;
    for (size_t i = 0u; i < n; ++i) {
        if (i == 4u || i == 7u) continue;
        if (stem[i] < '0' || stem[i] > '9') return 0;
    }
    const int y = (stem[0]-'0')*1000 + (stem[1]-'0')*100 + (stem[2]-'0')*10 + (stem[3]-'0');
    const int m = (stem[5]-'0')*10 + (stem[6]-'0');
    const int d = (stem[8]-'0')*10 + (stem[9]-'0');
    if (y < 1900 || y > 2300 || m < 1 || m > 12) return 0;
    if (!(d == 1 || d == 8 || d == 15 || d == 22 || d == 29)) return 0;
    if (year) *year = y;
    if (month) *month = m;
    if (day) *day = d;
    return 1;
}

static int tlal_atm_take_nums(const char* s, size_t n, int* out, int out_cap) {
    int count = 0;
    int cur = -1;
    if (!out || out_cap <= 0) return 0;
    for (size_t i = 0u; i < n; ++i) {
        const unsigned char ch = (unsigned char)s[i];
        if (ch >= '0' && ch <= '9') {
            if (cur < 0) cur = 0;
            if (cur < 100000000) cur = cur * 10 + (int)(ch - '0');
        } else if (cur >= 0) {
            if (count < out_cap) out[count++] = cur;
            cur = -1;
        }
    }
    if (cur >= 0 && count < out_cap) out[count++] = cur;
    return count;
}

static int tlal_atm_digits_only_u32(const char* s, size_t n, uint32_t* out) {
    uint32_t v = 0u;
    size_t digits = 0u;
    if (!s || !out) return 0;
    while (n && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' || *s == '"')) { ++s; --n; }
    while (n && (s[n - 1u] == ' ' || s[n - 1u] == '\t' || s[n - 1u] == '\r' || s[n - 1u] == '\n' || s[n - 1u] == '"')) --n;
    for (size_t i = 0u; i < n; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c < '0' || c > '9') return 0;
        v = v * 10u + (uint32_t)(c - '0');
        ++digits;
    }
    if (digits == 0u) return 0;
    *out = v;
    return (int)digits;
}

static int tlal_atm_two_digit_year(int y) {
    return y >= 100 ? y : (y >= 70 ? 1900 + y : 2000 + y);
}

static int tlal_atm_leap(int y) {
    return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
}

static int tlal_atm_month_days(int y, int m) {
    static const unsigned char mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 0;
    if (m == 2) return 28 + tlal_atm_leap(y);
    return mdays[m - 1];
}

static int tlal_atm_valid_ymdhms(int y, int m, int d, int h, int mi, int se) {
    const int md = tlal_atm_month_days(y, m);
    return y >= 1900 && y <= 2300 && md > 0 && d >= 1 && d <= md &&
           h >= 0 && h <= 23 && mi >= 0 && mi <= 59 && se >= 0 && se <= 59;
}

static int tlal_atm_assign_date3(int a, int b, int c, int* y, int* m, int* d) {
    if (a >= 1900) { *y = a; *m = b; *d = c; return 1; }
    if (c >= 1900 || c < 100) {
        *y = tlal_atm_two_digit_year(c);
        /* México usa día/mes/año; si el segundo campo vuelve imposible ese orden,
           se acepta mes/día/año para archivos externos. */
        if (b > 12 && a >= 1 && a <= 12) { *m = a; *d = b; }
        else { *d = a; *m = b; }
        return 1;
    }
    return 0;
}

static void tlal_atm_parse_time_parts(const char* s, size_t n, const int* nums, int count, int* h, int* mi, int* se) {
    uint32_t compact = 0u;
    int digits;
    if (!h || !mi || !se) return;
    digits = tlal_atm_digits_only_u32(s, n, &compact);
    if (digits == 4) { *h = (int)(compact / 100u); *mi = (int)(compact % 100u); return; }
    if (digits == 3) { *h = (int)(compact / 100u); *mi = (int)(compact % 100u); return; }
    if (digits == 6) { *h = (int)(compact / 10000u); *mi = (int)((compact / 100u) % 100u); *se = (int)(compact % 100u); return; }
    if (count > 0) {
        if (nums[0] >= 100 && nums[0] <= 2359) { *h = nums[0] / 100; *mi = nums[0] % 100; }
        else { *h = nums[0]; if (count > 1) *mi = nums[1]; if (count > 2) *se = nums[2]; }
    }
}

int tlal_atm_parse_stamp(const char* date, size_t date_n, const char* time, size_t time_n, TlalAtmStamp* out) {
    int dn[8];
    int tn[4];
    int dc, tc;
    int y = 0, m = 0, d = 0, h = 0, mi = 0, se = 0;
    uint32_t compact = 0u;
    int compact_digits;
    if (!out || !date || date_n == 0u) return 0;
    memset(out, 0, sizeof(*out));

    compact_digits = tlal_atm_digits_only_u32(date, date_n, &compact);
    if (compact_digits == 8) {
        if (compact >= 19000101u) { y = (int)(compact / 10000u); m = (int)((compact / 100u) % 100u); d = (int)(compact % 100u); }
        else { d = (int)(compact / 1000000u); m = (int)((compact / 10000u) % 100u); y = tlal_atm_two_digit_year((int)(compact % 10000u)); }
    } else if (compact_digits == 6) {
        const int a = (int)(compact / 10000u);
        const int b = (int)((compact / 100u) % 100u);
        const int c = (int)(compact % 100u);
        if (a >= 70) { y = 1900 + a; m = b; d = c; }
        else { y = 2000 + a; m = b; d = c; }
    } else {
        dc = tlal_atm_take_nums(date, date_n, dn, 8);
        if (dc < 3) return 0;
        if (!tlal_atm_assign_date3(dn[0], dn[1], dn[2], &y, &m, &d)) return 0;
        if (!time || time_n == 0u) {
            if (dc > 3) tlal_atm_parse_time_parts(NULL, 0u, dn + 3, dc - 3, &h, &mi, &se);
        }
    }

    tc = (time && time_n) ? tlal_atm_take_nums(time, time_n, tn, 4) : 0;
    if (tc > 0) tlal_atm_parse_time_parts(time, time_n, tn, tc, &h, &mi, &se);

    if (h == 24 && mi == 0 && se == 0) {
        /* RAMA/REDMA suelen etiquetar cierres horarios 1..24; se conserva la
           fecha civil y se ancla el último bloque a 23:00 para no crear un día
           artificial ni romper series horarias. */
        h = 23;
    }
    if (!tlal_atm_valid_ymdhms(y, m, d, h, mi, se)) return 0;
    out->year = y; out->month = m; out->day = d; out->hour = h; out->minute = mi; out->second = se;
    return 1;
}

static double tlal_scale(int z) {
    if (z < 0) z = 0;
    if (z > 30) z = 30;
    return ldexp(256.0, z);
}

double tlal_lon_px(double lon, int z) {
    return (lon + 180.0) / 360.0 * tlal_scale(z);
}

double tlal_lat_px(double lat, int z) {
    const double pi = 3.14159265358979323846;
    if (lat < -85.05112878) lat = -85.05112878;
    if (lat > 85.05112878) lat = 85.05112878;
    const double s = sin(lat * pi / 180.0);
    return (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * pi)) * tlal_scale(z);
}

void tlal_px_lonlat(double x, double y, int z, double* out_lon, double* out_lat) {
    const double pi = 3.14159265358979323846;
    const double scale = tlal_scale(z);
    const double lon = x / scale * 360.0 - 180.0;
    const double n = pi - 2.0 * pi * y / scale;
    const double lat = 180.0 / pi * atan(0.5 * (exp(n) - exp(-n)));
    if (out_lon) *out_lon = lon;
    if (out_lat) *out_lat = lat;
}


static int tlal_clampi(int v, int lo, int hi) {
    if (lo > hi) { const int t = lo; lo = hi; hi = t; }
    return v < lo ? lo : (v > hi ? hi : v);
}

int tlal_lod_desired_z(double scale_den, int center_inside_buffer, int moving, float idle_seconds,
                       int z_overview, int z_300k, int z_90k, int z_30k, int z_15k_idle) {
    if (!isfinite(scale_den) || scale_den < 1.0) scale_den = 1000000.0;
    if (!isfinite((double)idle_seconds) || idle_seconds < 0.0f) idle_seconds = 0.0f;

    int z = z_overview;
    if (scale_den <= 350000.0) z = z_300k;
    if (scale_den <= 110000.0) z = z_90k;
    if (scale_den <= 36000.0) z = z_30k;
    if (scale_den <= 15500.0) z = z_30k + 1;
    if (scale_den <= 9000.0) z = z_15k_idle;

    if (!center_inside_buffer && z > z_90k) z = z_90k;
    if (moving) {
        const int cap = center_inside_buffer ? z_90k : z_300k;
        if (z > cap) z = cap;
    } else if (idle_seconds < 0.35f) {
        const int cap = center_inside_buffer ? z_90k : z_300k;
        if (z > cap) z = cap;
    } else if (idle_seconds < 1.30f) {
        const int cap = center_inside_buffer ? z_30k : z_90k;
        if (z > cap) z = cap;
    }
    if (z < z_overview) z = z_overview;
    if (z > z_15k_idle) z = z_15k_idle;
    return z;
}

int tlal_lod_step_z(int current_z, int desired_z, int z_min, int z_max, int max_step) {
    if (max_step < 1) max_step = 1;
    current_z = tlal_clampi(current_z, z_min, z_max);
    desired_z = tlal_clampi(desired_z, z_min, z_max);
    if (desired_z > current_z + max_step) return current_z + max_step;
    return desired_z;
}

int tlal_tile_margin(float pitch) {
    if (!isfinite((double)pitch) || pitch <= 0.0001f) return 2;
    if (pitch < 0.42f) return 3;
    if (pitch < 0.92f) return 4;
    return 5;
}

size_t tlal_tile_draw_cap(int z, float pitch, float screen_w, float screen_h) {
    (void)screen_w;
    (void)screen_h;
    const float tile_w = screen_w > 0.0f ? screen_w / 256.0f : 8.0f;
    const float tile_h = screen_h > 0.0f ? screen_h / 256.0f : 5.0f;
    size_t cap = (size_t)((tile_w + 6.0f) * (tile_h + 6.0f));
    if (z >= 15) cap += 64u;
    if (z >= 18) cap += 96u;
    if (pitch > 0.42f) cap += 96u;
    if (pitch > 0.92f) cap += 128u;
    if (cap < 160u) cap = 160u;
    if (cap > 512u) cap = 512u;
    return cap;
}

int tlal_stream_budget(int moving, int startup_boost, float app_uptime, float warmup_seconds,
                       float idle_seconds, float deep_idle_seconds, int max_per_frame) {
    if (max_per_frame < 0) max_per_frame = 0;
    if (!isfinite((double)app_uptime)) app_uptime = 0.0f;
    if (!isfinite((double)idle_seconds)) idle_seconds = 0.0f;
    if (startup_boost) return max_per_frame > 4 ? 4 : max_per_frame;
    if (app_uptime < warmup_seconds) return 0;
    if (moving) return max_per_frame > 0 ? 1 : 0;
    if (idle_seconds >= deep_idle_seconds) return max_per_frame > 3 ? 3 : max_per_frame;
    return max_per_frame > 2 ? 2 : max_per_frame;
}

float tlal_fog_strength(float manual, double rh, double pm25, double aod, int moving, float idle_seconds) {
    if (isfinite((double)manual) && manual >= 0.0f) {
        if (manual > 1.0f) manual = 1.0f;
        return manual;
    }
    double f = 0.0;
    if (isfinite(rh) && rh > 0.0) f += (rh > 100.0 ? 100.0 : rh) / 100.0 * 0.16;
    if (isfinite(pm25) && pm25 > 0.0) f += (pm25 > 75.0 ? 75.0 : pm25) / 75.0 * 0.24;
    if (isfinite(aod) && aod > 0.0) f += (aod > 1.2 ? 1.2 : aod) / 1.2 * 0.28;
    if (moving) f += 0.10;
    else if (idle_seconds < 0.45f) f += 0.055;
    if (f > 0.56) f = 0.56;
    if (f < 0.0) f = 0.0;
    return (float)f;
}

double tlal_mpp(double lat_deg, int z, double scale) {
    const double pi = 3.14159265358979323846;
    const double earth = 6378137.0;
    if (!isfinite(lat_deg)) lat_deg = 19.4326;
    if (!isfinite(scale) || scale <= 1.0e-9) scale = 1.0;
    z = tlal_clampi(z, 0, 22);
    if (lat_deg < -85.0) lat_deg = -85.0;
    if (lat_deg > 85.0) lat_deg = 85.0;
    return cos(lat_deg * pi / 180.0) * 2.0 * pi * earth / (256.0 * (double)(1u << (unsigned)z)) / scale;
}

float tlal_m_to_px(double meters, double mpp, float min_px, float max_px) {
    if (!isfinite(meters) || meters <= 0.0 || !isfinite(mpp) || mpp <= 0.0) return min_px;
    if (max_px < min_px) { const float t = max_px; max_px = min_px; min_px = t; }
    const double px = meters / mpp;
    if (px < (double)min_px) return min_px;
    if (px > (double)max_px) return max_px;
    return (float)px;
}

float tlal_boundary_px(double mpp, int has_data, int out_of_focus) {
    (void)has_data;
    const float min_px = out_of_focus ? 0.22f : 0.30f;
    return tlal_m_to_px(10.0, mpp, min_px, 64.0f);
}

int tlal_boundary_alpha(double mpp, int has_data, int out_of_focus) {
    if (!isfinite(mpp) || mpp <= 0.0) mpp = 100.0;
    /* t crece cerca de calle: 10 m ocupan muchos pixeles; a escala regional
       la línea se vuelve información latente y no una malla blanca gruesa. */
    double t = 10.0 / mpp;
    if (t < 0.0) t = 0.0;
    if (t > 18.0) t = 18.0;
    t /= 18.0;
    /* Curva cúbica suave sin pow(): mucha diferencia perceptual cerca, poca lejos. */
    const double s = t * t * (3.0 - 2.0 * t);
    int a = (int)(18.0 + s * (has_data ? 160.0 : 126.0));
    if (out_of_focus) a = (int)((double)a * 0.48);
    return tlal_clampi(a, out_of_focus ? 5 : 9, has_data ? 188 : 148);
}

int tlal_vertex_stride(double mpp, int source_count) {
    if (source_count <= 0) return 1;
    if (!isfinite(mpp) || mpp <= 0.0) mpp = 80.0;
    int s = 1;
    if (mpp > 320.0) s = 24;
    else if (mpp > 160.0) s = 18;
    else if (mpp > 80.0) s = 12;
    else if (mpp > 40.0) s = 8;
    else if (mpp > 20.0) s = 5;
    else if (mpp > 8.0) s = 3;
    else if (mpp > 2.2) s = 2;
    if (source_count / s < 12) s = source_count > 12 ? source_count / 12 : 1;
    if (s < 1) s = 1;
    return s;
}

float tlal_fog_view(float fog, float pitch) {
    if (!isfinite((double)fog) || fog <= 0.0f) return 0.0f;
    if (!isfinite((double)pitch) || pitch < 0.0f) pitch = 0.0f;
    if (pitch > 1.0f) pitch = 1.0f;
    const float p2 = pitch * pitch;
    const float view_gain = 0.23f + 0.92f * p2 * (1.0f + 0.22f * pitch);
    float out = fog * view_gain;
    if (out > 0.72f) out = 0.72f;
    return out;
}


int tlal_atm_mesh_enabled(const char* key, size_t n) {
    if (!key || n == 0u) return 0;
    if (ozmvm_span_eq_lit(key, n, "wdr")) return 0;
    if (ozmvm_span_eq_lit(key, n, "o3") || ozmvm_span_eq_lit(key, n, "pm10") ||
        ozmvm_span_eq_lit(key, n, "pm25") || ozmvm_span_eq_lit(key, n, "pmco") ||
        ozmvm_span_eq_lit(key, n, "co") || ozmvm_span_eq_lit(key, n, "no") ||
        ozmvm_span_eq_lit(key, n, "no2") || ozmvm_span_eq_lit(key, n, "nox") ||
        ozmvm_span_eq_lit(key, n, "so2") || ozmvm_span_eq_lit(key, n, "h2s") ||
        ozmvm_span_eq_lit(key, n, "tmp") || ozmvm_span_eq_lit(key, n, "tmax") ||
        ozmvm_span_eq_lit(key, n, "tmin") || ozmvm_span_eq_lit(key, n, "rh") ||
        ozmvm_span_eq_lit(key, n, "pp") || ozmvm_span_eq_lit(key, n, "pa") ||
        ozmvm_span_eq_lit(key, n, "wsp") || ozmvm_span_eq_lit(key, n, "gr") ||
        ozmvm_span_eq_lit(key, n, "uv") || ozmvm_span_eq_lit(key, n, "uva") ||
        ozmvm_span_eq_lit(key, n, "uvb") || ozmvm_span_eq_lit(key, n, "uvc") ||
        ozmvm_span_eq_lit(key, n, "pblh") || ozmvm_span_eq_lit(key, n, "aod") ||
        ozmvm_span_eq_lit(key, n, "dewpoint") || ozmvm_span_eq_lit(key, n, "vpd") ||
        ozmvm_span_eq_lit(key, n, "abs_humidity") || ozmvm_span_eq_lit(key, n, "specific_humidity") ||
        ozmvm_span_eq_lit(key, n, "wet_bulb")) return 1;
    if (ozmvm_atm_key_is_contaminant(key, n)) return 1;
    if (ozmvm_atm_key_is_meteorological(key, n)) return 1;
    return 0;
}

int tlal_atm_station_draw_cap(int z, float pitch) {
    int cap;
    if (z < 9) cap = 64;
    else if (z < 11) cap = 140;
    else if (z < 13) cap = 260;
    else if (z < 15) cap = 420;
    else cap = 900;
    if (isfinite((double)pitch) && pitch > 0.45f) cap = (int)((double)cap * 0.72);
    if (cap < 32) cap = 32;
    return cap;
}

int tlal_atm_station_halo_alpha(double mpp, int variable_active) {
    if (!isfinite(mpp) || mpp <= 0.0) mpp = 120.0;
    double t = 30.0 / mpp;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    const double s = t * t * (3.0 - 2.0 * t);
    int a = (int)((variable_active ? 14.0 : 9.0) + s * (variable_active ? 32.0 : 20.0));
    return tlal_clampi(a, variable_active ? 9 : 5, variable_active ? 48 : 28);
}

double tlal_dist_m(double lon1, double lat1, double lon2, double lat2) {
    const double pi = 3.14159265358979323846;
    const double r = 6371008.8;
    const double p1 = lat1 * pi / 180.0;
    const double p2 = lat2 * pi / 180.0;
    const double dp = (lat2 - lat1) * pi / 180.0;
    const double dl = (lon2 - lon1) * pi / 180.0;
    const double sd = sin(dp * 0.5);
    const double sl = sin(dl * 0.5);
    const double a = sd * sd + cos(p1) * cos(p2) * sl * sl;
    const double b = 1.0 - a;
    return r * 2.0 * atan2(sqrt(a < 0.0 ? 0.0 : a), sqrt(b < 0.0 ? 0.0 : b));
}


size_t tonal_format_hour_copy(int start_year, int64_t hour, char* out, size_t out_cap) {
    int y = start_year;
    unsigned m = 1u, d = 1u, h = 0u;
    tonal_hour_to_civil(start_year, hour, &y, &m, &d, &h);
    if (!out || out_cap == 0) return 16u;
    const int n = snprintf(out, out_cap, "%04d-%02u-%02u %02u:00", y, m, d, h);
    if (n < 0) return tonal_copy_lit("", out, out_cap);
    return (size_t)n;
}

size_t tonal_format_compact_copy(int start_year, int64_t hour, int minute, char* out, size_t out_cap) {
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;
    int y = start_year;
    unsigned m = 1u, d = 1u, h = 0u;
    tonal_hour_to_civil(start_year, hour, &y, &m, &d, &h);
    if (!out || out_cap == 0) return 16u;
    const int n = snprintf(out, out_cap, "%02u-%02u-%04d %02u:%02d", d, m, y, h, minute);
    if (n < 0) return tonal_copy_lit("", out, out_cap);
    return (size_t)n;
}

size_t tonal_format_long_es_copy(int start_year, int64_t hour, int minute, char* out, size_t out_cap) {
    static const char* weekdays[7] = {"Jueves", "Viernes", "Sabado", "Domingo", "Lunes", "Martes", "Miercoles"};
    static const char* months[13] = {"", "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;
    const int64_t day_rel = tonal_div_floor_i64(hour, 24LL);
    int y = start_year;
    unsigned m = 1u, d = 1u, h24 = 0u;
    tonal_hour_to_civil(start_year, hour, &y, &m, &d, &h24);
    const int weekday = (int)tonal_mod_pos_i64(day_rel, 7LL);
    const int pm = h24 >= 12u;
    unsigned h12 = h24 % 12u;
    if (h12 == 0u) h12 = 12u;
    if (!out || out_cap == 0) return 64u;
    const int n = snprintf(out, out_cap, "%s %u de %s del %d a las %u:%02d %s",
                           weekdays[weekday], d, months[m < 1u ? 1u : (m > 12u ? 12u : m)], y,
                           h12, minute, pm ? "p.m." : "a.m.");
    if (n < 0) return tonal_copy_lit("", out, out_cap);
    return (size_t)n;
}

size_t tonal_format_week_label_copy(int start_year, int64_t hour, char* out, size_t out_cap) {
    const int64_t day_index = tonal_days_from_civil(start_year, 1u, 1u) + tonal_div_floor_i64(hour, 24LL);
    int year = start_year;
    unsigned month = 1u, day = 1u;
    tonal_civil_from_days(day_index, &year, &month, &day);
    const int64_t year_start = tonal_days_from_civil(year, 1u, 1u);
    const int day_of_year = (int)(day_index - year_start) + 1;
    const int iso_weekday = (int)tonal_mod_pos_i64(day_index + 3LL, 7LL) + 1;
    int iso_year = year;
    int iso_week = (day_of_year - iso_weekday + 10) / 7;
    if (iso_week < 1) {
        --iso_year;
        iso_week = tonal_iso_weeks_in_year(iso_year);
    } else {
        const int weeks_this_year = tonal_iso_weeks_in_year(year);
        if (iso_week > weeks_this_year) {
            ++iso_year;
            iso_week = 1;
        }
    }
    if (!out || out_cap == 0) return 13u;
    const int n = snprintf(out, out_cap, "Semana %02d · %04d", iso_week, iso_year);
    if (n < 0) return tonal_copy_lit("", out, out_cap);
    return (size_t)n;
}

const char* tonal_month_abbrev_es3(unsigned month) {
    static const char* months[13] = {"", "Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    if (month < 1u) month = 1u;
    if (month > 12u) month = 12u;
    return months[month];
}

int tonal_first_numeric_width(const char* raw) {
    if (!raw) return 0;
    const unsigned char* p = (const unsigned char*)raw;
    while (*p && isspace(*p)) ++p;
    int n = 0;
    while (*p && isdigit(*p)) { ++n; ++p; }
    return n;
}


static int tonal_expand_year2(int y) {
    if (y >= 0 && y <= 99) return y >= 70 ? 1900 + y : 2000 + y;
    return y;
}

static int tonal_valid_datetime(int start_year, int min_year, int max_year,
                                int y, int mo, int da, int hour, int minute,
                                int64_t* out_hour, int* out_minute) {
    y = tonal_expand_year2(y);
    if (!out_hour || !out_minute) return 0;
    if (y < min_year || y > max_year || mo < 1 || mo > 12 || da < 1 || da > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) return 0;
    *out_hour = tonal_hour_from_civil(start_year, y, (unsigned)mo, (unsigned)da, (unsigned)hour);
    *out_minute = minute;
    return 1;
}

int tonal_parse_hour_text(const char* text, int start_year, int min_year, int max_year, int64_t* out_hour) {
    int parts[5] = {0, 0, 0, -1, -1};
    if (!text || !out_hour) return 0;
    if (!ozmvm_parse_numeric_date_time_components(text, strlen(text), parts)) return 0;
    const int y = parts[0];
    const int mo = parts[1];
    const int da = parts[2];
    const int ho = parts[3] < 0 ? 0 : parts[3];
    if (y < min_year || y > max_year || mo < 1 || mo > 12 || da < 1 || da > 31 || ho < 0 || ho > 23) return 0;
    *out_hour = tonal_hour_from_civil(start_year, y, (unsigned)mo, (unsigned)da, (unsigned)ho);
    return 1;
}

int tonal_parse_datetime_text(const char* text, int start_year, int min_year, int max_year,
                              int64_t current_hour, int current_minute, int64_t* out_hour, int* out_minute) {
    int parts[5] = {0, 0, 0, -1, -1};
    if (!text || !out_hour || !out_minute) return 0;
    if (!ozmvm_parse_numeric_date_time_components(text, strlen(text), parts)) return 0;

    int a = parts[0];
    int b = parts[1];
    int c = parts[2];
    int ho = parts[3];
    int mi = parts[4];
    const int first_is_year = tonal_first_numeric_width(text) >= 4;
    const int clock_hour = (int)tonal_mod_pos_i64(current_hour, 24LL);
    if (current_minute < 0) current_minute = 0;
    if (current_minute > 59) current_minute = 59;
    if (ho < 0) {
        ho = clock_hour;
        mi = current_minute;
    } else if (mi < 0) {
        mi = 0;
    }
    if (first_is_year || a > 31) return tonal_valid_datetime(start_year, min_year, max_year, a, b, c, ho, mi, out_hour, out_minute);
    return tonal_valid_datetime(start_year, min_year, max_year, c, b, a, ho, mi, out_hour, out_minute);
}

int64_t tonal_hour_for_epi_week(int start_year, int year, int week) {
    if (week < 1) week = 1;
    if (week > 53) week = 53;
    return tonal_hour_from_civil(start_year, year, 1u, 1u, 0u) + (int64_t)(week - 1) * 7LL * 24LL;
}

const char* tonal_sign_name(int sign_index) {
    static const char* signs[20] = {
        "Cipactli", "Ehecatl", "Calli", "Cuetzpalin", "Coatl",
        "Miquiztli", "Mazatl", "Tochtli", "Atl", "Itzcuintli",
        "Ozomahtli", "Malinalli", "Acatl", "Ocelotl", "Cuauhtli",
        "Cozcacuauhtli", "Ollin", "Tecpatl", "Quiahuitl", "Xochitl"
    };
    if (sign_index < 0) sign_index = 0;
    if (sign_index > 19) sign_index = 19;
    return signs[sign_index];
}

const char* tonal_number_word(int number) {
    static const char* words[13] = {
        "Ce", "Ome", "Yei", "Nahui", "Macuilli", "Chicuace", "Chicome",
        "Chicueyi", "Chicnahui", "Matlactli", "Matlactli once",
        "Matlactli omome", "Matlactli omeyi"
    };
    if (number < 1) number = 1;
    if (number > 13) number = 13;
    return words[number - 1];
}

const char* tonal_sign_es(int sign_index) {
    static const char* es[20] = {
        "Cocodrilo", "Viento", "Casa", "Lagartija", "Serpiente",
        "Muerte", "Venado", "Conejo", "Agua", "Perro",
        "Mono", "Hierba", "Caña", "Jaguar", "Águila",
        "Zopilote", "Movimiento", "Pedernal", "Lluvia", "Flor"
    };
    if (sign_index < 0) sign_index = 0;
    if (sign_index > 19) sign_index = 19;
    return es[sign_index];
}

int tonal_stamp_from_civil(int year, unsigned month, unsigned day, TonalStamp* out) {
    if (!out) return 0;
    if (month < 1u) month = 1u;
    if (month > 12u) month = 12u;
    const unsigned dim = tonal_days_in_month(year, month);
    if (day < 1u) day = 1u;
    if (day > dim) day = dim;
    const int64_t anchor = tonal_days_from_civil(1521, 8u, 13u);
    const int64_t now = tonal_days_from_civil(year, month, day);
    const int64_t delta = now - anchor;
    out->number = (int)tonal_mod_pos_i64(delta, 13LL) + 1;
    out->sign_index = (int)tonal_mod_pos_i64(4LL + delta, 20LL);
    out->sign_name = tonal_sign_name(out->sign_index);
    return 1;
}

void tlac_spec_zero(TlacSpec* spec) {
    if (!spec) return;
    memset(spec, 0, sizeof(*spec));
    spec->chart_type = 6;
    spec->axis_x_domain = 3;
    spec->axis_z_domain = 4;
    spec->axis_z = 4;
    spec->use_selected_data = 1;
    spec->show_fit_line = 1;
}

void tlac_spec_limpia(TlacSpec* spec) {
    if (!spec) return;
    spec->axis_x_domain = tlac_clampi(spec->axis_x_domain, 0, 4);
    spec->axis_y_domain = tlac_clampi(spec->axis_y_domain, 0, 4);
    spec->axis_z_domain = tlac_clampi(spec->axis_z_domain, 0, 4);
    spec->axis_x = tlac_clampi(spec->axis_x, 0, 4);
    spec->axis_y = tlac_clampi(spec->axis_y, 0, 4);
    spec->axis_z = tlac_clampi(spec->axis_z, 0, 4);
    spec->chart_type = tlac_tipo_limpia(spec->chart_type);
    spec->fit_model = tlac_clampi(spec->fit_model, 0, 1);
    spec->exposure_aggregation = tlac_clampi(spec->exposure_aggregation, 0, 5);

    /* La proscripción se concentra aquí: cualquier ruta que intente reactivar
       círculos o XY genérico cae en dispersión O3 y en dominios canónicos. */
    spec->force_ozone_epi = spec->chart_type == 6 ||
        (spec->axis_x_domain == 1 && spec->axis_y_domain == 0) ||
        (spec->axis_x_domain == 0 && spec->axis_y_domain == 1);

    if (spec->force_ozone_epi) {
        spec->chart_type = 6;
        spec->use_selected_data = 1;
        spec->use_graph_disease_filter = 1;
        spec->show_fit_line = 1;
        spec->axis_y_domain = 0;
        spec->axis_y = 0;
        spec->axis_x_domain = 1;
        spec->axis_x = 1;
        spec->axis_z_domain = 3;
        spec->axis_z = 4;
    }
}

int tlac_spec_de_gal(const TlacGal* item, TlacSpec* spec) {
    if (!item || !spec) return 0;
    tlac_spec_zero(spec);
    spec->chart_type = item->chart_type;
    spec->axis_x_domain = item->x_domain;
    spec->axis_x = item->x_field;
    spec->axis_y_domain = item->y_domain;
    spec->axis_y = item->y_field;
    spec->axis_z_domain = item->z_domain;
    spec->axis_z = item->z_field;
    spec->dependency = 0;
    spec->measurement = 0;
    spec->exposure_aggregation = 0;
    spec->use_selected_data = 1;
    spec->compare_best_lag = item->ozone_template ? 1 : 0;
    spec->auto_best_lag = 0;
    spec->show_fit_line = item->chart_type == 6 ? 1 : 0;
    spec->fit_model = item->ozone_template ? 1 : 0;
    spec->lag_weeks = 0;
    spec->lag_days = 0;
    spec->lag_value = 0;
    spec->lag_unit = 0;
    if (item->ozone_template) {
        spec->chart_type = 6;
        spec->axis_y_domain = 0;
        spec->axis_y = 0;
        spec->axis_x_domain = 1;
        spec->axis_x = 1;
        spec->axis_z_domain = 3;
        spec->axis_z = 4;
        spec->use_graph_disease_filter = 1;
    }
    tlac_spec_limpia(spec);
    return 1;
}

size_t tlac_titulo_copy(const char* title, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return title ? strlen(title) : 0;
    if (!title) title = "";
    const size_t n = strlen(title);
    const size_t m = n < out_cap - 1 ? n : out_cap - 1;
    if (m) memcpy(out, title, m);
    out[m] = '\0';
    return n;
}

size_t tlac_titulo_base_copy(const TlacSpec* spec, char* out, size_t out_cap) {
    char tmp[128];
    if (!spec || spec->force_ozone_epi || tlac_tipo_limpia(spec ? spec->chart_type : 6) == 6) {
        return tlac_titulo_copy("Gráfica de dispersión", out, out_cap);
    }
    const char* type = tlac_tipo_label(spec->chart_type);
    const char* field = tlac_eje_campo_label(spec->axis_y_domain, spec->axis_y);
    const int n = snprintf(tmp, sizeof(tmp), "Gráfica de %s · %s", type, field);
    if (n < 0) return tlac_titulo_copy("Gráfica", out, out_cap);
    return tlac_titulo_copy(tmp, out, out_cap);
}

size_t tlac_tiempo_compacto_copy(int64_t unix_time, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return 15u;
    time_t t = unix_time > 0 ? (time_t)unix_time : time((time_t*)0);
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    {
        const struct tm* tmp = localtime(&t);
        if (tmp) tmv = *tmp;
    }
#endif
    const int n = snprintf(out, out_cap, "%04d%02d%02d_%02d%02d%02d",
                           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                           tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    if (n < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

TlacToc tlac_gal_fila_unica(float width, float height, float text_line_h) {
    TlacToc r;
    memset(&r, 0, sizeof(r));
    r.item_count = (int)tlac_gal_count();
    if (r.item_count > 5) r.item_count = 5;
    if (r.item_count < 0) r.item_count = 0;
    const float safe_w = width > 1.0f ? width : 1.0f;
    const float safe_h = height > 1.0f ? height : 1.0f;
    /* Galería plana: una sola franja geométrica, sin marco interno ni tarjeta dentro de tarjeta. */
    r.pad = tlac_clampf(safe_h * 0.09016994f, 12.0f, 26.0f);
    r.gap = tlac_clampf(safe_w * 0.00901699f, 10.0f, 18.0f);
    r.title_h = (text_line_h > 1.0f ? text_line_h : 14.0f) * 1.42f;
    r.grid_x = r.pad;
    r.grid_y = r.pad + r.title_h + (text_line_h > 1.0f ? text_line_h : 14.0f) * 0.72f;
    r.grid_w = safe_w - r.pad * 2.0f;
    if (r.grid_w < 1.0f) r.grid_w = 1.0f;
    r.grid_h = safe_h - r.grid_y - r.pad * 0.70f;
    if (r.grid_h < 1.0f) r.grid_h = 1.0f;
    const int gaps = r.item_count > 1 ? r.item_count - 1 : 0;
    r.tile_w = (r.grid_w - r.gap * (float)gaps) / (float)(r.item_count > 0 ? r.item_count : 1);
    if (r.tile_w < 1.0f) r.tile_w = 1.0f;
    r.tile_h = tlac_clampf(r.grid_h, 92.0f, 172.0f);
    return r;
}
