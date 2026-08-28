#include <GL/glew.h>
#include "UIManager.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/TextEffectsRenderer.h"
#include "backend/core/PerformanceGovernor.h"
#include "../toolbar/ConfigPanel.h"
#include "panels/HomePanel.h"
#include "panels/LibraryPanel.h"
#include "panels/StylesHubPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
#include "../external/tools/OpenURL.h"
#include "UIStrings.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/OClock.h"
#include "frontend/panels/overlay/OverlayLayerRender.h"
#include "frontend/views/Audio.h"
#include "Hub.h"
#include "frontend/panels/StreamingPanel.h"
#include "qrcodegen.hpp"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/ProjectionQualityPresets.h"
#include "AppIcons.h"
#include "DesignSystem.h"
#include "IconRail.h"
#include "MonitorTheme.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/panels/home/HomeIcons.h"
#include "biblio/LibraryIcons.h"
#include "LiveContentRenderer.h"
#include "LibrarySongs.h"
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

    ApplyProfessionalTheme();
    m_SettingsPanel.InitializeTheme();
int fbWidth, fbHeight;
glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
m_GlassRenderer.Initialize(fbWidth, fbHeight);
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

    s.WindowPadding          = ImVec2(22.0f, 18.0f);
    s.FramePadding           = ImVec2(14.0f,  9.0f);
    s.ItemSpacing            = ImVec2(12.0f,  8.0f);
    s.ItemInnerSpacing       = ImVec2( 8.0f,  6.0f);
    s.CellPadding            = ImVec2(10.0f,  7.0f);
    s.TouchExtraPadding      = ImVec2( 0.0f,  0.0f);
    s.IndentSpacing          = 18.0f;
    s.ScrollbarSize          =  6.0f;
    s.GrabMinSize            =  8.0f;

    s.WindowRounding         =  8.0f;
    s.ChildRounding          =  6.0f;
    s.FrameRounding          =  6.0f;
    s.PopupRounding          =  6.0f;
    s.ScrollbarRounding      = 10.0f;
    s.GrabRounding           =  6.0f;
    s.TabRounding            =  6.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize       = 1.0f;
    s.ChildBorderSize        = 1.0f;
    s.PopupBorderSize        = 1.0f;
    s.FrameBorderSize        = 0.0f;
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
    m_Panels.push_back(std::move(panel));
}

