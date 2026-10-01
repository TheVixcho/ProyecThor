#!/usr/bin/env bash
set -e
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"
PLATFORM="$1"

export PATH="/home/vixcho/.local/bin:/home/vixcho/.local/mingw-w64/bin:$PATH"

case "$PLATFORM" in
    windows)
        if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
            echo "================================================================="
            echo "[ERROR] x86_64-w64-mingw32-gcc no está instalado en el sistema."
            echo "Para compilar ProyecThor para Windows en CachyOS/Arch, ejecuta:"
            echo "  sudo pacman -S mingw-w64-gcc"
            exit 1
        fi
        cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/toolchain-mingw.cmake"
        cmake --build build-win
        ;;
    linux)
        cmake -B build-linux -G Ninja
        cmake --build build-linux
        ;;
    windows-run)
        if [ ! -f "build-win/ProyecThor.exe" ]; then
            if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
                cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/toolchain-mingw.cmake"
                cmake --build build-win
            else
                echo "Error: build-win/ProyecThor.exe no existe y mingw-w64-gcc no está instalado para compilarlo."
                exit 1
            fi
        fi
        cd build-win
        wine ProyecThor.exe
        ;;
    linux-run)
        if [ ! -f "build-linux/ProyecThor" ]; then
            cmake -B build-linux -G Ninja
            cmake --build build-linux
        fi
        cd build-linux
        ./ProyecThor
        ;;
    flatpak)
        cmake -B build-linux -G Ninja
        cmake --build build-linux
        rm -rf build-flatpak/staging build-flatpak/app build-flatpak/repo
        mkdir -p build-flatpak/staging
        DESTDIR="$PROJECT_ROOT/build-flatpak/staging" cmake --install build-linux --prefix=/app
        
        FLATPAK_BIN="flatpak"
        if ! command -v flatpak >/dev/null 2>&1 && command -v flatpak-spawn >/dev/null 2>&1; then
            FLATPAK_BIN="flatpak-spawn --host flatpak"
        fi

        $FLATPAK_BIN build-init "$PROJECT_ROOT/build-flatpak/app" io.github.thevixcho.ProyecThor org.freedesktop.Sdk org.freedesktop.Platform 25.08
        cp -r "$PROJECT_ROOT/build-flatpak/staging/app"/* "$PROJECT_ROOT/build-flatpak/app/files/"
        if [ -d "$PROJECT_ROOT/build-flatpak/app/files/lib64" ] && [ ! -e "$PROJECT_ROOT/build-flatpak/app/files/lib" ]; then
            ln -s lib64 "$PROJECT_ROOT/build-flatpak/app/files/lib"
        fi
        $FLATPAK_BIN build-finish "$PROJECT_ROOT/build-flatpak/app" \
            --command=proyecthor \
            --share=ipc \
            --socket=x11 \
            --socket=wayland \
            --device=dri \
            --socket=pulseaudio \
            --share=network
        $FLATPAK_BIN build-export "$PROJECT_ROOT/build-flatpak/repo" "$PROJECT_ROOT/build-flatpak/app"
        $FLATPAK_BIN build-bundle "$PROJECT_ROOT/build-flatpak/repo" "$PROJECT_ROOT/ProyecThor.flatpak" io.github.thevixcho.ProyecThor
        echo "=========================================================="
        echo "[OK] Paquete generado con éxito: ProyecThor.flatpak"
        echo "Para instalarlo: flatpak install --user -y --bundle ProyecThor.flatpak"
        echo "Para ejecutarlo: flatpak run io.github.thevixcho.ProyecThor"
        echo "=========================================================="
        ;;
    flatpak-run)
        if [ ! -f "ProyecThor.flatpak" ]; then
            "$0" flatpak
        fi
        FLATPAK_BIN="flatpak"
        if ! command -v flatpak >/dev/null 2>&1 && command -v flatpak-spawn >/dev/null 2>&1; then
            FLATPAK_BIN="flatpak-spawn --host flatpak"
        fi
        $FLATPAK_BIN install --user -y --bundle "$PROJECT_ROOT/ProyecThor.flatpak"
        $FLATPAK_BIN run io.github.thevixcho.ProyecThor
        ;;
    appimage)
        cmake -B build-linux -G Ninja
        cmake --build build-linux
        rm -rf build-appimage
        mkdir -p build-appimage/AppDir
        DESTDIR="$PROJECT_ROOT/build-appimage/AppDir" cmake --install build-linux --prefix=/usr
        cp "$PROJECT_ROOT/packaging/io.github.thevixcho.ProyecThor.desktop" "$PROJECT_ROOT/build-appimage/AppDir/"
        cp "$PROJECT_ROOT/packaging/proyecthor.png" "$PROJECT_ROOT/build-appimage/AppDir/io.github.thevixcho.ProyecThor.png"
        cp "$PROJECT_ROOT/packaging/proyecthor.png" "$PROJECT_ROOT/build-appimage/AppDir/.DirIcon"

        cat << 'EOF' > "$PROJECT_ROOT/build-appimage/AppDir/AppRun"
#!/bin/sh
set -e
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib/proyecthor:${HERE}/usr/lib64/proyecthor:${HERE}/usr/lib:${HERE}/usr/lib64:${LD_LIBRARY_PATH}"

if [ -d "${HERE}/usr/lib/proyecthor/plugins" ]; then
    export VLC_PLUGIN_PATH="${HERE}/usr/lib/proyecthor/plugins"
elif [ -d "${HERE}/usr/lib64/proyecthor/plugins" ]; then
    export VLC_PLUGIN_PATH="${HERE}/usr/lib64/proyecthor/plugins"
fi

if [ -d "${HERE}/usr/lib/proyecthor" ]; then
    cd "${HERE}/usr/lib/proyecthor"
elif [ -d "${HERE}/usr/lib64/proyecthor" ]; then
    cd "${HERE}/usr/lib64/proyecthor"
fi

exec ./ProyecThor "$@"
EOF
        chmod +x "$PROJECT_ROOT/build-appimage/AppDir/AppRun"

        if [ -d "$PROJECT_ROOT/build-appimage/AppDir/usr/lib64/proyecthor" ] && [ ! -e "$PROJECT_ROOT/build-appimage/AppDir/usr/lib/proyecthor" ]; then
            mkdir -p "$PROJECT_ROOT/build-appimage/AppDir/usr/lib"
            ln -s ../lib64/proyecthor "$PROJECT_ROOT/build-appimage/AppDir/usr/lib/proyecthor"
        fi

        APPIMAGETOOL="/home/vixcho/.local/bin/appimagetool"
        if ! command -v appimagetool >/dev/null 2>&1 && [ ! -f "$APPIMAGETOOL" ]; then
            mkdir -p /home/vixcho/.local/bin
            curl -L -o "$APPIMAGETOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
            chmod +x "$APPIMAGETOOL"
        fi

        APPIMAGETOOL_CMD="appimagetool"
        if [ -f "$APPIMAGETOOL" ]; then
            APPIMAGETOOL_CMD="$APPIMAGETOOL"
        fi

        if command -v flatpak-spawn >/dev/null 2>&1; then
            flatpak-spawn --host env ARCH=x86_64 "$APPIMAGETOOL_CMD" -n -u "gh-releases-zsync|TheVixcho|ProyecThor|latest|ProyecThor-*x86_64.AppImage.zsync" "$PROJECT_ROOT/build-appimage/AppDir" "$PROJECT_ROOT/ProyecThor-x86_64.AppImage"
        else
            env ARCH=x86_64 "$APPIMAGETOOL_CMD" -n -u "gh-releases-zsync|TheVixcho|ProyecThor|latest|ProyecThor-*x86_64.AppImage.zsync" "$PROJECT_ROOT/build-appimage/AppDir" "$PROJECT_ROOT/ProyecThor-x86_64.AppImage"
        fi

        echo "=========================================================="
        echo "[OK] AppImage generado con éxito: ProyecThor-x86_64.AppImage"
        echo "Para ejecutarlo: ./ProyecThor-x86_64.AppImage"
        echo "=========================================================="
        ;;
    appimage-run)
        if [ ! -f "ProyecThor-x86_64.AppImage" ]; then
            "$0" appimage
        fi
        if command -v flatpak-spawn >/dev/null 2>&1; then
            flatpak-spawn --host "$PROJECT_ROOT/ProyecThor-x86_64.AppImage"
        else
            "$PROJECT_ROOT/ProyecThor-x86_64.AppImage"
        fi
        ;;
    *)
        echo "Uso: $0 {linux|windows|linux-run|windows-run|flatpak|flatpak-run|appimage|appimage-run}"
        exit 1
        ;;
esac