#pragma once
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::Core {

    // Ventana secundaria nativa GLFW (soporta Wayland, X11 y Windows).
    // Cada ventana secundaria posee su propio contexto OpenGL y su propio
    // ImGuiContext independiente para renderizado directo y estable.
    class SecondaryOutputWindow {
    public:
        using RenderFn = std::function<void(int width, int height)>;

        using ContextDestroyCallback = std::function<void(GLFWwindow*)>;
        static void RegisterContextDestroyCallback(ContextDestroyCallback cb);

        SecondaryOutputWindow() = default;
        ~SecondaryOutputWindow() { Destroy(); }

        SecondaryOutputWindow(const SecondaryOutputWindow&)            = delete;
        SecondaryOutputWindow& operator=(const SecondaryOutputWindow&) = delete;
        SecondaryOutputWindow(SecondaryOutputWindow&&)                 = default;
        SecondaryOutputWindow& operator=(SecondaryOutputWindow&&)      = default;

        bool Create(GLFWwindow* sharedContext, int monitorIndex, const std::string& title);
        void Destroy();

        void RenderFrame(const RenderFn& renderFn);

        bool          IsActive()         const { return m_Window != nullptr; }
        GLFWwindow*   GetWindow()        const { return m_Window; }
        int           GetMonitorIndex()  const { return m_MonitorIndex; }
        ImGuiContext* GetImGuiContext()  const { return m_ImGuiContext; }

    private:
        static std::vector<ContextDestroyCallback>& DestroyCallbacks();

        GLFWwindow*   m_Window       = nullptr;
        ImGuiContext* m_ImGuiContext = nullptr;
        int           m_MonitorIndex = -1;
    };

} // namespace ProyecThor::Core