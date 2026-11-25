#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <csignal>
#include <exception>
#include <sstream>
#pragma comment(lib, "dbghelp.lib")
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
#include "ui/framework/bin/StyleGeneralApp.h"
#include "Version.h"
#include "core/AppLog.h"
#include "SettingsManager.h"
#include "PresentationCore.h"
#include "core/MediaImportManager.h"
#include "core/MemoryOptimizer.h"
#include "PerformanceGovernor.h"
#include "SystemStats.h"
#include "ui/framework/UIManager.h"
#include "ui/panels/LibraryPanel.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include "core/io/BibleXmlIO.h"
#include "core/MediaCollections.h"
#include "ui/panels/overlay/OverlayExportService.h"
#include "media/audio/WindowsAudioSession.h"
#include "ui/views/Audio.h"
#include "ui/panels/HomePanel.h"
#include "ui/panels/ViewPanel.h"
#include "ui/panels/StylesHubPanel.h"
#include "ui/panels/StreamingWorkspacePanel.h"
#include "ui/panels/VideoEditorPanel.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/overlay/OverlayLibraryTab.h"
#include "core/ThumbnailWorker.h"
#include "core/AppPaths.h"
#include "core/plugins/PluginManager.h"
#include "SplashScreen.h"

#ifdef _WIN32
    #pragma comment(lib, "dwmapi.lib")
    #pragma comment(lib, "gdi32.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "shell32.lib")
    #include <shellapi.h>

static void WriteCrashReport(const char* reason, const std::string& callstack) {
    try {
        std::ofstream f("crash_log.txt", std::ios::app);
        if (f.is_open()) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            f << "==================== CRASH REPORT ====================\n";
            f << "Reason: " << reason << "\n";
            f << "Time: " << std::ctime(&now);
            f << callstack << "\n";
            f << "======================================================\n\n";
            f.flush();
        }
    } catch (...) {}
    std::string msg = std::string("ProyecThor ha detectado un fallo critico:\n\nMotivo: ") + reason + "\n\n" + callstack;
    MessageBoxA(NULL, msg.c_str(), "ProyecThor Error Diagnostic", MB_OK | MB_ICONERROR);
}

static std::string CaptureCurrentCallstack() {
    std::ostringstream ss;
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, NULL, TRUE);

    void* stack[64];
    WORD count = CaptureStackBackTrace(0, 64, stack, NULL);

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {};
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    IMAGEHLP_LINE64 line;
    ZeroMemory(&line, sizeof(line));
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    ss << "Pila de llamadas (" << count << " frames):\n";
    for (WORD i = 0; i < count; ++i) {
        DWORD64 addr = (DWORD64)(stack[i]);
        if (SymFromAddr(process, addr, 0, symbol)) {
            DWORD disp = 0;
            if (SymGetLineFromAddr64(process, addr, &disp, &line)) {
                ss << "  [" << i << "] " << symbol->Name << " -> " << line.FileName << ":" << line.LineNumber << "\n";
            } else {
                ss << "  [" << i << "] " << symbol->Name << " (0x" << std::hex << addr << std::dec << ")\n";
            }
        } else {
            ss << "  [" << i << "] 0x" << std::hex << addr << std::dec << "\n";
        }
    }
    return ss.str();
}

static LONG WINAPI GlobalUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    std::ostringstream ss;
    ss << "Excepcion SEH codigo: 0x" << std::hex << (ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0) << std::dec << "\n";
    ss << CaptureCurrentCallstack();
    WriteCrashReport("Unhandled SEH Exception", ss.str());
    return EXCEPTION_CONTINUE_SEARCH;
}

static void GlobalTerminateHandler() {
    std::ostringstream ss;
    ss << "std::terminate() invocado.\n";
    if (auto ep = std::current_exception()) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            ss << "Excepcion std::exception: " << e.what() << "\nTipo: " << typeid(e).name() << "\n";
        } catch (...) {
            ss << "Excepcion C++ no estandar atrapada.\n";
        }
    } else {
        ss << "Sin excepcion C++ activa (posible abort() directo o thread destruido sin join/detach).\n";
    }
    ss << CaptureCurrentCallstack();
    WriteCrashReport("std::terminate", ss.str());
    _exit(1);
}

static void GlobalAbortHandler(int) {
    std::ostringstream ss;
    ss << "Senal SIGABRT capturada.\n";
    ss << CaptureCurrentCallstack();
    WriteCrashReport("SIGABRT", ss.str());
    _exit(2);
}

static void GlobalInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t) {
    std::ostringstream ss;
    ss << "Parametro invalido de CRT detectado.\n";
    if (expression) {
        char buf[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, expression, -1, buf, sizeof(buf), NULL, NULL);
        ss << "Expresion: " << buf << "\n";
    }
    if (file) {
        char buf[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, file, -1, buf, sizeof(buf), NULL, NULL);
        ss << "Archivo: " << buf << ":" << line << "\n";
    }
    ss << CaptureCurrentCallstack();
    WriteCrashReport("CRT Invalid Parameter", ss.str());
    _exit(3);
}

static int GlobalCrtReportHookW(int reportType, wchar_t* message, int* returnValue) {
    std::ostringstream ss;
    ss << "Informe de depuracion CRT (tipo=" << reportType << "): ";
    if (message) {
        char buf[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, message, -1, buf, sizeof(buf), NULL, NULL);
        ss << buf;
    }
    ss << "\n" << CaptureCurrentCallstack();
    WriteCrashReport("CRT Debug Report (Hook)", ss.str());
    if (returnValue) *returnValue = 0;
    return FALSE;
}

static void InstallCrashDiagnostics() {
    SetUnhandledExceptionFilter(GlobalUnhandledExceptionFilter);
    std::set_terminate(GlobalTerminateHandler);
    signal(SIGABRT, GlobalAbortHandler);
    _set_invalid_parameter_handler(GlobalInvalidParameterHandler);
    _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, GlobalCrtReportHookW);
}
#endif

namespace {

constexpr int kSplashWBase = 600;
constexpr int kSplashHBase = 380;
constexpr int kMainWBase   = 1280;
constexpr int kMainHBase   = 720;
}

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

#ifdef _WIN32
#pragma comment(lib, "advapi32.lib")

