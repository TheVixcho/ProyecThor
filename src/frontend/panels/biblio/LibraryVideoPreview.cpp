#include "LibraryVideoPreview.h"
#include "LibraryHelpers.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/FileDeletionManager.h"
#include "backend/monitors/MonitorUIHelpers.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

#include <imgui.h>
#include <algorithm>
#include <memory>

using namespace ProyecThor::UI::Components;

namespace ProyecThor::Library {

namespace {

// Player propio de este preview -- deliberadamente NO forceSilent (a
// diferencia del player de "Vista en Vivo"/Monitor, que por diseño nunca
// debe sonar): el pedido explicito de esta pantalla completa es poder
// escuchar el video mientras se revisa, con su propio slider de volumen.
// Arranca mudo igual (ver LoadIndex) para no sorprender con audio de golpe
// al abrir -- el operador decide si le sube el volumen.
std::unique_ptr<Core::VLCBasePlayer> s_Player;
bool        s_Open    = false;
int         s_Index   = -1;
bool        s_Playing = false;
bool        s_Muted   = true;
float       s_Volume  = 0.6f; // 0..2, mismo rango que Components::RenderVolumeRow

static bool s_HookRegistered = []() {
    Core::FileDeletionManager::RegisterUsageReleaseHook([](const std::string& /*path*/) {
        if (s_Open) {
            s_Open = false;
            s_Playing = false;
            if (s_Player) s_Player->Stop();
        }
    });
    return true;
}();

std::string FullVideoPath(const std::string& filename) {
    return GetAssetsPath() + "/videos/" + filename;
}

void LoadIndex(LibraryContext& ctx, int index) {
    if (index < 0 || index >= (int)ctx.items.size()) return;
    s_Index = index;
    if (!s_Player)
        s_Player = std::make_unique<Core::VLCBasePlayer>();
    s_Player->Play(FullVideoPath(ctx.items[index]), /*loop=*/false, /*startMuted=*/true);
    s_Player->SetVolume(static_cast<int>(s_Volume * 100.0f));
    s_Player->SetMute(s_Muted);
    s_Playing = true;
}

// Boton cuadrado con icono de StyleGeneralApp (mismo patron que
// GlassIconButton en LibraryVideos.cpp, no compartido porque ese es
// static/privado de ese archivo) -- fallback a un glifo de texto corto si
// el icono todavia no cargo.
bool PreviewIconButton(const char* id, const char* iconKey, const char* fallback, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.24f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasIcon = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    std::string label = (hasIcon ? "" : std::string(fallback)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    if (hasIcon) {
        ImVec2 bMin = ImGui::GetItemRectMin();
        ImVec2 bMax = ImGui::GetItemRectMax();
        const float pad = size.x * 0.28f;
        ImGui::GetWindowDrawList()->AddImage(it->second.textureID,
            ImVec2(bMin.x + pad, bMin.y + pad), ImVec2(bMax.x - pad, bMax.y - pad),
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(240, 240, 245, 255));
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

} // namespace

void OpenVideoPreview(LibraryContext& ctx, int index) {
    s_Open = true;
    LoadIndex(ctx, index);
}

void RenderVideoPreviewOverlay(LibraryContext& ctx) {
    if (!s_Open) return;

    if (s_Player) {
        s_Player->UpdateTexture();
        if (s_Player->ConsumeEndReached()) {
            // Fin del clip -> pasa solo al siguiente, como un reproductor real.
            if (s_Index + 1 < (int)ctx.items.size())
                LoadIndex(ctx, s_Index + 1);
            else
                s_Playing = false;
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.035f, 0.045f, 0.99f));

    ImGui::Begin("##VideoPreviewFullscreen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking);

    auto closePreview = [&]() {
        s_Open = false;
        if (s_Player) s_Player->Stop();
    };

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        closePreview();

    const std::string title = (s_Index >= 0 && s_Index < (int)ctx.items.size())
        ? StripExtension(ctx.items[s_Index]) : "";

    // ── Encabezado: titulo + cerrar ───────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.92f, 0.98f, 1.0f));
        ImGui::SetWindowFontScale(1.15f);
        ImGui::TextUnformatted(title.empty() ? "Vista previa" : title.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 34.0f + ImGui::GetCursorPosX());
        if (PreviewIconButton("pvClose", "close", "X", ImVec2(34.0f, 30.0f)))
            closePreview();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    const float footerH = 118.0f;
    const float fullW   = ImGui::GetContentRegionAvail().x;
    const float videoH  = std::max(80.0f, ImGui::GetContentRegionAvail().y - footerH);

    ImGui::BeginChild("##pvVideoArea", ImVec2(fullW, videoH), false, ImGuiWindowFlags_NoScrollbar);
    DrawVideoFrame(s_Player.get(), fullW, videoH, "Sin video cargado",
                   "PREVIEW", ImVec4(0.35f, 0.55f, 0.95f, 1.0f), false);
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // ── Barra de progreso + tiempo ────────────────────────────────────────
    int64_t curMs = s_Player ? s_Player->GetTime()   : 0;
    int64_t lenMs = s_Player ? s_Player->GetLength() : 0;
    float   pos   = (lenMs > 0)
        ? std::clamp(static_cast<float>(curMs) / static_cast<float>(lenMs), 0.0f, 1.0f)
        : 0.0f;

    if (BMSlider("##pvSeek", &pos, 0.0f, 1.0f, "",
                 ImVec4(1.0f, 1.0f, 1.0f, 0.12f), ImVec4(0.35f, 0.55f, 0.95f, 1.0f),
                 ImVec4(0.45f, 0.65f, 1.0f, 1.0f), fullW) && s_Player) {
        s_Player->SetPosition(pos);
    }
    DrawTimeRow(fullW, 0.0f, curMs, lenMs);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // ── Transporte: Anterior / Play-Pausa / Siguiente (navegan la lista) ──
    // + volumen. "Anterior"/"Siguiente" cambian de VIDEO (no ±10s) -- es lo
    // que tiene sentido en un preview pensado para recorrer la biblioteca.
    const float btnH  = 44.0f;
    const float gap   = 10.0f;
    const float navW  = 60.0f;
    const float playW = 90.0f;
    const float transportW = navW * 2.0f + playW + gap * 2.0f;

    ImGui::SetCursorPosX((fullW - transportW) * 0.5f);
    ImGui::BeginGroup();

    ImGui::BeginDisabled(s_Index <= 0);
    if (PreviewIconButton("pvPrev", "skip_prev", "<", ImVec2(navW, btnH)))
        LoadIndex(ctx, s_Index - 1);
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);

    if (PreviewIconButton("pvPlay", s_Playing ? "pause" : "play", s_Playing ? "||" : ">", ImVec2(playW, btnH))) {
        s_Playing = !s_Playing;
        if (s_Player) s_Player->SetPause(!s_Playing);
    }
    ImGui::SameLine(0.0f, gap);

    ImGui::BeginDisabled(s_Index < 0 || s_Index + 1 >= (int)ctx.items.size());
    if (PreviewIconButton("pvNext", "skip_next", ">", ImVec2(navW, btnH)))
        LoadIndex(ctx, s_Index + 1);
    ImGui::EndDisabled();

    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // ── Volumen (real -- este player, a diferencia del de Vista en Vivo,
    // no es forceSilent) ───────────────────────────────────────────────────
    {
        const float volW = std::min(320.0f, fullW);
        ImGui::SetCursorPosX((fullW - volW) * 0.5f);
        ImGui::BeginGroup();
        VolumeConfig vc{};
        vc.availW    = volW;
        vc.volumeH   = 28.0f;
        vc.pad       = 6.0f;
        vc.muteLabel = s_Muted ? "Mute" : "Vol";
        vc.volume    = &s_Volume;
        vc.muted     = &s_Muted;
        vc.sliderBg  = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
        vc.grab      = ImVec4(0.35f, 0.55f, 0.95f, 1.0f);
        vc.grabAct   = ImVec4(0.45f, 0.65f, 1.0f, 1.0f);
        vc.btnBase   = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
        vc.btnHov    = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
        vc.btnAct    = ImVec4(1.0f, 1.0f, 1.0f, 0.24f);
        vc.sliderId  = "##pvVolume";
        if (RenderVolumeRow(vc) && s_Player) {
            s_Player->SetMute(s_Muted);
            s_Player->SetVolume(static_cast<int>(s_Volume * 100.0f));
        }
        ImGui::EndGroup();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace ProyecThor::Library
