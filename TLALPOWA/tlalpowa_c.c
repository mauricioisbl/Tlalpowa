/* TLALPOWA: unidad C11 central fusionada.
   Núcleo caliente C: parsers, malla histórica, transporte atmosférico y hotdata. */
#ifndef _WIN32
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif


/* ===== core.c ===== */
#line 1 "core.c"





#include "core.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#define TLAL_FORCE_INLINE static __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define TLAL_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define TLAL_FORCE_INLINE static inline
#endif

#define TLAL_LIKELY(x)   (x)
#define TLAL_UNLIKELY(x) (x)

/* Predicados calientes ASCII: sin locale, sin tablas, sin llamadas CRT. */
TLAL_FORCE_INLINE int oz_is_space(char c) {
    const unsigned char u = (unsigned char)c;
    return u == ' ' || (u >= 9u && u <= 13u);
}

TLAL_FORCE_INLINE int oz_is_digit(char c) {
    return (unsigned char)(c - '0') <= 9u;
}

TLAL_FORCE_INLINE int oz_key_at(const char* s, size_t n, size_t p, const char* key, size_t key_len) {
    return p != 0u && p + key_len < n && s[p - 1u] == '"' && s[p + key_len] == '"' && memcmp(s + p, key, key_len) == 0;
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
    {6, 3, 0, 0, 0, 4, 0, 0, "Gráfica de dispersión", "incidencia semanal", "Grafica_dispersion.png"},
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
    if (!isfinite((double)pitch) || pitch <= 0.0001f) return 1;
    if (pitch < 0.42f) return 2;
    if (pitch < 0.92f) return 3;
    return 3;
}

size_t tlal_tile_draw_cap(int z, float pitch, float screen_w, float screen_h) {
    const float tile_w = screen_w > 0.0f ? screen_w / 256.0f : 8.0f;
    const float tile_h = screen_h > 0.0f ? screen_h / 256.0f : 5.0f;
    const float pad = (!isfinite((double)pitch) || pitch <= 0.0001f) ? 2.0f : (pitch < 0.42f ? 2.5f : 3.0f);
    size_t cap = (size_t)((tile_w + pad) * (tile_h + pad));
    if (z >= 15) cap += 24u;
    if (z >= 18) cap += 32u;
    if (pitch > 0.42f) cap += 32u;
    if (pitch > 0.92f) cap += 48u;
    if (cap < 72u) cap = 72u;
    if (cap > 224u) cap = 224u;
    return cap;
}

int tlal_stream_budget(int moving, int startup_boost, float app_uptime, float warmup_seconds,
                       float idle_seconds, float deep_idle_seconds, int max_per_frame) {
    if (max_per_frame < 0) max_per_frame = 0;
    if (!isfinite((double)app_uptime)) app_uptime = 0.0f;
    if (!isfinite((double)idle_seconds)) idle_seconds = 0.0f;
    if (startup_boost) return max_per_frame > 5 ? 5 : max_per_frame;
    if (app_uptime < warmup_seconds) return 0;
    if (moving) return max_per_frame > 3 ? 3 : max_per_frame;
    if (idle_seconds >= deep_idle_seconds) return max_per_frame > 4 ? 4 : max_per_frame;
    if (idle_seconds >= 0.20f) return max_per_frame > 3 ? 3 : max_per_frame;
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
    spec->use_selected_data = 0;
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

    /* O3-incidencia sólo se activa cuando el usuario cruza manualmente un eje
       epidemiológico con uno atmosférico. No se fuerza desde la galería. */
    spec->force_ozone_epi =
        (spec->axis_x_domain == 1 && spec->axis_y_domain == 0) ||
        (spec->axis_x_domain == 0 && spec->axis_y_domain == 1);

    if (spec->force_ozone_epi) {
        spec->chart_type = 6;
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
    spec->use_selected_data = 0;
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
        spec->use_graph_disease_filter = 0;
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
    if (!spec || spec->force_ozone_epi) {
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

/* ===== historical_mesh.c ===== */
#line 1 "historical_mesh.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static uint16_t tlal_u16le(const unsigned char* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static float tlal_f32le(const unsigned char* p) {
    uint32_t bits = (uint32_t)p[0] |
                    ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) |
                    ((uint32_t)p[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static FILE* tlal_mesh_fopen_utf8(const char* path_utf8) {
#ifdef _WIN32
    int chars;
    wchar_t* path_w;
    FILE* file = NULL;
    if (!path_utf8) return NULL;
    chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1, NULL, 0);
    if (chars <= 0) return NULL;
    path_w = (wchar_t*)malloc((size_t)chars * sizeof(*path_w));
    if (!path_w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1, path_w, chars) > 0) {
        if (_wfopen_s(&file, path_w, L"rb") != 0) file = NULL;
    }
    free(path_w);
    return file;
#else
    return path_utf8 ? fopen(path_utf8, "rb") : NULL;
#endif
}

void tlal_historical_native_mesh_release(TlalHistoricalNativeMesh* mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    memset(mesh, 0, sizeof(*mesh));
}

int tlal_historical_native_mesh_load_utf8(const char* path_utf8, TlalHistoricalNativeMesh* mesh) {
    unsigned char header[64];
    FILE* file;
    size_t bytes;
    size_t i;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t stride;

    if (!mesh) return 0;
    tlal_historical_native_mesh_release(mesh);
    file = tlal_mesh_fopen_utf8(path_utf8);
    if (!file) return 0;
    if (fread(header, 1u, 48u, file) != 48u ||
        (memcmp(header, "TLT3D001", 8u) != 0 &&
         memcmp(header, "TLP3D002", 8u) != 0)) {
        fclose(file);
        return 0;
    }
    mesh->format_version = memcmp(header, "TLP3D002", 8u) == 0 ? 2u : 1u;
    if (mesh->format_version == 2u) {
        if (fread(header + 48u, 1u, 16u, file) != 16u) {
            fclose(file);
            memset(mesh, 0, sizeof(*mesh));
            return 0;
        }
        mesh->source_triangle_count = (uint32_t)header[48] |
                                      ((uint32_t)header[49] << 8) |
                                      ((uint32_t)header[50] << 16) |
                                      ((uint32_t)header[51] << 24);
        mesh->memory_budget_mb = (uint32_t)header[52] |
                                 ((uint32_t)header[53] << 8) |
                                 ((uint32_t)header[54] << 16) |
                                 ((uint32_t)header[55] << 24);
        mesh->detail_ratio = tlal_f32le(header + 56);
    } else {
        mesh->source_triangle_count = 0u;
        mesh->memory_budget_mb = 0u;
        mesh->detail_ratio = 0.0f;
    }

    vertex_count = (uint32_t)header[8] |
                   ((uint32_t)header[9] << 8) |
                   ((uint32_t)header[10] << 16) |
                   ((uint32_t)header[11] << 24);
    triangle_count = (uint32_t)header[12] |
                     ((uint32_t)header[13] << 8) |
                     ((uint32_t)header[14] << 16) |
                     ((uint32_t)header[15] << 24);
    stride = (uint32_t)header[40] |
             ((uint32_t)header[41] << 8) |
             ((uint32_t)header[42] << 16) |
             ((uint32_t)header[43] << 24);

    mesh->lon_min = tlal_f32le(header + 16);
    mesh->lat_min = tlal_f32le(header + 20);
    mesh->lon_max = tlal_f32le(header + 24);
    mesh->lat_max = tlal_f32le(header + 28);
    mesh->height_min_m = tlal_f32le(header + 32);
    mesh->height_max_m = tlal_f32le(header + 36);

    if (vertex_count < 3u || vertex_count > 57000000u ||
        triangle_count == 0u || triangle_count > 19000000u ||
        vertex_count != triangle_count * 3u || stride != 12u ||
        !(mesh->lon_max > mesh->lon_min) || !(mesh->lat_max > mesh->lat_min) ||
        !(mesh->height_max_m > mesh->height_min_m)) {
        fclose(file);
        memset(mesh, 0, sizeof(*mesh));
        return 0;
    }

    bytes = (size_t)vertex_count * 12u;
    mesh->vertices = (TlalHistoricalVertex*)malloc((size_t)vertex_count * sizeof(*mesh->vertices));
    if (!mesh->vertices || sizeof(*mesh->vertices) != 12u ||
        fread(mesh->vertices, 1u, bytes, file) != bytes) {
        fclose(file);
        tlal_historical_native_mesh_release(mesh);
        return 0;
    }
    fclose(file);

    {
        const uint16_t endian_probe = 1u;
        if (*(const unsigned char*)&endian_probe == 0u) {
            for (i = 0u; i < (size_t)vertex_count; ++i) {
                TlalHistoricalVertex* vertex = mesh->vertices + i;
                vertex->lon = (uint16_t)((vertex->lon >> 8) | (vertex->lon << 8));
                vertex->lat = (uint16_t)((vertex->lat >> 8) | (vertex->lat << 8));
                vertex->height = (uint16_t)((vertex->height >> 8) | (vertex->height << 8));
            }
        }
    }
    mesh->vertex_count = vertex_count;
    mesh->triangle_count = triangle_count;
    if (mesh->format_version == 1u) {
        mesh->source_triangle_count = triangle_count;
        mesh->memory_budget_mb = (uint32_t)((bytes + 1048575u) / 1048576u);
        mesh->detail_ratio = 1.0f;
    }
    return 1;
}

/* ===== Nucleos/importador_atmosferico.c ===== */
#line 1 "Nucleos/importador_atmosferico.c"
/* Parser y transporte atmosferico C11: RAMA, REDMET/REDMA y RUOA/PEMBU. */
#ifndef _WIN32
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif


#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>
#ifndef SECURITY_FLAG_IGNORE_UNKNOWN_CA
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA 0x00000100
#endif
#ifndef SECURITY_FLAG_IGNORE_CERT_CN_INVALID
#define SECURITY_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#endif
#ifndef SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
#define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#endif
#ifndef SECURITY_FLAG_IGNORE_REVOCATION
#define SECURITY_FLAG_IGNORE_REVOCATION 0x00000080
#endif
#pragma comment(lib, "wininet.lib")
#endif

#ifndef TLAL_ARRAY_COUNT
#define TLAL_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#endif

static size_t tlal_env_size_clamped(const char* name, size_t fallback, size_t min_v, size_t max_v) {
    const char* raw = name ? getenv(name) : NULL;
    unsigned long long v;
    char* endp = NULL;
    if (!raw || !raw[0]) return fallback;
    errno = 0;
    v = strtoull(raw, &endp, 10);
    if (errno || endp == raw) return fallback;
    if (v < (unsigned long long)min_v) return min_v;
    if (v > (unsigned long long)max_v) return max_v;
    return (size_t)v;
}

#define TLAL_RUOA_LOGIN_URL "https://ruoa.unam.mx/login/?redirect_to=https%3A%2F%2Fruoa.unam.mx%2Fpembu%2Fdescargas_pembu%2F"
#define TLAL_RUOA_LOGIN_PLAIN_URL "https://ruoa.unam.mx/login/"
#define TLAL_RUOA_DOWNLOAD_PAGE_URL "https://ruoa.unam.mx/pembu/descargas_pembu/"
#define TLAL_RUOA_WP_LOGIN_URL "https://ruoa.unam.mx/wp-login.php"
#define TLAL_RUOA_WP_LOGIN_REDIRECT_URL "https://ruoa.unam.mx/wp-login.php?redirect_to=https%3A%2F%2Fruoa.unam.mx%2Fpembu%2Fdescargas_pembu%2F"

typedef struct TlalHttpResponse {
    char* bytes;
    size_t size;
    long status;
} TlalHttpResponse;

struct TlalRuoaSession {
#ifdef _WIN32
    HINTERNET internet;
#endif
    /*
     * Campos exactos extraidos de la pagina autenticada PEMBU. El endpoint
     * pembu_rd no debe recibir un nombre inferido desde el correo si la pagina
     * ya entrego nombre_usuario y correo_usuario. Estos buffers viven solo en
     * la sesion HTTP y nunca tocan IXIPTLAH.
     */
    char endpoint_user[256];
    char endpoint_email[256];
    int logged_in;
};

static void tlal_copy_msg(char* dst, size_t cap, const char* msg) {
    if (!dst || cap == 0u) return;
    if (!msg) msg = "";
    snprintf(dst, cap, "%s", msg);
    dst[cap - 1u] = '\0';
}

static void tlal_login_report(TlalRuoaLoginReport* r, const char* stage, const char* msg, long status, int ok) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->attempted = 1;
    r->ok = ok;
    r->http_status = status;
    tlal_copy_msg(r->stage, sizeof(r->stage), stage);
    tlal_copy_msg(r->message, sizeof(r->message), msg);
}

static void tlal_dl_report(TlalRuoaDownloadReport* r, const char* stage, const char* msg, long status, uint64_t bytes, int ok, int cancelled) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->ok = ok;
    r->cancelled = cancelled;
    r->http_status = status;
    r->bytes_written = bytes;
    tlal_copy_msg(r->stage, sizeof(r->stage), stage);
    tlal_copy_msg(r->message, sizeof(r->message), msg);
}

static int tlal_ascii_ieq_ch(char a, char b) {
    unsigned char aa = (unsigned char)a;
    unsigned char bb = (unsigned char)b;
    if (aa >= 'A' && aa <= 'Z') aa = (unsigned char)(aa - 'A' + 'a');
    if (bb >= 'A' && bb <= 'Z') bb = (unsigned char)(bb - 'A' + 'a');
    return aa == bb;
}

static const char* tlal_memcasestr_n(const char* hay, size_t hay_n, const char* needle) {
    size_t needle_n;
    size_t i;
    if (!hay || !needle) return NULL;
    needle_n = strlen(needle);
    if (needle_n == 0u) return hay;
    if (hay_n < needle_n) return NULL;
    for (i = 0u; i + needle_n <= hay_n; ++i) {
        size_t j = 0u;
        while (j < needle_n && tlal_ascii_ieq_ch(hay[i + j], needle[j])) ++j;
        if (j == needle_n) return hay + i;
    }
    return NULL;
}

static int tlal_contains_lit_i(const char* s, size_t n, const char* lit) {
    return tlal_memcasestr_n(s, n, lit) != NULL;
}

static char* tlal_strdup_range(const char* b, const char* e) {
    size_t n;
    char* out;
    if (!b || !e || e < b) return NULL;
    n = (size_t)(e - b);
    out = (char*)malloc(n + 1u);
    if (!out) return NULL;
    if (n) memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

static char* tlal_strdup0(const char* s) {
    if (!s) s = "";
    return tlal_strdup_range(s, s + strlen(s));
}

static void tlal_http_response_free(TlalHttpResponse* r) {
    if (!r) return;
    free(r->bytes);
    memset(r, 0, sizeof(*r));
}

static char* tlal_html_attr_value_dup(const char* tag_b, const char* tag_e, const char* attr_name) {
    char pat[128];
    const char* p;
    const char* v;
    const char* q;
    if (!tag_b || !tag_e || !attr_name || tag_e < tag_b) return NULL;
    snprintf(pat, sizeof(pat), "%s=", attr_name);
    p = tlal_memcasestr_n(tag_b, (size_t)(tag_e - tag_b), pat);
    if (!p) return NULL;
    v = p + strlen(pat);
    while (v < tag_e && isspace((unsigned char)*v)) ++v;
    if (v >= tag_e) return tlal_strdup0("");
    if (*v == '"' || *v == '\'') {
        const char quote = *v++;
        q = v;
        while (q < tag_e && *q != quote) ++q;
        return tlal_strdup_range(v, q);
    }
    q = v;
    while (q < tag_e && !isspace((unsigned char)*q) && *q != '>') ++q;
    return tlal_strdup_range(v, q);
}

static char* tlal_html_input_value_dup(const char* html, size_t html_n, const char* input_name) {
    char pat[256];
    const char* p;
    const char* tag_b;
    const char* tag_e;
    if (!html || !input_name) return NULL;
    snprintf(pat, sizeof(pat), "name=\"%s\"", input_name);
    p = tlal_memcasestr_n(html, html_n, pat);
    if (!p) {
        snprintf(pat, sizeof(pat), "name='%s'", input_name);
        p = tlal_memcasestr_n(html, html_n, pat);
    }
    if (!p) {
        snprintf(pat, sizeof(pat), "id=\"%s\"", input_name);
        p = tlal_memcasestr_n(html, html_n, pat);
    }
    if (!p) return NULL;
    tag_b = p;
    while (tag_b > html && *tag_b != '<') --tag_b;
    tag_e = p;
    while ((size_t)(tag_e - html) < html_n && *tag_e != '>') ++tag_e;
    if ((size_t)(tag_e - html) >= html_n) return NULL;
    return tlal_html_attr_value_dup(tag_b, tag_e, "value");
}

static int tlal_html_form_id_dup(const char* html, size_t html_n, char** out_form_id) {
    char* id;
    if (!out_form_id) return 0;
    *out_form_id = NULL;
    id = tlal_html_input_value_dup(html, html_n, "form_id");
    if (id && id[0]) { *out_form_id = id; return 1; }
    free(id);
    *out_form_id = tlal_strdup0("1110");
    return *out_form_id != NULL;
}

static char* tlal_html_first_post_form_action_dup(const char* html, size_t html_n) {
    const char* p;
    const char* tag_e;
    char* action;
    if (!html || html_n == 0u) return NULL;
    p = tlal_memcasestr_n(html, html_n, "<form");
    while (p) {
        tag_e = p;
        while ((size_t)(tag_e - html) < html_n && *tag_e != '>') ++tag_e;
        if ((size_t)(tag_e - html) >= html_n) return NULL;
        if (tlal_contains_lit_i(p, (size_t)(tag_e - p), "method=\"post\"") ||
            tlal_contains_lit_i(p, (size_t)(tag_e - p), "method='post'") ||
            tlal_contains_lit_i(p, (size_t)(tag_e - p), "method=post")) {
            action = tlal_html_attr_value_dup(p, tag_e, "action");
            if (action && action[0]) return action;
            free(action);
        }
        p = tlal_memcasestr_n(tag_e, html_n - (size_t)(tag_e - html), "<form");
    }
    return NULL;
}

static void tlal_percent_append(char** buf, size_t* len, size_t* cap, const char* s) {
    static const char hex[] = "0123456789ABCDEF";
    if (!buf || !len || !cap || !s) return;
    while (*s) {
        const unsigned char c = (unsigned char)*s++;
        char tmp[3];
        size_t add = 1u;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            tmp[0] = (char)c;
        } else if (c == ' ') {
            tmp[0] = '+';
        } else {
            tmp[0] = '%'; tmp[1] = hex[c >> 4u]; tmp[2] = hex[c & 15u]; add = 3u;
        }
        if (*len + add + 1u > *cap) {
            size_t ncap = (*cap ? *cap * 2u : 128u);
            char* nb;
            while (*len + add + 1u > ncap) ncap *= 2u;
            nb = (char*)realloc(*buf, ncap);
            if (!nb) return;
            *buf = nb;
            *cap = ncap;
        }
        memcpy(*buf + *len, tmp, add);
        *len += add;
        (*buf)[*len] = '\0';
    }
}

static int tlal_form_add(char** buf, size_t* len, size_t* cap, const char* key, const char* value) {
    size_t before;
    if (!buf || !len || !cap) return 0;
    if (*len) {
        if (*len + 2u > *cap) {
            char* nb = (char*)realloc(*buf, *cap + 128u);
            if (!nb) return 0;
            *buf = nb;
            *cap += 128u;
        }
        (*buf)[(*len)++] = '&';
        (*buf)[*len] = '\0';
    }
    before = *len;
    tlal_percent_append(buf, len, cap, key ? key : "");
    if (*len == before && key && key[0]) return 0;
    if (*len + 2u > *cap) {
        char* nb = (char*)realloc(*buf, *cap + 128u);
        if (!nb) return 0;
        *buf = nb;
        *cap += 128u;
    }
    (*buf)[(*len)++] = '=';
    (*buf)[*len] = '\0';
    before = *len;
    tlal_percent_append(buf, len, cap, value ? value : "");
    if (!*buf) return 0;
    return 1;
}

#ifdef _WIN32
static wchar_t* tlal_widen_utf8_dup(const char* s) {
    int n;
    wchar_t* out;
    if (!s) s = "";
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        size_t m = strlen(s);
        size_t i;
        out = (wchar_t*)calloc(m + 1u, sizeof(wchar_t));
        if (!out) return NULL;
        for (i = 0u; i < m; ++i) out[i] = (wchar_t)(unsigned char)s[i];
        return out;
    }
    out = (wchar_t*)calloc((size_t)n, sizeof(wchar_t));
    if (!out) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n);
    return out;
}

static FILE* tlal_fopen_utf8(const char* path, const char* mode) {
    FILE* f = NULL;
    wchar_t* wp = tlal_widen_utf8_dup(path);
    wchar_t* wm = tlal_widen_utf8_dup(mode);
    if (wp && wm) {
        if (_wfopen_s(&f, wp, wm) != 0) f = NULL;
    }
    free(wp);
    free(wm);
    return f;
}

static void tlal_close_internet(HINTERNET* h) {
    if (h && *h) { InternetCloseHandle(*h); *h = NULL; }
}

static int tlal_parse_url_w(const wchar_t* url, wchar_t** host, INTERNET_PORT* port, wchar_t** path_query, int* secure) {
    URL_COMPONENTSW uc;
    wchar_t scheme[16];
    wchar_t host_buf[512];
    wchar_t path[4096];
    wchar_t extra[4096];
    size_t pn;
    memset(&uc, 0, sizeof(uc));
    memset(scheme, 0, sizeof(scheme));
    memset(host_buf, 0, sizeof(host_buf));
    memset(path, 0, sizeof(path));
    memset(extra, 0, sizeof(extra));
    uc.dwStructSize = sizeof(uc);
    uc.lpszScheme = scheme; uc.dwSchemeLength = (DWORD)TLAL_ARRAY_COUNT(scheme);
    uc.lpszHostName = host_buf; uc.dwHostNameLength = (DWORD)TLAL_ARRAY_COUNT(host_buf);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = (DWORD)TLAL_ARRAY_COUNT(path);
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = (DWORD)TLAL_ARRAY_COUNT(extra);
    if (!InternetCrackUrlW(url, 0, 0, &uc)) return 0;
    *host = (wchar_t*)calloc((size_t)uc.dwHostNameLength + 1u, sizeof(wchar_t));
    if (!*host) return 0;
    memcpy(*host, host_buf, (size_t)uc.dwHostNameLength * sizeof(wchar_t));
    pn = (size_t)uc.dwUrlPathLength + (size_t)uc.dwExtraInfoLength;
    *path_query = (wchar_t*)calloc(pn + 1u, sizeof(wchar_t));
    if (!*path_query) { free(*host); *host = NULL; return 0; }
    memcpy(*path_query, path, (size_t)uc.dwUrlPathLength * sizeof(wchar_t));
    if (uc.dwExtraInfoLength) memcpy(*path_query + uc.dwUrlPathLength, extra, (size_t)uc.dwExtraInfoLength * sizeof(wchar_t));
    *port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
    *secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return 1;
}

