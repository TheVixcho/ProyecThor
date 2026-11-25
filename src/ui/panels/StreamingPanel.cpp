#include "StreamingPanel.h"
#include "core/PresentationCore.h"
#include "ui/framework/UIManager.h"
#include "SettingsManager.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

#if __has_include("qrcodegen.hpp")
#   define PROYECTHOR_HAS_QRCODEGEN 1
#   include "qrcodegen.hpp"
#endif

namespace ProyecThor::UI {

// ── Paleta Minimalista 100% Sólida (Cero transparencias, cero reflejos) ──────
static constexpr ImVec4 kGreen  = { 0.16f, 0.78f, 0.45f, 1.0f };
static constexpr ImVec4 kRed    = { 0.88f, 0.28f, 0.32f, 1.0f };
static constexpr ImVec4 kAmber  = { 0.94f, 0.72f, 0.20f, 1.0f };
static constexpr ImVec4 kAccent = { 0.25f, 0.56f, 0.98f, 1.0f };

static ImVec4 kBase      = { 0.06f, 0.07f, 0.09f, 1.0f };
static ImVec4 kSurface   = { 0.08f, 0.09f, 0.12f, 1.0f };
static ImVec4 kSurface2  = { 0.11f, 0.12f, 0.16f, 1.0f };
static ImVec4 kSurface3  = { 0.15f, 0.17f, 0.22f, 1.0f };
static ImVec4 kTextMain  = { 0.93f, 0.94f, 0.96f, 1.0f };
static ImVec4 kTextMuted = { 0.58f, 0.61f, 0.68f, 1.0f };
static ImVec4 kBorderCol = { 0.19f, 0.21f, 0.27f, 1.0f };

static ImVec4 LerpSolid(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        1.0f);
}

static ImU32 Col(ImVec4 v)
{
    v.w = 1.0f;
    return ImGui::ColorConvertFloat4ToU32(v);
}

static void SyncPalette()
{
    const auto& t = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    kBase      = ImVec4(t.base[0],        t.base[1],        t.base[2],        1.0f);
    kSurface   = ImVec4(t.surface0[0],    t.surface0[1],    t.surface0[2],    1.0f);
    kSurface2  = ImVec4(t.surface1[0],    t.surface1[1],    t.surface1[2],    1.0f);
    kSurface3  = ImVec4(t.surface2[0],    t.surface2[1],    t.surface2[2],    1.0f);
    kTextMain  = ImVec4(t.textPrimary[0], t.textPrimary[1], t.textPrimary[2], 1.0f);
    kTextMuted = ImVec4(t.textDim[0],     t.textDim[1],     t.textDim[2],     1.0f);
    kBorderCol = LerpSolid(kSurface2, kTextMuted, 0.22f);
}

// ── Animación ligera sin consumo en reposo ───────────────────────────────────
// Cuando el valor alcanza el destino se fija exactamente y no ejecuta más
// cálculos hasta la próxima interacción del usuario.
static float AnimFloat(ImGuiID id, ImU32 salt, float target, float speed = 16.0f)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* v = storage->GetFloatRef(id ^ salt, target);
    float diff = target - *v;
    if (std::fabs(diff) < 0.0008f) {
        *v = target;
        return target;
    }
    float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    *v += diff * std::min(1.0f, dt * speed);
    return *v;
}

// ── Contenedor de tarjeta plana (0 ventanas hijas extra, 2 draw calls) ───────
struct FlatCard {
    ImDrawList*        dl = nullptr;
    ImDrawListSplitter splitter;
    ImVec2             p0 = { 0.0f, 0.0f };
    float              width     = 0.0f;
    float              padX      = 20.0f;
    float              padY      = 16.0f;
    ImU32              bgCol     = 0;
    ImU32              borderCol = 0;
    float              rounding  = 12.0f;

    float InnerWidth() const { return std::max(40.0f, width - padX * 2.0f); }

    void Begin(float w, ImU32 bg, ImU32 border, float px = 20.0f, float py = 16.0f, float rnd = 12.0f)
    {
        dl        = ImGui::GetWindowDrawList();
        p0        = ImGui::GetCursorScreenPos();
        width     = w;
        padX      = px;
        padY      = py;
        bgCol     = bg;
        borderCol = border;
        rounding  = rnd;

        splitter.Split(dl, 2);
        splitter.SetCurrentChannel(dl, 1);

        ImGui::Indent(padX);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + padX, p0.y + padY));
    }

    void End()
    {
        float endContentY = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y;
        float bottomY     = std::max(p0.y + padY * 2.0f, endContentY + padY);

        ImGui::Unindent(padX);

        ImVec2 p1(p0.x + width, bottomY);
        splitter.SetCurrentChannel(dl, 0);
        dl->AddRectFilled(p0, p1, bgCol, rounding);
        dl->AddRect(p0, p1, borderCol, rounding, 0, 1.0f);
        splitter.Merge(dl);

        ImGui::SetCursorScreenPos(ImVec2(p0.x, bottomY));
        ImGui::Dummy(ImVec2(width, 0.0f));
    }
};

// ── Encabezado minimalista de sección ────────────────────────────────────────
static void SectionTitleMinimal(const char* label)
{
    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 4.0f, pos.y), Col(kTextMuted), label);
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() + 8.0f));
}

