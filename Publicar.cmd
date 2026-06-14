@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title MiausoftSuite - Publicador central
cd /d "%~dp0"

if /I "%~1"=="Tlalpowa" goto run_tlalpowa
if /I "%~1"=="Ilnamiki" goto run_ilnamiki
if /I "%~1"=="Biblioteca" goto run_biblioteca
if /I "%~1"=="MiausoftTools" goto run_tools
if /I "%~1"=="Suite" goto run_suite
if /I "%~1"=="Todo" goto run_todo

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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0CORE\Publicar.ps1" -Target "%TARGET%"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" (
  echo [ERROR] La publicacion termino con codigo %RC%.
) else (
  echo [OK] %TARGET% quedo publicado.
)
if not "%~1"=="" exit /b %RC%
choice /C 10 /N /M "Pulsa 1 para volver al menu o 0 para salir: "
if errorlevel 2 exit /b %RC%
goto main
