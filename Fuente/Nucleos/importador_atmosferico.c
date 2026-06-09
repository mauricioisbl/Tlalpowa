/* Parser y transporte atmosferico C11: RAMA, REDMET/REDMA y RUOA/PEMBU. */
#ifndef _WIN32
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "ruoa_pembu_bridge.h"

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
