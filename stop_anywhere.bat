@echo off
SETLOCAL ENABLEEXTENSIONS

REM Détecter le chemin absolu du dossier courant (où se trouve ce script)
SET "CURRENT_DIR=%~dp0"
SET "APACHE_DIR=%CURRENT_DIR%Apache24"
SET "APACHE_BIN=%APACHE_DIR%\bin"
SET "HTTPD_EXE=%APACHE_BIN%\httpd.exe"
SET "HTTPD_CONF=%APACHE_DIR%\conf\httpd.conf"

REM Vérifie si Apache est en cours d'exécution
echo 🔴 Tentative d'arrêt du serveur Apache...
"%HTTPD_EXE%" -k stop -f "%HTTPD_CONF%"

IF %ERRORLEVEL% EQU 0 (
    echo ✅ Apache arrêté avec succès.
) ELSE (
    echo ⚠️  Apache n'était peut-être pas démarré ou une erreur est survenue.
)

ENDLOCAL
