#include "VoiceAssistant.h"
#include "core/PresentationCore.h"
#include "core/AppPaths.h"
#include "core/io/BibleXmlIO.h"
#include "ui/views/biblia/BibleBookData.h"
#include "ui/views/biblia/BibleSearch.h"
#include "ui/views/biblia/BibleTextUtils.h"

#if defined(_WIN32)
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#pragma comment(lib, "windowsapp")
#include <windows.h>
#include <mmeapi.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Media.SpeechRecognition.h>
#include <winrt/Windows.Storage.h>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <regex>

static const CLSID CLSID_PolicyConfigClient =
    { 0x870c3566, 0x037f, 0x4924, { 0xbe, 0x32, 0x09, 0x0e, 0x78, 0x50, 0x95, 0xc6 } };
static const IID IID_IPolicyConfig =
    { 0xf8679f50, 0x850a, 0x41cf, { 0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8 } };

interface DECLSPEC_UUID("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole role) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
#endif

#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <iostream>
#include <unordered_set>

namespace ProyecThor::Core {

namespace {

// Reemplaza todas las ocurrencias de 'from' por 'to' en 'str'
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// Normaliza números hablados y términos religiosos a formato estándar
std::string NormalizeSpokenSpanish(const std::string& raw) {
    std::string s = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(raw));

    // Limpieza de caracteres de puntuación conflictivos
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == ',' || c == ';' || c == '?' || c == '!' || c == '¿' || c == '¡' || c == '\n' || c == '\r' || c == '"' || c == '(' || c == ')') {
            s[i] = ' ';
        } else if (c == '.') {
            // Si es un punto entre dígitos (ej: "1.3"), convertirlo a ":"
            if (i > 0 && std::isdigit((unsigned char)s[i-1]) &&
                i + 1 < s.size() && std::isdigit((unsigned char)s[i+1])) {
                s[i] = ':';
            } else {
                s[i] = ' ';
            }
        }
    }

    // Prefijos de libros
    s = ReplaceAll(s, "primera de ", "1 ");
    s = ReplaceAll(s, "primer libro de ", "1 ");
    s = ReplaceAll(s, "primero de ", "1 ");
    s = ReplaceAll(s, "primer ", "1 ");
    s = ReplaceAll(s, "1ra de ", "1 ");
    s = ReplaceAll(s, "1ra ", "1 ");

    s = ReplaceAll(s, "segunda de ", "2 ");
    s = ReplaceAll(s, "segundo libro de ", "2 ");
    s = ReplaceAll(s, "segundo de ", "2 ");
    s = ReplaceAll(s, "segundo ", "2 ");
    s = ReplaceAll(s, "2da de ", "2 ");
    s = ReplaceAll(s, "2da ", "2 ");

    s = ReplaceAll(s, "tercera de ", "3 ");
    s = ReplaceAll(s, "tercero de ", "3 ");
    s = ReplaceAll(s, "tercero ", "3 ");
    s = ReplaceAll(s, "3ra de ", "3 ");
    s = ReplaceAll(s, "3ra ", "3 ");

    s = ReplaceAll(s, "el libro de ", "");
    s = ReplaceAll(s, "libro de ", "");
    s = ReplaceAll(s, "epistola a los ", "");
    s = ReplaceAll(s, "carta a los ", "");
    s = ReplaceAll(s, "san ", "");

    // Conectores hablados
    s = ReplaceAll(s, "capitulo ", " ");
    s = ReplaceAll(s, "versiculo ", ":");
    s = ReplaceAll(s, "versiculos ", ":");
    s = ReplaceAll(s, "verso ", ":");
    s = ReplaceAll(s, "versos ", ":");

    // Centenas
    s = ReplaceAll(s, "ciento cincuenta", "150");
    s = ReplaceAll(s, "ciento cuarenta", "140");
    s = ReplaceAll(s, "ciento treinta", "130");
    s = ReplaceAll(s, "ciento veinte", "120");
    s = ReplaceAll(s, "ciento diez", "110");
    s = ReplaceAll(s, "ciento", "100");
    s = ReplaceAll(s, "cien", "100");

    // Decenas compuestas ("treinta y tres" -> "33", etc.)
    struct DecenaEntry { const char* name; int base; };
    static const DecenaEntry kDecenas[] = {
        { "noventa y ", 90 }, { "ochenta y ", 80 }, { "setenta y ", 70 },
        { "sesenta y ", 60 }, { "cincuenta y ", 50 }, { "cuarenta y ", 40 },
        { "treinta y ", 30 }
    };
    struct UnidadEntry { const char* name; int val; };
    static const UnidadEntry kUnidades[] = {
        { "nueve", 9 }, { "ocho", 8 }, { "siete", 7 }, { "seis", 6 },
        { "cinco", 5 }, { "cuatro", 4 }, { "tres", 3 }, { "dos", 2 },
        { "uno", 1 }, { "un", 1 }, { "una", 1 }
    };

    for (const auto& dec : kDecenas) {
        for (const auto& u : kUnidades) {
            std::string pat = std::string(dec.name) + u.name;
            std::string rep = std::to_string(dec.base + u.val);
            s = ReplaceAll(s, pat, rep);
        }
    }

    // Decenas redondas
    s = ReplaceAll(s, "noventa", "90");
    s = ReplaceAll(s, "ochenta", "80");
    s = ReplaceAll(s, "setenta", "70");
    s = ReplaceAll(s, "sesenta", "60");
    s = ReplaceAll(s, "cincuenta", "50");
    s = ReplaceAll(s, "cuarenta", "40");
    s = ReplaceAll(s, "treinta", "30");

    // Veintes
    s = ReplaceAll(s, "veintinueve", "29");
    s = ReplaceAll(s, "veintiocho", "28");
    s = ReplaceAll(s, "veintisiete", "27");
    s = ReplaceAll(s, "veintiseis", "26");
    s = ReplaceAll(s, "veinticinco", "25");
    s = ReplaceAll(s, "veinticuatro", "24");
    s = ReplaceAll(s, "veintitres", "23");
    s = ReplaceAll(s, "veintidos", "22");
    s = ReplaceAll(s, "veintiuno", "21");
    s = ReplaceAll(s, "veintiun", "21");
    s = ReplaceAll(s, "veinte", "20");

    // Dieces
    s = ReplaceAll(s, "diecinueve", "19");
    s = ReplaceAll(s, "dieciocho", "18");
    s = ReplaceAll(s, "diecisiete", "17");
    s = ReplaceAll(s, "dieciseis", "16");
    s = ReplaceAll(s, "quince", "15");
    s = ReplaceAll(s, "catorce", "14");
    s = ReplaceAll(s, "trece", "13");
    s = ReplaceAll(s, "doce", "12");
    s = ReplaceAll(s, "once", "11");
    s = ReplaceAll(s, "diez", "10");

    // Unidades sueltas
    auto replaceWord = [](std::string& target, const std::string& word, const std::string& rep) {
        size_t pos = 0;
        while ((pos = target.find(word, pos)) != std::string::npos) {
            bool leftOk = (pos == 0 || target[pos - 1] == ' ' || target[pos - 1] == ':');
            bool rightOk = (pos + word.length() == target.length() || target[pos + word.length()] == ' ' || target[pos + word.length()] == ':');
            if (leftOk && rightOk) {
                target.replace(pos, word.length(), rep);
                pos += rep.length();
            } else {
                pos += word.length();
            }
        }
    };

    replaceWord(s, "nueve", "9");
    replaceWord(s, "ocho", "8");
    replaceWord(s, "siete", "7");
    replaceWord(s, "seis", "6");
    replaceWord(s, "cinco", "5");
    replaceWord(s, "cuatro", "4");
    replaceWord(s, "tres", "3");
    replaceWord(s, "dos", "2");
    replaceWord(s, "uno", "1");
    replaceWord(s, "una", "1");

    // Arreglar espacios alrededor de dos puntos
    s = ReplaceAll(s, " : ", ":");
    s = ReplaceAll(s, " :", ":");
    s = ReplaceAll(s, ": ", ":");

    return s;
}

// Tokeniza una cadena en palabras
std::vector<std::string> ExtractWords(const std::string& text, size_t minLength = 3) {
    std::string clean = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(text));
    std::vector<std::string> words;
    std::string cur;
    for (char c : clean) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            cur += c;
        } else {
            if (cur.length() >= minLength) words.push_back(cur);
            cur.clear();
        }
    }
    if (cur.length() >= minLength) words.push_back(cur);
    return words;
}

