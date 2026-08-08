#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "frontend/panels/stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#ifdef _WIN32
#include <dwmapi.h>
#endif
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>
#include <mutex>
#include <atomic>
#include "httplib.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#include <climits>
#endif
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "Version.h"
#include "SettingsManager.h"
#include "PresentationCore.h"
#include "PerformanceGovernor.h"
#include "SystemStats.h"
#include "ui/UIManager.h"
#include "frontend/panels/LibraryPanel.h"
#include "frontend/panels/biblio/LibraryMultimedia.h"
#include "frontend/panels/overlay/OverlayExportService.h"
#include "frontend/panels/HomePanel.h"
#include "frontend/ui/Hub.h"
#include "frontend/panels/ViewPanel.h"
#include "frontend/panels/StylesHubPanel.h"
#include "frontend/panels/StreamingWorkspacePanel.h"
#include "frontend/panels/biblio/LibraryHelpers.h"
#include "backend/core/AppPaths.h"
#include "SplashScreen.h"

#ifdef _WIN32
    #pragma comment(lib, "dwmapi.lib")
    #pragma comment(lib, "shell32.lib")
    #include <shellapi.h>
#endif

namespace {

constexpr int kSplashWBase = 600;
constexpr int kSplashHBase = 380;
constexpr int kMainWBase   = 1280;
constexpr int kMainHBase   = 720;

}

// La app carga TODOS sus assets (fuentes en "bin/assets/...", "proyecthor.png",
// los splash_bg*.png, shaders/, lua/, etc.) con rutas relativas al directorio
// de trabajo -- eso solo funciona si el cwd es la carpeta donde vive el .exe.
// El acceso directo de escritorio lo garantiza (WorkingDir="{app}" en el
// instalador, ver ProyecThor.iss), pero "Abrir con ProyecThor"/doble click
// sobre un archivo asociado NO fija ningun working directory (Explorer deja
// el que tenga a mano) -- resultado: la app arranca "pelada", sin fuentes ni
// imagenes, porque busca "bin/assets/..." en un directorio que no es el suyo.
// Fix: fijar el cwd a la carpeta del propio ejecutable ANTES de cargar nada,
// sin importar como se haya lanzado.
static void ChangeToExecutableDirectory()
{
#ifdef _WIN32
    wchar_t exePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return;
    std::filesystem::path dir = std::filesystem::path(exePath).parent_path();
    SetCurrentDirectoryW(dir.c_str());
#else
    char exePath[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len <= 0) return;
    exePath[len] = '\0';
    std::filesystem::path dir = std::filesystem::path(exePath).parent_path();
    if (chdir(dir.c_str()) != 0)
        std::cerr << "[DIAG] No se pudo cambiar el directorio de trabajo a " << dir << "\n";
#endif
}

std::string GetAppDataFilePath(const std::string& filename)
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) return filename;
    std::filesystem::path dirPath = std::filesystem::path(appData) / "ProyecThor";
#else
    const char* home = std::getenv("HOME");
    if (!home) return filename;
    std::filesystem::path dirPath = std::filesystem::path(home) / ".config" / "ProyecThor";
#endif

    if (!std::filesystem::exists(dirPath))
        std::filesystem::create_directories(dirPath);

    return (dirPath / filename).string();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apertura externa ("Abrir con ProyecThor" / doble click sobre un archivo
//  asociado, ver packaging/windows/ProyecThor.iss y packaging/*.desktop) --
//  Windows lanza el .exe con la ruta como argumento; los entornos de
//  escritorio Linux invocan "proyecthor %U" (ver Exec= en los .desktop).
// ─────────────────────────────────────────────────────────────────────────────

// argv de main() no es confiable para rutas con caracteres no-ASCII en
// Windows (queda en la codepage ANSI activa, no UTF-8) -- CommandLineToArgvW
// da la linea de comandos real en UTF-16, mismo criterio que el resto de la
// app usa para paths (ver ProyecThor::Library::Utf8ToWide/WideToUtf8).
std::string GetPendingOpenFilePath(int argc, char** argv)
{
#ifdef _WIN32
    (void)argc; (void)argv;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) return {};
    std::string result;
    if (wargc > 1)
        result = ProyecThor::Library::WideToUtf8(std::wstring(wargv[1]));
    LocalFree(wargv);
    return result;
#else
    if (argc <= 1) return {};
    std::string raw = argv[1];

    // Algunos gestores de archivos (Nautilus/GNOME) invocan "%U" con URIs
    // file:// en vez de rutas planas -- hay que sacar el prefijo y
    // decodificar el percent-encoding (espacios como %20, etc.).
    const std::string prefix = "file://";
    if (raw.rfind(prefix, 0) == 0) raw = raw.substr(prefix.size());

    std::string decoded;
    decoded.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '%' && i + 2 < raw.size() &&
            std::isxdigit((unsigned char)raw[i + 1]) && std::isxdigit((unsigned char)raw[i + 2])) {
            decoded += static_cast<char>(std::stoi(raw.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            decoded += raw[i];
        }
    }
    return decoded;
#endif
}

