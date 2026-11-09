#include "ThumbnailWorker.h"
#include "AppPaths.h"
#include "MuninDRM.h"
#include "core/settings/SettingsManager.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <vlc/vlc.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

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

    // Escalamos directamente en libVLC al tamaño de miniatura para no reservar
    // búferes de 1080p/4K en RAM solo por una miniatura.
    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    const unsigned maxW = perf.lowMemoryMode ? 256u : (perf.performanceMode ? 320u : 512u);
    const unsigned maxH = perf.lowMemoryMode ? 144u : (perf.performanceMode ? 180u : 288u);
    if (*width > 0 && *height > 0 && (*width > maxW || *height > maxH)) {
        const double scale = std::min(static_cast<double>(maxW) / static_cast<double>(*width),
                                      static_cast<double>(maxH) / static_cast<double>(*height));
        *width  = std::max(16u, (static_cast<unsigned>(*width  * scale + 0.5) + 1u) & ~1u);
        *height = std::max(16u, (static_cast<unsigned>(*height * scale + 0.5) + 1u) & ~1u);
    }

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

std::filesystem::path ResolveFsPath(const std::string& path)
{
    std::error_code ec;
    std::filesystem::path p = std::filesystem::u8path(path);
    if (std::filesystem::exists(p, ec))
        return p;
    ec.clear();
    return std::filesystem::path(path);
}

// Decodifica el primer frame real de "path" a RGBA crudo. Corre en el hilo
// del worker — nunca en el hilo de UI/GL.
bool DecodeFirstFrame(const std::string& path, std::vector<uint8_t>& outPixels,
                       unsigned& outW, unsigned& outH, int maxWaitMs = 2000)
{
    libvlc_instance_t* inst = GetThumbVlcInstance();
    if (!inst) return false;

    const std::string vlcUtf8Path = ResolveFsPath(path).make_preferred().u8string();
    libvlc_media_t* media = libvlc_media_new_path(inst, vlcUtf8Path.c_str());
    if (!media) return false;
    libvlc_media_add_option(media, ":file-caching=80");
    libvlc_media_add_option(media, ":no-audio");

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
        while (waited < maxWaitMs) {
            bool got;
            { std::lock_guard<std::mutex> lk(ctx.mutex); got = ctx.gotFrame && ctx.width > 0; }
            if (got) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waited += 10;
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

void WritePngUtf8Safe(const std::string& path, int w, int h, const uint8_t* rgba)
{
    if (path.empty() || w <= 0 || h <= 0 || !rgba) return;
    std::error_code ec;
    std::filesystem::path outPath = std::filesystem::u8path(path);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path(), ec);

    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
        out.open(std::filesystem::path(path), std::ios::binary);
    }
    if (!out.is_open()) return;

    auto writeCb = [](void* context, void* data, int size) {
        auto* stream = static_cast<std::ofstream*>(context);
        if (stream && size > 0)
            stream->write(static_cast<const char*>(data), size);
    };
    stbi_write_png_to_func(writeCb, &out, w, h, 4, rgba, w * 4);
}

} // namespace

void ThumbnailWorker::WarmupVlcEngine()
{
    (void)GetThumbVlcInstance();

    // Precarga en RAM las DLLs de demux + avcodec + swscale + vmem de libVLC
    // decodificando 1 fotograma del video local más liviano disponible durante
    // el SplashScreen (evita el tirón en frío al activar el primer fondo).
    auto isVideoExt = [](const std::filesystem::path& p) {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".avi" || ext == ".webm";
    };

    std::filesystem::path bestCandidate;
    uintmax_t bestSize = UINTMAX_MAX;

    auto scanDir = [&](const std::filesystem::path& root) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) return;
        for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            if (!isVideoExt(it->path())) continue;
            uintmax_t sz = it->file_size(ec);
            if (!ec && sz > 0 && sz < bestSize) {
                bestSize = sz;
                bestCandidate = it->path();
                if (bestSize < (4ull * 1024ull * 1024ull))
                    break;
            }
        }
    };

    scanDir(std::filesystem::path(ProyecThor::GetAssetsPath()) / "backgrounds");
    if (bestCandidate.empty())
        scanDir(std::filesystem::path(ProyecThor::VideosPath()));

    if (!bestCandidate.empty()) {
        std::vector<uint8_t> dummyPixels;
        unsigned w = 0, h = 0;
        (void)DecodeFirstFrame(bestCandidate.u8string(), dummyPixels, w, h, 450);
    }
}

