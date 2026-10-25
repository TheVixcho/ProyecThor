#include <GL/glew.h>
#include "UIManager.h"
#include "core/PresentationCore.h"
#include "ui/framework/TextEffectsRenderer.h"
#include "core/PerformanceGovernor.h"
#include "../toolbar/ConfigPanel.h"
#include "panels/HomePanel.h"
#include "panels/LibraryPanel.h"
#include "panels/StylesHubPanel.h"
#include "panels/ViewPanel.h"
#include "panels/lab/LabPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
#include "../external/tools/OpenURL.h"
#include "UIStrings.h"
#include "ui/views/Announcements.h"
#include "ui/views/OClock.h"
#include "ui/panels/overlay/OverlayLayerRender.h"
#include "ui/views/Audio.h"
#include "ui/panels/StreamingPanel.h"
#include "qrcodegen.hpp"
#include "core/settings/SettingsManager.h"
#include "core/settings/ProjectionQualityPresets.h"
#include "AppIcons.h"
#include "DesignSystem.h"
#include "IconRail.h"
#include "MonitorTheme.h"
#include "MonitorUIHelpers.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include "ui/panels/home/HomeIcons.h"
#include "biblio/LibraryIcons.h"
#include "LiveContentRenderer.h"
#include "core/VoiceAssistant.h"
#include "LibrarySongs.h"
#include "PatchNotesRenderer.h"
#include "MediaImportModal.h"
#include "core/MediaImportManager.h"
#include "core/AuthLicenseManager.h"
#include <ctime>

namespace ProyecThor::UI {

namespace MT = MonitorTheme;

static bool g_ShowAbout = false;

static bool StatusDotToggle(ImDrawList* dl, const char* id, const char* label, bool on, ImVec4 onColor, float rowH)
{
    const float dotR = 5.0f;
    ImVec2 textSz = ImGui::CalcTextSize(label);
    float itemW = dotR * 2.0f + 6.0f + textSz.x + 14.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(itemW, rowH));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 center = { p0.x + dotR + 4.0f, p0.y + rowH * 0.5f };
    ImVec4 offColor = { 0.42f, 0.44f, 0.50f, 1.0f };
    Components::DrawStatusDot(dl, center, dotR, on ? onColor : offColor, on);

    ImVec4 textCol = on ? onColor : ImVec4(0.75f, 0.76f, 0.80f, hovered ? 1.0f : 0.85f);
    dl->AddText({ center.x + dotR + 6.0f, p0.y + (rowH - textSz.y) * 0.5f },
               ImGui::ColorConvertFloat4ToU32(textCol), label);

    return clicked;
}

UIManager::UIManager() : m_Window(nullptr), m_ShowConfig(false) {}
UIManager::~UIManager() { Shutdown(); }

bool UIManager::Initialize(GLFWwindow* window)
{
    m_Window = window;
    if (!m_Window) return false;

    m_Mode = WorkspaceMode::Projector;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    m_TransitionPanelOwned = std::make_shared<TransitionPanel>();
    m_TransitionPanel      = m_TransitionPanelOwned.get();

    m_SettingsPanel.SetOSCPanelRef(&m_OSC);
    m_SettingsPanel.SetBroadcastPanelRef(&m_Broadcast);
    m_SettingsPanel.SetStreamingPanelRef(&m_Red);
    m_SettingsPanel.SetSyncPanelRef(&m_Sync);
    m_SettingsPanel.SetUIManagerRef(this);

    ApplyProfessionalTheme();
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    m_GlassRenderer.Initialize(fbWidth, fbHeight);

    auto& core = Core::PresentationCore::Get();
    core.SetMainWindow(window);
    core.SetProjectorRenderFn([](int w, int h) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        UI::DrawPublicContent(dl, ImVec2(0, 0), ImVec2((float)w, (float)h), (float)w, (float)h);
    });
    core.SetStageRenderFn([](int w, int h) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        UI::DrawStageContent(dl, ImVec2(0, 0), ImVec2((float)w, (float)h));
    });

    return true;
}

// SOLO proporciones (padding/spacing/rounding/border) -- ya NO toca ningun
// ImGuiCol_* de color. Antes tenia una paleta gris fija completa (WindowBg,
// PopupBg, Button, Header, Tab, etc, unos 70 colores) que pisaba SIEMPRE lo
// que SettingsManager::ApplyTheme() (el tema realmente elegido en Ajustes >
// Apariencia) ya habia dejado bien puesto un instante antes en main.cpp,
// porque UIManager::Initialize() -> esta funcion corre DESPUES de ese
// ApplyTheme() inicial. Resultado: el tema elegido nunca se veia reflejado
// en el arranque, solo despues de tocar algo en Ajustes > Apariencia (que
// vuelve a llamar ApplyTheme() y "gana" recien ahi). Los colores ahora los
// pone una unica vez ApplyTheme(), nadie mas los toca.
void UIManager::ApplyProfessionalTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowPadding          = ImVec2( 8.0f,  8.0f);
    s.FramePadding           = ImVec2( 7.0f,  4.0f);
    s.ItemSpacing            = ImVec2( 6.0f,  5.0f);
    s.ItemInnerSpacing       = ImVec2( 5.0f,  4.0f);
    s.CellPadding            = ImVec2( 5.0f,  4.0f);
    s.TouchExtraPadding      = ImVec2( 0.0f,  0.0f);
    s.IndentSpacing          = 14.0f;
    s.ScrollbarSize          =  6.0f;
    s.GrabMinSize            =  8.0f;

    s.WindowRounding         =  2.0f;
    s.ChildRounding          =  1.0f;
    s.FrameRounding          =  2.0f;
    s.PopupRounding          =  2.0f;
    s.ScrollbarRounding      =  1.0f;
    s.GrabRounding           =  1.0f;
    s.TabRounding            =  2.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize       = 1.0f;
    s.ChildBorderSize        = 1.0f;
    s.PopupBorderSize        = 1.0f;
    s.FrameBorderSize        = 1.0f;
    s.TabBorderSize          = 0.0f;
    s.TabBarBorderSize       = 0.0f;

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        s.WindowRounding                = 0.0f;
        s.Colors[ImGuiCol_WindowBg].w    = 1.0f; // fuerza opaco (multi-viewport), no toca RGB
    }
}

void UIManager::AddPanel(std::shared_ptr<IPanel> panel)
{
    if (!panel) return;
    if (panel->GetName() == "Home")
        m_HomePanelRef = dynamic_cast<HomePanel*>(panel.get());
    m_Panels.push_back(std::move(panel));
}

bool UIManager::IsInBibleView() const
{
    if (Core::PresentationCore::Get().PeekSelection().type == Core::ItemType::Bible)
        return true;
    if (m_LibraryPanelRef &&
        m_LibraryPanelRef->GetSideMode() == LibrarySideMode::Categories &&
        m_LibraryPanelRef->GetCategory() == LibraryCategory::Bibles)
    {
        return true;
    }
    return false;
}

bool UIManager::IsBibleSearchActive() const
{
    if (!IsInBibleView()) return false;
    if (ImGui::GetIO().WantTextInput) return true;
    if (m_HomePanelRef && m_HomePanelRef->IsBibleSearchActive()) return true;
    return false;
}

void UIManager::OpenHub()
{
    OpenPatchNotes();
}

void UIManager::OpenPatchNotes()
{
    m_ShowPatchNotesModal = true;
}

// Ver comentario en UIManager.h. Cambia el preset SOLO en memoria (nunca
// llama Save()) para no pisar la preferencia real del usuario -- el cambio
// lo detecta solo BeginDockspace() (compara contra m_LastWorkspacePreset)
// y dispara el reset de layout.
void UIManager::EnterLibraryWorkspaceMode()
{
    ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace.layoutPreset =
        ProyecThor::Settings::WorkspaceLayoutPreset::Library;
    m_Mode = WorkspaceMode::Projector;
}

void UIManager::RequestSettings(int initialCategory)
{
    if (initialCategory >= 0)
        m_SettingsPanel.SetInitialCategory(initialCategory);
    m_ShowConfig = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderLiveOutputWindows — ventanas nativas "ProjectorLive"/"StageLive",
//  la salida real al publico. Se llama UNA VEZ POR FRAME desde el principio
//  de RenderAll(), ANTES de cualquier return anticipado (editor a pantalla
//  completa, modo Hub) -- estas ventanas nativas (ImGuiWindowClass con
//  ViewportFlagsOverrideSet = NoAutoMerge|TopMost, o sea su propia ventana
//  de SO) se destruyen solas si ImGui no vuelve a someter su Begin() por un
//  par de frames, asi que antes, cuando este bloque vivia mas abajo (adentro
//  del branch exclusivo de WorkspaceMode::Projector), abrir el editor de
//  Overlays/Estilos o volver al Hub mientras se proyectaba cortaba la
//  transmision real al publico sin que el operador lo pidiera.
// ─────────────────────────────────────────────────────────────────────────────
void UIManager::RenderLiveOutputWindows()
{
    float transNow = (float)glfwGetTime();
    float transDt  = transNow - m_TransitionLastTime;
    m_TransitionLastTime = transNow;
    transDt = std::min(transDt, 0.1f);

    if (m_TransitionPanel)
        m_TransitionPanel->Update(transDt);

    auto state = Core::PresentationCore::Get().GetState();

    // Viewports de post-FX extra que efectivamente se dibujaron este frame
    // -- se usa al final para podar (destruir) instancias de CompositePostChain
    // de monitores que el usuario destildo o que dejaron de estar activos.
    std::vector<ImGuiID> activeExtraProjectorIds;

    if (state.isProjecting)
    {
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        if (monitors && monitorCount > 0 &&
            state.targetMonitorIndex >= 0 &&
            state.targetMonitorIndex < monitorCount)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);

            if (mode && mode->width > 0 && mode->height > 0)
            {
                int mx, my;
                glfwGetMonitorPos(monitors[state.targetMonitorIndex], &mx, &my);

if (m_TransitionPanel) {
    Core::PresentationCore::Get().SetTransitionConfig(
        static_cast<int>(m_TransitionPanel->GetCurrentType()),
        m_TransitionPanel->GetDuration());

    // Duracion del crossfade de fondo: solo la toca el preset activo si
    // "Afecta a Fondos" esta prendido -- si no, se mantiene el default de
    // siempre (0.2s), ver BackgroundLayer::m_BlendSeconds.
    constexpr float kDefaultBgBlendSeconds = 0.2f;
    float bgBlendDuration = kDefaultBgBlendSeconds;
    if (m_TransitionPanel->AffectsBackground()) {
        bgBlendDuration = (m_TransitionPanel->GetCurrentType() == TransitionType::None)
            ? 0.01f
            : m_TransitionPanel->GetDuration();
    }
    Core::PresentationCore::Get().SetBackgroundBlendDuration(bgBlendDuration);
}

if (m_TransitionPanel && state.textTransitionTrigger != m_LastTransitionTrigger)
{
    m_LastTransitionTrigger = state.textTransitionTrigger;

    std::string outgoing = m_LastProjectedText;
    m_LastProjectedText   = state.currentText;

    // Si el preset activo no afecta a Letras, el texto cambia al instante
    // (no se llama Trigger(), igual que si el tipo fuera "Sin transición").
    if (!outgoing.empty() && !state.currentText.empty() && m_TransitionPanel->AffectsLyrics())
    {
        m_OutgoingText = outgoing;
        m_TransitionPanel->Trigger();
    }
}

                if (!Core::PresentationCore::Get().IsProjectorWindowActive())
                {
                    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                    {
                        RenderProjectorOutput("ProjectorLive", mx, my, mode, state,
                                               /*isPrimary=*/true, nullptr);
                    }
                }

                // Monitores de salida publica ADICIONALES (Multi-salida) --
                // Exclusivo de ProyecThor for Professionals.
                if (ProyecThor::Core::AuthLicenseManager::Get().IsPro())
                {
                    for (int extraIdx : state.extraTargetMonitors)
                    {
                        if (extraIdx == state.targetMonitorIndex) continue;
                        if (extraIdx < 0 || extraIdx >= monitorCount) continue;

                        const GLFWvidmode* exMode = glfwGetVideoMode(monitors[extraIdx]);
                        if (!exMode || exMode->width <= 0 || exMode->height <= 0) continue;

                        int exX, exY;
                        glfwGetMonitorPos(monitors[extraIdx], &exX, &exY);

                        std::string exName = "ProjectorLive_" + std::to_string(extraIdx);
                        RenderProjectorOutput(exName.c_str(), exX, exY, exMode, state,
                                               /*isPrimary=*/false, &activeExtraProjectorIds);
                    }
                }
            }
        }
    }

    // Libera instancias de post-FX de monitores extra que ya no esten
    // activas este frame (destildadas, o proyeccion detenida del todo).
    Core::PresentationCore::Get().PruneExtraProjectorViewports(activeExtraProjectorIds);

    if (state.isStaging)
    {
        // Monitor de Escenario (Stage Display) -- Exclusivo de ProyecThor for Professionals
        if (!ProyecThor::Core::AuthLicenseManager::Get().IsPro())
        {
            Core::PresentationCore::Get().SetStaging(false);
        }
        else
        {
            int stageMonitorIdx = state.stageMonitorIndex;
            int stageMonitorCount = 0;
            GLFWmonitor** stageMonitors = glfwGetMonitors(&stageMonitorCount);

            if (stageMonitors && stageMonitorIdx >= 0 && stageMonitorIdx < stageMonitorCount)
            {
                const GLFWvidmode* stageMode = glfwGetVideoMode(stageMonitors[stageMonitorIdx]);

                if (stageMode && stageMode->width > 0 && stageMode->height > 0)
                {
                    int smx = 0, smy = 0;
                    glfwGetMonitorPos(stageMonitors[stageMonitorIdx], &smx, &smy);

                    if (!Core::PresentationCore::Get().IsStageWindowActive())
                    {
                        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                        {
                            RenderStageOutput("StageLive", smx, smy, stageMode);
                        }
                        else
                        {
                            Core::PresentationCore::Get().SetStaging(false);
                        }
                    }

                    // Monitores de Stage ADICIONALES (opcional) -- solo Pro
                    for (int extraIdx : state.extraStageMonitors)
                    {
                        if (extraIdx == stageMonitorIdx) continue;
                        if (extraIdx < 0 || extraIdx >= stageMonitorCount) continue;

                        const GLFWvidmode* exMode = glfwGetVideoMode(stageMonitors[extraIdx]);
                        if (!exMode || exMode->width <= 0 || exMode->height <= 0) continue;

                        int exX, exY;
                        glfwGetMonitorPos(stageMonitors[extraIdx], &exX, &exY);

                        std::string exName = "StageLive_" + std::to_string(extraIdx);
                        RenderStageOutput(exName.c_str(), exX, exY, exMode);
                    }
                }
            }
        }
    }
}

