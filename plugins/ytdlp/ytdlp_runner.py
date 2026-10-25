#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
yt-dlp Runner Plugin para ProyecThor
Maneja la resolución de URLs directas, descarga y parseo de subtítulos,
y actualización de yt-dlp de forma desacoplada del código C++.
"""

import sys
import os
import re
import json
import shutil
import tempfile
import argparse
import subprocess
from pathlib import Path


def find_ytdlp_binary():
    """Localiza el binario de yt-dlp en el sistema o en la carpeta del plugin."""
    # 1. Variable de entorno explícita
    env_path = os.environ.get("YTDLP_PATH")
    if env_path and os.path.isfile(env_path) and os.access(env_path, os.X_OK):
        return [env_path]

    script_dir = Path(__file__).resolve().parent

    # 2. En la misma carpeta del plugin o binarios
    if sys.platform == "win32":
        local_candidates = [
            script_dir / "yt-dlp.exe",
            script_dir / "yt-dlp",
            script_dir.parent.parent / "extrabuild" / "yt-dlp.exe",
            script_dir.parent.parent / "bin" / "yt-dlp.exe",
        ]
    else:
        local_candidates = [
            script_dir / "yt-dlp",
            script_dir.parent.parent / "bin" / "yt-dlp",
        ]

    for c in local_candidates:
        if c.is_file():
            if sys.platform != "win32":
                try:
                    c.chmod(c.stat().st_mode | 0o111)
                except Exception:
                    pass
            return [str(c)]

    # 3. PATH del sistema
    which_name = "yt-dlp.exe" if sys.platform == "win32" else "yt-dlp"
    which_path = shutil.which(which_name) or shutil.which("yt-dlp")
    if which_path:
        return [which_path]

    # 4. Módulo de Python
    try:
        res = subprocess.run([sys.executable, "-m", "yt_dlp", "--version"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5)
        if res.returncode == 0:
            return [sys.executable, "-m", "yt_dlp"]
    except Exception:
        pass

    return None


def get_ytdlp_version(cmd_prefix):
    """Obtiene la versión actual de yt-dlp."""
    try:
        res = subprocess.run(cmd_prefix + ["--version"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8)
        if res.returncode == 0:
            return res.stdout.strip()
    except Exception:
        pass
    return "Desconocida"


def action_status(cmd_prefix):
    """Devuelve el estado de la instalación de yt-dlp."""
    if not cmd_prefix:
        return {
            "success": False,
            "installed": False,
            "error": "yt-dlp no encontrado en el sistema ni en la carpeta del plugin. "
                     "Coloca el ejecutable yt-dlp en plugins/ytdlp/ o instálalo con 'pip install yt-dlp'."
        }

    version = get_ytdlp_version(cmd_prefix)
    return {
        "success": True,
        "installed": True,
        "path": " ".join(cmd_prefix),
        "version": version
    }


def action_resolve_stream(cmd_prefix, url):
    """Resuelve la URL directa del video para reproducción en VLC."""
    if not cmd_prefix:
        return {
            "success": False,
            "error": "yt-dlp no está instalado o disponible."
        }

    if not url or not (url.startswith("http://") or url.startswith("https://")):
        return {
            "success": False,
            "error": "URL inválida: debe comenzar con http:// o https://"
        }

    cmd = cmd_prefix + [
        "-f", "best[ext=mp4]/best",
        "-g",
        "--no-playlist",
        "--no-warnings",
        url
    ]

    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=12)
        if proc.returncode != 0:
            err = proc.stderr.strip() or "Fallo al resolver la transmisión de video."
            return {"success": False, "error": err}

        stream_url = proc.stdout.strip().splitlines()
        if not stream_url:
            return {"success": False, "error": "No se obtuvo URL de transmisión del video."}

        return {
            "success": True,
            "url": stream_url[0].strip()
        }
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "Tiempo de espera agotado al resolver la URL (timeout)."}
    except Exception as e:
        return {"success": False, "error": str(e)}


def strip_html_tags(text):
    """Elimina etiquetas HTML como <c>, <i>, </b>."""
    return re.sub(r'<[^>]+>', '', text)


def parse_timestamp_ms(ts_str):
    """Convierte timestamp '00:00:01,500' a milisegundos."""
    m = re.match(r'(\d+):(\d+):(\d+)[,\.](\d+)', ts_str.strip())
    if not m:
        return -1
    h, mn, s, ms = map(int, m.groups())
    return (h * 3600 + mn * 60 + s) * 1000 + ms


def srt_to_lyrics(srt_content):
    """Convierte un archivo SRT a estrofas limpias para letras de canciones."""
    lines = srt_content.splitlines()
    result_blocks = []
    current_lines = []
    last_text = ""
    last_end_ms = -1
    cur_start_ms = -1
    cur_end_ms = -1

    state = "index"

    def flush_cue():
        nonlocal last_text, last_end_ms, current_lines
        if not current_lines:
            return
        combined = " ".join(current_lines).strip()
        cleaned = strip_html_tags(combined).strip()
        current_lines = []
        if not cleaned:
            return

        # Deduplicar líneas idénticas consecutivas (típicas de subtítulos autogenerados)
        if cleaned.lower() == last_text.lower():
            last_end_ms = cur_end_ms
            return

        # Si hay una pausa mayor a 1.8 segundos, insertar separación de estrofa
        if last_end_ms >= 0 and cur_start_ms >= 0 and (cur_start_ms - last_end_ms) > 1800 and result_blocks:
            result_blocks.append("")

        result_blocks.append(cleaned)
        last_text = cleaned
        last_end_ms = cur_end_ms

    for line in lines:
        line = line.strip()
        if state == "index":
            if not line:
                continue
            state = "timestamp"
            continue

        if state == "timestamp":
            if "-->" in line:
                parts = line.split("-->")
                cur_start_ms = parse_timestamp_ms(parts[0])
                cur_end_ms = parse_timestamp_ms(parts[1])
                state = "text"
            else:
                state = "index"
            continue

        if state == "text":
            if not line:
                flush_cue()
                state = "index"
            else:
                current_lines.append(line)

    flush_cue()
    return "\n".join(result_blocks).strip()


def action_get_lyrics(cmd_prefix, url):
    """Descarga subtítulos y los convierte a letra de canción con estrofas."""
    if not cmd_prefix:
        return {
            "success": False,
            "error": "yt-dlp no está instalado o disponible."
        }

    if not url or not (url.startswith("http://") or url.startswith("https://")):
        return {
            "success": False,
            "error": "URL inválida: debe comenzar con http:// o https://"
        }

    # 1. Obtener título del video
    title = "Canción importada"
    try:
        title_cmd = cmd_prefix + [
            "--skip-download",
            "--no-playlist",
            "--print", "%(title)s",
            url
        ]
        res = subprocess.run(title_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=12)
        if res.returncode == 0 and res.stdout.strip():
            candidate = res.stdout.strip().splitlines()[0]
            if candidate and not candidate.startswith("ERROR"):
                title = candidate
    except Exception:
        pass

    # 2. Descargar subtítulos en carpeta temporal
    with tempfile.TemporaryDirectory(prefix="proyecthor_plugin_subs_") as tmpdir:
        tmppath = Path(tmpdir)
        out_template = str(tmppath / "sub.%(ext)s")

        sub_cmd = cmd_prefix + [
            "--write-subs",
            "--write-auto-subs",
            "--sub-langs", "es,en",
            "--sub-format", "srt",
            "--skip-download",
            "--no-playlist",
            "--restrict-filenames",
            "-o", out_template,
            url
        ]

        try:
            sub_proc = subprocess.run(sub_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=25)
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Tiempo de espera agotado al descargar subtítulos."}
        except Exception as e:
            return {"success": False, "error": f"Error al ejecutar yt-dlp: {e}"}

        # Buscar el archivo .srt generado (preferencia: .es. > .en. > cualquier otro)
        srt_files = list(tmppath.glob("*.srt"))
        if not srt_files:
            return {
                "success": False,
                "error": "No se encontraron subtítulos (manuales ni automáticos) en español o inglés para este video."
            }

        target_srt = None
        for srt in srt_files:
            if ".es." in srt.name:
                target_srt = srt
                break
        if not target_srt:
            for srt in srt_files:
                if ".en." in srt.name:
                    target_srt = srt
                    break
        if not target_srt:
            target_srt = srt_files[0]

        try:
            content = target_srt.read_text(encoding="utf-8", errors="replace")
            lyrics = srt_to_lyrics(content)
            if not lyrics:
                return {
                    "success": False,
                    "error": "El archivo de subtítulos estaba vacío o no contenía texto procesable."
                }

            return {
                "success": True,
                "title": title,
                "lyrics": lyrics
            }
        except Exception as e:
            return {"success": False, "error": f"Error al procesar subtítulos: {e}"}


def action_update(cmd_prefix):
    """Actualiza yt-dlp a la última versión disponible."""
    if not cmd_prefix:
        return {
            "success": False,
            "error": "yt-dlp no está instalado."
        }

    output_lines = []
    # 1. Intentar actualizar con el comando directo
    try:
        up_cmd = cmd_prefix + ["-U"]
        res = subprocess.run(up_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=60)
        output_lines.append(res.stdout)
        if res.stderr:
            output_lines.append(res.stderr)
        if res.returncode == 0:
            return {
                "success": True,
                "output": "\n".join(output_lines).strip()
            }
    except Exception as e:
        output_lines.append(f"Fallo al ejecutar yt-dlp -U: {e}")

    # 2. Si falló o fue instalado por pip, intentar pip upgrade
    try:
        pip_cmd = [sys.executable, "-m", "pip", "install", "--upgrade", "yt-dlp"]
        res = subprocess.run(pip_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=60)
        output_lines.append(res.stdout)
        if res.stderr:
            output_lines.append(res.stderr)
        if res.returncode == 0:
            return {
                "success": True,
                "output": "Actualizado mediante pip:\n" + "\n".join(output_lines).strip()
            }
    except Exception as e:
        output_lines.append(f"Fallo al intentar pip: {e}")

    return {
        "success": False,
        "error": "No se pudo actualizar yt-dlp automáticamente:\n" + "\n".join(output_lines).strip()
    }


def main():
    parser = argparse.ArgumentParser(description="Plugin yt-dlp para ProyecThor")
    parser.add_argument("--action", choices=["status", "resolve", "lyrics", "update"], default="status",
                        help="Acción a ejecutar")
    parser.add_argument("--url", default="", help="URL del video a procesar")

    args = parser.parse_args()
    cmd_prefix = find_ytdlp_binary()

    result = {}
    if args.action == "status":
        result = action_status(cmd_prefix)
    elif args.action == "resolve":
        result = action_resolve_stream(cmd_prefix, args.url)
    elif args.action == "lyrics":
        result = action_get_lyrics(cmd_prefix, args.url)
    elif args.action == "update":
        result = action_update(cmd_prefix)

    # Imprimir el resultado en formato JSON para consumo por ProyecThor
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
