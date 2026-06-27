# MiausoftVisual

Biblioteca visual mínima, en C11 puro, extraída de Tlalpowa.

```c
MiausoftVisualTheme theme =
    miausoft_visual_theme(1, miausoft_rgba(133, 13, 55, 255));

MiausoftVisualElement button = {0};
button.kind = MIAUSOFT_TLALPOWA_BUTTON;
button.rect = (MiausoftRectI){x, y, w, h};
button.text = miausoft_visual_text_utf8(
    "Importar",
    MIAUSOFT_VISUAL_TEXT_CENTER | MIAUSOFT_VISUAL_TEXT_MIDDLE,
    MIAUSOFT_VISUAL_FONT_STRONG);
```

Plantillas disponibles:

- superficie, barra superior e inferior;
- botón, pestaña y botón pequeño;
- caja, entrada, búsqueda y combo;
- casilla;
- panel lateral y fila de árbol;
- progreso y deslizador;
- ventana e índice de configuración;
- tabla, cabecera y fila;
- tooltip, popup y modal;
- seleccionable, menú y cabecera plegable;
- tarjeta, etiqueta, viñeta y título de sección.

Separadores y pistas de desplazamiento existen sólo para expresar estructura:
Tlalpowa los dibuja invisibles.

Las medidas reutilizables salen de `miausoft_tlalpowa_metrics()` y
`miausoft_tlalpowa_layout()`. Para medidas adicionales se usa
`MiausoftPhiExpr`; por ejemplo `miausoft_phi_expr(1, 2, 0)` representa
`N1 + N2`.

`miausoft_tlalpowa_template()` devuelve el contrato rígido de cada elemento:
medidas PHI, carácter plano/translúcido/anidable y la máscara exacta de
parámetros permitidos. Por ejemplo, la casilla sólo admite contenido, estado,
color y anidación; opacidad, radio y ausencia de contorno no son configurables.

Los backends de widgets consumen `miausoft_tlalpowa_style_tokens()`. Esa
estructura contiene los valores literales usados por Tlalpowa para botones,
pestañas, cabeceras, deslizadores, scroll y navegación, sin que cada aplicación
pueda reinterpretarlos.

En Win32, un control del sistema puede seguir administrando teclado, ratón,
caret o selección, pero no imponer su aspecto. Organizador usa owner-draw para
botones y listas, custom-draw para el deslizador, fondos/recortes Tlalpowa para
edición y una ventana popup mínima renderizada como
`MIAUSOFT_TLALPOWA_TOOLTIP`.