// Misma ruta que ProyecThor::Audio::GetAudioPath() (AudioHelpers.h) -- ese
// header no se incluye aca porque arrastra stb_image.h, y este archivo ya
// compila esa implementacion mas arriba (STB_IMAGE_IMPLEMENTATION); una
// segunda inclusion redefiniria todos sus simbolos. AppPaths.h si es
// liviano (sin stb_image/imgui/GL) y ya centraliza la carpeta de datos
// (respeta Ajustes > Actualizaciones > "Carpeta de datos"), asi que se
// delega ahi en vez de recalcular %APPDATA% por su cuenta.
const std::string& GetAudioLibraryPath()
{
    static std::string s_Path;
    if (s_Path.empty())
        s_Path = ProyecThor::GetAssetsPath() + "/audio";
    return s_Path;
}

enum class PendingMediaKind { None, Audio, Video };

PendingMediaKind ClassifyMediaExtension(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Mismas listas que usan los dialogos de importar de Audio (ver
    // AudioPanel::ImportAudioFile) y Biblioteca > Videos.
    static const std::vector<std::string> kAudioExts = {
        ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff"
    };
    static const std::vector<std::string> kVideoExts = {
        ".mp4", ".mkv", ".avi", ".mov", ".webm"
    };
    if (std::find(kAudioExts.begin(), kAudioExts.end(), ext) != kAudioExts.end()) return PendingMediaKind::Audio;
    if (std::find(kVideoExts.begin(), kVideoExts.end(), ext) != kVideoExts.end()) return PendingMediaKind::Video;
    return PendingMediaKind::None;
}

// Copia el archivo abierto externamente a la biblioteca correspondiente (audio
// o Biblioteca > Videos) y lo selecciona -- EXACTAMENTE lo mismo que hace un
// click manual sobre un item en Biblioteca (ver LibraryMultimedia.cpp
// SelectMMItem / LibraryVideos.cpp), asi que cae en el mismo camino ya
// probado: AudioPanel::Update()/MonitorView::Update() escuchan
// PresentationCore::PeekSelection() cada frame y lo cargan en Preview (audio:
// reproduccion local audible via AudioPanel::Play(); video: Preview mudo de
// MonitorView) -- nunca se manda solo al proyector real, el operador decide
// eso aparte con "Enviar en vivo".
void ImportAndPreviewExternalFile(const std::string& externalPath)
{
    PendingMediaKind kind = ClassifyMediaExtension(externalPath);
    if (kind == PendingMediaKind::None) {
        std::cerr << "[DIAG] Archivo abierto externamente con extension no soportada, se ignora: "
                  << externalPath << "\n";
        return;
    }

    try {
#ifdef _WIN32
        std::filesystem::path src{ProyecThor::Library::Utf8ToWide(externalPath)};
#else
        std::filesystem::path src{externalPath};
#endif
        if (!std::filesystem::exists(src)) {
            std::cerr << "[DIAG] Archivo pasado por linea de comandos no existe: " << externalPath << "\n";
            return;
        }

        const std::string destDirUtf8 = (kind == PendingMediaKind::Audio)
            ? GetAudioLibraryPath()
            : (ProyecThor::Library::GetAssetsPath() + "/videos");

#ifdef _WIN32
        std::filesystem::path destDir{ProyecThor::Library::Utf8ToWide(destDirUtf8)};
#else
        std::filesystem::path destDir{destDirUtf8};
#endif
        std::filesystem::create_directories(destDir);
        std::filesystem::path dest = destDir / src.filename();
        std::filesystem::copy(src, dest, std::filesystem::copy_options::overwrite_existing);

#ifdef _WIN32
        std::string filenameUtf8 = ProyecThor::Library::WideToUtf8(dest.filename().wstring());
#else
        std::string filenameUtf8 = dest.filename().string();
#endif

        ProyecThor::Core::LibrarySelection sel;
        sel.title = filenameUtf8;
        sel.type  = (kind == PendingMediaKind::Audio)
            ? ProyecThor::Core::ItemType::Audio
            : ProyecThor::Core::ItemType::Video;
        ProyecThor::Core::PresentationCore::Get().SetSelection(sel);

        std::cerr << "[DIAG] Archivo abierto externamente importado y cargado en preview: "
                  << filenameUtf8 << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[DIAG] Error importando archivo abierto externamente: " << e.what() << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Instancia unica -- pedido explicito: "Abrir con" sobre otro archivo (o
//  doble click en el .exe) mientras ProyecThor ya esta corriendo NUNCA debe
//  abrir una segunda ventana. Se resuelve con un servidor HTTP minimo en
//  loopback (127.0.0.1, mismo mecanismo cpp-httplib que ya usa SyncServer
//  para el companion movil, pero en un puerto propio y sin exponerse a la
//  LAN): la primera instancia escucha, cualquier lanzamiento posterior
//  intenta hablarle ANTES de tocar GLFW/ventanas -- si le contesta, esta
//  segunda "instancia" nunca llega a existir de verdad, solo reenvia y sale.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
constexpr int kSingleInstancePort = 51973; // arbitrario, solo loopback, no debe chocar con SyncServer (8080 default)

std::mutex               g_IncomingFileMutex;
std::queue<std::string>  g_IncomingFiles;
std::atomic<bool>        g_FocusRequested{false};

std::unique_ptr<httplib::Server> g_InstanceServer;
std::thread                      g_InstanceServerThread;
}

// Intenta reenviarle filePath (puede ser "", ver mas abajo) a una instancia
// YA corriendo. Devuelve true si alguien contesto -- en ese caso el
// llamador debe salir de inmediato, sin inicializar nada mas: esta
// "instancia" nunca llega a ser la real.
bool TryForwardToRunningInstance(const std::string& filePath)
{
    httplib::Client cli("127.0.0.1", kSingleInstancePort);
    cli.set_connection_timeout(0, 400000); // 400ms -- loopback, no hay razon para tardar mas si hay alguien
    cli.set_read_timeout(1, 0);
    auto res = cli.Post("/open-file", filePath, "text/plain");
    return res && res->status == 200;
}

// Arranca el lado "servidor" -- lo llama la instancia que SI llega a
// inicializar de verdad, una vez que ya tiene ventana. ProcessSingleInstanceRequests
// (llamado una vez por frame desde RunMainLoop) drena lo que vaya llegando.
void StartSingleInstanceListener()
{
    g_InstanceServer = std::make_unique<httplib::Server>();
    g_InstanceServer->Post("/open-file", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.body.empty()) {
            std::lock_guard<std::mutex> lk(g_IncomingFileMutex);
            g_IncomingFiles.push(req.body);
        }
        // Sin archivo (doble click en el .exe con la app ya abierta): igual
        // se pide foco, asi el operador ve que ProyecThor ya estaba
        // corriendo en vez de que "no pase nada" en apariencia.
        g_FocusRequested = true;
        res.status = 200;
    });
    g_InstanceServerThread = std::thread([]() {
        g_InstanceServer->listen("127.0.0.1", kSingleInstancePort);
    });
}