static void tlal_set_wininet_timeouts(HINTERNET h, DWORD timeout_ms) {
    if (!h) return;
    InternetSetOptionW(h, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionW(h, INTERNET_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionW(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
}

static void tlal_relax_wininet_tls_for_ruoa(HINTERNET req, int secure) {
    DWORD security_flags;
    if (!req || !secure) return;
    security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                     SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                     SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                     SECURITY_FLAG_IGNORE_REVOCATION;
    InternetSetOptionW(req, INTERNET_OPTION_SECURITY_FLAGS, &security_flags, sizeof(security_flags));
}

static int tlal_http_request_memory(TlalRuoaSession* s,
                                    const char* method,
                                    const char* url_utf8,
                                    const char* referer_utf8,
                                    const char* extra_headers_utf8,
                                    const char* body,
                                    size_t body_n,
                                    size_t max_bytes,
                                    const volatile int* cancel_flag,
                                    TlalHttpResponse* out,
                                    char* err,
                                    size_t err_cap) {
    wchar_t* wurl = NULL;
    wchar_t* host = NULL;
    wchar_t* path_query = NULL;
    wchar_t* wmethod = NULL;
    wchar_t* wreferer = NULL;
    wchar_t* wheaders = NULL;
    HINTERNET conn = NULL;
    HINTERNET req = NULL;
    INTERNET_PORT port = 0;
    int secure = 0;
    DWORD flags;
    DWORD status_dw = 0;
    DWORD status_len = sizeof(status_dw);
    const wchar_t* accept[] = { L"text/csv", L"text/plain", L"text/html", L"application/xhtml+xml", L"application/octet-stream", L"*/*", NULL };
    char* bytes = NULL;
    size_t size = 0u;
    size_t cap = 0u;
    int ok = 0;
    if (out) memset(out, 0, sizeof(*out));
    if (err && err_cap) err[0] = '\0';
    if (!s || !s->internet || !url_utf8 || !url_utf8[0]) { tlal_copy_msg(err, err_cap, "sesion o url invalida"); return 0; }
    if (cancel_flag && *cancel_flag) { tlal_copy_msg(err, err_cap, "cancelado"); return 0; }
    if (max_bytes < 1024u) max_bytes = 1024u;
    wurl = tlal_widen_utf8_dup(url_utf8);
    wmethod = tlal_widen_utf8_dup(method ? method : "GET");
    wreferer = tlal_widen_utf8_dup(referer_utf8 ? referer_utf8 : TLAL_RUOA_DOWNLOAD_PAGE_URL);
    if (!wurl || !wmethod || !wreferer || !tlal_parse_url_w(wurl, &host, &port, &path_query, &secure)) {
        tlal_copy_msg(err, err_cap, "URL RUOA invalida");
        goto done;
    }
    tlal_set_wininet_timeouts(s->internet, 45000u);
    conn = InternetConnectW(s->internet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!conn) { tlal_copy_msg(err, err_cap, "InternetConnectW fallo"); goto done; }
    tlal_set_wininet_timeouts(conn, 45000u);
    flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE |
            INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RESYNCHRONIZE;
    if (secure) flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    req = HttpOpenRequestW(conn, wmethod, path_query, L"HTTP/1.1", wreferer, accept, flags, 0);
    if (!req) { tlal_copy_msg(err, err_cap, "HttpOpenRequestW fallo"); goto done; }
    tlal_set_wininet_timeouts(req, 45000u);
    tlal_relax_wininet_tls_for_ruoa(req, secure);
    {
        char headers[3072];
        if (body && body_n > 0u) {
            snprintf(headers, sizeof(headers),
                     "Accept: text/html,application/xhtml+xml,text/plain,*/*\r\n"
                     "Accept-Charset: utf-8,*;q=0.1\r\n"
                     "Accept-Language: es-MX,es;q=0.9,en;q=0.5\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Pragma: no-cache\r\n"
                     "Origin: https://ruoa.unam.mx\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "%s",
                     extra_headers_utf8 ? extra_headers_utf8 : "");
        } else {
            snprintf(headers, sizeof(headers),
                     "Accept: text/html,application/xhtml+xml,text/csv,text/plain,*/*\r\n"
                     "Accept-Charset: utf-8,*;q=0.1\r\n"
                     "Accept-Language: es-MX,es;q=0.9,en;q=0.5\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Pragma: no-cache\r\n"
                     "%s",
                     extra_headers_utf8 ? extra_headers_utf8 : "");
        }
        wheaders = tlal_widen_utf8_dup(headers);
    }
    if (!wheaders) { tlal_copy_msg(err, err_cap, "sin memoria para cabeceras"); goto done; }
    if (cancel_flag && *cancel_flag) { tlal_copy_msg(err, err_cap, "cancelado"); goto done; }
    if (!HttpSendRequestW(req, wheaders, (DWORD)wcslen(wheaders), body ? (LPVOID)body : NULL, (DWORD)body_n)) {
        DWORD gle = GetLastError();
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "HttpSendRequestW fallo codigo=%lu", (unsigned long)gle);
        tlal_copy_msg(err, err_cap, tmp);
        goto done;
    }
    if (HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_dw, &status_len, NULL)) {
        if (out) out->status = (long)status_dw;
    }
    if (status_dw >= 400u) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "HTTP %lu", (unsigned long)status_dw);
        tlal_copy_msg(err, err_cap, tmp);
        goto done;
    }
    for (;;) {
        char chunk[65536];
        DWORD got = 0;
        if (cancel_flag && *cancel_flag) { tlal_copy_msg(err, err_cap, "cancelado durante lectura"); goto done; }
        if (!InternetReadFile(req, chunk, (DWORD)sizeof(chunk), &got)) { tlal_copy_msg(err, err_cap, "InternetReadFile fallo"); goto done; }
        if (got == 0u) break;
        if (size + (size_t)got > max_bytes) { tlal_copy_msg(err, err_cap, "respuesta excedio limite de seguridad"); goto done; }
        if (size + (size_t)got + 1u > cap) {
            size_t ncap = cap ? cap * 2u : 131072u;
            char* nb;
            while (size + (size_t)got + 1u > ncap) ncap *= 2u;
            nb = (char*)realloc(bytes, ncap);
            if (!nb) { tlal_copy_msg(err, err_cap, "sin memoria para respuesta"); goto done; }
            bytes = nb;
            cap = ncap;
        }
        memcpy(bytes + size, chunk, got);
        size += (size_t)got;
        bytes[size] = '\0';
    }
    if (!bytes) {
        bytes = (char*)calloc(1u, 1u);
        if (!bytes) { tlal_copy_msg(err, err_cap, "sin memoria para respuesta vacia"); goto done; }
    }
    if (out) {
        out->bytes = bytes;
        out->size = size;
        out->status = (long)status_dw;
        bytes = NULL;
    }
    ok = 1;

done:
    free(bytes);
    tlal_close_internet(&req);
    tlal_close_internet(&conn);
    free(wheaders);
    free(wreferer);
    free(wmethod);
    free(path_query);
    free(host);
    free(wurl);
    return ok;
}

static int tlal_http_download_file_once(TlalRuoaSession* s,
                                        const char* url_utf8,
                                        const char* target_utf8,
                                        const volatile int* cancel_flag,
                                        TlalRuoaDownloadReport* report) {
    wchar_t* wurl = NULL;
    wchar_t* host = NULL;
    wchar_t* path_query = NULL;
    wchar_t* wtarget = NULL;
    wchar_t* wmethod = NULL;
    wchar_t* wreferer = NULL;
    wchar_t* wheaders = NULL;
    HINTERNET conn = NULL;
    HINTERNET req = NULL;
    INTERNET_PORT port = 0;
    int secure = 0;
    DWORD flags;
    DWORD status_dw = 0;
    DWORD status_len = sizeof(status_dw);
    const wchar_t* accept[] = { L"text/csv", L"text/plain", L"application/octet-stream", L"*/*", NULL };
    FILE* out = NULL;
    uint64_t total = 0u;
    int ok = 0;
    if (!s || !s->internet || !url_utf8 || !target_utf8) { tlal_dl_report(report, "download", "argumentos invalidos", 0, 0, 0, 0); return 0; }
    if (cancel_flag && *cancel_flag) { tlal_dl_report(report, "download", "cancelado", 0, 0, 0, 1); return 0; }
    wurl = tlal_widen_utf8_dup(url_utf8);
    wtarget = tlal_widen_utf8_dup(target_utf8);
    wmethod = tlal_widen_utf8_dup("GET");
    wreferer = tlal_widen_utf8_dup(TLAL_RUOA_DOWNLOAD_PAGE_URL);
    if (!wurl || !wtarget || !wmethod || !wreferer || !tlal_parse_url_w(wurl, &host, &port, &path_query, &secure)) {
        tlal_dl_report(report, "download", "URL o destino invalidos", 0, 0, 0, 0);
        goto done;
    }
    tlal_set_wininet_timeouts(s->internet, 90000u);
    conn = InternetConnectW(s->internet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!conn) { tlal_dl_report(report, "connect", "InternetConnectW fallo", 0, 0, 0, 0); goto done; }
    tlal_set_wininet_timeouts(conn, 90000u);
    flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE |
            INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RESYNCHRONIZE;
    if (secure) flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    req = HttpOpenRequestW(conn, wmethod, path_query, L"HTTP/1.1", wreferer, accept, flags, 0);
    if (!req) { tlal_dl_report(report, "request", "HttpOpenRequestW fallo", 0, 0, 0, 0); goto done; }
    tlal_set_wininet_timeouts(req, 90000u);
    tlal_relax_wininet_tls_for_ruoa(req, secure);
    wheaders = tlal_widen_utf8_dup("Accept: text/csv,text/plain,*/*\r\nAccept-Language: es-MX,es;q=0.9,en;q=0.5\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n");
    if (!wheaders) { tlal_dl_report(report, "headers", "sin memoria para cabeceras", 0, 0, 0, 0); goto done; }
    if (!HttpSendRequestW(req, wheaders, (DWORD)wcslen(wheaders), NULL, 0)) {
        DWORD gle = GetLastError();
        char msg[128];
        snprintf(msg, sizeof(msg), "HttpSendRequestW fallo codigo=%lu", (unsigned long)gle);
        tlal_dl_report(report, "request", msg, 0, 0, 0, 0);
        goto done;
    }
    if (HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_dw, &status_len, NULL)) {
        /* status captured */
    }
    if (status_dw >= 400u) {
        char msg[64];
        snprintf(msg, sizeof(msg), "HTTP %lu", (unsigned long)status_dw);
        tlal_dl_report(report, "http", msg, (long)status_dw, 0, 0, 0);
        goto done;
    }
    if (_wfopen_s(&out, wtarget, L"wb") != 0) out = NULL;
    if (!out) { tlal_dl_report(report, "file", "no se pudo abrir temporal", (long)status_dw, 0, 0, 0); goto done; }
    for (;;) {
        char chunk[131072];
        DWORD got = 0;
        if (cancel_flag && *cancel_flag) { tlal_dl_report(report, "download", "cancelado durante lectura", (long)status_dw, total, 0, 1); goto done; }
        if (!InternetReadFile(req, chunk, (DWORD)sizeof(chunk), &got)) { tlal_dl_report(report, "download", "InternetReadFile fallo", (long)status_dw, total, 0, 0); goto done; }
        if (got == 0u) break;
        if (fwrite(chunk, 1u, got, out) != got) { tlal_dl_report(report, "file", "fwrite fallo", (long)status_dw, total, 0, 0); goto done; }
        total += (uint64_t)got;
        if (total > (512ull * 1024ull * 1024ull)) { tlal_dl_report(report, "guard", "CSV excedio 512 MiB", (long)status_dw, total, 0, 0); goto done; }
    }
    fflush(out);
    ok = total > 0u;
    tlal_dl_report(report, "download", ok ? "ok" : "respuesta vacia", (long)status_dw, total, ok, 0);

done:
    if (out) fclose(out);
    if (!ok && target_utf8) remove(target_utf8);
    tlal_close_internet(&req);
    tlal_close_internet(&conn);
    free(wheaders);
    free(wreferer);
    free(wmethod);
    free(wtarget);
    free(path_query);
    free(host);
    free(wurl);
    return ok;
}

static char* tlal_replace_prefix_dup(const char* s, const char* old_prefix, const char* new_prefix) {
    const size_t old_n = old_prefix ? strlen(old_prefix) : 0u;
    const size_t new_n = new_prefix ? strlen(new_prefix) : 0u;
    const size_t s_n = s ? strlen(s) : 0u;
    char* out;
    if (!s || !old_prefix || !new_prefix || s_n < old_n || strncmp(s, old_prefix, old_n) != 0) return NULL;
    out = (char*)malloc(new_n + (s_n - old_n) + 1u);
    if (!out) return NULL;
    memcpy(out, new_prefix, new_n);
    memcpy(out + new_n, s + old_n, s_n - old_n + 1u);
    return out;
}

static int tlal_http_download_file(TlalRuoaSession* s,
                                   const char* url_utf8,
                                   const char* target_utf8,
                                   const volatile int* cancel_flag,
                                   TlalRuoaDownloadReport* report) {
    TlalRuoaDownloadReport first;
    char* alt = NULL;
    int ok;
    ok = tlal_http_download_file_once(s, url_utf8, target_utf8, cancel_flag, &first);
    if (ok) { if (report) *report = first; return 1; }
    if (first.cancelled) { if (report) *report = first; return 0; }
    /*
     * El JavaScript oficial usa https://www.ruoa.unam.mx:54151. En algunos
     * equipos la cadena TLS o el alias www falla intermitentemente; se prueban
     * variantes equivalentes sin cambiar parametros ni el formato del CSV.
     */
    alt = tlal_replace_prefix_dup(url_utf8, "https://www.ruoa.unam.mx:54151/", "https://ruoa.unam.mx:54151/");
    if (alt) {
        TlalRuoaDownloadReport r;
        ok = tlal_http_download_file_once(s, alt, target_utf8, cancel_flag, &r);
        free(alt);
        if (ok) { if (report) *report = r; return 1; }
        if (r.cancelled) { if (report) *report = r; return 0; }
    }
    alt = tlal_replace_prefix_dup(url_utf8, "https://www.ruoa.unam.mx:54151/", "http://www.ruoa.unam.mx:54151/");
    if (alt) {
        TlalRuoaDownloadReport r;
        ok = tlal_http_download_file_once(s, alt, target_utf8, cancel_flag, &r);
        free(alt);
        if (ok) { if (report) *report = r; return 1; }
        if (r.cancelled) { if (report) *report = r; return 0; }
    }
    alt = tlal_replace_prefix_dup(url_utf8, "https://www.ruoa.unam.mx:54151/", "http://ruoa.unam.mx:54151/");
    if (alt) {
        TlalRuoaDownloadReport r;
        ok = tlal_http_download_file_once(s, alt, target_utf8, cancel_flag, &r);
        free(alt);
        if (ok) { if (report) *report = r; return 1; }
        if (r.cancelled) { if (report) *report = r; return 0; }
    }
    if (report) *report = first;
    return 0;
}
#else
static FILE* tlal_fopen_utf8(const char* path, const char* mode) { return fopen(path, mode); }

static int tlal_http_request_memory(TlalRuoaSession* s,
                                    const char* method,
                                    const char* url_utf8,
                                    const char* referer_utf8,
                                    const char* extra_headers_utf8,
                                    const char* body,
                                    size_t body_n,
                                    size_t max_bytes,
                                    const volatile int* cancel_flag,
                                    TlalHttpResponse* out,
                                    char* err,
                                    size_t err_cap) {
    (void)s; (void)method; (void)url_utf8; (void)referer_utf8; (void)extra_headers_utf8;
    (void)body; (void)body_n; (void)max_bytes; (void)cancel_flag;
    if (out) memset(out, 0, sizeof(*out));
    tlal_copy_msg(err, err_cap, "puente HTTP RUOA disponible solo en Windows en este paquete");
    return 0;
}

static int tlal_http_download_file(TlalRuoaSession* s,
                                   const char* url_utf8,
                                   const char* target_utf8,
                                   const volatile int* cancel_flag,
                                   TlalRuoaDownloadReport* report) {
    (void)s; (void)url_utf8; (void)target_utf8; (void)cancel_flag;
    tlal_dl_report(report, "unsupported", "descarga RUOA directa disponible solo en Windows en este paquete", 0, 0, 0, 0);
    return 0;
}
#endif

static int tlal_ruoa_extract_endpoint_identity(TlalRuoaSession* session, const TlalHttpResponse* page) {
    char* endpoint_user;
    char* endpoint_email;
    if (!session || !page || !page->bytes) return 0;
    endpoint_user = tlal_html_input_value_dup(page->bytes, page->size, "nombre_usuario");
    endpoint_email = tlal_html_input_value_dup(page->bytes, page->size, "correo_usuario");
    if (endpoint_user && endpoint_user[0]) tlal_copy_msg(session->endpoint_user, sizeof(session->endpoint_user), endpoint_user);
    if (endpoint_email && endpoint_email[0]) tlal_copy_msg(session->endpoint_email, sizeof(session->endpoint_email), endpoint_email);
    free(endpoint_user);
    free(endpoint_email);
    return session->endpoint_user[0] != '\0' || session->endpoint_email[0] != '\0';
}

static int tlal_ruoa_download_page_is_authenticated(const TlalHttpResponse* page) {
    if (!page || !page->bytes) return 0;
    if (tlal_contains_lit_i(page->bytes, page->size, "um-field-user_password")) return 0;
    if (tlal_contains_lit_i(page->bytes, page->size, "Usuario o E-mail")) return 0;
    if (tlal_contains_lit_i(page->bytes, page->size, "Restricted content")) return 0;
    return tlal_contains_lit_i(page->bytes, page->size, "pembu_rd?") &&
           tlal_contains_lit_i(page->bytes, page->size, "nombre_usuario") &&
           tlal_contains_lit_i(page->bytes, page->size, "correo_usuario");
}

static int tlal_ruoa_verify_session(TlalRuoaSession* session, TlalRuoaLoginReport* report, const char* referer, const char* username_utf8) {
    TlalHttpResponse page;
    char err[512];
    int ok;
    memset(&page, 0, sizeof(page));
    err[0] = '\0';
    ok = tlal_http_request_memory(session, "GET", TLAL_RUOA_DOWNLOAD_PAGE_URL, referer ? referer : TLAL_RUOA_LOGIN_URL,
                                  NULL, NULL, 0u, 16u * 1024u * 1024u, NULL, &page, err, sizeof(err));
    if (!ok) {
        tlal_login_report(report, "verify_get", err, page.status, 0);
        tlal_http_response_free(&page);
        return 0;
    }
    if (tlal_ruoa_download_page_is_authenticated(&page)) {
        tlal_ruoa_extract_endpoint_identity(session, &page);
        if (!session->endpoint_user[0]) tlal_copy_msg(session->endpoint_user, sizeof(session->endpoint_user), username_utf8 ? username_utf8 : "");
        if (!session->endpoint_email[0]) tlal_copy_msg(session->endpoint_email, sizeof(session->endpoint_email), username_utf8 ? username_utf8 : "");
        session->logged_in = 1;
        tlal_login_report(report, "verify", "sesion RUOA validada", page.status, 1);
        tlal_http_response_free(&page);
        return 1;
    }
    tlal_login_report(report, "verify", "RUOA devolvio login/restricted en vez del modulo PEMBU", page.status, 0);
    tlal_http_response_free(&page);
    return 0;
}

static int tlal_ruoa_post_um_login(TlalRuoaSession* session,
                                   const char* action_url,
                                   const char* referer_url,
                                   const char* username_utf8,
                                   const char* password_utf8,
                                   const char* form_id,
                                   const char* nonce,
                                   const char* http_referer,
                                   TlalRuoaLoginReport* report) {
    char username_field[96];
    char password_field[96];
    char* body = NULL;
    size_t body_len = 0u;
    size_t body_cap = 0u;
    TlalHttpResponse post_resp;
    char err[512];
    int ok = 0;
    memset(&post_resp, 0, sizeof(post_resp));
    err[0] = '\0';
    snprintf(username_field, sizeof(username_field), "username-%s", form_id ? form_id : "1110");
    snprintf(password_field, sizeof(password_field), "user_password-%s", form_id ? form_id : "1110");
    if (!tlal_form_add(&body, &body_len, &body_cap, username_field, username_utf8) ||
        !tlal_form_add(&body, &body_len, &body_cap, password_field, password_utf8) ||
        !tlal_form_add(&body, &body_len, &body_cap, "form_id", form_id ? form_id : "1110") ||
        !tlal_form_add(&body, &body_len, &body_cap, "um_request", "") ||
        !tlal_form_add(&body, &body_len, &body_cap, "_wpnonce", nonce ? nonce : "") ||
        !tlal_form_add(&body, &body_len, &body_cap, "_wp_http_referer", (http_referer && http_referer[0]) ? http_referer : "/login/") ||
        !tlal_form_add(&body, &body_len, &body_cap, "rememberme", "1") ||
        !tlal_form_add(&body, &body_len, &body_cap, "redirect_to", TLAL_RUOA_DOWNLOAD_PAGE_URL)) {
        tlal_login_report(report, "login_form", "sin memoria para POST", 0, 0);
        free(body);
        return 0;
    }
    ok = tlal_http_request_memory(session, "POST", action_url ? action_url : TLAL_RUOA_LOGIN_PLAIN_URL,
                                  referer_url ? referer_url : TLAL_RUOA_LOGIN_URL,
                                  NULL, body, body_len, 10u * 1024u * 1024u, NULL, &post_resp, err, sizeof(err));
    if (!ok) tlal_login_report(report, "login_post", err, post_resp.status, 0);
    tlal_http_response_free(&post_resp);
    free(body);
    return ok;
}