#if defined(_WIN32)
// Busca dinámicamente cualquier paquete de reconocimiento de voz en español instalado en Windows (es-MX, es-ES, es-US, es-CO, etc.)
winrt::Windows::Globalization::Language ResolveBestSpanishLanguage() {
    using namespace winrt::Windows::Media::SpeechRecognition;
    using namespace winrt::Windows::Globalization;

    // 1. Revisar idiomas de temas soportados (TopicLanguages)
    try {
        for (auto const& lang : SpeechRecognizer::SupportedTopicLanguages()) {
            std::wstring tag = lang.LanguageTag().c_str();
            if (tag.rfind(L"es", 0) == 0) {
                return lang;
            }
        }
    } catch (...) {}

    // 2. Revisar idiomas de gramática soportados (GrammarLanguages)
    try {
        for (auto const& lang : SpeechRecognizer::SupportedGrammarLanguages()) {
            std::wstring tag = lang.LanguageTag().c_str();
            if (tag.rfind(L"es", 0) == 0) {
                return lang;
            }
        }
    } catch (...) {}

    // 3. Revisar si el idioma de voz del sistema es español
    try {
        auto sysLang = SpeechRecognizer::SystemSpeechLanguage();
        if (sysLang) {
            std::wstring tag = sysLang.LanguageTag().c_str();
            if (tag.rfind(L"es", 0) == 0) {
                return sysLang;
            }
        }
    } catch (...) {}

    return nullptr;
}
#endif

} // namespace

VoiceAssistant& VoiceAssistant::Get() {
    static VoiceAssistant instance;
    return instance;
}

VoiceAssistant::VoiceAssistant() {
    // Precargar datos en segundo plano para que cualquier PC (incluso de bajos recursos) arranque al instante
    std::thread([this]() {
        EnsureBibleLoaded();
        EnsureSongsCached();
    }).detach();
}

VoiceAssistant::~VoiceAssistant() {
    Stop();
}

std::vector<AudioInputDeviceInfo> VoiceAssistant::GetAvailableMicrophones() {
    std::vector<AudioInputDeviceInfo> list;
#if defined(_WIN32)
    HRESULT hrInit = CoInitialize(NULL);

    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr) || !pEnum) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return list;
    }

    IMMDevice* pDefaultDev = nullptr;
    std::wstring defaultId;
    if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &pDefaultDev)) && pDefaultDev) {
        LPWSTR pDefId = nullptr;
        if (SUCCEEDED(pDefaultDev->GetId(&pDefId)) && pDefId) {
            defaultId = pDefId;
            CoTaskMemFree(pDefId);
        }
        pDefaultDev->Release();
    }

    IMMDeviceCollection* pCol = nullptr;
    if (SUCCEEDED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED, &pCol)) && pCol) {
        UINT count = 0;
        pCol->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pCol->Item(i, &pDev)) && pDev) {
                LPWSTR pId = nullptr;
                std::wstring wid;
                if (SUCCEEDED(pDev->GetId(&pId)) && pId) {
                    wid = pId;
                    CoTaskMemFree(pId);
                }

                DWORD dwState = 0;
                pDev->GetState(&dwState);

                IPropertyStore* pStore = nullptr;
                std::string friendlyName = "Dispositivo " + std::to_string(i + 1);
                if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pStore)) && pStore) {
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    if (SUCCEEDED(pStore->GetValue(PKEY_Device_FriendlyName, &var)) && var.pwszVal) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, NULL, 0, NULL, NULL);
                        if (sz > 0) {
                            friendlyName.resize(sz - 1);
                            WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, &friendlyName[0], sz, NULL, NULL);
                        }
                    }
                    PropVariantClear(&var);
                    pStore->Release();
                }

                std::string lowName = friendlyName;
                std::transform(lowName.begin(), lowName.end(), lowName.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                bool isDesktop = (lowName.find("mezcla") != std::string::npos ||
                                  lowName.find("stereo mix") != std::string::npos ||
                                  lowName.find("cable output") != std::string::npos ||
                                  lowName.find("what u hear") != std::string::npos ||
                                  lowName.find("loopback") != std::string::npos ||
                                  lowName.find("voicemeeter") != std::string::npos ||
                                  lowName.find("audio del escritorio") != std::string::npos);

                // Si es un dispositivo de mezcla de escritorio y estaba deshabilitado, activarlo automáticamente
                if (dwState == DEVICE_STATE_DISABLED && isDesktop && !wid.empty()) {
                    IPolicyConfig* pPolicyConfig = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_PolicyConfigClient, NULL, CLSCTX_ALL, IID_IPolicyConfig, (void**)&pPolicyConfig)) && pPolicyConfig) {
                        pPolicyConfig->SetEndpointVisibility(wid.c_str(), 1);
                        pPolicyConfig->Release();
                        dwState = DEVICE_STATE_ACTIVE;
                    }
                }

                if (dwState == DEVICE_STATE_ACTIVE) {
                    std::string displayName = friendlyName;
                    if (isDesktop && displayName.find("Audio del Escritorio") == std::string::npos) {
                        displayName += " [Audio del Escritorio]";
                    }
                    int idSz = WideCharToMultiByte(CP_UTF8, 0, wid.c_str(), -1, NULL, 0, NULL, NULL);
                    std::string utf8Id;
                    if (idSz > 0) {
                        utf8Id.resize(idSz - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wid.c_str(), -1, &utf8Id[0], idSz, NULL, NULL);
                    }

                    list.push_back({ utf8Id, displayName, wid == defaultId, isDesktop, false });
                }
                pDev->Release();
            }
        }
        pCol->Release();
    }

    // Enumerar dispositivos de salida de audio (auriculares, altavoces, escritorio)
    // para que el usuario pueda seleccionarlos directamente como entrada de audio.
    IMMDeviceCollection* pRenderCol = nullptr;
    if (SUCCEEDED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderCol)) && pRenderCol) {
        UINT rCount = 0;
        pRenderCol->GetCount(&rCount);
        for (UINT i = 0; i < rCount; ++i) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pRenderCol->Item(i, &pDev)) && pDev) {
                LPWSTR pId = nullptr;
                std::wstring wid;
                if (SUCCEEDED(pDev->GetId(&pId)) && pId) {
                    wid = pId;
                    CoTaskMemFree(pId);
                }

                IPropertyStore* pStore = nullptr;
                std::string friendlyName = "Salida " + std::to_string(i + 1);
                if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pStore)) && pStore) {
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    if (SUCCEEDED(pStore->GetValue(PKEY_Device_FriendlyName, &var)) && var.pwszVal) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, NULL, 0, NULL, NULL);
                        if (sz > 0) {
                            friendlyName.resize(sz - 1);
                            WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, &friendlyName[0], sz, NULL, NULL);
                        }
                    }
                    PropVariantClear(&var);
                    pStore->Release();
                }

                int idSz = WideCharToMultiByte(CP_UTF8, 0, wid.c_str(), -1, NULL, 0, NULL, NULL);
                std::string utf8Id;
                if (idSz > 0) {
                    utf8Id.resize(idSz - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wid.c_str(), -1, &utf8Id[0], idSz, NULL, NULL);
                }

                std::string displayName = "🎧 " + friendlyName + " [Salida / Escritorio]";
                list.push_back({ "render::" + utf8Id, displayName, false, true, true });
                pDev->Release();
            }
        }
        pRenderCol->Release();
    }

    pEnum->Release();

    if (SUCCEEDED(hrInit)) CoUninitialize();
#endif
    return list;
}

bool VoiceAssistant::HasDesktopAudioDevice() {
    auto mics = GetAvailableMicrophones();
    for (const auto& m : mics) {
        if (m.isDesktopAudio && !m.isRenderDevice) return true;
    }
    return false;
}

std::string VoiceAssistant::GetDesktopAudioDeviceId() {
    auto mics = GetAvailableMicrophones();
    for (const auto& m : mics) {
        if (m.isDesktopAudio && !m.isRenderDevice) return m.id;
    }
    return {};
}

bool VoiceAssistant::SelectDesktopAudioDevice() {
    std::string id = GetDesktopAudioDeviceId();
    if (!id.empty()) {
        SetSelectedMicrophone(id);
        return true;
    }
    return false;
}

