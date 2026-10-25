#include "FrameEncodeWorker.h"
#include "stb_image_write.h"

namespace ProyecThor::Core {

static void StbCb(void* ctx, void* data, int size)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    const uint8_t* p = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), p, p + size);
}

FrameEncodeWorker::FrameEncodeWorker()
{
    m_Running.store(true);
    m_Thread = std::thread(&FrameEncodeWorker::ThreadFunc, this);
}

FrameEncodeWorker::~FrameEncodeWorker()
{
    m_Running.store(false);
    m_Cv.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
}

void FrameEncodeWorker::SubmitFrame(std::vector<uint8_t> rgb, int w, int h, int quality, EncodedCallback onEncoded)
{
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Pending = PendingFrame{ std::move(rgb), w, h, quality, std::move(onEncoded) };
    }
    m_Cv.notify_one();
}

void FrameEncodeWorker::ThreadFunc()
{
    while (true) {
        PendingFrame frame;
        {
            std::unique_lock<std::mutex> lk(m_Mutex);
            m_Cv.wait(lk, [this] { return !m_Running.load() || m_Pending.has_value(); });
            if (!m_Pending.has_value())
                return; // solo puede pasar si nos pidieron parar y no quedo nada pendiente
            frame = std::move(*m_Pending);
            m_Pending.reset();
        }

        std::vector<uint8_t> jpeg;
        jpeg.reserve(static_cast<size_t>(frame.w) * frame.h / 4);
        stbi_write_jpg_to_func(StbCb, &jpeg, frame.w, frame.h, 3, frame.rgb.data(), frame.quality);

        if (frame.onEncoded)
            frame.onEncoded(std::move(jpeg));

        if (!m_Running.load())
            return;
    }
}

} // namespace ProyecThor::Core