void StopSingleInstanceListener()
{
    if (g_InstanceServer) g_InstanceServer->stop();
    if (g_InstanceServerThread.joinable()) g_InstanceServerThread.join();
    g_InstanceServer.reset();
}

// Una vez por frame desde RunMainLoop: importa cualquier archivo que haya
// llegado de un lanzamiento posterior y trae la ventana al frente si se
// pidio foco.
void ProcessSingleInstanceRequests(GLFWwindow* window, ProyecThor::UI::UIManager& uiManager)
{
    std::string incoming;
    {
        std::lock_guard<std::mutex> lk(g_IncomingFileMutex);
        if (!g_IncomingFiles.empty()) {
            incoming = std::move(g_IncomingFiles.front());
            g_IncomingFiles.pop();
        }
    }
    if (!incoming.empty()) {
        ImportAndPreviewExternalFile(incoming);
        uiManager.EnterLibraryWorkspaceMode();
    }

    if (g_FocusRequested.exchange(false)) {
        glfwRestoreWindow(window);
        glfwShowWindow(window);
        glfwFocusWindow(window);
    }
}

GLuint LoadTextureFromFile(const char* filename)
{
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 4);
    if (!data)
    {
        std::cerr << "[DIAG] LoadTextureFromFile: no se pudo cargar '" << filename << "'\n";
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

#ifdef _WIN32
static void EnableDpiAwareness()
{
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32)
    {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx((HANDLE)(-4)))
        {
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }

    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore)
    {
        using SetAwarenessFn = HRESULT(WINAPI*)(int);
        auto setAwareness = (SetAwarenessFn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness && SUCCEEDED(setAwareness(2)))
        {
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    HMODULE user32b = LoadLibraryA("user32.dll");
    if (user32b)
    {
        using SetDpiAwareFn = BOOL(WINAPI*)();
        auto setDpiAware = (SetDpiAwareFn)GetProcAddress(user32b, "SetProcessDPIAware");
        if (setDpiAware) setDpiAware();
        FreeLibrary(user32b);
    }
}
#endif

namespace FrameProfiler
{
    static constexpr int kSampleFrames = 60;

    struct Accum
    {
        double totalMs = 0.0;
        int    count   = 0;
    };

    static Accum s_PollEvents;
    static Accum s_CoreUpdate;
    static Accum s_ImGuiBuild;
    static Accum s_ImGuiRender;
    static Accum s_PlatformWindows;
    static Accum s_SwapBuffers;
    static Accum s_FrameTotal;

    using Clock = std::chrono::steady_clock;

    static double ElapsedMs(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    static void Add(Accum& a, double ms)
    {
        a.totalMs += ms;
        a.count++;
    }

    static void ReportIfReady()
    {
        if (s_FrameTotal.count < kSampleFrames)
            return;

        auto avg = [](const Accum& a) { return a.totalMs / a.count; };
        double fps = 1000.0 / avg(s_FrameTotal);

        std::cout << "\n[PROFILE] Promedio ultimos " << kSampleFrames << " frames"
                  << " (fps estimado: " << fps << ")\n"
                  << "  PollEvents        : " << avg(s_PollEvents)       << " ms\n"
                  << "  core.Update()     : " << avg(s_CoreUpdate)       << " ms\n"
                  << "  ImGui build       : " << avg(s_ImGuiBuild)       << " ms\n"
                  << "  ImGui render(GL)  : " << avg(s_ImGuiRender)      << " ms\n"
                  << "  PlatformWindows   : " << avg(s_PlatformWindows)  << " ms\n"
                  << "  SwapBuffers       : " << avg(s_SwapBuffers)      << " ms\n"
                  << "  TOTAL frame       : " << avg(s_FrameTotal)       << " ms\n";

        s_PollEvents      = {};
        s_CoreUpdate      = {};
        s_ImGuiBuild      = {};
        s_ImGuiRender     = {};
        s_PlatformWindows = {};
        s_SwapBuffers     = {};
        s_FrameTotal      = {};
    }
}

namespace {

float DetectDpiScale()
{
    float scale = 1.0f;
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (primary)
    {
        float sx = 1.0f, sy = 1.0f;
        glfwGetMonitorContentScale(primary, &sx, &sy);
        if (sx > 0.0f) scale = sx;
    }
    std::cerr << "[DIAG] Escala de DPI detectada: " << (scale * 100.0f) << "%\n";
    return scale;
}

GLFWwindow* CreateSplashWindow(int splashW, int splashH, bool visible)
{
    glfwWindowHint(GLFW_DECORATED,             GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,              GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // Al abrir via "Abrir con"/archivo asociado se salta el splash visual
    // (ver skipSplash en main()) -- igual hace falta esta ventana/contexto
    // GL (CreateMainWindow comparte contexto con ella), asi que se crea
    // igual pero invisible en vez de saltearse del todo.
    glfwWindowHint(GLFW_VISIBLE,               visible ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(splashW, splashH, "ProyecThor", nullptr, nullptr);
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] FALLO: glfwCreateWindow(splash) devolvio nullptr. "
                  << "Codigo GLFW: " << code << " Descripcion: " << (desc ? desc : "N/A") << "\n";
        return nullptr;
    }
    std::cerr << "[DIAG] splashWindow creado OK\n";

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* vm = glfwGetVideoMode(primary);
    if (vm)
    {
        int mx = 0, my = 0;
        glfwGetMonitorPos(primary, &mx, &my);
        glfwSetWindowPos(window, mx + (vm->width - splashW) / 2, my + (vm->height - splashH) / 2);
    }
    return window;
}

ProyecThor::Splash::Fonts LoadSplashFonts(ImGuiIO& io, float dpiScale)
{
    using namespace ProyecThor::Settings;

    std::string fontPath;
    const std::string& customFontPath = SettingsManager::Get().GetSettings().theme.customFontPath;
    const char* defaultFontPath = "bin/assets/fonts/OpenSans-Regular.ttf";

    if (!customFontPath.empty() && IsValidFontFile(customFontPath))
        fontPath = customFontPath;
    else if (IsValidFontFile(defaultFontPath))
        fontPath = defaultFontPath;

    ProyecThor::Splash::Fonts fonts;
    if (!fontPath.empty())
    {
        fonts.title   = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 46.0f * dpiScale);
        fonts.regular = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 20.0f * dpiScale);
        fonts.small   = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f * dpiScale);
    }

    if (!fonts.title || !fonts.regular || !fonts.small)
        std::cerr << "[DIAG] ADVERTENCIA: no se pudo cargar la fuente en '" << fontPath
                  << "'. Verifica que el binario se ejecute desde el directorio correcto "
                  << "(donde existe bin/assets/...).\n";

    return fonts;
}

