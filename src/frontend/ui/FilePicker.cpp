#include "FilePicker.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdio>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::UI {

#ifdef _WIN32
std::string PickImageOrVideoFile() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Video e Imagen", L"*.mp4;*.mkv;*.avi;*.mov;*.jpg;*.jpeg;*.png"},
        {L"Videos",         L"*.mp4;*.mkv;*.avi;*.mov"},
        {L"Imágenes",       L"*.jpg;*.jpeg;*.png"},
    };
    dlg->SetFileTypes(3, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Elegir imagen o video");

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickImageFile() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Imágenes", L"*.jpg;*.jpeg;*.png"},
    };
    dlg->SetFileTypes(1, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Elegir imagen");

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickHtmlFile() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Archivos HTML y Web (*.html, *.htm)", L"*.html;*.htm;*.xhtml"},
        {L"Todos los archivos (*.*)",            L"*.*"},
    };
    dlg->SetFileTypes(2, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Elegir archivo HTML o sitio web local");

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring w(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    return w;
}

std::string PickFolder(const std::string& title) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    FILEOPENDIALOGOPTIONS opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS);
    dlg->SetTitle(Utf8ToWide(title).c_str());

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickSaveVideoPath(const std::string& defaultPath) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Video", L"*.mp4;*.mkv;*.webm;*.avi;*.mov"},
    };
    dlg->SetFileTypes(1, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Guardar video como");

    fs::path def(defaultPath);
    std::wstring wFolder = Utf8ToWide(def.parent_path().string());
    std::wstring wName   = Utf8ToWide(def.filename().string());
    if (!wName.empty()) dlg->SetFileName(wName.c_str());
    if (!wFolder.empty()) {
        IShellItem* folderItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(wFolder.c_str(), nullptr, IID_PPV_ARGS(&folderItem)))) {
            dlg->SetFolder(folderItem);
            folderItem->Release();
        }
    }

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickSaveTextPath(const std::string& defaultPath) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        {L"Texto plano", L"*.txt"},
    };
    dlg->SetFileTypes(1, filters);
    dlg->SetFileTypeIndex(1);
    dlg->SetTitle(L"Guardar subtitulos como");

    fs::path def(defaultPath);
    std::wstring wFolder = Utf8ToWide(def.parent_path().string());
    std::wstring wName   = Utf8ToWide(def.filename().string());
    if (!wName.empty()) dlg->SetFileName(wName.c_str());
    if (!wFolder.empty()) {
        IShellItem* folderItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(wFolder.c_str(), nullptr, IID_PPV_ARGS(&folderItem)))) {
            dlg->SetFolder(folderItem);
            folderItem->Release();
        }
    }

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pp, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pp, -1, result.data(), len, nullptr, nullptr);
                }
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}
#else
static std::string RunFilePickerCommands(const char* const commands[], size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char buffer[1024];
        std::string result;

        FILE* pipe = popen(commands[i], "r");
        if (!pipe) continue;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
        int status = pclose(pipe);
        if (status != 0) continue; // cancelado o la herramienta no esta instalada

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        if (!result.empty()) return result;
    }
    return {};
}

std::string PickImageOrVideoFile() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Elegir imagen o video\" "
        "--file-filter=\"Video e Imagen | *.mp4 *.mkv *.avi *.mov *.jpg *.jpeg *.png\" 2>/dev/null",
        "kdialog --getopenfilename . "
        "\"*.mp4 *.mkv *.avi *.mov *.jpg *.jpeg *.png|Video e Imagen\" 2>/dev/null"
    };
    return RunFilePickerCommands(commands, 2);
}

std::string PickImageFile() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Elegir imagen\" "
        "--file-filter=\"Imágenes | *.jpg *.jpeg *.png\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.jpg *.jpeg *.png|Imágenes\" 2>/dev/null"
    };
    return RunFilePickerCommands(commands, 2);
}

std::string PickHtmlFile() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Elegir archivo HTML o sitio web local\" "
        "--file-filter=\"Archivos HTML (*.html *.htm) | *.html *.htm *.xhtml\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.html *.htm *.xhtml|Archivos HTML\" 2>/dev/null"
    };
    return RunFilePickerCommands(commands, 2);
}

// zenity/kdialog son apps GTK/Qt independientes del compositor -- corren
// igual bajo X11 o Wayland (no dependen de ningun protocolo de portal
// especifico), asi que este mismo camino ya cubre Wayland sin nada extra.
std::string PickFolder(const std::string& title) {
    std::string cmd1 = "zenity --file-selection --directory --title=\"" + title + "\" 2>/dev/null";
    std::string cmd2 = "kdialog --getexistingdirectory . 2>/dev/null";
    const char* commands[] = { cmd1.c_str(), cmd2.c_str() };
    return RunFilePickerCommands(commands, 2);
}

std::string PickSaveVideoPath(const std::string& defaultPath) {
    std::string cmd1 = "zenity --file-selection --save --confirm-overwrite "
                        "--filename=\"" + defaultPath + "\" --title=\"Guardar video como\" 2>/dev/null";
    std::string cmd2 = "kdialog --getsavefilename \"" + defaultPath +
                        "\" \"*.mp4 *.mkv *.webm *.avi *.mov|Video\" 2>/dev/null";
    const char* commands[] = { cmd1.c_str(), cmd2.c_str() };
    return RunFilePickerCommands(commands, 2);
}

std::string PickSaveTextPath(const std::string& defaultPath) {
    std::string cmd1 = "zenity --file-selection --save --confirm-overwrite "
                        "--filename=\"" + defaultPath + "\" --title=\"Guardar subtitulos como\" 2>/dev/null";
    std::string cmd2 = "kdialog --getsavefilename \"" + defaultPath + "\" \"*.txt|Texto plano\" 2>/dev/null";
    const char* commands[] = { cmd1.c_str(), cmd2.c_str() };
    return RunFilePickerCommands(commands, 2);
}
#endif

bool LooksLikeVideoPath(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov";
}

} // namespace ProyecThor::UI
