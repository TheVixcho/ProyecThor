#include "CapturePanel.h"
#include "DesignSystem.h"
#include "backend/settings/SettingsManager.h"
#ifdef PT_HAVE_WAYLAND_CAPTURE
  #include "WaylandScreenCapture.h"
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
//  Platform detection
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN32)
  #define PT_PLATFORM_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #pragma comment(lib, "gdi32.lib")
  #include "Win32ScreenCapture.h"
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__linux__)
  #define PT_PLATFORM_LINUX
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/Xatom.h>
  #ifdef PT_HAVE_XCOMPOSITE
    #include <X11/extensions/Xcomposite.h>
  #endif
  #ifdef PT_USE_OPENCV
    #include <opencv2/videoio.hpp>
    #include <opencv2/imgproc.hpp>
  #endif
#elif defined(__APPLE__)
  #define PT_PLATFORM_MACOS
#endif

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers X11 (titulo UTF-8 + manejador de errores)
//  Antes vivian pegados por error dentro del bloque de deteccion de
//  plataforma (rompiendo el balance de #ifdef/#endif). Van aca, junto al
//  resto del codigo especifico de Linux que los usa.
// ─────────────────────────────────────────────────────────────────────────────
#if defined(PT_PLATFORM_LINUX)

// Instala un manejador de errores X11 que ignora errores asincronicos
// (p.ej. una ventana que se cierra justo entre el enumerado y la consulta
// de sus atributos). Sin esto, ese tipo de condicion de carrera benigna
// puede tumbar la aplicacion entera via el manejador default de Xlib.
static void EnsureXErrorHandlerInstalled() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    XSetErrorHandler([](Display*, XErrorEvent*) -> int { return 0; });
}

// Devuelve el titulo de una ventana X11. Preferimos _NET_WM_NAME (UTF-8,
// usado por gestores de ventanas modernos) y caemos a WM_NAME/XFetchName
// (legado, ICCCM) si no esta disponible.
static std::string GetWindowTitleX11(Display* dpy, Window w) {
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8Str   = XInternAtom(dpy, "UTF8_STRING",  False);

    if (netWmName != None && utf8Str != None) {
        Atom          actualType;
        int           actualFormat;
        unsigned long numItems  = 0;
        unsigned long bytesAfter = 0;
        unsigned char* data = nullptr;

        if (XGetWindowProperty(dpy, w, netWmName, 0, ~0L, False, utf8Str,
                                &actualType, &actualFormat, &numItems, &bytesAfter, &data)
                == Success && data)
        {
            std::string title(reinterpret_cast<char*>(data), numItems);
            XFree(data);
            if (!title.empty()) return title;
        }
    }

    char* name = nullptr;
    if (XFetchName(dpy, w, &name) && name) {
        std::string title(name);
        XFree(name);
        return title;
    }
    return "";
}

#endif // PT_PLATFORM_LINUX

// ─────────────────────────────────────────────────────────────────────────────
//  CaptureBackend — implementación opaca
//  Cuando PT_USE_OPENCV está definido usa cv::VideoCapture.
//  En caso contrario, genera un patrón de prueba animado (stub).
// ─────────────────────────────────────────────────────────────────────────────
struct CapturePanel::CaptureBackend {
#ifdef PT_USE_OPENCV
    cv::VideoCapture cap;
    cv::Mat          frameBGR;
    cv::Mat          frameRGBA;
#endif

    // Stub: buffer de píxeles RGBA para el patrón de prueba (camara sin OpenCV)
    std::vector<uint8_t> stubPixels;
    int                  stubW = 1280;
    int                  stubH =  720;
    float                stubTime = 0.0f;

    bool isOpen = false;

    // ── Captura de ventana / monitor ─────────────────────────────────────
    std::vector<uint8_t> screenPixels; // buffer RGBA reutilizado entre frames
    int                  screenW = 0;
    int                  screenH = 0;

    CaptureSourceType    activeScreenType    = CaptureSourceType::Unknown;
    int                  activeMonitorIndex  = -1;

    std::chrono::steady_clock::time_point lastScreenGrab{};
    // Limite de tasa de captura: no tiene sentido volver a leer la pantalla
    // completa cada frame de ImGui (podria ser 120+ fps); con 30 fps sobra
    // de sobra para proyeccion en vivo y evita cargar la CPU/X11 de mas.
    static constexpr double kMinScreenGrabInterval = 1.0 / 30.0;

    // Cuando la fuente desaparece de forma transitoria (ventana minimizada,
    // cambio de espacio de trabajo, monitor que tarda en re-enumerarse tras
    // un hotplug), sostenemos el ultimo frame valido por un margen corto en
    // vez de cortar el preview a "Sin señal" de golpe — ese corte abrupto es
    // justamente lo que se percibe como parpadeo. Pasado el margen, si la
    // fuente sigue sin responder, se admite que se perdio de verdad.
    bool screenStale = false;
    std::chrono::steady_clock::time_point lastGoodScreenGrab{};
    static constexpr double kStaleGraceSeconds = 2.0;

    const uint8_t* HoldStaleOrNull(int& w, int& h) {
        using namespace std::chrono;
        if (!screenPixels.empty() &&
            duration<double>(steady_clock::now() - lastGoodScreenGrab).count() < kStaleGraceSeconds) {
            screenStale = true;
            w = screenW; h = screenH;
            return screenPixels.data();
        }
        screenStale = false;
        return nullptr;
    }

#ifdef PT_PLATFORM_WIN32
    HWND activeHwnd = nullptr;
    // DXGI Desktop Duplication (ver Win32ScreenCapture.h) -- lee el
    // framebuffer ya compuesto por DWM, a diferencia de BitBlt/PrintWindow
    // sobre el DC de una ventana puntual, que queda en negro para apps con
    // aceleracion de hardware (navegadores, Electron, juegos).
    std::unique_ptr<Win32ScreenCapture> win32Capture;
#elif defined(PT_PLATFORM_LINUX)
    Display*   xDisplay      = nullptr;
    ::Window   activeXWindow = 0;
  #ifdef PT_HAVE_XCOMPOSITE
    bool xcompositeRedirected = false;
    // XComposite en modo "Automatic" delega el mantenimiento del pixmap
    // fuera de pantalla al compositor activo (picom, KWin, Mutter...). Sin
    // un compositor corriendo (WMs livianos: i3, dwm, openbox sin picom,
    // etc.) ese pixmap nunca se llena de contenido real -- da negro -- y en
    // algunos drivers hasta la ventana en pantalla se ve afectada. Por eso
    // solo lo activamos si confirmamos un compositor via _NET_WM_CM_Sx.
    bool compositorActive     = false;
  #endif
#endif

