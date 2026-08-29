#include "MonitorView.h"
#include "MonitorTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/UIManager.h"

// =============================================================================
//  MonitorVideoPanels.cpp
//  RenderPreviewMonitor y RenderLiveMonitor.
// =============================================================================

namespace ProyecThor::UI {

// Alias corto para evitar colision con constantes de MonitorDesign.h
// que tambien viven en ProyecThor::UI
namespace MT = MonitorTheme;
using namespace Design;
using namespace Components;

// =============================================================================
//  Monitor de Preview (PVW)
// =============================================================================
// Badge chico arriba a la izquierda -- mismo look que el que dibuja
// DrawVideoFrame (MonitorUIHelpers.cpp), duplicado aca porque Imagen/Audio no
// pasan por esa funcion (no tienen textura de video que mostrar).
static void DrawPreviewBadge(ImVec2 winPos, const char* label, ImVec4 accent)
{
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      labelSz = ImGui::CalcTextSize(label);
    float bx = winPos.x + 8.0f;
    float by = winPos.y + 8.0f;
    float bw = labelSz.x + 12.0f;
    float bh = labelSz.y + 6.0f;

    ImVec4 badgeBg = { accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.92f };
    dl->AddRectFilled({ bx, by }, { bx + bw, by + bh },
        ImGui::ColorConvertFloat4ToU32(badgeBg), 4.0f);
    dl->AddRect({ bx, by }, { bx + bw, by + bh },
        ImGui::ColorConvertFloat4ToU32({ accent.x, accent.y, accent.z, 0.70f }), 4.0f, 0, 1.0f);
    dl->AddText({ bx + 6.0f, by + 3.0f }, ImGui::ColorConvertFloat4ToU32(accent), label);
}

// Icono de "expandir" (4 esquinas en L) para el boton de pantalla completa
// -- no hay textura registrada para esto (ver LoadAppIcon en main.cpp), asi
// que se dibuja a mano, mismo criterio que el resto de iconos custom del
// proyecto que no tienen texture key (DrawSpeakerToggleIcon, DrawIcon_
// Shuffle en Audio.cpp, etc).
static void DrawExpandIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 col)
{
    float s   = size * 0.5f;
    float arm = size * 0.42f;
    float th  = 1.6f;

    ImVec2 tl(center.x - s, center.y - s), tr(center.x + s, center.y - s);
    ImVec2 bl(center.x - s, center.y + s), br(center.x + s, center.y + s);

    dl->AddLine(tl, { tl.x + arm, tl.y }, col, th); dl->AddLine(tl, { tl.x, tl.y + arm }, col, th);
    dl->AddLine(tr, { tr.x - arm, tr.y }, col, th); dl->AddLine(tr, { tr.x, tr.y + arm }, col, th);
    dl->AddLine(bl, { bl.x + arm, bl.y }, col, th); dl->AddLine(bl, { bl.x, bl.y - arm }, col, th);
    dl->AddLine(br, { br.x - arm, br.y }, col, th); dl->AddLine(br, { br.x, br.y - arm }, col, th);
}

