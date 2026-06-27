#include "MiausoftVisual.h"

#include <math.h>
#include <string.h>

static const double msv_phi_ratios[] = {
    MIAUSOFT_PHI_N0, MIAUSOFT_PHI_N1, MIAUSOFT_PHI_N2, MIAUSOFT_PHI_N3,
    MIAUSOFT_PHI_N4, MIAUSOFT_PHI_N5, MIAUSOFT_PHI_N6, MIAUSOFT_PHI_N7,
    MIAUSOFT_PHI_N8, MIAUSOFT_PHI_N9, MIAUSOFT_PHI_N10, MIAUSOFT_PHI_N11,
    MIAUSOFT_PHI_N12, MIAUSOFT_PHI_N13, MIAUSOFT_PHI_N14, MIAUSOFT_PHI_N15,
    MIAUSOFT_PHI_N16, MIAUSOFT_PHI_N17, MIAUSOFT_PHI_N18, MIAUSOFT_PHI_N19,
    MIAUSOFT_PHI_N20
};

static int msv_min_i(int a, int b) { return a < b ? a : b; }
static int msv_max_i(int a, int b) { return a > b ? a : b; }
static int msv_clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float msv_clamp_f(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float msv_max_f(float a, float b) { return a > b ? a : b; }

static uint8_t msv_u8(float v) {
    return (uint8_t)msv_clamp_i((int)(v + 0.5f), 0, 255);
}

static uint32_t msv_text_length8(const char* text) {
    size_t n;
    if (!text) return 0u;
    n = strlen(text);
    return n > 0xffffffffu ? 0xffffffffu : (uint32_t)n;
}

static uint32_t msv_text_length16(const uint16_t* text) {
    uint32_t n = 0u;
    if (!text) return 0u;
    while (text[n] && n != 0xffffffffu) ++n;
    return n;
}

static MiausoftRgb msv_rgb(unsigned int r, unsigned int g, unsigned int b) {
    MiausoftRgb c;
    c.r = (uint8_t)r;
    c.g = (uint8_t)g;
    c.b = (uint8_t)b;
    return c;
}

static MiausoftRgba msv_accent_or(const MiausoftVisualTheme* theme, MiausoftRgba color) {
    return color.a ? color : theme->accent;
}

static MiausoftRectI msv_inset(MiausoftRectI r, int x, int y) {
    r.x += x;
    r.y += y;
    r.w -= x * 2;
    r.h -= y * 2;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

static MiausoftRectI msv_rect(int x, int y, int w, int h) {
    MiausoftRectI r;
    r.x = x;
    r.y = y;
    r.w = w < 0 ? 0 : w;
    r.h = h < 0 ? 0 : h;
    return r;
}

static int msv_push_shape(MiausoftVisualDrawList* list,
                          MiausoftVisualDrawCommandKind kind,
                          MiausoftRectI rect,
                          MiausoftRgba color,
                          MiausoftRgba color2,
                          int radius) {
    MiausoftVisualDrawCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = (uint8_t)kind;
    cmd.rect = rect;
    cmd.color = color;
    cmd.color2 = color2;
    cmd.radius = radius;
    cmd.thickness = 1;
    return miausoft_visual_draw_list_push(list, &cmd);
}

static int msv_push_text(MiausoftVisualDrawList* list,
                         MiausoftRectI rect,
                         MiausoftRgba color,
                         MiausoftVisualText text) {
    MiausoftVisualDrawCommand cmd;
    if (!text.data || text.length == 0u) return 1;
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = MIAUSOFT_VISUAL_DRAW_TEXT;
    cmd.rect = rect;
    cmd.color = color;
    cmd.text = text;
    return miausoft_visual_draw_list_push(list, &cmd);
}

static int msv_push_triangle(MiausoftVisualDrawList* list,
                             int x1, int y1, int x2, int y2, int x3, int y3,
                             MiausoftRgba color) {
    MiausoftVisualDrawCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = MIAUSOFT_VISUAL_DRAW_TRIANGLE;
    cmd.rect.x = x1;
    cmd.rect.y = y1;
    cmd.x2 = x2;
    cmd.y2 = y2;
    cmd.x3 = x3;
    cmd.y3 = y3;
    cmd.color = color;
    return miausoft_visual_draw_list_push(list, &cmd);
}

int miausoft_visual_revision(void) {
    return MIAUSOFT_VISUAL_REVISION;
}

int miausoft_visual_validate(void) {
    MiausoftVisualTheme theme = miausoft_visual_theme(1, miausoft_rgba(133, 13, 55, 255));
    MiausoftVisualComponentStyle style = miausoft_visual_component_style(
        &theme, MIAUSOFT_TLALPOWA_CHECKBOX, 0u, theme.accent, 22);
    int kind;
    for (kind = MIAUSOFT_TLALPOWA_SURFACE; kind <= MIAUSOFT_TLALPOWA_LAST; ++kind) {
        if (!miausoft_tlalpowa_template_validate((MiausoftTlalpowaTemplateKind)kind))
            return 0;
    }
    return miausoft_visual_revision() == MIAUSOFT_VISUAL_REVISION &&
           theme.borderless != 0u && style.border_width == 0;
}

double miausoft_phi_ratio(unsigned int power) {
    const unsigned int count = (unsigned int)(sizeof(msv_phi_ratios) / sizeof(msv_phi_ratios[0]));
    double value;
    unsigned int i;
    if (power < count) return msv_phi_ratios[power];
    value = msv_phi_ratios[count - 1u];
    for (i = count - 1u; i < power; ++i) value /= MIAUSOFT_PHI;
    return value;
}

MiausoftPhiExpr miausoft_phi_expr(unsigned int n1, unsigned int n2, unsigned int n3) {
    MiausoftPhiExpr e;
    memset(&e, 0, sizeof(e));
    e.terms[e.count++] = (uint8_t)msv_clamp_i((int)n1, 0, 255);
    if (n2 != 0u) e.terms[e.count++] = (uint8_t)msv_clamp_i((int)n2, 0, 255);
    if (n3 != 0u) e.terms[e.count++] = (uint8_t)msv_clamp_i((int)n3, 0, 255);
    return e;
}

MiausoftPhiExpr miausoft_phi_expr_clamped(unsigned int n1, unsigned int n2, unsigned int n3,
                                          int min_px, int max_px) {
    MiausoftPhiExpr e = miausoft_phi_expr(n1, n2, n3);
    e.min_px = (int16_t)msv_clamp_i(min_px, -32768, 32767);
    e.max_px = (int16_t)msv_clamp_i(max_px, -32768, 32767);
    return e;
}

double miausoft_phi_expr_ratio(MiausoftPhiExpr expr) {
    double ratio = 0.0;
    unsigned int i;
    for (i = 0u; i < expr.count && i < 3u; ++i) ratio += miausoft_phi_ratio(expr.terms[i]);
    return ratio;
}

int miausoft_phi_expr_px(int extent, MiausoftPhiExpr expr) {
    const int base = extent < 1 ? 1 : extent;
    int px = (int)((double)base * miausoft_phi_expr_ratio(expr) + 0.5);
    if (expr.min_px > 0 && px < expr.min_px) px = expr.min_px;
    if (expr.max_px > 0 && px > expr.max_px) px = expr.max_px;
    return px;
}

int miausoft_phi_scale_px(int extent, unsigned int power, int minimum) {
    const int base = extent < 1 ? 1 : extent;
    const int px = (int)((double)base * miausoft_phi_ratio(power) + 0.5);
    return px < minimum ? minimum : px;
}

int miausoft_visual_px(int extent, double ratio, int minimum) {
    const int base = extent < 1 ? 1 : extent;
    const int px = (int)((double)base * ratio + 0.5);
    return px < minimum ? minimum : px;
}

MiausoftTlalpowaTemplate miausoft_tlalpowa_template(MiausoftTlalpowaTemplateKind kind) {
    MiausoftTlalpowaTemplate t;
    memset(&t, 0, sizeof(t));
    t.flags = MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS;
    t.adjustable = MIAUSOFT_TLALPOWA_PARAM_CONTENT | MIAUSOFT_TLALPOWA_PARAM_STATE;
    switch (kind) {
    case MIAUSOFT_TLALPOWA_SURFACE:
    case MIAUSOFT_TLALPOWA_SIDE_PANEL:
    case MIAUSOFT_TLALPOWA_SETTINGS_WINDOW:
    case MIAUSOFT_TLALPOWA_SETTINGS_INDEX:
    case MIAUSOFT_TLALPOWA_TABLE:
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_FLAT;
        break;
    case MIAUSOFT_TLALPOWA_TOP_BAR:
    case MIAUSOFT_TLALPOWA_BOTTOM_BAR:
        t.height = miausoft_phi_expr(7, 0, 0);
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_FLAT;
        break;
    case MIAUSOFT_TLALPOWA_BUTTON:
    case MIAUSOFT_TLALPOWA_TAB:
    case MIAUSOFT_TLALPOWA_BOX:
    case MIAUSOFT_TLALPOWA_INPUT:
    case MIAUSOFT_TLALPOWA_SEARCH:
    case MIAUSOFT_TLALPOWA_COMBO:
    case MIAUSOFT_TLALPOWA_SLIDER:
        t.height = miausoft_phi_expr(7, 0, 0);
        t.padding_x = miausoft_phi_expr(1, 0, 0);
        t.gap = miausoft_phi_expr(5, 0, 0);
        t.rounding = miausoft_phi_expr(2, 0, 0);
        if (kind == MIAUSOFT_TLALPOWA_TAB)
            t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_TOP_ROUNDED;
        if (kind == MIAUSOFT_TLALPOWA_SLIDER)
            t.adjustable |= MIAUSOFT_TLALPOWA_PARAM_VALUE | MIAUSOFT_TLALPOWA_PARAM_COLOR;
        break;
    case MIAUSOFT_TLALPOWA_SMALL_BUTTON:
        t.height = miausoft_phi_expr(7, 0, 0);
        t.padding_x = miausoft_phi_expr(1, 0, 0);
        t.gap = miausoft_phi_expr(5, 0, 0);
        t.rounding = miausoft_phi_expr(2, 0, 0);
        break;
    case MIAUSOFT_TLALPOWA_CHECKBOX:
        t.width = miausoft_phi_expr(1, 0, 0);
        t.height = miausoft_phi_expr(1, 0, 0);
        t.padding_x = miausoft_phi_expr(3, 0, 0);
        t.rounding = miausoft_phi_expr(2, 0, 0);
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_TRANSLUCENT |
                   MIAUSOFT_TLALPOWA_TEMPLATE_NESTABLE;
        t.adjustable |= MIAUSOFT_TLALPOWA_PARAM_COLOR |
                        MIAUSOFT_TLALPOWA_PARAM_NESTING;
        break;
    case MIAUSOFT_TLALPOWA_SIDE_TREE_ROW:
    case MIAUSOFT_TLALPOWA_TABLE_HEADER:
    case MIAUSOFT_TLALPOWA_TABLE_ROW:
    case MIAUSOFT_TLALPOWA_SELECTABLE:
    case MIAUSOFT_TLALPOWA_MENU_ITEM:
    case MIAUSOFT_TLALPOWA_COLLAPSING_HEADER:
        t.height = miausoft_phi_expr(8, 0, 0);
        t.padding_x = miausoft_phi_expr(3, 0, 0);
        t.gap = miausoft_phi_expr(7, 0, 0);
        t.rounding = miausoft_phi_expr(4, 0, 0);
        if (kind == MIAUSOFT_TLALPOWA_SIDE_TREE_ROW ||
            kind == MIAUSOFT_TLALPOWA_COLLAPSING_HEADER) {
            t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_NESTABLE;
            t.adjustable |= MIAUSOFT_TLALPOWA_PARAM_NESTING;
        }
        if (kind == MIAUSOFT_TLALPOWA_TABLE_ROW)
            t.adjustable |= MIAUSOFT_TLALPOWA_PARAM_ROW_INDEX;
        break;
    case MIAUSOFT_TLALPOWA_PROGRESS:
        t.height = miausoft_phi_expr(11, 0, 0);
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_FLAT;
        t.adjustable |= MIAUSOFT_TLALPOWA_PARAM_VALUE |
                        MIAUSOFT_TLALPOWA_PARAM_COLOR;
        break;
    case MIAUSOFT_TLALPOWA_TOOLTIP:
    case MIAUSOFT_TLALPOWA_POPUP:
    case MIAUSOFT_TLALPOWA_MODAL:
    case MIAUSOFT_TLALPOWA_CARD:
        t.padding_x = miausoft_phi_expr(2, 0, 0);
        t.padding_y = miausoft_phi_expr(2, 0, 0);
        t.gap = miausoft_phi_expr(5, 0, 0);
        t.rounding = miausoft_phi_expr(2, 0, 0);
        if (kind != MIAUSOFT_TLALPOWA_CARD)
            t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_TRANSLUCENT;
        break;
    case MIAUSOFT_TLALPOWA_LABEL:
    case MIAUSOFT_TLALPOWA_BULLET:
    case MIAUSOFT_TLALPOWA_SECTION_HEADER:
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_FLAT;
        break;
    case MIAUSOFT_TLALPOWA_SEPARATOR:
    case MIAUSOFT_TLALPOWA_SCROLL_TRACK:
        t.flags |= MIAUSOFT_TLALPOWA_TEMPLATE_FLAT |
                   MIAUSOFT_TLALPOWA_TEMPLATE_INVISIBLE;
        t.adjustable = 0u;
        break;
    default:
        memset(&t, 0, sizeof(t));
        break;
    }
    return t;
}

const char* miausoft_tlalpowa_template_name(MiausoftTlalpowaTemplateKind kind) {
    static const char* const names[] = {
        "none", "surface", "top_bar", "bottom_bar", "button", "tab",
        "small_button", "box", "input", "search", "checkbox", "side_panel",
        "side_tree_row", "progress", "settings_window", "settings_index",
        "slider", "table", "table_header", "table_row", "tooltip", "popup",
        "modal", "combo", "selectable", "menu_item", "collapsing_header",
        "card", "label", "bullet", "section_header", "separator", "scroll_track"
    };
    if (kind < MIAUSOFT_TLALPOWA_NONE || kind > MIAUSOFT_TLALPOWA_LAST)
        return "invalid";
    return names[(int)kind];
}

int miausoft_tlalpowa_template_validate(MiausoftTlalpowaTemplateKind kind) {
    MiausoftTlalpowaTemplate t;
    if (kind < MIAUSOFT_TLALPOWA_SURFACE || kind > MIAUSOFT_TLALPOWA_LAST)
        return 0;
    t = miausoft_tlalpowa_template(kind);
    if ((t.flags & MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS) == 0u)
        return 0;
    if ((t.flags & MIAUSOFT_TLALPOWA_TEMPLATE_INVISIBLE) != 0u &&
        t.adjustable != 0u)
        return 0;
    if (kind == MIAUSOFT_TLALPOWA_CHECKBOX &&
        (t.adjustable & (MIAUSOFT_TLALPOWA_PARAM_COLOR |
                         MIAUSOFT_TLALPOWA_PARAM_NESTING)) !=
        (MIAUSOFT_TLALPOWA_PARAM_COLOR |
         MIAUSOFT_TLALPOWA_PARAM_NESTING))
        return 0;
    return 1;
}

MiausoftTlalpowaMetrics miausoft_tlalpowa_metrics(int width, int height) {
    MiausoftTlalpowaMetrics m;
    const int w = width < 1 ? 1 : width;
    const int h = height < 1 ? 1 : height;
    memset(&m, 0, sizeof(m));
    m.width = w;
    m.height = h;
    m.top_bar_h = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_TLALPOWA_TOP_BAR_RATIO, 1));
    m.bottom_bar_h = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_TLALPOWA_BOTTOM_BAR_RATIO, 1));
    m.control_h = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_TLALPOWA_CONTROL_RATIO, 1));
    m.font_px = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_TLALPOWA_FONT_RATIO, 1));
    m.checkbox_side = msv_max_i(1, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N1, 1));
    m.gap = msv_max_i(1, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N5, 1));
    m.pad_tight = msv_max_i(1, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N3, 1));
    m.pad = msv_max_i(1, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N2, 1));
    m.rounding = msv_max_i(1, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N2, 1));
    m.row_h = msv_max_i(m.checkbox_side, m.font_px) +
              msv_max_i(2, miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N7, 1) +
                            miausoft_visual_px(m.control_h, MIAUSOFT_PHI_N8, 1));
    m.progress_h = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_TLALPOWA_PROGRESS_RATIO, 1));
    m.side_w = msv_max_i(1, miausoft_visual_px(w, MIAUSOFT_TLALPOWA_SIDE_DEFAULT_RATIO, 1));
    m.settings_index_w = msv_max_i(1, miausoft_visual_px(w, MIAUSOFT_TLALPOWA_SETTINGS_INDEX_RATIO, 1));
    return m;
}

