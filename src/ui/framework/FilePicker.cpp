#include "FilePicker.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdio>
#include <array>
#ifdef PT_HAVE_GIO_PORTAL
#include <gio/gio.h>
#endif
#endif

namespace fs = std::filesystem;

namespace ProyecThor::UI {

#ifdef _WIN32

static std::wstring ToWString(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), w.data(), len);
    return w;
}

static std::string ToString(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, s.data(), len, nullptr, nullptr);
    return s;
}

std::string PickFile(const std::string& title,
                     const std::vector<FileFilterItem>& filters)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    std::vector<std::wstring> names, specs;
    std::vector<COMDLG_FILTERSPEC> dlgFilters;
    names.reserve(filters.size());
    specs.reserve(filters.size());
    dlgFilters.reserve(filters.size());

    for (const auto& f : filters) {
        names.push_back(ToWString(f.name));
        std::wstring spec;
        for (size_t i = 0; i < f.patterns.size(); ++i) {
            if (i > 0) spec += L";";
            spec += ToWString(f.patterns[i]);
        }
        specs.push_back(spec);
    }
    for (size_t i = 0; i < names.size(); ++i) {
        dlgFilters.push_back({names[i].c_str(), specs[i].c_str()});
    }

    if (!dlgFilters.empty()) {
        dlg->SetFileTypes((UINT)dlgFilters.size(), dlgFilters.data());
        dlg->SetFileTypeIndex(1);
    }
    if (!title.empty()) {
        dlg->SetTitle(ToWString(title).c_str());
    }

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                result = ToString(pp);
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::vector<std::string> PickMultipleFiles(const std::string& title,
                                           const std::vector<FileFilterItem>& filters)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    DWORD dwFlags = 0;
    dlg->GetOptions(&dwFlags);
    dlg->SetOptions(dwFlags | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);

    std::vector<std::wstring> names, specs;
    std::vector<COMDLG_FILTERSPEC> dlgFilters;
    for (const auto& f : filters) {
        names.push_back(ToWString(f.name));
        std::wstring spec;
        for (size_t i = 0; i < f.patterns.size(); ++i) {
            if (i > 0) spec += L";";
            spec += ToWString(f.patterns[i]);
        }
        specs.push_back(spec);
    }
    for (size_t i = 0; i < names.size(); ++i) {
        dlgFilters.push_back({names[i].c_str(), specs[i].c_str()});
    }

    if (!dlgFilters.empty()) {
        dlg->SetFileTypes((UINT)dlgFilters.size(), dlgFilters.data());
        dlg->SetFileTypeIndex(1);
    }
    if (!title.empty()) {
        dlg->SetTitle(ToWString(title).c_str());
    }

    std::vector<std::string> results;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dlg->GetResults(&items))) {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD i = 0; i < count; ++i) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &item))) {
                    PWSTR pp = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                        std::string s = ToString(pp);
                        if (!s.empty()) results.push_back(s);
                        CoTaskMemFree(pp);
                    }
                    item->Release();
                }
            }
            items->Release();
        }
    }
    dlg->Release();
    return results;
}

std::string PickSaveFile(const std::string& title,
                         const std::string& defaultPath,
                         const std::vector<FileFilterItem>& filters)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    std::vector<std::wstring> names, specs;
    std::vector<COMDLG_FILTERSPEC> dlgFilters;
    for (const auto& f : filters) {
        names.push_back(ToWString(f.name));
        std::wstring spec;
        for (size_t i = 0; i < f.patterns.size(); ++i) {
            if (i > 0) spec += L";";
            spec += ToWString(f.patterns[i]);
        }
        specs.push_back(spec);
    }
    for (size_t i = 0; i < names.size(); ++i) {
        dlgFilters.push_back({names[i].c_str(), specs[i].c_str()});
    }

    if (!dlgFilters.empty()) {
        dlg->SetFileTypes((UINT)dlgFilters.size(), dlgFilters.data());
        dlg->SetFileTypeIndex(1);
    }
    if (!title.empty()) {
        dlg->SetTitle(ToWString(title).c_str());
    }
    if (!defaultPath.empty()) {
        fs::path p(defaultPath);
        if (!p.filename().empty()) {
            dlg->SetFileName(ToWString(p.filename().string()).c_str());
        }
    }

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                result = ToString(pp);
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::string PickFolder(const std::string& title) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return {};

    dlg->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    if (!title.empty()) dlg->SetTitle(ToWString(title).c_str());

    std::string result;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR pp = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                result = ToString(pp);
                CoTaskMemFree(pp);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