void MonitorView::RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h)
{
    auto  sel    = Core::PresentationCore::Get().PeekSelection();
    bool  hasTex = false;
    if (sel.type == Core::ItemType::Image)
        hasTex = (m_ImageView.GetTextureID() != 0);
    else if (sel.type == Core::ItemType::Audio)
        hasTex = true; // el disco animado siempre "tiene señal" visual
    else
        hasTex = (player && player->GetTextureID() != nullptr);
    float borderAlpha = hasTex ? 0.55f : 0.18f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(MT::k_PrevAccent.x, MT::k_PrevAccent.y, MT::k_PrevAccent.z, borderAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, hasTex ? 1.5f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_RLg);

    ImGui::BeginChild("##mon_prev", { w, h }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();
    dl->AddRectFilledMultiColor(
        wp,
        { wp.x + w, wp.y + h },
        IM_COL32(0, 0, 0, 110), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0),   IM_COL32(0, 0, 0, 110));

    if (sel.type == Core::ItemType::Image) {
        m_ImageView.Render(w, h);
        DrawPreviewBadge(wp, "PVW", MT::k_PrevAccent);
    } else if (sel.type == Core::ItemType::Audio) {
        UpdateSpinningDisc(m_DiscState, m_PreviewPlaying ? 1.4f : 0.0f, ImGui::GetIO().DeltaTime);
        float  radius = std::min(w, h) * 0.32f;
        ImVec2 center = { wp.x + w * 0.5f, wp.y + h * 0.5f };
        DrawSpinningDisc(dl, center, radius, m_DiscState, m_CurrentAudioArt);
        DrawPreviewBadge(wp, "PVW", MT::k_PrevAccent);
    } else {
        DrawVideoFrame(player, w, h, "SIN SEÑAL", "PVW", MT::k_PrevAccent, false);
    }

    // ── Boton "pantalla completa" ────────────────────────────────────────
    // Solo con contenido de verdad (hasTex) -- sin nada que mostrar no hay
    // nada que agrandar.
    if (hasTex)
    {
        const float btnSz = 24.0f;
        ImVec2 btnPos(wp.x + w - btnSz - 8.0f, wp.y + 8.0f);
        ImGui::SetCursorScreenPos(btnPos);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.24f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        bool clicked = ImGui::Button("##mon_prev_fs_btn", { btnSz, btnSz });
        DrawExpandIcon(dl, { btnPos.x + btnSz * 0.5f, btnPos.y + btnSz * 0.5f }, btnSz * 0.6f,
                      ImGui::ColorConvertFloat4ToU32(MT::k_PrevAccent));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ver en pantalla completa");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (clicked)
            RequestPreviewFullscreen(player);
    }

    // ── HUD flotante auto-oculto (dock central + barra de transporte) ─────
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseInPreview = (mousePos.x >= wp.x && mousePos.x <= wp.x + w &&
                           mousePos.y >= wp.y && mousePos.y <= wp.y + h);
    bool isInteracting = ImGui::IsAnyItemActive() || m_ShowEqPopup;
    if (mouseInPreview || isInteracting)
        m_HudIdleTimer = 0.0f;
    else
        m_HudIdleTimer += ImGui::GetIO().DeltaTime;

    float targetAlpha = (m_HudIdleTimer < 2.5f) ? 1.0f : 0.0f;
    m_HudAlpha += (targetAlpha - m_HudAlpha) * std::min(1.0f, ImGui::GetIO().DeltaTime * 7.0f);

    if (m_HudAlpha > 0.01f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_HudAlpha);

        // 1. Dock flotante central-derecha: TRANSMITIR (->) y BUCLE (<->)
        const float dockW = 58.0f;
        const float dockH = 106.0f;
        const float dockX = wp.x + w - dockW - 12.0f;
        const float dockY = wp.y + std::max(38.0f, (h - dockH) * 0.42f);
        ImVec2 dockMin(dockX, dockY);
        ImVec2 dockMax(dockX + dockW, dockY + dockH);

        dl->AddRectFilled(dockMin, dockMax, MT::ColAf(MT::k_Bg0, 0.76f * m_HudAlpha), MT::k_R);
        dl->AddRect(dockMin, dockMax, MT::ColAf(MT::k_BorderSubtle, 0.85f * m_HudAlpha), MT::k_R, 0, 1.0f);

        ImGui::SetCursorScreenPos({ dockMin.x + 3.0f, dockMin.y + 4.0f });
        RenderCenterColumn(dockW - 6.0f, dockH - 8.0f, player);

        // 2. Barra flotante inferior: Reproductor de Preview (Timeline + Controles + EQ)
        const float barH = 124.0f;
        const float barW = std::max(100.0f, w - 24.0f);
        const float barX = wp.x + 12.0f;
        const float barY = wp.y + h - barH - 8.0f;
        ImVec2 barMin(barX, barY);
        ImVec2 barMax(barX + barW, barY + barH);

        dl->AddRectFilled(barMin, barMax, MT::ColAf(MT::k_Bg0, 0.78f * m_HudAlpha), MT::k_RLg);
        dl->AddRect(barMin, barMax, MT::ColAf(MT::k_BorderSubtle, 0.90f * m_HudAlpha), MT::k_RLg, 0, 1.0f);

        ImGui::SetCursorScreenPos({ barMin.x + 4.0f, barMin.y + 2.0f });
        RenderPreviewControls(player, barW - 8.0f);

        ImGui::PopStyleVar();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// =============================================================================
//  Pantalla completa del Preview -- reusa UIManager::EnterFullscreenEditor
//  (mismo mecanismo que el editor de Estilos/Overlays: toma TODO el area de
//  contenido, oculta Biblioteca/Home/Vista en Vivo/Diseño). El contenido
//  real (video/disco/imagen) se dibuja grande + una barra de controles
//  inferior que se auto-oculta con inactividad del mouse, mismo criterio
//  "estilo reproductor" que YouTube/VLC fullscreen.
// =============================================================================

void MonitorView::RequestPreviewFullscreen(Core::VLCBasePlayer* player)
{
    if (!m_UIManagerRef) return;

    // Pedido explicito: ademas de tomar toda la ventana de la app (ver
    // hideToolbar abajo), la ventana del SISTEMA OPERATIVO tambien pasa a
    // fullscreen, igual que si el operador apretara F11. Si ya estaba en
    // fullscreen de antes, no se toca nada (ni al entrar ni al salir, ver
    // ExitPreviewFullscreen) -- m_EnteredOSFullscreen recuerda si fue este
    // flujo el que la puso.
    m_EnteredOSFullscreen = !m_UIManagerRef->IsWindowFullscreen();
    if (m_EnteredOSFullscreen)
        m_UIManagerRef->ToggleFullscreen();

    // hideToolbar=true (pedido explicito): a diferencia de Overlay/Estilos,
    // el Preview debe usar de verdad TODA la pantalla, sin dejarle espacio
    // arriba a la toolbar de modos.
    m_UIManagerRef->EnterFullscreenEditor([this, player]() {
        RenderPreviewFullscreenContent(player);
    }, /*hideToolbar=*/true);
}

void MonitorView::ExitPreviewFullscreen()
{
    if (!m_UIManagerRef) return;
    m_UIManagerRef->ExitFullscreenEditor();
    if (m_EnteredOSFullscreen) {
        m_UIManagerRef->ToggleFullscreen();
        m_EnteredOSFullscreen = false;
    }
}

void MonitorView::RenderFullscreenToolbar(Core::VLCBasePlayer* player, ImVec2 avail)
{
    // ── Auto-ocultar con inactividad del mouse ───────────────────────────
    static float  idleTimer    = 0.0f;
    static ImVec2 lastMousePos = ImGui::GetIO().MousePos;

    ImVec2 mouse = ImGui::GetIO().MousePos;
    float  moved = std::fabs(mouse.x - lastMousePos.x) + std::fabs(mouse.y - lastMousePos.y);
    if (moved > 0.5f) idleTimer = 0.0f;
    else              idleTimer += ImGui::GetIO().DeltaTime;
    lastMousePos = mouse;

    constexpr float kHideAfter = 2.5f;
    constexpr float kFadeDur   = 0.35f;
    float alpha = (idleTimer > kHideAfter)
        ? std::clamp(1.0f - (idleTimer - kHideAfter) / kFadeDur, 0.0f, 1.0f)
        : 1.0f;
    if (alpha <= 0.01f) return; // nada que dibujar -- reaparece solo al mover el mouse

    const float  barH   = MT::k_ControlsH;
    const float  padX   = 24.0f;
    ImVec2       winPos = ImGui::GetWindowPos();

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::SetCursorScreenPos({ winPos.x + padX, winPos.y + avail.y - barH - 16.0f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(MT::k_Bg1.x, MT::k_Bg1.y, MT::k_Bg1.z, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { MT::k_PadLg, MT::k_Pad });

    ImGui::BeginChild("##mon_prev_fs_toolbar", { avail.x - padX * 2.0f, barH }, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const float innerW = (avail.x - padX * 2.0f) - MT::k_PadLg * 2.0f;

    {
        ImVec2 headerPos = ImGui::GetCursorScreenPos();
        DrawStatusDot(ImGui::GetWindowDrawList(), { headerPos.x + 7.0f, headerPos.y + 9.0f },
                      4.0f, MT::k_PrevAccent, m_PreviewPlaying);
        ImGui::SetCursorPosX(MT::k_PadLg + 18.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
        ImGui::TextUnformatted("PREVIEW -- PANTALLA COMPLETA");
        ImGui::PopStyleColor();

        const float optBtnW = 100.0f, audioBtnW = 22.0f, closeBtnW = 22.0f, volSliderW = 90.0f;
        const float rightClusterW = optBtnW + 10.0f + audioBtnW + 8.0f + volSliderW + 10.0f + closeBtnW;
        ImGui::SameLine();
        ImGui::SetCursorPosX(MT::k_PadLg + innerW - rightClusterW);

        // "Opciones de reproduccion" -- solo tiene FSR por ahora, pero es un
        // menu propio (no un toggle suelto) para poder sumar mas opciones de
        // reproduccion del Preview a futuro sin reacomodar el toolbar.
        ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_NeutBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextDim);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("Opciones##mon_prev_fs_opts", { optBtnW, audioBtnW }))
            ImGui::OpenPopup("##mon_prev_fs_opts_popup");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        if (ImGui::BeginPopup("##mon_prev_fs_opts_popup")) {
            ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
            ImGui::TextUnformatted("OPCIONES DE REPRODUCCION");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (ImGui::Checkbox("Activar FSR para Preview", &m_PreviewFSREnabled))
                m_PreviewFSR.SetEnabled(m_PreviewFSREnabled);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reescala el video con FidelityFX Super Resolution (EASU+RCAS) "
                                  "al tamaño real de pantalla completa, en vez de estirarlo liso. "
                                  "Solo hace diferencia si el video fuente es mas chico que tu pantalla.");

            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 10);
        RenderPreviewAudioToggle(player, audioBtnW);

        // Slider de volumen -- pedido explicito para la vista de pantalla
        // completa (la barra acoplada normal solo tiene el toggle). Solo
        // tiene efecto mientras el audio de Preview esta activado.
        ImGui::SameLine(0, 8);
        ImGui::BeginDisabled(!m_PreviewAudioEnabled);
        if (BMSlider("##mon_prev_fs_vol", &m_PreviewVolume, 0.0f, 1.0f, "",
                     MT::k_PrevTrack, MT::k_PrevGrab,
                     { MT::k_PrevGrab.x * 1.2f, MT::k_PrevGrab.y * 1.2f, MT::k_PrevGrab.z * 1.2f, 1.0f },
                     volSliderW) && player)
        {
            player->SetVolume(static_cast<int>(m_PreviewVolume * 100.0f));
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, 10);
        ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_NeutBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextDim);
        if (ImGui::Button("X##mon_prev_fs_close", { closeBtnW, closeBtnW }))
            ExitPreviewFullscreen();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Salir de pantalla completa (Esc)");
        ImGui::PopStyleColor(4);
    }

    DrawAccentLine(innerW, MT::k_PrevAccentDim, 1.0f);
    RenderTransportRow(player, innerW);

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void MonitorView::RenderPreviewFullscreenContent(Core::VLCBasePlayer* player)
{
    // Sin offset de railH: EnterFullscreenEditor(..., hideToolbar=true)
    // saca la toolbar de modos por completo (ver UIManager::RenderAll), asi
    // que aca no hace falta dejarle espacio -- ocupa TODO el viewport,
    // pantalla completa real (pedido explicito).
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Begin("##mon_prev_fullscreen", nullptr, kFlags);

    ImVec2      avail  = ImGui::GetContentRegionAvail();
    ImVec2      winPos = ImGui::GetCursorScreenPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();

    auto sel = Core::PresentationCore::Get().PeekSelection();
    if (sel.type == Core::ItemType::Image) {
        m_ImageView.Render(avail.x, avail.y);
    } else if (sel.type == Core::ItemType::Audio) {
        UpdateSpinningDisc(m_DiscState, m_PreviewPlaying ? 1.4f : 0.0f, ImGui::GetIO().DeltaTime);
        float  radius = std::min(avail.x, avail.y) * 0.28f;
        ImVec2 center  = { winPos.x + avail.x * 0.5f, winPos.y + avail.y * 0.5f };
        DrawSpinningDisc(dl, center, radius, m_DiscState, m_CurrentAudioArt);
    } else {
        // showBadge=false: el badge "PVW" sobra aca, el toolbar ya dice
        // "PREVIEW -- PANTALLA COMPLETA". FSR opt-in (ver "Opciones de
        // reproduccion" en RenderFullscreenToolbar / m_PreviewFSREnabled).
        DrawVideoFrame(player, avail.x, avail.y, "SIN SEÑAL", "PVW", MT::k_PrevAccent, false,
                      /*showBadge=*/false, m_PreviewFSREnabled ? &m_PreviewFSR : nullptr);
    }

    RenderFullscreenToolbar(player, avail);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ExitPreviewFullscreen();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace ProyecThor::UI