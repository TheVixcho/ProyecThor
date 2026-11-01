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

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] No se encontro vcvars64.bat en Visual Studio.
    exit /b 1
)

call "%VCVARS%" >nul

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
    "    $content = \"#pragma once`r`n`r`n#define PROYECTHOR_VERSION_MAJOR 1`r`n#define PROYECTHOR_VERSION_MINOR 1`r`n#define PROYECTHOR_VERSION_PATCH 3`r`n#define PROYECTHOR_VERSION_STRING `\"1.1.3`\"`r`n#define PROYECTHOR_BUILD_NUMBER $num`r`n\"; " ^
    "    Set-Content -Path $verPath -Value $content -NoNewline; " ^
    "}; " ^
    "Write-Host ('[BUILD] Compilando ProyecThor v1.1.3 - Build #' + $num) -ForegroundColor Cyan"

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
if exist "src\vendor\sdk\vlc\libvlc.dll" (
    if not exist "build-win\libvlc.dll" copy /Y "src\vendor\sdk\vlc\libvlc.dll" "build-win\libvlc.dll" >nul
    if not exist "build-win\libvlccore.dll" copy /Y "src\vendor\sdk\vlc\libvlccore.dll" "build-win\libvlccore.dll" >nul
) else if exist "C:\Program Files\VideoLAN\VLC\libvlc.dll" (
    if not exist "build-win\libvlc.dll" copy /Y "C:\Program Files\VideoLAN\VLC\libvlc.dll" "build-win\libvlc.dll" >nul
    if not exist "build-win\libvlccore.dll" copy /Y "C:\Program Files\VideoLAN\VLC\libvlccore.dll" "build-win\libvlccore.dll" >nul
)
if not exist "build-win\plugins\access" (
    if exist "C:\Program Files\VideoLAN\VLC\plugins" (
        xcopy /E /I /Y /Q "C:\Program Files\VideoLAN\VLC\plugins" "build-win\plugins" >nul
    )
)

REM Empaquetado opcional con NSIS si se invoca con "package"
if /I "%~1"=="package" (
    call "%~dp0package-nsi.bat"
) else if /I "%~2"=="package" (
    call "%~dp0package-nsi.bat"
)
