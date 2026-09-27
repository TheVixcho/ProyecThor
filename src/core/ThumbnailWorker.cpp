#include "ThumbnailWorker.h"
#include "stb_image_write.h"
#include <vlc/vlc.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace ProyecThor::Core {

namespace {

// ── Captura headless de un frame via callbacks de video de libVLC ────────
// Mismo patron que VLCBasePlayer (vlc_format/vlc_lock/vlc_unlock): al
// registrar estos callbacks, libVLC nunca crea una ventana de salida propia
// — el pixel data va directo a este buffer en memoria.
struct FrameCtx {
    std::mutex            mutex;
    std::vector<uint8_t>  buf;
    unsigned              width  = 0;
    unsigned              height = 0;
    bool                  gotFrame = false;
};

unsigned FormatCB(void** opaque, char* chroma, unsigned* width, unsigned* height,
                   unsigned* pitches, unsigned* lines)
{
    auto* ctx = static_cast<FrameCtx*>(*opaque);
    std::memcpy(chroma, "RGBA", 4);
    *pitches = (*width) * 4;
    *lines   = *height;

    std::lock_guard<std::mutex> lk(ctx->mutex);
    ctx->width    = *width;
    ctx->height   = *height;
    ctx->buf.assign(static_cast<size_t>(*pitches) * (*lines), 0);
    ctx->gotFrame = false;
    return 1;
}

void* LockCB(void* opaque, void** planes)
{
    auto* ctx = static_cast<FrameCtx*>(opaque);
    ctx->mutex.lock();
    *planes = ctx->buf.data();
    return nullptr;
}

void UnlockCB(void* opaque, void* /*picture*/, void* const* /*planes*/)
{
    auto* ctx = static_cast<FrameCtx*>(opaque);
    ctx->gotFrame = true;
    ctx->mutex.unlock();
}

void DisplayCB(void* /*opaque*/, void* /*picture*/) {}

// Instancia libVLC compartida y perezosa, solo para miniaturas — evita
// pagar el costo de escanear el cache de plugins en cada miniatura.
libvlc_instance_t* GetThumbVlcInstance()
{
    static libvlc_instance_t* inst = []() -> libvlc_instance_t* {
        const char* args[] = { "--no-xlib", "--quiet", "--no-osd" };
        return libvlc_new(sizeof(args) / sizeof(args[0]), args);
    }();
    return inst;
}

// Decodifica el primer frame real de "path" a RGBA crudo. Corre en el hilo
// del worker — nunca en el hilo de UI/GL.
bool DecodeFirstFrame(const std::string& path, std::vector<uint8_t>& outPixels,
                       unsigned& outW, unsigned& outH)
{
    libvlc_instance_t* inst = GetThumbVlcInstance();
    if (!inst) return false;

    libvlc_media_t* media = libvlc_media_new_path(inst, path.c_str());
    if (!media) return false;

    libvlc_media_player_t* mp = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    if (!mp) return false;

    FrameCtx ctx;
    libvlc_video_set_format_callbacks(mp, FormatCB, nullptr);
    libvlc_video_set_callbacks(mp, LockCB, UnlockCB, DisplayCB, &ctx);
    libvlc_audio_set_mute(mp, 1);

    bool ok = false;
    if (libvlc_media_player_play(mp) == 0) {
        // Pedir el frame demasiado pronto (antes de que arranque el decode
        // real) todavia no tiene nada que devolver — se espera a que el
        // callback de formato + al menos un lock/unlock hayan corrido.
        int waited = 0;
        while (waited < 3000) {
            bool got;
            { std::lock_guard<std::mutex> lk(ctx.mutex); got = ctx.gotFrame && ctx.width > 0; }
            if (got) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            waited += 30;
        }

        std::lock_guard<std::mutex> lk(ctx.mutex);
        if (ctx.gotFrame && ctx.width > 0 && ctx.height > 0) {
            outPixels = ctx.buf;
            outW = ctx.width;
            outH = ctx.height;
            ok = true;
        }
    }

    libvlc_media_player_stop(mp);
    libvlc_media_player_release(mp);
    return ok;
}

} // namespace

ThumbnailWorker::ThumbnailWorker()
{
    m_Running.store(true);
    m_Thread = std::thread([this] { ThreadFunc(); });
}

ThumbnailWorker::~ThumbnailWorker()
{
    m_Running.store(false);
    m_Cv.notify_all();
    if (m_Thread.joinable()) m_Thread.join();
}

void ThumbnailWorker::Request(const std::string& key, const std::string& videoPath,
                              const std::string& cachePngPath)
{
    std::lock_guard<std::mutex> lk(m_QueueMutex);
    if (std::find(m_Seen.begin(), m_Seen.end(), key) != m_Seen.end())
        return;
    m_Seen.push_back(key);
    m_Pending.push_back({ key, videoPath, cachePngPath });
    m_Cv.notify_one();
}

void ThumbnailWorker::DrainResults(std::vector<Result>& out)
{
    std::lock_guard<std::mutex> lk(m_ResultsMutex);
    out.insert(out.end(), std::make_move_iterator(m_Results.begin()),
               std::make_move_iterator(m_Results.end()));
    m_Results.clear();
}

size_t ThumbnailWorker::PendingCount() const
{
    std::lock_guard<std::mutex> lk(m_QueueMutex);
    return m_Pending.size() + static_cast<size_t>(m_InFlight);
}

void ThumbnailWorker::ThreadFunc()
{
    while (m_Running.load()) {
        PendingItem item;
        {
            std::unique_lock<std::mutex> lk(m_QueueMutex);
            m_Cv.wait(lk, [this] { return !m_Pending.empty() || !m_Running.load(); });
            if (!m_Running.load()) break;
            item = std::move(m_Pending.front());
            m_Pending.pop_front();
            m_InFlight++;
        }

        Result r;
        r.key = item.key;
        if (DecodeFirstFrame(item.videoPath, r.pixels, r.width, r.height)) {
            if (!item.cachePngPath.empty()) {
                stbi_write_png(item.cachePngPath.c_str(), static_cast<int>(r.width),
                               static_cast<int>(r.height), 4, r.pixels.data(),
                               static_cast<int>(r.width) * 4);
            }
        }

        {
            std::lock_guard<std::mutex> lk(m_ResultsMutex);
            m_Results.push_back(std::move(r));
        }
        {
            std::lock_guard<std::mutex> lk(m_QueueMutex);
            m_InFlight--;
        }
    }
}

} // namespace ProyecThor::Core