static int tlal_ruoa_post_wp_login(TlalRuoaSession* session,
                                   const char* username_utf8,
                                   const char* password_utf8,
                                   TlalRuoaLoginReport* report) {
    char* body = NULL;
    size_t body_len = 0u;
    size_t body_cap = 0u;
    TlalHttpResponse page;
    TlalHttpResponse post_resp;
    char err[512];
    int ok;
    memset(&page, 0, sizeof(page));
    memset(&post_resp, 0, sizeof(post_resp));
    err[0] = '\0';
    /* testcookie es parte del flujo nativo de WordPress. */
    tlal_http_request_memory(session, "GET", TLAL_RUOA_WP_LOGIN_REDIRECT_URL, TLAL_RUOA_LOGIN_URL,
                             NULL, NULL, 0u, 8u * 1024u * 1024u, NULL, &page, err, sizeof(err));
    tlal_http_response_free(&page);
#ifdef _WIN32
    InternetSetCookieA("https://ruoa.unam.mx/", "wordpress_test_cookie", "WP Cookie check");
#endif
    if (!tlal_form_add(&body, &body_len, &body_cap, "log", username_utf8) ||
        !tlal_form_add(&body, &body_len, &body_cap, "pwd", password_utf8) ||
        !tlal_form_add(&body, &body_len, &body_cap, "rememberme", "forever") ||
        !tlal_form_add(&body, &body_len, &body_cap, "wp-submit", "Iniciar sesión") ||
        !tlal_form_add(&body, &body_len, &body_cap, "redirect_to", TLAL_RUOA_DOWNLOAD_PAGE_URL) ||
        !tlal_form_add(&body, &body_len, &body_cap, "testcookie", "1")) {
        tlal_login_report(report, "wp_login_form", "sin memoria para POST WordPress", 0, 0);
        free(body);
        return 0;
    }
    ok = tlal_http_request_memory(session, "POST", TLAL_RUOA_WP_LOGIN_URL, TLAL_RUOA_WP_LOGIN_REDIRECT_URL,
                                  NULL, body, body_len, 10u * 1024u * 1024u, NULL, &post_resp, err, sizeof(err));
    if (!ok) tlal_login_report(report, "wp_login_post", err, post_resp.status, 0);
    tlal_http_response_free(&post_resp);
    free(body);
    return ok;
}

TlalRuoaSession* tlal_ruoa_session_create(void) {
    TlalRuoaSession* s = (TlalRuoaSession*)calloc(1u, sizeof(*s));
    if (!s) return NULL;
#ifdef _WIN32
    s->internet = InternetOpenW(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Tlalpowa-RUOA-PEMBU/2026", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!s->internet) { free(s); return NULL; }
    tlal_set_wininet_timeouts(s->internet, 45000u);
#endif
    return s;
}

void tlal_ruoa_session_destroy(TlalRuoaSession* session) {
    if (!session) return;
#ifdef _WIN32
    tlal_close_internet(&session->internet);
#endif
    memset(session, 0, sizeof(*session));
    free(session);
}

const char* tlal_ruoa_session_endpoint_user_utf8(const TlalRuoaSession* session) {
    return (session && session->endpoint_user[0]) ? session->endpoint_user : "";
}

const char* tlal_ruoa_session_endpoint_email_utf8(const TlalRuoaSession* session) {
    return (session && session->endpoint_email[0]) ? session->endpoint_email : "";
}

int tlal_ruoa_session_login(TlalRuoaSession* session,
                            const char* username_utf8,
                            const char* password_utf8,
                            TlalRuoaLoginReport* report) {
    TlalHttpResponse login_page;
    char err[512];
    char* form_id = NULL;
    char* nonce = NULL;
    char* referer = NULL;
    char* action = NULL;
    int ok = 0;
    memset(&login_page, 0, sizeof(login_page));
    err[0] = '\0';
    if (!session || !username_utf8 || !username_utf8[0] || !password_utf8 || !password_utf8[0]) {
        tlal_login_report(report, "credenciales", "correo y contrasena son obligatorios", 0, 0);
        return 0;
    }
    session->logged_in = 0;
    session->endpoint_user[0] = '\0';
    session->endpoint_email[0] = '\0';

    if (tlal_ruoa_verify_session(session, report, TLAL_RUOA_LOGIN_URL, username_utf8)) return 1;

    if (!tlal_http_request_memory(session, "GET", TLAL_RUOA_LOGIN_URL, TLAL_RUOA_LOGIN_PLAIN_URL,
                                  NULL, NULL, 0u, 10u * 1024u * 1024u, NULL, &login_page, err, sizeof(err))) {
        tlal_login_report(report, "login_get", err, login_page.status, 0);
        goto done;
    }
    if (tlal_ruoa_verify_session(session, report, TLAL_RUOA_LOGIN_URL, username_utf8)) { ok = 1; goto done; }

    if (!tlal_html_form_id_dup(login_page.bytes, login_page.size, &form_id)) {
        tlal_login_report(report, "login_form", "no se encontro form_id", login_page.status, 0);
        goto wp_fallback;
    }
    nonce = tlal_html_input_value_dup(login_page.bytes, login_page.size, "_wpnonce");
    referer = tlal_html_input_value_dup(login_page.bytes, login_page.size, "_wp_http_referer");
    action = tlal_html_first_post_form_action_dup(login_page.bytes, login_page.size);
    if (!nonce || !nonce[0]) {
        tlal_login_report(report, "login_form", "no se encontro _wpnonce", login_page.status, 0);
        goto wp_fallback;
    }

    if (tlal_ruoa_post_um_login(session, action && action[0] ? action : TLAL_RUOA_LOGIN_PLAIN_URL,
                                TLAL_RUOA_LOGIN_URL, username_utf8, password_utf8, form_id, nonce, referer, report) &&
        tlal_ruoa_verify_session(session, report, TLAL_RUOA_LOGIN_URL, username_utf8)) { ok = 1; goto done; }

    if (tlal_ruoa_post_um_login(session, TLAL_RUOA_LOGIN_URL,
                                TLAL_RUOA_LOGIN_URL, username_utf8, password_utf8, form_id, nonce, referer, report) &&
        tlal_ruoa_verify_session(session, report, TLAL_RUOA_LOGIN_URL, username_utf8)) { ok = 1; goto done; }

    if (tlal_ruoa_post_um_login(session, TLAL_RUOA_LOGIN_PLAIN_URL,
                                TLAL_RUOA_LOGIN_PLAIN_URL, username_utf8, password_utf8, form_id, nonce, referer, report) &&
        tlal_ruoa_verify_session(session, report, TLAL_RUOA_LOGIN_PLAIN_URL, username_utf8)) { ok = 1; goto done; }

wp_fallback:
    if (tlal_ruoa_post_wp_login(session, username_utf8, password_utf8, report) &&
        tlal_ruoa_verify_session(session, report, TLAL_RUOA_WP_LOGIN_REDIRECT_URL, username_utf8)) { ok = 1; goto done; }

    if (!report || report->ok == 0) {
        tlal_login_report(report, "verify", "RUOA rechazo la sesion o exige confirmacion de correo", login_page.status, 0);
    }

done:
    tlal_http_response_free(&login_page);
    free(form_id);
    free(nonce);
    free(referer);
    free(action);
    return ok;
}

int tlal_ruoa_session_download_utf8(TlalRuoaSession* session,
                                    const char* url_utf8,
                                    const char* target_tmp_path_utf8,
                                    const volatile int* cancel_flag,
                                    TlalRuoaDownloadReport* report) {
    if (!session || !session->logged_in) {
        tlal_dl_report(report, "login", "sesion RUOA no validada", 0, 0, 0, 0);
        return 0;
    }
    return tlal_http_download_file(session, url_utf8, target_tmp_path_utf8, cancel_flag, report);
}

static uint64_t tlal_file_size_utf8(const char* path) {
    FILE* f;
    long long n;
#ifdef _WIN32
    wchar_t* wp = tlal_widen_utf8_dup(path);
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (wp && GetFileAttributesExW(wp, GetFileExInfoStandard, &data)) {
        ULARGE_INTEGER ui;
        ui.HighPart = data.nFileSizeHigh;
        ui.LowPart = data.nFileSizeLow;
        free(wp);
        return (uint64_t)ui.QuadPart;
    }
    free(wp);
#endif
    f = tlal_fopen_utf8(path, "rb");
    if (!f) return 0u;
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) { fclose(f); return 0u; }
    n = (long long)_ftelli64(f);
#else
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0u; }
    n = (long long)ftell(f);
#endif
    fclose(f);
    return n > 0 ? (uint64_t)n : 0u;
}

int tlal_ruoa_pembu_csv_valid_utf8(const char* path_utf8, uint64_t min_bytes) {
    FILE* f;
    char buf[16384];
    size_t n;
    if (!path_utf8 || !path_utf8[0]) return 0;
    if (tlal_file_size_utf8(path_utf8) < min_bytes) return 0;
    f = tlal_fopen_utf8(path_utf8, "rb");
    if (!f) return 0;
    n = fread(buf, 1u, sizeof(buf), f);
    fclose(f);
    if (n == 0u) return 0;
    if (tlal_contains_lit_i(buf, n, "<html") || tlal_contains_lit_i(buf, n, "<!doctype") ||
        tlal_contains_lit_i(buf, n, "usuario o e-mail") || tlal_contains_lit_i(buf, n, "contraseña") ||
        tlal_contains_lit_i(buf, n, "restricted content") || tlal_contains_lit_i(buf, n, "wordpress") ||
        tlal_contains_lit_i(buf, n, "bad gateway") || tlal_contains_lit_i(buf, n, "certificate verify failed") ||
        tlal_contains_lit_i(buf, n, "no existe") || tlal_contains_lit_i(buf, n, "not found")) return 0;
    return (tlal_contains_lit_i(buf, n, "fecha_hora") || tlal_contains_lit_i(buf, n, "fecha hora")) &&
           (tlal_contains_lit_i(buf, n, "temp") || tlal_contains_lit_i(buf, n, "programa de estaciones meteor")) &&
           (tlal_contains_lit_i(buf, n, "hum_rel") || tlal_contains_lit_i(buf, n, "hum rel") || tlal_contains_lit_i(buf, n, "temperatura"));
}


typedef struct TlalSpan { const char* s; size_t n; } TlalSpan;

#define TLAL_ATMOS_FMT_LONG 1
#define TLAL_ATMOS_FMT_PARAMETER_WIDE 2
#define TLAL_ATMOS_FMT_STATION_WIDE 4
#define TLAL_ATMOS_MAX_COLS 512
#define TLAL_ATMOS_MAX_CELL 127
#define TLAL_ATMOS_MAX_KEY 128
#define TLAL_ATMOS_MAX_PATH_KEY 1536

/* Vista de archivo: Windows usa file mapping real; las demás plataformas leen
   una copia contigua. El parser posterior sólo recibe puntero+tamaño y nunca
   depende de fgets, líneas fijas ni estados globales. */
typedef struct TlalFileView {
    const unsigned char* data;
    size_t size;
    unsigned char* owned;
#ifdef _WIN32
    HANDLE file;
    HANDLE mapping;
#endif
} TlalFileView;

typedef struct TlalPembuColDef {
    const char* canonical;
    const char* unit;
    const char* aliases[20];
} TlalPembuColDef;

static const TlalPembuColDef g_pembu_defs[] = {
    {"tmp", "C",       {"tmp", "temp", "temperatura", "temperature", "air temperature", "out temp", "outside temp", "temp out", "hi temp", "low temp", "tc", "t c", "temperatura ambiente", NULL}},
    {"rh",  "%",       {"rh", "hr", "hum rel", "hum_rel", "humedad relativa", "out hum", "outside hum", "humidity", "relative humidity", "hum out", "humedad", NULL}},
    {"wsp", "m/s",     {"wsp", "ws", "vv", "vel viento", "velocidad viento", "velocidad del viento", "rapidez v sostenido", "rapidez_v_sostenido", "wind speed", "wind spd", "windspeed", NULL}},
    {"wdr", "grados",  {"wdr", "wd", "dv", "dir viento", "direccion viento", "dirección viento", "direccion del viento", "dirección del viento", "dir v sostenido", "dir_v_sostenido", "wind dir", "wind direction", "winddir", NULL}},
    {"wgst", "m/s",    {"wgst", "gust", "gust speed", "wind gust", "racha", "rachas", "rapidez rachas", "rapidez_rachas", NULL}},
    {"wdr_gust", "grados", {"wdr gust", "gust direction", "dir rachas", "dir_rachas", "hi wind dir", NULL}},
    {"pa",  "hPa",     {"pa", "bp", "presion", "presión", "presion bar", "presion_bar", "bar", "barometer", "barometric pressure", "pressure", "presion atmosferica", "presión atmosférica", NULL}},
    {"pp",  "mm",      {"pp", "precipitacion", "precipitación", "rain", "rainfall", "rain rate", "rainrate", "precipitation", "precip", NULL}},
    {"gr",  "W/m2",    {"gr", "rs", "rad solar", "rad_solar", "radiacion solar", "radiación solar", "solar rad", "solarrad", "solar radiation", "global radiation", NULL}},
    {"uv",  "indice",  {"uv", "indice uv", "indice_uv", "uv index", "uvindex", NULL}},
    {"uv_dose", "mJ/cm2", {"dosis uv", "dosis_uv", "uv dose", NULL}},
    {"co", "ppm",      {"co", "monoxido de carbono", "monóxido de carbono", "carbon monoxide", "carbono monoxide", "monoxido carbono", NULL}},
    {"no", "ppb",      {"no", "oxido nitrico", "óxido nítrico", "nitric oxide", NULL}},
    {"no2", "ppb",     {"no2", "dioxido de nitrogeno", "dióxido de nitrógeno", "nitrogen dioxide", NULL}},
    {"nox", "ppb",     {"nox", "no x", "oxidos de nitrogeno", "óxidos de nitrógeno", "nitrogen oxides", NULL}},
    {"o3", "ppb",      {"o3", "ozono", "ozone", NULL}},
    {"so2", "ppb",     {"so2", "dioxido de azufre", "dióxido de azufre", "sulfur dioxide", NULL}},
    {"h2s", "ppb",     {"h2s", "acido sulfhidrico", "ácido sulfhídrico", NULL}},
    {"pm10", "ug/m3",  {"pm10", "pm 10", "particulas pm10", "partículas pm10", "particles pm10", "mp10", "pst", "psts", "tsp", "particulas suspendidas", NULL}},
    {"pm25", "ug/m3",  {"pm25", "pm2 5", "pm 2 5", "pm2.5", "pm 2.5", "pm2,5", "pm 2,5", "mp2 5", "mp 2 5", "particulas pm2 5", "partículas pm2 5", "particles pm2 5", NULL}},
    {"pmco", "ug/m3",  {"pmco", "pm coarse", "pm10 2 5", "pm10-2 5", "pm10 2.5", "pm10 2,5", "fraccion gruesa", "coarse", NULL}},
    {"pb", "ug/m3",    {"pb", "plomo", "lead", NULL}}
};

static int tlal_is_ascii_alnum_(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static TlalSpan tlal_span_trim(TlalSpan sp) {
    while (sp.n && isspace((unsigned char)sp.s[0])) { ++sp.s; --sp.n; }
    while (sp.n && isspace((unsigned char)sp.s[sp.n - 1u])) --sp.n;
    return sp;
}

static TlalSpan tlal_span_unquote(TlalSpan sp) {
    sp = tlal_span_trim(sp);
    if (sp.n >= 2u && sp.s[0] == '"' && sp.s[sp.n - 1u] == '"') { ++sp.s; sp.n -= 2u; }
    return tlal_span_trim(sp);
}

static int tlal_span_copy0(TlalSpan sp, char* out, size_t cap) {
    size_t n;
    if (!out || cap == 0u) return 0;
    sp = tlal_span_unquote(sp);
    n = sp.n < cap - 1u ? sp.n : cap - 1u;
    if (n) memcpy(out, sp.s, n);
    out[n] = '\0';
    return (int)n;
}

static size_t tlal_norm_key_copy(TlalSpan in, char* out, size_t cap) {
    size_t w = 0u;
    int prev_space = 1;
    size_t i;
    if (!out || cap == 0u) return 0u;
    in = tlal_span_unquote(in);
    if (in.n >= 3u && (unsigned char)in.s[0] == 0xEFu && (unsigned char)in.s[1] == 0xBBu && (unsigned char)in.s[2] == 0xBFu) { in.s += 3u; in.n -= 3u; }
    for (i = 0u; i < in.n; ++i) {
        unsigned char c = (unsigned char)in.s[i];
        char folded = 0;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        /* Las cabeceras oficiales y hojas exportadas mezclan UTF-8, ANSI y
           mayúsculas acentuadas. La normalización vive aquí para que todas las
           rutas del parser C comparen una sola clave ASCII, sin tablas pesadas ni
           dependencias de locale que cambien entre Windows y Linux. */
        if (c == 0xC3u && i + 1u < in.n) {
            const unsigned char d = (unsigned char)in.s[i + 1u];
            if (d == 0x81u || d == 0xA1u) folded = 'a';
            else if (d == 0x89u || d == 0xA9u) folded = 'e';
            else if (d == 0x8Du || d == 0xADu) folded = 'i';
            else if (d == 0x93u || d == 0xB3u) folded = 'o';
            else if (d == 0x9Au || d == 0xBAu || d == 0x9Cu || d == 0xBCu) folded = 'u';
            else if (d == 0x91u || d == 0xB1u) folded = 'n';
            if (folded) {
                if (w + 1u < cap) out[w++] = folded;
                prev_space = 0;
                ++i;
                continue;
            }
        }
        if (c == '_' || c == '-' || c == '/' || c == '\\' || c == '.' || c == ':' || c == '(' || c == ')' || c == '[' || c == ']' || c == ',' || c == ';' || c == '|' || c == '\t' || c == '\r' || c == '\n' || c == ' ') {
            if (!prev_space && w + 1u < cap) out[w++] = ' ';
            prev_space = 1;
            continue;
        }
        if (c < 32u) continue;
        if (w + 1u < cap) out[w++] = (char)c;
        prev_space = 0;
    }
    while (w && out[w - 1u] == ' ') --w;
    out[w] = '\0';
    return w;
}

static size_t tlal_norm_cstr_copy(const char* in, char* out, size_t cap) {
    TlalSpan sp;
    if (!in) in = "";
    sp.s = in;
    sp.n = strlen(in);
    return tlal_norm_key_copy(sp, out, cap);
}

static int tlal_streq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

static int tlal_streq_any(const char* k, const char* const* vals, int n) {
    int i;
    if (!k) return 0;
    for (i = 0; i < n; ++i) if (vals[i] && tlal_streq(k, vals[i])) return 1;
    return 0;
}

static int tlal_norm_has_phrase(const char* norm, const char* phrase) {
    const size_t pn = phrase ? strlen(phrase) : 0u;
    const char* p;
    if (!norm || !phrase || pn == 0u) return 0;
    p = strstr(norm, phrase);
    while (p) {
        const char before = p == norm ? ' ' : p[-1];
        const char after = p[pn];
        const int left_ok = !tlal_is_ascii_alnum_((unsigned char)before);
        const int right_ok = after == '\0' || !tlal_is_ascii_alnum_((unsigned char)after);
        if (left_ok && right_ok) return 1;
        p = strstr(p + 1, phrase);
    }
    return 0;
}

static int tlal_split_csv_span(TlalSpan line, int delim, TlalSpan* cols, int max_cols) {
    const char* s = line.s;
    size_t n = line.n;
    size_t start = 0u;
    size_t i;
    int count = 0;
    int quoted = 0;
    if (!s || !cols || max_cols <= 0) return 0;
    while (n && (s[n - 1u] == '\n' || s[n - 1u] == '\r')) --n;
    if (n >= 3u && (unsigned char)s[0] == 0xEFu && (unsigned char)s[1] == 0xBBu && (unsigned char)s[2] == 0xBFu) start = 3u;
    for (i = start; i <= n; ++i) {
        const int at_end = i == n;
        const unsigned char ch = at_end ? 0u : (unsigned char)s[i];
        if (!at_end && ch == '"') {
            if (quoted && i + 1u < n && s[i + 1u] == '"') { ++i; continue; }
            quoted = !quoted;
            continue;
        }
        if (at_end || (!quoted && ch == (unsigned char)delim)) {
            if (count < max_cols) {
                TlalSpan cell;
                cell.s = s + start;
                cell.n = i > start ? i - start : 0u;
                cols[count] = tlal_span_unquote(cell);
            }
            ++count;
            start = i + 1u;
        }
    }
    return count > max_cols ? max_cols : count;
}

static int tlal_infer_delim_span(TlalSpan line) {
    size_t t = 0u, c = 0u, s = 0u, p = 0u, i;
    int quoted = 0;
    for (i = 0u; i < line.n; ++i) {
        const unsigned char ch = (unsigned char)line.s[i];
        if (ch == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (ch == '\t') ++t;
        else if (ch == ',') ++c;
        else if (ch == ';') ++s;
        else if (ch == '|') ++p;
    }
    if (t >= c && t >= s && t >= p && t) return '\t';
    if (s >= c && s >= p && s) return ';';
    if (p >= c && p) return '|';
    return ',';
}

static int tlal_file_view_read_copy(const char* path_utf8, TlalFileView* v) {
    FILE* f;
    unsigned char* io_buf = NULL;
    uint64_t size64;
    size_t want;
    size_t got = 0u;
    if (!path_utf8 || !v) return 0;
    f = tlal_fopen_utf8(path_utf8, "rb");
    if (!f) return 0;
    {
        const size_t io_n = tlal_env_size_clamped("TLALPOWA_CSV_READ_BUFFER_BYTES",
                                                  4u * 1024u * 1024u,
                                                  64u * 1024u,
                                                  64u * 1024u * 1024u);
        io_buf = (unsigned char*)malloc(io_n);
        if (io_buf) setvbuf(f, (char*)io_buf, _IOFBF, io_n);
    }
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) { free(io_buf); fclose(f); return 0; }
    {
        __int64 pos = _ftelli64(f);
        if (pos <= 0) { free(io_buf); fclose(f); return 0; }
        size64 = (uint64_t)pos;
    }
    if (_fseeki64(f, 0, SEEK_SET) != 0) { free(io_buf); fclose(f); return 0; }
#else
    if (fseeko(f, 0, SEEK_END) != 0) { free(io_buf); fclose(f); return 0; }
    {
        off_t pos = ftello(f);
        if (pos <= 0) { free(io_buf); fclose(f); return 0; }
        size64 = (uint64_t)pos;
    }
    if (fseeko(f, 0, SEEK_SET) != 0) { free(io_buf); fclose(f); return 0; }
#endif
    if (size64 > (uint64_t)(SIZE_MAX - 1u)) { free(io_buf); fclose(f); return 0; }
    want = (size_t)size64;
    v->owned = (unsigned char*)malloc(want + 1u);
    if (!v->owned) { free(io_buf); fclose(f); return 0; }
    while (got < want) {
        const size_t step = fread(v->owned + got, 1u, want - got, f);
        if (step == 0u) {
            if (ferror(f)) { free(io_buf); fclose(f); free(v->owned); memset(v, 0, sizeof(*v)); return 0; }
            break;
        }
        got += step;
    }
    free(io_buf);
    fclose(f);
    if (got != want) { free(v->owned); memset(v, 0, sizeof(*v)); return 0; }
    v->owned[got] = 0u;
    v->data = v->owned;
    v->size = got;
    return 1;
}

static int tlal_file_view_open_utf8(const char* path_utf8, TlalFileView* v) {
    if (!v) return 0;
    memset(v, 0, sizeof(*v));
#ifdef _WIN32
    {
        wchar_t* wp = tlal_widen_utf8_dup(path_utf8);
        LARGE_INTEGER li;
        if (!wp) return 0;
        v->file = CreateFileW(wp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        free(wp);
        if (v->file != INVALID_HANDLE_VALUE && GetFileSizeEx(v->file, &li) && li.QuadPart > 0 && (uint64_t)li.QuadPart <= (uint64_t)SIZE_MAX) {
            v->mapping = CreateFileMappingW(v->file, NULL, PAGE_READONLY, 0, 0, NULL);
            if (v->mapping) {
                v->data = (const unsigned char*)MapViewOfFile(v->mapping, FILE_MAP_READ, 0, 0, 0);
                if (v->data) { v->size = (size_t)li.QuadPart; return 1; }
            }
        }
        if (v->mapping) { CloseHandle(v->mapping); v->mapping = NULL; }
        if (v->file && v->file != INVALID_HANDLE_VALUE) { CloseHandle(v->file); v->file = NULL; }
    }
#endif
    return tlal_file_view_read_copy(path_utf8, v);
}

static void tlal_file_view_close(TlalFileView* v) {
    if (!v) return;
#ifdef _WIN32
    if (v->data && !v->owned) UnmapViewOfFile(v->data);
    if (v->mapping) CloseHandle(v->mapping);
    if (v->file && v->file != INVALID_HANDLE_VALUE) CloseHandle(v->file);
#endif
    free(v->owned);
    memset(v, 0, sizeof(*v));
}

static int tlal_span_missing_token(TlalSpan sp) {
    char key[32];
    tlal_norm_key_copy(sp, key, sizeof(key));
    return key[0] == '\0' || tlal_streq(key, "na") || tlal_streq(key, "nan") || tlal_streq(key, "null") ||
           tlal_streq(key, "nd") || tlal_streq(key, "n d") || tlal_streq(key, "n a") || tlal_streq(key, "sin dato") ||
           tlal_streq(key, "no data") || tlal_streq(key, "missing");
}

static int tlal_span_to_double(TlalSpan sp, int delim, double* out) {
    const char* s;
    size_t n;
    size_t i = 0u;
    int sign = 1;
    int exp_sign = 1;
    int exp_val = 0;
    int digits = 0;
    int frac_digits = 0;
    double value = 0.0;
    (void)delim;
    if (!out) return 0;
    sp = tlal_span_unquote(sp);
    if (sp.n == 0u || sp.n > TLAL_ATMOS_MAX_CELL || tlal_span_missing_token(sp)) return 0;
    s = sp.s;
    n = sp.n;
    if (s[i] == '+' || s[i] == '-') { if (s[i] == '-') sign = -1; ++i; }
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c >= '0' && c <= '9') { value = value * 10.0 + (double)(c - '0'); ++digits; ++i; continue; }
        break;
    }
    if (i < n && (s[i] == '.' || s[i] == ',')) {
        double scale = 0.1;
        ++i;
        while (i < n) {
            unsigned char c = (unsigned char)s[i];
            if (c >= '0' && c <= '9') { value += scale * (double)(c - '0'); scale *= 0.1; ++digits; ++frac_digits; ++i; continue; }
            break;
        }
    }
    if (digits == 0) return 0;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-')) { if (s[i] == '-') exp_sign = -1; ++i; }
        if (i >= n || s[i] < '0' || s[i] > '9') return 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (exp_val < 400) exp_val = exp_val * 10 + (int)(s[i] - '0');
            ++i;
        }
    }
    while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i != n) {
        int unit_like = 0;
        size_t j;
        /* RAMA/REDMET exportan a veces celdas como "12 ppb", "0.7 %" o
           "35 µg/m3". El número inicial sigue siendo la medición; la unidad ya
           se resuelve por columna o por parámetro. Se aceptan sólo sufijos con
           forma de unidad para no tragar textos arbitrarios como dato. */
        for (j = i; j < n; ++j) {
            const unsigned char u = (unsigned char)s[j];
            if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '%' || u == '/' || u == '^' || u == 0xC2u || u == 0xC3u) { unit_like = 1; break; }
        }
        if (!unit_like) return 0;
    }
    value *= (double)sign;
    if (exp_val) value *= pow(10.0, (double)(exp_sign * exp_val));
    if (!isfinite(value)) return 0;
    /* Centinelas típicos de series ambientales; se rechazan sólo si son exactos
       para no perder temperaturas negativas reales. */
    if ((frac_digits == 0 && (value == -999.0 || value == -9999.0 || value == 9999.0)) || value <= -1.0e30 || value >= 1.0e30) return 0;
    *out = value;
    return 1;
}

