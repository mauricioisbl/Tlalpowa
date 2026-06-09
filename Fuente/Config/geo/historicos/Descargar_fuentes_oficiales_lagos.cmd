@echo off
setlocal EnableExtensions
set "BASE=%~dp0"
set "URL_CDMX=https://datos.cdmx.gob.mx/dataset/b8f7ada4-6a3a-46ff-8dcb-d9ba00d4c58b/resource/5f5de1e3-e530-460d-9829-0f1c61099b7e/download/lago-de-texcoco-1519.json"
set "URL_REPSA_HTTPS=https://www.repsa.unam.mx/documentos/Cuenca-Lagos.kmz"
set "URL_REPSA_HTTP=http://www.repsa.unam.mx/documentos/Cuenca-Lagos.kmz"

echo Descargando Lago de Texcoco 1519 GeoJSON oficial de Datos CDMX...
curl -L --fail --retry 3 --connect-timeout 20 "%URL_CDMX%" -o "%BASE%lago-de-texcoco-1519.geojson"

echo Descargando Cuenca-Lagos.kmz oficial de REPSA/UNAM...
curl -L --fail --retry 3 --connect-timeout 20 "%URL_REPSA_HTTPS%" -o "%BASE%Cuenca-Lagos.kmz"
if errorlevel 1 (
  echo Reintentando REPSA por HTTP...
  curl -L --fail --retry 3 --connect-timeout 20 "%URL_REPSA_HTTP%" -o "%BASE%Cuenca-Lagos.kmz"
)

echo.
echo Listo. Si alguna descarga fallo, coloque manualmente los archivos oficiales con estos nombres:
echo   %BASE%lago-de-texcoco-1519.geojson
echo   %BASE%Cuenca-Lagos.kmz
pause
