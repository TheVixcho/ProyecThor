#include "Win32ScreenCapture.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <vector>

// Linkeo de d3d11/dxgi/dwmapi via CMakeLists.txt (target_link_libraries),
// no via #pragma comment: con -static + mingw-w64 esas directivas .drectve
// no las toma el linker.

namespace ProyecThor::UI {

struct Win32ScreenCapture::Impl {
    ID3D11Device*           device      = nullptr;
    ID3D11DeviceContext*    context     = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D*        staging     = nullptr;
    RECT                     outputRect{};       // DesktopCoordinates del monitor duplicado
    UINT                     stagingW = 0, stagingH = 0;

    HWND cropHwnd = nullptr; // si no es null, recortar al rectangulo de esta ventana

    std::vector<uint8_t> frameRGBA;
    int  frameW = 0, frameH = 0;
    bool hasFrame = false;

    ~Impl() { Close(); }

    void Close() {
        if (staging)     { staging->Release();     staging     = nullptr; }
        if (duplication) { duplication->Release();  duplication = nullptr; }
        if (context)     { context->Release();      context     = nullptr; }
        if (device)      { device->Release();       device      = nullptr; }
        cropHwnd = nullptr;
        hasFrame = false;
        stagingW = stagingH = 0;
        frameW = frameH = 0;
    }