static int tlal_find_def_for_header(const char* key) {
    size_t i;
    if (!key || !key[0]) return -1;
    for (i = 0u; i < TLAL_ARRAY_COUNT(g_pembu_defs); ++i) {
        int a;
        if (tlal_streq(key, g_pembu_defs[i].canonical)) return (int)i;
        for (a = 0; a < 20 && g_pembu_defs[i].aliases[a]; ++a) if (tlal_streq(key, g_pembu_defs[i].aliases[a])) return (int)i;
    }
    return -1;
}

static const char* tlal_unit_for_canonical(const char* canonical) {
    size_t i;
    if (!canonical) return "";
    for (i = 0u; i < TLAL_ARRAY_COUNT(g_pembu_defs); ++i) if (tlal_streq(canonical, g_pembu_defs[i].canonical)) return g_pembu_defs[i].unit;
    return "";
}

static int tlal_parameter_is_meteorological_canonical(const char* canonical) {
    static const char* const meteo[] = {
        "tmp", "rh", "wsp", "wdr", "wgst", "wdr_gust", "pa", "pp", "gr", "uv", "uv_dose"
    };
    return tlal_streq_any(canonical, meteo, (int)TLAL_ARRAY_COUNT(meteo));
}

static int tlal_atmos_source_family_from_path(const char* path_utf8) {
    char key[TLAL_ATMOS_MAX_PATH_KEY];
    tlal_norm_cstr_copy(path_utf8, key, sizeof(key));
    if (tlal_norm_has_phrase(key, "ruoa") || tlal_norm_has_phrase(key, "pembu") ||
        tlal_norm_has_phrase(key, "observatorios atmosfericos")) return TLAL_ATMOS_SOURCE_RUOA;
    if (tlal_norm_has_phrase(key, "redmet") || tlal_norm_has_phrase(key, "redma") || tlal_norm_has_phrase(key, "meteorologicos") || tlal_norm_has_phrase(key, "meteorologico") ||
        tlal_norm_has_phrase(key, "meteorologica") || tlal_norm_has_phrase(key, "clima")) return TLAL_ATMOS_SOURCE_REDMA;
    if (tlal_norm_has_phrase(key, "rama") || tlal_norm_has_phrase(key, "simat") ||
        tlal_norm_has_phrase(key, "contaminantes") || tlal_norm_has_phrase(key, "contaminante")) return TLAL_ATMOS_SOURCE_RAMA;
    return TLAL_ATMOS_SOURCE_UNKNOWN;
}

static int tlal_atmos_source_family_accepts_parameter(int family, const char* canonical) {
    const int is_meteo = tlal_parameter_is_meteorological_canonical(canonical);
    if (!canonical || !canonical[0]) return 0;
    if (family == TLAL_ATMOS_SOURCE_RUOA) return is_meteo;
    if (family == TLAL_ATMOS_SOURCE_REDMA) return is_meteo;
    if (family == TLAL_ATMOS_SOURCE_RAMA) return !is_meteo;
    return 1;
}

static int tlal_find_or_canonicalize_parameter(TlalSpan sp, char* out, size_t cap) {
    char key[TLAL_ATMOS_MAX_KEY];
    int def;
    if (!out || cap == 0u) return 0;
    tlal_norm_key_copy(sp, key, sizeof(key));
    def = tlal_find_def_for_header(key);
    if (def >= 0) {
        snprintf(out, cap, "%s", g_pembu_defs[def].canonical);
        return 1;
    }
    return 0;
}

static int tlal_parameter_from_path(const char* path_utf8, char* out, size_t cap) {
    char key[TLAL_ATMOS_MAX_PATH_KEY];
    size_t i;
    if (!out || cap == 0u) return 0;
    out[0] = '\0';
    tlal_norm_cstr_copy(path_utf8, key, sizeof(key));
    for (i = 0u; i < TLAL_ARRAY_COUNT(g_pembu_defs); ++i) {
        int a;
        if (tlal_norm_has_phrase(key, g_pembu_defs[i].canonical)) { snprintf(out, cap, "%s", g_pembu_defs[i].canonical); return 1; }
        for (a = 0; a < 20 && g_pembu_defs[i].aliases[a]; ++a) {
            if (strlen(g_pembu_defs[i].aliases[a]) < 2u) continue;
            if (tlal_norm_has_phrase(key, g_pembu_defs[i].aliases[a])) { snprintf(out, cap, "%s", g_pembu_defs[i].canonical); return 1; }
        }
    }
    return 0;
}

static void tlal_station_code_copy(TlalSpan sp, char* out, size_t cap) {
    char tmp[96];
    size_t i;
    size_t w = 0u;
    if (!out || cap == 0u) return;
    tlal_span_copy0(sp, tmp, sizeof(tmp));
    for (i = 0u; tmp[i] && w + 1u < cap; ++i) {
        unsigned char c = (unsigned char)tmp[i];
        if (isalnum(c)) out[w++] = (char)toupper(c);
        else if (w) break;
    }
    out[w] = '\0';
}

static int tlal_station_header_copy(TlalSpan sp, char* out, size_t cap) {
    char key[TLAL_ATMOS_MAX_KEY];
    char tmp[64];
    size_t i;
    size_t w = 0u;
    int letters = 0;
    int digits = 0;
    static const char* const forbidden[] = {"date", "fecha", "time", "hora", "hr", "hour", "timestamp", "unit", "unidad", "valor", "value", "mean", "promedio", "lat", "lon", "longitud", "latitud"};
    if (!out || cap == 0u) return 0;
    out[0] = '\0';
    tlal_norm_key_copy(sp, key, sizeof(key));
    if (key[0] == '\0' || tlal_streq_any(key, forbidden, (int)TLAL_ARRAY_COUNT(forbidden)) || tlal_find_def_for_header(key) >= 0) return 0;
    tlal_span_copy0(sp, tmp, sizeof(tmp));
    for (i = 0u; tmp[i] && w + 1u < cap; ++i) {
        const unsigned char c = (unsigned char)tmp[i];
        if (isalnum(c)) {
            if (isalpha(c)) ++letters;
            if (isdigit(c)) ++digits;
            out[w++] = (char)toupper(c);
            continue;
        }
        if (w) break;
    }
    out[w] = '\0';
    if (w < 2u || w > 8u || letters == 0 || (letters == 0 && digits > 0)) { out[0] = '\0'; return 0; }
    return 1;
}

static int tlal_emit_atmos_row(TlalAtmosCsvRowFn callback, void* user,
                               const TlalAtmStamp* stamp, const char* station,
                               const char* parameter, const char* unit, double value,
                               int has_coordinates, double latitude, double longitude) {
    TlalAtmosCsvRow row;
    if (!callback || !stamp || !parameter || !parameter[0] || !isfinite(value)) return 1;
    row.stamp = *stamp;
    row.station_id = station && station[0] ? station : NULL;
    row.parameter_id = parameter;
    row.unit = unit && unit[0] ? unit : tlal_unit_for_canonical(parameter);
    row.value = value;
    row.latitude = latitude;
    row.longitude = longitude;
    row.has_coordinates = has_coordinates;
    return callback(&row, user);
}

static int tlal_line_is_noise(TlalSpan line) {
    TlalSpan t = tlal_span_trim(line);
    if (t.n == 0u) return 1;
    if (t.s[0] == '#' || t.s[0] == ';') return 1;
    if (t.n >= 2u && t.s[0] == '/' && t.s[1] == '/') return 1;
    if (t.n >= 5u && tlal_contains_lit_i(t.s, t.n < 64u ? t.n : 64u, "<html")) return 1;
    return 0;
}

static int tlal_atmos_csv_parse_file_utf8_impl(const char* path_utf8,
                                             int forced_source_family,
                                             TlalAtmosCsvRowFn callback,
                                             void* user,
                                             TlalAtmosCsvProgressFn progress_callback,
                                             void* progress_user,
                                             TlalAtmosCsvParseStats* stats) {
    TlalFileView view;
    TlalAtmosCsvParseStats st;
    const unsigned char* p;
    const unsigned char* end;
    int delim = ',';
    int header_done = 0;
    int date_col = -1, time_col = -1, stamp_col = -1, station_col = -1, param_col = -1, value_col = -1, unit_col = -1;
    int lat_col = -1, lon_col = -1;
    int col_defs[TLAL_ATMOS_MAX_COLS];
    char station_headers[TLAL_ATMOS_MAX_COLS][16];
    char path_parameter[64];
    TlalSpan cols[TLAL_ATMOS_MAX_COLS];
    int ok = 1;
    int source_family = TLAL_ATMOS_SOURCE_UNKNOWN;
    uint64_t next_progress_line = 16384u;
    uint64_t next_progress_byte = 1024u * 1024u;
    int i;
    memset(&st, 0, sizeof(st));
    for (i = 0; i < TLAL_ATMOS_MAX_COLS; ++i) { col_defs[i] = -1; station_headers[i][0] = '\0'; }
    path_parameter[0] = '\0';
    if (!path_utf8 || !callback) return 0;
    source_family = (forced_source_family >= TLAL_ATMOS_SOURCE_RAMA && forced_source_family <= TLAL_ATMOS_SOURCE_RUOA)
        ? forced_source_family
        : tlal_atmos_source_family_from_path(path_utf8);
    st.source_family = source_family;
    tlal_parameter_from_path(path_utf8, path_parameter, sizeof(path_parameter));
    if (path_parameter[0] && !tlal_atmos_source_family_accepts_parameter(source_family, path_parameter)) path_parameter[0] = '\0';
    if (!tlal_file_view_open_utf8(path_utf8, &view)) return 0;
    p = view.data;
    end = view.data + view.size;
    while (p < end) {
        const unsigned char* ls = p;
        const unsigned char* nl;
        TlalSpan line;
        int count;
        nl = (const unsigned char*)memchr(p, '\n', (size_t)(end - p));
        p = nl ? nl : end;
        line.s = (const char*)ls;
        line.n = (size_t)(p - ls);
        if (p < end && *p == '\n') ++p;
        while (line.n && (line.s[line.n - 1u] == '\r' || line.s[line.n - 1u] == '\n')) --line.n;
        if (progress_callback) {
            const uint64_t bytes_done = (uint64_t)(p - view.data);
            if (st.physical_lines >= next_progress_line || bytes_done >= next_progress_byte || p >= end) {
                if (!progress_callback(st.physical_lines, st.data_lines, st.emitted_measurements,
                                       bytes_done, (uint64_t)view.size, progress_user)) {
                    ok = 0;
                    goto done;
                }
                while (next_progress_line <= st.physical_lines) next_progress_line += 16384u;
                while (next_progress_byte <= bytes_done) next_progress_byte += 1024u * 1024u;
            }
        }
        if (tlal_line_is_noise(line)) continue;
        ++st.physical_lines;
        if (!header_done) {
            int c;
            int wide_count = 0;
            int station_wide_count = 0;
            date_col = time_col = stamp_col = station_col = param_col = value_col = unit_col = lat_col = lon_col = -1;
            for (i = 0; i < TLAL_ATMOS_MAX_COLS; ++i) { col_defs[i] = -1; station_headers[i][0] = '\0'; }
            delim = tlal_infer_delim_span(line);
            count = tlal_split_csv_span(line, delim, cols, TLAL_ATMOS_MAX_COLS);
            for (c = 0; c < count; ++c) {
                char key[TLAL_ATMOS_MAX_KEY];
                static const char* const date_keys[] = {"date", "fecha", "acq date", "acq_date", "dia", "dia medicion", "fecha medicion", "fecha local", "fch", "fechahora"};
                static const char* const time_keys[] = {"time", "hora", "hr", "hour", "acq time", "acq_time", "hora local", "horario", "h"};
                static const char* const stamp_keys[] = {"fecha hora", "fecha_hora", "timestamp", "time stamp", "date time", "datetime", "fecha local", "fecha utc", "fechahora", "fecha y hora"};
                static const char* const station_keys[] = {"id station", "station id", "cve station", "cve estac", "cve estacion", "cve_estac", "cve_estacion", "estacion", "id estacion", "clave estacion", "station", "sitio", "site", "id sitio", "clave sitio", "est", "station code"};
                static const char* const param_keys[] = {"id parameter", "id parametro", "cve parameter", "cve parametro", "cve param", "cve_param", "clave parametro", "parametro", "parameter", "param", "pollutant", "variable", "contaminante", "contaminante parametro", "id contaminante"};
                static const char* const value_keys[] = {"valor", "value", "measurement", "medicion", "mean", "promedio", "concentration", "concentracion", "lectura", "dato", "observacion", "obs", "valor horario", "promedio horario"};
                static const char* const unit_keys[] = {"unit", "units", "unidad", "unidades", "id unidad", "id_unit", "unit id", "unid", "uom"};
                static const char* const lat_keys[] = {"lat", "latitude", "latitud"};
                static const char* const lon_keys[] = {"lon", "lng", "long", "longitude", "longitud"};
                tlal_norm_key_copy(cols[c], key, sizeof(key));
                if (tlal_streq_any(key, date_keys, (int)TLAL_ARRAY_COUNT(date_keys))) { date_col = c; continue; }
                if (tlal_streq_any(key, time_keys, (int)TLAL_ARRAY_COUNT(time_keys))) { time_col = c; continue; }
                if (tlal_streq_any(key, stamp_keys, (int)TLAL_ARRAY_COUNT(stamp_keys))) { stamp_col = c; continue; }
                if (tlal_streq_any(key, station_keys, (int)TLAL_ARRAY_COUNT(station_keys))) { station_col = c; continue; }
                if (tlal_streq_any(key, param_keys, (int)TLAL_ARRAY_COUNT(param_keys))) { param_col = c; continue; }
                if (tlal_streq_any(key, value_keys, (int)TLAL_ARRAY_COUNT(value_keys))) { value_col = c; continue; }
                if (tlal_streq_any(key, unit_keys, (int)TLAL_ARRAY_COUNT(unit_keys))) { unit_col = c; continue; }
                if (tlal_streq_any(key, lat_keys, (int)TLAL_ARRAY_COUNT(lat_keys))) { lat_col = c; continue; }
                if (tlal_streq_any(key, lon_keys, (int)TLAL_ARRAY_COUNT(lon_keys))) { lon_col = c; continue; }
                col_defs[c] = tlal_find_def_for_header(key);
                if (col_defs[c] >= 0) { ++wide_count; continue; }
                if (path_parameter[0] && tlal_station_header_copy(cols[c], station_headers[c], sizeof(station_headers[c]))) ++station_wide_count;
            }
            /* Las tablas geoespaciales quedan al parser C++ existente para no
               colapsar coordenadas reales a una estación agregada. */
            if (lat_col >= 0 && lon_col >= 0 && station_col < 0) continue;
            if ((stamp_col >= 0 || date_col >= 0) && param_col >= 0 && value_col >= 0) {
                header_done = 1;
                st.header_found = 1;
                st.delimiter = delim;
                st.format_flags = TLAL_ATMOS_FMT_LONG;
            } else if ((stamp_col >= 0 || date_col >= 0) && wide_count > 0) {
                header_done = 1;
                st.header_found = 1;
                st.delimiter = delim;
                st.format_flags = TLAL_ATMOS_FMT_PARAMETER_WIDE;
                st.parameter_wide_columns = (uint64_t)wide_count;
            } else if ((stamp_col >= 0 || date_col >= 0) && path_parameter[0] && station_wide_count > 0) {
                header_done = 1;
                st.header_found = 1;
                st.delimiter = delim;
                st.format_flags = TLAL_ATMOS_FMT_STATION_WIDE;
                st.station_wide_columns = (uint64_t)station_wide_count;
            }
            continue;
        }
        count = tlal_split_csv_span(line, delim, cols, TLAL_ATMOS_MAX_COLS);
        if (count <= 0) continue;
        {
            TlalAtmStamp stamp;
            int stamp_ok = 0;
            char station_buf[64];
            double row_lat = 0.0;
            double row_lon = 0.0;
            int has_coordinates = 0;
            memset(&stamp, 0, sizeof(stamp));
            station_buf[0] = '\0';
            if (stamp_col >= 0 && stamp_col < count) stamp_ok = tlal_atm_parse_stamp(cols[stamp_col].s, cols[stamp_col].n, NULL, 0u, &stamp);
            else if (date_col >= 0 && date_col < count) {
                if (time_col >= 0 && time_col < count) stamp_ok = tlal_atm_parse_stamp(cols[date_col].s, cols[date_col].n, cols[time_col].s, cols[time_col].n, &stamp);
                else stamp_ok = tlal_atm_parse_stamp(cols[date_col].s, cols[date_col].n, NULL, 0u, &stamp);
            }
            if (!stamp_ok) { ++st.rejected_cells; continue; }
            if (stamp.minute == 0 && stamp.second == 0) ++st.hour_resolution_rows;
            else ++st.minute_resolution_rows;
            if (station_col >= 0 && station_col < count) tlal_station_code_copy(cols[station_col], station_buf, sizeof(station_buf));
            if (lat_col >= 0 && lon_col >= 0 && lat_col < count && lon_col < count &&
                tlal_span_to_double(cols[lat_col], delim, &row_lat) &&
                tlal_span_to_double(cols[lon_col], delim, &row_lon) &&
                row_lat >= -90.0 && row_lat <= 90.0 &&
                row_lon >= -180.0 && row_lon <= 180.0) {
                has_coordinates = 1;
            }
            ++st.data_lines;
            if (st.format_flags & TLAL_ATMOS_FMT_LONG) {
                char param_buf[64];
                char unit_buf[64];
                double value;
                if (param_col < 0 || value_col < 0 || param_col >= count || value_col >= count) { ++st.rejected_cells; continue; }
                if (!tlal_find_or_canonicalize_parameter(cols[param_col], param_buf, sizeof(param_buf))) { ++st.rejected_cells; continue; }
                if (!tlal_atmos_source_family_accepts_parameter(source_family, param_buf)) { ++st.rejected_cells; continue; }
                if (!tlal_span_to_double(cols[value_col], delim, &value)) { ++st.rejected_cells; continue; }
                unit_buf[0] = '\0';
                if (unit_col >= 0 && unit_col < count) tlal_span_copy0(cols[unit_col], unit_buf, sizeof(unit_buf));
                if (!unit_buf[0]) snprintf(unit_buf, sizeof(unit_buf), "%s", tlal_unit_for_canonical(param_buf));
                if (!tlal_emit_atmos_row(callback, user, &stamp, station_buf, param_buf, unit_buf, value,
                                         has_coordinates, row_lat, row_lon)) { ok = 0; goto done; }
                ++st.emitted_measurements;
            } else if (st.format_flags & TLAL_ATMOS_FMT_PARAMETER_WIDE) {
                for (i = 0; i < count && i < TLAL_ATMOS_MAX_COLS; ++i) {
                    int def_i = col_defs[i];
                    double value;
                    if (def_i < 0) continue;
                    if (!tlal_atmos_source_family_accepts_parameter(source_family, g_pembu_defs[def_i].canonical)) { ++st.rejected_cells; continue; }
                    if (!tlal_span_to_double(cols[i], delim, &value)) { ++st.rejected_cells; continue; }
                    if (!tlal_emit_atmos_row(callback, user, &stamp, station_buf,
                                           g_pembu_defs[def_i].canonical, g_pembu_defs[def_i].unit, value,
                                           has_coordinates, row_lat, row_lon)) { ok = 0; goto done; }
                    ++st.emitted_measurements;
                }
            } else if (st.format_flags & TLAL_ATMOS_FMT_STATION_WIDE) {
                const char* unit = tlal_unit_for_canonical(path_parameter);
                if (!tlal_atmos_source_family_accepts_parameter(source_family, path_parameter)) { ++st.rejected_cells; continue; }
                for (i = 0; i < count && i < TLAL_ATMOS_MAX_COLS; ++i) {
                    double value;
                    if (!station_headers[i][0]) continue;
                    if (!tlal_span_to_double(cols[i], delim, &value)) { ++st.rejected_cells; continue; }
                    if (!tlal_emit_atmos_row(callback, user, &stamp, station_headers[i], path_parameter, unit, value,
                                             has_coordinates, row_lat, row_lon)) { ok = 0; goto done; }
                    ++st.emitted_measurements;
                }
            }
        }
    }
done:
    if (progress_callback) {
        (void)progress_callback(st.physical_lines, st.data_lines, st.emitted_measurements,
                                (uint64_t)view.size, (uint64_t)view.size, progress_user);
    }
    tlal_file_view_close(&view);
    if (stats) *stats = st;
    return ok && st.header_found;
}

