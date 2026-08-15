#include "AIWebViewPanel.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "WebView2.h"
#include "backend/core/AppPaths.h"

#include <functional>
#include <atomic>
#include <string>

namespace ProyecThor::UI {

// ── ComPtr minimo ────────────────────────────────────────────────────────
// No se usa Microsoft::WRL::ComPtr (wrl/client.h) a proposito: es una
// plantilla pensada para MSVC, con compatibilidad historicamente pareja
// con MinGW-w64 segun la version del toolchain. Este wrapper hace lo mismo
// (Release() al destruirse/reasignarse) sin esa dependencia -- unas 20
// lineas, no vale la pena arriesgar el build por eso.
template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    explicit ComPtr(T* p) : m_p(p) {}
    ComPtr(const ComPtr& other) : m_p(other.m_p) { if (m_p) m_p->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : m_p(other.m_p) { other.m_p = nullptr; }
    ~ComPtr() { Reset(); }

    ComPtr& operator=(const ComPtr& other) {
        if (this != &other) { Reset(); m_p = other.m_p; if (m_p) m_p->AddRef(); }
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) { Reset(); m_p = other.m_p; other.m_p = nullptr; }
        return *this;
    }

    void Reset() { if (m_p) { m_p->Release(); m_p = nullptr; } }
    T*   Get() const { return m_p; }
    T**  GetAddressOf() { Reset(); return &m_p; }
    T*   operator->() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }

private:
    T* m_p = nullptr;
};

// ── Completion handlers ──────────────────────────────────────────────────
// WebView2 crea el entorno y el controller de forma asincronica -- avisa
// via estos handlers COM (implementados a mano, sin WRL::Callback por el
// mismo motivo del ComPtr de arriba). Referencia contada a mano segun el
// protocolo COM estandar: el "new" sin "delete" explicito no es un leak,
// WebView2 llama Release() el mismo cuando termina de usarlos.
class EnvironmentHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Environment*)>;
    explicit EnvironmentHandler(Callback cb) : m_cb(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        m_cb(result, env);
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --m_ref;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler || riid == IID_IUnknown) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
private:
    Callback           m_cb;
    std::atomic<ULONG> m_ref{ 1 };
};

class ControllerHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Controller*)>;
    explicit ControllerHandler(Callback cb) : m_cb(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        m_cb(result, controller);
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --m_ref;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler || riid == IID_IUnknown) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
private:
    Callback           m_cb;
    std::atomic<ULONG> m_ref{ 1 };
};

// Clase de ventana minima para el HWND hijo que hospeda al WebView2 -- no
// dibuja nada ella misma (WebView2 pinta todo su cliente), asi que el
// WndProc solo necesita el default.
static const wchar_t* kHostClassName = L"ProyecThorAIWebViewHost";

static void EnsureHostClassRegistered() {
    static bool s_registered = false;
    if (s_registered) return;
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kHostClassName;
    // MAKEINTRESOURCEW explicito (no el macro IDC_ARROW a secas): este
    // proyecto no define UNICODE/_UNICODE globalmente, asi que IDC_ARROW
    // resuelve a la variante ANSI (MAKEINTRESOURCEA) y no compila contra
    // LoadCursorW, que pide LPCWSTR.
    wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    // Pincel de fondo explicito (antes NULL): sin esto, WM_ERASEBKGND no
    // pinta nada definido y cualquier frame donde WebView2 todavia no puso
    // su propio contenido adentro (arranque, o mientras carga una pagina)
    // podia mostrar basura/negro sin explicacion en vez de un negro solido
    // intencional.
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    s_registered = true;
}

struct AIWebViewPanel::Impl {
    HWND parentHwnd = nullptr;
    HWND hostHwnd   = nullptr;

    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller>  controller;
    ComPtr<ICoreWebView2>            webview;

    bool comInitialized = false;
    bool creating       = false;
    bool ready          = false;
    bool failed         = false;
    std::string pendingUrl;
    std::string lastError;

    // ver comentario en UpdateBounds: si la creacion async nunca termina
    // (ni exito ni error real -- ej. algo se cuelga en el lado de WebView2),
    // sin esto el operador se queda mirando un rectangulo negro para
    // siempre, sin ningun mensaje.
    double creatingStartTime = 0.0;
};

AIWebViewPanel::AIWebViewPanel() : m_Impl(new Impl()) {}