MiausoftTlalpowaLayout miausoft_tlalpowa_layout(int width, int height, double side_ratio) {
    MiausoftTlalpowaLayout l;
    MiausoftTlalpowaMetrics m = miausoft_tlalpowa_metrics(width, height);
    const double ratio = side_ratio < MIAUSOFT_TLALPOWA_SIDE_MIN_RATIO
        ? MIAUSOFT_TLALPOWA_SIDE_MIN_RATIO
        : (side_ratio > MIAUSOFT_TLALPOWA_SIDE_MAX_RATIO
            ? MIAUSOFT_TLALPOWA_SIDE_MAX_RATIO : side_ratio);
    int side_w = miausoft_visual_px(m.width, ratio, 1);
    int bottom_y;
    int content_y;
    int workspace_h;
    if (side_w >= m.width) side_w = m.width - 1;
    if (side_w < 1) side_w = 1;
    bottom_y = m.height - m.bottom_bar_h;
    content_y = m.top_bar_h + m.progress_h;
    if (bottom_y <= content_y) bottom_y = content_y + 1;
    if (bottom_y > m.height) bottom_y = m.height;
    workspace_h = bottom_y - content_y;
    if (workspace_h < 1) workspace_h = 1;
    l.top = msv_rect(0, 0, m.width, m.top_bar_h);
    l.progress = msv_rect(0, m.top_bar_h, m.width, m.progress_h);
    l.content = msv_rect(0, content_y, m.width - side_w, workspace_h);
    l.side = msv_rect(m.width - side_w, content_y, side_w, workspace_h);
    l.bottom = msv_rect(0, bottom_y, m.width, m.height - bottom_y);
    return l;
}