    // Busca, entre todos los adaptadores/salidas del sistema, el monitor
    // cuyo rectangulo de escritorio contiene (x,y), y lo duplica.
    bool OpenOutputContaining(int x, int y) {
        Close();

        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory))))
            return false;

        bool found = false;
        for (UINT ai = 0; !found; ++ai) {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(ai, &adapter) == DXGI_ERROR_NOT_FOUND) break;

            for (UINT oi = 0; !found; ++oi) {
                IDXGIOutput* output = nullptr;
                if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND) break;

                DXGI_OUTPUT_DESC desc{};
                if (SUCCEEDED(output->GetDesc(&desc)) &&
                    x >= desc.DesktopCoordinates.left && x < desc.DesktopCoordinates.right &&
                    y >= desc.DesktopCoordinates.top  && y < desc.DesktopCoordinates.bottom)
                {
                    outputRect = desc.DesktopCoordinates;

                    D3D_FEATURE_LEVEL levels[] = {
                        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
                    };
                    D3D_FEATURE_LEVEL got{};
                    if (SUCCEEDED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                            levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &device, &got, &context)))
                    {
                        IDXGIOutput1* output1 = nullptr;
                        if (SUCCEEDED(output->QueryInterface(IID_IDXGIOutput1,
                                reinterpret_cast<void**>(&output1))))
                        {
                            found = SUCCEEDED(output1->DuplicateOutput(device, &duplication));
                            output1->Release();
                        }
                        if (!found) {
                            context->Release(); context = nullptr;
                            device->Release();  device  = nullptr;
                        }
                    }
                }
                output->Release();
            }
            adapter->Release();
        }
        factory->Release();
        return found;
    }

    const uint8_t* Grab(int& w, int& h) {
        if (!duplication) return nullptr;

        IDXGIResource* resource = nullptr;
        DXGI_OUTDUPL_FRAME_INFO info{};
        HRESULT hr = duplication->AcquireNextFrame(200, &info, &resource);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // Nada cambio en pantalla desde el ultimo grab -- no es un
            // error, sostenemos el ultimo frame ya decodificado.
            if (!hasFrame) return nullptr;
            w = frameW; h = frameH;
            return frameRGBA.data();
        }
        if (FAILED(hr)) {
            // La duplicacion se invalido (cambio de resolucion/modo,
            // bloqueo de pantalla, UAC, etc.) -- hay que recrearla desde
            // OpenMonitor/OpenWindow; señalamos fallo total.
            return nullptr;
        }

        ID3D11Texture2D* acquired = nullptr;
        resource->QueryInterface(IID_ID3D11Texture2D, reinterpret_cast<void**>(&acquired));
        resource->Release();
        if (!acquired) { duplication->ReleaseFrame(); return nullptr; }

        D3D11_TEXTURE2D_DESC desc{};
        acquired->GetDesc(&desc);

        if (!staging || stagingW != desc.Width || stagingH != desc.Height) {
            if (staging) { staging->Release(); staging = nullptr; }
            D3D11_TEXTURE2D_DESC sd = desc;
            sd.Usage          = D3D11_USAGE_STAGING;
            sd.BindFlags      = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags      = 0;
            if (FAILED(device->CreateTexture2D(&sd, nullptr, &staging))) {
                acquired->Release();
                duplication->ReleaseFrame();
                return nullptr;
            }
            stagingW = desc.Width; stagingH = desc.Height;
        }

        context->CopyResource(staging, acquired);
        acquired->Release();
        duplication->ReleaseFrame(); // liberar cuanto antes: ya tenemos nuestra copia

        // Rectangulo a recortar, relativo al monitor duplicado. Sin
        // cropHwnd (captura de monitor completo), es el monitor entero.
        RECT crop{ 0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height) };
        if (cropHwnd) {
            RECT winRect{};
            if (FAILED(DwmGetWindowAttribute(cropHwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                              &winRect, sizeof(winRect)))) {
                GetWindowRect(cropHwnd, &winRect);
            }
            crop.left   = std::clamp<LONG>(winRect.left   - outputRect.left, 0, static_cast<LONG>(desc.Width));
            crop.top    = std::clamp<LONG>(winRect.top    - outputRect.top,  0, static_cast<LONG>(desc.Height));
            crop.right  = std::clamp<LONG>(winRect.right  - outputRect.left, 0, static_cast<LONG>(desc.Width));
            crop.bottom = std::clamp<LONG>(winRect.bottom - outputRect.top,  0, static_cast<LONG>(desc.Height));
        }
        int cw = crop.right - crop.left;
        int ch = crop.bottom - crop.top;
        if (cw <= 0 || ch <= 0)
            return hasFrame ? (w = frameW, h = frameH, frameRGBA.data()) : nullptr;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
            return hasFrame ? (w = frameW, h = frameH, frameRGBA.data()) : nullptr;

        frameRGBA.assign(static_cast<size_t>(cw) * ch * 4, 0);
        const uint8_t* base = static_cast<const uint8_t*>(mapped.pData);
        for (int y = 0; y < ch; ++y) {
            const uint8_t* rowSrc = base + static_cast<size_t>(crop.top + y) * mapped.RowPitch
                                          + static_cast<size_t>(crop.left) * 4;
            uint8_t* rowDst = frameRGBA.data() + static_cast<size_t>(y) * cw * 4;
            for (int x = 0; x < cw; ++x) {
                // DXGI entrega BGRA; OpenGL espera RGBA.
                rowDst[x * 4 + 0] = rowSrc[x * 4 + 2];
                rowDst[x * 4 + 1] = rowSrc[x * 4 + 1];
                rowDst[x * 4 + 2] = rowSrc[x * 4 + 0];
                rowDst[x * 4 + 3] = 255;
            }
        }
        context->Unmap(staging, 0);

        frameW = cw; frameH = ch; hasFrame = true;
        w = cw; h = ch;
        return frameRGBA.data();
    }
};

Win32ScreenCapture::Win32ScreenCapture() : m_Impl(std::make_unique<Impl>()) {}
Win32ScreenCapture::~Win32ScreenCapture() = default;

bool Win32ScreenCapture::OpenMonitor(int x, int y) {
    return m_Impl->OpenOutputContaining(x, y);
}

bool Win32ScreenCapture::OpenWindow(void* hwnd) {
    HWND h = static_cast<HWND>(hwnd);
    RECT r{};
    if (!GetWindowRect(h, &r)) return false;
    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;
    if (!m_Impl->OpenOutputContaining(cx, cy)) return false;
    m_Impl->cropHwnd = h;
    return true;
}

void Win32ScreenCapture::Close() {
    m_Impl->Close();
}

const uint8_t* Win32ScreenCapture::GrabFrame(int& w, int& h) {
    return m_Impl->Grab(w, h);
}

} // namespace ProyecThor::UI

#endif // _WIN32
