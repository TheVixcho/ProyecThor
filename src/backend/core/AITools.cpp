#include "AITools.h"
#include "backend/core/AppPaths.h"
#include "frontend/panels/biblio/LibrarySongs.h"
#include "frontend/panels/biblio/LibraryHelpers.h"
#include <filesystem>

namespace ProyecThor::Core::AITools {

using json = nlohmann::json;
namespace fs = std::filesystem;

std::vector<ClaudeToolDef> GetToolDefinitions()
{
    std::vector<ClaudeToolDef> tools;

    tools.push_back({
        "list_songs",
        "Lista las canciones disponibles en la Biblioteca de ProyecThor: nombre de archivo exacto y titulo mostrado.",
        json{ { "type", "object" }, { "properties", json::object() } }
    });

    tools.push_back({
        "read_song",
        "Lee la letra completa de una cancion existente, dado su nombre de archivo exacto (ver list_songs).",
        json{
            { "type", "object" },
            { "properties", { { "filename", { { "type", "string" }, { "description", "Nombre de archivo exacto, ej. 'Amazing Grace.txt'" } } } } },
            { "required", json::array({ "filename" }) }
        }
    });

    tools.push_back({
        "create_song",
        "Crea una cancion NUEVA en la Biblioteca con un titulo y una letra. Cada estrofa va separada por una linea en blanco.",
        json{
            { "type", "object" },
            { "properties", {
                { "title",  { { "type", "string" } } },
                { "lyrics", { { "type", "string" }, { "description", "Letra completa, estrofas separadas por una linea en blanco" } } }
            } },
            { "required", json::array({ "title", "lyrics" }) }
        }
    });

    tools.push_back({
        "edit_song",
        "Reemplaza COMPLETO el contenido de una cancion YA EXISTENTE (ver list_songs para el nombre de archivo exacto). Sobreescribe toda la letra anterior -- si el pedido es agregar/cambiar solo una parte, primero hay que leer la cancion con read_song y mandar el texto completo modificado, no solo el fragmento.",
        json{
            { "type", "object" },
            { "properties", {
                { "filename", { { "type", "string" } } },
                { "lyrics",   { { "type", "string" } } }
            } },
            { "required", json::array({ "filename", "lyrics" }) }
        }
    });

    return tools;
}

bool NeedsConfirmation(const std::string& toolName)
{
    return toolName == "create_song" || toolName == "edit_song";
}

std::string DescribeCall(const std::string& toolName, const json& input)
{
    if (toolName == "create_song")
        return "Crear cancion nueva: \"" + input.value("title", "") + "\"";
    if (toolName == "edit_song")
        return "Reemplazar la letra de: " + input.value("filename", "");
    return toolName;
}

std::string Execute(const std::string& toolName, const json& input, bool& outIsError)
{
    outIsError = false;
    try
    {
        if (toolName == "list_songs")
        {
            std::string songsDir = ProyecThor::GetAssetsPath() + "/songs";
            json arr = json::array();
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(ProyecThor::Library::U8Path(songsDir), ec))
            {
                if (ec || !entry.is_regular_file()) continue;
                if (entry.path().extension() != ".txt") continue;
                std::string filename = ProyecThor::Library::PathToUtf8(entry.path().filename());
                arr.push_back({ { "filename", filename }, { "title", ProyecThor::Library::GetSongDisplayName(filename) } });
            }
            return arr.dump();
        }

        if (toolName == "read_song")
        {
            std::string filename = input.value("filename", "");
            if (filename.empty()) { outIsError = true; return "Falta 'filename'."; }

            auto verses = ProyecThor::Library::LoadSongVerses(filename);
            std::string joined;
            for (const auto& v : verses) { joined += v; joined += "\n\n"; }
            return joined.empty() ? "(la cancion esta vacia o no se encontro)" : joined;
        }

        if (toolName == "create_song")
        {
            std::string title  = input.value("title", "Cancion nueva");
            std::string lyrics = input.value("lyrics", "");
            ProyecThor::Library::CreateNewSongFromText(title, lyrics);
            return "Cancion creada.";
        }

        if (toolName == "edit_song")
        {
            std::string filename = input.value("filename", "");
            std::string lyrics   = input.value("lyrics", "");
            if (filename.empty()) { outIsError = true; return "Falta 'filename'."; }

            bool ok = ProyecThor::Library::SetSongText(filename, lyrics);
            if (!ok) { outIsError = true; return "No se encontro la cancion '" + filename + "'."; }
            return "Cancion actualizada.";
        }
    }
    catch (const std::exception& e)
    {
        outIsError = true;
        return std::string("Error ejecutando la herramienta: ") + e.what();
    }

    outIsError = true;
    return "Herramienta desconocida: " + toolName;
}

} // namespace ProyecThor::Core::AITools