MiausoftRgb miausoft_palette_light_root(void)  { return msv_rgb(250, 252, 253); }
MiausoftRgb miausoft_palette_light_frame(void) { return msv_rgb(235, 240, 244); }
MiausoftRgb miausoft_palette_light_ink(void)   { return msv_rgb(19, 22, 27); }
MiausoftRgb miausoft_palette_light_muted(void) { return msv_rgb(116, 126, 139); }
MiausoftRgb miausoft_palette_dark_root(void)   { return msv_rgb(10, 12, 15); }
MiausoftRgb miausoft_palette_dark_frame(void)  { return msv_rgb(19, 24, 30); }
MiausoftRgb miausoft_palette_dark_ink(void)    { return msv_rgb(236, 241, 246); }
MiausoftRgb miausoft_palette_dark_muted(void)  { return msv_rgb(149, 166, 182); }
MiausoftRgb miausoft_palette_accent(void)      { return msv_rgb(133, 13, 55); }

MiausoftRgba miausoft_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    MiausoftRgba c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

MiausoftColorF miausoft_color_f(float r, float g, float b, float a) {
    MiausoftColorF c;
    c.r = msv_clamp_f(r, 0.0f, 1.0f);
    c.g = msv_clamp_f(g, 0.0f, 1.0f);
    c.b = msv_clamp_f(b, 0.0f, 1.0f);
    c.a = msv_clamp_f(a, 0.0f, 1.0f);
    return c;
}

