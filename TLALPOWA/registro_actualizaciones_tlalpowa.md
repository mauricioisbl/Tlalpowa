# Registro consolidado de Tlalpowa


---

## ACTUALIZACIONES_CONSOLIDADAS_VIGENTES_2026_05_28.md

Consolidado vigente de actualizaciones de Tlalpowa al 28 de mayo de 2026. Este documento absorbe las bitácoras `ACTUALIZACION_*.md` que existían en `Fuente/Config` y conserva sólo la descripción funcional que coincide con la carpeta `Fuente` y los archivos base revisados. Las notas históricas que afirmaban fórmulas o superficies ya reemplazadas no se copian como autoridad vigente. La documentación estructural que no era bitácora de actualización, como `modelado_atmosferico_arquitectura.md`, `pipeline.policy.md` y `geo/README.md`, permanece como documento independiente porque no describe el mismo plano de cambios acumulados. La descripción consolidada debe entenderse como contrato de mantenimiento de la versión actual, no como cronología de intentos intermedios.

La interfaz vigente se ordena por una retícula áurea explícita `n0..n14`, calculada con `golden_w()` y `golden_h()` a partir del viewport efectivo. La fuente global se define por `kTlalpowaFontHeightRatio = kGoldenN9 + kGoldenN13`, y el alto canónico de los controles interactivos se define por `kTlalpowaControlHeightRatio = kGoldenN8 + kGoldenN12`. La barra superior no usa esa segunda suma como altura estructural total, sino `kTopBarHeightRatio = kGoldenN7`, porque funciona como franja de alojamiento donde el cromo interactivo más bajo queda centrado. La barra inferior histórica se fija en `kBottomBarHeightRatio = kGoldenN6`, con fusibles derivados de la misma sucesión para evitar degeneración en ventanas pequeñas. El panel lateral derecho parte de `kSidePanelDefaultRatio = kGoldenN4` y conserva un máximo cercano al ancho visual anterior para permitir recuperar espacio lateral sin volver a imponerlo como predeterminado. Ninguna barra, pestaña, pista, casilla o separación principal debe regresar a alturas en píxeles fijos; los pisos absolutos que subsisten son únicamente defensas contra colapso rasterizado o pérdida de legibilidad.

La piel visual vigente no se gobierna por bordes ni por separadores, sino por masas sólidas, radios, sombras suaves, espaciados y jerarquía tipográfica. `apply_minimal_theme()` ya no debe convertirse en una segunda retícula con `FramePadding`, `ItemSpacing`, `TabRounding`, `GrabRounding`, `ScrollbarSize` o medidas equivalentes fijadas por píxeles arbitrarios. Los controles nativos que quedan en Dear ImGui toman su grosor real de `golden_tab_frame_pad_y()`, que calcula la mitad exacta del excedente entre `golden_tab_chrome_height()` y la altura tipográfica visible. La relación buscada es que el texto ocupe cerca de `0.618` del alto del control y que el excedente se distribuya como respiración simétrica. Si se ajusta la escala visual futura, el cambio debe hacerse en la fuente global o en el alto canónico de control, no con offsets locales, `SetCursorPosY`, `FramePadding.y` inflado ni fuentes particulares por widget.

Los botones y las pestañas superiores vigentes no se describen como `ImGui::Button` crudos. El código actual usa plantillas propias con hitboxes registradas mediante `InvisibleButton`: `draw_golden_button_template()` para botones, `draw_golden_tab_template()` para pestañas y envoltorios como `draw_golden_button_flow()` para el flujo normal. La plantilla de botón usa las cuatro esquinas redondeadas, relleno sólido, sombra sutil y radio proporcional al alto seguro del control mediante un escalón áureo compacto. La plantilla de pestaña mantiene identidad propia: redondea sólo la parte superior, se integra por su base a la barra y usa `golden_visible_tab_rounding()` para que la curvatura de una pestaña abierta no se lea más débil que la de un botón cerrado. `golden_tab_rounding()` sigue siendo el radio canónico de controles nativos residuales, no el radio visual aumentado de las pestañas manuales. Reintroducir `AddRect`, `AddLine`, contornos, separadores o botones nativos para resolver jerarquía visual rompe el contrato vigente.

El texto de botones y pestañas vigentes se calcula con `golden_chrome_label_font_px()`, pero esa función devuelve la fuente global efectiva de ImGui y no deriva un tamaño local desde el alto del control. `golden_chrome_text_top_gap()` y `golden_chrome_text_bottom_gap()` reparten actualmente el excedente vertical por mitades, por lo que las descripciones antiguas que hablaban de un sesgo superior mediante `kGoldenN2`, de una barra superior `kGoldenN8` o de un alto de control igual a `kGoldenN7` ya no son la autoridad vigente. La comodidad visual de controles debe resolverse con padding horizontal, ancho automático y distribución centralizada del excedente, nunca aumentando la tipografía del botón, desplazando cada rótulo por separado o recuperando márgenes dentro de las plantillas.

Las casillas visibles vigentes forman una sola familia gráfica. `golden_checkbox_side()` no equivale al alto completo de `golden_tab_chrome_height()`, sino a un lado compacto de `golden_tab_chrome_height() * kGoldenN1`; esa reducción es deliberada porque una casilla cuadrada, sólida y de cuatro lados tiene más masa perceptual que una pestaña horizontal. `golden_checkbox_rounding()` permanece anclado al cromo canónico completo mediante `golden_tab_chrome_height() * kGoldenN4`, para no degradar el redondeo aprobado de controles nativos que comparten esa familia. `draw_golden_checkbox_visual_template()` concentra el dibujo: relleno cromático, sin borde, apagado mortecino cuando está inactiva, realce por hover, palomita UTF-8 directa para selección y guion simple para estado mixto. `draw_golden_checkbox_template()` añade la hitbox de flujo, `draw_color_check_square()` queda como alias de compatibilidad, y `draw_golden_checkbox_labeled()` ya no debe reimplementar casillas pequeñas ni usar `ImGui::Checkbox`. El clic izquierdo alterna selección y el clic derecho conserva las rutas de aislamiento donde el panel lateral lo exige.

El panel lateral derecho vigente es un inspector de visibilidad, búsqueda y lectura de capas, no un segundo tablero temporal ni epidemiológico. No debe volver a mostrar encabezados persistentes con semana epidemiológica, conteos de casos filtrados, totales acumulados o mensajes de ausencia de datos que dupliquen la barra histórica. Las filas jerárquicas mantienen la gramática editorial de viñeta triangular, casilla cromática y texto. La viñeta abre o cierra; la casilla selecciona; el texto identifica. Las categorías y subcategorías calculan su estado vacío, parcial o completo desde los elementos visibles y respetan el filtro de búsqueda. Demografía, movilidad, estaciones, contaminantes, enfermedades, sincronización anual y controles booleanos usan el mismo módulo de casilla. La movilidad vigente se controla por su categoría superior y por sistemas internos visibles como Metro, Metrobús, Transportes eléctricos, Trenes, RTP, Pumabús, Concesionados y Otros; no debe reaparecer una subcategoría intermedia de `Sistemas visibles` ni un segundo interruptor paralelo de la misma capa.

La ventana principal conserva marco nativo y barra superior propia. Las pestañas principales y de gráfica se dibujan en la barra superior sin invadir el mapa. En vista principal permanecen los accesos globales de importación, exportación y configuración; en vistas de gráfica aparece el acceso contextual para nueva gráfica si corresponde. Los botones superiores `Importar Datos` y `Exportar Datos` no abren ventanas flotantes externas: sólo seleccionan la página interna correspondiente dentro de `Configuracion` y fuerzan apagado de banderas heredadas. El botón de configuración es cuadrado, comparte el alto canónico de los demás controles de la barra y no debe recuperar padding vertical propio. Las rutas que intenten revivir `show_import_window`, `show_export_window` o superficies independientes se consideran rutas heredadas de saneamiento, no superficies vigentes.

`Configuracion` es la superficie única para usuario, importación y exportación. El índice izquierdo debe mantenerse como índice estrecho, no como panel dominante, con proporción áurea menor y cotas de seguridad. La página `Usuario` conserva campos de perfil como nombre, idioma y tema, un avatar circular más grande que el mínimo de control, inicial proporcional y padding común de contenido más ancho horizontalmente que verticalmente. Ese padding no debe trasladarse a `FramePadding`, porque modificaría la altura de combos, cajas y botones. La corrección vigente de la aserción roja de Dear ImGui en casillas etiquetadas está centralizada en la plantilla compartida: primero se registra la casilla canónica y después se añade el texto como ítem normal, sin construir un segundo cuadrado local ni depender de un `Dummy` artificial como arreglo final. Cualquier ajuste de usuario debe preservar hitboxes reales y flujo formal de ImGui.

`Importar datos` y `Exportar datos` existen como páginas internas de `Configuracion`; las funciones `draw_import_window()` y `draw_export_window()` son ventanas sepulcro que redirigen o apagan estado heredado y no deben contener `ImGui::Begin("Importar Datos")` ni `ImGui::Begin("Exportar Datos")`. El importador vigente se organiza por tipo físico de archivo: `CSV, JSON y XLSX`, `NetCDF, HDF y GeoTIFF`, `SHP, GeoJSON y PBF`, y `PDF`. La pestaña visible debe responder al formato que entra al programa; dominio, proveedor, disciplina, receta y fuente son metadatos internos. No debe volver una pestaña genérica por categoría temática ni un importador segmentado por proveedor si eso rompe la validación física del archivo.

Todas las pestañas del importador vigente comparten la misma gramática: pestañas superiores, una sola barra global de progreso, una línea compacta de salud y controles globales, una primera línea operativa con fuente o tipo documental, selector `Local/Web` y rango de años, y una segunda línea operativa con ruta larga y botón `Seleccionar carpeta`. El botón de ciclo de vida vive únicamente en la cabecera compacta. Cuando no hay tarea activa inicia importación; durante una tarea puede pausar, reanudar o detener de forma total según la semántica de interacción preservada. No deben reaparecer botones paralelos de importar por pestaña, actualizar, indexar, rehacer archivos internos, redescargar o abrir documentación antes de que exista un estado final observable. La ruta no debe subir a la fila de años, el selector de años no debe bajar a otra línea mientras haya ancho razonable, y la segunda barra de progreso dentro de tarjetas, pies o previews está prohibida.

La altura del importador vigente converge en la altura canónica del cromo PHI. Pestañas, botones sólidos, menús desplegables, cajas de dirección y selector inline de años comparten envolvente de interacción. Los controles nativos del importador se protegen con un scope local de altura que ajusta `FramePadding` sólo para que `Combo` e `InputText` igualen la altura de las plantillas manuales. La barra de progreso y la pista de años no son controles principales: comparten un grosor visual menor mediante una función de pista secundaria y, en el punto vigente más estricto, la barra compacta de progreso no debe añadir gap intrínseco ni avanzar el cursor más allá del grosor dibujado. La respiración vertical del bloque pertenece al layout explícito del importador, no a la barra como efecto secundario. El selector `Local/Web` calcula su ancho desde las mismas métricas de los botones canónicos, y el botón `Seleccionar carpeta` se mide con la plantilla vigente, no por anchuras mágicas.

