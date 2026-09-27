#include "OSCReceiver.h"

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketHandle = SOCKET;
    static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    using SocketHandle = int;
    static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace ProyecThor::Core {

OSCReceiver::~OSCReceiver() {
    Stop();
}

bool OSCReceiver::Start(int port, std::string* errorOut) {
    if (m_Listening.load()) {
        if (errorOut) *errorOut = "Ya hay una escucha activa; llama a Stop() primero.";
        return false;
    }

#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        if (errorOut) *errorOut = "No se pudo inicializar Winsock.";
        return false;
    }
#endif

    SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
        if (errorOut) *errorOut = "No se pudo crear el socket UDP.";
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (errorOut) *errorOut = "No se pudo escuchar en el puerto " + std::to_string(port) +
                                   " (¿ya esta en uso?).";
#if defined(_WIN32)
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return false;
    }

    m_Socket        = static_cast<decltype(m_Socket)>(sock);
    m_Port          = port;
    m_StopRequested = false;
    m_Listening     = true;
    m_Thread        = std::thread(&OSCReceiver::ListenLoop, this);
    return true;
}

void OSCReceiver::Stop() {
    if (!m_Listening.load() && !m_Thread.joinable()) return;

    m_StopRequested = true;
    if (m_Thread.joinable()) m_Thread.join();

    if (m_Socket != static_cast<decltype(m_Socket)>(kInvalidSocket)) {
#if defined(_WIN32)
        closesocket(static_cast<SocketHandle>(m_Socket));
        WSACleanup();
#else
        close(static_cast<SocketHandle>(m_Socket));
#endif
    }
    m_Socket    = static_cast<decltype(m_Socket)>(kInvalidSocket);
    m_Listening = false;
}

void OSCReceiver::ListenLoop() {
    SocketHandle sock = static_cast<SocketHandle>(m_Socket);
    char buffer[2048];

    while (!m_StopRequested.load()) {
        // select() con timeout corto: permite revisar m_StopRequested
        // periodicamente en vez de bloquear para siempre en recvfrom().
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 200 * 1000; // 200ms

        int selResult = select(static_cast<int>(sock) + 1, &readSet, nullptr, nullptr, &tv);
        if (selResult <= 0) continue; // timeout o error -> reintenta (revisa stop)

        int received = recvfrom(sock, buffer, static_cast<int>(sizeof(buffer)), 0, nullptr, nullptr);
        if (received <= 0) continue;

        OSCReceivedMessage msg;
        if (ParseOSCPacket(buffer, static_cast<size_t>(received), msg.address, msg.args)) {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_Queue.push_back(std::move(msg));
        }
    }
}

void OSCReceiver::DrainMessages(std::vector<OSCReceivedMessage>& out) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    for (auto& m : m_Queue) out.push_back(std::move(m));
    m_Queue.clear();
}

} // namespace ProyecThor::Core