    bool OpenCamera(int index) {
#ifdef PT_USE_OPENCV
        cap.open(index, cv::CAP_ANY);
        isOpen = cap.isOpened();
        return isOpen;
#else
        // Stub: siempre "abre" correctamente
        stubPixels.assign(stubW * stubH * 4, 0);
        isOpen = true;
        return true;
#endif
    }

    void Close() {
#ifdef PT_USE_OPENCV
        if (cap.isOpened()) cap.release();
#endif
        isOpen = false;
        stubPixels.clear();
    }

    /// Devuelve puntero a píxeles RGBA del último frame de camara (o nullptr).
    /// w/h se actualizan con las dimensiones reales.
    const uint8_t* GrabFrame(int& w, int& h) {
#ifdef PT_USE_OPENCV
        if (!cap.isOpened()) return nullptr;
        if (!cap.read(frameBGR) || frameBGR.empty()) return nullptr;
        cv::cvtColor(frameBGR, frameRGBA, cv::COLOR_BGR2RGBA);
        w = frameRGBA.cols;
        h = frameRGBA.rows;
        return frameRGBA.data;
#else
        // Genera un patrón animado SMPTE-like para debug
        stubTime += 0.016f;
        w = stubW; h = stubH;
        uint8_t* px = stubPixels.data();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float u = (float)x / w;
                float v = (float)y / h;
                uint8_t r = (uint8_t)((std::sin(u * 6.28f + stubTime)          * 0.5f + 0.5f) * 220 + 30);
                uint8_t g = (uint8_t)((std::sin(v * 6.28f + stubTime * 0.7f)   * 0.5f + 0.5f) * 220 + 30);
                uint8_t b = (uint8_t)((std::sin((u+v) * 4.0f + stubTime * 1.3f)* 0.5f + 0.5f) * 220 + 30);
                int idx = (y * w + x) * 4;
                px[idx+0] = r; px[idx+1] = g; px[idx+2] = b; px[idx+3] = 255;
            }
        }
        return px;
#endif
    }

    // ── Apertura / cierre de captura de ventana o monitor ────────────────
    bool OpenScreen(CaptureSourceType type, int monitorIndex, const std::string& windowHandle) {
        activeScreenType   = type;
        activeMonitorIndex = -1;

#ifdef PT_PLATFORM_WIN32
        activeHwnd = nullptr;
        if (!win32Capture) win32Capture = std::make_unique<Win32ScreenCapture>();

        if (type == CaptureSourceType::Window) {
            if (windowHandle.empty()) return false;
            activeHwnd = reinterpret_cast<HWND>(
                static_cast<uintptr_t>(std::stoull(windowHandle)));
            if (!IsWindow(activeHwnd)) {
                activeHwnd = nullptr;
                return false;
            }
            if (!win32Capture->OpenWindow(activeHwnd)) return false;
        } else if (type == CaptureSourceType::Monitor) {
            activeMonitorIndex = monitorIndex;
            int mCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mCount);
            if (activeMonitorIndex < 0 || activeMonitorIndex >= mCount) return false;
            int mx, my;
            glfwGetMonitorPos(monitors[activeMonitorIndex], &mx, &my);
            if (!win32Capture->OpenMonitor(mx, my)) return false;
        } else {
            return false;
        }
#elif defined(PT_PLATFORM_LINUX)
        if (!xDisplay) {
            xDisplay = XOpenDisplay(nullptr);
            if (!xDisplay) {
                std::cerr << "[CapturePanel] No se pudo abrir el display X11 para captura.\n";
                return false;
            }
        }

        activeXWindow = 0;
        if (type == CaptureSourceType::Window) {
            if (windowHandle.empty()) return false;
            activeXWindow = static_cast<::Window>(std::stoul(windowHandle));
            XWindowAttributes attrs;
            if (!XGetWindowAttributes(xDisplay, activeXWindow, &attrs)) {
                activeXWindow = 0;
                return false;
            }
#ifdef PT_HAVE_XCOMPOSITE
            // Solo redirigimos si hay un compositor activo (ver comentario
            // en la declaracion de compositorActive): sin uno, el pixmap
            // fuera de pantalla no se llena de contenido real y el preview
            // (o incluso la ventana original) se ve negro.
            {
                std::string cmPropName = "_NET_WM_CM_S" + std::to_string(DefaultScreen(xDisplay));
                Atom cmAtom = XInternAtom(xDisplay, cmPropName.c_str(), False);
                compositorActive = (cmAtom != None) && (XGetSelectionOwner(xDisplay, cmAtom) != None);
            }
            if (compositorActive) {
                // Redirige la ventana a un pixmap fuera de pantalla
                // mantenido por el compositor, para poder leer su contenido
                // compuesto real en GrabScreenFrame incluso si otra ventana
                // la tapa (sin esto, XGetImage directo sobre la ventana
                // devuelve basura en las zonas solapadas).
                XCompositeRedirectWindow(xDisplay, activeXWindow, CompositeRedirectAutomatic);
                xcompositeRedirected = true;
            }
#endif
        } else if (type == CaptureSourceType::Monitor) {
            activeMonitorIndex = monitorIndex;
        } else {
            return false;
        }
#else
        // macOS: sin implementacion todavia.
        (void)type; (void)monitorIndex; (void)windowHandle;
        return false;
