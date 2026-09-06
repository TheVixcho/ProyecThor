#include "SecondaryOutputWindow.h"
#include <GL/glew.h>
#include <iostream>

namespace ProyecThor::Core {

    std::vector<SecondaryOutputWindow::ContextDestroyCallback>&
    SecondaryOutputWindow::DestroyCallbacks()
    {
        static std::vector<ContextDestroyCallback> callbacks;
        return callbacks;
    }

    void SecondaryOutputWindow::RegisterContextDestroyCallback(ContextDestroyCallback cb)
    {
        DestroyCallbacks().push_back(std::move(cb));
    }

    bool SecondaryOutputWindow::Create(GLFWwindow* sharedContext, int monitorIndex, const std::string& title)
    {
        Destroy();

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (monitorIndex < 0 || monitorIndex >= monitorCount) {
            std::cerr << "[SecondaryOutputWindow] Indice de monitor invalido: " << monitorIndex << "\n";
            return false;
        }

        GLFWmonitor* target = monitors[monitorIndex];
        const GLFWvidmode* vm = glfwGetVideoMode(target);
        if (!vm) return false;

        int monX = 0, monY = 0;
        glfwGetMonitorPos(target, &monX, &monY);

        glfwWindowHint(GLFW_DECORATED,             GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING,              GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE,             GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW,         GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);

        m_Window = glfwCreateWindow(vm->width, vm->height, title.c_str(), nullptr, sharedContext);
        if (!m_Window) {
            const char* desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "[SecondaryOutputWindow] glfwCreateWindow fallo ('" << title
                      << "'). Código: " << code << " Desc: " << (desc ? desc : "N/A") << "\n";
            glfwDefaultWindowHints();
            return false;
        }

        glfwSetWindowPos(m_Window, monX, monY);

        GLFWwindow* backup = glfwGetCurrentContext();
        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);
        glfwMakeContextCurrent(backup);

        glfwShowWindow(m_Window);
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        m_MonitorIndex = monitorIndex;

        glfwDefaultWindowHints();
        return true;
    }

    void SecondaryOutputWindow::Destroy()
    {
        if (m_Window) {
            for (auto& cb : DestroyCallbacks())
                cb(m_Window);

            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        m_MonitorIndex = -1;
    }

    void SecondaryOutputWindow::RenderFrame(const RenderFn& renderFn)
    {
        if (!m_Window || !renderFn) return;

        GLFWwindow* backup = glfwGetCurrentContext();
        glfwMakeContextCurrent(m_Window);

        int fw = 0, fh = 0;
        glfwGetFramebufferSize(m_Window, &fw, &fh);
        if (fw > 0 && fh > 0) {
            glViewport(0, 0, fw, fh);
            renderFn(fw, fh);
        }

        glfwSwapBuffers(m_Window);
        glfwMakeContextCurrent(backup);
    }

} // namespace ProyecThor::Core