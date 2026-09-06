#include "LibraryModals.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "LibrarySongs.h"
#include "frontend/ui/DesignSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

static constexpr int kCat_Songs  = 0;
static constexpr int kCat_Videos = 1;
static constexpr int kCat_Bibles = 3;

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  RenderRenameModal
// =============================================================================
void RenderRenameModal(LibraryContext& ctx)
{
    if (ctx.showRenameModal) ImGui::OpenPopup("RenombrarModal##lib");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 440.f, 0.f });

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.078f, 0.078f, 0.082f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(20.f, 16.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8.f, 8.f));

    bool open = ctx.showRenameModal;
    if (ImGui::BeginPopupModal("RenombrarModal##lib", &open,
                               ImGuiWindowFlags_NoSavedSettings |
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
        ImGui::TextUnformatted(ctx.renameIsURL ? "Editar URL" : "Renombrar archivo");
        ImGui::PopStyleColor();

        AccentSep(ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::Text("Original: %s", ctx.renameOldName.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::TextUnformatted("Nuevo nombre:");
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
        ImGui::PushStyleColor(ImGuiCol_Border,         ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));

        bool pressEnter = ImGui::InputText(
            "##rename_input", ctx.renameBuffer, 512,
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if (!ctx.renameIsURL && !ctx.renameExtension.empty()) {
            ImGui::SameLine(0.f, 6.f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            ImGui::TextUnformatted(ctx.renameExtension.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        AccentSep(ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
        ImGui::Spacing();

        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 4.f));
            const float avail = ImGui::GetContentRegionAvail().x;
            const float sp    = ImGui::GetStyle().ItemSpacing.x;
            const float bw2   = std::floor((avail - sp) * 0.5f);

            bool doRename = pressEnter ||
                PillButton("Aceptar",  { bw2, 36.f },
                           k_BtnGreen,   k_BtnGreenH,   k_BtnGreenA,   k_BtnGreenT);
            ImGui::SameLine();
            bool doCancel =
                PillButton("Cancelar", { bw2, 36.f },
                           k_BtnNeutral, k_BtnNeutralH, k_BtnNeutralA, k_BtnNeutralT) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape);

            ImGui::PopStyleVar();

            if (doCancel) {
                ctx.showRenameModal = false;
                ImGui::CloseCurrentPopup();
            }

            if (doRename) {
                std::string newStem(ctx.renameBuffer);
                std::string newName = ctx.renameIsURL
                    ? newStem
                    : newStem + ctx.renameExtension;

                if (!newStem.empty() && newName != ctx.renameOldName) {
                    if (ctx.renameIsURL) {
                        if (ctx.renameURLIndex >= 0 &&
                            ctx.renameURLIndex < (int)ctx.streamURLs.size()) {
                            ctx.streamURLs[ctx.renameURLIndex] = newName;
                            ctx.saveStreamURLs();
                            ctx.selectedURLIndex = -1;
                        }
                    } else {
                        const std::string& base = GetAssetsPath();
                        std::string folder;
                        switch (ctx.currentCategoryInt) {
                            case kCat_Songs:     folder = base + "/songs/";     break;
                            case kCat_Videos:    folder = base + "/videos/";    break;
                            case 2:              folder = base + "/images/";    break;
                            case kCat_Bibles:    folder = base + "/bibles/";    break;
                            case 4:              folder = base + "/documents/"; break;
                            default:             folder = base + "/audio/";     break;
                        }
                        if (ctx.currentCategoryInt == kCat_Videos)
                            Core::PresentationCore::Get().StopBackgroundMedia();

                        fs::path oldPath = U8Path(folder) / U8Path(ctx.renameOldName);
                        fs::path newPath = U8Path(folder) / U8Path(newName);
                        try {
                            fs::rename(oldPath, newPath);
                            // Sin esto, renombrar una cancion deja el autor/
                            // etiquetas/estilo-fondo preset/color de estrofa/
                            // meta.json y las referencias en playlists
                            // huerfanos bajo el nombre de archivo viejo (ver
                            // LibrarySongs::MigrateSongSidecars).
                            if (ctx.currentCategoryInt == kCat_Songs)
                                MigrateSongSidecars(ctx.renameOldName, newName);
                            ctx.selectedIndex = -1;
                            ctx.refreshList();
                        } catch (const std::exception& e) {
                            std::cerr << "[RenameModal] Error: " << e.what() << '\n';
                        }
                    }
                }
                ctx.showRenameModal = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    if (!open) ctx.showRenameModal = false;
}

// =============================================================================
//  RenderDefaultStyleCombo
//  Muestra el combo de estilo por defecto para la categoría activa.
//  Cuando el usuario elige un estilo en el combo, lo persiste y además lo
//  aplica de inmediato si hay un item seleccionado, para que el cambio sea
//  visible en el proyector sin tener que reseleccionar la canción o biblia.
//
//  trailingReserve permite dejar libre un espacio a la derecha del combo
//  (por ejemplo para el botón de "Actualizar" en la misma fila, alineado
//  a la esquina). Se implementa con el truco de ancho negativo de ImGui:
//  SetNextItemWidth(-N) hace que el widget termine N pixeles antes del
//  borde derecho disponible.
// =============================================================================
void RenderDefaultStyleCombo(LibraryContext& ctx, float trailingReserve)
{
    if (ctx.currentCategoryInt != kCat_Songs &&
        ctx.currentCategoryInt != kCat_Bibles)
        return;

    Core::ItemType itemType = (ctx.currentCategoryInt == kCat_Songs)
        ? Core::ItemType::Song
        : Core::ItemType::Bible;

    std::string current =
        Core::PresentationCore::Get().GetCategoryDefaultStyle(itemType);
    std::vector<std::string> names =
        Core::PresentationCore::Get().GetSavedStyleNames();

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    ImGui::TextUnformatted("Estilo por defecto:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 8.f);

    const char* preview = current.empty() ? "(ninguno)" : current.c_str();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.f, 5.f));

    ImGui::SetNextItemWidth(trailingReserve > 0.f ? -trailingReserve : -1.f);

    if (ImGui::BeginCombo("##defstyle_combo", preview))
    {
        bool noneSelected = current.empty();
        ImGui::PushStyleColor(ImGuiCol_Text,
            noneSelected ? ImGui::ColorConvertU32ToFloat4(DS::AccentLight)
                         : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        if (ImGui::Selectable("(ninguno)", noneSelected))
            Core::PresentationCore::Get().SetCategoryDefaultStyle(itemType, "");
        ImGui::PopStyleColor();

        ImGui::Separator();

        if (names.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextUnformatted("  Sin estilos guardados");
            ImGui::PopStyleColor();
        }

        for (const auto& name : names) {
            bool sel = (name == current);
            ImGui::PushStyleColor(ImGuiCol_Text,
                sel ? ImGui::ColorConvertU32ToFloat4(DS::AccentLight)
                    : ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                  ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Header,
                                  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));

            if (ImGui::Selectable(name.c_str(), sel)) {
                Core::PresentationCore::Get().SetCategoryDefaultStyle(itemType, name);

                // Si hay un item activo en el proyector, aplica el nuevo estilo
                // de inmediato para que el cambio sea visible sin tener que
                // reseleccionar la canción o biblia.
                if (ctx.selectedIndex >= 0)
                    ctx.applyStyle(name);
            }
            ImGui::PopStyleColor(3);

            if (sel) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::AccentLight));
                ImGui::TextUnformatted("*");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

} // namespace ProyecThor::Library