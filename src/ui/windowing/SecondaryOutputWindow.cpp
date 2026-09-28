#include "SecondaryOutputWindow.h"
#include "core/PresentationCore.h"
#include <GL/glew.h>
#include "backends/imgui_impl_opengl3.h"
#include <iostream>
#include <filesystem>
#if !defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#endif

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

        GLFWmonitor* target = nullptr;
        if (monitorIndex >= 0 && monitorIndex < monitorCount) {
            target = monitors[monitorIndex];
        } else if (monitors && monitorCount > 0) {
            target = monitors[0];
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);
        glfwWindowHint(GLFW_AUTO_ICONIFY,          GLFW_FALSE);

        #if defined(GLFW_HAS_GETPLATFORM) && GLFW_HAS_GETPLATFORM
        bool isWayland = (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND);
        #else
        bool isWayland = false;
        #endif

        int winW = 1280;
        int winH = 720;
        int monX = 0, monY = 0;
        bool wantFullscreen = PresentationCore::Get().GetWindowFullscreen();
        bool isFullscreenOutput = false;

        if (target != nullptr && wantFullscreen)
        {
            // Salida en pantalla completa en el monitor fisico dedicado
            isFullscreenOutput = true;
            const GLFWvidmode* vm = glfwGetVideoMode(target);
            if (vm && vm->width > 0 && vm->height > 0) {
                winW = vm->width;
                winH = vm->height;
            }
            glfwGetMonitorPos(target, &monX, &monY);

            glfwWindowHint(GLFW_DECORATED,     GLFW_FALSE);
            glfwWindowHint(GLFW_FLOATING,      GLFW_TRUE);
            glfwWindowHint(GLFW_RESIZABLE,     GLFW_FALSE);
            glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);

            if (isWayland) {
                // En Wayland se debe pasar el monitor como 4to parametro para fullscreen nativo
                m_Window = glfwCreateWindow(winW, winH, title.c_str(), target, sharedContext);
            } else {
                // En Windows y X11, borderless window colocado en las coordenadas del monitor
                m_Window = glfwCreateWindow(winW, winH, title.c_str(), nullptr, sharedContext);
                if (m_Window) {
                    glfwSetWindowPos(m_Window, monX, monY);
                }
            }
        }
        else
        {
            // Modo Ventana (flotante / redimensionable para pruebas o OBS)
            winW = 960;
            winH = 540;
            if (target) {
                glfwGetMonitorPos(target, &monX, &monY);
                const GLFWvidmode* vm = glfwGetVideoMode(target);
                if (vm) {
                    monX += (vm->width - winW) / 2;
                    monY += (vm->height - winH) / 2;
                }
            }
            glfwWindowHint(GLFW_DECORATED,     GLFW_TRUE);
            glfwWindowHint(GLFW_FLOATING,      GLFW_FALSE);
            glfwWindowHint(GLFW_RESIZABLE,     GLFW_TRUE);
            glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

            std::string windowTitle = title;
            m_Window = glfwCreateWindow(winW, winH, windowTitle.c_str(), nullptr, sharedContext);
            if (m_Window) {
                glfwSetWindowPos(m_Window, monX, monY);
            }
        }

        if (!m_Window) {
            // Reintentar con GL 3.0 por si el driver no soporta 3.3
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            if (isFullscreenOutput) {
                if (isWayland) {
                    m_Window = glfwCreateWindow(winW, winH, title.c_str(), target, sharedContext);
                } else {
                    m_Window = glfwCreateWindow(winW, winH, title.c_str(), nullptr, sharedContext);
                    if (m_Window) glfwSetWindowPos(m_Window, monX, monY);
                }
            } else {
                std::string windowTitle = title;
                m_Window = glfwCreateWindow(winW, winH, windowTitle.c_str(), nullptr, sharedContext);
                if (m_Window) glfwSetWindowPos(m_Window, monX, monY);
            }
        }

        if (!m_Window) {
            const char* desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "[SecondaryOutputWindow] glfwCreateWindow fallo ('" << title
                      << "'). Código: " << code << " Desc: " << (desc ? desc : "N/A") << "\n";
            glfwDefaultWindowHints();
            return false;
        }

        // Contexto OpenGL e ImGui dedicado compartiendo atlas de fuentes
        GLFWwindow* backupWin = glfwGetCurrentContext();
        ImGuiContext* backupCtx = ImGui::GetCurrentContext();
        ImFontAtlas* sharedAtlas = backupCtx ? ImGui::GetIO().Fonts : nullptr;

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

        m_ImGuiContext = ImGui::CreateContext(sharedAtlas);
        if (m_ImGuiContext)
        {
            ImGui::SetCurrentContext(m_ImGuiContext);
            ImGui_ImplOpenGL3_Init("#version 130");

            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr; // no ensuciar imgui.ini con salidas secundarias

            if (!sharedAtlas)
            {
                io.Fonts->AddFontDefault();
                io.Fonts->Build();
            }
        }

        glfwShowWindow(m_Window);
        if (isFullscreenOutput) {
            glfwSetWindowPos(m_Window, monX, monY);
            glfwSetWindowSize(m_Window, winW, winH);
#if !defined(_WIN32)
            Display* dpy = glfwGetX11Display();
            ::Window xwin = glfwGetX11Window(m_Window);
            if (dpy && xwin) {
                ::Window root = DefaultRootWindow(dpy);

                // 1. Asignar el monitor físico a KWin / EWMH vía _NET_WM_FULLSCREEN_MONITORS
                Atom wmFullscreenMonitors = XInternAtom(dpy, "_NET_WM_FULLSCREEN_MONITORS", False);
                XEvent xevMon = {};
                xevMon.type = ClientMessage;
                xevMon.xclient.window = xwin;
                xevMon.xclient.message_type = wmFullscreenMonitors;
                xevMon.xclient.format = 32;
                xevMon.xclient.data.l[0] = monitorIndex; // top
                xevMon.xclient.data.l[1] = monitorIndex; // bottom
                xevMon.xclient.data.l[2] = monitorIndex; // left
                xevMon.xclient.data.l[3] = monitorIndex; // right
                xevMon.xclient.data.l[4] = 1;            // source indication
                XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xevMon);

                // 2. Activar _NET_WM_STATE_FULLSCREEN y _NET_WM_STATE_ABOVE en KWin
                Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", False);
                Atom wmFullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
                Atom wmAbove = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

                XEvent xevState = {};
                xevState.type = ClientMessage;
                xevState.xclient.window = xwin;
                xevState.xclient.message_type = wmState;
                xevState.xclient.format = 32;
                xevState.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
                xevState.xclient.data.l[1] = wmFullscreen;
                xevState.xclient.data.l[2] = wmAbove;
                xevState.xclient.data.l[3] = 1; // normal application
                XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xevState);

                // 3. Forzar posición y dimensiones
                XMoveResizeWindow(dpy, xwin, monX, monY, winW, winH);
                XRaiseWindow(dpy, xwin);
                XFlush(dpy);
            }
