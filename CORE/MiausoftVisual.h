#ifndef MIAUSOFT_VISUAL_H_INCLUDED
#define MIAUSOFT_VISUAL_H_INCLUDED

/*
 * Miausoft Visual: Tlalpowa es la única ley visual.
 *
 * C11 puro, sin heap, sin estado global, sin Win32, ImGui, OpenGL ni C++.
 * Las aplicaciones sólo entregan contenido, estado y rectángulos. Colores,
 * opacidades, redondez, espaciado y ausencia de contornos pertenecen a estas
 * plantillas y no son parámetros públicos.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIAUSOFT_VISUAL_REVISION 2026062201

#define MIAUSOFT_PHI     1.61803398874989484820
#define MIAUSOFT_PHI_N0  1.00000000000000000000
#define MIAUSOFT_PHI_N1  0.61803398874989484820
#define MIAUSOFT_PHI_N2  0.38196601125010515180
#define MIAUSOFT_PHI_N3  0.23606797749978969641
#define MIAUSOFT_PHI_N4  0.14589803375031545539
#define MIAUSOFT_PHI_N5  0.09016994374947424102
#define MIAUSOFT_PHI_N6  0.05572809000084121436
#define MIAUSOFT_PHI_N7  0.03444185374863302666
#define MIAUSOFT_PHI_N8  0.02128623625220818770
#define MIAUSOFT_PHI_N9  0.01315561749642483896
#define MIAUSOFT_PHI_N10 0.00813061875578334875
#define MIAUSOFT_PHI_N11 0.00502499874064149021
#define MIAUSOFT_PHI_N12 0.00310562001514185854
#define MIAUSOFT_PHI_N13 0.00191937872549963167
#define MIAUSOFT_PHI_N14 0.00118624128964222687
#define MIAUSOFT_PHI_N15 0.00073313743585740480
#define MIAUSOFT_PHI_N16 0.00045310385378482207
#define MIAUSOFT_PHI_N17 0.00028003358207258273
#define MIAUSOFT_PHI_N18 0.00017307027171223935
#define MIAUSOFT_PHI_N19 0.00010696331036034338
#define MIAUSOFT_PHI_N20 0.00006610696135189597

/* Relaciones observadas directamente en Tlalpowa.cpp. */
#define MIAUSOFT_TLALPOWA_TOP_BAR_RATIO       MIAUSOFT_PHI_N7
#define MIAUSOFT_TLALPOWA_BOTTOM_BAR_RATIO    MIAUSOFT_PHI_N7
#define MIAUSOFT_TLALPOWA_CONTROL_RATIO       MIAUSOFT_PHI_N7
#define MIAUSOFT_TLALPOWA_FONT_RATIO          MIAUSOFT_PHI_N9
#define MIAUSOFT_TLALPOWA_SIDE_MIN_RATIO      MIAUSOFT_PHI_N4
#define MIAUSOFT_TLALPOWA_SIDE_DEFAULT_RATIO  MIAUSOFT_PHI_N4
#define MIAUSOFT_TLALPOWA_SIDE_MAX_RATIO      MIAUSOFT_PHI_N3
#define MIAUSOFT_TLALPOWA_PROGRESS_RATIO      MIAUSOFT_PHI_N11
#define MIAUSOFT_TLALPOWA_SETTINGS_INDEX_RATIO \
    ((MIAUSOFT_PHI_N4 + MIAUSOFT_PHI_N5) * 0.5)

#define MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS      0x0001u
#define MIAUSOFT_TLALPOWA_TEMPLATE_FLAT            0x0002u
#define MIAUSOFT_TLALPOWA_TEMPLATE_TOP_ROUNDED     0x0004u
#define MIAUSOFT_TLALPOWA_TEMPLATE_TRANSLUCENT     0x0008u
#define MIAUSOFT_TLALPOWA_TEMPLATE_INVISIBLE       0x0010u
#define MIAUSOFT_TLALPOWA_TEMPLATE_NESTABLE        0x0020u

#define MIAUSOFT_TLALPOWA_PARAM_CONTENT            0x0001u
#define MIAUSOFT_TLALPOWA_PARAM_COLOR              0x0002u
#define MIAUSOFT_TLALPOWA_PARAM_STATE              0x0004u
#define MIAUSOFT_TLALPOWA_PARAM_VALUE              0x0008u
#define MIAUSOFT_TLALPOWA_PARAM_NESTING            0x0010u
#define MIAUSOFT_TLALPOWA_PARAM_ROW_INDEX          0x0020u