// ── Insignia sólida compacta (ej. PRO / Pantalla negra) ──────────────────────
static void DrawSolidBadge(ImDrawList* dl, ImVec2 pos, const char* text, ImVec4 bg, ImVec4 border, ImVec4 fg)
{
    ImGui::SetWindowFontScale(0.84f);
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImGui::SetWindowFontScale(1.0f);

    const float padX = 7.0f;
    const float h    = 20.0f;
    const float w    = ts.x + padX * 2.0f;

    ImVec2 p1(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, p1, Col(bg), 6.0f);
    dl->AddRect(pos, p1, Col(border), 6.0f, 0, 1.0f);

    ImFont* font = ImGui::GetFont();
    float   fsz  = ImGui::GetFontSize() * 0.84f;
    dl->AddText(font, fsz, ImVec2(pos.x + padX, pos.y + (h - ts.y) * 0.5f), Col(fg), text);
}

// ── Interruptor moldeable tipo app (estiramiento elástico sin costo extra) ───
static void DrawMoldableToggleVisual(ImDrawList* dl, ImVec2 p0, bool value, bool hovered, bool held, ImGuiID id)
{
    const ImVec2 size(44.0f, 24.0f);
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);

    float onT  = AnimFloat(id, 0x10u, value ? 1.0f : 0.0f, 15.0f);
    float actT = AnimFloat(id, 0x11u, held ? 1.0f : (hovered ? 0.35f : 0.0f), 20.0f);

    ImVec4 offTrack = LerpSolid(kSurface, kSurface3, 0.55f + actT * 0.25f);
    ImVec4 onTrack  = LerpSolid(kAccent, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), hovered ? 0.08f : 0.0f);
    ImVec4 trackCol = LerpSolid(offTrack, onTrack, onT);
    ImVec4 bdrCol   = LerpSolid(kBorderCol, onTrack, onT);

    dl->AddRectFilled(p0, p1, Col(trackCol), size.y * 0.5f);
    dl->AddRect(p0, p1, Col(bdrCol), size.y * 0.5f, 0, 1.0f);

    // Cápsula moldeable: círculo perfecto en reposo, se estira suavemente
    // al presionarse o a mitad de recorrido hacia el otro extremo.
    const float pad        = 3.0f;
    const float knobH      = size.y - pad * 2.0f;
    const float midStretch = 4.0f * onT * (1.0f - onT);
    const float knobW      = knobH + 6.0f * midStretch + 3.0f * actT;
    const float travel     = (size.x - pad * 2.0f) - knobW;

    float kx0 = p0.x + pad + travel * onT;
    float ky0 = p0.y + pad;

    ImVec4 knobIdle = ImVec4(0.84f, 0.86f, 0.90f, 1.0f);
    ImVec4 knobPure = ImVec4(0.99f, 0.99f, 1.00f, 1.0f);
    ImVec4 knobCol  = LerpSolid(knobIdle, knobPure, std::max(onT, actT));

    dl->AddRectFilled(ImVec2(kx0, ky0), ImVec2(kx0 + knobW, ky0 + knobH), Col(knobCol), knobH * 0.5f);
}

// ── Botón táctil moldeable con respuesta física al clic ──────────────────────
static bool DrawMoldableButton(const char* id, const char* label, ImVec2 size, ImVec4 baseCol, ImVec4 textCol)
{
    ImGui::PushID(id);
    ImGui::InvisibleButton("##mbtn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGuiID gid    = ImGui::GetID("##mbtn");
    float   hovT   = AnimFloat(gid, 0x20u, hovered ? 1.0f : 0.0f, 18.0f);
    float   pressT = AnimFloat(gid, 0x21u, held ? 1.0f : 0.0f, 24.0f);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();

    // Efecto táctil moldeable al presionar
    float insetX = 1.6f * pressT;
    float insetY = 1.2f * pressT;
    ImVec2 bp0(p0.x + insetX, p0.y + insetY);
    ImVec2 bp1(p1.x - insetX, p1.y - insetY);

    ImVec4 colHover = LerpSolid(baseCol, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.10f);
    ImVec4 colPress = LerpSolid(baseCol, kBase, 0.14f);
    ImVec4 fillCol  = LerpSolid(LerpSolid(baseCol, colHover, hovT), colPress, pressT);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bp0, bp1, Col(fillCol), 9.0f);

    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2((bp0.x + bp1.x - ts.x) * 0.5f, (bp0.y + bp1.y - ts.y) * 0.5f), Col(textCol), label);

    ImGui::PopID();
    return clicked;
}