#endif

        isOpen = true;
        return true;
    }

    void CloseScreen() {
#ifdef PT_PLATFORM_WIN32
        activeHwnd = nullptr;
        if (win32Capture) win32Capture->Close();
#elif defined(PT_PLATFORM_LINUX)
  #ifdef PT_HAVE_XCOMPOSITE
        if (xcompositeRedirected && xDisplay && activeXWindow) {
            XCompositeUnredirectWindow(xDisplay, activeXWindow, CompositeRedirectAutomatic);
        }
        xcompositeRedirected = false;
        compositorActive     = false;
  #endif
        activeXWindow = 0;
#endif
        activeScreenType   = CaptureSourceType::Unknown;
        activeMonitorIndex = -1;
        screenPixels.clear();
        screenStale        = false;
        isOpen = false;
    }

    /// Devuelve puntero a píxeles RGBA de la ventana/monitor capturado
    /// (o nullptr). w/h se actualizan con las dimensiones reales.
    /// Limita internamente la tasa de recaptura a kMinScreenGrabInterval.
    const uint8_t* GrabScreenFrame(int& w, int& h) {
        using namespace std::chrono;

        auto now = steady_clock::now();
        double elapsed = duration<double>(now - lastScreenGrab).count();
        if (elapsed < kMinScreenGrabInterval && !screenPixels.empty()) {
            w = screenW; h = screenH;
            return screenPixels.data();
        }
        lastScreenGrab = now;

#ifdef PT_PLATFORM_WIN32
        if (!win32Capture) return HoldStaleOrNull(w, h);

        int w32W = 0, w32H = 0;
        const uint8_t* w32px = win32Capture->GrabFrame(w32W, w32H);
        if (!w32px || w32W <= 0 || w32H <= 0) return HoldStaleOrNull(w, h);

        screenPixels.assign(w32px, w32px + static_cast<size_t>(w32W) * w32H * 4);
        w = w32W; h = w32H;
        screenW = w32W; screenH = w32H;
        screenStale = false;
        lastGoodScreenGrab = now;
        return screenPixels.data();

#elif defined(PT_PLATFORM_LINUX)
        if (!xDisplay) return nullptr;

        Drawable srcDrawable = 0;
        int x = 0, y = 0, width = 0, height = 0;
#ifdef PT_HAVE_XCOMPOSITE
        Pixmap compositePixmap = 0;
#endif

        if (activeScreenType == CaptureSourceType::Window) {
            if (!activeXWindow) return nullptr;
            XWindowAttributes attrs;
            if (!XGetWindowAttributes(xDisplay, activeXWindow, &attrs))
                return HoldStaleOrNull(w, h);

            // Si la ventana esta minimizada u oculta, XGetImage dispararia
            // un error X11 (BadMatch). En vez de arriesgarnos, sostenemos el
            // ultimo frame valido (con un margen de gracia — ver
            // HoldStaleOrNull) hasta que la ventana vuelva a estar visible.
            if (attrs.map_state != IsViewable)
                return HoldStaleOrNull(w, h);

            srcDrawable = activeXWindow;
            width  = attrs.width;
            height = attrs.height;

#ifdef PT_HAVE_XCOMPOSITE
            // Leemos del pixmap compuesto por el servidor (ver
            // XCompositeRedirectWindow en OpenScreen) en vez de la ventana
            // directamente: asi el contenido es correcto aunque otra
            // ventana la tape. Si por algun motivo no se puede nombrar el
            // pixmap (p.ej. la ventana se destruyo justo ahora), caemos de
            // vuelta a leer la ventana como antes.
            if (xcompositeRedirected) {
                compositePixmap = XCompositeNameWindowPixmap(xDisplay, activeXWindow);
                if (compositePixmap) srcDrawable = compositePixmap;
            }
#endif
        } else if (activeScreenType == CaptureSourceType::Monitor) {
            int mCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&mCount);
            if (activeMonitorIndex < 0 || activeMonitorIndex >= mCount) return HoldStaleOrNull(w, h);
            const GLFWvidmode* vm = glfwGetVideoMode(monitors[activeMonitorIndex]);
            if (!vm) return HoldStaleOrNull(w, h);
            int mx, my;
            glfwGetMonitorPos(monitors[activeMonitorIndex], &mx, &my);
            srcDrawable = RootWindow(xDisplay, DefaultScreen(xDisplay));
            x = mx; y = my;
            width = vm->width; height = vm->height;
        } else {
            return nullptr;
        }

        if (width <= 0 || height <= 0) return HoldStaleOrNull(w, h);

        XImage* img = XGetImage(xDisplay, srcDrawable, x, y, width, height,
                                AllPlanes, ZPixmap);
#ifdef PT_HAVE_XCOMPOSITE
        if (compositePixmap) XFreePixmap(xDisplay, compositePixmap);
#endif
        if (!img) return HoldStaleOrNull(w, h);

        screenPixels.assign(static_cast<size_t>(width) * height * 4, 0);

        // XGetPixel es lento pixel a pixel, pero es la forma portable de
        // interpretar cualquier mascara de color/bit-order que devuelva el
        // servidor X, sin asumir un formato de memoria especifico.
        for (int py = 0; py < height; ++py) {
            for (int px = 0; px < width; ++px) {
                unsigned long pixel = XGetPixel(img, px, py);
                uint8_t r = static_cast<uint8_t>((pixel & img->red_mask)   >> 16);
                uint8_t g = static_cast<uint8_t>((pixel & img->green_mask) >> 8);
                uint8_t b = static_cast<uint8_t>( pixel & img->blue_mask);
                size_t idx = (static_cast<size_t>(py) * width + px) * 4;
                screenPixels[idx + 0] = r;
                screenPixels[idx + 1] = g;
                screenPixels[idx + 2] = b;
                screenPixels[idx + 3] = 255;
            }
        }

        XDestroyImage(img);

        w = width; h = height;
        screenW = width; screenH = height;
        screenStale = false;
        lastGoodScreenGrab = now;
        return screenPixels.data();
#else
        (void)w; (void)h;
        return nullptr;