void ThumbnailWorker::DownscaleRGBAIfNeeded(std::vector<uint8_t>& pixels,
                                            unsigned& width, unsigned& height,
                                            unsigned maxW, unsigned maxH)
{
    if (pixels.empty() || width == 0 || height == 0 || maxW == 0 || maxH == 0)
        return;
    if (width <= maxW && height <= maxH)
        return;

    const double scaleX = static_cast<double>(maxW) / static_cast<double>(width);
    const double scaleY = static_cast<double>(maxH) / static_cast<double>(height);
    const double scale  = std::min(scaleX, scaleY);

    const unsigned dstW = std::max(1u, static_cast<unsigned>(width  * scale + 0.5));
    const unsigned dstH = std::max(1u, static_cast<unsigned>(height * scale + 0.5));
    if (dstW >= width && dstH >= height)
        return;

    std::vector<uint8_t> dst(static_cast<size_t>(dstW) * dstH * 4, 0);

    for (unsigned dy = 0; dy < dstH; ++dy) {
        const unsigned sy0 = (dy * height) / dstH;
        const unsigned sy1 = std::max(sy0 + 1, ((dy + 1) * height) / dstH);
        for (unsigned dx = 0; dx < dstW; ++dx) {
            const unsigned sx0 = (dx * width) / dstW;
            const unsigned sx1 = std::max(sx0 + 1, ((dx + 1) * width) / dstW);

            uint64_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            uint64_t count = 0;
            for (unsigned sy = sy0; sy < sy1 && sy < height; ++sy) {
                const uint8_t* row = pixels.data() + (static_cast<size_t>(sy) * width + sx0) * 4;
                for (unsigned sx = sx0; sx < sx1 && sx < width; ++sx) {
                    const uint64_t a = row[3];
                    sumR += static_cast<uint64_t>(row[0]) * a;
                    sumG += static_cast<uint64_t>(row[1]) * a;
                    sumB += static_cast<uint64_t>(row[2]) * a;
                    sumA += a;
                    ++count;
                    row += 4;
                }
            }

            uint8_t* out = dst.data() + (static_cast<size_t>(dy) * dstW + dx) * 4;
            if (count > 0 && sumA > 0) {
                out[0] = static_cast<uint8_t>(std::min<uint64_t>(255, (sumR + sumA / 2) / sumA));
                out[1] = static_cast<uint8_t>(std::min<uint64_t>(255, (sumG + sumA / 2) / sumA));
                out[2] = static_cast<uint8_t>(std::min<uint64_t>(255, (sumB + sumA / 2) / sumA));
                out[3] = static_cast<uint8_t>(std::min<uint64_t>(255, (sumA + count / 2) / count));
            }
        }
    }

    pixels.swap(dst);
    width  = dstW;
    height = dstH;
}

bool ThumbnailWorker::LoadThumbnailPixelsFromDisk(const std::string& path,
                                                  std::vector<uint8_t>& outPixels,
                                                  unsigned& outW, unsigned& outH,
                                                  unsigned maxW, unsigned maxH)
{
    outPixels.clear();
    outW = outH = 0;
    if (path.empty()) return false;

    std::filesystem::path fsPath = ResolveFsPath(path);
    std::ifstream in(fsPath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    const std::streamsize fileSz = in.tellg();
    if (fileSz <= 0) return false;
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> raw(static_cast<size_t>(fileSz));
    if (!in.read(reinterpret_cast<char*>(raw.data()), fileSz))
        return false;

    // Desencriptación en memoria si el archivo está protegido con MuninDRM
    std::vector<uint8_t> decrypted;
    const uint8_t* decodePtr = raw.data();
    size_t decodeLen = raw.size();

    if (MuninDRM::IsEncrypted(raw.data(), raw.size())) {
        if (MuninDRM::DecryptBuffer(raw.data(), raw.size(), decrypted)) {
            decodePtr = decrypted.data();
            decodeLen = decrypted.size();
        } else {
            return false;
        }
    }

    int w = 0, h = 0, n = 0;
    unsigned char* decoded = stbi_load_from_memory(decodePtr, static_cast<int>(decodeLen), &w, &h, &n, 4);
    if (!decoded || w <= 0 || h <= 0) {
        if (decoded) stbi_image_free(decoded);
        return false;
    }

    outW = static_cast<unsigned>(w);
    outH = static_cast<unsigned>(h);
    outPixels.assign(decoded, decoded + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    stbi_image_free(decoded);

    DownscaleRGBAIfNeeded(outPixels, outW, outH, maxW, maxH);
    return true;
}

ThumbnailWorker::ThumbnailWorker()
{
    m_Running.store(true);
    // El hilo secundario se crea perezosamente en el primer Request() para no
    // reservar pilas de hilos inactivos en equipos con RAM mínima.
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
    if (!m_Thread.joinable() && m_Running.load()) {
        m_Thread = std::thread([this] { ThreadFunc(); });
    }
    m_Seen.push_back(key);
    m_Pending.push_back({ key, videoPath, cachePngPath });
    m_Cv.notify_one();
}

void ThumbnailWorker::ResetSeen()
{
    std::lock_guard<std::mutex> lk(m_QueueMutex);
    m_Seen.clear();
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
            const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
            const unsigned maxW = perf.lowMemoryMode ? 256u : (perf.performanceMode ? 320u : 512u);
            const unsigned maxH = perf.lowMemoryMode ? 144u : (perf.performanceMode ? 180u : 288u);
            DownscaleRGBAIfNeeded(r.pixels, r.width, r.height, maxW, maxH);
            if (!item.cachePngPath.empty()) {
                WritePngUtf8Safe(item.cachePngPath, static_cast<int>(r.width),
                                 static_cast<int>(r.height), r.pixels.data());
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