#else

// =============================================================================
// LINUX: XDG Desktop Portal FileChooser (Principal, 100% compatible con Flatpak)
// con fallback a Zenity / KDialog en sistemas antiguos sin portal
// =============================================================================

#ifdef PT_HAVE_GIO_PORTAL
struct PortalResponseData {
    GMainLoop* loop = nullptr;
    int responseCode = -1;
    std::vector<std::string> uris;
    std::string expectedHandle;
    std::string token;
};

static void OnPortalResponse(GDBusConnection* connection,
                             const gchar* sender_name,
                             const gchar* object_path,
                             const gchar* interface_name,
                             const gchar* signal_name,
                             GVariant* parameters,
                             gpointer user_data)
{
    (void)connection; (void)sender_name;
    (void)interface_name; (void)signal_name;
    auto* data = static_cast<PortalResponseData*>(user_data);

    // Filtrar si la señal pertenece a otra petición
    if (object_path != nullptr) {
        bool match = false;
        if (!data->expectedHandle.empty() && data->expectedHandle == object_path) {
            match = true;
        } else if (!data->token.empty() && g_str_has_suffix(object_path, data->token.c_str())) {
            match = true;
        }
        if (!match) {
            return;
        }
    }

    guint32 response = 1;
    GVariantIter* iter = nullptr;
    g_variant_get(parameters, "(ua{sv})", &response, &iter);
    data->responseCode = static_cast<int>(response);

    if (response == 0 && iter != nullptr) {
        const gchar* key = nullptr;
        GVariant* val = nullptr;
        while (g_variant_iter_next(iter, "{&sv}", &key, &val)) {
            if (g_strcmp0(key, "uris") == 0) {
                GVariantIter* uri_iter = nullptr;
                g_variant_get(val, "as", &uri_iter);
                const gchar* uri = nullptr;
                while (g_variant_iter_next(uri_iter, "&s", &uri)) {
                    data->uris.emplace_back(uri);
                }
                g_variant_iter_free(uri_iter);
            }
            g_variant_unref(val);
        }
        g_variant_iter_free(iter);
    }
    if (data->loop && g_main_loop_is_running(data->loop)) {
        g_main_loop_quit(data->loop);
    }
}

