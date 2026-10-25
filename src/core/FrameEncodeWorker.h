#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace ProyecThor::Core {

// ── FrameEncodeWorker ─────────────────────────────────────────────────────────
// Codifica frames RGB a JPEG en un hilo dedicado. El encode de stb_image
// (stbi_write_jpg) puede tomar varias decenas de ms en CPU debil; hacerlo
// inline en el hilo de render/UI (como hacian StreamingPanel/StageDisplayPanel
// antes) produce un hitch visible justo en el mismo hilo que dibuja la
// ventana del proyector.
//
// Politica "ultimo frame gana": SubmitFrame() reemplaza el frame pendiente en
// vez de encolarlo. Si llega un frame nuevo mientras el anterior se esta
// codificando, el anterior se descarta — nunca se acumula backlog ni se
// entrega un frame viejo con retraso creciente.
class FrameEncodeWorker {
public:
    using EncodedCallback = std::function<void(std::vector<uint8_t> jpegBytes)>;

    FrameEncodeWorker();
    ~FrameEncodeWorker();

    FrameEncodeWorker(const FrameEncodeWorker&)            = delete;
    FrameEncodeWorker& operator=(const FrameEncodeWorker&) = delete;

    // Llamable desde el hilo de render. onEncoded se invoca desde el hilo
    // worker (no desde el hilo llamante) una vez terminado el encode.
    void SubmitFrame(std::vector<uint8_t> rgb, int w, int h, int quality, EncodedCallback onEncoded);

private:
    struct PendingFrame {
        std::vector<uint8_t> rgb;
        int             w = 0, h = 0, quality = 80;
        EncodedCallback onEncoded;
    };

    void ThreadFunc();

    std::thread                 m_Thread;
    std::atomic<bool>           m_Running{ false };
    std::mutex                  m_Mutex;
    std::condition_variable     m_Cv;
    std::optional<PendingFrame> m_Pending; // protegido por m_Mutex
};

} // namespace ProyecThor::Core
