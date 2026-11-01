#include "MuninCloudPanel.h"
#include "core/MuninCloudManager.h"
#include "core/AppPaths.h"
#include "core/ThumbnailWorker.h"
#include "core/settings/SettingsManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/biblio/LibraryIcons.h"

#include <imgui.h>
#include <GL/gl.h>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <vector>
#include <cmath>
#include <cctype>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

static inline void DrawFloatingIslandShadow(ImDrawList* dl, ImVec2 pos, ImVec2 size, float rounding) {
    const int   layers    = 6;
    const float maxOffset = 20.0f;
    for (int i = layers; i >= 1; --i) {
        float  t      = (float)i / (float)layers;
        float  offset = maxOffset * t;
        int    alpha  = (int)(44.0f * (1.0f - t * 0.5f));
        ImVec2 p0(pos.x - offset * 0.25f,         pos.y + offset * 0.25f);
        ImVec2 p1(pos.x + size.x + offset * 0.25f, pos.y + size.y + offset * 0.60f);
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, alpha), rounding + offset * 0.20f);
    }
}

enum class MuninCatIcon { All, Bible, Image, Video, Song };

struct MuninCategory {
    const char* label;
    const char* tag;
    const char* description;
    Core::MuninResourceType type;
    MuninCatIcon icon;
    ImU32 color;
};

static const MuninCategory k_MuninCategories[] = {
    { "Todos los Recursos", "ALL", "Catálogo completo sincronizado con la nube Cloudflare R2", Core::MuninResourceType::All,   MuninCatIcon::All,   IM_COL32( 70, 195, 220, 255) },
    { "Biblias",           "BIB", "Versiones completas listas para proyección y búsqueda",   Core::MuninResourceType::Bible, MuninCatIcon::Bible, IM_COL32(185, 130, 245, 255) },
    { "Fondos de Imagen",  "IMG", "Fondos protegidos en alta resolución para diapositivas", Core::MuninResourceType::Image, MuninCatIcon::Image, IM_COL32(245, 165,  75, 255) },
    { "Videos en Bucle",   "VID", "Fondos animados en bucle continuo de alto impacto",       Core::MuninResourceType::Video, MuninCatIcon::Video, IM_COL32( 80, 205, 165, 255) },
    { "Canciones",         "SNG", "Letras y acordes empaquetados para la biblioteca",        Core::MuninResourceType::Song,  MuninCatIcon::Song,  IM_COL32(235, 105, 165, 255) },
};
static constexpr int k_MuninCategoryCount = 5;

