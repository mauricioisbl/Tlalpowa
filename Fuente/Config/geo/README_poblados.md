# Pines de ciudades y pueblos

Tlalpowa carga pines puntuales desde catálogos TSV/CSV/TXT en esta carpeta y desde `Datos Externos/Territorio`. Los pines no se reconstruyen a partir de alcaldías ni municipios: cada fila representa una localidad puntual real, con latitud y longitud propias. La Ciudad de México se ancla siempre en el Zócalo como excepción explícita.

El cargador busca, entre otros, `poblados_mapa.tsv`, `poblados_centro_mexico.tsv`, `poblados_regionales.tsv`, `poblados_zmvm.tsv`, `localidades_inegi.tsv`, `geonames_mx.txt`, `MX.txt`, `cities1000.txt` y `cities500.txt`. También puede leer archivos GeoNames crudos, siempre que la fila tenga feature class `P` y país `MX`.

Para ampliar cobertura sin recompilar, ejecuta `Descargar_gazetteer_geonames_mx.cmd`. El script descarga `MX.txt`, `cities1000.txt` y `cities500.txt` desde GeoNames y los deja listos para el cargador. El renderizador filtra por viewport y nivel de detalle para evitar saturar la pantalla cuando el catálogo es nacional.

## Actualización de etiquetas de poblados

Las tarjetas de cada pin de ciudad o pueblo se renderizan con la misma lógica visual de tarjeta usada por las estaciones atmosféricas: encabezado, separador, línea demográfica principal y líneas de contexto. El catálogo acepta columnas adicionales opcionales: `population_year`, `elevation_m`, `admin1`, `admin2`, `notes` y `source_url`. Si se cargan catálogos INEGI o GeoNames completos, esos campos se conservan cuando existen; si un catálogo curado trae población sin año explícito, Tlalpowa la trata como referencia 2020 para que el tooltip mantenga una lectura estable y no vuelva a mostrar etiquetas crudas.

Se agregó `poblados_extendido.tsv` como semilla visible de muchos más pueblos originarios, cabeceras y ciudades regionales. Esta semilla no sustituye un catálogo oficial completo; sólo garantiza cobertura inmediata mientras se instala un repertorio exhaustivo.
