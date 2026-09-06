#include "TeamChatPanel.h"
#include "backend/core/PresentationCore.h"
#include "SettingsManager.h"
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>

#if __has_include("qrcodegen.hpp")
#   define PROYECTHOR_HAS_QRCODEGEN 1
#   include "qrcodegen.hpp"
#endif

namespace ProyecThor::UI {

// ── Paleta (misma familia visual que StreamingPanel, tono violeta propio
//    para diferenciar "Chat" de "Red" a simple vista) ──────────────────────
// kAccent/kRed quedan fijos a proposito: son la identidad visual de esta
// seccion (violeta), igual criterio que ViewToolsSettings::categoryColor.
// kSurface*/kGray* si se recalculan en SyncPalette() a partir del tema
// activo: eran fondos/texto fijos que quedaban negros sobre cualquier tema.
static constexpr ImVec4 kRed        = { 0.90f, 0.25f, 0.30f, 1.0f };
static ImVec4 kGrayDim    = { 0.45f, 0.47f, 0.55f, 1.0f };
static ImVec4 kGrayText   = { 0.65f, 0.68f, 0.75f, 1.0f };
static constexpr ImVec4 kAccent     = { 0.70f, 0.45f, 0.95f, 1.0f };
static constexpr ImVec4 kAccentLow  = { 0.70f, 0.45f, 0.95f, 0.15f };
static ImVec4 kSurface    = { 0.05f, 0.06f, 0.08f, 1.0f };
static ImVec4 kSurface2   = { 0.10f, 0.11f, 0.15f, 1.0f };
static ImVec4 kText       = { 0.92f, 0.94f, 0.97f, 1.0f };
static ImVec4 kBubbleMine = { 0.16f, 0.11f, 0.22f, 1.0f };

static ImU32 Col(ImVec4 v)  { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 ColA(ImVec4 v, float a) {
    v.w = a; return ImGui::ColorConvertFloat4ToU32(v);
}

// Pre-mezcla un tinte sobre un fondo base y devuelve un color solido
// (mismo criterio que DS::BlendOver en DesignSystem.cpp).
static ImVec4 BlendOver(const float* tint, float alpha, const float* base) {
    return ImVec4(
        tint[0] * alpha + base[0] * (1.0f - alpha),
        tint[1] * alpha + base[1] * (1.0f - alpha),
        tint[2] * alpha + base[2] * (1.0f - alpha),
        1.0f);
}

static void SyncPalette() {
    const auto& t = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    kSurface  = ImVec4(t.surface0[0], t.surface0[1], t.surface0[2], t.surface0[3]);
    kSurface2 = ImVec4(t.surface1[0], t.surface1[1], t.surface1[2], t.surface1[3]);
    kGrayText = ImVec4(t.textDim[0], t.textDim[1], t.textDim[2], t.textDim[3]);
    kGrayDim  = BlendOver(t.textPrimary, 0.35f, t.base);
    kText     = ImVec4(t.textPrimary[0], t.textPrimary[1], t.textPrimary[2], t.textPrimary[3]);
    kBubbleMine = ImVec4(
        kAccent.x * 0.25f + t.base[0] * 0.75f,
        kAccent.y * 0.25f + t.base[1] * 0.75f,
        kAccent.z * 0.25f + t.base[2] * 0.75f,
        1.0f);
}

static std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
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

static std::string FormatTime(int64_t timestampMs)
{
    std::time_t t = static_cast<std::time_t>(timestampMs / 1000);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    return buf;
}

// ── RebuildQR / DrawQR ─────────────────────────────────────────────────────
// Mismo esquema que StreamingPanel::RebuildQRTexture/DrawQR (qrcodegen si
// esta disponible, patron pseudo-aleatorio determinista como fallback).
void TeamChatPanel::RebuildQRTexture(const std::string& url)
{
    if (url == m_QRCachedURL) return;
    m_QRCachedURL = url;
    m_QRModules.clear();
    m_QRSize = 0;
    if (url.empty()) return;

#ifdef PROYECTHOR_HAS_QRCODEGEN
    try {
        auto qr   = qrcodegen::QrCode::encodeText(url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        m_QRSize  = qr.getSize();
        m_QRModules.resize(static_cast<size_t>(m_QRSize) * m_QRSize, 0);
        for (int y = 0; y < m_QRSize; ++y)
            for (int x = 0; x < m_QRSize; ++x)
                m_QRModules[y * m_QRSize + x] = qr.getModule(x, y) ? 1 : 0;
    } catch (...) {}
#else
    m_QRSize = 21;
    m_QRModules.assign(static_cast<size_t>(m_QRSize) * m_QRSize, 0);
    auto setM = [&](int x, int y, uint8_t v) {
        if (x >= 0 && x < m_QRSize && y >= 0 && y < m_QRSize)
            m_QRModules[y * m_QRSize + x] = v;
    };
    auto finder = [&](int ox, int oy) {
        for (int dy = 0; dy < 7; ++dy)
            for (int dx = 0; dx < 7; ++dx)
                setM(ox+dx, oy+dy, (dx==0||dx==6||dy==0||dy==6||(dx>=2&&dx<=4&&dy>=2&&dy<=4)) ? 1 : 0);
    };
    finder(0, 0); finder(14, 0); finder(0, 14);
    for (int i = 8; i <= 12; i += 2) { setM(i,6,1); setM(6,i,1); }

    size_t seed = std::hash<std::string>{}(url);
    for (int y = 0; y < m_QRSize; ++y) {
        for (int x = 0; x < m_QRSize; ++x) {
            if (m_QRModules[y * m_QRSize + x]) continue;
            seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
            m_QRModules[y * m_QRSize + x] = (seed & 1) ? 1 : 0;
        }
    }
#endif
}

void TeamChatPanel::DrawQR(ImDrawList* dl, ImVec2 origin, float size)
{
    if (m_QRSize <= 0) return;

    dl->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(250, 250, 252, 255), 10.0f);
    dl->AddRect(origin, ImVec2(origin.x + size, origin.y + size), Col(kAccentLow), 10.0f, 0, 2.0f);

    const float pad   = size * 0.08f;
    const float inner = size - pad * 2.0f;
    const float cell  = inner / static_cast<float>(m_QRSize);

    for (int row = 0; row < m_QRSize; ++row) {
        for (int col = 0; col < m_QRSize; ++col) {
            if (!m_QRModules[row * m_QRSize + col]) continue;
            float x0 = origin.x + pad + col * cell + 0.5f;
            float y0 = origin.y + pad + row * cell + 0.5f;
            float x1 = x0 + cell - 1.0f;
            float y1 = y0 + cell - 1.0f;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), Col(kSurface2), 1.5f);
        }
    }
}

