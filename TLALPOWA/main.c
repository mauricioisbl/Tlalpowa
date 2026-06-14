#include "core.h"
#include "miausoft_core.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

static int tlalpowa_cmd_is_literal(const char* command, const char* literal, size_t literal_n) {
    size_t i;
    for (i = 0u; i < literal_n; ++i) {
        if (command[i] != literal[i]) return 0;
    }
    return command[literal_n] == '\0';
}

#define TLALPOWA_CMD_IS(command, literal) \
    tlalpowa_cmd_is_literal((command), (literal), sizeof(literal) - 1u)

static TlalpowaCommand tlalpowa_command_from_argv(int argc, char** argv) {
    const char* command;
    if (argc < 2 || !argv || !argv[1]) return TLALPOWA_COMMAND_DEFAULT;
    command = argv[1];
    switch ((unsigned char)command[0]) {
    case 'a':
        if (TLALPOWA_CMD_IS(command, "app")) return TLALPOWA_COMMAND_APP;
        if (TLALPOWA_CMD_IS(command, "atmosphere")) return TLALPOWA_COMMAND_ATMOSPHERE;
        if (TLALPOWA_CMD_IS(command, "atmosphere-web")) return TLALPOWA_COMMAND_ATMOSPHERE_WEB;
        break;
    case 'e':
        if (TLALPOWA_CMD_IS(command, "epi-web")) return TLALPOWA_COMMAND_EPI_WEB;
        if (TLALPOWA_CMD_IS(command, "epi-audit")) return TLALPOWA_COMMAND_EPI_AUDIT;
        if (TLALPOWA_CMD_IS(command, "external-smoke")) return TLALPOWA_COMMAND_EXTERNAL_SMOKE;
        break;
    case 'g':
        if (TLALPOWA_CMD_IS(command, "gui")) return TLALPOWA_COMMAND_GUI;
        break;
    case 'i':
        if (TLALPOWA_CMD_IS(command, "ixiptlah-purge-epi-file")) return TLALPOWA_COMMAND_IXIPTLAH_PURGE_EPI_FILE;
        break;
    case 'l':
        if (TLALPOWA_CMD_IS(command, "launcher")) return TLALPOWA_COMMAND_LAUNCHER;
        break;
    case 'm':
        if (TLALPOWA_CMD_IS(command, "mapa")) return TLALPOWA_COMMAND_APP;
        break;
    case 'r':
        if (TLALPOWA_CMD_IS(command, "run")) return TLALPOWA_COMMAND_RUN;
        break;
    case 's':
        if (TLALPOWA_CMD_IS(command, "selftest")) return TLALPOWA_COMMAND_SELFTEST;
        if (TLALPOWA_CMD_IS(command, "satellite-web")) return TLALPOWA_COMMAND_SATELLITE_WEB;
        break;
    default:
        break;
    }
    return TLALPOWA_COMMAND_UNKNOWN;
}

#undef TLALPOWA_CMD_IS


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
    if (!miausoft_core_validate()) {
        return 70;
    }
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