static void RegisterWindowsShellAssociation()
{
    wchar_t exePath[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    std::wstring exeStr(exePath);
    std::wstring openCmd = L"\"" + exeStr + L"\" \"%1\"";
    std::wstring iconCmd = exeStr + L",0";

    auto setRegString = [](HKEY root, const std::wstring& subkey, const wchar_t* valName, const std::wstring& valData) {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, valName, 0, REG_SZ, reinterpret_cast<const BYTE*>(valData.c_str()), static_cast<DWORD>((valData.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    };

    // 1. Registro en Applications de Windows para "Abrir con..."
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\ProyecThor.exe", L"FriendlyAppName", L"ProyecThor");
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\ProyecThor.exe\\DefaultIcon", nullptr, iconCmd);
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\ProyecThor.exe\\shell\\open\\command", nullptr, openCmd);

    static const std::vector<std::wstring> kSupportedExts = {
        L".mp3", L".wav", L".flac", L".ogg", L".aac", L".m4a", L".wma", L".opus", L".aiff",
        L".mp4", L".mkv", L".avi", L".mov", L".webm",
        L".jpg", L".jpeg", L".png", L".bmp", L".webp", L".gif",
        L".txt", L".prosong"
    };

    for (const auto& ext : kSupportedExts) {
        setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\ProyecThor.exe\\SupportedTypes", ext.c_str(), L"");
        setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\" + ext + L"\\OpenWithProgids", L"ProyecThor.exe", L"");
    }

    // 2. Menu contextual click derecho directo en Windows Explorer: "Abrir con ProyecThor"
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\ProyecThor", nullptr, L"Abrir con ProyecThor");
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\ProyecThor", L"Icon", iconCmd);
    setRegString(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\ProyecThor\\command", nullptr, openCmd);

    // 3. SystemFileAssociations para categorias de audio, video e imagen
    static const wchar_t* kSysCats[] = { L"audio", L"video", L"image" };
    for (const auto* cat : kSysCats) {
        std::wstring base = std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + cat + L"\\shell\\ProyecThor";
        setRegString(HKEY_CURRENT_USER, base, nullptr, L"Abrir con ProyecThor");
        setRegString(HKEY_CURRENT_USER, base, L"Icon", iconCmd);
        setRegString(HKEY_CURRENT_USER, base + L"\\command", nullptr, openCmd);
    }
}
#endif

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

const std::string& GetAudioLibraryPath()
{
    static std::string s_Path;
    if (s_Path.empty())
        s_Path = ProyecThor::GetAssetsPath() + "/audio";
    return s_Path;
}

enum class PendingMediaKind { None, Audio, Video, Image, Song };

PendingMediaKind ClassifyMediaExtension(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::vector<std::string> kAudioExts = {
        ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff"
    };
    static const std::vector<std::string> kVideoExts = {
        ".mp4", ".mkv", ".avi", ".mov", ".webm"
    };
    static const std::vector<std::string> kImageExts = {
        ".jpg", ".jpeg", ".png", ".bmp", ".webp", ".gif"
    };
    static const std::vector<std::string> kSongExts = {
        ".txt", ".prosong"
    };
    if (std::find(kAudioExts.begin(), kAudioExts.end(), ext) != kAudioExts.end()) return PendingMediaKind::Audio;
    if (std::find(kVideoExts.begin(), kVideoExts.end(), ext) != kVideoExts.end()) return PendingMediaKind::Video;
    if (std::find(kImageExts.begin(), kImageExts.end(), ext) != kImageExts.end()) return PendingMediaKind::Image;
    if (std::find(kSongExts.begin(), kSongExts.end(), ext) != kSongExts.end()) return PendingMediaKind::Song;
    return PendingMediaKind::None;
}

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

        auto& core = ProyecThor::Core::PresentationCore::Get();
        // Abrir externamente como reproductor local: NUNCA proyectar en vivo directamente al monitor
        core.SetProjecting(false);

        if (kind == PendingMediaKind::Audio || kind == PendingMediaKind::Video || kind == PendingMediaKind::Image) {
            // Vincular directamente en su carpeta original (ej. Descargas) sin copiar el archivo
            // a disco ni duplicar contenidos.
            ProyecThor::Core::MediaImportManager::Get().LinkSingleExternalFileDirect(
                externalPath, ProyecThor::Core::MediaImportDest::Multimedia);
            ProyecThor::Library::RefreshMultimediaLists();
        }

        ProyecThor::Core::LibrarySelection sel;
        sel.title = externalPath;

        if (kind == PendingMediaKind::Audio) {
            sel.type  = ProyecThor::Core::ItemType::Audio;
            sel.contentData = { externalPath };
            core.SetSelection(sel);

            // Iniciar reproduccion local en el reproductor de musica sin proyectar
            if (auto* ap = ProyecThor::UI::AudioPanel::GetActiveInstance()) {
                ap->PlayFile(externalPath, /*live=*/false);
            }
        } else if (kind == PendingMediaKind::Video) {
            sel.type  = ProyecThor::Core::ItemType::Video;
            sel.contentData = { externalPath };
            core.SetSelection(sel);

            // Iniciar reproduccion local en el reproductor de video (con audio para escuchar/ver como reproductor)
            core.RequestPreviewLoad(externalPath, /*loop=*/false, /*startMuted=*/false);
        } else if (kind == PendingMediaKind::Image) {
            sel.type  = ProyecThor::Core::ItemType::Image;
            sel.contentData = { externalPath };
            core.SetSelection(sel);
            core.RequestPreviewStop();
        } else if (kind == PendingMediaKind::Song) {
            sel.type = ProyecThor::Core::ItemType::Song;
            std::ifstream f(src);
            if (f.is_open()) {
                std::string line;
                std::string stanza;
                while (std::getline(f, line)) {
                    if (line.empty() || line == "\r") {
                        if (!stanza.empty()) {
                            sel.contentData.push_back(stanza);
                            stanza.clear();
                        }
                    } else {
                        if (!stanza.empty()) stanza += "\n";
                        stanza += line;
                    }
                }
                if (!stanza.empty()) sel.contentData.push_back(stanza);
            }
            core.SetSelection(sel);
        }

        std::cerr << "[DIAG] Archivo abierto externamente vinculado y cargado en reproductor (sin proyectar de una): "
                  << externalPath << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[DIAG] Error importando archivo abierto externamente: " << e.what() << "\n";
    }
}

namespace {
constexpr int kSingleInstancePort = 51973; // arbitrario, solo loopback, no debe chocar con SyncServer (8080 default)

std::mutex               g_IncomingFileMutex;
std::queue<std::string>  g_IncomingFiles;
std::atomic<bool>        g_FocusRequested{false};
std::atomic<bool>        g_ReloadThemeRequested{false};

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
    g_InstanceServer->Post("/hot-reload", [](const httplib::Request&, httplib::Response& res) {
        ProyecThor::Settings::SettingsManager::Get().RequestRestart();
        res.status = 200;
        res.set_content("ok", "text/plain");
    });
    g_InstanceServer->Post("/reload-theme", [](const httplib::Request&, httplib::Response& res) {
        g_ReloadThemeRequested = true;
        res.status = 200;
        res.set_content("ok", "text/plain");
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

    if (g_ReloadThemeRequested.exchange(false)) {
        ProyecThor::Settings::SettingsManager::Get().LoadSettings();
        ProyecThor::Settings::SettingsManager::Get().ApplyTheme();
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

#ifdef _WIN32
void ApplyWin32DarkBackground(GLFWwindow* window, const float baseColor[4])
{
    if (!window) return;
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return;

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDarkMode, sizeof(useDarkMode));

    auto toByte = [](float v) -> BYTE {
        v = std::clamp(v, 0.0f, 1.0f);
        return static_cast<BYTE>(v * 255.0f + 0.5f);
    };
    static HBRUSH s_DarkBrush = nullptr;
    if (!s_DarkBrush)
        s_DarkBrush = CreateSolidBrush(RGB(toByte(baseColor[0]), toByte(baseColor[1]), toByte(baseColor[2])));
    if (s_DarkBrush)
        SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(s_DarkBrush));
}
#endif

GLFWwindow* CreateSplashWindow(int splashW, int splashH, bool visible, const float baseColor[4])
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

#ifdef _WIN32
    ApplyWin32DarkBackground(window, baseColor);
#else
    (void)baseColor;
#endif

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

static void LoadMainApplicationFonts(ImGuiIO& io, float dpiScale)
{
    using namespace ProyecThor::Settings;
    const auto& settings = SettingsManager::Get().GetSettings();
    const std::string& customFontPath = settings.theme.customFontPath;
    const auto& perf = settings.performance;
    const bool lightFonts = perf.performanceMode || perf.lowMemoryMode;
    const char* defaultFontPath = "bin/assets/fonts/OpenSans-Regular.ttf";

    std::string fontToLoad;
    if (!customFontPath.empty() && IsValidFontFile(customFontPath))
        fontToLoad = customFontPath;
    else if (IsValidFontFile(defaultFontPath))
        fontToLoad = defaultFontPath;

    // 1. Rango de símbolos: completo en modo normal, compacto (sin bloques de emojis
    //    masivos 0x1F000-0x1FAFF) en modo de RAM mínima para reducir el atlas GPU.
    static ImVector<ImWchar> s_FullGlyphRanges;
    static ImVector<ImWchar> s_CompactGlyphRanges;
    ImVector<ImWchar>& targetRanges = perf.lowMemoryMode ? s_CompactGlyphRanges : s_FullGlyphRanges;
    if (targetRanges.empty())
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        if (!perf.lowMemoryMode)
        {
            builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
            builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        }

        static const ImWchar kEssentialSymbols[] = {
            0x0100, 0x024F, // Latin Extended-A & B
            0x2000, 0x206F, // General Punctuation
            0x2070, 0x209F, // Superscripts and Subscripts
            0x20A0, 0x20CF, // Currency Symbols (€, $, £, etc.)
            0x2100, 0x214F, // Letterlike Symbols (№, ™, ℠)
            0x2150, 0x218F, // Number Forms (½, ⅓, etc.)
            0x2190, 0x21FF, // Arrows (←, ↑, →, ↓, ↔, ↕)
            0x2200, 0x22FF, // Mathematical Operators
            0x2300, 0x23FF, // Miscellaneous Technical (⌘, ⌚, ⌛, ⌫)
            0x2460, 0x24FF, // Enclosed Alphanumerics (①, ②, etc.)
            0x2500, 0x257F, // Box Drawing
            0x2580, 0x259F, // Block Elements
            0x25A0, 0x25FF, // Geometric Shapes (■, □, ▲, △, ▶, ▼, ◆, ●, ○)
            0x2600, 0x26FF, // Miscellaneous Symbols (☀, ☁, ⚡, ☕, ⛑, ⛪, ✝, ⚔, ⚙, ⚠)
            0x2700, 0x27BF, // Dingbats (✂, ✈, ✉, ✔, ✕, ✖, ✝, ✨, ✦, ✪, ✰, ➔)
            0x2B00, 0x2BFF, // Miscellaneous Symbols and Arrows (⬡, ⬢, ↺, ⟳, ⬅, ⬆, ⬇)
            0
        };
        static const ImWchar kExtendedPictographs[] = {
            0x1F000, 0x1F02F, // Mahjong
            0x1F0A0, 0x1F0FF, // Playing Cards
            0x1F300, 0x1F5FF, // Misc Symbols & Pictographs
            0x1F600, 0x1F64F, // Emoticons
            0x1F680, 0x1F6FF, // Transport and Map
            0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
            0x1FA70, 0x1FAFF, // Symbols and Pictographs Extended-A
            0
        };
        builder.AddRanges(kEssentialSymbols);
        if (!perf.lowMemoryMode)
            builder.AddRanges(kExtendedPictographs);
        builder.BuildRanges(&targetRanges);
    }

    ImFont* mainFont = nullptr;
    if (!fontToLoad.empty())
    {
        ImFontConfig cfg;
        cfg.OversampleH = lightFonts ? 1 : 2;
        cfg.OversampleV = lightFonts ? 1 : 2;
        cfg.PixelSnapH  = true;
        mainFont = io.Fonts->AddFontFromFileTTF(fontToLoad.c_str(), 16.0f * dpiScale, &cfg, targetRanges.Data);
    }

    // 2. Fusionar fuentes de emojis y símbolos del sistema (Windows / Linux)
    static const char* kFallbackFonts[] = {
        "bin/assets/fonts/NotoEmoji.ttf",
        "assets/bin/assets/fonts/NotoEmoji.ttf",
        "build-win/bin/assets/fonts/NotoEmoji.ttf",
        "build-linux/bin/assets/fonts/NotoEmoji.ttf",
        "bin/assets/fonts/Symbola.ttf",
        "assets/bin/assets/fonts/Symbola.ttf",
        "build-win/bin/assets/fonts/Symbola.ttf",
        "build-linux/bin/assets/fonts/Symbola.ttf",
        "C:\\Windows\\Fonts\\seguisym.ttf",
        "C:\\Windows\\Fonts\\seguiemj.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "/run/host/fonts/noto/NotoSansSymbols2-Regular.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/run/host/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/symbola/Symbola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };

    int mergedFallbacks = 0;
    for (const char* fallbackPath : kFallbackFonts)
    {
        if (perf.lowMemoryMode)
        {
            std::string fp(fallbackPath);
            if (fp.find("Emoji") != std::string::npos || fp.find("seguiemj") != std::string::npos)
                continue;
            if (mergedFallbacks >= 2)
                break;
        }

        if (IsValidFontFile(fallbackPath))
        {
            ImFontConfig mergeCfg;
            mergeCfg.MergeMode   = true;
            mergeCfg.OversampleH = 1;
            mergeCfg.OversampleV = 1;
            mergeCfg.PixelSnapH  = true;
            if (io.Fonts->AddFontFromFileTTF(fallbackPath, 16.0f * dpiScale, &mergeCfg, targetRanges.Data))
            {
                ++mergedFallbacks;
                std::cout << "[Font] Símbolos/emojis cargados desde: " << fallbackPath << "\n";
            }
        }
    }

    if (!mainFont)
        io.Fonts->AddFontDefault();
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
    StyleGeneralApp::LoadAppIcon("arrow_right",       "bin/assets/icons/ui/arrow_forward.png");
    StyleGeneralApp::LoadAppIcon("arrow_left",        "bin/assets/icons/ui/arrow_back.png");
    StyleGeneralApp::LoadAppIcon("skip_previous",     "bin/assets/icons/ui/skip_previous.png");
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

GLFWwindow* CreateMainWindow(GLFWwindow* splashWindow, int mainW, int mainH, const float baseColor[4])
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
    ApplyWin32DarkBackground(window, baseColor);
#else
    (void)baseColor;
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

    glfwSetDropCallback(window, [](GLFWwindow* /*w*/, int count, const char** paths) {
        if (count > 0 && paths) {
            std::vector<std::string> mediaPaths;
            for (int i = 0; i < count; i++) {
                if (!paths[i] || !*paths[i]) continue;
                std::string raw = paths[i];
                std::filesystem::path p = ProyecThor::Library::U8Path(raw);
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (ext == ".ptplugin" || ext == ".lua" || ext == ".dll" ||
                    ext == ".so" || ext == ".json" || ext == ".zip") {
                    ProyecThor::Core::PluginManager::Get().InstallPluginFromPath(raw);
                } else {
                    mediaPaths.push_back(raw);
                }
            }
            if (!mediaPaths.empty()) {
                ProyecThor::Core::MediaImportManager::Get().EnqueueManualFiles(
                    mediaPaths, ProyecThor::Core::MediaImportDest::Multimedia);
            }
        }
    });

    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (status != GLEW_OK && status != GLEW_ERROR_NO_GLX_DISPLAY)
        std::cerr << "[DIAG] ADVERTENCIA: glewInit() para mainWindow devolvio error: "
                  << glewGetErrorString(status) << "\n";

    glClearColor(baseColor[0], baseColor[1], baseColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return window;
}

static GLFWwindow* s_MainWindow = nullptr;

void InstallViewportRenderHook(GLFWwindow* mainWindow = nullptr)
{
    if (mainWindow)
        s_MainWindow = mainWindow;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    static void (*s_OrigCreateWindow)(ImGuiViewport*) = platform_io.Platform_CreateWindow;

    platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport)
    {
        if (s_OrigCreateWindow)
            s_OrigCreateWindow(viewport);
        GLFWwindow* w = static_cast<GLFWwindow*>(viewport->PlatformHandle);
        if (w)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            glfwMakeContextCurrent(w);
            glfwSwapInterval(0);
            glfwMakeContextCurrent(backup);
#ifdef _WIN32
            HWND hwnd = (HWND)viewport->PlatformHandleRaw;
            if (!hwnd) hwnd = glfwGetWin32Window(w);
            HWND mainHwnd = s_MainWindow ? glfwGetWin32Window(s_MainWindow) : NULL;
            if (hwnd && mainHwnd && hwnd != mainHwnd)
            {
                ::SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)mainHwnd);
            }
#endif
        }
    };

    static void (*s_OrigShowWindow)(ImGuiViewport*) = platform_io.Platform_ShowWindow;
    platform_io.Platform_ShowWindow = [](ImGuiViewport* viewport)
    {
        if (s_OrigShowWindow)
            s_OrigShowWindow(viewport);
#ifdef _WIN32
        HWND hwnd = (HWND)viewport->PlatformHandleRaw;
        if (!hwnd) {
            GLFWwindow* w = static_cast<GLFWwindow*>(viewport->PlatformHandle);
            if (w) hwnd = glfwGetWin32Window(w);
        }
        HWND mainHwnd = s_MainWindow ? glfwGetWin32Window(s_MainWindow) : NULL;
        if (hwnd && mainHwnd && hwnd != mainHwnd)
        {
            ::SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)mainHwnd);
        }
        if (hwnd && !(viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing))
        {
            ::BringWindowToTop(hwnd);
            ::SetForegroundWindow(hwnd);
        }