// ── Update ──────────────────────────────────────────────────────────────────
void TeamChatPanel::Update()
{
    auto& core  = Core::PresentationCore::Get();
    auto  store = core.GetChatMessageStore();
    bool  on    = core.IsChatRunning();

    if (on && store) {
        auto fresh = store->GetMessagesSince(m_LastSeenId);
        if (!fresh.empty()) {
            for (const auto& m : fresh) m_LastSeenId = std::max(m_LastSeenId, m.id);
            m_CachedMessages.insert(m_CachedMessages.end(), fresh.begin(), fresh.end());
        }
        RebuildQRTexture(core.GetState().chatURL);
    } else if (!m_QRCachedURL.empty()) {
        // El ChatMessageStore vive en PresentationCore (sobrevive a que el
        // server HTTP se apague y prenda de nuevo — ver PresentationCore::
        // ToggleChatServer), asi que el historial no se pierde. Solo
        // limpiamos el cache local del panel para que, al volver a activar
        // el chat, se vuelva a traer todo desde el store y quede consistente.
        RebuildQRTexture("");
        m_LastSeenId = 0;
        m_CachedMessages.clear();
    }
}

// ── RenderContent ─────────────────────────────────────────────────────────
void TeamChatPanel::RenderContent()
{
    SyncPalette();
    auto& core = Core::PresentationCore::Get();

    RenderServerControl();

    if (!core.IsChatRunning()) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(kGrayDim, 0.85f));
        ImGui::TextWrapped(
            "Al iniciar, se genera una URL y un QR propios del chat (distintos a "
            "los de Transmisión en Red) para que el equipo se sume desde su "
            "celular o notebook en la misma red WiFi, además de poder escribir "
            "aca mismo.");
        ImGui::PopStyleColor();
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // QR/URL en su propia tarjeta fija (NO adentro del scroll del log): con
    // muchos mensajes el operador no deberia tener que scrollear todo el
    // historial de vuelta arriba solo para volver a mostrarle el QR a
    // alguien que se suma tarde.
    RenderURLSection();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const float composerH = 46.0f;
    const float gapH      = 8.0f;
    float logH = ImGui::GetContentRegionAvail().y - composerH - gapH;
    if (logH < 140.0f) logH = 140.0f;

    RenderChatLog(logH);
    ImGui::Dummy(ImVec2(0.0f, gapH - 2.0f));
    RenderComposer();
}

