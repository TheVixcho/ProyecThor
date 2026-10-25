#pragma once
#include <cstdio>
#include <string>

namespace ProyecThor::Core {

#if defined(_WIN32)

// ─────────────────────────────────────────────────────────────────────────────
//  StartHiddenProcess / WaitHiddenProcess — reemplazo de _popen() para
//  lanzar procesos de consola (ffmpeg) SIN que Windows les abra una ventana
//  de consola propia visible. _popen() no da control sobre eso: como
//  ProyecThor es una app GUI (subsistema WIN32, sin consola), cualquier
//  proceso de consola que lance con _popen() se lleva una consola nueva
//  que parpadea en pantalla (visible un instante y despues se cierra) --
//  ademas de que _popen(cmd, "r") solo captura stdout, y ffmpeg reporta
//  sus errores por stderr, asi que el motivo real de un fallo se perdia
//  en esa consola invisible/descartada.
//
//  Este helper usa CreateProcess con CREATE_NO_WINDOW y redirige stdin/
//  stdout+stderr a pipes propios segun se pida.
// ─────────────────────────────────────────────────────────────────────────────

// Lanza 'commandLine' oculto. wantStdinPipe=true da un FILE* en *outStdin
// para escribirle datos binarios (mismo uso que _popen(cmd,"wb"); cerrar
// con fclose). wantOutputCapture=true da un FILE* en *outOutput con
// stdout+stderr COMBINADOS en modo lectura de texto (fgets), para poder
// mostrar el error real si el proceso falla. outProcessHandle queda listo
// para pasarle a WaitHiddenProcess(). Devuelve false si CreateProcess fallo.
bool StartHiddenProcess(const std::string& commandLine,
                         bool wantStdinPipe, FILE** outStdin,
                         bool wantOutputCapture, FILE** outOutput,
                         void** outProcessHandle);

// Espera a que el proceso termine y devuelve su codigo de salida real
// (GetExitCodeProcess, no lo que devolveria _pclose via cmd.exe). Cierra
// el handle del proceso -- no llamar dos veces con el mismo handle.
int WaitHiddenProcess(void* processHandle);

// Mata el proceso ya mismo (ej. boton "Cancelar" de una conversion en
// curso, ver MediaConverter::Cancel). NO cierra el handle -- el llamador
// igual necesita pasarlo a WaitHiddenProcess() despues para liberar
// recursos y obtener el codigo de salida (va a ser distinto de 0, el
// consumidor debe tratar una cancelacion como un caso aparte, no como
// error generico).
void TerminateHiddenProcess(void* processHandle);

#endif // _WIN32

} // namespace ProyecThor::Core