bool VoiceAssistant::IsDesktopAudioSelected() {
    std::string sel = GetSelectedMicrophone();
    if (sel.rfind("render::", 0) == 0) return true;
    auto mics = GetAvailableMicrophones();
    for (const auto& m : mics) {
        if ((m.id == sel || (sel.empty() && m.isDefault)) && m.isDesktopAudio) return true;
    }
    return false;
}

void VoiceAssistant::SetSelectedMicrophone(const std::string& deviceId) {
    if (deviceId.empty()) return;

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        if (m_SelectedMicrophoneId == deviceId) return;
        m_SelectedMicrophoneId = deviceId;
    }

    bool wasRunning = m_Running.load();
    if (wasRunning) {
        Stop();
    }

#if defined(_WIN32)
    std::string targetCaptureId = deviceId;
    if (deviceId.rfind("render::", 0) == 0) {
        // Dispositivo de salida/auriculares seleccionado como entrada:
        // Se asocia automáticamente al dispositivo de captura de mezcla/escritorio (Stereo Mix, Cable Output, etc.)
        std::string loopbackId = GetDesktopAudioDeviceId();
        if (!loopbackId.empty()) {
            targetCaptureId = loopbackId;
        }
    }

    if (targetCaptureId.rfind("render::", 0) != 0) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, targetCaptureId.c_str(), -1, NULL, 0);
        if (wlen > 0) {
            std::wstring wid(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, targetCaptureId.c_str(), -1, &wid[0], wlen);

            HRESULT hrInit = CoInitialize(NULL);
            IPolicyConfig* pPolicyConfig = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_PolicyConfigClient, NULL, CLSCTX_ALL,
                                          IID_IPolicyConfig, (void**)&pPolicyConfig);
            if (SUCCEEDED(hr) && pPolicyConfig) {
                pPolicyConfig->SetDefaultEndpoint(wid.c_str(), eConsole);
                pPolicyConfig->SetDefaultEndpoint(wid.c_str(), eCommunications);
                pPolicyConfig->Release();
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }
    }
#endif

    if (wasRunning) {
        Start();
    }
}

std::string VoiceAssistant::GetSelectedMicrophone() const {
    std::lock_guard<std::mutex> lk(m_Mutex);
    return m_SelectedMicrophoneId;
}

void VoiceAssistant::EnsureBibleLoaded() {
    std::lock_guard<std::recursive_mutex> lk(m_BibleMutex);
    if (m_BibleLoaded && !m_BibleData.books.empty()) return;

    std::vector<std::string> candidates = {
        BiblesPath() + "SpanishRVR1960Bible.xml",
        BiblesPath() + "/SpanishRVR1960Bible.xml",
        GetAssetsPath() + "/bibles/SpanishRVR1960Bible.xml",
        "bin/assets/bibles/SpanishRVR1960Bible.xml",
        "assets/bin/assets/bibles/SpanishRVR1960Bible.xml",
        "assets/bibles/SpanishRVR1960Bible.xml"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            if (UI::XmlIO::LoadBible(path, m_BibleData) && !m_BibleData.books.empty()) {
                m_BibleLoaded = true;
                return;
            }
        }
    }

    std::vector<std::string> searchDirs = { BiblesPath(), "assets/bin/assets/bibles", "bin/assets/bibles" };
    for (const auto& dir : searchDirs) {
        if (std::filesystem::exists(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".xml") {
                    if (UI::XmlIO::LoadBible(entry.path().string(), m_BibleData) && !m_BibleData.books.empty()) {
                        m_BibleLoaded = true;
                        return;
                    }
                }
            }
        }
    }
}

void VoiceAssistant::EnsureSongsCached() {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lk(m_SongsMutex);

    if (!m_CachedSongs.empty() && (now - m_LastSongsCacheTime < 45.0)) {
        return;
    }

    std::vector<std::string> dirs = {
        SongsPath(),
        GetAssetsPath() + "/songs",
        "assets/songs",
        "assets/bin/assets/songs",
        "bin/assets/songs"
    };

    std::vector<CachedVoiceSong> newCache;
    std::unordered_set<std::string> seenFiles;

    for (const auto& d : dirs) {
        if (!std::filesystem::exists(d)) continue;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(d)) {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                    std::string fname = entry.path().filename().string();
                    if (seenFiles.count(fname)) continue;
                    seenFiles.insert(fname);

                    std::ifstream f(entry.path().string());
                    if (!f.is_open()) continue;

                    std::vector<std::string> stanzas;
                    std::vector<std::vector<std::string>> stanzaWords;
                    std::string line, curStanza;

                    while (std::getline(f, line)) {
                        if (line.empty() || line == "\r") {
                            if (!curStanza.empty()) {
                                stanzas.push_back(curStanza);
                                stanzaWords.push_back(ExtractWords(curStanza, 3));
                                curStanza.clear();
                            }
                        } else {
                            if (!curStanza.empty()) curStanza += "\n";
                            curStanza += line;
                        }
                    }
                    if (!curStanza.empty()) {
                        stanzas.push_back(curStanza);
                        stanzaWords.push_back(ExtractWords(curStanza, 3));
                    }

                    if (!stanzas.empty()) {
                        CachedVoiceSong cs;
                        cs.filename = fname;
                        cs.title = entry.path().stem().string();
                        cs.stanzas = std::move(stanzas);
                        cs.stanzaWords = std::move(stanzaWords);
                        newCache.push_back(std::move(cs));
                    }
                }
            }
        } catch (...) {}
    }

    m_CachedSongs = std::move(newCache);
    m_LastSongsCacheTime = now;
}

bool VoiceAssistant::Start() {
    if (m_Running.load()) return true;

#if !defined(_WIN32)
    m_LastError = "El reconocimiento de voz nativo por ahora solo esta disponible en Windows.";
    return false;
#else
    // Verificar si hay micrófonos disponibles antes de arrancar
    auto mics = GetAvailableMicrophones();
    if (mics.empty()) {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_LastError = "NO_MICROPHONE_FOUND";
        return false;
    }

    m_Running = true;
    m_LastError.clear();

    if (m_WorkerThread.joinable())
        m_WorkerThread.join();

    m_WorkerThread = std::thread([this]() { RunRecognitionLoop(); });

    return true;
#endif
}

#if defined(_WIN32)
namespace {

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string out(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], sz, NULL, NULL);
    return out;
}

double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring out(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], sz);
    return out;
}

