#include "miausoft_core.h"

static const double k_phi_ratios[] = {
    MIAUSOFT_PHI_N0,
    MIAUSOFT_PHI_N1,
    MIAUSOFT_PHI_N2,
    MIAUSOFT_PHI_N3,
    MIAUSOFT_PHI_N4,
    MIAUSOFT_PHI_N5,
    MIAUSOFT_PHI_N6,
    MIAUSOFT_PHI_N7,
    MIAUSOFT_PHI_N8,
    MIAUSOFT_PHI_N9,
    MIAUSOFT_PHI_N10,
    MIAUSOFT_PHI_N11,
    MIAUSOFT_PHI_N12,
    MIAUSOFT_PHI_N13,
    MIAUSOFT_PHI_N14,
};

static MiausoftRgb rgb(unsigned int r, unsigned int g, unsigned int b) {
    MiausoftRgb value;
    value.r = (uint8_t)r;
    value.g = (uint8_t)g;
    value.b = (uint8_t)b;
    return value;
}

int miausoft_core_revision(void) {
    return MIAUSOFT_CORE_REVISION;
}

int miausoft_core_validate(void) {
    return miausoft_core_revision() == MIAUSOFT_CORE_REVISION;
}

double miausoft_phi_ratio(unsigned int power) {
    const unsigned int count =
        (unsigned int)(sizeof(k_phi_ratios) / sizeof(k_phi_ratios[0]));
    double value;
    unsigned int i;

    if (power < count) {
        return k_phi_ratios[power];
    }

    value = k_phi_ratios[count - 1];
    for (i = count - 1; i < power; ++i) {
        value /= MIAUSOFT_PHI;
    }
    return value;
}

int miausoft_phi_scale_px(int extent, unsigned int power, int minimum) {
    const double scaled = (double)extent * miausoft_phi_ratio(power);
    const int rounded = (int)(scaled + 0.5);
    return rounded < minimum ? minimum : rounded;
}

MiausoftRgb miausoft_palette_light_root(void) { return rgb(250, 252, 253); }
MiausoftRgb miausoft_palette_light_frame(void) { return rgb(235, 240, 244); }
MiausoftRgb miausoft_palette_light_ink(void) { return rgb(19, 22, 27); }
MiausoftRgb miausoft_palette_light_muted(void) { return rgb(116, 126, 139); }
MiausoftRgb miausoft_palette_dark_root(void) { return rgb(10, 12, 15); }
MiausoftRgb miausoft_palette_dark_frame(void) { return rgb(19, 24, 30); }
MiausoftRgb miausoft_palette_dark_ink(void) { return rgb(235, 240, 244); }
MiausoftRgb miausoft_palette_dark_muted(void) { return rgb(147, 158, 171); }
MiausoftRgb miausoft_palette_accent(void) { return rgb(133, 13, 55); }
