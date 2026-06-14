# Datos geográficos y catálogos consolidados

Esta entrega deja los datos dispersos de `Fuente/Tlalpowa` dentro de `tlalpowa_datos.json`. En ejecución, Tlalpowa materializa automáticamente los nombres heredados en `Build/runtime/config_consolidada`, por lo que el código puede seguir leyendo `zmvm.geojson`, catálogos TSV, movilidad JSONL y políticas JSON sin conservar esos archivos sueltos en `Fuente`.

# Geo · notas consolidadas


---

## README.md

# Geometria local de Tlalpowa

`zmvm.geojson` es el activo geografico que consume la aplicacion C++ y el visor web.

La version incluida actualmente contiene las 16 alcaldias de la Ciudad de Mexico tomadas del conjunto "Limite de Alcaldias (areas geoestadisticas municipales)" del portal de datos abiertos de CDMX, con autor INEGI y Marco Geoestadistico 2020. La capa se usa como base operativa porque los boletines epidemiologicos procesados reportan datos por alcaldia.

Los municipios externos del Valle de Mexico quedan previstos en el contrato de datos y en la UI como contexto metropolitano. Cuando se agregue una geometria oficial unificada ZMVM, debe reemplazar este archivo conservando `FeatureCollection` y propiedades de nombre/codigo equivalentes.

Fuente:
https://datos.cdmx.gob.mx/dataset/alcaldias


## Extensión operativa de visualización

`zmvm_buffer_50km.geojson` es una geometría derivada localmente a partir de `zmvm.geojson`: se disuelve la ZMVM completa, se reproyecta a UTM zona 14N, se aplica un buffer de 50 000 m desde el perímetro, se simplifica para mantenerlo liviano y se devuelve a WGS84. La aplicación lo usa para encuadrar, limitar paneo y separar tres lecturas visuales: ZMVM con detalle alto, anillo de 50 km con resolución baja permanente y exterior de 50 km fuertemente atenuado. El perímetro no se dibuja como línea; solo regula resolución y opacidad.

Las delimitaciones visibles deben venir preferentemente de la capa cartográfica OpenStreetMap/OpenMaps. El GeoJSON local queda como respaldo operativo para selección, hover, cómputo espacial y continuidad cuando la capa de mapa no responda.

---

## README_poblados.md

# Pines de ciudades y pueblos

Tlalpowa carga pines puntuales desde catálogos TSV/CSV/TXT en esta carpeta y desde `Datos Externos/Territorio`. Los pines no se reconstruyen a partir de alcaldías ni municipios: cada fila representa una localidad puntual real, con latitud y longitud propias. La Ciudad de México se ancla siempre en el Zócalo como excepción explícita.

El cargador busca, entre otros, `poblados_mapa.tsv`, `poblados_centro_mexico.tsv`, `poblados_regionales.tsv`, `poblados_zmvm.tsv`, `localidades_inegi.tsv`, `geonames_mx.txt`, `MX.txt`, `cities1000.txt` y `cities500.txt`. También puede leer archivos GeoNames crudos, siempre que la fila tenga feature class `P` y país `MX`.

Para ampliar cobertura sin recompilar, ejecuta `Descargar_gazetteer_geonames_mx.cmd`. El script descarga `MX.txt`, `cities1000.txt` y `cities500.txt` desde GeoNames y los deja listos para el cargador. El renderizador filtra por viewport y nivel de detalle para evitar saturar la pantalla cuando el catálogo es nacional.

## Actualización de etiquetas de poblados

Las tarjetas de cada pin de ciudad o pueblo se renderizan con la misma lógica visual de tarjeta usada por las estaciones atmosféricas: encabezado, separador, línea demográfica principal y líneas de contexto. El catálogo acepta columnas adicionales opcionales: `population_year`, `elevation_m`, `admin1`, `admin2`, `notes` y `source_url`. Si se cargan catálogos INEGI o GeoNames completos, esos campos se conservan cuando existen; si un catálogo curado trae población sin año explícito, Tlalpowa la trata como referencia 2020 para que el tooltip mantenga una lectura estable y no vuelva a mostrar etiquetas crudas.

Se agregó `poblados_extendido.tsv` como semilla visible de muchos más pueblos originarios, cabeceras y ciudades regionales. Esta semilla no sustituye un catálogo oficial completo; sólo garantiza cobertura inmediata mientras se instala un repertorio exhaustivo.

## Datos Territoriales

La capa `Datos Territoriales` queda centralizada en `tlalpowa_datos.json`: al arrancar, Tlalpowa materializa `tlalpowa_territorial.json` dentro de `Build/runtime/config_consolidada`. La UI queda organizada como estado → jurisdicción opcional → municipio/alcaldía. CDMX y la fracción ZMVM del Estado de México aprovechan los polígonos ya instalados en `zmvm.geojson`; para polígonos nacionales completos se conserva la ruta explícita de INEGI Marco Geoestadístico o GeoBoundaries sin introducir archivos dispersos dentro de `Fuente`.
