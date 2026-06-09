# TLALPOWA · i18n configuración y catálogos

Actualización acumulativa sobre la versión de tres idiomas embebidos.

## Cambios aplicados

- Se reforzó `tlalpowa_tr()` con una tabla estática adicional `tlalpowa_tr_deep()` compilada dentro de `Tlalpowa.cpp`.
- No se agrega lectura de XLSX, JSON, TXT ni diccionarios externos en ejecución.
- La ventana de configuración traduce ahora controles profundos de:
  - importación CSV/XLSX/JSON;
  - NetCDF/HDF/GeoTIFF/GRIB;
  - fuentes satelitales;
  - documentos PDF/ZIP;
  - credenciales temporales RUOA, CDSE, NASA Earthdata, LAADS y CDS/ADS;
  - etiquetas de fuente local/web, años, documentación y estados visibles fijos.
- Los catálogos atmosféricos y contaminantes pasan sus nombres por la capa embebida antes de dibujarse.
- Se cubren todos los nombres visibles declarados en `Config/atmosfera_contaminantes.tsv`.
- Se ampliaron grupos atmosféricos y derivados: satelitales, meteorológicos, carbono, gases, COV, aerosoles y metal particulado.
- Se retiró de los hovers del mapa el texto documental tipo `Fuente oficial CDMX/REPSA`.
- Las estaciones atmosféricas y de transporte muestran sólo el nombre visible, sin concatenaciones internas de sistema/línea/trazo.
- Los nombres de rutas GTFS ya no agregan sufijos `trazo N` al texto público.

## Validación local

- `tlalpowa_tr_deep`: 321 entradas adicionales, 0 colisiones FNV-1a detectadas al generar la tabla.
- Auditoría TSV: 0 nombres atmosféricos/contaminantes sin entrada embebida.
- `g++ -std=c++17 -fsyntax-only Tlalpowa.cpp` sin `TLALPOWA_ENABLE_IMGUI`: correcto.
- No se incluye carpeta `Datos` en el paquete final.
