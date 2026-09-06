#include "OSCPanel.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/OSCSender.h"
#include "backend/settings/SettingsManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>

namespace ProyecThor::UI {

// ── InputText atado a std::string ───────────────────────────────────────────
// Misma tecnica que SongEditView.cpp::InputTextStd (imgui_stdlib.h no esta
// vendorizado en este proyecto): via ImGuiInputTextFlags_CallbackResize en
// vez de un buffer char[] fijo.
namespace {
struct StdStringCbData { std::string* str; };

int StdStringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* cb = static_cast<StdStringCbData*>(data->UserData);
        std::string* str = cb->str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextStd(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    StdStringCbData cb{ str };
    return ImGui::InputText(label, str->data(), str->capacity() + 1, flags, StdStringResizeCallback, &cb);
}

// Encabezado de seccion con degradado + etiqueta, mismo criterio visual que
// CategoryConnections::SubDivider / StreamingPanel::SectionDivider -- acento
// del tema en vez del ImGui::SeparatorText() por defecto (linea plana gris,
// desentonaba contra el resto de Conexiones ya modernizado).
void OSCSectionHeader(const char* label) {
    const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    ImVec2      ts  = ImGui::CalcTextSize(label);
    float       cy  = pos.y + ts.y * 0.5f;

    ImVec4 accent(theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
    ImU32  colSolid = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.65f));
    ImU32  colFade  = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
    ImVec4 textDimV(theme.textDim[0], theme.textDim[1], theme.textDim[2], theme.textDim[3]);

    dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.95f)), label);
    float tailX = pos.x + ts.x + 12.0f;
    dl->AddRectFilledMultiColor({ tailX, cy }, { pos.x + w, cy + 1.5f }, colSolid, colFade, colFade, colFade);

    ImGui::Dummy(ImVec2(w, ts.y + 10.0f));
}