#endif
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

void RenderWarmupMainFrame(GLFWwindow* window, ProyecThor::UI::UIManager& uiManager,
                           const ProyecThor::Settings::ThemeSettings& theme, bool swapBuffers)
{
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(window, &fw, &fh);
    if (fw <= 0 || fh <= 0)
        return;

    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    const bool skipBlur = perf.performanceMode || perf.disableGlassBlur;
    if (skipBlur)
        uiManager.GetGlassRenderer().ReleaseTargets();
    else
        uiManager.GetGlassRenderer().Resize(fw, fh);

    glViewport(0, 0, fw, fh);
    glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    uiManager.RenderAll();

    ImGui::Render();
    ProyecThor::UI::OverlayExportService::Get().ProcessPending();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!skipBlur) {
        uiManager.GetGlassRenderer().CaptureCurrentFrame();
        uiManager.GetGlassRenderer().Blur(1.0f, 1);
    }

    {
        GLFWwindow* ctxBackup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(ctxBackup);
    }

    if (swapBuffers)
        glfwSwapBuffers(window);
}

void RunMainLoop(GLFWwindow* window, ProyecThor::UI::UIManager& uiManager,
                  const ProyecThor::Settings::ThemeSettings& theme)
{
    int    currentSwapInterval = 1;
    double lastLowMemTrimTime  = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        using Clock = FrameProfiler::Clock;
        auto frameStart = Clock::now();

        auto t0 = Clock::now();
        glfwPollEvents();
        FrameProfiler::Add(FrameProfiler::s_PollEvents, FrameProfiler::ElapsedMs(t0));

        auto& core = ProyecThor::Core::PresentationCore::Get();
        ProcessSingleInstanceRequests(window, uiManager);

        if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;

        auto t1 = Clock::now();
        core.Update();
        FrameProfiler::Add(FrameProfiler::s_CoreUpdate, FrameProfiler::ElapsedMs(t1));
        core.RenderAllSecondaryWindows();

        int fw, fh;
        glfwGetFramebufferSize(window, &fw, &fh);
        if (fw == 0 || fh == 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(core.IsProjecting() ? 16 : 50));
            continue;
        }

        const bool skipBlur = perf.performanceMode || perf.disableGlassBlur;
        if (skipBlur)
            uiManager.GetGlassRenderer().ReleaseTargets();
        else
            uiManager.GetGlassRenderer().Resize(fw, fh);

        glViewport(0, 0, fw, fh);
        glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto t2 = Clock::now();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        uiManager.RenderAll();

        // Atajos de desarrollo / recarga en caliente:
        // F5: Recarga inmediata de ajustes y estilos de UI (0ms sin reinicio)
        // Ctrl+F5 o Ctrl+R: Hot-restart de la aplicación preservando estado
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
                if (io.KeyCtrl) {
                    ProyecThor::Settings::SettingsManager::Get().RequestRestart();
                } else {
                    ProyecThor::Settings::SettingsManager::Get().LoadSettings();
                    ProyecThor::Settings::SettingsManager::Get().ApplyTheme();
                }
            } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                ProyecThor::Settings::SettingsManager::Get().RequestRestart();
            }
        }

        ImGui::Render();
        FrameProfiler::Add(FrameProfiler::s_ImGuiBuild, FrameProfiler::ElapsedMs(t2));

        ProyecThor::UI::OverlayExportService::Get().ProcessPending();

        auto t3 = Clock::now();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        FrameProfiler::Add(FrameProfiler::s_ImGuiRender, FrameProfiler::ElapsedMs(t3));

        if (!skipBlur) {
            uiManager.GetGlassRenderer().CaptureCurrentFrame();
            uiManager.GetGlassRenderer().Blur(1.0f, 1);
        }

        auto t4 = Clock::now();
        {
            GLFWwindow* ctxBackup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctxBackup);
        }
        FrameProfiler::Add(FrameProfiler::s_PlatformWindows, FrameProfiler::ElapsedMs(t4));

        int targetFps = perf.targetFPS;
        if (perf.performanceMode && targetFps <= 0) {
            targetFps = perf.lowMemoryMode ? 30 : 60;
        }
        const int desiredSwapInterval = (targetFps == 30) ? 2 : 1;
        if (desiredSwapInterval != currentSwapInterval) {
            glfwSwapInterval(desiredSwapInterval);
            currentSwapInterval = desiredSwapInterval;
        }

        auto t5 = Clock::now();
        glfwSwapBuffers(window);
        FrameProfiler::Add(FrameProfiler::s_SwapBuffers, FrameProfiler::ElapsedMs(t5));

        ProyecThor::Core::MemoryOptimizer::ProcessPending();

        if (targetFps > 0) {
            double targetMs = 1000.0 / static_cast<double>(targetFps);
            double elapsedMs = FrameProfiler::ElapsedMs(frameStart);
            if (elapsedMs + 0.5 < targetMs) {
                long long sleepMs = static_cast<long long>(targetMs - elapsedMs);
                if (sleepMs > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
                }
            }
        }

        if (perf.lowMemoryMode) {
            double now = glfwGetTime();
            if ((now - lastLowMemTrimTime) >= 45.0) {
                lastLowMemTrimTime = now;
                ProyecThor::Core::MemoryOptimizer::TrimWorkingSet();
            }
        }

        double frameTotalMs = FrameProfiler::ElapsedMs(frameStart);
        FrameProfiler::Add(FrameProfiler::s_FrameTotal, frameTotalMs);
        FrameProfiler::ReportIfReady();

        ProyecThor::Core::PerformanceGovernor::Get().ReportFrame(frameTotalMs);
        ProyecThor::Core::SystemStats::Get().Update();
    }
}

} // namespace

