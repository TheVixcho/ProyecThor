#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace ProyecThor::UI {

#ifdef PT_HAVE_WAYLAND_CAPTURE

// Captura de pantalla/ventana para sesiones Wayland via el portal de
// escritorio (org.freedesktop.portal.ScreenCast) + PipeWire.
//
// Por que existe esto por separado de CaptureBackend (ver CapturePanel.cpp):
// XGetImage/XComposite no sirven bajo Wayland. El compositor (KWin, Mutter,
// etc.) nunca vuelca el contenido real compuesto a la "ventana raiz" X11
// heredada -- es una restriccion de seguridad intencional del protocolo (a
// diferencia de X11, ningun cliente puede leer los pixeles de otro por
// default). El camino nativo es pedirle permiso al usuario via el portal, que
// entrega el video como un nodo de PipeWire.
class WaylandScreenCapture {
public:
    enum class State {
        Idle,
        Requesting,  // esperando el picker/permiso del sistema (KDE)
        Streaming,
        Error,
    };

    WaylandScreenCapture();
    ~WaylandScreenCapture();

    WaylandScreenCapture(const WaylandScreenCapture&)            = delete;
    WaylandScreenCapture& operator=(const WaylandScreenCapture&) = delete;

    // true si la sesion actual es Wayland (XDG_SESSION_TYPE=wayland o
    // WAYLAND_DISPLAY seteado). CapturePanel lo usa para decidir si mostrar
    // la fuente "Compartir pantalla o ventana..." en vez de enumerar
    // ventanas/monitores por X11 (que no sirve en este entorno).
    static bool IsWaylandSession();

    // Lanza la negociacion con el portal en un hilo dedicado; no bloquea al
    // llamante. El picker + dialogo de permiso de KDE aparecen de forma
    // asincronica; el resultado se refleja en GetState()/GetErrorMessage().
    void RequestSession();

    void Stop();

    State       GetState() const;
    std::string GetErrorMessage() const;

    // Ultimo frame RGBA disponible, o nullptr si GetState() != Streaming.
    // w/h se actualizan con las dimensiones reales. Llamable desde el hilo
    // de render; internamente copia el frame bajo lock (el hilo de PipeWire
    // escribe frames en paralelo) asi que el puntero devuelto es estable
    // hasta el proximo llamado a GrabFrame().
    const uint8_t* GrabFrame(int& w, int& h);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // PT_HAVE_WAYLAND_CAPTURE

} // namespace ProyecThor::UI