// ── Selector segmentado deslizante moldeable (estilo app nativa) ─────────────
static bool DrawSegmentedControl(const char* id, const char* const* labels, int count, int* currentIdx, float width, float height = 36.0f)
{
    bool changed = false;
    ImGui::PushID(id);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      p0  = ImGui::GetCursorScreenPos();
    ImVec2      p1(p0.x + width, p0.y + height);
    ImGuiID     gid = ImGui::GetID("##segTrack");

    dl->AddRectFilled(p0, p1, Col(kSurface), 9.0f);
    dl->AddRect(p0, p1, Col(kBorderCol), 9.0f, 0, 1.0f);

    const float pad  = 3.0f;
    const float segW = (width - pad * 2.0f) / static_cast<float>(count);
    const float segH = height - pad * 2.0f;

    float posT   = AnimFloat(gid, 0x30u, static_cast<float>(*currentIdx), 16.0f);
    float flight = std::min(1.0f, std::fabs(static_cast<float>(*currentIdx) - posT));

    // Pastilla activa que se estira ligeramente mientras viaja entre opciones
    float stretchX = flight * (segW * 0.14f);
    float squishY  = flight * 1.2f;
    float baseX    = p0.x + pad + posT * segW;
    float tx0      = std::max(p0.x + pad, baseX - stretchX * 0.5f);
    float tx1      = std::min(p1.x - pad, baseX + segW + stretchX * 0.5f);
    float ty0      = p0.y + pad + squishY;
    float ty1      = p0.y + pad + segH - squishY;

    ImVec4 thumbFill   = LerpSolid(kSurface3, kAccent, 0.24f);
    ImVec4 thumbBorder = LerpSolid(kBorderCol, kAccent, 0.72f);
    dl->AddRectFilled(ImVec2(tx0, ty0), ImVec2(tx1, ty1), Col(thumbFill), 7.0f);
    dl->AddRect(ImVec2(tx0, ty0), ImVec2(tx1, ty1), Col(thumbBorder), 7.0f, 0, 1.0f);

    for (int i = 0; i < count; ++i) {
        ImGui::PushID(i);
        ImVec2 sp0(p0.x + pad + i * segW, p0.y + pad);
        ImGui::SetCursorScreenPos(sp0);
        ImGui::InvisibleButton("##opt", ImVec2(segW, segH));

        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (clicked && *currentIdx != i) {
            *currentIdx = i;
            changed = true;
        }

        float selProx = std::clamp(1.0f - std::fabs(posT - static_cast<float>(i)), 0.0f, 1.0f);
        float hovT    = AnimFloat(ImGui::GetID("##opt"), 0x31u, hovered ? 1.0f : 0.0f, 18.0f);

        ImVec4 idleTxt = LerpSolid(kTextMuted, kTextMain, hovT * 0.7f);
        ImVec4 txtCol  = LerpSolid(idleTxt, kTextMain, selProx);

        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(sp0.x + (segW - ts.x) * 0.5f, sp0.y + (segH - ts.y) * 0.5f), Col(txtCol), labels[i]);
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::PopID();
    return changed;
}

// ── Control deslizante moldeable minimalista ─────────────────────────────────
static bool DrawMoldableSlider(const char* id, int* value, int minVal, int maxVal, float width)
{
    bool changed = false;
    ImGui::PushID(id);

    const float hitH = 24.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##sld", ImVec2(width, hitH));

    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    if (active) {
        float rel  = std::clamp((ImGui::GetIO().MousePos.x - p0.x) / std::max(1.0f, width), 0.0f, 1.0f);
        int   next = static_cast<int>(std::round(minVal + rel * (maxVal - minVal)));
        if (next != *value) {
            *value  = next;
            changed = true;
        }
    }

    ImGuiID gid        = ImGui::GetID("##sld");
    float   normTarget = static_cast<float>(*value - minVal) / static_cast<float>(maxVal - minVal);
    float   visT       = AnimFloat(gid, 0x40u, normTarget, 22.0f);
    float   actT       = AnimFloat(gid, 0x41u, active ? 1.0f : (hovered ? 0.40f : 0.0f), 20.0f);

    float trackH = 6.0f + 2.0f * actT;
    float cy     = p0.y + hitH * 0.5f;
    float knobW  = 16.0f + 5.0f * actT;
    float knobH  = 16.0f + 2.0f * actT;
    float kx     = p0.x + knobW * 0.5f + (width - knobW) * visT;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p0.x, cy - trackH * 0.5f), ImVec2(p0.x + width, cy + trackH * 0.5f), Col(kSurface), trackH * 0.5f);
    dl->AddRectFilled(ImVec2(p0.x, cy - trackH * 0.5f), ImVec2(kx, cy + trackH * 0.5f), Col(kAccent), trackH * 0.5f);

    ImVec2 kp0(kx - knobW * 0.5f, cy - knobH * 0.5f);
    ImVec2 kp1(kx + knobW * 0.5f, cy + knobH * 0.5f);
    dl->AddRectFilled(kp0, kp1, Col(ImVec4(0.99f, 0.99f, 1.0f, 1.0f)), knobH * 0.5f);
    dl->AddRect(kp0, kp1, Col(kAccent), knobH * 0.5f, 0, 1.5f);

    ImGui::PopID();
    return changed;
}

// ── CaptureAndPushFrame ───────────────────────────────────────────────────────
void StreamingPanel::CaptureAndPushFrame(int w, int h, int quality)
{
    if (w <= 0 || h <= 0) return;

    auto& core = Core::PresentationCore::Get();
    std::vector<uint8_t> rgb;
    if (!core.RenderProjectorToFBO(w, h, rgb)) return;

    m_EncodeWorker.SubmitFrame(std::move(rgb), w, h, quality,
        [](std::vector<uint8_t> jpeg) {
            Core::PresentationCore::Get().PushFrame(std::move(jpeg));
        });
}

// ── RebuildQR ─────────────────────────────────────────────────────────────────
void StreamingPanel::RebuildQRTexture(const std::string& url)
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

