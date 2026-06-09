# Tlalpowa — arquitectura de reconstrucción atmosférica

## Principio rector

La reconstrucción atmosférica de Tlalpowa debe producir campos espaciotemporales auditables de meteorología, radiación, nubes, viento y contaminantes sobre la ZMVM + 50 km sin convertir la interfaz en un proceso pesado. Por eso la arquitectura queda dividida en capas append-only: datos crudos, datos normalizados con control de calidad, fusión espaciotemporal, simulación física, ensamble con incertidumbre y exportación interoperable. Ninguna capa debe destruir la anterior. Ningún proceso largo debe correr en el hilo gráfico.

## Base computacional integrada en esta versión

Esta versión deja un núcleo C++ nuevo en `Fuente/AtmosphereModel.hpp` y `Fuente/AtmosphereModel.cpp`. Al abrir el programa, un hilo secundario indexa de forma presupuestada hasta 400 archivos atmosféricos detectados en `. DATOS ATMOSFÉRICOS`, `.DATOS ATMOSFÉRICOS`, `DATOS ATMOSFÉRICOS` o `datos/atmosfera`. Solo lee encabezados y metadatos, no carga CSV completos, y escribe registros nativos en contenedores mensuales `.ixiptlah` unificados directamente dentro de `Datos/`, con nombre `AAAA_MM.ixiptlah`; los checkpoints técnicos viven como `.ixiptlah` en `Build/runtime`. Si el proceso se interrumpe, el checkpoint evita volver a muestrear archivos ya registrados.

## Fuentes mínimas del modelo

La capa de superficie debe combinar RAMA/SIMAT para contaminantes criterio y meteorología urbana, RUOA para observación atmosférica universitaria de alta calidad, PEMBU para densificación meteorológica educativa y territorial, estaciones propias o manuales si se agregan, y catálogos de estaciones con coordenadas verificadas. SIMAT/SINAICA identifica el Sistema de Monitoreo Atmosférico de la Ciudad de México como operador de la red de monitoreo atmosférico de la CDMX; RUOA está orientada a investigación en indexacións de la atmósfera y parámetros meteorológicos; PEMBU integra estaciones meteorológicas del bachillerato universitario. Estas redes no deben fusionarse sin conservar fuente, instrumento, estación, coordenada, unidad, intervalo de medición y bandera de calidad.

La capa satelital debe usar Sentinel-5P/TROPOMI para dióxido de nitrógeno, dióxido de azufre, monóxido de carbono, ozono, aerosoles y formaldehído cuando sea aplicable; MODIS/MAIAC y VIIRS para profundidad óptica de aerosoles, nubes y apoyo de radiación; GOES para nubosidad, radiación y evolución diurna cuando se incorpore; ERA5 como reanálisis físico de fondo horario desde 1940; DEM SRTM/Copernicus para relieve, pendiente, orientación, altitud y barreras orográficas. Sentinel-5P está dedicado al monitoreo atmosférico con TROPOMI; ERA5 es un reanálisis horario disponible desde 1940.

## Capas de datos

### 1. Capa bronce: dato crudo inmutable

Cada archivo local se registra por ruta, tamaño, fecha de modificación, proveedor inferido, tipo de fuente, variables probables y encabezado. Esta capa solo inventaría identificadores técnicos, nunca corrige valores. Debe permitir rastrear cada punto reconstruido hasta el archivo original.

### 2. Capa plata: normalización y control de calidad

Cada observación debe normalizarse a una tabla larga con `datetime_utc`, `datetime_local`, `station_id`, `lat`, `lon`, `altitude_m`, `variable`, `value`, `unit_original`, `unit_canonical`, `source`, `instrument`, `qc_flag`, `qc_rule`, `uncertainty`, `raw_file_id` y `raw_line`. Las reglas mínimas son rangos físicos, valores congelados, duplicados temporales, saltos imposibles, coherencia de unidades, consistencia de estación, comparación con estaciones vecinas y comparación con ERA5/satélite cuando exista.

### 3. Capa oro: fusión espaciotemporal

La fusión debe reconstruir campos 3D o 2.5D según la variable. Temperatura, humedad, viento y radiación requieren corrección por altitud, rugosidad, exposición solar y relieve. Ozono, dióxido de nitrógeno, monóxido de carbono, partículas y dióxido de azufre requieren advección por viento, emisión probable, química atmosférica, inversión térmica, capa límite planetaria y topografía. La primera versión operacional puede usar una malla de 500 m o 1 km, pasos horarios, interpolación anisotrópica guiada por viento y relieve, y estimación explícita de incertidumbre por celda.

### 4. Capa física avanzada

