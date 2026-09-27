#pragma once
#include <string>
#include <vector>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  OSCSender — envio minimo de mensajes OSC (Open Sound Control) por UDP.
//  Pensado para el panel Yggdrasil (control de luces/dispositivos externos
//  que hablan OSC: QLC+, grandMA, Resolume, TouchOSC, etc.) — ProyecThor
//  actua como cliente/emisor, no como servidor (no escucha mensajes
//  entrantes). Implementacion propia porque el mensaje OSC es simple
//  (direccion + tipos + argumentos, todo alineado a 4 bytes) y no vale la
//  pena sumar una dependencia externa solo para esto.
// ─────────────────────────────────────────────────────────────────────────────

struct OSCArg {
    enum class Type { Int, Float, String };
    Type        type = Type::Float;
    int         intVal = 0;
    float       floatVal = 0.0f;
    std::string strVal;

    static OSCArg MakeInt(int v)                { OSCArg a; a.type = Type::Int;    a.intVal   = v; return a; }
    static OSCArg MakeFloat(float v)             { OSCArg a; a.type = Type::Float;  a.floatVal = v; return a; }
    static OSCArg MakeString(const std::string& v) { OSCArg a; a.type = Type::String; a.strVal = v; return a; }
};

// Arma el paquete binario de un mensaje OSC (direccion + tipos + args), sin
// enviarlo -- separado de Send() para poder testear/inspeccionar el armado.
std::vector<char> BuildOSCPacket(const std::string& address, const std::vector<OSCArg>& args);

// Parsea una lista de argumentos escritos a mano separados por coma (ej.
// "1, 0.5, hola") infiriendo el tipo de cada uno: entero si son solo
// digitos (con signo opcional), float si ademas tiene punto/exponente, y
// string en cualquier otro caso. Espacios alrededor de cada valor se
// recortan. Una entrada vacia produce una lista vacia (mensaje sin args).
std::vector<OSCArg> ParseOSCArgs(const std::string& argsText);

// Envia un mensaje OSC por UDP a host:port. Devuelve false y llena
// errorOut (si no es null) si fallo resolver el host o el envio.
bool SendOSCMessage(const std::string& host, int port, const std::string& address,
                     const std::vector<OSCArg>& args, std::string* errorOut = nullptr);

// Decodifica un paquete OSC recibido (direccion + tipos + args) -- inverso
// de BuildOSCPacket. Usado por OSCReceiver al llegar un datagrama UDP.
// Devuelve false si el paquete esta mal formado (usado por OSCReceiver
// para descartarlo en silencio). Solo entiende mensajes simples (no
// bundles "#bundle") -- suficiente para faders/controladores tipo TouchOSC.
bool ParseOSCPacket(const char* data, size_t len,
                     std::string& outAddress, std::vector<OSCArg>& outArgs);

} // namespace ProyecThor::Core
