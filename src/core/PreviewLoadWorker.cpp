#include "PreviewLoadWorker.h"
#include "media/player/VLCBasePlayer.h"
#include <future>
#include <memory>
#include <chrono>

namespace ProyecThor::Core {

PreviewLoadWorker::PreviewLoadWorker()
{
    m_Running.store(true);
    m_Thread = std::thread(&PreviewLoadWorker::ThreadFunc, this);
}

PreviewLoadWorker::~PreviewLoadWorker()
{
    m_Running.store(false);
    m_Cv.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
}

void PreviewLoadWorker::Request(std::function<void()> action)
{
    if (!action) return;
    {
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_Queue.push_back(std::move(action));
    }
    m_Cv.notify_one();
}

void PreviewLoadWorker::RequestLoad(VLCBasePlayer* player, const std::string& path, bool loop, bool startMuted)
{
    if (!player) return;
    Request([player, path, loop, startMuted] { player->Play(path, loop, startMuted); });
}

void PreviewLoadWorker::RequestStop(VLCBasePlayer* player)
{
    if (!player) return;
    Request([player] { player->Stop(); });
}

void PreviewLoadWorker::RequestStopSync(VLCBasePlayer* player, int timeoutMs)
{
    if (!player) return;
    auto prom = std::make_shared<std::promise<void>>();
    auto fut  = prom->get_future();
    Request([player, prom] {
        player->Stop();
        try { prom->set_value(); } catch (...) {}
    });
    if (fut.valid())
        fut.wait_for(std::chrono::milliseconds(timeoutMs));
}

void PreviewLoadWorker::Flush(int timeoutMs)
{
    auto prom = std::make_shared<std::promise<void>>();
    auto fut  = prom->get_future();
    Request([prom] {
        try { prom->set_value(); } catch (...) {}
    });
    if (fut.valid())
        fut.wait_for(std::chrono::milliseconds(timeoutMs));
}

void PreviewLoadWorker::ThreadFunc()
{
    while (true) {
        std::function<void()> action;
        {
            std::unique_lock<std::mutex> lk(m_Mutex);
            m_Cv.wait(lk, [this] { return !m_Running.load() || !m_Queue.empty(); });
            if (!m_Running.load() && m_Queue.empty())
                return;
            if (!m_Queue.empty()) {
                action = std::move(m_Queue.front());
                m_Queue.pop_front();
            }
        }

        if (action)
            action();

        if (!m_Running.load() && m_Queue.empty())
            return;
    }
}

} // namespace ProyecThor::Core