#ifndef _WIN32
static void EnsureLinuxEnvironment(int argc, char** argv)
{
    char exePath[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len <= 0) return;
    exePath[len] = '\0';
    std::filesystem::path dir = std::filesystem::path(exePath).parent_path();

    const char* currentLd = std::getenv("LD_LIBRARY_PATH");
    bool hasDirInLd = false;
    if (currentLd)
    {
        std::string ldStr = currentLd;
        if (ldStr.find(dir.string()) != std::string::npos)
            hasDirInLd = true;
    }

    if (!hasDirInLd && !std::getenv("PROYECTHOR_ENV_READY"))
    {
        std::string newLd = dir.string();
        if (currentLd && *currentLd)
            newLd += ":" + std::string(currentLd);
        setenv("LD_LIBRARY_PATH", newLd.c_str(), 1);
        setenv("PROYECTHOR_ENV_READY", "1", 1);
        execv("/proc/self/exe", argv);
    }
}
#endif

int main(int argc, char** argv)
{
    std::setlocale(LC_NUMERIC, "C");
#ifdef _WIN32
    InstallCrashDiagnostics();
#endif
#ifndef _WIN32
    EnsureLinuxEnvironment(argc, argv);
#endif
    ChangeToExecutableDirectory();
    ProyecThor::Core::AppLog::Inf("Init", "Iniciando ProyecThor v" + std::string(PROYECTHOR_VERSION_STRING) + " (Build #" + std::to_string(PROYECTHOR_BUILD_NUMBER) + ")");
    std::cerr << "[DIAG] Iniciando main()\n";
#ifdef _WIN32
    ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
    RegisterWindowsShellAssociation();
#endif
    const std::string pendingOpenFilePath = GetPendingOpenFilePath(argc, argv);

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
    // En Linux, si DISPLAY está disponible, preferir X11/XWayland por compatibilidad con libVLC y EWMH.
    // Si no hay DISPLAY o se fuerza Wayland, usar backend nativo Wayland.
    const char* forceWayland = std::getenv("PROYECTHOR_FORCE_WAYLAND");
    const char* displayEnv   = std::getenv("DISPLAY");
    if ((!forceWayland || std::string(forceWayland) == "0") && displayEnv && *displayEnv)
    {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        std::cerr << "[DIAG] Forzando backend GLFW a X11/XWayland para compatibilidad con VLC y EWMH\n";
    }
    else
    {
        std::cerr << "[DIAG] Usando backend nativo de GLFW (Wayland/auto)\n";
    }
#endif

    if (!glfwInit())
    {
#ifndef _WIN32
        // Reintentar con detección automática de plataforma si X11 falló
        glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        if (!glfwInit())
#endif
        {
            const char* desc = nullptr;
            int err = glfwGetError(&desc);
            std::cerr << "[DIAG] FALLO: glfwInit() devolvio false. Error " << err << ": "
                      << (desc ? desc : "desconocido") << "\n";
            return -1;
        }
    }
    int platform = glfwGetPlatform();
    const char* platName = (platform == GLFW_PLATFORM_WAYLAND) ? "Wayland" :
                           (platform == GLFW_PLATFORM_X11)     ? "X11/XWayland" :
                           (platform == GLFW_PLATFORM_WIN32)   ? "Win32" : "Desconocida";
    std::cerr << "[DIAG] glfwInit() OK. Plataforma activa: " << platName << "\n";

    const float dpiScale = DetectDpiScale();
    const int splashW = (int)(kSplashWBase * dpiScale);
    const int splashH = (int)(kSplashHBase * dpiScale);
    const int mainW   = (int)(kMainWBase   * dpiScale);
    const int mainH   = (int)(kMainHBase   * dpiScale);

    ProyecThor::Settings::SettingsManager::Get().LoadSettings();
    std::cerr << "[DIAG] SettingsManager::LoadSettings() OK\n";
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    const bool skipSplash = !pendingOpenFilePath.empty();

    GLFWwindow* splashWindow = CreateSplashWindow(splashW, splashH, !skipSplash, theme.base);
    if (!splashWindow)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(splashWindow);
    // Sin bloqueo por VSync durante la precarga para que los avances rápidos
    // de miniaturas cacheadas no sumen esperas artificiales de 16ms.
    glfwSwapInterval(0);
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK && glewStatus != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr << "[DIAG] FALLO: glewInit() devolvio error: " << glewGetErrorString(glewStatus) << "\n";
        glfwTerminate();
        return -1;
    }
    if (glewStatus == GLEW_ERROR_NO_GLX_DISPLAY)
        std::cerr << "[DIAG] glewInit() aviso: sin display GLX (esperable en Wayland/EGL). Core OpenGL cargado OK.\n";
    std::cerr << "[DIAG] glewInit() OK. Version OpenGL: " << (const char*)glGetString(GL_VERSION) << "\n";

    // Pintar inmediatamente el fondo oscuro del splash antes de cargar fuentes/texturas
    if (!skipSplash) {
        glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(splashWindow);
        glfwPollEvents();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiContext* splashContext = ImGui::GetCurrentContext();
    ImGuiIO& splashIO = ImGui::GetIO();
    splashIO.IniFilename = nullptr;

    ProyecThor::Splash::Fonts splashFonts = LoadSplashFonts(splashIO, dpiScale);
    splashFonts.ctx = splashContext;

    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);
    std::cerr << "[DIAG] ImGui inicializado para splashWindow OK\n";

    const ProyecThor::Splash::Art splashArt = ProyecThor::Splash::PickArt();
    const std::string creditText = "Illustration by: " + splashArt.author;

    GLuint logoTex = skipSplash ? 0 : LoadTextureFromFile("proyecthor.png");
    GLuint bgTex   = skipSplash ? 0 : LoadTextureFromFile(splashArt.filename.c_str());

    const ImVec2 splashSize((float)splashW, (float)splashH);

    // Mostrar el primer fotograma real del SplashScreen de inmediato antes de iniciar tareas pesadas
    if (!skipSplash) {
        ProyecThor::Splash::Render(splashWindow, splashSize,
            "Iniciando ProyecThor...", 0.03f,
            logoTex, bgTex, splashFonts, creditText, theme);
    }

    GLFWwindow* mainWindow = nullptr;
    ImGuiContext* mainContext = nullptr;
    ProyecThor::UI::UIManager uiManager;
    std::shared_ptr<ProyecThor::UI::HomePanel> homePanel;
    std::shared_ptr<ProyecThor::UI::LibraryPanel> libraryPanel;
    std::shared_ptr<ProyecThor::UI::ViewPanel> viewPanel;
    std::shared_ptr<ProyecThor::UI::StylesHubPanel> stylesHub;
    std::shared_ptr<ProyecThor::UI::StreamingWorkspacePanel> streamingWs;
    std::shared_ptr<ProyecThor::UI::VideoEditorPanel> videoEditor;

    auto formatItemProgress = [](int done, int total, const std::string& name) -> std::string {
        if (total <= 0) return {};
        std::string prefix = std::to_string(done) + " / " + std::to_string(total);
        if (name.empty())
            return prefix + " recursos procesados";
        return prefix + "  \xE2\x80\xA2  " + name;
    };

    const std::vector<ProyecThor::Splash::Step> steps = {
        { "Inicializando ventana principal y motor multimedia...", 1.4f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              report(0.05f, "Contexto gráfico OpenGL");
              mainWindow = CreateMainWindow(splashWindow, mainW, mainH, theme.base);
              if (!mainWindow) return;

              report(0.45f, "Instancias de reproducción libmpv");
              glfwMakeContextCurrent(mainWindow);
              ProyecThor::Core::PresentationCore::Get().SetMainWindow(mainWindow);

              const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
              if (!perf.performanceMode && !perf.lowMemoryMode) {
                  report(0.85f, "Decodificador de miniaturas de video");
                  ProyecThor::Core::ThumbnailWorker::WarmupMediaEngine();
              }

              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Cargando iconos y recursos gráficos...", 0.7f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow) return;
              report(0.15f, "Iconografía de controles e interfaz");
              glfwMakeContextCurrent(mainWindow);
              LoadUIIcons();
              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Cargando tipografías y fuentes de proyección...", 1.3f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow) return;
              glfwMakeContextCurrent(mainWindow);
              mainContext = ImGui::CreateContext();
              ImGui::SetCurrentContext(mainContext);
              ImGuiIO& io = ImGui::GetIO();
              io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
              io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
              io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
              io.IniFilename  = "proyecthor_ui.ini";

              report(0.20f, "Tipografías base y símbolos Unicode");
              LoadMainApplicationFonts(io, dpiScale);

              report(0.65f, "Fuentes personalizadas de proyección");
              ProyecThor::Core::PresentationCore::Get().LoadFontsIntoImGui();

              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Inicializando paneles y módulos de trabajo...", 1.6f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow || !mainContext) return;
              glfwMakeContextCurrent(mainWindow);
              ImGui::SetCurrentContext(mainContext);

              report(0.10f, "Subsistema de renderizado y temas");
              ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
              ImGui_ImplOpenGL3_Init("#version 130");
              ImGui::StyleColorsDark();
              ImGui::GetStyle().ScaleAllSizes(dpiScale);

              InstallViewportRenderHook(mainWindow);

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

              if (!uiManager.Initialize(mainWindow))
              {
                  std::cerr << "[DIAG] FALLO: uiManager.Initialize(mainWindow) devolvio false\n";
              }

              report(0.28f, "Complementos y salidas de proyección");
              ProyecThor::Core::PluginManager::Get().Initialize();
              ProyecThor::Settings::SettingsManager::Get().ApplyProjection();

              report(0.45f, "Biblioteca, Canciones, Biblias y Modelos 3D");
              homePanel    = std::make_shared<ProyecThor::UI::HomePanel>();
              libraryPanel = std::make_shared<ProyecThor::UI::LibraryPanel>();

              homePanel->SetAudioPanel(libraryPanel->GetAudioPanel());
              ProyecThor::Core::PresentationCore::Get().SetAudioPanelRef(libraryPanel->GetAudioPanel());
              homePanel->m_UIManagerRef = &uiManager;
              libraryPanel->SetUIManager(&uiManager);
              uiManager.SetLibraryPanelRef(libraryPanel.get());
              libraryPanel->SelectCategory(ProyecThor::UI::LibraryCategory::Songs);

              uiManager.AddPanel(libraryPanel);
              uiManager.AddPanel(homePanel);

              report(0.72f, "Vista en Vivo y Hub de Diseño");
              viewPanel = std::make_shared<ProyecThor::UI::ViewPanel>(&uiManager);
              uiManager.SetViewPanelRef(viewPanel.get());
              viewPanel->SetTeamChatPanelRef(&uiManager.GetChatPanel());
              uiManager.AddPanel(viewPanel);

              stylesHub = std::make_shared<ProyecThor::UI::StylesHubPanel>(&uiManager);
              stylesHub->SetTransitionPanel(uiManager.GetTransitionPanel());
              uiManager.SetStylesHubPanelRef(stylesHub.get());
              uiManager.AddPanel(stylesHub);

              report(0.90f, "Streaming y Editor de Video");
              streamingWs = std::make_shared<ProyecThor::UI::StreamingWorkspacePanel>(&uiManager.GetBroadcastPanel());
              streamingWs->SetUIManager(&uiManager);
              uiManager.AddPanel(streamingWs);

              videoEditor = std::make_shared<ProyecThor::UI::VideoEditorPanel>();
              videoEditor->SetUIManager(&uiManager);
              uiManager.AddPanel(videoEditor);

              if (!pendingOpenFilePath.empty())
              {
                  ImportAndPreviewExternalFile(pendingOpenFilePath);
                  uiManager.EnterLibraryWorkspaceMode();
              }

              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Precargando miniaturas de Props y Overlays...", 1.4f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow) return;
              const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
              if (perf.performanceMode || perf.lowMemoryMode) {
                  report(1.0f, "Carga diferida bajo demanda");
                  return;
              }
              glfwMakeContextCurrent(mainWindow);
              ProyecThor::UI::OverlayLibraryTab::PreloadAllThumbnails(
                  [&](int done, int total, const std::string& name) {
                      const float sub = (total > 0) ? ((float)done / (float)total) : 1.0f;
                      report(sub, formatItemProgress(done, total, name));
                  });
              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Precargando galería de fondos...", 1.8f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow || !stylesHub) return;
              const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
              if (perf.performanceMode || perf.lowMemoryMode) {
                  report(1.0f, "Carga diferida bajo demanda");
                  return;
              }
              glfwMakeContextCurrent(mainWindow);
              stylesHub->PreloadThumbnails(
                  [&](int done, int total, const std::string& name) {
                      const float sub = (total > 0) ? ((float)done / (float)total) : 1.0f;
                      report(sub, formatItemProgress(done, total, name));
                  });
              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Precargando letras, canciones y textos bíblicos...", 1.6f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow) return;
              report(0.05f, "Indexando cancionero y letras...");
              glfwMakeContextCurrent(mainWindow);
              ProyecThor::Library::PreloadAllSongs(
                  [&](int done, int total, const std::string& name) {
                      const float sub = (total > 0) ? (0.05f + 0.85f * ((float)done / (float)total)) : 0.90f;
                      report(sub, formatItemProgress(done, total, name));
                  });

              report(0.92f, "Precargando textos bíblicos...");
              std::string bibleDir = ProyecThor::GetAssetsPath() + "/bibles";
              std::error_code bec;
              if (std::filesystem::exists(bibleDir, bec)) {
                  for (const auto& bEntry : std::filesystem::directory_iterator(bibleDir, bec)) {
                      if (bec) break;
                      if (!bEntry.is_regular_file()) continue;
                      if (bEntry.path().extension() == ".xml") {
                          ProyecThor::UI::BibleData dummyBible;
                          ProyecThor::UI::XmlIO::LoadBible(bEntry.path().string(), dummyBible);
                          break;
                      }
                  }
              }
              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
              report(1.0f, "Textos y letras listos");
          }
        },

        { "Precargando contenido multimedia (video, audio e imagen)...", 2.4f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow) return;
              report(0.02f, "Indexando carpetas de medios y colecciones...");
              ProyecThor::Core::MediaCollectionManager::Get().Initialize();
              ProyecThor::Library::RefreshMultimediaLists();

              const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
              if (perf.performanceMode || perf.lowMemoryMode) {
                  report(1.0f, "Carga diferida bajo demanda");
                  return;
              }

              glfwMakeContextCurrent(mainWindow);
              ProyecThor::Library::PreloadMultimediaThumbnails(
                  [&](int done, int total, const std::string& name) {
                      const float sub = (total > 0) ? (0.05f + 0.95f * ((float)done / (float)total)) : 1.0f;
                      report(sub, formatItemProgress(done, total, name));
                  });
              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        },

        { "Preparando composición visual y espacio de trabajo...", 1.1f, nullptr,
          [&](ProyecThor::Splash::ProgressReporter report) {
              if (!mainWindow || !mainContext) return;
              glfwMakeContextCurrent(mainWindow);
              ImGui::SetCurrentContext(mainContext);
              glfwSwapInterval(0);

              // Precompila shaders de fondo (Blit + AMD FSR 1.0) en el contexto GL principal
              // para que el primer fondo aplicado aparezca al instante sin tirones de compilación.
              report(0.08f, "Precompilando shaders de proyección");
              ProyecThor::Core::PresentationCore::Get().WarmupGraphics(1920, 1080);

              const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
              report(0.25f, "Construyendo atlas tipográfico y layout de paneles");
              RenderWarmupMainFrame(mainWindow, uiManager, theme, false);

              if (!perf.performanceMode && !perf.lowMemoryMode) {
                  report(0.60f, "Renderizando vistas y galerías precargadas");
                  RenderWarmupMainFrame(mainWindow, uiManager, theme, false);
              }

              report(0.90f, "Estabilizando composición");
              RenderWarmupMainFrame(mainWindow, uiManager, theme, true);
              glFinish();

              glfwMakeContextCurrent(splashWindow);
              ImGui::SetCurrentContext(splashContext);
          }
        }
    };

    if (skipSplash) {
        auto noopReport = [](float, const std::string&) {};
        for (const auto& step : steps) {
            if (step.taskWithProgress) step.taskWithProgress(noopReport);
            else if (step.task)        step.task();
        }
    } else {
        ProyecThor::Splash::RunSteps(steps, splashWindow, splashSize,
            logoTex, bgTex, splashFonts, creditText, theme);
    }

    if (!mainWindow || !mainContext)
    {
        std::cerr << "[DIAG] FALLO: mainWindow o mainContext no se inicializaron correctamente.\n";
        ImGui::SetCurrentContext(splashContext);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(splashContext);
        glfwDestroyWindow(splashWindow);
        glfwTerminate();
        return -1;
    }

    if (!skipSplash) {
        if (!glfwWindowShouldClose(splashWindow)) {
            glfwMakeContextCurrent(splashWindow);
            ImGui::SetCurrentContext(splashContext);
            ProyecThor::Splash::Render(splashWindow, splashSize, "Iniciando ProyecThor...", 1.0f,
                logoTex, bgTex, splashFonts, creditText, theme);
        }
        if (glfwWindowShouldClose(splashWindow))
        {
            glfwMakeContextCurrent(splashWindow);
            ImGui::SetCurrentContext(splashContext);
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(splashContext);
            if (logoTex != 0) glDeleteTextures(1, &logoTex);
            if (bgTex   != 0) glDeleteTextures(1, &bgTex);
            glfwDestroyWindow(splashWindow);

            if (mainContext) {
                glfwMakeContextCurrent(mainWindow);
                ImGui::SetCurrentContext(mainContext);
                uiManager.Shutdown();
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext(mainContext);
            }
            glfwDestroyWindow(mainWindow);
            glfwTerminate();
            return 0;
        }
    }

    // ── Transición sin pantalla en blanco (Seamless Handoff) ──────────────────
    // Mostramos y dibujamos mainWindow ANTES de destruir splashWindow. En
    // Windows usamos DWMWA_CLOAK para mapear la superficie visible de DWM sin
    // que aparezca ningún fotograma blanco mientras se presenta el primer
    // SwapBuffers real de la ventana visible.
    glfwMakeContextCurrent(mainWindow);
    ImGui::SetCurrentContext(mainContext);
    glfwSwapInterval(0);

    StartSingleInstanceListener();