#endif
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de dibujo locales (mismo patron visual que el resto de paneles)
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    }
    ImU32 ColA(ImU32 col, int a) {
        return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
    }
    ImVec4 ToVec4(ImU32 col) {
        return ImGui::ColorConvertU32ToFloat4(col);
    }

    // Sombra suave reutilizando el mismo patrón visual que el resto de paneles.
    void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
        for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
            int alpha = static_cast<int>(34.0f - (i * 5.0f));
            dl->AddRectFilled(
                ImVec2(p0.x - i, p0.y - i + 3.0f),
                ImVec2(p1.x + i, p1.y + i + 3.0f),
                IM_COL32(0, 0, 0, std::max(0, alpha)), rounding + i);
        }
    }
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Ctor / Dtor
// ─────────────────────────────────────────────────────────────────────────────
CapturePanel::CapturePanel()
    : m_Backend(std::make_unique<CaptureBackend>())
#ifdef PT_HAVE_WAYLAND_CAPTURE
    , m_WaylandCapture(std::make_unique<WaylandScreenCapture>())
#endif
{
    RefreshSources();
}

CapturePanel::~CapturePanel() {
    StopCapture();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enumeración de fuentes
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::EnumerateCameras() {
#ifdef PT_USE_OPENCV
    // Si hay una camara en vivo, no volvemos a abrir su indice: la mayoria
    // de drivers (V4L2, DirectShow/MSMF) no toleran bien un segundo handle
    // concurrente al mismo dispositivo, y eso es causa tipica de glitches o
    // fallos de deteccion durante una captura activa. Reusamos lo que ya
    // sabemos de la fuente activa en su lugar.
    bool skipActiveCamera = m_IsCapturing &&
        m_ActiveSource.type == CaptureSourceType::Camera;

    for (int i = 0; i < 8; ++i) {
        if (skipActiveCamera && i == m_ActiveSource.index) {
            m_Sources.push_back(m_ActiveSource);
            continue;
        }
        cv::VideoCapture probe(i, cv::CAP_ANY);
        if (probe.isOpened()) {
            CaptureSource src;
            src.type  = CaptureSourceType::Camera;
            src.name  = "Cámara " + std::to_string(i);
            src.index = i;
            m_Sources.push_back(src);
            probe.release();
        }
    }
#else
    // Stub: añade una cámara ficticia para mostrar la UI
    CaptureSource stub;
    stub.type  = CaptureSourceType::Camera;
    stub.name  = "Cámara 0 (simulada)";
    stub.index = 0;
    m_Sources.push_back(stub);
#endif
}

void CapturePanel::EnumerateWindows() {
#ifdef PT_PLATFORM_WIN32
    // Enumera ventanas visibles con título
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* sources = reinterpret_cast<std::vector<CaptureSource>*>(lParam);
        if (!IsWindowVisible(hwnd)) return TRUE;
        char title[256] = {};
        GetWindowTextA(hwnd, title, sizeof(title));
        if (strlen(title) < 3) return TRUE;

        // Filtra ventanas de sistema
        char cls[128] = {};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (strcmp(cls, "Progman") == 0 || strcmp(cls, "Shell_TrayWnd") == 0) return TRUE;

        CaptureSource src;
        src.type   = CaptureSourceType::Window;
        src.name   = std::string(title);
        src.handle = std::to_string(reinterpret_cast<uintptr_t>(hwnd));
        sources->push_back(src);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&m_Sources));
#elif defined(PT_PLATFORM_LINUX)
    EnsureXErrorHandlerInstalled();

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        CaptureSource stub;
        stub.type = CaptureSourceType::Window;
        stub.name = "No se pudo conectar a X11";
        m_Sources.push_back(stub);
        return;
    }

    Window root = DefaultRootWindow(dpy);
    // False (en vez de True): siempre resuelve el atomo, incluso si nadie
    // lo habia interneado todavia en esta conexion. Con True, en casos
    // raros podia devolver None y la lista quedaba vacia sin motivo real.
    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", False);

    Atom          actualType;
    int           actualFormat;
    unsigned long numItems  = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;

    bool listOk = (netClientList != None) &&
        (XGetWindowProperty(dpy, root, netClientList, 0, ~0L, False, AnyPropertyType,
                            &actualType, &actualFormat, &numItems, &bytesAfter, &data)
         == Success) && data;

    if (listOk) {
        Window* windows = reinterpret_cast<Window*>(data);
        for (unsigned long i = 0; i < numItems; ++i) {
            Window w = windows[i];

            std::string title = GetWindowTitleX11(dpy, w);
            if (title.size() < 2) continue;

            CaptureSource src;
            src.type   = CaptureSourceType::Window;
            src.name   = title;
            src.handle = std::to_string(static_cast<unsigned long>(w));
            m_Sources.push_back(src);
        }
        XFree(data);
    } else {
        CaptureSource stub;
        stub.type = CaptureSourceType::Window;
        stub.name = "El gestor de ventanas no soporta _NET_CLIENT_LIST";
        m_Sources.push_back(stub);
    }

    XCloseDisplay(dpy);
#else
    // En macOS mostramos stub por ahora
    CaptureSource stub;
    stub.type = CaptureSourceType::Window;
    stub.name = "Ventana activa (simulada)";
    m_Sources.push_back(stub);
#endif
}

void CapturePanel::EnumerateMonitors() {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
        CaptureSource src;
        src.type  = CaptureSourceType::Monitor;
        src.name  = std::string("Monitor ") + std::to_string(i + 1)
                    + " — " + glfwGetMonitorName(monitors[i]);
        src.index = i;
        m_Sources.push_back(src);
    }
}

