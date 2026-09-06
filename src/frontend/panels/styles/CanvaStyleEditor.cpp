#include "CanvaStyleEditor.h"
#include "styles/TabTypography.h"
#include "styles/TabEffects.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/TextEffectsRenderer.h"
#include "frontend/views/SongBackgroundPicker.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>
#include <filesystem>

namespace ProyecThor::UI {

ImU32 CanvaPalette::ToU32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

static ImVec4 CanvaV(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

void CanvaPalette::Sync(const ProyecThor::Settings::ThemeSettings& t) {
    Accent       = CanvaV(t.accent);
    AccentHov    = CanvaV(t.accentLight);
    AccentActive = CanvaV(t.accentDim);
    Green        = CanvaV(t.success);
    Red          = CanvaV(t.danger);
    Surface0     = CanvaV(t.surface0);
    Surface1     = CanvaV(t.surface1);
    Surface2     = CanvaV(t.surface2);
    Border       = CanvaV(t.border);
    Text         = CanvaV(t.textPrimary);
    TextMuted    = CanvaV(t.textDim);
}

bool CanvaStyleEditor::PrimaryButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::AccentHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  CanvaPalette::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

bool CanvaStyleEditor::GhostButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 7.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

void CanvaStyleEditor::Badge(const char* label, ImVec4 color) {
    ImVec2 p       = ImGui::GetCursorScreenPos();
    ImVec2 textSz  = ImGui::CalcTextSize(label);
    const float px = 8.0f, py = 3.0f;
    ImVec2 rectMax = ImVec2(p.x + textSz.x + px * 2.0f, p.y + textSz.y + py * 2.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 bgColor = ImVec4(color.x * 0.20f, color.y * 0.20f, color.z * 0.20f, 1.0f);
    ImVec4 bdColor = ImVec4(color.x, color.y, color.z, 0.45f);
    dl->AddRectFilled(p, rectMax, CanvaPalette::ToU32(bgColor), 4.0f);
    dl->AddRect(p, rectMax, CanvaPalette::ToU32(bdColor), 4.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x + px, p.y + py));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + textSz.y + py * 2.0f + 2.0f));
}

void CanvaStyleEditor::SectionLabel(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
}

void CanvaStyleEditor::SegmentedButtons(const char* prefix,
                                         const char** labels, int count, int* current,
                                         float totalWidth, float height,
                                         const ImVec4& activeColor) {
    const float gap = 4.0f;
    float segW = (totalWidth - gap * (count - 1)) / static_cast<float>(count);

    for (int i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine(0.0f, gap);

        bool active = (*current == i);
        ImVec4 bgActive = ImVec4(
            activeColor.x * 0.28f,
            activeColor.y * 0.28f,
            activeColor.z * 0.55f,
            1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button,        active ? bgActive : CanvaPalette::Surface1);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_Text,          active ? activeColor : CanvaPalette::TextMuted);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        std::string id = std::string(labels[i])
                       + "##" + std::string(prefix)
                       + "_" + std::to_string(i);

        if (ImGui::Button(id.c_str(), ImVec2(segW, height)))
            *current = i;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
}

CanvaStyleEditor::CanvaStyleEditor(std::vector<std::string>* fontList,
                                   OnFontImportedCallback    onFontImported)
    : m_FontList(fontList)
{
    memset(m_Name, 0, sizeof(m_Name));

    m_TabTypography = std::make_unique<TabTypography>(fontList, std::move(onFontImported));
    m_TabEffects    = std::make_unique<TabEffects>();
}

CanvaStyleEditor::~CanvaStyleEditor() = default;

void CanvaStyleEditor::OpenNew(const StyleData& defaults) {
    m_IsEditingExisting = false;
    m_IsOpen            = true;
    m_SelectedFlag       = 0;
    m_RibbonTab          = 0;
    m_Data               = defaults;
    memset(m_Name, 0, sizeof(m_Name));
}

void CanvaStyleEditor::OpenEdit(const std::string& existingName, const StyleData& existingData) {
    m_IsEditingExisting = true;
    m_IsOpen            = true;
    m_SelectedFlag       = 0;
    m_RibbonTab          = 0;
    m_Data               = existingData;

    size_t len = std::min(existingName.size(), sizeof(m_Name) - 1);
    memcpy(m_Name, existingName.c_str(), len);
    m_Name[len] = '\0';
}