void UIManager::RenderProjectorOutput(const char* windowName, int mx, int my,
                                       const GLFWvidmode* mode,
                                       const Core::PresentationState& state,
                                       bool isPrimary,
                                       std::vector<ImGuiID>* activeExtraViewportIds)
{
                ImGui::SetNextWindowPos(ImVec2((float)mx, (float)my));
                ImGui::SetNextWindowSize(ImVec2((float)mode->width, (float)mode->height));

                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoDecoration          |
                    ImGuiWindowFlags_NoBackground          |
                    ImGuiWindowFlags_NoSavedSettings       |
                    ImGuiWindowFlags_NoFocusOnAppearing    |
                    ImGuiWindowFlags_NoNav                 |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGuiWindowClass projectorClass;
projectorClass.ViewportFlagsOverrideSet =
    ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
ImGui::SetNextWindowClass(&projectorClass);

ImGui::Begin(windowName, nullptr, flags);

                ImGuiID vpID = ImGui::GetWindowViewport()->ID;
                if (isPrimary) {
                    Core::PresentationCore::Get().SetProjectorPostFXViewportID(vpID);
                } else {
                    Core::PresentationCore::Get().RegisterExtraProjectorViewport(vpID);
                    if (activeExtraViewportIds) activeExtraViewportIds->push_back(vpID);
                }
                ImDrawList* drawList = ImGui::GetWindowDrawList();

bool showingLoadingScreen = Core::PresentationCore::Get().ShouldShowLoadingScreen();
if (showingLoadingScreen)
{
    drawList->AddRectFilled(
        ImVec2((float)mx, (float)my),
        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
        IM_COL32(0, 0, 0, 255));

    void* logoTex = Core::PresentationCore::Get().GetLoadingLogoTexture();
    int   logoW   = Core::PresentationCore::Get().GetLoadingLogoWidth();
    int   logoH   = Core::PresentationCore::Get().GetLoadingLogoHeight();
    if (logoTex && logoW > 0 && logoH > 0)
    {
        float destX = (float)mx, destY = (float)my;
        float destW = (float)mode->width, destH = (float)mode->height;
        float logoRatio   = (float)logoW / (float)logoH;
        float screenRatio = destW / destH;

        if (logoRatio > screenRatio + 0.001f) {
            destH = destW / logoRatio;
            destY = (float)my + ((float)mode->height - destH) * 0.5f;
        } else if (logoRatio < screenRatio - 0.001f) {
            destW = destH * logoRatio;
            destX = (float)mx + ((float)mode->width - destW) * 0.5f;
        }

        drawList->AddImage(logoTex, ImVec2(destX, destY), ImVec2(destX + destW, destY + destH),
                           ImVec2(0, 0), ImVec2(1, 1));
    }
}
else
{

if (state.bgType == Core::PresentationState::BackgroundType::SolidColor)
{
    ImU32 finalCol = IM_COL32(
        (int)(state.bgColor[0]*255), (int)(state.bgColor[1]*255),
        (int)(state.bgColor[2]*255), 255);

    drawList->AddRectFilled(
        ImVec2((float)mx, (float)my),
        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
        finalCol);
}

                if (state.bgType == Core::PresentationState::BackgroundType::Video)
                {
                    drawList->AddRectFilled(
                        ImVec2((float)mx, (float)my),
                        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                        IM_COL32(0, 0, 0, 255));

                    int srcW = 0, srcH = 0;
                    Core::PresentationCore::Get().GetBackgroundVideoSize(srcW, srcH);

                    bool stretch = Core::PresentationCore::Get().GetStretchToFill();

                    float destX = (float)mx;
                    float destY = (float)my;
                    float destW = (float)mode->width;
                    float destH = (float)mode->height;

                    if (!stretch && srcW > 0 && srcH > 0)
                    {
                        float videoRatio  = (float)srcW / (float)srcH;
                        float screenRatio = destW / destH;

                        if (videoRatio > screenRatio + 0.001f) {
                            destH = destW / videoRatio;
                            destY = (float)my + ((float)mode->width / screenRatio - destH) * 0.5f;
                        } else if (videoRatio < screenRatio - 0.001f) {
                            destW = destH * videoRatio;
                            destX = (float)mx + ((float)mode->height * screenRatio - destW) * 0.5f;
                        }
                    }

                    const auto& projSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection;
                    auto qualityMode = static_cast<ProyecThor::Settings::OutputQualityMode>(projSettings.outputQualityMode);
                    int qualityW = 0, qualityH = 0;
                    ProyecThor::Settings::ResolveQualityTarget(
                        qualityMode,
                        projSettings.outputPresetIndex, projSettings.outputWidth, projSettings.outputHeight,
                        mode->width, mode->height, qualityW, qualityH);

                    if (qualityMode == ProyecThor::Settings::OutputQualityMode::Auto)
                        Core::PerformanceGovernor::Get().ApplyCap(qualityW, qualityH);

                    void* texID = Core::PresentationCore::Get().GetProcessedBackgroundTexture(
                        qualityW, qualityH);

                    if (texID) {

                        bool hasBars = (destW < (float)mode->width - 0.5f) ||
                                       (destH < (float)mode->height - 0.5f);
                        if (hasBars && Core::PresentationCore::Get().GetFillBlurEnabled()) {
                            void* fillTex = Core::PresentationCore::Get().GetBackgroundFillTexture(
                                qualityW, qualityH);
                            if (fillTex) {

                                float b = std::clamp(
                                    Core::PresentationCore::Get().GetFillBlurBrightness(), 0.0f, 1.0f);
                                ImU32 fillTint = IM_COL32((int)(b * 255.0f), (int)(b * 255.0f), (int)(b * 255.0f), 255);
                                drawList->AddImage(fillTex,
                                    ImVec2((float)mx, (float)my),
                                    ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                                    ImVec2(0, 0), ImVec2(1, 1), fillTint);
                            }
                        }

                        auto& core = Core::PresentationCore::Get();
                        void* standbyTex = nullptr;
                        bool isTransActive = false;
                        float progress = 0.0f;
                        int transType = core.GetBackgroundTransitionType();

                        if (core.IsBackgroundSwapPending() && core.IsBackgroundStandbyReady())
                        {
                            standbyTex = core.GetStandbyBackgroundTexture();
                            progress = std::clamp(core.GetBackgroundBlendProgress(), 0.0f, 1.0f);
                            isTransActive = (standbyTex != nullptr);
                        }

                        RenderBackgroundWithTransition(drawList, texID, standbyTex,
                            ImVec2(destX, destY),
                            ImVec2(destX + destW, destY + destH),
                            transType, progress, isTransActive);
                    } else {
                        drawList->AddRectFilled(
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            IM_COL32(0, 0, 0, 255));
                    }
                }

                if (state.bgType == Core::PresentationState::BackgroundType::Audio)
                {
                    drawList->AddRectFilled(
                        ImVec2((float)mx, (float)my),
                        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                        IM_COL32(0, 0, 0, 255));

                    if (auto* audioPanel = Core::PresentationCore::Get().GetAudioPanelRef())
                        audioPanel->RenderLiveBackground((float)mx, (float)my,
                                                          (float)mode->width, (float)mode->height);
                }

                if (state.showText)
                {
                    auto DrawTextBlockInRect = [&](const std::string& text, const Core::TextBoxStyle& box,
                                                   bool isLyricsBox,
                                                   float offsetX, float offsetY,
                                                   float alphaMult, float scaleMult,
                                                   float targetX, float targetY, float targetW, float targetH)
                    {
                        if (text.empty() || alphaMult <= 0.001f) return;

                        auto& core = Core::PresentationCore::Get();
                        bool isSong = (core.PeekSelection().type == Core::ItemType::Song);

                        float screenScale = (float)mode->width / 1920.0f;
                        float boxW = std::max(10.0f, box.sizeW * targetW);
                        float boxH = std::max(10.0f, box.sizeH * targetH);

                        float shiftX = offsetX * targetW;
                        float shiftY = offsetY * targetH;
                        float boxX   = targetX + box.posX * targetW  - boxW * 0.5f + shiftX;
                        float boxY   = targetY + box.posY * targetH - boxH * 0.5f + shiftY;

                        if (box.bgMediaEnabled && !box.bgMediaPath.empty()) {
                            unsigned int bgTex = core.GetBoxBgTexture(isLyricsBox, box.bgMediaPath);
                            if (bgTex != 0) {
                                ImU32 tint = IM_COL32(255, 255, 255,
                                    (int)(std::clamp(box.bgMediaOpacity, 0.0f, 1.0f) * alphaMult * 255.0f));
                                drawList->AddImage((ImTextureID)(intptr_t)bgTex,
                                    ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH),
                                    ImVec2(0, 0), ImVec2(1, 1), tint);
                            }
                        }

                        float targetFontSize = box.textSize * screenScale;

                        ImFont* activeFont = core.GetImGuiFont(box.fontName, targetFontSize);
                        if (!activeFont) activeFont = ImGui::GetFont();

                        if (box.autoScale) {
                            while (targetFontSize > 10.0f) {
                                ImVec2 tSize = activeFont->CalcTextSizeA(
                                    targetFontSize, FLT_MAX, boxW, text.c_str());
                                if (tSize.y <= boxH) break;
                                targetFontSize -= 1.0f;
                            }
                        }

                        targetFontSize *= std::max(0.01f, scaleMult);

                        ImVec2 finalBlockSize = activeFont->CalcTextSizeA(
                            targetFontSize, FLT_MAX, boxW, text.c_str());

                        ImU32 col = ImGui::ColorConvertFloat4ToU32(
                            ImVec4(box.color[0], box.color[1],
                                   box.color[2], box.color[3] * alphaMult));

                        drawList->PushClipRect(
                            ImVec2(targetX, targetY),
                            ImVec2(targetX + targetW, targetY + targetH),
                            true);

                        if (isSong && box.hAlign == 1)
                        {
                            float startY = boxY;
                            if (box.vAlign == 1)
                                startY += (boxH - finalBlockSize.y) * 0.5f;
                            else if (box.vAlign == 2)
                                startY += (boxH - finalBlockSize.y);

                            float  currentY   = startY;
                            size_t startPos   = 0;
                            size_t endPos     = text.find('\n');
                            float  lineHeight = activeFont->CalcTextSizeA(
                                targetFontSize, FLT_MAX, boxW, "A").y;

                            while (startPos != std::string::npos)
                            {
                                std::string line = text.substr(startPos, endPos - startPos);
                                if (!line.empty() && line.back() == '\r') line.pop_back();

                                if (!line.empty()) {
                                    ImVec2 lineSize = activeFont->CalcTextSizeA(
                                        targetFontSize, FLT_MAX, boxW, line.c_str());
                                    float lineX = boxX + (boxW - lineSize.x) * 0.5f;

                                    DrawStyledText(drawList, activeFont, targetFontSize,
                                        ImVec2(lineX, currentY), col, line.c_str(),
                                        0.0f, screenScale, box.effects, alphaMult);
                                }

                                currentY += lineHeight;
                                if (endPos == std::string::npos) break;
                                startPos = endPos + 1;
                                endPos   = text.find('\n', startPos);
                            }
                        }
                        else
                        {
                            float textX = boxX;
                            if (box.hAlign == 1)
                                textX += (boxW - finalBlockSize.x) * 0.5f;
                            else if (box.hAlign == 2)
                                textX += (boxW - finalBlockSize.x);

                            float textY = boxY;
                            if (box.vAlign == 1)
                                textY += (boxH - finalBlockSize.y) * 0.5f;
                            else if (box.vAlign == 2)
                                textY += (boxH - finalBlockSize.y);

                            DrawStyledText(drawList, activeFont, targetFontSize,
                                ImVec2(textX, textY), col, text.c_str(),
                                boxW, screenScale, box.effects, alphaMult);
                        }

                        drawList->PopClipRect();
                    };

                    auto DrawTextBlock = [&](const std::string& text, const Core::TextBoxStyle& box,
                                             bool isLyricsBox, float offsetX, float offsetY,
                                             float alphaMult = 1.0f, float scaleMult = 1.0f)
                    {
                        DrawTextBlockInRect(text, box, isLyricsBox, offsetX, offsetY, alphaMult, scaleMult,
                                            (float)mx, (float)my, (float)mode->width, (float)mode->height);
                    };

                    bool transActive = m_TransitionPanel && m_TransitionPanel->IsActive();

                    if (state.tiles.enabled)
                    {
                        int slotCount = (state.tiles.layout == Core::TileLayout::QuadGrid) ? 4 : 2;
                        for (int i = 0; i < slotCount; i++)
                        {
                            const auto& slot = state.tiles.slots[i];
                            if (!slot.visible || (slot.text.empty() && slot.ref.empty())) continue;

                            float subX = (float)mx, subY = (float)my;
                            float subW = (float)mode->width, subH = (float)mode->height;

                            if (state.tiles.layout == Core::TileLayout::SplitVertical)
                            {
                                subW = (float)mode->width * 0.5f;
                                subX = (float)mx + i * subW;
                            }
                            else if (state.tiles.layout == Core::TileLayout::SplitHorizontal)
                            {
                                subH = (float)mode->height * 0.5f;
                                subY = (float)my + i * subH;
                            }
                            else // QuadGrid
                            {
                                subW = (float)mode->width * 0.5f;
                                subH = (float)mode->height * 0.5f;
                                int col = i % 2;
                                int row = i / 2;
                                subX = (float)mx + col * subW;
                                subY = (float)my + row * subH;
                            }

                            const auto& lyricsBox = slot.hasCustomStyle ? slot.lyricsBox : state.lyricsBox;
                            const auto& indexBox  = slot.hasCustomStyle ? slot.indexBox  : state.indexBox;
                            bool indEnabled       = slot.hasCustomStyle ? slot.indexEnabled : state.indexEnabled;

                            if (!slot.text.empty())
                                DrawTextBlockInRect(slot.text, lyricsBox, true, 0.0f, 0.0f, 1.0f, 1.0f,
                                                    subX, subY, subW, subH);

                            if (indEnabled && !slot.ref.empty())
                                DrawTextBlockInRect(slot.ref, indexBox, false, 0.0f, 0.0f, 1.0f, 1.0f,
                                                    subX, subY, subW, subH);
                        }

                        // Divisor sutil en proyector
                        if (state.tiles.showDividers)
                        {
                            if (state.tiles.layout == Core::TileLayout::SplitVertical)
                            {
                                float midX = (float)mx + (float)mode->width * 0.5f;
                                drawList->AddLine(
                                    ImVec2(midX, (float)my + (float)mode->height * 0.08f),
                                    ImVec2(midX, (float)(my + mode->height) - (float)mode->height * 0.08f),
                                    IM_COL32(255, 255, 255, 30), 1.0f);
                            }
                            else if (state.tiles.layout == Core::TileLayout::SplitHorizontal)
                            {
                                float midY = (float)my + (float)mode->height * 0.5f;
                                drawList->AddLine(
                                    ImVec2((float)mx + (float)mode->width * 0.08f, midY),
                                    ImVec2((float)(mx + mode->width) - (float)mode->width * 0.08f, midY),
                                    IM_COL32(255, 255, 255, 30), 1.0f);
                            }
                            else
                            {
                                float midX = (float)mx + (float)mode->width * 0.5f;
                                float midY = (float)my + (float)mode->height * 0.5f;
                                drawList->AddLine(
                                    ImVec2(midX, (float)my + (float)mode->height * 0.05f),
                                    ImVec2(midX, (float)(my + mode->height) - (float)mode->height * 0.05f),
                                    IM_COL32(255, 255, 255, 25), 1.0f);
                                drawList->AddLine(
                                    ImVec2((float)mx + (float)mode->width * 0.05f, midY),
                                    ImVec2((float)(mx + mode->width) - (float)mode->width * 0.05f, midY),
                                    IM_COL32(255, 255, 255, 25), 1.0f);
                            }
                        }
                    }
                    else
                    {
                        if (transActive) {
                            DrawTextBlock(m_OutgoingText, state.lyricsBox, true,
                                m_TransitionPanel->GetOutgoingOffsetX(),
                                m_TransitionPanel->GetOutgoingOffsetY(),
                                m_TransitionPanel->GetOutgoingAlpha(),
                                m_TransitionPanel->GetOutgoingScale());

                            DrawTextBlock(state.currentText, state.lyricsBox, true,
                                m_TransitionPanel->GetIncomingOffsetX(),
                                m_TransitionPanel->GetIncomingOffsetY(),
                                m_TransitionPanel->GetIncomingAlpha(),
                                m_TransitionPanel->GetIncomingScale());
                        } else if (!state.currentText.empty()) {
                            DrawTextBlock(state.currentText, state.lyricsBox, true, 0.0f, 0.0f, 1.0f, 1.0f);
                        }

                        if (state.indexEnabled && !state.currentRef.empty()) {
                            DrawTextBlock(state.currentRef, state.indexBox, false, 0.0f, 0.0f, 1.0f, 1.0f);
                        }
                    }
                }

                // ── Overlay (PNG transparente) ───────────────────────────────
                // Capa APARTE de fondo/texto (ver PresentationCore::
                // SetOverlayMedia) -- se dibuja encima de los dos, dejando ver
                // lo que haya debajo gracias al alpha real del PNG (por eso
                // AddImage sin tint opaco: el blending normal de ImGui ya
                // respeta el canal alpha de la textura).
                if (void* overlayTex = Core::PresentationCore::Get().GetOverlayTexture())
                {
                    drawList->AddImage(overlayTex,
                        ImVec2((float)mx, (float)my),
                        ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                        ImVec2(0, 0), ImVec2(1, 1));
                }

                // ── Capa 3D (Modelos y Recursos 3D en vivo) ─────────────────
                if (Core::PresentationCore::Get().IsLive3DModelActive())
                {
                    if (void* model3dTex = Core::PresentationCore::Get().GetLive3DModelTexture())
                    {
                        drawList->AddImage(model3dTex,
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            ImVec2(0, 0), ImVec2(1, 1));
                    }
                }

                // ── Capa de Laboratorio Matemático (Gráficas GeoGebra en vivo) ─────────
                if (Core::PresentationCore::Get().IsLiveLabActive())
                {
                    if (LabPanel* lab = Core::PresentationCore::Get().GetLabPanelRef())
                    {
                        lab->RenderLiveProjection(drawList,
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            (float)mode->width, (float)mode->height);
                    }
                }

                // ── Reloj/contador en vivo sobre el overlay ──────────────────
                // Ver mismo bloque en LiveContentRenderer.cpp (preview) -- debe
                // dibujarse identico aca para que la salida real al proyector
                // coincida con lo que ve el operador en "Vista en Vivo".
                {
                    auto& core = Core::PresentationCore::Get();
                    if (core.HasOverlayClockLayer())
                    {
                        std::string clockTxt = core.GetLiveOverlayClockText();
                        if (!clockTxt.empty())
                        {
                            ProyecThor::UI::OverlayLayer cl = core.GetOverlayClockLayer();
                            ImFont* clockFont = core.GetImGuiFont(cl.fontName, cl.fontSize);
                            if (!clockFont) clockFont = ImGui::GetFont();

                            float drawW = (float)mode->width, drawH = (float)mode->height;
                            float clockScale = drawW / (float)std::max(1, core.GetOverlayClockCanvasW());
                            float clockDispSize = std::max(4.0f, cl.fontSize * clockScale);
                            ImVec2 clockBlockSz = clockFont->CalcTextSizeA(clockDispSize, FLT_MAX, FLT_MAX, clockTxt.c_str());
                            ImVec2 clockCenter = ImVec2((float)mx + cl.posX * drawW, (float)my + cl.posY * drawH);
                            ImVec2 clockTL = ImVec2(clockCenter.x - clockBlockSz.x * 0.5f, clockCenter.y - clockBlockSz.y * 0.5f);

                            float clockColorOverride[4];
                            bool hasOverride = core.HasLiveOverlayClockColorOverride();
                            if (hasOverride) core.GetLiveOverlayClockColorOverride(clockColorOverride);

                            ProyecThor::UI::DrawOverlayLayerStyledText(drawList, clockFont, clockDispSize, clockTL,
                                clockBlockSz, cl, clockTxt.c_str(), clockScale, hasOverride ? clockColorOverride : nullptr);
                        }
                    }
                }
}

                if (!showingLoadingScreen)
                {
                    static auto s_AnnLastTime = std::chrono::steady_clock::now();
                    auto        annNow        = std::chrono::steady_clock::now();
                    float       annDt = std::chrono::duration<float>(annNow - s_AnnLastTime).count();
                    s_AnnLastTime = annNow;
                    annDt = std::min(annDt, 0.1f);

                    for (auto& p : m_Panels) {
                        if (p->GetName() == "Diseño") {

                            auto* stylesHub =
                                static_cast<ProyecThor::UI::StylesHubPanel*>(p.get());

                            if (stylesHub->GetAnnouncements().IsLive()) {
                                stylesHub->GetAnnouncements().RenderOnProjector(
                                    drawList,
                                    (float)mx, (float)my,
                                    (float)mode->width, (float)mode->height,
                                    annDt);
                            }

                            stylesHub->GetCapturePanel().RenderOnProjector(
                                drawList,
                                (float)mx, (float)my,
                                (float)mode->width, (float)mode->height);
                            break;
                        }
                    }
                }

                ImGui::End();
}

