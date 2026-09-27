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
            --socket=fallback-x11 \
            --socket=wayland \
            --device=dri \
            --socket=pulseaudio \
            --share=network \
            --filesystem=xdg-documents \
            --filesystem=xdg-videos \
            --filesystem=xdg-pictures \
            --filesystem=xdg-music \
            --filesystem=xdg-download \
            --talk-name=org.freedesktop.portal.FileChooser \
            --talk-name=org.freedesktop.portal.OpenURI
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
    *)
        echo "Uso: $0 {linux|windows|linux-run|windows-run|flatpak|flatpak-run}"
        exit 1
        ;;
esac