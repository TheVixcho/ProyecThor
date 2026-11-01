#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "core/SystemStats.h"
#include "core/MediaImportManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include "ui/panels/biblio/LibrarySongMeta.h"
#include <imgui.h>
#include <cmath>
#include <string>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI::Settings {

void SettingsPanel::RenderCategoryPerformance() {
    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    auto& perf = mgr.GetSettings().performance;

    if (SectionTitle("Modo Rendimiento", "Rendimiento")) {
        // ── Tarjeta 1: Switch Maestro "Modo Rendimiento" ───────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  perf.performanceMode
            ? ImVec4(0.20f, 0.80f, 0.55f, 0.40f)
            : ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

        float masterCardH = 148.0f;
        if (ImGui::BeginChild("##perfMasterCard", ImVec2(0.0f, masterCardH), true)) {
            bool perfMode = perf.performanceMode;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));
            if (ImGui::Checkbox("##togglePerfMode", &perfMode)) {
                perf.performanceMode = perfMode;
                if (perfMode) {
                    perf.disableGlassBlur = true;
                    perf.pauseBackgroundScan = true;
                    if (perf.targetFPS <= 0) perf.targetFPS = 60;
                }
                mgr.SaveSettings();
            }
            ImGui::PopStyleVar();

            ImGui::SameLine(0.0f, 10.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::TextColored(perf.performanceMode
                ? ImVec4(0.25f, 0.90f, 0.60f, 1.0f)
                : ImGui::ColorConvertU32ToFloat4(DS::TextPrimary),
                "Modo Rendimiento (Bajo Consumo)");
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            ImGui::TextWrapped("Diseñado para equipos con procesadores modestos o de gama de entrada "
                               "(Intel Pentium Gold, Celeron, Core 2 Duo, 4-8 GB de RAM y gráficas integradas Intel HD/UHD). "
                               "Optimiza la interfaz, detiene tareas en segundo plano no esenciales y evita el congelamiento de pantalla.");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.65f, 1.0f),
                perf.performanceMode ? "✔ MODO RENDIMIENTO ACTIVO: Reducción máxima de carga de CPU y GPU."
                                     : "○ Modo estándar: Todos los efectos visuales y escaneos de disco activos.");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // ── Tarjeta 2: Gráficos y Renderizado ──────────────────────────────────
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Gráficos y Renderizado");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

        if (ImGui::BeginChild("##perfGpuCard", ImVec2(0.0f, 175.0f), true)) {
            bool glassBlur = perf.disableGlassBlur;
            if (ImGui::Checkbox("Desactivar desenfoque de pantalla en GPU (Glass Blur)", &glassBlur)) {
                perf.disableGlassBlur = glassBlur;
                mgr.SaveSettings();
            }
            HelpTooltip("Elimina la pasada de captura del framebuffer y shaders de desenfoque gaussiano continuo. Recomendado para Intel UHD 610/620.");

            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            bool lowMem = perf.lowMemoryMode;
            if (ImGui::Checkbox("Modo de baja memoria (Búferes reducidos)", &lowMem)) {
                perf.lowMemoryMode = lowMem;
                mgr.SaveSettings();
            }
            HelpTooltip("Limita la memoria RAM utilizada por cachés y búferes intermedios. Ideal para computadoras con 4 u 8 GB de RAM.");

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // Selector de límite de fotogramas (FPS)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Límite de fotogramas por segundo (FPS):");
            ImGui::SetNextItemWidth(260.0f);

            int currentFps = perf.targetFPS;
            const char* fpsLabels[] = {
                "Sin límite (o según monitor)",
                "30 FPS (Ahorro máximo para Pentium)",
                "60 FPS (Equilibrado y fluido)"
            };
            int fpsValues[] = { 0, 30, 60 };
            int currentIdx = 0;
            if (currentFps == 30) currentIdx = 1;
            else if (currentFps == 60) currentIdx = 2;

            if (ImGui::Combo("##targetFpsCombo", &currentIdx, fpsLabels, 3)) {
                perf.targetFPS = fpsValues[currentIdx];
                mgr.SaveSettings();
            }
            HelpTooltip("Limitar a 60 o 30 FPS cede tiempo de CPU a la reproducción de video y evita que el hilo principal consuma el 100% de la CPU.");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // ── Tarjeta 3: Subprocesos y Disco ─────────────────────────────────────
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Subprocesos y Acceso a Disco");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

        static std::string s_CacheFeedback = "";
        static float s_CacheFeedbackTimer = 0.0f;

        if (ImGui::BeginChild("##perfThreadsCard", ImVec2(0.0f, 140.0f), true)) {
            bool pauseScan = perf.pauseBackgroundScan;
            if (ImGui::Checkbox("Pausar escaneo automático de carpetas y descargas", &pauseScan)) {
                perf.pauseBackgroundScan = pauseScan;
                mgr.SaveSettings();
            }
            HelpTooltip("Evita crear hilos secundarios periódicos cada 3 segundos que leen el disco en segundo plano. Puedes buscar archivos nuevos manualmente cuando quieras en Ajustes > Datos.");

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            if (DS::GlassButton("Limpiar caché de canciones y memoria RAM", ImVec2(310.0f, 32.0f))) {
                ProyecThor::Library::ClearSongIndexCache();
                s_CacheFeedback = "✔ Caché de canciones, autores y metadatos liberada de la memoria RAM.";
                s_CacheFeedbackTimer = 5.0f;
            }
            HelpTooltip("Vacía la caché en memoria para refrescarla limpiamente desde los archivos en disco.");

            if (!s_CacheFeedback.empty()) {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.65f, 1.0f), "%s", s_CacheFeedback.c_str());
                s_CacheFeedbackTimer -= ImGui::GetIO().DeltaTime;
                if (s_CacheFeedbackTimer <= 0.0f) s_CacheFeedback.clear();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // ── Tarjeta 4: Diagnóstico del Sistema en Tiempo Real ──────────────────
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Diagnóstico del Sistema");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

        if (ImGui::BeginChild("##perfDiagCard", ImVec2(0.0f, 185.0f), true)) {
            auto& sys = ProyecThor::Core::SystemStats::Get();
            float cpu = sys.CpuPercent();
            float ramUsed = sys.RamUsedMB();
            float ramTotal = sys.RamTotalMB();
            float fps = ImGui::GetIO().Framerate;
            const std::string& gpu = sys.GpuName();

            // Fila GPU y FPS
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Gráfica activa:");
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "%s", gpu.empty() ? "Detectando..." : gpu.c_str());

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 140.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "FPS:");
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(fps < 30.0f ? ImVec4(1.0f, 0.40f, 0.40f, 1.0f) : ImVec4(0.40f, 0.90f, 0.60f, 1.0f), "%.1f", fps);

            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            // Barra CPU
            float cpuFraction = std::clamp(cpu / 100.0f, 0.0f, 1.0f);
            ImVec4 cpuColor = (cpu > 80.0f) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                            : (cpu > 50.0f) ? ImVec4(0.95f, 0.75f, 0.25f, 1.0f)
                                            : ImVec4(0.35f, 0.75f, 0.95f, 1.0f);
            ImGui::Text("Uso de CPU del equipo:  %.1f %%", cpu);
            AnimatedProgressBar(cpuFraction, ImVec2(-1.0f, 8.0f), cpuColor);

            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            // Barra RAM
            float ramFraction = (ramTotal > 0.0f) ? std::clamp(ramUsed / ramTotal, 0.0f, 1.0f) : 0.0f;
            ImVec4 ramColor = (ramFraction > 0.85f) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) : ImVec4(0.40f, 0.80f, 0.55f, 1.0f);
            ImGui::Text("Memoria RAM:  %.0f MB / %.0f MB  (%.0f %%)", ramUsed, ramTotal, ramFraction * 100.0f);
            AnimatedProgressBar(ramFraction, ImVec2(-1.0f, 8.0f), ramColor);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}

} // namespace ProyecThor::UI::Settings
