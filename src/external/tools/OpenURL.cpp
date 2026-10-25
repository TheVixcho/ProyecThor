#include "OpenURL.h"
#include <cstdlib>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>
#elif !defined(__APPLE__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
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
        pid_t pid = fork();
        if (pid == 0) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
            _exit(127);
        }
#endif
    }

}