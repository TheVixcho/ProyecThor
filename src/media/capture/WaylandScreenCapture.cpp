#include "WaylandScreenCapture.h"

#ifdef PT_HAVE_WAYLAND_CAPTURE

#include <dbus/dbus.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <unistd.h>

namespace ProyecThor::UI {

namespace {

constexpr const char* kPortalDest      = "org.freedesktop.portal.Desktop";
constexpr const char* kPortalPath      = "/org/freedesktop/portal/desktop";
constexpr const char* kScreenCastIface = "org.freedesktop.portal.ScreenCast";
constexpr const char* kSessionIface    = "org.freedesktop.portal.Session";
constexpr const char* kRequestIface    = "org.freedesktop.portal.Request";

std::string MakeToken(const char* prefix) {
    static std::atomic<uint64_t> counter{0};
    return std::string(prefix) + std::to_string(static_cast<long>(getpid()))
         + "_" + std::to_string(counter.fetch_add(1));
}

// ── Helpers para armar opciones a{sv} ───────────────────────────────────────
void AppendStringOption(DBusMessageIter& dictIter, const char* key, const char* value) {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dictIter, &entry);
}
void AppendUint32Option(DBusMessageIter& dictIter, const char* key, uint32_t value) {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dictIter, &entry);
}
void AppendBoolOption(DBusMessageIter& dictIter, const char* key, bool value) {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_bool_t b = value ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &b);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dictIter, &entry);
}

// Busca 'key' en un iterador posicionado sobre un a{sv}; si es string, lo devuelve.
bool DictGetString(DBusMessageIter* dictIter, const char* key, std::string& out) {
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(dictIter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entryIter;
        dbus_message_iter_recurse(&arrayIter, &entryIter);
        const char* k = nullptr;
        dbus_message_iter_get_basic(&entryIter, &k);
        dbus_message_iter_next(&entryIter);
        if (k && key == std::string(k)) {
            DBusMessageIter variantIter;
            dbus_message_iter_recurse(&entryIter, &variantIter);
            if (dbus_message_iter_get_arg_type(&variantIter) == DBUS_TYPE_STRING) {
                const char* s = nullptr;
                dbus_message_iter_get_basic(&variantIter, &s);
                out = s ? s : "";
                return true;
            }
            return false;
        }
        dbus_message_iter_next(&arrayIter);
    }
    return false;
}

// Busca la clave "streams" (tipo a(ua{sv})) y devuelve el node_id del primero.
bool DictGetFirstStreamNodeId(DBusMessageIter* dictIter, uint32_t& outNodeId) {
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(dictIter, &arrayIter);
    while (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entryIter;
        dbus_message_iter_recurse(&arrayIter, &entryIter);
        const char* k = nullptr;
        dbus_message_iter_get_basic(&entryIter, &k);
        dbus_message_iter_next(&entryIter);
        if (k && std::string(k) == "streams") {
            DBusMessageIter variantIter;
            dbus_message_iter_recurse(&entryIter, &variantIter);
            DBusMessageIter streamsIter;
            dbus_message_iter_recurse(&variantIter, &streamsIter);
            if (dbus_message_iter_get_arg_type(&streamsIter) == DBUS_TYPE_STRUCT) {
                DBusMessageIter structIter;
                dbus_message_iter_recurse(&streamsIter, &structIter);
                dbus_message_iter_get_basic(&structIter, &outNodeId);
                return true;
            }
            return false;
        }
        dbus_message_iter_next(&arrayIter);
    }
    return false;
}

struct PortalResult {
    bool         ok   = false;
    uint32_t     code = 1;
    std::string  error;
    DBusMessage* msg  = nullptr; // due si ok; el llamador debe unref
};