static bool RunPortalFileChooser(const std::string& title,
                                const std::vector<FileFilterItem>& filters,
                                bool multiple,
                                bool directory,
                                bool isSave,
                                const std::string& defaultPath,
                                std::vector<std::string>& outPaths)
{
    GError* err = nullptr;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
    if (!conn) {
        if (err) g_error_free(err);
        return false;
    }

    static std::atomic<uint64_t> s_token_counter{0};
    std::string token = "proyecthor_portal_" + std::to_string(++s_token_counter);

    PortalResponseData respData;
    respData.token = token;

    GMainContext* context = g_main_context_new();
    g_main_context_push_thread_default(context);
    respData.loop = g_main_loop_new(context, FALSE);

    // Suscribirse a Response con G_DBUS_SIGNAL_FLAGS_NONE para que D-Bus cree la regla AddMatch
    guint subId = g_dbus_connection_signal_subscribe(
        conn,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        nullptr, // null para capturar el handle retornado independientemente del formato de bus name
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        OnPortalResponse,
        &respData,
        nullptr
    );

    GVariantBuilder optBuilder;
    g_variant_builder_init(&optBuilder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&optBuilder, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
    if (multiple) {
        g_variant_builder_add(&optBuilder, "{sv}", "multiple", g_variant_new_boolean(TRUE));
    }
    if (directory) {
        g_variant_builder_add(&optBuilder, "{sv}", "directory", g_variant_new_boolean(TRUE));
    }

    if (!filters.empty()) {
        GVariantBuilder filtersBuilder;
        g_variant_builder_init(&filtersBuilder, G_VARIANT_TYPE("a(sa(us))"));
        for (const auto& f : filters) {
            GVariantBuilder patBuilder;
            g_variant_builder_init(&patBuilder, G_VARIANT_TYPE("a(us)"));
            for (const auto& pat : f.patterns) {
                g_variant_builder_add(&patBuilder, "(us)", (guint32)0, pat.c_str());
            }
            GVariant* patVariant = g_variant_builder_end(&patBuilder);
            g_variant_builder_add(&filtersBuilder, "(s@a(us))", f.name.c_str(), patVariant);
        }
        GVariant* filtersVariant = g_variant_builder_end(&filtersBuilder);
        g_variant_builder_add(&optBuilder, "{sv}", "filters", filtersVariant);
    }

    if (isSave && !defaultPath.empty()) {
        fs::path p(defaultPath);
        if (!p.filename().empty()) {
            g_variant_builder_add(&optBuilder, "{sv}", "current_name",
                                  g_variant_new_string(p.filename().string().c_str()));
        }
    }

    GVariant* options = g_variant_builder_end(&optBuilder);

    GVariant* ret = g_dbus_connection_call_sync(
        conn,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        isSave ? "SaveFile" : "OpenFile",
        g_variant_new("(ss@a{sv})", "", title.c_str(), options),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &err
    );

    if (!ret) {
        g_dbus_connection_signal_unsubscribe(conn, subId);
        g_main_loop_unref(respData.loop);
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);
        g_object_unref(conn);
        if (err) g_error_free(err);
        return false;
    }

    const gchar* returnedHandle = nullptr;
    g_variant_get(ret, "(&o)", &returnedHandle);
    if (returnedHandle) {
        respData.expectedHandle = returnedHandle;
    }
    g_variant_unref(ret);

    g_main_loop_run(respData.loop);

    g_dbus_connection_signal_unsubscribe(conn, subId);
    g_main_loop_unref(respData.loop);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);
    g_object_unref(conn);

    if (respData.responseCode == 0) {
        for (const auto& uri : respData.uris) {
            gchar* filename = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
            if (filename) {
                outPaths.emplace_back(filename);
                g_free(filename);
            } else if (uri.rfind("file://", 0) == 0) {
                outPaths.emplace_back(uri.substr(7));
            } else {
                outPaths.emplace_back(uri);
            }
        }
    }
    return true; // Gestionado por el Portal exitosamente (incluso si el usuario canceló)
}
#endif

// Fallback clasico usando zenity o kdialog
static std::vector<std::string> RunZenityKdialogFallback(const std::string& title,
                                                        const std::vector<FileFilterItem>& filters,
                                                        bool multiple,
                                                        bool directory,
                                                        bool isSave,
                                                        const std::string& defaultPath)
{
    std::string filterArg;
    if (!filters.empty()) {
        std::string pats;
        for (const auto& f : filters) {
            for (const auto& p : f.patterns) {
                if (!pats.empty()) pats += " ";
                pats += p;
            }
        }
        filterArg = "--file-filter=\"" + filters[0].name + " | " + pats + "\" ";
    }

    std::string zenityCmd = "zenity --file-selection --title=\"" + title + "\" ";
    if (directory) zenityCmd += "--directory ";
    if (multiple)  zenityCmd += "--multiple --separator=\"\\n\" ";
    if (isSave) {
        zenityCmd += "--save --confirm-overwrite ";
        if (!defaultPath.empty()) zenityCmd += "--filename=\"" + defaultPath + "\" ";
    }
    if (!filterArg.empty() && !directory) zenityCmd += filterArg;
    zenityCmd += "2>/dev/null";

    std::string kdialogCmd;
    if (directory) {
        kdialogCmd = "kdialog --title=\"" + title + "\" --getexistingdirectory . 2>/dev/null";
    } else if (isSave) {
        kdialogCmd = "kdialog --title=\"" + title + "\" --getsavefilename \"" + defaultPath + "\" 2>/dev/null";
    } else {
        kdialogCmd = "kdialog --title=\"" + title + "\" " + (multiple ? "--multiple --separate-output " : "") +
                     "--getopenfilename . 2>/dev/null";
    }

    std::array<std::string, 2> cmds = { zenityCmd, kdialogCmd };
    for (const auto& cmd : cmds) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) continue;
        char buf[1024];
        std::string raw;
        while (fgets(buf, sizeof(buf), pipe) != nullptr) raw += buf;
        int status = pclose(pipe);
        if (status != 0 || raw.empty()) continue;

        std::vector<std::string> results;
        size_t start = 0, pos = 0;
        while ((pos = raw.find('\n', start)) != std::string::npos) {
            std::string line = raw.substr(start, pos - start);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (!line.empty()) results.push_back(line);
            start = pos + 1;
        }
        if (start < raw.size()) {
            std::string line = raw.substr(start);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
            if (!line.empty()) results.push_back(line);
        }
        if (!results.empty()) return results;
    }
    return {};
}