WRF-Chem debe considerarse el acoplamiento meteorología-química de referencia para corridas online; CMAQ debe considerarse el sistema multipolutante para simulaciones de calidad del aire con ozono, partículas, tóxicos y deposición; HYSPLIT debe usarse para trayectorias, dispersión y diagnóstico de transporte. El programa no debe ejecutar estos modelos pesados dentro del hilo UI; debe exportar paquetes de entrada y leer sus salidas como NetCDF-CF, GeoTIFF/COG o Zarr.

## Exportación interoperable

El formato maestro de campos atmosféricos debe ser NetCDF-CF para matrices con dimensiones tiempo, y, x, nivel, variable y metadatos normalizados. Los cortes raster deben exportarse como GeoTIFF/COG; las geometrías y agregados como GeoPackage o GeoParquet; los catálogos como STAC; las series tabulares como CSV largo y JSONL; y los cubos cloud-ready como Zarr. CF define metadatos claros para geolocalización, tiempo y magnitudes físicas; STAC estandariza metadatos de activos geoespaciales espaciotemporales.

## Antiat ascos obligatorios

La interfaz nunca debe parsear CSV masivos, PDF, satélite o reanálisis en el hilo de dibujo. Toda ingesta debe tener checkpoint, presupuesto máximo por corrida, escritura atómica, manifiestos append-only y posibilidad de continuar. Los renders deben ser degradables. Las páginas de boletín ya cerradas se omiten por checkpoint; las páginas con cuarentena quedan reintentables para recuperar únicamente datos faltantes cuando mejore el parser, sin reprocesar páginas sanas ni duplicar aceptadas.

## Referencias APA7

Copernicus Climate Data Store. (2026). *ERA5 hourly data on single levels from 1940 to present*.

Copernicus Data Space Ecosystem. (2026). *Sentinel-5P*.

NetCDF Climate and Forecast Metadata Conventions. (2026). *CF Conventions*.

NOAA Air Resources Laboratory. (2026). *HYSPLIT*.

Open Geospatial Consortium. (2025). *SpatioTemporal Asset Catalog*.

Programa de Estaciones Meteorológicas del Bachillerato Universitario. (2026). *Programa de Estaciones Meteorológicas del Bachillerato Universitario*.

Red Universitaria de Observatorios Atmosféricos. (2026). *Red Universitaria de Observatorios Atmosféricos*.

U.S. Environmental Protection Agency. (2026). *CMAQ: The Community Multiscale Air Quality Modeling System*.

UCAR Atmospheric Chemistry Observations & Modeling. (2026). *WRF-Chem*.

## Cartografía, escala, DEM real y cache progresivo — v19

El mapa vivo ya no debe presentar la etiqueta antigua `OpenMaps/OSM + satelital | 2 niveles | ZMVM + 50 km`. La atribución visible debe quedar siempre junto al mapa y debe reconocer explícitamente `© OpenStreetMap contributors`, la licencia ODbL, el uso de servidores OSMF cuando corresponda, la capa satelital Esri World Imagery cuando esté activa y el DEM Mapzen/AWS Terrarium cuando la malla de altura real se encuentre disponible.

La lectura de relieve no debe confundirse con sombreado. El relieve operativo del mapa nativo se define como DEM cuantitativo: cada tesela Terrarium se decodifica con `elevation_m = R*256 + G + B/256 - 32768` y se representa como malla ligera de levantamiento 3D. Esto mantiene altura real en metros sobre el mapa satelital o sobre el mapa OpenStreetMap, sin sustituirla por hillshade visual.

El arranque debe ser deliberadamente liviano. El programa inicia en z10 para satélite/mapa, transiciona con fundido gradual a z14 cuando la vista queda dentro de la ZMVM y sube a z16 cuando la escala equivale a alcaldía/municipio o cuando el usuario focaliza una jurisdicción. La descarga queda limitada a una tesela por frame y el DEM a una tesela por frame, de modo que la interfaz no espera a la red ni a la decodificación. El cache local aplica escritura atómica, reutilización inmediata y limpieza LRU con límite blando para impedir crecimiento indefinido sin borrar lo útil.


## Paradigma IXIPTLAH mensual unificado

Los nuevos productos internos no deben repartirse en subcarpetas por dominio. El contrato vigente es un archivo por mes en `Datos/AAAA_MM.ixiptlah`, con registros tipificados internamente para epidemiología, atmósfera, contaminantes y productos de render. La lectura interactiva usa índice volátil de encabezados, validado por tamaño y mtime, para saltar a offsets de payload sin inflar registros ajenos. La compresión es estrictamente lossless y ocurre después de serializar los valores de datos, por lo que no redondea concentraciones, coordenadas ni conteos epidemiológicos.