void UIManager::RenderStageOutput(const char* windowName, int smx, int smy,
                                   const GLFWvidmode* stageMode)
{
    ImGui::SetNextWindowPos(ImVec2((float)smx, (float)smy));
    ImGui::SetNextWindowSize(ImVec2((float)stageMode->width, (float)stageMode->height));

    ImGuiWindowFlags stageFlags =
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoBackground          |
        ImGuiWindowFlags_NoSavedSettings       |
        ImGuiWindowFlags_NoFocusOnAppearing    |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiWindowClass stageClass;
    stageClass.ViewportFlagsOverrideSet =
        ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
    ImGui::SetNextWindowClass(&stageClass);

    ImGui::Begin(windowName, nullptr, stageFlags);
    ImDrawList* stageDrawList = ImGui::GetWindowDrawList();

    DrawStageContent(
        stageDrawList,
        ImVec2((float)smx, (float)smy),
        ImVec2((float)(smx + stageMode->width), (float)(smy + stageMode->height)));

    ImGui::End();
}

void UIManager::RenderAll()
{
    // Re-sincroniza el tema TODOS los frames, no solo cuando se clickea un
    // preset en Ajustes > Apariencia -- pedido explicito: varios paneles
    // (Biblioteca, Monitor de Control) se quedaban con colores de un tema
    // anterior sin importar cual estuviera realmente elegido. ApplyTheme()
    // es barato (unas pocas asignaciones de ImVec4/ImU32, sin IO), asi que
    // hacerlo incondicional cada frame es mas robusto que confiar en que
    // CADA lugar que cambia el tema se acuerde de llamarlo -- si algo queda
    // "atrasado" un frame, se autocorrige en el siguiente en vez de
    // quedarse mal para siempre.
    ProyecThor::Settings::SettingsManager::Get().ApplyTheme();

     {
        ImGuiIO& io = ImGui::GetIO();
        const bool inBibleView       = IsInBibleView();
        const bool bibleSearchActive = IsBibleSearchActive();

        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            ProyecThor::External::OpenURL("https://proyecthor.web.app/");

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
            m_ShowConfig = true;

        if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F4, false))
            glfwSetWindowShouldClose(m_Window, true);

        if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
            ToggleFullscreen();

        // Shift+Z: abrir/cerrar Notas rapidas (ver Ajustes > Accesos
        // Rapidos). Se ignora mientras el usuario esta escribiendo en
        // cualquier campo de texto (io.WantTextInput) o dentro de la Biblia /
        // buscador de Biblia (donde solo operan combinaciones Ctrl/Alt/Fn).
        if (!io.WantTextInput && !inBibleView && !bibleSearchActive &&
            io.KeyShift && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            ToggleNotesWindow();

        // Ctrl+Shift+D: desacoplar/acoplar Vista en Vivo como ventana aparte
        if (!io.WantTextInput && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (m_ViewPanelRef) {
                m_ViewPanelRef->SetDetached(!m_ViewPanelRef->IsDetached());
            }
        }

        // Alt Gr + 1/2/3/4: colapsar/expandir Biblioteca/Home/Vista en
        // Vivo/Diseño. Alt Gr + 0: restablecer el entorno completo. Se
        // detecta con ImGuiKey_RightAlt (no io.KeyAlt/ImGuiMod_Alt): en
        // Windows, Alt Gr fisico se reporta como Alt derecho -- GLFW ya
        // descarta el Ctrl "fantasma" que el sistema sintetiza junto con
        // ella. Igual que Shift+Z, se ignora con un campo de texto activo:
        // en teclados Latam/ES, Alt Gr + 2/3/etc son "@"/"#" reales.
        if (!io.WantTextInput && ImGui::IsKeyDown(ImGuiKey_RightAlt))
        {
            for (int i = 0; i < kCollapsiblePanelCount; ++i)
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i), false))
                    TogglePanelCollapse(i);

            if (ImGui::IsKeyPressed(ImGuiKey_0, false))
                ResetPanelCollapse();
        }

        if (!io.WantTextInput && !bibleSearchActive && !ImGui::IsAnyItemActive() && !ImGui::IsKeyDown(ImGuiKey_RightAlt))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) || ImGui::IsKeyPressed(ImGuiKey_PageUp, false))
                Core::PresentationCore::Get().RequestNavigate(-1);
            else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) || ImGui::IsKeyPressed(ImGuiKey_PageDown, false))
                Core::PresentationCore::Get().RequestNavigate(1);
        }
    }

    m_Red.Update();
    m_Chat.Update();
    m_Broadcast.Update();
    m_Sync.Update();
    m_OSC.Update();

    // La toolbar de modos (Notas/Estilos/Streaming/PatchNotes)
    if (!(m_FullscreenEditorActive && m_FullscreenEditorHidesToolbar))
        RenderModeToolbar();

    // Salida real ("ProjectorLive"/"StageLive") -- SIEMPRE se renderiza aca,
    // antes de cualquier return anticipado de abajo, para que la transmision al publico nunca se
    // interrumpa solo porque el operador esta mirando otra cosa en su
    // propia pantalla. Ver comentario en UIManager.h.
    RenderLiveOutputWindows();

    // Se renderiza siempre, sin importar el modo/return anticipado de mas
    // abajo, para que "Archivo > Importar > Importar desde URL" funcione.
    RenderUrlImportModal();
    Core::MediaImportManager::Get().Update();
    RenderMediaImportModal();

    if (m_ShowNotes)
        RenderNotesWindow();

    // Se somete siempre: el WebView2 embebido necesita que se le avise UpdateBounds(..., visible=false)
    RenderAIAssistantWindow();
    Core::VoiceAssistant::Get().Update();
    RenderVoiceAssistantWindow();
    m_VoiceAssistantPanel.RenderAssistedPromptBanner();
    RenderProWindows();

    // Ventana flotante "Centro de Conexiones" (Red LAN, App Móvil, Transmisión, OSC, Chat)
    if (m_ShowConnectionsWindow)
        RenderConnectionsWindow();

    // Notas de Versión / PatchNotes modal (solo con Ctrl+Alt+N, nunca tecla simple N)
    if (m_ShowPatchNotesModal)
        RenderPatchNotesModal();
    if (m_ShowProFeatureModal)
        RenderProFeatureModal();
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && io.KeyCtrl && io.KeyAlt && !io.KeyShift &&
            !ImGui::IsKeyDown(ImGuiKey_RightAlt) && ImGui::IsKeyPressed(ImGuiKey_N, false))
            m_ShowPatchNotesModal = !m_ShowPatchNotesModal;
    }

    // Editor a pantalla completa (Overlay/Estilos) activo -- ver EnterFullscreenEditor.
    if (m_FullscreenEditorActive && m_FullscreenEditorRenderFn)
    {
        m_FullscreenEditorRenderFn();
        return;
    }

    BeginDockspace();

    const auto& str = ProyecThor::UI::GetUIStrings();

    // Presets reducidos (ver Settings::WorkspaceLayoutPreset): cada uno
    // somete solo un subconjunto de m_Panels este frame y, si corresponde,
    // bloquea a Biblioteca en una sola vista -- recalculado cada frame
    // (barato, mismo criterio que ApplyTheme() arriba) asi que nunca queda
    // desincronizado del preset real, sin importar por donde haya cambiado
    // (Ajustes, menu Espacio de trabajo, o "Abrir con ProyecThor").
    using ProyecThor::Settings::WorkspaceLayoutPreset;
    const WorkspaceLayoutPreset activePreset =
        ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace.layoutPreset;
    const bool isLibraryWorkspace   = (activePreset == WorkspaceLayoutPreset::Library);
    const bool isBroadcastWorkspace = (activePreset == WorkspaceLayoutPreset::Broadcast);
    const bool isVideoWorkspace     = (activePreset == WorkspaceLayoutPreset::Video);
    if (m_LibraryPanelRef) {
        m_LibraryPanelRef->SetMediaOnlyMode(isLibraryWorkspace);
    }

    for (auto& panel : m_Panels)
    {
        // "Library" es el GetName() interno de LibraryPanel (no el titulo
        // localizado de su ventana, ese es str.library).
        const std::string& panelName = panel->GetName();
        if (isLibraryWorkspace && panelName != "Library" && panelName != "Home")
            continue;
        // "Transmisión": Streaming (ver StreamingWorkspacePanel) ocupa el
        // lugar de Vista en Vivo -- esta NUNCA se dockea en ese preset (ver
        // BuildWorkspaceLayoutBroadcast), asi que no puede someterse o
        // queda flotando sin nodo. El panel de Streaming, al reves, solo
        // tiene sentido EN este preset.
        if (isBroadcastWorkspace && panelName == "Vista en Vivo")
            continue;
        if (!isBroadcastWorkspace && panelName == "Transmisión")
            continue;
        // "Transmisión" ahora es un preset exclusivo (ver
        // BuildWorkspaceLayoutBroadcast) -- "es solo para ver la
        // transmision, nada de proyeccion", asi que Biblioteca/Home/Diseño
        // tampoco se someten aca (mismo criterio que "Producción"/
        // "Biblioteca" arriba).
        if (isBroadcastWorkspace && (panelName == "Library" || panelName == "Home" || panelName == "Diseño"))
            continue;
        // "Producción" (VideoEditorPanel, GetName()=="VideoEditor"): a
        // pantalla completa, solo se somete en su propio preset -- si no
        // quedaria flotando sin nodo en el resto.
        if (panelName == "VideoEditor" && !isVideoWorkspace) continue;
        if (isVideoWorkspace && panelName != "VideoEditor") continue;
        // El colapso de contenido (Alt Gr + 1..4) NO se filtra aca: cada
        // panel lo consulta el mismo dentro de su Render(), despues de
        // correr su "pump incondicional" propio si tiene uno (ver
        // IsPanelCollapsedForRender en UIManager.h). Saltear Render() entero
        // desde aca rompia esos pumps (cola del Monitor en Home, Reloj en
        // Biblioteca) mientras el panel estaba oculto.
        panel->Render();
    }