#define MIAUSOFT_VISUAL_STATE_ACTIVE   0x0001u
#define MIAUSOFT_VISUAL_STATE_HOT      0x0002u
#define MIAUSOFT_VISUAL_STATE_PRESSED  0x0004u
#define MIAUSOFT_VISUAL_STATE_DISABLED 0x0008u
#define MIAUSOFT_VISUAL_STATE_FOCUSED  0x0010u
#define MIAUSOFT_VISUAL_STATE_EXPANDED 0x0020u
#define MIAUSOFT_VISUAL_STATE_MIXED    0x0040u
#define MIAUSOFT_VISUAL_STATE_PRIMARY  0x0080u
#define MIAUSOFT_VISUAL_STATE_SELECTED MIAUSOFT_VISUAL_STATE_ACTIVE

#define MIAUSOFT_VISUAL_TEXT_LEFT      0x0001u
#define MIAUSOFT_VISUAL_TEXT_CENTER    0x0002u
#define MIAUSOFT_VISUAL_TEXT_RIGHT     0x0004u
#define MIAUSOFT_VISUAL_TEXT_TOP       0x0010u
#define MIAUSOFT_VISUAL_TEXT_MIDDLE    0x0020u
#define MIAUSOFT_VISUAL_TEXT_BOTTOM    0x0040u
#define MIAUSOFT_VISUAL_TEXT_ELLIPSIS  0x0100u
#define MIAUSOFT_VISUAL_TEXT_WRAP      0x0200u

typedef struct MiausoftRgb {
    uint8_t r, g, b;
} MiausoftRgb;

typedef struct MiausoftRgba {
    uint8_t r, g, b, a;
} MiausoftRgba;

typedef struct MiausoftColorF {
    float r, g, b, a;
} MiausoftColorF;

typedef struct MiausoftRectI {
    int x, y, w, h;
} MiausoftRectI;

/*
 * Parámetro dimensional público. Cada término es phi^-N:
 *   {1}   = N1
 *   {1,2} = N1 + N2
 * No se admiten píxeles arbitrarios como parte del diseño.
 */
typedef struct MiausoftPhiExpr {
    uint8_t terms[3];
    uint8_t count;
    int16_t min_px;
    int16_t max_px;
} MiausoftPhiExpr;

/*
 * Sólo sobreviven elementos visibles que Tlalpowa realmente utiliza.
 * No hay Sheet, Hero, Radio, Toggle ni Badge: eran abstracciones externas.
 */
typedef enum MiausoftTlalpowaTemplateKind {
    MIAUSOFT_TLALPOWA_NONE = 0,
    MIAUSOFT_TLALPOWA_SURFACE,
    MIAUSOFT_TLALPOWA_TOP_BAR,
    MIAUSOFT_TLALPOWA_BOTTOM_BAR,
    MIAUSOFT_TLALPOWA_BUTTON,
    MIAUSOFT_TLALPOWA_TAB,
    MIAUSOFT_TLALPOWA_SMALL_BUTTON,
    MIAUSOFT_TLALPOWA_BOX,
    MIAUSOFT_TLALPOWA_INPUT,
    MIAUSOFT_TLALPOWA_SEARCH,
    MIAUSOFT_TLALPOWA_CHECKBOX,
    MIAUSOFT_TLALPOWA_SIDE_PANEL,
    MIAUSOFT_TLALPOWA_SIDE_TREE_ROW,
    MIAUSOFT_TLALPOWA_PROGRESS,
    MIAUSOFT_TLALPOWA_SETTINGS_WINDOW,
    MIAUSOFT_TLALPOWA_SETTINGS_INDEX,
    MIAUSOFT_TLALPOWA_SLIDER,
    MIAUSOFT_TLALPOWA_TABLE,
    MIAUSOFT_TLALPOWA_TABLE_HEADER,
    MIAUSOFT_TLALPOWA_TABLE_ROW,
    MIAUSOFT_TLALPOWA_TOOLTIP,
    MIAUSOFT_TLALPOWA_POPUP,
    MIAUSOFT_TLALPOWA_MODAL,
    MIAUSOFT_TLALPOWA_COMBO,
    MIAUSOFT_TLALPOWA_SELECTABLE,
    MIAUSOFT_TLALPOWA_MENU_ITEM,
    MIAUSOFT_TLALPOWA_COLLAPSING_HEADER,
    MIAUSOFT_TLALPOWA_CARD,
    MIAUSOFT_TLALPOWA_LABEL,
    MIAUSOFT_TLALPOWA_BULLET,
    MIAUSOFT_TLALPOWA_SECTION_HEADER,
    MIAUSOFT_TLALPOWA_SEPARATOR,
    MIAUSOFT_TLALPOWA_SCROLL_TRACK,
    MIAUSOFT_TLALPOWA_LAST = MIAUSOFT_TLALPOWA_SCROLL_TRACK
} MiausoftTlalpowaTemplateKind;