static void DrawMuninCatIcon(ImDrawList* dl, MuninCatIcon icon, ImVec2 c, float r, ImU32 color) {
    switch (icon) {
        case MuninCatIcon::All: {
            // Nube estilizada
            dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y + r * 0.05f), r * 0.45f, color);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.35f, c.y + r * 0.05f), r * 0.40f, color);
            dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.25f), r * 0.55f, color);
            dl->AddRectFilled(ImVec2(c.x - r * 0.60f, c.y + r * 0.05f), ImVec2(c.x + r * 0.60f, c.y + r * 0.50f), color, 3.0f);
            break;
        }
        case MuninCatIcon::Bible: {
            // Libro / Biblia abierta
            float bw = r * 0.75f, bh = r * 0.65f;
            dl->AddRect(ImVec2(c.x - bw, c.y - bh), ImVec2(c.x - 1.0f, c.y + bh), color, 1.5f, 0, 1.3f);
            dl->AddRect(ImVec2(c.x + 1.0f, c.y - bh), ImVec2(c.x + bw, c.y + bh), color, 1.5f, 0, 1.3f);
            dl->AddLine(ImVec2(c.x, c.y - bh), ImVec2(c.x, c.y + bh), color, 1.5f);
            // Líneas de texto
            dl->AddLine(ImVec2(c.x - bw + 3.0f, c.y - bh * 0.3f), ImVec2(c.x - 4.0f, c.y - bh * 0.3f), color, 1.1f);
            dl->AddLine(ImVec2(c.x - bw + 3.0f, c.y + bh * 0.1f), ImVec2(c.x - 4.0f, c.y + bh * 0.1f), color, 1.1f);
            dl->AddLine(ImVec2(c.x + 4.0f, c.y - bh * 0.3f), ImVec2(c.x + bw - 3.0f, c.y - bh * 0.3f), color, 1.1f);
            dl->AddLine(ImVec2(c.x + 4.0f, c.y + bh * 0.1f), ImVec2(c.x + bw - 3.0f, c.y + bh * 0.1f), color, 1.1f);
            break;
        }
        case MuninCatIcon::Image: {
            // Paleta de colores / imagen
            dl->AddCircle(c, r * 0.85f, color, 16, 1.3f);
            float dr = r * 0.25f;
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.20f), dr, color);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.30f, c.y - r * 0.25f), dr, color);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.10f, c.y + r * 0.35f), dr, color);
            break;
        }
        case MuninCatIcon::Video: {
            // Monitor con símbolo Play
            ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.60f), mx(c.x + r * 0.85f, c.y + r * 0.35f);
            dl->AddRect(mn, mx, color, 2.0f, 0, 1.3f);
            dl->AddLine(ImVec2(c.x, mx.y), ImVec2(c.x, mx.y + r * 0.35f), color, 1.3f);
            dl->AddLine(ImVec2(c.x - r * 0.40f, mx.y + r * 0.35f), ImVec2(c.x + r * 0.40f, mx.y + r * 0.35f), color, 1.3f);
            // Triángulo de reproducción
            float py = (mn.y + mx.y) * 0.5f;
            dl->AddTriangleFilled(ImVec2(c.x - r * 0.20f, py - r * 0.25f),
                                  ImVec2(c.x - r * 0.20f, py + r * 0.25f),
                                  ImVec2(c.x + r * 0.25f, py), color);
            break;
        }
        case MuninCatIcon::Song: {
            // Nota musical
            ImVec2 head(c.x - r * 0.35f, c.y + r * 0.40f);
            dl->AddCircleFilled(head, r * 0.30f, color);
            dl->AddLine(ImVec2(head.x + r * 0.28f, head.y - r * 0.05f), ImVec2(head.x + r * 0.28f, c.y - r * 0.70f), color, 1.5f);
            dl->AddLine(ImVec2(head.x + r * 0.28f, c.y - r * 0.70f), ImVec2(head.x + r * 0.65f, c.y - r * 0.48f), color, 1.5f);
            break;
        }
    }
}

MuninCloudPanel::MuninCloudPanel() = default;

MuninCloudPanel::~MuninCloudPanel() {
    ClearThumbnailCache();
}

void MuninCloudPanel::ClearThumbnailCache() {
    std::lock_guard<std::mutex> lk(m_ThumbMutex);
    for (auto& pair : m_ThumbTextures) {
        if (pair.second) {
            GLuint tex = (GLuint)(intptr_t)(pair.second);
            if (tex != 0) {
                glDeleteTextures(1, &tex);
            }
        }
    }
    m_ThumbTextures.clear();
}

ImTextureID MuninCloudPanel::GetThumbnailTexture(const Core::MuninResource& res) {
    {
        std::lock_guard<std::mutex> lk(m_ThumbMutex);
        auto it = m_ThumbTextures.find(res.id);
        if (it != m_ThumbTextures.end()) {
            return it->second;
        }
    }

    std::string localTarget = Core::MuninCloudManager::Get().ResolveTargetPath(res);
    std::string fileToLoad = "";

    if (fs::exists(localTarget) && res.type == Core::MuninResourceType::Image) {
        fileToLoad = localTarget;
    } else {
        std::string cacheDir = GetAssetsPath() + "/cache/munin_thumbs";
        std::string cachedFile = cacheDir + "/" + res.id + ".jpg";

        if (fs::exists(cachedFile)) {
            fileToLoad = cachedFile;
        } else if (res.type == Core::MuninResourceType::Image && !res.downloadUrl.empty()) {
            bool alreadyFetching = false;
            {
                std::lock_guard<std::mutex> lk(m_ThumbMutex);
                if (m_FetchingThumbs[res.id]) {
                    alreadyFetching = true;
                } else {
                    m_FetchingThumbs[res.id] = true;
                }
            }

            if (!alreadyFetching) {
                std::string url = res.thumbnailUrl.empty() ? res.downloadUrl : res.thumbnailUrl;
                std::string id = res.id;
                std::thread([id, url, cachedFile, cacheDir, this]() {
                    try { fs::create_directories(cacheDir); } catch (...) {}
                    std::string tempPart = cachedFile + ".part";
                    bool dlOk = Core::MuninCloudManager::DownloadFileSilent(url, tempPart);
                    if (dlOk && fs::exists(tempPart) && fs::file_size(tempPart) > 0) {
                        std::error_code ec;
                        fs::rename(tempPart, cachedFile, ec);
                    } else {
                        std::error_code ec;
                        fs::remove(tempPart, ec);
                    }
                    std::lock_guard<std::mutex> lk(m_ThumbMutex);
                    m_FetchingThumbs[id] = false;
                }).detach();
            }
        }
    }

    if (!fileToLoad.empty()) {
        std::vector<uint8_t> pixels;
        unsigned w = 0, h = 0;
        if (Core::ThumbnailWorker::LoadThumbnailPixelsFromDisk(fileToLoad, pixels, w, h, 480, 270)) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glBindTexture(GL_TEXTURE_2D, 0);

            ImTextureID texID = (ImTextureID)(intptr_t)tex;
            {
                std::lock_guard<std::mutex> lk(m_ThumbMutex);
                m_ThumbTextures[res.id] = texID;
            }
            return texID;
        }
    }

    return (ImTextureID)0;
}