MiausoftColorF miausoft_color_f_from_rgba(MiausoftRgba color) {
    return miausoft_color_f((float)color.r / 255.0f,
                            (float)color.g / 255.0f,
                            (float)color.b / 255.0f,
                            (float)color.a / 255.0f);
}

MiausoftRgba miausoft_rgba_mix(MiausoftRgba a, MiausoftRgba b, float t) {
    MiausoftRgba out;
    t = msv_clamp_f(t, 0.0f, 1.0f);
    out.r = msv_u8((float)a.r + ((float)b.r - (float)a.r) * t);
    out.g = msv_u8((float)a.g + ((float)b.g - (float)a.g) * t);
    out.b = msv_u8((float)a.b + ((float)b.b - (float)a.b) * t);
    out.a = msv_u8((float)a.a + ((float)b.a - (float)a.a) * t);
    return out;
}

MiausoftRgba miausoft_rgba_over(MiausoftRgba foreground, MiausoftRgba background) {
    MiausoftRgba out;
    const float a = (float)foreground.a / 255.0f;
    out.r = msv_u8((float)foreground.r * a + (float)background.r * (1.0f - a));
    out.g = msv_u8((float)foreground.g * a + (float)background.g * (1.0f - a));
    out.b = msv_u8((float)foreground.b * a + (float)background.b * (1.0f - a));
    out.a = 255u;
    return out;
}

MiausoftVisualTheme miausoft_visual_theme(int light_theme, MiausoftRgba accent) {
    MiausoftVisualTheme t;
    memset(&t, 0, sizeof(t));
    t.light = light_theme ? 1u : 0u;
    t.borderless = 1u;
    if (accent.a == 0u) accent = miausoft_rgba(133, 13, 55, 255);
    t.accent = accent;
    t.transparent = miausoft_rgba(0, 0, 0, 0);
    if (light_theme) {
        t.root = miausoft_rgba(250, 252, 253, 255);
        t.frame = miausoft_rgba(235, 240, 244, 255);
        t.frame_hovered = miausoft_rgba(230, 237, 243, 255);
        t.frame_active = miausoft_rgba(224, 233, 241, 255);
        t.popup = miausoft_rgba(253, 254, 255, 250);
        t.text = miausoft_rgba(19, 22, 27, 255);
        t.muted = miausoft_rgba(116, 126, 139, 255);
        t.topbar_a = miausoft_rgba(239, 246, 252, 255);
        t.topbar_b = miausoft_rgba(248, 251, 254, 255);
        t.progress_track = miausoft_rgba(225, 232, 239, 255);
        t.scrollbar_bg = miausoft_rgba(250, 252, 253, 0);
        t.scrollbar_grab = miausoft_rgba(173, 189, 204, 46);
        t.scrollbar_grab_hovered = miausoft_rgba(138, 158, 179, 71);
        t.scrollbar_grab_active = miausoft_rgba(112, 138, 163, 97);
    } else {
        t.root = miausoft_rgba(10, 12, 15, 255);
        t.frame = miausoft_rgba(19, 24, 30, 255);
        t.frame_hovered = miausoft_rgba(24, 30, 37, 255);
        t.frame_active = miausoft_rgba(30, 37, 44, 255);
        t.popup = miausoft_rgba(11, 14, 17, 250);
        t.text = miausoft_rgba(236, 241, 246, 255);
        t.muted = miausoft_rgba(149, 166, 182, 255);
        t.topbar_a = miausoft_rgba(14, 20, 27, 255);
        t.topbar_b = miausoft_rgba(8, 12, 17, 255);
        t.progress_track = miausoft_rgba(22, 29, 36, 255);
        t.scrollbar_bg = miausoft_rgba(10, 12, 15, 0);
        t.scrollbar_grab = miausoft_rgba(148, 179, 199, 36);
        t.scrollbar_grab_hovered = miausoft_rgba(179, 204, 224, 59);
        t.scrollbar_grab_active = miausoft_rgba(194, 219, 240, 82);
    }
    return t;
}