typedef struct MiausoftTlalpowaTemplate {
    MiausoftPhiExpr width;
    MiausoftPhiExpr height;
    MiausoftPhiExpr padding_x;
    MiausoftPhiExpr padding_y;
    MiausoftPhiExpr gap;
    MiausoftPhiExpr rounding;
    uint32_t flags;
    uint32_t adjustable;
} MiausoftTlalpowaTemplate;

typedef struct MiausoftTlalpowaMetrics {
    int width;
    int height;
    int top_bar_h;
    int bottom_bar_h;
    int control_h;
    int font_px;
    int checkbox_side;
    int row_h;
    int progress_h;
    int gap;
    int pad_tight;
    int pad;
    int rounding;
    int side_w;
    int settings_index_w;
} MiausoftTlalpowaMetrics;

typedef struct MiausoftTlalpowaLayout {
    MiausoftRectI top;
    MiausoftRectI progress;
    MiausoftRectI content;
    MiausoftRectI side;
    MiausoftRectI bottom;
} MiausoftTlalpowaLayout;

typedef struct MiausoftVisualTheme {
    MiausoftRgba root;
    MiausoftRgba frame;
    MiausoftRgba frame_hovered;
    MiausoftRgba frame_active;
    MiausoftRgba popup;
    MiausoftRgba text;
    MiausoftRgba muted;
    MiausoftRgba accent;
    MiausoftRgba topbar_a;
    MiausoftRgba topbar_b;
    MiausoftRgba progress_track;
    MiausoftRgba scrollbar_bg;
    MiausoftRgba scrollbar_grab;
    MiausoftRgba scrollbar_grab_hovered;
    MiausoftRgba scrollbar_grab_active;
    MiausoftRgba transparent;
    uint8_t light;
    uint8_t borderless;
    uint16_t reserved;
} MiausoftVisualTheme;

/*
 * Tokens literales del tema global aplicado por Tlalpowa a ImGui.
 * Los valores float conservan exactamente opacidades como 0.12 y 0.86;
 * el adaptador de cada backend sólo traduce estos datos, no los redefine.
 */
typedef struct MiausoftTlalpowaStyleTokens {
    MiausoftVisualTheme theme;
    float control_h;
    float font_px;
    float checkbox_side;
    float row_h;
    float progress_h;
    float pad_tight;
    float pad;
    float gap;
    float frame_pad_x;
    float frame_pad_y;
    float item_inner_x;
    float item_inner_y;
    float rounding;
    float scrollbar_size;
    float window_border_size;
    float child_border_size;
    float popup_border_size;
    float frame_border_size;
    float tab_border_size;
    MiausoftColorF button;
    MiausoftColorF button_hovered;
    MiausoftColorF button_active;
    MiausoftColorF check_mark;
    MiausoftColorF slider_grab;
    MiausoftColorF slider_grab_active;
    MiausoftColorF header;
    MiausoftColorF header_hovered;
    MiausoftColorF header_active;
    MiausoftColorF tab;
    MiausoftColorF tab_hovered;
    MiausoftColorF tab_active;
    MiausoftColorF nav_highlight;
    MiausoftColorF surface_check_mark;
    MiausoftColorF surface_slider_grab;
    MiausoftColorF surface_slider_grab_active;
} MiausoftTlalpowaStyleTokens;

typedef struct MiausoftVisualComponentStyle {
    MiausoftRgba fill;
    MiausoftRgba fill_secondary;
    MiausoftRgba text;
    MiausoftRgba mark;
    MiausoftRgba shadow;
    int radius;
    int inset;
    int shadow_offset;
    int border_width; /* Invariante: siempre cero. */
} MiausoftVisualComponentStyle;

typedef enum MiausoftVisualTextEncoding {
    MIAUSOFT_VISUAL_TEXT_NONE = 0,
    MIAUSOFT_VISUAL_TEXT_UTF8,
    MIAUSOFT_VISUAL_TEXT_UTF16
} MiausoftVisualTextEncoding;

typedef enum MiausoftVisualFontRole {
    MIAUSOFT_VISUAL_FONT_BODY = 0,
    MIAUSOFT_VISUAL_FONT_STRONG,
    MIAUSOFT_VISUAL_FONT_TITLE,
    MIAUSOFT_VISUAL_FONT_SMALL
} MiausoftVisualFontRole;

typedef struct MiausoftVisualText {
    const void* data;
    uint32_t length;
    uint16_t flags;
    uint8_t encoding;
    uint8_t font_role;
} MiausoftVisualText;

typedef enum MiausoftVisualDrawCommandKind {
    MIAUSOFT_VISUAL_DRAW_NONE = 0,
    MIAUSOFT_VISUAL_DRAW_RECT,
    MIAUSOFT_VISUAL_DRAW_ROUND_RECT,
    MIAUSOFT_VISUAL_DRAW_ROUND_TOP_RECT,
    MIAUSOFT_VISUAL_DRAW_GRADIENT_RECT,
    MIAUSOFT_VISUAL_DRAW_TEXT,
    MIAUSOFT_VISUAL_DRAW_LINE,
    MIAUSOFT_VISUAL_DRAW_TRIANGLE,
    MIAUSOFT_VISUAL_DRAW_CIRCLE
} MiausoftVisualDrawCommandKind;

