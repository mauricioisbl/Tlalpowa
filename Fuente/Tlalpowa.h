#pragma once

#include <filesystem>

namespace epi {

/* Contrato público del núcleo visible de interfaz.  Este encabezado debe
   permanecer pequeño: la UI puede crecer internamente, pero otros núcleos sólo
   deben conocer estas entradas estables y no depender de tipos ImGui/GLFW. */
int run_tlalpowa_app();
bool tlalpowa_tlalpowa3d_regeoref_selftest();
int run_atmosphere_web_import_cli(int source, int year_start, int year_end, bool overwrite_category);
int run_external_import_smoke_cli(const std::filesystem::path& source_root,
                                 const std::filesystem::path& output_root,
                                 int year_start,
                                 int year_end,
                                 bool inventory_only);
int run_satellite_web_import_cli(int source,
                                 const std::filesystem::path& output_root,
                                 int year_start,
                                 int year_end);
int run_epidemiology_web_download_cli(bool cdmx, bool edomex, int year_start, int year_end);

}  // namespace epi