Los límites temporales del importador no son globalmente 2019. PDF CDMX conserva blindaje 2019+, Edomex conserva rango histórico independiente, RAMA conserva inicio operativo amplio, movilidad conserva su cronología propia, satelital o raster conserva su cronología técnica y las fuentes externas usan el `min_year` de su especificación. En modo local, las carpetas de CDMX toleran grafías con y sin acento, variantes como `PDF extraídos`, `PDF extraidos`, `PDF`, rutas bajo `CDMX`, `SALUD`, `Datos` y `Descargas`, y sólo publican registros si hay contenido importable real. En modo web, una respuesta pequeña de metadatos no cuenta como dato incorporado. Las fuentes CKAN se resuelven hacia recursos reales y `datastore/dump` cuando existe archivo primario descargable. Las fuentes que requieren cuenta, token, licencia o descarga masiva quedan como receta reproducible hasta que el usuario coloque archivos primarios indexables. Un catálogo HTML, una auditoría de transferencia, un `package_show` o una receta de adquisición no deben inflar contadores, crear capa visual ni refinar la nube atmosférica.

La importación estable converge hacia IXIPTLAH mensual. La unidad física vigente es `Datos/AAAA_MM.ixiptlah`, no un archivo por fuente, variable, descarga ni familia. Dentro de cada mes conviven núcleos tipificados: epidemiología, atmósfera, movilidad, cartografía histórica, inventarios de fuente y resúmenes visuales. Un proceso que reconstruye atmósfera no debe borrar núcleos epidemiológicos; un proceso que reconstruye epidemiología no debe borrar núcleos atmosféricos; los metadatos de descarga no deben escribirse como casos ni como estaciones RAMA; un archivo sin fecha válida sólo puede caer como inventario defensivo, no como medición visible. NetCDF, HDF, GeoTIFF, GRIB, PBF, SHP y ZIP grandes pueden asentarse como linaje mensual, pero para modificar el mapa necesitan conversión explícita a observaciones, mallas o registros legibles. Los artefactos auxiliares viven como caché, bitácora o staging, no como verdad analítica paralela.

El extractor epidemiológico vigente trata los boletines como sopa de letras documental. Los encabezados multilínea se reconstruyen desde líneas textuales no estructurales contiguas ubicadas sobre el CIE-10 de la misma columna, sin arrastrar códigos CIE, `Rev.`, cláusulas `excepto`, años, rótulos `Sem/Acum/M/F` ni títulos del cuadro. El CIE-10 funciona como respaldo fuerte de identidad, pero no debe contaminar la etiqueta humana. Las cláusulas de exclusión como `excepto B18.0 y B18.1` se conservan como parte del rango de códigos, para evitar que códigos excluidos entren a la identidad epidemiológica. La carga visual deja de usar el rótulo degradado como identidad primaria cuando existe código CIE-10 o alias catalogado: IXIPTLAH, TSV, CSV y JSONL se canonicen contra `diseases.tsv` y `diseases_historial.tsv` antes de llegar al panel, al mapa o a las gráficas. Las filas equivalentes tras canonización se deduplican por semana, jurisdicción, enfermedad, año fuente, periodo, sexo y métrica. Las ampliaciones del catálogo deben ocurrir en TSV con alias y CIE-10, no con excepciones visuales dentro del panel lateral.

La reconstrucción de tablas epidemiológicas conserva validación conservadora. Ante ambigüedad de fecha, jurisdicción, sexo, periodo, métrica o unidad, la fila debe degradar localmente a cuarentena o diagnóstico, no contaminar el núcleo mensual. La semana epidemiológica visible no debe duplicarse en el panel lateral, pero el mapa conserva claves temporales internas no visibles para invalidar popups o estados fijados cuando cambia la semana o el intervalo. La variable lógica que sustituye referencias visibles antiguas debe entenderse como contexto interno del mapa, no como obligación de volver a dibujar conteos o semana en el panel derecho. La separación entre lectura, aceptación, publicación y renderizado es obligatoria: renderizar nunca debe retroalimentar la verdad persistida.

La atmósfera vigente conserva una frontera propia entre RAMA/REDMET, fuentes externas, satélite y recetas. RAMA/REDMET puede actualizar el campo atmosférico porque existe decodificador horario y reglas de agregación. Las fuentes externas heterogéneas se inventarian con linaje, huella, tamaño, fecha, proveedor, dominio y receta; no se reinterpretan como estaciones RAMA ni como observaciones clínicas. La nube atmosférica sólo se refina cuando hay observaciones o mallas decodificadas por importadores específicos. Los productos satelitales y raster no deben crear una raíz IXIPTLAH aparte; sus crudos pueden vivir bajo datos externos, pero sus índices y registros mensuales deben integrarse al archivo mensual correspondiente. La visualización atmosférica debe seguir distinguiendo radio cartográfico de representatividad, radio físico del sensor, distancia causal de exposición e interpolación validada.

La capa de mapas históricos vigente no es una mancha inventada por Tlalpowa. Las fuentes lacustres autoritativas aceptadas son archivos locales o cacheados trazables, en particular el GeoJSON de Lago de Texcoco 1519 del flujo CDMX/IPDP cuando está disponible y el KML/KMZ `Cuenca-Lagos` de REPSA/UNAM bajo sus nombres reales o equivalentes locales. Las raíces buscadas incluyen la configuración histórica de `Fuente`, carpetas de datos y descargas, y la caché autoritativa de `Build/cache_mapa/historicos_autoritativos`. Si la fuente oficial no existe o no pasa validaciones mínimas de tamaño, firma, coordenadas y contenido, la capa debe quedar vacía; la ausencia de fuente oficial no se sustituye con polígonos aproximados internos, huellas máximas inventadas, núcleos de profundidad derivados de centroides, KML/KMZ empacado artificialmente ni parches raster tipo Thomas Kole. Las descripciones antiguas que permitían fallback lacustre aproximado sólo son historia y no deben regir el render estricto vigente.

Cuando existe geometría histórica oficial, se dibuja como perímetro real con velo mínimo y no como triangulación convexa. `draw_historical_polygon_scanline_fill()` usa barrido horizontal par-impar para rellenar franjas interiores de anillos que pueden ser cóncavos, con bahías, estrechamientos y cientos de vértices. `draw_historical_external_kml_layer()` y `draw_historical_lacustrine_layer()` deben usar ese relleno o una triangulación robusta, no `AddConvexPolyFilled()` sobre anillos hidrográficos reales, porque esa primitiva forma abanicos o telarañas internas en polígonos no convexos. El perímetro se cierra explícitamente último→primero, después de normalizar duplicados consecutivos y coordenadas finales repetidas. El render histórico impone anti-anacronismo: desde el año 2000 no debe pintar superficies lacustres históricas activas, geometrías KML/KMZ antiguas ni huellas máximas sobre el presente. En rangos antiguos, la geometría puede mostrarse como contexto histórico sin sustituir el mapa satelital ni capas analíticas.

La lectura de KMZ vigente es acotada y no debe bloquear el render. Primero se intenta leer KML almacenado sin compresión dentro del KMZ, y si el archivo llega comprimido en Windows se puede extraer de forma confinada a la caché histórica autoritativa mediante el extractor robusto del proyecto. Las descargas, conversiones o extracciones no pertenecen a `draw_map()` como trabajo síncrono pesado. Las funciones de mapas históricos deben reexplorar periódicamente las raíces aceptadas porque una descarga en segundo plano puede terminar después del primer frame; cachear una lista vacía como definitiva sería una regresión. La profundidad visual de 5 m, cuando se use como atributo de lectura, no es un modelo batimétrico continuo ni autoriza generar superficies inventadas.

La línea temporal vigente opera sobre una época interna compatible con cachés y núcleos binarios, pero la interfaz publicada se acota desde 1500-01-01 hasta la fecha local actual más seis meses calendario. Arrastre, rueda, reproducción, intervalos, edición de campos y saltos pasan por `clamp_timeline_navigation_hour()`, por lo que ningún gesto debe dejar el pin rosa fuera del dominio navegable. El límite superior se calcula desde hora local cruda, suma seis meses calendario y corrige el día al último día válido del mes destino, evitando recursión indirecta y reduciendo costo por frame mediante caché. Las funciones de formato y conversión temporal usan división hacia menos infinito y módulo positivo para fechas anteriores a 1970, evitando deformar día u hora por truncamiento hacia cero. La semana visible es derivada de la fecha civil activa, no estado independiente.

La barra histórica inferior vigente debe leerse como un eje continuo de navegación temporal con pista compacta, pin rosa central, fecha activa elevada y controles invisibles por campo. La pista visible es una cápsula única de color, con hitbox más amplia que su grosor real para conservar arrastre cómodo. El clic simple sobre la pista o el pin sirve para arrastrar o seleccionar; el doble clic sobre el pin alterna reproducción/pausa. Reproducir no debe separarse de la fecha activa ni reactivarse por clic simple accidental. La fecha activa no debe regresar a una caja rectangular tradicional ni a un `InputText` visible; cada fragmento visible conserva zona invisible de interacción para rueda o edición. La rueda sobre semana desplaza siete días exactos, manteniendo hora y minuto y pasando por el clamp global. La edición por teclado captura caracteres desde `ImGuiIO::InputQueueCharacters`, maneja Backspace, Delete, Tab, Enter y Escape, y aplica día, mes o año por la misma ruta de clamp que rueda y arrastre.

La segmentación temporal vigente separa marcas por jerarquía semántica. El año activo muestra meses con abreviaturas de tres letras cuando hay espacio; los días del mes activo pueden vivir dentro de la cápsula de la pista; meses, años, décadas y siglos se dibujan como referencias contextuales de menor saturación y se suprimen localmente bajo la fecha activa cuando solapan con el pin central. Las descripciones antiguas que alternaban entre etiquetas inferiores, etiquetas superiores, footer `kGoldenN5`, footer `kGoldenN7` o escala cartográfica dentro de `pn.hist` no son la autoridad si contradicen el código vigente: actualmente `kBottomBarHeightRatio` es `kGoldenN6`, y la escala cartográfica compacta existe como overlay bajo dentro del mapa mediante `draw_compact_map_scale_overlay()`, no como contenido obligado del footer. La escala se calcula desde el viewport visible, usa distancias redondeadas con `nice_scale_meters()` y debe mantenerse separada del estado temporal si el código vigente la emite sobre el mapa.