void CapturePanel::RefreshSources() {
    m_Sources.clear();
    EnumerateCameras();
#ifdef PT_HAVE_WAYLAND_CAPTURE
    if (WaylandScreenCapture::IsWaylandSession()) {
        // Bajo Wayland, EnumerateWindows()/EnumerateMonitors() (X11 puro)
        // no sirven: el compositor no vuelca contenido real a la ventana
        // raiz X11 heredada, asi que esa lista quedaria vacia o mostraria
        // fuentes que despues capturan en negro. En su lugar, una unica
        // fuente que dispara el picker nativo del portal de escritorio.
        CaptureSource src;
        src.type = CaptureSourceType::Screen;
        src.name = "Compartir pantalla o ventana…";
        m_Sources.push_back(src);
        return;
    }
#endif
    EnumerateWindows();
    EnumerateMonitors();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Captura
// ─────────────────────────────────────────────────────────────────────────────
bool CapturePanel::StartCapture(const CaptureSource& src) {
    StopCapture();
    m_ActiveSource = src;

    bool opened = false;
    if (src.type == CaptureSourceType::Camera) {
        opened = m_Backend->OpenCamera(src.index);
    } else if (src.type == CaptureSourceType::Window ||
               src.type == CaptureSourceType::Monitor) {
        opened = m_Backend->OpenScreen(src.type, src.index, src.handle);
    }
#ifdef PT_HAVE_WAYLAND_CAPTURE
    else if (src.type == CaptureSourceType::Screen) {
        // Exito "optimista": la negociacion con el portal (picker + permiso
        // del sistema) sigue en curso en un hilo aparte. RenderPreview()
        // refleja el estado real (Requesting/Streaming/Error) mientras tanto.
        m_WaylandCapture->RequestSession();
        opened = true;
    }
#endif
    if (!opened) return false;

    // Crea la textura OpenGL si no existe
    if (m_PreviewTexID == 0) {
        glGenTextures(1, &m_PreviewTexID);
        glBindTexture(GL_TEXTURE_2D, m_PreviewTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    m_IsCapturing = true;
    return true;
}

void CapturePanel::StopCapture() {
    m_Backend->Close();
    m_Backend->CloseScreen();
#ifdef PT_HAVE_WAYLAND_CAPTURE
    m_WaylandCapture->Stop();
#endif
    m_IsCapturing = false;
    m_ProjectOnScreen = false;
}

void* CapturePanel::GetCurrentTexture() {
    if (!m_IsCapturing) return nullptr;

    // El preview (RenderContent) y el proyector (RenderOnProjector) piden
    // ambos la textura actual dentro del mismo frame real. Sin este cache,
    // cada uno dispararia su propio grab — para camara eso es un cap.read()
    // real por llamada, es decir dos frames de camara distintos consumidos
    // por tick de UI (preview y proyector desincronizados, posible traba).
    int frameCount = ImGui::GetFrameCount();
    if (frameCount == m_LastGrabFrameCount) return m_LastGrabResult;
    m_LastGrabFrameCount = frameCount;
    m_LastGrabResult     = nullptr;

    int w = 0, h = 0;
    const uint8_t* pixels = nullptr;

    if (m_ActiveSource.type == CaptureSourceType::Camera) {
        pixels = m_Backend->GrabFrame(w, h);
    }
#ifdef PT_HAVE_WAYLAND_CAPTURE
    else if (m_ActiveSource.type == CaptureSourceType::Screen) {
        pixels = m_WaylandCapture->GrabFrame(w, h);
    }
#endif
    else {
        pixels = m_Backend->GrabScreenFrame(w, h);
    }

    if (!pixels || w == 0 || h == 0) return nullptr;

    glBindTexture(GL_TEXTURE_2D, m_PreviewTexID);

    if (w != m_FrameW || h != m_FrameH) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        m_FrameW = w; m_FrameH = h;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    m_LastGrabResult = reinterpret_cast<void*>(static_cast<uintptr_t>(m_PreviewTexID));
    return m_LastGrabResult;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render al proyector
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderOnProjector(ImDrawList* dl,
                                     float px, float py,
                                     float pw, float ph)
{
    if (!m_IsCapturing || !m_ProjectOnScreen) return;

    void* tex = GetCurrentTexture();
    if (!tex) return;

    ImU32 col = IM_COL32(255, 255, 255, (int)(m_Opacity * 255));

    if (m_PlacementMode == PlacementMode::Custom) {
        // Recuadro libre: el usuario lo ubico/redimensiono a mano en
        // RenderPlacementEditor(). Bordes redondeados para que se lea como
        // una capa flotante, a diferencia del modo Pantalla completa
        // (borde a borde, sin redondear).
        float destX = px + m_CustomX0 * pw;
        float destY = py + m_CustomY0 * ph;
        float destW = (m_CustomX1 - m_CustomX0) * pw;
        float destH = (m_CustomY1 - m_CustomY0) * ph;
        float rounding = std::min(20.0f, std::min(destW, destH) * 0.06f);
        dl->AddImageRounded(tex,
            ImVec2(destX, destY),
            ImVec2(destX + destW, destY + destH),
            ImVec2(0,0), ImVec2(1,1), col, rounding);
        return;
    }

    float destX = px, destY = py, destW = pw, destH = ph;

    if (!m_StretchToFill && m_FrameW > 0 && m_FrameH > 0) {
        float vidR    = (float)m_FrameW / (float)m_FrameH;
        float scnR    = pw / ph;
        if (vidR > scnR + 0.001f) {
            destH = destW / vidR;
            destY = py + (ph - destH) * 0.5f;
        } else if (vidR < scnR - 0.001f) {
            destW = destH * vidR;
            destX = px + (pw - destW) * 0.5f;
        }
    }

    dl->AddImage(tex,
        ImVec2(destX, destY),
        ImVec2(destX + destW, destY + destH),
        ImVec2(0,0), ImVec2(1,1), col);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI helpers
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderSourceSelector() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##capSearch", "Buscar fuente…", m_SearchBuf, sizeof(m_SearchBuf));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Header,        ColA(DS::AccentColor, 55));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  ColA(DS::AccentColor, 80));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,   ColA(DS::AccentColor, 110));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusMedium);
    ImGui::BeginChild("##capSrcList", ImVec2(0, 180), true);

    const char* typeLabelPrev = nullptr;

    for (int i = 0; i < (int)m_Sources.size(); ++i) {
        const auto& src = m_Sources[i];

        // Filtro de búsqueda
        if (m_SearchBuf[0] != '\0') {
            std::string lower = src.name;
            std::string filt  = m_SearchBuf;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::transform(filt.begin(),  filt.end(),  filt.begin(),  ::tolower);
            if (lower.find(filt) == std::string::npos) continue;
        }

        // Separador de categoría
        const char* typeLabel = nullptr;
        switch (src.type) {
            case CaptureSourceType::Camera:  typeLabel = "CÁMARAS";   break;
            case CaptureSourceType::Window:  typeLabel = "VENTANAS";  break;
            case CaptureSourceType::Monitor: typeLabel = "MONITORES"; break;
            default: break;
        }
        if (typeLabel && typeLabel != typeLabelPrev) {
            if (typeLabelPrev) ImGui::Spacing();
            ImGui::TextColored(ToVec4(ColA(DS::AccentColor, 160)), "%s", typeLabel);
            typeLabelPrev = typeLabel;
        }

        std::string label = src.name + "##cap" + std::to_string(i);

        bool selected = (m_SelectedIdx == i);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0, 22))) {
            m_SelectedIdx = i;
        }
        if (selected)
            ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

void CapturePanel::RenderPreview() {
    void* tex = GetCurrentTexture();

    float avail = ImGui::GetContentRegionAvail().x;
    float previewH = avail * (9.0f / 16.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end = ImVec2(pos.x + avail, pos.y + previewH);
    ImDrawList* dlOuter = ImGui::GetWindowDrawList();
    DrawSoftShadow(dlOuter, pos, end, DS::RadiusLarge);

    ImU32 borderCol = m_IsCapturing ? ColA(DS::AccentColor, 160) : ColA(DS::TextHint, 120);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillBot));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusLarge);
    ImGui::BeginChild("##capPreview", ImVec2(avail, previewH), false, ImGuiWindowFlags_NoScrollbar);

    if (tex) {
        float imgW = avail, imgH = previewH;
        if (m_FrameW > 0 && m_FrameH > 0) {
            float vidR = (float)m_FrameW / (float)m_FrameH;
            float boxR = avail / previewH;
            if (vidR > boxR) { imgH = imgW / vidR; }
            else             { imgW = imgH * vidR; }
        }
        float offX = (avail   - imgW) * 0.5f;
        float offY = (previewH - imgH) * 0.5f;

        ImGui::SetCursorPos(ImVec2(offX, offY));
        ImGui::Image(tex, ImVec2(imgW, imgH));
    } else {
        std::string statusText;
        ImU32       statusCol = DS::TextHint;
#ifdef PT_HAVE_WAYLAND_CAPTURE
        if (m_IsCapturing && m_ActiveSource.type == CaptureSourceType::Screen) {
            auto state = m_WaylandCapture->GetState();
            if (state == WaylandScreenCapture::State::Requesting) {
                statusText = "Esperando permiso del sistema…";
            } else if (state == WaylandScreenCapture::State::Error) {
                statusText = m_WaylandCapture->GetErrorMessage();
                if (statusText.empty()) statusText = "No se pudo iniciar la captura";
                statusCol = DS::DangerColor;
            }
        }
#endif
        if (statusText.empty()) statusText = "Sin señal activa";

        ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
        ImGui::SetCursorPos(ImVec2((avail - textSize.x) * 0.5f, (previewH - textSize.y) * 0.5f));
        ImGui::TextColored(ToVec4(statusCol), "%s", statusText.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(pos, end, borderCol, DS::RadiusLarge, 0, 1.5f);

    if (m_IsCapturing) {
        // Indicador "EN VIVO" en la esquina, mismo lenguaje visual que
        // el indicador de transmision de OClock.
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.55f + 0.35f * std::sin(t * 3.0f);
        ImU32 dotCol = ColA(DS::DangerColor, static_cast<int>(160 + 90 * pulse));
        ImVec2 dotPos(pos.x + 14.0f, pos.y + 14.0f);
        dl->AddCircleFilled(dotPos, 4.0f, dotCol);
        dl->AddText(ImVec2(dotPos.x + 10.0f, dotPos.y - 7.0f),
                    ColA(DS::TextSecondary, 230), "EN VIVO");
    }

    if (m_IsCapturing && m_Backend->screenStale) {
        // La fuente (ventana/monitor) desaparecio de forma transitoria y
        // seguimos mostrando el ultimo frame valido (ver HoldStaleOrNull)
        // en vez de cortar a "Sin señal" de golpe. Lo señalamos para que la
        // degradacion nunca sea silenciosa — mismo criterio que el resto de
        // indicadores de estado de la app.
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.55f + 0.35f * std::sin(t * 4.0f);
        ImU32 badgeCol = ColA(DS::TextHint, static_cast<int>(180 + 60 * pulse));
        dl->AddText(ImVec2(pos.x + 14.0f, end.y - 22.0f), badgeCol, "Reconectando…");
    }
}

void CapturePanel::RenderControls() {
    ImGui::Spacing();

    ImGui::TextColored(ToVec4(DS::TextHint), "Opacidad");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,      ToVec4(DS::AccentColor));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ToVec4(DS::AccentLight));
    ImGui::SliderFloat("##capOp", &m_Opacity, 0.0f, 1.0f, "%.2f");
    ImGui::PopStyleColor(4);

    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "Ubicación");
    ImGui::SameLine();
    bool isFullscreen = (m_PlacementMode == PlacementMode::Fullscreen);
    if (ImGui::RadioButton("Pantalla completa", isFullscreen))
        m_PlacementMode = PlacementMode::Fullscreen;
    ImGui::SameLine();
    if (ImGui::RadioButton("Posición libre", !isFullscreen))
        m_PlacementMode = PlacementMode::Custom;

    if (isFullscreen) {
        ImGui::SameLine(0, 20);
        ImGui::Checkbox("Ajustar al proyector", &m_StretchToFill);
    } else {
        RenderPlacementEditor();
    }
}

