#pragma once

#ifdef _WIN32
/* Herramienta de construcción aislada. Mantenerla fuera del runtime evita que
   Windows Imaging Component contamine el núcleo analítico de TLALPOWA.
   Se expone main(), no wmain(), porque Compilar_Tlalpowa.cmd invoca cl.exe
   directamente; el wrapper convierte argv a rutas anchas sin forzar /ENTRY. */
int main(int argc, char** argv);
#endif