AIWebViewPanel::~AIWebViewPanel() {
    if (m_Impl->hostHwnd) DestroyWindow(m_Impl->hostHwnd);
    m_Impl->controller.Reset();
    m_Impl->webview.Reset();
    m_Impl->env.Reset();
    if (m_Impl->comInitialized) CoUninitialize();
    delete m_Impl;
}

void AIWebViewPanel::NavigateNow(AIWebViewPanel::Impl* impl, const std::string& url) {
    if (!impl->webview) { impl->pendingUrl = url; return; }
    std::wstring wurl(url.begin(), url.end()); // URLs son ASCII, alcanza con esta conversion simple
    impl->webview->Navigate(wurl.c_str());
}

void AIWebViewPanel::NavigateTo(const std::string& url) {
    if (m_Impl->failed) return;

    if (m_Impl->ready) {
        NavigateNow(m_Impl, url);
        return;
    }
    m_Impl->pendingUrl = url;
    if (m_Impl->creating) return; // ya se esta creando, cuando termine navega solo (ver lambda de abajo)
    m_Impl->creating = true;
    m_Impl->creatingStartTime = glfwGetTime();

    if (!m_Impl->comInitialized) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        // S_FALSE = ya estaba inicializado en este hilo (ver FilePicker.cpp) -- igual de valido.
        m_Impl->comInitialized = SUCCEEDED(hr);
    }

    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) { m_Impl->failed = true; m_Impl->lastError = "No hay ventana GLFW activa."; return; }
    m_Impl->parentHwnd = glfwGetWin32Window(win);
    if (!m_Impl->parentHwnd) { m_Impl->failed = true; m_Impl->lastError = "No se pudo obtener el HWND nativo."; return; }

    EnsureHostClassRegistered();
    m_Impl->hostHwnd = CreateWindowExW(
        0, kHostClassName, L"", WS_CHILD,
        0, 0, 100, 100,
        m_Impl->parentHwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_Impl->hostHwnd) { m_Impl->failed = true; m_Impl->lastError = "No se pudo crear la ventana anfitriona."; return; }

    // Perfil propio en AppData (cookies/sesion de login persisten entre
    // arranques) -- sin esto WebView2 intenta escribir al lado del .exe,
    // que puede ser de solo lectura (Program Files).
    std::string profileDir = ProyecThor::GetAppDataRoot() + "/webview2_profile";
    std::wstring wProfileDir(profileDir.begin(), profileDir.end());

    Impl* impl = m_Impl;
    auto* envHandler = new EnvironmentHandler(
        [impl](HRESULT hr, ICoreWebView2Environment* envRaw) {
            if (FAILED(hr) || !envRaw) {
                impl->failed = true;
                impl->lastError = "No se pudo crear el entorno de WebView2 (HRESULT 0x" +
                    std::to_string((unsigned long)hr) + "). ¿Esta instalado el WebView2 Runtime?";
                return;
            }
            impl->env = ComPtr<ICoreWebView2Environment>(envRaw);
            impl->env->AddRef();

            auto* ctrlHandler = new ControllerHandler(
                [impl](HRESULT hr2, ICoreWebView2Controller* ctrlRaw) {
                    if (FAILED(hr2) || !ctrlRaw) {
                        impl->failed = true;
                        impl->lastError = "No se pudo crear el controller de WebView2 (HRESULT 0x" +
                            std::to_string((unsigned long)hr2) + ").";
                        return;
                    }
                    impl->controller = ComPtr<ICoreWebView2Controller>(ctrlRaw);
                    impl->controller->AddRef();
                    impl->controller->get_CoreWebView2(impl->webview.GetAddressOf());
                    impl->ready = true;
                    if (!impl->pendingUrl.empty())
                        NavigateNow(impl, impl->pendingUrl);
                });
            envRaw->CreateCoreWebView2Controller(impl->hostHwnd, ctrlHandler);
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, wProfileDir.c_str(), nullptr, envHandler);
    if (FAILED(hr)) {
        m_Impl->failed = true;
        m_Impl->lastError = "CreateCoreWebView2EnvironmentWithOptions fallo (HRESULT 0x" +
            std::to_string((unsigned long)hr) + ").";
    }
}

