#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include "frontend/panels/StageDisplayPanel.h"

namespace ProyecThor::UI { class OSCPanel; class BroadcastPanel; class StreamingPanel; class SyncPanel; }

namespace ProyecThor::UI::Settings {

    class SettingsPanel {
    public:
        SettingsPanel();
        void Render(bool* isOpen);
        void InitializeTheme();
        void SetInitialCategory(int idx) { m_SelectedCategory = idx; }

        // Misma instancia que UIManager::GetOSCPanel() -- subcategoria "OSC"
        // dentro de Ajustes > Conexiones (ver CategoryConnections.cpp) pero
        // UIManager sigue siendo el dueño, para poder llamarle Update()
        // incondicionalmente cada frame sin importar si Ajustes esta
        // abierto. Ver cableado en UIManager::Initialize.
        void SetOSCPanelRef(ProyecThor::UI::OSCPanel* ref) { m_OSCPanelRef = ref; }

        // Misma instancia que UIManager::GetBroadcastPanel() -- subcategoria
        // "Streaming" (RTMP: Captura/Capa/Iniciar) dentro de Ajustes >
        // Conexiones, junto a Red/Mobile/OSC.
        void SetBroadcastPanelRef(ProyecThor::UI::BroadcastPanel* ref) { m_BroadcastPanelRef = ref; }

        // Misma instancia que UIManager::GetRedPanel() -- subcategoria "Red"
        // dentro de Ajustes > Conexiones (antes vivia en el sidebar de
        // Library junto a Reloj/Render/Mobile).
        void SetStreamingPanelRef(ProyecThor::UI::StreamingPanel* ref) { m_StreamingPanelRef = ref; }

        // Misma instancia que UIManager::GetSyncPanel() -- subcategoria
        // "Mobile" dentro de Ajustes > Conexiones (control del SyncServer/
        // app movil companion).
        void SetSyncPanelRef(ProyecThor::UI::SyncPanel* ref) { m_SyncPanelRef = ref; }
    private:
        int         m_SelectedCategory  = 0;
        int         m_PrevCategory      = -1;   // para detectar cambio de categoría
        float       m_SaveTimer         = 0.0f;
        std::string m_SaveStatusMsg     = "";
        float       m_CheckingAnim      = 0.0f; // ángulo del spinner manual

        bool  m_WasOpenLastFrame = false; // para detectar la transición cerrado -> abierto (sin animar la apertura)

        // Buscador del sidebar (ver RenderSidebar) -- filtra k_Categories por
        // etiqueta/descripción, sin agrupar por tema mientras hay texto.
        char m_SearchBuffer[64] = "";

        // ── Subcategorías (cada una es su propia "página", no un ancla de
        // scroll) ─────────────────────────────────────────────────────────
        // Cada llamada a SectionTitle() durante el render de la categoría
        // activa se registra aquí (su nombre de grupo, en orden de
        // aparición) y SOLO dibuja su cuerpo si es la subcategoría
        // seleccionada -- las demás no dibujan nada ese frame (ver
        // SectionTitle). El sidebar, para la categoría seleccionada,
        // muestra esta lista como subcategorías clickeables; clickear una
        // cambia m_SelectedSubsection, reemplazando por completo lo que se
        // ve en el área de contenido (pedido explícito: "dar más atención
        // una por una" en vez de un scroll largo con todo junto). Se
        // recalcula cada frame en RenderContent(), así que no hace falta
        // declarar nada a mano por categoría.
        std::vector<std::string> m_SectionAnchors;
        std::string m_SelectedSubsection; // subcategoría actualmente visible (vacío = todavía sin definir, ver SectionTitle)
        std::string m_PrevSubsection;     // para detectar cambio y resetear el scroll a 0

        // La fuente de la interfaz solo se aplica reiniciando (ver
        // CategoryTheme.cpp): al elegir una nueva se dispara este modal de
        // confirmación en vez de aplicarla en caliente.
        bool m_ShowFontRestartPrompt = false;

        void RenderSidebar();
        void RenderContent();
        void RenderSaveBar();

        void RenderCategoryTheme();
        void RenderCategoryProjection();
        void RenderCategoryConnections();
        void RenderCategoryStage();
        void RenderCategoryAudio();
        void RenderCategoryLanguage();
        void RenderCategoryUpdates();
        void RenderCategoryShortcuts();
        void RenderCategorySongs();
        void RenderCategoryData();

        ProyecThor::UI::OSCPanel*       m_OSCPanelRef       = nullptr;
        ProyecThor::UI::BroadcastPanel* m_BroadcastPanelRef = nullptr;
        ProyecThor::UI::StreamingPanel* m_StreamingPanelRef = nullptr;
        ProyecThor::UI::SyncPanel*      m_SyncPanelRef      = nullptr;

        // Antes vivia dentro del hub "Control" (ver ControlPanel, eliminado);
        // ahora es directamente el contenido de la categoria Stage de Ajustes.
        ProyecThor::UI::StageDisplayPanel m_StageDisplay;

        // Helpers
        // navGroup: agrupa varios SectionTitle bajo UNA sola entrada de
        // subcategoría en el sidebar (el primero con ese grupo decide si el
        // grupo entero se ve o no) -- por defecto (nullptr) cada título es
        // su propia subcategoría. Ver uso agrupado en CategoryTheme.cpp
        // (Temas/Colores/Fuentes/Diseño) y CategoryProjection.cpp (los 3
        // bloques de Streaming). Devuelve true si esta sección es la
        // seleccionada actualmente (el caller debe envolver su contenido en
        // "if (SectionTitle(...)) { ... }" -- si devuelve false no dibuja
        // nada, ni siquiera el título, para no filtrar nada de otra página).
        bool SectionTitle(const char* label, const char* navGroup = nullptr);
        void HelpTooltip(const char* desc);
        void AnimatedProgressBar(float fraction, ImVec2 size, ImVec4 col);
        void SpinnerWidget(float radius, float thickness, const ImVec4& color);
    };

} // namespace ProyecThor::UI::Settings