MiausoftTlalpowaStyleTokens miausoft_tlalpowa_style_tokens(
    float window_width,
    float window_height,
    float control_height,
    float text_line_height,
    int light_theme,
    MiausoftColorF accent) {
    MiausoftTlalpowaStyleTokens t;
    MiausoftRgba accent8;
    const float w = msv_max_f(1.0f, window_width);
    const float h = msv_max_f(1.0f, window_height);
    memset(&t, 0, sizeof(t));
    if (accent.a <= 0.0f)
        accent = miausoft_color_f(133.0f / 255.0f, 13.0f / 255.0f,
                                  55.0f / 255.0f, 1.0f);
    accent8 = miausoft_rgba(msv_u8(accent.r * 255.0f),
                            msv_u8(accent.g * 255.0f),
                            msv_u8(accent.b * 255.0f),
                            msv_u8(accent.a * 255.0f));
    t.theme = miausoft_visual_theme(light_theme, accent8);
    t.control_h = control_height > 0.0f
        ? control_height : msv_max_f(1.0f, h * (float)MIAUSOFT_TLALPOWA_CONTROL_RATIO);
    t.font_px = text_line_height > 0.0f
        ? text_line_height : msv_max_f(1.0f, h * (float)MIAUSOFT_TLALPOWA_FONT_RATIO);
    t.checkbox_side = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N1);
    t.gap = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N5);
    t.pad_tight = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N3);
    t.pad = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N2);
    t.rounding = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N2);
    t.frame_pad_x = msv_max_f(1.0f, t.control_h * (float)MIAUSOFT_PHI_N1);
    t.frame_pad_y = msv_max_f(0.0f, (t.control_h - t.font_px) * 0.5f);
    t.item_inner_x = msv_max_f(1.0f, w * (float)MIAUSOFT_PHI_N13);
    t.item_inner_y = msv_max_f(1.0f, h * (float)MIAUSOFT_PHI_N13);
    t.scrollbar_size = msv_max_f(1.0f, h * (float)MIAUSOFT_PHI_N10);
    t.progress_h = msv_max_f(1.0f, h * (float)MIAUSOFT_TLALPOWA_PROGRESS_RATIO);
    t.row_h = msv_max_f(t.checkbox_side, t.font_px) +
        msv_max_f(2.0f, t.control_h * (float)MIAUSOFT_PHI_N7 *
                           (float)(MIAUSOFT_PHI_N0 + MIAUSOFT_PHI_N1));

    t.button = miausoft_color_f_from_rgba(t.theme.frame);
    t.button_hovered = miausoft_color_f(accent.r, accent.g, accent.b,
                                        light_theme ? 0.12f : 0.20f);
    t.button_active = miausoft_color_f(accent.r, accent.g, accent.b,
                                       light_theme ? 0.20f : 0.32f);
    t.check_mark = accent;
    t.slider_grab = miausoft_color_f(accent.r, accent.g, accent.b, 0.86f);
    t.slider_grab_active = accent;
    t.header = miausoft_color_f(accent.r, accent.g, accent.b,
                                light_theme ? 0.10f : 0.16f);
    t.header_hovered = miausoft_color_f(accent.r, accent.g, accent.b,
                                        light_theme ? 0.16f : 0.24f);
    t.header_active = miausoft_color_f(accent.r, accent.g, accent.b,
                                       light_theme ? 0.22f : 0.34f);
    t.tab = miausoft_color_f_from_rgba(t.theme.frame);
    t.tab_hovered = miausoft_color_f(accent.r, accent.g, accent.b,
                                     light_theme ? 0.16f : 0.24f);
    t.tab_active = miausoft_color_f(accent.r, accent.g, accent.b,
                                    light_theme ? 0.18f : 0.28f);
    t.nav_highlight = miausoft_color_f(accent.r, accent.g, accent.b, 0.46f);
    t.surface_check_mark = miausoft_color_f(accent.r, accent.g, accent.b,
                                            light_theme ? 0.88f : 0.96f);
    t.surface_slider_grab = miausoft_color_f(accent.r, accent.g, accent.b,
                                             light_theme ? 0.78f : 0.86f);
    t.surface_slider_grab_active = miausoft_color_f(accent.r, accent.g, accent.b,
                                                    light_theme ? 0.92f : 1.0f);
    return t;
}

