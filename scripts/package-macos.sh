#!/usr/bin/env bash
set -euo pipefail

# Empaqueta ProyecThorSlint en un bundle autocontenido ProyecThor.app y genera .dmg
# Uso: scripts/package-macos.sh <BUILD_DIR> <ARCH_LABEL> <FULL_VERSION> <OUT_DIR> [SLINT_SDK_LIB_DIR]

BUILD_DIR="${1:?Falta BUILD_DIR}"
ARCH_LABEL="${2:?Falta ARCH_LABEL (arm64|x86_64)}"
FULL_VER="${3:?Falta FULL_VERSION}"
OUT_DIR="${4:?Falta OUT_DIR}"
SLINT_LIB_DIR="${5:-}"

SHORT_VER="$(echo "$FULL_VER" | sed -E 's/^([0-9]+\.[0-9]+\.[0-9]+).*/\1/')"
[ -n "$SHORT_VER" ] || SHORT_VER="1.2.0"

BIN_SRC="${BUILD_DIR}/ProyecThorSlint"
if [ ! -x "$BIN_SRC" ]; then
    echo "ERROR: No se encontro el ejecutable $BIN_SRC" >&2
    exit 1
fi

STAGE_DIR="$(mktemp -d /tmp/proyecthor-dmg.XXXXXX)"
trap 'rm -rf "$STAGE_DIR"' EXIT

APP_DIR="${STAGE_DIR}/ProyecThor.app"
mkdir -p "${APP_DIR}/Contents/MacOS" "${APP_DIR}/Contents/Frameworks" "${APP_DIR}/Contents/Resources" "${OUT_DIR}"

cp "$BIN_SRC" "${APP_DIR}/Contents/MacOS/ProyecThorSlint"
chmod +x "${APP_DIR}/Contents/MacOS/ProyecThorSlint"

# Generar icono .icns desde packaging/proyecthor.png
ICON_SRC="packaging/proyecthor.png"
if [ ! -f "$ICON_SRC" ] && [ -f "src/proyecthor.png" ]; then
    ICON_SRC="src/proyecthor.png"
fi
if [ -f "$ICON_SRC" ] && command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
    ICONSET_DIR="${STAGE_DIR}/ProyecThor.iconset"
    mkdir -p "$ICONSET_DIR"
    for sz in 16 32 128 256 512; do
        sips -z "$sz" "$sz" "$ICON_SRC" --out "${ICONSET_DIR}/icon_${sz}x${sz}.png" >/dev/null
        dbl=$((sz * 2))
        sips -z "$dbl" "$dbl" "$ICON_SRC" --out "${ICONSET_DIR}/icon_${sz}x${sz}@2x.png" >/dev/null
    done
    iconutil -c icns "$ICONSET_DIR" -o "${APP_DIR}/Contents/Resources/ProyecThor.icns"
    rm -rf "$ICONSET_DIR"
fi

# Copiar assets y licencias en Contents/Resources
if [ -d "assets" ]; then
    cp -R assets/. "${APP_DIR}/Contents/Resources/"
fi
[ -f "LICENSE" ] && cp "LICENSE" "${APP_DIR}/Contents/Resources/"
[ -f "THIRD-PARTY-NOTICES.md" ] && cp "THIRD-PARTY-NOTICES.md" "${APP_DIR}/Contents/Resources/"

# Escribir Info.plist válido para macOS
cat <<EOF > "${APP_DIR}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>es</string>
    <key>CFBundleDisplayName</key>
    <string>ProyecThor</string>
    <key>CFBundleExecutable</key>
    <string>ProyecThorSlint</string>
    <key>CFBundleIconFile</key>
    <string>ProyecThor.icns</string>
    <key>CFBundleIdentifier</key>
    <string>io.github.thevixcho.ProyecThor</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>ProyecThor</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${SHORT_VER}</string>
    <key>CFBundleVersion</key>
    <string>${FULL_VER}</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSCameraUsageDescription</key>
    <string>ProyecThor necesita acceso a la cámara para captura de video en vivo.</string>
    <key>NSMicrophoneUsageDescription</key>
    <string>ProyecThor necesita acceso al micrófono para captura de audio.</string>
    <key>NSLocalNetworkUsageDescription</key>
    <string>ProyecThor utiliza la red local para transmisión LAN, Smart TV y control remoto móvil.</string>
</dict>
</plist>
EOF

# Localizar y empaquetar libslint_cpp.dylib si fue enlazada dinámicamente
SLINT_DYLIB=""
if [ -n "$SLINT_LIB_DIR" ] && [ -f "${SLINT_LIB_DIR}/libslint_cpp.dylib" ]; then
    SLINT_DYLIB="${SLINT_LIB_DIR}/libslint_cpp.dylib"
