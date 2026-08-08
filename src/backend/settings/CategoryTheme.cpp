#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <array>
#endif

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

#ifndef _WIN32
// Selector de archivos para Linux/macOS: no hay dialogo nativo unico en
// estos sistemas, asi que se delega en zenity/kdialog (lo que este
// instalado). Mismo enfoque que TabTypography::OpenFontFileDialogUnix
// (reimplementado localmente aca, no se comparte cabecera entre ambos por
// ser un helper chico y de un solo uso en cada archivo).
static std::string OpenFontFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar fuente de la interfaz\" "
        "--file-filter=\"Fuentes | *.ttf *.otf *.ttc\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.ttf *.otf *.ttc|Fuentes\" 2>/dev/null"
    };

    for (const char* cmd : commands) {
        std::array<char, 1024> buffer{};
        std::string result;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;

        while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
            result += buffer.data();

        int status = pclose(pipe);
        if (status != 0) continue; // el usuario cancelo o la herramienta no existe

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        if (!result.empty())
            return result;
    }
    return {};
}
#endif

// Abre el dialogo nativo (Windows) o zenity/kdialog (Linux/macOS) para
// elegir un archivo de fuente. Devuelve la ruta absoluta, o vacio si el
// usuario cancelo / no hay herramienta disponible.
static std::string PickFontFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.hwndOwner           = NULL;
    ofn.lpstrFilter         = "Fuentes\0*.ttf;*.otf;*.ttc\0Todos los archivos\0*.*\0";
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return {};
    return filename;
#else
    return OpenFontFileDialogUnix();
#endif
}

// Resuelve un nombre de fuente (stem, sin extension) a su ruta completa
// dentro de assets/fonts. Misma lógica que
// PresentationCore::ResolveFontFilePath, reimplementada acá porque ese
// método es privado (solo lo usa PresentationCore internamente para
// resolver la fuente activa de las diapositivas).
static std::string ResolveFontPathByName(const std::string& fontName) {
    if (fontName.empty() || fontName == "Predeterminada") return "";

    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
    for (const char* ext : { ".ttf", ".otf", ".ttc" }) {
        std::filesystem::path candidate = fontsDir / (fontName + ext);
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec))
            return candidate.string();
    }
    return "";
}