// Espera la señal Response del Request en handlePath (pumpeando la conexion
// mientras tanto), hasta timeoutMs o hasta que *cancel se ponga en true.
PortalResult WaitForResponse(DBusConnection* conn, const std::string& handlePath,
                              int timeoutMs, std::atomic<bool>* cancel) {
    PortalResult result;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cancel && cancel->load()) { result.error = "cancelado"; return result; }
        dbus_connection_read_write(conn, 200);
        DBusMessage* msg = dbus_connection_pop_message(conn);
        if (!msg) continue;
        const char* path = dbus_message_get_path(msg);
        if (dbus_message_is_signal(msg, kRequestIface, "Response") && path && handlePath == path) {
            DBusMessageIter iter;
            dbus_message_iter_init(msg, &iter);
            dbus_message_iter_get_basic(&iter, &result.code);
            result.ok  = (result.code == 0);
            result.msg = msg; // el llamador sigue leyendo (results a{sv}) y debe unref
            return result;
        }
        dbus_message_unref(msg);
    }
    result.error = "tiempo de espera agotado esperando al portal";
    return result;
}

// Llama un metodo de org.freedesktop.portal.ScreenCast en el objeto fijo del
// portal y devuelve el object path del Request ('out o handle'). appendArgs
// arma el resto de los argumentos del metodo (incluida la options a{sv}).
std::string CallPortalRequestMethod(DBusConnection* conn, const char* method,
                                     const std::function<void(DBusMessageIter&)>& appendArgs,
                                     std::string* errOut) {
    DBusMessage* msg = dbus_message_new_method_call(kPortalDest, kPortalPath, kScreenCastIface, method);
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    appendArgs(args);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &err);
    dbus_message_unref(msg);
    if (!reply) {
        if (errOut) *errOut = dbus_error_is_set(&err) ? err.message : "sin respuesta del portal";
        dbus_error_free(&err);
        return {};
    }
    DBusMessageIter replyIter;
    dbus_message_iter_init(reply, &replyIter);
    const char* handleC = nullptr;
    dbus_message_iter_get_basic(&replyIter, &handleC);
    std::string handle = handleC ? handleC : "";
    dbus_message_unref(reply);
    return handle;
}

} // namespace

struct WaylandScreenCapture::Impl {
    std::atomic<State> state{State::Idle};
    mutable std::mutex stateMutex;
    std::string         errorMessage;

    std::thread        setupThread;
    std::atomic<bool>  cancelRequested{false};

    DBusConnection* dbusConn = nullptr;
    std::string     sessionHandle;

    pw_thread_loop* loop    = nullptr;
    pw_context*     context = nullptr;
    pw_core*        core    = nullptr;
    pw_stream*      stream  = nullptr;
    spa_hook        streamListener{};
    spa_video_info_raw videoFormat{};
    bool                formatKnown = false;
    bool                loggedFirstBuffer = false; // diagnostico: solo logea el primer buffer

    std::mutex           frameMutex;
    std::vector<uint8_t> frameRGBA;  // escrito por el hilo de PipeWire
    int                  frameW = 0, frameH = 0;
    bool                 hasFrame = false;
    std::vector<uint8_t> renderCopy; // solo tocado por GrabFrame (hilo de render)

    ~Impl() { TeardownAll(); }

    void SetState(State s, const std::string& err = "") {
        {
            std::lock_guard<std::mutex> lk(stateMutex);
            errorMessage = err;
        }
        state.store(s);
    }

    void TeardownPipeWire() {
        if (loop) pw_thread_loop_stop(loop);
        if (stream) { pw_stream_destroy(stream); stream = nullptr; }
        if (core) { pw_core_disconnect(core); core = nullptr; }
        if (context) { pw_context_destroy(context); context = nullptr; }
        if (loop) { pw_thread_loop_destroy(loop); loop = nullptr; }
        formatKnown = false;
        loggedFirstBuffer = false;
    }