Tonalli vigente es una lectura supracartográfica ligada a la fecha activa, no una decoración de la barra inferior que deba comprimir el footer. La casilla `Tonalli` permanece dentro del footer, en la esquina superior derecha, con el mismo módulo `golden_checkbox_side()` que las casillas laterales. El bloque Tonalli se dibuja después de procesar el control, con el estado efectivo del mismo fotograma, mediante foreground draw list, por encima del mapa y de capas cartográficas. La lectura se ancla visualmente al entorno del pin y la fecha activa, sin borde, con glifo cuadrado y leyenda inferior. Para fechas anteriores a 1521 o 1522 según la regla de activación histórica del módulo, el tonalli puede activarse automáticamente como lectura histórica; para fechas posteriores depende de la casilla. El cálculo modular del tonalpohualli mantiene el anclaje operativo `13/08/1521 -> 1 Coatl` y dibuja glifos vectoriales en tiempo real sin archivos ornamentales externos.

La pestaña de gráficas vigente conserva una galería transformable. Ya no existe un recuadro superior permanente que bloquee espacio para avisar que no hay observaciones; la ausencia de datos se informa dentro de cada gráfica creada. La galería 4×2 crea plantillas diferenciadas: puntos XY como dispersión, barras discretas, líneas, círculos de magnitud radial, área rellenada bajo línea, histograma por bins, correlación O3-incidencia y puntos O3 como dispersión ambiental, no como clon de correlación. Cuando una gráfica no tiene datos filtrados, debe mostrar previsualización tenue coherente con su tipo. Al crear una gráfica, la galería consume su espacio y reaparece debajo si aún hay cupo, evitando quedar como cabecera fija. Cada gráfica conserva selección por clic, menú contextual, configuración de ejes, duplicación, eliminación, exportación CSV y copia PNG diferida hasta después del render de ImGui/OpenGL; en Windows se publica PNG y CF_DIB como respaldo. El área de gráficas no debe recuperar márgenes externos, filetes, bordes ni separadores: la separación visual nace de masa, radio y padding interno.

El empaquetado y compilación vigentes se describen con cautela porque los documentos antiguos no coinciden por completo con los archivos base revisados. El `Compilar_Tlalpowa.cmd` actualmente valida que existan `CMakeLists.txt`, `Fuente` y `Fuente\Config\diseases.tsv`; si falta `diseases.tsv`, entra a `missing_config` y falla con diagnóstico, por lo que no debe documentarse como vigente una restauración automática desde `Fuente\Fuente\Config`, desde `Fuente.zip` o desde un TSV mínimo de emergencia mientras ese código no exista en el script. El compilador sí prepara entorno MSVC, CMake y Ninja o NMake, regenera `Tlalpowa.ico` desde `tlalpowa.png` si existe, configura CMake hacia staging en `Build\Producto`, compila, copia el ejecutable final a la carpeta base, empaqueta dependencias si el stage las contiene, ejecuta selftest salvo que se omita y conserva el mensaje de que la carpeta base no se convierte en repositorio Git. La política del publicador GitHub que aparecía en bitácoras anteriores no queda descrita como función vigente de `Fuente` porque en los archivos fuente revisados no hay implementador de ese publicador dentro del árbol empacado.

El proyecto vigente mantiene `CMakeLists.txt` como ensamblador de `Tlalpowa` con C y C++, salida runtime configurable, fuentes centrales en `Fuente`, icono Windows generado desde `tlalpowa.png` cuando existe, dependencia base de `nlohmann_json` fuera del bloque ImGui porque los manifiestos JSON se usan también en rutas no gráficas, y soporte ImGui/GLFW/OpenGL opcional mediante dependencias globales o `FetchContent`. La copia de Poppler portátil ocurre si la dependencia existe bajo la raíz global. La rama web está retirada del ensamblaje conservado: no debe reintroducirse una plantilla HTML/CSS/JS ni un generador estático como sustituto del visor nativo. La deduplicación epidemiológica pertenece a identidad analítica y no debe quedar acoplada a salidas estáticas ajenas al runtime gráfico.

Los comentarios de no regresión del código forman parte del estado vigente siempre que describan comportamiento real cercano. Su función es proteger fronteras: dato crudo no es dato aceptado, dato aceptado no es publicación, publicación no es renderizado, y renderizado no debe reescribir la verdad persistida. También fijan límites de concurrencia: las importaciones pueden avanzar mientras la UI renderiza, pero la interfaz sólo debe observar estados consistentes de detectados, indexados, aceptados, cuarentena y mensajes humanos. Las lecturas no bloqueadas de estructuras complejas, las escrituras parciales sin publicación atómica, las limpiezas destructivas de shards mensuales compartidos y los atajos visuales que se vuelven datos son regresiones. La regla general de Tlalpowa sigue siendo acumular, refactorizar y endurecer sin retirar funciones vigentes: cuando una bitácora antigua contradiga el código actual, debe prevalecer la fórmula o rutina actual; cuando el código actual no implemente una promesa de bitácora, no debe describirse como capacidad real hasta que exista una implementación revisable.

## Corte de lectura caliente IXIPTLAH / 2026-06-09

La interfaz permanece en C++ sólo para ventana, ImGui, hilos de presentación y estado visual. La ruta nueva `tlalpowa_hotdata.c` es C puro y se ejecuta durante la pantalla de carga para recorrer `Datos`, leer directorios terminales IXIPTLAH V1 sin materializar payloads completos, localizar llaves temporales reales por familia y priorizar siempre la fecha activa al navegar, tocando en caché del sistema payloads reales de esa fecha o de su vecino físico más cercano. Este calentamiento no escribe sidecars, no resume datos y no sustituye la lectura analítica: sólo reduce latencia posterior de apertura, seek y page-cache al navegar por fechas.

La pantalla de carga queda como barrera útil: además de catálogos, mapa y fechas reales disponibles, calienta IXIPTLAH y encabezados/mallas 3D con presupuesto acotado por `TLALPOWA_HOTDATA_TOUCH_MB` y `TLALPOWA_HOTDATA_RECORD_MB`, ambos limitados para permanecer bajo el contrato de RAM ligera. La carga epidemiológica inicial usa las fechas activas o registros reales disponibles, no una fecha sintética de calendario; la navegación atmosférica conserva el prefetch progresivo adelante/atrás sobre datos reales una vez seleccionado el contaminante.

## 2026-06-09 · Lectura temporal caliente V2 en C puro

La ruta `tlalpowa_hotdata.c` construye un índice temporal residente, compacto y de sólo metadatos, alimentado directamente desde los directorios terminales IXIPTLAH V1. En plataformas compatibles usa mapas de memoria de lectura para recorrer directorios y tocar páginas reales de payload sin copiar bloques completos a heap; si la plataforma no lo permite, conserva ruta `fopen/fread` acotada. La pantalla de bienvenida ya no espera sondeos masivos ni vecinos cronológicos: sólo indexa y deja cargados los últimos registros reales por categoría disponible. Con la interfaz viva, la fecha activa de epidemiología, meteorología y contaminantes se recalienta inmediatamente y los vecinos adelante/atrás se cargan en segundo plano por cercanía temporal.

La interfaz sigue siendo C++ únicamente para ventana, ImGui, hilos y estado visual. Al moverse la línea temporal, la interfaz sólo emite una pista no bloqueante; la lectura física real permanece en `tlalpowa_hotdata.c`, que busca registros cercanos por llave temporal y toca pequeñas ventanas de payload alrededor de epidemiología, meteorología y contaminantes. No se crean resúmenes ni sidecars; se leen páginas reales de los datos correspondientes y se mantiene el índice dentro de memoria ligera.

## 2026-06-09 — Hotdata V3: índice temporal binario y caché real de payload

Se refuerza `tlalpowa_hotdata.c` sin mover la interfaz C++: durante la pantalla de carga el núcleo C puro no sólo recorre IXIPTLAH, sino que deja un orden temporal residente y una caché LRU de payload real, acotada por `TLALPOWA_HOTDATA_CACHE_MB` y `TLALPOWA_HOTDATA_CACHE_LINES`. El cambio evita búsquedas lineales al navegar fechas: `tlalpowa_hotdata_prefetch_temporal()` pasa a localizar ventanas por búsqueda binaria sobre el índice temporal y expande alrededor de la fecha solicitada para epidemiología, meteorología y contaminantes. La representación queda preparada con `tlalpowa_hotdata_find_nearest()` y `tlalpowa_hotdata_read_hit()`, que devuelven y leen registros reales de esa fecha o su registro temporal más cercano, sin resúmenes ni sidecars.

Variables nuevas: `TLALPOWA_HOTDATA_CACHE_MB` controla memoria residente de payload caliente; `TLALPOWA_HOTDATA_CACHE_LINES` controla el número máximo de entradas LRU. El presupuesto sigue lejos de 500 MB por diseño: el índice sólo guarda metadatos compactos y la caché se limita explícitamente.

### 2026-06-09 — Hotdata V4: órdenes temporales por familia y recolección de ventana real

Se refuerza de nuevo `tlalpowa_hotdata.c` manteniendo la interfaz en C++ sólo para ventana/ImGui. La ruta caliente de datos sigue siendo C11 puro y ahora separa el índice temporal en órdenes compactos por epidemiología, meteorología, contaminantes y otros, además del orden global. Esto evita que cada cambio de fecha escanee registros de familias ajenas: la búsqueda del registro físico cercano pasa a usar el orden ya específico del tipo de dato, con expansión bidireccional alrededor de la llave temporal solicitada. La función nueva `tlalpowa_hotdata_collect_window()` entrega una ventana de hits reales ya ordenados por cercanía temporal; no genera resúmenes ni sidecars y sólo devuelve offsets/tamaños/rutas a payloads IXIPTLAH reales, listos para lectura directa con `tlalpowa_hotdata_read_hit()`.

La pantalla de carga conserva su prioridad: al abrir el programa se indexan los directorios terminales IXIPTLAH, se deja lista una barrera inicial y la navegación posterior siempre recalienta la fecha activa de epidemiología, meteorología y contaminantes, se toca malla 3D de forma acotada y se deja una caché LRU de payload real. La navegación posterior por fecha reutiliza esos órdenes específicos y sólo lee ventanas pequeñas adelante/atrás, evitando barridos completos y manteniendo memoria acotada por `TLALPOWA_HOTDATA_CACHE_MB` y `TLALPOWA_HOTDATA_CACHE_LINES`.

## 2026-06-09 — Hotdata V5: ventana temporal preparada para representación real

Se refuerza la ruta caliente `tlalpowa_hotdata.c` sin convertir la interfaz a C: la ventana/ImGui permanece en `Tlalpowa.cpp`, pero la selección, lectura y preparación física de datos quedan en C11 puro. Se agregó `tlalpowa_hotdata_prepare_temporal_view()`, que en una sola llamada localiza por búsqueda binaria la ventana real de registros IXIPTLAH cercanos a la fecha, devuelve los hits físicos y carga bytes reales de cada payload al caché LRU residente. La interfaz ya no sólo lanza un prefetch genérico: al mover la fecha prepara ventanas reales separadas de contaminantes, meteorología y epidemiología, con buffers fijos, sin STL, sin sidecars y sin resúmenes.

