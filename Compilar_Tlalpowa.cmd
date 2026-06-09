@echo off
REM Este archivo debe conservar finales CRLF: cmd.exe puede mutilar el primer caracter de comandos si se distribuye con LF puro.
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title Tlalpowa - Compilador CMD

for %%I in ("%~dp0.") do set "BASE=%%~fI"
for %%I in ("%~dp0.") do set "BASE_SHORT=%%~sI"
if not defined BASE_SHORT set "BASE_SHORT=%BASE%"
set "LOG=%BASE%\Compilar_Tlalpowa.log"
set "NO_PAUSE=0"
if not "%~1"=="" (
  for %%A in (%*) do (
    if /I "%%~A"=="/nopause" set "NO_PAUSE=1"
    if /I "%%~A"=="--nopause" set "NO_PAUSE=1"
  )
)

> "%LOG%" echo ============================================================
>> "%LOG%" echo Tlalpowa - log de compilacion
>> "%LOG%" echo ============================================================
>> "%LOG%" echo Inicio: %DATE% %TIME%
>> "%LOG%" echo CMD: %~f0
>> "%LOG%" echo Base: %BASE%
>> "%LOG%" echo ============================================================
>> "%LOG%" echo.

echo ============================================================
echo  Tlalpowa - compilador unico CMD
echo ============================================================
echo  Log: %LOG%
echo  Producto: %BASE%\Tlalpowa.exe
echo ============================================================
echo.

call :main %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  echo.
  echo [ERROR] Compilacion interrumpida. Ultimas lineas del log:
  echo ------------------------------------------------------------
  call :tail_log 80
  echo ------------------------------------------------------------
)
echo.
echo [LOG] %LOG%
echo [CODIGO DE SALIDA] %RC%
if "%NO_PAUSE%"=="1" exit /b %RC%
echo.
echo La consola queda abierta. Escribe exit y presiona Enter para cerrarla.
%ComSpec% /d /k
exit /b %RC%

:main
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%BASE%" || goto fatal_cd
set "BUILD=%BASE%\Build"
set "STAGE=%BUILD%\Producto"
set "STAGED_EXE=%STAGE%\Tlalpowa.exe"
set "CMAKE_BASE=%BASE_SHORT%"
set "CMAKE_BUILD=%BASE_SHORT%\Build"
set "CMAKE_STAGE=%CMAKE_BUILD%\Producto"
set "SOURCE=%BASE%\Fuente"
set "CONFIG_DIR=%SOURCE%\Config"
set "EXTERNAL=%BASE%\Descargas"
set "INTERNAL=%BASE%\Datos"
set "EXE=%BASE%\Tlalpowa.exe"
set "CONFIG=Release"
set "ENABLE_IMGUI=ON"
set "DO_CLEAN=0"
set "DO_SELFTEST=1"
set "DO_RUN=0"
set "DO_ICON_PNGS=1"
set "PURGE_06=0"
set "GEN=Ninja"
set "PYTHONUTF8=1"
set "TLALPOWA_UTF8=1"
set "TLALPOWA_BUILD_DIR=%BUILD%"
set "TLALPOWA_SOURCE_DIR=%SOURCE%"
set "TLALPOWA_CONFIG_DIR=%CONFIG_DIR%"
set "TLALPOWA_EXTERNAL_DATA_DIR=%EXTERNAL%"
set "TLALPOWA_INTERNAL_DATA_DIR=%INTERNAL%"
set "DEPS_ROOT=%ProgramData%\Tlalpowa\Dependencias"
set "TLALPOWA_DEPS_ROOT=%DEPS_ROOT%"
set "BUILD_JOBS=2"

