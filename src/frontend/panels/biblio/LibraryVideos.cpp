#include "LibraryVideos.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "LibraryVideoPreview.h"
#include "ui/DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/panels/layers/LayersTheme.h"
#include "backend/core/ThumbnailWorker.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "backend/core/PresentationCore.h"
#include "monitor/MonitorView.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <functional>
#include <system_error>
#include <GL/gl.h>
#include "stb_image.h"

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  GlassIconButton — boton con icono de StyleGeneralApp (fallback a glifo corto)
//  Mismo helper que en LibrarySongs.cpp: reemplaza texto largo ("Importar",
//  "Actualizar", "Eliminar", etc.) por iconos + tooltip.
//
//  FIX (tamaños): antes el icono se recortaba con el mismo "pad" en X e Y,
//  lo que en botones anchos y bajos (como los del footer, ancho/3) dejaba
//  un rectangulo horizontal en vez de un icono cuadrado -> se veia
//  "estirado"/deforme. Ahora se calcula un cuadrado a partir del lado MENOR
//  del boton y se centra, sin importar que tan ancho o bajo sea el boton.
// =============================================================================
static bool GlassIconButton(const char* id,
                             const char* iconKey,
                             const char* fallbackGlyph,
                             const char* tooltip,
                             ImVec2      size,
                             ImVec4      tint = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary))
{
    // Exactamente 4 PushStyleColor
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon) {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();

        // Cuadrado centrado, basado en el lado MENOR del boton (no estira).
        const float minSide  = std::min(size.x, size.y);
        const float iconSide = minSide * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        ImGui::GetWindowDrawList()->AddImage(
            it->second.textureID,
            pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }

    // Exactamente 4 PopStyleColor (y se eliminó el PopStyleVar huérfano)
    ImGui::PopStyleColor(4);

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// =============================================================================
//  Miniaturas de video (grid/lista) — mismo patron que LayersBgTab (Fondos,
//  ver LayersBgTab.cpp): el primer frame real se decodifica en 2do plano via
//  ThumbnailWorker (libVLC en modo callback puro, nunca abre ventana propia)
//  y se cachea en disco, asi que solo se paga el costo de decodificar una vez
//  por video en la vida de la instalacion. Cache privado de este tab, igual
//  criterio que Overlays/Fondos (cada uno con el suyo, no compartido).
// =============================================================================
static fs::path ThumbCacheDir() {
    fs::path dir = fs::path(GetAssetsPath()) / "thumbnails";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// Hash de path+tamaño: si el archivo se reemplaza por otro con el mismo
// nombre pero distinto contenido, se regenera en vez de mostrar para
// siempre la miniatura vieja.
static std::string ThumbCachePathFor(const std::string& absVideoPath) {
    std::error_code ec;
    auto sz = fs::file_size(absVideoPath, ec);
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return (ThumbCacheDir() / (std::to_string(h) + ".png")).string();
}

static ImTextureID LoadImageThumb(const char* path) {
    int w, h, n;
    unsigned char* d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    return (ImTextureID)(intptr_t)tex;
}

static std::unordered_map<std::string, ImTextureID> s_ThumbnailCache;
static ProyecThor::Core::ThumbnailWorker            s_ThumbWorker;
static bool  s_GridMode  = true;
static float s_ThumbZoom = 1.0f;

static ImTextureID GetVideoThumbnail(const std::string& path) {
    auto it = s_ThumbnailCache.find(path);
    if (it != s_ThumbnailCache.end()) return it->second;

    std::string abs = fs::absolute(fs::path(path)).string();

    // Ya generada en una sesion anterior: cargarla del cache de disco es
    // instantaneo (una imagen mas, via LoadImageThumb) y no toca el worker.
    std::string cachePath = ThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(cachePath, ec)) {
        ImTextureID t = LoadImageThumb(cachePath.c_str());
        if (t) { s_ThumbnailCache[path] = t; return t; }
    }

    // Todavia no existe: se pide en 2do plano y por ahora se deja SIN
    // entrar en cache — el proximo frame vuelve a preguntar, y cuando el
    // worker termine, DrainVideoThumbnails() ya habra puesto el resultado
    // real. Mientras tanto la tarjeta/fila cae en el icono generico "VID".
    s_ThumbWorker.Request(path, abs, cachePath);
    return 0;
}

// Llamar una vez por frame: sube a textura GL los frames que el worker haya
// terminado de decodificar desde el ultimo frame.
static void DrainVideoThumbnails() {
    std::vector<ProyecThor::Core::ThumbnailWorker::Result> results;
    s_ThumbWorker.DrainResults(results);
    for (auto& r : results) {
        ImTextureID t = 0;
        if (!r.pixels.empty() && r.width > 0 && r.height > 0) {
            GLuint tex; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)r.width, (GLsizei)r.height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, r.pixels.data());
            t = (ImTextureID)(intptr_t)tex;
        }
        // Si fallo (t == 0) igual entra en cache: evita reintentar sin fin
        // un archivo que no se puede decodificar.
        s_ThumbnailCache[r.key] = t;
    }
}

