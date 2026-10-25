@echo off
setlocal
cd /d "%~dp0.."

echo ========================================================
echo  Generando instalador de Windows con NSIS (makensis)
echo ========================================================

set "MAKENSIS="

if exist "C:\Program Files (x86)\NSIS\makensis.exe" goto :found_x86
if exist "C:\Program Files\NSIS\makensis.exe" goto :found_x64
goto :search_path

:found_x86
set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
goto :check_makensis

:found_x64
set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
goto :check_makensis

:search_path
where makensis >nul 2>&1
if not errorlevel 1 set "MAKENSIS=makensis"

:check_makensis
if "%MAKENSIS%"=="" (
    echo [ERROR] No se encontro makensis.exe en el sistema.
    echo Asegurate de que NSIS este instalado en C:\Program Files ^(x86^)\NSIS
    exit /b 1
)

if not exist "build-win\ProyecThor.exe" (
    echo [ERROR] No se encontro build-win\ProyecThor.exe.
    echo Compila la aplicacion primero ejecutando scripts\build-win.bat
    exit /b 1
)

if not exist "packaging\windows\dist" mkdir "packaging\windows\dist"

set "VERSION=1.1.0"
"%MAKENSIS%" -DVERSION="%VERSION%" "-DBUILD_DIR=%CD%\build-win" "-DOUT_DIR=%CD%\packaging\windows\dist" "packaging\windows\ProyecThor.nsi"

if errorlevel 1 (
    echo [ERROR] Fallo la generacion del instalador NSIS.
    exit /b 1
)

echo.
echo [OK] Instalador generado exitosamente en:
echo      packaging\windows\dist\ProyecThor_Setup.exe
echo ========================================================
