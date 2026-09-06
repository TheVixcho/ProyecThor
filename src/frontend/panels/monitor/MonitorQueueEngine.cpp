#include "MonitorQueueEngine.h"
#include "MonitorQueueHelpers.h"
#include "MonitorQueueIO.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/settings/SettingsManager.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace ProyecThor::UI {
using namespace QueueHelpers;

static Core::VLCBasePlayer* ActivePlayer()
{
    return Core::PresentationCore::Get().GetBackgroundPlayer();
}

void MonitorQueueEngine::Load()
{
    QueueIO::LoadQueue(m_Items);
    m_CurrentIndex  = -1;
    m_SelectedIndex = -1;
    m_State         = QueueState::Stopped;
}

void MonitorQueueEngine::Save() const { QueueIO::SaveQueue(m_Items); }

void MonitorQueueEngine::Add(const std::string& fullPath)
{
    m_Items.push_back(QueueEntry(k_PfxLocal, fullPath));
    Save();
}

void MonitorQueueEngine::AddURL(const std::string& url)
{
    m_Items.push_back(QueueEntry(k_PfxURL, url));
    Save();
}

static int RemapAfterMove(int tracked, int from, int to)
{
    if (tracked < 0)     return tracked;
    if (tracked == from) return to;
    if (from < to) { if (tracked > from && tracked <= to) return tracked - 1; }
    else           { if (tracked >= to && tracked < from) return tracked + 1; }
    return tracked;
}

static int RemapAfterRemove(int tracked, int removed, int newSize)
{
    if (tracked < 0)        return -1;
    if (tracked == removed) return -1;
    if (tracked > removed)  return tracked - 1;
    if (tracked >= newSize) return newSize - 1;
    return tracked;
}

void MonitorQueueEngine::Move(int from, int to)
{
    if (from < 0 || from >= (int)m_Items.size() ||
        to   < 0 || to   >= (int)m_Items.size() || from == to)
        return;

    std::string moved = m_Items[from];
    m_Items.erase(m_Items.begin() + from);
    m_Items.insert(m_Items.begin() + to, moved);

    m_CurrentIndex  = RemapAfterMove(m_CurrentIndex,  from, to);
    m_SelectedIndex = RemapAfterMove(m_SelectedIndex, from, to);
    Save();
}

void MonitorQueueEngine::Remove(int index)
{
    if (index < 0 || index >= (int)m_Items.size()) return;
    if (index == m_CurrentIndex)
        Stop();

    m_Items.erase(m_Items.begin() + index);
    int sz = (int)m_Items.size();
    m_CurrentIndex  = RemapAfterRemove(m_CurrentIndex,  index, sz);
    m_SelectedIndex = RemapAfterRemove(m_SelectedIndex, index, sz);
    Save();
}

void MonitorQueueEngine::Clear()
{
    Stop();
    m_Items.clear();
    m_SelectedIndex = -1;
    Save();
}

void MonitorQueueEngine::ApplyAV()
{
    Core::PresentationCore::Get().SetLiveMute(m_Muted);
    Core::PresentationCore::Get().SetLiveVolume(
        m_Muted ? 0 : static_cast<int>(std::min(m_Volume, 2.0f) * 100.0f));
}

void MonitorQueueEngine::PlayIndex(int index)
{
    auto* p = ActivePlayer();
    if (!p) { Stop(); return; }

    // Salta automaticamente cualquier entrada sin ruta valida, para que
    // la cola jamas quede "pegada" sin avanzar.
    while (index >= 0 && index < (int)m_Items.size())
    {
        std::string path = QueuePath(m_Items[index]);
        if (path.empty())
        {
            std::cerr << "[Queue] Entrada " << index << " sin ruta valida, se omite.\n";
            ++index;
            continue;
        }

        m_CurrentIndex  = index;
        m_SelectedIndex = index;
        m_State         = QueueState::Playing;
        m_PrefetchedForCurrent = false;

        auto& core = Core::PresentationCore::Get();

        // FIX (orden): SetProjecting() es lo que pone m_IsLiveToPublic=true
        // en BackgroundLayer, y SetBackgroundMedia()/SetVideo() SOLO habilita
        // el audio si m_IsLiveToPublic YA es true en el momento en que
        // corre. Antes SetBackgroundMedia() se llamaba primero: para el
        // PRIMER video de una sesion (m_IsLiveToPublic todavia false en ese
        // instante), el clip arrancaba mudo, y aunque SetProjecting()
        // reaplicaba el audio un instante despues, en la practica el
        // operador reportaba tener que mutear/desmutear a mano para que
        // sonara. Invertir el orden hace que el permiso de audio ya este
        // vigente ANTES de cargar el clip, sin depender de una segunda
        // pasada de "auto-corrección".
        core.SetProjecting(true);

        // FIX: la cola marcaba isProjecting=true pero nunca se aseguraba de
        // que el monitor destino estuviera configurado — eso solo pasaba si
        // el operador además prendia a mano el punto "Público" en Vista en
        // Vivo (ViewPanel::ToggleAudience). Ahora la cola se asegura de
        // tener el monitor destino fijado por su cuenta, igual que hace
        // ToggleAudience (que tampoco crea ya una ventana nativa propia —
        // ver el FIX en ViewPanel::ToggleAudience sobre por que se elimino).
        {
            auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
            int monitorCount = 0;
            glfwGetMonitors(&monitorCount);
            int monitorIndex = std::clamp(
                settings.projection.targetMonitor < 0 ? 1 : settings.projection.targetMonitor,
                0, std::max(0, monitorCount - 1));
            core.SetTargetMonitor(monitorIndex);
        }

        // loop = false SIEMPRE. La cola nunca debe pedirle al reproductor
        // que repita un clip a nivel nativo.
        //
        // CommitNextBackgroundMedia (en vez de SetBackgroundMedia directo):
        // si este mismo path ya se venia precargando en standby gracias a
        // PreloadNextIfNeeded() (dandole toda la duracion del item anterior
        // como margen, y quedando PAUSADO en su primer frame apenas listo,
        // ver BackgroundLayer::Update()), esto solo arma el swap sobre lo
        // que ya esta listo — corte instantaneo, sin recomenzar la carga
        // desde cero. Si no hay nada precargado que coincida (salto manual,
        // cola recien arrancada, reordenada), cae sola a una carga en frio.
        core.CommitNextBackgroundMedia(path, /*isVideo=*/true, /*allowAudio=*/true);

        ApplyAV();
        p->SetPause(false);

        std::string displayName = std::filesystem::path(path).filename().string();
        Core::LibrarySelection sel;
        sel.title = displayName;
        sel.type  = Core::ItemType::Video;
        core.SetSelection(sel, /*fromQueue=*/true);

        std::cout << "[Queue] Reproduciendo " << (index + 1) << "/" << m_Items.size()
                  << ": " << path << "\n";
        return;
    }

    // No quedo ningun item valido desde index en adelante: fin de cola.
    Stop();
}