Core::TextBoxStyle& CanvaStyleEditor::SelectedBox() {
    return (m_SelectedFlag == 0) ? m_Data.lyrics : m_Data.index;
}

bool CanvaStyleEditor::Render(OnSaveCallback onSave) {
    if (!m_IsOpen) return false;

    bool savedThisFrame = false;

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    RenderHeader(winPos, winSize, onSave, savedThisFrame);
    if (savedThisFrame || !m_IsOpen) return savedThisFrame;

    constexpr float kHeaderH = 64.0f;
    constexpr float kPadH    = 16.0f;

    ImGui::SetCursorScreenPos(ImVec2(winPos.x + kPadH, winPos.y + kHeaderH + 10.0f));
    ImGui::BeginGroup();
    RenderRibbon(winSize.x - kPadH * 2.0f);
    ImGui::EndGroup();

    ImVec2 afterRibbon  = ImGui::GetCursorScreenPos();
    float  canvasAvailH = (winPos.y + winSize.y) - afterRibbon.y - kPadH;

    ImGui::SetCursorScreenPos(ImVec2(winPos.x, afterRibbon.y + 10.0f));
    RenderCanvas(winSize.x, canvasAvailH);

    return savedThisFrame;
}

void CanvaStyleEditor::RenderHeader(ImVec2 winPos, ImVec2 winSize,
                                     OnSaveCallback& onSave, bool& savedThisFrame) {
    constexpr float kHeaderH = 64.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Surface1));
    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH - 1.0f),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH - 1.0f),
        CanvaPalette::ToU32(ImVec4(CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.35f)), 1.5f);

    std::string title = m_IsEditingExisting
        ? (std::string("Editar estilo -- ") + m_Name)
        : "Nuevo estilo de texto";
    dl->AddText(ImGui::GetFont(), 16.0f,
        ImVec2(winPos.x + 20.0f, winPos.y + 15.0f),
        CanvaPalette::ToU32(CanvaPalette::Text), title.c_str());

    // Selector de flag -- Letras | Indice -- decide sobre que caja
    // trabaja el resto del editor (ribbon + resaltado en el canvas).
    const char* flagLabels[] = { "Letras", "Indice" };
    float segW = 220.0f;
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + (winSize.x - segW) * 0.5f, winPos.y + (kHeaderH - 30.0f) * 0.5f));
    SegmentedButtons("flagSel", flagLabels, 2, &m_SelectedFlag, segW, 30.0f, CanvaPalette::Accent);

    // Nombre del estilo + Guardar/Cancelar -- esquina superior derecha.
    bool nameEmpty = (strlen(m_Name) == 0);
    const float nameW = 200.0f, saveW = 110.0f, cancelW = 90.0f, gap = 8.0f;
    float groupW = nameW + gap + saveW + gap + cancelW;
    float startX = winPos.x + winSize.x - 20.0f - groupW;
    float rowY   = winPos.y + (kHeaderH - 30.0f) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(startX, rowY));
    if (nameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.30f, 0.30f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Border);
    ImGui::PushStyleColor(ImGuiCol_Text,           CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::SetNextItemWidth(nameW);
    ImGui::InputTextWithHint("##styleNameInput", "Nombre del estilo", m_Name, sizeof(m_Name));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (nameEmpty) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, gap);
    bool canSave = !nameEmpty;
    if (!canSave) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
    if (PrimaryButton("Guardar", ImVec2(saveW, 30.0f)) && canSave) {
        if (onSave) onSave(std::string(m_Name), m_Data);
        savedThisFrame = true;
        m_IsOpen        = false;
    }
    if (!canSave) ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, gap);
    if (GhostButton("Cancelar", ImVec2(cancelW, 30.0f)))
        m_IsOpen = false;
}