void LoadUIIcons()
{
    StyleGeneralApp::LoadAppIcon("search",            "bin/assets/icons/ui/searchico.png");
    StyleGeneralApp::LoadAppIcon("izquierda",         "bin/assets/icons/ui/izquierda.png");
    StyleGeneralApp::LoadAppIcon("editar",            "bin/assets/icons/ui/editar.png");
    StyleGeneralApp::LoadAppIcon("play",              "bin/assets/icons/ui/play.png");
    StyleGeneralApp::LoadAppIcon("pause",             "bin/assets/icons/ui/pause.png");
    StyleGeneralApp::LoadAppIcon("add",               "bin/assets/icons/ui/add.png");
    StyleGeneralApp::LoadAppIcon("delete",            "bin/assets/icons/ui/delete.png");
    StyleGeneralApp::LoadAppIcon("skip_next",         "bin/assets/icons/ui/skip_next.png");
    StyleGeneralApp::LoadAppIcon("skip_prev",         "bin/assets/icons/ui/skip_previous.png");
    StyleGeneralApp::LoadAppIcon("forward_10",        "bin/assets/icons/ui/forward_10.png");
    StyleGeneralApp::LoadAppIcon("replay_10",         "bin/assets/icons/ui/replay_10.png");
    StyleGeneralApp::LoadAppIcon("volume_up",         "bin/assets/icons/ui/volume_up.png");
    StyleGeneralApp::LoadAppIcon("no_sound",          "bin/assets/icons/ui/no_sound.png");
    StyleGeneralApp::LoadAppIcon("favorite",          "bin/assets/icons/ui/favorite.png");
    StyleGeneralApp::LoadAppIcon("fit_screen",        "bin/assets/icons/ui/fit_screen.png");
    StyleGeneralApp::LoadAppIcon("arrow_forward",     "bin/assets/icons/ui/arrow_forward.png");
    StyleGeneralApp::LoadAppIcon("arrow_back",        "bin/assets/icons/ui/arrow_back.png");
    StyleGeneralApp::LoadAppIcon("repeat",            "bin/assets/icons/ui/repeat.png");
    StyleGeneralApp::LoadAppIcon("repeat_one",        "bin/assets/icons/ui/repeat_one.png");
    StyleGeneralApp::LoadAppIcon("stop",              "bin/assets/icons/ui/stop.png");
    StyleGeneralApp::LoadAppIcon("motion_play",       "bin/assets/icons/ui/motion_play.png");
    StyleGeneralApp::LoadAppIcon("cleaning_services", "bin/assets/icons/ui/cleaning_services.png");
    StyleGeneralApp::LoadAppIcon("add_to_queue",      "bin/assets/icons/ui/add_to_queue.png");
    StyleGeneralApp::LoadAppIcon("original_screen",   "bin/assets/icons/ui/original_screen.png");
    StyleGeneralApp::LoadAppIcon("add_photo",         "bin/assets/icons/ui/add_photo.png");
    StyleGeneralApp::LoadAppIcon("upload_file",       "bin/assets/icons/ui/upload_file.png");
    StyleGeneralApp::LoadAppIcon("history",           "bin/assets/icons/ui/history.png");
    StyleGeneralApp::LoadAppIcon("cards_star",        "bin/assets/icons/ui/cards_star.png");
}

