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