// ── DrawQR (Sólido, alto contraste en cualquier tema, sin sombras pesadas) ───
void StreamingPanel::DrawQR(ImDrawList* dl, ImVec2 origin, float size)
{
    if (m_QRSize <= 0) return;

    ImVec2 p1(origin.x + size, origin.y + size);
    dl->AddRectFilled(origin, p1, IM_COL32(250, 251, 253, 255), 10.0f);
    dl->AddRect(origin, p1, Col(kBorderCol), 10.0f, 0, 1.0f);

    const float pad   = size * 0.08f;
    const float inner = size - pad * 2.0f;
    const float cell  = inner / static_cast<float>(m_QRSize);

    for (int row = 0; row < m_QRSize; ++row) {
        for (int col = 0; col < m_QRSize; ++col) {
            if (!m_QRModules[row * m_QRSize + col]) continue;
            float x0 = origin.x + pad + col * cell + 0.35f;
            float y0 = origin.y + pad + row * cell + 0.35f;
            float x1 = x0 + cell - 0.7f;
            float y1 = y0 + cell - 0.7f;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(18, 20, 26, 255), 1.2f);
        }
    }
}

static int DesiredCaptureFPS(const Core::StreamConfig& cfg)
{
    switch (cfg.videoMode) {
        case Core::StreamConfig::VideoMode::UltraStable:
            return std::clamp(cfg.targetFPS, 24, 60);
        case Core::StreamConfig::VideoMode::HighQuality:
            return 30;
        case Core::StreamConfig::VideoMode::LowLatency:
        default:
            return 8;
    }
}

void StreamingPanel::Update()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    if (state.isStreamingNet && !m_Config.transparentMode && m_Config.sendBackground) {
        double now      = ImGui::GetTime();
        int    fps      = DesiredCaptureFPS(m_Config);
        double interval = 1.0 / static_cast<double>(fps);

        if (now - m_LastCaptureTime >= interval) {
            m_LastCaptureTime = now;
            CaptureAndPushFrame(m_Config.frameWidth, m_Config.frameHeight, m_Config.jpegQuality);
        }
    }

    if (state.isStreamingNet && !state.networkURL.empty()) {
        RebuildQRTexture(state.networkURL);
    }
}

void StreamingPanel::RenderContent()
{
    SyncPalette();
    auto& core = Core::PresentationCore::Get();

    // Margen lateral suave y ancho máximo centrado en monitores anchos
    float totalAvailW = ImGui::GetContentRegionAvail().x;
    const float minSideMargin = 4.0f;
    const float maxCardWidth  = 740.0f;
    float usableW = std::max(220.0f, totalAvailW - minSideMargin * 2.0f);
    float contentW = std::min(usableW, maxCardWidth);
    float leftOffset = minSideMargin + (usableW - contentW) * 0.5f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 0.0f));
    if (leftOffset > 0.5f) ImGui::Indent(leftOffset);

    ImGui::PushItemWidth(contentW);

    RenderServerControl();
    RenderURLSection();
    RenderLayerSelector();
    RenderQualitySelector();

    ImGui::PopItemWidth();

    if (m_ConfigDirty && core.IsStreamingNet() && core.GetNetworkServer()) {
        core.GetNetworkServer()->SetConfig(m_Config);
        m_ConfigDirty = false;
    }

    if (leftOffset > 0.5f) ImGui::Unindent(leftOffset);
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
    ImGui::PopStyleVar();
}

