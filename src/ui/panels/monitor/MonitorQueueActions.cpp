#include "MonitorView.h"
#include "MonitorQueueEngine.h"

// =============================================================================
//  MonitorQueueActions.cpp
//  Capa fina sobre MonitorQueueEngine. Toda la logica real (deteccion de
//  fin, reordenar, quitar, reproducir) vive en MonitorQueueEngine.cpp.
// =============================================================================

namespace ProyecThor::UI {

void MonitorView::LoadPlayQueue() { m_QueueEngine.Load(); }
void MonitorView::SavePlayQueue() { m_QueueEngine.Save(); }

void MonitorView::AddToQueue(const std::string& fullPath)
{
    m_QueueEngine.Add(fullPath);
}

void MonitorView::AddURLToQueue(const std::string& url)
{
    m_QueueEngine.AddURL(url);
}

void MonitorView::PlayQueueItem(int index)
{
    m_QueueEngine.PlayIndex(index);
    m_LivePlaying = m_QueueEngine.IsActive();
}

} // namespace ProyecThor::UI