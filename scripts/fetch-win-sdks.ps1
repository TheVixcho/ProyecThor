$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$RootDir = Split-Path -Parent $PSScriptRoot
$SlintVersion = "1.19.0"

# 1. libmpv-2.dll (Windows x64)
$MpvDll = Join-Path $RootDir "src\vendor\sdk\mpv\libmpv-2.dll"
if (-not (Test-Path $MpvDll)) {
    Write-Host "[SDK] Descargando libmpv-2.dll (Windows x64)..." -ForegroundColor Yellow
    $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest" -Headers @{ "User-Agent" = "ProyecThor-Build" }
    $asset = $rel.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d+.*\.7z$' } | Select-Object -First 1
    if (-not $asset) {
        $asset = $rel.assets | Where-Object { $_.name -match '^mpv-dev-x86_64.*\.7z$' } | Select-Object -First 1
    }
    if (-not $asset) { throw "No se encontro mpv-dev-x86_64 en shinchiro/mpv-winbuild-cmake" }

    $archive = Join-Path $env:TEMP "proyecthor-mpv-dev.7z"
    $tmpDir  = Join-Path $env:TEMP "proyecthor-mpv-extract"
    Remove-Item -Recurse -Force $tmpDir, $archive -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

    & curl.exe -fSL --retry 3 -o $archive $asset.browser_download_url
    if ($LASTEXITCODE -ne 0) { throw "curl.exe fallo descargando libmpv ($LASTEXITCODE)" }

    & cmake -E chdir $tmpDir tar xf $archive
    if ($LASTEXITCODE -ne 0) { throw "cmake tar fallo extrayendo $archive" }

    $dll = Get-ChildItem -Path $tmpDir -Recurse -Filter "libmpv-2.dll" | Select-Object -First 1
    if (-not $dll) { throw "No se encontro libmpv-2.dll dentro del paquete descargado" }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $MpvDll) | Out-Null
    Copy-Item -Force $dll.FullName $MpvDll
    Remove-Item -Recurse -Force $tmpDir, $archive -ErrorAction SilentlyContinue
    Write-Host "[SDK] libmpv-2.dll listo en $MpvDll" -ForegroundColor Green
}

# 2. Slint C++ SDK (Windows x64 MSVC)
$SlintDir = Join-Path $RootDir "src\vendor\sdk\slint-win"
$SlintCfg = Join-Path $SlintDir "lib\cmake\Slint\SlintConfig.cmake"
if (-not (Test-Path $SlintCfg)) {
    Write-Host "[SDK] Descargando Slint C++ SDK $SlintVersion (MSVC x64)..." -ForegroundColor Yellow
    $url = "https://github.com/slint-ui/slint/releases/download/v$SlintVersion/Slint-cpp-$SlintVersion-win64-MSVC-AMD64.exe"
    $installer = Join-Path $env:TEMP "Slint-cpp-$SlintVersion-win64.exe"
    & curl.exe -fSL --retry 3 -o $installer $url
    if ($LASTEXITCODE -ne 0) { throw "curl.exe fallo descargando Slint SDK ($LASTEXITCODE)" }

    $sevenZip = Get-Command 7z -ErrorAction SilentlyContinue
    if ($sevenZip) {
        $extractDir = Join-Path $env:TEMP "proyecthor-slint-extract"
        Remove-Item -Recurse -Force $extractDir -ErrorAction SilentlyContinue
        & 7z x $installer "-o$extractDir" -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "7z fallo extrayendo Slint SDK ($LASTEXITCODE)" }
        $cfg = Get-ChildItem -Path $extractDir -Recurse -Filter "SlintConfig.cmake" | Select-Object -First 1
        if (-not $cfg) { throw "No se encontro SlintConfig.cmake dentro de $extractDir" }
        $sdkRoot = Split-Path (Split-Path (Split-Path $cfg.DirectoryName -Parent) -Parent) -Parent
        New-Item -ItemType Directory -Force -Path $SlintDir | Out-Null
        Copy-Item -Recurse -Force "$sdkRoot\*" $SlintDir
        Remove-Item -Recurse -Force $extractDir -ErrorAction SilentlyContinue
    } else {
        New-Item -ItemType Directory -Force -Path $SlintDir | Out-Null
        $proc = Start-Process -FilePath $installer -ArgumentList "/S", "/D=$SlintDir" -Wait -PassThru
        if ($proc.ExitCode -ne 0) { throw "Instalador de Slint SDK fallo con codigo $($proc.ExitCode)" }
    }
    Remove-Item -Force $installer -ErrorAction SilentlyContinue
    if (-not (Test-Path $SlintCfg)) { throw "No se encontro SlintConfig.cmake en $SlintDir tras la instalacion" }
    Write-Host "[SDK] Slint C++ SDK listo en $SlintDir" -ForegroundColor Green
}
