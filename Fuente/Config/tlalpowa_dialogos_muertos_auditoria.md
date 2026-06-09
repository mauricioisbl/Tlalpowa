# Auditoría de diálogos muertos / tooltips no usados

Fecha técnica: 2026-06-09.

## Criterio aplicado

Se revisaron los textos de ayuda que habían quedado como argumentos muertos después de silenciar el catálogo lateral. En esta versión, el catálogo ya no conserva cadenas explicativas invisibles ni traducibles si ningún flujo visual las consume.

## Cambios aplicados

- Se eliminó el helper muerto `draw_side_scope_check`, que no tenía llamadas activas y conservaba un parámetro `tooltip` inaccesible.
- Se retiró el parámetro `tooltip` de estos helpers del catálogo lateral:
  - `draw_side_tree_header_checkbox`
  - `draw_side_tree_header_plain`
  - `draw_side_boolean_check`
  - `draw_side_network_filter_check`
- Se reescribieron las llamadas de catálogo para que no carguen ni conserven literales explicativos no usados.
- Se retiraron de la tabla i18n embebida los textos largos que ya no tenían ninguna ruta de renderizado ni referencia fuera de la propia tabla.

## Textos muertos purgados

- `Carga la representación 3D estática preconvertida; nunca abre Blender ni el .blend durante la ejecución.`
- `Mostrar u ocultar etiquetas de fuentes oficiales suficientemente visibles.`
- `Mostrar u ocultar Metro; sus líneas quedan como hojas explícitas de IXIPTLAH-SM.`
- `Neblina calibrada por humedad, PM2.5 y profundidad óptica de aerosoles; no carga teselas adicionales.`
- `Fuente oficial CDMX/REPSA; se dibuja como perímetro cerrado sin geometría reconstruida.`
- textos muertos de aislamiento/clic derecho/selección parcial del catálogo lateral.
- textos muertos de redes atmosféricas que ya no se muestran como globo largo.

## Conservado deliberadamente

- Tooltips atmosféricos y contaminantes compactos de subcategoría, con 1 a 3 palabras.
- Tarjeta atmosférica del mapa, porque sí se renderiza como tarjeta de estación/dato.
- Tooltips de controles de importación/configuración que siguen conectados a botones reales.
- Tooltips de gráficas y tablas técnicas que sí muestran valores concretos.
