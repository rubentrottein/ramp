@echo off
echo ====================================
echo PHP MySQL Diagnostic
echo ====================================

:: Get current directory
set "CURRENT_DIR=%~dp0"

echo.
echo 1. Checking PHP installation...
if exist "%CURRENT_DIR%php\php.exe" (
    echo ✓ PHP found at: %CURRENT_DIR%php\php.exe
    "%CURRENT_DIR%php\php.exe" -v
) else (
    echo ✗ PHP not found at: %CURRENT_DIR%php\php.exe
    echo Please check your PHP installation path
)

echo.
echo 2. Checking PHP extensions directory...
if exist "%CURRENT_DIR%php\ext" (
    echo ✓ Extensions directory found
    if exist "%CURRENT_DIR%php\ext\php_pdo_mysql.dll" (
        echo ✓ php_pdo_mysql.dll found
    ) else (
        echo ✗ php_pdo_mysql.dll NOT found
    )
    if exist "%CURRENT_DIR%php\ext\php_mysqli.dll" (
        echo ✓ php_mysqli.dll found
    ) else (
        echo ✗ php_mysqli.dll NOT found
    )
) else (
    echo ✗ Extensions directory not found at: %CURRENT_DIR%php\ext
)

echo.
echo 3. Checking MySQL client library...
if exist "%CURRENT_DIR%php\libmysql.dll" (
    echo ✓ libmysql.dll found in PHP directory
) else if exist "%CURRENT_DIR%php\libmariadb.dll" (
    echo ✓ libmariadb.dll found in PHP directory
) else if exist "%CURRENT_DIR%bin\libmysql.dll" (
    echo ✓ libmysql.dll found in Apache bin directory
) else if exist "%CURRENT_DIR%bin\libmariadb.dll" (
    echo ✓ libmariadb.dll found in Apache bin directory
) else (
    echo ✗ MySQL/MariaDB client library NOT found
    echo   Looking for: libmysql.dll or libmariadb.dll
)

echo.
echo 4. Checking php.ini...
if exist "%CURRENT_DIR%php\php.ini" (
    echo ✓ php.ini found
    echo.
    echo Extension directory setting:
    findstr /C:"extension_dir" "%CURRENT_DIR%php\php.ini"
    echo.
    echo MySQL extensions:
    findstr /C:"extension=pdo_mysql" "%CURRENT_DIR%php\php.ini"
    findstr /C:"extension=mysqli" "%CURRENT_DIR%php\php.ini"
) else (
    echo ✗ php.ini not found at: %CURRENT_DIR%php\php.ini
)

echo.
echo 5. Testing PHP extensions...
if exist "%CURRENT_DIR%php\php.exe" (
    echo Loaded extensions:
    "%CURRENT_DIR%php\php.exe" -m | findstr -i mysql
    if %ERRORLEVEL% NEQ 0 (
        echo ✗ No MySQL extensions loaded
    )
)

echo.
echo ====================================
echo Diagnostic complete
echo ====================================
pause