:parse_args
if "%~1"=="" goto after_args
if /I "%~1"=="clean" set "DO_CLEAN=1"& shift & goto parse_args
if /I "%~1"=="/clean" set "DO_CLEAN=1"& shift & goto parse_args
if /I "%~1"=="--clean" set "DO_CLEAN=1"& shift & goto parse_args
if /I "%~1"=="noclean" set "DO_CLEAN=0"& shift & goto parse_args
if /I "%~1"=="/noclean" set "DO_CLEAN=0"& shift & goto parse_args
if /I "%~1"=="--noclean" set "DO_CLEAN=0"& shift & goto parse_args
if /I "%~1"=="debug" set "CONFIG=Debug"& shift & goto parse_args
if /I "%~1"=="/debug" set "CONFIG=Debug"& shift & goto parse_args
if /I "%~1"=="--debug" set "CONFIG=Debug"& shift & goto parse_args
if /I "%~1"=="release" set "CONFIG=Release"& shift & goto parse_args
if /I "%~1"=="/release" set "CONFIG=Release"& shift & goto parse_args
if /I "%~1"=="--release" set "CONFIG=Release"& shift & goto parse_args
if /I "%~1"=="core" set "ENABLE_IMGUI=OFF"& shift & goto parse_args
if /I "%~1"=="/core" set "ENABLE_IMGUI=OFF"& shift & goto parse_args
if /I "%~1"=="/noimgui" set "ENABLE_IMGUI=OFF"& shift & goto parse_args
if /I "%~1"=="--noimgui" set "ENABLE_IMGUI=OFF"& shift & goto parse_args
if /I "%~1"=="/noselftest" set "DO_SELFTEST=0"& shift & goto parse_args
if /I "%~1"=="--noselftest" set "DO_SELFTEST=0"& shift & goto parse_args
if /I "%~1"=="/iconpng" set "DO_ICON_PNGS=1"& shift & goto parse_args
if /I "%~1"=="--iconpng" set "DO_ICON_PNGS=1"& shift & goto parse_args
if /I "%~1"=="/noiconpng" set "DO_ICON_PNGS=0"& shift & goto parse_args
if /I "%~1"=="--noiconpng" set "DO_ICON_PNGS=0"& shift & goto parse_args
if /I "%~1"=="run" set "DO_RUN=1"& shift & goto parse_args
if /I "%~1"=="/run" set "DO_RUN=1"& shift & goto parse_args
if /I "%~1"=="--run" set "DO_RUN=1"& shift & goto parse_args
if /I "%~1"=="/purge06" set "PURGE_06=1"& shift & goto parse_args
if /I "%~1"=="--purge06" set "PURGE_06=1"& shift & goto parse_args
if /I "%~1"=="ninja" set "GEN=Ninja"& shift & goto parse_args
if /I "%~1"=="/ninja" set "GEN=Ninja"& shift & goto parse_args
if /I "%~1"=="--ninja" set "GEN=Ninja"& shift & goto parse_args
if /I "%~1"=="nmake" set "GEN=NMake Makefiles"& shift & goto parse_args
if /I "%~1"=="/nmake" set "GEN=NMake Makefiles"& shift & goto parse_args
if /I "%~1"=="--nmake" set "GEN=NMake Makefiles"& shift & goto parse_args
if /I "%~1"=="/nopause" shift & goto parse_args
if /I "%~1"=="--nopause" shift & goto parse_args
if /I "%~1"=="/help" goto help
if /I "%~1"=="--help" goto help
echo [ERROR] Argumento no reconocido: %~1
goto help_error

:after_args
call :progress 5 "Validando proyecto"
echo Base:        %BASE%
echo Fuente:      %SOURCE%
echo Build:       %BUILD%
echo Stage:       %STAGE%
echo Modo:        %CONFIG%
echo ImGui:       %ENABLE_IMGUI%
echo Generador:   %GEN%
echo Iconos PNG:  %DO_ICON_PNGS%
echo.
>> "%LOG%" echo Base: %BASE%
>> "%LOG%" echo Build: %BUILD%
>> "%LOG%" echo Stage: %STAGE%
>> "%LOG%" echo Modo: %CONFIG%
>> "%LOG%" echo ImGui: %ENABLE_IMGUI%
>> "%LOG%" echo Generador: %GEN%
>> "%LOG%" echo Iconos PNG: %DO_ICON_PNGS%

if not exist "%BASE%\CMakeLists.txt" goto missing_project
if not exist "%SOURCE%" goto missing_project
if not exist "%CONFIG_DIR%\diseases.tsv" goto missing_config
if not exist "%EXTERNAL%" mkdir "%EXTERNAL%" || goto mkdir_fail
if not exist "%INTERNAL%" mkdir "%INTERNAL%" || goto mkdir_fail

call :progress 12 "Proscribiendo raster base"
call :purge_forbidden_root_rasters
if errorlevel 1 goto icon_fail