void CanvaStyleEditor::RenderRibbon(float width) {
    const char* tabLabels[] = { "Fuente / Alinear", "Efectos", "Fondo de pantalla" };
    ImVec4 bgActive = ImVec4(
        CanvaPalette::Accent.x * 0.20f, CanvaPalette::Accent.y * 0.20f,
        CanvaPalette::Accent.z * 0.42f, 1.0f);

    for (int t = 0; t < 3; t++) {
        if (t > 0) ImGui::SameLine(0.0f, 4.0f);
        bool active = (m_RibbonTab == t);

        ImGui::PushStyleColor(ImGuiCol_Button,        active ? bgActive : CanvaPalette::Surface1);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
        ImGui::PushStyleColor(ImGuiCol_Text,          active ? CanvaPalette::Accent : CanvaPalette::TextMuted);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));

        std::string id = std::string(tabLabels[t]) + "##ribbonTab" + std::to_string(t);
        if (ImGui::Button(id.c_str(), ImVec2(170.0f, 28.0f)))
            m_RibbonTab = t;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface1);
    ImGui::BeginChild("##ribbonContent", ImVec2(width, 128.0f), true);
    switch (m_RibbonTab) {
        case 0: RenderRibbonFontAlign(width - 24.0f); break;
        case 1: RenderRibbonEffects(width - 24.0f);   break;
        case 2: RenderRibbonBackground(width - 24.0f); break;
        default: break;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CanvaStyleEditor::RenderRibbonFontAlign(float /*width*/) {
    auto& box = SelectedBox();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    std::string btnLabel = "Tipografia: " + box.fontName + "  (" + std::to_string((int)box.textSize) + " px)";
    if (GhostButton(btnLabel.c_str(), ImVec2(300.0f, 32.0f)))
        ImGui::OpenPopup("##typographyPopup");

    if (ImGui::BeginPopup("##typographyPopup")) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, CanvaPalette::Surface0);
        m_TabTypography->Render(box, 260.0f);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 24.0f);
    ImGui::BeginGroup();
    SectionLabel("Horizontal");
    const char* hLabels[] = { "Izq", "Centro", "Der" };
    SegmentedButtons("ribbonHAlign", hLabels, 3, &box.hAlign, 190.0f, 26.0f, CanvaPalette::Accent);
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::BeginGroup();
    SectionLabel("Vertical");
    const char* vLabels[] = { "Arriba", "Centro", "Abajo" };
    SegmentedButtons("ribbonVAlign", vLabels, 3, &box.vAlign, 190.0f, 26.0f, CanvaPalette::Accent);
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 24.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, CanvaPalette::Accent);
    ImGui::Checkbox("Auto-reducir si no cabe", &box.autoScale);
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    // El Indice es opcional (por defecto apagado) -- muestra solo la
    // referencia biblica (ej. "Genesis 1:1"), nunca el cuerpo del
    // versiculo, que siempre usa la caja de Letras. Este toggle solo
    // aparece editando el flag Indice.
    if (m_SelectedFlag == 1) {
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, CanvaPalette::Gold);
        ImGui::Checkbox("Mostrar indice al proyectar un versiculo", &m_Data.indexEnabled);
        ImGui::PopStyleColor();
        ImGui::EndGroup();
    }
}

void CanvaStyleEditor::RenderRibbonEffects(float /*width*/) {
    auto& box = SelectedBox();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (GhostButton("Editar efectos...", ImVec2(220.0f, 32.0f)))
        ImGui::OpenPopup("##effectsPopup");

    ImGui::SameLine(0.0f, 16.0f);
    int activeCount =
        (box.effects.bgEnabled ? 1 : 0) + (box.effects.borderEnabled ? 1 : 0) +
        (box.effects.shadowEnabled ? 1 : 0) + (box.effects.chromaticAberrationEnabled ? 1 : 0) +
        (box.effects.glowEnabled ? 1 : 0) + (box.effects.neonEnabled ? 1 : 0) +
        (box.effects.underlineEnabled ? 1 : 0);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("%d efecto(s) activo(s)", activeCount);
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("##effectsPopup")) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, CanvaPalette::Surface0);
        m_TabEffects->Render(box.effects, 300.0f);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
}