La mejora reduce asignaciones repetidas y evita dobles recorridos: `tlalpowa_hotdata_collect_window()` y la nueva preparación comparten el mismo colector interno de índices temporales. Cada hit conserva ruta, offset, tamaño, tipo, esquema, familia y llave temporal; la representación puede leer directamente con `tlalpowa_hotdata_read_hit()` y, si el payload fue preparado durante la pista temporal, la lectura sale del caché de bytes reales. La pantalla de carga sigue intacta como primer estado visible y continúa cargando la fecha activa y después los vecinos reales por cercanía temporal durante la navegación progresiva adelante/atrás.

## 2026-06-09 — Hotdata V6: mapas retenidos y lectura directa sin reapertura

La ruta `tlalpowa_hotdata.c` conserva C11 puro y endurece la lectura real de payloads IXIPTLAH. Los archivos IXIPTLAH que ya fueron mapeados durante la pantalla de carga pueden quedar retenidos dentro del índice residente bajo un límite explícito de mapeo, evitando reabrir el mismo archivo en cada cambio de fecha. `tlalpowa_hotdata_read_hit()` y la caché LRU ahora intentan copiar directamente desde el mapa retenido antes de caer a `fopen/fread`, por lo que la representación de la fecha activa reduce llamadas al sistema, seeks repetidos y asignaciones indirectas. La retención queda acotada para conservar portabilidad y bajo consumo: si el conjunto de IXIPTLAH rebasa el límite interno, el archivo se desmapea y la ruta segura por lectura directa de archivo permanece disponible.

La pantalla de carga sigue siendo la primera escena visible y continúa haciendo trabajo real, pero ya no debe bloquear por minutos: mapea directorios terminales, indexa registros y prepara la hotdata inicial vigente: últimos 10 IXIPTLAH reales por categoría física. El 3D y los vecinos cronológicos amplios quedan fuera de la barrera de bienvenida salvo activación explícita por variable de entorno. La regla de navegación queda fija: fecha activa primero; después vecinos adelante/atrás por cercanía temporal. No se crean resúmenes ni sidecars; cada lectura conserva offset, tamaño y payload real de IXIPTLAH.

## 2026-06-09 — Hotdata V7: barrera estricta antes de desvanecer bienvenida

La ruta IXIPTLAH queda preparada antes de liberar la pantalla de bienvenida, y desde V11 esa preparación ya no es mínima: `tlalpowa_hotdata_prewarm_root()` indexa, retiene mapas bajo límite y ejecuta la hotdata inicial de últimos 10 registros reales por categoría física (`núcleo/tipo/esquema/capa`). La interfaz sólo marca `startup_hotdata_ready` si hubo IXIPTLAH reales, registros indexados y al menos un hit de ventana de arranque cargado; los vecinos cronológicos ya no forman parte de esta barrera.

Variables nuevas de ajuste: `TLALPOWA_HOTDATA_STARTUP_GATE_RECORDS` y `TLALPOWA_HOTDATA_STARTUP_GATE_KB`. Desde V11 esos valores fijan la hotdata inicial: el primer plano carga últimos 10 registros disponibles por categoría física y delega vecinos al segundo plano. La ruta sigue sin sidecars, sin resúmenes y sin tocar `Datos`; el arranque sólo deja una barrera inicial real antes de la navegación, y el caché de vecinos se llena después en segundo plano.

### V8 — Hot data estrictamente anclada a la fecha activa

La regla queda fijada en código y comentarios: la hot data no sirve una fecha terminal si el usuario está parado en otra fecha. La ruta C pura `tlalpowa_hotdata_prepare_active_temporal_view()` carga primero registros IXIPTLAH reales de la llave temporal solicitada o del registro físico más cercano; después precalienta vecinos adelante/atrás en orden de distancia temporal, del más cercano al más lejano. La interfaz emite una preparación sincrónica pequeña al detectar cambio de fecha para dejar la fecha activa en caché antes del uso visual inmediato, y luego expande en segundo plano con una ventana más amplia. No hay resúmenes, sidecars ni agregados sustitutos: cada hit conserva ruta, offset, tamaño, tipo, esquema, familia y llave temporal del payload real.

### V9 — Regla histórica sustituida: vecinos cronológicos en segundo plano

La bienvenida de TLALPOWA no debe esperar el prewarm progresivo completo. Esta regla histórica queda sustituida por V11: el primer plano carga últimos 10 IXIPTLAH reales por categoría física mediante `TLALPOWA_HOTDATA_STARTUP_GATE_RECORDS=10`, `TLALPOWA_HOTDATA_STARTUP_CATEGORY_LIMIT=512` y `TLALPOWA_HOTDATA_STARTUP_GATE_KB=64` como valores base. Una vez marcada `startup_hotdata_ready`, la interfaz se libera y un hilo de baja prioridad empieza a cargar vecinos cronológicos desde el más cercano hacia el más lejano alrededor de las últimas fechas por categoría. Al cambiar de fecha, la preparación pequeña de la fecha activa sigue siendo inmediata; la expansión amplia permanece en segundo plano. Esta regla sustituye cualquier comentario anterior que sugiriera esperar vecinos, sondeos masivos o 3D antes de desvanecer la bienvenida.

### V10 — Bienvenida de primer plano con últimos registros disponibles por categoría

La regla de V10 queda sustituida por V11: los últimos registros no son la fecha civil actual ni la fecha local del equipo. En Tlalpowa casi nunca habrá datos regionales actualizados al día presente, por lo que la barrera de bienvenida debe cargar los últimos 10 IXIPTLAH realmente disponibles por categoría física encontrada (`núcleo/tipo/esquema/capa`). La bienvenida puede permanecer más tiempo para asegurar ese primer plano real, pero no debe convertirse en barrido total: toma hasta diez payloads por categoría bajo `TLALPOWA_HOTDATA_STARTUP_CATEGORY_LIMIT`, `TLALPOWA_HOTDATA_STARTUP_GATE_KB` y `TLALPOWA_HOTDATA_STARTUP_TOUCH_MB`.

A los 2 segundos de arranque, si ese primer plano IXIPTLAH todavía no termina en Windows, la aplicación eleva temporalmente la clase del proceso a `ABOVE_NORMAL_PRIORITY_CLASS`; al terminar el primer plano, restaura la prioridad previa y permite desvanecer la bienvenida. Desde ese punto, los vecinos cronológicos quedan estrictamente en segundo plano y se cargan adelante/atrás por cercanía temporal, del más cercano al más lejano, sin bloquear la interfaz. No se crean resúmenes, sidecars ni agregados sustitutos: cada hit conserva ruta, offset, tamaño, tipo, esquema, núcleo y llave temporal del payload real.

### V11 — Hotdata inicial de últimos 10 registros por categoría

La regla vigente sustituye V10: la pantalla de bienvenida ya no se libera con un solo último payload por categoría. La hotdata inicial queda definida como los últimos DIEZ registros IXIPTLAH realmente disponibles por cada categoría física encontrada (`núcleo/tipo/esquema/capa`), sin usar la fecha civil actual ni exigir que los datos estén actualizados al día presente. Si una categoría tiene menos de diez registros, se cargan todos los disponibles. Esta barrera puede tardar un poco más que V9/V10 porque ahora prepara más datos útiles antes del primer uso visual, pero sigue sin convertirse en barrido histórico completo.

La carga de bienvenida usa `TLALPOWA_HOTDATA_STARTUP_GATE_RECORDS=10` como contrato base, `TLALPOWA_HOTDATA_STARTUP_CATEGORY_LIMIT=512` para no recortar categorías reales del conjunto regional normal, `TLALPOWA_HOTDATA_STARTUP_GATE_KB=64` para tocar payload real sin inflar RAM de forma innecesaria, y una caché con más líneas para no expulsar inmediatamente registros pequeños. El límite operativo sigue siendo por bytes (`TLALPOWA_HOTDATA_CACHE_MB`, `TLALPOWA_HOTDATA_STARTUP_TOUCH_MB`), no por deseo de cargar todo sin control; por tanto se conserva el presupuesto menor de 500 MB.

Después de que el usuario entra a la aplicación, la prioridad deja de ser la última fecha disponible y pasa a ser siempre la fecha/hora activa donde está parado el navegador temporal. Cada cambio de fecha ejecuta una preparación pequeña y sincrónica de esa llave o del registro físico más cercano; sólo después se expanden vecinos cronológicos adelante/atrás, del más cercano al más lejano, en segundo plano y con prioridad reducida. A los 2 segundos de bienvenida, si el primer plano IXIPTLAH no terminó en Windows, el proceso puede subir temporalmente a `ABOVE_NORMAL_PRIORITY_CLASS`; al terminar, restaura la prioridad previa y los vecinos continúan en hilo de fondo. Ningún comentario anterior debe interpretarse como autorización para usar resúmenes, sidecars, fecha civil actual o vecinos históricos como condición de desvanecimiento.


## 2026-06-09 — Hotdata V12: primera fecha visible antes del fade

La bienvenida ya no puede desvanecerse si sólo está lista la barrera de últimos 10 IXIPTLAH por categoría. Ahora también exige que la primera fecha visible quede preparada en caché real: al arrancar se alinea la línea temporal hacia la última fecha atmosférica disponible —contaminantes primero, meteorología después— y se calientan los registros reales de contaminantes, meteorología y epidemiología antes del fade. Tras entrar, cada movimiento de fecha ejecuta una ruta síncrona de alta prioridad para la fecha/hora activa y sólo después lanza vecinos cronológicos en segundo plano, reduciendo la espera visible posterior al desvanecimiento.

---

## modelado_atmosferico_arquitectura.md

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

---

## pipeline.policy.md

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

---

## TLALPOWA_ACTUALIZACION_2026_06_09_Z20_IXIPTLAH3D.md

# TLALPOWA · satélite z20 e IXIPTLAH3D

## Mapa satelital

La capa satelital queda limitada por contrato a z20. Desde la aproximación baja equivalente al umbral de 50 m se solicita z20; cualquier acercamiento posterior amplía la tesela z20 ya existente y nunca solicita z21, z22 ni z23. La caché satelital cambia a `world_imagery_openmaps_max_z20_v20260610` para evitar mezclar teselas antiguas generadas con la política z23.

## Tenochtitlan.blend

Las coordenadas fijas del flujo Tenochtitlan quedan establecidas en longitud `-99.1392933523`, latitud `19.4349566597`, escala `94.018982`, rotación `-0.0504` y escala vertical `1.0`. La salida nativa queda preferentemente como `.ixiptlah3d`, con lectura retrocompatible de `.tlalpowa3d`.

## Progreso Blender