// Botón cuadrado con el color de acento del preset, usado como swatch.
static bool PresetSwatch(const char* label, ThemePreset preset, ThemePreset active) {
    ThemeSettings preview = MakeThemePreset(preset);
    ImVec4 accent = ImVec4(preview.accent[0], preview.accent[1], preview.accent[2], 1.0f);
    ImVec4 base   = ImVec4(preview.base[0],   preview.base[1],   preview.base[2],   1.0f);

    bool selected = (preset == active);

    ImGui::PushID(label);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x+0.05f, base.y+0.05f, base.z+0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? accent : ImVec4(1,1,1,0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    bool clicked = ImGui::Button("##swatch", ImVec2(52, 52));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetItemRectMin();
    dl->AddCircleFilled(ImVec2(p.x + 38, p.y + 14), 8.0f, ImGui::ColorConvertFloat4ToU32(accent));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    ImGui::PopID();

    return clicked;
}

// Diagramas a mano de cada Entorno de trabajo (ver UIManager::
// BuildWorkspaceLayout*, que arma el DockBuilder real con las mismas
// proporciones) -- se dibujan adentro de la tarjeta de WorkspacePresetCard
// para que el usuario vea la disposicion antes de elegirla, en vez de un
// nombre suelto. "a"/"b" son la esquina superior-izquierda/inferior-derecha
// del area disponible dentro de la tarjeta; accentCol resalta el panel
// "Vista en Vivo" (el que mas cambia de lugar entre presets).
static void DrawWorkspaceDiagramClassic(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    float w = b.x - a.x, h = b.y - a.y, g = 3.0f;
    float leftW = w * 0.22f, rightW = w * 0.28f;
    float midW  = w - leftW - rightW - g * 2.0f;
    float homeH = h * 0.62f;

    dl->AddRectFilled({a.x, a.y}, {a.x + leftW, b.y}, panelCol, 2.0f); // Biblioteca
    dl->AddRectFilled({a.x + leftW + g, a.y}, {a.x + leftW + g + midW, a.y + homeH}, panelCol, 2.0f); // Home
    dl->AddRectFilled({a.x + leftW + g, a.y + homeH + g}, {a.x + leftW + g + midW, b.y}, panelCol, 2.0f); // Diseño
    dl->AddRectFilled({b.x - rightW, a.y}, {b.x, b.y}, accentCol, 2.0f); // Vista en Vivo
}

static void DrawWorkspaceDiagramSimple(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    // Cuatro columnas de alto completo, nada apilado: Biblioteca | Home |
    // Vista en Vivo | Diseño (Diseño se corre TODO a la derecha).
    float w = b.x - a.x, g = 3.0f;
    float leftW = w * 0.22f, vivW = w * 0.32f, rightW = w * 0.16f;
    float midW  = w - leftW - vivW - rightW - g * 3.0f;

    float x = a.x;
    dl->AddRectFilled({x, a.y}, {x + leftW, b.y}, panelCol, 2.0f); x += leftW + g; // Biblioteca
    dl->AddRectFilled({x, a.y}, {x + midW, b.y}, panelCol, 2.0f); x += midW + g; // Home
    dl->AddRectFilled({x, a.y}, {x + vivW, b.y}, accentCol, 2.0f); x += vivW + g; // Vista en Vivo
    dl->AddRectFilled({x, a.y}, {b.x, b.y}, panelCol, 2.0f); // Diseño
}

static void DrawWorkspaceDiagramBroadcast(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    float w = b.x - a.x, h = b.y - a.y, g = 3.0f;
    float topH  = h * 0.42f;
    float botY  = a.y + topH + g;
    float leftW = w * 0.24f, rightW = w * 0.32f;
    float midW  = w - leftW - rightW - g * 2.0f;

    dl->AddRectFilled({a.x, a.y}, {b.x, a.y + topH}, accentCol, 2.0f); // Vista en Vivo (franja superior)
    dl->AddRectFilled({a.x, botY}, {a.x + leftW, b.y}, panelCol, 2.0f); // Biblioteca
    dl->AddRectFilled({a.x + leftW + g, botY}, {a.x + leftW + g + midW, b.y}, panelCol, 2.0f); // Home
    dl->AddRectFilled({b.x - rightW, botY}, {b.x, b.y}, panelCol, 2.0f); // Diseño
}

static void DrawWorkspaceDiagramLibrary(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    // Biblioteca | Home -- sin Vista en Vivo/Diseño (ver
    // UIManager::BuildWorkspaceLayoutLibrary).
    float w = b.x - a.x, g = 3.0f;
    float leftW = w * 0.30f;
    float mainW = w - leftW - g;

    dl->AddRectFilled({a.x, a.y}, {a.x + leftW, b.y}, panelCol, 2.0f); // Biblioteca
    dl->AddRectFilled({a.x + leftW + g, a.y}, {a.x + leftW + g + mainW, b.y}, accentCol, 2.0f); // Home
}

static void DrawWorkspaceDiagramRender(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 /*panelCol*/, ImU32 accentCol) {
    // Biblioteca sola, a pantalla completa (ver UIManager::BuildWorkspaceLayoutRender).
    dl->AddRectFilled(a, b, accentCol, 2.0f);
}

using WorkspaceDiagramFn = void (*)(ImDrawList*, ImVec2, ImVec2, ImU32, ImU32);

// Tarjeta con el diagrama de arriba en vez de un swatch de color -- lo que
// cambia entre presets de Entorno de trabajo es la DISPOSICION de los
// paneles, no una paleta (ver PresetSwatch, mismo patron de seleccion).
static bool WorkspacePresetCard(const char* label, WorkspaceLayoutPreset preset,
                                WorkspaceLayoutPreset active, WorkspaceDiagramFn drawDiagram)
{
    const ImVec4 accent = ImVec4(0.45f, 0.60f, 1.00f, 1.0f);
    const ImVec4 base   = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
    bool selected = (preset == active);

    ImGui::PushID(label);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x + 0.04f, base.y + 0.04f, base.z + 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? accent : ImVec4(1, 1, 1, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button("##wscard", ImVec2(150.0f, 96.0f));

    ImVec2      p0 = ImGui::GetItemRectMin();
    ImVec2      p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 panelCol  = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.10f));
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(selected ? accent : ImVec4(1, 1, 1, 0.30f));
    drawDiagram(dl, { p0.x + 10.0f, p0.y + 10.0f }, { p1.x - 10.0f, p1.y - 10.0f }, panelCol, accentU32);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    ImGui::PopID();

    return clicked;
}