// ── RenderServerControl ───────────────────────────────────────────────────
// Con el chat detenido mostramos la tarjeta completa (puerto + boton grande)
// porque hay que configurarlo. Una vez activo, la tarjeta se reemplaza por
// una barra angosta de una sola linea — el operador ya configuro el puerto,
// y ese espacio le hace mas falta al log de mensajes que a este estado.
void TeamChatPanel::RenderServerControl()
{
    auto& core = Core::PresentationCore::Get();
    bool  on   = core.IsChatRunning();
    float w    = ImGui::GetContentRegionAvail().x;

    if (on) {
        const float barH = 38.0f;
        const float stopW = 84.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col(kBubbleMine));
        ImGui::PushStyleColor(ImGuiCol_Border,  ColA(kAccent, 0.31f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 0.0f));
        ImGui::BeginChild("##chatstatusbar", ImVec2(w, barH), true, ImGuiWindowFlags_NoScrollbar);

        // Bullet en vez de un dibujo manual con AddCircleFilled: se alinea
        // solo con el texto sin matematica de pixeles a mano.
        ImGui::SetCursorPosY((barH - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::Bullet();
        ImGui::SameLine(0.0f, 4.0f);
        // No mostramos m_Port aca: si Streaming ya tenia el server corriendo,
        // el chat se sumo a ESE puerto (puede no ser el que hay en m_Port) —
        // el puerto real esta en la URL de RenderURLSection, mas abajo.
        ImGui::TextUnformatted("Chat activo");
        ImGui::PopStyleColor();

        float rightX = ImGui::GetWindowContentRegionMax().x - stopW;
        ImGui::SameLine(rightX);
        ImGui::SetCursorPosY((barH - 24.0f) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kRed, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kRed, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kRed, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, kRed);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("Detener", ImVec2(stopW, 24.0f)))
            core.ToggleChatServer(false);
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        return;
    }

    float startLocalY = ImGui::GetCursorPosY();
    float cardH = 75.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p0 = ImGui::GetCursorScreenPos();
    ImVec2      p1 = ImVec2(p0.x + w, p0.y + cardH);

    DrawSoftShadow(dl, p0, p1, 10.0f);
    dl->AddRectFilled(p0, p1, Col(kSurface2), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 12), 10.0f, 0, 1.5f);

    float innerX  = 20.0f;
    float labelY  = (cardH - ImGui::GetTextLineHeight() * 2.0f - 6.0f) * 0.5f;

    bool streamingUp = core.IsStreamingNet();
    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY), Col(kGrayText), "CHAT DETENIDO");
    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY + ImGui::GetTextLineHeight() + 6.0f),
        ColA(kGrayDim, 0.8f),
        streamingUp ? "Se va a sumar al puerto que ya usa Transmisión en Red"
                    : "Configura el puerto y presiona Iniciar");

    float portW = 80.0f;
    ImGui::SetCursorPos(ImVec2(w - portW - 16.0f, startLocalY + (cardH - 28.0f) * 0.5f));
    ImGui::SetNextItemWidth(portW);

    ImGui::BeginDisabled(streamingUp);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::InputInt("##chatport", &m_Port, 0, 0);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    m_Port = std::max(1024, std::min(65535, m_Port));

    ImGui::SetCursorPosY(startLocalY + cardH);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::PushStyleColor(ImGuiCol_Border, ColA(kAccent, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (ImGui::Button("INICIAR CHAT", ImVec2(w, 0.0f)))
        core.ToggleChatServer(true, m_Port);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
}

// ── RenderURLSection ──────────────────────────────────────────────────────
// QR + texto + URL copiable, en su propia tarjeta fija.
void TeamChatPanel::RenderURLSection()
{
    auto  state = Core::PresentationCore::Get().GetState();
    float w       = ImGui::GetContentRegionAvail().x;
    float qrSize  = 84.0f;
    float cardPad = 14.0f;
    float cardH   = qrSize + cardPad * 2.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + w, p0.y + cardH);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawSoftShadow(dl, p0, p1, 14.0f);
    dl->AddRectFilled(p0, p1, Col(kSurface2), 14.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 12), 14.0f, 0, 1.0f);

    DrawQR(dl, ImVec2(p0.x + cardPad, p0.y + cardPad), qrSize);

    float textX = p0.x + cardPad * 2.0f + qrSize;
    dl->AddText(ImVec2(textX, p0.y + cardPad),
        Col(kAccent), "Sumate al chat del equipo");
    dl->AddText(ImVec2(textX, p0.y + cardPad + ImGui::GetTextLineHeight() + 4.0f),
        Col(kGrayDim), "Escaneá el QR o abrí el link desde la misma red WiFi.");

    ImGui::Dummy(ImVec2(w, cardH + 10.0f));

    // ── URL copiable ──────────────────────────────────────────────────────
    float btnW = 90.0f;
    float gap  = 8.0f;
    float fieldW = w - btnW - gap;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
    ImGui::PushStyleColor(ImGuiCol_Border, ColA(kAccent, 0.3f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    char urlBuf[256];
    std::strncpy(urlBuf, state.chatURL.c_str(), sizeof(urlBuf) - 1);
    urlBuf[sizeof(urlBuf) - 1] = '\0';

    ImGui::SetNextItemWidth(fieldW);
    ImGui::InputText("##chaturl", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.0f, gap);

    ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f));

    if (ImGui::Button("Copiar Link", ImVec2(btnW, 0.0f)))
        ImGui::SetClipboardText(state.chatURL.c_str());

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

