#pragma once
#include <string>
#include <vector>
#include <fstream>
#include "MonitorQueueHelpers.h"

// =============================================================================
//  MonitorQueueIO.h
//  Funciones de carga y guardado de la cola en disco.
//  Separadas para que MonitorView.cpp no incluya logica de IO directamente.
// =============================================================================

namespace ProyecThor::UI::QueueIO {

using namespace QueueHelpers;

// Carga las entradas validas del archivo de persistencia en el vector dado.
// Entradas invalidas (formato incorrecto) son ignoradas silenciosamente.
inline void LoadQueue(std::vector<std::string>& outQueue)
{
    outQueue.clear();
    std::ifstream f(GetQueueFilePath());
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (line.size() > 2 &&
            (line[0] == k_PfxLocal || line[0] == k_PfxURL) &&
            line[1] == '|')
        {
            outQueue.push_back(line);
        }
    }
}

// Guarda el contenido actual del vector en el archivo de persistencia.
inline void SaveQueue(const std::vector<std::string>& queue)
{
    std::ofstream f(GetQueueFilePath());
    if (!f.is_open()) return;
    for (const auto& e : queue)
        f << e << '\n';
}

} // namespace ProyecThor::UI::QueueIO