void AIWebViewPanel::UpdateBounds(int screenX, int screenY, int width, int height, bool visible) {
    // Si la creacion async (ver NavigateTo) nunca resuelve -- ni exito
    // (ready=true) ni error real (failed=true) -- antes esto se quedaba
    // mostrando un rectangulo negro para siempre sin ningun mensaje ("el
    // panel de IA no muestra nada"/"queda una ventana en negro"). Un
    // timeout generoso (WebView2 normalmente arranca en menos de 1-2s, pero
    // un perfil nuevo o un disco lento pueden tardar mas) lo convierte en un
    // error real y visible en vez de un cuelgue silencioso.
    if (m_Impl->creating && !m_Impl->ready && !m_Impl->failed) {
        if (glfwGetTime() - m_Impl->creatingStartTime > 15.0) {
            m_Impl->failed = true;
            m_Impl->lastError = "El navegador embebido tardo demasiado en arrancar (WebView2). "
                                 "Probá cerrar y volver a abrir el Asistente de IA.";
        }
    }

    if (!m_Impl->hostHwnd) return;

    // Coordenadas de pantalla -> coordenadas de cliente del HWND padre
    // (SetWindowPos espera coordenadas relativas al padre cuando la
    // ventana es WS_CHILD).
    POINT topLeft{ screenX, screenY };
    ScreenToClient(m_Impl->parentHwnd, &topLeft);

    if (!visible || width <= 0 || height <= 0 || m_Impl->failed) {
        ShowWindow(m_Impl->hostHwnd, SW_HIDE);
        if (m_Impl->controller) m_Impl->controller->put_IsVisible(FALSE);
        return;
    }

    // FIX ("queda una ventana en negro"): antes esto pasaba SWP_NOZORDER
    // (deja el Z-order tal cual estaba). Si por lo que sea otra ventana
    // nativa hija del mismo padre (ej. un video en vivo de la Biblioteca,
    // ver BackgroundLayer::NativePlayback) se crea DESPUES de esta, esa
    // otra queda arriba y tapa al WebView2 aunque este bien posicionado y
    // realmente esté mostrando la pagina por debajo. HWND_TOP fuerza a que
    // este siempre quede arriba de sus hermanos mientras esta visible.
    SetWindowPos(m_Impl->hostHwnd, HWND_TOP, topLeft.x, topLeft.y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (m_Impl->controller) {
        RECT bounds{ 0, 0, width, height };
        m_Impl->controller->put_Bounds(bounds);
        m_Impl->controller->put_IsVisible(TRUE);
    }
}

bool AIWebViewPanel::IsAvailable() const { return !m_Impl->failed; }
bool AIWebViewPanel::HasError() const { return m_Impl->failed; }
std::string AIWebViewPanel::GetLastError() const { return m_Impl->lastError; }

void AIWebViewPanel::Reparent(void* newParentHwnd) {
    if (!newParentHwnd || !m_Impl->hostHwnd) return;
    HWND newParent = static_cast<HWND>(newParentHwnd);
    if (newParent == m_Impl->parentHwnd) return;

    SetParent(m_Impl->hostHwnd, newParent);
    m_Impl->parentHwnd = newParent;
    // El proximo UpdateBounds() recalcula posicion/tamaño relativos al
    // padre nuevo -- no hace falta tocar el controller de WebView2 aca, el
    // control sigue siendo el mismo hijo de hostHwnd, solo cambio DONDE
    // vive hostHwnd.
}

bool AIWebViewPanel::IsReady() const { return m_Impl->ready && m_Impl->webview.Get() != nullptr; }

} // namespace ProyecThor::UI

#else // !_WIN32

namespace ProyecThor::UI {

struct AIWebViewPanel::Impl {};
AIWebViewPanel::AIWebViewPanel() : m_Impl(nullptr) {}
AIWebViewPanel::~AIWebViewPanel() {}
void AIWebViewPanel::NavigateTo(const std::string&) {}
void AIWebViewPanel::UpdateBounds(int, int, int, int, bool) {}
bool AIWebViewPanel::IsAvailable() const { return false; }
bool AIWebViewPanel::HasError() const { return true; }
std::string AIWebViewPanel::GetLastError() const { return "WebView2 solo esta disponible en Windows."; }
bool AIWebViewPanel::IsReady() const { return false; }
void AIWebViewPanel::Reparent(void*) {}

} // namespace ProyecThor::UI

#endif