El exportador de Blender corrige el cierre LRU de archivos temporales para no eliminar dos veces el mismo descriptor. Con ello la generación de contenedores grandes deja de abortar silenciosamente después del primer avance de la barra. Se añadió proceso directo desde coordenadas fijas para saltar la espera de confirmación manual cuando se quiera procesar de inmediato.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IMGUI_DOCKING_BUILD.md

# TLALPOWA 2026-06-10 — compilación ImGui docking

## Diagnóstico

La compilación fallaba porque el CMake anterior descargaba `ocornut/imgui` en tag `v1.90.9` de la línea master. Ese paquete no expone la API de docking/multi-viewports usada por Tlalpowa: `ImGuiWindowClass`, `ImGuiWindowFlags_NoDocking`, `ImGuiConfigFlags_ViewportsEnable`, `SetNextWindowViewport`, `SetNextWindowClass`, `GetWindowViewport`, `UpdatePlatformWindows` y `RenderPlatformWindowsDefault`.

## Corrección acumulativa

`CMakeLists.txt` ahora descarga `v1.90.9-docking` como dependencia separada `imgui_docking`, por lo que no reutiliza la caché previa `Build/_deps/imgui-src`. Las rutas de compilación se dirigen explícitamente a `Build/_deps/imgui_docking-src`.

## Sin marcha atrás funcional

Se conservan las correcciones previas: límite satelital duro z20, umbral cercano equivalente a 50 m, caché satelital z20, coordenadas fijas de `Tenochtitlan.blend`, salida preferente `.ixiptlah3d` y flujo directo IXIPTLAH3D.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_CARPETA_UNICA_CENTRO_AFUERA.md

# TLALPOWA ACTUALIZACION 2026-06-10 · IXIPTLAH3D CARPETA UNICA CENTRO-AFUERA

- Cada modelo 3D importado se instala en una unica carpeta directa dentro de Datos: `Datos/<modelo>_IXIPTLAH3D/`.
- No se crean subcarpetas internas `.tiles`; la tesela central, las teselas hermanas y los manifiestos viven juntos en esa carpeta.
- Cada archivo `.ixiptlah3d` representa una tesela espacial y mantiene validacion dura de 75 MB maximo por archivo.
- El archivo raiz `.ixiptlah3d` es la tesela central; las demas teselas se escriben radialmente del centro hacia afuera.
- El manifiesto declara `stream_order=center_out`, `storage_layout=single_model_folder_flat_tiles` y una secuencia radial explicita.
- El cargador respeta la secuencia radial del manifiesto; si no existe, reconstruye el orden centro-afuera desde los nombres de tesela.
- La subida a GPU de chunks visibles conserva orden centro-afuera dentro del conjunto visible y mantiene anillo cercano completo frente a LOD externo ultraligero.
- Se conserva compatibilidad de lectura con instalaciones antiguas que tengan carpeta `.tiles`, pero al instalar nuevas conversiones se aplana el contenido y se elimina esa subcarpeta.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_CENTRO_AFUERA_SOLIDO.md

# TLALPOWA ACTUALIZACION 2026-06-10 · IXIPTLAH3D CENTRO-AFUERA SOLIDO

- La exportacion IXIPTLAH3D escribe teselas estrictamente del centro del modelo hacia afuera.
- El manifiesto conserva `stream_order=center_out`, `center_tile_x` y `center_tile_y` para que la lectura mantenga el mismo orden.
- El cargador respeta el orden del manifiesto; si falta o se corrompe, reconstruye un orden radial desde los nombres de tesela.
- Los chunks 3D se reordenan internamente desde el centro del modelo y, durante dibujo, se suben a GPU desde la camara hacia afuera.
- Se conserva una tesela raiz central y todas las teselas hermanas son tratadas como un solo modelo continuo.
- Se conserva el limite duro de 75 MB por archivo `.ixiptlah3d`, con anillo cercano completo y anillo externo ultraligero.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_CONTRATO_SOLIDO.md

# TLALPOWA ACTUALIZACION 2026-06-10 · IXIPTLAH3D CONTRATO SOLIDO

- Se refuerza el contrato de carpeta unica: `Datos/<modelo>_IXIPTLAH3D/` contiene el archivo raiz, todas las tiles `.ixiptlah3d` hermanas y el manifiesto; no se aceptan subcarpetas `.tiles` en instalaciones nuevas.
- El manifiesto debe declarar `stream_order=center_out`, `storage_layout=single_model_folder_flat_tiles`, `tile_directory=.` y una `tile_sequence` que inicia en la tesela raiz central.
- La validacion rechaza tiles inexistentes, duplicadas, fuera de carpeta, residuales no declaradas, con firma invalida o mayores de 75 MB.
- La exportacion escribe cada contenedor mediante archivo temporal `.writing` y reemplazo atomico, evitando tiles corruptas si Blender se interrumpe.
- La regeorreferenciacion de modelos ya convertidos ahora opera sobre todo el tileset, no solamente sobre la tesela raiz.
- El cargador conserva la lectura centro-afuera y reconstruye el orden radial si falta manifiesto en instalaciones antiguas.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_FLUJO_CONFIRMACION.md

# TLALPOWA · IXIPTLAH3D flujo de confirmacion reforzado · 2026-06-10

## Objetivo

Reforzar el flujo completo desde la confirmacion de coordenadas del archivo `.blend` hasta la instalacion del contenedor `.ixiptlah3d` sobre el mapa satelital, sin depender de pasos intermedios ambiguos.

## Cambios

- `Tenochtitlan.blend` se reconoce al seleccionar o localizar el archivo y fija inmediatamente coordenadas, escala, rotacion y escala vertical.
- El boton `Confirmar coordenadas` convierte directamente `Tenochtitlan.blend` a `.ixiptlah3d` con esas coordenadas fijas.
- La conversion valida archivo `.blend`, salida, progreso y directorios antes de lanzar Blender.
- Si Blender falla, la barra no desaparece: queda visible con el error persistente y el diagnostico de progreso.
- El exportador Python escribe un estado de error en JSON si falla triangulacion, materiales, decimacion o escritura del contenedor.
- El exportador limpia seleccion de objetos antes de aplicar decimacion y evita propiedades Blender incompatibles entre versiones.
- La previsualizacion 4K prueba motores compatibles (`BLENDER_EEVEE_NEXT`, `BLENDER_EEVEE`, `BLENDER_WORKBENCH`) para no abortar por cambio de version.
- Al instalar correctamente el `.ixiptlah3d`, se actualizan los valores originales de georreferencia y el modelo queda cargado sobre satelite historico 1519.

## Politica

No se revierte ningun cambio previo: se conservan el hard-lock satelital z20, cache z20 nueva, docking build y salida preferente `.ixiptlah3d`.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_TILESET_75MB_ANILLOS.md

# TLALPOWA ACTUALIZACION 2026-06-10 · IXIPTLAH3D TILESET 75MB

- IXIPTLAH3D queda dividido como conjunto de teselas: un archivo .ixiptlah3d por tesela espacial.
- El archivo principal conserva la identidad del modelo; las teselas hermanas se instalan en `<modelo>.ixiptlah3d.tiles/` y se leen como un solo modelo continuo.
- Cada tesela .ixiptlah3d se valida con límite estricto de 75 MB; si una tesela rebasa ese límite, la exportación aborta con diagnóstico visible.
- El exportador cambia a retícula 64x64 para contener tamaño por archivo y sostener alta resolución local sin concentrar demasiada geometría en un solo bloque.
- El anillo cercano conserva fidelidad completa del blend; el anillo externo usa LOD ultraligero de muy baja densidad.
- El cargador GPU agrega todos los archivos de tesela al mismo índice espacial y selecciona por cámara: <=15 m usa LOD completo; exterior usa LOD ultraligero.
- La residencia GPU del 3D nativo se mantiene en presupuesto bajo de RAM y libera teselas antiguas por edad/uso.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_IXIPTLAH3D_TILESET_SOLIDIFICADO.md

# TLALPOWA ACTUALIZACION 2026-06-10 · IXIPTLAH3D TILESET SOLIDIFICADO

- Reticula IXIPTLAH3D elevada a 1024x1024 para que cada archivo .ixiptlah3d sea una tesela espacial mucho mas pequeña.
- Cada tesela se valida con limite duro de 75 MB; si una tesela rebasa, el exportador lo detiene con diagnostico antes de instalar un conjunto incompleto.
- El LOD cercano conserva fidelidad completa; el LOD exterior baja a 0.1% deliberadamente burdo para ser muy ligero.
- El anillo cercano ahora se decide por interseccion real tesela-camara: si la tesela toca el radio de 15 m, usa LOD completo.
- El lector sigue unificando el archivo raiz y la carpeta .tiles como un solo modelo 3D.
- Se limita la residencia GPU a 96 MB y se reducen cargas por cuadro para evitar picos de RAM.
- Se permiten reticulas IXIPTLAH3D hasta 4096 por eje para compatibilidad futura sin romper el formato.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_SANDBOX_TESELAS_MINIMAS.md

# TLALPOWA ACTUALIZACION 2026-06-10 · SANDBOX TESELAS MINIMAS

- Refuerzo estricto del flujo satelital z19 para cargar solo teselas visibles necesarias.
- El respaldo visual usa solo el padre inmediato z-1 y ya no dispara descarga de ese padre; solo se usa si ya existe en cache local o residente.
- Se elimina margen extra de dibujo para satelital y se limita el lote de teselas visibles en modo sandbox de baja RAM.
- Se bajan las texturas residentes de mapa para contener RAM y se reducen los presupuestos por cuadro para solicitudes y cargas de textura.
- Se mantiene el hard-lock z19 y el footer con lat/lon/z.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_TESELAS_DIRECTAS_Z19.md

# TLALPOWA actualización 2026-06-10: teselas satelitales directas z19

## Objetivo
Eliminar la carga escalonada de resoluciones satelitales intermedias al desplazar o ampliar el mapa.

## Reglas activas
- La resolución satelital visible se calcula directamente por escala métrica OpenMaps + z1.
- El límite absoluto sigue siendo z19.
- No se transita por z10, z11, z12... hasta llegar a la resolución vigente.
- La capa base solicita la resolución vigente directamente.
- Si una tesela de la resolución vigente no está cargada, sólo puede usarse como respaldo su padre inmediato z-1.
- No se dibuja ni descarga una cadena de padres múltiples.
- La caché satelital queda separada en `world_imagery_openmaps_max_z19_direct_current_parent1_v20260610`.

## Archivos tocados
- `Fuente/Tlalpowa.cpp`

## Validación estática
- No quedan bucles de backfill multizoom en `draw_base_lod`.
- `progressive_tile_zoom_target_from_previous` ya no hace avance unitario.
- El fallback visual por tesela está encapsulado en `draw_tile_layer` y sólo usa z-1.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_Z19_GRADUAL_FOOTER.md

# TLALPOWA · z19 satelital gradual y footer · 2026-06-10

