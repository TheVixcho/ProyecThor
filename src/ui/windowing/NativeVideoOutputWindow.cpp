#include "NativeVideoOutputWindow.h"
#include "core/PresentationCore.h"

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
        monitorIndex = PresentationCore::Get().GetState().targetMonitorIndex;
        if (monitorIndex < 0 || monitorIndex >= monitorCount) {
            monitorIndex = (monitorCount > 1) ? 1 : 0;
        }
    }
    m_MonitorIndex = monitorIndex;

    GLFWmonitor* target = (monitors && monitorCount > 0) ? monitors[monitorIndex] : nullptr;
    if (!target) return nullptr;

    const GLFWvidmode* vm = glfwGetVideoMode(target);
    if (!vm) return nullptr;

    int monX = 0, monY = 0;
    glfwGetMonitorPos(target, &monX, &monY);

    bool wantFullscreen = PresentationCore::Get().GetWindowFullscreen();
    if (!wantFullscreen) {
        m_Width  = 1280;
        m_Height = 720;
        if (vm->width < m_Width || vm->height < m_Height) {
            m_Width  = vm->width * 3 / 4;
            m_Height = vm->height * 3 / 4;
        }
        m_MonX   = monX + (vm->width - m_Width) / 2;
        m_MonY   = monY + (vm->height - m_Height) / 2;
    } else {
        m_MonX   = monX;
        m_MonY   = monY;
        m_Width  = vm->width;
        m_Height = vm->height;
    }

    if (!m_Window)
    {
        // GLFW_NO_API: sin contexto GL — VLC dibuja directo en la
        // superficie nativa via AttachNativeWindow().
        glfwWindowHint(GLFW_CLIENT_API,    GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED,     wantFullscreen ? GLFW_FALSE : GLFW_TRUE);
        glfwWindowHint(GLFW_FLOATING,      wantFullscreen ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE,     GLFW_TRUE);
        glfwWindowHint(GLFW_AUTO_ICONIFY,  GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE,       GLFW_TRUE);

        m_Window = glfwCreateWindow(m_Width, m_Height,
                                    "ProyecThor - Video (VLC)", nullptr, nullptr);
        glfwDefaultWindowHints();

        if (!m_Window) {
            const char* desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "[NativeVideoOutputWindow] glfwCreateWindow fallo. Código: "
                      << code << " Desc: " << (desc ? desc : "N/A") << "\n";
            return nullptr;
        }

        // Pintar apenas se crea: fondo negro inmediato (evita cualquier flash blanco).
        PaintWindowBlack(m_Window);
    }
    else
    {
        glfwSetWindowAttrib(m_Window, GLFW_DECORATED, wantFullscreen ? GLFW_FALSE : GLFW_TRUE);
        glfwSetWindowAttrib(m_Window, GLFW_FLOATING,  wantFullscreen ? GLFW_TRUE : GLFW_FALSE);
        glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, GLFW_TRUE);
    }

    glfwSetWindowPos(m_Window, m_MonX, m_MonY);
    glfwSetWindowSize(m_Window, m_Width, m_Height);

#ifndef _WIN32
    Display* dpy = glfwGetX11Display();
    ::Window xwin = glfwGetX11Window(m_Window);
    if (dpy && xwin) {
        XMoveResizeWindow(dpy, xwin, m_MonX, m_MonY, m_Width, m_Height);
        XFlush(dpy);
    }
#endif

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
    glfwSetWindowPos(m_Window, m_MonX, m_MonY);
    glfwSetWindowSize(m_Window, m_Width, m_Height);

#ifndef _WIN32
    Display* dpy = glfwGetX11Display();
    ::Window xwin = glfwGetX11Window(m_Window);
    if (dpy && xwin) {
        bool wantFullscreen = PresentationCore::Get().GetWindowFullscreen();
        Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom wmAbove = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

        XEvent xevState = {};
        xevState.type = ClientMessage;
        xevState.xclient.window = xwin;
        xevState.xclient.message_type = wmState;
        xevState.xclient.format = 32;
        xevState.xclient.data.l[0] = wantFullscreen ? 1 : 0; // 1 = _NET_WM_STATE_ADD, 0 = _NET_WM_STATE_REMOVE
        xevState.xclient.data.l[1] = wmAbove;
        xevState.xclient.data.l[2] = 0;
        xevState.xclient.data.l[3] = 1;
        ::Window root = DefaultRootWindow(dpy);
        XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xevState);

        XMoveResizeWindow(dpy, xwin, m_MonX, m_MonY, m_Width, m_Height);
        XRaiseWindow(dpy, xwin);
        XFlush(dpy);
    }
#endif

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