void UIManager::OpenHub()
{
    m_Hub.ForceOpen();
    m_Mode = WorkspaceMode::Hub;
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

void UIManager::RequestSettings()
{
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

                // Primario -- IDENTICO a como funcionaba antes de agregar
                // soporte multi-monitor.
                RenderProjectorOutput("ProjectorLive", mx, my, mode, state,
                                       /*isPrimary=*/true, nullptr);

                // Monitores de salida publica ADICIONALES (opcional) --
                // todos muestran exactamente lo mismo que el primario de
                // arriba (ver PresentationState::extraTargetMonitors,
                // poblado en PresentationCore::SetTargetMonitor).
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

    // Libera instancias de post-FX de monitores extra que ya no esten
    // activas este frame (destildadas, o proyeccion detenida del todo).
    Core::PresentationCore::Get().PruneExtraProjectorViewports(activeExtraProjectorIds);

    if (state.isStaging)
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

                RenderStageOutput("StageLive", smx, smy, stageMode);

                // Monitores de Stage ADICIONALES (opcional) -- mismo
                // criterio que el bloque de Proyector de arriba.
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
                    auto* player = Core::PresentationCore::Get().GetBackgroundPlayer();
                    if (player) player->GetVideoSize(srcW, srcH);

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

                        drawList->AddImage(texID,
                            ImVec2(destX, destY),
                            ImVec2(destX + destW, destY + destH),
                            ImVec2(0, 0), ImVec2(1, 1));
                    } else {
                        drawList->AddRectFilled(
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
                            IM_COL32(0, 0, 0, 255));
                    }

                    auto& core = Core::PresentationCore::Get();
                    if (core.IsBackgroundSwapPending() && core.IsBackgroundStandbyReady())
                    {
                        void* standbyTex = core.GetStandbyBackgroundTexture();
                        if (standbyTex)
                        {
                            float progress = std::clamp(core.GetBackgroundBlendProgress(), 0.0f, 1.0f);
                            ImU32 tint = IM_COL32(255, 255, 255, (int)(progress * 255.0f));
                            drawList->AddImage(standbyTex,
                                ImVec2(destX, destY),
                                ImVec2(destX + destW, destY + destH),
                                ImVec2(0, 0), ImVec2(1, 1), tint);
                        }
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
                    auto DrawTextBlock = [&](const std::string& text, const Core::TextBoxStyle& box,
                                            bool isLyricsBox,
                                            float offsetX, float offsetY,
                                            float alphaMult = 1.0f, float scaleMult = 1.0f)
                    {
                        if (text.empty() || alphaMult <= 0.001f) return;

                        auto& core = Core::PresentationCore::Get();
                        // El diseno de una caja (Letras) se dibuja siempre igual sin
                        // importar el tipo de contenido; "isSong" acá solo decide la
                        // FORMA del texto (canciones traen saltos de linea manuales,
                        // el cuerpo de un versiculo es un parrafo sin cortar y necesita
                        // wrap normal), no que caja/estilo usar.
                        bool isSong = (core.PeekSelection().type == Core::ItemType::Song);

                        float screenScale = (float)mode->width / 1920.0f;
                        float boxW = std::max(10.0f, box.sizeW * (float)mode->width);
                        float boxH = std::max(10.0f, box.sizeH * (float)mode->height);

                        float shiftX = offsetX * (float)mode->width;
                        float shiftY = offsetY * (float)mode->height;
                        float boxX   = (float)mx + box.posX * (float)mode->width  - boxW * 0.5f + shiftX;
                        float boxY   = (float)my + box.posY * (float)mode->height - boxH * 0.5f + shiftY;

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
                            ImVec2((float)mx, (float)my),
                            ImVec2((float)(mx + mode->width), (float)(my + mode->height)),
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

                    bool transActive = m_TransitionPanel && m_TransitionPanel->IsActive();

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

                    // Indice de referencia biblica -- OPCIONAL, caja aparte
                    // e independiente de Letras (ver TextBoxStyle::indexBox
                    // y BibleView::ProjectVerse/SetCurrentRef).
                    if (state.indexEnabled && !state.currentRef.empty()) {
                        DrawTextBlock(state.currentRef, state.indexBox, false, 0.0f, 0.0f, 1.0f, 1.0f);
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
        // cualquier campo de texto (io.WantTextInput) -- sin esto, tipear
        // una "Z" mayuscula en CUALQUIER lado de la app (incluida la propia
        // ventana de Notas) cerraria/abriria el panel a mitad de escritura.
        if (!io.WantTextInput && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            ToggleNotesWindow();

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
    }

    m_Red.Update();
    m_Chat.Update();
    m_Broadcast.Update();
    m_Sync.Update();
    m_OSC.Update();

    // La toolbar de modos (pills "Hub"/"Proyector" + Notas/Estilos/
    // Streaming) no se muestra en el Hub a proposito -- el Hub ya tiene su
    // propia forma de navegar (tarjetas centrales), esta barra solo tiene
    // sentido una vez en el workspace real. Tampoco se muestra si el editor
    // a pantalla completa activo pidio ocultarla (ver EnterFullscreenEditor/
    // m_FullscreenEditorHidesToolbar, usado por el Preview de Biblioteca
    // para ocupar de verdad TODA la pantalla).
    if (m_Mode != WorkspaceMode::Hub &&
        !(m_FullscreenEditorActive && m_FullscreenEditorHidesToolbar))
        RenderModeToolbar();

    // Salida real ("ProjectorLive"/"StageLive") -- SIEMPRE se renderiza aca,
    // antes de cualquier return anticipado de abajo (editor a pantalla
    // completa o Hub), para que la transmision al publico nunca se
    // interrumpa solo porque el operador esta mirando otra cosa en su
    // propia pantalla. Ver comentario en UIManager.h.
    RenderLiveOutputWindows();

    // Se renderiza siempre, sin importar el modo/return anticipado de mas
    // abajo, para que "Archivo > Importar > Importar desde URL" funcione
    // igual desde el Hub que desde el Proyector.
    RenderUrlImportModal();

    // Idem Notas: antes solo vivia dentro del workspace de Proyector, asi
    // que Shift+Z no hacia nada desde el Hub y la ventana se cerraba de
    // golpe (sin guardar) apenas se volvia a el mientras se seguia
    // proyectando. Ahora se somete siempre, sin importar el modo/editor a
    // pantalla completa activo, igual que la salida real de arriba.
    if (m_ShowNotes)
        RenderNotesWindow();

    // Se somete siempre (no solo cuando m_ShowAIAssistant es true): el
    // WebView2 embebido necesita que se le avise UpdateBounds(...,
    // visible=false) todos los frames mientras esta oculto, si no la ventana
    // nativa hija se queda flotando encima de lo que sea que este debajo.
    RenderAIAssistantWindow();

    // Editor a pantalla completa (Overlay/Estilos) activo -- ver
    // EnterFullscreenEditor. Reemplaza TODO lo de abajo (Hub/Proyector/
    // Ajustes/etc) por el contenido del editor, sin tocar la toolbar de
    // arriba (esa nunca se oculta, ver comentario en el header) ni la
    // salida real de arriba.
    if (m_FullscreenEditorActive && m_FullscreenEditorRenderFn)
    {
        m_FullscreenEditorRenderFn();
        return;
    }

if (m_Mode == WorkspaceMode::Hub)
    {
        if (m_Hub.Render())
        {
            if (!m_Hub.SettingsRequested())
            {
                m_Mode        = WorkspaceMode::Projector;
                m_ResetLayout = true;
            }
        }

        if (m_Hub.SettingsRequested())
        {
            m_ShowConfig = true;
            m_SettingsPanel.SetInitialCategory(m_Hub.GetActiveTab());
            m_Hub.ClearSettingsRequest();
        }

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

        RenderMainMenuBar();
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);

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
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.180f, 0.185f, 0.230f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.240f, 0.200f, 0.085f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.415f, 0.180f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.650f, 0.530f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.920f, 0.820f, 0.560f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
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

    RenderMainMenuBar();
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
    // SIEMPRE visible -- pedido explicito, no ocultable (ni por Ajustes ni
    // por el menu Vista): es el punto principal para saltar entre Hub y
    // Proyector y para el acceso rapido a Notas/Estilos/Streaming, asi que
    // no puede depender de una preferencia que la deje escondida.
    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;

    // Grupo izquierdo (Hub/Proyector) separado del resto por una linea
    // vertical -- Conexiones/Biblia ya no viven aca (ver comentario de
    // WorkspaceMode en UIManager.h): a la derecha de la linea solo quedan
    // Notas y Estilos, que no son WorkspaceMode (no reemplazan el contenido
    // de abajo, abren su propia ventana/popup encima).
    static const IconRailItem kItemsLeft[] = {
        { (int)WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { (int)WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
    };

    ImVec4 accent = ImGui::ColorConvertU32ToFloat4(DS::AccentColor);

    ImGuiViewport* vp     = ImGui::GetMainViewport();
    float          railH  = IconRailThickness(false);

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

    {
        ImDrawList*   dl      = ImGui::GetWindowDrawList();
        bool          showLbl = general.showRailLabels;
        const float   padY    = 4.0f;
        const float   padX    = 10.0f;
        const float   gap     = 6.0f;
        const float   iconGap = 3.0f;
        const float   btnH    = railH - padY * 2.0f;
        const float   rounding= 10.0f;

        ImFont* font          = ImGui::GetFont();
        const float labelSz   = std::max(9.0f, std::floor(ImGui::GetFontSize() * 0.72f));
        // *0.85: iconos un poco mas chicos que el maximo que entraria en
        // btnH -- pedido explicito, ahora que la barra queda prendida por
        // defecto se queria mas discreta.
        const float iconSz    = std::max(11.0f, (btnH - (showLbl ? (labelSz + iconGap) : 0.0f) - 2.0f) * 0.85f);

        ImGuiStorage* storage = ImGui::GetStateStorage();

        ImGui::SetCursorPos(ImVec2(10.0f, padY));

        // Pastillas de icono+etiqueta APILADOS (icono arriba, texto abajo) --
        // pedido explicito en vez del layout lado a lado de antes, mismo
        // idioma visual que una bottom-tab-bar. drawLabel/measureLabel usan
        // labelSz (mas chico que el font por defecto) para que el texto entre
        // completo debajo del icono sin agrandar la barra.
        auto measureLabelW = [&](const char* text) {
            return showLbl ? font->CalcTextSizeA(labelSz, FLT_MAX, 0.0f, text).x : 0.0f;
        };
        auto drawLabelCentered = [&](const char* text, float btnW, ImVec2 bMin, float labelY, ImU32 col) {
            if (!showLbl) return;
            float w = font->CalcTextSizeA(labelSz, FLT_MAX, 0.0f, text).x;
            float x = bMin.x + (btnW - w) * 0.5f;
            dl->AddText(font, labelSz, { x, labelY }, col, text);
        };

        // Una sola pastilla icono+etiqueta apilados -- factorizado para que
        // los grupos Hub/Proyector, Conexiones, Notas y Biblia (cada uno con
        // su propia fuente de "activo") compartan el mismo dibujo en vez de
        // triplicar/cuadruplicar el mismo bloque de ~30 lineas.
        auto RenderPill = [&](const char* label, DrawIconFn drawIcon, bool active, bool sameLine, float sameLineSpacing) -> bool {
            float lblW     = measureLabelW(label);
            float contentW = std::max(iconSz, lblW);
            float btnW     = contentW + padX * 2.0f;

            if (sameLine) ImGui::SameLine(0.0f, sameLineSpacing);

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImVec2 bMin   = cursor;
            ImVec2 bMax   = { cursor.x + btnW, cursor.y + btnH };

            ImGuiID hovId = ImGui::GetID(label);
            float*  pT    = storage->GetFloatRef(hovId ^ 0x51A17E5u, 0.0f);
            bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
            *pT += ((hovered ? 1.0f : 0.0f) - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
            float t = *pT;

            if (active) {
                dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(accent), rounding);
            } else if (t > 0.01f) {
                dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 18.0f)), rounding);
            }

            ImGui::SetCursorScreenPos(bMin);
            const std::string btnId = std::string("##modeTb_") + label;
            bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, btnH });

            ImU32 icCol;
            if (active) {
                icCol = IM_COL32(18, 18, 20, 255);
            } else {
                ImVec4 base  = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
                ImVec4 hover = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
                base.x += (hover.x - base.x) * t;
                base.y += (hover.y - base.y) * t;
                base.z += (hover.z - base.z) * t;
                icCol = ImGui::ColorConvertFloat4ToU32(base);
            }

            float iconX = bMin.x + (btnW - iconSz) * 0.5f;
            float iconY = bMin.y + 1.0f;
            drawIcon(dl, { iconX, iconY }, iconSz, icCol);
            drawLabelCentered(label, btnW, bMin, iconY + iconSz + iconGap, icCol);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", label);

            return clicked;
        };

        auto RenderModeItem = [&](const IconRailItem& item, bool sameLine, float sameLineSpacing) {
            bool active  = ((int)m_Mode == item.index);
            bool clicked = RenderPill(item.label, item.drawIcon, active, sameLine, sameLineSpacing);
            if (clicked && !active)
            {
                m_Mode = (WorkspaceMode)item.index;
                if (m_Mode == WorkspaceMode::Hub)       m_Hub.ForceOpen();
                if (m_Mode == WorkspaceMode::Projector) m_ResetLayout = true;
            }
        };

        // ── Grupo izquierdo: Hub / Proyector ─────────────────────────────
        for (int i = 0; i < (int)(sizeof(kItemsLeft) / sizeof(kItemsLeft[0])); i++)
            RenderModeItem(kItemsLeft[i], i > 0, gap);

        // ── Linea separadora ──────────────────────────────────────────────
        {
            ImGui::SameLine(0.0f, gap * 2.0f);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float  sepH = btnH * 0.7f;
            dl->AddRectFilled({ p.x, p.y + (btnH - sepH) * 0.5f },
                              { p.x + 1.0f, p.y + (btnH - sepH) * 0.5f + sepH },
                              IM_COL32(255, 255, 255, 30));
            ImGui::Dummy(ImVec2(1.0f, btnH));
        }

        // ── Grupo derecho: Notas y Estilos -- ninguno de los dos es un
        //    WorkspaceMode (no reemplazan el contenido de abajo): Notas
        //    abre/cierra una ventana flotante (ver RenderNotesWindow) y
        //    Estilos abre un popup para aplicar un estilo guardado sin ir
        //    hasta Diseño > Estilos.
        {
            bool clicked = RenderPill("Notas", HomeIcons::DrawIcon_Notepad, m_ShowNotes, true, gap * 2.0f);
            if (clicked) ToggleNotesWindow();
        }
        {
            bool clicked = RenderPill("Asistente IA", HomeIcons::DrawIcon_Sparkle, m_ShowAIAssistant, true, gap);
            if (clicked) ToggleAIAssistant();
        }
        {
            bool clicked = RenderPill("Estilos", AppIcons::DrawIcon_Layers, false, true, gap);
            if (clicked) ImGui::OpenPopup("##modeTbStylesPopup");
        }
        {
            // Abre Ajustes directo en "Proyección" (indice 1 de k_Categories,
            // ver SettingsPanel.cpp) -- Streaming (RTMP) vive ahi como
            // subcategoria, junto a Red/Mobile/OSC (ver CategoryProjection.cpp).
            bool clicked = RenderPill("Streaming", HomeIcons::DrawIcon_Broadcast, false, true, gap);
            if (clicked) {
                m_ShowConfig = true;
                m_SettingsPanel.SetInitialCategory(1);
            }
        }
        RenderStylesPopup();
    }

    RenderModeToolbarStatusActions(ImGui::GetWindowWidth(), railH);

    ImGui::End();

    vp->WorkPos.y  += railH;
    vp->WorkSize.y -= railH;
}