La capa satelital queda reducida a límite físico z19. Toda ruta interna que intente z20 o superior se normaliza hacia el padre z19 antes de formar URL, llave residente o ruta de caché; por tanto el programa no puede solicitar teselas satelitales por encima de z19 aunque el usuario acerque más la vista.

La selección de resolución satelital deja de saltar artificialmente al máximo desde gran altura. El LOD visible se calcula con la progresión métrica natural de teselas Web Mercator/OpenMaps y sólo se solicita una resolución adicional respecto a la estrictamente esperada, con avance gradual por cuadros y con tope absoluto en z19.

Se elimina el recuadro superior de límite satelital. La indicación de resolución pasa al footer, en la misma línea de latitud y longitud, con formato `zN`.

No se revierte ningún refuerzo previo de IXIPTLAH3D: se conserva el flujo endurecido desde confirmación de coordenadas del `.blend`, la salida preferente `.ixiptlah3d`, la instalación final y la carga sobre el satélite histórico.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_Z20_IXIPTLAH3D_HIPERREFUERZO.md

# TLALPOWA · refuerzo z20 e IXIPTLAH3D · 2026-06-10

## Satélite

- Límite físico estricto: World Imagery/OpenMaps queda bloqueado en z20.
- La caché satelital cambia a `world_imagery_openmaps_max_z20_hardlock_v20260610` para no reutilizar teselas residuales de pruebas anteriores.
- La decisión LOD salta directamente a z20 cuando la escala entra a 1:30000 o menor, cuando el zoom interactivo es fuerte o cuando está activo el flujo 3D.
- `draw_tile_layer` y `visible_tile_coverage_ratio` aplican una vista satelital capada: aunque alguna ruta futura entregue z21+, se redibuja y se cubre como z20 real.
- La URL, la llave residente y la ruta de caché normalizan x/y/z antes de descargar: no se puede formar una petición z21+.

## IXIPTLAH3D

- El exportador Blender reduce manejadores simultáneos y usa expulsión LRU segura mediante `pop`, sin doble borrado ni pérdida del descriptor.
- La barra de progreso se mantiene viva durante triangulación grande con pulsos por triángulos empaquetados y bytes escritos.
- La conversión directa conserva coordenadas fijas de Tenochtitlan y reporta el código de salida de Blender si falla antes de generar contenedor válido.
- Al terminar, el modelo se marca como cargado directamente sobre el satélite histórico 1519.

---

## TLALPOWA_ACTUALIZACION_2026_06_10_Z20_IXIPTLAH3D_REPARACION.md

# TLALPOWA actualización 2026-06-10 · z20 satelital e IXIPTLAH3D

## Mapa satelital

La capa satelital queda bloqueada físicamente en z20. La normalización ocurre antes de formar clave de caché, ruta de archivo y URL remota: cualquier ruta interna que intente z21 o superior se degrada al padre z20 con coordenadas de tesela reducidas, no sólo con el número de zoom truncado. Esto evita peticiones imposibles por coordenadas x/y de z21+ contra un endpoint z20.

La interfaz dibuja un chip persistente sobre el mapa: `SAT limite z20` en altura amplia y `SAT z20 bloqueado · no z21+` al entrar en el umbral fino. El límite se ve aunque todavía no se haya llegado al detalle máximo.

## IXIPTLAH3D

El exportador Blender deja de escribir cada triángulo dos veces. Antes el índice declaraba `triangles * 3 * 12` bytes, pero el archivo físico contenía el doble; por eso la validación rechazaba el contenedor y la barra de progreso parecía desaparecer tras el primer avance útil.

También se elimina una declaración duplicada en la ruta de regeorreferenciación, para mantener compilación limpia y evitar regresiones al editar modelos ya instalados.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_ARRANQUE_SEMANA_ESENCIAL.md

# TLALPOWA 2026-06-12 · arranque con semana esencial

- La bienvenida carga mapa, catálogos mínimos y hotdata IXIPTLAH de la última semana disponible por elemento físico.
- El gate inicial usa núcleo/tipo/esquema/capa como identidad de elemento y agrupa por bucket semanal real.
- La carga inicial queda acotada por RAM: cabeceras/bloques mínimos en primer plano; vecinos cronológicos y datos profundos en segundo plano tras el fade.
- El splash baja de latencia y mantiene mapa visible detrás; si el presupuesto se agota, abre con la semana esencial parcial suficiente y continúa en background.
- Cache caliente reducida para conservar bajo consumo y evitar pasar de 500 MB en escenarios normales.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_ARRANQUE_SIN_COLAPSO.md

# Tlalpowa — arranque sin colapso pos-bienvenida

Se endureció la transición entre la pantalla de bienvenida y la interfaz principal. La preparación IXIPTLAH de la fecha visible deja de ejecutarse de forma sincrónica dentro del frame de mapa y ahora corre en un hilo corto, acotado y protegido; la ventana no queda retenida ni bloqueada por hotdata temporal.

La carga inicial de IXIPTLAH quedó con presupuestos menores de memoria y CPU: caché, registros de puerta, bytes por registro y vecinos de fondo se reducen para mantener el proceso por debajo de picos grandes. La carga de datos pesados sigue diferida; si no se activa `TLALPOWA_STARTUP_LOAD_DATA`, `observations_loading` ya no queda prendido indefinidamente.

Se agregó un watchdog de arranque: si algún cargador inicial queda detenido, la interfaz se libera de forma segura y el resto permanece diferido. La bienvenida puede desvanecerse por condición completa o por límite de seguridad, evitando una pantalla retenida.

En Windows se añadió filtro de excepción no controlada con registro en `Tlalpowa.log` y reinicio único en modo `TLALPOWA_SAFE_STARTUP`, con presupuestos mínimos para recuperar la ventana si algún acceso nativo externo falla durante el primer arranque.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_ARRANQUE_STACK_GUARD.md

# TLALPOWA · arranque sin colapso por pila

Se corrigió la ruta de arranque que podía caer con `0xc00000fd` después de la bienvenida. La exploración IXIPTLAH de hotdata dejó de usar recursión de directorios y ahora usa una pila explícita en heap con profundidad acotada. En MSVC se amplió la pila del ejecutable a 8 MiB para evitar desbordamientos durante el primer render completo de mapa, catálogo y capas iniciales. Si existe un `Tlalpowa.log` con desbordamiento de pila reciente, el programa entra en arranque protegido y difiere la hotdata IXIPTLAH pesada sin bloquear la interfaz.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_COMPILACION_MSVCRT_NUL_SAFE.md

# TLALPOWA · compilación MSVC NUL-safe

Se corrige el fallo de compilación reportado por PRETLALPOWA.LOG.

- Se elimina el byte NUL físico dentro del literal de carácter usado por el lector de `Tlalpowa.log`.
- El literal queda expresado como `\0`, portable para MSVC/GCC/Clang.
- La corrección evita el error MSVC `C2137: constante de caracteres vacía`.
- La cascada posterior de errores de lambda/captura queda neutralizada al restaurar el parseo normal del archivo.
- Se conserva el arranque protegido por stack overflow y el flujo de gráficas alcaldía-semana.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_CASILLAS_INFERIORES_ESTABLES.md

# TLALPOWA · gráficas con selección inferior estable

- La correlación O3-incidencia ya no calcula todas las enfermedades cuando no hay una enfermedad explícitamente elegida.
- El selector inferior usa las mismas casillas visuales del catálogo lateral.
- La selección inferior queda en etapa pendiente y sólo se aplica al pulsar Generar.
- Al hacer clic en una casilla inferior no se modifica el catálogo lateral global ni se invalida la gráfica visible.
- Cada confirmación normaliza el filtro a un solo dato epidemiológico para evitar cálculos paralelos innecesarios.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_CONFIRMACION_EJES_DIRECTOS.md

# TLALPOWA · gráficas con confirmación y ejes directos

- Las gráficas nuevas quedan en espera hasta pulsar Generar.
- Solo la gráfica seleccionada y confirmada calcula puntos.
- Cambiar eje X/Y deja la gráfica pendiente; no recalcula hasta nueva confirmación.
- La selección de X/Y se abre desde la línea física del eje, no desde etiquetas laterales.
- O3-incidencia calcula solo el cruce manual confirmado y un único desfase vigente.
- Se eliminó la franja lateral de selección en la ventana de gráficas.
- Se suprimió el diálogo modal de configuración del flujo normal de gráficas.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_ELEMENTO_ATMOSFERICO_ALCALDIA_SEMANA.md

# TLALPOWA · graficas atmosfera-incidencia por alcaldia-semana

Esta actualizacion deja el cruce epidemiologico-atmosferico como una operacion manual, unica y comparable.

## Flujo

- El eje atmosferico ya no termina en una categoria generica.
- La categoria abre una lista de elementos medibles: ozono, PM10, PM2.5, NO2, SO2, CO, temperatura, humedad y cualquier variable atmosferica cargada por IXIPTLAH/RAMA.
- La seleccion del elemento se guarda en la especificacion de la grafica.
- La grafica no se recalcula hasta pulsar Generar.

## Comparabilidad

- Unidad analitica fija: alcaldia/municipio x semana epidemiologica x enfermedad.
- La incidencia se agrupa semanalmente dentro de la misma alcaldia/municipio.
- El elemento atmosferico se promedia semanalmente dentro de esa misma alcaldia/municipio.
- Los datos horarios, diarios o subdiarios se agregan a la semana epidemiologica antes de parearse.
- No se mezclan promedios globales con incidencias locales.

## Rendimiento

- Solo se calcula la grafica activa y confirmada.
- Solo se calcula una enfermedad y un elemento atmosferico por generacion.
- La cache diferencia elemento atmosferico, territorio, semana, desfase y agregacion.
- El render progresivo conserva puntos visibles mientras termina la construccion.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_FLUJO_SOLIDO_FINAL.md

# TLALPOWA · gráficas IXIPTLAH/O3 solidificadas

- La incidencia semanal reconoce Sem, SemDerivada, incidencia_semanal y variantes semanales por sexo.
- Toda ruta de gráfica semanal convierte entradas horarias/diarias/subdiarias a semana canónica antes de agrupar.
- El filtro temporal ya no descarta filas semanales válidas cuando la semana canónica no aparece en el índice visual.
- O3 manual conserva la selección explícita y reporta la causa cuando no existen pares comparables.
- La construcción progresiva mantiene CPU baja y publica puntos conforme se acumulan buckets visibles.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_IXIPTLAH_EJES.md

# TLALPOWA · gráficas IXIPTLAH y ejes interactivos

Esta actualización conecta la creación de gráficas epidemiológicas con la lectura IXIPTLAH reciente cuando el área de gráficas tiene al menos una gráfica activa y todavía no hay observaciones en RAM. La carga usa los snapshots epidemiológicos compactos ya embebidos en IXIPTLAH; si no existen, conserva el estado vacío sin inventar datos.