static std::string VideoFullPath(const std::string& filename) {
    if (std::filesystem::path(filename).is_absolute()) return filename;
    return GetAssetsPath() + "/videos/" + filename;
}

// =============================================================================
//  Menu contextual compartido entre fila (lista) y tarjeta (grid) — mismas 3
//  opciones que ya existian en el loop original.
//  Devuelve true si se elimino el item: en ese caso YA se llamo EndPopup()
//  (antes de deleteSelectedItem(), que refresca ctx.items) y el caller debe
//  cortar el loop sobre `filtered` sin llamar EndPopup() de nuevo.
// =============================================================================
static bool RenderVideoContextMenu(LibraryContext& ctx, const std::string& filename, int origIdx)
{
    if (ImGui::MenuItem("Enviar al monitor")) {
        std::string fp = VideoFullPath(filename);
        // FIX: no forzar un Stop() (corte a negro) antes de
        // SetBackgroundMedia() — SetVideo() ya maneja tanto la carga en
        // frio como el crossfade sobre lo que esta al aire.
        Core::PresentationCore::Get().SetBackgroundMedia(fp, true, /*allowAudio=*/true);
        Core::PresentationCore::Get().SetProjecting(true);
    }
    if (ImGui::MenuItem("Ver en pantalla completa") && origIdx >= 0) {
        OpenVideoPreview(ctx, origIdx);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Mover a Fondos (Backgrounds)")) {
        std::error_code ec;
        std::string srcPath = VideoFullPath(filename);
        fs::path src(srcPath);
        std::string targetDir = GetAssetsPath() + "/backgrounds";
        fs::create_directories(targetDir, ec);
        fs::path dst = fs::path(targetDir) / src.filename();
        fs::rename(src, dst, ec);
        ctx.refreshList();
    }
    if (ImGui::MenuItem("Copiar a Fondos (Backgrounds)")) {
        std::error_code ec;
        std::string srcPath = VideoFullPath(filename);
        fs::path src(srcPath);
        std::string targetDir = GetAssetsPath() + "/backgrounds";
        fs::create_directories(targetDir, ec);
        fs::path dst = fs::path(targetDir) / src.filename();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Renombrar")) {
        ctx.renameOldName  = filename;
        ctx.renameIsURL    = false;
        ctx.renameURLIndex = -1;
        ctx.selectedIndex  = origIdx;
        std::string stem = SplitExtension(filename, ctx.renameExtension);
        memset(ctx.renameBuffer, 0, 512);
        strncpy(ctx.renameBuffer, stem.c_str(), 511);
        ctx.showRenameModal = true;
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
    if (ImGui::MenuItem("Eliminar")) {
        ImGui::PopStyleColor();
        ctx.selectedIndex = origIdx;
        ImGui::EndPopup();
        ctx.deleteSelectedItem();
        ForceListUpdate() = true;
        return true;
    }
    ImGui::PopStyleColor();
    ImGui::EndPopup();
    return false;
}

// =============================================================================
//  RenderVideoRow — fila de lista (mismo look que antes, DS::GlassListRow)
//  con una miniatura chica agregada a la izquierda.
// =============================================================================
static void RenderVideoRow(LibraryContext& ctx, const std::string& filename, int origIdx,
                           bool& deletedInLoop, int rowIdx)
{
    ImGui::PushID(rowIdx);

    const float thumbSz = 22.0f;
    const float indent  = thumbSz + 16.0f;

    ImTextureID thumb = GetVideoThumbnail(VideoFullPath(filename));
    ImVec2      rowPos = ImGui::GetCursorScreenPos();
    bool        sel    = (ctx.selectedIndex == origIdx);
    std::string disp   = StripExtension(filename);

    bool clicked = DS::GlassListRow(disp.c_str(), sel, indent);

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       thumbY = rowPos.y + (DS::RowHeight - thumbSz) * 0.5f;
    if (thumb) {
        dl->AddImageRounded(thumb, {rowPos.x + 8.0f, thumbY},
                            {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                            {0,0}, {1,1}, IM_COL32_WHITE, DS::RadiusSmall);
    } else {
        dl->AddRectFilled({rowPos.x + 8.0f, thumbY},
                          {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                          DS::BtnDefaultFill, DS::RadiusSmall);
        ImVec2 ts = ImGui::CalcTextSize("V");
        dl->AddText({rowPos.x + 8.0f + (thumbSz-ts.x)*0.5f, thumbY + (thumbSz-ts.y)*0.5f},
                    DS::TextSecondary, "V");
    }

    if (clicked && origIdx >= 0) {
        ctx.selectedIndex = origIdx;
        Core::LibrarySelection s;
        s.title = VideoFullPath(filename);
        s.type  = Core::ItemType::Video;
        Core::PresentationCore::Get().SetSelection(s);
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        std::string fullPath = VideoFullPath(filename);
        ImGui::SetDragDropPayload("VIDEO_TO_QUEUE", fullPath.c_str(), fullPath.size() + 1);
        ImGui::SetDragDropPayload("MEDIA_ITEM_PATH", fullPath.c_str(), fullPath.size() + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
        ImGui::TextUnformatted(disp.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginPopupContextItem(("##ctx_lv" + std::to_string(rowIdx)).c_str())) {
        if (RenderVideoContextMenu(ctx, filename, origIdx))
            deletedInLoop = true;
    }

    ImGui::PopID();
}

// =============================================================================
//  RenderVideoCard — tarjeta de grid con miniatura grande (mismo lenguaje
//  visual que LayersBgTab::RenderBgCard, adaptado a los tokens DS:: que usa
//  el resto de Biblioteca).
// =============================================================================
static void RenderVideoCard(LibraryContext& ctx, const std::string& filename, int origIdx,
                            float W, float H, int col, int cols, bool& deletedInLoop, int cardIdx)
{
    ImGui::PushID(cardIdx);

    ImTextureID thumb  = GetVideoThumbnail(VideoFullPath(filename));
    ImVec2      pos    = ImGui::GetCursorScreenPos();
    bool        hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    float       t       = UI::LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    bool        sel     = (ctx.selectedIndex == origIdx);

    float  inset = 2.0f * t;
    ImVec2 p0    = { pos.x - inset, pos.y - inset };
    ImVec2 p1    = { pos.x + W + inset, pos.y + H + inset };

    dl->AddRectFilled(p0, p1, DS::BtnDefaultFill, DS::RadiusMedium);
    if (thumb)
        dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32_WHITE, DS::RadiusMedium);

    ImVec4 borderA = ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord);
    ImVec4 borderB = ImGui::ColorConvertU32ToFloat4(sel ? DS::AccentColor : DS::AccentColorHov);
    float  bt      = sel ? 1.0f : t;
    ImVec4 borderCol(
        borderA.x + (borderB.x - borderA.x) * bt,
        borderA.y + (borderB.y - borderA.y) * bt,
        borderA.z + (borderB.z - borderA.z) * bt,
        borderA.w + (borderB.w - borderA.w) * bt);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(borderCol), DS::RadiusMedium, 0, 1.0f + 0.8f*bt);

    // Chip "VID"
    {
        ImVec2 ts = ImGui::CalcTextSize("VID");
        float  bx = p0.x + 7.0f, by = p0.y + 7.0f;
        dl->AddRectFilled({bx, by}, {bx+ts.x+8.0f, by+ts.y+4.0f}, DS::AccentColorDim, DS::RadiusSmall);
        dl->AddText({bx+4.0f, by+2.0f}, DS::AccentLight, "VID");
    }

    std::string disp = StripExtension(filename);
    std::string dn   = disp.length() > 18 ? disp.substr(0,15) + "..." : disp;
    dl->AddRectFilled({p0.x, p1.y-26.0f}, {p1.x, p1.y}, IM_COL32(0,0,0,200), DS::RadiusMedium, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, DS::TextPrimary, dn.c_str());

    ImGui::SetNextItemAllowOverlap(); // deja que el boton de preview de abajo, dibujado encima, reciba su propio click
    ImGui::InvisibleButton(("##vidcard" + std::to_string(cardIdx)).c_str(), {W, H});

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && origIdx >= 0) {
        ctx.selectedIndex = origIdx;
        Core::LibrarySelection s;
        s.title = VideoFullPath(filename);
        s.type  = Core::ItemType::Video;
        Core::PresentationCore::Get().SetSelection(s);
    }

    // Boton "ver en pantalla completa" -- centrado sobre la miniatura, solo
    // visible al pasar el mouse (mismo hover t que ya se calcula arriba
    // para el borde). Ver LibraryVideoPreview.h.
    if (t > 0.01f && origIdx >= 0) {
        const float playSz = 40.0f;
        ImGui::SetCursorScreenPos({ pos.x + (W - playSz) * 0.5f, pos.y + (H - playSz) * 0.5f });
        if (GlassIconButton(("pvopen" + std::to_string(cardIdx)).c_str(), "play", ">",
                             "Ver en pantalla completa", { playSz, playSz }))
        {
            OpenVideoPreview(ctx, origIdx);
        }
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        std::string fullPath = VideoFullPath(filename);
        ImGui::SetDragDropPayload("VIDEO_TO_QUEUE", fullPath.c_str(), fullPath.size() + 1);
        ImGui::SetDragDropPayload("MEDIA_ITEM_PATH", fullPath.c_str(), fullPath.size() + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
        ImGui::TextUnformatted(dn.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginPopupContextItem(("##ctx_vc" + std::to_string(cardIdx)).c_str())) {
        if (RenderVideoContextMenu(ctx, filename, origIdx))
            deletedInLoop = true;
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// =============================================================================
//  RenderVideoSection — tabs Archivos / Stream
// =============================================================================
void RenderVideoSection(LibraryContext& ctx)
{
    static constexpr ImVec4 k_Tab      = { 0.06f, 0.06f, 0.11f, 1.00f };
    static constexpr ImVec4 k_TabHov   = { 0.12f, 0.14f, 0.24f, 1.00f };
    static constexpr ImVec4 k_TabSel   = { 0.14f, 0.24f, 0.60f, 1.00f };
    static constexpr ImVec4 k_TabSelTx = { 0.80f, 0.90f, 1.00f, 1.00f };
    static constexpr ImVec4 k_TabTx    = { 0.44f, 0.48f, 0.62f, 1.00f };

    ImGui::PushStyleColor(ImGuiCol_Tab,                k_Tab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         k_TabHov);
    ImGui::PushStyleColor(ImGuiCol_TabActive,          k_TabSel);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       k_Tab);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, k_TabSel);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding,  8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));

    if (ImGui::BeginTabBar("##video_tabs"))
    {
        bool tab1Active = (ImGui::GetCurrentTabBar()->SelectedTabId
                           == ImGui::GetID("Archivos##vt"));
        ImGui::PushStyleColor(ImGuiCol_Text, tab1Active ? k_TabSelTx : k_TabTx);
        if (ImGui::BeginTabItem("Archivos##vt")) {
            ImGui::PopStyleColor();
            ImGui::Spacing();
            RenderLocalVideoList(ctx);
            ImGui::EndTabItem();
        } else { ImGui::PopStyleColor(); }

        ImGui::PushStyleColor(ImGuiCol_Text, k_TabTx);
        if (ImGui::BeginTabItem("Stream##vt")) {
            ImGui::PopStyleColor();
            ImGui::Spacing();
            RenderStreamURLSection(ctx);
            ImGui::EndTabItem();
        } else { ImGui::PopStyleColor(); }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    // Se dibuja siempre, sin importar la pestaña activa -- tiene que poder
    // seguir mostrandose/cerrandose aunque el operador cambie de pestaña
    // mientras el preview a pantalla completa esta abierto.
    RenderVideoPreviewOverlay(ctx);
}

// =============================================================================
//  RenderLocalVideoList — mismo lenguaje glass que RenderSideList (canciones)
// =============================================================================
void RenderLocalVideoList(LibraryContext& ctx)
{
    // Sube a textura GL las miniaturas que el worker en 2do plano haya
    // terminado de decodificar desde el frame anterior (ver GetVideoThumbnail).
    DrainVideoThumbnails();

    // ── Barra de búsqueda ──────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::InputTextWithHint("##vsearch", "Buscar video...",
                                 ctx.searchBuffer, ctx.searchBufferSize))
        ForceListUpdate() = true;
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // ── Barra de vista: zoom (solo en grid) + alternar grid/lista ───────────
    {
        const float btnSz = 26.0f;
        const float zoomW = 76.0f;
        const float gap   = 4.0f;
        const float rowW  = zoomW + gap + btnSz*2.0f + gap*2.0f;
        const float avail = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));

        if (s_GridMode) {
            UI::LPZoomSlider("##vidzoom", &s_ThumbZoom, 0.65f, 1.8f, zoomW);
            ImGui::SameLine(0, gap);
        } else {
            ImGui::Dummy(ImVec2(zoomW, btnSz));
            ImGui::SameLine(0, gap);
        }

        ImGui::PushID("vidview");
        if (UI::LPCornerIconBtn("##gridm", UI::LPDrawGrid, "Vista en cuadrícula", {btnSz, btnSz}, s_GridMode))
            s_GridMode = true;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##listm", UI::LPDrawList, "Vista en lista", {btnSz, btnSz}, !s_GridMode))
            s_GridMode = false;
        ImGui::PopID();
    }

    ImGui::Spacing();

    // ── Espacio reservado para el footer (solo iconos: Importar | Actualizar | Eliminar) ─
    const float itemSpY   = ImGui::GetStyle().ItemSpacing.y;
    const float reservedH = DS::ButtonHeight + itemSpY * 2.0f + 6.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("##local_vid", { 0.f, -reservedH }, true))
    {
        static std::vector<std::string> filtered;
        static std::string lastQ;
        std::string cur(ctx.searchBuffer);
        std::transform(cur.begin(), cur.end(), cur.begin(),
                       [](unsigned char c){ return (char)::tolower(c); });

        if (cur != lastQ || ForceListUpdate()) {
            filtered.clear();
            for (const auto& item : ctx.items) {
                std::string lo = item;
                std::transform(lo.begin(), lo.end(), lo.begin(),
                               [](unsigned char c){ return (char)::tolower(c); });
                if (cur.empty() || lo.find(cur) != std::string::npos)
                    filtered.push_back(item);
            }
            lastQ             = cur;
            ForceListUpdate() = false;
        }

        if (filtered.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos({
                std::floor(avail.x * 0.5f - 70.f),
                std::floor(avail.y * 0.5f - 10.f) });
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted("Sin archivos de video");
            ImGui::PopStyleColor();
        }

        bool deletedInLoop = false;

        if (s_GridMode) {
            const float cW = 150.0f * s_ThumbZoom;
            const float cH = 92.0f  * s_ThumbZoom;
            const float minGap = 10.0f;

            float availWidth = ImGui::GetContentRegionAvail().x;
            int   cols       = std::max(1, static_cast<int>((availWidth + minGap) / (cW + minGap)));
            int   currentCol = 0;

            for (int n = 0; n < (int)filtered.size(); n++)
            {
                auto it2 = std::find(ctx.items.begin(), ctx.items.end(), filtered[n]);
                int origIdx = (it2 != ctx.items.end())
                    ? (int)std::distance(ctx.items.begin(), it2) : -1;

                RenderVideoCard(ctx, filtered[n], origIdx, cW, cH, currentCol, cols, deletedInLoop, n);
                if (deletedInLoop) break;

                currentCol++;
                if (currentCol < cols) {
                    ImGui::SameLine(0.0f, minGap);
                } else {
                    currentCol = 0;
                    ImGui::Dummy({0.0f, minGap});
                }
            }
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));

            for (int n = 0; n < (int)filtered.size(); n++)
            {
                auto it2 = std::find(ctx.items.begin(), ctx.items.end(), filtered[n]);
                int origIdx = (it2 != ctx.items.end())
                    ? (int)std::distance(ctx.items.begin(), it2) : -1;

                RenderVideoRow(ctx, filtered[n], origIdx, deletedInLoop, n);
                if (deletedInLoop) break;
            }

            ImGui::PopStyleVar(); // ItemSpacing
        }

        if (ImGui::BeginDragDropTarget()) {
            auto HandleDrop = [&](const ImGuiPayload* payload) {
                const char* droppedPath = (const char*)payload->Data;
                if (droppedPath && *droppedPath) {
                    std::error_code ec;
                    fs::path src(droppedPath);
                    std::string targetDir = GetAssetsPath() + "/videos";
                    fs::create_directories(targetDir, ec);
                    fs::path dst = fs::path(targetDir) / src.filename();
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                    ctx.refreshList();
                }
            };
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BG_FILE")) {
                HandleDrop(payload);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BG_ITEM_PATH")) {
                HandleDrop(payload);
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Footer con botones — solo iconos, universales, con tooltip ─────────
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float bw3   = std::floor((avail - sp * 2.0f) / 3.0f);
        const ImVec2 btnSize(bw3, DS::ButtonHeight);

        if (GlassIconButton("importVid", "upload_file", "^", "Importar", btnSize)) {
            ctx.importFile();
            ForceListUpdate() = true;
        }
        ImGui::SameLine();
        // NOTA: no existe "refresh.png" en assets/icons/ui, se usa "repeat"
        // (icono ciclico, ya cargado) que visualmente cumple la misma funcion.
        if (GlassIconButton("refreshVid", "repeat", "R", "Actualizar", btnSize)) {
            ctx.refreshList();
            ForceListUpdate() = true;
        }
        ImGui::SameLine();

        // Conversión segura de ImU32 a ImVec4 para evitar el error de tipos en los parámetros
        if (GlassIconButton("deleteVid", "delete", "X", "Eliminar", btnSize, ImGui::ColorConvertU32ToFloat4(DS::DangerColor))) {
            ctx.deleteSelectedItem();
            ForceListUpdate() = true;
        }
    }
}