void CanvaStyleEditor::RenderRibbonBackground(float /*width*/) {
    auto& box = SelectedBox();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushStyleColor(ImGuiCol_CheckMark, CanvaPalette::Accent);
    ImGui::Checkbox("Fondo de pantalla propio de este recuadro", &box.bgMediaEnabled);
    ImGui::PopStyleColor();

    if (!box.bgMediaEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped(
            "Desactivado: por defecto el recuadro es transparente y se ve el "
            "fondo general de la presentacion detras del texto.");
        ImGui::PopStyleColor();
        return;
    }

    std::string current = box.bgMediaPath.empty()
        ? std::string("(ninguno)")
        : std::filesystem::path(box.bgMediaPath).filename().string();

    if (GhostButton(("Cambiar: " + current).c_str(), ImVec2(280.0f, 30.0f)))
        ImGui::OpenPopup("##bgPickerPopup");

    if (ImGui::BeginPopup("##bgPickerPopup")) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, CanvaPalette::Surface0);
        if (ImGui::Selectable("(ninguno / transparente)")) {
            box.bgMediaPath.clear();
            box.bgMediaEnabled = false;
        }
        ImGui::Separator();
        for (const auto& e : ListSongBackgrounds()) {
            if (!e.isImage) continue; // fondo de recuadro: solo imagenes estaticas por ahora
            bool sel = (box.bgMediaPath == e.fullPath);
            if (ImGui::Selectable(e.label.c_str(), sel))
                box.bgMediaPath = e.fullPath;
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::SetNextItemWidth(160.0f);
    float pct = box.bgMediaOpacity * 100.0f;
    if (ImGui::DragFloat("Opacidad##bgOpacity", &pct, 0.5f, 0.0f, 100.0f, "%.0f%%"))
        box.bgMediaOpacity = std::clamp(pct / 100.0f, 0.0f, 1.0f);
}