std::string XmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (char c : in) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '\"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string SanitizePhraseForXml(const std::string& in) {
    std::string unaccented = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(in));
    std::string clean;
    for (char c : unaccented) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || uc == ' ') {
            clean += c;
        } else {
            clean += ' ';
        }
    }
    std::string collapsed;
    bool lastSpace = true;
    for (char c : clean) {
        if (c == ' ') {
            if (!lastSpace) collapsed += ' ';
            lastSpace = true;
        } else {
            collapsed += c;
            lastSpace = false;
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
    while (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
    return XmlEscape(collapsed);
}

std::string NumbersToWord(int n) {
    static const char* kWords[] = {
        "cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve",
        "diez", "once", "doce", "trece", "catorce", "quince", "dieciséis", "diecisiete", "dieciocho", "diecinueve", "veinte"
    };
    if (n >= 0 && n <= 20) return kWords[n];
    return std::to_string(n);
}

// Clasifica un error del motor. Devuelve true si es FATAL (no tiene sentido reintentar:
// requiere que el usuario cambie la configuracion de Windows).
bool ClassifySpeechError(const winrt::hresult_error& err, std::string& outCode) {
    std::string msg = WideToUtf8(std::wstring(err.message().c_str()));
    std::string low = msg;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    uint32_t code = (uint32_t)err.code();

    if (code == 0x80045509 || low.find("privacy policy") != std::string::npos) {
        outCode = "PRIVACY_POLICY_REQUIRED";
        return true;
    }
    if (code == 0x80070005 || low.find("denied") != std::string::npos || low.find("denegado") != std::string::npos) {
        outCode = "MIC_ACCESS_DENIED";
        return true;
    }
    if (code == 0x80045508 || low.find("no speech input") != std::string::npos) {
        outCode = "NO_MICROPHONE_FOUND";
        return false; // el microfono puede reconectarse: seguir reintentando
    }
    outCode = "Error de microfono: " + msg;
    return false;
}

} // namespace
#endif

std::wstring VoiceAssistant::BuildOfflineGrammarXml(const std::wstring& langTag) {
    wchar_t tempPath[MAX_PATH] = { 0 };
    DWORD len = GetTempPathW(MAX_PATH, tempPath);
    if (len == 0 || len > MAX_PATH) return L"";
    std::wstring xmlPath = std::wstring(tempPath) + L"ProyecThor_SpeechGrammar.xml";

    std::ofstream out(std::filesystem::path(xmlPath), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return L"";

    std::string langStr = WideToUtf8(langTag);
    if (langStr.empty()) langStr = "es-MX";

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<grammar version=\"1.0\" xml:lang=\"" << langStr << "\" mode=\"voice\" root=\"main\"\n";
    out << "         tag-format=\"semantics/1.0\" xmlns=\"http://www.w3.org/2001/06/grammar\">\n\n";

    out << "  <rule id=\"main\" scope=\"public\">\n";
    out << "    <one-of>\n";
    out << "      <item><ruleref uri=\"#bible_ref\"/></item>\n";
    out << "      <item><ruleref uri=\"#navigation\"/></item>\n";
    out << "      <item><ruleref uri=\"#church_phrases\"/></item>\n";
    out << "      <item><ruleref uri=\"#songs\"/></item>\n";
    out << "    </one-of>\n";
    out << "  </rule>\n\n";

    out << "  <rule id=\"bible_ref\">\n";
    out << "    <item><ruleref uri=\"#books\"/></item>\n";
    out << "    <item repeat=\"0-1\"><ruleref uri=\"#cap_word\"/></item>\n";
    out << "    <item><ruleref uri=\"#numbers\"/></item>\n";
    out << "    <item repeat=\"0-1\"><ruleref uri=\"#ver_word\"/></item>\n";
    out << "    <item repeat=\"0-1\"><ruleref uri=\"#numbers\"/></item>\n";
    out << "  </rule>\n\n";

    out << "  <rule id=\"cap_word\">\n";
    out << "    <one-of><item>capitulo</item></one-of>\n";
    out << "  </rule>\n\n";

    out << "  <rule id=\"ver_word\">\n";
    out << "    <one-of><item>versiculo</item><item>verso</item><item>al</item></one-of>\n";
    out << "  </rule>\n\n";

    // ── Libros de la Biblia (normalizados sin acentos para garantizar UTF-8 perfecto) ──
    out << "  <rule id=\"books\">\n    <one-of>\n";
    static const char* kBooks[] = {
        "genesis", "exodo", "levitico", "numeros", "deuteronomio",
        "josue", "jueces", "rut",
        "1 samuel", "primera de samuel", "primero de samuel", "samuel",
        "2 samuel", "segunda de samuel", "segundo de samuel",
        "1 reyes", "primera de reyes", "primero de reyes", "reyes",
        "2 reyes", "segunda de reyes", "segundo de reyes",
        "1 cronicas", "primera de cronicas", "cronicas",
        "2 cronicas", "segunda de cronicas",
        "esdras", "nehemias", "ester", "job",
        "salmos", "salmo", "proverbios", "proverbio", "eclesiastes", "cantares", "cantar de los cantares",
        "isaias", "jeremias", "lamentaciones", "ezequiel", "daniel",
        "oseas", "joel", "amos", "abdias", "jonas", "miqueas",
        "nahum", "habacuc", "sofonias", "hageo", "zacarias", "malaquias",
        "mateo", "marcos", "lucas", "juan", "hechos", "hechos de los apostoles", "romanos",
        "1 corintios", "primera de corintios", "primero de corintios", "primera corintios", "corintios",
        "2 corintios", "segunda de corintios", "segundo de corintios", "segunda corintios",
        "galatas", "efesios", "filipenses", "colosenses",
        "1 tesalonicenses", "primera de tesalonicenses", "primera tesalonicenses", "tesalonicenses",
        "2 tesalonicenses", "segunda de tesalonicenses", "segunda tesalonicenses",
        "1 timoteo", "primera de timoteo", "primera timoteo", "timoteo",
        "2 timoteo", "segunda de timoteo", "segunda timoteo",
        "tito", "filemon", "hebreos", "santiago",
        "1 pedro", "primera de pedro", "primero de pedro", "pedro",
        "2 pedro", "segunda de pedro", "segundo de pedro",
        "1 juan", "primera de juan", "primera juan",
        "2 juan", "segunda de juan", "segunda juan",
        "3 juan", "tercera de juan", "tercera juan",
        "judas", "apocalipsis"
    };
    for (const char* b : kBooks) {
        out << "      <item>" << b << "</item>\n";
    }

    // Agregar nombres adicionales desde la Biblia cargada
    {
        std::lock_guard<std::recursive_mutex> lk(m_BibleMutex);
        for (const auto& b : m_BibleData.books) {
            std::string bClean = SanitizePhraseForXml(b.name);
            if (!bClean.empty()) {
                out << "      <item>" << bClean << "</item>\n";
            }
        }
    }
    out << "    </one-of>\n  </rule>\n\n";

    // ── Números (1 a 150) ──
    out << "  <rule id=\"numbers\">\n    <one-of>\n";
    for (int i = 1; i <= 150; ++i) {
        out << "      <item>" << i << "</item>\n";
    }
    static const char* kNumWords[] = {
        "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve", "diez",
        "once", "doce", "trece", "catorce", "quince", "dieciseis", "diecisiete", "dieciocho", "diecinueve",
        "veinte", "veintiuno", "veintidos", "veintitres", "veinticuatro", "veinticinco",
        "veintiseis", "veintisiete", "veintiocho", "veintinueve",
        "treinta", "cuarenta", "cincuenta", "sesenta", "setenta", "ochenta", "noventa", "cien"
    };
    for (const char* nw : kNumWords) {
        out << "      <item>" << nw << "</item>\n";
    }
    static const char* kDecNames[] = { "treinta y", "cuarenta y", "cincuenta y", "sesenta y", "setenta y", "ochenta y", "noventa y" };
    static const char* kUniNames[] = { "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve" };
    for (const char* dec : kDecNames) {
        for (const char* uni : kUniNames) {
            out << "      <item>" << dec << " " << uni << "</item>\n";
        }
    }
    out << "      <item>ciento uno</item><item>ciento diecinueve</item><item>ciento cincuenta</item>\n";
    out << "    </one-of>\n  </rule>\n\n";

    // ── Navegación ──
    out << "  <rule id=\"navigation\">\n    <one-of>\n";
    static const char* kNav[] = {
        "siguiente", "siguiente diapositiva", "siguiente estrofa", "siguiente verso",
        "anterior", "anterior diapositiva", "anterior estrofa",
        "coro", "ir al coro", "repetir coro",
        "estrofa uno", "estrofa dos", "estrofa tres", "estrofa cuatro", "estrofa cinco", "estrofa seis", "estrofa siete", "estrofa ocho",
        "primera estrofa", "segunda estrofa", "tercera estrofa", "cuarta estrofa", "quinta estrofa", "sexta estrofa",
        "pantalla negra", "pantalla a negro", "pantalla en blanco", "pantalla blanca", "limpiar pantalla", "ocultar texto"
    };
    for (const char* nav : kNav) {
        out << "      <item>" << nav << "</item>\n";
    }
    out << "    </one-of>\n  </rule>\n\n";

    // ── Frases comunes cristianas / litúrgicas ──
    out << "  <rule id=\"church_phrases\">\n    <one-of>\n";
    static const char* kPhrases[] = {
        "dios esta aqui", "gloria a dios", "santo santo santo",
        "cuan grande es dios", "alabare", "aleluya", "amen",
        "cristo vive", "jesucristo", "espiritu santo", "padre celestial",
        "el senor es mi pastor", "nada me faltara", "dios es amor",
        "dios te bendiga", "bendito sea el senor", "te damos gracias"
    };
    for (const char* ph : kPhrases) {
        out << "      <item>" << ph << "</item>\n";
    }
    out << "    </one-of>\n  </rule>\n\n";

    // ── Canciones cargadas ──
    out << "  <rule id=\"songs\">\n    <one-of>\n";
    std::unordered_set<std::string> songItems;
    {
        std::lock_guard<std::mutex> lk(m_SongsMutex);
        for (const auto& song : m_CachedSongs) {
            std::string cleanTitle = SanitizePhraseForXml(song.title);
            if (!cleanTitle.empty() && cleanTitle.size() <= 80 && songItems.insert(cleanTitle).second) {
                out << "      <item>" << cleanTitle << "</item>\n";
            }
            for (const auto& stanza : song.stanzas) {
                std::istringstream stream(stanza);
                std::string line;
                while (std::getline(stream, line)) {
                    std::string cleanLine = SanitizePhraseForXml(line);
                    if (cleanLine.size() >= 4 && cleanLine.size() <= 60) {
                        if (songItems.size() >= 1200) break;
                        if (songItems.insert(cleanLine).second) {
                            out << "      <item>" << cleanLine << "</item>\n";
                        }
                    }
                }
                if (songItems.size() >= 1200) break;
            }
            if (songItems.size() >= 1200) break;
        }
    }
    if (songItems.empty()) {
        out << "      <item>canto de alabanza</item>\n";
    }
    out << "    </one-of>\n  </rule>\n\n";

    out << "</grammar>\n";
    out.close();
    return xmlPath;
}

std::vector<std::string> VoiceAssistant::BuildFallbackWordsList() {
    std::vector<std::string> words = {
        "genesis", "exodo", "levitico", "numeros", "deuteronomio", "josue", "jueces", "rut",
        "samuel", "reyes", "cronicas", "esdras", "nehemias", "ester", "job", "salmos",
        "proverbios", "eclesiastes", "cantares", "isaias", "jeremias", "lamentaciones", "ezequiel",
        "daniel", "oseas", "joel", "amos", "abdias", "jonas", "miqueas", "nahum", "habacuc",
        "sofonias", "hageo", "zacarias", "malaquias", "mateo", "marcos", "lucas", "juan",
        "hechos", "romanos", "corintios", "galatas", "efesios", "filipenses", "colosenses",
        "tesalonicenses", "timoteo", "tito", "filemon", "hebreos", "santiago", "pedro",
        "judas", "apocalipsis",
        "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve", "diez",
        "veinte", "treinta", "cuarenta", "cincuenta", "cien",
        "capitulo", "versiculo", "estrofa", "coro", "siguiente", "anterior",
        "pantalla negra", "pantalla en blanco", "dios esta aqui", "gloria a dios"
    };
    return words;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Supervisor del reconocimiento continuo.
//
//  El motor de Windows termina la sesion por muchos motivos ajenos a nosotros:
//  silencio (TimeoutExceeded), perdida de foco de la ventana o cambio del
//  dispositivo de audio (UserCanceled / Success), microfono desconectado, etc.
//  Para servicios de 1h30 o mas, este bucle:
//    1. Vigila el estado del reconocedor y reanuda la sesion apenas queda en
//       reposo (Idle), sin importar el motivo del cierre.
//    2. Si reanudar falla varias veces seguidas, destruye y recrea el
//       reconocedor completo (con espera progresiva de hasta 5 s).
//    3. Cada ~20 min, aprovechando un momento de silencio, recrea el
//       reconocedor de forma preventiva para evitar degradacion en sesiones
//       de muchas horas.
//  Solo se detiene por pedido del usuario (Stop) o por un error que requiere
//  cambiar la configuracion de Windows (privacidad / permisos).
// ─────────────────────────────────────────────────────────────────────────────
void VoiceAssistant::RunRecognitionLoop() {
#if defined(_WIN32)
    try { EnsureBibleLoaded(); } catch (...) {}
    try { EnsureSongsCached(); } catch (...) {}

    bool apartmentOk = false;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        apartmentOk = true;
    } catch (...) {}

    int consecutiveFailures = 0;

    while (m_Running.load()) {
        bool fatal = false;
        bool generationStarted = false;

        // Bloque anidado: todos los objetos WinRT se destruyen al salir de cada generacion.
        try {
            using namespace winrt;
            using namespace winrt::Windows::Media::SpeechRecognition;

            auto chosenLang = ResolveBestSpanishLanguage();
            const bool hadSpanish = (chosenLang != nullptr);

            SpeechRecognizer recognizer = nullptr;
            if (chosenLang) {
                try { recognizer = SpeechRecognizer(chosenLang); } catch (...) {}
            }
            if (!recognizer) {
                try { recognizer = SpeechRecognizer(SpeechRecognizer::SystemSpeechLanguage()); }
                catch (...) { recognizer = SpeechRecognizer(); }
            }

            std::wstring langTag = chosenLang ? chosenLang.LanguageTag().c_str() : L"es-MX";
            std::wstring grammarPath = BuildOfflineGrammarXml(langTag);
            bool grammarAdded = false;

            if (!grammarPath.empty() && std::filesystem::exists(grammarPath)) {
                try {
                    auto file = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(grammarPath).get();
                    SpeechRecognitionGrammarFileConstraint grammarConstraint(file, L"ProyecThorGrammar");
                    recognizer.Constraints().Append(grammarConstraint);
                    auto compileTest = recognizer.CompileConstraintsAsync().get();
                    if (compileTest.Status() == SpeechRecognitionResultStatus::Success) {
                        grammarAdded = true;
                    } else {
                        recognizer.Constraints().Clear();
                    }
                } catch (...) {
                    try { recognizer.Constraints().Clear(); } catch (...) {}
                }
            }

            if (!grammarAdded) {
                auto fallbackWords = BuildFallbackWordsList();
                std::vector<winrt::hstring> hWords;
                hWords.reserve(fallbackWords.size());
                for (const auto& w : fallbackWords) {
                    hWords.push_back(winrt::to_hstring(w));
                }
                SpeechRecognitionListConstraint listConstraint(winrt::single_threaded_vector<winrt::hstring>(std::move(hWords)), L"ProyecThorList");
                recognizer.Constraints().Append(listConstraint);
                auto compile = recognizer.CompileConstraintsAsync().get();
                if (compile.Status() != SpeechRecognitionResultStatus::Success) {
                    throw winrt::hresult_error(E_FAIL, L"No se pudieron compilar las restricciones de voz.");
                }
            }

            auto session = recognizer.ContinuousRecognitionSession();
            try { session.AutoStopSilenceTimeout(std::chrono::hours(24)); } catch (...) {}

            // Banderas compartidas con los callbacks (shared_ptr: sobreviven aunque llegue un evento tardio).
            auto sessionEnded = std::make_shared<std::atomic<bool>>(false);
            auto micLost      = std::make_shared<std::atomic<bool>>(false);

            auto revState = recognizer.StateChanged(winrt::auto_revoke,
                [this](SpeechRecognizer const&, SpeechRecognizerStateChangedEventArgs const& args) {
                    try {
                        auto st = args.State();
                        if (st == SpeechRecognizerState::SoundStarted ||
                            st == SpeechRecognizerState::SpeechDetected ||
                            st == SpeechRecognizerState::Processing) {
                            std::lock_guard<std::mutex> lk(m_Mutex);
                            m_LastAudioDetectedTime = NowSeconds();
                        }
                    } catch (...) {}
                });

            auto revCompleted = session.Completed(winrt::auto_revoke,
                [sessionEnded, micLost](SpeechContinuousRecognitionSession const&,
                                        SpeechContinuousRecognitionCompletedEventArgs const& args) {
                    try {
                        if (args.Status() == SpeechRecognitionResultStatus::MicrophoneUnavailable)
                            micLost->store(true);
                    } catch (...) {}
                    // Cualquier motivo de cierre (silencio, foco, cancelacion) => reanudar.
                    sessionEnded->store(true);
                });

            auto revHypothesis = recognizer.HypothesisGenerated(winrt::auto_revoke,
                [this](SpeechRecognizer const&, SpeechRecognitionHypothesisGeneratedEventArgs const& args) {
                    try {
                        auto hyp = args.Hypothesis();
                        if (!hyp) return;
                        std::string text = WideToUtf8(std::wstring(hyp.Text().c_str()));
                        if (text.empty()) return;
                        double now = NowSeconds();
                        std::lock_guard<std::mutex> lk(m_Mutex);
                        m_LiveHypothesis        = text;
                        m_LastHypothesisTime    = now;
                        m_LastAudioDetectedTime = now;
                        m_PendingTextToProcess  = text;
                        m_PendingTextIsFinal    = false;
                    } catch (...) {}
                });

            auto revResult = session.ResultGenerated(winrt::auto_revoke,
                [this](SpeechContinuousRecognitionSession const&,
                       SpeechContinuousRecognitionResultGeneratedEventArgs const& args) {
                    try {
                        auto result = args.Result();
                        if (!result || result.Status() != SpeechRecognitionResultStatus::Success) return;
                        std::string text = WideToUtf8(std::wstring(result.Text().c_str()));
                        if (text.empty()) return;
                        std::lock_guard<std::mutex> lk(m_Mutex);
                        // Evita crecimiento ilimitado de memoria en sesiones largas
                        if (text.size() > 2000) text.resize(2000);
                        m_LastHeard            = text;
                        m_LiveHypothesis.clear();
                        m_PendingTextToProcess = text;
                        m_PendingTextIsFinal   = true;
                    } catch (...) {}
                });

            session.StartAsync().get();
            generationStarted   = true;
            consecutiveFailures = 0;
            {
                std::lock_guard<std::mutex> lk(m_Mutex);
                m_LastError = hadSpanish ? std::string() : std::string("NO_SPANISH_SPEECH_PACK");
            }

            const double generationStart = NowSeconds();
            double idleSince   = -1.0;
            int    restartFail = 0;
            bool   rebuild     = false;

            while (m_Running.load() && !rebuild) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!m_Running.load()) break;

                const double now = NowSeconds();
                SpeechRecognizerState st = SpeechRecognizerState::Idle;
                try { st = recognizer.State(); } catch (...) { rebuild = true; break; }

                // ── Watchdog: la sesion termino (por el motivo que sea) → reanudar ──
                const bool ended = sessionEnded->load();
                if (st == SpeechRecognizerState::Idle || ended) {
                    if (idleSince < 0.0) idleSince = now;
                    // Pequena espera para que el motor libere el dispositivo de captura
                    if (now - idleSince >= 0.25) {
                        try {
                            sessionEnded->store(false);
                            session.StartAsync().get();
                            restartFail = 0;
                            idleSince   = -1.0;
                            if (micLost->exchange(false)) {
                                std::lock_guard<std::mutex> lk(m_Mutex);
                                if (m_LastError == "NO_MICROPHONE_FOUND") m_LastError.clear();
                            }
                        } catch (...) {
                            ++restartFail;
                            idleSince = now; // reintentar tras otra espera
                            if (micLost->load()) {
                                std::lock_guard<std::mutex> lk(m_Mutex);
                                m_LastError = "NO_MICROPHONE_FOUND";
                            }
                            if (restartFail >= 3) rebuild = true;
                        }
                    }
                } else {
                    idleSince = -1.0;
                }

                // ── Renovacion preventiva para sesiones de muchas horas ──
                if (!rebuild && (now - generationStart) > 20.0 * 60.0) {
                    double lastAudio;
                    bool   pendingText;
                    {
                        std::lock_guard<std::mutex> lk(m_Mutex);
                        lastAudio   = m_LastAudioDetectedTime;
                        pendingText = !m_PendingTextToProcess.empty() || !m_LiveHypothesis.empty();
                    }
                    if (!pendingText && (now - lastAudio) > 4.0) rebuild = true;
                }
            }

            // Desconectar callbacks ANTES de cancelar para que no lleguen eventos durante la destruccion
            revState.revoke();
            revCompleted.revoke();
            revHypothesis.revoke();
            revResult.revoke();

            try {
                if (recognizer.State() != SpeechRecognizerState::Idle)
                    session.CancelAsync().get();
            } catch (...) {}

            session    = nullptr;
            recognizer = nullptr;
        } catch (const winrt::hresult_error& err) {
            std::string code;
            fatal = ClassifySpeechError(err, code);
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_LastError = code;
        } catch (...) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_LastError = "Error inesperado en reconocimiento de voz.";
        }

        if (fatal) {
            m_Running = false;
            break;
        }
        if (!m_Running.load()) break;

        // Espera progresiva antes de recrear el reconocedor (0.5s, 1s, 2s, 4s, max 5s)
        if (!generationStarted) ++consecutiveFailures;
        int waitMs = generationStarted ? 200 : std::min(5000, 500 * (1 << std::min(consecutiveFailures - 1, 4)));
        for (int t = 0; t < waitMs && m_Running.load(); t += 50)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_LiveHypothesis.clear();
    }

    if (apartmentOk) {
        try { winrt::uninit_apartment(); } catch (...) {}
    }