call :progress 15 "Preparando carpetas"
if "%PURGE_06%"=="1" if exist "%BASE%\06_Build" rmdir /s /q "%BASE%\06_Build" || goto purge06_fail
if "%DO_CLEAN%"=="1" if exist "%BUILD%" rmdir /s /q "%BUILD%" || goto clean_fail
if exist "%BUILD%\CMakeCache.txt" (
  call :validate_cmake_cache
  if errorlevel 1 goto cache_fail
)
if not exist "%BUILD%" mkdir "%BUILD%" || goto mkdir_fail
if not exist "%STAGE%" mkdir "%STAGE%" || goto mkdir_fail
if exist "%STAGE%\Dependencias" rmdir /s /q "%STAGE%\Dependencias" || goto deps_stage_clean_fail

call :progress 25 "Detectando MSVC"
set "VCVARS="
set "CL_EXE="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS goto msvc_fail
call "%VCVARS%" >> "%LOG%" 2>&1 || goto vcvars_fail
where cl.exe >nul 2>nul || goto cl_fail
for /f "delims=" %%I in ('where cl.exe') do if not defined CL_EXE set "CL_EXE=%%I"
echo [MSVC] Visual Studio 2026: %VCVARS%>> "%LOG%"
echo [MSVC] Compilador C++: %CL_EXE%>> "%LOG%"
echo [MSVC] VisualStudioVersion: %VisualStudioVersion%>> "%LOG%"
echo [MSVC] Version:>> "%LOG%"
cl.exe 2>> "%LOG%" 1>&2
if /I not "%VisualStudioVersion%"=="18.0" goto msvc_version_fail
where cmake.exe >nul 2>nul || goto cmake_fail
if /I "%GEN%"=="Ninja" where ninja.exe >nul 2>nul || goto ninja_fail
if /I "%GEN%"=="NMake Makefiles" where nmake.exe >nul 2>nul || goto nmake_fail

if "%DO_ICON_PNGS%"=="1" (
  call :progress 30 "Procesando iconos PNG"
  call :process_icon_pngs
  if errorlevel 1 goto icon_png_fail
)

call :progress 34 "Liberando EXE anterior"
call :unlock_final_exe

call :progress 48 "Configurando CMake"
call :run_live cmake.exe -S "%CMAKE_BASE%" -B "%CMAKE_BUILD%" -G "%GEN%" "-DCMAKE_BUILD_TYPE:STRING=%CONFIG%" "-DCMAKE_TRY_COMPILE_CONFIGURATION:STRING=Release" "-DCMAKE_HAVE_LIBC_PTHREAD:BOOL=OFF" "-DCMAKE_USE_WIN32_THREADS_INIT:BOOL=ON" "-DCMAKE_USE_PTHREADS_INIT:BOOL=OFF" "-DTHREADS_PREFER_PTHREAD_FLAG:BOOL=OFF" "-DTLALPOWA_ENABLE_IMGUI:BOOL=%ENABLE_IMGUI%" "-DTLALPOWA_ENABLE_LTCG:BOOL=OFF" "-DTLALPOWA_ENABLE_CL_MP:BOOL=OFF" "-DTLALPOWA_PROCESS_ICON_PNGS:BOOL=OFF" "-DTLALPOWA_DEPS_ROOT:PATH=%DEPS_ROOT%" "-DTLALPOWA_RUNTIME_OUTPUT_DIR:PATH=%CMAKE_STAGE%" "-DCMAKE_PDB_OUTPUT_DIRECTORY:PATH=%CMAKE_BUILD%" "-DCMAKE_COMPILE_PDB_OUTPUT_DIRECTORY:PATH=%CMAKE_BUILD%" "-DCMAKE_SUPPRESS_REGENERATION:BOOL=ON"
if errorlevel 1 goto configure_fail

call :progress 66 "Compilando"
if /I "%GEN%"=="Ninja" (
  call :run_live cmake.exe --build "%CMAKE_BUILD%" --config "%CONFIG%" -- -j %BUILD_JOBS%
) else (
  call :run_live cmake.exe --build "%CMAKE_BUILD%" --config "%CONFIG%"
)
if errorlevel 1 goto build_fail