GLFWwindow* CreateMainWindow(GLFWwindow* splashWindow, int mainW, int mainH)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE,             GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE,               GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(mainW, mainH, "ProyecThor", nullptr, splashWindow);
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] glfwCreateWindow(main, GL 3.3) fallo. Codigo GLFW: " << code
                  << " Descripcion: " << (desc ? desc : "N/A") << ". Reintentando con GL 3.0...\n";

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(mainW, mainH, "ProyecThor", nullptr, splashWindow);
    }
    if (!window)
    {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        std::cerr << "[DIAG] FALLO DEFINITIVO: glfwCreateWindow(main) devolvio nullptr. "
                  << "Codigo GLFW: " << code << " Descripcion: " << (desc ? desc : "N/A") << "\n";
        return nullptr;
    }
    std::cerr << "[DIAG] mainWindow creado OK\n";

#ifdef _WIN32
    {
        HWND hwnd = glfwGetWin32Window(window);
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    }
#endif
    {
        GLFWimage images[1];
        images[0].pixels = stbi_load("proyecthor.png", &images[0].width, &images[0].height, 0, 4);
        if (images[0].pixels)
        {
            glfwSetWindowIcon(window, 1, images);
            stbi_image_free(images[0].pixels);
        }
        else
        {
            std::cerr << "[DIAG] ADVERTENCIA: no se pudo cargar 'proyecthor.png' para el icono de ventana.\n";
        }
    }

    glfwMakeContextCurrent(window);
    ProyecThor::Core::PresentationCore::Get().SetMainWindow(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (status != GLEW_OK)
        std::cerr << "[DIAG] ADVERTENCIA: glewInit() para mainWindow devolvio error: "
                  << glewGetErrorString(status) << "\n";

    return window;
}

void InstallViewportRenderHook()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_OrigCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;

    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport)
    {
        s_OrigCreateWindow(viewport);
        GLFWwindow* w = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (w)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            glfwMakeContextCurrent(w);
            glfwSwapInterval(0);
            glfwMakeContextCurrent(backup);
        }
    };

    static void (*s_OrigRenderWindow)(ImGuiViewport*, void*) = platform_io.Renderer_RenderWindow;

    platform_io.Renderer_RenderWindow = [](ImGuiViewport* viewport, void* renderArg)
    {
        auto& core = ProyecThor::Core::PresentationCore::Get();
        if (core.IsProjectorPostFXViewport(viewport->ID))
            core.RenderProjectorViewportPostFX(viewport, s_OrigRenderWindow);
        else if (core.IsExtraProjectorViewport(viewport->ID))
            core.RenderExtraProjectorViewportPostFX(viewport->ID, viewport, s_OrigRenderWindow);
        else if (s_OrigRenderWindow)
            s_OrigRenderWindow(viewport, renderArg);
    };
}

