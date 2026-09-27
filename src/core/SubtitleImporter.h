#pragma once
#include <string>

namespace ProyecThor::Core {

struct SubtitleFetchResult {
    bool        success = false;
    std::string title;   // Titulo del video (yt-dlp), sugerido como nombre de la cancion.
    std::string lyrics;  // Texto plano ya depurado (sin numeros de cue, timestamps ni tags),
                          // con lineas en blanco insertadas donde hubo una pausa larga entre
                          // cues -- listo para usarse como letra inicial de una cancion.
    std::string error;   // Mensaje legible si success == false.
};

// Descarga subtitulos de <url> via yt-dlp (empaquetado junto a la app, ver
// YtDlpPath) y los convierte a texto plano apto como letra de cancion.
// Mismo criterio de seleccion de pista que SudoMeke/subtitle-grabber
// (https://github.com/SudoMeke/subtitle-grabber): subtitulos manuales/
// oficiales preferidos sobre los auto-generados -- acá resuelto pidiendole
// directo a yt-dlp "--write-subs --write-auto-subs", que ya hace ese mismo
// fallback (manual si existe, si no auto-generado) para cada idioma pedido.
//
// Sincronica y puede tardar varios segundos (red + arranque de yt-dlp) --
// SIEMPRE debe llamarse desde un hilo de fondo, nunca desde el hilo de UI
// (ver RenderUrlImportModal en UIManager.cpp).
SubtitleFetchResult FetchSubtitlesAsLyrics(const std::string& url);

} // namespace ProyecThor::Core
