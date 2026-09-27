#pragma once
#include "OSCSender.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ProyecThor::Core {

// Un mensaje OSC ya decodificado, con la marca de tiempo en la que se
// encolo (para descartar mensajes viejos si hiciera falta en el futuro).
struct OSCReceivedMessage {
    std::string          address;
    std::vector<OSCArg>  args;
};

// ─────────────────────────────────────────────────────────────────────────────
//  OSCReceiver — escucha UDP en un hilo de fondo y decodifica los mensajes
//  entrantes (via OSCSender::ParseOSCPacket). Pensado para el panel
//  Yggdrasil: un controlador externo (TouchOSC, un fader fisico, etc.) le
//  manda mensajes a ProyecThor para controlar parametros en vivo.
//
//  Los mensajes NO se aplican desde el hilo de red (ImGui/PresentationCore
//  no son thread-safe): se encolan en m_Queue y el hilo principal los saca
//  llamando a DrainMessages() una vez por frame (mismo patron que
//  StreamingPanel::Update()).
// ─────────────────────────────────────────────────────────────────────────────
class OSCReceiver {
public:
    ~OSCReceiver();

    // Abre el socket UDP en 'port' y arranca el hilo de escucha. Devuelve
    // false (y llena errorOut) si el puerto ya esta en uso o no se pudo
    // crear el socket. Llamar a Stop() primero si ya estaba escuchando en
    // otro puerto.
    bool Start(int port, std::string* errorOut = nullptr);
    void Stop();
    bool IsListening() const { return m_Listening.load(); }
    int  GetPort() const     { return m_Port; }

    // Saca todos los mensajes acumulados desde la ultima llamada y los
    // agrega a 'out'. Llamar una vez por frame desde el hilo principal.
    void DrainMessages(std::vector<OSCReceivedMessage>& out);

private:
    void ListenLoop();

    std::thread       m_Thread;
    std::atomic<bool> m_Listening{ false };
    std::atomic<bool> m_StopRequested{ false };
    int               m_Port = 0;

#if defined(_WIN32)
    uintptr_t m_Socket = 0; // SOCKET, evita incluir winsock2.h en el header
#else
    int       m_Socket = -1;
#endif

    std::mutex                         m_QueueMutex;
    std::vector<OSCReceivedMessage>    m_Queue;
};

} // namespace ProyecThor::Core
