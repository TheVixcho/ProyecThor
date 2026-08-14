#include "StreamingPanel.h"
#include "backend/core/PresentationCore.h"
#include "SettingsManager.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#if __has_include("qrcodegen.hpp")
#   define PROYECTHOR_HAS_QRCODEGEN 1
#   include "qrcodegen.hpp"
#endif

namespace ProyecThor::UI {

// ── Paleta Mejorada (Estilo Cinemático/Premium) ───────────────────────────────
// kAccent/kGreen/kRed quedan fijos a proposito: identidad visual de esta
// seccion (azul), igual criterio que ViewToolsSettings::categoryColor.
// kSurface*/kGray* si se recalculan en SyncPalette() a partir del tema
// activo: eran fondos/texto fijos que quedaban negros sobre cualquier tema.
static constexpr ImVec4 kGreen      = { 0.15f, 0.85f, 0.45f, 1.0f };
static constexpr ImVec4 kRed        = { 0.90f, 0.25f, 0.30f, 1.0f };
static ImVec4 kGrayDim    = { 0.45f, 0.47f, 0.55f, 1.0f };
static ImVec4 kGrayText   = { 0.65f, 0.68f, 0.75f, 1.0f };
static constexpr ImVec4 kAccent     = { 0.25f, 0.55f, 1.00f, 1.0f };
static constexpr ImVec4 kAccentLow  = { 0.25f, 0.55f, 1.00f, 0.15f };
static ImVec4 kSurface    = { 0.05f, 0.06f, 0.08f, 1.0f };
static ImVec4 kSurface2   = { 0.10f, 0.11f, 0.15f, 1.0f };
static ImVec4 kSurface3   = { 0.13f, 0.15f, 0.20f, 1.0f };

static ImU32 Col(ImVec4 v)  { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 ColA(ImVec4 v, float a) {
    v.w = a; return ImGui::ColorConvertFloat4ToU32(v);
}

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
    kSurface3 = ImVec4(t.surface2[0], t.surface2[1], t.surface2[2], t.surface2[3]);
    kGrayText = ImVec4(t.textDim[0], t.textDim[1], t.textDim[2], t.textDim[3]);
    kGrayDim  = BlendOver(t.textPrimary, 0.35f, t.base);
}

// ── Efectos Visuales (Sombras y Gradientes) ───────────────────────────────────
static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 6.0f; i += 1.0f) {
        float alpha = 40.0f - (i * 6.0f);
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 4.0f), 
            ImVec2(p1.x + i, p1.y + i + 4.0f), 
            IM_COL32(0, 0, 0, (int)alpha), rounding + i);
    }
}

// ── CaptureAndPushFrame ───────────────────────────────────────────────────────
// La lectura de GPU (RenderProjectorToFBO, via PBO doble) es barata y se
// queda en el hilo de render. El encode JPEG se delega al FrameEncodeWorker
// (hilo dedicado) para que no bloquee ese mismo hilo — ver FrameEncodeWorker.h.
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

// ── DrawQR ────────────────────────────────────────────────────────────────────
void StreamingPanel::DrawQR(ImDrawList* dl, ImVec2 origin, float size)
{
    if (m_QRSize <= 0) return;

    DrawSoftShadow(dl, origin, ImVec2(origin.x + size, origin.y + size), 14.0f);

    dl->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32(250, 250, 252, 255), 14.0f);
    dl->AddRect(origin, ImVec2(origin.x + size, origin.y + size), Col(kAccentLow), 14.0f, 0, 2.0f);

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