#endif
}

void VoiceAssistant::Stop() {
    if (!m_Running.load()) return;
    m_Running = false;
    if (m_WorkerThread.joinable()) {
        m_WorkerThread.join();
    }
    std::lock_guard<std::mutex> lk(m_Mutex);
    m_LiveHypothesis.clear();
}

std::string VoiceAssistant::GetLastHeardText() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_LastHeard;
}

std::string VoiceAssistant::GetLiveHypothesis() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_LiveHypothesis;
}

bool VoiceAssistant::IsReceivingAudio() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return (now - m_LastAudioDetectedTime < 1.2) || (!m_LiveHypothesis.empty() && (now - m_LastHypothesisTime < 1.5));
}

bool VoiceAssistant::HasPendingMatch() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_HasPendingMatch;
}

VoiceMatchResult VoiceAssistant::GetPendingMatch() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_PendingMatch;
}

void VoiceAssistant::ConfirmPendingMatch() {
    VoiceMatchResult match;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_HasPendingMatch) return;
        match = m_PendingMatch;
        m_HasPendingMatch = false;
    }

    auto& core = Core::PresentationCore::Get();
    if (match.type == VoiceMatchType::SongVerse) {
        auto sel = core.PeekSelection();
        if (sel.type == ItemType::Song && match.slideIndex >= 0 && match.slideIndex < (int)sel.contentData.size()) {
            core.SetLayer2_Text(sel.contentData[match.slideIndex], match.slideIndex);
            core.SetActiveSlideIndex(match.slideIndex);
            core.SetProjecting(true);
        }
    } else if (match.type == VoiceMatchType::BibleVerse) {
        core.SetLayer2_Text(match.body);
        core.SetCurrentRef(match.ref);
        core.SetProjecting(true);

        if (match.bibleBookIdx >= 0 && match.bibleBookIdx < (int)m_BibleData.books.size()) {
            int bNum = m_BibleData.books[match.bibleBookIdx].canonicalNumber;
            int cNum = (match.bibleChapIdx >= 0 && match.bibleChapIdx < (int)m_BibleData.books[match.bibleBookIdx].chapters.size())
                       ? m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].number : 1;
            int vNum = (match.bibleChapIdx >= 0 && match.bibleVerseIdx >= 0 &&
                        match.bibleVerseIdx < (int)m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].verses.size())
                       ? m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].verses[match.bibleVerseIdx].number : 1;
            core.RequestBibleJump(bNum, cNum, vNum);
        }
    }
}