#ifdef _WIN32
    HWND mainHwnd = glfwGetWin32Window(mainWindow);
    if (mainHwnd) {
        BOOL cloak = TRUE;
        DwmSetWindowAttribute(mainHwnd, 13 /*DWMWA_CLOAK*/, &cloak, sizeof(cloak));
    }
#endif

    glfwShowWindow(mainWindow);
    glfwPollEvents();
    RenderWarmupMainFrame(mainWindow, uiManager, theme, true);
    glFinish();

#ifdef _WIN32
    if (mainHwnd) {
        BOOL cloak = FALSE;
        DwmSetWindowAttribute(mainHwnd, 13 /*DWMWA_CLOAK*/, &cloak, sizeof(cloak));
        DwmFlush();
    }
#endif

    glfwSwapInterval(1);

    // Ahora que mainWindow ya está completamente pintada en pantalla, cerramos el splash
    glfwMakeContextCurrent(splashWindow);
    ImGui::SetCurrentContext(splashContext);
    if (logoTex != 0) glDeleteTextures(1, &logoTex);
    if (bgTex   != 0) glDeleteTextures(1, &bgTex);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(splashContext);
    glfwDestroyWindow(splashWindow);
    splashWindow = nullptr;

    glfwMakeContextCurrent(mainWindow);
    ImGui::SetCurrentContext(mainContext);
    glfwFocusWindow(mainWindow);

    {
        const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
        if (perf.performanceMode || perf.lowMemoryMode) {
            ProyecThor::Core::MemoryOptimizer::TrimWorkingSet();
        }
    }

    // Lo último una vez cargada la app: consultar la licencia en servidores

    std::cerr << "[DIAG] Entrando al loop principal\n";
    RunMainLoop(mainWindow, uiManager, theme);

    std::cerr << "[DIAG] Saliendo del loop principal, cerrando limpio\n";

    StopSingleInstanceListener();

    uiManager.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(mainContext);

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    if (ProyecThor::Settings::SettingsManager::Get().IsRestartRequested())
        ProyecThor::Settings::RestartApplication();

    return 0;
}