// =============================================================================
//  RenderStreamURLSection — mismo lenguaje glass
// =============================================================================
void RenderStreamURLSection(LibraryContext& ctx)
{
    // PushStyleColor también acepta ImU32 directamente, o lo convertimos por seguridad
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::DangerColor));
    ImGui::TextWrapped("Pega una URL de YouTube, Twitch o cualquier stream HTTP/RTSP.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    bool pressEnter = ImGui::InputTextWithHint(
        "##url_in", "https://www.youtube.com/watch?v=...",
        ctx.urlInputBuffer, ctx.urlInputBufferSize,
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float sp    = ImGui::GetStyle().ItemSpacing.x;
        const float bw2   = std::floor((avail - sp) * 0.5f);
        const ImVec2 btnSize(bw2, DS::ButtonHeight);

        bool doAdd = pressEnter ||
            GlassIconButton("addUrl", "add", "+", "Agregar URL", btnSize);

        if (doAdd) {
            std::string url(ctx.urlInputBuffer);
            if (!url.empty() && url.rfind("http", 0) == 0) {
                if (std::find(ctx.streamURLs.begin(), ctx.streamURLs.end(), url)
                    == ctx.streamURLs.end()) {
                    ctx.streamURLs.push_back(url);
                    ctx.saveStreamURLs();
                }
                memset(ctx.urlInputBuffer, 0, ctx.urlInputBufferSize);
            }
        }
        ImGui::SameLine();

        if (GlassIconButton("delUrl", "delete", "X", "Eliminar URL", btnSize, ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
        {
            if (ctx.selectedURLIndex >= 0 &&
                ctx.selectedURLIndex < (int)ctx.streamURLs.size()) {
                ctx.streamURLs.erase(ctx.streamURLs.begin() + ctx.selectedURLIndex);
                ctx.selectedURLIndex = -1;
                ctx.saveStreamURLs();
            }
        }
    }

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    float listH = std::clamp(
        (float)ctx.streamURLs.size() * DS::RowHeight + 14.f,
        40.f, 200.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("##url_list", { 0.f, listH }, true))
    {
        if (ctx.streamURLs.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
            ImGui::TextUnformatted("  Sin URLs guardadas");
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));

        for (int i = 0; i < (int)ctx.streamURLs.size(); i++)
        {
            bool sel = (ctx.selectedURLIndex == i);
            std::string disp = TruncURL(ctx.streamURLs[i]);

            bool clicked = DS::GlassListRow(disp.c_str(), sel);

            if (clicked)
            {
                ctx.selectedURLIndex = i;
                Core::LibrarySelection s;
                s.title = ctx.streamURLs[i];
                s.type  = Core::ItemType::Video;
                Core::PresentationCore::Get().SetSelection(s);

                if (ImGui::IsMouseDoubleClicked(0)) {
                    // FIX: sin Stop() previo (corte a negro) — SetVideo()
                    // ya maneja carga en frio o crossfade, y el guard de
                    // reentrancia en Play() necesita que no se le limpie
                    // la ruta actual en cada click repetido.
                    Core::PresentationCore::Get().SetBackgroundMedia(ctx.streamURLs[i], true, /*allowAudio=*/true);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::string& url = ctx.streamURLs[i];
                ImGui::SetDragDropPayload("URL_TO_QUEUE", url.c_str(), url.size() + 1);
                ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
                ImGui::TextUnformatted(TruncURL(url, 38).c_str());
                ImGui::PopStyleColor();
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem(("##ctx_url" + std::to_string(i)).c_str()))
            {
                if (ImGui::MenuItem("Enviar al monitor")) {
                    Core::PresentationCore::Get().SetBackgroundMedia(ctx.streamURLs[i], true, /*allowAudio=*/true);
                    Core::PresentationCore::Get().SetProjecting(true);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Renombrar / Editar URL")) {
                    ctx.renameOldName  = ctx.streamURLs[i];
                    ctx.renameURLIndex = i;
                    ctx.renameIsURL    = true;
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, ctx.streamURLs[i].c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
                if (ImGui::MenuItem("Eliminar URL")) {
                    ImGui::PopStyleColor();
                    ctx.streamURLs.erase(ctx.streamURLs.begin() + i);
                    if (ctx.selectedURLIndex == i) ctx.selectedURLIndex = -1;
                    ctx.saveStreamURLs();
                    ImGui::EndPopup();
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", ctx.streamURLs[i].c_str());
        }

        ImGui::PopStyleVar(); // ItemSpacing
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    bool canAdd = (ctx.selectedURLIndex >= 0 &&
                   ctx.selectedURLIndex < (int)ctx.streamURLs.size());
    if (!canAdd) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.38f);
        GlassIconButton("addQueue", "add_to_queue", "+", "Agregar URL seleccionada a cola",
                        { -1.f, DS::ButtonHeight });
        ImGui::PopStyleVar();
    } else {
        if (GlassIconButton("addQueue2", "add_to_queue", "+",
                            "Agregar URL seleccionada a cola", { -1.f, DS::ButtonHeight }))
        {
            if (ctx.monitorRef)
                ctx.monitorRef->AddURLToQueue(ctx.streamURLs[ctx.selectedURLIndex]);
        }
    }
}

} // namespace ProyecThor::Library