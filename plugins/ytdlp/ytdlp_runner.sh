#!/usr/bin/env bash
# Wrapper shell para el plugin yt-dlp en Linux/macOS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if command -v python3 >/dev/null 2>&1; then
    exec python3 "${SCRIPT_DIR}/ytdlp_runner.py" "$@"
elif command -v python >/dev/null 2>&1; then
    exec python "${SCRIPT_DIR}/ytdlp_runner.py" "$@"
else
    echo '{"success": false, "error": "Python 3 no fue encontrado en el sistema para ejecutar el plugin."}'
    exit 1
fi