void VoiceAssistant::DismissPendingMatch() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_HasPendingMatch = false;
}

std::string VoiceAssistant::ApplyAudioFix(const std::string& input) const {
    if (!m_AudioFixEnabled || input.empty()) return input;
    std::string s = input;

    // 1. Dos números separados por espacio tras un nombre de libro (ej: "genesis 3 4" -> "genesis 3:4")
    //    Soporta también "genesis capitulo 3 versiculo 4" -> "genesis 3:4"
    try {
        std::regex twoNumsRegex(R"(\b([a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+(?:\s+[a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+)?)\s+(?:capitulo\s+|cap\s+)?(\d+)\s+(?:versiculo\s+|verso\s+|ver\s+)?(\d+)\b)", std::regex::icase);
        s = std::regex_replace(s, twoNumsRegex, "$1 $2:$3");
    } catch (...) {}

    // 2. Número de 3 dígitos unido tras un libro (ej: "juan 316" -> "juan 3:16")
    try {
        std::regex mergedThreeDigit(R"(\b([a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+(?:\s+[a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+)?)\s+([1-9])([0-9]{2})\b)", std::regex::icase);
        s = std::regex_replace(s, mergedThreeDigit, "$1 $2:$3");
    } catch (...) {}

    // 3. Número de 2 dígitos unido tras un libro bíblico (ej: "genesis 34" -> "genesis 3:4", "romanos 13" -> "romanos 1:3")
    try {
        std::regex mergedTwoDigit(R"(\b([a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+(?:\s+[a-zA-ZáéíóúÁÉÍÓÚñÑ0-9]+)?)\s+([1-9])([1-9])\b)", std::regex::icase);
        std::smatch m;
        if (std::regex_search(s, m, mergedTwoDigit)) {
            std::string bookCand = m[1].str();
            int bNum = UI::BibleBooks::ResolveAbbrev(bookCand);
            if (bNum <= 0) {
                auto cand = UI::BibleBooks::FindBookCandidates(bookCand);
                if (!cand.empty()) bNum = cand.front();
            }
            if (bNum > 0) {
                s = std::regex_replace(s, mergedTwoDigit, "$1 $2:$3");
            }
        }
    } catch (...) {}

    return s;
}

