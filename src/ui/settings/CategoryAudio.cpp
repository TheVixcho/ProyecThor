#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "../../PresentationCore.h"
#include "../../VLCBasePlayer.h"
#include <imgui.h>
#include <vector>
#include <string>

namespace ProyecThor::UI::Settings {

    void SettingsPanel::RenderCategoryAudio() {
        auto& a = ProyecThor::Settings::SettingsManager::Get().GetSettings().audio;
        bool changed = false;

        ImGui::TextDisabled("Controla el audio reproducido durante la proyeccion.");
        ImGui::Spacing();

        SectionTitle("Volumen");
        ImGui::SetNextItemWidth(260);
        changed |= ImGui::SliderInt("Volumen maestro##vol", &a.masterVolume, 0, 100, "%d%%");
        HelpTooltip("Volumen global de los videos de fondo y overlay.");

        changed |= ImGui::Checkbox("Silenciar##mute", &a.muted);
        ImGui::SameLine(0, 16);
        changed |= ImGui::Checkbox("Silenciar al dejar en negro##mob", &a.muteOnBlank);
        HelpTooltip("Baja el volumen automaticamente cuando no hay texto activo.");

        // Aplicar volumen al reproductor VLC de fondo en tiempo real
        if (changed) {
            Core::VLCBasePlayer* player = Core::PresentationCore::Get().GetBackgroundPlayer();
            if (player) {
                player->SetVolume(a.muted ? 0 : a.masterVolume);
                player->SetMute(a.muted);
            }
        }

        SectionTitle("Dispositivo de Salida");

        static std::vector<std::string> s_DeviceIds;
        static std::vector<std::string> s_DeviceNames;
        static bool s_DevicesLoaded = false;
        static int  s_SelectedDevice = 0;

        if (ImGui::Button("Actualizar dispositivos") || !s_DevicesLoaded) {
            s_DevicesLoaded = true;
            s_DeviceIds.clear();
            s_DeviceNames.clear();
            s_DeviceIds.push_back("");
            s_DeviceNames.push_back("(Por defecto del sistema)");

            Core::VLCBasePlayer* player = Core::PresentationCore::Get().GetBackgroundPlayer();
            if (player) {
                auto devices = player->GetAvailableAudioDevices();
                for (auto& d : devices) {
                    s_DeviceIds.push_back(d.id);
                    s_DeviceNames.push_back(d.description);
                }
            }

            s_SelectedDevice = 0;
            for (int i = 0; i < static_cast<int>(s_DeviceIds.size()); i++) {
                if (s_DeviceIds[i] == a.audioDevice) {
                    s_SelectedDevice = i;
                    break;
                }
            }
        }

        if (!s_DeviceNames.empty()) {
            std::vector<const char*> ptrs;
            ptrs.reserve(s_DeviceNames.size());
            for (auto& n : s_DeviceNames) {
                ptrs.push_back(n.c_str());
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##audiodev", &s_SelectedDevice, ptrs.data(), static_cast<int>(ptrs.size()))) {
                a.audioDevice = s_DeviceIds[s_SelectedDevice];
                Core::VLCBasePlayer* player = Core::PresentationCore::Get().GetBackgroundPlayer();
                if (player) {
                    player->SetAudioDevice(a.audioDevice);
                }
            }
            HelpTooltip("El dispositivo donde se reproducira el audio de los videos.");
        }
    }

} // namespace ProyecThor::UI::Settings