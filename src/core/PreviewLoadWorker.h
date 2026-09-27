#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ProyecThor::Core {

class VLCBasePlayer;

// ── PreviewLoadWorker ────────────────────────────────────────────────────────
// Ejecuta acciones arbitrarias, en orden, en un hilo dedicado — pensado para
// sacar del hilo principal cualquier llamada SINCRONICA a libVLC que pueda
// tardar (Play()/Stop(), que bloquean mientras VLC abre/cierra el archivo).
//
// Critico para el motor "libvlc" (ventana nativa, ver
// BackgroundLayer::m_NativeLoader): el hilo principal es el mismo que
// bombea los mensajes de esa ventana. Si Play()/Stop()/AttachNativeWindow/
// DetachNativeWindow corren ahi directo, el modulo de video de VLC puede
// quedar esperando ese bombeo mientras el hilo principal esta bloqueado
// DENTRO de esa misma llamada — deadlock. Y si esas llamadas se reparten
// entre el hilo principal y este worker sin mas cuidado, aparece una
// carrera distinta: dos hilos tocando el mismo libvlc_media_player_t al
// mismo tiempo (confirmado en la practica: Wine reportaba dos hilos
// bloqueados entre si en una critical section). La regla es entonces
// TODA operacion sobre ese reproductor (attach/detach de ventana,
// Play()/Stop()) tiene que pasar por ESTE MISMO worker, nunca una mezcla
// de hilo principal + worker — por eso Request() acepta una accion
// arbitraria en vez de exponer Play/Stop por separado: quien lo usa arma
// UNA sola accion combinada (ej. "adjuntar ventana + reproducir") en vez
// de encolar dos pedidos sueltos, que la politica de abajo podria
// intercalar o descartar en el orden equivocado.
//
// Originalmente pensado solo para el reproductor de Preview (una carga
// lenta ahi no debia trabar el video en vivo, instancia de VLC
// completamente separada) — el nombre quedo de ahi, pero es generico.
//
// Politica "ultimo pedido gana" (mismo patron que FrameEncodeWorker): si
// llega un pedido nuevo mientras el anterior todavia se esta procesando, el
// anterior se descarta apenas el que esta en curso termina — no se acumula
// una cola de pedidos viejos. Por eso cada pedido debe ser una accion
// autocontenida: no asumir que el pedido anterior llego a correr.
class PreviewLoadWorker {
public:
    PreviewLoadWorker();
    ~PreviewLoadWorker();

    PreviewLoadWorker(const PreviewLoadWorker&)            = delete;
    PreviewLoadWorker& operator=(const PreviewLoadWorker&) = delete;

    // Encola una accion para correr en el hilo del worker.
    void Request(std::function<void()> action);

    // Conveniencia para el caso simple (Preview: un solo Play()/Stop(),
    // sin nada mas que combinar). player debe seguir siendo valido
    // mientras exista este worker.
    void RequestLoad(VLCBasePlayer* player, const std::string& path, bool loop, bool startMuted);
    void RequestStop(VLCBasePlayer* player);
    void RequestStopSync(VLCBasePlayer* player, int timeoutMs = 1000);
    void Flush(int timeoutMs = 1000);

private:
    void ThreadFunc();

    std::thread                          m_Thread;
    std::atomic<bool>                    m_Running{ false };
    std::mutex                           m_Mutex;
    std::condition_variable              m_Cv;
    std::optional<std::function<void()>> m_Pending; // protegido por m_Mutex
};

} // namespace ProyecThor::Core
