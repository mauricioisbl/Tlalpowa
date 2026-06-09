#include "historical_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static uint16_t tlal_u16le(const unsigned char* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static float tlal_f32le(const unsigned char* p) {
    uint32_t bits = (uint32_t)p[0] |
                    ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) |
                    ((uint32_t)p[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static FILE* tlal_mesh_fopen_utf8(const char* path_utf8) {
#ifdef _WIN32
    int chars;
    wchar_t* path_w;
    FILE* file = NULL;
    if (!path_utf8) return NULL;
    chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1, NULL, 0);
    if (chars <= 0) return NULL;
    path_w = (wchar_t*)malloc((size_t)chars * sizeof(*path_w));
    if (!path_w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path_utf8, -1, path_w, chars) > 0) {
        if (_wfopen_s(&file, path_w, L"rb") != 0) file = NULL;
    }
    free(path_w);
    return file;
#else
    return path_utf8 ? fopen(path_utf8, "rb") : NULL;
#endif
}

void tlal_historical_native_mesh_release(TlalHistoricalNativeMesh* mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    memset(mesh, 0, sizeof(*mesh));
}

int tlal_historical_native_mesh_load_utf8(const char* path_utf8, TlalHistoricalNativeMesh* mesh) {
    unsigned char header[64];
    FILE* file;
    size_t bytes;
    size_t i;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t stride;

    if (!mesh) return 0;
    tlal_historical_native_mesh_release(mesh);
    file = tlal_mesh_fopen_utf8(path_utf8);
    if (!file) return 0;
    if (fread(header, 1u, 48u, file) != 48u ||
        (memcmp(header, "TLT3D001", 8u) != 0 &&
         memcmp(header, "TLP3D002", 8u) != 0)) {
        fclose(file);
        return 0;
    }
    mesh->format_version = memcmp(header, "TLP3D002", 8u) == 0 ? 2u : 1u;
    if (mesh->format_version == 2u) {
        if (fread(header + 48u, 1u, 16u, file) != 16u) {
            fclose(file);
            memset(mesh, 0, sizeof(*mesh));
            return 0;
        }
        mesh->source_triangle_count = (uint32_t)header[48] |
                                      ((uint32_t)header[49] << 8) |
                                      ((uint32_t)header[50] << 16) |
                                      ((uint32_t)header[51] << 24);
        mesh->memory_budget_mb = (uint32_t)header[52] |
                                 ((uint32_t)header[53] << 8) |
                                 ((uint32_t)header[54] << 16) |
                                 ((uint32_t)header[55] << 24);
        mesh->detail_ratio = tlal_f32le(header + 56);
    } else {
        mesh->source_triangle_count = 0u;
        mesh->memory_budget_mb = 0u;
        mesh->detail_ratio = 0.0f;
    }

    vertex_count = (uint32_t)header[8] |
                   ((uint32_t)header[9] << 8) |
                   ((uint32_t)header[10] << 16) |
                   ((uint32_t)header[11] << 24);
    triangle_count = (uint32_t)header[12] |
                     ((uint32_t)header[13] << 8) |
                     ((uint32_t)header[14] << 16) |
                     ((uint32_t)header[15] << 24);
    stride = (uint32_t)header[40] |
             ((uint32_t)header[41] << 8) |
             ((uint32_t)header[42] << 16) |
             ((uint32_t)header[43] << 24);

    mesh->lon_min = tlal_f32le(header + 16);
    mesh->lat_min = tlal_f32le(header + 20);
    mesh->lon_max = tlal_f32le(header + 24);
    mesh->lat_max = tlal_f32le(header + 28);
    mesh->height_min_m = tlal_f32le(header + 32);
    mesh->height_max_m = tlal_f32le(header + 36);

    if (vertex_count < 3u || vertex_count > 57000000u ||
        triangle_count == 0u || triangle_count > 19000000u ||
        vertex_count != triangle_count * 3u || stride != 12u ||
        !(mesh->lon_max > mesh->lon_min) || !(mesh->lat_max > mesh->lat_min) ||
        !(mesh->height_max_m > mesh->height_min_m) ||
        vertex_count > SIZE_MAX / sizeof(*mesh->vertices)) {
        fclose(file);
        memset(mesh, 0, sizeof(*mesh));
        return 0;
    }

    bytes = (size_t)vertex_count * 12u;
    mesh->vertices = (TlalHistoricalVertex*)malloc((size_t)vertex_count * sizeof(*mesh->vertices));
    if (!mesh->vertices || sizeof(*mesh->vertices) != 12u ||
        fread(mesh->vertices, 1u, bytes, file) != bytes) {
        fclose(file);
        tlal_historical_native_mesh_release(mesh);
        return 0;
    }
    fclose(file);

    {
        const uint16_t endian_probe = 1u;
        if (*(const unsigned char*)&endian_probe == 0u) {
            for (i = 0u; i < (size_t)vertex_count; ++i) {
                TlalHistoricalVertex* vertex = mesh->vertices + i;
                vertex->lon = (uint16_t)((vertex->lon >> 8) | (vertex->lon << 8));
                vertex->lat = (uint16_t)((vertex->lat >> 8) | (vertex->lat << 8));
                vertex->height = (uint16_t)((vertex->height >> 8) | (vertex->height << 8));
            }
        }
    }
    mesh->vertex_count = vertex_count;
    mesh->triangle_count = triangle_count;
    if (mesh->format_version == 1u) {
        mesh->source_triangle_count = triangle_count;
        mesh->memory_budget_mb = (uint32_t)((bytes + 1048575u) / 1048576u);
        mesh->detail_ratio = 1.0f;
    }
    return 1;
}