La agregación de incidencias deja de depender del estado global `pending_*` y ahora se calcula desde la especificación propia de cada `GraphSpec`. Esto evita que una gráfica contamine a otra y permite que cada tarjeta conserve tipo, eje X, eje Y, eje Z, filtro lateral y modelo de ajuste.

El eje X y el eje Y son editables con clic directo únicamente cuando la gráfica ya está seleccionada. El clic sobre la etiqueta `X`, la etiqueta `Y` o las zonas físicas de los ejes en la gráfica abre un selector mínimo de dominio y variable. Cada cambio recalcula la gráfica sobre IXIPTLAH, actualiza el título y reinicia el ajuste estadístico aplicable.

La dispersión genérica vuelve a aceptar ejes epidemiológicos. La plantilla O3-incidencia sigue siendo explícita y queda activada sólo por la combinación semántica O3 contra incidencia, no por todo gráfico de dispersión.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_IXIPTLAH_FLUJO_BLINDADO.md

# TLALPOWA · gráficas IXIPTLAH · flujo blindado

Actualización acumulada sobre `GRAFICAS_IXIPTLAH_FLUJO_SOLIDO`.

## Contrato aplicado

- La gráfica epidemiológica solo acepta como tabla completa una firma interna de gráficas IXIPTLAH o una firma de todas las enfermedades.
- Si la gráfica usa filtros laterales y no hay selección activa, se fuerza recarga dedicada para evitar que una tabla filtrada parcial alimente una gráfica que visualmente aparenta usar todo.
- Si la selección lateral existe, la tabla filtrada solo se considera compatible cuando su firma coincide exactamente con la selección vigente.
- La tabla pesada de gráficas IXIPTLAH se libera cuando no hay pestaña de gráficas activa que la necesite.
- La caché de puntos de gráfica queda protegida por mutex y su llave incluye firma de carga y tamaño de catálogos epidemiológicos.
- Al cambiar dominio de eje, la variable vuelve a 0 para evitar pares dominio-variable semánticamente arrastrados desde otro dominio.
- Si IXIPTLAH está cargado pero los ejes/filtros no producen puntos, la gráfica lo indica dentro del área de trazado en vez de fallar en silencio.

## Validación local

- `gcc -std=c11 -I Fuente -fsyntax-only Fuente/tlalpowa_c.c`
- `g++ -std=c++17 -I Fuente -fsyntax-only Fuente/Tlalpowa.cpp`
- Validación estructural ZIP posterior.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_IXIPTLAH_FLUJO_SOLIDO.md

# TLALPOWA · gráficas IXIPTLAH · flujo sólido

Fecha: 2026-06-12

- La carga automática de IXIPTLAH epidemiológico para gráficas ya no se dispara en bucle por frame cuando no hay datos; usa reintento compacto y enfriamiento.
- La primera lectura toma IXIPTLAH recientes; si no bastan, el segundo intento hace lectura completa en segundo plano.
- La tabla epidemiológica cargada específicamente para gráficas queda marcada como `__TLALPOWA_GRAPH_IXIPTLAH__` para no ser liberada por el limpiador de capas cuando la pestaña de gráficas está activa.
- Las gráficas epidemiológicas solo piden IXIPTLAH si su especificación realmente usa dominio epidemiológico.
- Al modificar el eje X o Y desde la propia gráfica, la especificación se limpia, se resincroniza con el panel de configuración abierto y se revalida la carga IXIPTLAH.
- Si la gráfica queda sin puntos porque la lectura sigue activa, se muestra el estado de carga dentro del área de trazado.
- Se mantiene el flujo de selección: primer clic selecciona la gráfica; segundo clic sobre etiqueta o eje físico X/Y abre el selector de dominio-variable.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_IXIPTLAH_NO_PASMADO.md

# TLALPOWA · gráficas IXIPTLAH sin vacío visual

- Si una gráfica O3-incidencia no encuentra pares atmosféricos comparables, ya no deja dos paneles vacíos: usa una vista inmediata de incidencia IXIPTLAH semanal.
- La vista de respaldo fuerza X=Semana, Y=Incidencia semanal y conserva el contrato de semanalización antes de filtrar, agrupar, cachear y dibujar.
- Si IXIPTLAH aún no está cargado, el panel muestra el estado real de carga en lugar de “Sin pares comparables”.
- La exportación CSV de O3-incidencia cae a CSV IXIPTLAH semanal cuando no existen pares O3 pero sí hay datos epidemiológicos.
- La construcción de puntos sigue siendo progresiva por rebanadas cortas para publicar puntos visibles sin esperar a completar toda la serie.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_O3_MANUAL_Y_PUNTOS.md

# TLALPOWA — gráficas IXIPTLAH con O3 manual

- La galería ya no crea automáticamente una gráfica O3-incidencia.
- La dispersión inicial abre como incidencia semanal IXIPTLAH contra semana.
- O3-incidencia sólo se activa cuando el usuario cruza manualmente un eje epidemiológico con un eje atmosférico.
- Las gráficas nuevas parten con todos los datos, no con filtro lateral vacío.
- En O3, un filtro de enfermedad vacío ya no elimina todos los pares: vacío significa sin filtro.
- La semanalización canónica y el render progresivo se mantienen.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_SEMANAL_CONTRATO_SOLIDO.md

# TLALPOWA · gráficas con contrato temporal semanal sólido

- Se añadió un contrato temporal único para gráficas: si cualquier eje o plantilla exige semana, la gráfica opera estrictamente en grano semanal.
- La conversión hora/día/semana se resuelve antes de filtrar, cachear, agrupar, exportar CSV o dibujar.
- Las observaciones subdiarias o diarias compatibles se suman dentro de la semana epidemiológica canónica cuando el destino es incidencia semanal.
- La ruta O3-incidencia queda forzada a semana: O3 horario/diario se reduce a valor semanal antes de parearse con incidencia.
- La llave de caché incorpora `grain` y `agg`, evitando reutilizar puntos nativos en gráficas semanales.
- Los CSV genéricos exponen `temporal_grain` y `temporal_aggregation`.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_SEMANAL_PROGRESIVO_BLINDADO.md

# TLALPOWA · gráficas semanales progresivas blindadas · 2026-06-12

- Corrige compatibilidad MSVC: `tlac_limpia_spec` queda declarada antes de su primer uso.
- Añade contrato temporal por fuente: si el bloque IXIPTLAH contiene datos semanales, la gráfica opera en semana canónica.
- Convierte datos horarios/diarios/subdiarios a semana epidemiológica antes de filtrar, agrupar, cachear, exportar y dibujar.
- Evita promover datos mensuales/anuales a semanas falsas cuando el contrato semanal está activo.
- Publica puntos visibles en rebanadas cortas por frame, con cachés acotadas y memoria progresiva limitada.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_SEMANAL_PROGRESIVO_ELEGANTE.md

# TLALPOWA — gráficas semanales progresivas elegantes

- Se consolidó el contrato temporal semanal: toda fuente horaria, diaria o subdiaria se proyecta a semana epidemiológica canónica antes de filtrar, agrupar, cachear, exportar o dibujar.
- Se agregó detección explícita de granularidad de origen por `period`, `metric` y forma temporal de la etiqueta.
- La construcción progresiva de puntos ahora usa rebanadas breves por frame y publica el búfer visible al crear nuevos buckets, evitando que la gráfica parezca detenida.
- Se acotaron cachés de puntos y estados progresivos para mantener bajo consumo de RAM.
- Se redujo el pre-reservado de correlación O3-incidencia para evitar picos innecesarios de memoria.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_SEMANAL_PROGRESIVO_RENDIMIENTO.md

# TLALPOWA · gráficas semanales progresivas

Fecha: 2026-06-12

## Contrato aplicado

- Toda gráfica que toque epidemiología, tiempo, desfase o promedio semanal entra en contrato semanal.
- Las observaciones diarias u horarias se convierten a semana epidemiológica canónica antes de filtrar, agrupar, cachear, exportar o dibujar.
- O3-incidencia conserva agregación semanal obligatoria.

## Rendimiento

- La gráfica de incidencias ya no bloquea el frame hasta terminar todo el recorrido IXIPTLAH.
- La construcción de puntos usa rebanadas cortas por frame y publica el subconjunto visible ya agregado.
- Los puntos aparecen conforme se crean; cuando termina, el resultado se congela en caché compacta.
- La caché de puntos se recorta a 48 entradas y la caché progresiva a 12 estados para evitar crecimiento de RAM.
- La agregación usa `unordered_map` y límites visibles por gráfica para reducir CPU y memoria.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_GRAFICAS_TEMPORAL_SEMANAL.md

# TLALPOWA · gráficas con coerción temporal semanal

Actualización acumulada del flujo IXIPTLAH-gráficas.

- Las gráficas que usan incidencia epidemiológica, semana, rango temporal semanal, desfase semanal u O3-incidencia fijan grano temporal semanal.
- Las etiquetas diarias u horarias se normalizan antes de filtrar, agrupar, cachear, exportar o dibujar.
- La ruta O3-incidencia agrupa por semana epidemiológica canónica antes de calcular exposición atmosférica.
- La caché de puntos y la caché O3 incluyen el grano temporal para impedir reutilización incompatible.
- Los CSV exportados declaran `temporal_grain`.
- La tarjeta de gráfica muestra el grano efectivo junto al conteo de puntos.

---

## TLALPOWA_ACTUALIZACION_2026_06_12_Msvc_Graficas_Atmo_Compilefix.md

# TLALPOWA · corrección MSVC de gráficas atmósfera-incidencia

- Se adelantaron las declaraciones de `graph_clean_single_disease_filter` y `tlac_campo_txt` inmediatamente después de `GraphSpec`.
- Se elimina la falla de MSVC por uso previo de identificadores en el flujo de gráficas.
- Se conserva el contrato: un contaminante/elemento atmosférico, una enfermedad, una alcaldía y una semana comparable por generación confirmada.
- Se conserva la selección directa sobre ejes X/Y y el cálculo únicamente al pulsar Generar.

---

## tlalpowa_catalogos_tooltips_minimos_auditoria.md

# TLALPOWA · auditoría i18n y globos de catálogo

Fecha: 2026-06-09

## Cambio aplicado

Se silenció de forma centralizada el catálogo lateral: cabeceras, hojas booleanas, filtros de red y árboles epidemiológicos ya no abren globos explicativos al pasar el cursor.

Se conserva únicamente el globo mínimo de variables atmosféricas/contaminantes, reducido a la subcategoría compacta de 1 a 3 palabras.

## Contrato de globos conservados

- Variables meteorológicas: sólo subcategoría compacta.
- Contaminantes atmosféricos: sólo subcategoría compacta.
- Enfermedades, movilidad, demografía, mapas históricos y calendarios: sin globo de catálogo.
- Tenochtitlan 3D nativo: sin globo de mapa.

## Validación automática

