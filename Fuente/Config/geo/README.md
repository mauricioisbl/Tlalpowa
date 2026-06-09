# Geometria local de Tlalpowa

`zmvm.geojson` es el activo geografico que consume la aplicacion C++ y el visor web.

La version incluida actualmente contiene las 16 alcaldias de la Ciudad de Mexico tomadas del conjunto "Limite de Alcaldias (areas geoestadisticas municipales)" del portal de datos abiertos de CDMX, con autor INEGI y Marco Geoestadistico 2020. La capa se usa como base operativa porque los boletines epidemiologicos procesados reportan datos por alcaldia.

Los municipios externos del Valle de Mexico quedan previstos en el contrato de datos y en la UI como contexto metropolitano. Cuando se agregue una geometria oficial unificada ZMVM, debe reemplazar este archivo conservando `FeatureCollection` y propiedades de nombre/codigo equivalentes.

Fuente:
https://datos.cdmx.gob.mx/dataset/alcaldias


## Extensión operativa de visualización

`zmvm_buffer_50km.geojson` es una geometría derivada localmente a partir de `zmvm.geojson`: se disuelve la ZMVM completa, se reproyecta a UTM zona 14N, se aplica un buffer de 50 000 m desde el perímetro, se simplifica para mantenerlo liviano y se devuelve a WGS84. La aplicación lo usa para encuadrar, limitar paneo y separar tres lecturas visuales: ZMVM con detalle alto, anillo de 50 km con resolución baja permanente y exterior de 50 km fuertemente atenuado. El perímetro no se dibuja como línea; solo regula resolución y opacidad.

Las delimitaciones visibles deben venir preferentemente de la capa cartográfica OpenStreetMap/OpenMaps. El GeoJSON local queda como respaldo operativo para selección, hover, cómputo espacial y continuidad cuando la capa de mapa no responda.