    void CloseSession() {
        if (dbusConn && !sessionHandle.empty()) {
            DBusMessage* msg = dbus_message_new_method_call(
                kPortalDest, sessionHandle.c_str(), kSessionIface, "Close");
            if (msg) {
                DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbusConn, msg, 2000, nullptr);
                if (reply) dbus_message_unref(reply);
                dbus_message_unref(msg);
            }
        }
        sessionHandle.clear();
    }

    void TeardownAll() {
        cancelRequested.store(true);
        if (setupThread.joinable()) setupThread.join();
        TeardownPipeWire();
        CloseSession();
        if (dbusConn) {
            dbus_connection_close(dbusConn);
            dbus_connection_unref(dbusConn);
            dbusConn = nullptr;
        }
        cancelRequested.store(false);
        {
            std::lock_guard<std::mutex> lk(frameMutex);
            hasFrame = false;
        }
    }

    // ── Callbacks de PipeWire (corren en el hilo interno de `loop`) ─────────
    static void OnStreamStateChanged(void* data, enum pw_stream_state old,
                                      enum pw_stream_state s, const char* error) {
        (void)old;
        auto* self = static_cast<Impl*>(data);
        std::fprintf(stderr, "[WaylandCapture] stream state -> %s%s%s\n",
                     pw_stream_state_as_string(s),
                     error ? " error=" : "", error ? error : "");
        switch (s) {
            case PW_STREAM_STATE_STREAMING:
                self->SetState(State::Streaming);
                break;
            case PW_STREAM_STATE_ERROR:
                self->SetState(State::Error, error ? error : "error de stream de PipeWire");
                break;
            case PW_STREAM_STATE_UNCONNECTED:
                if (self->state.load() == State::Streaming)
                    self->SetState(State::Error, "la fuente dejo de compartirse");
                break;
            default:
                break;
        }
    }

    static void OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* param) {
        auto* self = static_cast<Impl*>(data);
        if (!param || id != SPA_PARAM_Format) return;
        spa_video_info_raw info{};
        if (spa_format_video_raw_parse(param, &info) < 0) {
            std::fprintf(stderr, "[WaylandCapture] spa_format_video_raw_parse fallo\n");
            return;
        }
        self->videoFormat = info;
        self->formatKnown = true;
        std::fprintf(stderr, "[WaylandCapture] formato negociado: %dx%d formato=%d framerate=%d/%d\n",
                     info.size.width, info.size.height, (int)info.format,
                     info.framerate.num, info.framerate.denom);
    }

    static void OnStreamProcess(void* data) {
        auto* self = static_cast<Impl*>(data);
        struct pw_buffer* b = pw_stream_dequeue_buffer(self->stream);
        if (!b) return;

        struct spa_buffer* buf = b->buffer;
        bool loggedThisCall = false;
        if (!self->loggedFirstBuffer) {
            loggedThisCall = true;
            self->loggedFirstBuffer = true;
            std::fprintf(stderr,
                "[WaylandCapture] primer buffer: n_datas=%u formatKnown=%d data=%p chunk=%p "
                "chunk.size=%u chunk.offset=%u chunk.stride=%d\n",
                buf->n_datas, (int)self->formatKnown,
                buf->n_datas > 0 ? buf->datas[0].data : nullptr,
                (void*)(buf->n_datas > 0 ? buf->datas[0].chunk : nullptr),
                buf->n_datas > 0 && buf->datas[0].chunk ? buf->datas[0].chunk->size : 0,
                buf->n_datas > 0 && buf->datas[0].chunk ? buf->datas[0].chunk->offset : 0,
                buf->n_datas > 0 && buf->datas[0].chunk ? buf->datas[0].chunk->stride : 0);
        }

        if (self->formatKnown && buf->n_datas > 0 && buf->datas[0].data && buf->datas[0].chunk) {
            int w = static_cast<int>(self->videoFormat.size.width);
            int h = static_cast<int>(self->videoFormat.size.height);
            if (w > 0 && h > 0) {
                const uint8_t* base   = static_cast<const uint8_t*>(buf->datas[0].data);
                const uint8_t* src    = base + buf->datas[0].chunk->offset;
                int32_t        stride = buf->datas[0].chunk->stride > 0
                                    ? buf->datas[0].chunk->stride : w * 4;
                bool swapRB = (self->videoFormat.format == SPA_VIDEO_FORMAT_BGRA ||
                               self->videoFormat.format == SPA_VIDEO_FORMAT_BGRx);

                std::lock_guard<std::mutex> lk(self->frameMutex);
                self->frameRGBA.assign(static_cast<size_t>(w) * h * 4, 0);
                for (int y = 0; y < h; ++y) {
                    const uint8_t* rowSrc = src + static_cast<size_t>(y) * stride;
                    uint8_t*       rowDst = self->frameRGBA.data() + static_cast<size_t>(y) * w * 4;
                    for (int x = 0; x < w; ++x) {
                        uint8_t b0 = rowSrc[x * 4 + 0];
                        uint8_t g0 = rowSrc[x * 4 + 1];
                        uint8_t r0 = rowSrc[x * 4 + 2];
                        rowDst[x * 4 + 0] = swapRB ? r0 : b0;
                        rowDst[x * 4 + 1] = g0;
                        rowDst[x * 4 + 2] = swapRB ? b0 : r0;
                        rowDst[x * 4 + 3] = 255;
                    }
                }
                self->frameW = w; self->frameH = h;
                self->hasFrame = true;

                if (loggedThisCall) {
                    std::fprintf(stderr,
                        "[WaylandCapture] primer frame copiado %dx%d, primeros px BGRx/RGBA-crudos: "
                        "%02x %02x %02x %02x | %02x %02x %02x %02x\n",
                        w, h, src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);
                }
            }
        } else if (loggedThisCall) {
            std::fprintf(stderr, "[WaylandCapture] primer buffer sin datos utilizables, se descarta\n");
        }

        pw_stream_queue_buffer(self->stream, b);
    }

    // Negociacion D-Bus con el portal + arranque de PipeWire. Corre en
    // setupThread, nunca en el hilo de render (el picker/dialogo de KDE
    // puede tardar arbitrariamente hasta que el usuario interactua).
    void SetupThreadFunc() {
        DBusError err;
        dbus_error_init(&err);
        dbusConn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
        if (!dbusConn) {
            SetState(State::Error, dbus_error_is_set(&err) ? err.message : "no se pudo conectar al bus de sesion");
            dbus_error_free(&err);
            return;
        }
        dbus_connection_set_exit_on_disconnect(dbusConn, FALSE);
        dbus_bus_add_match(dbusConn,
            "type='signal',interface='org.freedesktop.portal.Request',member='Response'", &err);
        dbus_connection_flush(dbusConn);

        std::string errStr;

        // 1) CreateSession
        std::string sessionToken = MakeToken("pt_s_");
        std::string handle = CallPortalRequestMethod(dbusConn, "CreateSession",
            [&](DBusMessageIter& args) {
                DBusMessageIter dict;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
                std::string handleToken = MakeToken("pt_h_");
                AppendStringOption(dict, "handle_token", handleToken.c_str());
                AppendStringOption(dict, "session_handle_token", sessionToken.c_str());
                dbus_message_iter_close_container(&args, &dict);
            }, &errStr);
        if (handle.empty()) { SetState(State::Error, "CreateSession: " + errStr); return; }

        PortalResult resp = WaitForResponse(dbusConn, handle, 120000, &cancelRequested);
        if (!resp.ok) {
            if (resp.msg) dbus_message_unref(resp.msg);
            SetState(State::Error, resp.error.empty() ? "el usuario cancelo o denego el permiso" : resp.error);
            return;
        }
        {
            DBusMessageIter iter;
            dbus_message_iter_init(resp.msg, &iter);
            dbus_message_iter_next(&iter); // saltar 'u response', ir a 'a{sv} results'
            DictGetString(&iter, "session_handle", sessionHandle);
        }
        dbus_message_unref(resp.msg);
        if (sessionHandle.empty()) { SetState(State::Error, "el portal no devolvio session_handle"); return; }

        // 2) SelectSources: pantalla + ventana juntas, el picker de KDE deja
        // elegir cual de las dos en su propio dialogo.
        handle = CallPortalRequestMethod(dbusConn, "SelectSources",
            [&](DBusMessageIter& args) {
                const char* sessionPath = sessionHandle.c_str();
                dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sessionPath);
                DBusMessageIter dict;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
                AppendUint32Option(dict, "types", 3u);       // MONITOR(1) | WINDOW(2)
                AppendUint32Option(dict, "cursor_mode", 2u); // Embedded
                AppendBoolOption(dict, "multiple", false);
                std::string handleToken = MakeToken("pt_h_");
                AppendStringOption(dict, "handle_token", handleToken.c_str());
                dbus_message_iter_close_container(&args, &dict);
            }, &errStr);
        if (handle.empty()) { SetState(State::Error, "SelectSources: " + errStr); return; }

        resp = WaitForResponse(dbusConn, handle, 120000, &cancelRequested);
        if (!resp.ok) {
            if (resp.msg) dbus_message_unref(resp.msg);
            SetState(State::Error, resp.error.empty() ? "no se pudieron seleccionar fuentes" : resp.error);
            return;
        }
        dbus_message_unref(resp.msg);

        // 3) Start: dispara el dialogo de permiso real de KDE.
        handle = CallPortalRequestMethod(dbusConn, "Start",
            [&](DBusMessageIter& args) {
                const char* sessionPath = sessionHandle.c_str();
                dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sessionPath);
                const char* parentWindow = "";
                dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parentWindow);
                DBusMessageIter dict;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
                std::string handleToken = MakeToken("pt_h_");
                AppendStringOption(dict, "handle_token", handleToken.c_str());
                dbus_message_iter_close_container(&args, &dict);
            }, &errStr);
        if (handle.empty()) { SetState(State::Error, "Start: " + errStr); return; }

        resp = WaitForResponse(dbusConn, handle, 120000, &cancelRequested);
        if (!resp.ok) {
            if (resp.msg) dbus_message_unref(resp.msg);
            SetState(State::Error, resp.error.empty() ? "el usuario cancelo la selección" : resp.error);
            return;
        }
        uint32_t nodeId = 0;
        {
            DBusMessageIter iter;
            dbus_message_iter_init(resp.msg, &iter);
            dbus_message_iter_next(&iter);
            if (!DictGetFirstStreamNodeId(&iter, nodeId)) {
                dbus_message_unref(resp.msg);
                SetState(State::Error, "el portal no devolvio ningun stream");
                return;
            }
        }
        dbus_message_unref(resp.msg);

        // 4) OpenPipeWireRemote: sincronico, sin Request/Response de por medio.
        DBusMessage* msg = dbus_message_new_method_call(
            kPortalDest, kPortalPath, kScreenCastIface, "OpenPipeWireRemote");
        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        const char* sessionPath = sessionHandle.c_str();
        dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sessionPath);
        DBusMessageIter dict;
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
        dbus_message_iter_close_container(&args, &dict);

        DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbusConn, msg, 5000, &err);
        dbus_message_unref(msg);
        if (!reply) {
            SetState(State::Error, dbus_error_is_set(&err) ? err.message : "OpenPipeWireRemote fallo");
            dbus_error_free(&err);
            return;
        }
        int pwFd = -1;
        {
            DBusMessageIter it;
            dbus_message_iter_init(reply, &it);
            dbus_message_iter_get_basic(&it, &pwFd);
        }
        dbus_message_unref(reply);
        if (pwFd < 0) { SetState(State::Error, "file descriptor de PipeWire invalido"); return; }

        SetupPipeWire(pwFd, nodeId);
    }

    void SetupPipeWire(int pwFd, uint32_t nodeId) {
        static std::once_flag pwInitFlag;
        std::call_once(pwInitFlag, [] { pw_init(nullptr, nullptr); });

        loop = pw_thread_loop_new("pt-wayland-capture", nullptr);
        if (!loop) { SetState(State::Error, "no se pudo crear el loop de PipeWire"); ::close(pwFd); return; }

        pw_thread_loop_lock(loop);

        context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
        if (!context) {
            pw_thread_loop_unlock(loop);
            SetState(State::Error, "no se pudo crear el contexto de PipeWire");
            ::close(pwFd);
            return;
        }

        core = pw_context_connect_fd(context, pwFd, nullptr, 0); // toma ownership de pwFd
        if (!core) {
            pw_thread_loop_unlock(loop);
            SetState(State::Error, "no se pudo conectar PipeWire al remoto del portal");
            return;
        }

        pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr);
        stream = pw_stream_new(core, "ProyecThor Captura", props);
        if (!stream) {
            pw_thread_loop_unlock(loop);
            SetState(State::Error, "no se pudo crear el stream de PipeWire");
            return;
        }

        static const struct pw_stream_events kStreamEvents = [] {
            struct pw_stream_events ev{};
            ev.version       = PW_VERSION_STREAM_EVENTS;
            ev.state_changed = &Impl::OnStreamStateChanged;
            ev.param_changed = &Impl::OnStreamParamChanged;
            ev.process       = &Impl::OnStreamProcess;
            return ev;
        }();
        pw_stream_add_listener(stream, &streamListener, &kStreamEvents, this);

        uint8_t podBuffer[1024];
        spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        struct spa_pod_frame f;
        // C++ no permite tomar la direccion de un compound-literal rvalue
        // (a diferencia de C99, donde SPA_RECTANGLE/SPA_FRACTION son
        // lvalues) -- de ahi las variables con nombre en vez de pasar los
        // macros directamente por referencia.
        struct spa_rectangle sizeDefault = SPA_RECTANGLE(1920, 1080);
        struct spa_rectangle sizeMin     = SPA_RECTANGLE(1, 1);
        struct spa_rectangle sizeMax     = SPA_RECTANGLE(8192, 8192);
        struct spa_fraction  rateDefault = SPA_FRACTION(30, 1);
        struct spa_fraction  rateMin     = SPA_FRACTION(0, 1);
        struct spa_fraction  rateMax     = SPA_FRACTION(240, 1);
        spa_pod_builder_push_object(&b, &f, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
        spa_pod_builder_add(&b,
            SPA_FORMAT_mediaType,    SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4,
                SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRA,
                SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_BGRx),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(
                &sizeDefault, &sizeMin, &sizeMax),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                &rateDefault, &rateMin, &rateMax),
            0);
        const struct spa_pod* params[1];
        params[0] = static_cast<const struct spa_pod*>(spa_pod_builder_pop(&b, &f));

        // node_id como target_id: aunque pw_stream_connect lo marca como
        // "deprecated" en favor de la propiedad PW_KEY_TARGET_OBJECT, sigue
        // siendo la forma documentada/de referencia para consumir
        // exactamente el nodo que entrega el portal de ScreenCast.
        int connectRes = pw_stream_connect(stream, PW_DIRECTION_INPUT, nodeId,
            static_cast<enum pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
            params, 1);

        pw_thread_loop_unlock(loop);

        if (connectRes < 0) {
            SetState(State::Error, "no se pudo conectar el stream de PipeWire");
            return;
        }

        pw_thread_loop_start(loop);
        // El estado pasa a Streaming desde OnStreamStateChanged cuando el
        // stream realmente arranca; hasta entonces sigue en Requesting.
    }
};

