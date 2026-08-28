#include "Announcements.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/DesignSystem.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Nota de integración
//
//  Render(GlassRenderer&) ya NO abre su propia ventana — es contenido de la
//  sección "Anuncios" del sidebar de HomePanel (ver HomePanel.h/.cpp,
//  m_Announcements, dispatch en HomePanel::Render()).
//
//  La salida real al proyector sigue siendo independiente de esto: en
//  UIManager::RenderAll(), bloque ProjectorLive, se busca el panel "Home" en
//  m_Panels, se castea a HomePanel* y se llama incondicionalmente (sin
//  importar que sección del sidebar este activa):
//       if (homePanel->m_Announcements.IsLive()) {
//           ... RenderOnProjector(ImGui::GetWindowDrawList(), mx, my, mode->width, mode->height, dt);
//       }
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers locales — mismo patron que ControlPanel.cpp: todos los colores
//  salen de DS:: (DesignSystem), sincronizado con el tema activo.
// ─────────────────────────────────────────────────────────────────────────────

static ImU32 AnnColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}
static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}
static ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
static ImVec4 Brighten(const ImVec4& c, float amount) {
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

static ImU32 AnnCategoryColor(const std::string& cat) {
    if (cat == "Anuncios" || cat == "Anuncio") return IM_COL32(50, 180, 240, 255);   // Azul / Cian
    if (cat == "Avisos"   || cat == "Aviso")   return IM_COL32(82, 224, 160, 255);   // Verde menta
    if (cat == "Urgente")                      return IM_COL32(240, 80, 90, 255);    // Rojo
    if (cat == "Culto")                        return IM_COL32(245, 180, 50, 255);   // Ámbar
    return IM_COL32(160, 165, 180, 255);                                            // Gris / General
}

static bool SmallIconButton(const char* label, ImVec2 size,
                            ImVec4 col, ImVec4 colHov, ImVec4 colAct) {
    ImGui::PushStyleColor(ImGuiCol_Button,        col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  colAct);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

struct LoadedQuickNote {
    std::string id;
    std::string title;
    std::string content;
    std::string category;
    bool isFavorite = false;
    std::string updatedAt;
};

static std::vector<LoadedQuickNote> LoadAllQuickNotesFromDisk() {
    std::vector<LoadedQuickNote> list;
    try {
        std::filesystem::path p = std::filesystem::path(ProyecThor::GetAppDataRoot()) / "quick_notes.json";
        if (std::filesystem::exists(p)) {
            std::ifstream in(p);
            if (in.is_open()) {
                nlohmann::json j;
                in >> j;
                if (j.contains("notes") && j["notes"].is_array()) {
                    for (const auto& item : j["notes"]) {
                        LoadedQuickNote n;
                        n.id = item.value("id", "");
                        n.title = item.value("title", "");
                        n.content = item.value("content", "");
                        n.category = item.value("category", "General");
                        n.isFavorite = item.value("isFavorite", false);
                        n.updatedAt = item.value("updatedAt", "");
                        if (!n.content.empty()) list.push_back(n);
                    }
                }
            }
        }
    } catch (...) {}
    return list;
}

static void SaveNoteToQuickNotesLibrary(const std::string& title, const std::string& text, const std::string& category) {
    if (text.empty()) return;
    try {
        std::filesystem::path p = std::filesystem::path(ProyecThor::GetAppDataRoot()) / "quick_notes.json";
        nlohmann::json j;
        if (std::filesystem::exists(p)) {
            std::ifstream in(p);
            if (in.is_open()) {
                in >> j;
            }
        }
        if (!j.is_object()) j = nlohmann::json::object();
        if (!j.contains("notes") || !j["notes"].is_array()) j["notes"] = nlohmann::json::array();

        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf{};
#if defined(_WIN32)
        localtime_s(&tmBuf, &nowTime);
#else
        localtime_r(&nowTime, &tmBuf);
#endif
        char dateStr[64];
        std::strftime(dateStr, sizeof(dateStr), "%d/%m/%Y %H:%M", &tmBuf);

        std::string id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

        nlohmann::json item;
        item["id"] = id;
        item["title"] = title.empty() ? text.substr(0, std::min<size_t>(text.size(), 24)) : title;
        item["content"] = text;
        item["category"] = category.empty() ? "Anuncios" : category;
        item["isFavorite"] = false;
        item["updatedAt"] = dateStr;

        j["notes"].insert(j["notes"].begin(), item);

        std::ofstream out(p);
        if (out.is_open()) {
            out << j.dump(2);
        }
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

Announcements::Announcements() {
    Message first;
    std::strncpy(first.text, "Bienvenidos al servicio", sizeof(first.text) - 1);
    std::strncpy(first.tag, "Anuncios", sizeof(first.tag) - 1);
    m_Messages.push_back(first);

    // Cargar lista de fuentes al iniciar
    SyncFontList();
}

void Announcements::AddMessage(const std::string& text, const std::string& tag, bool enabled) {
    if (text.empty()) return;
    Message msg;
    std::strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    std::strncpy(msg.tag, tag.empty() ? "Anuncios" : tag.c_str(), sizeof(msg.tag) - 1);
    msg.tag[sizeof(msg.tag) - 1] = '\0';
    msg.enabled = enabled;
    m_Messages.push_back(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SyncFontList
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::SyncFontList() {
    Core::PresentationCore::Get().SyncFontListFromDisk(m_FontList);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GetCurrentMessage
// ─────────────────────────────────────────────────────────────────────────────

const std::string& Announcements::GetCurrentMessage() const {
    static std::string s_Cache;
    s_Cache.clear();

    if (m_Messages.empty()) return s_Cache;

    int total = (int)m_Messages.size();
    for (int i = 0; i < total; ++i) {
        int idx = (m_ActiveIndex + i) % total;
        if (m_Messages[idx].enabled && m_Messages[idx].text[0] != '\0') {
            s_Cache = m_Messages[idx].text;
            return s_Cache;
        }
    }
    return s_Cache;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TickScroll
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::TickScroll(float deltaTime, float contentWidth, float screenW) {
    if (m_Paused || m_Messages.empty()) return;

    float speed = (m_Direction == Direction::RightToLeft)
                  ? -m_SpeedPxPerSec
                  :  m_SpeedPxPerSec;

    m_ScrollOffset += speed * deltaTime;

    if (m_Direction == Direction::RightToLeft) {
        if (m_ScrollOffset < -(contentWidth + m_GapWidth)) {
            m_ScrollOffset = screenW;

            int total = (int)m_Messages.size();
            for (int i = 1; i <= total; ++i) {
                int next = (m_ActiveIndex + i) % total;
                if (m_Messages[next].enabled && m_Messages[next].text[0] != '\0') {
                    m_ActiveIndex = next;
                    break;
                }
            }
        }
    } else {
        if (m_ScrollOffset > screenW + m_GapWidth) {
            m_ScrollOffset = -(contentWidth);

            int total = (int)m_Messages.size();
            for (int i = 1; i <= total; ++i) {
                int next = (m_ActiveIndex + i) % total;
                if (m_Messages[next].enabled && m_Messages[next].text[0] != '\0') {
                    m_ActiveIndex = next;
                    break;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderOnProjector
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::RenderOnProjector(void* drawListPtr,
                                      float screenX, float screenY,
                                      float screenW, float screenH,
                                      float deltaTime) {
    if (!m_IsLive || m_Messages.empty()) return;

    ImDrawList* drawList = static_cast<ImDrawList*>(drawListPtr);
    auto& core           = Core::PresentationCore::Get();

    const std::string& msg = GetCurrentMessage();
    if (msg.empty()) return;

    float screenScale = screenW / 1920.0f;

    // ── Resolver fuente, tamaño y color según el modo activo ──────────────
    float   fontSize = m_FontSize * screenScale;
    ImFont* font     = nullptr;
    ImU32   textCol  = ImGui::ColorConvertFloat4ToU32(
        ImVec4(m_TextColor[0], m_TextColor[1], m_TextColor[2], m_TextColor[3]));

    if (m_StyleMode == 0 && m_AssignedStyleName[0] != '\0') {
        // Modo: estilo guardado — usa todos los campos del SavedStyle
        Core::SavedStyle resolvedStyle;
        if (core.GetSavedStyle(std::string(m_AssignedStyleName), resolvedStyle)) {
            fontSize = resolvedStyle.size * screenScale;
            font     = core.GetImGuiFont(resolvedStyle.fontName, fontSize);
            textCol  = ImGui::ColorConvertFloat4ToU32(
                ImVec4(resolvedStyle.color[0], resolvedStyle.color[1],
                       resolvedStyle.color[2], resolvedStyle.color[3]));
        }
    } else if (m_StyleMode == 1) {
        // Modo: estilo inline — fuente por nombre desde m_FontList
        if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
            font = core.GetImGuiFont(m_FontList[m_SelectedFontIndex], fontSize);
        }
    }

    if (!font) font = ImGui::GetFont();

    // ── Medir texto ────────────────────────────────────────────────────────
    ImVec2 textSize     = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, msg.c_str());
    float  contentWidth = textSize.x;

    TickScroll(deltaTime, contentWidth, screenW);

    // ── Posición vertical del banner ───────────────────────────────────────
    float bannerH = screenH * m_BannerHeightPct;
    float bannerY = screenY;

    switch (m_VPosition) {
        case 0: bannerY = screenY;                               break;
        case 1: bannerY = screenY + (screenH - bannerH) * 0.5f; break;
        case 2: bannerY = screenY + screenH - bannerH;           break;
        default: bannerY = screenY + screenH - bannerH;          break;
    }

    float bannerX  = screenX;
    float bannerX2 = screenX + screenW;
    float bannerY2 = bannerY + bannerH;

    // ── Fondo ──────────────────────────────────────────────────────────────
    if (m_ShowBg) {
        drawList->AddRectFilled(
            ImVec2(bannerX, bannerY), ImVec2(bannerX2, bannerY2),
            AnnColU32(m_BgR, m_BgG, m_BgB, m_BgA));

        float borderThickness = 2.0f * screenScale;
        ImU32 borderCol       = AnnColU32(0.25f, 0.30f, 0.55f, 0.80f);

        if (m_VPosition == 2 || m_VPosition == 1) {
            drawList->AddLine(ImVec2(bannerX, bannerY),  ImVec2(bannerX2, bannerY),
                borderCol, borderThickness);
        }
        if (m_VPosition == 0 || m_VPosition == 1) {
            drawList->AddLine(ImVec2(bannerX, bannerY2), ImVec2(bannerX2, bannerY2),
                borderCol, borderThickness);
        }
    }

    drawList->PushClipRect(ImVec2(bannerX, bannerY), ImVec2(bannerX2, bannerY2), true);

    float textX   = screenX + m_ScrollOffset;
    float textY   = bannerY + (bannerH - textSize.y) * 0.5f;
    float shadowOff = 2.0f * screenScale;

    drawList->AddText(font, fontSize,
        ImVec2(textX + shadowOff, textY + shadowOff),
        IM_COL32(0, 0, 0, 200), msg.c_str());

    drawList->AddText(font, fontSize,
        ImVec2(textX, textY),
        textCol, msg.c_str());

    drawList->PopClipRect();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render  —  panel de control (ImGui)
// ─────────────────────────────────────────────────────────────────────────────

void Announcements::Render(GlassRenderer& glass) {
    auto& core = Core::PresentationCore::Get();

    // ── Delta time interno ─────────────────────────────────────────────────
    float deltaTime = 0.016f;
    if (!m_FirstFrame) {
        auto now  = std::chrono::steady_clock::now();
        deltaTime = std::chrono::duration<float>(now - m_LastFrameTime).count();
        deltaTime = std::min(deltaTime, 0.1f);
    }
    m_FirstFrame    = false;
    m_LastFrameTime = std::chrono::steady_clock::now();

    // Tokens compartidos con el resto de la app (DS::), sincronizados desde
    // el tema activo — mismo patron que ControlPanel.cpp.
    const ImVec4 fillBase    = ToVec4(DS::BtnDefaultFill);
    const ImVec4 fillHover   = ToVec4(DS::BtnHoverFill);
    const ImVec4 textSection = ToVec4(DS::TextSecondary);
    const ImVec4 textSub     = ToVec4(DS::TextHint);
    const ImVec4 accent      = ToVec4(DS::AccentColor);
    const ImVec4 accentDim   = ToVec4(DS::AccentColorDim);
    const ImVec4 danger      = ToVec4(DS::DangerColor);
    const ImVec4 success     = ToVec4(DS::SuccessColor);

    (void)glass;
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted("Anuncios y Avisos");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Preview del letrero
    // ─────────────────────────────────────────────────────────────────────
    {
        float panelW   = ImGui::GetContentRegionAvail().x;
        float previewH = 50.0f;

        ImVec2      previewPos = ImGui::GetCursorScreenPos();
        ImDrawList* dl         = ImGui::GetWindowDrawList();

        dl->AddRectFilled(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            AnnColU32(m_BgR, m_BgG, m_BgB, m_IsLive ? m_BgA : 0.5f),
            6.0f);

        dl->AddRect(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            m_IsLive ? ColA(DS::AccentColor, 179) : DS::GlassBorder,
            6.0f, 0, 1.5f);

        dl->PushClipRect(
            previewPos,
            ImVec2(previewPos.x + panelW, previewPos.y + previewH),
            true);

        const std::string& msg = GetCurrentMessage();
        if (!msg.empty()) {
            ImFont* previewFont     = ImGui::GetFont();
            float   previewFontSize = ImGui::GetFontSize();

            if (m_StyleMode == 1) {
                // Escalar m_FontSize al ancho del panel
                float scale     = panelW / 1920.0f;
                previewFontSize = std::max(8.0f, m_FontSize * scale);
                if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
                    ImFont* f = core.GetImGuiFont(m_FontList[m_SelectedFontIndex], previewFontSize);
                    if (f) previewFont = f;
                }
            } else if (m_StyleMode == 0 && m_AssignedStyleName[0] != '\0') {
                Core::SavedStyle resolvedStyle;
                if (core.GetSavedStyle(std::string(m_AssignedStyleName), resolvedStyle)) {
                    float scale     = panelW / 1920.0f;
                    previewFontSize = std::max(8.0f, resolvedStyle.size * scale);
                    ImFont* f = core.GetImGuiFont(resolvedStyle.fontName, previewFontSize);
                    if (f) previewFont = f;
                }
            }

            ImVec2 textSize  = previewFont->CalcTextSizeA(previewFontSize, FLT_MAX, 0.0f, msg.c_str());
            float scrollFrac = m_ScrollOffset / 1920.0f;
            float textX      = previewPos.x + scrollFrac * panelW;
            float textY      = previewPos.y + (previewH - textSize.y) * 0.5f;

            ImU32 previewTextCol = m_IsLive ? DS::TextPrimary : DS::TextSecondary;

            if (m_StyleMode == 1) {
                previewTextCol = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(m_TextColor[0], m_TextColor[1],
                           m_TextColor[2], m_TextColor[3]));
            }

            dl->AddText(previewFont, previewFontSize,
                ImVec2(textX + 1.0f, textY + 1.0f), IM_COL32(0, 0, 0, 180), msg.c_str());
            dl->AddText(previewFont, previewFontSize,
                ImVec2(textX, textY), previewTextCol, msg.c_str());
        } else {
            ImVec2 phSize = ImGui::CalcTextSize("Sin mensajes habilitados");
            dl->AddText(
                ImVec2(previewPos.x + (panelW - phSize.x) * 0.5f,
                       previewPos.y + (previewH - phSize.y) * 0.5f),
                DS::TextHint,
                "Sin mensajes habilitados");
        }

        dl->PopClipRect();
        ImGui::Dummy(ImVec2(panelW, previewH));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Lista de mensajes y Filtros
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, textSection);
    ImGui::TextUnformatted("Mensajes de Anuncios y Avisos");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Filtros de categoría para anuncios
    {
        const std::vector<std::string> categories = { "Todos", "Anuncios", "Avisos", "Urgente", "Culto", "General" };

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

        for (const auto& cat : categories) {
            bool selected = (m_CategoryFilter == cat);

            ImVec4 bg = selected
                ? ImGui::ColorConvertU32ToFloat4(DS::AccentColor)
                : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);

            ImGui::PushStyleColor(ImGuiCol_Button, bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4(1,1,1,1) : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));

            if (ImGui::Button(cat.c_str()))
                m_CategoryFilter = cat;

            ImGui::PopStyleColor(3);
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::PopStyleVar(2);
    }

    ImGui::Spacing();

    float listH = std::min(180.0f, (float)m_Messages.size() * 38.0f + 12.0f);
    listH       = std::max(listH, 48.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToVec4(DS::GlassFillBot));
    ImGui::BeginChild("##ann_list", ImVec2(0, listH), true, ImGuiWindowFlags_None);

    int toDelete = -1;
    int toMoveUp = -1;

    const char* cycleTags[] = { "Anuncios", "Avisos", "Urgente", "Culto", "General" };

    for (int i = 0; i < (int)m_Messages.size(); ++i) {
        if (m_CategoryFilter != "Todos" && m_Messages[i].tag != m_CategoryFilter)
            continue;

        ImGui::PushID(i);

        bool isActive = (i == m_ActiveIndex);
        if (isActive && m_IsLive) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Brighten(accentDim, 0.05f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, fillBase);
        }

        ImGui::Checkbox("##en", &m_Messages[i].enabled);
        ImGui::SameLine(0, 6);

        // Pill interactiva de Categoría
        ImU32 catCol = AnnCategoryColor(m_Messages[i].tag);
        ImGui::PushStyleColor(ImGuiCol_Button, (catCol & 0x00FFFFFF) | 0x35000000);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (catCol & 0x00FFFFFF) | 0x55000000);
        ImGui::PushStyleColor(ImGuiCol_Text, catCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

        char tagBtnId[64];
        std::snprintf(tagBtnId, sizeof(tagBtnId), "%s##tagBtn", m_Messages[i].tag);
        if (ImGui::Button(tagBtnId, ImVec2(68.0f, 22.0f))) {
            // Ciclar etiqueta
            int curIdx = 0;
            for (int k = 0; k < 5; ++k) {
                if (std::strcmp(m_Messages[i].tag, cycleTags[k]) == 0) {
                    curIdx = k;
                    break;
                }
            }
            int nextIdx = (curIdx + 1) % 5;
            std::strncpy(m_Messages[i].tag, cycleTags[nextIdx], sizeof(m_Messages[i].tag) - 1);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clic para cambiar categoría / etiqueta");

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 6);

        float fieldW = ImGui::GetContentRegionAvail().x - 88.0f;
        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##msg", m_Messages[i].text, sizeof(m_Messages[i].text));

        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6);

        // Guardar este mensaje como Nota en la Biblioteca
        if (SmallIconButton("G", ImVec2(22, 22),
            fillBase, fillHover, Brighten(fillHover, 0.08f))) {
            SaveNoteToQuickNotesLibrary(m_Messages[i].text, m_Messages[i].text, m_Messages[i].tag);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Guardar este mensaje en la Biblioteca de Notas");

        ImGui::SameLine(0, 4);

        if (i > 0) {
            if (SmallIconButton("^", ImVec2(22, 22),
                fillBase, fillHover, Brighten(fillHover, 0.08f))) {
                toMoveUp = i;
            }
        } else {
            ImGui::Dummy(ImVec2(22, 22));
        }

        ImGui::SameLine(0, 4);

        if (SmallIconButton("x", ImVec2(22, 22),
            ToVec4(ColA(DS::DangerColor, 77)), ToVec4(ColA(DS::DangerColor, 128)), ToVec4(ColA(DS::DangerColor, 166)))) {
            toDelete = i;
        }

        ImGui::PopID();
    }

    if (toMoveUp > 0) {
        std::swap(m_Messages[toMoveUp], m_Messages[toMoveUp - 1]);
        if (m_ActiveIndex == toMoveUp)          m_ActiveIndex = toMoveUp - 1;
        else if (m_ActiveIndex == toMoveUp - 1) m_ActiveIndex = toMoveUp;
    }

    if (toDelete >= 0) {
        m_Messages.erase(m_Messages.begin() + toDelete);
        if (m_ActiveIndex >= (int)m_Messages.size())
            m_ActiveIndex = std::max(0, (int)m_Messages.size() - 1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Botones rápidos para agregar Anuncio / Aviso / Importar de Notas
    {
        float availW = ImGui::GetContentRegionAvail().x;
        float addBtnW = (availW - 12.0f) / 3.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

        // + Agregar Anuncio (Cian)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.55f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.45f, 0.70f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.12f, 0.30f, 0.48f, 1.0f));
        if (ImGui::Button("+ Anuncio", ImVec2(addBtnW, 28.0f))) {
            Message nm;
            std::strncpy(nm.text, "Nuevo anuncio", sizeof(nm.text) - 1);
            std::strncpy(nm.tag, "Anuncios", sizeof(nm.tag) - 1);
            m_Messages.push_back(nm);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 6);

        // + Agregar Aviso (Verde menta)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.32f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.55f, 0.40f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.38f, 0.28f, 1.0f));
        if (ImGui::Button("+ Aviso", ImVec2(addBtnW, 28.0f))) {
            Message nm;
            std::strncpy(nm.text, "Nuevo aviso", sizeof(nm.text) - 1);
            std::strncpy(nm.tag, "Avisos", sizeof(nm.tag) - 1);
            m_Messages.push_back(nm);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 6);

        // Importar de Notas (Ámbar)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.32f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.40f, 0.18f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.28f, 0.12f, 1.0f));
        if (ImGui::Button("Desde Notas", ImVec2(addBtnW, 28.0f))) {
            m_ShowNotesImportModal = true;
            m_ImportSearchFilter[0] = '\0';
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Animación
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, textSection);
    ImGui::TextUnformatted("Animación");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        fillBase);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, fillHover);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

    int dir = (int)m_Direction;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("##dir", &dir, "Derecha a izquierda\0Izquierda a derecha\0")) {
        m_Direction    = (Direction)dir;
        m_ScrollOffset = (m_Direction == Direction::RightToLeft) ? 1920.0f : -400.0f;
    }

    ImGui::Spacing();

    float panelW = ImGui::GetContentRegionAvail().x;
    float halfW  = (panelW - 8.0f) * 0.5f;

    ImGui::SetNextItemWidth(halfW);
    ImGui::SliderFloat("##spd", &m_SpeedPxPerSec, 20.0f, 600.0f, "Vel: %.0f px/s");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(halfW);
    ImGui::SliderFloat("##gap", &m_GapWidth, 50.0f, 600.0f, "Gap: %.0f px");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
    ImGui::Checkbox("Pausar animación", &m_Paused);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Posición y apariencia del banner
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, textSection);
    ImGui::TextUnformatted("Posición y apariencia");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        fillBase);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, fillHover);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##vpos", &m_VPosition, "Arriba\0Centro\0Abajo\0");

    ImGui::Spacing();

    float bannerPct = m_BannerHeightPct * 100.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##bh", &bannerPct, 3.0f, 20.0f, "Alto: %.1f%%")) {
        m_BannerHeightPct = bannerPct / 100.0f;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
    ImGui::Checkbox("Mostrar fondo", &m_ShowBg);
    ImGui::PopStyleColor();

    if (m_ShowBg) {
        ImGui::SameLine(0, 12);
        float bgColor[4] = { m_BgR, m_BgG, m_BgB, m_BgA };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        if (ImGui::ColorEdit4("##bgcol", bgColor,
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoInputs)) {
            m_BgR = bgColor[0]; m_BgG = bgColor[1];
            m_BgB = bgColor[2]; m_BgA = bgColor[3];
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color de fondo del banner");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Estilo de texto
    // ─────────────────────────────────────────────────────────────────────

    ImGui::PushStyleColor(ImGuiCol_Text, textSection);
    ImGui::TextUnformatted("Estilo de texto");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── Selector de modo ──────────────────────────────────────────────────
    {
        float modeW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
        float modeH = 26.0f;

        auto ModeButton = [&](const char* label, int modeValue) {
            bool   active = (m_StyleMode == modeValue);
            ImVec4 bg     = active ? accentDim : fillBase;
            ImVec4 bgH    = active ? Brighten(accentDim, 0.06f) : fillHover;
            ImVec4 textC  = active ? accent : textSub;

            ImGui::PushStyleColor(ImGuiCol_Button,        bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgH);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(accentDim, -0.06f));
            ImGui::PushStyleColor(ImGuiCol_Text,          textC);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

            if (ImGui::Button(label, ImVec2(modeW, modeH)))
                m_StyleMode = modeValue;

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        };

        ModeButton("Estilo guardado##mode0", 0);
        ImGui::SameLine(0, 4);
        ModeButton("Editar estilo##mode1",   1);
    }

    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  Modo 0: elegir estilo guardado
    // ─────────────────────────────────────────────────────────────────────
    if (m_StyleMode == 0) {
        std::vector<std::string> styleNames = core.GetSavedStyleNames();

        if (styleNames.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, Brighten(danger, -0.15f));
            ImGui::TextWrapped("No hay estilos guardados. Usa 'Editar estilo' para crear uno.");
            ImGui::PopStyleColor();
        } else {
            int currentIdx = 0;
            for (int i = 0; i < (int)styleNames.size(); ++i) {
                if (styleNames[i] == std::string(m_AssignedStyleName)) {
                    currentIdx = i;
                    break;
                }
            }

            std::string comboItems;
            for (const auto& name : styleNames) {
                comboItems += name;
                comboItems += '\0';
            }
            comboItems += '\0';

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        fillBase);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, fillHover);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            if (ImGui::Combo("##stylesel", &currentIdx, comboItems.c_str())) {
                if (currentIdx >= 0 && currentIdx < (int)styleNames.size()) {
                    std::strncpy(m_AssignedStyleName,
                                 styleNames[currentIdx].c_str(),
                                 sizeof(m_AssignedStyleName) - 1);
                    m_AssignedStyleName[sizeof(m_AssignedStyleName) - 1] = '\0';

                    // Precargar los valores en los campos inline para coherencia
                    Core::SavedStyle loaded;
                    if (core.GetSavedStyle(styleNames[currentIdx], loaded)) {
                        m_FontSize     = loaded.size;
                        m_TextColor[0] = loaded.color[0];
                        m_TextColor[1] = loaded.color[1];
                        m_TextColor[2] = loaded.color[2];
                        m_TextColor[3] = loaded.color[3];
                    }
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            // Resumen del estilo activo
            if (m_AssignedStyleName[0] != '\0') {
                Core::SavedStyle preview;
                if (core.GetSavedStyle(std::string(m_AssignedStyleName), preview)) {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(ColA(DS::AccentColor, 204)));
                    ImGui::Text("Estilo: %s  |  %.0f px  |  %s",
                        m_AssignedStyleName, preview.size, preview.fontName.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Modo 1: editar fuente, tamaño y color inline + guardar
    // ─────────────────────────────────────────────────────────────────────
    else {
        // ── Botón recargar fuentes ────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Button,        fillBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fillHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(fillHover, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        if (ImGui::Button("Recargar fuentes##ann", ImVec2(ImGui::GetContentRegionAvail().x, 24.0f))) {
            SyncFontList();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        // ── Fuente ────────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, textSub);
        ImGui::TextUnformatted("Fuente");
        ImGui::PopStyleColor();

        if (!m_FontList.empty()) {
            m_SelectedFontIndex = std::max(0, std::min(m_SelectedFontIndex, (int)m_FontList.size() - 1));

            std::string fontComboItems;
            for (const auto& fn : m_FontList) {
                fontComboItems += fn;
                fontComboItems += '\0';
            }
            fontComboItems += '\0';

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        fillBase);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, fillHover);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("##fontsel", &m_SelectedFontIndex, fontComboItems.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, Brighten(danger, -0.15f));
            ImGui::TextUnformatted("No hay fuentes cargadas. Presiona 'Recargar fuentes'.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // ── Tamaño de fuente ──────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, textSub);
        ImGui::TextUnformatted("Tamaño (px a 1920px de ancho)");
        ImGui::PopStyleColor();

        {
            float availW  = ImGui::GetContentRegionAvail().x;
            float sliderW = availW * 0.70f - 4.0f;
            float inputW  = availW * 0.30f - 4.0f;

            ImGui::PushStyleColor(ImGuiCol_FrameBg,          fillBase);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   fillHover);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,       accent);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Brighten(accent, 0.08f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

            ImGui::SetNextItemWidth(sliderW);
            ImGui::SliderFloat("##fontSize", &m_FontSize, 12.0f, 300.0f, "%.0f px");
            ImGui::SameLine(0, 8);
            ImGui::SetNextItemWidth(inputW);
            ImGui::InputFloat("##fontSizeInput", &m_FontSize, 0.0f, 0.0f, "%.0f");
            m_FontSize = std::max(12.0f, std::min(m_FontSize, 300.0f));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::Spacing();

        // ── Color del texto ───────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, textSub);
        ImGui::TextUnformatted("Color del texto");
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::ColorEdit4("##textcol", m_TextColor,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        ImGui::PopStyleVar();

        ImGui::Spacing();

        // ── Botón guardar este estilo con nombre ──────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Button,        accentDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(accentDim, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(accentDim, -0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

        if (ImGui::Button("Guardar como estilo...##annSave",
                          ImVec2(ImGui::GetContentRegionAvail().x, 28.0f))) {
            if (m_SaveStyleName[0] == '\0') {
                std::strncpy(m_SaveStyleName, "Mi estilo anuncio",
                             sizeof(m_SaveStyleName) - 1);
            }
            ImGui::OpenPopup("##ann_save_popup");
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // ── Popup de guardado ──────────────────────────────────────────────
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
        if (ImGui::BeginPopup("##ann_save_popup")) {
            ImGui::PushStyleColor(ImGuiCol_Text, textSection);
            ImGui::TextUnformatted("Nombre del estilo:");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        fillBase);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, fillHover);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputText("##annSaveName", m_SaveStyleName, sizeof(m_SaveStyleName));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            bool nameOk = (m_SaveStyleName[0] != '\0');

            if (!nameOk)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

            ImGui::PushStyleColor(ImGuiCol_Button,        accentDim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(accentDim, 0.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(accentDim, -0.1f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

            if (ImGui::Button("Guardar##annSaveBtn", ImVec2(144.0f, 28.0f)) && nameOk) {
                // Construir el SavedStyle con los valores inline actuales.
                // SaveStyle() toma el nombre desde el campo style.name.
                Core::SavedStyle newStyle;
                newStyle.name      = std::string(m_SaveStyleName);
                newStyle.size      = m_FontSize;
                newStyle.color[0]  = m_TextColor[0];
                newStyle.color[1]  = m_TextColor[1];
                newStyle.color[2]  = m_TextColor[2];
                newStyle.color[3]  = m_TextColor[3];

                if (m_SelectedFontIndex >= 0 && m_SelectedFontIndex < (int)m_FontList.size()) {
                    newStyle.fontName = m_FontList[m_SelectedFontIndex];
                } else {
                    newStyle.fontName = "Predeterminada";
                }

                // Defaults razonables para los campos que Announcements no edita
                newStyle.hAlign    = 1;
                newStyle.vAlign    = 1;
                newStyle.autoScale = false;
                for (int k = 0; k < 4; ++k) newStyle.margins[k] = 50.0f;

                core.SaveStyle(newStyle);

                // Seleccionar el estilo recién guardado para que quede activo
                std::strncpy(m_AssignedStyleName, m_SaveStyleName,
                             sizeof(m_AssignedStyleName) - 1);
                m_AssignedStyleName[sizeof(m_AssignedStyleName) - 1] = '\0';

                ImGui::CloseCurrentPopup();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            if (!nameOk) ImGui::PopStyleVar();

            ImGui::SameLine(0, 8);

            ImGui::PushStyleColor(ImGuiCol_Button,        fillBase);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fillHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(fillHover, -0.05f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

            if (ImGui::Button("Cancelar##annCancelBtn", ImVec2(144.0f, 28.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─────────────────────────────────────────────────────────────────────
    //  SECCIÓN: Transmisión en vivo
    // ─────────────────────────────────────────────────────────────────────

    float btnW = ImGui::GetContentRegionAvail().x;
    float btnH = 38.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

    if (!m_IsLive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        accentDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(accentDim, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(accentDim, -0.1f));

        if (ImGui::Button("Transmitir anuncios", ImVec2(btnW, btnH))) {
            m_IsLive       = true;
            m_Paused       = false;
            m_ScrollOffset = (m_Direction == Direction::RightToLeft) ? 1920.0f : -400.0f;
            m_ActiveIndex  = 0;
            for (int i = 0; i < (int)m_Messages.size(); ++i) {
                if (m_Messages[i].enabled && m_Messages[i].text[0] != '\0') {
                    m_ActiveIndex = i;
                    break;
                }
            }
        }

        ImGui::PopStyleColor(3);
    } else {
        float halfBtn = (btnW - 8.0f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button,        Brighten(danger, -0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(danger, 0.05f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(danger, -0.2f));

        if (ImGui::Button("Detener", ImVec2(halfBtn, btnH))) {
            m_IsLive = false;
            m_Paused = false;
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 8);

        // "Pausar"/"Reanudar" son indicadores tipo semaforo (ambar/verde) —
        // se mantienen literales a proposito, igual que el boton de Mute en
        // ControlPanel, en vez de derivarse del acento del tema.
        if (!m_Paused) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.38f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.50f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.38f, 0.28f, 0.05f, 1.0f));
            if (ImGui::Button("Pausar", ImVec2(halfBtn, btnH))) m_Paused = true;
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        Brighten(success, -0.25f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(success, -0.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Brighten(success, -0.35f));
            if (ImGui::Button("Reanudar", ImVec2(halfBtn, btnH))) m_Paused = false;
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, danger);
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextPrimary));
        ImGui::TextUnformatted(m_Paused ? "EN PAUSA" : "EN VIVO");
        ImGui::PopStyleColor();

        const std::string& currentMsg = GetCurrentMessage();
        if (!currentMsg.empty()) {
            ImGui::SameLine(0, 12);
            ImGui::PushStyleColor(ImGuiCol_Text, textSub);
            std::string display = currentMsg.size() > 28
                                  ? currentMsg.substr(0, 25) + "..."
                                  : currentMsg;
            ImGui::TextUnformatted(display.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::PopStyleVar();

    if (m_ShowNotesImportModal) {
        RenderNotesImportModal();
    }
}

void Announcements::RenderNotesImportModal() {
    ImGui::OpenPopup("Importar desde Biblioteca de Notas");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 430.0f), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImGui::ColorConvertU32ToFloat4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(18.0f, 16.0f));

    if (ImGui::BeginPopupModal("Importar desde Biblioteca de Notas", &m_ShowNotesImportModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Selecciona notas guardadas para añadir al banner de anuncios:");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##importFilter", "Buscar notas por título o texto...", m_ImportSearchFilter, sizeof(m_ImportSearchFilter));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        auto notes = LoadAllQuickNotesFromDisk();

        std::string filterLower = m_ImportSearchFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        std::vector<LoadedQuickNote> filtered;
        for (const auto& n : notes) {
            if (!filterLower.empty()) {
                std::string tLower = n.title;
                std::string cLower = n.content;
                std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
                std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
                if (tLower.find(filterLower) == std::string::npos && cLower.find(filterLower) == std::string::npos)
                    continue;
            }
            filtered.push_back(n);
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillBot));
        ImGui::BeginChild("##importNotesList", ImVec2(0.0f, 260.0f), true);

        if (filtered.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 40.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            const char* noNotesMsg = notes.empty()
                ? "No hay notas guardadas en la biblioteca todavía."
                : "No se encontraron notas con el término buscado.";
            ImVec2 nsz = ImGui::CalcTextSize(noNotesMsg);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - nsz.x) * 0.5f);
            ImGui::TextUnformatted(noNotesMsg);
            ImGui::PopStyleColor();
        } else {
            for (size_t i = 0; i < filtered.size(); ++i) {
                const auto& note = filtered[i];
                ImGui::PushID((int)i);

                ImU32 catCol = AnnCategoryColor(note.category);

                // Badge de categoría
                ImVec2 catSz = ImGui::CalcTextSize(note.category.c_str());
                ImVec2 catPos = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(catPos, ImVec2(catPos.x + catSz.x + 8.0f, catPos.y + catSz.y + 2.0f),
                    (catCol & 0x00FFFFFF) | 0x35000000, DS::RadiusSmall);

                ImGui::SetCursorScreenPos(ImVec2(catPos.x + 4.0f, catPos.y));
                ImGui::PushStyleColor(ImGuiCol_Text, catCol);
                ImGui::SetWindowFontScale(0.85f);
                ImGui::TextUnformatted(note.category.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::SameLine(0.0f, 10.0f);

                // Título
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
                ImGui::TextUnformatted(note.title.c_str());
                ImGui::PopStyleColor();

                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);

                // Botón Añadir
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.65f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.75f, 0.95f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
                if (ImGui::Button("+ Añadir", ImVec2(68.0f, 22.0f))) {
                    AddMessage(note.content, note.category, true);
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);

                // Snippet de contenido
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
                std::string snip = note.content;
                if (snip.size() > 80) snip = snip.substr(0, 75) + "...";
                ImGui::TextWrapped("  %s", snip.c_str());
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PopID();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        float btnW = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;

        if (DS::GlassButton("Añadir Todas", ImVec2(btnW, 32.0f), DS::AccentColor)) {
            for (const auto& note : filtered) {
                AddMessage(note.content, note.category, true);
            }
            m_ShowNotesImportModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine(0.0f, 10.0f);

        if (DS::GlassButton("Cerrar", ImVec2(btnW, 32.0f), DS::TextHint)) {
            m_ShowNotesImportModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI
