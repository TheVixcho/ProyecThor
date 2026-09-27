#pragma once
#include <string>

namespace ProyecThor::Core {

// Ruta al ffmpeg empaquetado junto al ejecutable en Windows (ver
// extrabuild/ffmpeg.exe), resuelta via GetModuleFileName -- no depende de
// que el directorio de trabajo actual sea el de la app. Si no se encuentra
// empaquetado, cae a "ffmpeg" (PATH del sistema). En el resto de
// plataformas siempre devuelve "ffmpeg" (se espera un ffmpeg del sistema).
// Compartido entre StreamEncoder (Streaming/RTMP) y MediaConverter
// (Biblioteca > Render), para no duplicar esta logica en los dos.
std::string FfmpegPath();

// Corre "<ffmpegPath> -version" y confirma que responde. Usado antes de
// arrancar cualquier proceso real de ffmpeg, para poder mostrar un error
// claro en vez de que el pipe falle en silencio.
bool FfmpegAvailable(const std::string& ffmpegPath);

} // namespace ProyecThor::Core