int tlal_atmos_csv_parse_file_utf8_for_family(const char* path_utf8,
                                              int forced_source_family,
                                              TlalAtmosCsvRowFn callback,
                                              void* user,
                                              TlalAtmosCsvParseStats* stats) {
    return tlal_atmos_csv_parse_file_utf8_impl(path_utf8, forced_source_family, callback, user, NULL, NULL, stats);
}

int tlal_atmos_csv_parse_file_utf8_for_family_progress(const char* path_utf8,
                                                       int forced_source_family,
                                                       TlalAtmosCsvRowFn callback,
                                                       void* user,
                                                       TlalAtmosCsvProgressFn progress_callback,
                                                       void* progress_user,
                                                       TlalAtmosCsvParseStats* stats) {
    return tlal_atmos_csv_parse_file_utf8_impl(path_utf8, forced_source_family, callback, user,
                                               progress_callback, progress_user, stats);
}

int tlal_atmos_csv_parse_file_utf8(const char* path_utf8,
                                   TlalAtmosCsvRowFn callback,
                                   void* user,
                                   TlalAtmosCsvParseStats* stats) {
    return tlal_atmos_csv_parse_file_utf8_impl(path_utf8, TLAL_ATMOS_SOURCE_UNKNOWN, callback, user, NULL, NULL, stats);
}

typedef struct TlalPembuWrapCtx {
    TlalPembuRowFn callback;
    void* user;
} TlalPembuWrapCtx;

static int tlal_pembu_wrap_callback(const TlalAtmosCsvRow* row, void* user) {
    TlalPembuWrapCtx* ctx = (TlalPembuWrapCtx*)user;
    TlalPembuRow out;
    if (!row || !ctx || !ctx->callback) return 0;
    out.stamp = row->stamp;
    out.parameter_id = row->parameter_id;
    out.unit = row->unit;
    out.value = row->value;
    return ctx->callback(&out, ctx->user);
}

int tlal_pembu_csv_parse_file_utf8(const char* path_utf8,
                                   TlalPembuRowFn callback,
                                   void* user,
                                   TlalPembuParseStats* stats) {
    TlalAtmosCsvParseStats ast;
    TlalPembuWrapCtx ctx;
    int ok;
    if (!path_utf8 || !callback) return 0;
    ctx.callback = callback;
    ctx.user = user;
    memset(&ast, 0, sizeof(ast));
    ok = tlal_atmos_csv_parse_file_utf8(path_utf8, tlal_pembu_wrap_callback, &ctx, &ast);
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->physical_lines = ast.physical_lines;
        stats->data_lines = ast.data_lines;
        stats->emitted_measurements = ast.emitted_measurements;
        stats->rejected_cells = ast.rejected_cells;
        stats->header_found = ast.header_found;
        stats->delimiter = ast.delimiter;
    }
    return ok;
}

/* ===== tlalpowa_hotdata.c ===== */
#line 1 "tlalpowa_hotdata.c"
#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#define TLALPOWA_PATH_SEP '\\'
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>
#define TLALPOWA_PATH_SEP '/'
#endif

#ifndef TLALPOWA_HOT_PATH_MAX
#define TLALPOWA_HOT_PATH_MAX 4096u
#endif

#define TLAL_IX_TRAILER_BYTES 40u
#define TLAL_IX_DIR_HEADER_BYTES 24u
#define TLAL_IX_DIR_ENTRY_BYTES 104u
#define TLAL_TOUCH_BLOCK_BYTES (256u * 1024u)
#define TLAL_MAX_PAYLOAD_BYTES (512ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_BUDGET (72ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_BUDGET (192ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_CACHE_BYTES (24ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_CACHE_BYTES (96ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES (64ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_RETAINED_MAP_BYTES (128ull * 1024ull * 1024ull)
#define TLAL_HOT_CACHE_LINES_MAX 4096u
#define TLAL_HOT_CACHE_LINES_DEFAULT 2048u
#define TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT 1u
#define TLAL_HOT_STARTUP_GATE_RECORDS_MAX 8u
#define TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT (64u * 1024u)
#define TLAL_HOT_STARTUP_GATE_BYTES_MAX (2u * 1024u * 1024u)
#define TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT 128u
#define TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX 256u
#define TLAL_HOT_STARTUP_WEEK_LIMIT_MAX 128u
#define TLAL_PAGE_PROBE_BYTES 4096u
#define TLAL_CORE_SLOT_COUNT 5u
#define TLAL_CORE_SLOT_ANY 0u
#define TLAL_CORE_SLOT_EPI 1u
#define TLAL_CORE_SLOT_MET 2u
#define TLAL_CORE_SLOT_CON 3u
#define TLAL_CORE_SLOT_OTH 4u

static const unsigned char TLAL_IX_MAGIC[8] = {'I','X','I','P','T','L','A','H'};
static const unsigned char TLAL_IX_DIR_MAGIC[8] = {'I','X','D','I','R','V','1','A'};
static const unsigned char TLAL_IX_DIR_END_MAGIC[8] = {'I','X','D','I','R','E','N','D'};

#ifdef _WIN32
typedef struct TlalMappedFile {
    unsigned char* data;
    uint64_t size;
    HANDLE file;
    HANDLE mapping;
    int mapped;
} TlalMappedFile;
#else
typedef struct TlalMappedFile {
    unsigned char* data;
    uint64_t size;
    int fd;
    int mapped;
} TlalMappedFile;
#endif

typedef struct TlalHotFile {
    char path[TLALPOWA_HOT_PATH_MAX];
    uint64_t size;
    uint32_t kind;
    TlalMappedFile map;
} TlalHotFile;

typedef struct TlalHotRecord {
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t stored_size;
    uint64_t raw_size;
    uint64_t layer_hash;
    uint64_t narrow_bucket;
    uint64_t hour_bucket;
    uint64_t week_bucket;
    uint64_t wide_bucket;
    uint32_t type;
    uint32_t schema;
    uint32_t codec;
    uint32_t core_group;
    uint32_t quality_flags;
    uint32_t file_index;
} TlalHotRecord;

typedef struct TlalHotCandidate {
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t stored_size;
    uint32_t type;
    uint32_t schema;
    uint32_t core_group;
    uint32_t file_index;
    uint32_t record_index;
} TlalHotCandidate;

typedef struct TlalHotCacheLine {
    unsigned char* data;
    uint64_t bytes;
    uint64_t tick;
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t layer_hash;
    uint32_t file_index;
    uint32_t record_index;
    uint32_t core_group;
} TlalHotCacheLine;

typedef struct TlalHotRuntimeIndex {
    TlalHotFile* files;
    size_t file_count;
    size_t file_cap;
    TlalHotRecord* records;
    size_t record_count;
    size_t record_cap;
    uint32_t* temporal_order;
    size_t temporal_order_count;
    uint32_t* core_order[TLAL_CORE_SLOT_COUNT];
    size_t core_order_count[TLAL_CORE_SLOT_COUNT];
    TlalHotCacheLine cache[TLAL_HOT_CACHE_LINES_MAX];
    uint64_t cache_bytes;
    uint64_t cache_limit_bytes;
    uint64_t cache_tick;
    uint64_t mapped_file_bytes;
    uint64_t mapped_file_limit_bytes;
    uint32_t cache_line_count;
} TlalHotRuntimeIndex;

typedef struct TlalHotState {
    TlalpowaHotDataConfig cfg;
    TlalpowaHotDataStats* stats;
    uint64_t budget_left;
    uint32_t ix_seen_limit;
    unsigned char* touch_buffer;
    TlalHotRuntimeIndex index;
    TlalHotCandidate latest_any;
    TlalHotCandidate latest_epi;
    TlalHotCandidate latest_met;
    TlalHotCandidate latest_con;
    TlalHotCandidate latest_oth;
} TlalHotState;

typedef struct TlalTopHit {
    uint64_t score;
    uint32_t record_index;
} TlalTopHit;

typedef struct TlalStartupCategory {
    uint32_t core_group;
    uint32_t type;
    uint32_t schema;
    uint64_t layer_hash;
    uint64_t temporal_key;
    uint32_t record_count;
    uint32_t record_indices[TLAL_HOT_STARTUP_GATE_RECORDS_MAX];
} TlalStartupCategory;

static TlalHotRuntimeIndex g_tlal_hot_index;
static volatile unsigned char g_tlal_hot_sink;

/*
CONTRATO FIJO DE HOT DATA TLALPOWA:
1) La bienvenida NO busca la fecha civil actual. Casi nunca los datos regionales
   estan al dia; por tanto el primer plano toma la ULTIMA SEMANA REAL con
   registros de al menos el 75% de las categorias fisicas esenciales.
2) Categoria fisica significa nucleo/tipo/esquema/capa; asi contaminantes,
   meteorologia y epidemiologia no se colapsan en un unico resumen ni en una
   fecha inventada.
3) La bienvenida puede durar lo necesario solo para esa hotdata esencial: una
   muestra inicial por categoria cubierta y la primera fecha visible preparada;
   mapa, teselas, movilidad y reconstrucciones quedan fuera del candado.
4) Tras entrar a la aplicacion, la fecha/hora activa solicitada tiene prioridad
   absoluta y sincronica: se sirve antes que cualquier vecino o barrido amplio.
5) Los vecinos cronologicos se precalientan despues, adelante/atras por distancia,
   del mas cercano al mas lejano, en segundo plano y sin robar la ruta activa.
6) El hilo progresivo posterior nunca bloquea bienvenida y corre con menor
   prioridad; el primer plano puede elevar prioridad temporalmente si pasan 2 s.
7) Nunca se sustituyen payloads por resumenes, sidecars ni agregados falsos.
*/
#ifdef _WIN32
static volatile LONG g_tlal_hot_lock_word = 0;
static void tlal_hot_lock(void) { while (InterlockedCompareExchange(&g_tlal_hot_lock_word, 1, 0) != 0) Sleep(0); }
static void tlal_hot_unlock(void) { InterlockedExchange(&g_tlal_hot_lock_word, 0); }
#else
static volatile int g_tlal_hot_lock_word = 0;
static void tlal_hot_lock(void) { while (__sync_lock_test_and_set(&g_tlal_hot_lock_word, 1)) sched_yield(); }
static void tlal_hot_unlock(void) { __sync_lock_release(&g_tlal_hot_lock_word); }
#endif

static void tlal_mapped_file_init(TlalMappedFile* mf) {
    if (!mf) return;
    memset(mf, 0, sizeof(*mf));
#ifdef _WIN32
    mf->file = INVALID_HANDLE_VALUE;
#else
    mf->fd = -1;
#endif
}

static uint32_t tlal_rd_u32_le(const unsigned char* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t tlal_rd_u64_le(const unsigned char* p) {
    return ((uint64_t)tlal_rd_u32_le(p)) | (((uint64_t)tlal_rd_u32_le(p + 4)) << 32);
}

static uint64_t tlal_u64_abs_diff(uint64_t a, uint64_t b) {
    return a >= b ? a - b : b - a;
}

TLAL_FORCE_INLINE int tlal_ascii_eq_ci_n(const char* a, const char* b, size_t n) {
    size_t i;
    for (i = 0u; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') ca = (unsigned char)(ca + 32u);
        if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') cb = (unsigned char)(cb + 32u);
        if (ca != cb) return 0;
    }
    return 1;
}

static int tlal_has_suffix_ascii(const char* path, const char* suffix) {
    size_t n, m;
    if (!path || !suffix) return 0;
    n = strlen(path);
    m = strlen(suffix);
    if (m > n) return 0;
#ifdef _WIN32
    return tlal_ascii_eq_ci_n(path + n - m, suffix, m);
#else
    return memcmp(path + n - m, suffix, m) == 0;
#endif
}

static int tlal_join_path(char* out, size_t out_cap, const char* a, const char* b) {
    size_t na, nb;
    if (!out || out_cap == 0u || !a || !b) return 0;
    na = strlen(a);
    nb = strlen(b);
    if (na + nb + 2u > out_cap) return 0;
    memcpy(out, a, na);
    if (na > 0u && a[na - 1u] != '/' && a[na - 1u] != '\\') out[na++] = TLALPOWA_PATH_SEP;
    memcpy(out + na, b, nb + 1u);
    return 1;
}

static uint64_t tlal_file_size_stream(FILE* f) {
    long long end_pos;
    if (!f) return 0ull;
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) return 0ull;
    end_pos = _ftelli64(f);
    if (_fseeki64(f, 0, SEEK_SET) != 0) return 0ull;
#else
    if (fseeko(f, 0, SEEK_END) != 0) return 0ull;
    end_pos = (long long)ftello(f);
    if (fseeko(f, 0, SEEK_SET) != 0) return 0ull;
#endif
    return end_pos > 0 ? (uint64_t)end_pos : 0ull;
}

static int tlal_seek_stream(FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, (long long)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

static void tlal_unmap_file(TlalMappedFile* mf) {
    if (!mf) return;
#ifdef _WIN32
    if (mf->data) UnmapViewOfFile(mf->data);
    if (mf->mapping) CloseHandle(mf->mapping);
    if (mf->file && mf->file != INVALID_HANDLE_VALUE) CloseHandle(mf->file);
    mf->data = NULL;
    mf->mapping = NULL;
    mf->file = INVALID_HANDLE_VALUE;
    mf->size = 0ull;
    mf->mapped = 0;
#else
    if (mf->data && mf->data != MAP_FAILED) munmap(mf->data, (size_t)mf->size);
    if (mf->fd >= 0) close(mf->fd);
    mf->data = NULL;
    mf->fd = -1;
    mf->size = 0ull;
    mf->mapped = 0;
#endif
}

static int tlal_map_file_readonly(const char* path, TlalMappedFile* mf) {
    if (!path || !mf) return 0;
    tlal_mapped_file_init(mf);
#ifdef _WIN32
    mf->file = INVALID_HANDLE_VALUE;
    mf->mapping = NULL;
    mf->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->file == INVALID_HANDLE_VALUE) return 0;
    {
        LARGE_INTEGER li;
        if (!GetFileSizeEx(mf->file, &li) || li.QuadPart <= 0) { tlal_unmap_file(mf); return 0; }
        mf->size = (uint64_t)li.QuadPart;
    }
    if (mf->size > TLAL_MAX_PAYLOAD_BYTES) { tlal_unmap_file(mf); return 0; }
    mf->mapping = CreateFileMappingA(mf->file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mf->mapping) { tlal_unmap_file(mf); return 0; }
    mf->data = (unsigned char*)MapViewOfFile(mf->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!mf->data) { tlal_unmap_file(mf); return 0; }
    mf->mapped = 1;
    return 1;
#else
    mf->fd = open(path, O_RDONLY);
    if (mf->fd < 0) return 0;
    {
        struct stat sb;
        if (fstat(mf->fd, &sb) != 0 || sb.st_size <= 0) { tlal_unmap_file(mf); return 0; }
        mf->size = (uint64_t)sb.st_size;
    }
    if (mf->size > TLAL_MAX_PAYLOAD_BYTES) { tlal_unmap_file(mf); return 0; }
    mf->data = (unsigned char*)mmap(NULL, (size_t)mf->size, PROT_READ, MAP_PRIVATE, mf->fd, 0);
    if (mf->data == MAP_FAILED) { mf->data = NULL; tlal_unmap_file(mf); return 0; }
    mf->mapped = 1;
    return 1;
#endif
}

static void tlal_runtime_index_free(TlalHotRuntimeIndex* ix) {
    size_t i;
    if (!ix) return;
    for (i = 0; i < ix->cache_line_count && i < TLAL_HOT_CACHE_LINES_MAX; ++i) {
        free(ix->cache[i].data);
        ix->cache[i].data = NULL;
    }
    {
        size_t slot;
        for (slot = 0u; slot < TLAL_CORE_SLOT_COUNT; ++slot) free(ix->core_order[slot]);
    }
    for (i = 0; i < ix->file_count; ++i) {
        tlal_unmap_file(&ix->files[i].map);
    }
    ix->mapped_file_bytes = 0ull;
    free(ix->files);
    free(ix->records);
    free(ix->temporal_order);
    memset(ix, 0, sizeof(*ix));
}

void tlalpowa_hotdata_release_runtime_index(void) {
    tlal_hot_lock();
    tlal_runtime_index_free(&g_tlal_hot_index);
    tlal_hot_unlock();
}

static int tlal_runtime_add_file(TlalHotRuntimeIndex* ix, const char* path, uint64_t size, uint32_t kind, uint32_t* out_index) {
    TlalHotFile* nf;
    size_t nc;
    if (!ix || !path || !out_index) return 0;
    if (ix->file_count == ix->file_cap) {
        nc = ix->file_cap ? ix->file_cap * 2u : 64u;
        nf = (TlalHotFile*)realloc(ix->files, nc * sizeof(*nf));
        if (!nf) return 0;
        ix->files = nf;
        ix->file_cap = nc;
    }
    *out_index = (uint32_t)ix->file_count;
    memset(&ix->files[ix->file_count], 0, sizeof(ix->files[ix->file_count]));
    tlal_mapped_file_init(&ix->files[ix->file_count].map);
    snprintf(ix->files[ix->file_count].path, sizeof(ix->files[ix->file_count].path), "%s", path);
    ix->files[ix->file_count].size = size;
    ix->files[ix->file_count].kind = kind;
    ix->file_count++;
    return 1;
}

static int tlal_runtime_add_record(TlalHotRuntimeIndex* ix, const TlalHotRecord* rec, uint32_t* out_index) {
    TlalHotRecord* nr;
    size_t nc;
    if (!ix || !rec || !out_index) return 0;
    if (ix->record_count == ix->record_cap) {
        nc = ix->record_cap ? ix->record_cap * 2u : 4096u;
        nr = (TlalHotRecord*)realloc(ix->records, nc * sizeof(*nr));
        if (!nr) return 0;
        ix->records = nr;
        ix->record_cap = nc;
    }
    *out_index = (uint32_t)ix->record_count;
    ix->records[ix->record_count++] = *rec;
    return 1;
}

static int tlal_type_is_epi(uint32_t t) {
    return t == 1u || t == 2u || t == 101u || t == 106u;
}

static int tlal_type_is_atm(uint32_t t) {
    return t == 10u || t == 11u || t == 12u || t == 13u || t == 14u || t == 15u ||
           t == 16u || t == 102u || t == 103u || t == 104u || t == 105u;
}