void RunMainLoop(GLFWwindow* window, ProyecThor::UI::UIManager& uiManager,
                  const ProyecThor::Settings::ThemeSettings& theme)
{
    while (!glfwWindowShouldClose(window))
    {
        using Clock = FrameProfiler::Clock;
        auto frameStart = Clock::now();

        auto t0 = Clock::now();
        glfwPollEvents();
        FrameProfiler::Add(FrameProfiler::s_PollEvents, FrameProfiler::ElapsedMs(t0));

        auto& core = ProyecThor::Core::PresentationCore::Get();

        // Instancia unica: drena cualquier archivo/pedido de foco que haya
        // mandado un lanzamiento posterior de "Abrir con" (ver
        // StartSingleInstanceListener/TryForwardToRunningInstance).
        ProcessSingleInstanceRequests(window, uiManager);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            core.SetProjecting(false);
            core.ClearLayer2();
        }

        if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        auto t1 = Clock::now();
        core.Update();
        FrameProfiler::Add(FrameProfiler::s_CoreUpdate, FrameProfiler::ElapsedMs(t1));
        core.RenderAllSecondaryWindows();

        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);
        if (fw == 0 || fh == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        uiManager.GetGlassRenderer().Resize(fw, fh);
        glViewport(0, 0, fw, fh);
        glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto t2 = Clock::now();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        uiManager.RenderAll();

        ImGui::Render();
        FrameProfiler::Add(FrameProfiler::s_ImGuiBuild, FrameProfiler::ElapsedMs(t2));

        ProyecThor::UI::OverlayExportService::Get().ProcessPending();

        auto t3 = Clock::now();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        FrameProfiler::Add(FrameProfiler::s_ImGuiRender, FrameProfiler::ElapsedMs(t3));

        uiManager.GetGlassRenderer().CaptureCurrentFrame();
        uiManager.GetGlassRenderer().Blur(1.0f, 1);

        auto t4 = Clock::now();
        {
            GLFWwindow* ctxBackup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctxBackup);
        }
        FrameProfiler::Add(FrameProfiler::s_PlatformWindows, FrameProfiler::ElapsedMs(t4));

        auto t5 = Clock::now();
        glfwSwapBuffers(window);
        FrameProfiler::Add(FrameProfiler::s_SwapBuffers, FrameProfiler::ElapsedMs(t5));

        double frameTotalMs = FrameProfiler::ElapsedMs(frameStart);
        FrameProfiler::Add(FrameProfiler::s_FrameTotal, frameTotalMs);
        FrameProfiler::ReportIfReady();

        ProyecThor::Core::PerformanceGovernor::Get().ReportFrame(frameTotalMs);
        ProyecThor::Core::SystemStats::Get().Update();
    }
}

}