call :progress 88 "Instalando EXE unico"
if not exist "%STAGED_EXE%" goto exe_missing_stage
call :unlock_final_exe
copy /y "%STAGED_EXE%" "%EXE%" >nul || goto install_fail
if not exist "%EXE%" goto exe_missing
if exist "%STAGE%\Dependencias" (
  if exist "%BASE%\Dependencias" rmdir /s /q "%BASE%\Dependencias" || goto deps_package_fail
  robocopy "%STAGE%\Dependencias" "%BASE%\Dependencias" /MIR /NFL /NDL /NJH /NJS /NP >> "%LOG%" 2>&1
  if errorlevel 8 goto deps_package_fail
  echo [OK] Dependencias portables: %BASE%\Dependencias
  >> "%LOG%" echo [OK] Dependencias portables: %BASE%\Dependencias
)
if exist "%STAGE%" del /f /q "%STAGE%\*.exe" >nul 2>nul

call :progress 95 "Selftest"
if "%DO_SELFTEST%"=="1" call :run_live "%EXE%" selftest
if errorlevel 1 goto selftest_fail
if "%DO_RUN%"=="1" start "" "%EXE%"
call :progress 100 "Terminado"
echo.
echo [OK] Compilacion finalizada. EXE final: %EXE%
echo [OK] Para subirlo usa Publicar_Tlalpowa.cmd; la carpeta base no se convierte en repositorio Git.
>> "%LOG%" echo [OK] Compilacion finalizada. EXE final: %EXE%
exit /b 0

:progress
setlocal EnableDelayedExpansion
set /a P=%~1
set "TXT=%~2"
set /a DONE=P/5
set "BAR="
for /L %%B in (1,1,20) do (
  if %%B LEQ !DONE! (set "BAR=!BAR!#") else (set "BAR=!BAR!-")
)
echo [!BAR!] !P!%%  !TXT!
>> "%LOG%" echo [PROGRESO] !P!%% !TXT!
endlocal & exit /b 0

:run_live
setlocal EnableExtensions DisableDelayedExpansion
set "RUN_OUT=%TEMP%\tlalpowa_run_%RANDOM%%RANDOM%.txt"
>> "%LOG%" echo.
>> "%LOG%" echo [CMD] %*
%* > "%RUN_OUT%" 2>&1
set "CMD_RC=%ERRORLEVEL%"
type "%RUN_OUT%"
type "%RUN_OUT%" >> "%LOG%"
del /f /q "%RUN_OUT%" >nul 2>nul
endlocal & exit /b %CMD_RC%

