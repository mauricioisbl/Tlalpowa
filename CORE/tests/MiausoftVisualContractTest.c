#include "MiausoftVisual.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int require_true(int condition, const char* message) {
    if (condition) return 1;
    fprintf(stderr, "MiausoftVisual: %s\n", message);
    return 0;
}

int main(void) {
    MiausoftVisualTheme theme;
    MiausoftTlalpowaMetrics metrics;
    MiausoftTlalpowaLayout layout;
    MiausoftVisualDrawCommand storage[64];
    MiausoftVisualDrawList list;
    MiausoftVisualElement element;
    MiausoftVisualComponentStyle idle_check;
    MiausoftVisualComponentStyle active_check;
    MiausoftTlalpowaStyleTokens tokens;
    MiausoftPhiExpr sum;
    MiausoftTlalpowaTemplate surface;
    int ok = 1;
    int kind;
    static const uint32_t states[] = {
        0u,
        MIAUSOFT_VISUAL_STATE_HOT,
        MIAUSOFT_VISUAL_STATE_PRESSED,
        MIAUSOFT_VISUAL_STATE_ACTIVE,
        MIAUSOFT_VISUAL_STATE_DISABLED,
        MIAUSOFT_VISUAL_STATE_FOCUSED,
        MIAUSOFT_VISUAL_STATE_ACTIVE | MIAUSOFT_VISUAL_STATE_HOT,
        MIAUSOFT_VISUAL_STATE_PRIMARY | MIAUSOFT_VISUAL_STATE_HOT,
        MIAUSOFT_VISUAL_STATE_MIXED
    };

    ok &= require_true(miausoft_visual_validate() != 0, "contrato invalido");
    ok &= require_true(miausoft_visual_revision() == MIAUSOFT_VISUAL_REVISION,
                       "revision incoherente");

    sum = miausoft_phi_expr(1, 2, 0);
    ok &= require_true(fabs(miausoft_phi_expr_ratio(sum) - 1.0) < 0.000000001,
                       "N1 + N2 debe ser N0");
    ok &= require_true(miausoft_phi_expr_px(1000, sum) == 1000,
                       "la suma aurea debe resolver en pixeles");

    surface = miausoft_tlalpowa_template(MIAUSOFT_TLALPOWA_SURFACE);
    ok &= require_true(surface.height.count == 0 && surface.rounding.count == 0,
                       "una superficie no inventa alto ni redondez");
    ok &= require_true((surface.flags & (MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS |
                                        MIAUSOFT_TLALPOWA_TEMPLATE_FLAT)) ==
                       (MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS |
                        MIAUSOFT_TLALPOWA_TEMPLATE_FLAT),
                       "la superficie debe ser plana y sin contorno");

    theme = miausoft_visual_theme(1, miausoft_rgba(133, 13, 55, 255));
    metrics = miausoft_tlalpowa_metrics(1920, 1080);
    layout = miausoft_tlalpowa_layout(1920, 1080,
        MIAUSOFT_TLALPOWA_SIDE_DEFAULT_RATIO);
    ok &= require_true(theme.borderless == 1, "Tlalpowa debe ser borderless");
    ok &= require_true(metrics.top_bar_h ==
        miausoft_visual_px(1080, MIAUSOFT_PHI_N7, 1),
        "barra superior distinta de N7");
    ok &= require_true(metrics.bottom_bar_h ==
        miausoft_visual_px(1080, MIAUSOFT_PHI_N7, 1),
        "barra inferior distinta de N7");
    ok &= require_true(metrics.control_h ==
        miausoft_visual_px(1080, MIAUSOFT_PHI_N7, 1),
        "control distinto de N7");
    ok &= require_true(layout.side.x == layout.content.x + layout.content.w &&
                       layout.side.w == metrics.side_w,
                       "el panel lateral debe ser el panel derecho de Tlalpowa");
    ok &= require_true(layout.progress.y == layout.top.y + layout.top.h &&
                       layout.progress.h == metrics.progress_h,
                       "la barra de progreso debe vivir debajo del header");
    ok &= require_true(layout.content.y == layout.progress.y + layout.progress.h &&
                       layout.side.y == layout.content.y,
                       "el contenido no debe solaparse con header ni progreso");
    ok &= require_true(layout.top.y == 0 &&
                       layout.bottom.y + layout.bottom.h == 1080,
                       "las barras deben cerrar el viewport");

    tokens = miausoft_tlalpowa_style_tokens(
        1920.0f, 1080.0f, (float)metrics.control_h, 14.0f, 1,
        miausoft_color_f(0.10f, 0.48f, 0.86f, 1.0f));
    ok &= require_true(tokens.window_border_size == 0.0f &&
                       tokens.child_border_size == 0.0f &&
                       tokens.popup_border_size == 0.0f &&
                       tokens.frame_border_size == 0.0f &&
                       tokens.tab_border_size == 0.0f,
                       "los tokens globales introdujeron contorno");
    ok &= require_true(fabs(tokens.button_hovered.a - 0.12f) < 0.000001f &&
                       fabs(tokens.slider_grab.a - 0.86f) < 0.000001f &&
                       fabs(tokens.nav_highlight.a - 0.46f) < 0.000001f,
                       "las opacidades globales ya no son las de Tlalpowa");
    ok &= require_true(fabs(tokens.rounding -
                       tokens.control_h * (float)MIAUSOFT_PHI_N2) < 0.0001f,
                       "la redondez no es control por N2");
    ok &= require_true(fabs(tokens.checkbox_side -
                       tokens.control_h * (float)MIAUSOFT_PHI_N1) < 0.0001f,
                       "la casilla no es control por N1");

    idle_check = miausoft_visual_component_style(
        &theme, MIAUSOFT_TLALPOWA_CHECKBOX, 0u, theme.accent,
        metrics.checkbox_side);
    active_check = miausoft_visual_component_style(
        &theme, MIAUSOFT_TLALPOWA_CHECKBOX, MIAUSOFT_VISUAL_STATE_ACTIVE,
        theme.accent, metrics.checkbox_side);
    ok &= require_true(active_check.fill.a > idle_check.fill.a,
                       "la casilla activa debe ganar opacidad");
    ok &= require_true(idle_check.border_width == 0 &&
                       active_check.border_width == 0,
                       "la casilla no admite contorno");

    for (kind = MIAUSOFT_TLALPOWA_SURFACE;
         kind <= MIAUSOFT_TLALPOWA_LAST; ++kind) {
        MiausoftTlalpowaTemplate descriptor;
        const char* name;
        descriptor = miausoft_tlalpowa_template(
            (MiausoftTlalpowaTemplateKind)kind);
        name = miausoft_tlalpowa_template_name(
            (MiausoftTlalpowaTemplateKind)kind);
        ok &= require_true(name && name[0] != '\0' &&
                           strcmp(name, "invalid") != 0,
                           "una plantilla no tiene nombre canonico");
        ok &= require_true(miausoft_tlalpowa_template_validate(
                           (MiausoftTlalpowaTemplateKind)kind) != 0,
                           "descriptor de plantilla invalido");
        ok &= require_true((descriptor.flags &
                            MIAUSOFT_TLALPOWA_TEMPLATE_BORDERLESS) != 0u,
                           "descriptor sin invariante borderless");
        {
            size_t state_index;
            for (state_index = 0u;
                 state_index < sizeof(states) / sizeof(states[0]);
                 ++state_index) {
                MiausoftVisualComponentStyle style;
                memset(storage, 0, sizeof(storage));
                memset(&element, 0, sizeof(element));
                miausoft_visual_draw_list_init(&list, storage,
                    (uint32_t)(sizeof(storage) / sizeof(storage[0])));
                element.kind = (MiausoftTlalpowaTemplateKind)kind;
                element.rect.x = 0;
                element.rect.y = 0;
                element.rect.w = 320;
                element.rect.h = metrics.control_h;
                element.text = miausoft_visual_text_utf8("Tlalpowa",
                    MIAUSOFT_VISUAL_TEXT_LEFT | MIAUSOFT_VISUAL_TEXT_MIDDLE,
                    MIAUSOFT_VISUAL_FONT_BODY);
                element.value = 0.3819660113f;
                element.value_secondary = 0.6180339887f;
                element.color = theme.accent;
                element.color_secondary = miausoft_rgba(85, 122, 155, 255);
                element.state = states[state_index];
                style = miausoft_visual_component_style(&theme, element.kind,
                    element.state, element.color, element.rect.h);
                ok &= require_true(style.border_width == 0,
                                   "una plantilla introdujo contorno");
                ok &= require_true(miausoft_visual_emit(
                                   &list, &theme, &element) != 0,
                                   "una plantilla Tlalpowa no pudo emitir comandos");
                ok &= require_true(list.overflow == 0,
                                   "una plantilla excedio el buffer fijo");
                if ((descriptor.flags &
                     MIAUSOFT_TLALPOWA_TEMPLATE_INVISIBLE) != 0u) {
                    ok &= require_true(list.count == 0u,
                                       "una plantilla invisible dibujo algo");
                }
            }
        }
    }

    return ok ? 0 : 1;
}
