#include "OSCSender.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cctype>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

namespace ProyecThor::Core {

// ── Armado del paquete binario ──────────────────────────────────────────────

// Agrega 'value' a 'out' seguido de al menos un byte nulo, rellenando hasta
// el proximo multiplo de 4 (asi lo exige el formato OSC para strings).
static void AppendOSCString(std::vector<char>& out, const std::string& value) {
    out.insert(out.end(), value.begin(), value.end());
    size_t pad = 4 - (value.size() % 4);
    if (pad == 0) pad = 4; // siempre al menos un byte nulo
    for (size_t i = 0; i < pad; i++) out.push_back('\0');
}

static void AppendOSCInt32(std::vector<char>& out, int32_t value) {
    uint32_t v = static_cast<uint32_t>(value);
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 8)  & 0xFF));
    out.push_back(static_cast<char>( v        & 0xFF));
}

static void AppendOSCFloat32(std::vector<char>& out, float value) {
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(value), "float debe ser de 32 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    AppendOSCInt32(out, static_cast<int32_t>(bits));
}

std::vector<char> BuildOSCPacket(const std::string& address, const std::vector<OSCArg>& args) {
    std::vector<char> packet;

    std::string addr = address;
    if (addr.empty() || addr[0] != '/') addr = "/" + addr;
    AppendOSCString(packet, addr);

    std::string typeTags = ",";
    for (const auto& a : args) {
        switch (a.type) {
            case OSCArg::Type::Int:    typeTags += 'i'; break;
            case OSCArg::Type::Float:  typeTags += 'f'; break;
            case OSCArg::Type::String: typeTags += 's'; break;
        }
    }
    AppendOSCString(packet, typeTags);

    for (const auto& a : args) {
        switch (a.type) {
            case OSCArg::Type::Int:    AppendOSCInt32(packet, a.intVal);       break;
            case OSCArg::Type::Float:  AppendOSCFloat32(packet, a.floatVal);   break;
            case OSCArg::Type::String: AppendOSCString(packet, a.strVal);      break;
        }
    }

    return packet;
}

// ── Decodificacion de paquetes recibidos ────────────────────────────────────

// Lee un string OSC (null-terminated, relleno a multiplo de 4) a partir de
// 'pos'. Devuelve false si se pasa del buffer. Avanza 'pos' hasta despues
// del relleno.
static bool ReadOSCString(const char* data, size_t len, size_t& pos, std::string& out) {
    size_t start = pos;
    while (pos < len && data[pos] != '\0') pos++;
    if (pos >= len) return false; // nunca encontro el null terminator
    out.assign(data + start, pos - start);

    pos++; // saltar el null
    size_t consumed = pos - start;
    size_t pad = (4 - (consumed % 4)) % 4;
    pos += pad;
    return pos <= len;
}

static bool ReadOSCInt32(const char* data, size_t len, size_t& pos, int32_t& out) {
    if (pos + 4 > len) return false;
    uint32_t v = (static_cast<uint8_t>(data[pos])     << 24) |
                 (static_cast<uint8_t>(data[pos + 1]) << 16) |
                 (static_cast<uint8_t>(data[pos + 2]) << 8)  |
                  static_cast<uint8_t>(data[pos + 3]);
    out = static_cast<int32_t>(v);
    pos += 4;
    return true;
}

static bool ReadOSCFloat32(const char* data, size_t len, size_t& pos, float& out) {
    int32_t bits;
    if (!ReadOSCInt32(data, len, pos, bits)) return false;
    uint32_t ubits = static_cast<uint32_t>(bits);
    static_assert(sizeof(out) == sizeof(ubits), "float debe ser de 32 bits");
    std::memcpy(&out, &ubits, sizeof(out));
    return true;
}

