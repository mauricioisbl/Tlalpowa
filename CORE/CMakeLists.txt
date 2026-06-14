cmake_minimum_required(VERSION 3.24)
project(MiausoftSuite LANGUAGES C CXX)

option(MIAUSOFT_BUILD_TLALPOWA "Build Tlalpowa" OFF)
option(MIAUSOFT_BUILD_APPS "Build Ilnamiki, Biblioteca and Miausoft tools" ON)

get_filename_component(MIAUSOFT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MIAUSOFT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MIAUSOFT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MIAUSOFT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${MIAUSOFT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${MIAUSOFT_ROOT}")

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(miausoft_core STATIC
    miausoft_core.c
    tlalpowa_common_runtime.c
)
target_include_directories(miausoft_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

function(miausoft_require_core target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "No existe el destino requerido: ${target}")
    endif()
    target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries("${target}" PRIVATE miausoft_core)
    set_property(TARGET "${target}" PROPERTY MIAUSOFT_CORE_ATTACHED TRUE)
endfunction()

if(MIAUSOFT_BUILD_TLALPOWA)
    add_subdirectory("${MIAUSOFT_ROOT}/TLALPOWA" "${CMAKE_BINARY_DIR}/Tlalpowa")
    miausoft_require_core(Tlalpowa)
endif()

if(MIAUSOFT_BUILD_APPS)
    function(miausoft_add_windows_app target output_name directory source resource)
        add_executable(${target} WIN32
            "${MIAUSOFT_ROOT}/${directory}/${source}"
            "${MIAUSOFT_ROOT}/${directory}/${resource}"
        )
        set_target_properties(${target} PROPERTIES OUTPUT_NAME "${output_name}")
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(${target} PRIVATE
            UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(${target} PRIVATE
            user32 gdi32 gdiplus comdlg32 shell32 ole32 oleaut32
            advapi32 shlwapi comctl32 bcrypt dwmapi uxtheme uuid
            urlmon wininet
        )
        if(MSVC)
            target_compile_options(${target} PRIVATE /bigobj)
        endif()
        miausoft_require_core(${target})
    endfunction()

    add_executable(Ilnamiki WIN32
        "${MIAUSOFT_ROOT}/ILNAMIKI/Ilnamiki.cpp"
        "${MIAUSOFT_ROOT}/ILNAMIKI/Ilnamiki.rc"
    )
    target_include_directories(Ilnamiki PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(Ilnamiki PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Ilnamiki PRIVATE shell32 user32)
    miausoft_require_core(Ilnamiki)

    miausoft_add_windows_app(
        Biblioteca Biblioteca "MIAUSOFTOOLS/Biblioteca" Biblioteca.cpp Biblioteca.rc)
    target_sources(Biblioteca PRIVATE
        "${MIAUSOFT_ROOT}/MIAUSOFTOOLS/Biblioteca/nucleo_documentos.c")
    miausoft_add_windows_app(
        Convertidor_Completo Miausoft_Convertidor_Completo
        "MIAUSOFTOOLS/Convertidor_Completo" Convertidor_Completo.cpp Convertidor_Completo.rc)
    miausoft_add_windows_app(
        Convertidor_Por_Capitulos Miausoft_Convertidor_Por_Capitulos
        "MIAUSOFTOOLS/Convertidor_Por_Capitulos" Convertidor_Por_Capitulos.cpp Convertidor_Por_Capitulos.rc)
    miausoft_add_windows_app(
        Fusionador_Divisor Miausoft_Fusionador_Divisor
        "MIAUSOFTOOLS/Fusionador_Divisor" Fusionador_Divisor.cpp Fusionador_Divisor.rc)
    miausoft_add_windows_app(
        Organizador Organizador
        "MIAUSOFTOOLS/Organizador_Biblioteca" Organizador_Biblioteca.cpp Organizador_Biblioteca.rc)
    miausoft_add_windows_app(
        Reemplazador_Caracteres Miausoft_Reemplazador_Caracteres
        "MIAUSOFTOOLS/Reemplazador_Caracteres" Reemplazador_Caracteres.cpp Reemplazador_Caracteres.rc)
    miausoft_add_windows_app(
        MiausoftSuite_Installer MiausoftSuite_Installer
        "MIAUSOFTOOLS/Installer" MiausoftSuite_Installer.cpp MiausoftSuite_Installer.rc)

    add_dependencies(MiausoftSuite_Installer
        Ilnamiki Biblioteca Convertidor_Completo Convertidor_Por_Capitulos
        Fusionador_Divisor Organizador Reemplazador_Caracteres)

    add_custom_target(MiausoftTools DEPENDS
        Convertidor_Completo Convertidor_Por_Capitulos Fusionador_Divisor
        Organizador Reemplazador_Caracteres MiausoftSuite_Installer)
endif()

get_property(MIAUSOFT_ALL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS MIAUSOFT_ALL_TARGETS)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "EXECUTABLE")
        get_target_property(core_attached "${target}" MIAUSOFT_CORE_ATTACHED)
        if(NOT core_attached)
            message(FATAL_ERROR
                "El ejecutable ${target} no esta conectado al CORE compartido.")
        endif()
    endif()
endforeach()