void CapturePanel::RenderPlacementEditor() {
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint),
        "Arrastrá el recuadro o sus esquinas para ubicarlo y redimensionarlo");
    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    float boxH  = avail * (9.0f / 16.0f);
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 end  = ImVec2(pos.x + avail, pos.y + boxH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, end, IM_COL32(10, 10, 13, 255), DS::RadiusMedium);
    dl->AddRect(pos, end, ColA(DS::TextHint, 110), DS::RadiusMedium);

    ImGui::Dummy(ImVec2(avail, boxH));

    const float kMinSize = 0.08f; // tamano minimo normalizado, evita colapsar a 0
    const float kHandleR = 8.0f;  // radio del agarre de esquina en pixeles

    ImVec2 rectMin(pos.x + m_CustomX0 * avail, pos.y + m_CustomY0 * boxH);
    ImVec2 rectMax(pos.x + m_CustomX1 * avail, pos.y + m_CustomY1 * boxH);

    // ── Cuerpo: arrastrar reposiciona sin cambiar el tamaño ──────────────
    ImGui::SetCursorScreenPos(rectMin);
    ImGui::InvisibleButton("##capPlaceBody", ImVec2(rectMax.x - rectMin.x, rectMax.y - rectMin.y));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        float w = m_CustomX1 - m_CustomX0, h = m_CustomY1 - m_CustomY0;
        m_CustomX0 = std::clamp(m_CustomX0 + d.x / avail, 0.0f, 1.0f - w);
        m_CustomY0 = std::clamp(m_CustomY0 + d.y / boxH,  0.0f, 1.0f - h);
        m_CustomX1 = m_CustomX0 + w;
        m_CustomY1 = m_CustomY0 + h;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    // ── Esquinas: arrastrar redimensiona anclando la esquina opuesta ─────
    struct Corner { bool right, bottom; const char* id; ImGuiMouseCursor cursor; };
    static const Corner kCorners[4] = {
        { false, false, "##capPlaceTL", ImGuiMouseCursor_ResizeNWSE },
        { true,  false, "##capPlaceTR", ImGuiMouseCursor_ResizeNESW },
        { false, true,  "##capPlaceBL", ImGuiMouseCursor_ResizeNESW },
        { true,  true,  "##capPlaceBR", ImGuiMouseCursor_ResizeNWSE },
    };
    for (const Corner& c : kCorners) {
        float cx = c.right ? rectMax.x : rectMin.x;
        float cy = c.bottom ? rectMax.y : rectMin.y;
        ImGui::SetCursorScreenPos(ImVec2(cx - kHandleR, cy - kHandleR));
        ImGui::InvisibleButton(c.id, ImVec2(kHandleR * 2.0f, kHandleR * 2.0f));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(c.cursor);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            float nx = (c.right ? m_CustomX1 : m_CustomX0) + d.x / avail;
            float ny = (c.bottom ? m_CustomY1 : m_CustomY0) + d.y / boxH;
            if (c.right) m_CustomX1 = std::clamp(nx, m_CustomX0 + kMinSize, 1.0f);
            else         m_CustomX0 = std::clamp(nx, 0.0f, m_CustomX1 - kMinSize);
            if (c.bottom) m_CustomY1 = std::clamp(ny, m_CustomY0 + kMinSize, 1.0f);
            else          m_CustomY0 = std::clamp(ny, 0.0f, m_CustomY1 - kMinSize);
        }
    }

    // ── Dibujo del recuadro (posicion ya puede haber cambiado arriba) ────
    rectMin = ImVec2(pos.x + m_CustomX0 * avail, pos.y + m_CustomY0 * boxH);
    rectMax = ImVec2(pos.x + m_CustomX1 * avail, pos.y + m_CustomY1 * boxH);
    dl->AddRectFilled(rectMin, rectMax, ColA(DS::AccentColor, 50), DS::RadiusMedium);
    dl->AddRect(rectMin, rectMax, DS::AccentColor, DS::RadiusMedium, 0, 2.0f);
    for (const Corner& c : kCorners) {
        ImVec2 hp(c.right ? rectMax.x : rectMin.x, c.bottom ? rectMax.y : rectMin.y);
        dl->AddCircleFilled(hp, 4.0f, DS::AccentLight);
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, end.y));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Escenas rápidas — 8 botones de color, guardan/recuperan una
//  configuración completa de captura (fuente + recuadro de posición libre
//  + opacidad) para saltar entre "escenas" con un click en vivo.
// ─────────────────────────────────────────────────────────────────────────────
bool CapturePanel::SnapshotCurrentCapture(Settings::CaptureSceneSettings& out) const {
    // Solo tiene sentido guardar algo que esté realmente en pantalla, en
    // modo "Posición libre" -- si no, al recuperar la escena se aplicaría
    // un recuadro viejo/default en vez del que el usuario ve ahora.
    if (!m_IsCapturing || m_PlacementMode != PlacementMode::Custom) return false;

    out.assigned     = true;
    out.sourceType   = static_cast<int>(m_ActiveSource.type);
    out.sourceIndex  = m_ActiveSource.index;
    out.sourceHandle = m_ActiveSource.handle;
    out.sourceName   = m_ActiveSource.name;
    out.x0 = m_CustomX0; out.y0 = m_CustomY0; out.x1 = m_CustomX1; out.y1 = m_CustomY1;
    out.opacity = m_Opacity;
    return true;
}

