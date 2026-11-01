#include "LibrarySongs.h"
#include "LibrarySongMeta.h"
#include "LibraryModals.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/framework/DesignSystem.h"
#include "ui/views/biblia/BibleNameUtils.h"
#include "ui/views/biblia/BibleTextUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "core/PresentationCore.h"
#include "UIStrings.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include "LibraryTags.h"

#include "LibraryPlaylists.h"
#include "LibraryMultimedia.h"
#include "core/MediaCollections.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <mutex>

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;

static constexpr int kCat_Songs     = 0;
static constexpr int kCat_Videos    = 1;
static constexpr int kCat_Images    = 2;
static constexpr int kCat_Bibles    = 3;
static constexpr int kCat_Documents = 4;
static constexpr int kCat_Audio     = 5;

namespace ProyecThor::Library {

static void SelectLibraryItem(LibraryContext& ctx,
                              const std::vector<std::string>& filteredItems,
                              int n);

// GlassIconButton ahora vive en DesignSystem.h/.cpp (DS::GlassIconButton) para
// que el editor de canciones (SongEditView) tambien pueda usarlo — mismo
// comportamiento exacto, solo se movio de lugar. Este `using` evita tocar
// cada uno de los call-sites de este archivo.
using DS::GlassIconButton;

// =============================================================================
//  ApplyDefaultStyleIfSet
// =============================================================================
static void ApplyDefaultStyleIfSet(LibraryContext& ctx)
{
    if (ctx.currentCategoryInt == kCat_Songs)
    {
        // Preset por cancion (ver tarjeta de ajustes en SongView) tiene
        // prioridad sobre el viejo default de categoria completa; si la
        // cancion no tiene nada guardado, cae al default anterior (por si
        // quedo alguno seteado de antes de este cambio).
        if (ctx.selectedIndex < 0 || ctx.selectedIndex >= (int)ctx.items.size()) return;
        const std::string& filename = ctx.items[ctx.selectedIndex];

        std::string style = GetSongStyle(filename);
        if (style.empty())
            style = Core::PresentationCore::Get().GetCategoryDefaultStyle(Core::ItemType::Song);
        if (!style.empty())
            ctx.applyStyle(style);

        SongBackground bg = GetSongBackground(filename);
        if (!bg.path.empty())
            Core::PresentationCore::Get().SetBackgroundMedia(bg.path, bg.isVideo, false);

        return;
    }

    if (ctx.currentCategoryInt == kCat_Bibles)
    {
        std::string defaultStyle =
            Core::PresentationCore::Get().GetCategoryDefaultStyle(Core::ItemType::Bible);
        if (!defaultStyle.empty())
            ctx.applyStyle(defaultStyle);
    }
}

// =============================================================================
//  LoadSongVerses
//
//  Paso 1 (sin cambios respecto de siempre): separa la letra en parrafos por
//  linea en blanco.
//  Paso 2 (nuevo, rework del editor): si la cancion tiene configurado
//  linesPerSlide (1/2/3, via GetSongMeta/SetSongMeta — LibrarySongMeta.h),
//  cada parrafo se subdivide ademas en grupos de esa cantidad de lineas
//  fisicas, SIN cruzar nunca el limite de parrafo (un parrafo de 5 lineas
//  con linesPerSlide=2 da diapositivas de [2,2,1], nunca se funde con el
//  siguiente). linesPerSlide=0 (centinela: cancion sin sidecar todavia, o
//  con sidecar pero sin este campo configurado) preserva el comportamiento
//  de siempre — un parrafo completo es una sola diapositiva — para no
//  romper canciones existentes al actualizar.
// =============================================================================
std::vector<std::string> GroupLyricsIntoSlides(const std::string& normalizedContent, int linesPerSlide)
{
    std::vector<std::string> verses;
    if (normalizedContent.empty()) return verses;

    std::vector<std::vector<std::string>> paragraphs;
    std::vector<std::string> currentParagraph;
    std::string line;
    std::istringstream stream(normalizedContent);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            if (!currentParagraph.empty()) {
                paragraphs.push_back(currentParagraph);
                currentParagraph.clear();
            }
        } else {
            currentParagraph.push_back(line);
        }
    }
    if (!currentParagraph.empty()) paragraphs.push_back(currentParagraph);

    for (const auto& para : paragraphs) {
        if (linesPerSlide <= 0) {
            std::string joined;
            for (const auto& l : para) joined += l + '\n';
            verses.push_back(joined);
            continue;
        }

        for (size_t i = 0; i < para.size(); i += (size_t)linesPerSlide) {
            size_t end = std::min(para.size(), i + (size_t)linesPerSlide);
            std::string joined;
            for (size_t k = i; k < end; ++k) joined += para[k] + '\n';
            verses.push_back(joined);
        }
    }

    return verses;
}

int CalcVerseDurationMs(const std::string& stanza, int bpm)
{
    if (bpm <= 0) return 0;

    int lines = stanza.empty() ? 0 : 1;
    for (char c : stanza) if (c == '\n') lines++;
    if (lines <= 0) lines = 1;

    constexpr int kBeatsPerLine = 4;
    double beatSec  = 60.0 / (double)bpm;
    double totalSec = (double)lines * (double)kBeatsPerLine * beatSec;
    return (int)std::lround(totalSec * 1000.0);
}

// =============================================================================
//  Detalle y Análisis de Coincidencias de Búsqueda de Canciones
// =============================================================================
enum class SongMatchKind {
    None,
    Title,
    Author,
    Tag,
    Verse
};

struct SongMatchDetail {
    SongMatchKind kind        = SongMatchKind::None;
    int           stanzaIndex = -1;
    int           lineIndex   = -1;
    std::string   snippet;
    std::string   matchParam;
};

static inline std::string NormalizeForSearch(const std::string& s)
{
    return ProyecThor::UI::TextUtils::StripAccents(ProyecThor::UI::TextUtils::ToLowerUTF8(s));
}

std::string GetSongDisplayName(const std::string& filename)
{
    std::string title = GetSongMeta(filename).title;
    return !title.empty() ? title : StripExtension(filename);
}

// =============================================================================
//  Autor de cancion — persistido aparte en songs_authors.ini
//  (con caché en RAM y validación de última modificación para evitar I/O)
// =============================================================================
static std::string SongAuthorsFilePath()
{
    return GetAssetsPath() + "/../songs_authors.ini";
}

static std::mutex s_SongAuthorsMutex;
static std::unordered_map<std::string, std::string> s_SongAuthorsCache;
static fs::file_time_type s_SongAuthorsLastMod{};
static bool s_SongAuthorsLoaded = false;

// Forward declaration de invalidación de caché de canción
static void InvalidateSongCacheEntry(const std::string& filename);

static std::unordered_map<std::string, std::string> LoadSongAuthors()
{
    std::lock_guard<std::mutex> lock(s_SongAuthorsMutex);
    std::string path = SongAuthorsFilePath();
    std::error_code ec;
    fs::file_time_type curMod = fs::last_write_time(U8Path(path), ec);
    if (!ec && s_SongAuthorsLoaded && curMod == s_SongAuthorsLastMod) {
        return s_SongAuthorsCache;
    }

    if (ec) {
        if (s_SongAuthorsLoaded) return s_SongAuthorsCache;
        s_SongAuthorsCache.clear();
        s_SongAuthorsLoaded = true;
        return s_SongAuthorsCache;
    }

    std::unordered_map<std::string, std::string> authors;
    std::ifstream f(U8Path(path));
    if (!f.is_open()) {
        s_SongAuthorsCache = authors;
        s_SongAuthorsLoaded = true;
        s_SongAuthorsLastMod = curMod;
        return authors;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        authors[line.substr(0, sep)] = line.substr(sep + 1);
    }
    s_SongAuthorsCache = authors;
    s_SongAuthorsLoaded = true;
    s_SongAuthorsLastMod = curMod;
    return authors;
}

void SetSongAuthor(const std::string& filename, const std::string& author)
{
    auto authors = LoadSongAuthors();
    if (author.empty()) authors.erase(filename);
    else                authors[filename] = author;

    std::string path = SongAuthorsFilePath();
    std::ofstream f(U8Path(path));
    if (!f.is_open()) return;
    for (const auto& [k, v] : authors)
        f << k << "=" << v << "\n";
    f.close();

    std::lock_guard<std::mutex> lock(s_SongAuthorsMutex);
    s_SongAuthorsCache = authors;
    std::error_code ec;
    s_SongAuthorsLastMod = fs::last_write_time(U8Path(path), ec);
    s_SongAuthorsLoaded = true;

    InvalidateSongCacheEntry(filename);
}

std::string GetSongAuthor(const std::string& filename)
{
    auto authors = LoadSongAuthors();
    auto it = authors.find(filename);
    return it != authors.end() ? it->second : "";
}

// =============================================================================
//  Etiquetas de cancion — songs_tags.ini, con caché en RAM
//  Formato: archivo=tag1,tag2,tag3
// =============================================================================
static std::string SongTagsFilePath()
{
    return GetAssetsPath() + "/../songs_tags.ini";
}

static std::vector<std::string> SplitTags(const std::string& raw)
{
    std::vector<std::string> out;
    std::stringstream ss(raw);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        // trim espacios
        size_t a = tag.find_first_not_of(' ');
        size_t b = tag.find_last_not_of(' ');
        if (a == std::string::npos) continue;
        out.push_back(tag.substr(a, b - a + 1));
    }
    return out;
}

static std::string JoinTags(const std::vector<std::string>& tags)
{
    std::string out;
    for (size_t i = 0; i < tags.size(); i++) {
        if (i) out += ",";
        out += tags[i];
    }
    return out;
}

static std::mutex s_SongTagsMutex;
static std::unordered_map<std::string, std::string> s_SongTagsRawCache;
static fs::file_time_type s_SongTagsRawLastMod{};
static bool s_SongTagsRawLoaded = false;

static std::unordered_map<std::string, std::string> LoadSongTagsRaw()
{
    std::lock_guard<std::mutex> lock(s_SongTagsMutex);
    std::string path = SongTagsFilePath();
    std::error_code ec;
    fs::file_time_type curMod = fs::last_write_time(U8Path(path), ec);
    if (!ec && s_SongTagsRawLoaded && curMod == s_SongTagsRawLastMod) {
        return s_SongTagsRawCache;
    }

    if (ec) {
        if (s_SongTagsRawLoaded) return s_SongTagsRawCache;
        s_SongTagsRawCache.clear();
        s_SongTagsRawLoaded = true;
        return s_SongTagsRawCache;
    }

    std::unordered_map<std::string, std::string> map;
    std::ifstream f(U8Path(path));
    if (!f.is_open()) {
        s_SongTagsRawCache = map;
        s_SongTagsRawLoaded = true;
        s_SongTagsRawLastMod = curMod;
        return map;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        map[line.substr(0, sep)] = line.substr(sep + 1);
    }
    s_SongTagsRawCache = map;
    s_SongTagsRawLoaded = true;
    s_SongTagsRawLastMod = curMod;
    return map;
}

std::vector<std::string> GetSongTags(const std::string& filename)
{
    auto map = LoadSongTagsRaw();
    auto it = map.find(filename);
    if (it == map.end()) return {};
    return SplitTags(it->second);
}

void SetSongTags(const std::string& filename, const std::vector<std::string>& tags)
{
    auto map = LoadSongTagsRaw();
    if (tags.empty()) map.erase(filename);
    else              map[filename] = JoinTags(tags);

    std::string path = SongTagsFilePath();
    std::ofstream f(U8Path(path));
    if (!f.is_open()) return;
    for (const auto& [k, v] : map)
        f << k << "=" << v << "\n";
    f.close();

    std::lock_guard<std::mutex> lock(s_SongTagsMutex);
    s_SongTagsRawCache = map;
    std::error_code ec;
    s_SongTagsRawLastMod = fs::last_write_time(U8Path(path), ec);
    s_SongTagsRawLoaded = true;

    InvalidateSongCacheEntry(filename);
}

// =============================================================================
//  Caché e Indexación en memoria de Canciones (para búsqueda instantánea >300 canciones)
// =============================================================================
struct SongCacheEntry {
    fs::file_time_type fileWriteTime{};
    std::string displayName;
    std::string displayNameNorm;
    std::string fileStemNorm;
    std::string author;
    std::string authorNorm;
    std::vector<std::string> tags;
    std::vector<std::string> tagsNorm;
    std::vector<std::string> verses;
    std::vector<std::string> versesNorm;
    std::string allLyricsNorm;
};

static std::unordered_map<std::string, SongCacheEntry> s_SongCache;
static std::mutex s_SongCacheMutex;

static void InvalidateSongCacheEntry(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(s_SongCacheMutex);
    s_SongCache.erase(filename);
}

