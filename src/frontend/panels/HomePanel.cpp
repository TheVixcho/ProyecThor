#include "HomePanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/UIStrings.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::UI {

HomePanel::HomePanel() {
    // Ya no se construye ningun VLCBasePlayer propio: el preview de
    // biblioteca usa el player real de PresentationCore (forceSilent=true,
    // ver PresentationCoreImpl::preview), que es estructuralmente mudo y
    // se re-silencia solo cada frame via PresentationCore::Update().
}

HomePanel::~HomePanel() {
    // Nada que detener aca: el ciclo de vida del preview real lo maneja
    // PresentationCore/PresentationCoreImpl.
}

void HomePanel::RenderHomeContent()
{
    auto selection = Core::PresentationCore::Get().PeekSelection();
    Core::VLCBasePlayer* previewPlayer = Core::PresentationCore::Get().GetPreviewPlayer();

    if (selection.type == Core::ItemType::Audio)
    {
        if (m_AudioPanelRef)
        {
            m_AudioPanelRef->Update();
            m_AudioPanelRef->RenderPlayerView();
        }
        else
        {
            ImGui::TextDisabled("Reproductor de audio no disponible.");
        }
    }
    else if (selection.type == Core::ItemType::Bible)
    {
        m_BibleView.Render();
    }
    else if (selection.type == Core::ItemType::Song)
    {
        m_SongView.Render();
    }
    else if (selection.type == Core::ItemType::Image)
    {
        m_MediaView.Render(previewPlayer);
    }
    else if (selection.type == Core::ItemType::Video)
    {
        m_MonitorView.Render(previewPlayer);
        ImGui::Separator();
        ImGui::Spacing();
        m_MediaView.Render(previewPlayer);
    }
    else if (selection.type == Core::ItemType::Documents)
    {
        m_DocumentView.Render(selection.title, selection.contentData);
    }
    else
    {
        // FIX: antes esto era solo un cartel de "Seleccione algo" — la cola
        // del Monitor (m_MonitorView) SOLO se dibujaba con selection.type
        // == Video, asi que en cualquier otro momento (nada seleccionado,
        // o una cancion/biblia/documento activo) la cola literalmente no
        // estaba en pantalla: ni el boton Reproducir, ni el target de
        // drag-and-drop para agregar videos, nada. Eso explicaba reportes
        // de "agrego un video y no pasa nada" / "aprieto reproducir y no
        // hace nada" — no era que la lógica fallara, es que la UI de la
        // cola no estaba ahi para interactuar. Ahora se muestra tambien
        // aca (el estado por defecto de Home, sin nada mas seleccionado),
        // asi la cola queda accesible de forma confiable sin depender de
        // tener un video puntual seleccionado.
        m_MonitorView.Render(previewPlayer);
    }
}

void HomePanel::Render()
{
    // El boton de pantalla completa del Preview (ver MonitorView::
    // RequestPreviewFullscreen) necesita UIManager para poder tomar todo el
    // area de contenido -- se re-propaga cada frame (asignacion de puntero,
    // gratis) en vez de un setter propio porque m_UIManagerRef se fija
    // directo como campo publico desde main.cpp, sin un punto unico
    // despues de construir HomePanel donde enganchar esto una sola vez.
    if (m_UIManagerRef)
        m_MonitorView.SetUIManager(m_UIManagerRef);

    // Pump incondicional: la cola del Monitor (MonitorView::Update -> avanza
    // al siguiente clip cuando VLC reporta fin real) tiene que correr
    // siempre, no solo cuando Home esta dibujando su contenido (aunque en la
    // practica Home ya no tiene otras pestañas que la tapen — esto se
    // mantiene por si el panel se llega a colapsar/ocultar).
    m_MonitorView.Update();

    // Alt Gr + 4: si Home esta colapsado (o pasando el punto medio de la
    // animacion), no dibujar la ventana ni su sidebar/contenido -- el pump
    // de arriba ya corrio, asi que la cola del Monitor sigue avanzando
    // igual que si el panel estuviera visible.
    if (m_UIManagerRef && m_UIManagerRef->IsPanelCollapsedForRender(GetName()))
        return;

    bool visible = false;
    if (m_UIManagerRef)
    {
        visible = DS::BeginGlassPanel(GetName().c_str(), m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        visible = ImGui::Begin(GetName().c_str());
        ImGui::PopStyleVar();
    }

    if (!visible)
    {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##homeContent", ImVec2(0.f, 0.f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    // El editor de estilos (Diseño > Estilos) ya no se "acopla" aca -- ahora
    // se abre a pantalla completa ocultando TODOS los paneles (ver
    // UIManager::EnterFullscreenEditor, usado desde LayersStyleTab), asi que
    // Home vuelve a dibujar siempre su contenido normal.
    RenderHomeContent();

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