// ── Helpers de layout ─────────────────────────────────────────────────────────
static void SectionDivider(const char* label)
{
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

// Progreso animado (0..1), mismo patron de ImGuiStorage+lerp que el resto
// de la pasada de modernizacion (ver CategoryTheme::AnimT / BroadcastPanel::
// BroadcastAnimT) -- cada archivo tiene su propia copia chica por vivir en
// unidades de traduccion separadas.
static float StreamAnimT(ImGuiID id, ImU32 salt, bool target, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(id ^ salt, target ? 1.0f : 0.0f);
    float dst = target ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// Interruptor tipo "toggle" (pastilla + circulo animado) en vez del
// checkbox cuadrado por defecto de ImGui -- pedido explicito de "mejores
// botones", mismo lenguaje visual que un toggle moderno.
static bool DrawToggleSwitch(const char* id, bool value) {
    ImGui::PushID(id);
    ImVec2 size(38.0f, 20.0f);
    ImGui::InvisibleButton("##t", size);
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    ImGuiID gid = ImGui::GetID("##t");
    float   onT = StreamAnimT(gid, 0xA1u, value, 10.0f);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 trackCol(
        kSurface3.x + (kAccent.x - kSurface3.x) * onT,
        kSurface3.y + (kAccent.y - kSurface3.y) * onT,
        kSurface3.z + (kAccent.z - kSurface3.z) * onT,
        hovered ? 1.0f : 0.9f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(trackCol), size.y * 0.5f);
    // Borde siempre visible -- sin esto, en estado "apagado" la pastilla
    // (kSurface3) se confundia con el fondo de la tarjeta que la contiene
    // (kSurface2, un tono muy parecido) y quedaba leyendose como un
    // circulo suelto en vez de un interruptor.
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, onT > 0.5f ? 0 : 35), size.y * 0.5f, 0, 1.2f);

    float knobR = size.y * 0.5f - 2.0f;
    float knobX = p0.x + size.y * 0.5f + (size.x - size.y) * onT;
    float knobY = (p0.y + p1.y) * 0.5f;
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));

    ImGui::PopID();
    return clicked;
}
// Frecuencia de CAPTURA deseada segun el modo activo. Debe calzar con el
// ritmo al que realmente se va a enviar, para no gastar CPU comprimiendo
// frames que nunca se transmiten a tiempo (o que quedan obsoletos antes
// de salir por /stream), y para que el modo Ultra reciba un frame nuevo
// justo cuando el pacing del servidor lo necesita.
static int DesiredCaptureFPS(const Core::StreamConfig& cfg)
{
    switch (cfg.videoMode) {
        case Core::StreamConfig::VideoMode::UltraStable:
            return std::clamp(cfg.targetFPS, 24, 60);
        case Core::StreamConfig::VideoMode::HighQuality:
            return 30;
        case Core::StreamConfig::VideoMode::LowLatency:
        default:
            // El cliente solo pollea /frame cada ~150-500ms; capturar mas
            // rapido que eso es trabajo tirado.
            return 8;
    }
}

void StreamingPanel::Update()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    if (state.isStreamingNet && m_Config.sendBackground) {
        double now      = ImGui::GetTime();
        int    fps      = DesiredCaptureFPS(m_Config);
        double interval = 1.0 / static_cast<double>(fps);

        if (now - m_LastCaptureTime >= interval) {
            m_LastCaptureTime = now;
            CaptureAndPushFrame(m_Config.frameWidth, m_Config.frameHeight, m_Config.jpegQuality);
        }
    }

    if (state.isStreamingNet)
        RebuildQRTexture(state.networkURL);
    else if (!m_QRCachedURL.empty())
        RebuildQRTexture("");
}

void StreamingPanel::RenderContent()
{
    SyncPalette();
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    // Sin BeginChild propio a proposito: RenderContent() solo se llama desde
    // dentro del area ya scrolleable de Ajustes > Conexiones (ver
    // CategoryConnections.cpp) -- un child scrolleable anidado aca adentro
    // producia un doble scrollbar (uno de este child, otro del contenedor
    // de Ajustes) y le recortaba altura disponible a las tarjetas de abajo.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

    RenderServerControl();
    RenderLayerSelector();
    RenderQualitySelector();

    if (state.isStreamingNet) {
        RenderURLSection();
    }

    ImGui::Dummy(ImVec2(0.0f, 20.0f)); // Espacio final respiratorio
    ImGui::PopStyleVar();
}