WaylandScreenCapture::WaylandScreenCapture() : m_Impl(std::make_unique<Impl>()) {}
WaylandScreenCapture::~WaylandScreenCapture() = default;

bool WaylandScreenCapture::IsWaylandSession() {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if (sessionType && std::string(sessionType) == "wayland") return true;
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    return waylandDisplay && waylandDisplay[0] != '\0';
}

void WaylandScreenCapture::RequestSession() {
    m_Impl->TeardownAll(); // por si habia una sesion previa
    m_Impl->SetState(State::Requesting);
    m_Impl->setupThread = std::thread(&Impl::SetupThreadFunc, m_Impl.get());
}

void WaylandScreenCapture::Stop() {
    m_Impl->TeardownAll();
    m_Impl->SetState(State::Idle);
}

WaylandScreenCapture::State WaylandScreenCapture::GetState() const {
    return m_Impl->state.load();
}

std::string WaylandScreenCapture::GetErrorMessage() const {
    std::lock_guard<std::mutex> lk(m_Impl->stateMutex);
    return m_Impl->errorMessage;
}

const uint8_t* WaylandScreenCapture::GrabFrame(int& w, int& h) {
    Impl* impl = m_Impl.get();
    std::lock_guard<std::mutex> lk(impl->frameMutex);
    if (!impl->hasFrame) return nullptr;
    impl->renderCopy = impl->frameRGBA; // copia bajo lock: el puntero devuelto
                                         // debe seguir siendo valido despues
                                         // de soltar el lock, y el hilo de
                                         // PipeWire puede reescribir frameRGBA
                                         // en cualquier momento.
    w = impl->frameW; h = impl->frameH;
    return impl->renderCopy.data();
}

} // namespace ProyecThor::UI

#endif // PT_HAVE_WAYLAND_CAPTURE
