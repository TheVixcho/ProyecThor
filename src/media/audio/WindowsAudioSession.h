#pragma once

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#pragma comment(lib, "ole32.lib")

namespace ProyecThor::Audio {

inline void EnsureProcessAudioUnmutedAndMaxVolume()
{
    // COM por hilo (idempotente).
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)hrInit;

    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en))) || !en)
        return;

    IMMDevice* dev = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev)) && dev) {
        IAudioSessionManager2* mgr = nullptr;
        if (SUCCEEDED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(&mgr))) && mgr) {
            IAudioSessionEnumerator* sessions = nullptr;
            if (SUCCEEDED(mgr->GetSessionEnumerator(&sessions)) && sessions) {
                int count = 0;
                sessions->GetCount(&count);
                const DWORD myPid = GetCurrentProcessId();
                for (int i = 0; i < count; ++i) {
                    IAudioSessionControl* ctl = nullptr;
                    if (FAILED(sessions->GetSession(i, &ctl)) || !ctl) continue;
                    IAudioSessionControl2* ctl2 = nullptr;
                    if (SUCCEEDED(ctl->QueryInterface(__uuidof(IAudioSessionControl2),
                                                      reinterpret_cast<void**>(&ctl2))) && ctl2) {
                        DWORD pid = 0;
                        if (SUCCEEDED(ctl2->GetProcessId(&pid)) && pid == myPid) {
                            ctl2->SetDisplayName(L"ProyecThor", NULL);
                            ISimpleAudioVolume* sav = nullptr;
                            if (SUCCEEDED(ctl->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                              reinterpret_cast<void**>(&sav))) && sav) {
                                BOOL isMuted = FALSE;
                                if (SUCCEEDED(sav->GetMute(&isMuted)) && isMuted) {
                                    sav->SetMute(FALSE, NULL);
                                }
                                float masterVol = 1.0f;
                                if (SUCCEEDED(sav->GetMasterVolume(&masterVol)) && masterVol < 0.999f) {
                                    sav->SetMasterVolume(1.0f, NULL);
                                }
                                sav->Release();
                            }
                        }
                        ctl2->Release();
                    }
                    ctl->Release();
                }
                sessions->Release();
            }
            mgr->Release();
        }
        dev->Release();
    }
    en->Release();
}

} // namespace ProyecThor::Audio
#else
namespace ProyecThor::Audio {
inline void EnsureProcessAudioUnmutedAndMaxVolume() {}
}
#endif
