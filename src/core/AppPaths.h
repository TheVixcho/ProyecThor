#pragma once
#include <string>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#else
#include <cstdlib>
#include <filesystem>
#endif

namespace ProyecThor {

#ifdef _WIN32
    inline std::string WinWideToUtf8(const wchar_t* w) {
        if (!w || !*w) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) return "";
        std::string out(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    inline std::filesystem::path WinUtf8ToPath(const std::string& u8) {
        if (u8.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), nullptr, 0);
        if (len <= 0) return std::filesystem::path(u8);
        std::wstring w(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), w.data(), len);
        return std::filesystem::path(w);
    }

    inline std::string GetWinAppDataUtf8() {
        wchar_t appDataBuf[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataBuf)))
            return WinWideToUtf8(appDataBuf);
        return "";
    }
#endif

    // Ubicacion FIJA (nunca se mueve, siempre el %APPDATA%/~/.local/share
    // "de fabrica") de un archivo chico de una sola linea que puede apuntar
    // a donde vive de verdad la carpeta de datos -- necesaria para poder
    // resolver "donde estan mis datos" ANTES de saber donde estan. Vacio o
    // ausente = usar la ubicacion default de siempre. Ver Ajustes >
    // Actualizaciones > "Carpeta de datos" (CategoryUpdates.cpp), que es lo
    // unico que escribe este archivo.
    inline std::string GetDataDirRedirectFilePath() {
#ifdef _WIN32
        std::string appData = GetWinAppDataUtf8();
        if (!appData.empty())
            return appData + "\\ProyecThor\\datadir.txt";
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Library/Application Support/ProyecThor/datadir.txt" : "";
#else
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/.local/share/ProyecThor/datadir.txt" : "";
#endif
    }

    // Lee la redireccion si existe y apunta a una carpeta valida. "" si no
    // hay ninguna guardada (o esta vacia) -- en ese caso GetAssetsPath()
    // sigue con la ubicacion default de siempre.
    inline std::string ReadDataDirRedirect() {
        std::string redirectFile = GetDataDirRedirectFilePath();
        if (redirectFile.empty()) return "";
#ifdef _WIN32
        std::ifstream f(WinUtf8ToPath(redirectFile));
#else
        std::ifstream f(redirectFile);
#endif
        if (!f.is_open()) return "";
        std::string customRoot;
        std::getline(f, customRoot);
        while (!customRoot.empty() && (customRoot.back() == '\r' || customRoot.back() == '\n'))
            customRoot.pop_back();
        return customRoot;
    }

    inline const std::string& GetAssetsPath() {
        static std::string s_AssetsPath;
        if (!s_AssetsPath.empty()) return s_AssetsPath;

        std::string customRoot = ReadDataDirRedirect();
        if (!customRoot.empty()) {
            s_AssetsPath = customRoot + "/assets";
            return s_AssetsPath;
        }

#ifdef _WIN32
        std::string appData = GetWinAppDataUtf8();
        if (!appData.empty()) {
            s_AssetsPath = appData + "\\ProyecThor\\assets";
        } else {
            s_AssetsPath = "assets";
        }
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        if (home) {
            std::string dir = std::string(home) + "/Library/Application Support/ProyecThor/assets";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            s_AssetsPath = dir;
        } else {
            s_AssetsPath = "assets";
        }
#else
        const char* home = std::getenv("HOME");
        if (home) {
            std::string dir = std::string(home) + "/.local/share/ProyecThor/assets";
            std::filesystem::create_directories(dir);
            s_AssetsPath = dir;
        } else {
            s_AssetsPath = "assets";
        }
#endif
        return s_AssetsPath;
    }

    inline std::string SongsPath()     { return GetAssetsPath() + "/songs/";     }
    inline std::string VideosPath()    { return GetAssetsPath() + "/videos/";    }
    inline std::string ImagesPath()    { return GetAssetsPath() + "/images/";    }
    inline std::string BiblesPath()    { return GetAssetsPath() + "/bibles/";    }
    inline std::string DocumentsPath() { return GetAssetsPath() + "/documents/"; }

    // Overlays guardados (ver OverlayLibraryTab/OverlayRecipeIO): "<name>.overlay"
    // (receta) + "<name>.png" (rasterizado transparente) + "images/" (capas de
    // imagen importadas). Sin barra final -- a diferencia de las de arriba, para
    // poder usarla directo como filesystem::path en OverlayLibraryTab/SyncServer.
    inline std::string OverlaysPath()  { return GetAssetsPath() + "/overlays";   }

    // Imagenes propias de la app (ej. el Logo de pantalla de carga, ver
    // Ajustes > Proyeccion): igual que Fondos (LayersBgTab::BgRootDir), los
    // archivos elegidos se COPIAN aca en vez de guardar la ruta externa tal
    // cual — asi quedan junto con el resto de los datos de la app y no se
    // rompen si el archivo original se mueve/borra/no existe en otra
    // maquina.
    inline std::string BrandingPath()  { return GetAssetsPath() + "/branding/";  }
    inline std::string WebPath()       { return GetAssetsPath() + "/web/";       }

    // Raiz real de AppData\ProyecThor (un nivel arriba de assets/): ahi
    // tambien viven settings.json, songs_authors.ini, themes/, etc. Usada
    // por SyncServer para sincronizar TODO el arbol de datos del usuario,
    // no solo assets/ -- ver SyncServer.cpp. Tambien es la raiz que mueve
    // Ajustes > Actualizaciones > "Carpeta de datos" al cambiar de
    // ubicacion.
    inline std::string GetAppDataRoot() {
        std::string assets = GetAssetsPath(); // ".../ProyecThor/assets"
        const std::string fwdSuffix  = "/assets";
        const std::string backSuffix = "\\assets";
        if (assets.size() > fwdSuffix.size() &&
            assets.compare(assets.size() - fwdSuffix.size(), fwdSuffix.size(), fwdSuffix) == 0) {
            return assets.substr(0, assets.size() - fwdSuffix.size());
        }
        if (assets.size() > backSuffix.size() &&
            assets.compare(assets.size() - backSuffix.size(), backSuffix.size(), backSuffix) == 0) {
            return assets.substr(0, assets.size() - backSuffix.size());
        }
        return assets;
    }

    // Raiz default "de fabrica" (ignora cualquier redireccion guardada) --
    // usada SOLO por el flujo de cambio de carpeta (CategoryUpdates.cpp)
    // para saber de DONDE copiar cuando la redireccion actual todavia
    // apunta ahi (primer cambio) o para mostrar "carpeta original" en la UI.
    inline std::string GetDefaultAppDataRoot() {
#ifdef _WIN32
        std::string appData = GetWinAppDataUtf8();
        if (!appData.empty())
            return appData + "\\ProyecThor";
        return "assets";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Library/Application Support/ProyecThor" : "assets";
#else
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/.local/share/ProyecThor" : "assets";
#endif
    }

} // namespace ProyecThor
