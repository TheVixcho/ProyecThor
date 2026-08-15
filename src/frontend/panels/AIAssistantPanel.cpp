#include "AIAssistantPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/panels/home/HomeIcons.h"
#include "external/tools/OpenURL.h"
#include "backend/core/AITools.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/ui/LoadingSpinner.h"

#include <imgui.h>
#include <cstring>

namespace ProyecThor::UI {

AIAssistantPanel::~AIAssistantPanel()
{
    if (m_Worker.joinable())
        m_Worker.join();
}

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

    bool showingBrowser = false;
    if (open) {
        RenderModeTabs();
        ImGui::Spacing();

        if (m_Mode == Mode::Basic) {
            if (m_Provider == Provider::None)
                RenderProviderPicker();
            else {
                RenderBrowserArea();
                showingBrowser = true;
            }
        } else {
            RenderAdvancedMode();
        }
    }

    DS::EndGlassPanel();

    // El WebView2 embebido solo debe estar visible en Basica + navegador
    // activo -- si se cerro la ventana, se cambio a Avanzada, o se volvio al
    // selector de proveedor, hay que esconderlo (RenderBrowserArea ya se
    // encarga del caso "adentro de si misma", ver sus propios returns).
    if (!*pShow || !showingBrowser)
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void AIAssistantPanel::RenderModeTabs()
{
    auto pill = [&](const char* label, Mode mode) {
        bool active = (m_Mode == mode);
        ImVec4 base = active ? ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim)
                              : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1, 1, 1, 1) : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        if (ImGui::Button(label, ImVec2(120.0f, 28.0f)) && m_Mode != mode) {
            m_Mode = mode;
            m_WebView.UpdateBounds(0, 0, 0, 0, false);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    };
    pill("Básica", Mode::Basic);
    ImGui::SameLine(0.0f, 6.0f);
    pill("Avanzada", Mode::Advanced);
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
        m_Mode == Mode::Basic ? "  -- chat web, iniciar sesion normal" : "  -- API propia, puede crear/editar canciones");
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
        //
        // FIX ("el panel de IA no muestra nada"): estos dos return de aca
        // abajo se olvidaban de esconder la ventana nativa del WebView2 --
        // si un frame anterior ya la habia mostrado (UpdateBounds(...,true))
        // y RECIEN DESPUES se detecto la falla (es async, ver NavigateTo),
        // esa ventana nativa (blanca, vacia) se quedaba pegada ARRIBA de
        // todo este texto para siempre, tapandolo. Se llama UpdateBounds con
        // visible=false ANTES de cada return de este bloque para que nunca
        // quede una ventana nativa huerfana tapando el panel.
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Se abrio en tu navegador externo (esta ventana no puede embeberlo aca).");
        if (DS::GlassButton("Volver a abrir", ImVec2(160.0f, 30.0f)))
            ProyecThor::External::OpenURL(ProviderURL(static_cast<int>(m_Provider)));
        return;
    }

    if (m_WebView.HasError()) {
        m_WebView.UpdateBounds(0, 0, 0, 0, false); // ver comentario arriba
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::DangerColor), "%s", m_WebView.GetLastError().c_str());
        return;
    }

    // El resto del panel queda vacio a proposito: el WebView2 (ventana nativa
    // hija) se dibuja el mismo, por encima de este rectangulo -- ver
    // UpdateBounds mas abajo, que lo posiciona exactamente sobre este child.
    // Mientras todavia no esta listo (arranque async, ver AIWebViewPanel::
    // NavigateTo) se deja un "Cargando..." VISIBLE en vez de nada -- antes
    // esta franja quedaba completamente vacia (solo negro) sin explicar que
    // el navegador seguia arrancando, indistinguible de "esto esta roto".
    ImGui::BeginChild("##aiWebArea", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool ready = m_WebView.IsReady();
    if (!ready) {
        ImVec2 avail  = ImGui::GetContentRegionAvail();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 center = { avail.x * 0.5f, avail.y * 0.42f };
        DrawLoadingSpinner(ImGui::GetWindowDrawList(), { origin.x + center.x, origin.y + center.y }, 18.0f);

        const char* msg = "Cargando el navegador...";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos({ center.x - ts.x * 0.5f, center.y + 30.0f });
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "%s", msg);
    }
    ImVec2 areaPos  = ImGui::GetWindowPos();
    ImVec2 areaSize = ImGui::GetWindowSize();
    ImGui::EndChild();

    // FIX ("selecciono mi IA y no carga nada"): esto antes llamaba
    // UpdateBounds(..., true) SIEMPRE, sin importar IsReady() -- la ventana
    // nativa (fondo negro solido, ver EnsureHostClassRegistered) se mostraba
    // y se traia al frente YA MISMO, tapando el "Cargando..." de arriba
    // (dibujado por ImGui, JUSTO DEBAJO en Z-order de esa ventana nativa)
    // desde el primer frame -- asi que en la practica ese mensaje nunca se
    // llegaba a ver, quedaba solo el rectangulo negro sin explicacion
    // ninguna. Ahora la ventana nativa solo se muestra/sube al frente una
    // vez que el controller+webview de verdad terminaron de crearse.
    if (ready) {
        m_WebView.UpdateBounds(static_cast<int>(areaPos.x), static_cast<int>(areaPos.y),
                                static_cast<int>(areaSize.x), static_cast<int>(areaSize.y), true);
    } else {
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Modo Avanzada -- chat directo contra la API de Claude, con tool-use para
//  revisar/crear/editar canciones (ver backend/core/AITools.h). El pedido
//  HTTP corre en un hilo aparte (ClaudeClient::SendRequest es sincronico a
//  proposito, ver su comentario) -- nunca se llama desde este hilo de
//  render. El resultado se entrega via m_WorkerResult protegido por mutex y
//  se consume una sola vez por turno, mismo patron que
//  UIManager::RenderUrlImportModal.
// ─────────────────────────────────────────────────────────────────────────────

static const char* kSystemPrompt =
    "Sos el asistente de IA integrado en ProyecThor, un software de presentacion "
    "de letras para iglesias. Con las herramientas disponibles podes listar, leer, "
    "crear y editar canciones de la Biblioteca del usuario. Las canciones son "
    "texto plano: cada estrofa/diapositiva va separada por una linea en blanco. "
    "Cuando el usuario pida modificar solo una parte de una cancion existente, "
    "primero leé la cancion completa con read_song y mandale a edit_song el "
    "texto COMPLETO ya modificado (edit_song siempre reemplaza todo el contenido, "
    "no solo un fragmento). Responde siempre en español, de forma breve y directa.";

void AIAssistantPanel::RenderAdvancedMode()
{
    auto& aiSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().ai;

    if (aiSettings.apiKey.empty()) {
        RenderApiKeySetup();
        return;
    }

    RenderChat();
}

void AIAssistantPanel::RenderApiKeySetup()
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Conecta tu API key de Claude");
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
        "Se guarda solo en este equipo (Ajustes). Con esto la IA puede crear y editar\n"
        "canciones de tu Biblioteca directamente -- siempre te va a pedir confirmacion\n"
        "antes de guardar cualquier cambio.");
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    if (!m_ApiKeyBufInit) {
        std::snprintf(m_ApiKeyBuf, sizeof(m_ApiKeyBuf), "%s",
            ProyecThor::Settings::SettingsManager::Get().GetSettings().ai.apiKey.c_str());
        m_ApiKeyBufInit = true;
    }

    ImGui::SetNextItemWidth(420.0f);
    ImGui::InputText("API key (sk-ant-...)", m_ApiKeyBuf, sizeof(m_ApiKeyBuf), ImGuiInputTextFlags_Password);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (DS::GlassButton("Guardar", ImVec2(120.0f, 30.0f))) {
        auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().ai;
        s.apiKey  = m_ApiKeyBuf;
        s.enabled = !s.apiKey.empty();
        ProyecThor::Settings::SettingsManager::Get().Save();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
        "Consigue una key en console.anthropic.com > API Keys.");
}

