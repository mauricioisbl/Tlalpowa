@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul

set "NO_PAUSE=0"
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "BASE=%SCRIPT_DIR%"
set "LOG=%SCRIPT_DIR%\PRETLALPOWA.LOG"

:parse_args
if "%~1"=="" goto args_done
set "ARG=%~1"
if /I "%ARG%"=="/nopause" set "NO_PAUSE=1"& shift & goto parse_args
if /I "%ARG%"=="--nopause" set "NO_PAUSE=1"& shift & goto parse_args
if /I "%ARG%"=="--root" goto parse_root_next
if /I "%ARG%"=="/root" goto parse_root_next
if /I "%ARG:~0,7%"=="--root=" goto parse_root_equals
if /I "%ARG:~0,6%"=="/root=" goto parse_root_equals_slash
> "%LOG%" echo ============================================================
>> "%LOG%" echo Tlalpowa - procesamiento estricto de iconos
>> "%LOG%" echo Inicio: %DATE% %TIME%
>> "%LOG%" echo [ERROR] Argumento no reconocido: %ARG%
echo [ERROR] Argumento no reconocido: %ARG%
echo [LOG] Conservado por fallo: %LOG%
if "%NO_PAUSE%"=="1" exit /b 1
pause
exit /b 1

:parse_root_next
shift
if "%~1"=="" goto missing_root
set "BASE=%~1"
shift
goto parse_args

:parse_root_equals
set "BASE=%ARG:~7%"
shift
goto parse_args

:parse_root_equals_slash
set "BASE=%ARG:~6%"
shift
goto parse_args

:missing_root
> "%LOG%" echo ============================================================
>> "%LOG%" echo Tlalpowa - procesamiento estricto de iconos
>> "%LOG%" echo Inicio: %DATE% %TIME%
>> "%LOG%" echo [ERROR] --root requiere una ruta explicita.
echo [ERROR] --root requiere una ruta explicita.
echo [LOG] Conservado por fallo: %LOG%
if "%NO_PAUSE%"=="1" exit /b 1
pause
exit /b 1

:args_done
set "BASE=%BASE:"=%"
for %%I in ("%BASE%\.") do set "BASE=%%~fI"
set "LOG=%BASE%\PRETLALPOWA.LOG"

if not exist "%BASE%\" (
  set "LOG=%SCRIPT_DIR%\PRETLALPOWA.LOG"
  > "%LOG%" echo ============================================================
  >> "%LOG%" echo Tlalpowa - procesamiento estricto de iconos
  >> "%LOG%" echo Inicio: %DATE% %TIME%
  >> "%LOG%" echo [ERROR] La carpeta base no existe: %BASE%
  echo [ERROR] No pude entrar a la carpeta base: %BASE%
  echo [LOG] Conservado por fallo: %LOG%
  if "%NO_PAUSE%"=="1" exit /b 1
  pause
  exit /b 1
)

set "LOG=%BASE%\PRETLALPOWA.LOG"
set "SOURCE_PNG=%BASE%\Datos\icon\tlalpowa.png"
set "OUTPUT_ICO=%BASE%\Fuente\Tlalpowa.ico"
set "CONVERTER=%BASE%\Fuente\Tools\Convert-PngToIco.ps1"
set "POLARIZER_CPP=%BASE%\Fuente\Tools\tlalpowa_icon_polarizer.cpp"
set "TOOL_DIR=%BASE%\Build\Herramientas"
set "POLARIZER_EXE=%TOOL_DIR%\tlalpowa_icon_polarizer.exe"

if defined PRETLALPOWA_PARENT (
  >> "%LOG%" echo.
  >> "%LOG%" echo ============================================================
  >> "%LOG%" echo Tlalpowa - procesamiento estricto de iconos
  >> "%LOG%" echo Inicio: %DATE% %TIME%
  >> "%LOG%" echo Invocado por: %PRETLALPOWA_PARENT%
  >> "%LOG%" echo Base: %BASE%
  >> "%LOG%" echo Fuente canonica PNG: %SOURCE_PNG%
  >> "%LOG%" echo Salida ICO: %OUTPUT_ICO%
  >> "%LOG%" echo ============================================================
) else (
  > "%LOG%" echo ============================================================
  >> "%LOG%" echo Tlalpowa - procesamiento estricto de iconos
  >> "%LOG%" echo Inicio: %DATE% %TIME%
  >> "%LOG%" echo Base: %BASE%
  >> "%LOG%" echo Fuente canonica PNG: %SOURCE_PNG%
  >> "%LOG%" echo Salida ICO: %OUTPUT_ICO%
  >> "%LOG%" echo ============================================================
)

cd /d "%BASE%" || goto fatal_base

call :purge_root_rasters || goto fail

if not exist "%SOURCE_PNG%" (
  echo [ERROR] Falta Datos\icon\tlalpowa.png.
  >> "%LOG%" echo [ERROR] Falta %SOURCE_PNG%.
  goto fail
)
if not exist "%CONVERTER%" (
  echo [ERROR] Falta Fuente\Tools\Convert-PngToIco.ps1.
  >> "%LOG%" echo [ERROR] Falta %CONVERTER%.
  goto fail
)

