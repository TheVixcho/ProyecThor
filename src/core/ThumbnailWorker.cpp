#include "ThumbnailWorker.h"
#include "AppPaths.h"
#include "core/settings/SettingsManager.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <mpv/client.h>
#include <mpv/render.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ProyecThor::Core {

namespace {

// Tope de miniaturas recordadas por el worker. En GPUs con poca VRAM (p.ej.
// GT710, 2 GB) las texturas de miniatura se acumulan sin control; limitar el
// historial de claves generadas acota cuántas texturas puede llegar a pedir
// el consumidor a la vez. Al superarlo se descarta la entrada más vieja
// (LRU): esa miniatura podrá regenerarse si se vuelve a necesitar.
constexpr size_t kMaxCachedThumbnails = 200;

std::filesystem::path ResolveFsPath(const std::string& path)
{
    std::error_code ec;
    std::filesystem::path p = std::filesystem::u8path(path);
    if (std::filesystem::exists(p, ec))
        return p;
    ec.clear();
    return std::filesystem::path(path);
}

// Decodifica el primer frame real de "path" a RGBA crudo via libmpv (SW render).
// Corre en el hilo del worker — nunca abre ventanas ni requiere contexto GL.
bool DecodeFirstFrame(const std::string& path, std::vector<uint8_t>& outPixels,
                       unsigned& outW, unsigned& outH, int maxWaitMs = 2000)
{
    std::setlocale(LC_NUMERIC, "C");

    mpv_handle* mpv = mpv_create();
    if (!mpv) return false;

    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "msg-level", "all=no");
    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "audio", "no");
    mpv_set_option_string(mpv, "aid", "no");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "hwdec", "no");
    mpv_set_option_string(mpv, "osd-level", "0");
    mpv_set_option_string(mpv, "idle", "yes");
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "video-timing-offset", "0");
    mpv_set_option_string(mpv, "sws-fast", "yes");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return false;
    }

    mpv_render_param createParams[] = {
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW) },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };
    mpv_render_context* rctx = nullptr;
    if (mpv_render_context_create(&rctx, mpv, createParams) < 0 || !rctx) {
        mpv_terminate_destroy(mpv);
        return false;
    }

    const std::string utf8Path = ResolveFsPath(path).make_preferred().u8string();
    const char* cmd[] = { "loadfile", utf8Path.c_str(), "replace", nullptr };
    bool ok = false;

    if (mpv_command(mpv, cmd) >= 0) {
        int waited = 0;
        bool failed = false;
        int64_t vw = 0, vh = 0;

        while (waited < maxWaitMs && !failed) {
            mpv_event* ev = mpv_wait_event(mpv, 0.01);
            if (ev) {
                if (ev->event_id == MPV_EVENT_END_FILE) {
                    auto* ef = static_cast<mpv_event_end_file*>(ev->data);
                    if (ef && ef->reason == MPV_END_FILE_REASON_ERROR)
                        failed = true;
                }
            }

            uint64_t flags = mpv_render_context_update(rctx);
            if ((flags & MPV_RENDER_UPDATE_FRAME) != 0) {
                if (mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &vw) >= 0 &&
                    mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &vh) >= 0 &&
                    vw > 0 && vh > 0) {
                    break;
                }
            }
            waited += 10;
        }

        if (!failed && vw > 0 && vh > 0) {
            const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
            const unsigned maxW = perf.lowMemoryMode ? 256u : (perf.performanceMode ? 320u : 512u);
            const unsigned maxH = perf.lowMemoryMode ? 144u : (perf.performanceMode ? 180u : 288u);
            unsigned tw = static_cast<unsigned>(vw);
            unsigned th = static_cast<unsigned>(vh);
            if (tw > maxW || th > maxH) {
                const double scale = std::min(static_cast<double>(maxW) / tw,
                                              static_cast<double>(maxH) / th);
                tw = std::max(16u, (static_cast<unsigned>(tw * scale + 0.5) + 1u) & ~1u);
                th = std::max(16u, (static_cast<unsigned>(th * scale + 0.5) + 1u) & ~1u);
            }

            size_t stride = static_cast<size_t>(tw) * 4u;
            std::vector<uint8_t> pixels(stride * th, 0);
            int swSize[2] = { static_cast<int>(tw), static_cast<int>(th) };
            mpv_render_param renderParams[] = {
                { MPV_RENDER_PARAM_SW_SIZE,    swSize },
                { MPV_RENDER_PARAM_SW_FORMAT,  const_cast<char*>("rgba") },
                { MPV_RENDER_PARAM_SW_STRIDE,  &stride },
                { MPV_RENDER_PARAM_SW_POINTER, pixels.data() },
                { MPV_RENDER_PARAM_INVALID,    nullptr }
            };

            if (mpv_render_context_render(rctx, renderParams) >= 0) {
                outPixels = std::move(pixels);
                outW = tw;
                outH = th;
                ok = true;
            }
        }
    }

    mpv_render_context_free(rctx);
    mpv_terminate_destroy(mpv);
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

void ThumbnailWorker::WarmupMediaEngine()
{
    // libmpv no requiere escaneo de plugins externo al arranque; precarga minima de 1 frame si existe video local.
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

    int w = 0, h = 0, n = 0;
    unsigned char* decoded = stbi_load_from_memory(raw.data(), static_cast<int>(raw.size()), &w, &h, &n, 4);
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
    // Evicción LRU: el frente de m_Seen es la clave más antigua. Mantener el
    // historial acotado evita que el consumidor acumule texturas sin límite
    // en GPUs con poca VRAM.
    if (m_Seen.size() > kMaxCachedThumbnails) {
        m_Seen.erase(m_Seen.begin(),
                     m_Seen.begin() + (m_Seen.size() - kMaxCachedThumbnails));
    }
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
