@echo off
SETLOCAL ENABLEEXTENSIONS

REM Détecte le dossier racine du script
SET "CURRENT_DIR=%~dp0"
SET "APACHE_DIR=%CURRENT_DIR%Apache24"
SET "APACHE_BIN=%APACHE_DIR%\bin"
SET "HTTPD_EXE=%APACHE_BIN%\httpd.exe"
SET "HTTPD_CONF=%APACHE_DIR%\conf\httpd.conf"

REM Démarre Apache avec le bon fichier de configuration
echo 🟢 Lancement du serveur Apache...
"%HTTPD_EXE%" -k start -f "%HTTPD_CONF%"

IF %ERRORLEVEL% EQU 0 (
    echo ✅ Apache démarré avec succès.
    timeout /t 2 >nul
    echo 🌐 Ouverture de http://localhost dans le navigateur...
    start "" http://localhost
) ELSE (
    echo ❌ Erreur lors du démarrage d'Apache.
)

ENDLOCAL