bool VoiceAssistant::TryMatchBible(const std::string& rawSpoken, VoiceMatchResult& outMatch) {
    EnsureBibleLoaded();
    std::lock_guard<std::recursive_mutex> lk(m_BibleMutex);
    if (!m_BibleLoaded || m_BibleData.books.empty()) return false;

    std::string textToParse = rawSpoken;
    if (m_AudioFixEnabled) {
        textToParse = ApplyAudioFix(textToParse);
    }

    std::string norm = NormalizeSpokenSpanish(textToParse);
    if (m_AudioFixEnabled) {
        norm = ApplyAudioFix(norm);
    }

    std::vector<std::string> tokens;
    std::string cur;
    for (char c : norm) {
        if (c == ' ') {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    if (tokens.empty()) return false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string bookCand = tokens[i];
        int bookNum = UI::BibleBooks::ResolveAbbrev(bookCand);
        size_t startToken = i;

        if (bookNum <= 0 && i + 1 < tokens.size()) {
            std::string twoTokens = tokens[i] + " " + tokens[i + 1];
            bookNum = UI::BibleBooks::ResolveAbbrev(twoTokens);
            if (bookNum > 0) {
                bookCand = twoTokens;
                startToken = i;
            }
        }

        if (bookNum <= 0) {
            auto candidates = UI::BibleBooks::FindBookCandidates(bookCand);
            if (!candidates.empty()) {
                bookNum = candidates.front();
            }
        }

        if (bookNum > 0) {
            std::string query = bookCand;
            size_t nextIdx = (bookCand.find(' ') != std::string::npos) ? (startToken + 2) : (startToken + 1);
            for (size_t k = nextIdx; k < tokens.size() && k < nextIdx + 3; ++k) {
                query += " " + tokens[k];
            }
            if (m_AudioFixEnabled) {
                query = ApplyAudioFix(query);
            }

            int outBook = -1, outChap = -1, outVerse = -1;
            if (UI::Search::ParseSmartQuery(m_BibleData, query, outBook, outChap, outVerse)) {
                if (outBook >= 0 && outBook < (int)m_BibleData.books.size()) {
                    const auto& book = m_BibleData.books[outBook];
                    if (outChap >= 0 && outChap < (int)book.chapters.size()) {
                        const auto& chap = book.chapters[outChap];
                        if (outVerse >= 0 && outVerse < (int)chap.verses.size()) {
                            const auto& verse = chap.verses[outVerse];

                            outMatch.type          = VoiceMatchType::BibleVerse;
                            outMatch.bibleBookIdx  = outBook;
                            outMatch.bibleChapIdx  = outChap;
                            outMatch.bibleVerseIdx = outVerse;
                            outMatch.ref           = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number);
                            outMatch.title         = outMatch.ref;
                            outMatch.body          = verse.text;
                            outMatch.rawSpoken     = rawSpoken;
                            outMatch.confidence    = 0.95f;
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool VoiceAssistant::TryMatchActiveSong(const std::string& rawSpoken, VoiceMatchResult& outMatch) {
    auto& core = Core::PresentationCore::Get();
    auto sel = core.PeekSelection();

    if (sel.type != ItemType::Song || sel.contentData.empty()) {
        return false;
    }

    std::vector<std::string> spokenWords = ExtractWords(rawSpoken, 3);
    if (spokenWords.empty()) return false;

    std::string spokenNorm = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(rawSpoken));

    float bestScore = 0.0f;
    int   bestIndex = -1;

    for (int i = 0; i < (int)sel.contentData.size(); ++i) {
        const std::string& stanza = sel.contentData[i];
        std::vector<std::string> stanzaWords = ExtractWords(stanza, 3);
        if (stanzaWords.empty()) continue;

        int matches = 0;
        for (const auto& sw : spokenWords) {
            if (std::find(stanzaWords.begin(), stanzaWords.end(), sw) != stanzaWords.end()) {
                matches++;
            }
        }

        float score = static_cast<float>(matches) / static_cast<float>(spokenWords.size());

        std::string stanzaNorm = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(stanza));
        if (stanzaNorm.find(spokenNorm) != std::string::npos) {
            score += 0.25f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestScore >= m_ConfidenceThreshold && bestIndex >= 0) {
        outMatch.type       = VoiceMatchType::SongVerse;
        outMatch.slideIndex = bestIndex;
        outMatch.title      = sel.title + " (Estrofa " + std::to_string(bestIndex + 1) + ")";
        outMatch.body       = sel.contentData[bestIndex];
        outMatch.ref        = sel.title;
        outMatch.rawSpoken  = rawSpoken;
        outMatch.confidence = bestScore;
        return true;
    }

    return false;
}

bool VoiceAssistant::TryMatchSong(const std::string& rawSpoken, VoiceMatchResult& outMatch) {
    // 1. Si hay una canción activa seleccionada, probar primero esa canción
    if (TryMatchActiveSong(rawSpoken, outMatch)) {
        return true;
    }

    std::vector<std::string> spokenWords = ExtractWords(rawSpoken, 3);
    if (spokenWords.empty()) return false;

    std::string spokenNorm = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(rawSpoken));

    // 2. Búsqueda en canciones de la biblioteca (en memoria, sin lecturas de disco repetitivas)
    EnsureSongsCached();
    std::lock_guard<std::mutex> lk(m_SongsMutex);

    float globalBestScore     = 0.0f;
    int   globalBestSongIdx   = -1;
    int   globalBestStanzaIdx = -1;

    for (size_t sIdx = 0; sIdx < m_CachedSongs.size(); ++sIdx) {
        const auto& cSong = m_CachedSongs[sIdx];
        for (size_t i = 0; i < cSong.stanzas.size(); ++i) {
            const auto& sWords = cSong.stanzaWords[i];
            if (sWords.empty()) continue;

            int matches = 0;
            for (const auto& sw : spokenWords) {
                if (std::find(sWords.begin(), sWords.end(), sw) != sWords.end()) matches++;
            }
            float score = static_cast<float>(matches) / static_cast<float>(spokenWords.size());
            std::string sNorm = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(cSong.stanzas[i]));
            if (sNorm.find(spokenNorm) != std::string::npos) score += 0.25f;

            if (score > globalBestScore) {
                globalBestScore     = score;
                globalBestSongIdx   = (int)sIdx;
                globalBestStanzaIdx = (int)i;
            }
        }
    }

    if (globalBestScore >= 0.70f && globalBestSongIdx >= 0 && globalBestSongIdx < (int)m_CachedSongs.size()) {
        auto& core = Core::PresentationCore::Get();
        const auto& hitSong = m_CachedSongs[globalBestSongIdx];
        LibrarySelection newSel;
        newSel.title       = hitSong.filename;
        newSel.type        = ItemType::Song;
        newSel.contentData = hitSong.stanzas;
        core.SetSelection(newSel);

        outMatch.type       = VoiceMatchType::SongVerse;
        outMatch.slideIndex = globalBestStanzaIdx;
        outMatch.title      = hitSong.title + " (Estrofa " + std::to_string(globalBestStanzaIdx + 1) + ")";
        outMatch.body       = hitSong.stanzas[globalBestStanzaIdx];
        outMatch.ref        = hitSong.title;
        outMatch.rawSpoken  = rawSpoken;
        outMatch.confidence = globalBestScore;
        return true;
    }

    return false;
}

void VoiceAssistant::ProcessSpokenText(const std::string& rawText) {
    if (rawText.empty()) return;

    std::string textToUse = rawText;
    if (m_AudioFixEnabled) {
        textToUse = ApplyAudioFix(textToUse);
    }

    std::string norm = NormalizeSpokenSpanish(textToUse);
    if (m_AudioFixEnabled) {
        norm = ApplyAudioFix(norm);
    }
    std::string lowerNorm = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(norm));

    while (!lowerNorm.empty() && (lowerNorm.front() == ' ' || lowerNorm.front() == '\t')) lowerNorm.erase(lowerNorm.begin());
    while (!lowerNorm.empty() && (lowerNorm.back() == ' ' || lowerNorm.back() == '\t')) lowerNorm.pop_back();

    auto& core = Core::PresentationCore::Get();

    // ── Comandos de navegación de diapositivas por voz ──
    if (lowerNorm == "pantalla negra" || lowerNorm == "pantalla a negro" ||
        lowerNorm == "limpiar pantalla" || lowerNorm == "ocultar texto") {
        core.SetProjecting(false);
        return;
    }

    // Comandos de salto de estrofas de canciones (permitidos en modo Todo o Solo Canción Seleccionada)
    if (m_MatchScope == VoiceMatchScope::All || m_MatchScope == VoiceMatchScope::SelectedSongOnly) {
        if (lowerNorm == "siguiente" || lowerNorm == "siguiente estrofa" || 
            lowerNorm == "siguiente diapositiva" || lowerNorm == "siguiente verso") {
            auto sel = core.PeekSelection();
            int cur = core.GetActiveSlideIndex();
            if (sel.type == ItemType::Song && cur + 1 < (int)sel.contentData.size()) {
                int next = cur + 1;
                core.SetLayer2_Text(sel.contentData[next], next);
                core.SetActiveSlideIndex(next);
                core.SetProjecting(true);
            }
            return;
        }
        if (lowerNorm == "anterior" || lowerNorm == "anterior estrofa" || 
            lowerNorm == "anterior diapositiva") {
            auto sel = core.PeekSelection();
            int cur = core.GetActiveSlideIndex();
            if (sel.type == ItemType::Song && cur > 0 && cur - 1 < (int)sel.contentData.size()) {
                int prev = cur - 1;
                core.SetLayer2_Text(sel.contentData[prev], prev);
                core.SetActiveSlideIndex(prev);
                core.SetProjecting(true);
            }
            return;
        }
        if (lowerNorm == "coro" || lowerNorm == "ir al coro" || lowerNorm == "repetir coro") {
            auto sel = core.PeekSelection();
            if (sel.type == ItemType::Song && !sel.contentData.empty()) {
                int coroIdx = -1;
                for (size_t i = 0; i < sel.contentData.size(); ++i) {
                    std::string stLower = UI::TextUtils::StripAccents(UI::TextUtils::ToLowerUTF8(sel.contentData[i]));
                    if (stLower.find("coro") != std::string::npos) {
                        coroIdx = (int)i;
                        break;
                    }
                }
                if (coroIdx < 0 && sel.contentData.size() > 1) {
                    coroIdx = 1;
                }
                if (coroIdx >= 0) {
                    core.SetLayer2_Text(sel.contentData[coroIdx], coroIdx);
                    core.SetActiveSlideIndex(coroIdx);
                    core.SetProjecting(true);
                    return;
                }
            }
        }
        for (int s = 1; s <= 12; ++s) {
            if (lowerNorm == ("estrofa " + std::to_string(s)) ||
                lowerNorm == ("estrofa " + NumbersToWord(s))) {
                auto sel = core.PeekSelection();
                if (sel.type == ItemType::Song && s - 1 < (int)sel.contentData.size()) {
                    core.SetLayer2_Text(sel.contentData[s - 1], s - 1);
                    core.SetActiveSlideIndex(s - 1);
                    core.SetProjecting(true);
                    return;
                }
            }
        }
    }

    VoiceMatchResult match;

    if (m_MatchScope == VoiceMatchScope::BibleOnly) {
        // En modo solo Biblia: se busca ÚNICAMENTE en las escrituras, jamás en canciones
        if (m_BibleEnabled && TryMatchBible(rawText, match)) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_PendingMatch          = match;
            m_HasPendingMatch       = true;
            m_PendingMatchTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            return;
        }
    } else if (m_MatchScope == VoiceMatchScope::SelectedSongOnly) {
        // En modo solo canción seleccionada: se busca ÚNICAMENTE en las estrofas de la canción activa actual
        if (TryMatchActiveSong(rawText, match)) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_PendingMatch          = match;
            m_HasPendingMatch       = true;
            m_PendingMatchTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            return;
        }
    } else { // VoiceMatchScope::All
        // Modo todos los datos: prioridad bíblica primero, luego canciones
        if (m_BibleEnabled && TryMatchBible(rawText, match)) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_PendingMatch          = match;
            m_HasPendingMatch       = true;
            m_PendingMatchTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            return;
        }

        if (m_SongsEnabled && TryMatchSong(rawText, match)) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_PendingMatch          = match;
            m_HasPendingMatch       = true;
            m_PendingMatchTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            return;
        }
    }
}

