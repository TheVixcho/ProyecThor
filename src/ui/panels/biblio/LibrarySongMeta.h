#pragma once
#include <string>
#include <vector>

namespace ProyecThor::Library {

// =============================================================================
//  LibrarySongMeta — sidecar NUEVO por cancion (assets/songs_meta/<archivo>.json),
//  aditivo: no reemplaza songs_authors.ini/songs_tags.ini/songs_style.ini/
//  songs_background.ini/songs_stanza_colors.ini, que siguen intactos y son la
//  fuente de verdad para lo que ya leian (busqueda, preset de estilo/fondo por
//  cancion, color cosmetico por estrofa). Este archivo solo guarda lo nuevo
//  del rework del editor: campos de metadatos ampliados (Nota/Extra/Derechos
//  de autor) y la config de "lineas por diapositiva". (El estilo/fondo por
//  linea individual que se penso en un primer momento se saco: ya existe un
//  preset de estilo/fondo por CANCION ENTERA — tarjeta "Ajustes" del grid de
//  estrofas en SongView — y tener los dos era redundante.)
// =============================================================================

struct SongMeta {
    int         version      = 1;
    std::string title;
    std::string artistAuthor; // se espeja tambien en songs_authors.ini via SetSongAuthor
    std::string note;
    std::string copyright;
    std::string extra;

    // 0 = sin configurar todavia (centinela "legacy": una estrofa completa
    // sigue siendo una sola diapositiva, igual que el comportamiento de
    // antes de este rework). 1/2/3 = cantidad de lineas fisicas por
    // diapositiva dentro de cada estrofa.
    int linesPerSlide = 0;

    // ── Auto-avance por tempo (ver SongView, barra "Tempo"/Reproducir) ────
    // 0 = sin configurar (auto-avance desactivado). BPM usado para calcular
    // cuanto dura en pantalla cada diapositiva (ver Library::CalcVerseDurationMs).
    int tempoBpm = 0;

    // Override manual de duracion por diapositiva, en milisegundos, mismo
    // indice que LoadSongVerses/ComputePreviewSlides -- ver icono de reloj
    // en el editor unificado (SongEditView). -1 (o indice fuera de rango,
    // vector mas corto que la cantidad real de diapositivas) = sin override,
    // usar el calculo automatico por tempo.
    std::vector<int> verseDurationOverrideMs;
};

SongMeta GetSongMeta(const std::string& filename);
void     SetSongMeta(const std::string& filename, const SongMeta& meta);

// Mueve el sidecar JSON de <oldFilename> a <newFilename> (usado al renombrar
// una cancion, ver LibraryModals::RenderRenameModal) — sin esto, renombrar
// el .txt deja el meta.json huerfano bajo el nombre viejo y la cancion
// "pierde" Nota/Derechos de autor/lineas-por-diapositiva/tempo al renombrarla.
void RenameSongMeta(const std::string& oldFilename, const std::string& newFilename);

} // namespace ProyecThor::Library
