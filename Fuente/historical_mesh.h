#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TlalHistoricalVertex {
    uint16_t lon;
    uint16_t lat;
    uint16_t height;
    int8_t normal_x;
    int8_t normal_y;
    int8_t normal_z;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} TlalHistoricalVertex;

typedef struct TlalHistoricalNativeMesh {
    uint32_t format_version;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t source_triangle_count;
    uint32_t memory_budget_mb;
    float detail_ratio;
    float lon_min;
    float lat_min;
    float lon_max;
    float lat_max;
    float height_min_m;
    float height_max_m;
    TlalHistoricalVertex* vertices;
} TlalHistoricalNativeMesh;

int tlal_historical_native_mesh_load_utf8(const char* path_utf8, TlalHistoricalNativeMesh* mesh);
void tlal_historical_native_mesh_release(TlalHistoricalNativeMesh* mesh);

#ifdef __cplusplus
}
#endif