static uint32_t tlal_core_group_for_type(uint32_t t) {
    if (tlal_type_is_epi(t)) return TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY;
    if (t == 105u) return TLALPOWA_HOTDATA_CORE_METEOROLOGY;
    if (tlal_type_is_atm(t)) return TLALPOWA_HOTDATA_CORE_CONTAMINANT;
    return TLALPOWA_HOTDATA_CORE_OTHER;
}

static void tlal_consider_candidate(TlalHotCandidate* slot, const TlalHotRecord* rec, uint32_t record_index) {
    if (!slot || !rec || rec->temporal_key == 0ull) return;
    if (rec->temporal_key < slot->temporal_key) return;
    slot->temporal_key = rec->temporal_key;
    slot->payload_offset = rec->payload_offset;
    slot->stored_size = rec->stored_size;
    slot->type = rec->type;
    slot->schema = rec->schema;
    slot->core_group = rec->core_group;
    slot->file_index = rec->file_index;
    slot->record_index = record_index;
}

static void tlal_touch_mapped_span(TlalHotState* st, const unsigned char* data, uint64_t file_size, uint64_t offset, uint64_t requested, uint64_t* out_touched) {
    uint64_t remain, pos, end;
    if (out_touched) *out_touched = 0ull;
    if (!st || !data || offset >= file_size || requested == 0ull || st->budget_left == 0ull) return;
    remain = file_size - offset;
    if (remain > requested) remain = requested;
    if (remain > st->budget_left) remain = st->budget_left;
    end = offset + remain;
    pos = offset;
    while (pos < end) {
        g_tlal_hot_sink ^= data[pos];
        pos += TLAL_PAGE_PROBE_BYTES;
    }
    if (end > offset) g_tlal_hot_sink ^= data[end - 1ull];
    st->budget_left -= remain;
    if (st->stats) st->stats->touched_bytes += remain;
    if (out_touched) *out_touched = remain;
}

static void tlal_touch_file_span(TlalHotState* st, const char* path, uint64_t offset, uint64_t requested) {
    FILE* f;
    uint64_t size, remain, touched_here;
    if (!st || !path || !*path || !st->touch_buffer || st->budget_left == 0ull || requested == 0ull) return;
    f = fopen(path, "rb");
    if (!f) return;
    size = tlal_file_size_stream(f);
    if (offset >= size) { fclose(f); return; }
    remain = size - offset;
    if (remain > requested) remain = requested;
    if (remain > st->budget_left) remain = st->budget_left;
    if (!tlal_seek_stream(f, offset)) { fclose(f); return; }
    touched_here = 0ull;
    while (remain > 0ull) {
        size_t step = remain > TLAL_TOUCH_BLOCK_BYTES ? TLAL_TOUCH_BLOCK_BYTES : (size_t)remain;
        size_t got = fread(st->touch_buffer, 1u, step, f);
        if (got == 0u) break;
        g_tlal_hot_sink ^= st->touch_buffer[0];
        remain -= (uint64_t)got;
        st->budget_left -= (uint64_t)got;
        touched_here += (uint64_t)got;
        if (got < step || st->budget_left == 0ull) break;
    }
    if (st->stats) st->stats->touched_bytes += touched_here;
    fclose(f);
}

static uint64_t tlal_read_path_span(const char* path, uint64_t offset, void* out, uint64_t requested) {
    FILE* f;
    uint64_t size, remain;
    size_t got;
    if (!path || !*path || !out || requested == 0ull) return 0ull;
    f = fopen(path, "rb");
    if (!f) return 0ull;
    size = tlal_file_size_stream(f);
    if (offset >= size) { fclose(f); return 0ull; }
    remain = size - offset;
    if (remain > requested) remain = requested;
    if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
    if (!tlal_seek_stream(f, offset)) { fclose(f); return 0ull; }
    got = fread(out, 1u, (size_t)remain, f);
    fclose(f);
    return got;
}

static uint64_t tlal_read_hot_file_span(const TlalHotFile* file, uint64_t offset, void* out, uint64_t requested) {
    uint64_t remain;
    if (!file || !out || requested == 0ull) return 0ull;
    if (file->map.data && offset < file->map.size) {
        remain = file->map.size - offset;
        if (remain > requested) remain = requested;
        if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
        memcpy(out, file->map.data + offset, (size_t)remain);
        return remain;
    }
    return tlal_read_path_span(file->path, offset, out, requested);
}

static uint64_t tlal_runtime_cache_limit(const TlalHotRuntimeIndex* ix) {
    if (!ix || ix->cache_limit_bytes == 0ull) return TLAL_HOT_DEFAULT_CACHE_BYTES;
    return ix->cache_limit_bytes;
}

static uint32_t tlal_runtime_cache_lines(const TlalHotRuntimeIndex* ix) {
    if (!ix || ix->cache_line_count == 0u) return TLAL_HOT_CACHE_LINES_DEFAULT;
    return ix->cache_line_count > TLAL_HOT_CACHE_LINES_MAX ? TLAL_HOT_CACHE_LINES_MAX : ix->cache_line_count;
}

static int tlal_cache_line_matches(const TlalHotCacheLine* ln, const TlalHotRecord* rec, uint64_t need) {
    return ln && ln->data && rec && ln->file_index == rec->file_index &&
           ln->payload_offset == rec->payload_offset && ln->temporal_key == rec->temporal_key &&
           ln->layer_hash == rec->layer_hash && ln->bytes >= need;
}

static void tlal_cache_evict_line(TlalHotRuntimeIndex* ix, size_t pos) {
    if (!ix || pos >= TLAL_HOT_CACHE_LINES_MAX) return;
    if (ix->cache[pos].data) {
        if (ix->cache_bytes >= ix->cache[pos].bytes) ix->cache_bytes -= ix->cache[pos].bytes;
        else ix->cache_bytes = 0ull;
        free(ix->cache[pos].data);
    }
    memset(&ix->cache[pos], 0, sizeof(ix->cache[pos]));
}

static size_t tlal_cache_pick_slot(TlalHotRuntimeIndex* ix) {
    uint32_t limit, i;
    size_t oldest = 0u;
    uint64_t oldest_tick = UINT64_MAX;
    if (!ix) return 0u;
    limit = tlal_runtime_cache_lines(ix);
    for (i = 0u; i < limit; ++i) {
        if (!ix->cache[i].data) return (size_t)i;
        if (ix->cache[i].tick < oldest_tick) { oldest_tick = ix->cache[i].tick; oldest = (size_t)i; }
    }
    return oldest;
}

static uint64_t tlal_cache_load_record(TlalHotState* st, const TlalHotRecord* rec, uint64_t bytes) {
    const TlalHotFile* file;
    unsigned char* mem;
    uint64_t need, limit, room, got;
    size_t pos;
    uint32_t line_limit;
    if (!st || !rec || rec->file_index >= st->index.file_count || bytes == 0ull) return 0ull;
    file = &st->index.files[rec->file_index];
    if (!file->path[0]) return 0ull;
    need = rec->stored_size < bytes ? rec->stored_size : bytes;
    if (need == 0ull) return 0ull;
    limit = tlal_runtime_cache_limit(&st->index);
    if (limit == 0ull) return 0ull;
    if (need > (uint64_t)st->cfg.max_payload_bytes_per_record && st->cfg.max_payload_bytes_per_record != 0u)
        need = (uint64_t)st->cfg.max_payload_bytes_per_record;
    if (need > limit / 2ull && limit >= 2ull) need = limit / 2ull;
    if (st->budget_left != 0ull && need > st->budget_left) need = st->budget_left;
    if (need == 0ull) return 0ull;
    line_limit = tlal_runtime_cache_lines(&st->index);
    for (pos = 0u; pos < (size_t)line_limit; ++pos) {
        if (tlal_cache_line_matches(&st->index.cache[pos], rec, need)) {
            st->index.cache[pos].tick = ++st->index.cache_tick;
            if (st->stats) { st->stats->cache_hits += 1ull; st->stats->cache_bytes = st->index.cache_bytes; }
            g_tlal_hot_sink ^= st->index.cache[pos].data[0];
            return need;
        }
    }
    if (st->stats) st->stats->cache_misses += 1ull;
    room = limit;
    while (st->index.cache_bytes + need > room) {
        uint64_t before = st->index.cache_bytes;
        tlal_cache_evict_line(&st->index, tlal_cache_pick_slot(&st->index));
        if (st->index.cache_bytes == before) break;
    }
    if (need > room || st->index.cache_bytes + need > room) return 0ull;
    mem = (unsigned char*)malloc((size_t)need);
    if (!mem) return 0ull;
    got = tlal_read_hot_file_span(file, rec->payload_offset, mem, need);
    if (got == 0ull) { free(mem); return 0ull; }
    pos = tlal_cache_pick_slot(&st->index);
    tlal_cache_evict_line(&st->index, pos);
    st->index.cache[pos].data = mem;
    st->index.cache[pos].bytes = got;
    st->index.cache[pos].tick = ++st->index.cache_tick;
    st->index.cache[pos].temporal_key = rec->temporal_key;
    st->index.cache[pos].payload_offset = rec->payload_offset;
    st->index.cache[pos].layer_hash = rec->layer_hash;
    st->index.cache[pos].file_index = rec->file_index;
    st->index.cache[pos].record_index = (uint32_t)(rec - st->index.records);
    st->index.cache[pos].core_group = rec->core_group;
    st->index.cache_bytes += got;
    st->budget_left = st->budget_left > got ? st->budget_left - got : 0ull;
    if (st->stats) { st->stats->touched_bytes += got; st->stats->cache_bytes = st->index.cache_bytes; }
    g_tlal_hot_sink ^= mem[0];
    return (uint64_t)got;
}

static void tlal_touch_record(TlalHotState* st, const TlalHotRecord* rec, uint64_t bytes) {
    const TlalHotFile* file;
    uint64_t n;
    if (!st || !rec || rec->file_index >= st->index.file_count || bytes == 0ull) return;
    n = rec->stored_size < bytes ? rec->stored_size : bytes;
    if (tlal_cache_load_record(st, rec, n) != 0ull) return;
    file = &st->index.files[rec->file_index];
    if (file->map.data) {
        tlal_touch_mapped_span(st, file->map.data, file->map.size, rec->payload_offset, n, NULL);
        return;
    }
    tlal_touch_file_span(st, file->path, rec->payload_offset, n);
}

