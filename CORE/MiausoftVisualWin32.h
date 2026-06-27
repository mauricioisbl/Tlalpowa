#ifndef MIAUSOFT_VISUAL_WIN32_H_INCLUDED
#define MIAUSOFT_VISUAL_WIN32_H_INCLUDED

#include "MiausoftVisual.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MiausoftVisualWin32Fonts {
    HFONT body;
    HFONT strong;
    HFONT title;
    HFONT small_font;
} MiausoftVisualWin32Fonts;

typedef struct MiausoftVisualWin32Context {
    HDC hdc;
    MiausoftVisualWin32Fonts fonts;
} MiausoftVisualWin32Context;

COLORREF miausoft_visual_win32_color(MiausoftRgba color);
void miausoft_visual_win32_render(const MiausoftVisualWin32Context* context, const MiausoftVisualDrawList* list);

#ifdef __cplusplus
}
#endif

#endif
#endif