// ── RenderChatLog ──────────────────────────────────────────────────────────
// Scroll propio, separado de la tarjeta de QR/URL (que queda fija arriba,
// fuera de este child) — con muchos mensajes el QR sigue ahi sin scrollear.
void TeamChatPanel::RenderChatLog(float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Col(kSurface2));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ColA(kGrayText, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ColA(kAccent, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);

    ImGui::BeginChild("##chatlog", ImVec2(0.0f, height), true, ImGuiWindowFlags_None);

    bool nearBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 24.0f;

    RenderMessages();

    if (nearBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
}

// ── RenderMessages ────────────────────────────────────────────────────────
void TeamChatPanel::RenderMessages()
{
    if (m_CachedMessages.empty()) {
        const char* hint = "Todavía no hay mensajes. Escribí el primero.";
        float tw = ImGui::CalcTextSize(hint).x;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - tw) * 0.5f));
        ImGui::TextColored(kGrayDim, "%s", hint);
        return;
    }

    std::string myNick = Trim(m_NicknameBuf);

    for (const auto& m : m_CachedMessages) {
        ImGui::PushID(static_cast<int>(m.id));

        bool mine = (m.nickname == myNick);
        ImVec4 nickCol = mine ? kAccent : ImVec4(0.55f, 0.80f, 0.85f, 1.0f);

        ImGui::TextColored(nickCol, "%s", m.nickname.c_str());
        ImGui::SameLine();
        ImGui::TextColored(kGrayDim, "  %s", FormatTime(m.timestampMs).c_str());

        float copyW = 58.0f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - copyW);
        ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::SmallButton("Copiar"))
            ImGui::SetClipboardText(m.text.c_str());
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);

        ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);
        ImGui::PushStyleColor(ImGuiCol_Text, kText);
        ImGui::TextWrapped("%s", m.text.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PopID();
    }
}

// ── RenderComposer ────────────────────────────────────────────────────────
void TeamChatPanel::RenderComposer()
{
    auto& core = Core::PresentationCore::Get();
    float w    = ImGui::GetContentRegionAvail().x;

    float nickW = 110.0f;
    float sendW = 84.0f;
    float gap   = 8.0f;
    float textW = w - nickW - sendW - gap * 2.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
    ImGui::PushStyleColor(ImGuiCol_Border, ColA(kAccent, 0.3f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::SetNextItemWidth(nickW);
    ImGui::InputText("##chatnick", m_NicknameBuf, sizeof(m_NicknameBuf));

    ImGui::SameLine(0.0f, gap);
    ImGui::SetNextItemWidth(textW);
    bool enterPressed = ImGui::InputText("##chatmsg", m_MessageBuf, sizeof(m_MessageBuf),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0.0f, gap);

    ImGui::PushStyleColor(ImGuiCol_Button, ColA(kAccent, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    bool clickedSend = ImGui::Button("Enviar", ImVec2(sendW, 0.0f));

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(4);

    if (clickedSend || enterPressed) {
        std::string nick = Trim(m_NicknameBuf);
        std::string text = Trim(m_MessageBuf);
        if (nick.empty()) { std::strncpy(m_NicknameBuf, "Operador", sizeof(m_NicknameBuf)); nick = "Operador"; }
        if (!text.empty() && core.GetChatMessageStore()) {
            core.GetChatMessageStore()->PostChatMessage(nick, text);
            m_MessageBuf[0] = '\0';
        }
    }
}

} // namespace ProyecThor::UI
