#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VIEWER="${HOME}/.cargo/bin/slint-viewer"

if ! command -v slint-viewer >/dev/null 2>&1; then
    if [[ -x "${VIEWER}" ]]; then
        export PATH="${HOME}/.cargo/bin:${PATH}"
    else
        echo "slint-viewer no encontrado. Instalando slint-viewer 1.19.0..."
        cargo install slint-viewer --version 1.19.0 --locked
        export PATH="${HOME}/.cargo/bin:${PATH}"
    fi
fi

echo "Iniciando Hot-Reload de UI Slint..."
echo "Edita cualquier archivo en src/ui_slint/ui/ y guarda (Ctrl+S) para verlo al instante."
exec slint-viewer --auto-reload "${ROOT_DIR}/src/ui_slint/ui/preview_dev.slint"
