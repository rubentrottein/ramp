@echo off
echo.
echo === Arrêt d'Apache 2.4 ===
cd /d C:\Apache24\bin
httpd.exe -k stop
echo.
pause
