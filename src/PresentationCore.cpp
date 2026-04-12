#include "PresentationCore.h"
#include "BackgroundLayer.h"
#include "OverlayLayer.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace ProyecThor::Core {

    class PresentationCoreImpl {
    public:
        BackgroundLayer background;
        OverlayLayer    overlay;
    };

    PresentationCore::PresentationCore()
        : m_Impl(std::make_unique<PresentationCoreImpl>()) {}

    PresentationCore::~PresentationCore() {
        DestroyProjectorWindow();
    }

    // -------------------------------------------------------------------------
    //  SELECCIÓN
    // -------------------------------------------------------------------------

    void PresentationCore::SetSelection(const LibrarySelection& selection) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CurrentSelection = selection;
    }

    // Consume la selección (la limpia tras leerla).
    // Solo debe llamarse desde MediaView, que es quien actúa sobre ella.
    LibrarySelection PresentationCore::GetSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        LibrarySelection sel = m_CurrentSelection;
        m_CurrentSelection.title = "";
        m_CurrentSelection.type  = ItemType::None;
        m_CurrentSelection.contentData.clear();
        return sel;
    }

    // Lee la selección sin consumirla.
    // Usar en paneles que solo necesitan saber qué está seleccionado.
    LibrarySelection PresentationCore::PeekSelection() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_CurrentSelection;
    }

    // -------------------------------------------------------------------------
    //  ESTADO Y TEXTURAS
    // -------------------------------------------------------------------------

    PresentationState PresentationCore::GetState() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_State;
    }

    void* PresentationCore::GetBackgroundTexture() {
        return m_Impl ? m_Impl->background.GetTextureID() : nullptr;
    }

    void* PresentationCore::GetOverlayTexture() {
        return m_Impl ? m_Impl->overlay.GetTextureID() : nullptr;
    }

    // -------------------------------------------------------------------------
    //  UPDATE Y RENDER
    // -------------------------------------------------------------------------

    void PresentationCore::Update() {
        if (m_Impl) {
            m_Impl->background.Update();
            m_Impl->overlay.Update();
        }
    }

    void PresentationCore::RenderProjectorWindow() {
        // Renderiza en la ventana de proyección secundaria si existe,
        // de lo contrario no hace nada (evita contaminar la ventana principal).
        if (!m_ProjectorWindow) return;

        GLFWwindow* previous = glfwGetCurrentContext();
        glfwMakeContextCurrent(m_ProjectorWindow);

        glViewport(0, 0, m_ProjectorWidth, m_ProjectorHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (m_Impl) {
            m_Impl->background.Render(m_ProjectorWidth, m_ProjectorHeight);
            m_Impl->overlay.Render();
        }

        glfwSwapBuffers(m_ProjectorWindow);
        glfwMakeContextCurrent(previous);
    }

    // -------------------------------------------------------------------------
    //  VENTANA DE PROYECCIÓN EN MONITOR SECUNDARIO
    // -------------------------------------------------------------------------

    void PresentationCore::CreateProjectorWindow() {
    // 1. Evitar duplicados
    if (m_ProjectorWindow) return;

    // 2. OBTENER MONITORES ACTUALIZADOS
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    if (!monitors || monitorCount == 0) {
        std::cerr << "[Projector] Error: No se detectaron monitores físicos.\n";
        return;
    }

    // 3. RECUPERAR PREFERENCIA (Asegúrate de que tu SettingsManager ya cargó el archivo)
    int targetIndex = 0;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        // Sugerencia: Si targetMonitorIndex es -1 o 0 por defecto, 
        // y tenemos más de una pantalla, forzamos la 1 (secundaria).
        if (m_State.targetMonitorIndex <= 0 && monitorCount > 1) {
            targetIndex = 1; 
        } else {
            targetIndex = m_State.targetMonitorIndex;
        }
    }

    // 4. VALIDACIÓN DE SEGURIDAD
    if (targetIndex >= monitorCount || targetIndex < 0) {
        std::cout << "[Projector] Monitor configurado (" << targetIndex 
                  << ") no disponible. Usando monitor secundario por defecto si existe.\n";
        targetIndex = (monitorCount > 1) ? 1 : 0;
    }

    GLFWmonitor* targetMonitor = monitors[targetIndex];
    const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
    if (!mode) return;

    m_ProjectorWidth  = mode->width;
    m_ProjectorHeight = mode->height;

    // 5. CONFIGURACIÓN DE HINTS (CRÍTICO)
    glfwDefaultWindowHints(); // Resetear estados previos
    glfwWindowHint(GLFW_DECORATED,      GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,       GLFW_TRUE);  // Siempre encima
    glfwWindowHint(GLFW_AUTO_ICONIFY,   GLFW_FALSE); // IMPORTANTE: No se minimiza si clickeas la UI principal
    glfwWindowHint(GLFW_FOCUS_ON_SHOW,  GLFW_FALSE); // No robar el foco a la ventana de control
    
    // Compartir contexto con la ventana principal (asumiendo que es el contexto actual)
    GLFWwindow* sharedCtx = glfwGetCurrentContext();

    // 6. CREACIÓN DE VENTANA
    m_ProjectorWindow = glfwCreateWindow(
        m_ProjectorWidth, m_ProjectorHeight, 
        "ProyecThor - Output", nullptr, sharedCtx);

    if (!m_ProjectorWindow) {
        std::cerr << "[Projector] Falló la creación de la ventana.\n";
        return;
    }

    // 7. POSICIONAMIENTO ABSOLUTO
    int mx, my;
    glfwGetMonitorPos(targetMonitor, &mx, &my);
    glfwSetWindowPos(m_ProjectorWindow, mx, my);
    
    // Algunos SO necesitan un refresco de tamaño tras mover la ventana sin bordes
    glfwSetWindowSize(m_ProjectorWindow, m_ProjectorWidth, m_ProjectorHeight);

    std::cout << "[Projector] Proyectando en: " << glfwGetMonitorName(targetMonitor) 
              << " (" << targetIndex << ") a " << m_ProjectorWidth << "x" << m_ProjectorHeight << "\n";
}

    void PresentationCore::DestroyProjectorWindow() {
        if (m_ProjectorWindow) {
            glfwDestroyWindow(m_ProjectorWindow);
            m_ProjectorWindow = nullptr;
        }
    }

    GLFWwindow* PresentationCore::GetProjectorWindow() const {
        return m_ProjectorWindow;
    }

    // -------------------------------------------------------------------------
    //  CONTROLES DE CAPAS
    // -------------------------------------------------------------------------

    void PresentationCore::SetBackgroundMedia(const std::string& path, bool isVideo) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgPath = path;
            m_State.bgType = isVideo
                ? PresentationState::BackgroundType::Video
                : PresentationState::BackgroundType::SolidColor;
        }
        if (m_Impl) {
            if (isVideo) m_Impl->background.SetVideo(path);
            else         m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
        }
    }

    void PresentationCore::StopBackgroundMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgPath    = "";
            m_State.bgType    = PresentationState::BackgroundType::SolidColor;
            m_State.bgColor[0] = 0.0f;
            m_State.bgColor[1] = 0.0f;
            m_State.bgColor[2] = 0.0f;
        }
        if (m_Impl) m_Impl->background.SetSolidColor(0.0f, 0.0f, 0.0f);
    }

    void PresentationCore::SetLayer0_Color(float r, float g, float b) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.bgColor[0] = r;
            m_State.bgColor[1] = g;
            m_State.bgColor[2] = b;
            m_State.bgType     = PresentationState::BackgroundType::SolidColor;
            m_State.bgPath     = "";
        }
        if (m_Impl) m_Impl->background.SetSolidColor(r, g, b);
    }

    void PresentationCore::SetOverlayMedia(const std::string& path) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = path;
        }
        if (m_Impl) m_Impl->overlay.PlayOverlay(path);
    }

    void PresentationCore::StopOverlayMedia() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_State.overlayPath = "";
        }
        if (m_Impl) m_Impl->overlay.StopOverlay();
    }

    void PresentationCore::UpdateTextStyle(float size, const float color[4],
                                           int align, const float margins[4],
                                           bool autoScale) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.textSize      = size;
        m_State.textAlignment = align;
        m_State.autoScale     = autoScale;
        for (int i = 0; i < 4; i++) {
            m_State.textColor[i] = color[i];
            if (margins) m_State.margins[i] = margins[i];
        }
    }

    void PresentationCore::SetLayer2_Text(const std::string& text) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = text;
        m_State.showText    = !text.empty();
    }

    void PresentationCore::ClearLayer2() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.currentText = "";
        m_State.showText    = false;
    }

    void PresentationCore::SetProjecting(bool projecting) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.isProjecting = projecting;
    }

    void PresentationCore::SetTargetMonitor(int index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_State.targetMonitorIndex = index;
    }

    void PresentationCore::SetProjectorSize(int w, int h) {
        m_ProjectorWidth  = w;
        m_ProjectorHeight = h;
    }

    VLCBasePlayer* PresentationCore::GetBackgroundPlayer() {
        return m_Impl ? m_Impl->background.GetPlayer() : nullptr;
    }

    VLCBasePlayer* PresentationCore::GetOverlayPlayer() {
        return m_Impl ? m_Impl->overlay.GetPlayer() : nullptr;
    }

} // namespace ProyecThor::Core