void MuninCloudPanel::Render(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    auto& munin = Core::MuninCloudManager::Get();
    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const ImVec2 baseSize(1040.0f, 700.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    // Fondo oscurecido (Dimmed Backdrop Scrim estilo Ajustes)
    ImDrawList* bgDl = ImGui::GetBackgroundDrawList(vp);
    if (bgDl) {
        bgDl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(0, 0, 0, 150));
    }

    ImGui::SetNextWindowPos(workCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(baseSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(840, 560), ImVec2(FLT_MAX, FLT_MAX));

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ThemeCol(theme.surface0, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ThemeCol(theme.surface1, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ThemeCol(theme.border, 0.70f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool open = ImGui::Begin("MuninCloud — Nube de Recursos Cloudflare R2", pOpen,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);

    bool ownPopupOpen = ImGui::GetCurrentContext()->OpenPopupStack.Size > 0 &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
         ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup));
    if (!ownPopupOpen) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!open) {
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float margin  = 14.0f;
    const float gap     = 12.0f;
    const float sideW   = 240.0f;
    const float footerH = 64.0f;
    const float islandH = avail.y - margin * 2.0f;
    const float contentW = avail.x - margin * 2.0f - sideW - gap;

    ImVec2 origin(ImGui::GetCursorScreenPos().x + margin, ImGui::GetCursorScreenPos().y + margin);
    ImVec2 sideOrigin(origin.x, origin.y);
    ImVec2 contentOrigin(origin.x + sideW + gap, origin.y);

    ImDrawList* winDl = ImGui::GetWindowDrawList();
    DrawFloatingIslandShadow(winDl, sideOrigin,    ImVec2(sideW,    islandH), theme.windowRounding);
    DrawFloatingIslandShadow(winDl, contentOrigin, ImVec2(contentW, islandH), theme.windowRounding);

    // Divisor visual entre las dos islas
    {
        float midX = sideOrigin.x + sideW + gap * 0.5f;
        float y0   = sideOrigin.y + islandH * 0.20f;
        float y1   = sideOrigin.y + islandH * 0.80f;
        ImU32 segCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.90f));
        winDl->AddLine(ImVec2(midX, y0), ImVec2(midX, y1), segCol, 1.5f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Isla Derecha: Contenido y Tarjetas de Recursos
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::SetCursorScreenPos(contentOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));

    ImGui::BeginChild("##munin_content_island", ImVec2(contentW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImVec2 islandAvail = ImGui::GetContentRegionAvail();

    // Área con scroll para el contenido
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(35.0f, 30.0f));
    ImGui::BeginChild("##munin_content_scroll", ImVec2(islandAvail.x, islandAvail.y - footerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
    ImGui::PopStyleVar();

    const MuninCategory& activeCat = k_MuninCategories[m_SelectedCategoryIdx];

    // Encabezado de la página de contenido
    {
        ImGui::BeginGroup();
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
            ImGui::SetWindowFontScale(1.4f);
            ImGui::TextUnformatted(activeCat.label);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::TextUnformatted(activeCat.description);
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();

        // Botón derecho: Sincronizar catálogo
        float headAvailW = ImGui::GetContentRegionAvail().x;
        float btnW = 160.0f;
        if (headAvailW > btnW + 20.0f) {
            ImGui::SameLine(0.0f, headAvailW - btnW);
        }
        bool busy = munin.IsBusy();
        if (busy) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.surface2, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.surface3, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.surface1, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.frameRounding);

        if (ImGui::Button(busy ? "Sincronizando..." : "↻ Actualizar Catálogo", ImVec2(btnW, 32.0f))) {
            munin.RefreshCatalog([this](bool ok, const std::string& msg) {
                m_ToastMessage = msg;
                m_ToastTimer = 4.0f;
            });
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        if (busy) ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Separador líquido degradado (estilo Ajustes)
        ImVec2 sepP = ImGui::GetCursorScreenPos();
        float sepW = ImGui::GetContentRegionAvail().x;
        ImVec4 accentV = ThemeCol(theme.accent);
        ImU32 sepAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.6f));
        ImU32 sepFade   = ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.0f));
        ImDrawList* cDl = ImGui::GetWindowDrawList();
        cDl->AddRectFilledMultiColor(sepP, ImVec2(sepP.x + sepW, sepP.y + 1.5f), sepAccent, sepFade, sepFade, sepAccent);

        ImGui::Dummy(ImVec2(0.0f, 18.0f));
    }

    // Cuadrícula de Recursos
    auto resources = munin.GetResources(activeCat.type, m_SearchFilter);

    if (resources.empty()) {
        ImGui::Spacing();
        ImGui::Indent(15.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
        ImGui::TextUnformatted("No se encontraron recursos disponibles para esta categoría o búsqueda.");
        ImGui::PopStyleColor();
        ImGui::Unindent(15.0f);
    } else {
        float gridAvailW = ImGui::GetContentRegionAvail().x;
        int cols = (gridAvailW > 860.0f) ? 3 : (gridAvailW > 520.0f ? 2 : 1);

        if (ImGui::BeginTable("##muninCardsTable", cols, ImGuiTableFlags_SizingStretchProp)) {
            for (size_t i = 0; i < resources.size(); i++) {
                const auto& r = resources[i];
                ImGui::TableNextColumn();
                ImGui::PushID(r.id.c_str());

                bool installed = munin.IsResourceInstalled(r);
                bool downloading = munin.IsDownloading(r.id);

                ImVec4 bdrCol = downloading ? ThemeCol(theme.accentLight, 0.9f) :
                                (installed ? ThemeCol(theme.success, 0.65f) : ThemeCol(theme.border, 0.50f));

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface1, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_Border,  bdrCol);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.frameRounding + 2.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));

                bool isMedia = (r.type == Core::MuninResourceType::Image || r.type == Core::MuninResourceType::Video);
                float cardH = isMedia ? 228.0f : 142.0f;

                std::string childId = "##cardChild_" + r.id;
                if (ImGui::BeginChild(childId.c_str(), ImVec2(0.0f, cardH), true, ImGuiWindowFlags_NoScrollbar)) {
                    ImDrawList* cardDl = ImGui::GetWindowDrawList();
                    float cardAvailW = ImGui::GetContentRegionAvail().x;

                    if (isMedia) {
                        float thumbH = 92.0f;
                        ImVec2 tP0 = ImGui::GetCursorScreenPos();
                        ImVec2 tP1 = ImVec2(tP0.x + cardAvailW, tP0.y + thumbH);

                        ImTextureID tex = GetThumbnailTexture(r);
                        if (tex) {
                            cardDl->AddImageRounded(tex, tP0, tP1, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, theme.frameRounding);
                            cardDl->AddRect(tP0, tP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.4f)), theme.frameRounding, 0, 1.0f);
                        } else {
                            cardDl->AddRectFilled(tP0, tP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.8f)), theme.frameRounding);
                            cardDl->AddRect(tP0, tP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.3f)), theme.frameRounding, 0, 1.0f);

                            float icSz = 26.0f;
                            ImVec2 icCenter = ImVec2(tP0.x + (cardAvailW - icSz) * 0.5f, tP0.y + (thumbH - icSz) * 0.5f - 4.0f);
                            if (r.type == Core::MuninResourceType::Image) {
                                AppIcons::DrawIcon_Layers(cardDl, icCenter, icSz, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.8f)));
                            } else {
                                AppIcons::DrawIcon_Monitor(cardDl, icCenter, icSz, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.8f)));
                            }
                        }
                        ImGui::Dummy(ImVec2(0.0f, thumbH + 6.0f));
                    } else {
                        // Badge superior para Textos / Biblias
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.60f, 0.95f, 1.0f));
                        ImGui::Text("[ 📖 BIBLIA ]");
                        ImGui::PopStyleColor();

                        ImGui::SameLine(0.0f, ImGui::GetContentRegionAvail().x - 65.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
                        ImGui::TextUnformatted(r.sizeStr.c_str());
                        ImGui::PopStyleColor();
                    }

                    // Título
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
                    std::string dTitle = r.title;
                    float maxTitleW = ImGui::GetContentRegionAvail().x;
                    if (ImGui::CalcTextSize(dTitle.c_str()).x > maxTitleW) {
                        while (!dTitle.empty() && ImGui::CalcTextSize((dTitle + "...").c_str()).x > maxTitleW) {
                            dTitle.pop_back();
                        }
                        dTitle += "...";
                    }
                    ImGui::TextUnformatted(dTitle.c_str());
                    ImGui::PopStyleColor();

                    // Descripción
                    std::string desc = r.description;
                    if (desc.empty() && !r.tags.empty()) {
                        desc = "#" + r.tags[0];
                        if (r.tags.size() > 1) desc += " #" + r.tags[1];
                    }
                    if (ImGui::CalcTextSize(desc.c_str()).x > maxTitleW) {
                        while (!desc.empty() && ImGui::CalcTextSize((desc + "...").c_str()).x > maxTitleW) {
                            desc.pop_back();
                        }
                        desc += "...";
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textFaint));
                    ImGui::TextUnformatted(desc.c_str());
                    ImGui::PopStyleColor();

                    // Botón de descarga
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    float btnWAction = -1.0f;
                    float btnHAction = 26.0f;

                    if (downloading) {
                        float prog = munin.GetDownloadProgress(r.id);
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ThemeCol(theme.accent, 0.85f));
                        ImGui::ProgressBar(prog, ImVec2(btnWAction, btnHAction), "Descargando...");
                        ImGui::PopStyleColor();
                    } else if (installed) {
                        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(34, 197, 94, 60));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(34, 197, 94, 110));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(34, 197, 94, 160));
                        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(180, 255, 200, 255));
                        const char* instLabel = (r.type == Core::MuninResourceType::Image) ? "✓ En Fondos (Reinstalar)" : "✓ Instalado (Reinstalar)";
                        if (ImGui::Button(instLabel, ImVec2(btnWAction, btnHAction))) {
                            munin.DownloadResource(r, [this, r](bool ok, const std::string& msg) {
                                m_ToastMessage = ok ? ("'" + r.title + "' descargado e instalado.") : msg;
                                m_ToastTimer = 4.0f;
                            });
                        }
                        ImGui::PopStyleColor(4);
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.accent, 0.80f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.accentLight, 0.95f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.accentDim, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));
                        const char* downLabel = (r.type == Core::MuninResourceType::Image) ? "⬇ Descargar a Fondos" : "⬇ Descargar e Instalar";
                        if (ImGui::Button(downLabel, ImVec2(btnWAction, btnHAction))) {
                            munin.DownloadResource(r, [this, r](bool ok, const std::string& msg) {
                                m_ToastMessage = ok ? ("'" + r.title + "' instalado con éxito.") : msg;
                                m_ToastTimer = 4.0f;
                            });
                        }
                        ImGui::PopStyleColor(4);
                    }

                    ImGui::Dummy(ImVec2(0.0f, 0.0f));
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild(); // munin_content_scroll

    // Barra inferior de estado (Footer Bar estilo Ajustes)
    {
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        float  barY    = winPos.y + winSize.y - footerH;

        ImDrawList* dlFoot = ImGui::GetWindowDrawList();

        dlFoot->AddRectFilled(
            ImVec2(winPos.x, barY),
            ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.98f)),
            theme.windowRounding,
            ImDrawFlags_RoundCornersBottom);

        dlFoot->AddLine(
            ImVec2(winPos.x, barY),
            ImVec2(winPos.x + winSize.x, barY),
            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.9f)),
            1.5f);

        float textY = barY + (footerH - ImGui::GetTextLineHeight()) * 0.5f;

        if (m_ToastTimer > 0.0f) {
            float dt = ImGui::GetIO().DeltaTime;
            m_ToastTimer -= dt;
            dlFoot->AddText(ImVec2(winPos.x + 30.0f, textY),
                            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accentLight)),
                            m_ToastMessage.c_str());
        } else {
            char statusBuf[128];
            snprintf(statusBuf, sizeof(statusBuf), "✓ Conectado a Cloudflare R2 • %d recursos oficiales disponibles", (int)resources.size());
            dlFoot->AddText(ImVec2(winPos.x + 30.0f, textY),
                            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)),
                            statusBuf);
        }

        // Indicador a la derecha
        const char* rInfo = "Almacenamiento Global R2 • Tráfico Ilimitado";
        float rInfoW = ImGui::CalcTextSize(rInfo).x;
        dlFoot->AddText(ImVec2(winPos.x + winSize.x - rInfoW - 30.0f, textY),
                        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint)),
                        rInfo);
    }

    ImGui::EndChild(); // munin_content_island
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // ─────────────────────────────────────────────────────────────────────────
    //  Isla Izquierda: Navegación del Sidebar (Estilo Ajustes)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::SetCursorScreenPos(sideOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 25.0f));

    ImGui::BeginChild("##munin_sidebar", ImVec2(sideW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImDrawList* sideDl = ImGui::GetWindowDrawList();

    // Encabezado de la barra lateral con icono de nube
    ImVec2 headerP = ImGui::GetCursorScreenPos();
    AppIcons::DrawIcon_Cloud(sideDl, ImVec2(headerP.x + 20.0f, headerP.y - 1.0f), 22.0f, IM_COL32(56, 189, 248, 255));

    ImGui::SetCursorPosX(48.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("MuninCloud");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(22.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextUnformatted("Nube de Recursos R2");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Buscador en Sidebar (Exactamente como en Ajustes)
    const float searchW = sideW - 20.0f;
    ImGui::SetCursorPosX(10.0f);
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ThemeCol(theme.surface2, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ThemeCol(theme.surface2, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ThemeCol(theme.surface3, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Text,           ThemeCol(theme.textPrimary));
        ImGui::PushStyleColor(ImGuiCol_Border,         ThemeCol(theme.border, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   theme.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(30.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::SetNextItemWidth(searchW);
        ImGui::InputTextWithHint("##muninSearch", "Buscar recursos...", m_SearchFilter, sizeof(m_SearchFilter));
        ImVec2 fMin = ImGui::GetItemRectMin();
        ImVec2 fMax = ImGui::GetItemRectMax();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        // Icono de lupa dibujado
        ImU32 glassCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint));
        ImVec2 gC(fMin.x + 15.0f, (fMin.y + fMax.y) * 0.5f);
        sideDl->AddCircle(gC, 5.2f, glassCol, 12, 1.4f);
        ImVec2 hDir(0.71f, 0.71f);
        sideDl->AddLine(ImVec2(gC.x + 5.2f * hDir.x,       gC.y + 5.2f * hDir.y),
                        ImVec2(gC.x + 5.2f * hDir.x * 1.85f, gC.y + 5.2f * hDir.y * 1.85f),
                        glassCol, 1.6f);

        if (m_SearchFilter[0] != '\0') {
            ImVec2 xC(fMax.x - 16.0f, (fMin.y + fMax.y) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(xC.x - 9.0f, xC.y - 9.0f));
            if (ImGui::InvisibleButton("##muninSearchClear", ImVec2(18.0f, 18.0f)))
                m_SearchFilter[0] = '\0';
            ImU32 xCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint, ImGui::IsItemHovered() ? 1.0f : 0.7f));
            sideDl->AddLine(ImVec2(xC.x - 3.5f, xC.y - 3.5f), ImVec2(xC.x + 3.5f, xC.y + 3.5f), xCol, 1.5f);
            sideDl->AddLine(ImVec2(xC.x - 3.5f, xC.y + 3.5f), ImVec2(xC.x + 3.5f, xC.y - 3.5f), xCol, 1.5f);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Lista de Categorías en el Sidebar (con animación de píldora fija)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));

    const float itemH = 45.0f;
    const float itemW = sideW - 20.0f;

    sideDl->ChannelsSplit(2);
    float targetPillY = -1.0f;

    for (int i = 0; i < k_MuninCategoryCount; i++) {
        bool selected = (m_SelectedCategoryIdx == i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f;

        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));

        char selectId[32];
        snprintf(selectId, sizeof(selectId), "##cat%d", i);

        sideDl->ChannelsSetCurrent(1);

        bool clicked = ImGui::Selectable(selectId, selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH));
        bool hovered = ImGui::IsItemHovered() && !selected;

        if (clicked && m_SelectedCategoryIdx != i) {
            m_SelectedCategoryIdx = i;
            m_CurrentTab = k_MuninCategories[i].type;
        }

        if (selected) {
            targetPillY = p.y - ImGui::GetWindowPos().y;
        } else if (hovered) {
            sideDl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                                  ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.5f)), 8.0f);
        }

        // Chip circular con icono de categoría
        ImVec4 iconColV = ImGui::ColorConvertU32ToFloat4(k_MuninCategories[i].color);
        ImVec2 badgeC(p.x + 22.0f, p.y + itemH * 0.5f);
        ImVec4 badgeFillV = iconColV;
        badgeFillV.w = selected ? 0.24f : (hovered ? 0.16f : 0.10f);
        sideDl->AddCircleFilled(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeFillV), 20);
        if (selected) {
            ImVec4 badgeRingV = iconColV;
            badgeRingV.w = 0.55f;
            sideDl->AddCircle(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeRingV), 20, 1.2f);
        }
        iconColV.w *= selected ? 1.0f : (hovered ? 0.90f : 0.62f);
        DrawMuninCatIcon(sideDl, k_MuninCategories[i].icon, badgeC, 8.5f, ImGui::ColorConvertFloat4ToU32(iconColV));

        float labelY = p.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f;
        float labelX = p.x + 42.0f;
        ImVec4 labelColV = selected ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textDim, hovered ? 1.0f : 0.85f);
        sideDl->AddText(ImVec2(labelX, labelY), ImGui::ColorConvertFloat4ToU32(labelColV), k_MuninCategories[i].label);

        // Contador de elementos en la categoría
        int count = (int)munin.GetResources(k_MuninCategories[i].type).size();
        std::string cntStr = std::to_string(count);
        float cntW = ImGui::CalcTextSize(cntStr.c_str()).x;
        ImVec2 cntP(p.x + itemW - cntW - 12.0f, labelY);
        sideDl->AddText(cntP, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint)), cntStr.c_str());

        ImGui::PopStyleColor(3);
    }

    // Píldora de selección activa en el canal 0 (igual a Ajustes)
    if (targetPillY >= 0.0f) {
        sideDl->ChannelsSetCurrent(0);
        ImVec2 pillP(ImGui::GetWindowPos().x + 10.0f, ImGui::GetWindowPos().y + targetPillY);

        ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.30f));
        ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
        sideDl->AddRectFilledMultiColor(pillP, ImVec2(pillP.x + itemW, pillP.y + itemH), colLeft, colRight, colRight, colLeft);

        sideDl->AddRectFilled(ImVec2(pillP.x, pillP.y + 8.0f), ImVec2(pillP.x + 4.0f, pillP.y + itemH - 8.0f),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent)), 2.0f);
    }

    sideDl->ChannelsMerge();
    ImGui::PopStyleVar(); // ItemSpacing

    // Tarjeta inferior de estado de conexión R2 (dibujada limpia sin alterar cursor)
    {
        ImVec2 winP = ImGui::GetWindowPos();
        ImVec2 cardP0(winP.x + 10.0f, winP.y + islandH - 75.0f);
        ImVec2 cardP1(cardP0.x + sideW - 20.0f, cardP0.y + 60.0f);
        sideDl->AddRectFilled(cardP0, cardP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.60f)), theme.frameRounding);
        sideDl->AddRect(cardP0, cardP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.40f)), theme.frameRounding, 0, 1.0f);

        // Punto verde de estado activo
        sideDl->AddCircleFilled(ImVec2(cardP0.x + 16.0f, cardP0.y + 22.0f), 4.5f, IM_COL32(34, 197, 94, 255));
        sideDl->AddText(ImVec2(cardP0.x + 28.0f, cardP0.y + 14.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), "Cloudflare R2");
        sideDl->AddText(ImVec2(cardP0.x + 28.0f, cardP0.y + 32.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint)), "Conexión Activa");
    }

    ImGui::EndChild(); // munin_sidebar
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::End();
}

} // namespace ProyecThor::UI
