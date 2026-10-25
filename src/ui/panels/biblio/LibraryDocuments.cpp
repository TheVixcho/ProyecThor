#include "LibraryDocuments.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/bin/StyleGeneralApp.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;
namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  GlassIconButton — boton con icono de StyleGeneralApp (fallback a glifo corto)
//  Mismo helper que en LibrarySongs.cpp / LibraryVideos.cpp.
//
//  FIX (tamaños): antes el icono se recortaba con el mismo "pad" en X e Y,
//  lo que en botones anchos y bajos (footer, ancho/3) dejaba un rectangulo
//  horizontal en vez de un icono cuadrado -> se veia estirado/deforme.
//  Ahora se calcula un cuadrado a partir del lado MENOR del boton y se
//  centra, sin importar que tan ancho o bajo sea el boton.
// =============================================================================
static bool GlassIconButton(const char* id,
                             const char* iconKey,
                             const char* fallbackGlyph,
                             const char* tooltip,
                             ImVec2      size,
                             ImVec4      tint = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary))
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
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

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

void RenderDocumentSection(LibraryContext& ctx, UI::DocumentView& documentView)
{
    const std::string& base     = GetAssetsPath();
    const std::string  docsPath = base + "/documents";

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);

    if (ImGui::BeginChild("##doc_list", { 0.f, 165.0f }, true))
    {
        if (ctx.items.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.38f, 0.55f, 1.0f));
            ImGui::TextUnformatted("  Sin documentos. Usa Importar.");
            ImGui::PopStyleColor();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));
        bool deletedInLoop = false;

        for (int n = 0; n < (int)ctx.items.size(); n++)
        {
            const bool sel = (ctx.selectedIndex == n);

            bool clicked = DS::GlassListRow(ctx.items[n].c_str(), sel);

            if (clicked)
            {
                ctx.selectedIndex = n;
                fs::path docDir   = U8Path(docsPath) / U8Path(ctx.items[n]);
                std::string originalFile;

                if (fs::exists(docDir) && fs::is_directory(docDir)) {
                    for (const auto& entry : fs::directory_iterator(docDir)) {
                        std::string ext = ProyecThor::Library::PathToUtf8(entry.path().extension());
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                                       [](unsigned char c){ return (char)::tolower(c); });
                        if (ext == ".pdf" || ext == ".pptx" ||
                            ext == ".ppt" || ext == ".odp") {
                            originalFile = ProyecThor::Library::PathToUtf8(entry.path());
                            break;
                        }
                    }
                }

                if (!originalFile.empty() && originalFile != ctx.loadedDocPath) {
                    ctx.loadedDocPath = originalFile;
                    std::string cacheDir = ProyecThor::Library::PathToUtf8(docDir) + "/cache";
                    documentView.LoadDocument(originalFile, cacheDir);
                }
            }

            if (ImGui::BeginPopupContextItem(("##ctx_doc" + std::to_string(n)).c_str()))
            {
                if (ImGui::MenuItem("Renombrar")) {
                    ctx.renameOldName  = ctx.items[n];
                    ctx.renameIsURL    = false;
                    ctx.renameURLIndex = -1;
                    ctx.selectedIndex  = n;
                    std::string stem = SplitExtension(ctx.items[n], ctx.renameExtension);
                    memset(ctx.renameBuffer, 0, 512);
                    strncpy(ctx.renameBuffer, stem.c_str(), 511);
                    ctx.showRenameModal = true;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.40f, 1.0f));
                if (ImGui::MenuItem("Eliminar")) {
                    ImGui::PopStyleColor();
                    ctx.selectedIndex = n;
                    ImGui::EndPopup();
                    ctx.deleteSelectedItem();
                    deletedInLoop = true;
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (deletedInLoop) break;
        }

        ImGui::PopStyleVar(); // ItemSpacing
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

        if (GlassIconButton("importDoc", "upload_file", "^", "Importar", btnSize))
            ctx.importFile();
        ImGui::SameLine();
        // NOTA: no existe "refresh.png" en assets/icons/ui, se usa "repeat"
        // (icono ciclico, ya cargado) que visualmente cumple la misma funcion.
        if (GlassIconButton("refreshDoc", "repeat", "R", "Actualizar", btnSize))
            ctx.refreshList();
        ImGui::SameLine();
        if (GlassIconButton("deleteDoc", "delete", "X", "Eliminar", btnSize,
                            ImGui::ColorConvertU32ToFloat4(DS::DangerColor)))
            ctx.deleteSelectedItem();
    }

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();
    documentView.Render();
}

} // namespace ProyecThor::Library