if (m_FocusViewNextFrame) {
        ImGui::SetWindowFocus("Vista en Vivo");
        m_FocusViewNextFrame = false;
    }
    // (Salida real movida a RenderLiveOutputWindows(), llamada al principio
    // de RenderAll() -- ver comentario ahi y en UIManager.h.)

    if (m_ShowConfig)
        m_SettingsPanel.Render(&m_ShowConfig);

    {
        auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
        if (general.showPerfPanel)
        {
            bool wasOpen = general.showPerfPanel;
            m_PerformancePanel.Render(&general.showPerfPanel);
            if (wasOpen && !general.showPerfPanel)
                ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    if (g_ShowAbout)
        ImGui::OpenPopup(str.menuAbout);

    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    ImVec2 work_pos  = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 center    = ImVec2(work_pos.x + work_size.x * 0.5f,
                              work_pos.y + work_size.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.060f, 0.065f, 0.088f, 0.99f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.250f, 0.210f, 0.090f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);

    if (ImGui::BeginPopupModal(str.menuAbout, &g_ShowAbout,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
        ImGui::SetWindowFontScale(1.20f);
        ImGui::Text("%s", str.appTitle);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.420f, 0.420f, 0.420f, 1.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        ImGui::Text("v0.1.6");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.250f, 0.210f, 0.090f, 0.60f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.780f, 0.770f, 0.740f, 1.0f));
        ImGui::Text("%s", str.aboutDesc);
        ImGui::Spacing();
        ImGui::TextWrapped("%s", str.aboutNonProfit);
        ImGui::Spacing();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.500f, 0.500f, 0.490f, 1.0f));
        ImGui::Text("Creado por TheVixcho y la comunidad de ProyecThor");
        ImGui::Spacing();
        ImGui::TextUnformatted("Colaboradores: Oscar Farias, Victor Farias, Fabiola Fernandez");
        ImGui::Spacing();
        ImGui::TextDisabled("2026");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.650f, 0.650f, 0.650f, 1.0f));
        ImGui::TextUnformatted("Formato y datos bíblicos (XML):");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.350f, 0.650f, 0.950f, 1.0f));
        if (ImGui::Selectable("Beblia (Holy-Bible-XML-Format)##bible_credit", false, ImGuiSelectableFlags_DontClosePopups)) {
            ProyecThor::External::OpenURL("https://github.com/Beblia/Holy-Bible-XML-Format");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Abrir https://github.com/Beblia/Holy-Bible-XML-Format en el navegador");
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.180f, 0.185f, 0.230f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.240f, 0.200f, 0.085f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.415f, 0.180f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.650f, 0.530f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.920f, 0.820f, 0.560f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16.0f, 6.0f));

        if (ImGui::Button(str.close)) {
            ImGui::CloseCurrentPopup();
            g_ShowAbout = false;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

   EndDockspace();
}

static void RenderSocialQrMenu(const char* url)
{
    ImGui::Text("Escanea con tu celular:");
    ImGui::Spacing();

    static const char* s_CachedUrl = nullptr;
    static qrcodegen::QrCode s_CachedQr = qrcodegen::QrCode::encodeText(" ", qrcodegen::QrCode::Ecc::MEDIUM);

    if (s_CachedUrl != url)
    {
        s_CachedQr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::MEDIUM);
        s_CachedUrl = url;
    }

    int qrSize = s_CachedQr.getSize();
    float cellSize = 5.0f;
    float margin = cellSize * 2.0f;
    float totalSize = (qrSize * cellSize) + (margin * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, ImVec2(pos.x + totalSize, pos.y + totalSize), IM_COL32(255, 255, 255, 255));

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (s_CachedQr.getModule(x, y)) {
                ImVec2 minP(pos.x + margin + (x * cellSize), pos.y + margin + (y * cellSize));
                ImVec2 maxP(pos.x + margin + ((x + 1) * cellSize), pos.y + margin + ((y + 1) * cellSize));
                drawList->AddRectFilled(minP, maxP, IM_COL32(0, 0, 0, 255));
            }
        }
    }

    ImGui::Dummy(ImVec2(totalSize, totalSize));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Abrir en el navegador", ImVec2(totalSize, 0)))
        ProyecThor::External::OpenURL(url);
}

void UIManager::ToggleFullscreen()
{
    if (!m_Window) return;

    if (glfwGetWindowMonitor(m_Window) != nullptr) {
        glfwSetWindowMonitor(m_Window, nullptr, m_WindowedX, m_WindowedY, m_WindowedW, m_WindowedH, 0);
    } else {
        glfwGetWindowPos(m_Window, &m_WindowedX, &m_WindowedY);
        glfwGetWindowSize(m_Window, &m_WindowedW, &m_WindowedH);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor) return;
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) return;
        glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
}