MiausoftVisualComponentStyle miausoft_visual_component_style(
    const MiausoftVisualTheme* theme,
    MiausoftTlalpowaTemplateKind kind,
    uint32_t state,
    MiausoftRgba color,
    int extent) {
    MiausoftVisualComponentStyle s;
    MiausoftRgba accent;
    const int h = extent < 1 ? 1 : extent;
    const int hot = (state & MIAUSOFT_VISUAL_STATE_HOT) != 0u;
    const int pressed = (state & MIAUSOFT_VISUAL_STATE_PRESSED) != 0u;
    const int active = (state & MIAUSOFT_VISUAL_STATE_ACTIVE) != 0u;
    const int disabled = (state & MIAUSOFT_VISUAL_STATE_DISABLED) != 0u;
    memset(&s, 0, sizeof(s));
    if (!theme) return s;
    accent = msv_accent_or(theme, color);
    s.fill = theme->root;
    s.fill_secondary = s.fill;
    s.text = disabled ? miausoft_rgba_mix(theme->muted, theme->root, 0.45f) : theme->text;
    s.mark = accent;
    s.shadow = theme->transparent;
    s.radius = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N2, 1));
    s.border_width = 0;

    switch (kind) {
    case MIAUSOFT_TLALPOWA_TOP_BAR:
        s.fill = theme->topbar_a;
        s.fill_secondary = theme->topbar_b;
        s.radius = 0;
        break;
    case MIAUSOFT_TLALPOWA_BUTTON:
    case MIAUSOFT_TLALPOWA_TAB:
    case MIAUSOFT_TLALPOWA_SMALL_BUTTON:
        if ((state & MIAUSOFT_VISUAL_STATE_PRIMARY) != 0u &&
            kind != MIAUSOFT_TLALPOWA_TAB) {
            accent.a = pressed ? 255u : (hot ? 214u : 240u);
            s.fill = accent;
            s.text = miausoft_rgba(255, 255, 255, disabled ? 150u : 255u);
        } else if (active) {
            s.fill = theme->light ? miausoft_rgba(255, 255, 255, 255)
                                  : miausoft_rgba(20, 28, 37, 255);
            s.text = theme->light ? miausoft_rgba(27, 36, 47, 255)
                                  : miausoft_rgba(238, 244, 250, 255);
        } else if (hot || pressed) {
            s.fill = theme->light ? miausoft_rgba(236, 244, 251, 244)
                                  : miausoft_rgba(39, 51, 65, 242);
            s.text = theme->light ? miausoft_rgba(76, 91, 108, 242)
                                  : miausoft_rgba(191, 204, 217, 242);
        } else {
            s.fill = theme->light ? miausoft_rgba(226, 236, 246, 205)
                                  : miausoft_rgba(28, 38, 50, 218);
            s.text = theme->light ? miausoft_rgba(76, 91, 108, 242)
                                  : miausoft_rgba(191, 204, 217, 242);
        }
        s.radius = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N2, 1));
        s.inset = 0;
        s.shadow_offset = kind == MIAUSOFT_TLALPOWA_TAB
            ? msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N12, 1))
            : msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N10, 1));
        s.shadow = theme->light
            ? miausoft_rgba(120, 140, 160,
                (state & MIAUSOFT_VISUAL_STATE_PRIMARY) ? 34u : (active ? 18u : 24u))
            : miausoft_rgba(0, 0, 0,
                (state & MIAUSOFT_VISUAL_STATE_PRIMARY) ? 74u : (active ? 46u : 58u));
        break;
    case MIAUSOFT_TLALPOWA_BOX:
    case MIAUSOFT_TLALPOWA_INPUT:
    case MIAUSOFT_TLALPOWA_SEARCH:
    case MIAUSOFT_TLALPOWA_COMBO:
        s.fill = (state & MIAUSOFT_VISUAL_STATE_FOCUSED) != 0u
            ? theme->frame_active : (hot ? theme->frame_hovered : theme->frame);
        break;
    case MIAUSOFT_TLALPOWA_CHECKBOX: {
        const uint8_t alpha = disabled ? (theme->light ? 41u : 51u)
            : (active ? 240u : ((state & MIAUSOFT_VISUAL_STATE_MIXED) != 0u
                ? 158u : (hot ? 117u : 71u)));
        const float lum = ((float)accent.r * 0.299f + (float)accent.g * 0.587f +
                           (float)accent.b * 0.114f) / 255.0f;
        accent.a = alpha;
        s.fill = accent;
        s.mark = disabled
            ? (theme->light ? miausoft_rgba(78, 88, 98, 116) : miausoft_rgba(220, 228, 236, 116))
            : (lum > 0.60f ? miausoft_rgba(20, 27, 34, 214) : miausoft_rgba(255, 255, 255, 226));
        break;
    }
    case MIAUSOFT_TLALPOWA_SIDE_TREE_ROW:
    case MIAUSOFT_TLALPOWA_SELECTABLE:
    case MIAUSOFT_TLALPOWA_MENU_ITEM:
    case MIAUSOFT_TLALPOWA_COLLAPSING_HEADER:
        s.fill = hot
            ? (theme->light ? miausoft_rgba(40, 72, 96, 10)
                            : miausoft_rgba(220, 235, 250, 12))
            : theme->transparent;
        s.radius = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N4, 1));
        break;
    case MIAUSOFT_TLALPOWA_PROGRESS:
        s.fill = theme->progress_track;
        s.fill_secondary = accent;
        s.radius = 0;
        break;
    case MIAUSOFT_TLALPOWA_SLIDER:
        s.fill = theme->frame;
        accent.a = (state & MIAUSOFT_VISUAL_STATE_ACTIVE) != 0u ? 255u : 219u;
        s.fill_secondary = accent;
        s.radius = msv_max_i(1, miausoft_visual_px(h, MIAUSOFT_PHI_N2, 1));
        break;
    case MIAUSOFT_TLALPOWA_TABLE_ROW:
        s.fill = active
            ? miausoft_rgba_mix(theme->root, accent, theme->light ? 0.12f : 0.18f)
            : ((state & MIAUSOFT_VISUAL_STATE_HOT) != 0u
                ? miausoft_rgba_mix(theme->root, theme->text, theme->light ? 0.035f : 0.055f)
                : theme->root);
        s.radius = 0;
        break;
    case MIAUSOFT_TLALPOWA_TOOLTIP:
    case MIAUSOFT_TLALPOWA_POPUP:
    case MIAUSOFT_TLALPOWA_MODAL:
        s.fill = theme->popup;
        break;
    case MIAUSOFT_TLALPOWA_CARD:
        s.fill = miausoft_rgba_mix(theme->frame, theme->root, 0.24f);
        break;
    case MIAUSOFT_TLALPOWA_SURFACE:
    case MIAUSOFT_TLALPOWA_BOTTOM_BAR:
    case MIAUSOFT_TLALPOWA_SIDE_PANEL:
    case MIAUSOFT_TLALPOWA_SETTINGS_WINDOW:
    case MIAUSOFT_TLALPOWA_SETTINGS_INDEX:
    case MIAUSOFT_TLALPOWA_TABLE:
    case MIAUSOFT_TLALPOWA_TABLE_HEADER:
        s.fill = theme->root;
        s.radius = 0;
        break;
    case MIAUSOFT_TLALPOWA_LABEL:
    case MIAUSOFT_TLALPOWA_BULLET:
    case MIAUSOFT_TLALPOWA_SECTION_HEADER:
    case MIAUSOFT_TLALPOWA_SEPARATOR:
    case MIAUSOFT_TLALPOWA_SCROLL_TRACK:
        s.fill = theme->transparent;
        s.radius = 0;
        break;
    default:
        break;
    }
    return s;
}

MiausoftVisualText miausoft_visual_text_utf8(const char* text, uint16_t flags,
                                             MiausoftVisualFontRole role) {
    MiausoftVisualText t;
    t.data = text;
    t.length = msv_text_length8(text);
    t.flags = flags;
    t.encoding = MIAUSOFT_VISUAL_TEXT_UTF8;
    t.font_role = (uint8_t)role;
    return t;
}

MiausoftVisualText miausoft_visual_text_utf16(const uint16_t* text, uint16_t flags,
                                              MiausoftVisualFontRole role) {
    MiausoftVisualText t;
    t.data = text;
    t.length = msv_text_length16(text);
    t.flags = flags;
    t.encoding = MIAUSOFT_VISUAL_TEXT_UTF16;
    t.font_role = (uint8_t)role;
    return t;
}

void miausoft_visual_draw_list_init(MiausoftVisualDrawList* list,
                                    MiausoftVisualDrawCommand* storage,
                                    uint32_t capacity) {
    if (!list) return;
    list->commands = storage;
    list->capacity = storage ? capacity : 0u;
    list->count = 0u;
    list->overflow = 0u;
}

void miausoft_visual_draw_list_reset(MiausoftVisualDrawList* list) {
    if (!list) return;
    list->count = 0u;
    list->overflow = 0u;
}

