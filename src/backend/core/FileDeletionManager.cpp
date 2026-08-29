#include "FileDeletionManager.h"
#include "PresentationCore.h"
#include "backend/core/AppPaths.h"

#include <filesystem>
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <restartmanager.h>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::Core {

namespace {

std::mutex                                      g_HooksMutex;
std::vector<FileDeletionManager::UsageReleaseHook> g_Hooks;

#ifdef _WIN32
std::wstring Utf8ToWideStr(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (count <= 0) return L"";
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], count);
    return wstr;
}

std::string WideToUtf8Str(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (count <= 0) return "";
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], count, NULL, NULL);
    return str;
}

// Estructuras para POSIX delete semantics en Windows 10/11
#ifndef FILE_DISPOSITION_FLAG_DELETE
#define FILE_DISPOSITION_FLAG_DELETE 0x00000001
#endif
#ifndef FILE_DISPOSITION_FLAG_POSIX_SEMANTICS
#define FILE_DISPOSITION_FLAG_POSIX_SEMANTICS 0x00000002
#endif
#ifndef FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE
#define FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE 0x00000010
#endif

typedef struct _PT_FILE_DISPOSITION_INFO_EX {
    DWORD Flags;
} PT_FILE_DISPOSITION_INFO_EX;

#ifndef FileDispositionInfoEx
#define FileDispositionInfoEx ((FILE_INFO_BY_HANDLE_CLASS)21)
#endif

bool TryPosixDelete(const std::wstring& wpath) {
    HANDLE hFile = CreateFileW(
        wpath.c_str(),
        DELETE | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    PT_FILE_DISPOSITION_INFO_EX dispInfo{};
    dispInfo.Flags = FILE_DISPOSITION_FLAG_DELETE |
                     FILE_DISPOSITION_FLAG_POSIX_SEMANTICS |
                     FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;

    BOOL ok = SetFileInformationByHandle(
        hFile,
        FileDispositionInfoEx,
        &dispInfo,
        sizeof(dispInfo));

    CloseHandle(hFile);
    return ok != FALSE;
}

// Cierra / finaliza procesos externos que bloqueen el archivo usando Windows Restart Manager
void TryUnlockViaRestartManager(const std::wstring& wpath) {
    HMODULE hRstrt = LoadLibraryA("rstrtmgr.dll");
    if (!hRstrt) return;

    typedef DWORD (WINAPI *pfnRmStartSession)(DWORD *pSessionHandle, DWORD dwSessionFlags, WCHAR strSessionKey[]);
    typedef DWORD (WINAPI *pfnRmRegisterResources)(DWORD dwSessionHandle, UINT nFiles, LPCWSTR rgsFileNames[], UINT nApplications, RM_UNIQUE_PROCESS rgApplications[], UINT nServices, LPCWSTR rgsServiceNames[]);
    typedef DWORD (WINAPI *pfnRmGetList)(DWORD dwSessionHandle, UINT *pnProcInfoNeeded, UINT *pnProcInfo, RM_PROCESS_INFO rgAffectedApps[], LPDWORD lpdwRebootReasons);
    typedef DWORD (WINAPI *pfnRmShutdown)(DWORD dwSessionHandle, ULONG lActionFlags, RM_WRITE_STATUS_CALLBACK fnStatus);
    typedef DWORD (WINAPI *pfnRmEndSession)(DWORD dwSessionHandle);

    auto pRmStartSession       = (pfnRmStartSession)GetProcAddress(hRstrt, "RmStartSession");
    auto pRmRegisterResources   = (pfnRmRegisterResources)GetProcAddress(hRstrt, "RmRegisterResources");
    auto pRmGetList            = (pfnRmGetList)GetProcAddress(hRstrt, "RmGetList");
    auto pRmShutdown           = (pfnRmShutdown)GetProcAddress(hRstrt, "RmShutdown");
    auto pRmEndSession         = (pfnRmEndSession)GetProcAddress(hRstrt, "RmEndSession");

    if (!pRmStartSession || !pRmRegisterResources || !pRmGetList || !pRmEndSession) {
        FreeLibrary(hRstrt);
        return;
    }

    DWORD sessionHandle = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    if (pRmStartSession(&sessionHandle, 0, sessionKey) == ERROR_SUCCESS) {
        LPCWSTR files[] = { wpath.c_str() };
        if (pRmRegisterResources(sessionHandle, 1, files, 0, NULL, 0, NULL) == ERROR_SUCCESS) {
            UINT nProcInfoNeeded = 0;
            UINT nProcInfo       = 0;
            DWORD dwRebootReasons = RmRebootReasonNone;

            DWORD res = pRmGetList(sessionHandle, &nProcInfoNeeded, &nProcInfo, NULL, &dwRebootReasons);
            if (res == ERROR_MORE_DATA && nProcInfoNeeded > 0) {
                std::vector<RM_PROCESS_INFO> apps(nProcInfoNeeded);
                nProcInfo = nProcInfoNeeded;
                if (pRmGetList(sessionHandle, &nProcInfoNeeded, &nProcInfo, apps.data(), &dwRebootReasons) == ERROR_SUCCESS) {
                    DWORD currentPid = GetCurrentProcessId();
                    for (UINT i = 0; i < nProcInfo; ++i) {
                        DWORD targetPid = apps[i].Process.dwProcessId;
                        if (targetPid != currentPid && targetPid != 0) {
                            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, targetPid);
                            if (hProc) {
                                TerminateProcess(hProc, 0);
                                CloseHandle(hProc);
                            }
                        }
                    }
                }
            }
        }
        pRmEndSession(sessionHandle);
    }
    FreeLibrary(hRstrt);
}

