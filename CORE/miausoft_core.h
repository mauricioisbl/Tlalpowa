#ifndef MIAUSOFT_CORE_H
#define MIAUSOFT_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIAUSOFT_PHI 1.6180339887498948482
#define MIAUSOFT_PHI_N0 1.0
#define MIAUSOFT_PHI_N1 0.6180339887498948482
#define MIAUSOFT_PHI_N2 0.3819660112501051518
#define MIAUSOFT_PHI_N3 0.2360679774997896964
#define MIAUSOFT_PHI_N4 0.1458980337503154554
#define MIAUSOFT_PHI_N5 0.0901699437494742410
#define MIAUSOFT_PHI_N6 0.0557280900008412144
#define MIAUSOFT_PHI_N7 0.0344418537486330266
#define MIAUSOFT_PHI_N8 0.0212862362522081878
#define MIAUSOFT_PHI_N9 0.0131556174964248388
#define MIAUSOFT_PHI_N10 0.0081306187557833489
#define MIAUSOFT_PHI_N11 0.0050249987406414899
#define MIAUSOFT_PHI_N12 0.0031056200151418590
#define MIAUSOFT_PHI_N13 0.0019193787254996310
#define MIAUSOFT_PHI_N14 0.0011862412896422280
#define MIAUSOFT_CORE_REVISION 20260614

typedef struct MiausoftRgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} MiausoftRgb;

int miausoft_core_revision(void);
int miausoft_core_validate(void);
double miausoft_phi_ratio(unsigned int power);
int miausoft_phi_scale_px(int extent, unsigned int power, int minimum);

MiausoftRgb miausoft_palette_light_root(void);
MiausoftRgb miausoft_palette_light_frame(void);
MiausoftRgb miausoft_palette_light_ink(void);
MiausoftRgb miausoft_palette_light_muted(void);
MiausoftRgb miausoft_palette_dark_root(void);
MiausoftRgb miausoft_palette_dark_frame(void);
MiausoftRgb miausoft_palette_dark_ink(void);
MiausoftRgb miausoft_palette_dark_muted(void);
MiausoftRgb miausoft_palette_accent(void);

#ifdef __cplusplus
}
#endif

#endif
