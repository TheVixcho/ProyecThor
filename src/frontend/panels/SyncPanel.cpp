#include "SyncPanel.h"
#include "SettingsManager.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <random>
#include <cstring>
#include <algorithm>

namespace ProyecThor::UI {

// ── Paleta ─────────────────────────────────────────────────────────────────
// Igual criterio que StreamingPanel::SyncPalette(): se lee del tema activo
// en cada frame en vez de fijar colores propios, para que Mobile combine con
// cualquier preset (Dark/Light/OrangeBlack/Jazz/...) que el usuario tenga.
//
// A PROPOSITO no reusa el verde/rojo pulsante de StreamingPanel (Red): esa
// identidad visual (punto parpadeante + "TRANSMITIENDO" en mayusculas) es la
// de "esto esta en vivo, cuidado con el ancho de banda/rendimiento" -- y
// Mobile no tiene nada que ver con eso (solo texto/config chicos, sin video
// ni audio). Usa un violeta calmo, sin animacion de latido, para que se lea
// como una utilidad liviana en vez de una transmision.
static ImVec4 kSurface, kSurface2, kSurface3, kAccent, kGrayText, kGrayDim;
static constexpr ImVec4 kMobileOn  = { 0.58f, 0.56f, 0.97f, 1.0f }; // violeta calmo — "conectado", no "en vivo"
static constexpr ImVec4 kMobileOff = { 0.55f, 0.57f, 0.66f, 1.0f }; // gris neutro — "apagado", sin alarma

static ImU32 Col(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 ColA(ImVec4 v, float a) { v.w = a; return ImGui::ColorConvertFloat4ToU32(v); }

static void SyncPalette() {
    const auto& t = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    kSurface  = ImVec4(t.surface0[0], t.surface0[1], t.surface0[2], t.surface0[3]);
    kSurface2 = ImVec4(t.surface1[0], t.surface1[1], t.surface1[2], t.surface1[3]);
    kSurface3 = ImVec4(t.surface2[0], t.surface2[1], t.surface2[2], t.surface2[3]);
    kAccent   = ImVec4(t.accent[0], t.accent[1], t.accent[2], 1.0f);
    kGrayText = ImVec4(t.textDim[0], t.textDim[1], t.textDim[2], t.textDim[3]);
    kGrayDim  = ImVec4(t.textPrimary[0], t.textPrimary[1], t.textPrimary[2], t.textFaint[3]);
}

static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 6.0f; i += 1.0f) {
        float alpha = 40.0f - (i * 6.0f);
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 4.0f),
            ImVec2(p1.x + i, p1.y + i + 4.0f),
            IM_COL32(0, 0, 0, (int)alpha), rounding + i);
    }
}

static void SectionDivider(const char* label) {
    ImGui::Dummy(ImVec2(0, 10.0f));
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    ImVec2      ts  = ImGui::CalcTextSize(label);

    float cy  = pos.y + ts.y * 0.5f;
    float gap = 12.0f;
    float tx  = pos.x + (w - ts.x) * 0.5f;

    dl->AddRectFilledMultiColor(ImVec2(pos.x, cy), ImVec2(tx - gap, cy + 1.0f),
        IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 0));
    dl->AddRectFilledMultiColor(ImVec2(tx + ts.x + gap, cy), ImVec2(pos.x + w, cy + 1.0f),
        IM_COL32(55, 60, 85, 180), IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 0), IM_COL32(55, 60, 85, 180));

    dl->AddText(ImVec2(tx, pos.y), Col(kAccent), label);
    ImGui::Dummy(ImVec2(w, ts.y + 12.0f));
}

// ── SyncPanel ──────────────────────────────────────────────────────────────
SyncPanel::SyncPanel() {
    auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().sync;
    m_Port  = s.port;
}

SyncPanel::~SyncPanel() {
    if (m_Server.IsRunning()) m_Server.Stop();
}

std::string SyncPanel::GenerateRandomPin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 9);
    std::string pin;
    for (int i = 0; i < 6; i++) pin += static_cast<char>('0' + dist(gen));
    return pin;
}

