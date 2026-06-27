# OpenStreetMap territorial ligero

El fondo visible de Tlalpowa permanece satelital. OpenStreetMap aporta
poblados, colonias, cuerpos de agua y relaciones administrativas.

Las relaciones OSM se convierten directamente en `MapFeature`. Por eso la
geometría usada para dibujar límites también alimenta selección, muestreo
espacial, promedios de concentración dentro de un área y gráficas
territoriales.

Las consultas se dividen en celdas y se guardan como JSON compacto en la caché
del mapa. La altitud usa `ele` de OSM cuando existe y Terrarium como respaldo
cuantitativo. `zmvm.geojson` sólo completa territorios que OSM todavía no haya
resuelto. Los importadores heredados TSV/CSV/GeoNames están fuera de la
compilación.
