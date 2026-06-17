@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title MiausoftSuite - Publicador central
cd /d "%~dp0"
set "SCRIPT_DIR=%~dp0"

if /I "%~1"=="/?" goto help
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help

set "EXTRA="
set "DRYRUN="
set "LOGIN="
set "TARGET="
set "HADARGS="

:parse_args
if "%~1"=="" goto args_done
set "HADARGS=1"
if /I "%~1"=="/simulacion" set "EXTRA=%EXTRA% -DryRun"&set "DRYRUN=1"&shift&goto parse_args
if /I "%~1"=="--dry-run" set "EXTRA=%EXTRA% -DryRun"&set "DRYRUN=1"&shift&goto parse_args
if /I "%~1"=="/sin-compilar" set "EXTRA=%EXTRA% -NoCompile"&shift&goto parse_args
if /I "%~1"=="--no-compile" set "EXTRA=%EXTRA% -NoCompile"&shift&goto parse_args
if /I "%~1"=="/iniciar-sesion" set "LOGIN=1"&shift&goto parse_args
if /I "%~1"=="--login" set "LOGIN=1"&shift&goto parse_args
if /I "%~1"=="Tlalpowa" set "TARGET=Tlalpowa"&shift&goto parse_args
if /I "%~1"=="Ilnamiki" set "TARGET=Ilnamiki"&shift&goto parse_args
if /I "%~1"=="Biblioteca" set "TARGET=Biblioteca"&shift&goto parse_args
if /I "%~1"=="MiausoftTools" set "TARGET=MiausoftTools"&shift&goto parse_args
if /I "%~1"=="Suite" set "TARGET=Suite"&shift&goto parse_args
if /I "%~1"=="Todo" set "TARGET=Todo"&shift&goto parse_args
if /I "%~1"=="ConvertidorCompleto" set "TARGET=ConvertidorCompleto"&shift&goto parse_args
if /I "%~1"=="ConvertidorCapitulos" set "TARGET=ConvertidorCapitulos"&shift&goto parse_args
if /I "%~1"=="FusionadorDivisor" set "TARGET=FusionadorDivisor"&shift&goto parse_args
if /I "%~1"=="Reemplazador" set "TARGET=Reemplazador"&shift&goto parse_args
if /I "%~1"=="Organizador" set "TARGET=Organizador"&shift&goto parse_args
if /I "%~1"=="Installer" set "TARGET=Installer"&shift&goto parse_args
echo Argumento no reconocido: %~1
echo Usa Publicar.cmd /? para ver la ayuda.
exit /b 2

:args_done
if "%LOGIN%"=="1" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CORE\Publicar.ps1" -Login
  set "RC=%ERRORLEVEL%"
  if not "%RC%"=="0" exit /b %RC%
  if "%TARGET%"=="" exit /b 0
)
if not "%TARGET%"=="" goto run

:main
cls
echo ============================================================
echo  MIAUSOFTSUITE - PUBLICADOR CENTRAL
echo ============================================================
echo  [1] Tlalpowa       ^> mauricioisbl/Tlalpowa
echo  [2] Ilnamiki       ^> mauricioisbl/MiausoftSuite
echo  [3] Biblioteca     ^> mauricioisbl/MiausoftSuite
echo  [4] MiausoftTools  ^> mauricioisbl/MiausoftSuite
echo  [5] Suite completa ^> mauricioisbl/MiausoftSuite
echo  [6] Publicar todo  ^> ambos repositorios
echo  [0] Salir
echo ============================================================
choice /C 1234560 /N /M "Pulsa un numero: "
if errorlevel 7 exit /b 0
if errorlevel 6 goto run_todo
if errorlevel 5 goto run_suite
if errorlevel 4 goto tools
if errorlevel 3 goto run_biblioteca
if errorlevel 2 goto run_ilnamiki
if errorlevel 1 goto run_tlalpowa

:tools
cls
echo ============================================================
echo  PUBLICAR MIAUSOFTTOOLS
echo ============================================================
echo  [1] Convertidor completo
echo  [2] Convertidor por capitulos
echo  [3] Fusionador y divisor
echo  [4] Reemplazador de caracteres
echo  [5] Organizador
echo  [6] Instalador
echo  [7] Todas las herramientas
echo  [0] Volver
echo ============================================================
choice /C 12345670 /N /M "Pulsa un numero: "
if errorlevel 8 goto main
if errorlevel 7 goto run_tools
if errorlevel 6 set "TARGET=Installer"&goto run
if errorlevel 5 set "TARGET=Organizador"&goto run
if errorlevel 4 set "TARGET=Reemplazador"&goto run
if errorlevel 3 set "TARGET=FusionadorDivisor"&goto run
if errorlevel 2 set "TARGET=ConvertidorCapitulos"&goto run
if errorlevel 1 set "TARGET=ConvertidorCompleto"&goto run

:run_tlalpowa
set "TARGET=Tlalpowa"
goto run
:run_ilnamiki
set "TARGET=Ilnamiki"
goto run
:run_biblioteca
set "TARGET=Biblioteca"
goto run
:run_tools
set "TARGET=MiausoftTools"
goto run
:run_suite
set "TARGET=Suite"
goto run
:run_todo
set "TARGET=Todo"

:run
cls
echo Publicando %TARGET%...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CORE\Publicar.ps1" -Target "%TARGET%" %EXTRA%
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [ERROR] La publicacion termino con codigo %RC%.
) else (
  if "%DRYRUN%"=="1" (
    echo [OK] Simulacion de %TARGET% completada.
  ) else (
    echo [OK] %TARGET% quedo publicado.
  )
)
if "%HADARGS%"=="1" exit /b %RC%
choice /C 10 /N /M "Pulsa 1 para volver al menu o 0 para salir: "
if errorlevel 2 exit /b %RC%
goto main

:help
echo(Publicar.cmd [Tlalpowa^|Ilnamiki^|Biblioteca^|MiausoftTools^|Suite^|Todo] [/simulacion] [/sin-compilar] [/iniciar-sesion]
echo(
echo(Publica el destino indicado. Sin argumentos abre el menu interactivo.
echo(
echo(  /?             Muestra esta ayuda.
echo(  --help         Muestra esta ayuda.
echo(  /simulacion    Prepara y detecta cambios sin commit ni push.
echo(  /sin-compilar  Publica sin ejecutar la compilacion previa.
echo(  /iniciar-sesion  Abre/valida el login GitHub de Git Credential Manager.
exit /b 0