- Grupos atmosféricos únicos revisados: 19.
- Máximo de palabras por etiqueta compacta: 2.
- Etiquetas compactas con más de 3 palabras: 0.
- Llamadas tooltip en núcleo lateral revisado: 2.
- Tooltips atmosféricos compactos conservados: 2.
- Retorno activo del tooltip largo de Tenochtitlan: no.

## Etiquetas compactas

- `aerosol_satelital` → Aerosoles
- `aerosoles` → Aerosoles
- `aerosoles_derivados` → Aerosoles derivados
- `carbono_derivado` → Carbono
- `contaminante_criterio` → Criterio
- `cov` → Compuestos volátiles
- `cov_derivados` → COV derivados
- `focos_calor_satelital` → Focos calor
- `gas_efecto_invernadero` → GEI
- `gas_efecto_invernadero_satelital` → GEI
- `gas_traza` → Gases traza
- `gas_traza_satelital` → Gases traza
- `gases_derivados` → Gases derivados
- `metales` → Metales
- `meteorologico` → Meteorología
- `meteorologico_derivado` → Derivados meteorológicos
- `nubes_satelital` → Nubes
- `radiacion_satelital` → Radiación
- `superficie_satelital` → Superficie

## Incidencias

Sin incidencias.

---

## tlalpowa_dialogos_muertos_auditoria.md

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

---

## tlalpowa_i18n_config_catalogos_auditoria.md

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

---

## tlalpowa_i18n_embebido_auditoria.md

# TLALPOWA · i18n embebido

Esta actualización activa tres perfiles lingüísticos de interfaz sin depender de diccionarios externos durante la ejecución:

- Español (México): `es-MX`
- Náhuatl (Central): `nah-central`
- Inglés (UK): `en-GB`

## Contrato técnico

La traducción vive compilada dentro de `Tlalpowa.cpp` mediante `tlalpowa_tr()`. La consulta usa hash FNV-1a de 64 bits más verificación exacta con `strcmp`, por lo que no abre XLSX, TXT, JSON, INI ni archivos auxiliares cuando la interfaz pinta textos. Las cadenas no se alojan dinámicamente; son literales estáticos del binario. La selección del idioma sigue usando la preferencia persistente existente `perfil_idioma` / `perfil_idioma_clave`, pero ahora se aplica por fotograma antes de dibujar la UI.

## Criterio náhuatl

El glosario base se contrastó con el archivo privado `DICCIONARIO ESPAÑOL NÁHUATL CENTRAL.xlsx` proporcionado para esta intervención. En términos técnicos ausentes del diccionario —por ejemplo, gráfica, filtro, RAMA, CSV, PNG, GitHub o estadística— se mantuvieron préstamos técnicos o compuestos semánticos conservadores para evitar falsos cognados o traducciones artificialmente rígidas.

## Verificación incluida

- Tabla estática sin colisiones de `case` hash.
- Prueba aislada de compilación C++17 para el núcleo `tlalpowa_tr()`.
- Comprobación de sintaxis del archivo `Tlalpowa.cpp` en la ruta sin ImGui habilitado.
- El paquete no incluye carpeta `Datos`.


---

## tlalpowa_carga_inicial_segura.md

# TLALPOWA · carga inicial segura

Corrección acumulativa del arranque posterior al log `0x80000003`. Se eliminó el primer frame ImGui manual previo al bucle principal y se sustituyó por un clear OpenGL mínimo, sin texturas ni fuentes, para evitar asserts/punteros gráficos antes del ciclo estable. La bienvenida visible vuelve a depender del overlay ya protegido del render principal.

## Cambios aplicados

- Primer signo visual: ventana nativa visible + velo OpenGL mínimo antes de iniciar cargas pesadas.
- Sin carga de iconos ni texturas en el frame cero.
- Sin autoactivar todas las casillas epidemiológicas/atmosféricas al abrir; se evita disparar cargas completas por selección artificial.
- Hotdata inicial limitada a núcleos epidemiológico, meteorológico y contaminante.
- Mapa diferido unos milisegundos y sin competir frontalmente con la hotdata inicial.
- Working set orientado a 250 MiB de forma blanda, no rígida, para evitar fallos por reserva.
- Menos caché residente, menos vecinos temporales y reposo activo/idle más bajo en CPU.

---

## tlalpowa_carga_inicial_z19_embebido.md

# TLALPOWA · carga inicial con Z19 embebido

Corrección acumulativa del arranque posterior a la prueba en la que el velo se quitaba antes de que el mapa esencial y los epidemiológicos recientes terminaran de quedar visibles. La bienvenida sigue siendo el primer elemento mostrado, pero ahora sólo se libera cuando hotdata, catálogos, epidemiología reciente y mapa esencial quedaron publicados, con escape máximo para no bloquear indefinidamente ante un error de archivos.

## Cambios aplicados

- Bienvenida mínima visible antes de hilos pesados.
- Retención del velo extendida a 5.10 s con fade de 0.34 s.
- Salida del velo condicionada a: catálogos, epidemiología reciente, hotdata 75%, primera fecha visible y mapa esencial.
- Escape máximo a 24 s si alguna pieza externa falla, evitando bloqueo permanente.
- El mapa deja de esperar artificialmente al final del velo: empieza en cuanto hotdata/catálogos esenciales están listos.
- Epidemiología reciente inicial sube de 3 a 5 archivos IXIPTLAH por defecto, con límite configurable hasta 12.
- Z19 embebido ultraligero como textura PNG interna de 128×128; se usa como respaldo satelital inmediato y como relleno de teselas ausentes.
- La normalización satelital impide rutas menores a Z10 y remapea cualquier solicitud menor al primer Z10 válido.
- DEM inicial remapeado a Z10 mínimo.
- Descarga satelital reducida a 1 solicitud/frame y carga de texturas locales elevada a 3/frame para priorizar caché ya existente sin quemar CPU/red.

## Verificación incluida

- `GeneratedRuntimeResources.c`: sintaxis C11 correcta.
- `tlalpowa_c.c`: sintaxis C11 correcta.
- Revisión de balance de llaves en `Tlalpowa.cpp`: balance 0, sin cierres negativos.


## 2026-06-13 · zoom satelital embebido Z10/Z19

- Arranque satelital corregido: Z10 queda como base embebida ligera para capas superiores y Z19 como respaldo de detalle.
- Z11..Z18 quedan dinámicos por web/caché; no se empacan como recursos internos.
- Modo sin conexión: z0..z15 se cubren con Z10 embebido y z16..z19 con Z19 embebido.
- Eliminada la degradación dura a z13 durante movimiento; la única degradación permitida es al padre inmediato z-1.

## 2026-06-13 · fondo satelital Z5 y carga sin huecos

- Se añadió una capa satelital embebida Z5 de muy baja resolución como fondo completo inmediato del mapa.
- Z5 se dibuja primero para que ninguna tesela ausente deje cuadros vacíos durante arranque, movimiento o modo sin conexión.
- Z10 queda como capa superior ligera para altura media; Z11..Z18 son las únicas teselas dinámicas web/caché.
- Z19 queda como respaldo embebido fino, sin forzar descarga masiva a máxima resolución.
- Se cambió la caché satelital a una familia nueva para no reutilizar restos cuadriculados de pruebas anteriores.
- Se bajó el presupuesto residente de texturas de mapa de 64 a 40 y el límite blando de caché de 256 MiB a 128 MiB.
- Se redujo el velo mínimo de bienvenida de 5.10 s a 3.15 s, manteniendo la condición de catálogos, epidemiología reciente, hotdata 75% y mapa esencial.
- Se bloqueó la descarga para Z10 y Z19 embebidos; la red queda concentrada en Z11..Z18 con 2 solicitudes/frame y 2 cargas/frame.

## 2026-06-13 · mapa satelital web real Z0-Z19 con respaldo offline Z5/Z15

- La capa satelital deja de bloquear niveles dinámicos: ahora forma URL, clave de caché y ruta física con el zoom web real Z0..Z19.
- Se retiró la política de Z11..Z18 como único rango dinámico: el zoom visible se calcula por resolución métrica continua y se pide directo, sin escalera intermedia.
- Los respaldos embebidos sólo se dibujan cuando fallan descargas satelitales consecutivas y se activa modo offline temporal.
- Offline: Z0..Z9 se cubren con copia Z5; Z10..Z19 se cubren con copia Z15.
- La tesela faltante intenta únicamente su padre inmediato z-1 antes del respaldo offline; no hay regresión a escalones bajos arbitrarios.
- Se subió el presupuesto de streaming satelital y residentes para cubrir pantallas grandes sin cortar la periferia visible.

## 2026-06-13 · gráficas XY ozono-incidencia progresivas

- La correlación atmosférica-epidemiológica ya no exige una sola enfermedad: si no hay filtro propio usa las enfermedades visibles y, si tampoco existen, calcula sobre toda la cohorte cargada.
- La selección temporal de la gráfica usa el rango visible real `week_start..week_end`; ya no colapsa por error a una sola semana al cruzar ozono contra incidencia.
- La construcción de pares X/Y se volvió progresiva por cuadros: conserva el cursor, acumula puntos ya calculados y no reinicia la gráfica entre llamadas.
- La dispersión mantiene puntos previos mientras llegan exposiciones atmosféricas asíncronas; así evita parpadeo, vaciados intermedios y saltos visuales.
- Las gráficas confirmadas siguen actualizándose aunque el usuario interactúe con controles laterales o pierdan foco momentáneo.
- La caché final sólo se consolida cuando la correlación está completa; los resultados parciales quedan como estado vivo de render, no como producto final congelado.

## 2026-06-13 · gráficas XY incidencia/O3 endurecidas

- La serie temporal de incidencia ahora ordena las semanas en sentido cronológico real y conserva el extremo temporal reciente en vez de retener semanas viejas por inversión del comparador.
- El límite visual de puntos cronológicos deja de ser 96 fijo: usa `TLALPOWA_GRAPH_CHRONO_POINT_CAP` con base 4096 para que la dispersión de incidencia no pierda puntos útiles.
- El cruce atmósfera-incidencia ya no se detiene cuando una exposición territorial semanal queda pendiente; la fila pasa a una cola de reintento y el escaneo continúa para publicar los demás puntos disponibles.
- La cola de reintento O3/atmósfera se vuelve incremental por frame y mantiene puntos previos visibles mientras resuelve exposiciones atrasadas.
- El caché binario de pares atmósfera-incidencia se invalida a versión nueva para no reutilizar cohortes viejas generadas bajo reglas parciales.


## 2026-06-13 · Catálogo territorial centralizado

Se agregó la casilla lateral Datos Territoriales con estructura estado → jurisdicción opcional → municipio/alcaldía, respaldada por `tlalpowa_territorial.json` dentro del paquete `tlalpowa_datos.json`. La capa usa polígonos ZMVM ya instalados cuando existen y conserva actualización externa trazable para catálogos nacionales sin dispersar archivos.