std::string PickFile(const std::string& title,
                     const std::vector<FileFilterItem>& filters)
{
    std::vector<std::string> res;
#ifdef PT_HAVE_GIO_PORTAL
    if (RunPortalFileChooser(title, filters, false, false, false, "", res)) {
        return res.empty() ? "" : res.front();
    }
#endif
    res = RunZenityKdialogFallback(title, filters, false, false, false, "");
    return res.empty() ? "" : res.front();
}

std::vector<std::string> PickMultipleFiles(const std::string& title,
                                           const std::vector<FileFilterItem>& filters)
{
    std::vector<std::string> res;
#ifdef PT_HAVE_GIO_PORTAL
    if (RunPortalFileChooser(title, filters, true, false, false, "", res)) {
        return res;
    }
#endif
    return RunZenityKdialogFallback(title, filters, true, false, false, "");
}

std::string PickSaveFile(const std::string& title,
                         const std::string& defaultPath,
                         const std::vector<FileFilterItem>& filters)
{
    std::vector<std::string> res;
#ifdef PT_HAVE_GIO_PORTAL
    if (RunPortalFileChooser(title, filters, false, false, true, defaultPath, res)) {
        return res.empty() ? "" : res.front();
    }
#endif
    res = RunZenityKdialogFallback(title, filters, false, false, true, defaultPath);
    return res.empty() ? "" : res.front();
}

std::string PickFolder(const std::string& title)
{
    std::vector<std::string> res;
#ifdef PT_HAVE_GIO_PORTAL
    if (RunPortalFileChooser(title, {}, false, true, false, "", res)) {
        return res.empty() ? "" : res.front();
    }
#endif
    res = RunZenityKdialogFallback(title, {}, false, true, false, "");
    return res.empty() ? "" : res.front();
}

#endif

// =============================================================================
// Implementaciones de conveniencia multiplataforma
// =============================================================================

std::string PickImageOrVideoFile() {
    return PickFile("Elegir imagen o video", {
        {"Video e Imagen", {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.jpg", "*.jpeg", "*.png"}},
        {"Videos",         {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm"}},
        {"Imágenes",       {"*.jpg", "*.jpeg", "*.png"}},
        {"Todos los archivos", {"*"}}
    });
}

std::string PickImageFile() {
    return PickFile("Elegir imagen", {
        {"Imágenes", {"*.jpg", "*.jpeg", "*.png"}},
        {"Todos los archivos", {"*"}}
    });
}

std::string PickHtmlFile() {
    return PickFile("Elegir archivo HTML o sitio web local", {
        {"Archivos HTML y Web (*.html, *.htm)", {"*.html", "*.htm", "*.xhtml"}},
        {"Todos los archivos (*.*)", {"*"}}
    });
}

std::string PickAudioFile() {
    return PickFile("Seleccionar archivo de audio", {
        {"Audio", {"*.mp3", "*.flac", "*.wav", "*.ogg", "*.aac", "*.m4a", "*.wma", "*.opus", "*.aiff"}},
        {"Todos los archivos", {"*"}}
    });
}

std::string PickFontFile() {
    return PickFile("Seleccionar fuente", {
        {"Fuentes (*.ttf, *.otf, *.ttc)", {"*.ttf", "*.otf", "*.ttc"}},
        {"Todos los archivos", {"*"}}
    });
}

std::string PickSvgFile() {
    return PickFile("Seleccionar archivo SVG", {
        {"Archivos SVG (*.svg)", {"*.svg"}},
        {"Todos los archivos", {"*"}}
    });
}

std::string PickSaveVideoPath(const std::string& defaultPath) {
    return PickSaveFile("Guardar video como", defaultPath, {
        {"Video MP4 (*.mp4)", {"*.mp4"}},
        {"Video Matroska (*.mkv)", {"*.mkv"}},
        {"Video WebM (*.webm)", {"*.webm"}}
    });
}

std::string PickSaveTextPath(const std::string& defaultPath) {
    return PickSaveFile("Guardar subtitulos como", defaultPath, {
        {"Texto plano (*.txt)", {"*.txt"}},
        {"Todos los archivos", {"*"}}
    });
}

bool LooksLikeVideoPath(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm";
}

} // namespace ProyecThor::UI