void CanvaStyleEditor::RenderCanvas(float availW, float availH) {
    float aspect = 1920.0f / 1080.0f;
    float cw = availW;
    float ch = cw / aspect;
    if (ch > availH) { ch = availH; cw = ch * aspect; }
    cw = std::max(cw, 100.0f);
    ch = std::max(ch, 60.0f);

    float offsetX = std::max(0.0f, (availW - cw) * 0.5f);
    if (offsetX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##styleCanvasArea", ImVec2(cw, ch), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_CanvasScreenPos  = ImGui::GetWindowPos();
    m_CanvasScreenSize = ImGui::GetWindowSize();

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImDrawList* fgDl = ImGui::GetForegroundDrawList();

    // Dibuja primero el flag NO seleccionado y despues el seleccionado, para
    // que si se superponen el seleccionado quede arriba (mas facil de
    // agarrar con el mouse) -- mismo criterio que la seleccion en
    // OverlayCanvasEditor.
    int other = 1 - m_SelectedFlag;
    RenderFlag(other, m_CanvasScreenPos, m_CanvasScreenSize, dl, fgDl);
    RenderFlag(m_SelectedFlag, m_CanvasScreenPos, m_CanvasScreenSize, dl, fgDl);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void CanvaStyleEditor::RenderFlag(int flagIdx, ImVec2 p0, ImVec2 canvasSize,
                                   ImDrawList* dl, ImDrawList* fgDl) {
    Core::TextBoxStyle& box = (flagIdx == 0) ? m_Data.lyrics : m_Data.index;
    const char* label       = (flagIdx == 0) ? "LETRAS" : "INDICE";
    ImVec4 flagColor        = (flagIdx == 0) ? CanvaPalette::Accent : CanvaPalette::Gold;
    bool   isDisabled       = (flagIdx == 1) && !m_Data.indexEnabled;

    float boxW = std::max(20.0f, box.sizeW * canvasSize.x);
    float boxH = std::max(20.0f, box.sizeH * canvasSize.y);
    ImVec2 tl = ImVec2(p0.x + box.posX * canvasSize.x - boxW * 0.5f,
                        p0.y + box.posY * canvasSize.y - boxH * 0.5f);
    ImVec2 br = ImVec2(tl.x + boxW, tl.y + boxH);

    if (!isDisabled && box.bgMediaEnabled && !box.bgMediaPath.empty()) {
        unsigned int tex = Core::PresentationCore::Get().GetBoxBgTexture(flagIdx == 0, box.bgMediaPath);
        if (tex != 0) {
            ImU32 tint = IM_COL32(255, 255, 255, (int)(std::clamp(box.bgMediaOpacity, 0.0f, 1.0f) * 255.0f));
            dl->AddImage((ImTextureID)(intptr_t)tex, tl, br, ImVec2(0, 0), ImVec2(1, 1), tint);
        }
    }

    dl->AddRect(tl, br, CanvaPalette::ToU32(ImVec4(flagColor.x, flagColor.y, flagColor.z, isDisabled ? 0.25f : 0.55f)),
        4.0f, 0, isDisabled ? 1.0f : 1.5f);
    if (isDisabled) {
        // Rayado diagonal simple para marcar visualmente "desactivado" --
        // el recuadro se puede seguir moviendo/redimensionando aunque el
        // indice este apagado, solo no se proyecta.
        dl->AddLine(tl, br, CanvaPalette::ToU32(ImVec4(flagColor.x, flagColor.y, flagColor.z, 0.20f)), 1.0f);
        dl->AddLine(ImVec2(br.x, tl.y), ImVec2(tl.x, br.y), CanvaPalette::ToU32(ImVec4(flagColor.x, flagColor.y, flagColor.z, 0.20f)), 1.0f);
    }

    // Texto de muestra, con el estilo real del recuadro (fuente/tamano/
    // color/alineacion/auto-escala/efectos) -- misma logica que
    // DrawTextBlock/DrawPublicContent usan al proyectar de verdad. El
    // Indice SOLO muestra la referencia (ej. "Genesis 1:1"), nunca el
    // cuerpo del versiculo -- eso va en la caja de Letras.
    {
        const char* sample = (flagIdx == 0) ? "Letra de\nla cancion" : "Genesis 1:1";

        float sc = canvasSize.x / 1920.0f;
        float displaySize = box.textSize * sc;

        ImFont* font = Core::PresentationCore::Get().GetImGuiFont(box.fontName, 60.0f);
        if (!font) font = ImGui::GetFont();

        if (box.autoScale) {
            while (displaySize > 6.0f) {
                ImVec2 ts = font->CalcTextSizeA(displaySize, FLT_MAX, boxW, sample);
                if (ts.y <= boxH && ts.x <= boxW) break;
                displaySize -= 0.5f;
            }
        }

        ImVec2 blockSz = font->CalcTextSizeA(displaySize, FLT_MAX, boxW, sample);

        float tx = tl.x;
        if      (box.hAlign == 1) tx += (boxW - blockSz.x) * 0.5f;
        else if (box.hAlign == 2) tx += boxW - blockSz.x;

        float ty = tl.y;
        if      (box.vAlign == 1) ty += (boxH - blockSz.y) * 0.5f;
        else if (box.vAlign == 2) ty += boxH - blockSz.y;

        float dimMul = isDisabled ? 0.35f : 1.0f;
        ImU32 textCol = IM_COL32(
            (int)(box.color[0] * 255), (int)(box.color[1] * 255),
            (int)(box.color[2] * 255), (int)(box.color[3] * 255 * dimMul));

        dl->PushClipRect(tl, br, true);
        DrawStyledText(dl, font, displaySize, ImVec2(tx, ty), textCol, sample, boxW, sc, box.effects);
        dl->PopClipRect();
    }

    // Rotulo (chip) inset en la esquina superior-izquierda -- dibujado con
    // ImDrawList directo (no ImGui::Badge) para no depender del cursor de
    // layout ni arriesgar quedar clippeado fuera del recuadro.
    {
        std::string chipLabel = isDisabled ? (std::string(label) + " (desactivado)") : label;
        ImVec2 lblSz  = ImGui::CalcTextSize(chipLabel.c_str());
        ImVec2 chipP0 = ImVec2(tl.x + 6.0f, tl.y + 6.0f);
        ImVec2 chipP1 = ImVec2(chipP0.x + lblSz.x + 12.0f, chipP0.y + lblSz.y + 6.0f);
        dl->AddRectFilled(chipP0, chipP1, IM_COL32(15, 15, 18, 210), 4.0f);
        dl->AddText(ImVec2(chipP0.x + 6.0f, chipP0.y + 3.0f), CanvaPalette::ToU32(flagColor), chipLabel.c_str());
    }

    bool isSelected = (m_SelectedFlag == flagIdx);
    if (isSelected)
        fgDl->AddRect(ImVec2(tl.x - 3.0f, tl.y - 3.0f), ImVec2(br.x + 3.0f, br.y + 3.0f),
            CanvaPalette::ToU32(flagColor), 4.0f, 0, 2.0f);

    ImGui::PushID(flagIdx + 500);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton("##flagHit", ImVec2(boxW, boxH));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    if (ImGui::IsItemActivated()) {
        m_SelectedFlag    = flagIdx;
        m_DraggingFlag    = flagIdx;
        m_DragStartMouse  = ImGui::GetIO().MousePos;
        m_DragStartPosX   = box.posX;
        m_DragStartPosY   = box.posY;
    }
    if (m_DraggingFlag == flagIdx && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = (mouse.x - m_DragStartMouse.x) / canvasSize.x;
        float dy = (mouse.y - m_DragStartMouse.y) / canvasSize.y;
        box.posX = std::clamp(m_DragStartPosX + dx, 0.0f, 1.0f);
        box.posY = std::clamp(m_DragStartPosY + dy, 0.0f, 1.0f);
    }
    if (ImGui::IsItemDeactivated()) m_DraggingFlag = -1;
    ImGui::PopID();

    if (isSelected) {
        RenderResizeHandle(flagIdx, tl, 0, fgDl);
        RenderResizeHandle(flagIdx, ImVec2(br.x, tl.y), 1, fgDl);
        RenderResizeHandle(flagIdx, ImVec2(tl.x, br.y), 2, fgDl);
        RenderResizeHandle(flagIdx, br, 3, fgDl);
    }
}

void CanvaStyleEditor::RenderResizeHandle(int flagIdx, ImVec2 handlePos, int corner, ImDrawList* fgDl) {
    Core::TextBoxStyle& box = (flagIdx == 0) ? m_Data.lyrics : m_Data.index;
    constexpr float kHandleR = 5.0f;
    constexpr float kHitR    = 9.0f;

    ImGui::PushID(flagIdx * 10 + corner + 900);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(ImVec2(handlePos.x - kHitR, handlePos.y - kHitR));
    ImGui::InvisibleButton("##resizeHandle", ImVec2(kHitR * 2.0f, kHitR * 2.0f));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

    fgDl->AddCircleFilled(handlePos, kHandleR, CanvaPalette::ToU32(CanvaPalette::Accent));
    fgDl->AddCircle(handlePos, kHandleR, IM_COL32(20, 20, 24, 255), 0, 1.5f);

    if (ImGui::IsItemActivated()) {
        m_ResizingFlag     = flagIdx;
        m_ResizeCorner     = corner;
        m_ResizeStartMouse = ImGui::GetIO().MousePos;
        m_ResizeStartPosX  = box.posX;
        m_ResizeStartPosY  = box.posY;
        m_ResizeStartSizeW = box.sizeW;
        m_ResizeStartSizeH = box.sizeH;
    }
    if (m_ResizingFlag == flagIdx && m_ResizeCorner == corner &&
        ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = (mouse.x - m_ResizeStartMouse.x) / m_CanvasScreenSize.x;
        float dy = (mouse.y - m_ResizeStartMouse.y) / m_CanvasScreenSize.y;

        float hw = m_ResizeStartSizeW * 0.5f, hh = m_ResizeStartSizeH * 0.5f;
        float startTLx = m_ResizeStartPosX - hw, startTLy = m_ResizeStartPosY - hh;
        float startBRx = m_ResizeStartPosX + hw, startBRy = m_ResizeStartPosY + hh;

        float movX = (corner == 1 || corner == 3) ? startBRx + dx : startTLx + dx;
        float movY = (corner == 2 || corner == 3) ? startBRy + dy : startTLy + dy;
        float fixX = (corner == 1 || corner == 3) ? startTLx      : startBRx;
        float fixY = (corner == 2 || corner == 3) ? startTLy      : startBRy;

        constexpr float kMinSize = 0.04f;
        box.sizeW = std::max(kMinSize, std::abs(movX - fixX));
        box.sizeH = std::max(kMinSize, std::abs(movY - fixY));
        box.posX  = std::clamp((movX + fixX) * 0.5f, 0.0f, 1.0f);
        box.posY  = std::clamp((movY + fixY) * 0.5f, 0.0f, 1.0f);
    }
    if (ImGui::IsItemDeactivated()) m_ResizingFlag = -1;
    ImGui::PopID();
}

} // namespace ProyecThor::UI
