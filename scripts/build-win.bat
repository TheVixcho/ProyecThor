@echo off
setlocal
cd /d "%~dp0.."

REM Soporte de Hot-Reload / Recarga en Caliente:
REM Si ProyecThor.exe está abierto, en Windows no se puede sobreescribir pero SÍ renombrar.
REM Renombramos el binario en uso a .old.exe para permitir que el linker genere el nuevo sin error LNK1104.
set "WAS_RUNNING=0"
tasklist /FI "IMAGENAME eq ProyecThor.exe" 2>NUL | find /I /N "ProyecThor.exe">NUL
if "%ERRORLEVEL%"=="0" (
    set "WAS_RUNNING=1"
    if exist "build-win\ProyecThor.old.exe" del /F /Q "build-win\ProyecThor.old.exe" >nul 2>&1
    if exist "build-win\ProyecThor.exe" ren "build-win\ProyecThor.exe" "ProyecThor.old.exe" >nul 2>&1
    if exist "build-win\ProyecThor.old.pdb" del /F /Q "build-win\ProyecThor.old.pdb" >nul 2>&1
    if exist "build-win\ProyecThor.pdb" ren "build-win\ProyecThor.pdb" "ProyecThor.old.pdb" >nul 2>&1
)

set "VCVARS="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
    )
)
if not defined VCVARS (
    for %%V in (18 2022 17) do (
        for %%E in (Community Professional Enterprise BuildTools Preview) do (
            if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
            )
        )
    )
)
if not defined VCVARS (
    echo [ERROR] No se encontro vcvars64.bat en Visual Studio. Instala Desktop development with C++.
    exit /b 1
)

call "%VCVARS%" >nul

REM Descargar SDKs binarios faltantes (libmpv-2.dll / Slint C++ SDK) si no existen
if not exist "src\vendor\sdk\mpv\libmpv-2.dll" goto :fetch_sdks
if not exist "src\vendor\sdk\slint-win\lib\cmake\Slint\SlintConfig.cmake" goto :fetch_sdks
goto :sdks_ready
:fetch_sdks
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fetch-win-sdks.ps1"
if errorlevel 1 exit /b %errorlevel%
:sdks_ready

REM Actualizar contador de compilacion (Build Number) e inyectar en Version.h
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$bFile = 'build_count.txt'; " ^
    "$num = 1; " ^
    "if (Test-Path $bFile) { " ^
    "    $raw = (Get-Content $bFile -Raw).Trim(); " ^
    "    if ($raw -match '^\d+$') { $num = [int]$raw } " ^
    "}; " ^
    "$num++; " ^
    "Set-Content -Path $bFile -Value $num -NoNewline; " ^
    "$verDir = 'build-win/generated'; " ^
    "if (Test-Path $verDir) { " ^
    "    $verPath = Join-Path $verDir 'Version.h'; " ^
    "    $content = \"#pragma once`r`n`r`n#define PROYECTHOR_VERSION_MAJOR 1`r`n#define PROYECTHOR_VERSION_MINOR 2`r`n#define PROYECTHOR_VERSION_PATCH 0`r`n#define PROYECTHOR_VERSION_STRING `\"1.2.0`\"`r`n#define PROYECTHOR_BUILD_NUMBER $num`r`n\"; " ^
    "    Set-Content -Path $verPath -Value $content -NoNewline; " ^
    "}; " ^
    "Write-Host ('[BUILD] Compilando ProyecThor v1.2.0 - Build #' + $num) -ForegroundColor Cyan"

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Debug"

if not exist "build-win\build.ninja" (
    cmake -B build-win -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    if errorlevel 1 exit /b %errorlevel%
)

cmake --build build-win --parallel
if errorlevel 1 exit /b %errorlevel%

REM Si el programa estaba abierto, notificar recarga en caliente automática
if "%WAS_RUNNING%"=="1" (
    echo [HOT-RELOAD] Notificando recarga en caliente a ProyecThor...
    powershell -NoProfile -Command "try { $null = Invoke-RestMethod -Uri 'http://127.0.0.1:51973/hot-reload' -Method POST -TimeoutSec 1 } catch {}" >nul 2>&1
)

REM Asegurar DLLs requeridas en build-win
if exist "src\vendor\sdk\pdfium\bin\pdfium.dll" (
    if not exist "build-win\pdfium.dll" copy /Y "src\vendor\sdk\pdfium\bin\pdfium.dll" "build-win\pdfium.dll" >nul
)
if exist "src\vendor\sdk\mpv\libmpv-2.dll" (
    if not exist "build-win\libmpv-2.dll" copy /Y "src\vendor\sdk\mpv\libmpv-2.dll" "build-win\libmpv-2.dll" >nul
)

REM Empaquetado opcional con NSIS si se invoca con "package"
if /I "%~1"=="package" (
    call "%~dp0package-nsi.bat"
) else if /I "%~2"=="package" (
    call "%~dp0package-nsi.bat"
)
