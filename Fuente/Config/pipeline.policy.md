# Política de extracción segura

1. La extracción usa cajas de palabras de PDF, nunca coordenadas manuales.
2. Las filas solo se aceptan si se reconocen alcaldías canónicas de la Ciudad de México.
3. Las columnas se reconstruyen por acumulación geométrica de tokens numéricos alineados.
4. Toda columna queda etiquetada con su regla de validación: suma exacta de 16 alcaldías contra Total, imputación de una sola alcaldía desde Total, o aceptación provisional si las 16 alcaldías están presentes y falta el renglón Total.
5. Si falla el encabezado, la conversión numérica, la completitud mínima o la suma total no recuperable, la columna se manda a cuarentena.
6. El Excel futuro será una vista derivada; la fuente primaria son CSV, JSONL y SQL.

## Política v45: Datos plano y validez por cobertura de enfermedades

`Datos` queda reservado para salidas epidemiológicas planas. La salida principal es `Datos/Epidemiologia` y dentro de ella no deben generarse subcarpetas; los archivos vivos, manifiestos, JSONL, CSV, SQL, XLSX cuando exista, HTML de inspección y estado de corrida se escriben como archivos hermanos.

Las carpetas heredadas `Datos/corrida_mapa_vivo`, `Datos/mapa_parcelas`, `Datos/mapa_parcelas_base` y `Datos/Nueva carpeta` no son insumo obligatorio y no deben recrearse desde compilación normal. La caché cartográfica, cuando se use, vive fuera de `Datos`, bajo `Build/cache_mapa`.

La validez de una semana epidemiológica no se decide por número bruto de filas ni por casos mayores a cero. Una semana se considera suficientemente procesada cuando cubre más del 80 % del catálogo de enfermedades configurado; una enfermedad con valor 0 cuenta como procesada si su fila fue capturada. Este criterio protege contra cierres forzados que producen años falsamente vacíos sin castigar ceros epidemiológicos reales.