void UIManager::RenderModeToolbarStatusActions(float winW, float railH)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    const bool  audienceOn = core.IsProjecting();
    const bool  stageOn    = sd.useLAN ? core.IsStreamingNet() : core.IsStaging();
    const float rowH       = std::min(28.0f, railH - 4.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const char* clearLabel = "Borrar Todo";
    ImVec2      clearTxtSz = ImGui::CalcTextSize(clearLabel);
    const float clearIconSz  = rowH * 0.55f;
    const float clearIconGap = 8.0f;
    float       clearGroupW  = clearIconSz + clearIconGap + clearTxtSz.x;
    float       clearBtnW    = clearGroupW + 24.0f;

    ImVec2 dotSzAudience = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Público").x + 14.0f, rowH);
    ImVec2 dotSzStage    = ImVec2(5.0f * 2.0f + 6.0f + ImGui::CalcTextSize("Stage").x    + 14.0f, rowH);

    const float gap   = 14.0f;
    float       totalW = dotSzAudience.x + gap + dotSzStage.x + gap + clearBtnW;
    float       startX = std::max(10.0f, winW - totalW - 12.0f);

    ImGui::SetCursorPos(ImVec2(startX, (railH - rowH) * 0.5f));

    if (StatusDotToggle(dl, "##modeTbDotAudience", "Público", audienceOn, MT::k_LiveAccent, rowH))
        ToggleAudience(!audienceOn);

    ImGui::SameLine(0.0f, gap);

    if (StatusDotToggle(dl, "##modeTbDotStage", "Stage", stageOn, MT::k_PrevAccent, rowH))
        ToggleStageQuick(!stageOn);

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
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rowH * 0.5f);
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
            settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
            0, std::max(0, monitorCount - 1));

        core.SetTargetMonitor(monitorIndex);
        std::cout << "[UIManager] Proyección iniciada en monitor " << monitorIndex << ".\n";
    } else {
        std::cout << "[UIManager] Proyección detenida.\n";
    }

    core.SetProjecting(active);
}