// ── RenderServerControl ───────────────────────────────────────────────────────
void StreamingPanel::RenderServerControl()
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();
    bool  on    = state.isStreamingNet;
    float w     = ImGui::GetContentRegionAvail().x;
    float t     = static_cast<float>(ImGui::GetTime());

    // Capturamos la base del layout local actual
    float startLocalY = ImGui::GetCursorPosY();
    float cardH = 75.0f; 

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p0 = ImGui::GetCursorScreenPos();
    ImVec2      p1 = ImVec2(p0.x + w, p0.y + cardH);

    DrawSoftShadow(dl, p0, p1, 10.0f);

    ImU32 bg  = on ? ColA(kGreen, 0.16f) : Col(kSurface2);
    ImU32 bdr = on ? ColA(kGreen, 0.31f) : ColA(kGrayText, 0.15f);
    dl->AddRectFilled(p0, p1, bg, 10.0f);
    dl->AddRect(p0, p1, bdr, 10.0f, 0, 1.5f);

    if (on) {
        float pulse = 0.4f + 0.6f * std::sin(t * 2.5f);
        dl->AddRectFilled(p0, ImVec2(p0.x + 4.0f, p0.y + cardH),
            ColA(kGreen, pulse), 10.0f, ImDrawFlags_RoundCornersLeft);
    }

    // Dibujamos textos internos respetando el flujo sin saltar a posiciones absolutas rotas
    float innerX  = 20.0f;
    float labelY  = (cardH - ImGui::GetTextLineHeight() * 2.0f - 6.0f) * 0.5f;

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY),
        on ? Col(kGreen) : Col(kGrayText),
        on ? "TRANSMITIENDO" : "SERVIDOR DETENIDO");

    if (on) {
        float dotPulse = 0.6f + 0.4f * std::sin(t * 4.0f);
        dl->AddCircleFilled(ImVec2(p0.x + innerX - 10.0f, p0.y + labelY + 7.0f), 3.5f, ColA(kGreen, dotPulse));
    }

    dl->AddText(ImVec2(p0.x + innerX, p0.y + labelY + ImGui::GetTextLineHeight() + 6.0f),
        on ? ColA(kGreen, 0.8f) : ColA(kGrayDim, 0.8f),
        on ? (std::string("Puerto local ") + std::to_string(m_Port) + " abierto").c_str()
           : "Configura el puerto y presiona Iniciar");

    // Input de puerto perfectamente alineado usando coordenadas Locales controladas
    float portW = 80.0f;
    ImGui::SetCursorPos(ImVec2(w - portW - 16.0f, startLocalY + (cardH - 28.0f) * 0.5f));
    ImGui::SetNextItemWidth(portW);
    
    ImGui::BeginDisabled(on);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::InputInt("##port", &m_Port, 0, 0);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    m_Port = std::max(1024, std::min(65535, m_Port));

    // Forzamos el avance limpio del cursor al final exacto de la tarjeta de estado
    ImGui::SetCursorPosY(startLocalY + cardH);
    ImGui::Spacing();

    // ── Botón ON/OFF ─────────────────────────────────────────────────────
    // InvisibleButton + hover animado en vez de ImGui::Button con un solo
    // color plano -- mismo criterio ya aplicado en BroadcastPanel/OSCPanel,
    // para que el boton principal de esta pagina se sienta tan "vivo" como
    // el resto de Conexiones ya modernizado.
    {
        ImVec4 mainCol = on ? kRed : kGreen;
        const char* mainLabel = on ? "DETENER TRANSMISIÓN" : "INICIAR TRANSMISIÓN";

        ImGui::PushID("##serverToggleBtn");
        ImVec2 btnSize(w, 40.0f);
        ImGui::InvisibleButton("##b", btnSize);
        bool hoveredBtn = ImGui::IsItemHovered();
        bool clickedBtn = ImGui::IsItemClicked();

        ImGuiID bid    = ImGui::GetID("##b");
        float   hoverT = StreamAnimT(bid, 0xB3u, hoveredBtn, 14.0f);

        ImVec2 bp0 = ImGui::GetItemRectMin();
        ImVec2 bp1 = ImGui::GetItemRectMax();
        dl->AddRectFilled(bp0, bp1, ColA(mainCol, 0.15f + hoverT * 0.10f), 10.0f);
        dl->AddRect(bp0, bp1, ColA(mainCol, 0.5f + hoverT * 0.25f), 10.0f, 0, 1.5f);

        ImVec2 lts = ImGui::CalcTextSize(mainLabel);
        dl->AddText({ (bp0.x + bp1.x - lts.x) * 0.5f, (bp0.y + bp1.y - lts.y) * 0.5f },
            Col(mainCol), mainLabel);
        ImGui::PopID();

        if (clickedBtn) {
            if (!on) core.ToggleNetworkStream(true, m_Port);
            else     core.ToggleNetworkStream(false);
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
}

// ── RenderLayerSelector ───────────────────────────────────────────────────────
void StreamingPanel::RenderLayerSelector()
{
    auto& core = Core::PresentationCore::Get();
    bool  on   = core.IsStreamingNet();
    float w    = ImGui::GetContentRegionAvail().x;

    SectionDivider("CAPAS A TRANSMITIR");

    bool dirty = false;
    struct Row { const char* id; const char* name; const char* desc; bool* val; };
    Row rows[] = {
        { "##cbg", "Fondo",   "Color sólido o cámara base", &m_Config.sendBackground },
        { "##ctx", "Textos",  "Letras, Biblia y canciones", &m_Config.sendText       },
        { "##cov", "Overlay", "Gráficos e imágenes superpuestas", &m_Config.sendOverlay    },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rowH  = 44.0f;
    float totalRowsH = rowH * 3.0f;

    ImVec2 groupP0 = ImGui::GetCursorScreenPos();
    DrawSoftShadow(dl, groupP0, ImVec2(groupP0.x + w, groupP0.y + totalRowsH), 12.0f);

    // Guardamos la base del eje Y local de las filas
    float startRowsLocalY = ImGui::GetCursorPosY();

    // Columna de nombre a ancho FIJO (antes la descripcion arrancaba justo
    // despues del nombre via SameLine, asi que "Fondo"/"Textos"/"Overlay"
    // -- todos de largo distinto -- dejaban la columna de descripciones
    // despareja entre filas. Ahora el nombre vive en una columna de ancho
    // constante y la descripcion siempre arranca en la misma X, y el
    // control pasa al toggle switch a la derecha (mismo layout "etiqueta
    // izquierda / control derecha" del resto de la app).
    const float nameColX = 16.0f;
    const float nameColW = 90.0f;
    const float descColX = nameColX + nameColW + 8.0f;
    const float toggleX  = w - 38.0f - 16.0f;

    for (int i = 0; i < 3; ++i) {
        auto& row   = rows[i];
        float currentLocalY = startRowsLocalY + (i * rowH);

        ImVec2 p0 = ImVec2(groupP0.x, groupP0.y + (i * rowH));
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + rowH);

        ImDrawFlags corners = (i == 0) ? ImDrawFlags_RoundCornersTop :
                              (i == 2) ? ImDrawFlags_RoundCornersBottom :
                                         ImDrawFlags_RoundCornersNone;

        dl->AddRectFilled(p0, p1, Col(kSurface2), 12.0f, corners);

        if (i < 2)
            dl->AddLine(ImVec2(p0.x + 16.0f, p1.y), ImVec2(p1.x - 16.0f, p1.y), IM_COL32(255, 255, 255, 10), 1.0f);

        float textY = currentLocalY + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
        ImGui::SetCursorPos(ImVec2(nameColX, textY));
        ImGui::TextUnformatted(row.name);

        ImGui::SetCursorPos(ImVec2(descColX, textY));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (toggleX - descColX - 16.0f));
        ImGui::TextColored(kGrayDim, "%s", row.desc);
        ImGui::PopTextWrapPos();

        ImGui::SetCursorPos(ImVec2(toggleX, currentLocalY + (rowH - 20.0f) * 0.5f));
        char toggleId[16];
        snprintf(toggleId, sizeof(toggleId), "##tg%d", i);
        if (DrawToggleSwitch(toggleId, *row.val)) { *row.val = !*row.val; dirty = true; }
    }

    // Avanzamos el cursor de forma segura saltándonos las 3 filas
    ImGui::SetCursorPosY(startRowsLocalY + totalRowsH);

    if (dirty) m_ConfigDirty = true;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("RESOLUCIÓN Y CALIDAD");

    // ── Contenedor de resolución ─────────────────────────────────────────
    // Reformulado de raiz: las dos versiones anteriores calculaban la
    // altura de la tarjeta A MANO (sumando alturas/gaps estimados) y
    // posicionaban cada fila con SetCursorPos absoluto -- cualquier
    // numerito que no calzara exacto con lo que ImGui realmente dibujaba
    // dejaba huecos o textos pegados/flotando. Ahora el contenido fluye
    // solo dentro de un BeginChild con AutoResizeY: el alto lo calcula
    // ImGui a partir de lo que efectivamente se dibuja, no una estimacion
    // nuestra. Definicion visual via borde + fondo propio del child (en
    // vez de la sombra externa que usan las otras tarjetas de esta pagina)
    // -- un child anidado es OTRA ventana de ImGui con su propia
    // ImDrawList, así que el truco de ChannelsSplit para meter una sombra
    // "detrás" (que sí funciona con dibujado plano, ver la pildora de
    // seleccion del sidebar de Ajustes) no aplica de forma confiable acá.
    static bool s_ShowCustomRes = false;
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col(kSurface2));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 14));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::BeginChild("##resCard", ImVec2(w, 0.0f),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        float innerW = ImGui::GetContentRegionAvail().x;

        ImGui::TextColored(kGrayText, "Tamaño");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        struct Preset { const char* label; int w; int h; };
        static const Preset presets[] = {
            { "Básica",   640,  360  },
            { "Estándar", 1280, 720  },
            { "Alta",     1920, 1080 },
        };
        float presetGap = 8.0f;
        float presetW   = (innerW - presetGap * 2.0f) / 3.0f;

        for (int i = 0; i < 3; i++) {
            const auto& pr = presets[i];
            bool active = (m_Config.frameWidth == pr.w && m_Config.frameHeight == pr.h);

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, active ? ColA(kAccent, 0.30f) : Col(kSurface));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.40f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColA(kAccent, 0.50f));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? kAccent : kGrayText);
            ImGui::PushStyleColor(ImGuiCol_Border, active ? ColA(kAccent, 0.6f) : IM_COL32(255, 255, 255, 20));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            if (ImGui::Button(pr.label, ImVec2(presetW, 30.0f))) {
                m_Config.frameWidth  = pr.w;
                m_Config.frameHeight = pr.h;
                m_ConfigDirty = true;
                s_ShowCustomRes = false;
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(5);
            ImGui::PopID();
            if (i < 2) ImGui::SameLine(0.0f, presetGap);
        }

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Fila "Personalizado": texto a la izquierda, toggle pegado al
        // borde derecho -- ambos sobre la MISMA linea logica (SameLine con
        // X absoluto), altura de fila = la mas alta de las dos (el toggle,
        // 20px), asi ninguno queda flotando respecto del otro.
        {
            float rowH = std::max(ImGui::GetTextLineHeight(), 20.0f);
            float rowY = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(rowY + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextColored(kGrayDim, "Personalizado (ancho/alto exactos)");
            ImGui::SameLine(innerW - 38.0f);
            ImGui::SetCursorPosY(rowY + (rowH - 20.0f) * 0.5f);
            if (DrawToggleSwitch("##customRes", s_ShowCustomRes)) s_ShowCustomRes = !s_ShowCustomRes;
            ImGui::SetCursorPosY(rowY + rowH);
        }

        if (s_ShowCustomRes) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            float half = (innerW - 16.0f) * 0.5f;

            ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kGrayDim, "Ancho");
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetNextItemWidth(half - 45.0f);
            ImGui::InputInt("##rw", &m_Config.frameWidth, 0, 0);

            ImGui::SameLine(0.0f, 16.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kGrayDim, "Alto");
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetNextItemWidth(half - 45.0f);
            ImGui::InputInt("##rh", &m_Config.frameHeight, 0, 0);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(1);

            m_Config.frameWidth  = std::max(320,  std::min(1920, m_Config.frameWidth));
            m_Config.frameHeight = std::max(180,  std::min(1080, m_Config.frameHeight));
        }

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Calidad de imagen (antes "Compresión JPEG" -- termino tecnico
        // que no dice nada a alguien que no sabe de video).
        ImGui::TextColored(kGrayText, "Calidad de imagen:");
        ImGui::SameLine();
        ImGui::TextColored(kAccent, "%d%%", m_Config.jpegQuality);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ColA(kAccent, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::SetNextItemWidth(innerW);

        if (ImGui::SliderInt("##q", &m_Config.jpegQuality, 20, 100, ""))
            m_ConfigDirty = true;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    if (m_ConfigDirty && on) {
        core.GetNetworkServer()->SetConfig(m_Config);
        m_ConfigDirty = false;
    }
}

void StreamingPanel::RenderQualitySelector()
{
    auto& core = Core::PresentationCore::Get();
    bool  on   = core.IsStreamingNet();
    float w    = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("MODO DE TRANSMISIÓN");

    using VM = Core::StreamConfig::VideoMode;

    struct Card {
        const char* id; const char* title; const char* sub1; const char* sub2;
        bool active; VM mode;
    } cards[3] = {
        { "##ll", "Bajo Consumo", "~150 ms de retraso",
          "Para redes o celulares lentos",
          m_Config.videoMode == VM::LowLatency,  VM::LowLatency  },
        { "##hq", "Alta Calidad", "< 33 ms de retraso",
          "Fluido -- recomendado",
          m_Config.videoMode == VM::HighQuality, VM::HighQuality },
        { "##us", "Ultra Estable", "Mas retraso, cero cortes",
          "Ideal para pantallas grandes",
          m_Config.videoMode == VM::UltraStable, VM::UltraStable },
    };

    float gap   = 10.0f;
    float cardW = (w - gap * 2.0f) / 3.0f;
    float cardH = 90.0f;

    float startSelectorLocalY = ImGui::GetCursorPosY();
    ImVec2 baseScreenPos = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 3; ++i) {
        auto& c = cards[i];
        float localX = i * (cardW + gap);

        ImVec2 p0 = ImVec2(baseScreenPos.x + localX, baseScreenPos.y);
        ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSoftShadow(dl, p0, p1, 12.0f);

        ImU32 bg  = c.active ? Col(kSurface3) : Col(kSurface2);
        ImU32 bdr = c.active ? ColA(kAccent, 0.6f) : IM_COL32(255, 255, 255, 10);

        dl->AddRectFilled(p0, p1, bg, 12.0f);
        dl->AddRect(p0, p1, bdr, 12.0f, 0, c.active ? 2.0f : 1.0f);

        if (c.active) {
            dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, p0.y + 20.0f),
                ColA(kAccent, 0.15f), ColA(kAccent, 0.15f), ColA(kAccent, 0.0f), ColA(kAccent, 0.0f));
            dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + 4.0f), Col(kAccent), 12.0f, ImDrawFlags_RoundCornersTop);
        }

        ImGui::SetCursorPos(ImVec2(localX, startSelectorLocalY));
        ImGui::InvisibleButton(c.id, ImVec2(cardW, cardH));

        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (hovered && !c.active)
            dl->AddRectFilled(p0, p1, ColA(kAccent, 0.05f), 12.0f);

        float lh = ImGui::GetTextLineHeight();
        float py = p0.y + 12.0f;
        float px = p0.x + 12.0f;

        dl->AddText(ImVec2(px, py), c.active ? Col(kAccent) : Col(kGrayText), c.title);
        py += lh + 5.0f;
        dl->AddText(ImVec2(px, py), Col(kGrayDim), c.sub1);
        py += lh + 3.0f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.82f, ImVec2(px, py),
            ColA(kGrayDim, 0.7f), c.sub2, nullptr, cardW - 16.0f);

        if (!c.active && clicked) {
            m_Config.videoMode = c.mode;
            // Al entrar a Ultra, subimos la calidad por defecto a un piso
            // alto (el usuario puede bajarla despues si su red no aguanta).
            if (c.mode == VM::UltraStable && m_Config.jpegQuality < 92)
                m_Config.jpegQuality = 95;
            m_ConfigDirty = true;
        }
    }

    ImGui::SetCursorPosY(startSelectorLocalY + cardH);

    // ── Selector de FPS, solo visible/relevante en modo Ultra ─────────────
    if (m_Config.videoMode == VM::UltraStable) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(kGrayText, "Cuadros por segundo:");
        ImGui::SameLine(0.0f, 10.0f);

        bool is30 = (m_Config.targetFPS == 30);
        bool is60 = (m_Config.targetFPS == 60);

        auto fpsButton = [&](const char* label, int fps, bool active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ColA(kAccent, 0.35f) : Col(kSurface2));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColA(kAccent, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? kAccent : kGrayText);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            bool clicked = ImGui::Button(label, ImVec2(64.0f, 28.0f));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (clicked && m_Config.targetFPS != fps) {
                m_Config.targetFPS = fps;
                m_ConfigDirty = true;
            }
        };

        fpsButton("30 FPS", 30, is30);
        ImGui::SameLine(0.0f, 6.0f);
        fpsButton("60 FPS", 60, is60);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(kGrayDim, 0.85f));
        ImGui::TextWrapped(
            "Prioriza fluidez perfecta sobre latencia: el servidor envia a "
            "ritmo fijo y con calidad alta. Recomendado con JPEG en 90%% o mas "
            "y red WiFi estable — a 60 FPS + calidad alta el consumo de ancho "
            "de banda es considerablemente mayor.");
        ImGui::PopStyleColor();
    }

    if (m_ConfigDirty && on) {
        core.GetNetworkServer()->SetConfig(m_Config);
        m_ConfigDirty = false;
    }
}

