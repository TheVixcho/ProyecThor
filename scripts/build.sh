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
    *)
        echo "Uso: $0 {linux|windows|linux-run|windows-run}"
        exit 1
        ;;
esac