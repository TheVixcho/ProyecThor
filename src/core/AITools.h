#pragma once
#include <string>
#include <vector>
#include "ClaudeClient.h"

namespace ProyecThor::Core::AITools {

// ─────────────────────────────────────────────────────────────────────────────
//  AITools — puente entre el modelo (ver ClaudeClient) y la Biblioteca real
//  de canciones. Modo "Avanzada" del Asistente de IA (ver AIAssistantPanel):
//  el operador pone su propia API key de Claude y la IA puede revisar/crear/
//  editar canciones, pero SOLO las herramientas que escriben algo en disco
//  (create_song/edit_song) pasan primero por una confirmacion explicita del
//  operador -- ver NeedsConfirmation. Las de solo lectura (list_songs/
//  read_song) se ejecutan directo, no hay nada que confirmar.
// ─────────────────────────────────────────────────────────────────────────────

std::vector<ClaudeToolDef> GetToolDefinitions();

bool NeedsConfirmation(const std::string& toolName);

// Descripcion corta en español para mostrar en el dialogo de confirmacion
// antes de ejecutar una herramienta que escribe algo (ej. "Editar la letra
// de 'Amazing Grace.txt'").
std::string DescribeCall(const std::string& toolName, const nlohmann::json& input);

// Ejecuta la herramienta (ya confirmada si NeedsConfirmation() era true) y
// devuelve el texto de resultado a reinyectar como tool_result. outIsError
// queda en true si la herramienta fallo (ver MakeToolResultMessage).
std::string Execute(const std::string& toolName, const nlohmann::json& input, bool& outIsError);

} // namespace ProyecThor::Core::AITools
