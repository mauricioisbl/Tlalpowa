@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title MiausoftSuite - Jalar cambios
cd /d "%~dp0"
set "SCRIPT_DIR=%~dp0"

if /I "%~1"=="/?" goto help
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help

set "NOPAUSE="
set "DRYRUN="
set "HADARGS="

:parse_args
if "%~1"=="" goto run
set "HADARGS=1"
if /I "%~1"=="/sin-pausa" set "NOPAUSE=-NoPause"&shift&goto parse_args
if /I "%~1"=="/simulacion" set "DRYRUN=-DryRun"&shift&goto parse_args
if /I "%~1"=="--dry-run" set "DRYRUN=-DryRun"&shift&goto parse_args
echo Argumento no reconocido: %~1
echo Usa JalarCambios.cmd /? para ver la ayuda.
exit /b 2

:run
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CORE\JalarCambios.ps1" %NOPAUSE% %DRYRUN%
set "EXITCODE=%ERRORLEVEL%"

if "%HADARGS%"=="" pause
exit /b %EXITCODE%

:help
echo(JalarCambios.cmd [/sin-pausa] [/simulacion]
echo(
echo(Jala la rama main de MiausoftSuite desde GitHub con rebase y autostash.
echo(Detecta archivos cambiados, eliminados y renombrados; si D: esta conectado
echo(y no es el origen del codigo, sincroniza D:\MiausoftSuite por cambio de KB.
echo(
echo(  /?          Muestra esta ayuda.
echo(  /sin-pausa  Ejecuta sin pausa final.
echo(  /simulacion  Detecta cambios contra origin/main sin tocar archivos ni D:.
exit /b 0
