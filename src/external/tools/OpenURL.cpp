#include "OpenURL.h"
#include <cstdlib>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace ProyecThor::External {

    void OpenURL(const std::string& url) {
        if (url.empty()) return;

#ifdef _WIN32
        // ShellExecuteA requiere la librería shell32
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
        std::string command = "open \"" + url + "\"";
        std::system(command.c_str());
#else
        std::string command = "xdg-open \"" + url + "\"";
        std::system(command.c_str());
#endif
    }

}