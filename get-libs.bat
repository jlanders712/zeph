@echo off
rem Double-click this file (it must sit inside the phone folder).
rem Downloads the map libraries into the lib folder. Needs internet, Windows 10 or newer.
cd /d "%~dp0"
if not exist lib mkdir lib
echo Downloading map libraries into %cd%\lib ...
curl.exe -fL --retry 3 -o lib\maplibre-gl.js  https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.js  || goto fail
curl.exe -fL --retry 3 -o lib\maplibre-gl.css https://cdn.jsdelivr.net/npm/maplibre-gl@4/dist/maplibre-gl.css || goto fail
curl.exe -fL --retry 3 -o lib\pmtiles.js      https://cdn.jsdelivr.net/npm/pmtiles@3/dist/pmtiles.js          || goto fail
echo.
echo DONE. The lib folder now has maplibre-gl.js, maplibre-gl.css and pmtiles.js.
pause
exit /b 0
:fail
echo.
echo DOWNLOAD FAILED. Check your internet connection, or save the three links by hand (see README).
pause
exit /b 1