// ── 1. Estado del Servidor + Puerto + Acción ─────────────────────────────────
void StreamingPanel::RenderServerControl()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();
    bool  on    = state.isStreamingNet;
    float w     = std::min(ImGui::GetContentRegionAvail().x, 740.0f);

    float liveT = AnimFloat(ImGui::GetID("##srvStateAnim"), 0x01u, on ? 1.0f : 0.0f, 14.0f);
    ImVec4 cardBorder = LerpSolid(kBorderCol, LerpSolid(kSurface2, kGreen, 0.50f), liveT);

    FlatCard card;
    card.Begin(w, Col(kSurface2), Col(cardBorder), 20.0f, 14.0f);
    float innerW = card.InnerWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH  = 36.0f;
    const float btnW  = 112.0f;
    const float portW = 74.0f;

    auto toggleServer = [&]() {
        if (!on) {
            m_StatusError.clear();
            if (!core.ToggleNetworkStream(true, m_Port, &m_Config)) {
                m_StatusError = "El puerto " + std::to_string(m_Port) + " está ocupado por otro programa.";
            } else {
                if (core.GetNetworkServer()) core.GetNetworkServer()->SetConfig(m_Config);
                m_ConfigDirty = false;
            }
        } else {
            core.ToggleNetworkStream(false);
            m_StatusError.clear();
        }
    };

    ImVec4 btnCol = LerpSolid(kAccent, kRed, liveT);
    const char* btnLabel = on ? "Detener" : "Iniciar";
    const char* statusText = on ? "En vivo" : "Detenido";
    ImVec4 statusTxtCol = LerpSolid(kTextMain, kGreen, liveT);
    ImVec4 dotCol       = LerpSolid(kTextMuted, kGreen, liveT);

    float framePadY = std::max(2.0f, (rowH - ImGui::GetFontSize()) * 0.5f);

    if (innerW >= 360.0f) {
        ImVec2 rowP0 = ImGui::GetCursorScreenPos();

        // Indicador de estado a la izquierda
        dl->AddCircleFilled(ImVec2(rowP0.x + 6.0f, rowP0.y + rowH * 0.5f), 4.5f, Col(dotCol), 16);
        float lh = ImGui::GetTextLineHeight();
        dl->AddText(ImVec2(rowP0.x + 20.0f, rowP0.y + (rowH - lh) * 0.5f), Col(statusTxtCol), statusText);

        // Controles alineados a la derecha
        float btnX  = rowP0.x + innerW - btnW;
        float portX = btnX - 10.0f - portW;
        ImVec2 portLblSz = ImGui::CalcTextSize("Puerto");
        dl->AddText(ImVec2(portX - 8.0f - portLblSz.x, rowP0.y + (rowH - portLblSz.y) * 0.5f), Col(kTextMuted), "Puerto");

        ImGui::SetCursorScreenPos(ImVec2(portX, rowP0.y));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border,  Col(kBorderCol));
        ImGui::PushStyleColor(ImGuiCol_Text,    Col(on ? kTextMuted : kTextMain));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, framePadY));
        ImGui::SetNextItemWidth(portW);
        ImGui::InputInt("##port", &m_Port, 0, 0, on ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            m_Port = std::clamp(m_Port, 1024, 65535);
        }
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(btnX, rowP0.y));
        if (DrawMoldableButton("##srvToggle", btnLabel, ImVec2(btnW, rowH), btnCol, ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) {
            m_Port = std::clamp(m_Port, 1024, 65535);
            toggleServer();
        }

        ImGui::SetCursorScreenPos(ImVec2(rowP0.x, rowP0.y + rowH));
        ImGui::Dummy(ImVec2(innerW, 0.0f));
    } else {
        // Fallback apilado para paneles muy angostos
        ImVec2 rowP0 = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(rowP0.x + 6.0f, rowP0.y + rowH * 0.5f), 4.5f, Col(dotCol), 16);
        float lh = ImGui::GetTextLineHeight();
        dl->AddText(ImVec2(rowP0.x + 20.0f, rowP0.y + (rowH - lh) * 0.5f), Col(statusTxtCol), statusText);

        float portX = rowP0.x + innerW - portW;
        ImVec2 portLblSz = ImGui::CalcTextSize("Puerto");
        dl->AddText(ImVec2(portX - 8.0f - portLblSz.x, rowP0.y + (rowH - portLblSz.y) * 0.5f), Col(kTextMuted), "Puerto");

        ImGui::SetCursorScreenPos(ImVec2(portX, rowP0.y));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border,  Col(kBorderCol));
        ImGui::PushStyleColor(ImGuiCol_Text,    Col(on ? kTextMuted : kTextMain));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, framePadY));
        ImGui::SetNextItemWidth(portW);
        ImGui::InputInt("##portNarrow", &m_Port, 0, 0, on ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            m_Port = std::clamp(m_Port, 1024, 65535);
        }
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(rowP0.x, rowP0.y + rowH + 10.0f));
        if (DrawMoldableButton("##srvToggleNarrow", btnLabel, ImVec2(innerW, rowH), btnCol, ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) {
            m_Port = std::clamp(m_Port, 1024, 65535);
            toggleServer();
        }
    }

    card.End();

    if (!m_StatusError.empty() && !on) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, Col(kRed));
        ImGui::TextWrapped("%s", m_StatusError.c_str());
        ImGui::PopStyleColor();
    }
}