bool ParseOSCPacket(const char* data, size_t len, std::string& outAddress, std::vector<OSCArg>& outArgs) {
    outArgs.clear();
    if (len == 0 || data[0] != '/') return false; // no es un mensaje simple (ej. "#bundle")

    size_t pos = 0;
    if (!ReadOSCString(data, len, pos, outAddress)) return false;

    std::string typeTags;
    if (!ReadOSCString(data, len, pos, typeTags)) return false;
    if (typeTags.empty() || typeTags[0] != ',') return false;

    for (size_t i = 1; i < typeTags.size(); i++) {
        char tag = typeTags[i];
        if (tag == 'i') {
            int32_t v;
            if (!ReadOSCInt32(data, len, pos, v)) return false;
            outArgs.push_back(OSCArg::MakeInt(v));
        } else if (tag == 'f') {
            float v;
            if (!ReadOSCFloat32(data, len, pos, v)) return false;
            outArgs.push_back(OSCArg::MakeFloat(v));
        } else if (tag == 's') {
            std::string v;
            if (!ReadOSCString(data, len, pos, v)) return false;
            outArgs.push_back(OSCArg::MakeString(v));
        } else if (tag == 'T') {
            // Bool "true" -- OSC 1.0 estandar: el tipo va SOLO en el type
            // tag, sin bytes de dato en el cuerpo del mensaje (a
            // diferencia de i/f/s). TouchOSC y la mayoria de controladores
            // de luces mandan sus botones/toggles asi -- sin esto, cada
            // apriete de un toggle llegaba con args vacios y se ignoraba
            // en silencio (ver OSCPanel::ApplyReceivedMessages).
            outArgs.push_back(OSCArg::MakeInt(1));
        } else if (tag == 'F') {
            outArgs.push_back(OSCArg::MakeInt(0));
        } else if (tag == 'N' || tag == 'I') {
            // Nil / Infinitum -- tampoco llevan bytes de dato, y no hay un
            // valor util que darles acá: se saltean sin agregar arg (no
            // hace falta "break", el resto del mensaje sigue siendo
            // parseable normalmente).
        } else {
            // Tipo no soportado (blob, timetag, array, etc.) -- se
            // descarta el resto del mensaje en vez de fallar todo el
            // parseo: el address y los args ya leidos siguen siendo
            // utiles.
            break;
        }
    }
    return true;
}

// ── Parseo de argumentos escritos a mano ────────────────────────────────────

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// true si 'token' es un entero valido (signo opcional + solo digitos).
static bool LooksLikeInt(const std::string& token) {
    size_t i = (token[0] == '+' || token[0] == '-') ? 1 : 0;
    if (i >= token.size()) return false;
    for (; i < token.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

// true si 'token' parsea como float y usa la notacion tipica (punto/exponente
// o directamente ya fallo LooksLikeInt) -- se apoya en strtof y valida que se
// haya consumido el token entero.
static bool LooksLikeFloat(const std::string& token, float& out) {
    if (token.empty()) return false;
    char* end = nullptr;
    float v = std::strtof(token.c_str(), &end);
    if (end != token.c_str() + token.size()) return false;
    out = v;
    return true;
}

std::vector<OSCArg> ParseOSCArgs(const std::string& argsText) {
    std::vector<OSCArg> result;
    std::string trimmedAll = Trim(argsText);
    if (trimmedAll.empty()) return result;

    size_t pos = 0;
    while (pos <= trimmedAll.size()) {
        size_t comma = trimmedAll.find(',', pos);
        std::string token = Trim(trimmedAll.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));

        if (!token.empty()) {
            if (LooksLikeInt(token)) {
                result.push_back(OSCArg::MakeInt(std::atoi(token.c_str())));
            } else {
                float f;
                if (LooksLikeFloat(token, f)) result.push_back(OSCArg::MakeFloat(f));
                else                          result.push_back(OSCArg::MakeString(token));
            }
        }

        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return result;
}

// ── Envio por UDP ────────────────────────────────────────────────────────────

#if defined(_WIN32)

bool SendOSCMessage(const std::string& host, int port, const std::string& address,
                     const std::vector<OSCArg>& args, std::string* errorOut) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        if (errorOut) *errorOut = "No se pudo inicializar Winsock.";
        return false;
    }

    bool ok = false;
    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    int gaiErr = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (gaiErr != 0 || !result) {
        if (errorOut) *errorOut = "No se pudo resolver la direccion: " + host;
        WSACleanup();
        return false;
    }

    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        if (errorOut) *errorOut = "No se pudo crear el socket UDP.";
        freeaddrinfo(result);
        WSACleanup();
        return false;
    }

    std::vector<char> packet = BuildOSCPacket(address, args);
    int sent = sendto(sock, packet.data(), (int)packet.size(), 0, result->ai_addr, (int)result->ai_addrlen);
    if (sent == SOCKET_ERROR) {
        if (errorOut) *errorOut = "Fallo el envio UDP (codigo " + std::to_string(WSAGetLastError()) + ").";
    } else {
        ok = true;
    }

    closesocket(sock);
    freeaddrinfo(result);
    WSACleanup();
    return ok;
}

#else

bool SendOSCMessage(const std::string& host, int port, const std::string& address,
                     const std::vector<OSCArg>& args, std::string* errorOut) {
    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    int gaiErr = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (gaiErr != 0 || !result) {
        if (errorOut) *errorOut = "No se pudo resolver la direccion: " + host;
        return false;
    }

    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        if (errorOut) *errorOut = "No se pudo crear el socket UDP.";
        freeaddrinfo(result);
        return false;
    }

    std::vector<char> packet = BuildOSCPacket(address, args);
    ssize_t sent = sendto(sock, packet.data(), packet.size(), 0, result->ai_addr, result->ai_addrlen);

    bool ok = (sent >= 0);
    if (!ok && errorOut) *errorOut = "Fallo el envio UDP.";

    close(sock);
    freeaddrinfo(result);
    return ok;
}

#endif

} // namespace ProyecThor::Core