elif [ -f "${BUILD_DIR}/libslint_cpp.dylib" ]; then
    SLINT_DYLIB="${BUILD_DIR}/libslint_cpp.dylib"
else
    SLINT_DYLIB="$(find "$BUILD_DIR" -name 'libslint_cpp.dylib' -print -quit 2>/dev/null || true)"
fi

BUNDLER_SEARCH_ARGS=()
if [ -n "$SLINT_DYLIB" ] && [ -f "$SLINT_DYLIB" ]; then
    cp -f "$SLINT_DYLIB" "${APP_DIR}/Contents/Frameworks/libslint_cpp.dylib"
    chmod u+w "${APP_DIR}/Contents/Frameworks/libslint_cpp.dylib"
    install_name_tool -id "@executable_path/../Frameworks/libslint_cpp.dylib" \
        "${APP_DIR}/Contents/Frameworks/libslint_cpp.dylib" || true
    BUNDLER_SEARCH_ARGS+=(-s "$(dirname "$SLINT_DYLIB")")
    # Reescribir cualquier referencia @rpath o ruta absoluta a libslint_cpp.dylib en el ejecutable
    while IFS= read -r dep; do
        case "$dep" in
            *libslint_cpp*.dylib)
                install_name_tool -change "$dep" "@executable_path/../Frameworks/libslint_cpp.dylib" \
                    "${APP_DIR}/Contents/MacOS/ProyecThorSlint" || true
                ;;
        esac
    done < <(otool -L "${APP_DIR}/Contents/MacOS/ProyecThorSlint" | awk 'NR>1 {print $1}')
fi

# Incluir ffmpeg de Homebrew dentro del bundle para ondas FFT y miniaturas
FFMPEG_ARGS=()
if command -v ffmpeg >/dev/null 2>&1; then
    cp "$(command -v ffmpeg)" "${APP_DIR}/Contents/MacOS/ffmpeg"
    chmod +x "${APP_DIR}/Contents/MacOS/ffmpeg"
    FFMPEG_ARGS+=(-x "${APP_DIR}/Contents/MacOS/ffmpeg")
fi

# Empaquetar libmpv y todas las dependencias dylib de Homebrew en Contents/Frameworks
if command -v dylibbundler >/dev/null 2>&1; then
    dylibbundler -od -b \
        -x "${APP_DIR}/Contents/MacOS/ProyecThorSlint" \
        "${FFMPEG_ARGS[@]}" \
        "${BUNDLER_SEARCH_ARGS[@]}" \
        -d "${APP_DIR}/Contents/Frameworks" \
        -p "@executable_path/../Frameworks/"
else
    echo "ADVERTENCIA: dylibbundler no encontrado; las librerías de Homebrew no serán empaquetadas." >&2
fi

# Verificar que no queden rutas absolutas a Homebrew en el binario principal y que Metal.framework esté enlazado
echo "Verificando dependencias Mach-O de ProyecThorSlint:"
otool -L "${APP_DIR}/Contents/MacOS/ProyecThorSlint"
if otool -L "${APP_DIR}/Contents/MacOS/ProyecThorSlint" | grep -E '/opt/homebrew/|/usr/local/'; then
    echo "ERROR: Quedan dependencias sin empaquetar en ProyecThorSlint" >&2
    exit 1
fi
if [ -f "${APP_DIR}/Contents/Frameworks/libslint_cpp.dylib" ]; then
    echo "Verificando enlace nativo con Metal.framework en libslint_cpp.dylib:"
    otool -L "${APP_DIR}/Contents/Frameworks/libslint_cpp.dylib" | grep -E "Metal\.framework|QuartzCore\.framework"
fi

# Firma ad-hoc obligatoria en Apple Silicon tras reescribir dylibs con install_name_tool
codesign --force --deep --sign - "${APP_DIR}"

# Crear enlace a /Applications y generar imagen .dmg comprimida
ln -s /Applications "${STAGE_DIR}/Applications"
OUT_VERSIONED="${OUT_DIR}/ProyecThor-${FULL_VER}-macos-${ARCH_LABEL}.dmg"
OUT_GENERIC="${OUT_DIR}/ProyecThor-macos-${ARCH_LABEL}.dmg"
rm -f "$OUT_VERSIONED" "$OUT_GENERIC"

hdiutil create -volname "ProyecThor" \
    -srcfolder "$STAGE_DIR" \
    -ov -format UDZO \
    "$OUT_VERSIONED"

cp -f "$OUT_VERSIONED" "$OUT_GENERIC"
ls -lh "$OUT_DIR"
echo "[OK] Paquete macOS (${ARCH_LABEL}) generado: $OUT_VERSIONED"
