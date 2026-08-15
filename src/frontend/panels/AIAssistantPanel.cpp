#include "AIAssistantPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/panels/home/HomeIcons.h"
#include "external/tools/OpenURL.h"

#include <imgui.h>

namespace ProyecThor::UI {

namespace {

const char* ProviderName(int p) {
    switch (p) {
        case 1: return "Claude";
        case 2: return "ChatGPT";
        case 3: return "Gemini";
        default: return "";
    }
}
const char* ProviderURL(int p) {
    switch (p) {
        case 1: return "https://claude.ai/new";
        case 2: return "https://chat.openai.com/";
        case 3: return "https://gemini.google.com/app";
        default: return "";
    }
}

// Tarjeta grande de seleccion de proveedor -- mismo lenguaje visual que
// LanguageCard (CategoryLanguage.cpp) / PresetSwatch (CategoryTheme.cpp):
// hover sube el fondo un poco, sin bordes duros.
bool ProviderCard(const char* id, const char* name, const char* desc, ImVec2 pos, ImVec2 size)
{
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(id);
    ImGui::InvisibleButton("##card", size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 fill = hovered ? DS::BtnHoverFill : DS::BtnDefaultFill;
    ImU32 bord = hovered ? DS::BtnHoverBord : DS::BtnDefaultBord;
    dl->AddRectFilled(pos, { pos.x + size.x, pos.y + size.y }, fill, DS::RadiusMedium);
    dl->AddRect(pos, { pos.x + size.x, pos.y + size.y }, bord, DS::RadiusMedium);

    ImVec2 nameSz = ImGui::CalcTextSize(name);
    dl->AddText({ pos.x + (size.x - nameSz.x) * 0.5f, pos.y + size.y * 0.36f }, DS::TextPrimary, name);
    ImVec2 descSz = ImGui::CalcTextSize(desc);
    dl->AddText({ pos.x + (size.x - descSz.x) * 0.5f, pos.y + size.y * 0.60f }, DS::TextSecondary, desc);

    return clicked;
}

} // namespace

void AIAssistantPanel::SelectProvider(Provider p)
{
    m_Provider = p;
    if (m_WebView.IsAvailable())
        m_WebView.NavigateTo(ProviderURL(static_cast<int>(p)));
    else
        ProyecThor::External::OpenURL(ProviderURL(static_cast<int>(p)));
}

void AIAssistantPanel::Render(bool* pShow, GlassRenderer& glass)
{
    if (!pShow || !*pShow) {
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 960.0f, 680.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 560.0f, 420.0f }, { 10000.0f, 10000.0f });

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Asistente IA", glass, pShow,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(16.0f, 14.0f));

    if (open) {
        if (m_Provider == Provider::None)
            RenderProviderPicker();
        else
            RenderBrowserArea();
    }

    DS::EndGlassPanel();

    if (!*pShow)
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void AIAssistantPanel::RenderProviderPicker()
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Elegi con que IA queres chatear");
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
        "Se abre el sitio real de esa IA -- inicia sesion como en cualquier navegador. "
        "ProyecThor no ve ni guarda tu usuario/clave.");
    ImGui::Dummy(ImVec2(0.0f, 18.0f));

    const float gap    = 16.0f;
    const float avail  = ImGui::GetContentRegionAvail().x;
    const float cardW  = (avail - gap * 2.0f) / 3.0f;
    const float cardH  = 128.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();

    struct Entry { Provider p; const char* name; const char* desc; };
    static const Entry kEntries[3] = {
        { Provider::Claude,  "Claude",  "Anthropic" },
        { Provider::ChatGPT, "ChatGPT", "OpenAI" },
        { Provider::Gemini,  "Gemini",  "Google" },
    };
    for (int i = 0; i < 3; ++i) {
        ImVec2 pos = { origin.x + i * (cardW + gap), origin.y };
        if (ProviderCard(kEntries[i].name, kEntries[i].name, kEntries[i].desc, pos, { cardW, cardH }))
            SelectProvider(kEntries[i].p);
    }
    ImGui::Dummy(ImVec2(0.0f, cardH));

    if (!m_WebView.IsAvailable()) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
            "%s", m_WebView.GetLastError().empty()
                ? "El navegador embebido no esta disponible en este sistema -- se abrira en tu navegador externo."
                : m_WebView.GetLastError().c_str());
    }
}

void AIAssistantPanel::RenderBrowserArea()
{
    // Barra superior: proveedor actual + volver a elegir.
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "%s",
        ProviderName(static_cast<int>(m_Provider)));
    ImGui::SameLine();
    if (DS::GlassButton("Cambiar de IA", ImVec2(140.0f, 26.0f))) {
        m_Provider = Provider::None;
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
        return;
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (!m_WebView.IsAvailable()) {
        // Ya se abrio en el navegador externo al elegir -- solo se deja un
        // recordatorio + boton para reabrirlo por si lo cerro.
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Se abrio en tu navegador externo (esta ventana no puede embeberlo aca).");
        if (DS::GlassButton("Volver a abrir", ImVec2(160.0f, 30.0f)))
            ProyecThor::External::OpenURL(ProviderURL(static_cast<int>(m_Provider)));
        return;
    }

    if (m_WebView.HasError()) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::DangerColor), "%s", m_WebView.GetLastError().c_str());
        return;
    }

    // El resto del panel queda vacio a proposito: el WebView2 (ventana nativa
    // hija) se dibuja el mismo, por encima de este rectangulo -- ver
    // UpdateBounds mas abajo, que lo posiciona exactamente sobre este child.
    ImGui::BeginChild("##aiWebArea", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 areaPos  = ImGui::GetWindowPos();
    ImVec2 areaSize = ImGui::GetWindowSize();
    ImGui::EndChild();

    m_WebView.UpdateBounds(static_cast<int>(areaPos.x), static_cast<int>(areaPos.y),
                            static_cast<int>(areaSize.x), static_cast<int>(areaSize.y), true);
}

} // namespace ProyecThor::UI
