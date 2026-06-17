cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0077 NEW)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(Tlalpowa VERSION 1.0.0 LANGUAGES C CXX)
if(WIN32)
  enable_language(RC)
endif()

option(TLALPOWA_ENABLE_IMGUI "Compilar interfaz grafica ImGui/GLFW/OpenGL" ON)
option(TLALPOWA_ENABLE_LTCG "Activar LTCG/IPO en Release" OFF)
option(TLALPOWA_ENABLE_CL_MP "Activar /MP en MSVC" OFF)
option(TLALPOWA_PROCESS_ICON_PNGS "Reservado para Compilar.cmd; no procesa PNG desde CMake" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

set(TLALPOWA_FUENTE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(TLALPOWA_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../CORE" ABSOLUTE)

set(TLALPOWA_SOURCES
  "${TLALPOWA_FUENTE_DIR}/main.c"
  "${TLALPOWA_FUENTE_DIR}/tlalpowa_c.c"
  "${TLALPOWA_FUENTE_DIR}/tlalpowa_nucleo.cpp"
  "${TLALPOWA_FUENTE_DIR}/Tlalpowa.cpp"
  "${TLALPOWA_FUENTE_DIR}/GeneratedRuntimeResources.c")


set(TLALPOWA_TERRITORIO_FIJO_ARCHIVOS
  "${TLALPOWA_FUENTE_DIR}/Datos/territorio_centro_mexico.tlalgeo"
  "${TLALPOWA_FUENTE_DIR}/Datos/estados_centro_mexico.tlalgeo"
  "${TLALPOWA_FUENTE_DIR}/Datos/territorio_centro_mexico_catalogo.json")

foreach(_tlalpowa_territorio_fijo IN LISTS TLALPOWA_TERRITORIO_FIJO_ARCHIVOS)
  if(NOT EXISTS "${_tlalpowa_territorio_fijo}")
    message(FATAL_ERROR "Falta dato territorial fijo: ${_tlalpowa_territorio_fijo}")
  endif()
endforeach()

if(NOT TARGET miausoft_core)
  add_library(miausoft_core STATIC
    "${TLALPOWA_COMMON_DIR}/miausoft_core.c"
    "${TLALPOWA_COMMON_DIR}/tlalpowa_common_runtime.c")
  target_include_directories(miausoft_core PUBLIC "${TLALPOWA_COMMON_DIR}")
endif()

if(WIN32)
  list(APPEND TLALPOWA_SOURCES "${TLALPOWA_FUENTE_DIR}/app_icon.rc")
  set_source_files_properties("${TLALPOWA_FUENTE_DIR}/app_icon.rc" PROPERTIES
    COMPILE_OPTIONS "/I${TLALPOWA_FUENTE_DIR}")
endif()

add_executable(Tlalpowa WIN32 ${TLALPOWA_SOURCES})

target_include_directories(Tlalpowa PRIVATE
  "${TLALPOWA_FUENTE_DIR}"
  "${TLALPOWA_COMMON_DIR}")
target_link_libraries(Tlalpowa PRIVATE nlohmann_json::nlohmann_json miausoft_core)
set_property(TARGET Tlalpowa PROPERTY MIAUSOFT_CORE_ATTACHED TRUE)

add_custom_command(TARGET Tlalpowa POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tlalpowa>/TLALPOWA/Datos"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TLALPOWA_TERRITORIO_FIJO_ARCHIVOS} "$<TARGET_FILE_DIR:Tlalpowa>/TLALPOWA/Datos")

target_compile_definitions(Tlalpowa PRIVATE
  _CRT_SECURE_NO_WARNINGS
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  UNICODE
  _UNICODE)

if(MSVC)
  target_compile_options(Tlalpowa PRIVATE /utf-8 /EHsc /bigobj /Zc:__cplusplus)
  if(TLALPOWA_ENABLE_CL_MP)
    target_compile_options(Tlalpowa PRIVATE /MP)
  endif()
  target_compile_options(Tlalpowa PRIVATE "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Release>:/Gw>" "$<$<CONFIG:Release>:/Gy>")
  target_link_options(Tlalpowa PRIVATE
    /INCREMENTAL:NO
    /STACK:16777216
    "$<$<CONFIG:Release>:/OPT:REF>"
    "$<$<CONFIG:Release>:/OPT:ICF>")
else()
  target_compile_options(Tlalpowa PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

if(TLALPOWA_ENABLE_LTCG)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT TLALPOWA_IPO_OK OUTPUT TLALPOWA_IPO_MSG)
  if(TLALPOWA_IPO_OK)
    set_property(TARGET Tlalpowa PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endif()

if(TLALPOWA_ENABLE_IMGUI)
  target_compile_definitions(Tlalpowa PRIVATE TLALPOWA_ENABLE_IMGUI=1)

  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip)
  FetchContent_MakeAvailable(glfw)

  # Tlalpowa usa docking + multi-viewports para ventanas con marco nativo.
  # La rama master de Dear ImGui no expone ImGuiWindowClass, NoDocking ni ViewportsEnable.
  # Se usa un nombre de dependencia distinto para no reutilizar caches _deps/imgui-src antiguas.
  set(TLALPOWA_IMGUI_DOCKING_TAG "v1.90.9-docking" CACHE STRING "Dear ImGui docking tag requerido por Tlalpowa")
  FetchContent_Declare(imgui_docking
    URL "https://github.com/ocornut/imgui/archive/refs/tags/${TLALPOWA_IMGUI_DOCKING_TAG}.zip")
  FetchContent_MakeAvailable(imgui_docking)

  FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip)
  FetchContent_MakeAvailable(stb)

  find_package(OpenGL REQUIRED)

  add_library(tlalpowa_imgui STATIC
    "${imgui_docking_SOURCE_DIR}/imgui.cpp"
    "${imgui_docking_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_docking_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_docking_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_docking_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_docking_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_docking_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")

  target_include_directories(tlalpowa_imgui PUBLIC
    "${imgui_docking_SOURCE_DIR}"
    "${imgui_docking_SOURCE_DIR}/backends")
  target_link_libraries(tlalpowa_imgui PUBLIC glfw OpenGL::GL)
  if(MSVC)
    target_compile_options(tlalpowa_imgui PRIVATE /utf-8 /EHsc)
  endif()

  target_include_directories(Tlalpowa PRIVATE
    "${imgui_docking_SOURCE_DIR}"
    "${imgui_docking_SOURCE_DIR}/backends"
    "${stb_SOURCE_DIR}")
  target_link_libraries(Tlalpowa PRIVATE tlalpowa_imgui glfw OpenGL::GL)
endif()

if(WIN32)
  target_link_libraries(Tlalpowa PRIVATE
    wininet
    urlmon
    bcrypt
    shell32
    ole32
    uuid
    comdlg32
    gdi32
    user32
    advapi32
    dwmapi
    shlwapi
    ws2_32)
endif()

if(DEFINED TLALPOWA_RUNTIME_OUTPUT_DIR AND NOT "${TLALPOWA_RUNTIME_OUTPUT_DIR}" STREQUAL "")
  set_target_properties(Tlalpowa PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TLALPOWA_RUNTIME_OUTPUT_DIR}")
  foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set_target_properties(Tlalpowa PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${TLALPOWA_RUNTIME_OUTPUT_DIR}")
  endforeach()
endif()