void UIManager::ToggleStageQuick(bool active)
{
    auto& core = Core::PresentationCore::Get();
    auto& sd   = ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    if (active) {
        if (sd.useLAN) {
            core.ToggleNetworkStream(true, sd.lanPort);
            return;
        }

        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        if (monitorCount < 2) {
            std::cerr << "[UIManager] No hay suficientes monitores para activar el stage.\n";
            return;
        }

        int stageMonitorIndex = std::clamp(sd.monitorIndex < 0 ? 1 : sd.monitorIndex, 0, monitorCount - 1);
        sd.monitorIndex = stageMonitorIndex;
        ProyecThor::Settings::SettingsManager::Get().Save();
        core.SetStaging(true, stageMonitorIndex);
    } else {
        if (sd.useLAN) core.ToggleNetworkStream(false);
        else           core.SetStaging(false);
    }
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
        // Se cerro este frame (boton X nativo, no ToggleNotesWindow) -- ver
        // comentario en QuickNotes.h: nunca dejar la ventana cerrarse sin
        // guardar lo ultimo tipeado.
        m_NotesPanel.PersistNow();
        s_WasOpenLastFrame = false;
    }
}

void UIManager::RenderAIAssistantWindow()
{
    m_AIAssistant.Render(&m_ShowAIAssistant, m_GlassRenderer);
}

