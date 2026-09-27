#include "HiddenProcess.h"

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <fcntl.h>

namespace ProyecThor::Core {

// HANDLE de un archivo NUL abierto en el modo pedido, para los streams que
// no se piden capturar (Windows exige un handle valido para los 3 std*
// cuando se usa STARTF_USESTDHANDLES, no se puede dejar nullptr).
static HANDLE OpenNul(bool forWrite) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    return CreateFileA("NUL", forWrite ? GENERIC_WRITE : GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool StartHiddenProcess(const std::string& commandLine,
                         bool wantStdinPipe, FILE** outStdin,
                         bool wantOutputCapture, FILE** outOutput,
                         void** outProcessHandle) {
    if (outStdin)  *outStdin  = nullptr;
    if (outOutput) *outOutput = nullptr;
    if (outProcessHandle) *outProcessHandle = nullptr;

    SECURITY_ATTRIBUTES saInheritable = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    // ── stdin ────────────────────────────────────────────────────────────
    HANDLE childStdin  = nullptr; // extremo que hereda el proceso hijo
    HANDLE parentWrite = nullptr; // extremo que nos quedamos nosotros
    if (wantStdinPipe) {
        if (!CreatePipe(&childStdin, &parentWrite, &saInheritable, 0)) return false;
        SetHandleInformation(parentWrite, HANDLE_FLAG_INHERIT, 0);
    } else {
        childStdin = OpenNul(false);
    }

    // ── stdout+stderr combinados ────────────────────────────────────────
    HANDLE childOutput = nullptr; // extremo que hereda el proceso hijo (stdout Y stderr)
    HANDLE parentRead   = nullptr; // extremo que nos quedamos nosotros
    if (wantOutputCapture) {
        if (!CreatePipe(&parentRead, &childOutput, &saInheritable, 0)) {
            if (childStdin) CloseHandle(childStdin);
            if (parentWrite) CloseHandle(parentWrite);
            return false;
        }
        SetHandleInformation(parentRead, HANDLE_FLAG_INHERIT, 0);
    } else {
        childOutput = OpenNul(true);
    }

    STARTUPINFOA si = {};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput  = childStdin;
    si.hStdOutput = childOutput;
    si.hStdError  = childOutput; // combinado: mismo pipe para stdout y stderr

    PROCESS_INFORMATION pi = {};

    // CreateProcess necesita un buffer de char* MUTABLE para el command
    // line (puede reescribirlo internamente) -- commandLine.c_str() es
    // const, asi que se copia a un buffer propio.
    std::string cmdCopy = commandLine;

    BOOL ok = CreateProcessA(
        nullptr, cmdCopy.data(), nullptr, nullptr,
        /*bInheritHandles=*/TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr, &si, &pi);

    // Los extremos "child" ya los heredo el proceso (o son el NUL, que no
    // hace falta mas aca) -- cerrarlos en el padre para no dejarlos colgados.
    if (childStdin)  CloseHandle(childStdin);
    if (childOutput) CloseHandle(childOutput);

    if (!ok) {
        if (parentWrite) CloseHandle(parentWrite);
        if (parentRead)  CloseHandle(parentRead);
        return false;
    }

    CloseHandle(pi.hThread); // no hace falta, solo nos importa hProcess

    if (parentWrite) {
        int fd = _open_osfhandle((intptr_t)parentWrite, 0);
        if (outStdin) *outStdin = fd >= 0 ? _fdopen(fd, "wb") : nullptr;
    }
    if (parentRead) {
        int fd = _open_osfhandle((intptr_t)parentRead, _O_RDONLY);
        if (outOutput) *outOutput = fd >= 0 ? _fdopen(fd, "r") : nullptr;
    }
    if (outProcessHandle) *outProcessHandle = (void*)pi.hProcess;

    return true;
}

int WaitHiddenProcess(void* processHandle) {
    HANDLE h = (HANDLE)processHandle;
    if (!h) return -1;

    WaitForSingleObject(h, INFINITE);
    DWORD exitCode = (DWORD)-1;
    GetExitCodeProcess(h, &exitCode);
    CloseHandle(h);
    return (int)exitCode;
}

void TerminateHiddenProcess(void* processHandle) {
    HANDLE h = (HANDLE)processHandle;
    if (!h) return;
    TerminateProcess(h, 1);
}

} // namespace ProyecThor::Core

#endif // _WIN32