void UIManager::RenderModeToolbar()
{
    RenderQuickSwitcher();

    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    const auto& str = ProyecThor::UI::GetUIStrings();

    ImGuiViewport* vp     = ImGui::GetMainViewport();
    bool           showLbl = general.showRailLabels;
    float          railH  = std::max(showLbl ? 66.0f : 44.0f, IconRailThickness(false));

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, railH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
    ImGui::Begin("##ModeToolbar", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    float statusPosScreenX = 0.0f;

    {
        ImDrawList*   dl      = ImGui::GetWindowDrawList();
        const float   islandH = 36.0f;
        const float   islandRounding = islandH * 0.5f;
        const float   islandGap      = 16.0f;

        ImFont* font          = ImGui::GetFont();
        const float labelSz   = ImGui::GetFontSize();
        const float iconSz    = 18.0f;
        const float targetRadius = 15.5f;

        ImGuiStorage* storage = ImGui::GetStateStorage();

        auto LerpVec4 = [](const ImVec4& a, const ImVec4& b, float t) -> ImVec4 {
            return ImVec4(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t
            );
        };

        auto measureLabelW = [&](const char* text) {
            return (showLbl && text) ? ImGui::CalcTextSize(text).x : 0.0f;
        };

        struct IslandItem {
            const char*           label;
            DrawIconFn            drawIcon;
            std::function<bool()> isActive;
            std::function<void()> onClick;
            ImU32                 themeColor;
        };

        ImVec2 winPos = ImGui::GetWindowPos();
        float winW = ImGui::GetWindowWidth();
        float islandOffsetY = showLbl ? 5.0f : ((railH - islandH) * 0.5f);
        float islandTopY = winPos.y + islandOffsetY;

        auto CalcIslandW = [&](const std::vector<IslandItem>& items) -> float {
            if (items.empty()) return 0.0f;
            const int count = (int)items.size();
            float maxPairHalfSum = 0.0f;
            for (int i = 0; i < count - 1; ++i) {
                if (showLbl && items[i].label && items[i + 1].label) {
                    float w1 = measureLabelW(items[i].label);
                    float w2 = measureLabelW(items[i + 1].label);
                    maxPairHalfSum = std::max(maxPairHalfSum, (w1 + w2) * 0.5f);
                }
            }
            float btnStep = showLbl ? std::max(44.0f, maxPairHalfSum + 10.0f) : (islandH + 4.0f);
            return islandRounding * 2.0f + (float)(count - 1) * btnStep;
        };

        auto RenderIsland = [&](const char* islandIdStr, const std::vector<IslandItem>& items, float islandScreenX) {
            if (items.empty()) return;

            const int count = (int)items.size();

            // Encontrar la separación mínima necesaria para que los textos estándar no se toquen
            float maxPairHalfSum = 0.0f;
            for (int i = 0; i < count - 1; ++i) {
                if (showLbl && items[i].label && items[i + 1].label) {
                    float w1 = measureLabelW(items[i].label);
                    float w2 = measureLabelW(items[i + 1].label);
                    maxPairHalfSum = std::max(maxPairHalfSum, (w1 + w2) * 0.5f);
                }
            }
            float btnStep = showLbl ? std::max(44.0f, maxPairHalfSum + 10.0f) : (islandH + 4.0f);
            float islandW = islandRounding * 2.0f + (float)(count - 1) * btnStep;

            ImVec2 islandMin = { islandScreenX, islandTopY };
            ImVec2 islandMax = { islandScreenX + islandW, islandTopY + islandH };

            // ── Fondo de la cápsula flotante (ceñida/pegada a los botones en todos sus bordes) ──
            dl->AddRectFilled({ islandMin.x, islandMin.y + 1.5f }, { islandMax.x, islandMax.y + 1.5f },
                              IM_COL32(0, 0, 0, 45), islandRounding);
            dl->AddRectFilled(islandMin, islandMax, IM_COL32(20, 22, 28, 235), islandRounding);
            dl->AddRect(islandMin, islandMax, IM_COL32(255, 255, 255, 24), islandRounding, 0, 1.0f);

            // Determinar qué ítem está activo
            int activeIndex = -1;
            for (int i = 0; i < count; ++i) {
                if (items[i].isActive && items[i].isActive()) {
                    activeIndex = i;
                    break;
                }
            }

            ImGuiID islandAnimId = ImGui::GetID(islandIdStr);
            float* pLastIdx = storage->GetFloatRef(islandAnimId ^ 0x0001u, 0.0f);
            if (activeIndex >= 0) {
                *pLastIdx = (float)activeIndex;
            }

            float targetCenterX = islandMin.x + islandRounding + (*pLastIdx) * btnStep;
            float targetCenterY = islandMin.y + islandH * 0.5f;

            int colIdx = (activeIndex >= 0) ? activeIndex : (int)(*pLastIdx);
            colIdx = std::clamp(colIdx, 0, count - 1);
            ImVec4 targetColor = ImGui::ColorConvertU32ToFloat4(items[colIdx].themeColor);

            float* pCurX     = storage->GetFloatRef(islandAnimId ^ 0x1111u, targetCenterX);
            float* pCurY     = storage->GetFloatRef(islandAnimId ^ 0x2222u, targetCenterY);
            float* pCurR     = storage->GetFloatRef(islandAnimId ^ 0x3333u, targetRadius);
            float* pCurAlpha = storage->GetFloatRef(islandAnimId ^ 0x4444u, (activeIndex >= 0 ? 1.0f : 0.0f));
            float* pColR     = storage->GetFloatRef(islandAnimId ^ 0x5555u, targetColor.x);
            float* pColG     = storage->GetFloatRef(islandAnimId ^ 0x6666u, targetColor.y);
            float* pColB     = storage->GetFloatRef(islandAnimId ^ 0x7777u, targetColor.z);

            float dt = ImGui::GetIO().DeltaTime;
            float animSpeed = 16.0f;

            float dx = targetCenterX - *pCurX;
            *pCurX     += dx * std::min(1.0f, dt * animSpeed);
            *pCurY     += (targetCenterY - *pCurY) * std::min(1.0f, dt * animSpeed);
            *pCurR     += (targetRadius - *pCurR) * std::min(1.0f, dt * animSpeed);
            *pCurAlpha += ((activeIndex >= 0 ? 1.0f : 0.0f) - *pCurAlpha) * std::min(1.0f, dt * animSpeed);
            *pColR     += (targetColor.x - *pColR) * std::min(1.0f, dt * animSpeed);
            *pColG     += (targetColor.y - *pColG) * std::min(1.0f, dt * animSpeed);
            *pColB     += (targetColor.z - *pColB) * std::min(1.0f, dt * animSpeed);

            float speedX = std::abs(dx) * animSpeed;
            float stretch = std::clamp(speedX * 0.012f, 0.0f, 4.0f);

            // ── Círculo animado tipo interruptor de teléfono (Switch Knob móvil) ──
            if (*pCurAlpha > 0.01f) {
                ImVec4 c = ImVec4(*pColR, *pColG, *pColB, *pCurAlpha);
                ImVec2 cCenter = ImVec2(*pCurX, *pCurY);
                float r = *pCurR;

                // 1. Resplandor exterior difuso (ceñido al borde)
                ImVec4 glow = c;
                glow.w = *pCurAlpha * 0.22f;
                if (stretch > 0.5f) {
                    dl->AddRectFilled(
                        { cCenter.x - r - stretch - 1.5f, cCenter.y - r - 1.5f },
                        { cCenter.x + r + stretch + 1.5f, cCenter.y + r + 1.5f },
                        ImGui::ColorConvertFloat4ToU32(glow), r + 1.5f);
                } else {
                    dl->AddCircleFilled(cCenter, r + 1.5f, ImGui::ColorConvertFloat4ToU32(glow), 24);
                }

                // 2. Círculo sólido / cápsula de interruptor
                ImVec4 fill = c;
                fill.w = *pCurAlpha * 0.35f;
                if (stretch > 0.5f) {
                    dl->AddRectFilled(
                        { cCenter.x - r - stretch, cCenter.y - r },
                        { cCenter.x + r + stretch, cCenter.y + r },
                        ImGui::ColorConvertFloat4ToU32(fill), r);
                    fill.w = *pCurAlpha * 0.90f;
                    dl->AddRect(
                        { cCenter.x - r - stretch, cCenter.y - r },
                        { cCenter.x + r + stretch, cCenter.y + r },
                        ImGui::ColorConvertFloat4ToU32(fill), r, 0, 1.3f);
                } else {
                    dl->AddCircleFilled(cCenter, r, ImGui::ColorConvertFloat4ToU32(fill), 24);
                    fill.w = *pCurAlpha * 0.90f;
                    dl->AddCircle(cCenter, r, ImGui::ColorConvertFloat4ToU32(fill), 24, 1.3f);
                }
            }

            // ── Botones interactivos e iconos ──
            for (int i = 0; i < count; ++i) {
                const auto& item = items[i];

                float btnCenterX = islandMin.x + islandRounding + (float)i * btnStep;
                float btnCenterY = islandMin.y + islandH * 0.5f;
                ImVec2 iconCenter = { btnCenterX, btnCenterY };

                float hitLeft  = (i == 0) ? islandMin.x : (btnCenterX - btnStep * 0.5f);
                float hitRight = (i == count - 1) ? islandMax.x : (btnCenterX + btnStep * 0.5f);
                float hitH     = showLbl ? (islandH + ImGui::GetTextLineHeight() + 8.0f) : islandH;
                ImVec2 bMin = { hitLeft, islandMin.y };
                ImVec2 bMax = { hitRight, islandMin.y + hitH };

                bool active = item.isActive ? item.isActive() : false;

                ImGuiID hovId = ImGui::GetID(item.label);
                float* pHoverT = storage->GetFloatRef(hovId ^ 0x51A17E5u, 0.0f);
                bool hovered = ImGui::IsMouseHoveringRect(bMin, bMax, false);
                *pHoverT += ((hovered ? 1.0f : 0.0f) - *pHoverT) * std::min(1.0f, dt * 14.0f);
                float tHov = *pHoverT;

                ImVec4 baseCol = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
                ImVec4 actCol  = ImGui::ColorConvertU32ToFloat4(item.themeColor);

                // Base circular redondeada sutil para cada icono
                dl->AddCircleFilled(iconCenter, targetRadius, IM_COL32(255, 255, 255, 8), 24);
                dl->AddCircle(iconCenter, targetRadius, IM_COL32(255, 255, 255, 14), 24, 1.0f);

                // Círculo con el color del botón en hover si no está activo
                if (tHov > 0.005f && !active) {
                    ImVec4 hovCircle = actCol;
                    hovCircle.w = tHov * 0.28f;
                    dl->AddCircleFilled(iconCenter, targetRadius, ImGui::ColorConvertFloat4ToU32(hovCircle), 24);
                    hovCircle.w = tHov * 0.45f;
                    dl->AddCircle(iconCenter, targetRadius, ImGui::ColorConvertFloat4ToU32(hovCircle), 24, 1.0f);
                }

                ImGui::SetCursorScreenPos(bMin);
                const std::string btnId = std::string("##islandBtn_") + item.label;
                if (ImGui::InvisibleButton(btnId.c_str(), { bMax.x - bMin.x, bMax.y - bMin.y })) {
                    if (item.onClick) item.onClick();
                }

                // ── Color de Icono y Texto (aparece en hover o cuando está presionado/activo) ──
                float colorT = active ? 1.0f : tHov;
                ImVec4 finalV = LerpVec4(baseCol, actCol, colorT);
                finalV.w = 1.0f;
                ImU32 textIconCol = ImGui::ColorConvertFloat4ToU32(finalV);

                // Dibujar icono concéntrico con el círculo redondeado
                float iconX = btnCenterX - iconSz * 0.5f;
                float iconY = btnCenterY - iconSz * 0.5f;
                if (item.drawIcon) {
                    item.drawIcon(dl, { iconX, iconY }, iconSz, textIconCol);
                }

                // Dibujar etiqueta FUERA de la isla, directamente abajo del botón con tamaño de texto completo
                if (showLbl && item.label) {
                    float lblW = measureLabelW(item.label);
                    float lblX = btnCenterX - lblW * 0.5f;
                    float lblY = islandMax.y + 4.0f;
                    dl->AddText({ lblX, lblY }, textIconCol, item.label);
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetTooltip("%s", item.label);
                }
            }
        };

        // ── Isla 1: Contenido (Canciones, Playlists, Biblia, Documentos) ───
        std::vector<IslandItem> island1 = {
            {
                "Canciones",
                Library::DrawIcon_Music,
                [this]() {
                    return !IsPanelCollapsed(0) && !IsAnyToolWindowOpen() &&
                           (m_LibraryPanelRef &&
                            m_LibraryPanelRef->GetCategory() == LibraryCategory::Songs &&
                            !m_LibraryPanelRef->IsShowingPlaylists() &&
                            m_LibraryPanelRef->GetSideMode() == LibrarySideMode::Categories);
                },
                [this]() {
                    CloseAllToolWindows();
                    if (m_LibraryPanelRef) {
                        m_LibraryPanelRef->SelectSideMode(LibrarySideMode::Categories);
                        m_LibraryPanelRef->SelectCategory(LibraryCategory::Songs);
                        m_LibraryPanelRef->SetShowPlaylists(false);
                    }
                    SetActiveLeftPanel(ActiveLeftPanel::Library);
                    SetPanelCollapsed(0, false);
                    SetPanelCollapsed(1, true);
                },
                IM_COL32(56, 189, 248, 255) // Cyan celeste
            },
            {
                "Playlists",
                Library::DrawIcon_Playlist,
                [this]() {
                    return !IsPanelCollapsed(0) && !IsAnyToolWindowOpen() &&
                           (m_LibraryPanelRef &&
                            m_LibraryPanelRef->GetCategory() == LibraryCategory::Songs &&
                            m_LibraryPanelRef->IsShowingPlaylists() &&
                            m_LibraryPanelRef->GetSideMode() == LibrarySideMode::Categories);
                },
                [this]() {
                    CloseAllToolWindows();
                    if (m_LibraryPanelRef) {
                        m_LibraryPanelRef->SelectSideMode(LibrarySideMode::Categories);
                        m_LibraryPanelRef->SelectCategory(LibraryCategory::Songs);
                        m_LibraryPanelRef->SetShowPlaylists(true);
                    }
                    SetActiveLeftPanel(ActiveLeftPanel::Library);
                    SetPanelCollapsed(0, false);
                    SetPanelCollapsed(1, true);
                },
                IM_COL32(168, 85, 247, 255) // Púrpura amatista
            },
            {
                "Biblia",
                Library::DrawIcon_Cross,
                [this]() {
                    return !IsAnyToolWindowOpen() &&
                           (m_LibraryPanelRef &&
                            m_LibraryPanelRef->GetCategory() == LibraryCategory::Bibles &&
                            m_LibraryPanelRef->GetSideMode() == LibrarySideMode::Categories);
                },
                [this]() {
                    CloseAllToolWindows();
                    if (m_LibraryPanelRef) {
                        m_LibraryPanelRef->SelectSideMode(LibrarySideMode::Categories);
                        m_LibraryPanelRef->SelectCategory(LibraryCategory::Bibles);
                    }
                    SetActiveLeftPanel(ActiveLeftPanel::Library);
                    SetPanelCollapsed(0, true);
                    SetPanelCollapsed(1, true);
                },
                IM_COL32(251, 191, 36, 255) // Ámbar dorado
            },
            {
                "Documentos",
                Library::DrawIcon_Document,
                [this]() {
                    return !IsPanelCollapsed(0) && !IsAnyToolWindowOpen() &&
                           (m_LibraryPanelRef &&
                            m_LibraryPanelRef->GetCategory() == LibraryCategory::Documents &&
                            m_LibraryPanelRef->GetSideMode() == LibrarySideMode::Categories);
                },
                [this]() {
                    CloseAllToolWindows();
                    if (m_LibraryPanelRef) {
                        m_LibraryPanelRef->SelectSideMode(LibrarySideMode::Categories);
                        m_LibraryPanelRef->SelectCategory(LibraryCategory::Documents);
                    }
                    SetActiveLeftPanel(ActiveLeftPanel::Library);
                    SetPanelCollapsed(0, false);
                    SetPanelCollapsed(1, true);
                },
                IM_COL32(52, 211, 153, 255) // Verde esmeralda
            }
        };

        // ── Isla 2: Notas - Voz - Estilos - Conexiones (exactamente 256px) ──
        std::vector<IslandItem> island2 = {
            {
                "Notas",
                HomeIcons::DrawIcon_Notepad,
                [this]() { return m_ShowNotes; },
                [this]() {
                    bool wasOpen = m_ShowNotes;
                    CloseAllToolWindows();
                    m_ShowNotes = !wasOpen;
                },
                IM_COL32(250, 204, 21, 255) // Amarillo sol
            },
            {
                "Voz",
                HomeIcons::DrawIcon_Microphone,
                [this]() { return m_ShowVoiceAssistant || Core::VoiceAssistant::Get().IsRunning(); },
                [this]() {
                    bool wasOpen = m_ShowVoiceAssistant;
                    CloseAllToolWindows();
                    m_ShowVoiceAssistant = !wasOpen;
                },
                IM_COL32(244, 63, 94, 255) // Rojo coral
            },
            {
                "Estilos",
                AppIcons::DrawIcon_Layers,
                []() { return ImGui::IsPopupOpen("##modeTbStylesPopup"); },
                [this]() {
                    CloseAllToolWindows();
                    ImGui::OpenPopup("##modeTbStylesPopup");
                },
                IM_COL32(236, 72, 153, 255) // Rosa fucsia
            },
            {
                "Conexiones",
                HomeIcons::DrawIcon_Broadcast,
                [this]() { return m_ShowConnectionsWindow; },
                [this]() {
                    bool wasOpen = m_ShowConnectionsWindow;
                    CloseAllToolWindows();
                    m_ShowConnectionsWindow = !wasOpen;
                },
                IM_COL32(14, 165, 233, 255) // Azul cielo
            }
        };

        if (general.showAIAssistantToolbar) {
            island2.push_back({
                "Asistente IA",
                HomeIcons::DrawIcon_Sparkle,
                [this]() { return m_ShowAIAssistant; },
                [this]() {
                    bool wasOpen = m_ShowAIAssistant;
                    CloseAllToolWindows();
                    m_ShowAIAssistant = !wasOpen;
                },
                IM_COL32(217, 70, 239, 255) // Magenta IA
            });
        }

        // ── Isla 3: Cuenta/Pro - Configuración - Ayuda ────────────
        auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();
        const bool isProActive = authMgr.IsPro();
        const char* accountLabel = isProActive ? "Pro" : (authMgr.IsLoggedIn() ? "Cuenta" : "Cuenta");
        ImU32 accountCol = isProActive
            ? IM_COL32(245, 180, 50, 255)  // Dorado Pro
            : IM_COL32(70, 195, 220, 255);  // Cyan

        std::vector<IslandItem> island3 = {
            {
                accountLabel,
                AppIcons::DrawIcon_Account,
                [this]() { return m_ShowConfig; },
                [this]() {
                    CloseAllToolWindows();
                    RequestSettings(10);
                },
                accountCol
            },
            {
                "Configuración",
                AppIcons::DrawIcon_Settings,
                [this]() { return m_ShowConfig; },
                [this]() {
                    bool wasOpen = m_ShowConfig;
                    CloseAllToolWindows();
                    m_ShowConfig = !wasOpen;
                },
                IM_COL32(99, 102, 241, 255) // Índigo elegante
            },
            {
                "Ayuda",
                AppIcons::DrawIcon_Help,
                []() { return ImGui::IsPopupOpen("##modeTbHelpPopup"); },
                [this]() {
                    CloseAllToolWindows();
                    ImGui::OpenPopup("##modeTbHelpPopup");
                },
                IM_COL32(45, 212, 191, 255) // Verde turquesa / menta
            }
        };

        // ── Medir ancho de la isla de estado en vivo (Público / Stage / Borrar Todo) ──
        const float statusRowH   = 28.0f;
        const float clearIconSz  = 14.0f;
        const float clearIconGap = 8.0f;
        float clearGroupW  = clearIconSz + clearIconGap + ImGui::CalcTextSize("Borrar Todo").x;
        float clearBtnW    = clearGroupW + 20.0f;
        float dotAudienceW = 5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Público").x + 14.0f;
        float dotStageW    = 5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Stage").x    + 14.0f;
        float statusTotalW = dotAudienceW + 8.0f + dotStageW + 8.0f + clearBtnW;
        float statusIslandW = statusTotalW + 12.0f;

        // Isla de estados: extremo derecho
        float statusScreenX = winPos.x + winW - statusIslandW - 14.0f;
        statusPosScreenX = statusScreenX;

        // ── Isla Pro: Nube Pro - Chat IA - Imágenes IA (Exclusiva para ProyecThor for Professionals) ──
        std::vector<IslandItem> islandPro;
        if (isProActive) {
            islandPro = {
                {
                    "Nube Pro",
                    AppIcons::DrawIcon_Cloud,
                    [this]() { return m_ShowProCloudSongs; },
                    [this]() {
                        bool wasOpen = m_ShowProCloudSongs;
                        CloseAllToolWindows();
                        m_ShowProCloudSongs = !wasOpen;
                    },
                    IM_COL32(56, 189, 248, 255) // Azul cielo / Nube
                },
                {
                    "Chat IA",
                    AppIcons::DrawIcon_Sparkle,
                    [this]() { return m_ShowProAIChat; },
                    [this]() {
                        bool wasOpen = m_ShowProAIChat;
                        CloseAllToolWindows();
                        m_ShowProAIChat = !wasOpen;
                    },
                    IM_COL32(192, 132, 252, 255) // Lavanda / IA
                },
                {
                    "Imágenes IA",
                    AppIcons::DrawIcon_Image,
                    [this]() { return m_ShowProImageGen; },
                    [this]() {
                        bool wasOpen = m_ShowProImageGen;
                        CloseAllToolWindows();
                        m_ShowProImageGen = !wasOpen;
                    },
                    IM_COL32(251, 191, 36, 255) // Dorado / Fondos
                }
            };
        }

        // 1. Primera isla: full izquierda
        float island1W = CalcIslandW(island1);
        float island1X = winPos.x + 14.0f;
        RenderIsland("island1", island1, island1X);

        // 2. Segunda isla: al medio exacto
        float island2W = CalcIslandW(island2);
        float island2X = winPos.x + (winW - island2W) * 0.5f;
        if (island2X < island1X + island1W + 14.0f) {
            island2X = island1X + island1W + 14.0f;
        }

        // 3. Tercera isla (Configuración / Ayuda) y Cuarta isla (Isla Pro):
        float rightEdge = statusScreenX - 12.0f;
        float island3W = CalcIslandW(island3);
        float island3X = rightEdge - island3W;

        float islandProW = 0.0f;
        float islandProX = 0.0f;
        if (isProActive && !islandPro.empty()) {
            islandProW = CalcIslandW(islandPro);
            islandProX = island3X - 12.0f - islandProW;
            if (island2X + island2W + 14.0f > islandProX) {
                island2X = islandProX - island2W - 14.0f;
            }
        } else {
            if (island2X + island2W + 14.0f > island3X) {
                island2X = island3X - island2W - 14.0f;
            }
        }

        RenderIsland("island2", island2, island2X);
        if (isProActive && !islandPro.empty()) {
            RenderIsland("islandPro", islandPro, islandProX);
        }
        RenderIsland("island3", island3, island3X);

        RenderStylesPopup();

        // ── Menú desplegable Ayuda ───────────────────────────────────────
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,          ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
        ImGui::PushStyleColor(ImGuiCol_Header,           ImVec4(0.20f, 0.40f, 0.85f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,    ImVec4(0.25f, 0.48f, 0.95f, 0.65f));

        if (ImGui::BeginPopup("##modeTbHelpPopup"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem("Notas de versión (PatchNotes)", "Ctrl+Alt+N"))
                m_ShowPatchNotesModal = true;

            if (ImGui::MenuItem(str.menuDocs, "F1"))
                ProyecThor::External::OpenURL("https://proyecthor.web.app/");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.345f, 0.403f, 0.941f, 1.0f));
            if (ImGui::MenuItem("Reporte de bugs"))
                ProyecThor::External::OpenURL("https://github.com/TheVixcho/ProyecThor/issues");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.145f, 0.827f, 0.400f, 1.0f));
            bool whatsappOpen = ImGui::BeginMenu("Canal de WhatsApp");
            ImGui::PopStyleColor();
            if (whatsappOpen)
            {
                const char* whatsappUrl = "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19";
                RenderSocialQrMenu(whatsappUrl);
                ImGui::EndMenu();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.290f, 0.780f, 0.490f, 1.0f));
            bool mobileAppOpen = ImGui::BeginMenu("App móvil (control remoto)");
            ImGui::PopStyleColor();
            if (mobileAppOpen)
            {
                const char* mobileAppUrl = "https://play.google.com/store/apps/details?id=the.proyecthor.mobile";
                RenderSocialQrMenu(mobileAppUrl);
                ImGui::EndMenu();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
            if (ImGui::MenuItem(str.menuDonations))
                ProyecThor::External::OpenURL("https://ko-fi.com/vixcho");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::MenuItem(str.menuAbout))
                g_ShowAbout = true;

            ImGui::Spacing();
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    RenderModeToolbarStatusActions(statusPosScreenX, railH);

    ImGui::End();

    vp->WorkPos.y  += railH;
    vp->WorkSize.y -= railH;
}

