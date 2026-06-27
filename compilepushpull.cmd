@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title MiausoftSuite - compile push pull
cd /d "%~dp0"
set "ROOT=%~dp0"
set "CORE=%ROOT%core"
set "LOG=%ROOT%PUBLICAR.LOG"
set "ARGMODE=0"
set "EXTRA_ARGS="

if /I "%~1"=="/?" goto help
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help

if /I "%~1"=="compilar" set "ARGMODE=1" & shift & goto compile_arg
if /I "%~1"=="compile" set "ARGMODE=1" & shift & goto compile_arg
if /I "%~1"=="publicar" set "ARGMODE=1" & shift & goto publish_arg
if /I "%~1"=="push" set "ARGMODE=1" & shift & goto publish_arg
if /I "%~1"=="jalar" set "ARGMODE=1" & shift & goto pull_arg
if /I "%~1"=="pull" set "ARGMODE=1" & shift & goto pull_arg
if /I "%~1"=="directorio" set "ARGMODE=1" & goto directory
if not "%~1"=="" goto help

:main
cls
echo ============================================================
echo  MIAUSOFTSUITE
echo ============================================================
echo  [1] Compilar
echo  [2] Publicar / push
echo  [3] Jalar / pull
echo  [4] Generar rutas_directorio.txt
echo  [0] Salir
echo ============================================================
choice /C 12340 /N /M "Pulsa un numero: "
if errorlevel 5 exit /b 0
if errorlevel 4 goto directory
if errorlevel 3 goto pull_menu
if errorlevel 2 goto publish_menu
if errorlevel 1 goto compile_menu

:compile_menu
call :target_menu "COMPILAR"
if "%TARGET%"=="" goto main
goto run_compile

:publish_menu
call :target_menu "PUBLICAR"
if "%TARGET%"=="" goto main
goto run_publish

:pull_menu
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%CORE%\JalarCambios.ps1"
set "RC=%ERRORLEVEL%"
pause
goto main

:compile_arg
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=Todo"
goto run_compile

:publish_arg
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=Todo"
set "EXTRA_ARGS=%~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9"
goto run_publish

:pull_arg
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%CORE%\JalarCambios.ps1" %~1 %~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9
exit /b %ERRORLEVEL%

:run_compile
echo Compilando %TARGET%...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%CORE%\Compilar.ps1" -Target "%TARGET%"
set "RC=%ERRORLEVEL%"
if "%ARGMODE%"=="1" exit /b %RC%
pause
goto main

:run_publish
echo Publicando %TARGET%...
echo Registro directo: %LOG%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%CORE%\Publicar.ps1" -Target "%TARGET%" %EXTRA_ARGS%
set "RC=%ERRORLEVEL%"
if "%ARGMODE%"=="1" exit /b %RC%
pause
goto main

:directory
python "%ROOT%directorio.py"
set "RC=%ERRORLEVEL%"
if "%ARGMODE%"=="1" exit /b %RC%
pause
goto main

:target_menu
set "TARGET="
cls
echo ============================================================
echo  %~1
echo ============================================================
echo  [1] Tlalpowa
echo  [2] Ilnamiki
echo  [3] Organizador
echo  [T] Todo
echo  [0] Volver
echo ============================================================
choice /C 123T0 /N /M "Pulsa una opcion: "
if errorlevel 5 exit /b 0
if errorlevel 4 set "TARGET=Todo"&exit /b 0
if errorlevel 3 set "TARGET=Organizador"&exit /b 0
if errorlevel 2 set "TARGET=Ilnamiki"&exit /b 0
if errorlevel 1 set "TARGET=Tlalpowa"&exit /b 0
exit /b 0

:help
echo compilepushpull.cmd [compilar^|publicar^|push^|jalar^|pull^|directorio] [destino] [opciones]
echo.
echo Destinos: Tlalpowa, Ilnamiki, Organizador, Todo.
echo.
echo Sin argumentos abre el indice interactivo.
exit /b 0
