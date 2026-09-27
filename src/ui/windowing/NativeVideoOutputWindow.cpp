#include "NativeVideoOutputWindow.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif
#include <cstdint>
#include <iostream>

namespace ProyecThor::Core {

// Sin esto, la primera vez que se muestra una ventana recien creada (antes
// de que VLC adjunte su salida y pinte el primer frame real) se ve blanca
// unos instantes — el fondo por defecto de una ventana nueva en ambas
// plataformas — que se nota feo, sobre todo en pantalla completa. Pintarla
// negro de una (y, en Windows, dejar el fondo de la CLASE en negro tambien,
// por si un WM_ERASEBKGND futuro llega antes que VLC) hace que el "hueco"
// mientras se arranca un clip nuevo se vea como una pantalla en negro
// normal, no como un flash blanco.
//
// IMPORTANTE: esto cubre el fondo de LA VENTANA (GDI/X11), pero no lo que
// el propio modulo de video de VLC (Direct3D9/11, XVideo) pinte encima una
// vez que se le adjunta — ese es otro nivel, fuera de nuestro control
// directo. Por eso ademas de esto, la ventana no se REVELA (Reveal()) hasta
// que BackgroundLayer::Update() confirma que el reproductor nuevo ya esta
// reproduciendo de verdad (ver m_NativeRevealPending) — este pintado negro
// es la red de seguridad para el rato en que la ventana existe pero esta
// oculta/recien creada, no la solucion completa por si sola.
static void PaintWindowBlack(GLFWwindow* window)
{
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return;

    SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));

    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC hdc = GetDC(hwnd);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(hwnd, hdc);
#else
    Display* dpy = glfwGetX11Display();
    ::Window  xwin = glfwGetX11Window(window);
    if (!dpy || !xwin) return;

    XSetWindowBackground(dpy, xwin, BlackPixel(dpy, DefaultScreen(dpy)));
    XClearWindow(dpy, xwin);
    XFlush(dpy);
#endif
}

// Crea (la primera vez) o reposiciona la ventana sobre el monitor indicado,
// SIN mostrarla — comun a Show()/CreateHidden(). Devuelve el handle nativo,
// o nullptr si fallo.
void* NativeVideoOutputWindow::CreateHidden(int monitorIndex)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex < 0 || monitorIndex >= monitorCount) {
        std::cerr << "[NativeVideoOutputWindow] Indice de monitor invalido: " << monitorIndex << "\n";
        return nullptr;
    }

    GLFWmonitor* target = monitors[monitorIndex];
    const GLFWvidmode* vm = glfwGetVideoMode(target);
    if (!vm) return nullptr;

    int monX = 0, monY = 0;
    glfwGetMonitorPos(target, &monX, &monY);

    if (!m_Window)
    {
        // GLFW_NO_API: sin contexto GL — VLC dibuja directo en la
        // superficie nativa via AttachNativeWindow().
        glfwWindowHint(GLFW_CLIENT_API,    GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED,     GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING,      GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE,     GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE,       GLFW_FALSE);

        m_Window = glfwCreateWindow(vm->width, vm->height,
                                    "ProyecThor - Video (VLC)", nullptr, nullptr);
        glfwDefaultWindowHints();

        if (!m_Window) {
            const char* desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "[NativeVideoOutputWindow] glfwCreateWindow fallo. Código: "
                      << code << " Desc: " << (desc ? desc : "N/A") << "\n";
            return nullptr;
        }

        // Pintar apenas se crea: nunca debe llegar a mostrarse (ni
        // siquiera un frame) con el fondo blanco por defecto.
        PaintWindowBlack(m_Window);
    }

    glfwSetWindowPos(m_Window, monX, monY);
    glfwSetWindowSize(m_Window, vm->width, vm->height);

#ifdef _WIN32
    return static_cast<void*>(glfwGetWin32Window(m_Window));
#else
    return reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(m_Window)));
#endif
}

void* NativeVideoOutputWindow::Show(int monitorIndex)
{
    void* handle = CreateHidden(monitorIndex);
    if (handle) Reveal();
    return handle;
}

void NativeVideoOutputWindow::Reveal()
{
    if (!m_Window) return;
    glfwShowWindow(m_Window);
    m_Visible = true;
}

void NativeVideoOutputWindow::Hide()
{
    if (m_Window) glfwHideWindow(m_Window);
    m_Visible = false;
}

void NativeVideoOutputWindow::Destroy()
{
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    m_Visible = false;
}

} // namespace ProyecThor::Core
