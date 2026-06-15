#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#pragma execution_character_set("utf-8")

#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <cwchar>
#include <cstdlib>
#include <windowsx.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <iterator>
#include <deque>
#include <commctrl.h>
#include <strsafe.h>
#include "miausoft_core.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

namespace miausoft_visual {

inline constexpr double phi = MIAUSOFT_PHI;
inline constexpr double inv_phi = MIAUSOFT_PHI_N1;
inline constexpr double window_width_ratio = MIAUSOFT_PHI_N1;
inline constexpr double window_height_ratio = MIAUSOFT_PHI_N1;
inline constexpr double window_min_width_ratio = MIAUSOFT_PHI_N2;
inline constexpr double icon_panel_ratio = MIAUSOFT_PHI_N4;
inline constexpr double golden_padding_ratio = MIAUSOFT_PHI_N14;
inline constexpr double config_panel_width_ratio = 0.500000000000000; // panel de configuración izquierdo, más legible sin ocupar toda la ventana
inline constexpr unsigned progress_bar_phi_power = MIAUSOFT_PROGRESS_BAR_PHI_POWER; // phi^-11: grosor común de progreso de toda la suite.

inline int clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int max_i(int a, int b) { return a > b ? a : b; }
inline int min_i(int a, int b) { return a < b ? a : b; }

inline COLORREF mix_color(COLORREF a, COLORREF b, double t);
inline COLORREF accent_color();
inline COLORREF page_color();
inline COLORREF ink_color();
inline COLORREF muted_color();

inline int golden_pad_x(int client_width) {
    return clamp_i(static_cast<int>(std::llround(static_cast<double>(max_i(1, client_width)) * golden_padding_ratio)), 1, 8);
}

inline int golden_pad_y(int client_height) {
    return clamp_i(static_cast<int>(std::llround(static_cast<double>(max_i(1, client_height)) * golden_padding_ratio)), 3, 12);
}

inline int icon_panel_width(int client_width) {
    return max_i(1, static_cast<int>(std::llround(static_cast<double>(max_i(1, client_width)) * icon_panel_ratio)));
}

inline int lateral_button_max_width(int client_width) {
    const int panel = icon_panel_width(client_width);
    const int pad = max_i(1, golden_pad_x(panel));
    return max_i(42, panel - 2 * pad);
}

inline int preferred_window_width_px() {
    return max_i(1, static_cast<int>(std::llround(static_cast<double>(GetSystemMetrics(SM_CXSCREEN)) * window_width_ratio)));
}

inline int preferred_window_height_px() {
    return max_i(1, static_cast<int>(std::llround(static_cast<double>(GetSystemMetrics(SM_CYSCREEN)) * window_height_ratio)));
}

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

inline void apply_rounded_top_window(HWND hwnd) {
    if (!hwnd) return;
    const int pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}

inline void apply_rounded_child_region(HWND hwnd, int width, int height, int radius = 18) {
    if (!hwnd || width <= 0 || height <= 0) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
    if (rgn) SetWindowRgn(hwnd, rgn, TRUE); // El sistema adopta el HRGN si SetWindowRgn tiene éxito.
}

inline bool icon_panel_point(HWND hwnd, LPARAM lp) {
    if (!hwnd) return false;
    RECT rc{}; GetClientRect(hwnd, &rc);
    const int w = max_i(1, rc.right - rc.left);
    const int h = max_i(1, rc.bottom - rc.top);
    const int x = GET_X_LPARAM(lp);
    const int y = GET_Y_LPARAM(lp);
    return x >= 0 && y >= 0 && x < icon_panel_width(w) && y < h;
}

inline int registry_read_dword(const wchar_t* app_key, const wchar_t* name, int fallback) {
    if (!app_key || !*app_key || !name || !*name) return fallback;
    std::wstring sub = L"Software\\MiausoftSuite\\";
    sub += app_key;
    DWORD type = 0, value = 0, bytes = sizeof(value);
    LONG rc = RegGetValueW(HKEY_CURRENT_USER, sub.c_str(), name, RRF_RT_REG_DWORD, &type, &value, &bytes);
    if (rc != ERROR_SUCCESS) return fallback;
    return static_cast<int>(value);
}

inline void registry_write_dword(const wchar_t* app_key, const wchar_t* name, int value) {
    if (!app_key || !*app_key || !name || !*name) return;
    value = value < 0 ? 0 : value;
    const int old_value = registry_read_dword(app_key, name, -2147483647);
    if (old_value == value) return;
    std::wstring sub = L"Software\\MiausoftSuite\\";
    sub += app_key;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS && key) {
        DWORD v = static_cast<DWORD>(value);
        RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
        RegCloseKey(key);
    }
}

inline int read_min_txt_kb(const wchar_t* app_key, int fallback_kb = 9) {
    return clamp_i(registry_read_dword(app_key, L"NoGuardarTxtMenoresAKB", fallback_kb), 0, 999999);
}

inline void write_min_txt_kb(const wchar_t* app_key, int kb) {
    registry_write_dword(app_key, L"NoGuardarTxtMenoresAKB", clamp_i(kb, 0, 999999));
}


enum class SettingValueKind { Int, Bool, Text, PrioritySlider };

struct SettingFieldDef {
    const wchar_t* name;
    const wchar_t* label;
    SettingValueKind kind;
    int default_int;
    int min_int;
    int max_int;
    const wchar_t* default_text;
    const wchar_t* unit;
    const wchar_t* category;
};

inline std::wstring registry_read_string(const wchar_t* app_key, const wchar_t* name, const std::wstring& fallback) {
    if (!app_key || !*app_key || !name || !*name) return fallback;
    std::wstring sub = L"Software\\MiausoftSuite\\";
    sub += app_key;
    DWORD type = 0, bytes = 0;
    LONG rc = RegGetValueW(HKEY_CURRENT_USER, sub.c_str(), name, RRF_RT_REG_SZ, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || bytes < sizeof(wchar_t)) return fallback;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    rc = RegGetValueW(HKEY_CURRENT_USER, sub.c_str(), name, RRF_RT_REG_SZ, &type, value.data(), &bytes);
    if (rc != ERROR_SUCCESS) return fallback;
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value.empty() ? fallback : value;
}

inline void registry_write_string(const wchar_t* app_key, const wchar_t* name, const std::wstring& value) {
    if (!app_key || !*app_key || !name || !*name) return;
    if (registry_read_string(app_key, name, L"\xFFFF\xFFFE_MIAUSOFT_NO_VALUE_") == value) return;
    std::wstring sub = L"Software\\MiausoftSuite\\";
    sub += app_key;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS && key) {
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), bytes);
        RegCloseKey(key);
    }
}

inline int read_config_int(const wchar_t* app_key, const wchar_t* name, int fallback, int lo = 0, int hi = 999999) {
    return clamp_i(registry_read_dword(app_key, name, fallback), lo, hi);
}

inline void write_config_int(const wchar_t* app_key, const wchar_t* name, int value, int lo = 0, int hi = 999999) {
    registry_write_dword(app_key, name, clamp_i(value, lo, hi));
}

