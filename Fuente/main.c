#include "main.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

static TlalpowaCommand tlalpowa_command_from_argv(int argc, char** argv) {
    const char* command;
    if (argc < 2 || !argv || !argv[1]) return TLALPOWA_COMMAND_DEFAULT;
    command = argv[1];
    if (strcmp(command, "selftest") == 0) return TLALPOWA_COMMAND_SELFTEST;
    if (strcmp(command, "atmosphere") == 0) return TLALPOWA_COMMAND_ATMOSPHERE;
    if (strcmp(command, "atmosphere-web") == 0) return TLALPOWA_COMMAND_ATMOSPHERE_WEB;
    if (strcmp(command, "epi-web") == 0) return TLALPOWA_COMMAND_EPI_WEB;
    if (strcmp(command, "external-smoke") == 0) return TLALPOWA_COMMAND_EXTERNAL_SMOKE;
    if (strcmp(command, "satellite-web") == 0) return TLALPOWA_COMMAND_SATELLITE_WEB;
    if (strcmp(command, "epi-audit") == 0) return TLALPOWA_COMMAND_EPI_AUDIT;
    if (strcmp(command, "ixiptlah-purge-epi-file") == 0) return TLALPOWA_COMMAND_IXIPTLAH_PURGE_EPI_FILE;
    if (strcmp(command, "launcher") == 0) return TLALPOWA_COMMAND_LAUNCHER;
    if (strcmp(command, "gui") == 0) return TLALPOWA_COMMAND_GUI;
    if (strcmp(command, "app") == 0 || strcmp(command, "mapa") == 0) return TLALPOWA_COMMAND_APP;
    if (strcmp(command, "run") == 0) return TLALPOWA_COMMAND_RUN;
    return TLALPOWA_COMMAND_UNKNOWN;
}

static int tlalpowa_main(int argc, char** argv) {
    return tlalpowa_execute_command(tlalpowa_command_from_argv(argc, argv), argc, argv);
}

#ifdef _WIN32

static char* tlalpowa_utf8_from_wide(const wchar_t* src) {
    int bytes;
    char* dst;
    if (!src) return NULL;
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) return NULL;
    dst = (char*)malloc((size_t)bytes);
    if (!dst) return NULL;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, -1, dst, bytes, NULL, NULL) <= 0) {
        free(dst);
        return NULL;
    }
    return dst;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
    LPWSTR* wide_argv;
    char** argv;
    int argc = 0;
    int i;
    int rc;
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show;

    wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wide_argv || argc <= 0) {
        if (wide_argv) LocalFree(wide_argv);
        return tlalpowa_main(0, NULL);
    }

    argv = (char**)calloc((size_t)argc, sizeof(*argv));
    if (!argv) {
        LocalFree(wide_argv);
        return 2;
    }

    for (i = 0; i < argc; ++i) {
        argv[i] = tlalpowa_utf8_from_wide(wide_argv[i]);
        if (!argv[i]) {
            while (i > 0) free(argv[--i]);
            free(argv);
            LocalFree(wide_argv);
            return 2;
        }
    }
    LocalFree(wide_argv);

    rc = tlalpowa_main(argc, argv);
    for (i = 0; i < argc; ++i) free(argv[i]);
    free(argv);
    return rc;
}

#else

int main(int argc, char** argv) {
    return tlalpowa_main(argc, argv);
}

#endif