void SettingsPanel::RenderCategoryTheme() {
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    ImGui::TextDisabled("Elige un tema predeterminado o personaliza los colores.");
    ImGui::Spacing();

    if (SectionTitle("Temas predeterminados", "Temas")) {
        struct PresetEntry { const char* label; ThemePreset preset; };
        static const PresetEntry presets[] = {
            { "Oscuro",           ThemePreset::Dark        },
            { "Claro",            ThemePreset::Light       },
            { "Naranja y Negro",  ThemePreset::OrangeBlack },
            { "Jazz",             ThemePreset::Jazz        },
            { "Ko-fi",            ThemePreset::Kofi        },
            { "Verde",            ThemePreset::Deadlock    },
            { "Galaxia",          ThemePreset::Galaxy      },
            { "Mek",              ThemePreset::Mek         },
        };

        int perRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 90.0f));
        for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); i++) {
            if (PresetSwatch(presets[i].label, presets[i].preset, theme.preset)) {
                ProyecThor::Settings::SettingsManager::Get().ApplyPreset(presets[i].preset);
            }
            if ((i + 1) % perRow != 0) ImGui::SameLine();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Preset activo: %s",
            ProyecThor::Settings::ThemePresetName(theme.preset));
    }

    // ── Entorno de trabajo ───────────────────────────────────────────────────
    // Ordenamiento de los 4 paneles dockeados (Biblioteca/Home/Vista en Vivo/
    // Diseño) -- ver UIManager::BuildWorkspaceLayout* para el DockBuilder
    // real de cada uno. Cambiar la seleccion reconstruye el layout solo
    // (UIManager lo detecta comparando contra el ultimo valor aplicado, ver
    // m_LastWorkspacePreset), no hace falta reiniciar ni pedirlo aparte.
    if (SectionTitle("Entorno de trabajo", "Entorno de trabajo")) {
        ImGui::TextDisabled("Elige como se acomodan Biblioteca, Home, Vista en Vivo y Diseño en pantalla.");
        ImGui::Spacing();

        auto& workspace = ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace;

        struct WsEntry { const char* label; WorkspaceLayoutPreset preset; WorkspaceDiagramFn diagram; };
        static const WsEntry entries[] = {
            { "Clásico",     WorkspaceLayoutPreset::Classic,   DrawWorkspaceDiagramClassic   },
            { "Simple",      WorkspaceLayoutPreset::Simple,    DrawWorkspaceDiagramSimple    },
            { "Transmisión", WorkspaceLayoutPreset::Broadcast, DrawWorkspaceDiagramBroadcast },
            { "Biblioteca",  WorkspaceLayoutPreset::Library,   DrawWorkspaceDiagramLibrary   },
            { "Render",      WorkspaceLayoutPreset::Render,    DrawWorkspaceDiagramRender    },
        };

        for (int i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++) {
            if (WorkspacePresetCard(entries[i].label, entries[i].preset, workspace.layoutPreset, entries[i].diagram)) {
                workspace.layoutPreset = entries[i].preset;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }
            if (i < (int)(sizeof(entries) / sizeof(entries[0])) - 1) ImGui::SameLine();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Entorno activo: %s",
            ProyecThor::Settings::WorkspaceLayoutPresetName(workspace.layoutPreset));
    }

    // Compartidas por todos los bloques de "Colores"/"Diseño" de abajo --
    // declaradas afuera de cualquier if(SectionTitle) para que `changed`
    // acumule sin importar cual bloque este visible este frame, y el
    // chequeo final (ver mas abajo) sea incondicional.
    bool changed = false;
    static ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf |
        ImGuiColorEditFlags_NoInputs;

    if (SectionTitle("Personalizar colores", "Colores")) {
        ImGui::TextDisabled("Editar cualquier color aquí lo marca como tema \"Personalizado\".");
        ImGui::Spacing();
    }

    if (SectionTitle("Fondos y superficies", "Colores")) {
        changed |= ImGui::ColorEdit4("Fondo principal##base", theme.base, flags);
        HelpTooltip("Color de ventanas principales.");
        changed |= ImGui::ColorEdit4("Superficie 0##s0", theme.surface0, flags);
        changed |= ImGui::ColorEdit4("Superficie 1##s1", theme.surface1, flags);
        changed |= ImGui::ColorEdit4("Superficie 2##s2", theme.surface2, flags);
        changed |= ImGui::ColorEdit4("Superficie 3##s3", theme.surface3, flags);
    }

    if (SectionTitle("Acento", "Colores")) {
        changed |= ImGui::ColorEdit4("Acento##ac",       theme.accent,      flags);
        changed |= ImGui::ColorEdit4("Acento claro##acl",theme.accentLight, flags);
        changed |= ImGui::ColorEdit4("Acento oscuro##acd",theme.accentDim,  flags);
        changed |= ImGui::ColorEdit4("Acento tenue##acf",theme.accentFaint, flags);
    }

    if (SectionTitle("Bordes", "Colores")) {
        changed |= ImGui::ColorEdit4("Borde##bd",        theme.border,      flags);
        changed |= ImGui::ColorEdit4("Borde tenue##bdf",  theme.borderFaint, flags);
    }

    if (SectionTitle("Texto", "Colores")) {
        changed |= ImGui::ColorEdit4("Texto principal##tp",   theme.textPrimary, flags);
        changed |= ImGui::ColorEdit4("Texto secundario##td",  theme.textDim,     flags);
        changed |= ImGui::ColorEdit4("Texto inactivo##tf",    theme.textFaint,   flags);
    }

    if (SectionTitle("Estados", "Colores")) {
        changed |= ImGui::ColorEdit4("Error##dg",   theme.danger,  flags);
        changed |= ImGui::ColorEdit4("Éxito##sc",   theme.success, flags);
    }

    if (SectionTitle("Forma", "Diseño")) {
        changed |= ImGui::SliderFloat("Redondeo de ventanas", &theme.windowRounding, 0.0f, 24.0f, "%.0f");
        changed |= ImGui::SliderFloat("Redondeo de controles", &theme.frameRounding, 0.0f, 16.0f, "%.0f");
        changed |= ImGui::SliderFloat("Grosor de scrollbar",   &theme.scrollbarSize, 4.0f, 16.0f, "%.0f");
    }

    if (changed) {
        theme.preset = ThemePreset::Custom;
        ProyecThor::Settings::SettingsManager::Get().ApplyTheme(); // preview en vivo
    }

    // ── Fuente de la interfaz ────────────────────────────────────────────────
    // Mismo selector que "Edición de estilo" (ver TabTypography::
    // RenderFontSelector): combo con las fuentes que ya están en
    // assets/fonts, + un botón para importar una nueva. Nada de elegir un
    // .ttf suelto del disco cada vez -- se elige de la misma lista/carpeta
    // que usa el resto de la app. El cambio se valida (IsValidFontFile) y
    // se aplica la próxima vez que se abra ProyecThor -- ver main.cpp.
    if (SectionTitle("Fuente de la interfaz", "Fuentes")) {
        ImGui::TextDisabled("Cambia la tipografía de toda la app.");
        ImGui::Spacing();

        std::vector<std::string> fontList;
        ProyecThor::Core::PresentationCore::Get().SyncFontListFromDisk(fontList);

        std::string currentFontName = theme.customFontPath.empty()
            ? "Predeterminada"
            : std::filesystem::path(theme.customFontPath).stem().string();

        const float importBtnW = 90.0f;
        const float comboGap    = 6.0f;
        float comboW = ImGui::GetContentRegionAvail().x - importBtnW - comboGap;

        ImGui::SetNextItemWidth(comboW);
        if (ImGui::BeginCombo("##uiFont", currentFontName.c_str())) {
            for (const auto& name : fontList) {
                bool sel = (name == currentFontName);
                if (ImGui::Selectable(name.c_str(), sel)) {
                    theme.customFontPath = ResolveFontPathByName(name);
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(0.0f, comboGap);
        if (ImGui::Button("+ Fuente", ImVec2(importBtnW, 0.0f))) {
            std::string picked = PickFontFileDialog();
            if (!picked.empty()) {
                try {
                    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
                    std::filesystem::create_directories(fontsDir);

                    std::filesystem::path src(picked);
                    std::filesystem::path dst = fontsDir / src.filename();
                    std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);

                    theme.customFontPath = dst.string();
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                } catch (const std::exception&) {
                    // Import fallido (permisos, disco, etc.): se deja la
                    // selección de fuente tal como estaba.
                }
            }
        }

        if (!theme.customFontPath.empty() && !IsValidFontFile(theme.customFontPath)) {
            ImGui::TextColored(ImVec4(0.93f, 0.35f, 0.35f, 1.0f),
                "\"%s\" no se pudo leer -- se usará la predeterminada.", currentFontName.c_str());
        }
    }

    // La fuente de la interfaz cambia el atlas de ImGui completo (y el de
    // la pantalla de carga) -- eso no se puede "reemplazar en caliente" de
    // forma segura mientras la app esta corriendo con VLC/GL en varias
    // ventanas a la vez, asi que en vez de aplicarla en silencio recien en
    // el proximo arranque, se ofrece reiniciar ya mismo. Fuera del
    // if(SectionTitle) a proposito: el modal tiene que seguir pudiendo
    // dibujarse aunque el usuario cambie de subcategoria mientras esta
    // abierto.
    if (m_ShowFontRestartPrompt) {
        ImGui::OpenPopup("Reiniciar para aplicar la fuente");
        m_ShowFontRestartPrompt = false;
    }
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Reiniciar para aplicar la fuente", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("La nueva fuente de la interfaz se aplica reiniciando ProyecThor. ¿Reiniciar ahora?");
        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        const float btnW = 150.0f;
        if (ImGui::Button("Reiniciar ahora", ImVec2(btnW, 34.0f))) {
            ProyecThor::Settings::SettingsManager::Get().RequestRestart();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, 10.0f);
        if (ImGui::Button("Más tarde", ImVec2(btnW, 34.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Colores de categorías (sidebar de Biblioteca) ───────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada categoría en el sidebar izquierdo de la Biblioteca (Letra/Video/
    // Imagen/Biblia/Documentos/Audio). Ver LibrarySidebar.cpp.
    if (SectionTitle("Colores de categorías (Biblioteca)", "Colores")) {
        ImGui::TextDisabled("Color de identidad de cada categoría en el sidebar de la Biblioteca.");
        ImGui::Spacing();

        auto& sidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;
        static const char* kCatLabels[6] = { "Letra", "Video", "Imagen", "Biblia", "Documentos", "Audio" };
        bool sidebarChanged = false;
        for (int i = 0; i < 6; i++) {
            std::string id = std::string(kCatLabels[i]) + "##libcat" + std::to_string(i);
            sidebarChanged |= ImGui::ColorEdit4(id.c_str(), sidebar.categoryColor[i], flags);
        }
        if (sidebarChanged) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    // ── Colores de categorías (sidebar de Home) ─────────────────────────────
    // Independiente del tema general: solo afecta el color de identidad de
    // cada sección en el sidebar de Home (Home/Reloj/Anuncios/Notas/Captura/
    // Transmisión). Ver HomeSidebar.cpp.
    if (SectionTitle("Colores de categorías (Home)", "Colores")) {
        ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Home.");
        ImGui::Spacing();

        auto& homeSidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().homeSidebar;
        static const char* kHomeCatLabels[6] = {
            "Home", "Contadores", "Anuncios", "Notas Rápidas", "Captura", "Transmisión en Red"
        };
        bool homeSidebarChanged = false;
        for (int i = 0; i < 6; i++) {
            std::string id = std::string(kHomeCatLabels[i]) + "##homecat" + std::to_string(i);
            homeSidebarChanged |= ImGui::ColorEdit4(id.c_str(), homeSidebar.categoryColor[i], flags);
        }
        if (homeSidebarChanged) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    // ── Colores de categorías (hub de Control) ──────────────────────────────
    if (SectionTitle("Colores de categorías (Control)", "Colores")) {
        ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Control.");
        ImGui::Spacing();

        auto& controlHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().controlHub;
        static const char* kControlCatLabels[2] = { "Control", "Stage Display" };
        bool controlHubChanged = false;
        for (int i = 0; i < 2; i++) {
            std::string id = std::string(kControlCatLabels[i]) + "##controlcat" + std::to_string(i);
            controlHubChanged |= ImGui::ColorEdit4(id.c_str(), controlHub.categoryColor[i], flags);
        }
        if (controlHubChanged) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    // ── Colores de categorías (hub de Diseño) ───────────────────────────────
    if (SectionTitle("Colores de categorías (Diseño)", "Colores")) {
        ImGui::TextDisabled("Color de identidad de cada sección en el sidebar de Diseño.");
        ImGui::Spacing();

        auto& stylesHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
        static const char* kStylesCatLabels[3] = { "Fondos", "Estilos", "Transiciones" };
        bool stylesHubChanged = false;
        for (int i = 0; i < 3; i++) {
            std::string id = std::string(kStylesCatLabels[i]) + "##stylescat" + std::to_string(i);
            stylesHubChanged |= ImGui::ColorEdit4(id.c_str(), stylesHub.categoryColor[i], flags);
        }
        if (stylesHubChanged) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }
}

} // namespace ProyecThor::UI::Settings