void ClearSongIndexCache()
{
    {
        std::lock_guard<std::mutex> lock(s_SongCacheMutex);
        s_SongCache.clear();
    }
    {
        std::lock_guard<std::mutex> lock(s_SongAuthorsMutex);
        s_SongAuthorsLoaded = false;
        s_SongAuthorsCache.clear();
    }
    {
        std::lock_guard<std::mutex> lock(s_SongTagsMutex);
        s_SongTagsRawLoaded = false;
        s_SongTagsRawCache.clear();
    }
    ClearSongMetaCache();
}

static SongCacheEntry GetSongCacheEntry(const std::string& filename)
{
    std::string fullPath = GetAssetsPath() + "/songs/" + filename;
    std::error_code ec;
    fs::file_time_type curTime = fs::last_write_time(U8Path(fullPath), ec);

    {
        std::lock_guard<std::mutex> lock(s_SongCacheMutex);
        auto it = s_SongCache.find(filename);
        if (it != s_SongCache.end() && !ec && it->second.fileWriteTime == curTime) {
            return it->second;
        }
    }

    SongCacheEntry entry;
    entry.fileWriteTime   = curTime;
    entry.displayName     = GetSongDisplayName(filename);
    entry.displayNameNorm = NormalizeForSearch(entry.displayName);
    entry.fileStemNorm    = NormalizeForSearch(StripExtension(filename));
    entry.author          = GetSongAuthor(filename);
    entry.authorNorm      = NormalizeForSearch(entry.author);
    entry.tags            = GetSongTags(filename);
    entry.tagsNorm.reserve(entry.tags.size());
    for (const auto& t : entry.tags) {
        entry.tagsNorm.push_back(NormalizeForSearch(t));
    }

    std::ifstream file(U8Path(fullPath), std::ios::binary);
    if (!file.is_open()) {
        entry.verses.push_back("Error: No se pudo abrir el archivo.\nRuta: " + fullPath);
        return entry;
    }

    std::string raw((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();

    if (!raw.empty()) {
        std::string content = NormalizeToUtf8(raw);
        const int linesPerSlide = GetSongMeta(filename).linesPerSlide;
        entry.verses = GroupLyricsIntoSlides(content, linesPerSlide);
        entry.versesNorm.reserve(entry.verses.size());
        std::string allNorm;
        for (const auto& v : entry.verses) {
            std::string vn = NormalizeForSearch(v);
            entry.versesNorm.push_back(vn);
            allNorm.append(vn);
            allNorm.push_back('\n');
        }
        entry.allLyricsNorm = std::move(allNorm);
    }

    std::lock_guard<std::mutex> lock(s_SongCacheMutex);
    s_SongCache[filename] = entry;
    return entry;
}

std::vector<std::string> LoadSongVerses(const std::string& filename)
{
    return GetSongCacheEntry(filename).verses;
}

// =============================================================================
//  Preset por cancion (estilo + fondo) — mismo patron ini que autores/tags.
// =============================================================================
static std::string SongStyleFilePath()
{
    return GetAssetsPath() + "/../songs_style.ini";
}

static std::string SongBackgroundFilePath()
{
    return GetAssetsPath() + "/../songs_background.ini";
}

static std::string StanzaColorsFilePath()
{
    return GetAssetsPath() + "/../songs_stanza_colors.ini";
}

// Reutiliza el mismo formato "clave=valor" linea por linea que autores/tags;
// esto ya se repite 4 veces (autores, tags, estilo, fondo) asi que se
// generaliza en un par de helpers genericos en vez de copiar el mismo
// load/save por cuarta vez.
static std::unordered_map<std::string, std::string> LoadKeyValueIni(const std::string& path)
{
    std::unordered_map<std::string, std::string> map;
    std::ifstream f(U8Path(path));
    if (!f.is_open()) return map;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        map[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return map;
}

static void SaveKeyValueIni(const std::string& path, const std::unordered_map<std::string, std::string>& map)
{
    std::ofstream f(U8Path(path));
    if (!f.is_open()) return;
    for (const auto& [k, v] : map)
        f << k << "=" << v << "\n";
}

std::string GetSongStyle(const std::string& filename)
{
    auto map = LoadKeyValueIni(SongStyleFilePath());
    auto it = map.find(filename);
    return it != map.end() ? it->second : "";
}

void SetSongStyle(const std::string& filename, const std::string& styleName)
{
    auto map = LoadKeyValueIni(SongStyleFilePath());
    if (styleName.empty()) map.erase(filename);
    else                   map[filename] = styleName;
    SaveKeyValueIni(SongStyleFilePath(), map);
}

SongBackground GetSongBackground(const std::string& filename)
{
    auto map = LoadKeyValueIni(SongBackgroundFilePath());
    auto it = map.find(filename);
    if (it == map.end() || it->second.size() < 2 || it->second[1] != ':') return {};

    SongBackground bg;
    bg.isVideo = (it->second[0] == '1');
    bg.path    = it->second.substr(2);
    return bg;
}

void SetSongBackground(const std::string& filename, const std::string& path, bool isVideo)
{
    auto map = LoadKeyValueIni(SongBackgroundFilePath());
    if (path.empty()) map.erase(filename);
    else              map[filename] = std::string(isVideo ? "1:" : "0:") + path;
    SaveKeyValueIni(SongBackgroundFilePath(), map);
}

void ClearSongBackground(const std::string& filename)
{
    SetSongBackground(filename, "", false);
}

unsigned int GetStanzaColor(const std::string& filename, int stanzaIndex)
{
    auto map = LoadKeyValueIni(StanzaColorsFilePath());
    auto it = map.find(filename + "#" + std::to_string(stanzaIndex));
    if (it == map.end()) return 0u;
    return static_cast<unsigned int>(std::strtoul(it->second.c_str(), nullptr, 16));
}

void SetStanzaColor(const std::string& filename, int stanzaIndex, unsigned int colorU32)
{
    auto map = LoadKeyValueIni(StanzaColorsFilePath());
    std::string key = filename + "#" + std::to_string(stanzaIndex);
    if (colorU32 == 0u) {
        map.erase(key);
    } else {
        std::ostringstream oss;
        oss << std::hex << colorU32;
        map[key] = oss.str();
    }
    SaveKeyValueIni(StanzaColorsFilePath(), map);
}

void MigrateSongSidecars(const std::string& oldFilename, const std::string& newFilename)
{
    if (oldFilename.empty() || newFilename.empty() || oldFilename == newFilename) return;

    InvalidateSongCacheEntry(oldFilename);
    InvalidateSongCacheEntry(newFilename);

    // Autor / etiquetas / estilo preset / fondo preset: mapas simples
    // "archivo=valor", solo hay que reasignar la clave.
    auto migrateSimpleKey = [&](const std::string& path) {
        auto map = LoadKeyValueIni(path);
        auto it = map.find(oldFilename);
        if (it == map.end()) return;
        std::string value = it->second;
        map.erase(it);
        map[newFilename] = value;
        SaveKeyValueIni(path, map);
    };
    migrateSimpleKey(SongAuthorsFilePath());
    migrateSimpleKey(SongTagsFilePath());
    migrateSimpleKey(SongStyleFilePath());
    migrateSimpleKey(SongBackgroundFilePath());

    // Color de estrofa: clave compuesta "archivo#indice" — hay que
    // reescribir el prefijo de cada entrada que pertenezca a esta cancion.
    {
        auto map = LoadKeyValueIni(StanzaColorsFilePath());
        const std::string oldPrefix = oldFilename + "#";
        bool changed = false;
        std::unordered_map<std::string, std::string> updated;
        updated.reserve(map.size());
        for (auto& [k, v] : map) {
            if (k.rfind(oldPrefix, 0) == 0) {
                updated[newFilename + "#" + k.substr(oldPrefix.size())] = v;
                changed = true;
            } else {
                updated[k] = v;
            }
        }
        if (changed) SaveKeyValueIni(StanzaColorsFilePath(), updated);
    }

    RenameSongMeta(oldFilename, newFilename);
    RenameSongInAllPlaylists(oldFilename, newFilename);
}

// =============================================================================
//  RenameNewSongToTitleIfApplicable
// =============================================================================
namespace {

bool IsAutoGeneratedNewSongStem(const std::string& stem)
{
    static const char* kBases[] = { "Nueva canción", "Canción pegada", "Canción importada" };
    for (const char* base : kBases) {
        if (stem == base) return true;
        // Variantes con sufijo de deduplicacion: "<base> (2)", "<base> (3)"...
        const std::string prefix = std::string(base) + " (";
        if (stem.rfind(prefix, 0) == 0 && !stem.empty() && stem.back() == ')')
            return true;
    }
    return false;
}

// Reemplaza caracteres invalidos como nombre de archivo en Windows (y
// problematicos en Linux/macOS) por "-", y recorta espacios/puntos finales
// (Windows tampoco los permite ahi).
std::string SanitizeTitleForFilename(const std::string& title)
{
    std::string out;
    out.reserve(title.size());
    for (char c : title) {
        switch (c) {
            case '/': case '\\': case ':': case '*':
            case '?': case '"':  case '<': case '>': case '|':
                out += '-';
                break;
            default:
                out += c;
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    return out;
}

} // namespace

std::string RenameNewSongToTitleIfApplicable(const std::string& filename, const std::string& title)
{
    std::string ext;
    const std::string currentStem = SplitExtension(filename, ext);

    if (!IsAutoGeneratedNewSongStem(currentStem)) return filename;

    const std::string desiredStem = SanitizeTitleForFilename(title);
    if (desiredStem.empty() || desiredStem == currentStem) return filename;

    const std::string songsDir = GetAssetsPath() + "/songs/";
    std::string newFilename = desiredStem + ext;
    int suffix = 2;
    while (fs::exists(U8Path(songsDir + newFilename))) {
        newFilename = desiredStem + " (" + std::to_string(suffix) + ")" + ext;
        ++suffix;
    }

    std::error_code ec;
    fs::rename(U8Path(songsDir + filename), U8Path(songsDir + newFilename), ec);
    if (ec) return filename; // no se pudo renombrar, seguir con el nombre viejo

    MigrateSongSidecars(filename, newFilename);
    ForceListUpdate() = true;
    ForceLibraryRescan() = true;
    return newFilename;
}

// =============================================================================
//  RenderPaneHeader
//  Titulo discreto de columna del grid (Canciones / Playlists), con un
//  contador opcional. Da aire respecto al borde superior del panel y marca
//  visualmente donde empieza cada seccion.
// =============================================================================
static void RenderPaneHeader(const char* label, int count)
{
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
    if (count >= 0) ImGui::Text("%s   ·   %d", label, count);
    else            ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}
// =============================================================================
//  Detalle y Análisis de Coincidencias de Búsqueda de Canciones
// =============================================================================
static SongMatchDetail AnalyzeSongMatch(const std::string& filename, const std::string& rawQuery, LibraryContext& ctx)
{
    SongMatchDetail detail;
    if (rawQuery.empty()) return detail;

    std::string qNorm = NormalizeForSearch(rawQuery);
    if (qNorm.empty()) return detail;

    // Palabras separadas para soporte de búsqueda flexible multi-palabra
    std::vector<std::string> words;
    {
        std::stringstream ss(qNorm);
        std::string w;
        while (ss >> w) {
            if (!w.empty()) words.push_back(w);
        }
    }

    SongCacheEntry entry = GetSongCacheEntry(filename);

    // 1. Título y nombre de archivo (con y sin extensión)
    if (entry.displayNameNorm.find(qNorm) != std::string::npos || entry.fileStemNorm.find(qNorm) != std::string::npos) {
        detail.kind = SongMatchKind::Title;
        detail.snippet = entry.displayName;
        return detail;
    }
    if (words.size() > 1) {
        bool allWordsMatch = true;
        for (const auto& w : words) {
            if (entry.displayNameNorm.find(w) == std::string::npos && entry.fileStemNorm.find(w) == std::string::npos) {
                allWordsMatch = false;
                break;
            }
        }
        if (allWordsMatch) {
            detail.kind = SongMatchKind::Title;
            detail.snippet = entry.displayName;
            return detail;
        }
    }

    // 2. Autor
    if (!entry.authorNorm.empty()) {
        if (entry.authorNorm.find(qNorm) != std::string::npos) {
            detail.kind = SongMatchKind::Author;
            detail.matchParam = entry.author;
            detail.snippet = entry.author;
            return detail;
        }
        if (words.size() > 1) {
            bool allWordsInAuthor = true;
            for (const auto& w : words) {
                if (entry.authorNorm.find(w) == std::string::npos) {
                    allWordsInAuthor = false;
                    break;
                }
            }
            if (allWordsInAuthor) {
                detail.kind = SongMatchKind::Author;
                detail.matchParam = entry.author;
                detail.snippet = entry.author;
                return detail;
            }
        }
    }

    // 3. Etiquetas (Tags)
    for (size_t ti = 0; ti < entry.tags.size(); ++ti) {
        if (ti < entry.tagsNorm.size() && entry.tagsNorm[ti].find(qNorm) != std::string::npos) {
            detail.kind = SongMatchKind::Tag;
            detail.matchParam = entry.tags[ti];
            detail.snippet = entry.tags[ti];
            return detail;
        }
    }

    // 4. Letra / Estrofas (Verses)
    // Filtro de rechazo ultrarrápido en RAM: si la frase completa no está en toda la letra,
    // o si alguna palabra requerida no existe en toda la letra, salir inmediatamente
    if (entry.allLyricsNorm.empty()) return detail;

    if (entry.allLyricsNorm.find(qNorm) == std::string::npos) {
        if (words.size() <= 1) return detail;
        for (const auto& w : words) {
            if (entry.allLyricsNorm.find(w) == std::string::npos) {
                return detail;
            }
        }
    }

    // Si pasó el filtro de rechazo, buscar la estrofa y línea exacta para el snippet
    for (size_t vi = 0; vi < entry.versesNorm.size(); ++vi) {
        const std::string& stanzaNorm = entry.versesNorm[vi];
        bool stanzaMatches = (stanzaNorm.find(qNorm) != std::string::npos);
        if (!stanzaMatches && words.size() > 1) {
            bool allWordsInStanza = true;
            for (const auto& w : words) {
                if (stanzaNorm.find(w) == std::string::npos) {
                    allWordsInStanza = false;
                    break;
                }
            }
            if (allWordsInStanza) stanzaMatches = true;
        }

        if (stanzaMatches) {
            detail.kind = SongMatchKind::Verse;
            detail.stanzaIndex = (int)vi;

            const std::string& stanza = (vi < entry.verses.size()) ? entry.verses[vi] : "";
            std::stringstream ss(stanza);
            std::string line;
            int li = 0;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                std::string lineNorm = NormalizeForSearch(line);
                bool lineMatches = (lineNorm.find(qNorm) != std::string::npos);
                if (!lineMatches && !words.empty()) {
                    lineMatches = (lineNorm.find(words[0]) != std::string::npos);
                }
                if (lineMatches) {
                    detail.lineIndex = li;
                    size_t start = line.find_first_not_of(" \t");
                    if (start != std::string::npos) line = line.substr(start);
                    detail.snippet = line;
                    break;
                }
                li++;
            }
            if (detail.snippet.empty()) {
                detail.snippet = stanza.substr(0, std::min<size_t>(60, stanza.size()));
            }
            return detail;
        }
    }

    return detail;
}

// Dibuja un texto resaltando la porcion que coincide con query
static void DrawHighlightedText(ImDrawList* dl, ImFont* font, float fontSize,
                                ImVec2 pos, const std::string& text, const std::string& query,
                                ImU32 normalCol, ImU32 highlightCol, ImU32 highlightBgCol)
{
    if (query.empty() || text.empty()) {
        dl->AddText(font, fontSize, pos, normalCol, text.c_str());
        return;
    }

    std::string textNorm  = NormalizeForSearch(text);
    std::string queryNorm = NormalizeForSearch(query);

    size_t matchPos = textNorm.find(queryNorm);
    if (matchPos == std::string::npos && !queryNorm.empty()) {
        std::stringstream ss(queryNorm);
        std::string firstWord;
        if (ss >> firstWord) {
            matchPos = textNorm.find(firstWord);
            if (matchPos != std::string::npos) {
                queryNorm = firstWord;
            }
        }
    }

    std::string textLo = text;
    std::string qLo    = query;
    std::transform(textLo.begin(), textLo.end(), textLo.begin(), [](unsigned char c){ return (char)::tolower(c); });
    std::transform(qLo.begin(),    qLo.end(),    qLo.begin(),    [](unsigned char c){ return (char)::tolower(c); });
    size_t rawMatchPos = textLo.find(qLo);

    size_t finalPos = (rawMatchPos != std::string::npos) ? rawMatchPos : matchPos;
    if (finalPos == std::string::npos || finalPos >= text.size()) {
        dl->AddText(font, fontSize, pos, normalCol, text.c_str());
        return;
    }

    size_t matchLen = std::min(query.size(), text.size() - finalPos);
    std::string before  = text.substr(0, finalPos);
    std::string matched = text.substr(finalPos, matchLen);
    std::string after   = text.substr(finalPos + matchLen);

    float curX = pos.x;
    if (!before.empty()) {
        dl->AddText(font, fontSize, ImVec2(curX, pos.y), normalCol, before.c_str());
        curX += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, before.c_str()).x;
    }

    if (!matched.empty()) {
        ImVec2 mSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, matched.c_str());
        if (highlightBgCol != 0) {
            dl->AddRectFilled(ImVec2(curX - 2.0f, pos.y - 1.0f),
                              ImVec2(curX + mSz.x + 2.0f, pos.y + fontSize + 1.0f),
                              highlightBgCol, 2.5f);
        }
        dl->AddText(font, fontSize, ImVec2(curX, pos.y), highlightCol, matched.c_str());
        curX += mSz.x;
    }

    if (!after.empty()) {
        dl->AddText(font, fontSize, ImVec2(curX, pos.y), normalCol, after.c_str());
    }
}

// =============================================================================
//  SongListRow
//  Fila de lista para canciones, igual a DS::GlassListRow pero con soporte
//  para pintar el fondo con el color de la etiqueta asignada a la cancion y
//  resaltar detalles de coincidencia específicos en búsquedas.
// =============================================================================
static inline float RadiusSmallLocal() { return DS::RadiusSmall; }

static bool SongListRow(const char* label, bool selected,
                        ImVec4 tagColor, bool hasTag,
                        float indent = 14.0f, float height = DS::RowHeight,
                        float trailingReserve = 0.0f,
                        const SongMatchDetail* match = nullptr,
                        const std::string& query = "",
                        const char* badgePrefix = nullptr)
{
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float  rowW   = ImGui::GetContentRegionAvail().x;
    float  clickW = std::max(0.0f, rowW - trailingReserve);

    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(clickW, height));
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rMin = cursor;
    ImVec2 rMax = ImVec2(cursor.x + rowW, cursor.y + height);

    // ── Tinte de etiqueta (si tiene) ────────────────────────────────────────
    if (hasTag) {
        float alpha = selected ? 0.38f : (hovered ? 0.30f : 0.20f);
        ImU32 tagBg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(tagColor.x, tagColor.y, tagColor.z, alpha));
        dl->AddRectFilled(rMin, rMax, tagBg, RadiusSmallLocal());

        // Barra lateral con el color solido de la etiqueta
        dl->AddRectFilled(rMin, ImVec2(rMin.x + 3.0f, rMax.y),
                          ImGui::ColorConvertFloat4ToU32(tagColor), 1.5f);
    }

    // ── Fondo de seleccion / hover ─────────────────────────────────────────
    if (selected) {
        dl->AddRectFilled(rMin, rMax, DS::RowSelectedFill, DS::RadiusSmall);
        dl->AddRectFilled(rMin, ImVec2(rMin.x + 3.0f, rMax.y), DS::RowSelectedBar, 1.5f);
        dl->AddLine(
            ImVec2(rMin.x + 4.0f, rMax.y - 0.5f),
            ImVec2(rMax.x,        rMax.y - 0.5f),
            DS::SepColor, 1.0f);
    } else if (hovered && !hasTag) {
        dl->AddRectFilled(rMin, rMax, DS::RowHoverFill, DS::RadiusSmall);
        dl->AddRect(rMin, rMax, DS::SepColor, DS::RadiusSmall, 0, 0.5f);
    }

    // ── Texto de la fila (con recorte estricto para nunca desbordar) ───────
    ImFont* font   = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();

    dl->PushClipRect(rMin, ImVec2(std::max(rMin.x, rMax.x - trailingReserve - 6.0f), rMax.y), true);

    bool hasMatchSnippet = (match != nullptr && match->kind != SongMatchKind::None && !query.empty());

    if (!hasMatchSnippet) {
        // Fila simple de 1 línea
        ImVec2 textSz  = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        float textX = rMin.x + indent;
        float textY = rMin.y + std::floor((height - textSz.y) * 0.5f);

        if (badgePrefix != nullptr && badgePrefix[0] != '\0') {
            ImVec2 bSz = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, badgePrefix);
            float bH = 16.0f;
            float bW = bSz.x + 8.0f;
            ImVec2 bMin(textX, rMin.y + std::floor((height - bH) * 0.5f));
            ImVec2 bMax(textX + bW, bMin.y + bH);
            dl->AddRectFilled(bMin, bMax, selected ? IM_COL32(99, 112, 255, 65) : IM_COL32(255, 255, 255, 16), 3.5f);
            dl->AddText(font, fontSize * 0.85f,
                        ImVec2(bMin.x + 4.0f, bMin.y + (bH - bSz.y) * 0.5f),
                        selected ? DS::AccentLight : DS::TextSecondary, badgePrefix);
            textX = bMax.x + 7.0f;
        }

        ImU32 textCol = selected ? DS::TextPrimary : DS::TextSecondary;
        dl->AddText(ImVec2(textX, textY), textCol, label);
    } else {
        // Fila de 2 líneas enriquecida con indicador específico de la coincidencia
        float textX = rMin.x + indent;
        float line1Y = rMin.y + 4.0f;
        ImU32 textCol = selected ? DS::TextPrimary : DS::TextSecondary;

        // Fila 1: Título / Autor
        DrawHighlightedText(dl, font, fontSize, ImVec2(textX, line1Y), label, query,
                            textCol, IM_COL32(255, 230, 110, 255), IM_COL32(235, 175, 40, 50));

        // Fila 2: Badge indicador + Fragmento específico coincidente
        float line2Y = rMin.y + 24.0f;

        char badgeBuf[48] = "";
        ImU32 badgeBg = IM_COL32(90, 170, 245, 45);
        ImU32 badgeBord = IM_COL32(90, 170, 245, 130);
        ImU32 badgeTextCol = IM_COL32(140, 205, 255, 255);

        if (match->kind == SongMatchKind::Verse) {
            snprintf(badgeBuf, sizeof(badgeBuf), "Estrofa %d", match->stanzaIndex + 1);
            badgeBg = IM_COL32(90, 170, 245, 45);
            badgeBord = IM_COL32(90, 170, 245, 130);
            badgeTextCol = IM_COL32(140, 205, 255, 255);
        } else if (match->kind == SongMatchKind::Author) {
            snprintf(badgeBuf, sizeof(badgeBuf), "Autor");
            badgeBg = IM_COL32(180, 120, 245, 45);
            badgeBord = IM_COL32(180, 120, 245, 130);
            badgeTextCol = IM_COL32(215, 170, 255, 255);
        } else if (match->kind == SongMatchKind::Tag) {
            snprintf(badgeBuf, sizeof(badgeBuf), "Etiqueta");
            badgeBg = IM_COL32(80, 200, 140, 45);
            badgeBord = IM_COL32(80, 200, 140, 130);
            badgeTextCol = IM_COL32(120, 240, 180, 255);
        } else if (match->kind == SongMatchKind::Title) {
            snprintf(badgeBuf, sizeof(badgeBuf), "Título");
            badgeBg = IM_COL32(235, 180, 80, 45);
            badgeBord = IM_COL32(235, 180, 80, 130);
            badgeTextCol = IM_COL32(255, 215, 130, 255);
        }

        float curX = textX;
        if (badgeBuf[0] != '\0') {
            ImVec2 bSz = ImGui::CalcTextSize(badgeBuf);
            float padX = 5.0f, padY = 1.0f;
            ImVec2 bMin(curX, line2Y - 1.0f);
            ImVec2 bMax(curX + bSz.x + padX * 2.0f, line2Y + bSz.y + padY * 2.0f);
            dl->AddRectFilled(bMin, bMax, badgeBg, 3.0f);
            dl->AddRect(bMin, bMax, badgeBord, 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(curX + padX, line2Y + padY), badgeTextCol, badgeBuf);
            curX = bMax.x + 6.0f;
        }

        if (!match->snippet.empty()) {
            std::string snip = "\"" + match->snippet + "\"";
            DrawHighlightedText(dl, font, fontSize * 0.92f, ImVec2(curX, line2Y), snip, query,
                                IM_COL32(200, 205, 215, 210), IM_COL32(255, 230, 110, 255), IM_COL32(235, 175, 40, 50));
        }
    }

    dl->PopClipRect();

    return clicked;
}

