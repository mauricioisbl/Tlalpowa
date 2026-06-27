# Núcleo visual de MiausoftSuite

Tlalpowa es la única autoridad visual. El núcleo no define una estética
genérica ni conserva controles de aplicaciones eliminadas.

Archivos públicos:

- `MiausoftVisual.h`: contrato C11, expresiones PHI, métricas, layout y plantillas.
- `MiausoftVisual.c`: implementación sin heap, sin estado mutable y sin plataforma.
- `MiausoftVisualWin32.h/.c`: renderizador GDI opcional para aplicaciones Win32.

Las aplicaciones incluyen únicamente:

```c
#include "MiausoftVisual.h"
```

Una aplicación Win32 que use el renderizador incluye además
`MiausoftVisualWin32.h`.

## Invariantes

- C11 puro en el núcleo.
- Cero contornos en todos los estados.
- Dimensiones públicas expresadas como `phi^-N` o sumas de hasta tres escalones.
- Cada plantilla declara sus únicos parámetros ajustables; el resto es invariante.
- `miausoft_tlalpowa_style_tokens()` reproduce literalmente el tema global de
  Tlalpowa, incluidas opacidades fraccionarias, espaciado y radios.
- Barra superior, barra inferior y controles con la geometría observada en Tlalpowa.
- Panel lateral derecho.
- Ninguna plantilla `Sheet`, `Hero`, `Radio`, `Toggle` o `Badge`.
- Sin asignación dinámica y sin estado global mutable.

## Aplicaciones supervivientes

- Tlalpowa consume el contrato directamente y sigue siendo la referencia externa.
- Ilnamiki usa el layout Tlalpowa y sus controles reales.
- Organizador conserva sólo un adaptador Win32 activo; no contiene bloques de
  compatibilidad visual ni una segunda ventana de configuración. Botones,
  casillas, deslizador, progreso, lista de materias y tooltip se dibujan con
  plantillas Tlalpowa; los controles Win32 restantes sólo aportan entrada,
  foco, selección o edición.

Compilación central:

```powershell
.\core\Compilar.ps1 -Target Todo -Configuration Release
```