inline std::string settings_wide_to_utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(max_i(0, n)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

inline std::wstring settings_utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(max_i(0, n)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

inline std::vector<SettingFieldDef> settings_fields_for_app(const std::wstring& app_key, bool has_txt_threshold, int default_kb) {
    std::vector<SettingFieldDef> f;
    const wchar_t* cat = L"General";
    auto section = [&](const wchar_t* c) { cat = (c && *c) ? c : L"General"; };
    auto add_int = [&](const wchar_t* name, const wchar_t* label, int defv, int lo, int hi, const wchar_t* unit = L"") {
        f.push_back({name, label, SettingValueKind::Int, defv, lo, hi, L"", unit, cat});
    };
    auto add_bool = [&](const wchar_t* name, const wchar_t* label, int defv) {
        f.push_back({name, label, SettingValueKind::Bool, defv ? 1 : 0, 0, 1, L"", L"", cat});
    };
    auto add_text = [&](const wchar_t* name, const wchar_t* label, const wchar_t* defv) {
        f.push_back({name, label, SettingValueKind::Text, 0, 0, 0, defv ? defv : L"", L"", cat});
    };
    auto add_priority = [&]() {
        f.push_back({L"PrioridadProceso", L"Rendimiento de Windows", SettingValueKind::PrioritySlider, 1, 0, 2, L"", L"↑normal · alta · tiempo real", cat});
    };

    if (app_key == L"Organizador") {
        section(L"OCR");
        add_bool(L"AplicarOCR", L"OCR cuando falte texto", 1);
        add_int (L"OCRModo", L"Modo OCR de entrada", 1, 0, 2, L"0 no · 1 falta · 2 antes");
        add_bool(L"OCRGuardarPDF", L"Persistir capa OCR", 1);
        add_bool(L"OCRBackupPDF", L"Backup pre-OCR", 1);
        add_int (L"OCRPPI", L"PPI OCR / re-OCR", 1200, 150, 1200, L"ppi");
        add_int (L"OCRPageTimeoutSec", L"Timeout OCR/página", 120, 15, 3600, L"s/pág");
        add_int (L"OCRPSM", L"PSM segmentación", 1, 0, 13, L"psm");
        add_text(L"OCRIdiomas", L"Idiomas Tesseract", L"spa+eng");
        add_int (L"OCRJobs", L"OCR paralelo interno", 2, 1, 16, L"jobs");
        add_int (L"OCROptimizar", L"Compresión OCRmyPDF", 1, 0, 3, L"0-3");
        add_bool(L"OCRRotar", L"Corregir orientación", 1);
        add_bool(L"OCRDeskew", L"Enderezar inclinación", 1);
        add_bool(L"OCRLimpiar", L"Prelimpiar imagen", 1);
        add_bool(L"OCRTesseractSinDobleEspacio", L"Colapsar espacios OCR", 1);
        add_bool(L"OCRBlacklistDesde1Miau", L"Blacklist segura 1.miau", 1);
        add_bool(L"OCRMarcarPDFDPI", L"Marca DPI en PDF", 1);
        add_bool(L"OCRLimpiarFinalPDF", L"Limpieza final OCR", 1);
        add_bool(L"OCRRedo", L"Rehacer capa OCR", 1);
        add_int (L"OCRRachaPaginasSinTexto", L"Racha para OCR parcial", 1, 1, 1000, L"págs");
        add_int (L"OCRFragmentosAntesPDFCompleto", L"Escalar a OCR total", 3, 1, 1000, L"rachas");
        add_int (L"OCRForzarPDFCompletoSiRachaMayorPct", L"OCR total si racha >", 5, 1, 50, L"% págs");
        add_bool(L"ReintentarPDFSinTextoConOCR", L"Reintentar sin-texto con OCR", 1);
        section(L"Rendimiento");
        add_priority();
        add_int (L"ArchivosSimultaneos", L"Paralelo sin OCR", 10, 1, 64, L"jobs");
        add_int (L"ArchivosSimultaneosOCR", L"Paralelo con OCR", 5, 1, 16, L"jobs");
        section(L"Diccionario bibliográfico");
        add_int (L"DiagnosticJsonParts", L"Partes JSON diagnóstico", 3, 1, 32, L"partes");
        add_int (L"DiagnosticSampleChars", L"Muestra por libro", 2048, 256, 50000, L"caracteres");
        add_int (L"DiagnosticConcurrency", L"Archivos simultáneos", 7, 1, 32, L"jobs");
        add_int (L"DiagnosticFlushEvery", L"Flush JSON cada", 32, 1, 512, L"registros");
        add_int (L"DiagnosticPdfFastPages", L"Páginas rápidas PDF", 12, 1, 96, L"págs iniciales");
        add_int (L"DiagnosticPdfTimeoutSec", L"Timeout extractor PDF", 45, 5, 600, L"s");
        add_bool(L"DiagnosticMutoolFallback", L"Usar mutool si Poppler falla", 1);
        add_bool(L"DiagnosticPreferTxtHomologo", L"Priorizar TXT homólogo", 1);
        section(L"Texto");
        add_int (L"PaginasDocumentoMayor", L"PDF largo desde", 500, 1, 200000, L"págs");
        add_int (L"PaginaSinTextoChars", L"Página con texto desde", 16, 1, 10000, L"letras");
        add_int (L"TxtGrandeKB", L"TXT grande desde", 9750, 256, 1048576, L"KiB");
        add_int (L"TxtParteMaxKB", L"Parte TXT máxima", 9750, 256, 1048576, L"KiB");
        add_int (L"RachaSinTextoValor", L"Racha sin texto crítica", 7, 1, 100000, L"% o págs");
        add_bool(L"RachaSinTextoEsPorcentaje", L"Racha como porcentaje", 1);
        add_int (L"NoGuardarTxtMenoresAKB", L"Descartar TXT menor", 10, 0, 999999, L"KiB");
        add_int (L"RatioBajoPorMil", L"Extracción pobre si <", 10, 0, 1000, L"‰");
        section(L"Tiempos");
        add_int (L"PDFPageTimeoutSec", L"Timeout extracción/página", 75, 10, 3600, L"s");
        add_int (L"PDFDirectTimeoutMinSec", L"Timeout PDF directo mínimo", 45, 10, 7200, L"s");
        add_int (L"PDFDirectTimeoutMaxSec", L"Timeout PDF directo máximo", 800, 60, 21600, L"s");
        return f;
    }

    if (app_key == L"ConvertidorCompleto") {
        section(L"Rendimiento");
        add_priority();
        add_int (L"ArchivosSimultaneos", L"Archivos en paralelo", 4, 1, 64, L"jobs");
        section(L"PDF");
        add_bool(L"PDFPorPaginas", L"PDF por página", 1);
        add_bool(L"PDFPreferirLayout", L"Conservar layout PDF", 1);
        section(L"OCR");
        add_int (L"OCRSiTxtMenorKB", L"OCR si TXT <", 1, 0, 999999, L"KiB");
        add_int (L"OCRSiPaginaMenorChars", L"Página con texto desde", 16, 1, 10000, L"letras");
        add_text(L"OCRIdiomas", L"Idiomas Tesseract", L"spa+eng");
        add_int (L"OCRPPI", L"PPI OCR / re-OCR", 1200, 150, 1200, L"ppi");
        add_int (L"OCRPageTimeoutSec", L"Timeout OCR/página", 120, 15, 3600, L"s/pág");
        add_int (L"PDFPageTimeoutSec", L"Timeout extracción/página", 75, 10, 3600, L"s");
        add_int (L"OCRPSM", L"PSM segmentación", 1, 0, 13, L"psm");
        add_int (L"OCRJobs", L"Procesos OCR", 2, 1, 16, L"jobs");
        add_bool(L"OCRRotar", L"Rotar OCR", 1);
        add_bool(L"OCRDeskew", L"Enderezar OCR", 1);
        add_bool(L"OCRLimpiar", L"Prelimpiar imagen", 1);
        add_bool(L"OCRTesseractSinDobleEspacio", L"Colapsar espacios OCR", 1);
        add_bool(L"OCRBlacklistDesde1Miau", L"Blacklist segura 1.miau", 1);
        add_bool(L"OCRMarcarPDFDPI", L"Marca DPI en PDF", 1);
        add_bool(L"OCRLimpiarFinalPDF", L"Limpieza final OCR", 1);
        add_int (L"OCRRachaPaginasSinTexto", L"Racha para OCR parcial", 1, 1, 1000, L"págs");
        add_int (L"OCRFragmentosAntesPDFCompleto", L"Escalar a OCR total", 3, 1, 1000, L"rachas");
        add_int (L"OCRPDFCompletoDesdePaginas", L"PDF largo OCR total", 500, 1, 200000, L"págs");
        add_int (L"OCRForzarPDFCompletoSiRachaMayorPct", L"OCR total si racha >", 5, 1, 50, L"% págs");
        section(L"Texto");
        add_int (L"NoGuardarTxtMenoresAKB", L"Descartar TXT menor", default_kb, 0, 999999, L"KiB");
        section(L"Compatibilidad");
        add_bool(L"UsarLibreOffice", L"Usar LibreOffice si hace falta", 1);
        return f;
    }

    if (app_key == L"ConvertidorPorCapitulos") {
        section(L"Rendimiento");
        add_priority();
        section(L"Detección");
        add_int (L"MaxPaginasEscaneoTitulos", L"Máx. páginas para detectar", 0, 0, 200000, L"0=todas");
        add_int (L"LineasPorPaginaParaTitulo", L"Líneas revisadas por página", 18, 1, 200, L"líneas");
        section(L"PDF");
        add_bool(L"PDFPreferirLayout", L"Conservar layout PDF", 1);
        section(L"OCR");
        add_int (L"OCRSiPaginaMenorChars", L"OCR si página <", 16, 1, 10000, L"letras");
        add_text(L"OCRIdiomas", L"Idiomas Tesseract", L"spa+eng");
        add_int (L"OCRPPI", L"PPI OCR / re-OCR", 600, 150, 1200, L"ppi");
        add_bool(L"OCRRotar", L"Rotar OCR", 1);
        add_bool(L"OCRDeskew", L"Enderezar OCR", 1);
        section(L"Texto");
        add_int (L"NoGuardarTxtMenoresAKB", L"Descartar TXT menor", default_kb, 0, 999999, L"KiB");
        return f;
    }

    if (has_txt_threshold) {
        section(L"Texto");
        add_int(L"NoGuardarTxtMenoresAKB", L"No guardar TXT menores a", default_kb, 0, 999999, L"KiB");
    }
    return f;
}

struct SettingsPanelState {
    HWND owner = nullptr;
    HWND panel = nullptr;
    HWND close = nullptr;
    HWND save = nullptr;
    HWND load = nullptr;
    HWND reset = nullptr;
    HWND vscroll = nullptr;
    HWND tooltip = nullptr;
    HFONT font = nullptr;
    HFONT title_font = nullptr;
    HFONT hint_font = nullptr;
    HBRUSH panel_brush = nullptr;
    HBRUSH edit_brush = nullptr;
    std::wstring app_key;
    bool has_txt_threshold = false;
    int default_kb = 9;
    bool internal_update = false;
    int scroll_y = 0;
    int content_h = 0;
    int active_category = 0;
    std::vector<SettingFieldDef> fields;
    std::vector<HWND> labels;
    std::vector<HWND> hints;
    std::vector<HWND> controls;
    std::vector<HWND> units;
    std::vector<std::wstring> categories;
    std::vector<HWND> nav_buttons; // Conservado para compatibilidad interna; ya no se crean botones índice.
    std::deque<std::wstring> tooltip_texts;
    ULONGLONG scroll_badge_until = 0;
    std::wstring scroll_badge_text;
    int last_panel_w = 0;
    int last_panel_h = 0;
    bool layout_busy = false;
};

inline const wchar_t* settings_panel_class_name() { return L"MiausoftGoldenSettingsPanel"; }
inline constexpr int SETTINGS_ID_CLOSE = 7104;
inline constexpr int SETTINGS_ID_SAVE = 7105;
inline constexpr int SETTINGS_ID_LOAD = 7106;
inline constexpr int SETTINGS_ID_RESET = 7107;
inline constexpr int SETTINGS_ID_SCROLLBAR = 7108;
// Rango heredado para destruir/ignorar botones índice de builds anteriores.
// No debe usarse para navegación porque se solapaba con SETTINGS_ID_EDIT_BASE.
inline constexpr int SETTINGS_ID_LEGACY_NAV_BASE = 7120;
inline constexpr int SETTINGS_ID_LEGACY_NAV_END = 7199;
inline constexpr int SETTINGS_ID_EDIT_BASE = 7200;
inline constexpr int SETTINGS_ID_CHECK_BASE = 7400;
inline constexpr int SETTINGS_ID_TRACK_BASE = 7600;
inline constexpr BYTE SETTINGS_PANEL_ALPHA = 255;

inline HFONT settings_make_font(int px, int weight = FW_NORMAL) {
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

inline void settings_init_common_controls() {
    static bool done = false;
    if (done) return;
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    done = true;
}

inline std::wstring settings_field_value(const SettingsPanelState* st, size_t i) {
    if (!st || i >= st->fields.size()) return L"";
    const auto& f = st->fields[i];
    if (f.kind == SettingValueKind::Text) return registry_read_string(st->app_key.c_str(), f.name, f.default_text ? f.default_text : L"");
    int v = registry_read_dword(st->app_key.c_str(), f.name, f.default_int);
    v = clamp_i(v, f.min_int, f.max_int);
    return std::to_wstring(v);
}

inline void settings_write_field_value(const SettingsPanelState* st, size_t i, const std::wstring& raw) {
    if (!st || i >= st->fields.size()) return;
    const auto& f = st->fields[i];
    if (f.kind == SettingValueKind::Text) {
        registry_write_string(st->app_key.c_str(), f.name, raw);
    } else {
        if (raw.empty()) return;
        int v = clamp_i(_wtoi(raw.c_str()), f.min_int, f.max_int);
        registry_write_dword(st->app_key.c_str(), f.name, v);
    }
}

inline bool settings_same_text(const wchar_t* a, const wchar_t* b) {
    if (!a) a = L"";
    if (!b) b = L"";
    return lstrcmpW(a, b) == 0;
}

inline std::vector<std::wstring> settings_categories_from_fields(const std::vector<SettingFieldDef>& fields) {
    std::vector<std::wstring> out;
    for (const auto& f : fields) {
        const wchar_t* c = (f.category && *f.category) ? f.category : L"General";
        bool seen = false;
        for (const auto& x : out) if (x == c) { seen = true; break; }
        if (!seen) out.push_back(c);
    }
    if (out.empty()) out.push_back(L"General");
    return out;
}

inline bool settings_is_priority_field(const SettingFieldDef& f) {
    return f.name && lstrcmpW(f.name, L"PrioridadProceso") == 0;
}

inline const wchar_t* settings_priority_label(int level) {
    switch (clamp_i(level, 0, 2)) {
    case 0: return L"↑ normal";
    case 1: return L"alta";
    case 2: return L"tiempo real";
    default: return L"alta";
    }
}

inline DWORD settings_priority_class_for_level(int level) {
    switch (clamp_i(level, 0, 2)) {
    case 2: return REALTIME_PRIORITY_CLASS;
    case 1: return HIGH_PRIORITY_CLASS;
    default: return ABOVE_NORMAL_PRIORITY_CLASS;
    }
}

inline int settings_thread_priority_for_level(int level) {
    switch (clamp_i(level, 0, 2)) {
    case 2: return THREAD_PRIORITY_TIME_CRITICAL;
    case 1: return THREAD_PRIORITY_HIGHEST;
    default: return THREAD_PRIORITY_ABOVE_NORMAL;
    }
}

inline int settings_priority_level_from_class(DWORD cls) {
    if (cls == REALTIME_PRIORITY_CLASS) return 2;
    if (cls == HIGH_PRIORITY_CLASS) return 1;
    return 0;
}

inline int apply_priority_level_to_process(HANDLE process, HANDLE thread, int wanted_level) {
    int desired = clamp_i(wanted_level, 0, 2);
    int applied = 0;
    if (process) {
        for (int cur = desired; cur >= 0; --cur) {
            if (SetPriorityClass(process, settings_priority_class_for_level(cur))) {
                DWORD got = GetPriorityClass(process);
                applied = settings_priority_level_from_class(got ? got : settings_priority_class_for_level(cur));
                break;
            }
        }
    }
    if (thread) {
        if (!SetThreadPriority(thread, settings_thread_priority_for_level(applied))) {
            SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
        }
    }
    return applied;
}

inline int apply_priority_from_config(const wchar_t* app_key, int fallback_level = 1) {
    const int level = read_config_int(app_key, L"PrioridadProceso", fallback_level, 0, 2);
    return apply_priority_level_to_process(GetCurrentProcess(), GetCurrentThread(), level);
}

inline int apply_priority_to_child_from_config(const wchar_t* app_key, HANDLE process, HANDLE thread, int fallback_level = 1) {
    const int level = read_config_int(app_key, L"PrioridadProceso", fallback_level, 0, 2);
    return apply_priority_level_to_process(process, thread, level);
}

inline std::wstring settings_specific_hint(const SettingFieldDef& f) {
    const wchar_t* n = f.name ? f.name : L"";
    if (settings_same_text(n, L"AplicarOCR")) return L"Activado: intenta OCR cuando la extracción nativa no entrega letras suficientes; desactivado conserva solo texto incrustado o extracción directa.";
    if (settings_same_text(n, L"OCRModo")) return L"Mejor inicio: 1. 0 nunca ejecuta OCR; 1 lo usa solo si falta texto útil; 2 lo aplica antes de extraer para PDFs escaneados o mixtos.";
    if (settings_same_text(n, L"OCRGuardarPDF")) return L"Activado: guarda la capa textual OCR dentro del PDF para acelerar ejecuciones futuras; desactívalo si solo quieres TXT temporal.";
    if (settings_same_text(n, L"OCRBackupPDF")) return L"Activado por seguridad: conserva una copia previa antes de reemplazar un PDF con su versión OCRizada; desactívalo solo si ya tienes respaldo externo y quieres ahorrar espacio.";
    if (settings_same_text(n, L"OCRRachaPaginasSinTexto")) return L"Default: 1 página sin al menos 16 letras reales. Dispara OCR parcial inmediato; si el documento es mayor al umbral de PDF largo y la racha rebasa 5% de páginas, se escala a OCR total para evitar miles de TXT basura de 1 KiB.";
    if (settings_same_text(n, L"OCRFragmentosAntesPDFCompleto")) return L"Default: 3 rachas. Sirve como fusible: después de varios huecos separados deja de reparar fragmentos aislados y OCRiza el PDF entero, que suele ser más seguro en escaneos extensos.";
    if (settings_same_text(n, L"OCRForzarPDFCompletoSiRachaMayorPct")) return L"Default: 5%. En PDFs largos, una racha consecutiva mayor a este porcentaje fuerza OCR total aunque haya marcas previas; protege contra falsos positivos de texto residual, pies de página o páginas de 1 KiB.";
    if (settings_same_text(n, L"OCRPDFCompletoDesdePaginas")) return L"Default: 500 páginas. En el Convertidor, a partir de este tamaño una racha larga de páginas sin texto útil se trata como señal estructural de PDF escaneado y dispara OCR total.";
    if (settings_same_text(n, L"OCRPPI")) return L"Default: 1200 ppi para máxima sensibilidad. Re-OCRiza si el marcador previo fue generado con menos PPI; usa 600 si quieres equilibrio, 1200 para letra diminuta, escaneo borroso o libros con capa textual falsa.";
    if (settings_same_text(n, L"OCRPageTimeoutSec")) return L"Mejor inicio: 120 s por página. El presupuesto total de OCR se calcula por páginas; sube solo para escaneos enormes o equipos lentos.";
    if (settings_same_text(n, L"OCRPSM")) return L"Mejor inicio: PSM 1. Segmenta automáticamente con orientación; prueba 3 para páginas uniformes y 6 solo si cada página es un bloque simple.";
    if (settings_same_text(n, L"OCRIdiomas")) return L"Mejor inicio: spa+eng. Añade idiomas solo si aparecen realmente; cada idioma extra aumenta ambigüedad, memoria y tiempo.";
    if (settings_same_text(n, L"OCRJobs")) return L"Default: 2 jobs internos por OCRmyPDF. Mantén 1-2 para calidad estable; subirlo solo conviene con RAM abundante, SSD rápido y pocos PDFs simultáneos.";
    if (settings_same_text(n, L"OCROptimizar")) return L"Mejor inicio: 1. 0 evita recomprimir; 2-3 reducen más tamaño pero pueden volver lento el OCR y no mejoran reconocimiento.";
    if (settings_same_text(n, L"OCRRotar")) return L"Activado: permite corregir páginas giradas antes de reconocer; útil en escaneos mixtos y casi sin costo conceptual.";
    if (settings_same_text(n, L"OCRDeskew")) return L"Activado: endereza inclinación de escaneo; mejora sensibilidad en texto diagonal, borroso o fotografiado.";
    if (settings_same_text(n, L"OCRLimpiar")) return L"Activado si existe unpaper: limpia ruido de fondo antes de OCR. Se omite automáticamente si la herramienta no está disponible.";
    if (settings_same_text(n, L"OCRTesseractSinDobleEspacio")) return L"Activado: escribe preserve_interword_spaces 0 para que el texto seleccionable no arrastre dobles espacios artificiales.";
    if (settings_same_text(n, L"OCRBlacklistDesde1Miau")) return L"Activado: lee 1.miau y solo bloquea dentro de Tesseract caracteres raros que tus reglas destructivas eliminan de forma inequívoca; no bloquea letras, números, espacios ni puntuación normal, y además trata líneas basura como no-texto para disparar OCR.";
    if (settings_same_text(n, L"OCRMarcarPDFDPI")) return L"Activado: después del OCR intenta escribir una marca visual diminuta en la esquina superior de cada página OCRizada, equivalente a ⟦⟁ DPI N⟧; si Ghostscript falla, conserva el PDF OCR sin romperlo.";
    if (settings_same_text(n, L"OCRLimpiarFinalPDF")) return L"Activado: aplica limpieza final cuando el sistema escala a OCR completo; mejora selección del PDF con costo adicional.";
    if (settings_same_text(n, L"OCRRedo")) return L"Activado: usa --force-ocr cuando el sistema decide OCRizar, para que OCRmyPDF no salte páginas con texto falso; el marcador ⟦⟁ DPI N⟧ solo bloquea trabajo redundante con igual o mayor PPI.";
    if (settings_same_text(n, L"TxtGrandeKB")) return L"Mejor inicio: 9750 KiB. Define cuándo un TXT se parte; baja para mazos/importadores delicados, sube para conservar libros completos.";
    if (settings_same_text(n, L"TxtParteMaxKB")) return L"Mejor inicio: 9750 KiB. Tamaño máximo de cada parte; se aplica después de 1.miau y respeta palabras/UTF-8.";
    if (settings_same_text(n, L"ArchivosSimultaneos")) return L"Mejor inicio: 4-10. Sube en TXT/PDF liviano; baja si el disco, antivirus o caché se saturan.";
    if (settings_same_text(n, L"ArchivosSimultaneosOCR")) return L"Default: 5 PDFs simultáneos con OCR, mientras cada OCR usa 2 jobs internos. Baja este valor si RAM, SSD o antivirus saturan; no lo confundas con OCRJobs.";
    if (settings_same_text(n, L"DiagnosticJsonParts")) return L"Divide el diagnóstico bibliográfico en varios JSON para que ChatGPT pueda procesarlos sin perder libros; default: 3 partes.";
    if (settings_same_text(n, L"DiagnosticSampleChars")) return L"Caracteres extraídos por libro para diagnóstico; default: 2048. Sube solo si necesitas identificar ediciones difíciles.";
    if (settings_same_text(n, L"DiagnosticConcurrency")) return L"Archivos diagnosticados en paralelo por el módulo de diccionario; default: 7.";
    if (settings_same_text(n, L"DiagnosticFlushEvery")) return L"Frecuencia de volcado incremental del JSON para proteger avances si Windows interrumpe el proceso.";
    if (settings_same_text(n, L"PrioridadProceso")) return L"Mejor inicio: alta. Normal conserva respuesta del equipo; tiempo real solo para sesiones dedicadas donde no tocarás Windows.";
    if (settings_same_text(n, L"PaginasDocumentoMayor")) return L"Default: 500 páginas. A partir de aquí, una racha consecutiva mayor a 5% sin letras útiles se considera falla estructural y fuerza OCR total del PDF.";
    if (settings_same_text(n, L"PaginaSinTextoChars")) return L"Default: 16 letras alfabéticas reales. No cuentan números, espacios, signos, separadores, símbolos, URLs, marcas OCR ni líneas basura de 1.miau; si queda por debajo, la página se considera candidata a OCR.";
    if (settings_same_text(n, L"OCRSiPaginaMenorChars")) return L"Default: 16 letras reales. El Convertidor ignora números, signos, espacios, marcadores y basura editorial al decidir si una página ya tiene texto o necesita OCR.";
    if (settings_same_text(n, L"RachaSinTextoValor")) return L"Mejor inicio: 7%. Con porcentaje activado escala según tamaño del PDF; en modo absoluto equivale a número fijo de páginas.";
    if (settings_same_text(n, L"RachaSinTextoEsPorcentaje")) return L"Activado: interpreta la racha crítica como porcentaje del documento; desactivado la trata como número absoluto de páginas.";
    if (settings_same_text(n, L"NoGuardarTxtMenoresAKB")) return L"Default: 10 KiB. Rechaza residuos pequeños después de limpiar; usa 0 solo si quieres conservar salidas mínimas aunque parezcan extracción fallida.";
    if (settings_same_text(n, L"RatioBajoPorMil")) return L"Default: 10‰. Si el TXT final queda demasiado pequeño frente al PDF fuente, se marca como extracción pobre y no se acepta como libro sano.";
    if (settings_same_text(n, L"PDFPageTimeoutSec")) return L"Default: 75 s por página extraída; el OCR mantiene 120 s/página. Sube si tienes PDFs enormes o discos lentos; baja solo si quieres abortar páginas dañadas más rápido.";
    if (settings_same_text(n, L"PDFDirectTimeoutMinSec")) return L"Mejor inicio: 45 s. Tiempo mínimo para extracción directa del PDF completo; protege libros cortos sin eternizar fallas.";
    if (settings_same_text(n, L"PDFDirectTimeoutMaxSec")) return L"Default: 800 s. Tope para extracción directa de libros gigantes; no controla el OCR por página, que usa OCRPageTimeoutSec.";
    if (settings_same_text(n, L"PDFPorPaginas")) return L"Activado: extrae PDF página por página para recuperar avances, detectar huecos y OCRizar solo lo necesario.";
    if (settings_same_text(n, L"PDFPreferirLayout")) return L"Activado: intenta conservar orden visual de columnas y saltos; desactívalo si genera texto demasiado fragmentado.";
    if (settings_same_text(n, L"OCRSiTxtMenorKB")) return L"Mejor inicio: 1 KiB. Si el TXT total queda por debajo, intenta OCR; 0 desactiva este disparador.";
    if (settings_same_text(n, L"DiagnosticSampleChars")) return L"Mejor inicio: 2048. Muestra suficiente para identificar bibliografía sin leer todo el libro.";
    if (settings_same_text(n, L"DiagnosticConcurrency")) return L"Mejor inicio: 7. Sube si el disco responde; baja si el diagnóstico se vuelve errático por I/O.";
    if (settings_same_text(n, L"DiagnosticPdfFastPages")) return L"Mejor inicio: 12 páginas iniciales. Aumenta precisión bibliográfica; también aumenta tiempo por PDF.";
    if (settings_same_text(n, L"DiagnosticPdfTimeoutSec")) return L"Mejor inicio: 45 s. Sube para PDFs pesados; baja para detectar rápidamente archivos problemáticos.";
    if (settings_same_text(n, L"MaxPaginasEscaneoTitulos")) return L"Mejor inicio: 0 = todo. Usa 80-200 si el libro es enorme y los títulos aparecen al inicio.";
    if (settings_same_text(n, L"LineasPorPaginaParaTitulo")) return L"Mejor inicio: 18 líneas. Sube si los títulos aparecen más abajo; baja para PDFs limpios.";
    if (settings_same_text(n, L"DiagnosticJsonParts")) return L"Mejor inicio: 3. Sube si el JSON queda demasiado pesado para revisar o importar.";
    if (settings_same_text(n, L"DiagnosticFlushEvery")) return L"Mejor inicio: 32 registros. Baja para más seguridad ante cierres; sube para menor escritura en disco.";
    return L"";
}

inline std::wstring settings_field_hint(const SettingFieldDef& f) {
    std::wstring hint = settings_specific_hint(f);
    if (!hint.empty()) return hint;
    if (f.kind == SettingValueKind::Bool) return f.default_int ? L"Predeterminado: activado. Desactívalo solo si sabes qué costo quieres evitar." : L"Predeterminado: desactivado. Actívalo solo si necesitas esta conducta.";
    if (f.kind == SettingValueKind::Text) {
        std::wstring d = f.default_text ? f.default_text : L"";
        return L"Predeterminado: " + d + L". Mantén el formato exacto cuando se use como parámetro externo.";
    }
    std::wstring unit = (f.unit && *f.unit) ? (std::wstring(L" ") + f.unit) : L"";
    return L"Rango " + std::to_wstring(f.min_int) + L"-" + std::to_wstring(f.max_int) + unit + L" · predeterminado " + std::to_wstring(f.default_int) + unit + L".";
}

inline std::wstring settings_field_short_hint(const SettingFieldDef& f) {
    const wchar_t* n = f.name ? f.name : L"";
    if (settings_same_text(n, L"OCRRachaPaginasSinTexto")) return L"1 pág sin 16 letras: OCR parcial.";
    if (settings_same_text(n, L"OCRFragmentosAntesPDFCompleto")) return L"3 rachas separadas: OCR total.";
    if (settings_same_text(n, L"OCRForzarPDFCompletoSiRachaMayorPct")) return L">5% seguido en PDF largo: OCR total.";
    if (settings_same_text(n, L"OCRPDFCompletoDesdePaginas")) return L"500 págs: activa regla de racha %.";
    if (settings_same_text(n, L"OCRPPI")) return L"1200 máxima calidad; 600 equilibrio.";
    if (settings_same_text(n, L"OCRPageTimeoutSec")) return L"120 s/pág OCR y Tesseract.";
    if (settings_same_text(n, L"OCRPSM")) return L"PSM 1 auto; 3 página; 6 bloque.";
    if (settings_same_text(n, L"OCRJobs")) return L"2 jobs internos; estable en RAM/I/O.";
    if (settings_same_text(n, L"ArchivosSimultaneos")) return L"10 sin OCR; baja si satura disco.";
    if (settings_same_text(n, L"ArchivosSimultaneosOCR")) return L"5 PDFs OCR; 2 jobs internos c/u.";
    if (settings_same_text(n, L"DiagnosticJsonParts")) return L"JSON diagnóstico particionado.";
    if (settings_same_text(n, L"DiagnosticSampleChars")) return L"2048 chars por libro.";
    if (settings_same_text(n, L"DiagnosticConcurrency")) return L"7 diagnósticos en paralelo.";
    if (settings_same_text(n, L"DiagnosticFlushEvery")) return L"Flush incremental robusto.";
    if (settings_same_text(n, L"PrioridadProceso")) return L"Alta por default; tiempo real solo dedicado.";
    if (settings_same_text(n, L"PaginaSinTextoChars")) return L"16 letras reales; ignora números/signos.";
    if (settings_same_text(n, L"OCRSiPaginaMenorChars")) return L"16 letras reales; ignora ruido.";
    if (settings_same_text(n, L"NoGuardarTxtMenoresAKB")) return L"10 KiB mínimo; 0 conserva residuos.";
    if (settings_same_text(n, L"TxtGrandeKB")) return L"9750 KiB: umbral para partir TXT.";
    if (settings_same_text(n, L"TxtParteMaxKB")) return L"9750 KiB: máximo por parte TXT.";
    if (settings_same_text(n, L"PDFPageTimeoutSec")) return L"75 s extracción/pág; OCR 120 s.";
    if (settings_same_text(n, L"OCRBlacklistDesde1Miau")) return L"Blacklist segura: raros sí, letras no.";
    if (settings_same_text(n, L"OCRMarcarPDFDPI")) return L"Marca DPI mínima en esquina superior.";
    if (settings_same_text(n, L"OCRTesseractSinDobleEspacio")) return L"Evita dobles espacios seleccionables.";
    if (settings_same_text(n, L"OCRIdiomas")) return L"spa+eng; no agregues idiomas sobrantes.";
    if (settings_same_text(n, L"OCRModo")) return L"1 si falta texto; 2 antes de extraer.";
    if (settings_same_text(n, L"OCROptimizar")) return L"1 equilibrio; 2-3 solo comprimen más.";
    if (settings_same_text(n, L"OCRSiTxtMenorKB")) return L"1 KiB dispara OCR total/sidecar.";
    if (settings_same_text(n, L"DiagnosticSampleChars")) return L"2048 caracteres por libro.";
    if (settings_same_text(n, L"DiagnosticConcurrency")) return L"7 archivos simultáneos.";
    if (settings_same_text(n, L"DiagnosticPdfFastPages")) return L"12 páginas iniciales.";
    if (settings_same_text(n, L"DiagnosticPdfTimeoutSec")) return L"45 s por extractor diagnóstico.";
    if (settings_same_text(n, L"MaxPaginasEscaneoTitulos")) return L"0 = todo el documento.";
    if (settings_same_text(n, L"LineasPorPaginaParaTitulo")) return L"18 líneas por página.";
    if (settings_same_text(n, L"DiagnosticJsonParts")) return L"3 partes JSON.";
    if (settings_same_text(n, L"DiagnosticFlushEvery")) return L"32 registros por flush.";
    if (f.kind == SettingValueKind::Bool) return f.default_int ? L"Predeterminado: activo." : L"Predeterminado: pausado.";
    if (f.kind == SettingValueKind::Text) return L"Pasa el cursor para ver formato.";
    std::wstring unit = (f.unit && *f.unit) ? (std::wstring(L" ") + f.unit) : L"";
    return L"Predeterminado: " + std::to_wstring(f.default_int) + unit + L".";
}

inline bool settings_field_in_active_category(const SettingsPanelState* st, size_t i) {
    (void)st;
    (void)i;
    return true;
}


inline std::wstring settings_tooltip_text(const SettingsPanelState* st, size_t i) {
    if (!st || i >= st->fields.size()) return L"";
    const auto& f = st->fields[i];
    std::wstring value = settings_field_value(st, i);
    std::wstring out = f.label ? f.label : L"";
    if (f.unit && *f.unit) out += L" [" + std::wstring(f.unit) + L"]";
    out += L"\n" + settings_field_hint(f);
    if (!value.empty()) out += L"\nValor actual: " + value;
    return out;
}

inline void settings_add_tooltip(SettingsPanelState* st, HWND target, const std::wstring& text) {
    if (!st || !st->tooltip || !target || text.empty()) return;
    st->tooltip_texts.push_back(text);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = st->panel;
    ti.uId = reinterpret_cast<UINT_PTR>(target);
    ti.lpszText = const_cast<LPWSTR>(st->tooltip_texts.back().c_str());
    SendMessageW(st->tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

inline void settings_update_priority_unit(SettingsPanelState* st, size_t i, int level);

inline void settings_refresh_controls(SettingsPanelState* st) {
    if (!st) return;
    st->internal_update = true;
    for (size_t i = 0; i < st->fields.size() && i < st->controls.size(); ++i) {
        HWND c = st->controls[i];
        if (!c) continue;
        const auto& f = st->fields[i];
        if (f.kind == SettingValueKind::Bool) {
            int v = registry_read_dword(st->app_key.c_str(), f.name, f.default_int);
            SendMessageW(c, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTextW(c, v ? L"sí" : L"no");
        } else if (f.kind == SettingValueKind::PrioritySlider) {
            int v = registry_read_dword(st->app_key.c_str(), f.name, f.default_int);
            v = clamp_i(v, f.min_int, f.max_int);
            SendMessageW(c, TBM_SETPOS, TRUE, v);
            settings_update_priority_unit(st, i, v);
        } else {
            SetWindowTextW(c, settings_field_value(st, i).c_str());
        }
    }
    st->internal_update = false;
}

inline void settings_update_scrollbar(HWND panel, SettingsPanelState* st, int visible_h) {
    if (!panel || !st) return;
    visible_h = max_i(1, visible_h);
    const int virtual_h = max_i(st->content_h, visible_h + 1);
    const int max_pos = max_i(0, virtual_h - visible_h);
    st->scroll_y = clamp_i(st->scroll_y, 0, max_pos);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = max_i(1, virtual_h - 1);
    si.nPage = static_cast<UINT>(visible_h);
    si.nPos = st->scroll_y;

    if (st->vscroll && IsWindow(st->vscroll)) {
        SetScrollInfo(st->vscroll, SB_CTL, &si, TRUE);
        SetWindowLongPtrW(st->vscroll, GWL_STYLE, GetWindowLongPtrW(st->vscroll, GWL_STYLE) | WS_VISIBLE);
        ShowWindow(st->vscroll, SW_SHOWNOACTIVATE);
        EnableScrollBar(st->vscroll, SB_CTL, (virtual_h > visible_h + 1) ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH);
        RedrawWindow(st->vscroll, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    } else {
        SetScrollInfo(panel, SB_VERT, &si, TRUE);
        ShowScrollBar(panel, SB_VERT, TRUE);
        EnableScrollBar(panel, SB_VERT, (virtual_h > visible_h + 1) ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH);
    }
}


inline BOOL CALLBACK settings_destroy_legacy_nav_button_proc(HWND child, LPARAM) {
    const int id = GetDlgCtrlID(child);
    wchar_t cls[32]{};
    const int classLen = GetClassNameW(child, cls, static_cast<int>(std::size(cls)));
    if (classLen <= 0 || _wcsicmp(cls, L"Button") != 0) return TRUE;
    if (id >= SETTINGS_ID_LEGACY_NAV_BASE && id <= SETTINGS_ID_LEGACY_NAV_END) {
        DestroyWindow(child);
        return TRUE;
    }
    // Cinturón y tirantes: builds viejos llegaron a crear botones de índice
    // con IDs variables; si el texto coincide con secciones, se elimina sin
    // tocar Exportar/Importar/Reset/cerrar ni checks.
    wchar_t text[96]{};
    GetWindowTextW(child, text, static_cast<int>(std::size(text)));
    if (lstrcmpW(text, L"OCR") == 0 || lstrcmpW(text, L"Rendimiento") == 0 ||
        lstrcmpW(text, L"Texto") == 0 || lstrcmpW(text, L"Tiempos") == 0 ||
        lstrcmpW(text, L"General") == 0) {
        DestroyWindow(child);
    }
    return TRUE;
}

inline void settings_destroy_legacy_nav_buttons(HWND panel) {
    if (panel) EnumChildWindows(panel, settings_destroy_legacy_nav_button_proc, 0);
}

inline std::wstring settings_category_for_content_y(const SettingsPanelState* st, int content_y, int rowH, int headerH) {
    if (!st || st->fields.empty()) return L"";
    int y = 0;
    std::wstring last;
    std::wstring current = L"";
    for (const auto& f : st->fields) {
        std::wstring cat = (f.category && *f.category) ? f.category : L"General";
        if (cat != last) {
            current = cat;
            if (content_y < y + headerH) return current;
            y += headerH;
            last = cat;
        }
        if (content_y < y + rowH) return current;
        y += rowH;
    }
    return current.empty() ? L"General" : current;
}

inline void settings_mark_scroll_badge(SettingsPanelState* st, HWND hwnd, int rowH, int headerH) {
    (void)hwnd; (void)rowH; (void)headerH;
    if (!st) return;
    // Sin etiquetas ni botones de sección: la navegación visual queda en la
    // barra vertical real. No se dibuja píldora flotante de OCR/Rendimiento/etc.
    st->scroll_badge_text.clear();
    st->scroll_badge_until = 0;
}



inline void settings_show_if_needed(HWND hwnd, bool visible) {
    if (!hwnd) return;
    const bool now = IsWindowVisible(hwnd) != FALSE;
    if (now != visible) ShowWindow(hwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

inline int settings_field_row_h(int panel_height) {
    return clamp_i(panel_height / 7, 36, 43);
}

inline int settings_section_header_h(int panel_height) {
    (void)panel_height;
    return 0; // no se dibujan ni reservan etiquetas de sección como botones.
}

inline int settings_nav_width(int panel_width) {
    return clamp_i(static_cast<int>(std::llround(static_cast<double>(max_i(1, panel_width)) * 0.205)), 104, 150);
}

inline int settings_rows_top(int w, int h, int nav_rows) {
    (void)w;
    (void)nav_rows;
    const int top = max_i(8, golden_pad_y(h) * 2);
    const int closeSide = clamp_i(static_cast<int>(std::llround(static_cast<double>(max_i(1, h)) * 0.074)), 20, 27);
    const int actionH = 23;
    return top + closeSide + 8 + actionH + 12;
}


inline void settings_update_priority_unit(SettingsPanelState* st, size_t i, int level) {
    if (!st || i >= st->fields.size() || i >= st->units.size() || !st->units[i]) return;
    if (level < 0) level = read_config_int(st->app_key.c_str(), st->fields[i].name, st->fields[i].default_int, st->fields[i].min_int, st->fields[i].max_int);
    std::wstring txt = settings_priority_label(level);
    SetWindowTextW(st->units[i], txt.c_str());
}

inline void settings_layout(HWND panel) {
    auto* st = reinterpret_cast<SettingsPanelState*>(GetWindowLongPtrW(panel, GWLP_USERDATA));
    if (!st || st->layout_busy) return;
    st->layout_busy = true;
    RECT rc{}; GetClientRect(panel, &rc);
    const int w = max_i(1, rc.right - rc.left);
    const int h = max_i(1, rc.bottom - rc.top);
    const int px = clamp_i(w / 44, 8, 14);
    const int gap = 6;
    const int top = max_i(8, golden_pad_y(h) * 2);
    const int closeSide = clamp_i(static_cast<int>(std::llround(static_cast<double>(h) * 0.074)), 20, 27);
    const int actionH = 23;
    const int actionW = clamp_i((w - px * 2 - closeSide - gap * 5) / 3, 86, 128);
    const int actionTop = top + closeSide + 8;
    const int rowsTop = actionTop + actionH + 12;
    const int visibleH = max_i(1, h - rowsTop - 8);
    const int rowH = settings_field_row_h(h);
    const int headerH = settings_section_header_h(h);
    const int scrollW = max_i(GetSystemMetrics(SM_CXVSCROLL), 17);
    const int scrollbarReserve = scrollW + 6;
    const int contentX = px;
    const int contentW = max_i(180, w - px * 2 - scrollbarReserve);

    std::vector<int> fieldY(st->fields.size(), 0);
    int contentY = 0;
    for (size_t i = 0; i < st->fields.size(); ++i) {
        fieldY[i] = contentY;
        contentY += rowH;
    }
    st->content_h = contentY + 10;
    settings_update_scrollbar(panel, st, visibleH);

    const int unitW = clamp_i(contentW / 5, 68, 118);
    const int editW = clamp_i(contentW / 5, 70, 104);
    const int labelW = max_i(108, contentW - unitW - editW - gap * 3);
    const int sliderW = max_i(126, contentW - labelW - unitW - gap * 3);

    HDWP hdwp = BeginDeferWindowPos(static_cast<int>(st->fields.size() * 4 + 8));
    auto defer_or_move = [&](HWND child, int x, int y, int cw, int ch) {
        if (!child) return;
        if (hdwp) {
            HDWP next = DeferWindowPos(hdwp, child, nullptr, x, y, cw, ch,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            if (next) { hdwp = next; return; }
            EndDeferWindowPos(hdwp); hdwp = nullptr;
        }
        SetWindowPos(child, nullptr, x, y, cw, ch, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    };

    if (st->close) defer_or_move(st->close, w - px - closeSide - scrollbarReserve, top, closeSide, closeSide);
    if (st->vscroll) {
        defer_or_move(st->vscroll, w - scrollW - 1, rowsTop, scrollW, max_i(1, h - rowsTop - 1));
        ShowWindow(st->vscroll, SW_SHOWNOACTIVATE);
    }
    const int ax = px;
    if (st->save)  defer_or_move(st->save,  ax, actionTop, actionW, actionH);
    if (st->load)  defer_or_move(st->load,  ax + actionW + gap, actionTop, actionW, actionH);
    if (st->reset) defer_or_move(st->reset, ax + (actionW + gap) * 2, actionTop, actionW, actionH);


    // Área estrictamente desplazable. Ningún control de filas puede invadir
    // el encabezado fijo donde viven Exportar/Importar/Reset/cerrar.
    // Si una fila queda parcialmente por encima de rowsTop, se oculta completa:
    // así no quedan botones/campos “por capas” debajo de las acciones.
    const int viewportTop = rowsTop;
    const int viewportBottom = max_i(viewportTop + 1, h - 8);
    for (size_t i = 0; i < st->fields.size(); ++i) {
        const int y = rowsTop + fieldY[i] - st->scroll_y;
        const bool visible = (y >= viewportTop) && (y < viewportBottom);
        HWND lab = (i < st->labels.size()) ? st->labels[i] : nullptr;
        HWND hint = (i < st->hints.size()) ? st->hints[i] : nullptr;
        HWND c = (i < st->controls.size()) ? st->controls[i] : nullptr;
        HWND u = (i < st->units.size()) ? st->units[i] : nullptr;
        settings_show_if_needed(lab, visible);
        settings_show_if_needed(hint, visible);
        settings_show_if_needed(c, visible);
        settings_show_if_needed(u, visible && st->fields[i].kind != SettingValueKind::Bool);
        if (visible) {
            if (lab) defer_or_move(lab, contentX, y + 2, labelW, 17);
            if (hint) defer_or_move(hint, contentX, y + 20, labelW, max_i(14, rowH - 23));
            if (c) {
                if (st->fields[i].kind == SettingValueKind::Bool) defer_or_move(c, contentX + labelW + gap, y + 7, editW + unitW + gap, 22);
                else if (st->fields[i].kind == SettingValueKind::PrioritySlider) defer_or_move(c, contentX + labelW + gap, y + 2, sliderW, 32);
                else defer_or_move(c, contentX + labelW + gap, y + 6, editW, 23);
            }
            if (u && st->fields[i].kind != SettingValueKind::Bool) {
                const int ux = (st->fields[i].kind == SettingValueKind::PrioritySlider) ? (contentX + labelW + gap + sliderW + gap) : (contentX + labelW + gap + editW + gap);
                defer_or_move(u, ux, y + 8, unitW, 19);
            }
        }
    }

    if (hdwp) EndDeferWindowPos(hdwp);

    // Acciones fijas siempre arriba en el orden Z de sus hermanos. No se usan
    // como máscara ni como capa de scroll: simplemente nunca se permite que
    // las filas invadan su rectángulo. Este refuerzo evita clics accidentales
    // cuando Windows conserva un z-order viejo tras varios scrolls.
    auto keep_on_top = [](HWND child) {
        if (child) SetWindowPos(child, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    };
    keep_on_top(st->save);
    keep_on_top(st->load);
    keep_on_top(st->reset);
    keep_on_top(st->close);
    keep_on_top(st->vscroll);

    st->last_panel_w = w;
    st->last_panel_h = h;
    settings_update_scrollbar(panel, st, visibleH);
    RedrawWindow(panel, nullptr, nullptr, RDW_INVALIDATE | RDW_NOCHILDREN);
    st->layout_busy = false;
}


inline void settings_draw_button(DRAWITEMSTRUCT* dis) {
    if (!dis || !dis->hDC) return;
    RECT rc = dis->rcItem;
    const int id = GetDlgCtrlID(dis->hwndItem);
    auto* st = reinterpret_cast<SettingsPanelState*>(GetWindowLongPtrW(GetParent(dis->hwndItem), GWLP_USERDATA));
    const bool close = id == SETTINGS_ID_CLOSE;
    const bool active = false;
    COLORREF page = page_color();
    COLORREF fill = RGB(244, 244, 242);
    if (active) fill = mix_color(page, accent_color(), 0.22);
    if (dis->itemState & ODS_SELECTED) fill = mix_color(page, accent_color(), 0.30);
    if (close) fill = RGB(248, 248, 246);
    HBRUSH br = CreateSolidBrush(fill);
    HGDIOBJ oldb = SelectObject(dis->hDC, br);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ oldp = SelectObject(dis->hDC, pen);
    const int r = max_i(10, (rc.bottom - rc.top) / 2);
    RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(dis->hDC, oldp);
    SelectObject(dis->hDC, oldb);
    DeleteObject(pen);
    DeleteObject(br);
    wchar_t t[96]{};
    GetWindowTextW(dis->hwndItem, t, 96);
    HFONT useFont = (st && st->font) ? st->font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HFONT old = (HFONT)SelectObject(dis->hDC, useFont);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, close ? muted_color() : ink_color());
    DrawTextW(dis->hDC, t, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dis->hDC, old);
}

inline bool settings_save_profile(SettingsPanelState* st) {
    if (!st) return false;
    std::vector<wchar_t> file(32768, L'\0');
    std::wstring suggested = st->app_key + L"_config.miau";
    const HRESULT copyHr = StringCchCopyW(file.data(), file.size(), suggested.c_str());
    if (FAILED(copyHr)) file[0] = L'\0';
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->panel;
    ofn.lpstrFilter = L"Configuración Miausoft (*.miau)\0*.miau\0Todos (*.*)\0*.*\0\0";
    ofn.lpstrFile = file.data();
    ofn.nMaxFile = static_cast<DWORD>(file.size());
    ofn.lpstrDefExt = L"miau";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    std::ofstream out(std::filesystem::path(file.data()), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "# miausoft-config-v1\n";
    out << "app=" << settings_wide_to_utf8(st->app_key) << "\n";
    for (size_t i = 0; i < st->fields.size(); ++i) {
        out << settings_wide_to_utf8(st->fields[i].name) << "=" << settings_wide_to_utf8(settings_field_value(st, i)) << "\n";
    }
    return true;
}

inline bool settings_load_profile(SettingsPanelState* st) {
    if (!st) return false;
    std::vector<wchar_t> file(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->panel;
    ofn.lpstrFilter = L"Configuración Miausoft (*.miau)\0*.miau\0Todos (*.*)\0*.*\0\0";
    ofn.lpstrFile = file.data();
    ofn.nMaxFile = static_cast<DWORD>(file.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return false;
    std::ifstream in(std::filesystem::path(file.data()), std::ios::binary);
    if (!in) return false;
    std::map<std::wstring, std::wstring> kv;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[settings_utf8_to_wide(line.substr(0, eq))] = settings_utf8_to_wide(line.substr(eq + 1));
    }
    for (size_t i = 0; i < st->fields.size(); ++i) {
        auto it = kv.find(st->fields[i].name);
        if (it != kv.end()) settings_write_field_value(st, i, it->second);
    }
    settings_refresh_controls(st);
    apply_priority_from_config(st->app_key.c_str(), 1);
    settings_layout(st->panel);
    return true;
}

inline void settings_reset_defaults(SettingsPanelState* st) {
    if (!st) return;
    for (size_t i = 0; i < st->fields.size(); ++i) {
        const auto& f = st->fields[i];
        if (f.kind == SettingValueKind::Text) registry_write_string(st->app_key.c_str(), f.name, f.default_text ? f.default_text : L"");
        else registry_write_dword(st->app_key.c_str(), f.name, clamp_i(f.default_int, f.min_int, f.max_int));
    }
    settings_refresh_controls(st);
    apply_priority_from_config(st->app_key.c_str(), 1);
    settings_layout(st->panel);
}

inline void settings_jump_to_category(SettingsPanelState* st, int category_index) {
    if (!st || category_index < 0 || category_index >= static_cast<int>(st->categories.size())) return;
    const int rowH = settings_field_row_h(st->last_panel_h > 0 ? st->last_panel_h : 240);
    const int headerH = settings_section_header_h(st->last_panel_h > 0 ? st->last_panel_h : 240);
    int y = 0;
    std::wstring last;
    for (size_t i = 0; i < st->fields.size(); ++i) {
        std::wstring cat = (st->fields[i].category && *st->fields[i].category) ? st->fields[i].category : L"General";
        if (cat != last) {
            if (cat == st->categories[static_cast<size_t>(category_index)]) {
                st->scroll_y = y;
                settings_mark_scroll_badge(st, st->panel, rowH, headerH);
                settings_layout(st->panel);
                return;
            }
            y += headerH;
            last = cat;
        }
        y += rowH;
    }
}


inline LRESULT CALLBACK settings_panel_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<SettingsPanelState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<SettingsPanelState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        if (st) st->panel = hwnd;
    }
    switch (msg) {
    case WM_CREATE:
        if (st) {
            settings_init_common_controls();
            st->font = settings_make_font(10, FW_NORMAL);
            st->title_font = settings_make_font(13, FW_SEMIBOLD);
            st->hint_font = settings_make_font(9, FW_NORMAL);
            st->panel_brush = CreateSolidBrush(RGB(249, 249, 248));
            st->edit_brush = CreateSolidBrush(RGB(255, 255, 255));
            st->fields = settings_fields_for_app(st->app_key, st->has_txt_threshold, st->default_kb);
            st->categories.clear();
            st->tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (st->tooltip) {
                SendMessageW(st->tooltip, TTM_SETMAXTIPWIDTH, 0, 520);
                SendMessageW(st->tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 180);
                SendMessageW(st->tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 24000);
            }
            st->close = CreateWindowW(L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_CLOSE), nullptr, nullptr);
            st->save = CreateWindowW(L"BUTTON", L"Exportar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_SAVE), nullptr, nullptr);
            st->load = CreateWindowW(L"BUTTON", L"Importar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_LOAD), nullptr, nullptr);
            st->reset = CreateWindowW(L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_RESET), nullptr, nullptr);
            st->vscroll = CreateWindowExW(0, L"SCROLLBAR", nullptr, WS_CHILD | WS_VISIBLE | SBS_VERT | WS_TABSTOP,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_SCROLLBAR), nullptr, nullptr);
            for (HWND b : {st->close, st->save, st->load, st->reset}) if (b) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
            settings_add_tooltip(st, st->close, L"Cerrar configuración. Los cambios ya fueron adoptados al modificarlos.");
            settings_add_tooltip(st, st->save, L"Exporta esta configuración a un archivo .miau. No es necesario para aplicar cambios.");
            settings_add_tooltip(st, st->load, L"Importa una configuración .miau y la aplica inmediatamente.");
            settings_add_tooltip(st, st->reset, L"Restablece los valores predeterminados y los aplica inmediatamente.");

            st->nav_buttons.clear();
            settings_destroy_legacy_nav_buttons(hwnd);

            st->labels.resize(st->fields.size(), nullptr);
            st->hints.resize(st->fields.size(), nullptr);
            st->controls.resize(st->fields.size(), nullptr);
            st->units.resize(st->fields.size(), nullptr);
            for (size_t i = 0; i < st->fields.size(); ++i) {
                const auto& f = st->fields[i];
                std::wstring hintText = settings_field_short_hint(f);
                HWND lab = CreateWindowW(L"STATIC", f.label ? f.label : L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                    0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                HWND hint = CreateWindowW(L"STATIC", hintText.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                    0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                if (lab) SendMessageW(lab, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
                if (hint) SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(st->hint_font), TRUE);
                st->labels[i] = lab;
                st->hints[i] = hint;
                const std::wstring tip = settings_tooltip_text(st, i);
                settings_add_tooltip(st, lab, tip);
                settings_add_tooltip(st, hint, tip);
                if (f.kind == SettingValueKind::Bool) {
                    HWND c = CreateWindowW(L"BUTTON", L"sí", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_CHECK_BASE + static_cast<int>(i)), nullptr, nullptr);
                    if (c) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
                    st->controls[i] = c;
                    settings_add_tooltip(st, c, tip);
                } else if (f.kind == SettingValueKind::PrioritySlider) {
                    HWND c = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS | TBS_TOOLTIPS,
                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_TRACK_BASE + static_cast<int>(i)), nullptr, nullptr);
                    HWND u = CreateWindowW(L"STATIC", L"alta", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                        0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                    if (c) {
                        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
                        SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(f.min_int, f.max_int));
                        SendMessageW(c, TBM_SETTICFREQ, 1, 0);
                        SendMessageW(c, TBM_SETPAGESIZE, 0, 1);
                        SendMessageW(c, TBM_SETLINESIZE, 0, 1);
                    }
                    if (u) SendMessageW(u, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
                    st->controls[i] = c;
                    st->units[i] = u;
                    settings_add_tooltip(st, c, tip);
                    settings_add_tooltip(st, u, tip);
                } else {
                    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
                    if (f.kind == SettingValueKind::Int) style |= ES_NUMBER | ES_CENTER;
                    HWND c = CreateWindowExW(0, L"EDIT", L"", style,
                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(SETTINGS_ID_EDIT_BASE + static_cast<int>(i)), nullptr, nullptr);
                    HWND u = CreateWindowW(L"STATIC", f.unit ? f.unit : L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                        0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                    if (c) {
                        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
                        SendMessageW(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
                    }
                    if (u) SendMessageW(u, WM_SETFONT, reinterpret_cast<WPARAM>(st->hint_font), TRUE);
                    st->controls[i] = c;
                    st->units[i] = u;
                    settings_add_tooltip(st, c, tip);
                    settings_add_tooltip(st, u, tip);
                }
            }
            settings_destroy_legacy_nav_buttons(hwnd);
            settings_refresh_controls(st);
            settings_layout(hwnd);
        }
        return 0;
    case WM_SIZE:
        settings_layout(hwnd);
        return 0;
    case WM_HSCROLL:
        if (st && reinterpret_cast<HWND>(lp)) {
            HWND source = reinterpret_cast<HWND>(lp);
            for (size_t i = 0; i < st->controls.size() && i < st->fields.size(); ++i) {
                if (st->controls[i] == source && st->fields[i].kind == SettingValueKind::PrioritySlider) {
                    int v = static_cast<int>(SendMessageW(source, TBM_GETPOS, 0, 0));
                    v = clamp_i(v, st->fields[i].min_int, st->fields[i].max_int);
                    registry_write_dword(st->app_key.c_str(), st->fields[i].name, v);
                    settings_update_priority_unit(st, i, v);
                    apply_priority_from_config(st->app_key.c_str(), 1);
                    return 0;
                }
            }
        }
        break;
    case WM_VSCROLL:
        if (st) {
            RECT rc{}; GetClientRect(hwnd, &rc);
            const int old = st->scroll_y;
            const int page = max_i(1, rc.bottom - rc.top - settings_rows_top(rc.right - rc.left, rc.bottom - rc.top, 0));
            const int maxPos = max_i(0, st->content_h - page);
            switch (LOWORD(wp)) {
            case SB_LINEUP: st->scroll_y -= 28; break;
            case SB_LINEDOWN: st->scroll_y += 28; break;
            case SB_PAGEUP: st->scroll_y -= page; break;
            case SB_PAGEDOWN: st->scroll_y += page; break;
            case SB_TOP: st->scroll_y = 0; break;
            case SB_BOTTOM: st->scroll_y = maxPos; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: {
                HWND source = reinterpret_cast<HWND>(lp);
                HWND target = (source && st->vscroll && source == st->vscroll) ? st->vscroll : hwnd;
                const int bar = (target == st->vscroll) ? SB_CTL : SB_VERT;
                SCROLLINFO si{}; si.cbSize = sizeof(si); si.fMask = SIF_TRACKPOS | SIF_POS;
                if (GetScrollInfo(target, bar, &si)) st->scroll_y = (LOWORD(wp) == SB_THUMBTRACK) ? si.nTrackPos : si.nPos;
                break;
            }}
            st->scroll_y = clamp_i(st->scroll_y, 0, maxPos);
            if (old != st->scroll_y) {
                settings_mark_scroll_badge(st, hwnd, settings_field_row_h(rc.bottom - rc.top), settings_section_header_h(rc.bottom - rc.top));
                settings_layout(hwnd);
            } else {
                settings_update_scrollbar(hwnd, st, page);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (st) {
            RECT rc{}; GetClientRect(hwnd, &rc);
            const int page = max_i(1, rc.bottom - rc.top - settings_rows_top(rc.right - rc.left, rc.bottom - rc.top, 0));
            const int old = st->scroll_y;
            st->scroll_y = clamp_i(st->scroll_y - GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 56, 0, max_i(0, st->content_h - page));
            if (old != st->scroll_y) {
                settings_mark_scroll_badge(st, hwnd, settings_field_row_h(rc.bottom - rc.top), settings_section_header_h(rc.bottom - rc.top));
                settings_layout(hwnd);
            }
        }
        return 0;
    case WM_TIMER:
        if (st && wp == 0x53A1) {
            KillTimer(hwnd, 0x53A1);
            if (GetTickCount64() >= st->scroll_badge_until) {
                st->scroll_badge_until = 0;
                RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
            }
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (st) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            SetBkMode(hdc, TRANSPARENT);
            HWND child = reinterpret_cast<HWND>(lp);
            bool isHint = false;
            for (HWND hnd : st->hints) if (hnd == child) { isHint = true; break; }
            SetTextColor(hdc, isHint ? muted_color() : ink_color());
            return reinterpret_cast<LRESULT>(st->panel_brush ? st->panel_brush : GetStockObject(NULL_BRUSH));
        }
        break;
    case WM_CTLCOLOREDIT:
        if (st) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(255, 255, 255));
            SetTextColor(hdc, ink_color());
            return reinterpret_cast<LRESULT>(st->edit_brush ? st->edit_brush : GetStockObject(WHITE_BRUSH));
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DRAWITEM:
        settings_draw_button(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;
    case WM_COMMAND:
        if (st) {
            const int id = LOWORD(wp);
            if (id == SETTINGS_ID_CLOSE) { DestroyWindow(hwnd); return 0; }
            if (id == SETTINGS_ID_SAVE) { settings_save_profile(st); return 0; }
            if (id == SETTINGS_ID_LOAD) { settings_load_profile(st); return 0; }
            if (id == SETTINGS_ID_RESET) { settings_reset_defaults(st); return 0; }
            if (!st->internal_update && id >= SETTINGS_ID_EDIT_BASE && id < SETTINGS_ID_EDIT_BASE + 500 && HIWORD(wp) == EN_CHANGE) {
                const size_t i = static_cast<size_t>(id - SETTINGS_ID_EDIT_BASE);
                if (i < st->fields.size() && i < st->controls.size()) {
                    wchar_t buf[512]{}; GetWindowTextW(st->controls[i], buf, 512);
                    settings_write_field_value(st, i, buf);
                }
                return 0;
            }
            if (!st->internal_update && id >= SETTINGS_ID_CHECK_BASE && id < SETTINGS_ID_CHECK_BASE + 500 && HIWORD(wp) == BN_CLICKED) {
                const size_t i = static_cast<size_t>(id - SETTINGS_ID_CHECK_BASE);
                if (i < st->fields.size() && i < st->controls.size()) {
                    int checked = (SendMessageW(st->controls[i], BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
                    registry_write_dword(st->app_key.c_str(), st->fields[i].name, checked);
                    SetWindowTextW(st->controls[i], checked ? L"sí" : L"no");
                }
                return 0;
            }
        }
        break;
    case WM_PAINT:
        if (st) {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc{}; GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, st->panel_brush ? st->panel_brush : reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            const int w = max_i(1, rc.right - rc.left);
            const int h = max_i(1, rc.bottom - rc.top);
            const int px = clamp_i(w / 44, 8, 14);
            const int gap = 6;
            const int top = max_i(8, golden_pad_y(h) * 2);
            const int closeSide = clamp_i(static_cast<int>(std::llround(static_cast<double>(h) * 0.074)), 20, 27);
            const int actionH = 23;
            const int actionTop = top + closeSide + 8;
            const int rowsTop = actionTop + actionH + 12;
            const int rowH = settings_field_row_h(h);
            const int headerH = settings_section_header_h(h);
            const int scrollbarReserve = GetSystemMetrics(SM_CXVSCROLL) + 4;
            const int contentX = px;
            const int contentW = max_i(180, w - px * 2 - scrollbarReserve);
            const int titleRight = w - px - closeSide - scrollbarReserve - 8;

            HFONT old = (HFONT)SelectObject(hdc, st->title_font ? st->title_font : GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, ink_color());
            RECT title{px, top, max_i(px + 80, titleRight), top + closeSide};
            std::wstring titleText = L"Configuración · " + st->app_key;
            DrawTextW(hdc, titleText.c_str(), -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

            HFONT old2 = (HFONT)SelectObject(hdc, st->font ? st->font : GetStockObject(DEFAULT_GUI_FONT));
            // Sin encabezados ni píldoras de sección: OCR/Rendimiento/Texto/Tiempos
            // ya no se pintan en ninguna forma dentro de la configuración.
            SelectObject(hdc, old2);
            SelectObject(hdc, old);
            EndPaint(hwnd, &ps);
        }
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->title_font) DeleteObject(st->title_font);
            if (st->hint_font) DeleteObject(st->hint_font);
            if (st->panel_brush) DeleteObject(st->panel_brush);
            if (st->edit_brush) DeleteObject(st->edit_brush);
            RemovePropW(st->owner, L"MiausoftSettingsPanel");
            delete st;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

inline void ensure_settings_class(HINSTANCE inst) {
    static ATOM atom = 0;
    if (atom) return;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = settings_panel_proc;
    wc.hInstance = inst ? inst : GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = settings_panel_class_name();
    wc.style = 0;
    atom = RegisterClassExW(&wc);
}

inline void relayout_settings_panel(HWND owner) {
    HWND panel = reinterpret_cast<HWND>(GetPropW(owner, L"MiausoftSettingsPanel"));
    if (!owner || !panel || !IsWindow(panel)) return;
    // La configuración vive como segunda ventana real, no como panel adherido.
    // Por eso no se reposiciona al mover/redimensionar la ventana principal.
    if (!IsWindowVisible(panel)) ShowWindow(panel, SW_SHOWNOACTIVATE);
    settings_layout(panel);
}

inline void toggle_settings_panel(HWND owner, const wchar_t* app_key, bool has_txt_threshold = false, int default_kb = 9) {
    if (!owner) return;
    HWND existing = reinterpret_cast<HWND>(GetPropW(owner, L"MiausoftSettingsPanel"));
    if (existing && IsWindow(existing)) {
        DestroyWindow(existing);
        RemovePropW(owner, L"MiausoftSettingsPanel");
        return;
    }
    HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    if (!inst) inst = GetModuleHandleW(nullptr);
    ensure_settings_class(inst);
    auto* st = new SettingsPanelState();
    st->owner = owner;
    st->app_key = (app_key && *app_key) ? app_key : L"General";
    st->has_txt_threshold = has_txt_threshold;
    st->default_kb = default_kb;
    RECT wr{}; GetWindowRect(owner, &wr);
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const int panelW = clamp_i(static_cast<int>(std::llround(static_cast<double>(sw) * 0.36)), 560, max_i(560, sw - 80));
    const int panelH = clamp_i(static_cast<int>(std::llround(static_cast<double>(sh) * 0.74)), 520, max_i(520, sh - 80));
    int x = wr.right + 14;
    if (x + panelW > sw - 24) x = max_i(24, wr.left - panelW - 14);
    if (x + panelW > sw - 24) x = max_i(24, (sw - panelW) / 2);
    int y = max_i(24, min_i(wr.top, sh - panelH - 24));
    HWND panel = CreateWindowExW(WS_EX_APPWINDOW, settings_panel_class_name(), L"Configuración Miausoft",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, panelW, panelH, nullptr, nullptr, inst, st);
    if (!panel) { delete st; return; }
    SetPropW(owner, L"MiausoftSettingsPanel", panel);
    settings_layout(panel);
    BringWindowToTop(panel);
    SetForegroundWindow(panel);
    UpdateWindow(panel);
}

inline bool handle_settings_invocation(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, const wchar_t* app_key, bool has_txt_threshold = false, int default_kb = 9) {
    switch (msg) {
    case WM_SIZE:
    case WM_MOVE:
    case WM_WINDOWPOSCHANGED:
        relayout_settings_panel(hwnd);
        return false;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
        if (icon_panel_point(hwnd, lp)) {
            toggle_settings_panel(hwnd, app_key, has_txt_threshold, default_kb);
            return true;
        }
        return false;
    case WM_NCRBUTTONUP:
        if (wp == HTSYSMENU || wp == HTCAPTION || wp == HTNOWHERE) {
            toggle_settings_panel(hwnd, app_key, has_txt_threshold, default_kb);
            return true;
        }
        return false;
    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(wp) == hwnd || wp == 0) {
            toggle_settings_panel(hwnd, app_key, has_txt_threshold, default_kb);
            return true;
        }
        return false;
    }
    return false;
}

inline COLORREF mix_color(COLORREF a, COLORREF b, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    auto ch = [&](int ca, int cb) -> BYTE {
        return static_cast<BYTE>(std::lround((1.0 - t) * ca + t * cb));
    };
    return RGB(ch(GetRValue(a), GetRValue(b)), ch(GetGValue(a), GetGValue(b)), ch(GetBValue(a), GetBValue(b)));
}

inline bool system_color_candidate_ok(COLORREF c) {
    const int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    const int brightness = r + g + b;
    const int spread = max_i(max_i(std::abs(r - g), std::abs(g - b)), std::abs(r - b));
    return brightness > 45 && brightness < 735 && spread >= 10;
}

inline COLORREF colorref_from_argb(DWORD v) {
    return RGB(static_cast<BYTE>((v >> 16) & 0xFF), static_cast<BYTE>((v >> 8) & 0xFF), static_cast<BYTE>(v & 0xFF));
}

inline COLORREF colorref_from_abgr(DWORD v) {
    return RGB(static_cast<BYTE>(v & 0xFF), static_cast<BYTE>((v >> 8) & 0xFF), static_cast<BYTE>((v >> 16) & 0xFF));
}

inline bool read_hkcu_dword_raw(const wchar_t* subkey, const wchar_t* name, DWORD& value) {
    if (!subkey || !*subkey || !name || !*name) return false;
    DWORD type = 0, bytes = sizeof(value);
    LONG rc = RegGetValueW(HKEY_CURRENT_USER, subkey, name, RRF_RT_REG_DWORD, &type, &value, &bytes);
    return rc == ERROR_SUCCESS && bytes == sizeof(value);
}

inline bool try_registry_accent_color(COLORREF& out) {
    struct Candidate { const wchar_t* subkey; const wchar_t* name; bool prefer_abgr; };
    const Candidate keys[] = {
        {L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", true},
        {L"Software\\Microsoft\\Windows\\DWM", L"ColorizationColor", false},
        {L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", L"AccentColorMenu", true},
        {L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", L"StartColorMenu", true},
    };
    for (const auto& k : keys) {
        DWORD raw = 0;
        if (!read_hkcu_dword_raw(k.subkey, k.name, raw)) continue;
        COLORREF first = k.prefer_abgr ? colorref_from_abgr(raw) : colorref_from_argb(raw);
        COLORREF second = k.prefer_abgr ? colorref_from_argb(raw) : colorref_from_abgr(raw);
        if (system_color_candidate_ok(first)) { out = first; return true; }
        if (system_color_candidate_ok(second)) { out = second; return true; }
    }
    return false;
}

inline COLORREF accent_color() {
    const MiausoftRgb color = miausoft_palette_accent();
    return RGB(color.r, color.g, color.b);
}

inline int suite_progress_bar_thickness(int client_height) {
    return miausoft_progress_bar_thickness_px(max_i(1, client_height));
}

inline int top_progress_height(int client_height) {
    return suite_progress_bar_thickness(client_height);
}


inline int title_top_y(int client_height) {
    const int h = max_i(1, client_height);
    const int bar = top_progress_height(h);
    return bar + clamp_i(static_cast<int>(std::llround(static_cast<double>(h) * 0.150)), 30, 42);
}

inline int dialog_title_px(int client_height) {
    (void)client_height;
    return 13;
}

inline int dialog_subtitle_px(int client_height) {
    (void)client_height;
    return 10;
}

inline int progress_icon_y(int client_height, int icon_side) {
    const int h = max_i(1, client_height);
    (void)icon_side;
    return top_progress_height(h) + clamp_i(static_cast<int>(std::llround(static_cast<double>(h) / 13.0)), 14, 22);
}

inline int force_button_w(int icon_panel_width_or_side) {
    const int v = max_i(1, icon_panel_width_or_side);
    const int pad = max_i(1, golden_pad_x(v));
    return max_i(42, v - 2 * pad);
}

inline int force_button_h(int client_height) {
    const int h = max_i(1, client_height);
    return clamp_i(static_cast<int>(std::llround(static_cast<double>(h) * 0.112)), 23, 26);
}

inline int force_button_y(int client_height, int button_height) {
    return max_i(0, max_i(1, client_height) - button_height - 12);
}

inline const wchar_t* ui_font_family() { return L"Segoe UI"; }

inline int icon_side_for_panel(int icon_panel_width, int client_height) {
    const int panel = max_i(1, icon_panel_width);
    const int h = max_i(1, client_height);
    const int padX = max_i(1, golden_pad_x(panel));
    const int padY = max_i(3, golden_pad_y(h));
    const int side_by_panel = max_i(1, panel - 2 * padX);
    const int btnH = force_button_h(h);
    const int gap = 6;
    const int reserved_for_three_buttons = btnH * 3 + gap * 2 + clamp_i(h / 22, 8, 18);
    const int y = progress_icon_y(h, side_by_panel);
    const int side_by_height = max_i(1, h - y - reserved_for_three_buttons - padY);
    return max_i(42, min_i(side_by_panel, side_by_height));
}

inline int button_gap(int client_height) {
    (void)client_height;
    return 6;
}

inline int button_stack_top_y(int client_height, int icon_side, int button_height, int button_count) {
    const int h = max_i(1, client_height);
    const int count = clamp_i(button_count, 1, 8);
    const int gap = button_gap(h);
    const int stack_h = button_height * count + gap * (count - 1);
    const int bottom_aligned = max_i(top_progress_height(h) + 8, h - 12 - stack_h);
    const int icon_bottom = progress_icon_y(h, icon_side) + icon_side;
    const int after_icon = icon_bottom + clamp_i(h / 14, 12, 18);
    if (bottom_aligned >= after_icon) return bottom_aligned;
    return min_i(max_i(after_icon, top_progress_height(h) + 8), max_i(top_progress_height(h) + 8, h - stack_h - 6));
}

inline int title_text_height(int client_height) {
    return clamp_i(max_i(1, client_height) / 10, 22, 30);
}

inline int subtitle_top_y(int client_height) {
    return title_top_y(client_height) + title_text_height(client_height) + 5;
}

inline int table_button_w(int client_width) {
    (void)client_width;
    return 116;
}

inline int table_button_h(int client_height) {
    return force_button_h(client_height);
}

inline std::wstring ellipsize_middle(std::wstring s, size_t limit) {
    if (limit < 8 || s.size() <= limit) return s;
    const size_t left = (limit - 1) / 2;
    const size_t right = limit - left - 1;
    return s.substr(0, left) + L"…" + s.substr(s.size() - right);
}

inline COLORREF page_color() {
    const MiausoftRgb color = miausoft_palette_light_root();
    return RGB(color.r, color.g, color.b);
}
inline COLORREF panel_color() {
    const MiausoftRgb color = miausoft_palette_light_frame();
    return RGB(color.r, color.g, color.b);
}
inline COLORREF ink_color() {
    const MiausoftRgb color = miausoft_palette_light_ink();
    return RGB(color.r, color.g, color.b);
}
inline COLORREF muted_color() {
    const MiausoftRgb color = miausoft_palette_light_muted();
    return RGB(color.r, color.g, color.b);
}
inline COLORREF progress_track_color() { return panel_color(); }
inline COLORREF progress_track_soft_color() { return mix_color(accent_color(), page_color(), 0.84); }

inline void fill_rect(HDC hdc, const RECT& rc, COLORREF color) {
    HBRUSH br = CreateSolidBrush(color);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

inline void draw_top_progress(HDC hdc, int width, int height, double progress, double ghost = -1.0) {
    const int bar_h = top_progress_height(height);
    RECT track{0, 0, max_i(1, width), bar_h};
    fill_rect(hdc, track, progress_track_soft_color());
    const COLORREF accent = accent_color();
    if (ghost >= 0.0) {
        RECT gr = track;
        const double g = std::clamp(ghost, 0.0, 1.0);
        gr.right = gr.left + static_cast<int>((gr.right - gr.left) * g);
        if (gr.right > gr.left) fill_rect(hdc, gr, mix_color(progress_track_color(), accent, 0.58));
    }
    RECT cr = track;
    const double p = std::clamp(progress, 0.0, 1.0);
    cr.right = cr.left + static_cast<int>((cr.right - cr.left) * p);
    if (cr.right > cr.left) fill_rect(hdc, cr, accent);
}

} // namespace miausoft_visual
