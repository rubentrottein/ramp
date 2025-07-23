@echo off
SETLOCAL ENABLEEXTENSIONS

REM Détecte le dossier racine du script
SET "BASE_DIR=%~dp0"
SET "APACHE_DIR=%BASE_DIR%"
SET "APACHE_BIN=%APACHE_DIR%\bin"
SET "HTTPD_EXE=%APACHE_BIN%\httpd.exe"
SET "HTTPD_CONF=%APACHE_DIR%\conf\httpd.conf"

REM Définir la racine Apache pour httpd.conf
SET SRVROOT=%BASE_DIR:\=/%

REM Arrêter Apache proprement
"%HTTPD_EXE%" -k stop -f "%HTTPD_CONF%"

ENDLOCAL