#include "LibrarySongs.h"
#include "LibrarySongMeta.h"
#include "LibraryModals.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/DesignSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "LibraryTags.h"

#include "LibraryPlaylists.h"
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

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;

static constexpr int kCat_Songs     = 0;
static constexpr int kCat_Videos    = 1;
static constexpr int kCat_Images    = 2;
static constexpr int kCat_Bibles    = 3;
static constexpr int kCat_Documents = 4;

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

std::vector<std::string> LoadSongVerses(const std::string& filename)
{
    std::vector<std::string> verses;
    std::ifstream file(U8Path(GetAssetsPath() + "/songs/" + filename),
                       std::ios::binary);
    if (!file.is_open()) {
        verses.push_back("Error: No se pudo abrir el archivo.\nRuta: " +
                         GetAssetsPath() + "/songs/" + filename);
        return verses;
    }

    std::string raw((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();
    if (raw.empty()) return verses;

    std::string content = NormalizeToUtf8(raw);
    const int linesPerSlide = GetSongMeta(filename).linesPerSlide;
    return GroupLyricsIntoSlides(content, linesPerSlide);
}

// =============================================================================
//  Autor de cancion — persistido aparte en songs_authors.ini
//  (mismo patron que category_styles.ini en PresentationCore). No se mete
//  dentro del .txt de la cancion para no romper el parseo por estrofas de
//  LoadSongVerses (separadas por linea en blanco).
// =============================================================================
static std::string SongAuthorsFilePath()
{
    return GetAssetsPath() + "/../songs_authors.ini";
}

static std::unordered_map<std::string, std::string> LoadSongAuthors()
{
    std::unordered_map<std::string, std::string> authors;
    std::ifstream f(U8Path(SongAuthorsFilePath()));
    if (!f.is_open()) return authors;

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        authors[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return authors;
}

void SetSongAuthor(const std::string& filename, const std::string& author)
{
    auto authors = LoadSongAuthors();
    if (author.empty()) authors.erase(filename);
    else                authors[filename] = author;

    std::ofstream f(U8Path(SongAuthorsFilePath()));
    if (!f.is_open()) return;
    for (const auto& [k, v] : authors)
        f << k << "=" << v << "\n";
}

std::string GetSongAuthor(const std::string& filename)
{
    auto authors = LoadSongAuthors();
    auto it = authors.find(filename);
    return it != authors.end() ? it->second : "";
}

// =============================================================================
//  Etiquetas de cancion — songs_tags.ini, mismo patron que songs_authors.ini.
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

static std::unordered_map<std::string, std::string> LoadSongTagsRaw()
{
    std::unordered_map<std::string, std::string> map;
    std::ifstream f(U8Path(SongTagsFilePath()));
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

    std::ofstream f(U8Path(SongTagsFilePath()));
    if (!f.is_open()) return;
    for (const auto& [k, v] : map)
        f << k << "=" << v << "\n";
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

std::string GetSongDisplayName(const std::string& filename)
{
    std::string title = GetSongMeta(filename).title;
    return !title.empty() ? title : StripExtension(filename);
}

void MigrateSongSidecars(const std::string& oldFilename, const std::string& newFilename)
{
    if (oldFilename.empty() || newFilename.empty() || oldFilename == newFilename) return;

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
//  SongListRow
//  Fila de lista para canciones, igual a DS::GlassListRow pero con soporte
//  para pintar el fondo con el color de la etiqueta asignada a la cancion.
//  Esto es lo que reemplaza a la vieja pestaña "Etiquetas": ahora la
//  etiqueta se ve DIRECTAMENTE en la lista de canciones, sin tener que
//  cambiar de pestaña para diferenciarlas.
// =============================================================================
// Pequeño helper porque DS::RadiusSmall * 0.5f se repite; evita magic number
// suelto en la funcion de abajo.
static inline float RadiusSmallLocal() { return DS::RadiusSmall * 0.5f; }

// trailingReserve: pixeles a dejar libres a la derecha de la fila SIN que
// el area clickeable de seleccion los cubra (para poder poner un control
// propio ahi encima, ej. el icono de creditos de Biblias — mismo criterio
// que "selectW" en RenderPlaylistsSection). El fondo (tinte/selección/hover)
// sigue pintando el ancho COMPLETO de la fila, solo se achica el
// InvisibleButton de seleccion.
static bool SongListRow(const char* label, bool selected,
                        ImVec4 tagColor, bool hasTag,
                        float indent = 14.0f, float height = DS::RowHeight,
                        float trailingReserve = 0.0f)
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
    // Se dibuja PRIMERO, como base, para que la seleccion/hover puedan
    // superponerse encima sin perder la referencia de color.
    if (hasTag) {
        float alpha = selected ? 0.38f : (hovered ? 0.30f : 0.20f);
        ImU32 tagBg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(tagColor.x, tagColor.y, tagColor.z, alpha));
        dl->AddRectFilled(rMin, rMax, tagBg, RadiusSmallLocal());

        // Barra lateral con el color solido de la etiqueta: se nota incluso
        // cuando el fondo de seleccion/hover queda encima.
        dl->AddRectFilled(rMin, ImVec2(rMin.x + 3.0f, rMax.y),
                          ImGui::ColorConvertFloat4ToU32(tagColor), 1.5f);
    }

    // ── Fondo de seleccion / hover (igual que GlassListRow) ─────────────────
    if (selected) {
        // Plano: un solo tono en vez del degrade de 4 colores ("liquid glass").
        dl->AddRectFilled(rMin, rMax, IM_COL32(99, 112, 255, 42));

        dl->AddRectFilled(rMin, ImVec2(rMin.x + 3.0f, rMax.y), DS::RowSelectedBar, 1.5f);

        dl->AddLine(
            ImVec2(rMin.x + 4.0f, rMax.y - 0.5f),
            ImVec2(rMax.x,        rMax.y - 0.5f),
            IM_COL32(99, 112, 255, 40), 1.0f);
    } else if (hovered && !hasTag) {
        // Si ya hay un tinte de etiqueta, el hover no dibuja encima (para no
        // ensuciar el color); el resaltado ya se nota por el aumento de
        // alpha del tinte de arriba.
        dl->AddRectFilled(rMin, rMax, DS::RowHoverFill, DS::RadiusSmall * 0.5f);
        dl->AddRect(rMin, rMax, IM_COL32(255, 255, 255, 18), DS::RadiusSmall * 0.5f, 0, 0.5f);
    }

    // ── Texto ────────────────────────────────────────────────────────────────
    ImFont* font   = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();
    ImVec2 textSz  = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);

    float textX = rMin.x + indent;
    float textY = rMin.y + std::floor((height - textSz.y) * 0.5f);

    ImU32 textCol = selected ? DS::TextPrimary : DS::TextSecondary;
    dl->AddText(ImVec2(textX, textY), textCol, label);

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
                ImU32 bg = isActive ? IM_COL32(99, 112, 255, 42)
                         : isHovered ? DS::RowHoverFill
                         : IM_COL32(0, 0, 0, 0);

                dl->AddRectFilled(p_min, p_max, bg, DS::RadiusSmall * 0.5f);
                if (isHovered && !isActive)
                    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 18), DS::RadiusSmall * 0.5f, 0, 0.5f);
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
    if (ImGui::BeginPopupModal("NuevaPlaylistModal##lib", &showNewModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted(str.libPlaylistNameLabel);
        ImGui::SetNextItemWidth(320.f);
        ImGui::InputText("##newPlName", nameBuffer, sizeof(nameBuffer));
        ImGui::Spacing();
        if (DS::GlassButton(str.libCreateBtn, { 150.f, 34.f })) {
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
    if (ImGui::BeginPopupModal("RenombrarPlaylistModal##lib", &showRenameModal,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted(str.libNewNameLabel);
        ImGui::SetNextItemWidth(320.f);
        ImGui::InputText("##renPlName", nameBuffer, sizeof(nameBuffer));
        ImGui::Spacing();
        if (DS::GlassButton(str.libRename, { 150.f, 34.f })) {
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
        matches.reserve(ctx.items.size());
        for (const auto& item : ctx.items)
        {
            if (!q.empty())
            {
                std::string title = GetSongDisplayName(item);
                std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c){ return (char)::tolower(c); });

                bool match = title.find(q) != std::string::npos;

                if (!match) {
                    std::string author = GetSongAuthor(item);
                    std::transform(author.begin(), author.end(), author.begin(), [](unsigned char c){ return (char)::tolower(c); });
                    match = author.find(q) != std::string::npos;
                }

                if (!match) continue;
            }
            matches.push_back(item);
        }
        std::sort(matches.begin(), matches.end(), [](const std::string& a, const std::string& b) {
            std::string ta = GetSongDisplayName(a), tb = GetSongDisplayName(b);
            std::transform(ta.begin(), ta.end(), ta.begin(), [](unsigned char c){ return (char)::tolower(c); });
            std::transform(tb.begin(), tb.end(), tb.begin(), [](unsigned char c){ return (char)::tolower(c); });
            return ta < tb;
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
            const float rowH        = 38.0f;
            const float actionZoneW = 96.0f;
            const ImVec2 addBtnSize(26.f, 26.f);
            const float lineH       = ImGui::GetTextLineHeight();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 3.f));

            for (const auto& item : matches)
            {
                ImGui::PushID(item.c_str());

                bool already = std::find(currentSongs.begin(), currentSongs.end(), item) != currentSongs.end();

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

                std::string title  = GetSongDisplayName(item);
                std::string author = GetSongAuthor(item);

                dl->PushClipRect(p_min, { p_min.x + clickW - 8.f, p_max.y }, true);
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
                ImGui::PopID();
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
    static std::string lastSearch;

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
        std::transform(cur.begin(), cur.end(), cur.begin(), [](unsigned char c){ return (char)::tolower(c); });

        const bool shouldRebuild = (cur != lastSearch) || ForceListUpdate();
        if (shouldRebuild) {
            filteredItems.clear();
            for (const auto& item : ctx.items) {
                bool match = true;
                if (!cur.empty()) {
                    std::string lo = item;
                    std::transform(lo.begin(), lo.end(), lo.begin(), [](unsigned char c){ return (char)::tolower(c); });
                    match = lo.find(cur) != std::string::npos;
                    if (!match && ctx.currentCategoryInt == kCat_Songs) {
                        std::string title = GetSongDisplayName(item);
                        std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c){ return (char)::tolower(c); });
                        if (title.find(cur) != std::string::npos) match = true;
                    }
                    if (!match && ctx.currentCategoryInt == kCat_Songs) {
                        std::string author = GetSongAuthor(item);
                        std::transform(author.begin(), author.end(), author.begin(), [](unsigned char c){ return (char)::tolower(c); });
                        if (author.find(cur) != std::string::npos) match = true;
                    }
                    if (!match && ctx.currentCategoryInt == kCat_Songs) {
                        for (const auto& t : ctx.getSongTags(item)) {
                            std::string tl = t;
                            std::transform(tl.begin(), tl.end(), tl.begin(), [](unsigned char c){ return (char)::tolower(c); });
                            if (tl.find(cur) != std::string::npos) { match = true; break; }
                        }
                    }
                    if (!match && ctx.currentCategoryInt == kCat_Songs) {
                        for (const auto& v : ctx.loadSongVerses(item)) {
                            std::string vl = v;
                            std::transform(vl.begin(), vl.end(), vl.begin(), [](unsigned char c){ return (char)::tolower(c); });
                            if (vl.find(cur) != std::string::npos) { match = true; break; }
                        }
                    }
                }

                if (match) filteredItems.push_back(item);
            }
            lastSearch = cur;
            ForceListUpdate() = false;
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
        // Se activa solo cuando la ventana raiz del panel de Biblioteca tiene
        // el foco y no hay ningun campo de texto activo (buscador, renombrar,
        // etc.), para no robarle las flechas a esos inputs.
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

        for (int n = 0; n < (int)filteredItems.size(); ++n)
        {
            auto it = std::find(ctx.items.begin(), ctx.items.end(), filteredItems[n]);
            int origIdx = (it != ctx.items.end())
                ? (int)std::distance(ctx.items.begin(), it) : -1;

            const bool sel   = (ctx.selectedIndex == origIdx);
            std::string disp = (ctx.currentCategoryInt == kCat_Songs)
                ? GetSongDisplayName(filteredItems[n])
                : StripExtension(filteredItems[n]);

            if (ctx.currentCategoryInt == kCat_Songs)
            {
                std::string author = GetSongAuthor(filteredItems[n]);
                if (!author.empty())
                    disp += "  —  " + author;
            }

            // ── Color de fondo por etiqueta ──────────────────────────────────
            ImVec4 tagColor(0.f, 0.f, 0.f, 0.f);
            bool   hasTag = false;
            if (ctx.currentCategoryInt == kCat_Songs) {
                auto songTags = ctx.getSongTags(filteredItems[n]);
                if (!songTags.empty()) {
                    auto grpIt = std::find_if(tagGroups.begin(), tagGroups.end(),
                        [&](const SongTagGroup& g){ return g.id == songTags[0]; });
                    if (grpIt != tagGroups.end()) {
                        tagColor = grpIt->color;
                        hasTag   = true;
                    }
                }
            }

            ImGui::PushID(n);
            bool clicked = SongListRow(disp.c_str(), sel, tagColor, hasTag,
                                       14.0f, DS::RowHeight, 0.0f);

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

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = 10.0f;
        const float bw3   = std::floor((avail - sp * 2.0f) / 3.0f);
        const ImVec2 btnSize(bw3, DS::ButtonHeight);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(sp, sp));

        if (GlassIconButton("newSong", "add", "+", str.newLabel, btnSize))
            CreateNewSong(ctx);
        ImGui::SameLine();
        if (GlassIconButton("importSong", "upload_file", "^", str.importLabel, btnSize))
            ctx.importFile();
        ImGui::SameLine();
        if (GlassIconButton("deleteSong", "delete", "X", str.deleteLabel, btnSize,
                            ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
            ctx.deleteSelectedItem();

        ImGui::PopStyleVar();
    }
}
// =============================================================================
//  RenderSongsAndPlaylistsGrid
//  Muestra Canciones y Playlists a la vez, divididas en grid horizontal
//  (columna izquierda / derecha) cuando hay ancho suficiente, o apiladas
//  verticalmente en paneles angostos. Cada columna tiene margen interno
//  propio (WindowPadding) y estan separadas por una linea sutil, para que
//  ninguna de las dos ocupe mas espacio del que corresponde.
// =============================================================================
static void RenderSongsAndPlaylistsGrid(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    // FIX: antes se mostraban Canciones y Playlists a la vez (en columnas o
    // apiladas), lo que le dejaba poco espacio real a cada lista. Ahora es
    // un toggle tipo pestaña: una sola lista visible por vez, con TODO el
    // ancho/alto del panel para ella.
    static bool showPlaylists = false;

    // ── Selector Canciones / Playlists ───────────────────────────────────
    {
        const float tabH = 30.0f;
        const float gap  = 6.0f;
        const float tabW = std::floor((ImGui::GetContentRegionAvail().x - gap) * 0.5f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

        // Botón "Canciones"
        {
            ImVec4 fill = ImGui::ColorConvertU32ToFloat4(!showPlaylists ? DS::AccentColorDim : DS::BtnDefaultFill);
            ImVec4 hov  = ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill);
            ImVec4 text = ImGui::ColorConvertU32ToFloat4(!showPlaylists ? DS::TextPrimary : DS::TextSecondary);
            ImGui::PushStyleColor(ImGuiCol_Button,        fill);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  hov);
            ImGui::PushStyleColor(ImGuiCol_Text,          text);
            std::string songsLabel = std::string(str.libCatSongs) + "  (" + std::to_string(ctx.items.size()) + ")";
            if (ImGui::Button(songsLabel.c_str(), { tabW, tabH }))
                showPlaylists = false;
            ImGui::PopStyleColor(4);
        }

        ImGui::SameLine(0.f, gap);

        // Botón "Playlists"
        {
            ImVec4 fill = ImGui::ColorConvertU32ToFloat4(showPlaylists ? DS::AccentColorDim : DS::BtnDefaultFill);
            ImVec4 hov  = ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill);
            ImVec4 text = ImGui::ColorConvertU32ToFloat4(showPlaylists ? DS::TextPrimary : DS::TextSecondary);
            ImGui::PushStyleColor(ImGuiCol_Button,        fill);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  hov);
            ImGui::PushStyleColor(ImGuiCol_Text,          text);
            std::string plLabel = std::string(str.libPlaylistsTab) + "  (" + std::to_string(ctx.listPlaylists().size()) + ")";
            if (ImGui::Button(plLabel.c_str(), { tabW, tabH }))
                showPlaylists = true;
            ImGui::PopStyleColor(4);
        }

        ImGui::PopStyleVar();
    }

    ImGui::Spacing();

    const float contentH = ImGui::GetContentRegionAvail().y;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));

    if (!showPlaylists)
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

    ForceListUpdate() = true;
    return true;
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

    // ── Barra de búsqueda + boton de actualizar (mismo renglon: antes el
    //    refresh vivia en su propio renglon abajo, junto al combo de estilo
    //    por defecto) ───────────────────────────────────────────────────────
    const float refreshBtnW = DS::ButtonHeight;
    const float refreshGap  = 8.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-(refreshBtnW + refreshGap));
    if (ImGui::InputTextWithHint("##search", str.libSearchHint,
                                 ctx.searchBuffer, ctx.searchBufferSize))
        ForceListUpdate() = true;
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    // Ícono de búsqueda superpuesto
    if (StyleGeneralApp::Icons.count("search") > 0) {
        void* icon = StyleGeneralApp::Icons["search"].textureID;
        if (icon) {
            constexpr float pad = 5.0f;
            ImVec2 iMin = ImGui::GetItemRectMin();
            ImVec2 iMax = ImGui::GetItemRectMax();
            float  bH   = iMax.y - iMin.y;
            ImGui::GetWindowDrawList()->AddImage(
                icon,
                { iMax.x - bH + pad, iMin.y + pad },
                { iMax.x - pad,       iMax.y - pad },
                { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 100));
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

    // ── Canciones: se muestran junto a Playlists en un grid, sin pestañas ───
    if (ctx.currentCategoryInt == kCat_Songs)
        RenderSongsAndPlaylistsGrid(ctx);
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