// Boton relleno con el color de acento del tema, animado al hover -- mismo
// patron InvisibleButton + lerp que el resto de la pasada de modernizacion
// (ver BroadcastPanel::BroadcastAnimT / CategoryTheme::AnimT).
bool OSCAccentButton(const char* id, const char* label, ImVec4 col, float w = 0.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImVec2 pad = ImGui::GetStyle().FramePadding;
    ImVec2 ts  = ImGui::CalcTextSize(label);
    ImVec2 size(w > 0.0f ? w : ts.x + pad.x * 2.0f + 8.0f, ImGui::GetFrameHeight());

    ImGui::PushID(id);
    ImGui::InvisibleButton("##b", size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    ImGuiID gid = ImGui::GetID("##b");
    float*  t   = storage->GetFloatRef(gid ^ 0x05u, hovered ? 1.0f : 0.0f);
    float   dst = hovered ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 fill(col.x, col.y, col.z, 0.72f + *t * 0.20f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(fill), 7.0f);
    dl->AddText({ (p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f },
        IM_COL32(255, 255, 255, 255), label);
    ImGui::PopID();
    return clicked;
}
} // namespace

OSCPanel::OSCPanel() {
    BuildParamRegistry();

    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
    if (y.autoListen) {
        std::string err;
        if (m_Receiver.Start(y.listenPort, &err))
            m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
        else
            m_ListenStatus = "Error al escuchar: " + err;
    }
}

OSCPanel::~OSCPanel() {
    m_Receiver.Stop();
}

// Registro de parametros en vivo que se pueden vincular a OSC. Por ahora
// cubre los efectos de Shaders (todos exponen Get/SetXxxIntensity o
// Get/SetXxxAmount en 0..1 sobre PresentationCore, ver ShadersPanel.cpp
// para el mismo patron usado por los sliders manuales). Overlays no tiene
// todavia parametros de opacidad/escala en vivo expuestos a nivel de
// PresentationCore -- sumarlos es un paso aparte (requiere tocar el
// compositor de overlays), no algo que se pueda enganchar aca sin riesgo.
void OSCPanel::BuildParamRegistry() {
    using Core::PresentationCore;

    m_Params = {
        { "Shaders > CRT (scanlines)",
          []{ return PresentationCore::Get().GetCRTScanlineIntensity(); },
          [](float v){ PresentationCore::Get().SetCRTScanlineIntensity(v); } },

        { "Shaders > Grano de pelicula",
          []{ return PresentationCore::Get().GetGrainIntensity(); },
          [](float v){ PresentationCore::Get().SetGrainIntensity(v); } },

        { "Shaders > Saturación (color)",
          []{ return PresentationCore::Get().GetSaturationAmount(); },
          [](float v){ PresentationCore::Get().SetSaturationAmount(v); } },

        { "Shaders > Vinetado",
          []{ return PresentationCore::Get().GetVignetteIntensity(); },
          [](float v){ PresentationCore::Get().SetVignetteIntensity(v); } },

        { "Shaders > Desenfoque (Blur)",
          []{ return PresentationCore::Get().GetBlurIntensity(); },
          [](float v){ PresentationCore::Get().SetBlurIntensity(v); } },

        { "Shaders > Nitidez (Sharpen)",
          []{ return PresentationCore::Get().GetSharpenIntensity(); },
          [](float v){ PresentationCore::Get().SetSharpenIntensity(v); } },

        { "Shaders > Resplandor (Bloom)",
          []{ return PresentationCore::Get().GetBloomIntensity(); },
          [](float v){ PresentationCore::Get().SetBloomIntensity(v); } },

        { "Shaders > Aberración cromática",
          []{ return PresentationCore::Get().GetChromaticAberrationIntensity(); },
          [](float v){ PresentationCore::Get().SetChromaticAberrationIntensity(v); } },

        { "Shaders > VHS",
          []{ return PresentationCore::Get().GetVHSIntensity(); },
          [](float v){ PresentationCore::Get().SetVHSIntensity(v); } },

        { "Shaders > Cine",
          []{ return PresentationCore::Get().GetCineIntensity(); },
          [](float v){ PresentationCore::Get().SetCineIntensity(v); } },

        { "Shaders > Contraste",
          []{ return PresentationCore::Get().GetContrastAmount(); },
          [](float v){ PresentationCore::Get().SetContrastAmount(v); } },

        { "Shaders > Luminosidad",
          []{ return PresentationCore::Get().GetLuminosityAmount(); },
          [](float v){ PresentationCore::Get().SetLuminosityAmount(v); } },

        { "Shaders > TAA (antialiasing)",
          []{ return PresentationCore::Get().GetTAAIntensity(); },
          [](float v){ PresentationCore::Get().SetTAAIntensity(v); } },
    };
}

// Saca los mensajes acumulados del receptor y los aplica: en modo
// "Aprender" (m_LearningIndex >= 0), el PRIMER mensaje que llega se
// convierte en el binding de ese parametro; si no, cualquier mensaje cuya
// direccion coincida con un binding existente actualiza el parametro en
// vivo (primer argumento numerico, clamp 0..1 -- mismo rango que usan los
// sliders manuales de Shaders).
void OSCPanel::ApplyReceivedMessages() {
    if (!m_Receiver.IsListening()) return;

    m_DrainBuffer.clear();
    m_Receiver.DrainMessages(m_DrainBuffer);
    if (m_DrainBuffer.empty()) return;

    auto& bindings = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.bindings;

    for (const auto& msg : m_DrainBuffer) {
        if (m_LearningIndex >= 0 && m_LearningIndex < (int)m_Params.size()) {
            const std::string& paramName = m_Params[m_LearningIndex].name;

            auto it = std::find_if(bindings.begin(), bindings.end(),
                [&](const ProyecThor::Settings::OSCBinding& b){ return b.paramName == paramName; });
            if (it != bindings.end()) it->oscAddress = msg.address;
            else                      bindings.push_back({ paramName, msg.address });

            m_LearningIndex = -1;
            continue; // el mismo mensaje que se uso para aprender no dispara el valor
        }

        auto it = std::find_if(bindings.begin(), bindings.end(),
            [&](const ProyecThor::Settings::OSCBinding& b){ return b.oscAddress == msg.address; });
        if (it == bindings.end() || msg.args.empty()) continue;

        float value = 0.0f;
        bool  hasValue = true;
        switch (msg.args[0].type) {
            case Core::OSCArg::Type::Float:  value = msg.args[0].floatVal;             break;
            case Core::OSCArg::Type::Int:    value = (float)msg.args[0].intVal;        break;
            default:                         hasValue = false;                        break;
        }
        if (!hasValue) continue;
        value = std::clamp(value, 0.0f, 1.0f);

        auto pit = std::find_if(m_Params.begin(), m_Params.end(),
            [&](const BindableParam& p){ return p.name == it->paramName; });
        if (pit != m_Params.end()) pit->set(value);
    }
}

void OSCPanel::Update() {
    ApplyReceivedMessages();
}

void OSCPanel::RenderConnectionSection() {
    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
    const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    ImVec4 accent (theme.accent[0],  theme.accent[1],  theme.accent[2],  1.0f);
    ImVec4 success(theme.success[0], theme.success[1], theme.success[2], 1.0f);

    OSCSectionHeader("CONEXIÓN");
    ImGui::TextWrapped("Dirección a la que se envían los mensajes (luces). No hace falta "
                        "para recibir/Aprender, eso usa el puerto de escucha de abajo.");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::SetNextItemWidth(160.0f);
    InputTextStd("IP de destino", &y.targetIp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Puerto de destino", &y.targetPort, 0);
    y.targetPort = std::clamp(y.targetPort, 1, 65535);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("Puerto de escucha (recibir)", &y.listenPort, 0);
    y.listenPort = std::clamp(y.listenPort, 1, 65535);

    ImGui::SameLine(0.0f, 14.0f);
    bool listening = m_Receiver.IsListening();
    if (listening) {
        if (OSCAccentButton("##stopListen", "Detener escucha", ImVec4(0.30f, 0.32f, 0.40f, 1.0f))) {
            m_Receiver.Stop();
            m_ListenStatus = "Detenido.";
        }
    } else {
        if (OSCAccentButton("##startListen", "Iniciar escucha", accent)) {
            std::string err;
            if (m_Receiver.Start(y.listenPort, &err))
                m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
            else
                m_ListenStatus = "Error al escuchar: " + err;
        }
    }

    ImGui::Checkbox("Escuchar automáticamente al abrir ProyecThor", &y.autoListen);

    // Punto de estado con pulso mientras escucha -- mismo criterio que el
    // indicador LIVE de BroadcastPanel, en vez de solo un texto de color.
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        const char* msg = m_ListenStatus.empty() ? "Sin iniciar." : m_ListenStatus.c_str();
        ImVec4 statusCol = listening ? success : ImVec4(0.60f, 0.62f, 0.70f, 1.0f);

        if (listening) {
            float pulse = 0.55f + 0.45f * std::sin((float)ImGui::GetTime() * 3.0f);
            dl->AddCircleFilled({ pos.x + 6.0f, pos.y + ImGui::GetTextLineHeight() * 0.5f }, 4.5f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(statusCol.x, statusCol.y, statusCol.z, pulse)));
            ImGui::Dummy(ImVec2(16.0f, 0.0f));
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImGui::TextColored(statusCol, "%s", msg);
    }
    ImGui::Spacing();
}

void OSCPanel::RenderControlListSection() {
    auto& bindings = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.bindings;

    OSCSectionHeader("CONTROL LIST (RECIBIR + OSC LEARN)");
    ImGui::TextWrapped("Vincula un parámetro en vivo de ProyecThor a un mensaje OSC entrante: "
                        "apreta \"Aprender\", mové el fader/control externo, y queda vinculado.");
    ImGui::Spacing();

    if (!ImGui::BeginTable("##oscControlList", 4,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn("Parámetro",     ImGuiTableColumnFlags_WidthStretch, 0.34f);
    ImGui::TableSetupColumn("Valor",         ImGuiTableColumnFlags_WidthStretch, 0.20f);
    ImGui::TableSetupColumn("Dirección OSC", ImGuiTableColumnFlags_WidthStretch, 0.26f);
    ImGui::TableSetupColumn("Acción",        ImGuiTableColumnFlags_WidthStretch, 0.20f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)m_Params.size(); i++) {
        const auto& p = m_Params[i];
        ImGui::PushID(i);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(p.name.c_str());

        ImGui::TableSetColumnIndex(1);
        float value = p.get();
        ImGui::ProgressBar(value, ImVec2(-FLT_MIN, 0.0f));

        ImGui::TableSetColumnIndex(2);
        auto it = std::find_if(bindings.begin(), bindings.end(),
            [&](const ProyecThor::Settings::OSCBinding& b){ return b.paramName == p.name; });
        if (it != bindings.end()) ImGui::TextUnformatted(it->oscAddress.c_str());
        else                      ImGui::TextDisabled("Sin vincular");

        ImGui::TableSetColumnIndex(3);
        const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
        ImVec4 accent(theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
        if (m_LearningIndex == i) {
            if (OSCAccentButton("##cancelLearn", "Cancelar (esperando...)", ImVec4(0.75f, 0.20f, 0.20f, 1.0f)))
                m_LearningIndex = -1;
        } else {
            if (OSCAccentButton("##learn", "Aprender", accent)) {
                m_LearningIndex = i;
                if (!m_Receiver.IsListening()) {
                    auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
                    std::string err;
                    if (m_Receiver.Start(y.listenPort, &err))
                        m_ListenStatus = "Escuchando en el puerto " + std::to_string(y.listenPort);
                    else
                        m_ListenStatus = "Error al escuchar: " + err;
                }
            }
            if (it != bindings.end()) {
                ImGui::SameLine();
                if (OSCAccentButton("##unbind", "Quitar", ImVec4(0.30f, 0.32f, 0.40f, 1.0f))) bindings.erase(it);
            }
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
    ImGui::Spacing();
}

void OSCPanel::RenderSendSection() {
    auto& messages = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil.messages;
    const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    ImVec4 accent(theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);

    OSCSectionHeader("LUCES (ENVIAR)");
    ImGui::TextWrapped("Cada fila es una luz/cue disparable a mano. El punto de color muestra "
                        "si el último envío a esa luz funcionó.");
    ImGui::Spacing();

    if (OSCAccentButton("##addLight", "+ Agregar luz", accent)) {
        ProyecThor::Settings::OSCMessageDef m;
        m.label = "Luz " + std::to_string(messages.size() + 1);
        messages.push_back(m);
    }
    ImGui::Spacing();

    for (int i = 0; i < (int)messages.size(); i++) {
        auto& m = messages[i];
        ImGui::PushID(i + 1000);
        ImGui::BeginGroup();

        // Punto de estado: gris = nunca probado, verde = ultimo envio OK,
        // rojo = fallo -- para "trackear" de un vistazo cual luz es cual.
        ImVec4 dotCol = ImVec4(0.45f, 0.47f, 0.55f, 1.0f);
        if (!m.lastSentAt.empty())
            dotCol = m.lastSendOk ? ImVec4(0.30f, 0.86f, 0.48f, 1.0f) : ImVec4(0.90f, 0.30f, 0.30f, 1.0f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 dotPos = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(dotPos.x + 6.0f, dotPos.y + 10.0f), 5.0f, ImGui::ColorConvertFloat4ToU32(dotCol));
        ImGui::Dummy(ImVec2(16.0f, 1.0f));
        ImGui::SameLine();

        ImGui::SetNextItemWidth(140.0f);
        InputTextStd("##label", &m.label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        InputTextStd("Dirección##addr", &m.address);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        InputTextStd("Argumentos##args", &m.argsText);
        ImGui::SameLine();

        if (OSCAccentButton("##send", "Enviar", accent)) {
            auto& y = ProyecThor::Settings::SettingsManager::Get().GetSettings().yggdrasil;
            std::string err;
            bool ok = Core::SendOSCMessage(y.targetIp, y.targetPort, m.address,
                                            Core::ParseOSCArgs(m.argsText), &err);
            m.lastSendOk = ok;

            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timeBuf[16];
            std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&now));
            m.lastSentAt = timeBuf;
        }
        ImGui::SameLine();
        if (OSCAccentButton("##delLight", "Borrar", ImVec4(0.75f, 0.20f, 0.20f, 1.0f))) {
            messages.erase(messages.begin() + i);
            ImGui::EndGroup();
            ImGui::PopID();
            break; // el vector se resizeo -- no seguir iterando este frame
        }

        if (!m.lastSentAt.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s a las %s", m.lastSendOk ? "OK" : "Fallo", m.lastSentAt.c_str());
        }

        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void OSCPanel::RenderContent() {
    RenderConnectionSection();
    RenderControlListSection();
    RenderSendSection();
}

} // namespace ProyecThor::UI
