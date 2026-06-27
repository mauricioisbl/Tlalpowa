#include "MiausoftVisualWin32.h"

#if defined(_WIN32)

#include <wingdi.h>

static HFONT msv_win32_font(const MiausoftVisualWin32Context* context, uint8_t role) {
    HFONT font = 0;
    if (!context) return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    switch (role) {
        case MIAUSOFT_VISUAL_FONT_STRONG: font = context->fonts.strong; break;
        case MIAUSOFT_VISUAL_FONT_TITLE: font = context->fonts.title; break;
        case MIAUSOFT_VISUAL_FONT_SMALL: font = context->fonts.small_font; break;
        default: font = context->fonts.body; break;
    }
    return font ? font : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static RECT msv_win32_rect(MiausoftRectI r) {
    RECT out;
    out.left = r.x;
    out.top = r.y;
    out.right = r.x + r.w;
    out.bottom = r.y + r.h;
    return out;
}

static UINT msv_win32_text_flags(uint16_t flags) {
    UINT out = DT_NOPREFIX;
    if (flags & MIAUSOFT_VISUAL_TEXT_CENTER) out |= DT_CENTER;
    else if (flags & MIAUSOFT_VISUAL_TEXT_RIGHT) out |= DT_RIGHT;
    else out |= DT_LEFT;
    if (flags & MIAUSOFT_VISUAL_TEXT_MIDDLE) out |= DT_VCENTER | DT_SINGLELINE;
    else if (flags & MIAUSOFT_VISUAL_TEXT_BOTTOM) out |= DT_BOTTOM | DT_SINGLELINE;
    else out |= DT_TOP;
    if (flags & MIAUSOFT_VISUAL_TEXT_ELLIPSIS) out |= DT_END_ELLIPSIS;
    if (flags & MIAUSOFT_VISUAL_TEXT_WRAP) out |= DT_WORDBREAK;
    return out;
}

static void msv_win32_fill(HDC hdc, RECT rc, MiausoftRgba color) {
    HBRUSH brush;
    if (!hdc || rc.right <= rc.left || rc.bottom <= rc.top) return;
    brush = CreateSolidBrush(miausoft_visual_win32_color(color));
    if (!brush) return;
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

static void msv_win32_round(HDC hdc, RECT rc, MiausoftRgba color, int radius) {
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    COLORREF c;
    if (!hdc || rc.right <= rc.left || rc.bottom <= rc.top) return;
    c = miausoft_visual_win32_color(color);
    brush = CreateSolidBrush(c);
    pen = CreatePen(PS_SOLID, 1, c);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    old_brush = SelectObject(hdc, brush);
    old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void msv_win32_round_top(HDC hdc, RECT rc, MiausoftRgba color, int radius) {
    HRGN rounded;
    HRGN lower;
    HBRUSH brush;
    if (!hdc || rc.right <= rc.left || rc.bottom <= rc.top) return;
    radius = radius < 1 ? 1 : radius;
    rounded = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, radius * 2, radius * 2);
    lower = CreateRectRgn(rc.left, rc.top + radius, rc.right + 1, rc.bottom + 1);
    brush = CreateSolidBrush(miausoft_visual_win32_color(color));
    if (rounded && lower && brush) {
        CombineRgn(rounded, rounded, lower, RGN_OR);
        FillRgn(hdc, rounded, brush);
    }
    if (brush) DeleteObject(brush);
    if (lower) DeleteObject(lower);
    if (rounded) DeleteObject(rounded);
}

static void msv_win32_gradient(HDC hdc, RECT rc, MiausoftRgba a, MiausoftRgba b) {
    TRIVERTEX vertices[2];
    GRADIENT_RECT gradient;
    if (!hdc || rc.right <= rc.left || rc.bottom <= rc.top) return;
    vertices[0].x = rc.left;
    vertices[0].y = rc.top;
    vertices[0].Red = (COLOR16)((unsigned int)a.r << 8);
    vertices[0].Green = (COLOR16)((unsigned int)a.g << 8);
    vertices[0].Blue = (COLOR16)((unsigned int)a.b << 8);
    vertices[0].Alpha = 0xffff;
    vertices[1].x = rc.right;
    vertices[1].y = rc.bottom;
    vertices[1].Red = (COLOR16)((unsigned int)b.r << 8);
    vertices[1].Green = (COLOR16)((unsigned int)b.g << 8);
    vertices[1].Blue = (COLOR16)((unsigned int)b.b << 8);
    vertices[1].Alpha = 0xffff;
    gradient.UpperLeft = 0;
    gradient.LowerRight = 1;
    GradientFill(hdc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
}

static void msv_win32_text(const MiausoftVisualWin32Context* context, const MiausoftVisualDrawCommand* cmd) {
    RECT rc;
    HFONT font;
    HGDIOBJ old_font;
    UINT flags;
    wchar_t wide[4096];
    int wide_len = 0;
    if (!context || !context->hdc || !cmd || !cmd->text.data || cmd->text.length == 0) return;
    rc = msv_win32_rect(cmd->rect);
    font = msv_win32_font(context, cmd->text.font_role);
    old_font = SelectObject(context->hdc, font);
    SetBkMode(context->hdc, TRANSPARENT);
    SetTextColor(context->hdc, miausoft_visual_win32_color(cmd->color));
    flags = msv_win32_text_flags(cmd->text.flags);

    if (cmd->text.encoding == MIAUSOFT_VISUAL_TEXT_UTF16) {
        DrawTextW(context->hdc, (const wchar_t*)cmd->text.data, (int)cmd->text.length, &rc, flags);
    } else if (cmd->text.encoding == MIAUSOFT_VISUAL_TEXT_UTF8) {
        const char* utf8 = (const char*)cmd->text.data;
        int input_len = (int)cmd->text.length;
        const int wide_cap = (int)(sizeof(wide) / sizeof(wide[0])) - 1;
        if (input_len > wide_cap) input_len = wide_cap;
        while (input_len > 0) {
            wide_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, input_len, wide, wide_cap);
            if (wide_len > 0) break;
            --input_len;
        }
        if (wide_len > 0) {
            wide[wide_len] = L'\0';
            DrawTextW(context->hdc, wide, wide_len, &rc, flags);
        }
    }
    SelectObject(context->hdc, old_font);
}

COLORREF miausoft_visual_win32_color(MiausoftRgba color) {
    return RGB(color.r, color.g, color.b);
}

void miausoft_visual_win32_render(const MiausoftVisualWin32Context* context, const MiausoftVisualDrawList* list) {
    uint32_t i;
    if (!context || !context->hdc || !list || !list->commands) return;
    for (i = 0; i < list->count; ++i) {
        const MiausoftVisualDrawCommand* cmd = &list->commands[i];
        RECT rc = msv_win32_rect(cmd->rect);
        switch ((MiausoftVisualDrawCommandKind)cmd->kind) {
            case MIAUSOFT_VISUAL_DRAW_RECT:
                msv_win32_fill(context->hdc, rc, cmd->color);
                break;
            case MIAUSOFT_VISUAL_DRAW_ROUND_RECT:
                msv_win32_round(context->hdc, rc, cmd->color, cmd->radius);
                break;
            case MIAUSOFT_VISUAL_DRAW_ROUND_TOP_RECT:
                msv_win32_round_top(context->hdc, rc, cmd->color, cmd->radius);
                break;
            case MIAUSOFT_VISUAL_DRAW_GRADIENT_RECT:
                msv_win32_gradient(context->hdc, rc, cmd->color, cmd->color2);
                break;
            case MIAUSOFT_VISUAL_DRAW_TEXT:
                msv_win32_text(context, cmd);
                break;
            case MIAUSOFT_VISUAL_DRAW_LINE: {
                HPEN pen = CreatePen(PS_SOLID, cmd->thickness > 0 ? cmd->thickness : 1, miausoft_visual_win32_color(cmd->color));
                HGDIOBJ old = SelectObject(context->hdc, pen);
                MoveToEx(context->hdc, cmd->rect.x, cmd->rect.y, 0);
                LineTo(context->hdc, cmd->x2, cmd->y2);
                SelectObject(context->hdc, old);
                DeleteObject(pen);
                break;
            }
            case MIAUSOFT_VISUAL_DRAW_TRIANGLE: {
                POINT points[3];
                HBRUSH brush = CreateSolidBrush(miausoft_visual_win32_color(cmd->color));
                HPEN pen = CreatePen(PS_SOLID, 1, miausoft_visual_win32_color(cmd->color));
                HGDIOBJ old_brush = SelectObject(context->hdc, brush);
                HGDIOBJ old_pen = SelectObject(context->hdc, pen);
                points[0].x = cmd->rect.x; points[0].y = cmd->rect.y;
                points[1].x = cmd->x2; points[1].y = cmd->y2;
                points[2].x = cmd->x3; points[2].y = cmd->y3;
                Polygon(context->hdc, points, 3);
                SelectObject(context->hdc, old_pen);
                SelectObject(context->hdc, old_brush);
                DeleteObject(pen);
                DeleteObject(brush);
                break;
            }
            case MIAUSOFT_VISUAL_DRAW_CIRCLE: {
                HBRUSH brush = CreateSolidBrush(miausoft_visual_win32_color(cmd->color));
                HPEN pen = CreatePen(PS_SOLID, 1, miausoft_visual_win32_color(cmd->color));
                HGDIOBJ old_brush = SelectObject(context->hdc, brush);
                HGDIOBJ old_pen = SelectObject(context->hdc, pen);
                Ellipse(context->hdc, rc.left, rc.top, rc.right, rc.bottom);
                SelectObject(context->hdc, old_pen);
                SelectObject(context->hdc, old_brush);
                DeleteObject(pen);
                DeleteObject(brush);
                break;
            }
            default:
                break;
        }
    }
}

#endif