std::string MonitorQueueEngine::FindNextValidPath(int fromIndexInclusive) const
{
    for (int i = fromIndexInclusive; i < (int)m_Items.size(); ++i)
    {
        std::string path = QueuePath(m_Items[i]);
        if (!path.empty()) return path;
    }
    return "";
}

void MonitorQueueEngine::PreloadNextIfNeeded()
{
    if (m_PrefetchedForCurrent) return;

    auto& core = Core::PresentationCore::Get();

    // Espera a que el swap del item ACTUAL ya haya terminado (Standby()
    // queda libre recien ahi) — precargar mientras el propio swap del
    // actual sigue en curso pisaria el player que esta por pasar a Active
    // (ver el guard dentro de BackgroundLayer::Prefetch).
    if (core.IsBackgroundSwapPending()) return;

    std::string nextPath = FindNextValidPath(m_CurrentIndex + 1);
    if (!nextPath.empty())
        core.PreloadNextBackgroundMedia(nextPath, /*allowAudio=*/true);

    m_PrefetchedForCurrent = true;
}

void MonitorQueueEngine::TogglePlayStop()
{
    if (m_State != QueueState::Stopped) { Stop(); return; }
    if (!m_Items.empty()) {
        m_ConsecutiveErrors = 0; // arranque manual: reset del contador de errores
        PlayIndex(0); // arranque directo, sin preflight: el primer item se carga como cualquier otro
    }
}

void MonitorQueueEngine::Stop()
{
    Core::PresentationCore::Get().StopBackgroundMedia();
    m_CurrentIndex = -1;
    m_State        = QueueState::Stopped;
    m_PrefetchedForCurrent = false;
}

static constexpr int kMaxConsecutiveQueueErrors = 3;

void MonitorQueueEngine::Update()
{
    if (m_State != QueueState::Playing || m_CurrentIndex < 0)
        return;

    auto* p = ActivePlayer();
    if (!p) { Stop(); return; }

    // Precarga del siguiente item mientras el actual todavia esta
    // reproduciendose (ver PreloadNextIfNeeded) — le da al siguiente clip
    // toda la duracion restante del actual como margen, en vez de recien
    // empezar a cargar en el instante en que este termina.
    PreloadNextIfNeeded();

    // Unica condicion de avance: el evento REAL de fin de clip que reporta
    // VLC. ConsumeEndReached() solo puede devolver true una vez por clip,
    // asi que este avance ocurre exactamente una vez, de forma instantanea,
    // sin ventana de tiempo en la que algo mas pueda "repetir" el clip.
    if (!p->ConsumeEndReached())
        return;

    // Distingue un error real (codec no soportado, archivo corrupto) de un
    // fin de clip normal: antes ambos se trataban igual y la cola podia
    // saltar en silencio para siempre si todos los items estaban rotos.
    if (p->ConsumeHadError()) {
        ++m_ConsecutiveErrors;
        std::cerr << "[Queue] Error reproduciendo item " << (m_CurrentIndex + 1) << "/"
                  << m_Items.size() << " (codec no soportado o archivo corrupto).\n";

        if (m_ConsecutiveErrors >= kMaxConsecutiveQueueErrors) {
            std::cerr << "[Queue] " << m_ConsecutiveErrors
                      << " errores seguidos: se detiene la cola en vez de seguir saltando.\n";
            Stop();
            return;
        }
    } else {
        m_ConsecutiveErrors = 0;

        // Boton "Loop" de Monitor (ver MonitorCenterColumn.cpp / PresentationCore::
        // GetLiveLoop) -- estaba desconectado del todo: BackgroundLayer::SetVideo
        // fuerza loop=false SIEMPRE para videos reales (allowAudio=true, ver
        // comentario ahi) porque esta cola depende de que ConsumeEndReached()
        // dispare de verdad para avanzar. En vez de loopear a nivel VLC, el loop
        // se logra aca: si esta prendido, se vuelve a reproducir el MISMO indice
        // en vez de avanzar -- un clip real que termino sin error se repite en
        // loop; un clip que dio error nunca se repite (cae al avance normal de
        // abajo, que lo saltea).
        if (Core::PresentationCore::Get().GetLiveLoop()) {
            PlayIndex(m_CurrentIndex);
            return;
        }
    }

    PlayIndex(m_CurrentIndex + 1);
}

} // namespace ProyecThor::UI
