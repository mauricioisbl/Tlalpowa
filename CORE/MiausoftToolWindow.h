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
#include <string>
#include <algorithm>
#include <cmath>
#include "MiausoftVisualConfig.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef MIAUSOFT_APP_TITLE
#define MIAUSOFT_APP_TITLE L"Miausoft"
#endif
#ifndef MIAUSOFT_APP_SUBTITLE
#define MIAUSOFT_APP_SUBTITLE L"Herramienta de Miausoft preparada para integración funcional."
#endif
#ifndef MIAUSOFT_APP_DETAIL
#define MIAUSOFT_APP_DETAIL L""
#endif
#ifndef MIAUSOFT_WINDOW_CLASS
#define MIAUSOFT_WINDOW_CLASS L"MiausoftUniformWindow"
#endif
#ifndef IDI_MIAUSOFT_ICON
#define IDI_MIAUSOFT_ICON 101
#endif

namespace miausoft_ui {
static HINSTANCE g_inst = nullptr;
static HICON g_icon_big = nullptr;
static HICON g_icon_small = nullptr;
static constexpr double MIAUSOFT_WINDOW_WIDTH_RATIO = miausoft_visual::window_width_ratio;
static constexpr double MIAUSOFT_WINDOW_HEIGHT_RATIO = miausoft_visual::window_height_ratio;
static constexpr double MIAUSOFT_ICON_PANEL_RATIO = miausoft_visual::icon_panel_ratio;

static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int max_i(int a, int b) { return a > b ? a : b; }
static int min_i(int a, int b) { return a < b ? a : b; }

static COLORREF mix(COLORREF a, COLORREF b, double t) { return miausoft_visual::mix_color(a, b, t); }

static COLORREF accent_color() { return miausoft_visual::accent_color(); }

static HFONT make_font(int px, int weight = FW_NORMAL, bool italic = false) {
    return CreateFontW(-px, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void draw_text(HDC hdc, const wchar_t* s, RECT rc, HFONT f, COLORREF color, UINT flags) {
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, s, -1, &rc, flags | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void fill_round(HDC hdc, RECT rc, COLORREF c, int radius) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC screen = BeginPaint(hwnd, &ps);
    RECT rc{}; GetClientRect(hwnd, &rc);
    int w = max_i(1, rc.right - rc.left), h = max_i(1, rc.bottom - rc.top);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    const COLORREF page = RGB(249, 249, 248);
    const COLORREF ink = RGB(26, 26, 26);
    const COLORREF muted = RGB(82, 82, 82);
    const COLORREF track = miausoft_visual::progress_track_soft_color();
    const COLORREF accent = accent_color();
    HBRUSH bg = CreateSolidBrush(page);
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    const int iconPanelW = miausoft_visual::icon_panel_width(w);
    const int pad = max_i(miausoft_visual::golden_pad_x(w), miausoft_visual::golden_pad_y(h));
    const int iconSide = miausoft_visual::icon_side_for_panel(iconPanelW, h);
    const int iconX = max_i(0, (iconPanelW - iconSide) / 2);
    const int iconY = miausoft_visual::progress_icon_y(h, iconSide);
    if (g_icon_big) DrawIconEx(mem, iconX, iconY, g_icon_big, iconSide, iconSide, 0, nullptr, DI_NORMAL);

    const int contentX = iconPanelW;
    const int contentR = w - pad;
    const int titleY = miausoft_visual::title_top_y(h);
    HFONT title = make_font(miausoft_visual::dialog_title_px(h), FW_NORMAL);
    HFONT sub = make_font(miausoft_visual::dialog_subtitle_px(h), FW_NORMAL);
    HFONT detail = make_font(clamp_i(h / 20, 8, 11), FW_NORMAL);
    const int titleH = miausoft_visual::title_text_height(h);
    RECT tr{contentX, titleY, contentR, titleY + titleH};
    RECT sr{contentX, miausoft_visual::subtitle_top_y(h), contentR, miausoft_visual::subtitle_top_y(h) + 18};
    RECT dr{contentX, sr.bottom + 6, contentR, sr.bottom + 40};
    draw_text(mem, MIAUSOFT_APP_TITLE, tr, title, ink, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    draw_text(mem, MIAUSOFT_APP_SUBTITLE, sr, sub, muted, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    draw_text(mem, MIAUSOFT_APP_DETAIL, dr, detail, RGB(125,125,125), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    const int barH = miausoft_visual::top_progress_height(h);
    RECT bar{0, 0, w, barH};
    fill_round(mem, bar, track, barH);
    RECT fill = bar;
    fill.right = bar.left + (int)((bar.right - bar.left) * 0.24);
    fill_round(mem, fill, accent, barH);
    HFONT pct = make_font(14, FW_NORMAL);
    RECT pctRc{contentR - 56, bar.bottom + 8, contentR, bar.bottom + 24};
    draw_text(mem, L"24%", pctRc, pct, muted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(pct);

    BitBlt(screen, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    DeleteObject(title); DeleteObject(sub); DeleteObject(detail);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE:
    case WM_MOVE: miausoft_visual::relayout_settings_panel(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: if (miausoft_visual::handle_settings_invocation(hwnd, msg, wp, lp, L"General", false, 9)) return 0; break;
    case WM_NCRBUTTONUP:
    case WM_CONTEXTMENU: if (miausoft_visual::handle_settings_invocation(hwnd, msg, wp, lp, L"General", false, 9)) return 0; break;
    case WM_DWMCOLORIZATIONCOLORCHANGED: InvalidateRect(hwnd, nullptr, FALSE); return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace miausoft_ui

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    using namespace miausoft_ui;
    g_inst = hInst;
    g_icon_big = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MIAUSOFT_ICON), IMAGE_ICON, 180, 180, LR_DEFAULTCOLOR);
    g_icon_small = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MIAUSOFT_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.hIcon = g_icon_big ? g_icon_big : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = g_icon_small ? g_icon_small : wc.hIcon;
    wc.lpszClassName = MIAUSOFT_WINDOW_CLASS;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const int ww = max_i(1, (int)std::llround((double)sw * MIAUSOFT_WINDOW_WIDTH_RATIO));
    const int wh = max_i(1, (int)std::llround((double)sh * MIAUSOFT_WINDOW_HEIGHT_RATIO));
    const int x = (sw - ww) / 2;
    const int y = (sh - wh) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, MIAUSOFT_APP_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, ww, wh, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    miausoft_visual::apply_rounded_top_window(hwnd);
    if (g_icon_big) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_icon_big);
    if (g_icon_small) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_icon_small);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (g_icon_small) DestroyIcon(g_icon_small);
    if (g_icon_big) DestroyIcon(g_icon_big);
    return (int)msg.wParam;
}

int main() { return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT); }