int miausoft_visual_draw_list_push(MiausoftVisualDrawList* list,
                                   const MiausoftVisualDrawCommand* command) {
    if (!list || !command || !list->commands || list->count >= list->capacity) {
        if (list) list->overflow = 1u;
        return 0;
    }
    list->commands[list->count++] = *command;
    return 1;
}

int miausoft_visual_emit(MiausoftVisualDrawList* list,
                         const MiausoftVisualTheme* theme,
                         const MiausoftVisualElement* element) {
    MiausoftVisualComponentStyle style;
    MiausoftRectI r;
    MiausoftRectI content;
    MiausoftRgba fill;
    float value;
    int ok = 1;
    int side;
    int arrow;
    int cx;
    int cy;
    if (!list || !theme || !element) return 0;
    r = element->rect;
    if (r.w <= 0 || r.h <= 0) return 1;
    style = miausoft_visual_component_style(
        theme, element->kind, element->state, element->color, r.h);
    content = r;

    switch (element->kind) {
    case MIAUSOFT_TLALPOWA_TOP_BAR:
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_GRADIENT_RECT,
                             r, style.fill, style.fill_secondary, 0);
        break;
    case MIAUSOFT_TLALPOWA_BUTTON:
    case MIAUSOFT_TLALPOWA_SMALL_BUTTON:
        if (style.shadow.a != 0u) {
            MiausoftRectI shadow = r;
            shadow.y += style.shadow_offset;
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, shadow,
                miausoft_rgba_over(style.shadow, theme->root),
                miausoft_rgba_over(style.shadow, theme->root), style.radius);
        }
        fill = style.fill.a < 255u ? miausoft_rgba_over(style.fill, theme->root) : style.fill;
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, r, fill, fill, style.radius);
        if ((element->state & (MIAUSOFT_VISUAL_STATE_HOT | MIAUSOFT_VISUAL_STATE_PRESSED)) != 0u) {
            MiausoftRgba wash = (element->state & MIAUSOFT_VISUAL_STATE_PRIMARY) != 0u
                ? miausoft_rgba(255, 255, 255, 22)
                : theme->accent;
            if ((element->state & MIAUSOFT_VISUAL_STATE_PRIMARY) == 0u)
                wash.a = theme->light ? 20u : 33u;
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, r,
                miausoft_rgba_over(wash, fill), miausoft_rgba_over(wash, fill), style.radius);
        }
        ok &= msv_push_text(list, r, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_TAB:
        if ((element->state & MIAUSOFT_VISUAL_STATE_ACTIVE) != 0u && style.shadow.a != 0u) {
            MiausoftRectI shadow = r;
            shadow.y += style.shadow_offset;
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_TOP_RECT, shadow,
                miausoft_rgba_over(style.shadow, theme->root),
                miausoft_rgba_over(style.shadow, theme->root), style.radius);
        }
        fill = style.fill.a < 255u ? miausoft_rgba_over(style.fill, theme->root) : style.fill;
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_TOP_RECT, r, fill, fill, style.radius);
        if ((element->state & MIAUSOFT_VISUAL_STATE_ACTIVE) != 0u) {
            MiausoftRectI seal = r;
            seal.y = r.y + r.h - msv_max_i(1, miausoft_visual_px(r.h, MIAUSOFT_PHI_N10, 1));
            seal.h = r.y + r.h - seal.y;
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT, seal, fill, fill, 0);
        }
        ok &= msv_push_text(list, r, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_BOX:
    case MIAUSOFT_TLALPOWA_INPUT:
    case MIAUSOFT_TLALPOWA_SEARCH:
    case MIAUSOFT_TLALPOWA_TOOLTIP:
    case MIAUSOFT_TLALPOWA_POPUP:
    case MIAUSOFT_TLALPOWA_MODAL:
    case MIAUSOFT_TLALPOWA_CARD:
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                             r, style.fill, style.fill, style.radius);
        content = msv_inset(r, msv_max_i(1, miausoft_visual_px(r.h, MIAUSOFT_PHI_N2, 1)), 0);
        ok &= msv_push_text(list, content, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_COMBO:
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                             r, style.fill, style.fill, style.radius);
        side = r.h;
        arrow = msv_max_i(4, miausoft_visual_px(side, MIAUSOFT_PHI_N3, 1));
        cx = r.x + r.w - side / 2;
        cy = r.y + r.h / 2;
        ok &= msv_push_triangle(list, cx - arrow / 2, cy - arrow / 4,
            cx + arrow / 2, cy - arrow / 4, cx, cy + arrow / 3, style.text);
        content = msv_inset(r, msv_max_i(1, miausoft_visual_px(r.h, MIAUSOFT_PHI_N2, 1)), 0);
        content.w -= side;
        ok &= msv_push_text(list, content, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_CHECKBOX:
        side = msv_min_i(r.w, r.h);
        content = msv_rect(r.x, r.y + (r.h - side) / 2, side, side);
        fill = miausoft_rgba_over(style.fill, theme->root);
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                             content, fill, fill, style.radius);
        if ((element->state & MIAUSOFT_VISUAL_STATE_HOT) != 0u &&
            (element->state & MIAUSOFT_VISUAL_STATE_DISABLED) == 0u) {
            MiausoftRgba wash = miausoft_rgba(255, 255, 255, theme->light ? 28u : 16u);
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, content,
                                 miausoft_rgba_over(wash, fill),
                                 miausoft_rgba_over(wash, fill), style.radius);
        }
        if ((element->state & (MIAUSOFT_VISUAL_STATE_ACTIVE | MIAUSOFT_VISUAL_STATE_MIXED)) != 0u) {
            MiausoftRectI mark_box = content;
            const int inset = msv_max_i(1, miausoft_visual_px(side, MIAUSOFT_PHI_N3, 1));
            mark_box.x += inset;
            mark_box.y += inset;
            mark_box.w -= inset * 2;
            mark_box.h -= inset * 2;
            if ((element->state & MIAUSOFT_VISUAL_STATE_MIXED) != 0u &&
                (element->state & MIAUSOFT_VISUAL_STATE_ACTIVE) == 0u) {
                mark_box.y = content.y + content.h / 2 - msv_max_i(1, side / 14);
                mark_box.h = msv_max_i(1, side / 7);
                ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                                     mark_box, miausoft_rgba_over(style.mark, fill),
                                     miausoft_rgba_over(style.mark, fill),
                                     msv_max_i(1, mark_box.h / 2));
            } else if (mark_box.w > 0 && mark_box.h > 0) {
                ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                                     mark_box, miausoft_rgba_over(style.mark, fill),
                                     miausoft_rgba_over(style.mark, fill),
                                     msv_max_i(1, style.radius / 2));
            }
        }
        if (element->text.data && element->text.length != 0u) {
            const int gap = msv_max_i(1, miausoft_visual_px(side, MIAUSOFT_PHI_N3, 1));
            MiausoftRectI label = r;
            label.x += side + gap;
            label.w -= side + gap;
            ok &= msv_push_text(list, label, style.text, element->text);
        }
        break;
    case MIAUSOFT_TLALPOWA_SIDE_TREE_ROW:
    case MIAUSOFT_TLALPOWA_COLLAPSING_HEADER:
        if (style.fill.a != 0u) {
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, r,
                miausoft_rgba_over(style.fill, theme->root),
                miausoft_rgba_over(style.fill, theme->root), style.radius);
        }
        side = r.h;
        arrow = msv_max_i(5, miausoft_visual_px(side, MIAUSOFT_PHI_N1, 1));
        cx = r.x + arrow / 2 + element->nesting * msv_max_i(1, miausoft_visual_px(side, MIAUSOFT_PHI_N2, 1));
        cy = r.y + r.h / 2;
        if ((element->state & MIAUSOFT_VISUAL_STATE_EXPANDED) != 0u) {
            ok &= msv_push_triangle(list, cx - arrow / 2, cy - arrow / 4,
                cx + arrow / 2, cy - arrow / 4, cx, cy + arrow / 3, style.text);
        } else {
            ok &= msv_push_triangle(list, cx - arrow / 4, cy - arrow / 2,
                cx - arrow / 4, cy + arrow / 2, cx + arrow / 3, cy, style.text);
        }
        content = r;
        content.x = cx + arrow;
        content.w = r.x + r.w - content.x;
        ok &= msv_push_text(list, content, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_SELECTABLE:
    case MIAUSOFT_TLALPOWA_MENU_ITEM:
        if (style.fill.a != 0u) {
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT, r,
                miausoft_rgba_over(style.fill, theme->root),
                miausoft_rgba_over(style.fill, theme->root), style.radius);
        }
        ok &= msv_push_text(list,
            msv_inset(r, msv_max_i(1, miausoft_visual_px(r.h, MIAUSOFT_PHI_N2, 1)), 0),
            style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_PROGRESS:
        value = msv_clamp_f(element->value, 0.0f, 1.0f);
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT, r, style.fill, style.fill, 0);
        if ((element->state & MIAUSOFT_VISUAL_STATE_MIXED) != 0u) {
            MiausoftRgba secondary = element->color_secondary.a
                ? element->color_secondary
                : miausoft_rgba_mix(style.fill, style.fill_secondary, 0.58f);
            MiausoftRectI secondary_rect = r;
            secondary_rect.w = (int)((float)r.w *
                msv_clamp_f(element->value_secondary, 0.0f, 1.0f) + 0.5f);
            if (secondary_rect.w > 0)
                ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT,
                                     secondary_rect, secondary, secondary, 0);
        }
        content = r;
        content.w = (int)((float)r.w * value + 0.5f);
        if (content.w > 0)
            ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT,
                                 content, style.fill_secondary, style.fill_secondary, 0);
        break;
    case MIAUSOFT_TLALPOWA_SLIDER:
        value = msv_clamp_f(element->value, 0.0f, 1.0f);
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                             r, style.fill, style.fill, style.radius);
        side = msv_max_i(1, miausoft_visual_px(r.h, MIAUSOFT_PHI_N1, 1));
        cx = r.x + side / 2 +
             (int)((float)msv_max_i(0, r.w - side) * value + 0.5f);
        content = msv_rect(cx - side / 2,
                           r.y + (r.h - side) / 2,
                           side, side);
        fill = style.fill_secondary.a < 255u
            ? miausoft_rgba_over(style.fill_secondary, style.fill)
            : style.fill_secondary;
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
                             content, fill, fill, style.radius);
        break;
    case MIAUSOFT_TLALPOWA_LABEL:
    case MIAUSOFT_TLALPOWA_SECTION_HEADER:
        ok &= msv_push_text(list, r, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_BULLET:
        side = msv_max_i(3, miausoft_visual_px(r.h, MIAUSOFT_PHI_N3, 1));
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_CIRCLE,
            msv_rect(r.x, r.y + (r.h - side) / 2, side, side),
            style.text, style.text, side / 2);
        content = r;
        content.x += side * 2;
        content.w -= side * 2;
        ok &= msv_push_text(list, content, style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_TABLE_ROW:
        if ((element->row_index & 1) != 0 &&
            (element->state & MIAUSOFT_VISUAL_STATE_ACTIVE) == 0u) {
            style.fill = miausoft_rgba_mix(theme->frame, theme->root, 0.62f);
        }
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT, r, style.fill, style.fill, 0);
        ok &= msv_push_text(list,
            msv_inset(r, msv_max_i(1, miausoft_visual_px(r.w, MIAUSOFT_PHI_N11, 1)), 0),
            style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_SURFACE:
    case MIAUSOFT_TLALPOWA_BOTTOM_BAR:
    case MIAUSOFT_TLALPOWA_SIDE_PANEL:
    case MIAUSOFT_TLALPOWA_SETTINGS_WINDOW:
    case MIAUSOFT_TLALPOWA_SETTINGS_INDEX:
    case MIAUSOFT_TLALPOWA_TABLE:
    case MIAUSOFT_TLALPOWA_TABLE_HEADER:
        ok &= msv_push_shape(list, MIAUSOFT_VISUAL_DRAW_RECT, r, style.fill, style.fill, 0);
        ok &= msv_push_text(list,
            msv_inset(r, msv_max_i(1, miausoft_visual_px(r.w, MIAUSOFT_PHI_N11, 1)), 0),
            style.text, element->text);
        break;
    case MIAUSOFT_TLALPOWA_SEPARATOR:
    case MIAUSOFT_TLALPOWA_SCROLL_TRACK:
        /* Tlalpowa los vuelve invisibles deliberadamente. */
        break;
    default:
        break;
    }
    return ok && list->overflow == 0u;
}
