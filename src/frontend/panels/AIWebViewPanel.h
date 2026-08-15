#pragma once
#include <string>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  AIWebViewPanel — navegador real embebido (WebView2, Windows-only). Pese
//  al nombre (nacio para el Asistente de IA, ver AIAssistantPanel), es
//  generico: tambien la usa WebBrowserPanel para mostrar cualquier URL
//  (incluido "Enviar a Público", ver Reparent). El usuario inicia sesion
//  normal en lo que sea que este mostrando -- ProyecThor no ve ni guarda
//  esas credenciales, es un navegador real con su propio perfil (cookies/
//  sesion) en AppData, ProyecThor solo lo posiciona/muestra.
//
//  En Linux (sin WebView2 disponible) esta clase existe igual pero
//  IsAvailable() siempre devuelve false -- el llamador cae a abrir el
//  navegador externo del sistema en cambio (ver OpenURL.cpp).
// ─────────────────────────────────────────────────────────────────────────────
class AIWebViewPanel {
public:
    AIWebViewPanel();
    ~AIWebViewPanel();

    AIWebViewPanel(const AIWebViewPanel&)            = delete;
    AIWebViewPanel& operator=(const AIWebViewPanel&) = delete;

    // Crea el WebView2 la primera vez que hace falta (asincronico -- no
    // bloquea el frame actual) y navega a <url> apenas este listo. Llamar
    // de nuevo con otra URL mientras ya esta listo navega inmediato (asi
    // cambia de proveedor de IA sin recrear el navegador entero).
    void NavigateTo(const std::string& url);

    // Llamar UNA VEZ POR FRAME mientras la app este viva (no solo cuando el
    // panel esta abierto): sincroniza la posicion/tamaño/visibilidad de la
    // ventana nativa del WebView2 con el rect de PANTALLA (no de ventana)
    // que le corresponde este frame. visible=false la oculta sin
    // destruirla (mantiene la sesion/cookies intactas para la proxima vez).
    void UpdateBounds(int screenX, int screenY, int width, int height, bool visible);

    // false en Linux, o si algo fallo al crear el entorno de WebView2
    // (tipicamente: el Runtime de WebView2 no esta instalado -- viene con
    // Windows 10/11 de fabrica via Edge, pero una instalacion minimal/
    // servidor podria no tenerlo).
    bool IsAvailable() const;
    bool HasError() const;
    std::string GetLastError() const;

    // true una vez que el controller+webview de verdad terminaron de
    // crearse (ver NavigateTo) y ya se le mando la URL -- mientras esto sea
    // false y HasError() tambien sea false, el navegador esta en proceso de
    // arrancar (puede tardar); el llamador puede usarlo para mostrar
    // "Cargando..." en vez de un rectangulo vacio sin explicacion.
    bool IsReady() const;

    // Reparenta la ventana nativa del WebView2 a <newParentHwnd> (ej. la
    // ventana real de "ProjectorLive", ver PresentationCore::
    // GetProjectorNativeWindow) -- para "Enviar a Público" (WebBrowserPanel):
    // los siguientes UpdateBounds() posicionan el navegador relativo a ESE
    // padre en vez del principal. Pasar nullptr no hace nada (evita
    // reparentar a una ventana que todavia no existe este frame). No-op si
    // el navegador ni siquiera se creo todavia (ver NavigateTo).
    void Reparent(void* newParentHwnd);

private:
    struct Impl;
    static void NavigateNow(Impl* impl, const std::string& url);
    Impl* m_Impl;
};

} // namespace ProyecThor::UI