#endif
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        } else {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        m_MonitorIndex = monitorIndex;
        glfwDefaultWindowHints();

        glfwMakeContextCurrent(backupWin);
        ImGui::SetCurrentContext(backupCtx);

        return true;
    }

    void SecondaryOutputWindow::Destroy()
    {
        if (m_Window) {
            for (auto& cb : DestroyCallbacks())
                cb(m_Window);

            if (m_ImGuiContext) {
                GLFWwindow* backupWin = glfwGetCurrentContext();
                ImGuiContext* backupCtx = ImGui::GetCurrentContext();

                glfwMakeContextCurrent(m_Window);
                ImGui::SetCurrentContext(m_ImGuiContext);
                ImGui_ImplOpenGL3_Shutdown();
                ImGui::DestroyContext(m_ImGuiContext);
                m_ImGuiContext = nullptr;

                glfwMakeContextCurrent(backupWin);
                ImGui::SetCurrentContext(backupCtx);
            }

            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        m_MonitorIndex = -1;
    }

    void SecondaryOutputWindow::RenderFrame(const RenderFn& renderFn)
    {
        if (!m_Window || !renderFn) return;

        // Si hay un video nativo de VLC reproduciéndose a pantalla completa
        // en esta misma salida, NO hacer glClear ni glfwSwapBuffers de negro
        // continuo: evita el parpadeo negro entre las dos superficies en X11/compositor.
        if (PresentationCore::Get().IsActiveNativeVideo()) {
            return;
        }

        if (glfwWindowShouldClose(m_Window)) {
            Destroy();
            return;
        }

        // Si hay un video nativo de VLC reproduciéndose a pantalla completa
        // en esta misma salida, NO hacer glClear ni glfwSwapBuffers de negro
        // continuo: evita el parpadeo negro entre las dos superficies en X11/compositor.
        if (PresentationCore::Get().IsActiveNativeVideo()) {
            return;
        }

        GLFWwindow* backupWin = glfwGetCurrentContext();
        ImGuiContext* backupCtx = ImGui::GetCurrentContext();

        glfwMakeContextCurrent(m_Window);

        int fw = 0, fh = 0;
        glfwGetFramebufferSize(m_Window, &fw, &fh);

        if (fw > 0 && fh > 0 && m_ImGuiContext)
        {
            ImGui::SetCurrentContext(m_ImGuiContext);
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(fw), static_cast<float>(fh));
            io.DeltaTime   = 1.0f / 60.0f;

            glViewport(0, 0, fw, fh);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(fw), static_cast<float>(fh)));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##SecondaryOutputCanvas", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoNav);

            renderFn(fw, fh);

            ImGui::End();
            ImGui::PopStyleVar(2);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glfwSwapBuffers(m_Window);

        glfwMakeContextCurrent(backupWin);
        ImGui::SetCurrentContext(backupCtx);
    }

} // namespace ProyecThor::Core