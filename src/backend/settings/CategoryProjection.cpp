#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "ProjectionQualityPresets.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/FilePicker.h"
#include <imgui.h>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <string>

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

static bool QualityModeButton(const char* id, const char* label, bool active, float width) {
    ImVec4 base   = active ? ImVec4(0.25f, 0.45f, 0.85f, 0.35f) : ImVec4(1,1,1,0.05f);
    ImVec4 hover  = active ? ImVec4(0.25f, 0.45f, 0.85f, 0.45f) : ImVec4(1,1,1,0.10f);
    ImVec4 border = active ? ImVec4(0.35f, 0.55f, 0.95f, 1.0f)  : ImVec4(1,1,1,0.12f);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, hover);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button(label, ImVec2(width, 36.0f));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

static bool ModernToggle(const char* id, bool* value, const float accent[4], const float track[4]) {
    ImGui::PushID(id);

    const float w = 42.0f, h = 22.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##sw", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *value = !*value;

    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(ImGui::GetID("##swT"), *value ? 1.0f : 0.0f);
    *t += ((*value ? 1.0f : 0.0f) - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto Lerp = [](float a, float b, float x) { return a + (b - a) * x; };
    ImVec4 trackCol(
        Lerp(track[0], accent[0], *t), Lerp(track[1], accent[1], *t),
        Lerp(track[2], accent[2], *t), 1.0f);
    if (hovered) { trackCol.x = std::min(1.0f, trackCol.x * 1.08f); trackCol.y = std::min(1.0f, trackCol.y * 1.08f); trackCol.z = std::min(1.0f, trackCol.z * 1.08f); }

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(trackCol), h * 0.5f);

    float thumbR = h * 0.5f - 2.5f;
    float thumbX = p.x + h * 0.5f + (w - h) * (*t);
    float thumbY = p.y + h * 0.5f;
    dl->AddCircleFilled(ImVec2(thumbX, thumbY), thumbR, IM_COL32(255, 255, 255, 255), 16);

    ImGui::PopID();
    return clicked;
}

    void SettingsPanel::RenderCategoryProjection() {
        auto& p   = ProyecThor::Settings::SettingsManager::Get().GetSettings().projection;
        bool  changed = false;

        ImGui::TextDisabled("Ajustes que afectan directamente la ventana proyectada.");
        ImGui::Spacing();

        // ── Monitor de Salida ─────────────────────────────────────────────────
        if (SectionTitle("Monitor de Salida")) {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

            if (monitors && monitorCount > 0) {
                std::vector<std::string> itemLabels;
                std::vector<const char*> names;

                for (int i = 0; i < monitorCount; i++) {
                    std::string label = "[" + std::to_string(i) + "] "
                                      + glfwGetMonitorName(monitors[i]);
                    if (i == 0) label += " (Principal)";
                    itemLabels.push_back(label);
                }
                for (const auto& label : itemLabels) {
                    names.push_back(label.c_str());
                }

                int sel = std::clamp(p.targetMonitor, 0, monitorCount - 1);

                ImGui::SetNextItemWidth(350.0f);
                if (ImGui::Combo("Monitor de Proyeccion##mon", &sel,
                                 names.data(), static_cast<int>(names.size()))) {
                    p.targetMonitor = sel;
                    changed = true;
                }
                HelpTooltip("Elige en que pantalla se mostrara la proyeccion.\n"
                            "Se recomienda usar la pantalla secundaria (indice 1 o superior).");

                // Monitores ADICIONALES (opcional) -- todos muestran
                // exactamente lo mismo que el monitor principal de arriba.
                // Pensado para quien maneja varias pantallas de salida al
                // publico a la vez (ver PresentationState::extraTargetMonitors).
                if (monitorCount > 1) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Enviar tambien a estas pantallas (opcional):");
                    static const float accent[4] = { 0.35f, 0.55f, 0.95f, 1.0f };
                    static const float track[4]  = { 1.0f, 1.0f, 1.0f, 0.10f };

                    for (int i = 0; i < monitorCount; i++) {
                        if (i == sel) continue;

                        bool isExtra = std::find(p.extraMonitors.begin(), p.extraMonitors.end(), i)
                                       != p.extraMonitors.end();
                        bool wasExtra = isExtra;

                        ModernToggle(("##extramon" + std::to_string(i)).c_str(), &isExtra, accent, track);
                        if (isExtra != wasExtra) {
                            if (isExtra) {
                                p.extraMonitors.push_back(i);
                            } else {
                                p.extraMonitors.erase(
                                    std::remove(p.extraMonitors.begin(), p.extraMonitors.end(), i),
                                    p.extraMonitors.end());
                            }
                            changed = true;
                        }

                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%s", names[i]);
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                   "No se detectaron monitores adicionales.");
            }
        }

        ImGui::Spacing();

        // ── Video y Renderizado ───────────────────────────────────────────────
        // Fusiona Calidad de Salida + Motor de Renderizado + FSR Upscaling en
        // una sola entrada de sidebar (navGroup="Video y Renderizado") -- las
        // 3 son, en el fondo, "como se ve/rinde el video de fondo", separarlas
        // en 3 subcategorias sueltas era ruido de navegacion sin necesidad.
        if (SectionTitle("Calidad de Salida (Video de Fondo)", "Video y Renderizado")) {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            int monW = 1920, monH = 1080;
            if (monitors && monitorCount > 0) {
                int idx = std::clamp(p.targetMonitor, 0, monitorCount - 1);
                if (const GLFWvidmode* vm = glfwGetVideoMode(monitors[idx])) {
                    monW = vm->width; monH = vm->height;
                }
            }

            auto mode = static_cast<OutputQualityMode>(p.outputQualityMode);

            float w = ImGui::GetContentRegionAvail().x;
            float btnW = (w - 12.0f) / 3.0f;

            if (QualityModeButton("qmAuto", "Auto", mode == OutputQualityMode::Auto, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Auto; changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("qmPreset", "Preset", mode == OutputQualityMode::Preset, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Preset; changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("qmCustom", "Personalizado", mode == OutputQualityMode::Custom, btnW)) {
                p.outputQualityMode = (int)OutputQualityMode::Custom; changed = true;
            }
            HelpTooltip("Controla a que resolucion se procesa/reescala (FSR) el video de fondo "
                        "antes de mostrarlo. El texto en vivo siempre se ve nitido a resolucion "
                        "nativa, sin importar esta opcion.\n\n"
                        "Auto: la app elige un objetivo razonable segun el monitor.\n"
                        "Preset: estandares fijos de resolucion/fps.\n"
                        "Personalizado: tu eliges ancho, alto y fps.");

            ImGui::Spacing();
            mode = static_cast<OutputQualityMode>(p.outputQualityMode);

            if (mode == OutputQualityMode::Preset) {
                float pw = (w - 6.0f * (kQualityPresetCount - 1)) / kQualityPresetCount;
                for (int i = 0; i < kQualityPresetCount; i++) {
                    if (i > 0) ImGui::SameLine(0.0f, 6.0f);
                    if (QualityModeButton((std::string("qp") + std::to_string(i)).c_str(),
                                          kQualityPresets[i].label, p.outputPresetIndex == i, pw)) {
                        p.outputPresetIndex = i; changed = true;
                    }
                }
            } else if (mode == OutputQualityMode::Custom) {
                float half = (w - 16.0f) * 0.5f;
                ImGui::TextColored(ImVec4(0.7f,0.7f,0.75f,1.0f), "Ancho");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(half - 60.0f);
                int customW = p.outputWidth;
                if (ImGui::InputInt("##qw", &customW, 0, 0)) {
                    p.outputWidth = std::clamp(customW, 320, monW);
                    changed = true;
                }
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::TextColored(ImVec4(0.7f,0.7f,0.75f,1.0f), "Alto");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(half - 60.0f);
                int customH = p.outputHeight;
                if (ImGui::InputInt("##qh", &customH, 0, 0)) {
                    p.outputHeight = std::clamp(customH, 180, monH);
                    changed = true;
                }

                ImGui::SetNextItemWidth(200.0f);
                int fps = p.targetFPS;
                if (ImGui::SliderInt("FPS objetivo", &fps, 15, 60)) {
                    p.targetFPS = fps;
                    changed = true;
                }
            }

            int qW = 0, qH = 0;
            ResolveQualityTarget(mode, p.outputPresetIndex, p.outputWidth, p.outputHeight,
                                 monW, monH, qW, qH);
            ImGui::Spacing();
            ImGui::TextDisabled("Objetivo actual: %dx%d (monitor: %dx%d)", qW, qH, monW, monH);

            if (mode != OutputQualityMode::Auto || qW < monW || qH < monH) {
                bool fsrOn = Core::PresentationCore::Get().GetFSREnabled();
                if (!fsrOn && (qW < monW || qH < monH)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                        "FSR esta desactivado: el video de fondo no se reescalara con nitidez.");
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Motor de Renderizado (Videos)");

            int engine = Core::PresentationCore::Get().GetVideoRenderEngine();
            float w2    = ImGui::GetContentRegionAvail().x;
            float btnW2 = (w2 - 6.0f) * 0.5f;

            if (QualityModeButton("engOpenGL", "OpenGL", engine == 0, btnW2)) {
                p.videoRenderEngine = 0;
                Core::PresentationCore::Get().SetVideoRenderEngine(0);
                changed = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (QualityModeButton("engLibvlc", "libvlc", engine == 1, btnW2)) {
                p.videoRenderEngine = 1;
                Core::PresentationCore::Get().SetVideoRenderEngine(1);
                changed = true;
            }
            HelpTooltip("Solo afecta a VIDEOS reales (Biblioteca > Videos / cola del Monitor "
                        "con audio) -- los Fondos (loops decorativos, imagenes, color solido) "
                        "siempre se muestran por OpenGL, con overlays y texto en vivo encima, "
                        "sin importar esta opcion.\n\n"
                        "OpenGL (default): el video se compone junto con overlays/texto/"
                        "anuncios en la misma salida.\n"
                        "libvlc: el video se muestra en una ventana nativa aparte, con el "
                        "renderer acelerado propio de VLC. Cambiar este ajuste requiere "
                        "reiniciar Audiencia para que tenga efecto.");

            if (engine == 1) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                    "Con libvlc: mientras un video este activo, sin overlays/texto encima "
                    "y sin transicion animada entre clips (corte seco).");
            }

            ImGui::Spacing();
            ImGui::SeparatorText("FSR Upscaling");

            // OJO: la fuente de verdad es p.fsrEnabled/p.fsrSharpness (el mismo
            // ProjectionSettings que usa Ajustes > Diseño > Shaders), no el
            // estado en vivo de PresentationCore directamente -- leer/escribir
            // solo el getter/setter en vivo (como hacía esto antes) dejaba a
            // este control y al de Shaders mostrando/guardando cosas distintas
            // (uno mostraba el valor viejo, y Guardar terminaba pisando el
            // cambio hecho acá con ese valor viejo). Mismo patrón que
            // ShadersPanel::RenderContent().
            if (ImGui::Checkbox("Activar FSR 1.0", &p.fsrEnabled)) {
                Core::PresentationCore::Get().SetFSREnabled(p.fsrEnabled);
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(Mejora calidad de video de baja resolucion)");

            if (p.fsrEnabled) {
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::SliderFloat("Nitidez FSR", &p.fsrSharpness, 0.0f, 2.0f, "%.2f")) {
                    Core::PresentationCore::Get().SetFSRSharpness(p.fsrSharpness);
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("0=Max  2=Suave");
            }
        }

        ImGui::Spacing();

        // ── Marca ─────────────────────────────────────────────────────────────
        // Fusiona Logo + Fondos (las dos cosas que definen la "identidad
        // visual" que se ve al proyectar: el logo de carga y el video/imagen
        // de fondo detras del texto) en una sola entrada de sidebar.
        if (SectionTitle("Logo", "Marca")) {
            std::string display = p.loadingLogoPath.empty()
                ? "(sin logo)"
                : std::filesystem::path(p.loadingLogoPath).filename().string();
            ImGui::TextDisabled("%s", display.c_str());
            HelpTooltip("Imagen que se muestra a la salida real (al publico) mientras un "
                        "fondo o video esta cargando, en vez de dejar ver un frame "
                        "entrecortado o desactualizado. Si no se elige ninguna, la pantalla "
                        "simplemente mantiene el ultimo fondo listo hasta que el nuevo "
                        "termine de cargar (comportamiento de siempre).");

            if (ImGui::Button("Elegir imagen...")) {
                std::string picked = ProyecThor::UI::PickImageFile();
                if (!picked.empty()) {
                    // Se copia a la carpeta de datos de la app (igual que ya
                    // hace Fondos, ver LayersBgTab::ImportBackground) en vez
                    // de guardar la ruta externa tal cual: asi el logo queda
                    // junto con el resto de los assets de ProyecThor y no se
                    // rompe si el archivo original se mueve, se borra, o el
                    // perfil se usa en otra maquina.
                    std::filesystem::path src(picked);
                    std::filesystem::path destDir = ProyecThor::BrandingPath();
                    std::error_code ec;
                    std::filesystem::create_directories(destDir, ec);
                    std::filesystem::path dest = std::filesystem::path(destDir) / src.filename();
                    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        p.loadingLogoPath = dest.string();
                        changed = true;
                    }
                }
            }
            if (!p.loadingLogoPath.empty()) {
                ImGui::SameLine();
                if (ImGui::Button("Quitar##logo")) {
                    p.loadingLogoPath.clear();
                    changed = true;
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Fondos");

            const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

            bool pingPong = p.bgPingPongLoop;
            if (ModernToggle("##bgPingPong", &pingPong, theme.accent, theme.surface3)) {
                p.bgPingPongLoop = pingPong;
                Core::PresentationCore::Get().SetBackgroundPingPongLoop(pingPong);
                changed = true;
            }
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Bucle falso (reproducir y luego en reversa)");
            HelpTooltip("Un Fondo en bucle normal siempre vuelve de golpe al mismo frame "
                        "inicial, lo que se nota como un salto o corte cada vez que repite.\n\n"
                        "Con esto activado, el Fondo reproduce hacia adelante hasta el final "
                        "y despues \"hacia atras\" hasta el principio (en vez de cortar), dando "
                        "sensacion de bucle continuo aunque en realidad nunca deja de ser el "
                        "mismo clip yendo y viniendo.\n\n"
                        "Solo afecta a Fondos (loops decorativos) -- Videos reales de la "
                        "Biblioteca o la cola del Monitor nunca se reproducen en reversa. "
                        "El cambio se aplica al proximo Fondo que se cargue, no al que ya "
                        "esta reproduciendose ahora mismo.");
        }

        ImGui::Spacing();

        // Red/Mobile/Streaming/OSC viven en su propia categoria de nivel
        // superior -- ver Ajustes > Conexiones (CategoryConnections.cpp).

        // Guardar cambios si hubo alguno
        if (changed) {
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

} // namespace ProyecThor::UI::Settings