void VoiceAssistant::Update() {
    // 1. Despachar texto pendiente de forma 100% segura en el hilo principal
    std::string textToProcess;
    {
        double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        std::lock_guard<std::mutex> lk(m_Mutex);
        if (!m_PendingTextToProcess.empty()) {
            if (m_PendingTextIsFinal || (now - m_LastHypothesisTime > 0.40)) {
                textToProcess = m_PendingTextToProcess;
                m_PendingTextToProcess.clear();
            }
        }
    }

    if (!textToProcess.empty()) {
        ProcessSpokenText(textToProcess);
    }

    // 2. Manejo de coincidencias detectadas (proyección o banner asistido)
    VoiceMatchResult match;
    bool hasMatch = false;
    double timestamp = 0.0;

    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        if (m_HasPendingMatch) {
            match     = m_PendingMatch;
            hasMatch  = true;
            timestamp = m_PendingMatchTimestamp;
        }
    }

    if (!hasMatch) return;

    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // En modo Auto-proyectar: se envía directamente a la pantalla respetando un cooldown de seguridad
    if (m_AutoProject) {
        std::string matchKey = (match.type == VoiceMatchType::SongVerse)
                               ? (match.ref + "_" + std::to_string(match.slideIndex))
                               : (match.ref);

        if (matchKey != m_LastProjectedKey || (now - m_LastProjectedTime) > 1.5) {
            auto& core = Core::PresentationCore::Get();

            if (match.type == VoiceMatchType::SongVerse) {
                auto sel = core.PeekSelection();
                if (sel.type == ItemType::Song && match.slideIndex >= 0 && match.slideIndex < (int)sel.contentData.size()) {
                    core.SetLayer2_Text(sel.contentData[match.slideIndex], match.slideIndex);
                    core.SetActiveSlideIndex(match.slideIndex);
                    core.SetProjecting(true);
                }
            } else if (match.type == VoiceMatchType::BibleVerse) {
                core.SetLayer2_Text(match.body);
                core.SetCurrentRef(match.ref);
                core.SetProjecting(true);

                if (match.bibleBookIdx >= 0 && match.bibleBookIdx < (int)m_BibleData.books.size()) {
                    int bNum = m_BibleData.books[match.bibleBookIdx].canonicalNumber;
                    int cNum = (match.bibleChapIdx >= 0 && match.bibleChapIdx < (int)m_BibleData.books[match.bibleBookIdx].chapters.size())
                               ? m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].number : 1;
                    int vNum = (match.bibleChapIdx >= 0 && match.bibleVerseIdx >= 0 &&
                                match.bibleVerseIdx < (int)m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].verses.size())
                               ? m_BibleData.books[match.bibleBookIdx].chapters[match.bibleChapIdx].verses[match.bibleVerseIdx].number : 1;
                    core.RequestBibleJump(bNum, cNum, vNum);
                }
            }

            m_LastProjectedKey  = matchKey;
            m_LastProjectedTime = now;
        }

        std::lock_guard<std::mutex> lk(m_Mutex);
        m_HasPendingMatch = false;
    } else {
        // En modo asistido: caduca a los 8 segundos si el operador no confirma
        if (now - timestamp > 8.0) {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_HasPendingMatch = false;
        }
    }
}

} // namespace ProyecThor::Core
