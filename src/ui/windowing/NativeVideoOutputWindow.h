#pragma once

struct GLFWwindow;

namespace ProyecThor::Core {

// Ventana nativa minima y borderless para el motor de renderizado "VLC
// (ventana nativa)" (ver BackgroundLayer::SetUseNativeEngine). A
// diferencia de SecondaryOutputWindow (pensada para render propio via un
// contexto GL compartido, ver su .cpp), esta NO crea contexto GL: VLC
// dibuja el video directo en la superficie nativa via
// VLCBasePlayer::AttachNativeWindow(), y un contexto GL de mas ahi solo
// podria interferir con el renderer acelerado de VLC sobre esa misma
// ventana.
class NativeVideoOutputWindow {
public:
    NativeVideoOutputWindow()  = default;
    ~NativeVideoOutputWindow() { Destroy(); }

    NativeVideoOutputWindow(const NativeVideoOutputWindow&)            = delete;
    NativeVideoOutputWindow& operator=(const NativeVideoOutputWindow&) = delete;

    // Crea (la primera vez) o reposiciona (si ya existe) la ventana sobre
    // el monitor indicado y la muestra. Devuelve el handle nativo (HWND en
    // Windows, X11 Window casteada a void* en Linux) listo para pasarle a
    // VLCBasePlayer::AttachNativeWindow(), o nullptr si fallo.
    void* Show(int monitorIndex);

    // Igual que Show(), pero NO la hace visible (glfwShowWindow) — usar
    // junto con Reveal() cuando hace falta que la ventana ya exista (para
    // adjuntarle un reproductor y empezar a reproducir) pero se decida
    // recien despues, con otra informacion, si conviene mostrarla ya
    // (ver BackgroundLayer::Update() / m_NativeRevealPending: no se
    // revela hasta que el video nuevo confirma que ya esta reproduciendo,
    // para nunca exponer el instante de inicializacion propio del modulo
    // de video de VLC — que puede pintar cualquier cosa, incluso blanco,
    // antes de su primer frame real).
    void* CreateHidden(int monitorIndex);

    // Hace visible una ventana ya creada por CreateHidden(). No-op si no
    // hay ventana.
    void Reveal();

    // Oculta la ventana sin destruirla — Show()/Reveal() la vuelven a
    // mostrar sin recrearla.
    void Hide();

    void Destroy();

    bool IsVisible() const { return m_Visible; }

private:
    GLFWwindow* m_Window  = nullptr;
    bool        m_Visible = false;
};

} // namespace ProyecThor::Core
