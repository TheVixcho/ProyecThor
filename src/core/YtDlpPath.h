#pragma once
#include <string>

namespace ProyecThor::Core {

// Ruta al yt-dlp empaquetado junto al ejecutable en Windows (ver
// extrabuild/yt-dlp.exe), resuelta via GetModuleFileName -- mismo patron que
// FfmpegPath (ver FfmpegPath.h/.cpp), no depende de que el directorio de
// trabajo actual sea el de la app. Si no se encuentra empaquetado, cae a
// "yt-dlp" (PATH del sistema). En el resto de plataformas siempre devuelve
// "yt-dlp" (se espera un yt-dlp del sistema).
std::string YtDlpPath();

// Directorio donde vive el ejecutable de la app (mismo que contiene
// ffmpeg.exe/yt-dlp.exe empaquetados en Windows). Usado para pasarle
// --ffmpeg-location a yt-dlp explicitamente: sin esto, yt-dlp busca ffmpeg
// en PATH y no encuentra el que ya viene empaquetado al lado suyo, y falla
// silenciosamente la conversion de subtitulos a .srt en equipos sin ffmpeg
// instalado por separado. Vacio si no se pudo resolver (yt-dlp busca solo).
std::string BundledToolsDir();

} // namespace ProyecThor::Core