void UIManager::RenderUrlImportModal()
{
    // Se consume el resultado (y se une el hilo) apenas esta listo, SIEMPRE
    // -- incluso si el operador ya cerro la ventana mientras corria en
    // segundo plano. Sin esto, un intento nuevo mas tarde pisaria con "="
    // un std::thread todavia no unido y std::terminate() explota.
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
            // Si el operador ya cerro/cancelo mientras se descargaba, no se
            // crea la cancion igual a sus espaldas -- se descarta el
            // resultado en silencio.
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
            if (m_UrlImportThread.joinable()) m_UrlImportThread.join(); // por si quedo un intento anterior sin unir
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
    // Sin color de fondo propio -- hereda ImGuiCol_PopupBg del tema activo
    // (ver SettingsManager::ApplyTheme), como cualquier otro popup sin
    // override. Antes tenia un ImVec4 fijo aca que lo tapaba y quedaba
    // desentonado con el tema elegido en Ajustes > Apariencia.
    ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
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
        { WorkspaceMode::Hub,        HomeIcons::DrawIcon_Home,      "Hub"        },
        { WorkspaceMode::Projector,  AppIcons::DrawIcon_Monitor,    "Proyector"  },
    };
    constexpr int kCount = 2;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        m_QuickSwitchOpen  = !m_QuickSwitchOpen;
        for (int i = 0; i < kCount; i++)
            if (kItems[i].mode == m_Mode) m_QuickSwitchIndex = i;
    }
    if (!m_QuickSwitchOpen) return;

    auto Activate = [&](int idx) {
        m_Mode = kItems[idx].mode;
        if (m_Mode == WorkspaceMode::Hub)       m_Hub.ForceOpen();
        if (m_Mode == WorkspaceMode::Projector) m_ResetLayout = true;
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

    // Sin colores propios -- hereda WindowBg/Border del tema activo (ver
    // SettingsManager::ApplyTheme), antes fijos y desentonados con el tema
    // elegido en Ajustes > Apariencia.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
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
    RenderQuickSwitcher();

    const auto& str = ProyecThor::UI::GetUIStrings();

    // Sin MenuBarBg/Text propios -- heredan del tema activo (ver
    // SettingsManager::ApplyTheme), antes fijos y ademas ignorando el
    // color realmente elegido en Ajustes > Apariencia.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(14.0f, 10.0f));

    if (ImGui::BeginMainMenuBar())
    {

        if (ImGui::BeginMenu("ProyecThor"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem(str.menuPrefs, "Ctrl+P"))
                m_ShowConfig = true;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.45f, 0.45f, 1.0f));
            if (ImGui::MenuItem(str.menuExit, "Alt+F4"))
                glfwSetWindowShouldClose(m_Window, true);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        // Acceso rapido a Ajustes > Apariencia > Entorno de trabajo (ver
        // CategoryTheme.cpp para el selector completo con diagramas) -- para
        // cambiar de disposicion sin salir a Ajustes. Mismo mecanismo:
        // escribe workspace.layoutPreset y guarda; UIManager::BeginDockspace
        // detecta el cambio solo y reconstruye el layout.
        if (ImGui::BeginMenu("Espacio de trabajo"))
        {
            ImGui::Spacing();

            auto& workspace = ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace;

            struct WsEntry { const char* label; ProyecThor::Settings::WorkspaceLayoutPreset preset; };
            static const WsEntry kWorkspaceEntries[] = {
                { "Clásico",     ProyecThor::Settings::WorkspaceLayoutPreset::Classic   },
                { "Simple",      ProyecThor::Settings::WorkspaceLayoutPreset::Simple    },
                { "Biblioteca",  ProyecThor::Settings::WorkspaceLayoutPreset::Library   },
                { "Producción",  ProyecThor::Settings::WorkspaceLayoutPreset::Video     },
            };
            for (const auto& e : kWorkspaceEntries)
            {
                bool active = (workspace.layoutPreset == e.preset);
                if (ImGui::MenuItem(e.label, nullptr, active))
                {
                    workspace.layoutPreset = e.preset;
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
            }

            // Ex-menu "Vista" (retirado, absorbido aca).
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::MenuItem(str.menuResetLayout))
                m_ResetLayout = true;

            if (ImGui::MenuItem("Hub de inicio"))
                OpenHub();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            {
                auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
                if (ImGui::MenuItem("Titulos en barras de iconos", nullptr, general.showRailLabels))
                {
                    general.showRailLabels = !general.showRailLabels;
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }

                if (ImGui::MenuItem("Rendimiento", nullptr, general.showPerfPanel))
                {
                    general.showPerfPanel = !general.showPerfPanel;
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }

                if (ImGui::MenuItem("Botones de limpieza (Vista en Vivo)", nullptr, general.showViewQuickActions))
                {
                    general.showViewQuickActions = !general.showViewQuickActions;
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
            }

            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuFile))
        {
            ImGui::Spacing();

            if (ImGui::BeginMenu(str.importLabel))
            {
                if (ImGui::MenuItem("Importar canción desde portapapeles"))
                {
                    const char* clip = ImGui::GetClipboardText();
                    if (clip && clip[0] != '\0')
                        ProyecThor::Library::CreateNewSongFromClipboard(clip);
                }
                if (ImGui::MenuItem("Importar desde URL"))
                {
                    m_ShowUrlImport = true;
                    m_UrlImportLastError.clear();
                }
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(str.menuHelp))
        {
            ImGui::Spacing();

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

            // App movil de control remoto (Android, ver SyncPanel/SyncServer)
            // -- mismo criterio que los canales de arriba: submenu con QR
            // para escanear con el celular en vez de tipear la URL a mano.
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.290f, 0.780f, 0.490f, 1.0f));
            bool mobileAppOpen = ImGui::BeginMenu("App movil (control remoto)");
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Pantallas"))
        {
            // Salto directo a Ajustes > Pantallas (indice 3 de k_Categories,
            // ver SettingsPanel.cpp -- categoria de Stage, renombrada a
            // "Pantallas"): que monitor/LAN usa, layout de celdas, etc. son
            // varios ajustes relacionados entre si (a diferencia de un
            // toggle simple), asi que abre esa seccion en vez de intentar
            // duplicarlos sueltos en un menu.
            ImGui::Spacing();
            if (ImGui::MenuItem("Configuración de Stage"))
            {
                m_ShowConfig = true;
                m_SettingsPanel.SetInitialCategory(2);
            }
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ventana"))
        {
            ImGui::Spacing();
            bool isFullscreen = (glfwGetWindowMonitor(m_Window) != nullptr);
            if (ImGui::MenuItem("Pantalla completa", "F11", isFullscreen))
                ToggleFullscreen();
            ImGui::Spacing();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleVar(3);
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
// Biblioteca a la izquierda; a la derecha Vista en Vivo; en el centro, Home
// arriba y Diseño abajo.
void UIManager::BuildWorkspaceLayoutClassic(ImGuiID dockspace_id)
{
    const auto& str = ProyecThor::UI::GetUIStrings();
    ImGuiID     dock_main = dockspace_id;

    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, nullptr, &dock_main);
    ImGuiID dock_left_top, dock_left_bottom;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.40f, &dock_left_bottom, &dock_left_top);

    ImGuiID dock_right;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.37f, &dock_right, &dock_main);

    ImGuiID dock_main_top, dock_main_bottom;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.30f, &dock_main_bottom, &dock_main_top);

    ImGui::DockBuilderDockWindow(str.library,     dock_left_top);
    ImGui::DockBuilderDockWindow("Home",          dock_main_top);
    ImGui::DockBuilderDockWindow("Vista en Vivo", dock_right);
    ImGui::DockBuilderDockWindow("Diseño",        dock_main_bottom);

    ImGuiID leafNodes[] = { dock_left_top, dock_left_bottom, dock_main_top, dock_main_bottom, dock_right };
    for (ImGuiID nodeId : leafNodes)
        if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(nodeId))
            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGui::DockBuilderFinish(dockspace_id);

    // Orden fijo (1=Biblioteca, 2=Diseño, 3=Vista en Vivo, 4=Home), pedido
    // explicito del operador -- ver Alt Gr + 1..4 en RenderAll. Se apunta al
    // nodo CONTENEDOR del split (dock_left/dock_right/dock_main_top/
    // dock_main_bottom), no a dock_left_top -- ese es el que controla el
    // ancho/alto real hacia el resto del layout; dock_left_top solo reparte
    // ESE espacio ya fijo verticalmente contra dock_left_bottom.
    m_PanelCollapse[0] = { dock_left,        ImVec2(0, 0), true,  false, 0.0f }; // 1: Biblioteca
    m_PanelCollapse[1] = { dock_main_bottom, ImVec2(0, 0), false, false, 0.0f }; // 2: Diseño
    m_PanelCollapse[2] = { dock_right,       ImVec2(0, 0), true,  false, 0.0f }; // 3: Vista en Vivo
    m_PanelCollapse[3] = { dock_main_top,    ImVec2(0, 0), false, false, 0.0f }; // 4: Home
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
    // dock_main (lo que sobra) es Home, la columna medio-izquierda;
    // dock_mid_right es Vista en Vivo, la columna medio-derecha -- ahora la
    // mas ancha de las dos columnas del medio (~32% del total vs ~30% de
    // Home), en vez de quedar mas chica que Home.

    ImGui::DockBuilderDockWindow(str.library,     dock_left_top);
    ImGui::DockBuilderDockWindow("Home",          dock_main);
    ImGui::DockBuilderDockWindow("Vista en Vivo", dock_mid_right);
    ImGui::DockBuilderDockWindow("Diseño",        dock_right);

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
    m_PanelCollapse[index].collapsed = !m_PanelCollapse[index].collapsed;
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
    // Ancho/alto del "riel" colapsado -- lo bastante angosto para leerse
    // como "oculto" sin llegar a 0 (DockBuilderSetNodeSize exige > 0, y un
    // nodo de dock a 0px se pone inestable).
    const float kCollapsedPx = 40.0f;
    const float kSpeed       = 9.0f;
    const float dt           = ImGui::GetIO().DeltaTime;

    for (auto& p : m_PanelCollapse)
    {
        if (p.nodeId == 0) continue;

        const float target = p.collapsed ? 1.0f : 0.0f;
        if (p.animT == target) continue; // en reposo -- no pelear con un resize manual del operador

        p.animT += (target - p.animT) * std::min(1.0f, dt * kSpeed);
        if (std::fabs(p.animT - target) < 0.004f) p.animT = target;

        ImGuiDockNode* node = ImGui::DockBuilderGetNode(p.nodeId);
        if (!node) continue;

        const float expandedPx = p.axisIsWidth ? p.expandedSize.x : p.expandedSize.y;
        const float animatedPx = expandedPx + (kCollapsedPx - expandedPx) * p.animT;

        ImVec2 size = node->Size;
        if (p.axisIsWidth) size.x = std::max(1.0f, animatedPx);
        else                size.y = std::max(1.0f, animatedPx);
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

}
