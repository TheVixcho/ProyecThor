#pragma once
#include "LibraryContext.h"
#include <string>
#include <vector>

namespace ProyecThor::Library {

void RenderSideList(LibraryContext& ctx);

// Divide la letra de <filename> en diapositivas: primero por estrofa (linea
// en blanco = limite, igual que siempre), y dentro de cada estrofa agrupando
// las lineas fisicas de a GetSongMeta(filename).linesPerSlide (si esta
// configurado) — ver LibrarySongMeta.h. Sin config guardada, una estrofa
// completa sigue siendo una sola diapositiva (comportamiento legacy).
std::vector<std::string> LoadSongVerses(const std::string& filename);

// Nucleo del agrupado de LoadSongVerses, factorizado para que SongEditView
// pueda recalcular el mismo resultado sobre el buffer EN MEMORIA (mientras
// el usuario todavia esta escribiendo/pegando, antes de guardar en disco) y
// asi el panel de preview del editor sea fiel a lo que se va a guardar.
// linesPerSlide<=0 = centinela legacy (una estrofa completa = una diapositiva).
std::vector<std::string> GroupLyricsIntoSlides(const std::string& normalizedContent, int linesPerSlide);

// Duracion calculada para una estrofa/diapositiva a partir del tempo:
// 1 compas (4 tiempos) por cada linea fisica de texto -- heuristica simple,
// pensada para ajustarse a mano cuando el fraseo real no es "4 tiempos por
// linea" (ver icono de reloj en el editor unificado, que guarda un override
// en LibrarySongMeta::verseDurationOverrideMs). bpm<=0 devuelve 0 (auto-avance
// desactivado).
int CalcVerseDurationMs(const std::string& stanza, int bpm);

void CreateNewSong(LibraryContext& ctx);

// Variante de CreateNewSong para el menu Archivo > Importar > "Importar
// canción desde portapapeles": crea el archivo con <clipboardText> como
// letra inicial (en vez de vacio) y pide que el editor unificado se abra
// directo. No recibe LibraryContext (a diferencia de CreateNewSong) porque
// se llama desde la barra de menu, que no tiene una instancia a mano —
// hace el mismo select+RequestSongEditorOpen directo contra PresentationCore.
void CreateNewSongFromClipboard(const std::string& clipboardText);

// Variante para el menu Archivo > Importar > "Importar desde URL": mismo
// patron que CreateNewSongFromClipboard, pero <suggestedTitle> (titulo del
// video, via yt-dlp) se usa como base del nombre de archivo en vez de
// "Cancion pegada" -- ver SubtitleImporter::FetchSubtitlesAsLyrics y
// RenderUrlImportModal en UIManager.cpp.
void CreateNewSongFromText(const std::string& suggestedTitle, const std::string& text);

// Sobreescribe el contenido de una cancion YA EXISTENTE (a diferencia de
// CreateNewSongFromText, que siempre crea un archivo nuevo con dedup de
// nombre) -- usado por el Asistente de IA en modo Avanzada (ver AITools.cpp)
// para editar letras con confirmacion previa del operador. Devuelve false
// sin tocar nada si <filename> no existe.
bool SetSongText(const std::string& filename, const std::string& text);

std::string GetSongAuthor(const std::string& filename);
void SetSongAuthor(const std::string& filename, const std::string& author);

// Nombre a mostrar en listas/playlists/buscador: el Titulo guardado desde el
// editor unificado (LibrarySongMeta::title) si existe, si no el nombre de
// archivo sin extension (comportamiento legacy). Para canciones YA
// renombradas a mano (o con un Titulo distinto del archivo) el .txt en si no
// se toca — esta funcion es lo que hace que ese Titulo realmente se "vea" en
// la Biblioteca en vez de quedar solo en el sidecar. Ver
// RenameNewSongToTitleIfApplicable para el unico caso en el que el archivo
// SI se renombra automaticamente.
std::string GetSongDisplayName(const std::string& filename);

// Cancion recien creada por CreateNewSong/CreateNewSongFromClipboard (todavia
// con su nombre generico "Nueva cancion(...).txt" / "Cancion pegada(...).txt")
// a la que el usuario ya le puso <title> en el editor unificado: renombra el
// .txt (y migra todos sus sidecars, ver MigrateSongSidecars) para que el
// nombre en disco coincida, evitando dedup con "(2)", "(3)"... si ya existe
// un archivo con ese nombre. Soluciona que las canciones nuevas se sigan
// guardando para siempre como "Nueva cancion" en el sistema de archivos (ver
// src/notes.txt). No-op (devuelve <filename> sin cambios) si la cancion ya
// tiene un nombre de archivo propio, si <title> esta vacio, o si el
// renombrado en disco falla. Llamar SIEMPRE con el filename devuelto en
// adelante (ver SongEditView::FlushIfDirty).
std::string RenameNewSongToTitleIfApplicable(const std::string& filename, const std::string& title);

// Migra TODOS los sidecars de una cancion (autor, etiquetas, estilo/fondo
// preset, color de estrofa, meta JSON de LibrarySongMeta, y las referencias
// en cada playlist) de <oldFilename> a <newFilename> — llamar SIEMPRE
// despues de un fs::rename exitoso del .txt de la cancion (ver
// LibraryModals::RenderRenameModal), o esos datos quedan huerfanos bajo el
// nombre de archivo viejo.
void MigrateSongSidecars(const std::string& oldFilename, const std::string& newFilename);

std::vector<std::string> GetSongTags(const std::string& filename);
void SetSongTags(const std::string& filename, const std::vector<std::string>& tags);

// ── Preset por cancion (estilo + fondo por defecto) ─────────────────────────
// Reemplaza el viejo "estilo por defecto" a nivel de categoría completa: cada
// cancion puede tener su propio estilo/fondo preferido, asignado desde la
// tarjeta de ajustes (la primera del grid de estrofas en SongView). Si la
// cancion no tiene nada guardado, no se aplica nada (el operador elige a
// mano desde el selector, como siempre).
std::string GetSongStyle(const std::string& filename);
void SetSongStyle(const std::string& filename, const std::string& styleName);

struct SongBackground { std::string path; bool isVideo = false; };
SongBackground GetSongBackground(const std::string& filename);
void SetSongBackground(const std::string& filename, const std::string& path, bool isVideo);
void ClearSongBackground(const std::string& filename);

// ── Color de etiqueta por estrofa ────────────────────────────────────────────
// Tag visual libre (no un "tipo" automático como Verso/Coro de ProPresenter,
// que requeriria parsear estructura que este parser de texto plano no tiene):
// el operador le pone color a mano a cada tarjeta para agrupar visualmente.
// 0 = sin color asignado (se dibuja neutro).
unsigned int GetStanzaColor(const std::string& filename, int stanzaIndex);
void SetStanzaColor(const std::string& filename, int stanzaIndex, unsigned int colorU32);

void ApplySongSelection(LibraryContext& ctx, const std::string& filename);

} // namespace ProyecThor::Library