:tail_log
setlocal EnableDelayedExpansion
set "N=%~1"
if not defined N set "N=60"
set "TMP_TAIL=%TEMP%\tlalpowa_tail_%RANDOM%%RANDOM%.txt"
copy /y "%LOG%" "%TMP_TAIL%" >nul 2>nul
for /f "delims=" %%L in ('find /c /v "" ^< "%TMP_TAIL%"') do set "COUNT=%%L"
set /a START=COUNT-N
if !START! LSS 0 set /a START=0
set /a I=0
for /f "usebackq delims=" %%L in ("%TMP_TAIL%") do (
  if !I! GEQ !START! echo(%%L
  set /a I+=1
)
del /f /q "%TMP_TAIL%" >nul 2>nul
endlocal & exit /b 0

:unlock_final_exe
taskkill /IM "Tlalpowa.exe" /F >nul 2>nul
for /L %%R in (1,1,3) do (
  if exist "%EXE%" (
    attrib -R "%EXE%" >nul 2>nul
    del /f /q "%EXE%" >nul 2>nul
    if exist "%EXE%" timeout /t 1 /nobreak >nul 2>nul
  )
)
exit /b 0

:validate_cmake_cache
setlocal EnableExtensions DisableDelayedExpansion
set "CACHE=%BUILD%\CMakeCache.txt"
set "NEEDS_CLEAN=0"
findstr /C:"CMAKE_GENERATOR:INTERNAL=%GEN%" "%CACHE%" >nul 2>nul || set "NEEDS_CLEAN=1"
if "%NEEDS_CLEAN%"=="0" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$cache=$env:CACHE; $base=(Resolve-Path -LiteralPath $env:BASE).ProviderPath; $build=Join-Path $base 'Build'; $cmakeBase=$env:CMAKE_BASE; $cmakeBuild=$env:CMAKE_BUILD; $sourcePattern='(?m)^Tlalpowa_SOURCE_DIR:STATIC=(.+)$'; $binaryPattern='(?m)^Tlalpowa_BINARY_DIR:STATIC=(.+)$'; $text=Get-Content -LiteralPath $cache -Raw -ErrorAction Stop; function SamePath($left, $right) { try { $l=[System.IO.Path]::GetFullPath(($left -replace '/','\')).TrimEnd('\'); $r=[System.IO.Path]::GetFullPath(($right -replace '/','\')).TrimEnd('\'); return [string]::Equals($l,$r,[System.StringComparison]::OrdinalIgnoreCase) } catch { return $false } }; function MatchesAny($left, $paths) { foreach($path in $paths) { if($path -and (SamePath $left $path)) { return $true } }; return $false }; if($text -match $sourcePattern -and -not (MatchesAny $Matches[1].Trim() @($base,$cmakeBase))) { exit 20 }; if($text -match $binaryPattern -and -not (MatchesAny $Matches[1].Trim() @($build,$cmakeBuild))) { exit 21 }; exit 0" >nul 2>> "%LOG%"
  if errorlevel 1 set "NEEDS_CLEAN=1"
)
if "%NEEDS_CLEAN%"=="1" (
  echo [INFO] Build pertenece a otra ruta o generador; lo regenero automaticamente.
  >> "%LOG%" echo [INFO] Build pertenece a otra ruta o generador; lo regenero automaticamente.
  rmdir /s /q "%BUILD%" || (endlocal & exit /b 1)
)
endlocal & exit /b 0


:process_icon_pngs
if not exist "%BASE%\Procesar_Iconos_Tlalpowa.cmd" (
  echo [ERROR] Falta Procesar_Iconos_Tlalpowa.cmd para normalizar iconos PNG.
  >> "%LOG%" echo [ERROR] Falta Procesar_Iconos_Tlalpowa.cmd para normalizar iconos PNG.
  exit /b 1
)
REM Procesa PNGs de iconografía antes de configurar CMake.
call :run_live %ComSpec% /d /c call "%BASE%\Procesar_Iconos_Tlalpowa.cmd" /nopause --root "%BASE%"
exit /b %ERRORLEVEL%


:purge_forbidden_root_rasters
setlocal EnableExtensions DisableDelayedExpansion
set "PURGED=0"
for %%F in ("%BASE%\*.ico" "%BASE%\*.png") do (
  if exist "%%~fF" (
    echo [INFO] Eliminando raster proscrito de carpeta base: %%~nxF
    >> "%LOG%" echo [INFO] Eliminando raster proscrito de carpeta base: %%~fF
    del /f /q "%%~fF" >nul 2>> "%LOG%" || (
      echo [ERROR] No pude eliminar raster proscrito de carpeta base: %%~fF
      >> "%LOG%" echo [ERROR] No pude eliminar raster proscrito de carpeta base: %%~fF
      endlocal & exit /b 1
    )
    set "PURGED=1"
  )
)
if not exist "%BASE%\Datos\icon\tlalpowa.png" (
  echo [ERROR] Falta Datos\icon\tlalpowa.png; el ICO debe generarse desde el catálogo icon, no desde la base.
  >> "%LOG%" echo [ERROR] Falta Datos\icon\tlalpowa.png para generar Fuente\Tlalpowa.ico
  endlocal & exit /b 1
)
if not exist "%SOURCE%\Tools\Convert-PngToIco.ps1" (
  echo [ERROR] Falta Fuente\Tools\Convert-PngToIco.ps1 para generar el unico Fuente\Tlalpowa.ico.
  >> "%LOG%" echo [ERROR] Falta Fuente\Tools\Convert-PngToIco.ps1
  endlocal & exit /b 1
)
echo [OK] Carpeta base libre de *.ico y *.png; el unico ICO permitido se regenerara en Fuente.
>> "%LOG%" echo [OK] Carpeta base libre de *.ico y *.png; fuente canonical: Datos\icon\tlalpowa.png; salida: Fuente\Tlalpowa.ico
endlocal & exit /b 0


:help
echo Uso:
echo   Compilar_Tlalpowa.cmd [clean] [debug^|release] [core] [run] [/noselftest] [/noiconpng] [/purge06] [/nmake^|/ninja] [/nopause]
echo.
echo Opciones:
echo   clean       Borra Build antes de configurar.
echo   noclean     Reutiliza Build. Es el valor predeterminado.
echo   debug       Compila Debug.
echo   release     Compila Release. Es el valor predeterminado.
echo   core        Compila sin ImGui/GLFW para diagnosticar el nucleo.
echo   run         Abre el EXE al terminar.
echo   /noselftest Omite la prueba selftest.
echo   /iconpng    Normaliza PNGs de iconografia antes de compilar. Es el valor predeterminado.
echo   /noiconpng  Omite la normalizacion previa de iconos PNG.
echo   /purge06    Borra la antigua carpeta 06_Build si existe.
echo   /nmake      Usa NMake Makefiles.
echo   /ninja      Usa Ninja. Es el valor predeterminado.
echo   /nopause    Sale con codigo de error sin dejar la consola abierta.
echo.
echo Icono:
echo   Si existe Datos\icon\tlalpowa.png, siempre se regenera el unico Fuente\Tlalpowa.ico antes de compilar.
echo   La normalizacion masiva de PNGs en Datos\icon corre antes de configurar; /noiconpng la omite.
echo.
exit /b 0

:help_error
exit /b 2
:missing_project
echo [ERROR] Este CMD debe ejecutarse desde la carpeta base del proyecto, junto a CMakeLists.txt y Fuente.
exit /b 1
:missing_config
echo [ERROR] No encontre la configuracion ligera esperada en Fuente\Config\diseases.tsv.
exit /b 1
:mkdir_fail
echo [ERROR] No pude crear una carpeta requerida. Revisa permisos y rutas.
exit /b 1
:icon_fail
echo [ERROR] No pude dejar la carpeta base libre de *.ico/*.png o falta Datos\icon\tlalpowa.png.
exit /b 1
:purge06_fail
echo [ERROR] No pude borrar 06_Build. Cierra procesos o ventanas abiertas dentro de esa carpeta.
exit /b 1
:clean_fail
echo [ERROR] No pude limpiar Build. Cierra procesos de compilacion o ventanas abiertas dentro de Build.
exit /b 1
:msvc_fail
echo [ERROR] No encontre MSVC x64. Instala Visual Studio Build Tools con Desktop development with C++.
exit /b 1
:vcvars_fail
echo [ERROR] vcvars64.bat fallo al preparar el entorno de compilacion.
exit /b 1
:cl_fail
echo [ERROR] cl.exe no quedo disponible despues de vcvars64.bat.
exit /b 1
:msvc_version_fail
echo [ERROR] El compilador activo no pertenece a Visual Studio 2026 ^(Visual Studio 18^).
echo [ERROR] Ruta detectada: %CL_EXE%
exit /b 1
:cmake_fail
echo [ERROR] cmake.exe no esta disponible. Instala CMake o agregalo al PATH.
exit /b 1
:nmake_fail
echo [ERROR] No encontre NMake. Repara MSVC o ejecuta con /ninja si Ninja esta disponible.
exit /b 1
:ninja_fail
echo [ERROR] Pediste Ninja, pero ninja.exe no esta disponible en PATH.
exit /b 1
:cache_fail
echo [ERROR] No pude validar o regenerar Build\CMakeCache.txt. Cierra procesos dentro de Build y reintenta con /clean.
exit /b 1
:icon_png_fail
echo [ERROR] Fallo la normalizacion previa de iconos PNG. Revisa Procesar_Iconos_Tlalpowa.log.
exit /b 1
:configure_fail
echo [ERROR] Fallo la configuracion CMake. Revisa el diagnostico anterior.
exit /b 1
:build_fail
echo [ERROR] Fallo la compilacion. Revisa el primer error de C++/RC/linker mostrado arriba.
exit /b 1
:exe_missing_stage
echo [ERROR] CMake termino, pero no encontre el EXE de staging: %STAGED_EXE%
exit /b 1
:install_fail
echo [ERROR] No pude instalar el EXE final en carpeta base. Cierra Tlalpowa, antivirus o exploradores que lo bloqueen y reintenta.
exit /b 1
:exe_missing
echo [ERROR] No encontre el EXE final: %EXE%
exit /b 1
:deps_package_fail
echo [ERROR] No pude empaquetar Dependencias junto al EXE.
exit /b 1
:deps_stage_clean_fail
echo [ERROR] No pude limpiar Dependencias de staging. Cierra procesos o ventanas abiertas dentro de Build\Producto.
exit /b 1
:selftest_fail
echo [ERROR] El ejecutable compilo, pero selftest devolvio error.
exit /b 1
:fatal_cd
echo [ERROR] No pude entrar a la carpeta base del script.
exit /b 1