// ── 2. Tarjeta desplegable de Enlace + QR (solo cuando está transmitiendo) ──
void StreamingPanel::RenderURLSection()
{
    auto  state = Core::PresentationCore::Get().GetState();
    float w     = std::min(ImGui::GetContentRegionAvail().x, 740.0f);

    float openT = AnimFloat(ImGui::GetID("##qrCollapseAnim"), 0x01u, state.isStreamingNet ? 1.0f : 0.0f, 14.0f);
    if (openT <= 0.005f) return;

    static double s_CopiedUntil = 0.0;
    bool isCopied = (ImGui::GetTime() < s_CopiedUntil);
    float copyT   = AnimFloat(ImGui::GetID("##copyBtnAnim"), 0x02u, isCopied ? 1.0f : 0.0f, 16.0f);

    ImGui::Dummy(ImVec2(0.0f, 12.0f * openT));

    const float padX   = 20.0f;
    const float padY   = 16.0f;
    const float innerW = std::max(40.0f, w - padX * 2.0f);
    const float qrSize = 104.0f;
    const bool  sideBySide = (innerW >= 360.0f);
    const float fullH  = sideBySide ? (qrSize + padY * 2.0f) : (padY + qrSize + 14.0f + 78.0f + padY);
    const float visH   = fullH * openT;

    ImVec2 cardP0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushClipRect(cardP0, ImVec2(cardP0.x + w, cardP0.y + visH), true);

    ImVec2 cardP1(cardP0.x + w, cardP0.y + fullH);
    dl->AddRectFilled(cardP0, cardP1, Col(kSurface2), 12.0f);
    dl->AddRect(cardP0, cardP1, Col(kBorderCol), 12.0f, 0, 1.0f);

    float lh = ImGui::GetTextLineHeight();
    const float rowH  = 36.0f;
    const float copyW = 96.0f;
    float framePadY = std::max(2.0f, (rowH - ImGui::GetFontSize()) * 0.5f);

    char urlBuf[256];
    std::strncpy(urlBuf, state.networkURL.c_str(), sizeof(urlBuf) - 1);
    urlBuf[sizeof(urlBuf) - 1] = '\0';

    ImVec4 copyCol = LerpSolid(kAccent, kGreen, copyT);
    const char* copyLabel = isCopied ? "¡Copiado!" : "Copiar";

    if (sideBySide) {
        DrawQR(dl, ImVec2(cardP0.x + padX, cardP0.y + padY), qrSize);

        float rx = cardP0.x + padX + qrSize + 20.0f;
        float rw = std::max(100.0f, innerW - qrSize - 20.0f);
        float blockH = lh * 2.0f + 4.0f + 12.0f + rowH;
        float ry = cardP0.y + padY + std::max(0.0f, (qrSize - blockH) * 0.5f);

        dl->AddText(ImVec2(rx, ry), Col(kTextMain), "Enlace de conexión");
        dl->AddText(ImVec2(rx, ry + lh + 4.0f), Col(kTextMuted), "Ábrelo en OBS o escanéalo con tu celular");

        float uy   = ry + lh * 2.0f + 16.0f;
        float urlW = std::max(60.0f, rw - copyW - 8.0f);

        ImGui::SetCursorScreenPos(ImVec2(rx, uy));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border,  Col(kBorderCol));
        ImGui::PushStyleColor(ImGuiCol_Text,    Col(kTextMain));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, framePadY));
        ImGui::SetNextItemWidth(urlW);
        ImGui::InputText("##urlField", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(rx + urlW + 8.0f, uy));
        if (DrawMoldableButton("##copyUrlBtn", copyLabel, ImVec2(copyW, rowH), copyCol, ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) {
            ImGui::SetClipboardText(state.networkURL.c_str());
            s_CopiedUntil = ImGui::GetTime() + 1.6;
        }
    } else {
        float qrX = cardP0.x + (w - qrSize) * 0.5f;
        DrawQR(dl, ImVec2(qrX, cardP0.y + padY), qrSize);

        float ry = cardP0.y + padY + qrSize + 14.0f;
        float rx = cardP0.x + padX;
        dl->AddText(ImVec2(rx, ry), Col(kTextMain), "Enlace de conexión");

        float uy   = ry + lh + 10.0f;
        float urlW = std::max(60.0f, innerW - copyW - 8.0f);

        ImGui::SetCursorScreenPos(ImVec2(rx, uy));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border,  Col(kBorderCol));
        ImGui::PushStyleColor(ImGuiCol_Text,    Col(kTextMain));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, framePadY));
        ImGui::SetNextItemWidth(urlW);
        ImGui::InputText("##urlFieldNarrow", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(rx + urlW + 8.0f, uy));
        if (DrawMoldableButton("##copyUrlBtnNarrow", copyLabel, ImVec2(copyW, rowH), copyCol, ImVec4(1.0f, 1.0f, 1.0f, 1.0f))) {
            ImGui::SetClipboardText(state.networkURL.c_str());
            s_CopiedUntil = ImGui::GetTime() + 1.6;
        }
    }

    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(cardP0.x, cardP0.y + visH));
    ImGui::Dummy(ImVec2(w, 0.0f));
}

// ── 3. Contenido / Capas (Sin solapamientos ni textos innecesarios) ─────────
void StreamingPanel::RenderLayerSelector()
{
    float w = std::min(ImGui::GetContentRegionAvail().x, 740.0f);
    const bool isPro = true;

    if (!isPro && m_Config.transparentMode) {
        m_Config.transparentMode = false;
        m_ConfigDirty = true;
    }

    SectionTitleMinimal("CONTENIDO");

    struct LayerRow {
        const char* id;
        const char* name;
        bool*       val;
        bool        isBgRow;
        bool        isTranspRow;
    };

    LayerRow rows[4] = {
        { "##layer_bg", "Fondo",         &m_Config.sendBackground,  true,  false },
        { "##layer_tr", "Transparencia", &m_Config.transparentMode, false, true  },
        { "##layer_tx", "Textos",        &m_Config.sendText,        false, false },
        { "##layer_ov", "Overlay",       &m_Config.sendOverlay,     false, false },
    };

    FlatCard card;
    card.Begin(w, Col(kSurface2), Col(kBorderCol), 12.0f, 6.0f);
    float innerW   = card.InnerWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH  = 46.0f;
    const float hPad  = 8.0f;
    const float lh    = ImGui::GetTextLineHeight();

    for (int i = 0; i < 4; ++i) {
        auto& row = rows[i];
        ImVec2 rp0 = ImGui::GetCursorScreenPos();

        if (i > 0) {
            ImU32 sepCol = Col(LerpSolid(kSurface2, kBorderCol, 0.65f));
            dl->AddLine(ImVec2(rp0.x + hPad, rp0.y), ImVec2(rp0.x + innerW - hPad, rp0.y), sepCol, 1.0f);
        }

        ImGui::InvisibleButton(row.id, ImVec2(innerW, rowH));
        bool hovered = ImGui::IsItemHovered();
        bool held    = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGuiID rid  = ImGui::GetID(row.id);
        float   hovT = AnimFloat(rid, 0x20u, hovered ? 1.0f : 0.0f, 18.0f);

        if (hovT > 0.005f) {
            ImVec4 hovBg = LerpSolid(kSurface2, kSurface3, hovT * 0.70f);
            dl->AddRectFilled(ImVec2(rp0.x, rp0.y + 2.0f), ImVec2(rp0.x + innerW, rp0.y + rowH - 2.0f), Col(hovBg), 8.0f);
        }

        // Etiqueta principal medida dinámicamente para cero solapamientos
        float textX = rp0.x + hPad;
        float textY = rp0.y + (rowH - lh) * 0.5f;
        dl->AddText(ImVec2(textX, textY), Col(kTextMain), row.name);

        float afterNameX = textX + ImGui::CalcTextSize(row.name).x + 10.0f;
        float badgeY     = rp0.y + (rowH - 20.0f) * 0.5f;

        if (row.isBgRow && !m_Config.sendBackground && !m_Config.transparentMode) {
            DrawSolidBadge(dl, ImVec2(afterNameX, badgeY), "Pantalla negra",
                kSurface, kBorderCol, kTextMuted);
        } else if (row.isTranspRow) {
            DrawSolidBadge(dl, ImVec2(afterNameX, badgeY), "PRO",
                LerpSolid(kSurface2, kAmber, 0.16f),
                LerpSolid(kSurface2, kAmber, 0.45f),
                kAmber);
        }

        // Interruptor moldeable alineado a la derecha
        ImVec2 swPos(rp0.x + innerW - hPad - 44.0f, rp0.y + (rowH - 24.0f) * 0.5f);
        DrawMoldableToggleVisual(dl, swPos, *row.val, hovered, held, rid);

        if (clicked) {
            *row.val = !*row.val;
            if (row.isTranspRow && *row.val) {
                m_Config.sendBackground = false;
            } else if (row.isBgRow && *row.val) {
                m_Config.transparentMode = false;
            }
            m_ConfigDirty = true;
        }
    }

    card.End();
}