static int tlal_parse_ixiptlah_directory_mapped(TlalHotState* st, const char* path) {
    TlalMappedFile mf;
    uint64_t size, count, dir_off, entries_bytes, expected_end;
    uint32_t version, entry_size, dir_version, file_index;
    const unsigned char* trailer;
    const unsigned char* dir;
    const unsigned char* entry;
    uint64_t i;
    if (!st || !path) return 0;
    if (!tlal_map_file_readonly(path, &mf)) return 0;
    size = mf.size;
    if (st->stats) st->stats->mapped_files += 1ull;
    if (size < 12ull + TLAL_IX_TRAILER_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (memcmp(mf.data, TLAL_IX_MAGIC, sizeof(TLAL_IX_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    version = tlal_rd_u32_le(mf.data + 8);
    if (version != 1u) { tlal_unmap_file(&mf); return 0; }
    trailer = mf.data + (size - TLAL_IX_TRAILER_BYTES);
    if (memcmp(trailer, TLAL_IX_DIR_END_MAGIC, sizeof(TLAL_IX_DIR_END_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    dir_version = tlal_rd_u32_le(trailer + 8);
    entry_size = tlal_rd_u32_le(trailer + 12);
    count = tlal_rd_u64_le(trailer + 16);
    dir_off = tlal_rd_u64_le(trailer + 24);
    if (dir_version != 1u || entry_size < 48u || entry_size > TLAL_IX_DIR_ENTRY_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (count > 16777216ull) { tlal_unmap_file(&mf); return 0; }
    entries_bytes = count * (uint64_t)entry_size;
    if (count != 0ull && entries_bytes / count != (uint64_t)entry_size) { tlal_unmap_file(&mf); return 0; }
    expected_end = dir_off + TLAL_IX_DIR_HEADER_BYTES + entries_bytes;
    if (expected_end != size - TLAL_IX_TRAILER_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (dir_off + TLAL_IX_DIR_HEADER_BYTES > size) { tlal_unmap_file(&mf); return 0; }
    dir = mf.data + dir_off;
    if (memcmp(dir, TLAL_IX_DIR_MAGIC, sizeof(TLAL_IX_DIR_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    if (tlal_rd_u32_le(dir + 8) != 1u || tlal_rd_u32_le(dir + 12) != entry_size || tlal_rd_u64_le(dir + 16) != count) { tlal_unmap_file(&mf); return 0; }
    if (!tlal_runtime_add_file(&st->index, path, size, 1u, &file_index)) { tlal_unmap_file(&mf); return 0; }

    if (st->stats) {
        st->stats->ixiptlah_records += count;
        st->stats->ixiptlah_directories += 1ull;
    }

    entry = dir + TLAL_IX_DIR_HEADER_BYTES;
    for (i = 0; i < count; ++i, entry += entry_size) {
        TlalHotRecord rec;
        uint32_t record_index;
        uint64_t probed = 0ull;
        memset(&rec, 0, sizeof(rec));
        rec.type = tlal_rd_u32_le(entry + 0);
        rec.schema = tlal_rd_u32_le(entry + 4);
        rec.payload_offset = tlal_rd_u64_le(entry + 8);
        rec.stored_size = tlal_rd_u64_le(entry + 16);
        rec.raw_size = tlal_rd_u64_le(entry + 24);
        rec.codec = tlal_rd_u32_le(entry + 32);
        rec.quality_flags = tlal_rd_u32_le(entry + 36);
        rec.layer_hash = tlal_rd_u64_le(entry + 40);
        rec.temporal_key = tlal_rd_u64_le(entry + 48);
        rec.file_index = file_index;
        if (entry_size >= 104u) {
            rec.narrow_bucket = tlal_rd_u64_le(entry + 64);
            rec.hour_bucket = tlal_rd_u64_le(entry + 72);
            rec.week_bucket = tlal_rd_u64_le(entry + 80);
            rec.wide_bucket = tlal_rd_u64_le(entry + 88);
            rec.core_group = tlal_rd_u32_le(entry + 96);
            rec.quality_flags = tlal_rd_u32_le(entry + 100);
        }
        if (rec.core_group == 0u) rec.core_group = tlal_core_group_for_type(rec.type);
        if (rec.payload_offset > dir_off || rec.stored_size > dir_off - rec.payload_offset) continue;
        if (rec.stored_size > TLAL_MAX_PAYLOAD_BYTES || rec.raw_size > TLAL_MAX_PAYLOAD_BYTES) continue;
        if (rec.codec != 0u && rec.codec != 1u && rec.codec != 2u) continue;
        if (!tlal_runtime_add_record(&st->index, &rec, &record_index)) break;
        if (st->stats) st->stats->indexed_records += 1ull;
        tlal_consider_candidate(&st->latest_any, &rec, record_index);
        if (rec.core_group == TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY) tlal_consider_candidate(&st->latest_epi, &rec, record_index);
        else if (rec.core_group == TLALPOWA_HOTDATA_CORE_METEOROLOGY) tlal_consider_candidate(&st->latest_met, &rec, record_index);
        else if (rec.core_group == TLALPOWA_HOTDATA_CORE_CONTAMINANT) tlal_consider_candidate(&st->latest_con, &rec, record_index);
        else tlal_consider_candidate(&st->latest_oth, &rec, record_index);
        if (st->cfg.probe_bytes_per_record != 0u && st->budget_left != 0ull) {
            uint64_t n = rec.stored_size < (uint64_t)st->cfg.probe_bytes_per_record ? rec.stored_size : (uint64_t)st->cfg.probe_bytes_per_record;
            tlal_touch_mapped_span(st, mf.data, mf.size, rec.payload_offset, n, &probed);
            if (st->stats) st->stats->record_probe_bytes += probed;
        }
    }
    if (st->cfg.keep_runtime_index && file_index < st->index.file_count && mf.mapped &&
        st->index.mapped_file_bytes <= st->index.mapped_file_limit_bytes &&
        mf.size <= st->index.mapped_file_limit_bytes - st->index.mapped_file_bytes) {
        st->index.files[file_index].map = mf;
        st->index.mapped_file_bytes += mf.size;
        if (st->stats) st->stats->retained_mapped_file_bytes = st->index.mapped_file_bytes;
        tlal_mapped_file_init(&mf);
    }
    tlal_unmap_file(&mf);
    return 1;
}

static void tlal_prewarm_candidate(TlalHotState* st, const TlalHotCandidate* c, uint64_t per) {
    if (!st || !c || c->temporal_key == 0ull || c->record_index >= st->index.record_count) return;
    tlal_touch_record(st, &st->index.records[c->record_index], per);
}

static void tlal_prewarm_latest_candidates(TlalHotState* st) {
    uint64_t per;
    if (!st) return;
    per = st->cfg.max_payload_bytes_per_record ? (uint64_t)st->cfg.max_payload_bytes_per_record : (2ull * 1024ull * 1024ull);
    tlal_prewarm_candidate(st, &st->latest_con, per);
    tlal_prewarm_candidate(st, &st->latest_met, per);
    tlal_prewarm_candidate(st, &st->latest_epi, per);
    tlal_prewarm_candidate(st, &st->latest_oth, per / 2ull);
    if (st->stats) {
        st->stats->latest_contaminant_key = st->latest_con.temporal_key;
        st->stats->latest_meteorology_key = st->latest_met.temporal_key;
        st->stats->latest_epidemiology_key = st->latest_epi.temporal_key;
        st->stats->latest_atmosphere_key = st->latest_con.temporal_key ? st->latest_con.temporal_key : st->latest_met.temporal_key;
        st->stats->latest_temporal_key = st->latest_any.temporal_key;
    }
}

static const TlalHotRecord* g_sort_records = NULL;
static int tlal_cmp_record_order(const void* a, const void* b) {
    const uint32_t ia = *(const uint32_t*)a;
    const uint32_t ib = *(const uint32_t*)b;
    const TlalHotRecord* ra = &g_sort_records[ia];
    const TlalHotRecord* rb = &g_sort_records[ib];
    if (ra->temporal_key != rb->temporal_key) return ra->temporal_key < rb->temporal_key ? -1 : 1;
    if (ra->core_group != rb->core_group) return ra->core_group < rb->core_group ? -1 : 1;
    if (ra->type != rb->type) return ra->type < rb->type ? -1 : 1;
    if (ra->file_index != rb->file_index) return ra->file_index < rb->file_index ? -1 : 1;
    if (ra->payload_offset != rb->payload_offset) return ra->payload_offset < rb->payload_offset ? -1 : 1;
    return 0;
}

static uint32_t tlal_core_slot(uint32_t core_group) {
    if (core_group == TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY) return TLAL_CORE_SLOT_EPI;
    if (core_group == TLALPOWA_HOTDATA_CORE_METEOROLOGY) return TLAL_CORE_SLOT_MET;
    if (core_group == TLALPOWA_HOTDATA_CORE_CONTAMINANT) return TLAL_CORE_SLOT_CON;
    if (core_group == TLALPOWA_HOTDATA_CORE_OTHER) return TLAL_CORE_SLOT_OTH;
    return TLAL_CORE_SLOT_ANY;
}

static int tlal_runtime_alloc_order(uint32_t** out, size_t n) {
    if (!out) return 0;
    *out = NULL;
    if (n == 0u) return 1;
    *out = (uint32_t*)malloc(n * sizeof(**out));
    return *out != NULL;
}

static int tlal_runtime_build_temporal_order(TlalHotRuntimeIndex* ix) {
    size_t i, n;
    size_t counts[TLAL_CORE_SLOT_COUNT];
    size_t write_pos[TLAL_CORE_SLOT_COUNT];
    if (!ix || ix->record_count == 0u) return 0;
    free(ix->temporal_order);
    ix->temporal_order = NULL;
    ix->temporal_order_count = 0u;
    for (i = 0u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        free(ix->core_order[i]);
        ix->core_order[i] = NULL;
        ix->core_order_count[i] = 0u;
        counts[i] = 0u;
        write_pos[i] = 0u;
    }
    n = ix->record_count;
    if (!tlal_runtime_alloc_order(&ix->temporal_order, n)) return 0;
    for (i = 0u; i < n; ++i) {
        uint32_t slot = tlal_core_slot(ix->records[i].core_group);
        ix->temporal_order[i] = (uint32_t)i;
        if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT) counts[slot] += 1u;
    }
    for (i = 1u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        if (!tlal_runtime_alloc_order(&ix->core_order[i], counts[i])) {
            size_t j;
            for (j = 1u; j < TLAL_CORE_SLOT_COUNT; ++j) { free(ix->core_order[j]); ix->core_order[j] = NULL; ix->core_order_count[j] = 0u; }
            free(ix->temporal_order); ix->temporal_order = NULL; ix->temporal_order_count = 0u;
            return 0;
        }
        ix->core_order_count[i] = counts[i];
    }
    for (i = 0u; i < n; ++i) {
        uint32_t slot = tlal_core_slot(ix->records[i].core_group);
        if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT && ix->core_order[slot]) {
            ix->core_order[slot][write_pos[slot]++] = (uint32_t)i;
        }
    }
    g_sort_records = ix->records;
    qsort(ix->temporal_order, n, sizeof(*ix->temporal_order), tlal_cmp_record_order);
    ix->temporal_order_count = n;
    for (i = 1u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        if (ix->core_order[i] && ix->core_order_count[i] > 1u)
            qsort(ix->core_order[i], ix->core_order_count[i], sizeof(*ix->core_order[i]), tlal_cmp_record_order);
    }
    g_sort_records = NULL;
    return 1;
}

static int tlal_record_matches_core(const TlalHotRecord* r, uint32_t core_group) {
    return r && (core_group == TLALPOWA_HOTDATA_CORE_ANY || r->core_group == core_group);
}

static const uint32_t* tlal_runtime_order_for_core(const TlalHotRuntimeIndex* ix, uint32_t core_group, size_t* out_count) {
    uint32_t slot;
    if (out_count) *out_count = 0u;
    if (!ix) return NULL;
    slot = tlal_core_slot(core_group);
    if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT && ix->core_order[slot] && ix->core_order_count[slot] != 0u) {
        if (out_count) *out_count = ix->core_order_count[slot];
        return ix->core_order[slot];
    }
    if (ix->temporal_order && ix->temporal_order_count != 0u) {
        if (out_count) *out_count = ix->temporal_order_count;
        return ix->temporal_order;
    }
    return NULL;
}

static size_t tlal_temporal_lower_bound_order(const TlalHotRuntimeIndex* ix, const uint32_t* order, size_t count, uint64_t key) {
    size_t lo = 0u, hi = count;
    if (!ix || !order || count == 0u) return 0u;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1u);
        const TlalHotRecord* r = &ix->records[order[mid]];
        if (r->temporal_key < key) lo = mid + 1u;
        else hi = mid;
    }
    return lo;
}

static int tlal_runtime_find_nearest_record_index(const TlalHotRuntimeIndex* ix, uint32_t core_group, uint64_t temporal_key, uint32_t* out_record_index) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint64_t best = UINT64_MAX;
    uint32_t best_index = UINT32_MAX;
    uint32_t guard = 0u;
    if (out_record_index) *out_record_index = UINT32_MAX;
    if (!ix || !out_record_index || !ix->records || ix->record_count == 0u || temporal_key == 0ull) return 0;
    order = tlal_runtime_order_for_core(ix, core_group, &order_count);
    if (!order || order_count == 0u) {
        size_t i;
        for (i = 0; i < ix->record_count; ++i) {
            const TlalHotRecord* r = &ix->records[i];
            uint64_t d;
            if (!tlal_record_matches_core(r, core_group) || r->temporal_key == 0ull) continue;
            d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d < best) { best = d; best_index = (uint32_t)i; if (d == 0ull) break; }
        }
        if (best_index == UINT32_MAX) return 0;
        *out_record_index = best_index;
        return 1;
    }
    right = tlal_temporal_lower_bound_order(ix, order, order_count, temporal_key);
    left = right;
    while ((left > 0u || right < order_count) && guard < 1024u) {
        int take_left = 0;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &ix->records[order[left - 1u]];
            const TlalHotRecord* rr = &ix->records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else take_left = left > 0u;
        if (take_left) {
            const uint32_t ri = order[--left];
            const TlalHotRecord* r = &ix->records[ri];
            uint64_t d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d > best && best_index != UINT32_MAX) break;
            if (tlal_record_matches_core(r, core_group)) { best = d; best_index = ri; if (d == 0ull) break; }
        } else {
            const uint32_t ri = order[right++];
            const TlalHotRecord* r = &ix->records[ri];
            uint64_t d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d > best && best_index != UINT32_MAX) break;
            if (tlal_record_matches_core(r, core_group)) { best = d; best_index = ri; if (d == 0ull) break; }
        }
        ++guard;
    }
    if (best_index == UINT32_MAX) return 0;
    *out_record_index = best_index;
    return 1;
}

static void tlal_hit_from_record(const TlalHotRuntimeIndex* ix, const TlalHotRecord* r, TlalpowaHotDataHit* hit) {
    if (!ix || !r || !hit || r->file_index >= ix->file_count) return;
    memset(hit, 0, sizeof(*hit));
    hit->temporal_key = r->temporal_key;
    hit->payload_offset = r->payload_offset;
    hit->stored_size = r->stored_size;
    hit->layer_hash = r->layer_hash;
    hit->type = r->type;
    hit->schema = r->schema;
    hit->core_group = r->core_group;
    hit->file_index = r->file_index;
    snprintf(hit->path, sizeof(hit->path), "%s", ix->files[r->file_index].path);
}

/*
Orden temporal estricto: la salida inicia con la llave solicitada si existe; si no,
con el registro real mas cercano. Despues alterna izquierda/derecha segun
distancia absoluta, por lo que el prefetch avanza desde el vecino mas cercano
hacia el mas lejano sin barrer registros ajenos a la familia solicitada.
Durante la bienvenida esta rutina se usa solo con los ultimos registros por
categoria; los vecinos cronologicos amplios se cargan ya con la interfaz viva.
*/
static uint32_t tlal_collect_record_indices_near(const TlalHotRuntimeIndex* ix,
                                                 uint32_t core_group,
                                                 uint64_t temporal_key,
                                                 uint32_t max_hits,
                                                 uint32_t* out_indices,
                                                 uint64_t* out_exact_hits) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint32_t out = 0u;
    uint32_t guard = 0u;
    uint64_t exact = 0ull;
    if (out_exact_hits) *out_exact_hits = 0ull;
    if (!ix || !out_indices || max_hits == 0u || temporal_key == 0ull || !ix->records || ix->record_count == 0u) return 0u;
    order = tlal_runtime_order_for_core(ix, core_group, &order_count);
    if (!order || order_count == 0u) return 0u;
    right = tlal_temporal_lower_bound_order(ix, order, order_count, temporal_key);
    left = right;
    while (out < max_hits && (left > 0u || right < order_count) && guard < 4096u) {
        int take_left = 0;
        const TlalHotRecord* r;
        uint32_t ri;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &ix->records[order[left - 1u]];
            const TlalHotRecord* rr = &ix->records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else {
            take_left = left > 0u;
        }
        ri = take_left ? order[--left] : order[right++];
        r = &ix->records[ri];
        if (tlal_record_matches_core(r, core_group)) {
            out_indices[out++] = ri;
            if (r->temporal_key == temporal_key) exact += 1ull;
        }
        ++guard;
    }
    if (out_exact_hits) *out_exact_hits = exact;
    return out;
}

/*
Prefetch progresivo: temporal_key es la fecha activa o la ultima fecha real de
una categoria. Se toca primero esa llave o su vecino fisico mas cercano; luego
los registros adelante/atras por cercania temporal real, sin bloquear la
bienvenida cuando se invoca desde el hilo de fondo.
*/
static void tlal_prewarm_temporal_near(TlalHotState* st, uint32_t core_group, uint64_t temporal_key, uint32_t want, uint64_t bytes) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint32_t touched = 0u, guard = 0u;
    if (!st || temporal_key == 0ull || want == 0u || bytes == 0ull || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    order = tlal_runtime_order_for_core(&st->index, core_group, &order_count);
    if (!order || order_count == 0u) return;
    right = tlal_temporal_lower_bound_order(&st->index, order, order_count, temporal_key);
    left = right;
    if (st->stats) st->stats->binary_searches += 1ull;
    while (touched < want && (left > 0u || right < order_count) && guard < 4096u && st->budget_left != 0ull) {
        int take_left = 0;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &st->index.records[order[left - 1u]];
            const TlalHotRecord* rr = &st->index.records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else take_left = left > 0u;
        if (take_left) {
            const TlalHotRecord* r = &st->index.records[order[--left]];
            if (tlal_record_matches_core(r, core_group)) { tlal_touch_record(st, r, bytes); ++touched; }
        } else {
            const TlalHotRecord* r = &st->index.records[order[right++]];
            if (tlal_record_matches_core(r, core_group)) { tlal_touch_record(st, r, bytes); ++touched; }
        }
        ++guard;
    }
    if (st->stats) st->stats->progressive_records_touched += touched;
}

static int tlal_startup_category_same(const TlalStartupCategory* c, const TlalHotRecord* r) {
    return c && r && c->core_group == r->core_group && c->type == r->type &&
           c->schema == r->schema && c->layer_hash == r->layer_hash;
}


static int tlal_hot_leap_year(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static int tlal_hot_day_of_year(int y, int m, int d) {
    static const uint16_t days_before_month[2][13] = {
        {0,0,31,59,90,120,151,181,212,243,273,304,334},
        {0,0,31,60,91,121,152,182,213,244,274,305,335}
    };
    if (y < 1 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    return (int)days_before_month[tlal_hot_leap_year(y) ? 1 : 0][m] + d;
}

static uint64_t tlal_temporal_week_bucket_from_key(uint64_t key) {
    int y, m, d, w, doy;
    if (key == 0ull) return 0ull;
    if (key >= 10000000000ull) {
        key /= 100ull;
        key /= 100ull;
        d = (int)(key % 100ull);
        key /= 100ull;
        m = (int)(key % 100ull);
        key /= 100ull;
        y = (int)key;
        doy = tlal_hot_day_of_year(y, m, d);
        if (doy <= 0) return 0ull;
        w = (doy + 6) / 7;
        if (w < 1) w = 1;
        if (w > 53) w = 53;
        return (uint64_t)y * 100ull + (uint64_t)w;
    }
    if (key >= 1000000ull) {
        uint64_t yw = key / 10000ull;
        y = (int)(yw / 100ull);
        w = (int)(yw % 100ull);
        if (y > 0 && w >= 1 && w <= 53) return (uint64_t)y * 100ull + (uint64_t)w;
    }
    if (key >= 10000ull) {
        y = (int)(key / 10000ull);
        w = (int)((key / 100ull) % 100ull);
        if (y > 0 && w >= 1 && w <= 53) return (uint64_t)y * 100ull + (uint64_t)w;
    }
    return 0ull;
}

static uint64_t tlal_record_week_bucket(const TlalHotRecord* r) {
    if (!r) return 0ull;
    if (r->week_bucket != 0ull) {
        if (r->week_bucket > 1000000ull) return tlal_temporal_week_bucket_from_key(r->week_bucket);
        return r->week_bucket;
    }
    return tlal_temporal_week_bucket_from_key(r->temporal_key);
}

typedef struct TlalStartupWeekGate {
    uint64_t week_bucket;
    uint32_t covered_categories;
    uint32_t record_by_category[TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX];
} TlalStartupWeekGate;

static uint32_t tlal_startup_find_category_index(const TlalStartupCategory* cats, uint32_t count, const TlalHotRecord* r) {
    uint32_t i;
    if (!cats || !r) return UINT32_MAX;
    for (i = 0u; i < count; ++i) {
        if (cats[i].core_group == r->core_group && cats[i].type == r->type &&
            cats[i].schema == r->schema && cats[i].layer_hash == r->layer_hash) return i;
    }
    return UINT32_MAX;
}

static uint32_t tlal_startup_find_week_index(TlalStartupWeekGate* weeks, uint32_t* count, uint64_t week_bucket) {
    uint32_t i, weakest;
    uint64_t weakest_week;
    if (!weeks || !count || week_bucket == 0ull) return UINT32_MAX;
    for (i = 0u; i < *count; ++i) if (weeks[i].week_bucket == week_bucket) return i;
    if (*count < TLAL_HOT_STARTUP_WEEK_LIMIT_MAX) {
        uint32_t pos = (*count)++;
        weeks[pos].week_bucket = week_bucket;
        weeks[pos].covered_categories = 0u;
        for (i = 0u; i < TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX; ++i) weeks[pos].record_by_category[i] = UINT32_MAX;
        return pos;
    }
    weakest = UINT32_MAX;
    weakest_week = UINT64_MAX;
    for (i = 0u; i < *count; ++i) {
        if (weeks[i].week_bucket < weakest_week) { weakest_week = weeks[i].week_bucket; weakest = i; }
    }
    if (weakest != UINT32_MAX && week_bucket > weakest_week) {
        weeks[weakest].week_bucket = week_bucket;
        weeks[weakest].covered_categories = 0u;
        for (i = 0u; i < TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX; ++i) weeks[weakest].record_by_category[i] = UINT32_MAX;
        return weakest;
    }
    return UINT32_MAX;
}

static int tlal_startup_record_is_better_for_week(const TlalHotRuntimeIndex* ix, uint32_t old_ri, uint32_t new_ri) {
    const TlalHotRecord* a;
    const TlalHotRecord* b;
    if (!ix || new_ri >= ix->record_count) return 0;
    if (old_ri == UINT32_MAX || old_ri >= ix->record_count) return 1;
    a = &ix->records[old_ri];
    b = &ix->records[new_ri];
    if (b->temporal_key != a->temporal_key) return b->temporal_key > a->temporal_key;
    return b->stored_size > a->stored_size;
}

static void tlal_startup_category_insert_record(TlalStartupCategory* c,
                                                const TlalHotRuntimeIndex* ix,
                                                uint32_t record_index,
                                                uint32_t per_category) {
    uint32_t pos, i;
    uint64_t key;
    if (!c || !ix || record_index >= ix->record_count || per_category == 0u) return;
    if (per_category > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    key = ix->records[record_index].temporal_key;
    if (key == 0ull) return;
    for (i = 0u; i < c->record_count; ++i) {
        if (c->record_indices[i] == record_index) return;
    }
    pos = c->record_count;
    for (i = 0u; i < c->record_count; ++i) {
        const uint32_t ri = c->record_indices[i];
        const uint64_t old_key = ri < ix->record_count ? ix->records[ri].temporal_key : 0ull;
        if (key > old_key || (key == old_key && record_index > ri)) { pos = i; break; }
    }
    if (c->record_count < per_category) {
        for (i = c->record_count; i > pos; --i) c->record_indices[i] = c->record_indices[i - 1u];
        c->record_indices[pos] = record_index;
        c->record_count += 1u;
    } else if (pos < per_category) {
        for (i = per_category - 1u; i > pos; --i) c->record_indices[i] = c->record_indices[i - 1u];
        c->record_indices[pos] = record_index;
    }
    if (c->record_count != 0u) {
        const uint32_t top = c->record_indices[0];
        c->temporal_key = top < ix->record_count ? ix->records[top].temporal_key : key;
    }
}

static void tlal_startup_category_consider(TlalStartupCategory* cats,
                                           uint32_t* count,
                                           uint32_t limit,
                                           const TlalHotRuntimeIndex* ix,
                                           uint32_t record_index,
                                           uint32_t per_category) {
    uint32_t i;
    uint32_t weakest = UINT32_MAX;
    uint64_t weakest_key = UINT64_MAX;
    const TlalHotRecord* r;
    if (!cats || !count || !ix || record_index >= ix->record_count || limit == 0u) return;
    r = &ix->records[record_index];
    if (r->temporal_key == 0ull) return;
    for (i = 0u; i < *count; ++i) {
        if (tlal_startup_category_same(&cats[i], r)) {
            tlal_startup_category_insert_record(&cats[i], ix, record_index, per_category);
            return;
        }
    }
    if (*count < limit) {
        TlalStartupCategory* c = &cats[(*count)++];
        memset(c, 0, sizeof(*c));
        c->core_group = r->core_group;
        c->type = r->type;
        c->schema = r->schema;
        c->layer_hash = r->layer_hash;
        tlal_startup_category_insert_record(c, ix, record_index, per_category);
        return;
    }
    for (i = 0u; i < *count; ++i) {
        if (cats[i].temporal_key < weakest_key) { weakest_key = cats[i].temporal_key; weakest = i; }
    }
    if (weakest != UINT32_MAX && r->temporal_key > cats[weakest].temporal_key) {
        TlalStartupCategory* c = &cats[weakest];
        memset(c, 0, sizeof(*c));
        c->core_group = r->core_group;
        c->type = r->type;
        c->schema = r->schema;
        c->layer_hash = r->layer_hash;
        tlal_startup_category_insert_record(c, ix, record_index, per_category);
    }
}

static int tlal_startup_category_cmp_desc(const void* a, const void* b) {
    const TlalStartupCategory* ca = (const TlalStartupCategory*)a;
    const TlalStartupCategory* cb = (const TlalStartupCategory*)b;
    if (ca->core_group != cb->core_group) return ca->core_group < cb->core_group ? -1 : 1;
    if (ca->type != cb->type) return ca->type < cb->type ? -1 : 1;
    if (ca->temporal_key != cb->temporal_key) return ca->temporal_key > cb->temporal_key ? -1 : 1;
    if (ca->schema != cb->schema) return ca->schema < cb->schema ? -1 : 1;
    if (ca->layer_hash != cb->layer_hash) return ca->layer_hash < cb->layer_hash ? -1 : 1;
    return 0;
}

static void tlal_prewarm_latest_categories(TlalHotState* st) {
    TlalStartupCategory cats[TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX];
    TlalStartupWeekGate weeks[TLAL_HOT_STARTUP_WEEK_LIMIT_MAX];
    uint32_t count = 0u;
    uint32_t week_count = 0u;
    uint32_t limit, per_category, i, j;
    uint64_t bytes, expected;
    uint64_t selected_week = 0ull;
    uint32_t selected_week_index = UINT32_MAX;
    uint32_t selected_covered = 0u;
    uint32_t required_covered = 0u;
    TlalHotCandidate selected_any;
    TlalHotCandidate selected_epi;
    TlalHotCandidate selected_met;
    TlalHotCandidate selected_con;
    memset(&selected_any, 0, sizeof(selected_any));
    memset(&selected_epi, 0, sizeof(selected_epi));
    memset(&selected_met, 0, sizeof(selected_met));
    memset(&selected_con, 0, sizeof(selected_con));
    if (!st || st->index.record_count == 0u) return;
    limit = st->cfg.startup_gate_category_limit;
    if (limit == 0u) limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT;
    if (limit > TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX) limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX;
    per_category = st->cfg.startup_gate_records_per_core;
    if (per_category == 0u) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT;
    if (per_category > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    memset(cats, 0, sizeof(cats));
    memset(weeks, 0, sizeof(weeks));
    for (i = 0u; i < TLAL_HOT_STARTUP_WEEK_LIMIT_MAX; ++i) {
        for (j = 0u; j < TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX; ++j) weeks[i].record_by_category[j] = UINT32_MAX;
    }

    for (i = 0u; i < st->index.record_count; ++i) {
        const TlalHotRecord* r = &st->index.records[i];
        if (r->core_group != TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY &&
            r->core_group != TLALPOWA_HOTDATA_CORE_METEOROLOGY &&
            r->core_group != TLALPOWA_HOTDATA_CORE_CONTAMINANT) continue;
        tlal_startup_category_consider(cats, &count, limit, &st->index, i, per_category);
    }
    if (count > 1u) qsort(cats, count, sizeof(cats[0]), tlal_startup_category_cmp_desc);
    if (count == 0u) return;
    required_covered = (count * 3u + 3u) / 4u;
    if (required_covered == 0u) required_covered = 1u;

    for (i = 0u; i < st->index.record_count; ++i) {
        const TlalHotRecord* r = &st->index.records[i];
        uint32_t cat_ix, wk_ix;
        uint64_t wk;
        if (r->core_group != TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY &&
            r->core_group != TLALPOWA_HOTDATA_CORE_METEOROLOGY &&
            r->core_group != TLALPOWA_HOTDATA_CORE_CONTAMINANT) continue;
        cat_ix = tlal_startup_find_category_index(cats, count, r);
        if (cat_ix == UINT32_MAX) continue;
        wk = tlal_record_week_bucket(r);
        if (wk == 0ull) continue;
        wk_ix = tlal_startup_find_week_index(weeks, &week_count, wk);
        if (wk_ix == UINT32_MAX) continue;
        if (weeks[wk_ix].record_by_category[cat_ix] == UINT32_MAX) weeks[wk_ix].covered_categories += 1u;
        if (tlal_startup_record_is_better_for_week(&st->index, weeks[wk_ix].record_by_category[cat_ix], i)) {
            weeks[wk_ix].record_by_category[cat_ix] = i;
        }
    }

    for (i = 0u; i < week_count; ++i) {
        if (weeks[i].covered_categories >= required_covered && weeks[i].week_bucket >= selected_week) {
            selected_week = weeks[i].week_bucket;
            selected_week_index = i;
            selected_covered = weeks[i].covered_categories;
        }
    }
    if (selected_week_index == UINT32_MAX) {
        for (i = 0u; i < week_count; ++i) {
            if (weeks[i].covered_categories > selected_covered ||
                (weeks[i].covered_categories == selected_covered && weeks[i].week_bucket > selected_week)) {
                selected_week = weeks[i].week_bucket;
                selected_week_index = i;
                selected_covered = weeks[i].covered_categories;
            }
        }
    }

    if (selected_week_index != UINT32_MAX) {
        for (i = 0u; i < count; ++i) {
            cats[i].record_count = 0u;
            for (j = 0u; j < TLAL_HOT_STARTUP_GATE_RECORDS_MAX; ++j) cats[i].record_indices[j] = UINT32_MAX;
        }
        for (i = 0u; i < count; ++i) {
            const uint32_t ri = weeks[selected_week_index].record_by_category[i];
            if (ri != UINT32_MAX && ri < st->index.record_count) {
                cats[i].record_indices[0] = ri;
                cats[i].record_count = 1u;
            }
        }
    }

    expected = 0ull;
    for (i = 0u; i < count; ++i) expected += (uint64_t)cats[i].record_count;
    if (st->stats) {
        st->stats->startup_gate_expected_hits = expected;
        st->stats->startup_gate_selected_week_bucket = selected_week;
        st->stats->startup_gate_selected_coverage_percent = count ? ((uint64_t)selected_covered * 100ull) / (uint64_t)count : 0ull;
    }
    bytes = st->cfg.startup_gate_bytes_per_record;
    if (bytes == 0ull) bytes = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    for (i = 0u; i < count && st->budget_left != 0ull; ++i) {
        for (j = 0u; j < cats[i].record_count && j < per_category && st->budget_left != 0ull; ++j) {
            const uint32_t ri = cats[i].record_indices[j];
            const TlalHotRecord* r;
            uint64_t got;
            if (ri >= st->index.record_count) continue;
            r = &st->index.records[ri];
            got = tlal_cache_load_record(st, r, bytes);
            if (got != 0ull && st->stats) {
                st->stats->startup_gate_hits += 1ull;
                st->stats->startup_gate_bytes += got;
                st->stats->prepared_hits += 1ull;
                st->stats->prepared_bytes += got;
            }
            tlal_consider_candidate(&selected_any, r, ri);
            if (r->core_group == TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY) tlal_consider_candidate(&selected_epi, r, ri);
            else if (r->core_group == TLALPOWA_HOTDATA_CORE_METEOROLOGY) tlal_consider_candidate(&selected_met, r, ri);
            else if (r->core_group == TLALPOWA_HOTDATA_CORE_CONTAMINANT) tlal_consider_candidate(&selected_con, r, ri);
        }
    }
    if (selected_any.temporal_key != 0ull) st->latest_any = selected_any;
    if (selected_epi.temporal_key != 0ull) st->latest_epi = selected_epi;
    if (selected_met.temporal_key != 0ull) st->latest_met = selected_met;
    if (selected_con.temporal_key != 0ull) st->latest_con = selected_con;
    if (st->stats) {
        st->stats->startup_gate_categories = count;
        st->stats->progressive_records_touched += st->stats->startup_gate_hits;
        st->stats->cache_bytes = st->index.cache_bytes;
        st->stats->latest_contaminant_key = st->latest_con.temporal_key;
        st->stats->latest_meteorology_key = st->latest_met.temporal_key;
        st->stats->latest_epidemiology_key = st->latest_epi.temporal_key;
        st->stats->latest_atmosphere_key = st->latest_con.temporal_key ? st->latest_con.temporal_key : st->latest_met.temporal_key;
        st->stats->latest_temporal_key = st->latest_any.temporal_key;
    }
}

static void tlal_startup_gate_core(TlalHotState* st, uint32_t core_group, uint64_t temporal_key) {
    uint32_t indices[TLAL_HOT_STARTUP_GATE_RECORDS_MAX];
    uint32_t cap, count, i;
    uint64_t exact_hits = 0ull;
    uint64_t bytes;
    if (!st || temporal_key == 0ull || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    cap = st->cfg.startup_gate_records_per_core;
    if (cap == 0u) return;
    if (cap > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) cap = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    bytes = st->cfg.startup_gate_bytes_per_record;
    if (bytes == 0ull) bytes = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    count = tlal_collect_record_indices_near(&st->index, core_group, temporal_key, cap, indices, &exact_hits);
    if (st->stats) {
        st->stats->binary_searches += 1ull;
        st->stats->startup_gate_exact_hits += exact_hits;
        st->stats->startup_gate_expected_hits += count;
        st->stats->progressive_records_touched += count;
    }
    for (i = 0u; i < count && st->budget_left != 0ull; ++i) {
        const TlalHotRecord* r = &st->index.records[indices[i]];
        uint64_t got = tlal_cache_load_record(st, r, bytes);
        if (got != 0ull && st->stats) {
            st->stats->startup_gate_hits += 1ull;
            st->stats->startup_gate_bytes += got;
            st->stats->prepared_hits += 1ull;
            st->stats->prepared_bytes += got;
        }
    }
}

static void tlal_prewarm_startup_gate(TlalHotState* st) {
    uint64_t before;
    if (!st || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    before = st->stats ? st->stats->startup_gate_hits : 0ull;
    tlal_prewarm_latest_categories(st);
    if (st->stats && st->stats->startup_gate_hits > before) { st->stats->cache_bytes = st->index.cache_bytes; return; }
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_CONTAMINANT, st->latest_con.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_METEOROLOGY, st->latest_met.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY, st->latest_epi.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_OTHER, st->latest_oth.temporal_key);
    if (st->stats) st->stats->cache_bytes = st->index.cache_bytes;
}

static void tlal_touch_3d_file(TlalHotState* st, const char* path) {
    FILE* f;
    uint64_t size, head, tail;
    if (!st || !path || !st->cfg.enable_3d_touch || st->budget_left == 0ull) return;
    f = fopen(path, "rb");
    if (!f) return;
    size = tlal_file_size_stream(f);
    fclose(f);
    if (size == 0ull) return;
    head = 2ull * 1024ull * 1024ull;
    tail = 2ull * 1024ull * 1024ull;
    tlal_touch_file_span(st, path, 0ull, head);
    if (size > tail) tlal_touch_file_span(st, path, size - tail, tail);
    if (st->stats) st->stats->latest_3d_bytes = size;
}

TLAL_FORCE_INLINE int tlal_is_dot_dir(const char* s) {
    return !s || (s[0] == '.' && (s[1] == '\0' || (s[1] == '.' && s[2] == '\0')));
}

#ifdef _WIN32
static void tlal_scan_dir(TlalHotState* st, const char* root, uint32_t depth) {
    char pattern[TLALPOWA_HOT_PATH_MAX];
    intptr_t h;
    struct _finddatai64_t fd;
    if (!st || !root || depth > st->cfg.max_depth) return;
    if (!tlal_join_path(pattern, sizeof(pattern), root, "*")) return;
    h = _findfirsti64(pattern, &fd);
    if (h == -1) return;
    do {
        char path[TLALPOWA_HOT_PATH_MAX];
        if (tlal_is_dot_dir(fd.name)) continue;
        if (!tlal_join_path(path, sizeof(path), root, fd.name)) continue;
        if (fd.attrib & _A_SUBDIR) {
            tlal_scan_dir(st, path, depth + 1u);
        } else {
            if (st->stats) st->stats->files_seen += 1ull;
            if (tlal_has_suffix_ascii(path, ".ixiptlah")) {
                if (st->cfg.max_ixiptlah_files && st->ix_seen_limit >= st->cfg.max_ixiptlah_files) continue;
                ++st->ix_seen_limit;
                if (st->stats) st->stats->ixiptlah_files += 1ull;
                if (!tlal_parse_ixiptlah_directory_mapped(st, path) && st->stats) st->stats->failed_files += 1ull;
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d") || tlal_has_suffix_ascii(path, ".ixiptlah3d")) {
                tlal_touch_3d_file(st, path);
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d.json") || tlal_has_suffix_ascii(path, ".ixiptlah3d.json")) {
                tlal_touch_file_span(st, path, 0ull, 512ull * 1024ull);
            }
        }
    } while (_findnexti64(h, &fd) == 0);
    _findclose(h);
}
#else
static void tlal_scan_dir(TlalHotState* st, const char* root, uint32_t depth) {
    DIR* d;
    struct dirent* de;
    if (!st || !root || depth > st->cfg.max_depth) return;
    d = opendir(root);
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        char path[TLALPOWA_HOT_PATH_MAX];
        struct stat sb;
        if (tlal_is_dot_dir(de->d_name)) continue;
        if (!tlal_join_path(path, sizeof(path), root, de->d_name)) continue;
        if (stat(path, &sb) != 0) continue;
        if (S_ISDIR(sb.st_mode)) {
            tlal_scan_dir(st, path, depth + 1u);
        } else if (S_ISREG(sb.st_mode)) {
            if (st->stats) st->stats->files_seen += 1ull;
            if (tlal_has_suffix_ascii(path, ".ixiptlah")) {
                if (st->cfg.max_ixiptlah_files && st->ix_seen_limit >= st->cfg.max_ixiptlah_files) continue;
                ++st->ix_seen_limit;
                if (st->stats) st->stats->ixiptlah_files += 1ull;
                if (!tlal_parse_ixiptlah_directory_mapped(st, path) && st->stats) st->stats->failed_files += 1ull;
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d") || tlal_has_suffix_ascii(path, ".ixiptlah3d")) {
                tlal_touch_3d_file(st, path);
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d.json") || tlal_has_suffix_ascii(path, ".ixiptlah3d.json")) {
                tlal_touch_file_span(st, path, 0ull, 512ull * 1024ull);
            }
        }
    }
    closedir(d);
}
#endif

static void tlal_config_normalize(TlalpowaHotDataConfig* c) {
    if (!c) return;
    if (c->max_total_touch_bytes == 0ull) c->max_total_touch_bytes = TLAL_HOT_DEFAULT_BUDGET;
    if (c->max_total_touch_bytes > TLAL_HOT_MAX_BUDGET) c->max_total_touch_bytes = TLAL_HOT_MAX_BUDGET;
    if (c->max_payload_bytes_per_record == 0u) c->max_payload_bytes_per_record = 2u * 1024u * 1024u;
    if (c->max_payload_bytes_per_record > 32u * 1024u * 1024u) c->max_payload_bytes_per_record = 32u * 1024u * 1024u;
    if (c->max_depth == 0u) c->max_depth = 1u;
    if (c->probe_bytes_per_record > 64u * 1024u) c->probe_bytes_per_record = 64u * 1024u;
    if (c->progressive_neighbor_records > 256u) c->progressive_neighbor_records = 256u;
    if (c->neighbor_bytes_per_record == 0u) c->neighbor_bytes_per_record = 256u * 1024u;
    if (c->neighbor_bytes_per_record > 4u * 1024u * 1024u) c->neighbor_bytes_per_record = 4u * 1024u * 1024u;
    if (c->max_runtime_cache_bytes == 0ull) c->max_runtime_cache_bytes = TLAL_HOT_DEFAULT_CACHE_BYTES;
    if (c->max_runtime_cache_bytes > TLAL_HOT_MAX_CACHE_BYTES) c->max_runtime_cache_bytes = TLAL_HOT_MAX_CACHE_BYTES;
    if (c->runtime_cache_lines == 0u) c->runtime_cache_lines = TLAL_HOT_CACHE_LINES_DEFAULT;
    if (c->runtime_cache_lines > TLAL_HOT_CACHE_LINES_MAX) c->runtime_cache_lines = TLAL_HOT_CACHE_LINES_MAX;
    if (c->startup_gate_records_per_core == 0u) c->startup_gate_records_per_core = TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT;
    if (c->startup_gate_records_per_core > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) c->startup_gate_records_per_core = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    if (c->startup_gate_bytes_per_record == 0u) c->startup_gate_bytes_per_record = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    if (c->startup_gate_bytes_per_record > TLAL_HOT_STARTUP_GATE_BYTES_MAX) c->startup_gate_bytes_per_record = TLAL_HOT_STARTUP_GATE_BYTES_MAX;
    if (c->startup_gate_category_limit == 0u) c->startup_gate_category_limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT;
    if (c->startup_gate_category_limit > TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX) c->startup_gate_category_limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX;
}

TlalpowaHotDataConfig tlalpowa_hotdata_default_config(void) {
    static const TlalpowaHotDataConfig defaults = {
        TLAL_HOT_DEFAULT_BUDGET,
        6u,
        4096u,
        2u * 1024u * 1024u,
        0u,
        0u,
        0u,
        256u * 1024u,
        1u,
        TLAL_HOT_DEFAULT_CACHE_BYTES,
        TLAL_HOT_CACHE_LINES_DEFAULT,
        TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT,
        TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT,
        TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT
    };
    return defaults;
}

int tlalpowa_hotdata_prewarm_root(const char* root_utf8,
                                  const TlalpowaHotDataConfig* config,
                                  TlalpowaHotDataStats* stats) {
    TlalHotState st;
    int ok = 1;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (!root_utf8 || !*root_utf8) return 0;
    memset(&st, 0, sizeof(st));
    st.cfg = config ? *config : tlalpowa_hotdata_default_config();
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index.cache_limit_bytes = st.cfg.max_runtime_cache_bytes;
    st.index.cache_line_count = st.cfg.runtime_cache_lines;
    st.index.mapped_file_limit_bytes = st.cfg.max_runtime_cache_bytes >= TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES / 4ull ?
        st.cfg.max_runtime_cache_bytes * 4ull : TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES;
    if (st.index.mapped_file_limit_bytes > TLAL_HOT_MAX_RETAINED_MAP_BYTES)
        st.index.mapped_file_limit_bytes = TLAL_HOT_MAX_RETAINED_MAP_BYTES;
    st.touch_buffer = (unsigned char*)malloc(TLAL_TOUCH_BLOCK_BYTES);
    if (!st.touch_buffer) return 0;
    tlal_scan_dir(&st, root_utf8, 0u);
    (void)tlal_runtime_build_temporal_order(&st.index);
    /*
    REGLA DE BIENVENIDA DE PRIMER PLANO:
    se ignora la fecha civil actual y se toman los ultimos registros IXIPTLAH
    disponibles por categoria fisica. La pantalla puede permanecer mas tiempo
    para asegurar ese primer plano real, pero NO espera vecinos cronologicos;
    ellos se cargan despues, en segundo plano, del mas cercano al mas lejano.
    */
    tlal_prewarm_latest_candidates(&st);
    tlal_prewarm_startup_gate(&st);
    if (stats) stats->retained_mapped_file_bytes = st.index.mapped_file_bytes;
    if (st.cfg.keep_runtime_index) {
        if (stats) stats->cache_bytes = st.index.cache_bytes;
        tlal_hot_lock();
        tlal_runtime_index_free(&g_tlal_hot_index);
        g_tlal_hot_index = st.index;
        memset(&st.index, 0, sizeof(st.index));
        tlal_hot_unlock();
    } else {
        tlal_runtime_index_free(&st.index);
        if (stats) stats->cache_bytes = 0ull;
    }
    free(st.touch_buffer);
    if (stats && stats->failed_files > 0ull && stats->ixiptlah_directories == 0ull) ok = 0;
    return ok;
}

int tlalpowa_hotdata_prefetch_temporal(uint32_t core_group,
                                       uint64_t temporal_key,
                                       uint32_t neighbor_records,
                                       uint32_t bytes_per_record,
                                       TlalpowaHotDataStats* stats) {
    TlalHotState st;
    uint32_t want;
    uint64_t bytes;
    int ok;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (temporal_key == 0ull) return 0;
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0; }
    memset(&st, 0, sizeof(st));
    st.cfg = tlalpowa_hotdata_default_config();
    st.cfg.max_total_touch_bytes = 32ull * 1024ull * 1024ull;
    st.cfg.neighbor_bytes_per_record = bytes_per_record ? bytes_per_record : 256u * 1024u;
    st.cfg.progressive_neighbor_records = neighbor_records ? neighbor_records : 12u;
    st.cfg.max_runtime_cache_bytes = g_tlal_hot_index.cache_limit_bytes ? g_tlal_hot_index.cache_limit_bytes : TLAL_HOT_DEFAULT_CACHE_BYTES;
    st.cfg.runtime_cache_lines = g_tlal_hot_index.cache_line_count ? g_tlal_hot_index.cache_line_count : TLAL_HOT_CACHE_LINES_DEFAULT;
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index = g_tlal_hot_index;
    if (!st.index.temporal_order || st.index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st.index);
    st.touch_buffer = (unsigned char*)malloc(TLAL_TOUCH_BLOCK_BYTES);
    if (!st.touch_buffer) { tlal_hot_unlock(); return 0; }
    want = st.cfg.progressive_neighbor_records ? st.cfg.progressive_neighbor_records : 1u;
    bytes = st.cfg.neighbor_bytes_per_record ? (uint64_t)st.cfg.neighbor_bytes_per_record : (256ull * 1024ull);
    tlal_prewarm_temporal_near(&st, core_group, temporal_key, want, bytes);
    if (stats) stats->cache_bytes = st.index.cache_bytes;
    g_tlal_hot_index = st.index;
    memset(&st.index, 0, sizeof(st.index));
    free(st.touch_buffer);
    ok = stats ? (stats->progressive_records_touched > 0ull) : 1;
    tlal_hot_unlock();
    return ok;
}

int tlalpowa_hotdata_find_nearest(uint32_t core_group,
                                  uint64_t temporal_key,
                                  TlalpowaHotDataHit* hit) {
    uint32_t record_index;
    int ok = 0;
    if (hit) memset(hit, 0, sizeof(*hit));
    if (!hit || temporal_key == 0ull) return 0;
    tlal_hot_lock();
    if (g_tlal_hot_index.records && g_tlal_hot_index.record_count != 0u &&
        g_tlal_hot_index.temporal_order && g_tlal_hot_index.temporal_order_count != 0u &&
        tlal_runtime_find_nearest_record_index(&g_tlal_hot_index, core_group, temporal_key, &record_index)) {
        tlal_hit_from_record(&g_tlal_hot_index, &g_tlal_hot_index.records[record_index], hit);
        ok = 1;
    }
    tlal_hot_unlock();
    return ok;
}

uint32_t tlalpowa_hotdata_collect_window(uint32_t core_group,
                                         uint64_t temporal_key,
                                         uint32_t max_hits,
                                         TlalpowaHotDataHit* hits) {
    uint32_t stack_indices[256];
    uint32_t local_indices_small[32];
    uint32_t* indices;
    uint32_t count = 0u, i, cap;
    uint64_t exact_hits = 0ull;
    if (!hits || max_hits == 0u || temporal_key == 0ull) return 0u;
    cap = max_hits;
    if (cap > 256u) cap = 256u;
    memset(hits, 0, (size_t)cap * sizeof(*hits));
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0u; }
    indices = cap <= 32u ? local_indices_small : stack_indices;
    count = tlal_collect_record_indices_near(&g_tlal_hot_index, core_group, temporal_key, cap, indices, &exact_hits);
    for (i = 0u; i < count; ++i) tlal_hit_from_record(&g_tlal_hot_index, &g_tlal_hot_index.records[indices[i]], &hits[i]);
    (void)exact_hits;
    tlal_hot_unlock();
    return count;
}

uint64_t tlalpowa_hotdata_read_hit(const TlalpowaHotDataHit* hit,
                                   void* out_buffer,
                                   uint64_t out_capacity,
                                   uint64_t payload_relative_offset) {
    uint64_t remain, absolute_offset, got = 0ull;
    uint32_t i, limit;
    if (!hit || !out_buffer || out_capacity == 0ull || !hit->path[0]) return 0ull;
    if (payload_relative_offset >= hit->stored_size) return 0ull;
    if (hit->payload_offset > UINT64_MAX - payload_relative_offset) return 0ull;
    remain = hit->stored_size - payload_relative_offset;
    if (remain > out_capacity) remain = out_capacity;
    if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
    absolute_offset = hit->payload_offset + payload_relative_offset;
    tlal_hot_lock();
    limit = tlal_runtime_cache_lines(&g_tlal_hot_index);
    for (i = 0u; i < limit; ++i) {
        TlalHotCacheLine* ln = &g_tlal_hot_index.cache[i];
        if (ln->data && ln->file_index == hit->file_index && ln->payload_offset == hit->payload_offset &&
            ln->temporal_key == hit->temporal_key && ln->layer_hash == hit->layer_hash &&
            payload_relative_offset < ln->bytes) {
            uint64_t cached = ln->bytes - payload_relative_offset;
            if (cached > remain) cached = remain;
            memcpy(out_buffer, ln->data + payload_relative_offset, (size_t)cached);
            ln->tick = ++g_tlal_hot_index.cache_tick;
            tlal_hot_unlock();
            return cached;
        }
    }
    if (hit->file_index < g_tlal_hot_index.file_count) {
        got = tlal_read_hot_file_span(&g_tlal_hot_index.files[hit->file_index], absolute_offset, out_buffer, remain);
        if (got != 0ull) { tlal_hot_unlock(); return got; }
    }
    tlal_hot_unlock();
    return tlal_read_path_span(hit->path, absolute_offset, out_buffer, remain);
}

uint32_t tlalpowa_hotdata_prepare_active_temporal_view(uint32_t core_group,
                                                       uint64_t temporal_key,
                                                       uint32_t active_hits,
                                                       uint32_t active_bytes_per_hit,
                                                       uint32_t neighbor_hits,
                                                       uint32_t neighbor_bytes_per_hit,
                                                       TlalpowaHotDataHit* hits,
                                                       TlalpowaHotDataStats* stats) {
    TlalHotState st;
    uint32_t indices[256];
    uint32_t cap, count = 0u, i;
    uint64_t exact_hits = 0ull;
    uint64_t active_bytes, neighbor_bytes, max_bytes;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (hits) memset(hits, 0, (size_t)((active_hits + neighbor_hits) > 256u ? 256u : (active_hits + neighbor_hits)) * sizeof(*hits));
    if (temporal_key == 0ull) return 0u;
    if (active_hits == 0u) active_hits = 1u;
    cap = active_hits + neighbor_hits;
    if (cap == 0u) return 0u;
    if (cap > 256u) cap = 256u;
    if (active_hits > cap) active_hits = cap;
    active_bytes = active_bytes_per_hit ? (uint64_t)active_bytes_per_hit : 96ull * 1024ull;
    neighbor_bytes = neighbor_bytes_per_hit ? (uint64_t)neighbor_bytes_per_hit : 64ull * 1024ull;
    if (active_bytes > 4ull * 1024ull * 1024ull) active_bytes = 4ull * 1024ull * 1024ull;
    if (neighbor_bytes > 4ull * 1024ull * 1024ull) neighbor_bytes = 4ull * 1024ull * 1024ull;
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0u; }
    count = tlal_collect_record_indices_near(&g_tlal_hot_index, core_group, temporal_key, cap, indices, &exact_hits);
    if (count == 0u) { tlal_hot_unlock(); return 0u; }
    memset(&st, 0, sizeof(st));
    st.cfg = tlalpowa_hotdata_default_config();
    max_bytes = active_bytes > neighbor_bytes ? active_bytes : neighbor_bytes;
    st.cfg.max_total_touch_bytes = active_bytes * (uint64_t)(active_hits < count ? active_hits : count);
    if (count > active_hits) st.cfg.max_total_touch_bytes += neighbor_bytes * (uint64_t)(count - active_hits);
    if (st.cfg.max_total_touch_bytes > 64ull * 1024ull * 1024ull) st.cfg.max_total_touch_bytes = 64ull * 1024ull * 1024ull;
    st.cfg.max_payload_bytes_per_record = (uint32_t)(max_bytes > 32ull * 1024ull * 1024ull ? 32ull * 1024ull * 1024ull : max_bytes);
    st.cfg.max_runtime_cache_bytes = g_tlal_hot_index.cache_limit_bytes ? g_tlal_hot_index.cache_limit_bytes : TLAL_HOT_DEFAULT_CACHE_BYTES;
    st.cfg.runtime_cache_lines = g_tlal_hot_index.cache_line_count ? g_tlal_hot_index.cache_line_count : TLAL_HOT_CACHE_LINES_DEFAULT;
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index = g_tlal_hot_index;
    for (i = 0u; i < count; ++i) {
        const TlalHotRecord* r = &st.index.records[indices[i]];
        const uint64_t bytes = i < active_hits ? active_bytes : neighbor_bytes;
        uint64_t got = tlal_cache_load_record(&st, r, bytes);
        if (hits) tlal_hit_from_record(&st.index, r, &hits[i]);
        if (stats && got != 0ull) {
            stats->prepared_hits += 1ull;
            stats->prepared_bytes += got;
        }
    }
    if (stats) {
        stats->exact_window_hits = exact_hits;
        stats->binary_searches += 1ull;
        stats->cache_bytes = st.index.cache_bytes;
        stats->progressive_records_touched += count;
    }
    g_tlal_hot_index = st.index;
    memset(&st.index, 0, sizeof(st.index));
    tlal_hot_unlock();
    return count;
}

uint32_t tlalpowa_hotdata_prepare_temporal_view(uint32_t core_group,
                                                uint64_t temporal_key,
                                                uint32_t max_hits,
                                                uint32_t bytes_per_hit,
                                                TlalpowaHotDataHit* hits,
                                                TlalpowaHotDataStats* stats) {
    return tlalpowa_hotdata_prepare_active_temporal_view(core_group,
                                                        temporal_key,
                                                        max_hits,
                                                        bytes_per_hit,
                                                        0u,
                                                        bytes_per_hit,
                                                        hits,
                                                        stats);
}