void SyncPanel::Update() {
    if (m_AutoStartTried) return;
    m_AutoStartTried = true;

    auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().sync;
    if (!s.enabled) return;

    if (s.pairingPin.empty()) {
        s.pairingPin = GenerateRandomPin();
        ProyecThor::Settings::SettingsManager::Get().SaveSettings();
    }

    m_Port = s.port;
    m_Server.SetRootDir(ProyecThor::GetAppDataRoot());
    m_Server.SetPairingToken(s.pairingPin);
    m_Server.Start(m_Port);
}

void SyncPanel::RenderContent() {
    SyncPalette();

    // Sin BeginChild propio: ver comentario equivalente en
    // StreamingPanel::RenderContent() -- este metodo solo vive embebido
    // dentro del area ya scrolleable de Ajustes > Conexiones, un child
    // scrolleable anidado aca adentro causaba doble scrollbar.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

    RenderServerControl();
    if (m_Server.IsRunning()) RenderPairingSection();

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::PopStyleVar();
}

void SyncPanel::RenderServerControl() {
    auto& settings = ProyecThor::Settings::SettingsManager::Get();
    auto& sync     = settings.GetSettings().sync;
    bool  on       = m_Server.IsRunning();
    float w        = ImGui::GetContentRegionAvail().x;

    float startLocalY = ImGui::GetCursorPosY();
    float cardH = 75.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p0 = ImGui::GetCursorScreenPos();
    ImVec2      p1 = ImVec2(p0.x + w, p0.y + cardH);

    DrawSoftShadow(dl, p0, p1, 10.0f);

    ImVec4 stateCol = on ? kMobileOn : kMobileOff;
    ImU32 bg  = on ? ColA(stateCol, 0.14f) : Col(kSurface2);
    ImU32 bdr = on ? ColA(stateCol, 0.30f) : ColA(kGrayText, 0.15f);
    dl->AddRectFilled(p0, p1, bg, 10.0f);
    dl->AddRect(p0, p1, bdr, 10.0f, 0, 1.5f);

    // Barra lateral SOLIDA (sin pulso): a diferencia de Red/Streaming, Mobile
    // no esta "en vivo" -- es una utilidad de fondo, así que no hace falta
    // ningun parpadeo que sugiera "cuidado, esto esta transmitiendo".
    dl->AddRectFilled(p0, ImVec2(p0.x + 4.0f, p0.y + cardH),
        ColA(stateCol, on ? 0.9f : 0.35f), 10.0f, ImDrawFlags_RoundCornersLeft);

    float innerX = 20.0f;
    float labelY = (cardH - ImGui::GetTextLineHeight() * 2.0f - 6.0f) * 0.5f;

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY),
        Col(stateCol),
        on ? "Mobile conectado" : "Mobile desactivado");

    // Punto de estado quieto (sin animación) -- indica "disponible", no
    // "grabando/transmitiendo".
    dl->AddCircleFilled(ImVec2(p0.x + innerX - 10.0f, p0.y + labelY + 7.0f), 3.5f, ColA(stateCol, on ? 1.0f : 0.5f));

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY + ImGui::GetTextLineHeight() + 6.0f),
        on ? ColA(stateCol, 0.8f) : ColA(kGrayDim, 0.8f),
        on ? ("Puerto local " + std::to_string(m_Server.GetPort()) + " abierto").c_str()
           : "Elegi el puerto y activalo cuando quieras usar la app");

    float portW = 80.0f;
    ImGui::SetCursorPos(ImVec2(w - portW - 16.0f, startLocalY + (cardH - 28.0f) * 0.5f));
    ImGui::SetNextItemWidth(portW);

    ImGui::BeginDisabled(on);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::InputInt("##syncport", &m_Port, 0, 0);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    m_Port = std::max(1024, std::min(65535, m_Port));

    ImGui::SetCursorPosY(startLocalY + cardH);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.0f, 12.0f));

    if (!on) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kMobileOn, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kMobileOn, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kMobileOn, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, kMobileOn);
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kMobileOn, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button("Activar Mobile", ImVec2(w, 0.0f))) {
            if (sync.pairingPin.empty()) sync.pairingPin = GenerateRandomPin();
            sync.enabled = true;
            sync.port    = m_Port;
            settings.SaveSettings();

            m_Server.SetRootDir(ProyecThor::GetAppDataRoot());
            m_Server.SetPairingToken(sync.pairingPin);
            m_Server.Start(m_Port);
        }

        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(5);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kMobileOff, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kMobileOff, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kMobileOff, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, kMobileOff);
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kMobileOff, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button("Desactivar Mobile", ImVec2(w, 0.0f))) {
            m_Server.Stop();
            sync.enabled = false;
            settings.SaveSettings();
        }

        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(5);
    }
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // Aclaracion explicita para que no se confunda con Red (streaming): esto
    // no manda video/audio, es liviano y no compite por recursos con la
    // proyeccion en vivo.
    ImGui::PushStyleColor(ImGuiCol_Text, ColA(kGrayDim, 0.75f));
    ImGui::TextWrapped(
        "Liviano: solo intercambia texto y ajustes con la app (canciones, "
        "biblias, control remoto). No manda video ni audio, así que no le "
        "resta rendimiento a la proyección en vivo.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

void SyncPanel::RenderPairingSection() {
    auto& settings = ProyecThor::Settings::SettingsManager::Get();
    auto& sync     = settings.GetSettings().sync;
    float w        = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("EMPAREJAR CON PROYECTHOR MOBILE");

    // ── Tarjeta con IP:puerto y PIN, bien grandes para leer desde lejos ────
    {
        float cardH = 140.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + cardH);

        DrawSoftShadow(dl, p0, p1, 14.0f);
        dl->AddRectFilled(p0, p1, Col(kSurface2), 14.0f);
        dl->AddRect(p0, p1, ColA(kAccent, 0.3f), 14.0f, 0, 1.5f);

        std::string url = m_Server.GetBaseURL();

        ImVec2 urlSize = ImGui::CalcTextSize(url.c_str());
        dl->AddText(nullptr, ImGui::GetFontSize() * 1.15f,
            ImVec2(p0.x + (w - urlSize.x * 1.15f) * 0.5f, p0.y + 20.0f),
            Col(kGrayDim), url.c_str());

        dl->AddText(ImVec2(p0.x + 24.0f, p0.y + 56.0f), Col(kGrayText), "PIN de emparejamiento");

        std::string pin = sync.pairingPin;
        std::string spacedPin;
        for (size_t i = 0; i < pin.size(); i++) {
            spacedPin += pin[i];
            if (i + 1 < pin.size()) spacedPin += "  ";
        }
        ImVec2 pinSize = ImGui::CalcTextSize(spacedPin.c_str());
        dl->AddText(nullptr, ImGui::GetFontSize() * 1.8f,
            ImVec2(p0.x + (w - pinSize.x * 1.8f) * 0.5f, p0.y + 76.0f),
            Col(kAccent), spacedPin.c_str());

        ImGui::Dummy(ImVec2(w, cardH + 10.0f));
    }

    // ── Botones: copiar URL / regenerar PIN ───────────────────────────────
    {
        float gap  = 8.0f;
        float btnW = (w - gap) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f));

        if (ImGui::Button("Copiar dirección", ImVec2(btnW, 0.0f)))
            ImGui::SetClipboardText(m_Server.GetBaseURL().c_str());

        ImGui::SameLine(0.0f, gap);

        if (ImGui::Button("Regenerar PIN", ImVec2(btnW, 0.0f))) {
            sync.pairingPin = GenerateRandomPin();
            settings.SaveSettings();
            m_Server.SetPairingToken(sync.pairingPin);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ColA(kGrayDim, 0.85f));
    ImGui::TextWrapped(
        "En el celular, abri ProyecThor Mobile > Sincronizar: deberia "
        "encontrar este PC solo si estan en la misma red WiFi. La primera "
        "vez vas a tener que escribir el PIN de arriba para emparejar los "
        "dispositivos. Despues, usa Subir/Bajar/Sincronizar todo segun que "
        "lado tenga los datos mas recientes -- nunca se borra nada "
        "automáticamente. El mismo emparejamiento habilita además la "
        "sección Control remoto de la app movil (elegir canción/verso/fondo "
        "y proyectarlo, estilo Holyrics).");
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI
