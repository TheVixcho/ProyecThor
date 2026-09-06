#include "TabTypography.h"
#include "backend/core/AppPaths.h"
#include "backend/core/PresentationCore.h"
#include "DesignSystem.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <array>
#include <memory>
#endif
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

#ifndef _WIN32
// Selector de archivos para Linux/macOS via zenity/kdialog.
static std::string OpenFontFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar fuente\" "
        "--file-filter=\"Fuentes | *.ttf *.otf *.ttc\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.ttf *.otf *.ttc|Fuentes\" 2>/dev/null"
    };

    for (const char* cmd : commands) {
        std::array<char, 1024> buffer{};
        std::string result;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;

        while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
            result += buffer.data();

        int status = pclose(pipe);
        if (status != 0) continue;

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        if (!result.empty())
            return result;
    }
    return {};
}
#endif

TabTypography::TabTypography(std::vector<std::string>* fontList,
                             OnFontImportedCallback onFontImported)
    : m_FontList(fontList)
    , m_OnFontImported(std::move(onFontImported))
{
}

void TabTypography::Render(Core::TextBoxStyle& box, float colWidth) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    CanvaStyleEditor::Badge("TIPOGRAFIA", CanvaPalette::Accent);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    RenderFontSelector(box, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderColorPicker(box, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderSizeSlider(box, colWidth);
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    RenderAutoScaleCheckbox(box);
}

void TabTypography::RenderFontSelector(Core::TextBoxStyle& box, float colWidth) {
    CanvaStyleEditor::SectionLabel("Fuente");

    const float importBtnH = 26.0f;

    ImGui::PushStyleColor(ImGuiCol_Button,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  CanvaPalette::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text,          CanvaPalette::Accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.0f, 6.0f));

    if (ImGui::Button("+ Importar fuente", ImVec2(colWidth, importBtnH)))
        ImportFont();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    auto& core = Core::PresentationCore::Get();

    const int   cols     = 2;
    const float gap      = 6.0f;
    const float cardW    = (colWidth - gap * (cols - 1)) / (float)cols;
    const float cardH    = 46.0f;
    const float previewSz = 17.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface0);
    ImGui::BeginChild("##fontGrid", ImVec2(colWidth, 172.0f), true);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    std::vector<std::string> allFonts = { "Predeterminada" };
    if (m_FontList) allFonts.insert(allFonts.end(), m_FontList->begin(), m_FontList->end());

    for (int i = 0; i < (int)allFonts.size(); i++) {
        if (i % cols != 0) ImGui::SameLine(0.0f, gap);

        const std::string& name = allFonts[i];
        bool selected = (box.fontName == name);

        ImGui::PushID(i);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = { p0.x + cardW, p0.y + cardH };

        ImGui::InvisibleButton("##fontCard", { cardW, cardH });
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        ImU32 bg = selected ? CanvaPalette::ToU32(ImVec4(CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.22f))
                             : hovered ? CanvaPalette::ToU32(CanvaPalette::Surface2)
                                       : CanvaPalette::ToU32(CanvaPalette::Surface1);
        ImU32 border = selected ? CanvaPalette::ToU32(CanvaPalette::Accent) : CanvaPalette::ToU32(CanvaPalette::Border);

        dl->AddRectFilled(p0, p1, bg, 4.0f);
        dl->AddRect(p0, p1, border, 4.0f, 0, selected ? 1.6f : 1.0f);

        ImFont* previewFont = core.GetImGuiFont(name, previewSz);
        ImU32   textCol     = CanvaPalette::ToU32(selected ? CanvaPalette::Accent : CanvaPalette::Text);
        ImVec2  tsz = previewFont ? previewFont->CalcTextSizeA(previewSz, FLT_MAX, cardW - 12.0f, name.c_str())
                                  : ImGui::CalcTextSize(name.c_str());
        ImVec2  tpos = { p0.x + (cardW - std::min(tsz.x, cardW - 12.0f)) * 0.5f, p0.y + (cardH - tsz.y) * 0.5f };
        if (previewFont)
            dl->AddText(previewFont, previewSz, tpos, textCol, name.c_str(), nullptr, cardW - 12.0f);
        else
            dl->AddText(tpos, textCol, name.c_str());

        if (clicked) box.fontName = name;
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (m_ImportMsgTimer > 0.0f) {
        m_ImportMsgTimer -= ImGui::GetIO().DeltaTime;
        bool isOk = (m_ImportStatus == "ok");
        ImGui::PushStyleColor(ImGuiCol_Text,
            isOk ? CanvaPalette::Green : ImVec4(0.93f, 0.26f, 0.36f, 1.0f));
        ImGui::TextUnformatted(isOk
            ? "Fuente importada correctamente"
            : m_ImportStatus.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
    }
}

void TabTypography::RenderColorPicker(Core::TextBoxStyle& box, float colWidth) {
    CanvaStyleEditor::SectionLabel("Color del texto");
    ImGui::SetNextItemWidth(colWidth);
    ImGui::ColorEdit4("##editColor", box.color,
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_DisplayRGB);
}

void TabTypography::RenderSizeSlider(Core::TextBoxStyle& box, float colWidth) {
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("Tamaño   %.0f px", box.textSize);
    ImGui::PopStyleColor();

    DS::ModernSlider("##editSize", &box.textSize, 10.0f, 300.0f, colWidth);
}

void TabTypography::RenderAutoScaleCheckbox(Core::TextBoxStyle& box) {
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      CanvaPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::Checkbox("Auto-reducir si el texto no cabe", &box.autoScale);
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextWrapped(
        "Cuando el texto supera la zona segura, el tamaño se reduce automáticamente "
        "hasta que entre. Útil para presentaciones con contenido variable.");
    ImGui::PopStyleColor();
}

void TabTypography::ImportFont() {
    std::string selectedPath;

#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.hwndOwner           = NULL;
    ofn.lpstrFilter         = "Fuentes\0*.ttf;*.otf;*.ttc\0Todos los archivos\0*.*\0";
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return;
    selectedPath = filename;
#else
    selectedPath = OpenFontFileDialogUnix();
    if (selectedPath.empty()) return;
#endif

    try {
        std::filesystem::path fontsDir =
            std::filesystem::path(GetAssetsPath()) / "fonts";

        std::filesystem::create_directories(fontsDir);

        std::filesystem::path src(selectedPath);
        std::filesystem::path dst = fontsDir / src.filename();

        std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);

        std::string fontName = src.stem().string();

        if (m_FontList) {
            bool exists = std::any_of(
                m_FontList->begin(), m_FontList->end(),
                [&fontName](const std::string& f) { return f == fontName; });
            if (!exists)
                m_FontList->push_back(fontName);
        }

        if (m_OnFontImported)
            m_OnFontImported(dst.string());

        m_ImportStatus   = "ok";
        m_ImportMsgTimer = 3.0f;

    } catch (const std::exception& e) {
        m_ImportStatus   = std::string("Error: ") + e.what();
        m_ImportMsgTimer = 4.0f;
    }
}

} // namespace ProyecThor::UI