// ── 4. Calidad Unificada (Tamaño, Fluidez y Nitidez en una sola tarjeta) ────
void StreamingPanel::RenderQualitySelector()
{
    float w = std::min(ImGui::GetContentRegionAvail().x, 740.0f);
    using VM = Core::StreamConfig::VideoMode;

    SectionTitleMinimal("CALIDAD");

    FlatCard card;
    card.Begin(w, Col(kSurface2), Col(kBorderCol), 20.0f, 16.0f);
    float innerW   = card.InnerWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float lh       = ImGui::GetTextLineHeight();

    // ── A. Tamaño ────────────────────────────────────────────────────────────
    static bool s_ManualRes = false;
    int resIdx = 3;
    if (!s_ManualRes) {
        if      (m_Config.frameWidth == 640  && m_Config.frameHeight == 360)  resIdx = 0;
        else if (m_Config.frameWidth == 1280 && m_Config.frameHeight == 720)  resIdx = 1;
        else if (m_Config.frameWidth == 1920 && m_Config.frameHeight == 1080) resIdx = 2;
        else s_ManualRes = true;
    }

    {
        ImVec2 hp = ImGui::GetCursorScreenPos();
        dl->AddText(hp, Col(kTextMain), "Tamaño");

        char dimLabel[32];
        std::snprintf(dimLabel, sizeof(dimLabel), "%d × %d", m_Config.frameWidth, m_Config.frameHeight);
        ImVec2 ds = ImGui::CalcTextSize(dimLabel);
        dl->AddText(ImVec2(hp.x + innerW - ds.x, hp.y), Col(kTextMuted), dimLabel);
        ImGui::Dummy(ImVec2(innerW, lh + 8.0f));
    }

    static const char* const kResLabels[4] = { "Básica", "Estándar", "Alta", "Manual" };
    if (DrawSegmentedControl("##resSeg", kResLabels, 4, &resIdx, innerW)) {
        if (resIdx == 0) { m_Config.frameWidth = 640;  m_Config.frameHeight = 360;  s_ManualRes = false; m_ConfigDirty = true; }
        if (resIdx == 1) { m_Config.frameWidth = 1280; m_Config.frameHeight = 720;  s_ManualRes = false; m_ConfigDirty = true; }
        if (resIdx == 2) { m_Config.frameWidth = 1920; m_Config.frameHeight = 1080; s_ManualRes = false; m_ConfigDirty = true; }
        if (resIdx == 3) { s_ManualRes = true; }
    }

    // Desplegable suave solo para tamaño manual
    float manualT = AnimFloat(ImGui::GetID("##manualResAnim"), 0x01u, s_ManualRes ? 1.0f : 0.0f, 15.0f);
    if (manualT > 0.005f) {
        const float subH    = 46.0f;
        const float curSubH = subH * manualT;
        ImVec2 sp0 = ImGui::GetCursorScreenPos();

        ImGui::PushClipRect(sp0, ImVec2(sp0.x + innerW, sp0.y + curSubH), true);

        const float inputH    = 34.0f;
        const float inputW    = 86.0f;
        const float framePadY = std::max(2.0f, (inputH - ImGui::GetFontSize()) * 0.5f);
        float iy = sp0.y + 12.0f;
        float cx = sp0.x;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border,  Col(kBorderCol));
        ImGui::PushStyleColor(ImGuiCol_Text,    Col(kTextMain));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, framePadY));

        ImVec2 wLbl = ImGui::CalcTextSize("Ancho");
        dl->AddText(ImVec2(cx, iy + (inputH - wLbl.y) * 0.5f), Col(kTextMuted), "Ancho");
        cx += wLbl.x + 8.0f;

        ImGui::SetCursorScreenPos(ImVec2(cx, iy));
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputInt("##customW", &m_Config.frameWidth, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            m_Config.frameWidth = std::clamp(m_Config.frameWidth, 320, 1920);
            m_ConfigDirty = true;
        }
        cx += inputW + 14.0f;

        ImVec2 xSep = ImGui::CalcTextSize("×");
        dl->AddText(ImVec2(cx, iy + (inputH - xSep.y) * 0.5f), Col(kTextMuted), "×");
        cx += xSep.x + 14.0f;

        ImVec2 hLbl = ImGui::CalcTextSize("Alto");
        dl->AddText(ImVec2(cx, iy + (inputH - hLbl.y) * 0.5f), Col(kTextMuted), "Alto");
        cx += hLbl.x + 8.0f;

        ImGui::SetCursorScreenPos(ImVec2(cx, iy));
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputInt("##customH", &m_Config.frameHeight, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            m_Config.frameHeight = std::clamp(m_Config.frameHeight, 180, 1080);
            m_ConfigDirty = true;
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);
        ImGui::PopClipRect();

        ImGui::SetCursorScreenPos(ImVec2(sp0.x, sp0.y + curSubH));
        ImGui::Dummy(ImVec2(innerW, 0.0f));
    }

    // Separador limpio
    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    {
        ImVec2 dp = ImGui::GetCursorScreenPos();
        dl->AddLine(dp, ImVec2(dp.x + innerW, dp.y), Col(LerpSolid(kSurface2, kBorderCol, 0.65f)), 1.0f);
    }
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // ── B. Fluidez ───────────────────────────────────────────────────────────
    {
        ImVec2 hp = ImGui::GetCursorScreenPos();
        dl->AddText(hp, Col(kTextMain), "Fluidez");
        ImGui::Dummy(ImVec2(innerW, lh + 8.0f));
    }

    int modeIdx = 1;
    if      (m_Config.videoMode == VM::LowLatency)  modeIdx = 0;
    else if (m_Config.videoMode == VM::HighQuality) modeIdx = 1;
    else if (m_Config.videoMode == VM::UltraStable) modeIdx = 2;

    static const char* const kModeLabels[3] = { "Ahorro", "Normal", "Máxima" };
    if (DrawSegmentedControl("##modeSeg", kModeLabels, 3, &modeIdx, innerW)) {
        if (modeIdx == 0) m_Config.videoMode = VM::LowLatency;
        if (modeIdx == 1) m_Config.videoMode = VM::HighQuality;
        if (modeIdx == 2) {
            m_Config.videoMode = VM::UltraStable;
            if (m_Config.jpegQuality < 92) m_Config.jpegQuality = 95;
        }
        m_ConfigDirty = true;
    }

    // Desplegable suave de FPS solo cuando está en fluidez Máxima
    float fpsT = AnimFloat(ImGui::GetID("##fpsSubAnim"), 0x01u, (m_Config.videoMode == VM::UltraStable) ? 1.0f : 0.0f, 15.0f);
    if (fpsT > 0.005f) {
        const float subH    = 44.0f;
        const float curSubH = subH * fpsT;
        ImVec2 fp0 = ImGui::GetCursorScreenPos();

        ImGui::PushClipRect(fp0, ImVec2(fp0.x + innerW, fp0.y + curSubH), true);

        const float pillW = std::min(168.0f, innerW * 0.55f);
        const float pillH = 32.0f;
        float py = fp0.y + 12.0f;

        dl->AddText(ImVec2(fp0.x, py + (pillH - lh) * 0.5f), Col(kTextMuted), "Velocidad");

        int fpsIdx = (m_Config.targetFPS >= 60) ? 1 : 0;
        static const char* const kFpsLabels[2] = { "30 FPS", "60 FPS" };
        ImGui::SetCursorScreenPos(ImVec2(fp0.x + innerW - pillW, py));
        if (DrawSegmentedControl("##fpsSeg", kFpsLabels, 2, &fpsIdx, pillW, pillH)) {
            m_Config.targetFPS = (fpsIdx == 1) ? 60 : 30;
            m_ConfigDirty = true;
        }

        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(fp0.x, fp0.y + curSubH));
        ImGui::Dummy(ImVec2(innerW, 0.0f));
    }

    // Separador limpio
    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    {
        ImVec2 dp = ImGui::GetCursorScreenPos();
        dl->AddLine(dp, ImVec2(dp.x + innerW, dp.y), Col(LerpSolid(kSurface2, kBorderCol, 0.65f)), 1.0f);
    }
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // ── C. Calidad de imagen ─────────────────────────────────────────────────
    {
        ImVec2 hp = ImGui::GetCursorScreenPos();
        dl->AddText(hp, Col(kTextMain), "Calidad de imagen");

        char pctLabel[16];
        std::snprintf(pctLabel, sizeof(pctLabel), "%d%%", m_Config.jpegQuality);
        ImVec2 ps = ImGui::CalcTextSize(pctLabel);
        dl->AddText(ImVec2(hp.x + innerW - ps.x, hp.y), Col(kAccent), pctLabel);
        ImGui::Dummy(ImVec2(innerW, lh + 8.0f));
    }

    if (DrawMoldableSlider("##qualitySlider", &m_Config.jpegQuality, 20, 100, innerW)) {
        m_ConfigDirty = true;
    }

    card.End();
}

} // namespace ProyecThor::UI