typedef struct MiausoftVisualDrawCommand {
    MiausoftRectI rect;
    MiausoftRgba color;
    MiausoftRgba color2;
    MiausoftVisualText text;
    int x2, y2, x3, y3;
    int radius;
    int thickness;
    uint8_t kind;
    uint8_t reserved[3];
} MiausoftVisualDrawCommand;

typedef struct MiausoftVisualDrawList {
    MiausoftVisualDrawCommand* commands;
    uint32_t capacity;
    uint32_t count;
    uint32_t overflow;
} MiausoftVisualDrawList;

typedef struct MiausoftVisualElement {
    MiausoftTlalpowaTemplateKind kind;
    MiausoftRectI rect;
    MiausoftVisualText text;
    MiausoftRgba color; /* a=0 usa el acento del tema. */
    MiausoftRgba color_secondary;
    uint32_t state;
    float value;
    float value_secondary;
    int nesting;
    int row_index;
} MiausoftVisualElement;

int miausoft_visual_revision(void);
int miausoft_visual_validate(void);
double miausoft_phi_ratio(unsigned int power);
MiausoftPhiExpr miausoft_phi_expr(unsigned int n1, unsigned int n2, unsigned int n3);
MiausoftPhiExpr miausoft_phi_expr_clamped(unsigned int n1, unsigned int n2, unsigned int n3, int min_px, int max_px);
double miausoft_phi_expr_ratio(MiausoftPhiExpr expr);
int miausoft_phi_expr_px(int extent, MiausoftPhiExpr expr);
int miausoft_phi_scale_px(int extent, unsigned int power, int minimum);
int miausoft_visual_px(int extent, double ratio, int minimum);

MiausoftTlalpowaTemplate miausoft_tlalpowa_template(MiausoftTlalpowaTemplateKind kind);
const char* miausoft_tlalpowa_template_name(MiausoftTlalpowaTemplateKind kind);
int miausoft_tlalpowa_template_validate(MiausoftTlalpowaTemplateKind kind);
MiausoftTlalpowaMetrics miausoft_tlalpowa_metrics(int width, int height);
MiausoftTlalpowaLayout miausoft_tlalpowa_layout(int width, int height, double side_ratio);

MiausoftRgb miausoft_palette_light_root(void);
MiausoftRgb miausoft_palette_light_frame(void);
MiausoftRgb miausoft_palette_light_ink(void);
MiausoftRgb miausoft_palette_light_muted(void);
MiausoftRgb miausoft_palette_dark_root(void);
MiausoftRgb miausoft_palette_dark_frame(void);
MiausoftRgb miausoft_palette_dark_ink(void);
MiausoftRgb miausoft_palette_dark_muted(void);
MiausoftRgb miausoft_palette_accent(void);

MiausoftRgba miausoft_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
MiausoftColorF miausoft_color_f(float r, float g, float b, float a);
MiausoftColorF miausoft_color_f_from_rgba(MiausoftRgba color);
MiausoftRgba miausoft_rgba_mix(MiausoftRgba a, MiausoftRgba b, float t);
MiausoftRgba miausoft_rgba_over(MiausoftRgba foreground, MiausoftRgba background);
MiausoftVisualTheme miausoft_visual_theme(int light_theme, MiausoftRgba accent);
MiausoftTlalpowaStyleTokens miausoft_tlalpowa_style_tokens(
    float window_width,
    float window_height,
    float control_height,
    float text_line_height,
    int light_theme,
    MiausoftColorF accent);
MiausoftVisualComponentStyle miausoft_visual_component_style(
    const MiausoftVisualTheme* theme,
    MiausoftTlalpowaTemplateKind kind,
    uint32_t state,
    MiausoftRgba color,
    int extent);

MiausoftVisualText miausoft_visual_text_utf8(const char* text, uint16_t flags, MiausoftVisualFontRole role);
MiausoftVisualText miausoft_visual_text_utf16(const uint16_t* text, uint16_t flags, MiausoftVisualFontRole role);
void miausoft_visual_draw_list_init(MiausoftVisualDrawList* list, MiausoftVisualDrawCommand* storage, uint32_t capacity);
void miausoft_visual_draw_list_reset(MiausoftVisualDrawList* list);
int miausoft_visual_draw_list_push(MiausoftVisualDrawList* list, const MiausoftVisualDrawCommand* command);
int miausoft_visual_emit(MiausoftVisualDrawList* list, const MiausoftVisualTheme* theme, const MiausoftVisualElement* element);

#ifdef __cplusplus
}
#endif

#endif