int main(int argc, char** argv)
{
    // PRIMERO que nada -- todo lo que sigue (splash, fuentes, iconos, etc.)
    // carga assets con rutas relativas y asume que el cwd es la carpeta del
    // .exe (ver comentario en ChangeToExecutableDirectory).
    ChangeToExecutableDirectory();

    std::cerr << "[DIAG] Iniciando main()\n";

    // Capturado ACA (antes de que glfwInit/etc. puedan tocar el estado del
    // proceso) pero recien despachado mas abajo, una vez que UIManager y sus
    // paneles ya existen -- ver ImportAndPreviewExternalFile.
    const std::string pendingOpenFilePath = GetPendingOpenFilePath(argc, argv);

    // Instancia unica -- pedido explicito: si ya hay una ProyecThor
    // corriendo, le mandamos el archivo (si hay) y salimos ACA MISMO, antes
    // de tocar GLFW/splash/ventanas. Esta "instancia" nunca llega a existir
    // de verdad.
    if (TryForwardToRunningInstance(pendingOpenFilePath))
    {
        std::cerr << "[DIAG] Ya habia una instancia de ProyecThor corriendo -- se le mando "
                     "el pedido y esta instancia nueva no continua.\n";
        return 0;
    }

#ifdef _WIN32
    EnableDpiAwareness();
#endif

#ifndef _WIN32
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    std::cerr << "[DIAG] Forzando backend GLFW a X11/XWayland (necesario para GLEW)\n";
#endif

    if (!glfwInit())
    {
        std::cerr << "[DIAG] FALLO: glfwInit() devolvio false\n";
        return -1;
    }
    std::cerr << "[DIAG] glfwInit() OK\n";

    const float dpiScale = DetectDpiScale();
    const int splashW = (int)(kSplashWBase * dpiScale);
    const int splashH = (int)(kSplashHBase * dpiScale);
    const int mainW   = (int)(kMainWBase   * dpiScale);
    const int mainH   = (int)(kMainHBase   * dpiScale);

    ProyecThor::Settings::SettingsManager::Get().LoadSettings();
    std::cerr << "[DIAG] SettingsManager::LoadSettings() OK\n";
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    // Abierto via "Abrir con"/archivo asociado -- el operador quiere ver ESE
    // archivo lo antes posible, no el splash con su arte/creditos. La
    // ventana de splash igual se crea (comparte contexto GL con
    // CreateMainWindow mas abajo) pero invisible, y los pasos de carga
    // corren directo sin dibujar ningun frame del splash.
    const bool skipSplash = !pendingOpenFilePath.empty();

    GLFWwindow* splashWindow = CreateSplashWindow(splashW, splashH, !skipSplash);
    if (!splashWindow)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(splashWindow);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK)
    {
        std::cerr << "[DIAG] FALLO: glewInit() devolvio error: " << glewGetErrorString(glewStatus) << "\n";
        glfwTerminate();
        return -1;
    }
    std::cerr << "[DIAG] glewInit() OK. Version OpenGL: " << (const char*)glGetString(GL_VERSION) << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& splashIO = ImGui::GetIO();
    splashIO.IniFilename = nullptr;

    ProyecThor::Splash::Fonts splashFonts = LoadSplashFonts(splashIO, dpiScale);

    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);
    std::cerr << "[DIAG] ImGui inicializado para splashWindow OK\n";

    const ProyecThor::Splash::Art splashArt = ProyecThor::Splash::PickArt();
    const std::string creditText = "Illustration by: " + splashArt.author;

    // Con skipSplash no se dibuja NINGUN frame del splash (ver loop de pasos
    // y "ultimo frame" mas abajo) -- no tiene sentido gastar tiempo/GPU
    // subiendo estas texturas para nada.
    GLuint logoTex = skipSplash ? 0 : LoadTextureFromFile("proyecthor.png");
    GLuint bgTex   = skipSplash ? 0 : LoadTextureFromFile(splashArt.filename.c_str());

    GLFWwindow* mainWindow = nullptr;
    const ImVec2 splashSize((float)splashW, (float)splashH);

    const std::vector<ProyecThor::Splash::Step> steps = {
        { "Inicializando motor grafico OpenGL...", [&](){
            mainWindow = CreateMainWindow(splashWindow, mainW, mainH);
        }},

        { "Cargando iconos y recursos graficos...", [&](){
            if (!mainWindow) return;
            glfwMakeContextCurrent(mainWindow);
            LoadUIIcons();
        }},

        { "Escaneando biblioteca de video, audio e imagenes...", [](){
            ProyecThor::Library::RefreshMultimediaLists();
        }},
    };

    constexpr int kFinalStep = 1;
    const int     kTotalSteps = (int)steps.size() + kFinalStep;

    for (int i = 0; i < (int)steps.size(); ++i) {
        if (skipSplash)
            steps[i].task(); // mismo trabajo, sin el frame de progreso dibujado
        else
            ProyecThor::Splash::RunStep(steps[i], i, kTotalSteps, splashWindow, splashSize,
                logoTex, bgTex, splashFonts, creditText, theme);
    }

    if (!mainWindow)
    {
        std::cerr << "[DIAG] FALLO: mainWindow sigue siendo nullptr despues del loop de carga. "
                  << "Revisa los mensajes [DIAG] anteriores para ver donde fallo la creacion "
                  << "de la ventana principal.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }

    // Ultimo frame del splash, mostrado ANTES de armar la interfaz principal
    // (UIManager + paneles) -- ese armado es sincronico y no puede volver a
    // dibujar el splash (destruye su contexto de ImGui mas abajo), asi que
    // sin este frame el splash quedaba "congelado" en el mensaje anterior
    // durante ese tramo, dando la sensacion de una traba invisible. Con
    // skipSplash la ventana ya es invisible y nunca se mostro nada -- no
    // hace falta este frame tampoco.
    if (!skipSplash) {
        glfwMakeContextCurrent(splashWindow);
        ProyecThor::Splash::Render(splashWindow, splashSize, "Preparando interfaz y paneles...", 1.0f,
            logoTex, bgTex, splashFonts, creditText, theme);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (logoTex != 0) glDeleteTextures(1, &logoTex);
    if (bgTex   != 0) glDeleteTextures(1, &bgTex);

    glfwMakeContextCurrent(mainWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = "proyecthor_ui.ini";

    {
        using namespace ProyecThor::Settings;
        const std::string& customFontPath = SettingsManager::Get().GetSettings().theme.customFontPath;
        const char* defaultFontPath = "bin/assets/fonts/OpenSans-Regular.ttf";

        std::string fontToLoad;
        if (!customFontPath.empty() && IsValidFontFile(customFontPath))
            fontToLoad = customFontPath;
        else if (IsValidFontFile(defaultFontPath))
            fontToLoad = defaultFontPath;

        if (!fontToLoad.empty())
            io.Fonts->AddFontFromFileTTF(fontToLoad.c_str(), 16.0f * dpiScale);
    }
    ProyecThor::Core::PresentationCore::Get().LoadFontsIntoImGui();

    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);

    InstallViewportRenderHook();

    ProyecThor::Settings::SettingsManager::Get().ApplyTheme();

    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    {
        int monitorCount = 0;
        glfwGetMonitors(&monitorCount);
        auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
        if (settings.projection.targetMonitor == -1)
            settings.projection.targetMonitor = (monitorCount > 1) ? 1 : 0;
    }

    std::cerr << "[DIAG] Antes de uiManager.Initialize()\n";
    ProyecThor::UI::UIManager uiManager;
    if (!uiManager.Initialize(mainWindow))
    {
        std::cerr << "[DIAG] FALLO: uiManager.Initialize(mainWindow) devolvio false\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(mainWindow);
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }
    std::cerr << "[DIAG] uiManager.Initialize() OK\n";

    ProyecThor::Settings::SettingsManager::Get().ApplyProjection();

    auto homePanel    = std::make_shared<ProyecThor::UI::HomePanel>();
    auto libraryPanel = std::make_shared<ProyecThor::UI::LibraryPanel>();
    homePanel->SetAudioPanel(libraryPanel->GetAudioPanel());
    ProyecThor::Core::PresentationCore::Get().SetAudioPanelRef(libraryPanel->GetAudioPanel());
    homePanel->m_UIManagerRef = &uiManager;
    libraryPanel->SetUIManager(&uiManager);
    uiManager.SetLibraryPanelRef(libraryPanel.get());

    uiManager.AddPanel(libraryPanel);
    uiManager.AddPanel(homePanel);

    auto viewPanel = std::make_shared<ProyecThor::UI::ViewPanel>(&uiManager);
    viewPanel->SetTeamChatPanelRef(&uiManager.GetChatPanel());
    uiManager.AddPanel(viewPanel);

    auto stylesHub = std::make_shared<ProyecThor::UI::StylesHubPanel>(&uiManager);
    stylesHub->SetTransitionPanel(uiManager.GetTransitionPanelOwned().get());
    uiManager.AddPanel(stylesHub);

    // Preset "Transmisión" (ver UIManager::BuildWorkspaceLayoutBroadcast):
    // misma instancia de BroadcastPanel que ya usa Ajustes > Conexiones, asi
    // que activar/mirar el streaming desde cualquiera de los dos lados
    // queda sincronizado solo.
    auto streamingWs = std::make_shared<ProyecThor::UI::StreamingWorkspacePanel>(&uiManager.GetBroadcastPanel());
    streamingWs->SetUIManager(&uiManager);
    uiManager.AddPanel(streamingWs);

    std::cerr << "[DIAG] Todos los paneles agregados OK\n";

    // "Abrir con ProyecThor" / doble click sobre un archivo asociado -- recien
    // aca, con AudioPanelRef ya registrado y la biblioteca ya escaneada
    // (ver step del splash "Escaneando biblioteca..."), asi que ya existe
    // todo lo que ImportAndPreviewExternalFile necesita tocar. Ademas, en
    // vez de arrancar en el Hub, se salta directo al workspace "Media y
    // Preview" (Biblioteca > Medios + Home) con el archivo ya cargado ahi
    // -- pedido explicito.
    if (!pendingOpenFilePath.empty())
    {
        ImportAndPreviewExternalFile(pendingOpenFilePath);
        uiManager.EnterLibraryWorkspaceMode();
    }

    {
        int fw, fh;
        glfwGetFramebufferSize(mainWindow, &fw, &fh);
        if (fw > 0 && fh > 0)
        {
            glViewport(0, 0, fw, fh);
            glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            uiManager.RenderAll();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            uiManager.GetGlassRenderer().CaptureCurrentFrame();
            uiManager.GetGlassRenderer().Blur(1.0f, 1);

            {
                GLFWwindow* ctxBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(ctxBackup);
            }

            glfwSwapBuffers(mainWindow);
        }
    }

    glfwDestroyWindow(splashWindow);
    splashWindow = nullptr;

    glfwShowWindow(mainWindow);
    glfwFocusWindow(mainWindow);

    // Recien aca: esta es la instancia real (la unica que llega tan lejos,
    // ver TryForwardToRunningInstance mas arriba) -- empieza a escuchar por
    // si otro lanzamiento de "Abrir con" llega despues.
    StartSingleInstanceListener();

    std::cerr << "[DIAG] Entrando al loop principal\n";
    RunMainLoop(mainWindow, uiManager, theme);

    std::cerr << "[DIAG] Saliendo del loop principal, cerrando limpio\n";

    StopSingleInstanceListener();

    uiManager.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
        ProyecThor::Settings::RestartApplication();

    return 0;
}