bool TryTrashFallback(const std::wstring& wpath) {
    try {
        std::string assetsDir = ProyecThor::GetAssetsPath();
        fs::path trashDir = fs::path(assetsDir) / ".trash";
        std::error_code ec;
        fs::create_directories(trashDir, ec);

        std::random_device rd;
        std::mt19937_64 gen(rd());
        uint64_t rnd = gen();
        fs::path tempDst = trashDir / ("del_" + std::to_string(rnd) + ".tmp");
        std::wstring wTempDst = Utf8ToWideStr(tempDst.string());

        if (MoveFileExW(wpath.c_str(), wTempDst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            // Marcamos para eliminacion al reiniciar en caso de seguir retenido
            MoveFileExW(wTempDst.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            DeleteFileW(wTempDst.c_str());
            return true;
        }
    } catch (...) {}
    return false;
}
#endif

void CleanAssociatedSidecars(const std::string& fullPath) {
    std::error_code ec;
    // 1. Sidecar de letras de audio (.lyrics.json)
    try {
        fs::path lyricsPath = fs::path(fullPath + ".lyrics.json");
        if (fs::exists(lyricsPath, ec)) {
            fs::remove(lyricsPath, ec);
        }
    } catch (...) {}

    // 2. Caché de miniaturas de videos (.png en cache de assets/thumbnails)
    try {
        std::string absPath = fs::absolute(fs::path(fullPath)).string();
        std::hash<std::string> hasher;
        std::string hashStr = std::to_string(hasher(absPath));
        fs::path thumbPath = fs::path(ProyecThor::GetAssetsPath()) / "thumbnails" / (hashStr + ".png");
        if (fs::exists(thumbPath, ec)) {
            fs::remove(thumbPath, ec);
        }
    } catch (...) {}
}

} // namespace

void FileDeletionManager::RegisterUsageReleaseHook(UsageReleaseHook hook) {
    std::lock_guard<std::mutex> lock(g_HooksMutex);
    g_Hooks.push_back(std::move(hook));
}

void FileDeletionManager::ReleaseAllUsages(const std::string& fullPath) {
    if (fullPath.empty()) return;

    // 1. Notificar a PresentationCore para detener preview y selección si coinciden
    PresentationCore::Get().ReleasePathUsages(fullPath);

    // 2. Notificar a todos los hooks registrados (AudioPanel, DocumentView, LibraryVideoPreview, etc.)
    {
        std::vector<UsageReleaseHook> hooksCopy;
        {
            std::lock_guard<std::mutex> lock(g_HooksMutex);
            hooksCopy = g_Hooks;
        }
        for (const auto& hook : hooksCopy) {
            if (hook) {
                try {
                    hook(fullPath);
                } catch (const std::exception& e) {
                    std::cerr << "[FileDeletionManager] Error en hook: " << e.what() << '\n';
                } catch (...) {}
            }
        }
    }

    // 3. Limpiar sidecars y caché
    CleanAssociatedSidecars(fullPath);

    // 4. Pausa mínima para permitir que los hilos decodificadores cierren sus descriptores
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

bool FileDeletionManager::ForceDeleteFile(const std::string& path) {
    if (path.empty()) return true;

    // Normalizar ruta
    std::string fullPath = path;
    try {
        if (fs::path(path).is_relative()) {
            fullPath = fs::absolute(fs::path(path)).string();
        }
    } catch (...) {}

    // 1. Liberar todos los usos en memoria y reproductores
    ReleaseAllUsages(fullPath);

#ifdef _WIN32
    std::wstring wpath = Utf8ToWideStr(fullPath);

    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        // El archivo ya no existe
        return true;
    }

    // Quitar atributo Read-Only si lo tuviera
    if (attrs & FILE_ATTRIBUTE_READONLY) {
        SetFileAttributesW(wpath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    }

    // Intento 1: DeleteFileW directo
    if (DeleteFileW(wpath.c_str())) {
        return true;
    }

    // Intento 2: POSIX delete semantics (Windows 10+)
    if (TryPosixDelete(wpath)) {
        return true;
    }

    // Intento 3: Reintentos con pequeñas esperas mientras VLC/trabajadores liberan el handle
    for (int retry = 0; retry < 6; ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        if (DeleteFileW(wpath.c_str()) || TryPosixDelete(wpath)) {
            return true;
        }
    }

    // Intento 4: Desbloqueo mediante Restart Manager para matar procesos auxiliares
    TryUnlockViaRestartManager(wpath);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    if (DeleteFileW(wpath.c_str()) || TryPosixDelete(wpath)) {
        return true;
    }

    // Intento 5: std::filesystem::remove como fallback general
    std::error_code ec;
    fs::remove(fs::u8path(fullPath), ec);
    if (!ec) return true;

    // Intento 6: Contingencia instantánea mediante traslado a carpeta .trash
    if (TryTrashFallback(wpath)) {
        return true;
    }

    std::cerr << "[FileDeletionManager] No se pudo eliminar el archivo: " << fullPath << '\n';
    return false;

#else
    // Linux / POSIX
    std::error_code ec;
    fs::path u8p(fullPath);
    if (!fs::exists(u8p, ec)) return true;

    for (int retry = 0; retry < 5; ++retry) {
        fs::remove(u8p, ec);
        if (!ec) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    return false;
#endif
}

bool FileDeletionManager::ForceDeleteDirectory(const std::string& path) {
    if (path.empty()) return true;

    std::string fullPath = path;
    try {
        if (fs::path(path).is_relative()) {
            fullPath = fs::absolute(fs::path(path)).string();
        }
    } catch (...) {}

    // Recorrer y liberar todos los archivos del directorio primero
    try {
        fs::path dirPath = fs::u8path(fullPath);
        std::error_code ec;
        if (!fs::exists(dirPath, ec)) return true;

        if (fs::is_directory(dirPath, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(dirPath, ec)) {
                if (entry.is_regular_file(ec)) {
                    ForceDeleteFile(entry.path().string());
                }
            }
        }
        fs::remove_all(dirPath, ec);
        return !ec;
    } catch (const std::exception& e) {
        std::cerr << "[FileDeletionManager] Error al eliminar directorio: " << e.what() << '\n';
    } catch (...) {}

#ifdef _WIN32
    std::wstring wpath = Utf8ToWideStr(fullPath);
    return RemoveDirectoryW(wpath.c_str()) != FALSE;
#else
    std::error_code ec;
    fs::remove_all(fs::u8path(fullPath), ec);
    return !ec;
#endif
}

} // namespace ProyecThor::Core