void CapturePanel::ApplyCaptureScene(const Settings::CaptureSceneSettings& sc) {
    if (!sc.assigned) return;

    CaptureSource src;
    src.type   = static_cast<CaptureSourceType>(sc.sourceType);
    src.index  = sc.sourceIndex;
    src.handle = sc.sourceHandle;
    src.name   = sc.sourceName;

    m_PlacementMode = PlacementMode::Custom;
    m_CustomX0 = sc.x0; m_CustomY0 = sc.y0; m_CustomX1 = sc.x1; m_CustomY1 = sc.y1;
    m_Opacity  = sc.opacity;

    // Si ya está capturando exactamente esa fuente, no reabrir el
    // dispositivo (evita el parpadeo/reinicio innecesario) -- solo se
    // actualiza posición/opacidad, ya hecho arriba.
    bool sameSource = m_IsCapturing &&
        m_ActiveSource.type == src.type && m_ActiveSource.index == src.index &&
        m_ActiveSource.handle == src.handle;
    if (!sameSource)
        StartCapture(src);

    // "Cambio rápido de escena" implica que queda al aire de una: no tiene
    // sentido recuperar una escena y que el operador tenga que acordarse
    // de tocar "Enviar al proyector" aparte.
    m_ProjectOnScreen = true;
}

