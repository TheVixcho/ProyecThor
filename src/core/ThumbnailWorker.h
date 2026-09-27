#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

namespace ProyecThor::Core {

// ── ThumbnailWorker ───────────────────────────────────────────────────────
// Genera miniaturas de video en un hilo dedicado — decodifica el primer
// frame real via libVLC en modo callback puro (mismo mecanismo que usa
// VLCBasePlayer para el video en vivo), asi que NUNCA abre una ventana
// propia (a diferencia de pedirle a libVLC un snapshot con salida de video
// por defecto). Tambien saca el trabajo lento (abrir+decodificar el
// archivo) del hilo de UI, para que la biblioteca no se trabe generando
// miniaturas — las tarjetas van apareciendo de a poco a medida que cada
// resultado esta listo.
//
// La creacion de texturas GL NO pasa por aca: este worker deja los pixeles
// crudos (RGBA) listos en una cola; quien lo usa (LayersBgTab) tiene que
// llamar DrainResults() una vez por frame DESDE el hilo con contexto GL y
// recien ahi hacer glTexImage2D.
class ThumbnailWorker {
public:
    struct Result {
        std::string          key;       // el mismo "key" pasado a Request(), para mapear de vuelta
        std::vector<uint8_t> pixels;    // RGBA crudo; vacio si no se pudo generar
        unsigned              width  = 0;
        unsigned              height = 0;
    };

    ThumbnailWorker();
    ~ThumbnailWorker();

    ThumbnailWorker(const ThumbnailWorker&)            = delete;
    ThumbnailWorker& operator=(const ThumbnailWorker&) = delete;

    // Encola un pedido (se ignora silenciosamente si "key" ya fue pedido
    // antes, exitoso o no — evita reintentos infinitos por archivo).
    //   key       — identificador para reencontrar el resultado (ver Result::key)
    //   videoPath — path absoluto del video a decodificar
    //   cachePngPath — si no esta vacio, se escribe ahi una copia en disco
    //                  del frame (PNG) para no tener que regenerarlo en la
    //                  proxima sesion de la app.
    void Request(const std::string& key, const std::string& videoPath,
                 const std::string& cachePngPath);

    // Llamar UNA VEZ POR FRAME desde el hilo principal/GL: mueve los
    // resultados listos a "out" (la cola interna queda vacia).
    void DrainResults(std::vector<Result>& out);

    // Cuantos pedidos quedan sin terminar (para mostrar "Generando
    // miniaturas... (N)" en la UI).
    size_t PendingCount() const;

private:
    struct PendingItem {
        std::string key;
        std::string videoPath;
        std::string cachePngPath;
    };

    void ThreadFunc();

    std::thread              m_Thread;
    std::atomic<bool>        m_Running{ false };

    mutable std::mutex        m_QueueMutex;
    std::condition_variable   m_Cv;
    std::deque<PendingItem>   m_Pending;
    std::vector<std::string>  m_Seen;       // keys ya pedidos alguna vez (dedup)
    int                        m_InFlight = 0;

    std::mutex                m_ResultsMutex;
    std::vector<Result>       m_Results;
};

} // namespace ProyecThor::Core
