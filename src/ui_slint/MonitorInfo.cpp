#include "MonitorInfo.h"

#include <algorithm>

#if defined(_WIN32)
#   include <windows.h>
#elif defined(__APPLE__)
// macOS: sin enumeración por ahora (se delega al compositor). Podría añadirse
// vía CoreGraphics (CGGetActiveDisplayList) en el futuro.
#else
#   include <dlfcn.h>
#endif

namespace ProyecThor::SlintUI {

#if defined(_WIN32)

static BOOL CALLBACK MonitorEnumCb(HMONITOR hMon, HDC, LPRECT, LPARAM data) {
    auto* out = reinterpret_cast<std::vector<MonitorInfo>*>(data);
    MONITORINFOEXA mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoA(hMon, &mi)) {
        MonitorInfo m;
        m.x         = mi.rcMonitor.left;
        m.y         = mi.rcMonitor.top;
        m.width     = mi.rcMonitor.right - mi.rcMonitor.left;
        m.height    = mi.rcMonitor.bottom - mi.rcMonitor.top;
        m.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        m.name      = mi.szDevice;
        out->push_back(std::move(m));
    }
    return TRUE;
}

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumCb,
                        reinterpret_cast<LPARAM>(&monitors));
    // Primario primero.
    std::stable_sort(monitors.begin(), monitors.end(),
        [](const MonitorInfo& a, const MonitorInfo& b) { return a.isPrimary && !b.isPrimary; });
    return monitors;
}

#elif defined(__APPLE__)

std::vector<MonitorInfo> EnumerateMonitors() {
    return {};
}

#else // Linux / BSD — XRandR cargado dinámicamente (no fuerza dependencia de X11)

namespace {

// Tipos mínimos de Xlib/XRandR para no incluir las cabeceras (y así no exigir
// -lX11 -lXrandr en el enlace). Solo se usa lo imprescindible.
struct XDisplay;
using XWindow = unsigned long;
using RRCrtc  = unsigned long;
using RROutput = unsigned long;

struct XRRMonitorInfoMin {
    unsigned long name;    // Atom
    int           primary;
    int           automatic;
    int           noutput;
    int           x, y;
    int           width, height;
    int           mwidth, mheight;
    RROutput*     outputs;
};

} // namespace

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;

    void* xlib   = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (!xlib) xlib = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL);
    void* xrandr = dlopen("libXrandr.so.2", RTLD_LAZY | RTLD_LOCAL);
    if (!xrandr) xrandr = dlopen("libXrandr.so", RTLD_LAZY | RTLD_LOCAL);
    if (!xlib || !xrandr) {
        if (xlib)   dlclose(xlib);
        if (xrandr) dlclose(xrandr);
        return monitors; // sin X11/XRandR disponible (p. ej. Wayland puro)
    }

    using XOpenDisplayFn       = XDisplay* (*)(const char*);
    using XCloseDisplayFn      = int (*)(XDisplay*);
    using XDefaultRootWindowFn = XWindow (*)(XDisplay*);
    using XRRGetMonitorsFn     = XRRMonitorInfoMin* (*)(XDisplay*, XWindow, int, int*);
    using XRRFreeMonitorsFn    = void (*)(XRRMonitorInfoMin*);

    auto XOpenDisplay       = reinterpret_cast<XOpenDisplayFn>(dlsym(xlib, "XOpenDisplay"));
    auto XCloseDisplay      = reinterpret_cast<XCloseDisplayFn>(dlsym(xlib, "XCloseDisplay"));
    auto XDefaultRootWindow = reinterpret_cast<XDefaultRootWindowFn>(dlsym(xlib, "XDefaultRootWindow"));
    auto XRRGetMonitors     = reinterpret_cast<XRRGetMonitorsFn>(dlsym(xrandr, "XRRGetMonitors"));
    auto XRRFreeMonitors    = reinterpret_cast<XRRFreeMonitorsFn>(dlsym(xrandr, "XRRFreeMonitors"));

    if (XOpenDisplay && XCloseDisplay && XDefaultRootWindow && XRRGetMonitors && XRRFreeMonitors) {
        if (XDisplay* dpy = XOpenDisplay(nullptr)) {
            XWindow root = XDefaultRootWindow(dpy);
            int count = 0;
            // get_active = 1 -> solo monitores conectados y activos.
            XRRMonitorInfoMin* mons = XRRGetMonitors(dpy, root, 1, &count);
            if (mons && count > 0) {
                for (int i = 0; i < count; ++i) {
                    MonitorInfo m;
                    m.x         = mons[i].x;
                    m.y         = mons[i].y;
                    m.width     = mons[i].width;
                    m.height    = mons[i].height;
                    m.isPrimary = mons[i].primary != 0;
                    m.name      = "Monitor " + std::to_string(i + 1);
                    monitors.push_back(std::move(m));
                }
                XRRFreeMonitors(mons);
            }
            XCloseDisplay(dpy);
        }
    }

    dlclose(xrandr);
    dlclose(xlib);

    // Primario primero para que el índice 0 sea siempre el principal.
    std::stable_sort(monitors.begin(), monitors.end(),
        [](const MonitorInfo& a, const MonitorInfo& b) { return a.isPrimary && !b.isPrimary; });
    return monitors;
}

#endif

int DefaultProjectionMonitorIndex(const std::vector<MonitorInfo>& monitors) {
    if (monitors.empty()) return 0;
    // Prioriza el monitor secundario (el primero que no sea el primario).
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (!monitors[i].isPrimary) return static_cast<int>(i);
    }
    // Si todos marcan primario o solo hay uno, usa el segundo si existe.
    return monitors.size() > 1 ? 1 : 0;
}

} // namespace ProyecThor::SlintUI
