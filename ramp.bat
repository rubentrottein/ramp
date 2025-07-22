@echo off
SETLOCAL ENABLEEXTENSIONS

REM Définir les chemins relatifs
SET "BASE_DIR=%~dp0"
SET "APACHE_DIR=%BASE_DIR%Apache24"
SET "APACHE_BIN=%APACHE_DIR%\bin"
SET "HTTPD_EXE=%APACHE_BIN%\httpd.exe"
SET "HTTPD_CONF=%APACHE_DIR%\conf\httpd.conf"

:MENU
cls
echo ==========================
echo     🌐 RAMP - MENU
echo ==========================
echo 1. Démarrer le serveur Apache
echo 2. Arrêter le serveur Apache
echo 3. Ouvrir http://localhost
echo 4. Quitter
echo.

set /p choice=Choix [1-4] : 

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto OPEN
if "%choice%"=="4" goto END
goto MENU

:START
echo 🟢 Lancement du serveur Apache...
"%HTTPD_EXE%" -k start -f "%HTTPD_CONF%"
IF %ERRORLEVEL% EQU 0 (
    echo ✅ Apache démarre avec succès.
) ELSE (
    echo ❌ Erreur lors du démarrage.
)
timeout /t 2 >nul
goto MENU

:STOP
echo 🔴 Arrêt du serveur Apache...
"%HTTPD_EXE%" -k stop -f "%HTTPD_CONF%"
IF %ERRORLEVEL% EQU 0 (
    echo ✅ Apache arrêté.
) ELSE (
    echo ⚠️  Le service était peut-être déjà arrêté.
)
timeout /t 2 >nul
goto MENU

:OPEN
echo 🌐 Ouverture de http://localhost...
start "" http://localhost
timeout /t 1 >nul
goto MENU

:END
echo 👋 Au revoir !
timeout /t 1 >nul
exit