void AIAssistantPanel::RenderChat()
{
    // Consumir el resultado del turno en curso, si ya termino (ver StartTurn).
    {
        std::optional<Core::ClaudeResponse> resp;
        {
            std::lock_guard<std::mutex> lk(m_WorkerMutex);
            if (m_WorkerResult.has_value()) {
                resp = m_WorkerResult;
                m_WorkerResult.reset();
            }
        }
        if (resp.has_value()) {
            m_WorkerBusy = false;
            if (m_Worker.joinable()) m_Worker.join();

            if (!resp->ok) {
                m_ChatLog.push_back({ false, "Error: " + resp->error });
            } else {
                m_History.push_back(Core::MakeAssistantMessage(resp->assistantContent));
                if (!resp->text.empty())
                    m_ChatLog.push_back({ false, resp->text });

                if (!resp->toolCalls.empty()) {
                    m_PendingBatch        = resp->toolCalls;
                    m_PendingBatchIdx     = 0;
                    m_PendingBatchResults = nlohmann::json::array();
                    ProcessBatch();
                }
            }
        }
    }

    // ── Log de chat ───────────────────────────────────────────────────────
    float inputH = 74.0f;
    float confirmH = m_HasPendingConfirm ? 84.0f : 0.0f;
    ImGui::BeginChild("##aiChatLog", ImVec2(0.0f, -(inputH + confirmH)), true);
    for (const auto& line : m_ChatLog) {
        ImVec4 col = line.fromUser ? ImGui::ColorConvertU32ToFloat4(DS::TextPrimary)
                                    : ImGui::ColorConvertU32ToFloat4(DS::AccentLight);
        ImGui::TextColored(col, "%s", line.fromUser ? "Vos" : "Claude");
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
    }
    if (m_WorkerBusy) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "Pensando...");
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // ── Confirmacion de accion pendiente (create_song/edit_song) ───────────
    if (m_HasPendingConfirm) {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::BeginChild("##aiConfirm", ImVec2(0.0f, confirmH - 6.0f), true);
        std::string desc = Core::AITools::DescribeCall(m_PendingConfirmCall.name, m_PendingConfirmCall.input);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "La IA quiere: %s", desc.c_str());
        if (DS::GlassButton("Aprobar", ImVec2(110.0f, 28.0f), DS::SuccessColor))
            ApproveOrRejectPending(true);
        ImGui::SameLine();
        if (DS::GlassButton("Rechazar", ImVec2(110.0f, 28.0f), DS::DangerColor))
            ApproveOrRejectPending(false);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // ── Entrada de texto ─────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    bool canSend = !m_WorkerBusy && !m_HasPendingConfirm;
    ImGui::BeginDisabled(!canSend);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
    bool enterPressed = ImGui::InputText("##aiInput", m_InputBuf, sizeof(m_InputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool sendClicked = DS::GlassButton("Enviar", ImVec2(74.0f, 0.0f));
    ImGui::EndDisabled();

    if (canSend && (enterPressed || sendClicked) && m_InputBuf[0] != '\0') {
        m_ChatLog.push_back({ true, m_InputBuf });
        m_History.push_back(Core::MakeUserTextMessage(m_InputBuf));
        m_InputBuf[0] = '\0';
        StartTurn();
    }
}

void AIAssistantPanel::StartTurn()
{
    if (m_WorkerBusy) return;
    if (m_Worker.joinable()) m_Worker.join();

    m_WorkerBusy = true;

    auto& aiSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().ai;
    std::string apiKey = aiSettings.apiKey;
    std::string model  = aiSettings.model;
    std::vector<Core::ClaudeMessage> historyCopy = m_History;

    m_Worker = std::thread([this, apiKey, model, historyCopy]() {
        Core::ClaudeClient client;
        auto tools = Core::AITools::GetToolDefinitions();
        Core::ClaudeResponse resp = client.SendRequest(apiKey, model, kSystemPrompt, historyCopy, tools);

        std::lock_guard<std::mutex> lk(m_WorkerMutex);
        m_WorkerResult = resp;
    });
}

void AIAssistantPanel::ProcessBatch()
{
    while (m_PendingBatchIdx < m_PendingBatch.size()) {
        const Core::ClaudeToolCall& call = m_PendingBatch[m_PendingBatchIdx];

        if (Core::AITools::NeedsConfirmation(call.name)) {
            m_HasPendingConfirm  = true;
            m_PendingConfirmCall = call;
            return; // se retoma desde ApproveOrRejectPending
        }

        bool isErr = false;
        std::string result = Core::AITools::Execute(call.name, call.input, isErr);
        m_PendingBatchResults.push_back({
            { "type", "tool_result" }, { "tool_use_id", call.id },
            { "content", result }, { "is_error", isErr }
        });
        ++m_PendingBatchIdx;
    }

    // Lote resuelto por completo -- se manda como UN mensaje "user" con
    // todos los tool_result (la API exige que cada tool_use del turno
    // anterior tenga su tool_result correspondiente en el siguiente mensaje).
    m_History.push_back(Core::ClaudeMessage{ "user", m_PendingBatchResults });
    m_PendingBatch.clear();
    m_PendingBatchIdx = 0;
    m_PendingBatchResults = nlohmann::json::array();
    StartTurn();
}

void AIAssistantPanel::ApproveOrRejectPending(bool approve)
{
    const Core::ClaudeToolCall call = m_PendingConfirmCall;
    m_HasPendingConfirm = false;

    bool isErr = false;
    std::string result = approve
        ? Core::AITools::Execute(call.name, call.input, isErr)
        : std::string("El usuario rechazo esta accion, no se hizo ningun cambio.");
    if (!approve) isErr = true;

    m_ChatLog.push_back({ false,
        (approve ? std::string("[Aprobado] ") : std::string("[Rechazado] ")) +
        Core::AITools::DescribeCall(call.name, call.input) });

    m_PendingBatchResults.push_back({
        { "type", "tool_result" }, { "tool_use_id", call.id },
        { "content", result }, { "is_error", isErr }
    });
    ++m_PendingBatchIdx;

    ProcessBatch();
}

} // namespace ProyecThor::UI
