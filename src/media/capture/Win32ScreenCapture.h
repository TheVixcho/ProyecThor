#pragma once

#ifdef _WIN32
#include <cstdint>
#include <memory>

namespace ProyecThor::UI {

// Captura de monitor/ventana en Windows via DXGI Desktop Duplication -- el
// mismo mecanismo que usa OBS para "Display Capture": lee directo el
// framebuffer ya compuesto por DWM, incluido contenido acelerado por
// hardware (navegadores, Electron, juegos, etc.), a diferencia de
// BitBlt/PrintWindow sobre el DC de una ventana puntual, que para varias
// de esas apps queda directamente en negro.
//
// Para "ventana puntual" no usamos Windows.Graphics.Capture (WGC, lo que
// usa OBS para "Window Capture"): los headers de interop que hacen falta
// (CreateDirect3D11DeviceFromDXGIDevice, IDirect3DDxgiInterfaceAccess) no
// estan en este toolchain mingw-w64. En su lugar, duplicamos el monitor
// completo donde esta la ventana y recortamos al rectangulo actual de esa
// ventana -- el contenido sale del mismo framebuffer compuesto (correcto
// para apps con aceleracion de hardware); la unica diferencia real con WGC
// es que si otra ventana tapa parcialmente esa zona, se ve lo que hay
// realmente en pantalla ahi, como cualquier captura de region.
class Win32ScreenCapture {
public:
    Win32ScreenCapture();
    ~Win32ScreenCapture();

    Win32ScreenCapture(const Win32ScreenCapture&)            = delete;
    Win32ScreenCapture& operator=(const Win32ScreenCapture&) = delete;

    // x,y: punto (en coordenadas virtuales de escritorio) dentro del
    // monitor a duplicar completo -- CapturePanel ya conoce esa posicion
    // via glfwGetMonitorPos().
    bool OpenMonitor(int x, int y);

    // Duplica el monitor que contiene la ventana y recorta al rectangulo
    // actual de esa ventana en cada frame (se sigue leyendo su posicion,
    // por si se mueve).
    bool OpenWindow(void* hwnd);

    void Close();

    // Devuelve puntero a pixeles RGBA del ultimo frame (o nullptr si
    // todavia no hay ninguno util). w/h se actualizan con las dimensiones
    // reales. Sostiene el ultimo frame valido en timeouts benignos
    // (AcquireNextFrame sin cambios de pantalla desde el ultimo grab).
    const uint8_t* GrabFrame(int& w, int& h);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace ProyecThor::UI
#endif // _WIN32