// =============================================================================
//  RenderToolbarRow
//  Combo de "Estilo por defecto" a nivel de categoría — solo le queda a
//  Biblias. En Canciones se saco: ahora cada cancion tiene su propio
//  estilo/fondo preset (ver la tarjeta de ajustes en SongView), que
//  reemplaza al default compartido por toda la categoria. El boton
//  "Actualizar" ya no vive aca: se movio junto al buscador (ver
//  RenderSideList), asi que esta fila directamente no dibuja nada fuera de
//  Biblias.
// =============================================================================
static void RenderToolbarRow(LibraryContext& ctx)
{
    if (ctx.currentCategoryInt != kCat_Bibles)
        return;

    RenderDefaultStyleCombo(ctx, 0.0f);
}

// =============================================================================
//  RenderPlaylistsSection — contenido de la columna "Playlists".
//  Lista de playlists, o el detalle (tarjetas) de la playlist abierta con
//  soporte para reordenar, quitar cancion, renombrar, eliminar y agregar
//  canciones.
// =============================================================================
static void RenderPlaylistsSection(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    static std::string openPlaylist;
    static bool        showNewModal      = false;
    static bool        showRenameModal   = false;
    static bool        showAddSongsModal = false;
    static char        nameBuffer[256]   = {};

    std::vector<std::string> playlists = ctx.listPlaylists();

    if (!openPlaylist.empty() &&
        std::find(playlists.begin(), playlists.end(), openPlaylist) == playlists.end())
        openPlaylist.clear();

    // FIX: footerH debe reflejar EXACTAMENTE lo que se dibuja despues del
    // EndChild (gap + boton + margen inferior). Antes faltaban 6px porque
    // el Dummy({0,12}) final no estaba contemplado, y el panel padre
    // (NoScrollbar) recortaba el sobrante contra el borde.
    const float kFooterGap    = ImGui::GetStyle().ItemSpacing.y;
    const float kFooterMargin = 12.0f;
    const float footerH       = DS::ButtonHeight + kFooterGap + kFooterMargin;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(12.f, 10.f));

    if (ImGui::BeginChild("PlaylistsChild", { 0.f, -footerH }, true))
    {
        if (openPlaylist.empty())
        {
            if (playlists.empty()) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                ImGui::SetCursorPos({ std::floor(avail.x * 0.5f - 80.f), std::floor(avail.y * 0.5f - 10.f) });
                ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
                ImGui::TextUnformatted(str.libNoPlaylistsYet);
                ImGui::PopStyleColor();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 6.f));
            for (const auto& name : playlists)
            {
                auto songs = ctx.loadPlaylistSongs(name);
                std::string disp = name + "  (" + std::to_string(songs.size()) + ")";
                if (DS::GlassListRow(disp.c_str(), ctx.activePlaylistName == name))
                    openPlaylist = name;

                if (ImGui::BeginPopupContextItem(("##ctx_pl" + name).c_str()))
                {
                    if (ImGui::MenuItem(str.libRename)) {
                        ctx.renameOldName = name;
                        memset(nameBuffer, 0, sizeof(nameBuffer));
                        strncpy(nameBuffer, name.c_str(), sizeof(nameBuffer) - 1);
                        showRenameModal = true;
                    }
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
                    if (ImGui::MenuItem(str.deleteLabel)) {
                        ctx.deletePlaylist(name);
                        if (ctx.activePlaylistName == name) ctx.activePlaylistName.clear();
                    }
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }
            }
            ImGui::PopStyleVar();
        }
        else
        {
            // ── Header: boton volver + titulo + contador ────────────────────
            // FIX: antes titulo y contador se posicionaban con SameLine(rightX)
            // en la misma linea; en paneles angostos ese calculo daba un
            // rightX menor al cursor real (tras dibujar el titulo) y las dos
            // etiquetas terminaban superpuestas ("2acanciones" ilegible).
            // Ahora van en lineas separadas: siempre legibles sin importar
            // el ancho disponible, y el título se trunca con "..." si no
            // entra en vez de desbordar el panel.
            if (DS::GlassButton(str.libBackShort, { 90.f, 26.f }, DS::TextSecondary))
                openPlaylist.clear();

            ImGui::Spacing();

            {
                float maxTitleW = std::max(20.0f, ImGui::GetContentRegionAvail().x);
                std::string title = openPlaylist;
                if (ImGui::CalcTextSize(title.c_str()).x > maxTitleW)
                {
                    while (!title.empty() &&
                          ImGui::CalcTextSize((title + "...").c_str()).x > maxTitleW)
                        title.pop_back();
                    title += "...";
                }
                ImGui::PushStyleColor(ImGuiCol_Text, DS::TextPrimary);
                ImGui::TextUnformatted(title.c_str());
                ImGui::PopStyleColor();
            }

            auto songs = ctx.loadPlaylistSongs(openPlaylist);
            char countBuf[48];
            snprintf(countBuf, sizeof(countBuf),
                songs.size() == 1 ? str.libSongCountOneFmt : str.libSongCountManyFmt,
                (int)songs.size());
            std::string countLabel = countBuf;
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted(countLabel.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            DS::GlassSeparator();
            ImGui::Spacing();
            ImGui::Spacing();

            // ── Filas de canciones, mismo estilo minimalista y plano que la
            //    lista de Canciones (sin caja/borde en reposo, sin badge de
            //    numero: lo que se ve es el NOMBRE de la cancion) ───────────
            const float cardH = 34.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));

            for (int i = 0; i < (int)songs.size(); i++)
            {
                ImGui::PushID(i);

                bool isActive = (ctx.activePlaylistName == openPlaylist && ctx.activePlaylistIndex == i);

                ImVec2 p_min = ImGui::GetCursorScreenPos();
                float  cardW = ImGui::GetContentRegionAvail().x;
                ImVec2 p_max = { p_min.x + cardW, p_min.y + cardH };

                // Tamaños de los controles y ancho de la zona clickeable de
                // seleccion. IMPORTANTE: el InvisibleButton de seleccion NO
                // debe cubrir la zona de controles (subir/bajar/quitar), o
                // captura el click primero y los botones nunca se activan
                // (ImGui resuelve el mouse-capture por orden de declaracion,
                // no por z-order visual). Por eso el InvisibleButton se
                // limita a "selectW" en vez de ocupar todo "cardW".
                const ImVec2 ctrlSize(22.f, 22.f);
                const float  ctrlGap   = 4.0f;
                const float  ctrlZoneW = 3.0f * ctrlSize.x + 2.0f * ctrlGap + 10.0f;
                const float  selectW   = std::max(cardW - ctrlZoneW, cardW * 0.4f);

                ImGui::SetCursorScreenPos(p_min);
                ImGui::InvisibleButton("##card", { selectW, cardH });
                bool clicked   = ImGui::IsItemClicked();
                bool isHovered = ImGui::IsItemHovered();

                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Mismo tratamiento plano y minimalista que SongListRow: en
                // reposo no hay caja ni borde, solo texto sobre el fondo del
                // panel. El acento se nota unicamente en hover (relleno +
                // borde sutil) o activo (tinte translucido + barra lateral).
                ImU32 bg = isActive ? DS::RowSelectedFill
                         : isHovered ? DS::RowHoverFill
                         : IM_COL32(0, 0, 0, 0);

                dl->AddRectFilled(p_min, p_max, bg, DS::RadiusSmall);
                if (isHovered && !isActive)
                    dl->AddRect(p_min, p_max, DS::SepColor, DS::RadiusSmall, 0, 0.5f);
                if (isActive)
                    dl->AddRectFilled(p_min, { p_min.x + 3.0f, p_max.y }, DS::RowSelectedBar, 1.5f);

                // Titulo: el nombre de la cancion es el contenido principal
                // de la fila (antes era un numero en un badge redondo y el
                // titulo quedaba recortado a 1-2 letras).
                std::string title = GetSongDisplayName(songs[i]);
                ImVec2 titlePos = { p_min.x + 12.f, p_min.y + std::floor((cardH - ImGui::GetTextLineHeight()) * 0.5f) };
                dl->PushClipRect(p_min, { p_min.x + selectW - 8.f, p_max.y }, true);
                dl->AddText(titlePos, isActive ? DS::TextPrimary : DS::TextSecondary, title.c_str());
                dl->PopClipRect();

                if (clicked) ctx.selectPlaylistSong(openPlaylist, i);

                // ── Controles: subir / bajar / quitar ────────────────────────
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ctrlGap, 0.f));
                ImGui::SetCursorScreenPos({ p_min.x + selectW + 6.0f, p_min.y + (cardH - ctrlSize.y) * 0.5f });

                ImGui::BeginDisabled(i == 0);
                if (PillButton("^", ctrlSize, k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA, k_BtnNeutralT))
                    ctx.movePlaylistSong(openPlaylist, i, -1);
                ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::BeginDisabled(i == (int)songs.size() - 1);
                if (PillButton("v", ctrlSize, k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA, k_BtnNeutralT))
                    ctx.movePlaylistSong(openPlaylist, i, 1);
                ImGui::EndDisabled();

                ImGui::SameLine();
                if (PillButton("X", ctrlSize, k_BtnDel, k_BtnDelH, k_BtnDelA, k_BtnDelT))
                    ctx.removeSongFromPlaylist(openPlaylist, i);

                ImGui::PopStyleVar();

                ImGui::SetCursorScreenPos({ p_min.x, p_max.y });
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                ImGui::PopID();
            }

            ImGui::PopStyleVar(); // ItemSpacing filas

            if (songs.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
                ImGui::TextUnformatted(str.libNoSongsInPlaylist);
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();
            if (DS::GlassButton(str.libAddSongsBtn, { -1.f, DS::ButtonHeight }))
                showAddSongsModal = true;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    // FIX: este Dummy reproduce el mismo gap que antes daba ImGui::Spacing(),
    // pero ahora esta explicitamente incluido en footerH via kFooterGap.

    if (openPlaylist.empty()) {
        if (DS::GlassButton(str.libNewPlaylistBtn, { -1.f, DS::ButtonHeight })) {
            memset(nameBuffer, 0, sizeof(nameBuffer));
            showNewModal = true;
        }
    } else {
        ImGui::Dummy({ ImGui::GetContentRegionAvail().x, DS::ButtonHeight });
    }

    ImGui::Dummy({ 10.0f, kFooterMargin });

    // ── Modal nueva playlist ──────────────────────────────────────────────
    if (showNewModal) ImGui::OpenPopup("NuevaPlaylistModal##lib");
    ImGuiViewport* vpPl = ImGui::GetMainViewport();
    if (vpPl) {
        ImGui::SetNextWindowViewport(vpPl->ID);
        ImGui::SetNextWindowPos(vpPl->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("NuevaPlaylistModal##lib", &showNewModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted(str.libPlaylistNameLabel);
        ImGui::SetNextItemWidth(320.f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##newPlName", nameBuffer, sizeof(nameBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (DS::GlassButton(str.libCreateBtn, { 150.f, 34.f }) || enterPressed) {
            std::string nm(nameBuffer);
            if (!nm.empty() && ctx.createPlaylist(nm)) {
                openPlaylist = nm;
                showNewModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (DS::GlassButton(str.cancel, { 150.f, 34.f }, DS::TextSecondary)) {
            showNewModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Modal renombrar ───────────────────────────────────────────────────
    if (showRenameModal) ImGui::OpenPopup("RenombrarPlaylistModal##lib");
    if (vpPl) {
        ImGui::SetNextWindowViewport(vpPl->ID);
        ImGui::SetNextWindowPos(vpPl->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("RenombrarPlaylistModal##lib", &showRenameModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted(str.libNewNameLabel);
        ImGui::SetNextItemWidth(320.f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##renPlName", nameBuffer, sizeof(nameBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (DS::GlassButton(str.libRename, { 150.f, 34.f }) || enterPressed) {
            std::string nm(nameBuffer);
            if (!nm.empty() && ctx.renamePlaylist(ctx.renameOldName, nm)) {
                if (openPlaylist == ctx.renameOldName) openPlaylist = nm;
                if (ctx.activePlaylistName == ctx.renameOldName) ctx.activePlaylistName = nm;
                showRenameModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (DS::GlassButton(str.cancel, { 150.f, 34.f }, DS::TextSecondary)) {
            showRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

   // ── Modal agregar canciones ───────────────────────────────────────────
    static char addSongsSearchBuffer[256] = {};
    // Detecta la transicion cerrado->abierto: antes esto disparaba
    // OpenPopup()+memset() en CADA frame mientras el modal seguia abierto
    // (showAddSongsModal se queda en true todo ese tiempo), no solo al
    // abrirlo. Mientras el buscador tenia el foco, ImGui "autocuraba" el
    // texto visible cada frame -- pero al hacer click en un resultado (el
    // buscador pierde el foco, justo el momento de agregar la cancion), el
    // buffer quedaba vacio de nuevo y la lista volvia a mostrar TODAS las
    // canciones sin filtrar, asi que el click siguiente caia en la fila
    // equivocada. El llamado repetido a OpenPopup() con el popup ya abierto
    // tambien interfiere con el estado interno de "recien aparecio" de
    // ImGui, que es probablemente la causa del tamano roto (1x1px) reportado.
    static bool addSongsModalWasOpen = false;
    if (showAddSongsModal && !addSongsModalWasOpen) {
        ImGui::OpenPopup("AgregarCancionesModal##lib");
        memset(addSongsSearchBuffer, 0, sizeof(addSongsSearchBuffer));
    }
    addSongsModalWasOpen = showAddSongsModal;

    // Mismo tratamiento visual que RenderRenameModal (LibraryModals.cpp) --
    // este modal no tenia NINGUN estilo propio (fondo/borde/redondeo por
    // defecto de ImGui puro), por eso se veia completamente distinto al
    // resto de los popups de la Biblioteca.
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.078f, 0.078f, 0.082f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(20.f, 16.f));

    if (ImGui::BeginPopupModal("AgregarCancionesModal##lib", &showAddSongsModal,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        // Antes 420x520 -- se sentia chico para algo que se usa seguido y
        // se beneficia de tener espacio (buscar + ver la lista completa de
        // resultados). Mismo tamano que el panel de Ajustes.
        ImGui::SetWindowSize({ 900.f, 650.f }, ImGuiCond_Appearing);

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
        ImGui::TextUnformatted(str.libAddSongsTitle);
        ImGui::PopStyleColor();
        AccentSep(ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
        ImGui::Spacing();

        // ── Buscador ─────────────────────────────────────────────────────
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##addSongsSearch", str.libAddSongsSearchHint,
                                 addSongsSearchBuffer, sizeof(addSongsSearchBuffer));

        ImGui::Spacing();
        ImGui::Separator();

        // ── Lista filtrada, ordenada alfabeticamente ──────────────────────
        // Antes recorria ctx.items en el orden del disco (practicamente
        // aleatorio para el operador) y cada fila era un ImGui::Selectable
        // de una sola linea de texto -- sin separacion visual entre filas,
        // sin autor visible salvo que matcheara la busqueda, y "ya agregada"
        // se notaba solo por un tono de verde en el propio titulo. Ahora las
        // filas usan el mismo lenguaje visual que el resto de Biblioteca
        // (hover/borde sutil, radio chico) y una zona de accion fija a la
        // derecha: botón "+" para agregar, o insignia verde si ya esta.
        std::string q(addSongsSearchBuffer);
        std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c){ return (char)::tolower(c); });

        auto currentSongs = ctx.loadPlaylistSongs(openPlaylist);

        std::vector<std::string> matches;
        std::unordered_map<std::string, SongMatchDetail> modalMatchMap;
        matches.reserve(ctx.items.size());
        for (const auto& item : ctx.items)
        {
            std::string qNorm = NormalizeForSearch(q);
            if (!qNorm.empty())
            {
                SongMatchDetail match = AnalyzeSongMatch(item, q, ctx);
                if (match.kind == SongMatchKind::None) continue;
                modalMatchMap[item] = match;
            }
            matches.push_back(item);
        }
        std::sort(matches.begin(), matches.end(), [](const std::string& a, const std::string& b) {
            return GetSongCacheEntry(a).displayNameNorm < GetSongCacheEntry(b).displayNameNorm;
        });

        {
            char countBuf[64];
            snprintf(countBuf, sizeof(countBuf),
                matches.size() == 1 ? str.libSongsFoundOneFmt : str.libSongsFoundManyFmt,
                (int)matches.size());
            std::string countLabel = countBuf;
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted(countLabel.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();

        if (ImGui::BeginChild("AddSongsList", { 0.f, -50.f }))
        {
            const float actionZoneW = 96.0f;
            const ImVec2 addBtnSize(26.f, 26.f);
            const float lineH       = ImGui::GetTextLineHeight();
            ImFont* font            = ImGui::GetFont();
            float fontSize          = ImGui::GetFontSize();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 3.f));

            ImGuiListClipper clipper;
            clipper.Begin((int)matches.size());
            while (clipper.Step())
            {
                for (int matchIdx = clipper.DisplayStart; matchIdx < clipper.DisplayEnd; ++matchIdx)
                {
                    const auto& item = matches[matchIdx];
                    ImGui::PushID(item.c_str());

                bool already = std::find(currentSongs.begin(), currentSongs.end(), item) != currentSongs.end();

                const SongMatchDetail* mDetail = nullptr;
                if (!q.empty()) {
                    auto mIt = modalMatchMap.find(item);
                    if (mIt != modalMatchMap.end()) mDetail = &mIt->second;
                }
                const bool hasMatchDetail = (mDetail != nullptr && mDetail->kind != SongMatchKind::None);
                const float rowH = hasMatchDetail ? 46.0f : 38.0f;

                ImVec2 p_min  = ImGui::GetCursorScreenPos();
                float  rowW   = ImGui::GetContentRegionAvail().x;
                ImVec2 p_max  = { p_min.x + rowW, p_min.y + rowH };
                float  clickW = std::max(rowW - actionZoneW, rowW * 0.5f);

                ImGui::SetCursorScreenPos(p_min);
                ImGui::InvisibleButton("##row", { clickW, rowH });
                bool rowClicked = !already && ImGui::IsItemClicked();
                bool isHovered  = !already && ImGui::IsItemHovered();

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImU32 bg = already ? IM_COL32(82, 224, 160, 16)
                         : isHovered ? DS::RowHoverFill
                         : IM_COL32(0, 0, 0, 0);
                dl->AddRectFilled(p_min, p_max, bg, DS::RadiusSmall);
                if (isHovered)
                    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 18), DS::RadiusSmall, 0, 0.5f);

                SongCacheEntry entry = GetSongCacheEntry(item);
                std::string title  = entry.displayName;
                std::string author = entry.author;

                dl->PushClipRect(p_min, { p_min.x + clickW - 8.f, p_max.y }, true);
                if (!hasMatchDetail)
                {
                    if (author.empty())
                    {
                        ImVec2 titleSz = ImGui::CalcTextSize(title.c_str());
                        dl->AddText({ p_min.x + 12.f, p_min.y + (rowH - titleSz.y) * 0.5f },
                                    DS::TextPrimary, title.c_str());
                    }
                    else
                    {
                        dl->AddText({ p_min.x + 12.f, p_min.y + rowH * 0.5f - lineH - 1.0f },
                                    DS::TextPrimary, title.c_str());
                        dl->AddText({ p_min.x + 12.f, p_min.y + rowH * 0.5f + 1.0f },
                                    DS::TextSecondary, author.c_str());
                    }
                }
                else
                {
                    // Fila 1: Título y Autor resaltados
                    std::string dispLine = author.empty() ? title : (title + "  —  " + author);
                    DrawHighlightedText(dl, font, fontSize, ImVec2(p_min.x + 12.f, p_min.y + 4.0f),
                                        dispLine, q, DS::TextPrimary, IM_COL32(255, 230, 110, 255), IM_COL32(235, 175, 40, 50));

                    // Fila 2: Badge indicador + fragmento específico
                    float line2Y = p_min.y + 24.0f;
                    char badgeBuf[48] = "";
                    ImU32 badgeBg = IM_COL32(90, 170, 245, 45);
                    ImU32 badgeBord = IM_COL32(90, 170, 245, 130);
                    ImU32 badgeTextCol = IM_COL32(140, 205, 255, 255);

                    if (mDetail->kind == SongMatchKind::Verse) {
                        snprintf(badgeBuf, sizeof(badgeBuf), "Estrofa %d", mDetail->stanzaIndex + 1);
                        badgeBg = IM_COL32(90, 170, 245, 45);
                        badgeBord = IM_COL32(90, 170, 245, 130);
                        badgeTextCol = IM_COL32(140, 205, 255, 255);
                    } else if (mDetail->kind == SongMatchKind::Author) {
                        snprintf(badgeBuf, sizeof(badgeBuf), "Autor");
                        badgeBg = IM_COL32(180, 120, 245, 45);
                        badgeBord = IM_COL32(180, 120, 245, 130);
                        badgeTextCol = IM_COL32(215, 170, 255, 255);
                    } else if (mDetail->kind == SongMatchKind::Tag) {
                        snprintf(badgeBuf, sizeof(badgeBuf), "Etiqueta");
                        badgeBg = IM_COL32(80, 200, 140, 45);
                        badgeBord = IM_COL32(80, 200, 140, 130);
                        badgeTextCol = IM_COL32(120, 240, 180, 255);
                    } else if (mDetail->kind == SongMatchKind::Title) {
                        snprintf(badgeBuf, sizeof(badgeBuf), "Título");
                        badgeBg = IM_COL32(235, 180, 80, 45);
                        badgeBord = IM_COL32(235, 180, 80, 130);
                        badgeTextCol = IM_COL32(255, 215, 130, 255);
                    }

                    float curX = p_min.x + 12.f;
                    if (badgeBuf[0] != '\0') {
                        ImVec2 bSz = ImGui::CalcTextSize(badgeBuf);
                        float padX = 5.0f, padY = 1.0f;
                        ImVec2 bMin(curX, line2Y - 1.0f);
                        ImVec2 bMax(curX + bSz.x + padX * 2.0f, line2Y + bSz.y + padY * 2.0f);
                        dl->AddRectFilled(bMin, bMax, badgeBg, 3.0f);
                        dl->AddRect(bMin, bMax, badgeBord, 3.0f, 0, 1.0f);
                        dl->AddText(ImVec2(curX + padX, line2Y + padY), badgeTextCol, badgeBuf);
                        curX = bMax.x + 6.0f;
                    }

                    if (!mDetail->snippet.empty()) {
                        std::string snip = "\"" + mDetail->snippet + "\"";
                        DrawHighlightedText(dl, font, fontSize * 0.92f, ImVec2(curX, line2Y), snip, q,
                                            IM_COL32(200, 205, 215, 210), IM_COL32(255, 230, 110, 255), IM_COL32(235, 175, 40, 50));
                    }
                }
                dl->PopClipRect();

                // ── Zona de accion: agregar, o insignia si ya esta ────────
                ImGui::SetCursorScreenPos({ p_max.x - actionZoneW, p_min.y });
                ImGui::BeginGroup();
                if (already)
                {
                    const char* badge = str.libAddedBadge;
                    ImVec2 badgeSz = ImGui::CalcTextSize(badge);
                    ImVec2 badgePos = { p_max.x - 10.f - badgeSz.x, p_min.y + (rowH - badgeSz.y) * 0.5f };
                    dl->AddText(badgePos, DS::SuccessColor, badge);
                }
                else
                {
                    ImGui::SetCursorScreenPos({ p_max.x - addBtnSize.x - 10.f, p_min.y + (rowH - addBtnSize.y) * 0.5f });
                    if (GlassIconButton("addone", "add", "+", str.libAddToPlaylistTooltip, addBtnSize,
                                        ImGui::ColorConvertU32ToFloat4(DS::AccentLight)))
                        ctx.addSongToPlaylist(openPlaylist, item);
                }
                ImGui::EndGroup();

                if (rowClicked)
                    ctx.addSongToPlaylist(openPlaylist, item);

                ImGui::SetCursorScreenPos({ p_min.x, p_max.y });
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                ImGui::PopID();
            }
            }

            ImGui::PopStyleVar();

            if (matches.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
                ImGui::TextUnformatted(str.libNoResults);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();

        if (DS::GlassButton(str.libDoneBtn, { -1.f, 34.f })) {
            showAddSongsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
// =============================================================================
//  RenderItemsListPane
//  Lista con scroll + footer de acciones (Nuevo / Importar / Eliminar) para
//  la categoria activa. Para Canciones, cada fila se pinta con el color de
//  la primera etiqueta asignada (si tiene). Soporta navegacion con flechas
//  arriba/abajo cuando el panel de Biblioteca tiene el foco.
// =============================================================================
static void RenderItemsListPane(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    const float itemSpY   = ImGui::GetStyle().ItemSpacing.y;
    const float btnRowH   = DS::ButtonHeight;
    const float reservedH = btnRowH + itemSpY * 2.0f + 10.0f;

    static std::vector<std::string> filteredItems;
    static std::unordered_map<std::string, SongMatchDetail> s_SongMatchMap;
    static std::string lastSearch;
    static int lastCategory = -1;
    static size_t lastItemsCount = 0;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(10.f, 8.f));

    if (ImGui::BeginChild("ListChild", { 0.f, -reservedH }, true,
                          ImGuiWindowFlags_NoNavInputs))
    {
        // Etiquetas cargadas una vez por frame (no por fila), asi el lookup
        // de color por cancion es solo una busqueda en memoria.
        auto tagGroups = LoadSongTagGroups();

        std::string cur(ctx.searchBuffer);
        const bool shouldRebuild = (cur != lastSearch) ||
                                   (ctx.currentCategoryInt != lastCategory) ||
                                   (ctx.items.size() != lastItemsCount) ||
                                   ForceListUpdate();
        if (shouldRebuild) {
            filteredItems.clear();
            s_SongMatchMap.clear();

            if (ctx.currentCategoryInt == kCat_Songs) {
                Core::PresentationCore::Get().SetSongSearchQuery(cur);
            }

            std::string curNorm = NormalizeForSearch(cur);

            for (const auto& item : ctx.items) {
                bool match = true;
                if (!curNorm.empty()) {
                    if (ctx.currentCategoryInt == kCat_Songs) {
                        SongMatchDetail detail = AnalyzeSongMatch(item, cur, ctx);
                        if (detail.kind != SongMatchKind::None) {
                            match = true;
                            s_SongMatchMap[item] = detail;
                        } else {
                            match = false;
                        }
                    } else {
                        std::string lo = NormalizeForSearch(item);
                        if (ctx.currentCategoryInt == kCat_Bibles) {
                            auto bInfo = ProyecThor::UI::BibleNames::ParseBibleName(item);
                            lo += " " + NormalizeForSearch(bInfo.shortCode) + " " + NormalizeForSearch(bInfo.fullTitle) + " " + NormalizeForSearch(bInfo.langCode);
                        }
                        match = lo.find(curNorm) != std::string::npos;
                    }
                }

                if (match) filteredItems.push_back(item);
            }
            lastSearch = cur;
            lastCategory = ctx.currentCategoryInt;
            lastItemsCount = ctx.items.size();
            ForceListUpdate() = false;
        }

        // Failsafe: si la búsqueda está vacía o es solo espacios y filteredItems quedó vacío teniendo canciones en ctx.items
        std::string checkCurNorm = NormalizeForSearch(cur);
        if (checkCurNorm.empty() && filteredItems.empty() && !ctx.items.empty()) {
            filteredItems = ctx.items;
            s_SongMatchMap.clear();
            lastItemsCount = ctx.items.size();
            lastCategory = ctx.currentCategoryInt;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 4.f));

        if (filteredItems.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos({
                ImGui::GetCursorPosX() + std::floor(avail.x * 0.5f - 55.f),
                ImGui::GetCursorPosY() + std::floor(avail.y * 0.5f - 10.f) });
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted(str.libNoResults);
            ImGui::PopStyleColor();
        }

        // ── Navegacion con flechas arriba/abajo ──────────────────────────────
        static int s_PendingScrollIdx = -1;
        if (!filteredItems.empty() &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow) &&
            !ImGui::IsAnyItemActive())
        {
            int curFilteredIdx = -1;
            if (ctx.selectedIndex >= 0 && ctx.selectedIndex < (int)ctx.items.size()) {
                auto fIt = std::find(filteredItems.begin(), filteredItems.end(), ctx.items[ctx.selectedIndex]);
                if (fIt != filteredItems.end())
                    curFilteredIdx = (int)std::distance(filteredItems.begin(), fIt);
            }

            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   delta = -1;

            if (delta != 0) {
                int newIdx = (curFilteredIdx < 0)
                    ? (delta > 0 ? 0 : (int)filteredItems.size() - 1)
                    : std::clamp(curFilteredIdx + delta, 0, (int)filteredItems.size() - 1);

                SelectLibraryItem(ctx, filteredItems, newIdx);
                s_PendingScrollIdx = newIdx;
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)filteredItems.size());
        if (s_PendingScrollIdx >= 0) {
            clipper.IncludeItemByIndex(s_PendingScrollIdx);
        }

        while (clipper.Step())
        {
        for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
        {
            auto it = std::find(ctx.items.begin(), ctx.items.end(), filteredItems[n]);
            int origIdx = (it != ctx.items.end())
                ? (int)std::distance(ctx.items.begin(), it) : -1;

            const bool sel   = (ctx.selectedIndex == origIdx);
            std::string disp;
            std::string badgePrefixStr;
            ImVec4 tagColor(0.f, 0.f, 0.f, 0.f);
            bool   hasTag = false;

            if (ctx.currentCategoryInt == kCat_Songs) {
                SongCacheEntry entry = GetSongCacheEntry(filteredItems[n]);
                disp = entry.displayName;
                if (!entry.author.empty())
                    disp += "  —  " + entry.author;

                if (!entry.tags.empty()) {
                    auto grpIt = std::find_if(tagGroups.begin(), tagGroups.end(),
                        [&](const SongTagGroup& g){ return g.id == entry.tags[0]; });
                    if (grpIt != tagGroups.end()) {
                        tagColor = grpIt->color;
                        hasTag   = true;
                    }
                }
            } else if (ctx.currentCategoryInt == kCat_Bibles) {
                auto bInfo = ProyecThor::UI::BibleNames::ParseBibleName(filteredItems[n]);
                badgePrefixStr = bInfo.langCode;
                disp = bInfo.shortCode;
                if (!bInfo.fullTitle.empty() && bInfo.fullTitle != bInfo.shortCode)
                    disp += "  ·  " + bInfo.fullTitle;
            } else {
                disp = StripExtension(filteredItems[n]);
            }

            const SongMatchDetail* matchPtr = nullptr;
            if (ctx.currentCategoryInt == kCat_Songs && !checkCurNorm.empty()) {
                auto mIt = s_SongMatchMap.find(filteredItems[n]);
                if (mIt != s_SongMatchMap.end()) matchPtr = &mIt->second;
            }
            const float rowHeight = (matchPtr != nullptr && matchPtr->kind != SongMatchKind::None)
                ? (DS::RowHeight + 16.0f) : DS::RowHeight;

            ImGui::PushID(n);
            bool clicked = SongListRow(disp.c_str(), sel, tagColor, hasTag,
                                       10.0f, rowHeight, 0.0f, matchPtr, cur,
                                       badgePrefixStr.empty() ? nullptr : badgePrefixStr.c_str());

            if (ImGui::BeginPopupContextItem("song_ctx", ImGuiPopupFlags_MouseButtonRight)) {
                if (ctx.currentCategoryInt == kCat_Songs) {
                    if (ImGui::BeginMenu(str.libAssignTag)) {
                        if (tagGroups.empty()) {
                            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
                            ImGui::TextUnformatted("Crea etiquetas en");
                            ImGui::TextUnformatted("Ajustes > Canciones");
                            ImGui::PopStyleColor();
                        } else {
                            for (const auto& group : tagGroups) {
                                const bool assigned = SongHasTag(filteredItems[n], group.id);
                                if (ImGui::MenuItem(group.name.c_str(), nullptr, assigned)) {
                                    auto tags = ctx.getSongTags(filteredItems[n]);
                                    auto tagIt = std::find(tags.begin(), tags.end(), group.id);
                                    if (assigned) tags.erase(tagIt); else tags.push_back(group.id);
                                    ctx.setSongTags(filteredItems[n], tags);
                                    ForceListUpdate() = true;
                                }
                            }
                            ImGui::Separator();
                            if (ImGui::MenuItem(str.libRemoveAllTags)) {
                                ctx.setSongTags(filteredItems[n], {});
                                ForceListUpdate() = true;
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                }
                if (ImGui::MenuItem(str.libRename)) {
                    ctx.renameOldName  = filteredItems[n];
                    ctx.renameIsURL    = false;
                    ctx.renameURLIndex = -1;
                    ctx.selectedIndex  = origIdx;
                    std::string stem = SplitExtension(filteredItems[n], ctx.renameExtension);
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, stem.c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
                if (ImGui::MenuItem(str.deleteLabel)) {
                    ImGui::PopStyleColor();
                    ctx.selectedIndex = origIdx;
                    ImGui::CloseCurrentPopup();
                    ctx.deleteSelectedItem();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
            ImGui::PopID();

            if (n == s_PendingScrollIdx) {
                ImGui::SetScrollHereY(0.5f);
                s_PendingScrollIdx = -1;
            }

            if (clicked)
                SelectLibraryItem(ctx, filteredItems, n);
        }
        }

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = 10.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(sp, sp));

        if (ctx.currentCategoryInt == kCat_Songs) {
            const float bw3 = std::floor((avail - sp * 2.0f) / 3.0f);
            const ImVec2 btnSize(bw3, DS::ButtonHeight);

            if (GlassIconButton("newSong", "add", "+", str.newLabel, btnSize))
                CreateNewSong(ctx);
            ImGui::SameLine();
            if (GlassIconButton("importSong", "upload_file", "^", str.importLabel, btnSize))
                ctx.importFile();
            ImGui::SameLine();
            if (GlassIconButton("deleteSong", "delete", "X", str.deleteLabel, btnSize,
                                ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
                ctx.deleteSelectedItem();
        } else {
            const float bw2 = std::floor((avail - sp) / 2.0f);
            const ImVec2 btnSize(bw2, DS::ButtonHeight);

            if (GlassIconButton("importSong", "upload_file", "^", str.importLabel, btnSize))
                ctx.importFile();
            ImGui::SameLine();
            if (GlassIconButton("deleteSong", "delete", "X", str.deleteLabel, btnSize,
                                ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
                ctx.deleteSelectedItem();
        }

        ImGui::PopStyleVar();
    }
}
// =============================================================================
//  RenderSongsAndPlaylistsGrid
//  La selección entre Canciones y Playlists ahora se realiza directamente
//  desde la isla superior (Isla 1) para que este panel goce del 100% del alto
//  vertical desde el buscador hacia abajo, mostrando listas mucho más largas.
// =============================================================================
static void RenderSongsAndPlaylistsGrid(LibraryContext& ctx)
{
    const float contentH = ImGui::GetContentRegionAvail().y;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));

    if (!ctx.showPlaylistsTab)
    {
        ImGui::BeginChild("SongsPaneOnly", ImVec2(0.f, contentH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderItemsListPane(ctx);
        ImGui::EndChild();
    }
    else
    {
        ImGui::BeginChild("PlaylistsPaneOnly", ImVec2(0.f, contentH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderPlaylistsSection(ctx);
        ImGui::EndChild();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
// =============================================================================
//  CreateNewSong
//
//  Rework del editor: ya no abre un popup pidiendo titulo/autor/contenido
//  antes de crear el archivo (RenderSongEditor, retirado). Crea de una un
//  archivo vacio con nombre único, lo selecciona, y pide (cue "consumir una
//  vez" de PresentationCore) que SongView entre directo al editor unificado
//  apenas la seleccion coincida — el titulo visible se cambia desde ahi.
// =============================================================================
void CreateNewSong(LibraryContext& ctx)
{
    const std::string base = "Nueva canción";
    std::string filename = base + ".txt";
    int suffix = 2;
    while (fs::exists(U8Path(GetAssetsPath() + "/songs/" + filename))) {
        filename = base + " (" + std::to_string(suffix) + ").txt";
        ++suffix;
    }

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open())
        f << "\xEF\xBB\xBF";
    f.close();

    ctx.refreshList();

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = ctx.loadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    auto it = std::find(ctx.items.begin(), ctx.items.end(), filename);
    if (it != ctx.items.end())
        ctx.selectedIndex = (int)std::distance(ctx.items.begin(), it);

    Core::PresentationCore::Get().RequestSongEditorOpen(filename);
}

// =============================================================================
//  CreateNewSongFromText — nucleo compartido por CreateNewSongFromClipboard
//  y la importacion desde URL (ver LibrarySongs.h): elige un nombre de
//  archivo libre a partir de <suggestedTitle>, escribe <text> como letra
//  inicial, y abre el editor unificado directo. Sin LibraryContext (se
//  llama desde el menu Archivo, que no tiene una instancia a mano).
// =============================================================================
void CreateNewSongFromText(const std::string& suggestedTitle, const std::string& text)
{
    // Caracteres invalidos en nombres de archivo Windows (los mismos quedan
    // afuera en Linux por prolijidad, aunque ahi solo '/' es realmente
    // invalido) -- se reemplazan por espacio y se recorta el resultado.
    std::string base = suggestedTitle;
    for (char& c : base) {
        if (std::string("\\/:*?\"<>|").find(c) != std::string::npos)
            c = ' ';
    }
    while (!base.empty() && (base.front() == ' ' || base.front() == '.')) base.erase(base.begin());
    while (!base.empty() && (base.back()  == ' ' || base.back()  == '.')) base.pop_back();
    if (base.empty()) base = "Canción importada";
    if (base.size() > 80) base.resize(80); // nombres de archivo demasiado largos rompen algunos filesystems

    std::string filename = base + ".txt";
    int suffix = 2;
    while (fs::exists(U8Path(GetAssetsPath() + "/songs/" + filename))) {
        filename = base + " (" + std::to_string(suffix) + ").txt";
        ++suffix;
    }

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open()) {
        f << "\xEF\xBB\xBF";
        f << text;
    }
    f.close();

    ForceListUpdate() = true;

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = LoadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    Core::PresentationCore::Get().RequestSongEditorOpen(filename);
}

void CreateNewSongFromClipboard(const std::string& clipboardText)
{
    CreateNewSongFromText("Canción pegada", clipboardText);
}

bool SetSongText(const std::string& filename, const std::string& text)
{
    std::string fullPath = GetAssetsPath() + "/songs/" + filename;
    if (!fs::exists(U8Path(fullPath)))
        return false;

    std::ofstream f(U8Path(fullPath));
    if (!f.is_open())
        return false;

    f << "\xEF\xBB\xBF";
    f << text;
    f.close();

    InvalidateSongCacheEntry(filename);
    ForceListUpdate() = true;
    return true;
}

bool UpdateSongStanza(const std::string& filename, int stanzaIndex, const std::string& newText)
{
    std::string fullPath = GetAssetsPath() + "/songs/" + filename;
    if (!fs::exists(U8Path(fullPath)))
        return false;

    std::ifstream file(U8Path(fullPath), std::ios::binary);
    if (!file.is_open())
        return false;

    std::string raw((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();

    std::string content = NormalizeToUtf8(raw);
    const int linesPerSlide = GetSongMeta(filename).linesPerSlide;

    // Separar en párrafos preservando líneas
    std::vector<std::vector<std::string>> paragraphs;
    std::vector<std::string> currentParagraph;
    std::string line;
    std::istringstream stream(content);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            if (!currentParagraph.empty()) {
                paragraphs.push_back(currentParagraph);
                currentParagraph.clear();
            }
        } else {
            currentParagraph.push_back(line);
        }
    }
    if (!currentParagraph.empty()) paragraphs.push_back(currentParagraph);

    // Separar newText en líneas
    std::vector<std::string> newLines;
    std::istringstream nss(newText);
    std::string nl;
    while (std::getline(nss, nl)) {
        if (!nl.empty() && nl.back() == '\r') nl.pop_back();
        newLines.push_back(nl);
    }
    while (!newLines.empty() && newLines.back().empty()) {
        newLines.pop_back();
    }
    if (newLines.empty()) {
        newLines.push_back("");
    }

    // Reemplazar la diapositiva correspondiente
    int currentSlide = 0;
    bool replaced = false;

    for (size_t p = 0; p < paragraphs.size(); ++p) {
        if (linesPerSlide <= 0) {
            if (currentSlide == stanzaIndex) {
                paragraphs[p] = newLines;
                replaced = true;
                break;
            }
            currentSlide++;
        } else {
            for (size_t i = 0; i < paragraphs[p].size(); i += (size_t)linesPerSlide) {
                if (currentSlide == stanzaIndex) {
                    size_t countToRemove = std::min(paragraphs[p].size() - i, (size_t)linesPerSlide);
                    paragraphs[p].erase(paragraphs[p].begin() + i, paragraphs[p].begin() + i + countToRemove);
                    paragraphs[p].insert(paragraphs[p].begin() + i, newLines.begin(), newLines.end());
                    replaced = true;
                    break;
                }
                currentSlide++;
            }
            if (replaced) break;
        }
    }

    if (!replaced) {
        paragraphs.push_back(newLines);
    }

    // Reconstruir texto completo
    std::string reconstructed;
    for (size_t p = 0; p < paragraphs.size(); ++p) {
        if (p > 0) reconstructed += "\n\n";
        for (size_t l = 0; l < paragraphs[p].size(); ++l) {
            if (l > 0) reconstructed += "\n";
            reconstructed += paragraphs[p][l];
        }
    }
    reconstructed += "\n";

    if (!SetSongText(filename, reconstructed))
        return false;

    Core::PresentationCore::Get().UpdateCurrentSelectionStanza(stanzaIndex, newText);
    return true;
}

// =============================================================================
//  RenderMediaFoldersPane
//  Muestra las carpetas / colecciones para Videos, Imágenes o Música en la
//  barra lateral con botón de reproducción directa en cola, conteo de items,
//  reordenamiento por arrastre y creación/edición de carpetas.
// =============================================================================
static void RenderMediaFoldersPane(LibraryContext& ctx, Core::CollectionMediaType type)
{
    auto& colMgr = Core::MediaCollectionManager::Get();
    auto colNames = colMgr.GetCollectionNames(type);
    std::string activeCol = colMgr.GetActiveCollection(type);

    int totalCount = 0;
    std::vector<std::string> allFilesFallback;
    if (type == Core::CollectionMediaType::Video) {
        const auto& list = Library::GetVideoList();
        totalCount = (int)list.size();
        for (const auto& it : list) allFilesFallback.push_back(it.filename);
    } else if (type == Core::CollectionMediaType::Image) {
        const auto& list = Library::GetImageList();
        totalCount = (int)list.size();
        for (const auto& it : list) allFilesFallback.push_back(it.filename);
    } else {
        const auto& list = Library::GetAudioList();
        totalCount = (int)list.size();
        for (const auto& it : list) allFilesFallback.push_back(it.filename);
    }

    bool isQueuePlaying = colMgr.IsQueuePlaying() && colMgr.GetQueueType() == type;
    std::string queueCol = isQueuePlaying ? colMgr.GetQueueCollectionName() : "";

    static bool s_ShowNewFolderModal = false;
    static char s_NewFolderNameBuf[64] = "";
    static bool s_ShowRenameFolderModal = false;
    static std::string s_RenameTargetFolder = "";
    static char s_RenameFolderNameBuf[64] = "";

    const float itemSpY   = ImGui::GetStyle().ItemSpacing.y;
    const float btnRowH   = DS::ButtonHeight;
    const float reservedH = btnRowH + itemSpY * 2.0f + 6.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(6.f, 6.f));

    if (ImGui::BeginChild("FoldersListChild", { 0.f, -reservedH }, true))
    {
        std::string searchLower = ctx.searchBuffer ? ctx.searchBuffer : "";
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto RenderFolderRow = [&](const std::string& name, int count, bool isAll, int colIdx) {
            if (!searchLower.empty()) {
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(searchLower) == std::string::npos) return;
            }

            bool isSelected = isAll ? activeCol.empty() : (activeCol == name);
            bool isThisQueueRunning = isQueuePlaying && (isAll ? queueCol.empty() : (queueCol == name));

            ImGui::PushID(isAll ? "##all_folder" : name.c_str());

            const float rowH = 34.0f;
            const float btnSz = 24.0f;
            const float spX = 4.0f;
            const float actionW = isAll ? btnSz : (btnSz * 2.0f + spX);
            const float availRowW = ImGui::GetContentRegionAvail().x;
            const float mainW = std::max(10.0f, availRowW - actionW - spX - 8.0f);

            ImVec2 pMin = ImGui::GetCursorScreenPos();
            ImVec2 pMax = { pMin.x + availRowW, pMin.y + rowH };

            std::string invId = "##rowInvBtn_" + (isAll ? std::string("all") : name);
            bool rowClicked = ImGui::InvisibleButton(invId.c_str(), ImVec2(mainW, rowH));
            bool rowHovered = ImGui::IsItemHovered();

            if (!isAll) {
                // Drag Source for reordering
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("FOLDER_REORDER_IDX", &colIdx, sizeof(int));
                    ImGui::Text("Mover carpeta '%s'", name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Drag Target for reordering & dropping media items
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("FOLDER_REORDER_IDX")) {
                        int fromIdx = *(const int*)p->Data;
                        colMgr.MoveCollection(type, fromIdx, colIdx);
                    }
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MEDIA_ITEM_FILENAME")) {
                        const char* droppedFile = (const char*)p->Data;
                        colMgr.AddItemToCollection(type, name, droppedFile);
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginPopupContextItem("##folderRowCtxMenu")) {
                    if (ImGui::MenuItem("▶ Reproducir en Cola")) {
                        colMgr.SetActiveCollection(type, name);
                        colMgr.StartQueue(type, name, {}, 0);
                    }
                    ImGui::Separator();
                    if (colIdx > 0 && ImGui::MenuItem("🔼 Mover arriba")) {
                        colMgr.MoveCollection(type, colIdx, colIdx - 1);
                    }
                    if (colIdx < (int)colNames.size() - 1 && ImGui::MenuItem("🔽 Mover abajo")) {
                        colMgr.MoveCollection(type, colIdx, colIdx + 1);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("✏ Renombrar")) {
                        s_ShowRenameFolderModal = true;
                        s_RenameTargetFolder = name;
                        strncpy_s(s_RenameFolderNameBuf, name.c_str(), sizeof(s_RenameFolderNameBuf) - 1);
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(239, 68, 68, 255));
                    if (ImGui::MenuItem("🗑 Eliminar Carpeta")) {
                        colMgr.DeleteCollection(type, name);
                        if (activeCol == name) colMgr.SetActiveCollection(type, "");
                    }
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }
            }

            if (rowClicked) {
                colMgr.SetActiveCollection(type, isAll ? "" : name);
            }

            // Background & border across full row width
            ImU32 bgCol = isSelected ? IM_COL32(56, 189, 248, 35) :
                          (rowHovered ? IM_COL32(255, 255, 255, 18) : IM_COL32(255, 255, 255, 6));
            ImU32 bdrCol = isSelected ? IM_COL32(56, 189, 248, 180) :
                           (rowHovered ? IM_COL32(255, 255, 255, 40) : IM_COL32(255, 255, 255, 12));
            dl->AddRectFilled(pMin, pMax, bgCol, DS::RadiusSmall);
            dl->AddRect(pMin, pMax, bdrCol, DS::RadiusSmall, 0, isSelected ? 1.5f : 1.0f);

            // Icon 📁 and Folder Name
            float textY = pMin.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
            dl->AddText({ pMin.x + 8.0f, textY }, isAll ? IM_COL32(56, 189, 248, 255) : IM_COL32(245, 158, 11, 255), "📁");

            std::string dispName = isAll ? ((type == Core::CollectionMediaType::Video) ? "Todos los Videos" :
                                            (type == Core::CollectionMediaType::Image) ? "Todas las Imágenes" :
                                                                                         "Toda la Música") : name;
            float maxTextW = mainW - 46.0f;
            if (ImGui::CalcTextSize(dispName.c_str()).x > maxTextW) {
                while (!dispName.empty() && ImGui::CalcTextSize((dispName + "...").c_str()).x > maxTextW) {
                    dispName.pop_back();
                }
                dispName += "...";
            }

            ImU32 nameCol = isSelected ? IM_COL32(255, 255, 255, 255) :
                            (rowHovered ? IM_COL32(240, 240, 240, 255) : DS::TextPrimary);
            dl->AddText({ pMin.x + 28.0f, textY }, nameCol, dispName.c_str());

            std::string countStr = "(" + std::to_string(count) + ")";
            float countX = pMin.x + 30.0f + ImGui::CalcTextSize(dispName.c_str()).x + 6.0f;
            if (countX + ImGui::CalcTextSize(countStr.c_str()).x < pMin.x + mainW) {
                dl->AddText({ countX, textY }, DS::TextSecondary, countStr.c_str());
            }

            // Buttons on the right
            ImGui::SameLine(0.0f, spX);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            if (isThisQueueRunning) {
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(239, 68, 68, 180));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(239, 68, 68, 240));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(220, 38, 38, 255));
                if (ImGui::Button("⏹##qStop", ImVec2(btnSz, rowH))) {
                    colMgr.StopQueue();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Detener cola");
                ImGui::PopStyleColor(3);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(34, 197, 94, 80));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(34, 197, 94, 180));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(34, 197, 94, 240));
                if (ImGui::Button("▶##qPlay", ImVec2(btnSz, rowH))) {
                    if (isAll) {
                        colMgr.SetActiveCollection(type, "");
                        colMgr.StartQueue(type, "", allFilesFallback, 0);
                    } else {
                        colMgr.SetActiveCollection(type, name);
                        colMgr.StartQueue(type, name, {}, 0);
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(isAll ? "Reproducir todos en cola" : "Reproducir carpeta en cola");
                ImGui::PopStyleColor(3);
            }

            if (!isAll) {
                ImGui::SameLine(0.0f, spX);
                if (ImGui::Button("...##ctxBtn", ImVec2(btnSz, rowH))) {
                    ImGui::OpenPopup("##folderRowCtxMenu");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Opciones de carpeta");
            }
            ImGui::PopStyleVar();

            ImGui::PopID();
        };

        // 1. "Todos" row
        RenderFolderRow("Todos", totalCount, true, -1);

        // 2. Custom folder rows
        for (int i = 0; i < (int)colNames.size(); i++) {
            const auto* c = colMgr.GetCollection(type, colNames[i]);
            int count = c ? (int)c->itemFilenames.size() : 0;
            RenderFolderRow(colNames[i], count, false, i);
        }

        if (colNames.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("  Sin carpetas creadas.");
            ImGui::TextDisabled("  Haz clic abajo en '+ Nueva Carpeta'.");
        }
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Footer Buttons: [+ Nueva Carpeta] [Importar] [Actualizar] ────────
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float bwNew = avail - 2.0f * (DS::ButtonHeight + sp);
        const ImVec2 newBtnSize(bwNew, DS::ButtonHeight);
        const ImVec2 iconBtnSize(DS::ButtonHeight, DS::ButtonHeight);

        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(56, 189, 248, 60));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 189, 248, 140));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(56, 189, 248, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        if (ImGui::Button("+ Nueva Carpeta", newBtnSize)) {
            s_ShowNewFolderModal = true;
            s_NewFolderNameBuf[0] = '\0';
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (GlassIconButton("importMediaFolders", "upload_file", "^", "Importar archivo", iconBtnSize)) {
            ctx.importFile();
        }
        ImGui::SameLine();
        if (GlassIconButton("refreshMediaFolders", "repeat", "R", "Actualizar", iconBtnSize)) {
            ctx.refreshList();
        }
    }

    // Modal: Nueva Carpeta
    if (s_ShowNewFolderModal) {
        ImGui::OpenPopup("Nueva Carpeta##modalFolder");
    }
    ImGuiViewport* vpFolder = ImGui::GetMainViewport();
    if (vpFolder) {
        ImGui::SetNextWindowViewport(vpFolder->ID);
        ImGui::SetNextWindowPos(vpFolder->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("Nueva Carpeta##modalFolder", &s_ShowNewFolderModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Nombre de la nueva carpeta:");
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##newFldName", s_NewFolderNameBuf, sizeof(s_NewFolderNameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (ImGui::Button("Crear", ImVec2(100.0f, DS::ButtonHeight)) || enterPressed) {
            std::string n = s_NewFolderNameBuf;
            if (!n.empty()) {
                colMgr.CreateCollection(type, n);
                colMgr.SetActiveCollection(type, n);
            }
            s_ShowNewFolderModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, DS::ButtonHeight))) {
            s_ShowNewFolderModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Modal: Renombrar Carpeta
    if (s_ShowRenameFolderModal) {
        ImGui::OpenPopup("Renombrar Carpeta##modalRenameFld");
    }
    if (vpFolder) {
        ImGui::SetNextWindowViewport(vpFolder->ID);
        ImGui::SetNextWindowPos(vpFolder->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("Renombrar Carpeta##modalRenameFld", &s_ShowRenameFolderModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Nuevo nombre para '%s':", s_RenameTargetFolder.c_str());
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##renFldName", s_RenameFolderNameBuf, sizeof(s_RenameFolderNameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (ImGui::Button("Guardar", ImVec2(100.0f, DS::ButtonHeight)) || enterPressed) {
            std::string n = s_RenameFolderNameBuf;
            if (!n.empty() && n != s_RenameTargetFolder) {
                colMgr.RenameCollection(type, s_RenameTargetFolder, n);
            }
            s_ShowRenameFolderModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, DS::ButtonHeight))) {
            s_ShowRenameFolderModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// =============================================================================
//  RenderSideList
//  Todo el contenido vive dentro de un contenedor con padding parejo en
//  los 4 lados (izquierda/derecha/arriba/abajo), para que el buscador, el
//  combo, la grilla y los botones no queden pegados a los bordes del panel.
// =============================================================================
void RenderSideList(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 14.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8.f, 8.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::BeginChild("LibraryPad", ImGui::GetContentRegionAvail(), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // ── Selector de Assets (Canciones, Playlists, Videos, Música, Imágenes) ──
    const float availW = ImGui::GetContentRegionAvail().x;
    constexpr float tabH = 32.0f;
    constexpr float tabGap = 4.0f;

    struct AssetDef {
        const char* label;
        int category;
        bool isPlaylist;
        ImU32 accentCol;
        void (*drawIcon)(ImDrawList*, ImVec2, float, ImU32);
    };

    const AssetDef k_Assets[] = {
        { "Canciones",  kCat_Songs,     false, IM_COL32(56, 189, 248, 255), Library::DrawIcon_Music },
        { "Playlists",  kCat_Songs,     true,  IM_COL32(168, 85, 247, 255), Library::DrawIcon_Playlist },
        { "Videos",     kCat_Videos,    false, IM_COL32(239, 68, 68, 255),   Library::DrawIcon_Video },
        { "Música",     kCat_Audio,     false, IM_COL32(34, 197, 94, 255),   Library::DrawIcon_Audio },
        { "Imágenes",   kCat_Images,    false, IM_COL32(245, 158, 11, 255), Library::DrawIcon_Image },
        { "Documentos", kCat_Documents, false, IM_COL32(99, 102, 241, 255),  Library::DrawIcon_Document },
    };

    bool singleRow = (availW >= 240.0f);
    int row1Count  = singleRow ? 6 : 3;
    int row2Count  = singleRow ? 0 : 3;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    auto DrawAssetButton = [&](int i, float btnW) {
        const auto& a = k_Assets[i];
        bool isActive = false;
        if (a.isPlaylist) {
            isActive = (ctx.currentCategoryInt == kCat_Songs && ctx.showPlaylistsTab);
        } else if (a.category == kCat_Songs) {
            isActive = (ctx.currentCategoryInt == kCat_Songs && !ctx.showPlaylistsTab);
        } else {
            isActive = (ctx.currentCategoryInt == a.category);
        }

        ImVec2 pMin = ImGui::GetCursorScreenPos();
        ImVec2 pMax = { pMin.x + btnW, pMin.y + tabH };

        std::string btnId = "##libAssetBtn_" + std::to_string(i);
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), ImVec2(btnW, tabH));
        bool hovered = ImGui::IsItemHovered();

        ImU32 bgCol = isActive ? ImGui::ColorConvertFloat4ToU32(ImVec4(
                                    ((a.accentCol >> 0) & 0xFF) / 255.0f,
                                    ((a.accentCol >> 8) & 0xFF) / 255.0f,
                                    ((a.accentCol >> 16) & 0xFF) / 255.0f, 0.22f))
                               : (hovered ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 9));

        ImU32 bdrCol = isActive ? a.accentCol : (hovered ? IM_COL32(255, 255, 255, 50) : IM_COL32(255, 255, 255, 16));
        float bdrThick = isActive ? 1.5f : 1.0f;

        dl->AddRectFilled(pMin, pMax, bgCol, DS::RadiusSmall);
        dl->AddRect(pMin, pMax, bdrCol, DS::RadiusSmall, 0, bdrThick);

        dl->PushClipRect(pMin, pMax, true);
        float iconSz = 16.0f;
        ImVec2 iconPos = { pMin.x + (btnW - iconSz) * 0.5f, pMin.y + (tabH - iconSz) * 0.5f };
        ImU32 textCol = isActive ? IM_COL32(255, 255, 255, 255) : (hovered ? IM_COL32(230, 230, 230, 255) : DS::TextSecondary);
        ImU32 icCol   = isActive ? a.accentCol : textCol;

        if (a.drawIcon) {
            a.drawIcon(dl, iconPos, iconSz, icCol);
        }
        dl->PopClipRect();

        if (hovered && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", a.label);
        }

        if (clicked) {
            if (a.isPlaylist) {
                ctx.currentCategoryInt = kCat_Songs;
                ctx.showPlaylistsTab   = true;
                ctx.selectedIndex      = -1;
            } else if (a.category == kCat_Songs) {
                ctx.currentCategoryInt = kCat_Songs;
                ctx.showPlaylistsTab   = false;
                ctx.selectedIndex      = -1;
                ctx.refreshList();
            } else {
                ctx.currentCategoryInt = a.category;
                ctx.showPlaylistsTab   = false;
                ctx.selectedIndex      = -1;
                ctx.refreshList();
            }
        }
    };

    float wRow1 = (availW - tabGap * (row1Count - 1)) / row1Count;
    for (int i = 0; i < row1Count; i++) {
        if (i > 0) ImGui::SameLine(0.0f, tabGap);
        DrawAssetButton(i, wRow1);
    }

    if (row2Count > 0) {
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
        float wRow2 = (availW - tabGap * (row2Count - 1)) / row2Count;
        for (int i = 0; i < row2Count; i++) {
            if (i > 0) ImGui::SameLine(0.0f, tabGap);
            DrawAssetButton(row1Count + i, wRow2);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Barra de búsqueda + boton de actualizar (mismo renglon: antes el
    //    refresh vivia en su propio renglon abajo, junto al combo de estilo
    //    por defecto) ───────────────────────────────────────────────────────
    const float refreshBtnW = DS::ButtonHeight;
    const float refreshGap  = 8.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-(refreshBtnW + refreshGap));

    const char* searchHint = str.libSearchHint;
    if (ctx.showPlaylistsTab) searchHint = "Buscar playlist...";
    else if (ctx.currentCategoryInt == kCat_Videos) searchHint = "Buscar carpeta de videos...";
    else if (ctx.currentCategoryInt == kCat_Audio) searchHint = "Buscar carpeta de música...";
    else if (ctx.currentCategoryInt == kCat_Images) searchHint = "Buscar carpeta de imágenes...";
    else if (ctx.currentCategoryInt == kCat_Documents) searchHint = "Buscar documentos...";

    if (ImGui::InputTextWithHint("##search", searchHint,
                                 ctx.searchBuffer, ctx.searchBufferSize))
        ForceListUpdate() = true;
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    // Ícono de búsqueda superpuesto (solo si hay espacio para que no choque con el texto)
    if (StyleGeneralApp::Icons.count("search") > 0) {
        void* icon = StyleGeneralApp::Icons["search"].textureID;
        if (icon) {
            constexpr float pad = 5.0f;
            ImVec2 iMin = ImGui::GetItemRectMin();
            ImVec2 iMax = ImGui::GetItemRectMax();
            float  bH   = iMax.y - iMin.y;
            float  bW   = iMax.x - iMin.x;
            const char* shownTxt = (ctx.searchBuffer && ctx.searchBuffer[0] != '\0') ? ctx.searchBuffer : str.libSearchHint;
            float txtW = shownTxt ? ImGui::CalcTextSize(shownTxt).x : 0.0f;
            if (bW > txtW + bH + 14.0f) {
                ImGui::GetWindowDrawList()->AddImage(
                    icon,
                    { iMax.x - bH + pad, iMin.y + pad },
                    { iMax.x - pad,       iMax.y - pad },
                    { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 100));
            }
        }
    }

    ImGui::SameLine(0.f, refreshGap);
    if (GlassIconButton("refreshTop", "repeat", "R", str.refresh, { refreshBtnW, DS::ButtonHeight }))
        ctx.refreshList();

    ImGui::Spacing();

    // ── Estilo por defecto (solo Biblias: en Canciones ahora cada cancion
    //    tiene su propio preset, ver la tarjeta de ajustes en SongView) ──────
    RenderToolbarRow(ctx);

    ImGui::Spacing();

    // ── Vistas según categoría activa ───
    if (ctx.currentCategoryInt == kCat_Songs)
        RenderSongsAndPlaylistsGrid(ctx);
    else if (ctx.currentCategoryInt == kCat_Videos)
        RenderMediaFoldersPane(ctx, Core::CollectionMediaType::Video);
    else if (ctx.currentCategoryInt == kCat_Images)
        RenderMediaFoldersPane(ctx, Core::CollectionMediaType::Image);
    else if (ctx.currentCategoryInt == kCat_Audio)
        RenderMediaFoldersPane(ctx, Core::CollectionMediaType::Audio);
    else
        RenderItemsListPane(ctx);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// =============================================================================
//  SelectLibraryItem
//  Logica de seleccion compartida entre click en la lista y navegacion con
//  flechas arriba/abajo. Recibe el indice DENTRO de filteredItems (no el
//  indice original en ctx.items).
// =============================================================================
static void SelectLibraryItem(LibraryContext& ctx,
                              const std::vector<std::string>& filteredItems,
                              int n)
{
    if (n < 0 || n >= (int)filteredItems.size()) return;

    auto it = std::find(ctx.items.begin(), ctx.items.end(), filteredItems[n]);
    int origIdx = (it != ctx.items.end())
        ? (int)std::distance(ctx.items.begin(), it) : -1;

    ctx.selectedIndex = origIdx;

    Core::LibrarySelection s;
    s.title = filteredItems[n];
    switch (ctx.currentCategoryInt) {
        case kCat_Songs:     s.type = Core::ItemType::Song;      break;
        case kCat_Videos:    s.type = Core::ItemType::Video;     break;
        case kCat_Images:    s.type = Core::ItemType::Image;     break;
        case kCat_Bibles:    s.type = Core::ItemType::Bible;     break;
        case kCat_Documents: s.type = Core::ItemType::Documents; break;
        case kCat_Audio:     s.type = Core::ItemType::Audio;     break;
        default:             s.type = Core::ItemType::None;      break;
    }
    if (ctx.currentCategoryInt == kCat_Songs)
        s.contentData = ctx.loadSongVerses(filteredItems[n]);
    else if (ctx.currentCategoryInt == kCat_Documents) {
        fs::path docDir =
            U8Path(GetAssetsPath() + "/documents") / U8Path(filteredItems[n]);
        if (fs::exists(docDir) && fs::is_directory(docDir)) {
            std::vector<std::string> pages;
            for (const auto& pe : fs::directory_iterator(docDir))
                if (pe.is_regular_file())
                    pages.push_back(ProyecThor::Library::PathToUtf8(pe.path()));
            std::sort(pages.begin(), pages.end());
            s.contentData = pages;
        }
    }

    Core::PresentationCore::Get().SetSelection(s);
    ApplyDefaultStyleIfSet(ctx);
}

} // namespace ProyecThor::Library