// ── RenderURLSection ──────────────────────────────────────────────────────────
void StreamingPanel::RenderURLSection()
{
    auto  state = Core::PresentationCore::Get().GetState();
    float w     = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    SectionDivider("ACCESO DISPOSITIVOS MÓVILES");

    // ── QR ────────────────────────────────────────────────────────────────
    {
        float qrSize = std::min(w * 0.75f, 220.0f);
        float cardPad = 16.0f;
        float cardW   = qrSize + cardPad * 2.0f;
        float cardH   = qrSize + cardPad * 2.0f;
        float cardOffX = (w - cardW) * 0.5f;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 cp0 = ImVec2(p0.x + cardOffX, p0.y);
        ImVec2 cp1 = ImVec2(cp0.x + cardW, cp0.y + cardH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSoftShadow(dl, cp0, cp1, 16.0f);

        dl->AddRectFilled(cp0, cp1, Col(kSurface2), 16.0f);
        dl->AddRect(cp0, cp1, IM_COL32(255, 255, 255, 12), 16.0f, 0, 1.0f);

        DrawQR(dl, ImVec2(cp0.x + cardPad, cp0.y + cardPad), qrSize);

        ImGui::Dummy(ImVec2(w, cardH + 10.0f));
    }

    // ── URL copiable ──────────────────────────────────────────────────────
    {
        float btnW = 90.0f;
        float gap  = 8.0f;
        float fieldW = w - btnW - gap;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col(kSurface));
        ImGui::PushStyleColor(ImGuiCol_Border, ColA(kAccent, 0.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        char urlBuf[256];
        std::strncpy(urlBuf, state.networkURL.c_str(), sizeof(urlBuf) - 1);
        urlBuf[sizeof(urlBuf) - 1] = '\0';

        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputText("##url", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_ReadOnly);
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
            ImGui::SetClipboardText(state.networkURL.c_str());

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    }

    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // Hint
    {
        const char* hint = "Abre esta URL desde la cámara de tu teléfono";
        const char* hint2 = "estando en la misma red WiFi local.";
        ImVec2 ts1 = ImGui::CalcTextSize(hint);
        ImVec2 ts2 = ImGui::CalcTextSize(hint2);
        
        ImGui::SetCursorPosX((w - ts1.x) * 0.5f);
        ImGui::TextUnformatted(hint);
        
        ImGui::SetCursorPosX((w - ts2.x) * 0.5f);
        ImGui::TextColored(kGrayDim, "%s", hint2);
    }
}

} // namespace ProyecThor::UI