if exist "%POLARIZER_CPP%" (
  if not exist "%TOOL_DIR%" mkdir "%TOOL_DIR%" >> "%LOG%" 2>&1 || goto fail
  if not exist "%POLARIZER_EXE%" (
    where cl.exe >nul 2>nul
    if not errorlevel 1 (
      echo [INFO] Compilando normalizador PNG de iconografia.
      >> "%LOG%" echo [INFO] Compilando: %POLARIZER_CPP%
      cl.exe /nologo /EHsc /O2 /std:c++20 /utf-8 /W4 /permissive- /DUNICODE /D_UNICODE /Fo"%TOOL_DIR%\tlalpowa_icon_polarizer.obj" /Fe"%POLARIZER_EXE%" "%POLARIZER_CPP%" Windowscodecs.lib Ole32.lib /link /SUBSYSTEM:CONSOLE >> "%LOG%" 2>&1
      if errorlevel 1 (
        echo [ADVERTENCIA] No se pudo compilar el normalizador PNG; continuo con conversion ICO canonica.
        >> "%LOG%" echo [ADVERTENCIA] Fallo la compilacion del normalizador PNG. No es fatal para el build principal.
        if exist "%POLARIZER_EXE%" del /f /q "%POLARIZER_EXE%" >nul 2>> "%LOG%"
      )
    ) else (
      echo [INFO] cl.exe no esta disponible; se omite normalizacion standalone previa.
      >> "%LOG%" echo [INFO] cl.exe no esta disponible; CMake podra construir TlalpowaIconPolarizer despues.
    )
  )
  if exist "%POLARIZER_EXE%" (
    echo [INFO] Normalizando PNGs de iconografia.
    >> "%LOG%" echo [INFO] Ejecutando %POLARIZER_EXE% --root %BASE%
    "%POLARIZER_EXE%" --root "%BASE%" >> "%LOG%" 2>&1
    if errorlevel 1 (
      echo [ADVERTENCIA] El normalizador PNG fallo; continuo con conversion ICO canonica.
      >> "%LOG%" echo [ADVERTENCIA] Fallo la ejecucion del normalizador PNG. No es fatal para el build principal.
    )
  )
)

where powershell.exe >nul 2>nul
if errorlevel 1 (
  echo [ERROR] No encontre powershell.exe para convertir PNG a ICO.
  >> "%LOG%" echo [ERROR] No encontre powershell.exe.
  goto fail
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%CONVERTER%" -InputPng "%SOURCE_PNG%" -OutputIco "%OUTPUT_ICO%" >> "%LOG%" 2>&1
if errorlevel 1 goto fail
if not exist "%OUTPUT_ICO%" goto fail

call :purge_root_rasters || goto fail

echo [OK] Iconos procesados: Fuente\Tlalpowa.ico regenerado desde Datos\icon\tlalpowa.png.
>> "%LOG%" echo [OK] ICO canonico regenerado y carpeta base libre de *.ico/*.png.
if defined PRETLALPOWA_PARENT (
  if "%NO_PAUSE%"=="1" exit /b 0
  echo.
  echo Log: %LOG%
  pause
  exit /b 0
)
call :delete_success_log
if errorlevel 1 (
  echo [ERROR] El procesamiento termino con codigo 0, pero no pude eliminar PRETLALPOWA.LOG: %LOG%
  echo [LOG] Conservado por fallo de limpieza: %LOG%
  if "%NO_PAUSE%"=="1" exit /b 1
  echo.
  echo Log: %LOG%
  pause
  exit /b 1
)
echo [LOG] PRETLALPOWA.LOG eliminado por salida 0.
if "%NO_PAUSE%"=="1" exit /b 0
echo.
echo Procesamiento terminado sin errores.
pause
exit /b 0

:delete_success_log
for /L %%L in (1,1,10) do (
  if exist "%LOG%" del /f /q "%LOG%" >nul 2>nul
  if exist "%LOG%" timeout /t 1 /nobreak >nul 2>nul
)
if exist "%LOG%" exit /b 1
exit /b 0

:purge_root_rasters
setlocal EnableExtensions DisableDelayedExpansion
for %%F in ("%BASE%\*.ico" "%BASE%\*.png") do (
  if exist "%%~fF" (
    echo [INFO] Eliminando raster proscrito de carpeta base: %%~nxF
    >> "%LOG%" echo [INFO] Eliminando raster proscrito: %%~fF
    del /f /q "%%~fF" >nul 2>> "%LOG%" || (endlocal & exit /b 1)
  )
)
endlocal & exit /b 0

:fatal_base
echo [ERROR] No pude entrar a la carpeta base: %BASE%.
>> "%LOG%" echo [ERROR] No pude entrar a %BASE%.
goto fail

:fail
echo [ERROR] Fallo el procesamiento de iconos. Revisa %LOG%.
if "%NO_PAUSE%"=="1" exit /b 1
echo.
echo Log: %LOG%
pause
exit /b 1