void CapturePanel::SaveCurrentAsScene(int slot) {
    if (slot < 0 || slot >= Settings::kCaptureSceneCount) return;
    auto& sc = Settings::SettingsManager::Get().GetSettings().capture.scenes[slot];
    if (!SnapshotCurrentCapture(sc)) return;
    Settings::SettingsManager::Get().Save();
}

void CapturePanel::RecallScene(int slot) {
    if (slot < 0 || slot >= Settings::kCaptureSceneCount) return;
    const auto& sc = Settings::SettingsManager::Get().GetSettings().capture.scenes[slot];
    ApplyCaptureScene(sc);
}

void CapturePanel::ClearScene(int slot) {
    if (slot < 0 || slot >= Settings::kCaptureSceneCount) return;
    Settings::SettingsManager::Get().GetSettings().capture.scenes[slot] = Settings::CaptureSceneSettings{};
    Settings::SettingsManager::Get().Save();
}

void CapturePanel::RenderSceneButtons() {
    static const ImU32 kSceneColors[Settings::kCaptureSceneCount] = {
        IM_COL32(230,  90,  90, 255), // 1 rojo
        IM_COL32(230, 150,  70, 255), // 2 naranja
        IM_COL32(230, 210,  70, 255), // 3 amarillo
        IM_COL32(120, 210, 120, 255), // 4 verde
        IM_COL32( 90, 190, 220, 255), // 5 celeste
        IM_COL32(100, 130, 230, 255), // 6 azul
        IM_COL32(170, 120, 230, 255), // 7 violeta
        IM_COL32(230, 120, 180, 255), // 8 rosa
    };

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();
    DS::GlassSectionHeader("ESCENAS RÁPIDAS DE CAPTURA");
    // Icono "(i)" al final de la misma fila del header (mismo truco de
    // SameLine + SetCursorPosX que el boton de refresh en RenderContent):
    // GlassSectionHeader dibuja su texto directo por ImDrawList y avanza el
    // cursor con un Dummy de ancho completo, asi que alcanza para alinear
    // algo mas a la derecha en esa misma linea. Reemplaza el texto de ayuda
    // que antes quedaba siempre visible -- ahora solo aparece al pasar el
    // mouse, para no saturar de letra un panel que ya tiene bastante.
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
    ImGui::TextUnformatted("(i)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click: aplicar.\nClick derecho: guardar la posición libre actual o borrar.");
    ImGui::Spacing();

    auto& scenes = Settings::SettingsManager::Get().GetSettings().capture.scenes;
    const bool canSave = m_IsCapturing && m_PlacementMode == PlacementMode::Custom;

    const int   cols     = 4;
    const float btnSize  = 52.0f;
    const float spacing  = 8.0f;

    for (int i = 0; i < Settings::kCaptureSceneCount; i++) {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        auto& sc = scenes[i];
        ImVec4 baseCol = ImGui::ColorConvertU32ToFloat4(kSceneColors[i]);
        ImVec4 fillCol = sc.assigned ? baseCol : ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.14f);
        ImVec4 hovCol  = ImVec4(baseCol.x, baseCol.y, baseCol.z, sc.assigned ? 0.85f : 0.30f);
        ImVec4 bordCol = sc.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.35f)
                                      : ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.55f);

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        fillCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  hovCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   hovCol);
        ImGui::PushStyleColor(ImGuiCol_Border,         bordCol);
        ImGui::PushStyleColor(ImGuiCol_Text,           sc.assigned ? ImVec4(0.08f, 0.08f, 0.09f, 1.0f) : ToVec4(DS::TextHint));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);

        char label[8];
        snprintf(label, sizeof(label), "%d", i + 1);
        bool clicked = ImGui::Button(label, ImVec2(btnSize, btnSize));

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        if (clicked && sc.assigned) RecallScene(i);

        if (ImGui::BeginPopupContextItem("##sceneCtx")) {
            if (ImGui::MenuItem(sc.assigned ? "Reemplazar con posición actual" : "Guardar posición actual acá",
                                nullptr, false, canSave))
                SaveCurrentAsScene(i);
            if (!canSave) {
                ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
                ImGui::TextWrapped("Necesita una fuente activa en modo \"Posición libre\".");
                ImGui::PopStyleColor();
            }
            if (sc.assigned) {
                ImGui::Separator();
                if (ImGui::MenuItem("Borrar escena"))
                    ClearScene(i);
            }
            ImGui::EndPopup();
        }

        if (sc.assigned && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", sc.sourceName.c_str());

        ImGui::PopID();
    }
}

void CapturePanel::RenderProjectButton() {
    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    float w = ImGui::GetContentRegionAvail().x;
    bool canStart = (m_SelectedIdx >= 0 && m_SelectedIdx < (int)m_Sources.size());

    if (!m_IsCapturing) {
        ImGui::BeginDisabled(!canStart);
        DS::GlassButton("Iniciar captura", ImVec2(w, 36.0f), DS::SuccessColor);
        if (ImGui::IsItemClicked() && canStart)
            StartCapture(m_Sources[m_SelectedIdx]);
        ImGui::EndDisabled();
    } else {
        if (m_ProjectOnScreen) {
            if (DS::GlassButton("Quitar del proyector", ImVec2(w, 36.0f), DS::DangerColor))
                m_ProjectOnScreen = false;
        } else {
            if (DS::GlassButton("Enviar al proyector", ImVec2(w, 36.0f), DS::AccentColor))
                m_ProjectOnScreen = true;
        }

        ImGui::Spacing();
        if (DS::GlassButton("Detener captura", ImVec2(w, 28.0f), DS::AccentColorDim))
            StopCapture();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────
void CapturePanel::RenderContent() {
    // ── Cabecera con botón de actualizar ─────────────────────────────────
    DS::GlassSectionHeader("FUENTE DE CAPTURA");
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 24);
    if (ImGui::SmallButton("##capRefresh")) RefreshSources();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Actualizar fuentes");

    ImGui::Spacing();
    RenderSourceSelector();

    ImGui::Spacing();
    DS::GlassSectionHeader("PREVISUALIZACIÓN");
    ImGui::Spacing();
    RenderPreview();

    RenderControls();
    RenderSceneButtons();
    RenderProjectButton();
}
} // namespace ProyecThor::UI