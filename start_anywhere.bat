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

REM Lancer Apache en arrière-plan
start "" "%HTTPD_EXE%" -f "%HTTPD_CONF%"

REM Ouvrir le navigateur
timeout /t 2 >nul
start "" http://localhost

ENDLOCAL