void UIManager::RenderModeToolbarStatusActions(float screenX, float railH)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();
    const bool isPro = authMgr.IsPro();

    const bool  audienceOn = core.IsProjecting();
    const bool  stageOn    = isPro ? (sd.useLAN ? core.IsStreamingNet() : core.IsStaging()) : false;
    if (!isPro && core.IsStaging()) {
        core.SetStaging(false);
    }
    const float rowH       = 28.0f;
    const float islandH    = 36.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const char* clearLabel = "Borrar Todo";
    ImVec2      clearTxtSz = ImGui::CalcTextSize(clearLabel);
    const float clearIconSz  = 14.0f;
    const float clearIconGap = 8.0f;
    float       clearGroupW  = clearIconSz + clearIconGap + clearTxtSz.x;
    float       clearBtnW    = clearGroupW + 20.0f;

    const char* stageLabel = isPro ? "Stage" : "Stage [PRO]";
    ImVec2 dotSzAudience = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Público").x + 14.0f, rowH);
    ImVec2 dotSzStage    = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize(stageLabel).x    + 14.0f, rowH);

    const float gap   = 8.0f;
    float       totalW = dotSzAudience.x + gap + dotSzStage.x + gap + clearBtnW;

    float islandW = totalW + 12.0f;
    float islandY = (railH - islandH) * 0.5f;

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 islandMin = { screenX, winPos.y + islandY };
    ImVec2 islandMax = { screenX + islandW, winPos.y + islandY + islandH };

    // ── Fondo de la cápsula de estados en vivo (centrada verticalmente en la barra) ──
    dl->AddRectFilled({ islandMin.x, islandMin.y + 1.5f }, { islandMax.x, islandMax.y + 1.5f },
                      IM_COL32(0, 0, 0, 45), islandH * 0.5f);
    dl->AddRectFilled(islandMin, islandMax,
                      IM_COL32(20, 22, 28, 235), islandH * 0.5f);
    dl->AddRect(islandMin, islandMax,
                IM_COL32(255, 255, 255, 24), islandH * 0.5f, 0, 1.0f);

    float startX = (screenX - winPos.x) + 6.0f;
    ImGui::SetCursorPos(ImVec2(startX, (railH - rowH) * 0.5f));

    if (StatusDotToggle(dl, "##modeTbDotAudience", "Público", audienceOn, MT::k_LiveAccent, rowH))
        ToggleAudience(!audienceOn);

    ImGui::SameLine(0.0f, gap);

    if (StatusDotToggle(dl, "##modeTbDotStage", stageLabel, stageOn, isPro ? MT::k_PrevAccent : ImVec4(0.85f, 0.65f, 0.15f, 1.0f), rowH)) {
        if (!isPro) {
            ShowProFeatureModal(
                "Monitor de Escenario (Stage Display)",
                "La salida de Monitor de Escenario (Stage Display) para predicadores y músicos es una función exclusiva de ProyecThor for Professionals.\n\nEn la versión estándar puedes proyectar a 1 pantalla secundaria estándar. Activa tu suscripción Pro para habilitar el monitor de escenario.");
        } else {
            ToggleStageQuick(!stageOn);
        }
    }

    ImGui::SameLine(0.0f, gap);

    {
        auto toVec4 = [](ImU32 c, float alpha) {
            ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
            v.w = alpha;
            return v;
        };
        ImGui::PushStyleColor(ImGuiCol_Button,        toVec4(DS::DangerColor, 40.0f  / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(DS::DangerColor, 90.0f  / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  toVec4(DS::DangerColor, 140.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,        toVec4(DS::DangerColor, 100.0f / 255.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool clicked = ImGui::Button("##modeTbBorrarTodo", ImVec2(clearBtnW, rowH));

        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        float  cStartX = bMin.x + ((bMax.x - bMin.x) - clearGroupW) * 0.5f;
        float  cCenterY = (bMin.y + bMax.y) * 0.5f;
        ImU32  dangerCol = DS::DangerColor;

        auto it = StyleGeneralApp::Icons.find("cleaning_services");
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
        {
            dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
                { cStartX, cCenterY - clearIconSz * 0.5f }, { cStartX + clearIconSz, cCenterY + clearIconSz * 0.5f },
                ImVec2(0, 0), ImVec2(1, 1), dangerCol);
        }
        dl->AddText({ cStartX + clearIconSz + clearIconGap, cCenterY - clearTxtSz.y * 0.5f }, dangerCol, clearLabel);

        if (clicked)
        {
            core.ClearLayer2();
            core.StopBackgroundMedia();
            if (auto* a   = core.GetAnnouncementsRef())  a->SetLive(false);
            if (auto* clk = core.GetOClockRef())          clk->StopTransmitting();
            if (auto* cap = core.GetCapturePanelRef())    cap->Stop();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }
}

void UIManager::ToggleAudience(bool active)
{
    auto& core = Core::PresentationCore::Get();

    if (active) {
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        int monitorIndex = std::clamp(
            settings.projection.targetMonitor < 0 ? (monitorCount > 1 ? 1 : 0) : settings.projection.targetMonitor,
            0, std::max(0, monitorCount - 1));

        core.SetTargetMonitor(monitorIndex);
#ifndef _WIN32
        core.CreateProjectorWindow(monitorIndex);
#endif
        core.SetProjecting(true);
        std::cout << "[UIManager] Proyección iniciada en monitor " << monitorIndex << ".\n";
    } else {
#ifndef _WIN32
        core.DestroyProjectorWindow();
#endif
        core.SetProjecting(false);
        std::cout << "[UIManager] Proyección detenida.\n";
    }
}

void UIManager::ToggleStageQuick(bool active)
{
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();
    if (!authMgr.IsPro()) {
        ShowProFeatureModal("Monitor de Escenario (Stage Display)",
                            "La salida de Monitor de Escenario (Stage Display) para predicadores y músicos es una función exclusiva de ProyecThor for Professionals.\n\nEn la versión estándar puedes proyectar a 1 pantalla secundaria estándar. Activa tu suscripción Pro para habilitar el monitor de escenario.");
        Core::PresentationCore::Get().SetStaging(false);
        return;
    }

    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    if (active) {
        if (sd.useLAN) {
            core.ToggleNetworkStream(true, sd.lanPort);
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        int stageMonitorIndex = (monitorCount > 1)
            ? std::clamp(sd.monitorIndex < 0 ? 1 : sd.monitorIndex, 0, monitorCount - 1)
            : 0;
        sd.monitorIndex = stageMonitorIndex;
        ProyecThor::Settings::SettingsManager::Get().Save();
#ifndef _WIN32
        core.CreateStageWindow(stageMonitorIndex);
#endif
        core.SetStaging(true, stageMonitorIndex);
        std::cout << "[UIManager] Stage Display iniciado en monitor " << stageMonitorIndex << ".\n";
    } else {
        if (sd.useLAN) {
            core.ToggleNetworkStream(false);
        } else {
#ifndef _WIN32
            core.DestroyStageWindow();
#endif
            core.SetStaging(false);
        }
        std::cout << "[UIManager] Stage Display detenido.\n";
    }
}

bool UIManager::IsAnyToolWindowOpen() const
{
    return m_ShowNotes || m_ShowConnectionsWindow || m_ShowConfig || m_ShowVoiceAssistant || m_ShowAIAssistant ||
           m_ShowProCloudSongs || m_ShowProAIChat || m_ShowProImageGen;
}

void UIManager::CloseAllToolWindows()
{
    if (m_ShowNotes)
        m_NotesPanel.PersistNow();
    m_ShowNotes             = false;
    m_ShowConnectionsWindow = false;
    m_ShowConfig            = false;
    m_ShowVoiceAssistant    = false;
    m_ShowAIAssistant       = false;
    m_ShowProCloudSongs     = false;
    m_ShowProAIChat         = false;
    m_ShowProImageGen       = false;
}

void UIManager::ToggleNotesWindow()
{
    m_ShowNotes = !m_ShowNotes;
    if (!m_ShowNotes)
        m_NotesPanel.PersistNow();
}

void UIManager::RenderNotesWindow()
{
    static bool s_WasOpenLastFrame = false;
    const bool  justOpened = !s_WasOpenLastFrame;
    s_WasOpenLastFrame = true;

    const ImVec2 baseSize(580.0f, 620.0f);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    if (justOpened) {
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(baseSize, ImGuiCond_Always);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 480.0f), ImVec2(10000.0f, 10000.0f));

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Notas", m_GlassRenderer, &m_ShowNotes,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(16.0f, 14.0f));

    if (open)
        m_NotesPanel.Render();

    DS::EndGlassPanel();

    if (!m_ShowNotes) {
        m_NotesPanel.PersistNow();
        s_WasOpenLastFrame = false;
    }
}

void UIManager::RenderAIAssistantWindow()
{
    m_AIAssistant.Render(&m_ShowAIAssistant, m_GlassRenderer);
}

void UIManager::RenderVoiceAssistantWindow()
{
    m_VoiceAssistantPanel.Render(&m_ShowVoiceAssistant, m_GlassRenderer);
}

void UIManager::RenderProWindows()
{
    m_ProCloudSongsPanel.Render(&m_ShowProCloudSongs, m_GlassRenderer);
    m_ProAIChatPanel.Render(&m_ShowProAIChat, m_GlassRenderer);
    m_ProImageGenPanel.Render(&m_ShowProImageGen, m_GlassRenderer);
}

void UIManager::RenderConnectionsWindow()
{
    static bool s_WasOpenLastFrame = false;
    const bool  justOpened = !s_WasOpenLastFrame;
    s_WasOpenLastFrame = true;

    const ImVec2 baseSize(760.0f, 680.0f);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    if (justOpened) {
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(baseSize, ImGuiCond_Always);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(620.0f, 520.0f), ImVec2(10000.0f, 10000.0f));

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Centro de Conexiones", m_GlassRenderer, &m_ShowConnectionsWindow,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(16.0f, 14.0f));

    if (open)
    {
        const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
        auto& core = Core::PresentationCore::Get();

        // ── Cabecera / Status Summary Bar ──
        bool lanActive = core.GetNetworkServer() && core.GetNetworkServer()->IsRunning();
        bool syncActive = ProyecThor::Settings::SettingsManager::Get().GetSettings().sync.enabled;
        bool bcastActive = m_Broadcast.IsStreaming();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(theme.surface1[0], theme.surface1[1], theme.surface1[2], 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(theme.border[0], theme.border[1], theme.border[2], 0.35f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));

        if (ImGui::BeginChild("##conn_summary_bar", ImVec2(0, 48), true, ImGuiWindowFlags_NoScrollbar))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float itemW = ImGui::GetContentRegionAvail().x / 4.0f;

            auto DrawStatusChip = [&](int idx, const char* label, bool active, const char* detail) {
                float chipX = p0.x + idx * itemW;
                ImVec2 dotPos(chipX + 8.0f, p0.y + 14.0f);
                ImU32 dotCol = active ? IM_COL32(34, 197, 94, 255) : IM_COL32(148, 163, 184, 160);
                dl->AddCircleFilled(dotPos, 4.0f, dotCol);
                if (active) dl->AddCircle(dotPos, 6.5f, IM_COL32(34, 197, 94, 90), 16, 1.2f);

                ImGui::SetCursorScreenPos(ImVec2(chipX + 18.0f, p0.y + 4.0f));
                ImGui::TextColored(ImVec4(theme.textPrimary[0], theme.textPrimary[1], theme.textPrimary[2], 1.0f), "%s", label);
                ImGui::SetCursorScreenPos(ImVec2(chipX + 18.0f, p0.y + 18.0f));
                ImGui::TextColored(ImVec4(theme.textDim[0], theme.textDim[1], theme.textDim[2], 0.9f), "%s", detail);
            };

            DrawStatusChip(0, "Red (LAN)", lanActive, lanActive ? "En línea :8080" : "Detenido");
            DrawStatusChip(1, "App Móvil", syncActive, syncActive ? "Sincronizado" : "Inactivo");
            DrawStatusChip(2, "Transmisión", bcastActive, bcastActive ? "Emitiendo" : "En espera");
            DrawStatusChip(3, "OSC Control", true, ":8000 / :9000");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Pestañas de Conexión Estilo Segmented Bar ──
        struct TabInfo { const char* label; const char* icon; };
        static const TabInfo tabs[] = {
            { "Red (LAN)",      "\xF0\x9F\x8C\x90" }, // 🌐
            { "App Móvil",      "\xF0\x9F\x93\xB1" }, // 📱
            { "Transmisión",    "\xF0\x9F\x93\xA1" }, // 📡
            { "Control OSC",    "\xF0\x9F\x8E\x9B" }, // 🎛
            { "Chat de Equipo", "\xF0\x9F\x92\xAC" }, // 💬
        };
        const int tabCount = 5;
        const float tabW = (ImGui::GetContentRegionAvail().x - (tabCount - 1) * 6.0f) / tabCount;

        for (int i = 0; i < tabCount; i++) {
            if (i > 0) ImGui::SameLine(0, 6.0f);

            bool isSelected = (m_ConnectionsActiveTab == i);
            ImVec4 btnBg = isSelected ? ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.85f)
                                      : ImVec4(theme.surface1[0], theme.surface1[1], theme.surface1[2], 0.55f);
            ImVec4 btnTxt = isSelected ? ImVec4(1, 1, 1, 1)
                                       : ImVec4(theme.textPrimary[0], theme.textPrimary[1], theme.textPrimary[2], 0.85f);

            ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(theme.surface2[0], theme.surface2[1], theme.surface2[2], 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(theme.surface3[0], theme.surface3[1], theme.surface3[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, btnTxt);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            char tabLabel[64];
            snprintf(tabLabel, sizeof(tabLabel), "%s %s##conn_tab_%d", tabs[i].icon, tabs[i].label, i);
            if (ImGui::Button(tabLabel, ImVec2(tabW, 34.0f))) {
                m_ConnectionsActiveTab = i;
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // ── Contenedor de la Pestaña Activa ──
        if (ImGui::BeginChild("##conn_tab_content", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            switch (m_ConnectionsActiveTab) {
                case 0: // Red LAN
                    m_Red.RenderContent();
                    break;
                case 1: // App Móvil / Sync
                    m_Sync.RenderContent();
                    break;
                case 2: // Transmisión / Captura
                    m_Broadcast.RenderCaptureSection();
                    ImGui::Spacing();
                    m_Broadcast.RenderLayerSection();
                    ImGui::Spacing();
                    m_Broadcast.RenderStartSection();
                    break;
                case 3: // OSC
                    m_OSC.RenderContent();
                    break;
                case 4: // Chat del Equipo
                    m_Chat.RenderContent();
                    break;
                default:
                    break;
            }
        }
        ImGui::EndChild();
    }

    DS::EndGlassPanel();

    if (!m_ShowConnectionsWindow) {
        s_WasOpenLastFrame = false;
    }
}

void UIManager::RenderUrlImportModal()
{
    bool resultReady = false;
    ProyecThor::Core::SubtitleFetchResult resultCopy;
    {
        std::lock_guard<std::mutex> lk(m_UrlImportMutex);
        if (m_UrlImportResult.has_value() && !m_UrlImportRunning) {
            resultCopy   = *m_UrlImportResult;
            resultReady  = true;
            m_UrlImportResult.reset();
        }
    }
    if (resultReady) {
        if (m_UrlImportThread.joinable())
            m_UrlImportThread.join();

        if (resultCopy.success) {
            if (m_ShowUrlImport) {
                ProyecThor::Library::CreateNewSongFromText(resultCopy.title, resultCopy.lyrics);
                m_ShowUrlImport         = false;
                m_UrlImportBuffer[0]    = '\0';
                m_UrlImportLastError.clear();
            }
        } else {
            m_UrlImportLastError = resultCopy.error;
        }
    }

    if (!m_ShowUrlImport) return;

    const ImVec2 baseSize(480.0f, 230.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(workCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(baseSize, ImGuiCond_Appearing);

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    bool open = ImGui::Begin("Importar desde URL", &m_ShowUrlImport,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize);

    if (open) {
        ImGui::TextWrapped("Pega el link de un video (YouTube y similares). Se buscan sus subtitulos "
                            "-- primero en espa\xC3\xB1ol, si no en ingles -- y se usan como letra "
                            "inicial de una cancion nueva.");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::BeginDisabled(m_UrlImportRunning);
        ImGui::SetNextItemWidth(-1.0f);
        bool enterPressed = ImGui::InputTextWithHint("##urlImportInput", "https://www.youtube.com/watch?v=...",
            m_UrlImportBuffer, sizeof(m_UrlImportBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        bool wantStart = false;
        if (m_UrlImportRunning) {
            ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Buscando subtitulos...");
        } else {
            if (ImGui::Button("Importar", ImVec2(120.0f, 32.0f)))
                wantStart = true;
            if (enterPressed)
                wantStart = true;
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(100.0f, 32.0f))) {
                m_ShowUrlImport      = false;
                m_UrlImportBuffer[0] = '\0';
                m_UrlImportLastError.clear();
            }
        }

        if (!m_UrlImportLastError.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", m_UrlImportLastError.c_str());
            ImGui::PopStyleColor();
        }

        if (wantStart && !m_UrlImportRunning && m_UrlImportBuffer[0] != '\0') {
            if (m_UrlImportThread.joinable()) m_UrlImportThread.join();
            m_UrlImportLastError.clear();
            m_UrlImportRunning = true;
            {
                std::lock_guard<std::mutex> lk(m_UrlImportMutex);
                m_UrlImportResult.reset();
            }
            std::string urlCopy = m_UrlImportBuffer;
            m_UrlImportThread = std::thread([this, urlCopy]() {
                ProyecThor::Core::SubtitleFetchResult res = ProyecThor::Core::FetchSubtitlesAsLyrics(urlCopy);
                std::lock_guard<std::mutex> lk(m_UrlImportMutex);
                m_UrlImportResult  = std::move(res);
                m_UrlImportRunning = false;
            });
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void UIManager::RenderStylesPopup()
{
    ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopup("##modeTbStylesPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
        ImGui::TextUnformatted("ESTILOS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        auto& core  = Core::PresentationCore::Get();
        auto  names = core.GetSavedStyleNames();

        if (names.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            ImGui::TextWrapped("Todavia no guardaste ningun estilo (Diseño > Estilos).");
            ImGui::PopStyleColor();
        } else {
            for (const auto& name : names) {
                if (ImGui::Selectable(name.c_str())) {
                    core.ApplyStyleByName(name);
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
}

void UIManager::RenderQuickSwitcher()
{
    struct QSItem { WorkspaceMode mode; DrawIconFn icon; const char* label; };
    static const QSItem kItems[] = {
        { WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
    };
    constexpr int kCount = 1;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        m_QuickSwitchOpen  = !m_QuickSwitchOpen;
        for (int i = 0; i < kCount; i++)
            if (kItems[i].mode == m_Mode) m_QuickSwitchIndex = i;
    }
    if (!m_QuickSwitchOpen) return;

    auto Activate = [&](int idx) {
        (void)idx;
        m_Mode = WorkspaceMode::Projector;
        m_ResetLayout = true;
        m_QuickSwitchOpen = false;
    };

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) m_QuickSwitchOpen = false;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        m_QuickSwitchIndex = (m_QuickSwitchIndex + 1) % kCount;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        m_QuickSwitchIndex = (m_QuickSwitchIndex + kCount - 1) % kCount;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        Activate(m_QuickSwitchIndex);
    if (!m_QuickSwitchOpen) return;

    const ImVec2 winSize(340.0f, 44.0f + kCount * 42.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - winSize.x) * 0.5f,
                                    vp->WorkPos.y + (vp->WorkSize.y - winSize.y) * 0.5f));
    ImGui::SetNextWindowSize(winSize);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(10.0f, 10.0f));

    ImGui::Begin("##QuickSwitcher", &m_QuickSwitchOpen,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoDocking    | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoNav);

    ImGui::TextDisabled("Ir a...   (flechas + Enter, Esc para cerrar)");
    ImGui::Spacing();

    for (int i = 0; i < kCount; i++)
    {
        bool sel = (i == m_QuickSwitchIndex);
        ImGui::PushID(i);

        ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const float rowH = 38.0f;

        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.45f, 0.90f, 0.55f));
        if (ImGui::Selectable("##qsRow", sel, 0, ImVec2(0.0f, rowH)))
            Activate(i);
        if (sel) ImGui::PopStyleColor();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 182, 198, 255);
        kItems[i].icon(dl, ImVec2(rowPos.x + 8.0f, rowPos.y + (rowH - 22.0f) * 0.5f), 22.0f, col);
        dl->AddText(ImVec2(rowPos.x + 42.0f, rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
                    col, kItems[i].label);

        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void UIManager::RenderMainMenuBar()
{
    // Obsoleto: Los menús ("letras") ahora están completamente integrados en RenderModeToolbar().
}

void UIManager::BeginDockspace()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    window_flags |=
        ImGuiWindowFlags_NoTitleBar            | ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoResize              | ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus          |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("ProyecThorWorkspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // Debe correr ANTES de someter el DockSpace de este frame -- empuja el
    // tamaño animado de los nodos colapsados/expandidos (ver Alt Gr + 1..4
    // en RenderAll) para que el paso de layout de abajo ya lo tenga en
    // cuenta, mismo criterio que DockBuilderSetNodeSize(dockspace_id, ...)
    // un poco mas abajo en esta misma funcion.
    UpdatePanelCollapseAnim();

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Ajustes > Apariencia > Entorno de trabajo cambio desde el ultimo frame
    // -- fuerza un reset de layout sin que la pagina de Ajustes necesite
    // conocer a UIManager (solo escribe el setting, esto lo detecta solo).
    {
        int currentPreset = (int)ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace.layoutPreset;
        if (currentPreset != m_LastWorkspacePreset)
        {
            m_LastWorkspacePreset = currentPreset;
            m_ResetLayout         = true;
        }
    }

    if (m_ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id))
    {
        m_ResetLayout = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        using ProyecThor::Settings::WorkspaceLayoutPreset;
        switch (ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace.layoutPreset)
        {
            case WorkspaceLayoutPreset::Simple:
                BuildWorkspaceLayoutSimple(dockspace_id);
                break;
            case WorkspaceLayoutPreset::Broadcast:
                BuildWorkspaceLayoutBroadcast(dockspace_id);
                break;
            case WorkspaceLayoutPreset::Library:
                BuildWorkspaceLayoutLibrary(dockspace_id);
                break;
            case WorkspaceLayoutPreset::Video:
                BuildWorkspaceLayoutVideo(dockspace_id);
                break;
            default:
                BuildWorkspaceLayoutClassic(dockspace_id);
                break;
        }

        for (auto& p : m_PanelCollapse)
            if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(p.nodeId))
                p.expandedSize = node->Size;

        m_FocusViewNextFrame = true;
    }
}

// ── Entorno de trabajo: "Clásico" (default) ─────────────────────────────────
// Vista en Vivo a la derecha (alto completo); a la izquierda-centro, Diseño
// ("bottommain") pasa por debajo TANTO de Biblioteca (izquierda) COMO de Home
// ("main"), subiendo Biblioteca y su barra inferior (+/importar/borrar) y
// otorgándole a Diseño todo el ancho combinado para mostrar cómodamente todos
// sus paneles sin apretarlos.
void UIManager::BuildWorkspaceLayoutClassic(ImGuiID dockspace_id)
{
    const auto& str = ProyecThor::UI::GetUIStrings();
    ImGuiID     dock_main = dockspace_id;

    ImGuiID dock_right;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, &dock_right, &dock_main);

    ImGuiID dock_top_area, dock_main_bottom;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_main_bottom, &dock_top_area);

    ImGuiID dock_left, dock_main_top;
    ImGui::DockBuilderSplitNode(dock_top_area, ImGuiDir_Left, 0.347f, &dock_left, &dock_main_top);

    ImGui::DockBuilderDockWindow(str.library,        dock_left);
    ImGui::DockBuilderDockWindow("Home",             dock_main_top);
    ImGui::DockBuilderDockWindow("Vista en Vivo",    dock_right);
    ImGui::DockBuilderDockWindow("Diseño",           dock_main_bottom);

    ImGuiID leafNodes[] = { dock_left, dock_main_top, dock_main_bottom, dock_right };
    for (ImGuiID nodeId : leafNodes)
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(nodeId))
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGui::DockBuilderFinish(dockspace_id);

    // Orden fijo (1=Biblioteca, 2=Diseño, 3=Vista en Vivo, 4=Home), pedido
    // explicito del operador -- ver Alt Gr + 1..4 en RenderAll.
    m_PanelCollapse[0] = { dock_left,        ImVec2(0, 0), true,  false, 0.0f }; // 1: Biblioteca
    m_PanelCollapse[1] = { dock_main_bottom, ImVec2(0, 0), false, false, 0.0f }; // 2: Diseño
    m_PanelCollapse[2] = { dock_right,       ImVec2(0, 0), true,  false, 0.0f }; // 3: Vista en Vivo
    m_PanelCollapse[3] = { dock_main_top,    ImVec2(0, 0), true,  false, 0.0f }; // 4: Home
}

// ── Entorno de trabajo: "Simple" (estilo Holyrics) ──────────────────────────
// Cuatro columnas de alto completo, nada apilado verticalmente: Biblioteca |
// Home | Vista en Vivo | Diseño. Diseño se corre TODO a la derecha (a la
// derecha de Vista en Vivo) en vez de vivir abajo de Home -- Home y Vista en
// Vivo quedan cada uno en su propia columna al medio. Diseño (y en paneles
// angostos, Vista en Vivo) quedan angostos y altos aca -- ver
// StylesHubPanel::Render, que detecta esto y pasa su rail de iconos a
// vertical, y ViewPanel::RenderCompactWide para el caso ancho-y-bajo (no
// aplica en este preset, Vista en Vivo ya es una columna alta).
void UIManager::BuildWorkspaceLayoutSimple(ImGuiID dockspace_id)
{
    const auto& str = ProyecThor::UI::GetUIStrings();
    ImGuiID     dock_main = dockspace_id;

    // Ratios pensados para que Vista en Vivo (video 16:9, cuyo alto depende
    // de su ancho) quede con una columna realmente usable -- antes le tocaba
    // ~26% y Home (que solo necesita ancho para su Preview+Cola) se llevaba
    // demasiado, dejando el video chico con espacio vacio abajo.
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.22f, nullptr, &dock_main);
    ImGuiID dock_left_top, dock_left_bottom;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.40f, &dock_left_bottom, &dock_left_top);

    ImGuiID dock_right;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.20f, &dock_right, &dock_main);

    ImGuiID dock_mid_right;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.52f, &dock_mid_right, &dock_main);

    ImGui::DockBuilderDockWindow(str.library,        dock_left_top);
    ImGui::DockBuilderDockWindow("Home",             dock_main);
    ImGui::DockBuilderDockWindow("Vista en Vivo",    dock_mid_right);
    ImGui::DockBuilderDockWindow("Diseño",           dock_right);

    ImGuiID leafNodes[] = { dock_left_top, dock_left_bottom, dock_main, dock_mid_right, dock_right };
    for (ImGuiID nodeId : leafNodes)
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(nodeId))
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGui::DockBuilderFinish(dockspace_id);

    // Las 4 son columnas puras (splits izquierda/derecha en cadena) -- todas
    // colapsan por ancho.
    m_PanelCollapse[0] = { dock_left,      ImVec2(0, 0), true, false, 0.0f }; // 1: Biblioteca
    m_PanelCollapse[1] = { dock_right,     ImVec2(0, 0), true, false, 0.0f }; // 2: Diseño
    m_PanelCollapse[2] = { dock_mid_right, ImVec2(0, 0), true, false, 0.0f }; // 3: Vista en Vivo
    m_PanelCollapse[3] = { dock_main,      ImVec2(0, 0), true, false, 0.0f }; // 4: Home
}

// ── Entorno de trabajo: "Transmisión" ───────────────────────────────────────
// Streaming (Captura/Capas/Iniciar, ver StreamingWorkspacePanel) a pantalla
// completa, SOLO -- pedido explicito: "elimina todo lo relacionado a
// proyeccion, es solo para ver la transmision a un servidor... en su lugar
// paneles para manejar las capas". Antes tambien mostraba Biblioteca/Home/
// Diseño en tres columnas abajo (todo eso es "proyeccion") -- se saco del
// todo; el manejo de fuentes ahora vive DENTRO de la propia franja de
// Transmisión, ver BroadcastPanel::RenderLayerSection (lista de capas:
// Captura/Overlay/Vista en vivo).
void UIManager::BuildWorkspaceLayoutBroadcast(ImGuiID dockspace_id)
{
    ImGui::DockBuilderDockWindow("Transmisión", dockspace_id);
    ImGui::DockBuilderFinish(dockspace_id);

    for (auto& p : m_PanelCollapse) { p.nodeId = 0; p.collapsed = false; p.animT = 0.0f; }
}

// ── Entorno de trabajo: "Biblioteca" ────────────────────────────────────────
// Biblioteca (bloqueada en la categoria Medios, ver LibraryPanel::
// SetMediaOnlyMode -- sincronizado cada frame en RenderAll segun el preset
// activo) a la izquierda, Home (Preview, ya se adapta solo al tipo de
// contenido seleccionado) ocupando el resto -- sin Vista en Vivo/Diseño.
// Pensado para operar solo reproduciendo contenido de la biblioteca; "Abrir
// con ProyecThor" tambien activa este preset para esa sesion (ver
// EnterLibraryWorkspaceMode en main.cpp), sin pisar el preset guardado.
void UIManager::BuildWorkspaceLayoutLibrary(ImGuiID dockspace_id)
{
    const auto& str = ProyecThor::UI::GetUIStrings();
    ImGuiID     dock_main = dockspace_id;

    ImGuiID dock_left;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.30f, &dock_left, &dock_main);

    ImGui::DockBuilderDockWindow(str.library, dock_left);
    ImGui::DockBuilderDockWindow("Home",      dock_main);

    ImGuiID leafNodes[] = { dock_left, dock_main };
    for (ImGuiID nodeId : leafNodes)
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(nodeId))
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGui::DockBuilderFinish(dockspace_id);

    // Diseño/Vista en Vivo no existen en este layout -- nodeId=0 es un
    // no-op seguro para Alt Gr+2/3 (ver IsPanelCollapsedForRender/
    // TogglePanelCollapse). Biblioteca/Home SI pueden colapsar (Alt Gr+1/4),
    // mismo split izq/der que Vista en Vivo en Clasico -> axisIsWidth=true.
    m_PanelCollapse[0] = { dock_left, ImVec2(0, 0), true, false, 0.0f }; // 1: Biblioteca
    m_PanelCollapse[1] = { 0,         ImVec2(0, 0), false, false, 0.0f }; // 2: Diseño
    m_PanelCollapse[2] = { 0,         ImVec2(0, 0), false, false, 0.0f }; // 3: Vista en Vivo
    m_PanelCollapse[3] = { dock_main, ImVec2(0, 0), true, false, 0.0f }; // 4: Home
}

// ── Entorno de trabajo: "Producción" ────────────────────────────────────────
// Una sola ventana ocupa todo el dockspace, sin Biblioteca/Home/Vista en
// Vivo/Diseño alrededor -- VideoEditorPanel (titulo real de ventana
// "Producción") absorbe Render/Colorimetria/Canales de trabajo/Audio(DAW)/
// Overlays como pestañas internas.
void UIManager::BuildWorkspaceLayoutVideo(ImGuiID dockspace_id)
{
    ImGui::DockBuilderDockWindow("Producción", dockspace_id);
    ImGui::DockBuilderFinish(dockspace_id);
    for (auto& p : m_PanelCollapse) { p.nodeId = 0; p.collapsed = false; p.animT = 0.0f; }
}

bool UIManager::IsPanelCollapsedForRender(const std::string& name) const
{
    int idx = -1;
    if      (name == "Library")        idx = 0; // Biblioteca
    else if (name == "Diseño")          idx = 1;
    else if (name == "Vista en Vivo")   idx = 2;
    else if (name == "Home")            idx = 3;

    if (idx < 0) return false;

    const PanelCollapseState& p = m_PanelCollapse[idx];
    if (p.nodeId == 0) return false; // ese panel no existe en el preset activo
    return p.animT > 0.5f;
}

void UIManager::TogglePanelCollapse(int index)
{
    if (index < 0 || index >= kCollapsiblePanelCount) return;
    if (m_PanelCollapse[index].nodeId == 0) return;
    m_PanelCollapse[index].collapsed = !m_PanelCollapse[index].collapsed;
}

void UIManager::SetPanelCollapsed(int index, bool collapsed)
{
    if (index < 0 || index >= kCollapsiblePanelCount) return;
    if (m_PanelCollapse[index].nodeId == 0) return;
    m_PanelCollapse[index].collapsed = collapsed;
}

bool UIManager::IsPanelCollapsed(int index) const
{
    if (index < 0 || index >= kCollapsiblePanelCount) return false;
    return m_PanelCollapse[index].collapsed;
}

void UIManager::ResetPanelCollapse()
{
    for (auto& p : m_PanelCollapse) { p.collapsed = false; p.animT = 0.0f; }
    // Mismo camino que el menu Vista > "Restablecer Entorno": reconstruye
    // el arbol de docking entero desde cero, asi los paneles vuelven a sus
    // proporciones originales en vez de quedar en el tamaño que tenian
    // justo antes del reset.
    m_ResetLayout = true;
}

void UIManager::UpdatePanelCollapseAnim()
{
    // Ancho/alto del "riel" colapsado por defecto (para paneles sin riel propio).
    const float kCollapsedPx = 40.0f;
    const float kSpeed       = 9.0f;
    const float dt           = ImGui::GetIO().DeltaTime;

    for (int idx = 0; idx < kCollapsiblePanelCount; ++idx)
    {
        auto& p = m_PanelCollapse[idx];
        if (p.nodeId == 0) continue;

        ImGuiDockNode* node = ImGui::DockBuilderGetNode(p.nodeId);
        if (!node) continue;

        // Si está en reposo expandido, mantener actualizado expandedSize si el operador redimensionó manualmente
        if (!p.collapsed && p.animT <= 0.001f)
        {
            if (p.axisIsWidth && node->Size.x > 80.0f)
                p.expandedSize.x = node->Size.x;
            else if (!p.axisIsWidth && node->Size.y > 80.0f)
                p.expandedSize.y = node->Size.y;
        }

        const float target = p.collapsed ? 1.0f : 0.0f;
        if (p.animT == target) continue; // en reposo -- no pelear con un resize manual del operador

        p.animT += (target - p.animT) * std::min(1.0f, dt * kSpeed);
        if (std::fabs(p.animT - target) < 0.004f) p.animT = target;

        // Determinar collapsedPx según el panel:
        // Si es el panel de Diseño (idx == 1), debe mantener visible su riel de botones
        // (horizontal en Clásico, vertical en Simple).
        float targetCollapsedPx = kCollapsedPx;
        if (idx == 0)
        {
            targetCollapsedPx = 0.0f;
        }
        else if (idx == 1 && !p.axisIsWidth)
        {
            targetCollapsedPx = IconRailThickness(false) + 4.0f;
        }
        else if (idx == 1 && p.axisIsWidth)
        {
            targetCollapsedPx = IconRailThickness(true) + 4.0f;
        }

        float expandedPx = p.axisIsWidth ? p.expandedSize.x : p.expandedSize.y;
        if (expandedPx <= targetCollapsedPx + 10.0f)
        {
            expandedPx = p.axisIsWidth ? 340.0f : 240.0f;
            if (p.axisIsWidth) p.expandedSize.x = expandedPx;
            else               p.expandedSize.y = expandedPx;
        }

        const float animatedPx = expandedPx + (targetCollapsedPx - expandedPx) * p.animT;

        ImVec2 size = node->Size;
        if (p.axisIsWidth) size.x = std::max(1.0f, animatedPx);
        else               size.y = std::max(1.0f, animatedPx);
        if (size.x <= 0.0f) size.x = 1.0f;
        if (size.y <= 0.0f) size.y = 1.0f;

        ImGui::DockBuilderSetNodeSize(p.nodeId, size);
    }
}

void UIManager::EndDockspace()
{
    ImGui::End();
}

void UIManager::Shutdown()
{
    // Puede bloquear un instante si una descarga de subtitulos seguia en
    // curso -- preferible a std::terminate() por destruir un std::thread
    // todavia joinable (ver RenderUrlImportModal).
    if (m_UrlImportThread.joinable())
        m_UrlImportThread.join();
    m_Panels.clear();
}

void UIManager::RenderPatchNotesModal()
{
    if (!m_ShowPatchNotesModal) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float modalW = std::clamp(vp->WorkSize.x * 0.75f, 440.0f, 820.0f);
    const float modalH = std::clamp(vp->WorkSize.y * 0.75f, 360.0f, 600.0f);

    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.12f, 0.15f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));

    if (ImGui::Begin("Notas de Versión (PatchNotes)##Modal", &m_ShowPatchNotesModal,
                     ImGuiWindowFlags_NoCollapse))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_ShowPatchNotesModal = false;
        }

        // Barra superior
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::AccentLight), "ProyecThor — Historial de Versiones");
        ImGui::SameLine();
        ImGui::TextDisabled("•  v" PROYECTHOR_VERSION_STRING);
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        const float footerAreaH = 40.0f;

        PatchNotesStyleConfig cfg;
        cfg.windowBg      = ImVec4(0.11f, 0.12f, 0.15f, 0.98f);
        cfg.border        = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);
        cfg.accent        = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);
        cfg.accentLight   = ImGui::ColorConvertU32ToFloat4(DS::AccentLight);
        cfg.textPrimary   = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
        cfg.textSecondary = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
        cfg.cardBg        = ImVec4(0.15f, 0.16f, 0.21f, 0.60f);
        cfg.cardBorder    = ImVec4(0.26f, 0.28f, 0.35f, 0.45f);
        cfg.scale         = 1.0f;

        static PatchNotesViewState s_PatchNotesState;
        RenderPatchNotesViewContent(s_PatchNotesState, cfg, footerAreaH);

        // Barra inferior
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        if (ImGui::Button("Cerrar", ImVec2(100.0f, 28.0f))) {
            m_ShowPatchNotesModal = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void UIManager::ShowProFeatureModal(const std::string& title, const std::string& description)
{
    m_ProFeatureModalTitle = title;
    m_ProFeatureModalDesc  = description;
    m_ShowProFeatureModal  = true;
}

void UIManager::RenderProFeatureModal()
{
    if (!m_ShowProFeatureModal) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float modalW = 500.0f;
    const float modalH = 260.0f;

    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.15f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.95f, 0.75f, 0.20f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));

    if (ImGui::Begin("Función ProyecThor for Professionals##Modal", &m_ShowProFeatureModal,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_ShowProFeatureModal = false;
        }

        // Título con badge dorado PRO
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.20f, 1.0f));
        ImGui::TextUnformatted(m_ProFeatureModalTitle.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
        ImGui::TextUnformatted("— Exclusivo Pro");
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.88f, 0.94f, 1.0f));
        ImGui::TextWrapped("%s", m_ProFeatureModalDesc.c_str());
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        // Botones inferiores: Ir a Cuenta (Pro) / Cerrar
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.75f, 0.22f, 1.0f));
        if (ImGui::Button("Ver Planes Pro en Cuenta", ImVec2(210.0f, 32.0f))) {
            m_ShowProFeatureModal = false;
            CloseAllToolWindows();
            RequestSettings(10); // Categoría Cuenta / Pro
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, 12.0f);
        if (DS::GlassButton("Cerrar", ImVec2(100.0f, 32.0f))) {
            m_ShowProFeatureModal = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

}
