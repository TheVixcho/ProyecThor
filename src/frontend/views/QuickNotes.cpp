#include "QuickNotes.h"
#include "Announcements.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/DesignSystem.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI {

static ImVec4 Brighten(const ImVec4& c, float amount) {
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

static std::filesystem::path GetQuickNotesLibraryPath() {
    return std::filesystem::path(ProyecThor::GetAppDataRoot()) / "quick_notes.json";
}

static std::string GetCurrentTimestampString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tm);
    return std::string(buf);
}

static ImU32 GetCategoryColor(const std::string& cat) {
    if (cat == "Anuncios" || cat == "Anuncio") return IM_COL32(50, 180, 240, 255);   // Azul / Cian
    if (cat == "Avisos"   || cat == "Aviso")   return IM_COL32(82, 224, 160, 255);   // Verde menta
    if (cat == "Urgente")                      return IM_COL32(240, 80, 90, 255);    // Rojo
    if (cat == "Culto")                        return IM_COL32(245, 180, 50, 255);   // Ámbar
    return IM_COL32(160, 165, 180, 255);                                            // Gris / General
}

static int CountWords(const char* str) {
    int count = 0;
    bool inWord = false;
    while (*str) {
        if (std::isspace(static_cast<unsigned char>(*str))) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            count++;
        }
        str++;
    }
    return count;
}

QuickNotes::QuickNotes() : m_IsLive(false) {
    m_TextBuffer.fill('\0');
    LoadPersisted();
    LoadLibraryFromDisk();
}

QuickNotes::~QuickNotes() {
    auto& core = Core::PresentationCore::Get();
    if (m_TransmitMode == QuickNoteTransmitMode::MainOnly || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNote();
    if (m_TransmitMode == QuickNoteTransmitMode::LANOnly  || m_TransmitMode == QuickNoteTransmitMode::Both)
        core.ClearQuickNoteLAN();

    PersistNow();
    SaveLibraryToDisk();
}

std::string QuickNotes::GetName() const { return "QuickNotes"; }

void QuickNotes::LoadPersisted() {
    const std::string& saved = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.quickNotesText;
    size_t n = std::min(saved.size(), m_TextBuffer.size() - 1);
    std::copy(saved.begin(), saved.begin() + n, m_TextBuffer.begin());
    m_TextBuffer[n] = '\0';
}

void QuickNotes::PersistNow() {
    auto& general = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    std::string text(m_TextBuffer.data());
    if (general.quickNotesText == text) return;
    general.quickNotesText = text;
    ProyecThor::Settings::SettingsManager::Get().Save();
    m_LastPersistTime = ImGui::GetTime();
}

void QuickNotes::LoadLibraryFromDisk() {
    m_SavedNotes.clear();
    std::filesystem::path p = GetQuickNotesLibraryPath();
    if (!std::filesystem::exists(p)) return;

    try {
        std::ifstream f(p);
        if (!f.is_open()) return;
        nlohmann::json j;
        f >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                QuickNoteItem note;
                note.id         = item.value("id", "");
                note.title      = item.value("title", "Sin título");
                note.content    = item.value("content", "");
                note.category   = item.value("category", "General");
                note.styleName  = item.value("styleName", "");
                note.updatedAt  = item.value("updatedAt", "");
                note.isFavorite = item.value("isFavorite", false);
                if (!note.id.empty() && !note.content.empty())
                    m_SavedNotes.push_back(std::move(note));
            }
        }
    } catch (...) {
    }
}

void QuickNotes::SaveLibraryToDisk() {
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& note : m_SavedNotes) {
            nlohmann::json item;
            item["id"]         = note.id;
            item["title"]      = note.title;
            item["content"]    = note.content;
            item["category"]   = note.category;
            item["styleName"]  = note.styleName;
            item["updatedAt"]  = note.updatedAt;
            item["isFavorite"] = note.isFavorite;
            j.push_back(std::move(item));
        }
        std::filesystem::path p = GetQuickNotesLibraryPath();
        std::ofstream f(p);
        if (f.is_open()) {
            f << j.dump(2);
        }
    } catch (...) {
    }
}

void QuickNotes::SaveCurrentToLibrary(const std::string& title, const std::string& category) {
    std::string text(m_TextBuffer.data());
    if (text.empty()) return;

    std::string effectiveTitle = title;
    if (effectiveTitle.empty()) {
        effectiveTitle = text.substr(0, std::min<size_t>(text.size(), 28));
        if (text.size() > 28) effectiveTitle += "...";
    }

    QuickNoteItem note;
    note.id         = "qn_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    note.title      = effectiveTitle;
    note.content    = text;
    note.category   = category.empty() ? "General" : category;
    note.styleName  = m_StyleName;
    note.updatedAt  = GetCurrentTimestampString();
    note.isFavorite = false;

    m_SavedNotes.insert(m_SavedNotes.begin(), std::move(note));
    SaveLibraryToDisk();

    m_FeedbackMessage = "Nota guardada en la biblioteca: " + effectiveTitle;
    m_FeedbackTime    = ImGui::GetTime();
}

void QuickNotes::LoadFromLibrary(const QuickNoteItem& item, bool transmitImmediately) {
    size_t n = std::min(item.content.size(), m_TextBuffer.size() - 1);
    std::copy(item.content.begin(), item.content.begin() + n, m_TextBuffer.begin());
    m_TextBuffer[n] = '\0';
    if (!item.styleName.empty())
        m_StyleName = item.styleName;

    PersistNow();

    if (transmitImmediately) {
        if (m_TransmitMode == QuickNoteTransmitMode::Off)
            m_TransmitMode = QuickNoteTransmitMode::MainOnly;
        SyncTransmission();
        m_FeedbackMessage = "Transmitiendo en vivo: " + item.title;
    } else {
        m_CurrentTab      = QuickNotesTab::LiveEditor;
        m_FeedbackMessage = "Nota cargada en el editor: " + item.title;
    }
    m_FeedbackTime = ImGui::GetTime();
}

void QuickNotes::DeleteFromLibrary(const std::string& id) {
    auto it = std::remove_if(m_SavedNotes.begin(), m_SavedNotes.end(),
        [&id](const QuickNoteItem& item) { return item.id == id; });
    if (it != m_SavedNotes.end()) {
        m_SavedNotes.erase(it, m_SavedNotes.end());
        SaveLibraryToDisk();
        m_FeedbackMessage = "Nota eliminada de la biblioteca";
        m_FeedbackTime    = ImGui::GetTime();
    }
}

void QuickNotes::ToggleFavorite(const std::string& id) {
    for (auto& item : m_SavedNotes) {
        if (item.id == id) {
            item.isFavorite = !item.isFavorite;
            SaveLibraryToDisk();
            break;
        }
    }
}

void QuickNotes::SyncTransmission() {
    auto& core = Core::PresentationCore::Get();
    std::string text = std::string(m_TextBuffer.data());

    bool wasMain = (m_PrevTransmitMode == QuickNoteTransmitMode::MainOnly || m_PrevTransmitMode == QuickNoteTransmitMode::Both);
    bool wasLAN  = (m_PrevTransmitMode == QuickNoteTransmitMode::LANOnly  || m_PrevTransmitMode == QuickNoteTransmitMode::Both);
    bool isMain  = (m_TransmitMode     == QuickNoteTransmitMode::MainOnly || m_TransmitMode     == QuickNoteTransmitMode::Both);
    bool isLAN   = (m_TransmitMode     == QuickNoteTransmitMode::LANOnly  || m_TransmitMode     == QuickNoteTransmitMode::Both);

    if (isMain && !m_StyleName.empty())
        core.ApplyStyleByName(m_StyleName);

    if (isMain) {
        if (text.empty()) core.ClearQuickNote();
        else               core.SetLiveQuickNote(text);
    } else if (wasMain) {
        core.ClearQuickNote();
    }

    if (isLAN) {
        if (text.empty()) core.ClearQuickNoteLAN();
        else               core.SetLiveQuickNoteLAN(text, nullptr, isMain ? "" : m_StyleName);
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();
    }

    m_PrevTransmitMode = m_TransmitMode;
}

void QuickNotes::ClearFromCore() {
    auto& core = Core::PresentationCore::Get();
    core.ClearQuickNote();
    core.ClearQuickNoteLAN();
    m_TransmitMode     = QuickNoteTransmitMode::Off;
    m_PrevTransmitMode = QuickNoteTransmitMode::Off;
}

void QuickNotes::RenderHeaderBar() {
    bool isLive = (m_TransmitMode != QuickNoteTransmitMode::Off);

    ImGui::BeginGroup();

    // Título principal
    ImGui::SetWindowFontScale(1.15f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
    ImGui::TextUnformatted("Notas Rápidas");
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0.0f, 12.0f);

    // Indicador Live con pulso
    if (isLive) {
        float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime() * 5.0));
        ImVec4 dotCol = ImVec4(0.32f + pulse * 0.1f, 0.88f, 0.62f, 1.0f);

        const char* destText = "EN VIVO";
        if (m_TransmitMode == QuickNoteTransmitMode::MainOnly) destText = "EN VIVO (Pantalla)";
        else if (m_TransmitMode == QuickNoteTransmitMode::LANOnly) destText = "EN VIVO (Solo LAN)";
        else if (m_TransmitMode == QuickNoteTransmitMode::Both) destText = "EN VIVO (Pantalla + LAN)";

        ImVec2 badgePad(8.0f, 3.0f);
        ImVec2 badgePos = ImGui::GetCursorScreenPos();
        ImVec2 textSz = ImGui::CalcTextSize(destText);
        ImVec2 badgeMin(badgePos.x, badgePos.y - 1.0f);
        ImVec2 badgeMax(badgePos.x + textSz.x + badgePad.x * 2.0f + 12.0f, badgePos.y + textSz.y + badgePad.y * 2.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(badgeMin, badgeMax, IM_COL32(35, 100, 65, 180), DS::RadiusSmall);
        dl->AddRect(badgeMin, badgeMax, IM_COL32(82, 224, 160, 200), DS::RadiusSmall, 0, 1.2f);
        dl->AddCircleFilled(ImVec2(badgeMin.x + 10.0f, (badgeMin.y + badgeMax.y) * 0.5f), 3.5f,
            ImGui::ColorConvertFloat4ToU32(dotCol));

        ImGui::SetCursorScreenPos(ImVec2(badgeMin.x + 18.0f, badgeMin.y + badgePad.y));
        ImGui::PushStyleColor(ImGuiCol_Text, dotCol);
        ImGui::TextUnformatted(destText);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextUnformatted("• En espera");
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();

    // Selector de pestañas a la derecha
    const float tabW = 125.0f;
    const float tabH = 28.0f;
    const float totalTabsW = tabW * 2.0f + 6.0f;

    ImGui::SameLine(ImGui::GetWindowWidth() - totalTabsW - 20.0f);

    auto TabPill = [&](const char* label, QuickNotesTab tab, bool isActive) {
        ImVec4 bg = isActive
            ? ImGui::ColorConvertU32ToFloat4(DS::AccentColor)
            : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        ImVec4 fg = isActive
            ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
            : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);

        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
        ImGui::PushStyleColor(ImGuiCol_Text, fg);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

        if (ImGui::Button(label, ImVec2(tabW, tabH)))
            m_CurrentTab = tab;

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    };

    TabPill("Editor en Vivo", QuickNotesTab::LiveEditor, m_CurrentTab == QuickNotesTab::LiveEditor);
    ImGui::SameLine(0.0f, 6.0f);

    char libLabel[64];
    std::snprintf(libLabel, sizeof(libLabel), "Biblioteca (%zu)", m_SavedNotes.size());
    TabPill(libLabel, QuickNotesTab::Library, m_CurrentTab == QuickNotesTab::Library);
}

void QuickNotes::RenderStyleSelector() {
    auto& core = Core::PresentationCore::Get();
    std::vector<std::string> styleNames = core.GetSavedStyleNames();
    std::string preview = m_StyleName.empty() ? "Usar estilo actual de ProyecThor" : m_StyleName;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Estilo visual del texto:");

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::BeginCombo("##quickNoteStyle", preview.c_str())) {
        bool noneSelected = m_StyleName.empty();
        if (ImGui::Selectable("Usar estilo actual de ProyecThor", noneSelected))
            m_StyleName.clear();
        for (const auto& name : styleNames) {
            bool sel = (m_StyleName == name);
            if (ImGui::Selectable(name.c_str(), sel))
                m_StyleName = name;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void QuickNotes::RenderTransmitCards() {
    auto& core       = Core::PresentationCore::Get();
    bool  netAvailable = core.IsStreamingNet();

    struct ModeOpt { const char* label; QuickNoteTransmitMode mode; bool needsNet; };
    ModeOpt opts[4] = {
        { "Apagado",  QuickNoteTransmitMode::Off,      false },
        { "Pantalla", QuickNoteTransmitMode::MainOnly, false },
        { "Solo LAN", QuickNoteTransmitMode::LANOnly,  true  },
        { "Ambos",    QuickNoteTransmitMode::Both,     true  },
    };

    float w     = ImGui::GetContentRegionAvail().x;
    float gap   = 8.0f;
    float cardW = (w - gap * 3.0f) / 4.0f;
    float cardH = 40.0f;

    ImVec4 successCol = ImGui::ColorConvertU32ToFloat4(DS::SuccessColor);

    for (int i = 0; i < 4; ++i) {
        auto& opt = opts[i];
        bool disabled = opt.needsNet && !netAvailable;
        bool active   = (m_TransmitMode == opt.mode) && !disabled;

        if (i > 0) ImGui::SameLine(0, gap);

        ImVec4 bg = active
            ? ImVec4(successCol.x * 0.25f, successCol.y * 0.25f, successCol.z * 0.25f, 0.85f)
            : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        ImVec4 bgHover = active
            ? ImVec4(successCol.x * 0.35f, successCol.y * 0.35f, successCol.z * 0.35f, 0.95f)
            : ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill);

        ImGui::PushStyleColor(ImGuiCol_Button,        bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHover);
        ImGui::PushStyleColor(ImGuiCol_Text,
            disabled ? ImGui::ColorConvertU32ToFloat4(DS::TextHint)
                     : (active ? successCol : ImGui::ColorConvertU32ToFloat4(DS::TextPrimary)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.5f : 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, active ? successCol : ImGui::ColorConvertU32ToFloat4(DS::GlassBorder));

        ImGui::BeginDisabled(disabled);
        if (ImGui::Button(opt.label, ImVec2(cardW, cardH)))
            m_TransmitMode = opt.mode;
        ImGui::EndDisabled();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

    if (!netAvailable) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::SetWindowFontScale(0.88f);
        ImGui::TextUnformatted("Inicia el servidor en el panel de Transmisión para habilitar salidas por red LAN.");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        if (m_TransmitMode == QuickNoteTransmitMode::LANOnly) m_TransmitMode = QuickNoteTransmitMode::Off;
        if (m_TransmitMode == QuickNoteTransmitMode::Both)    m_TransmitMode = QuickNoteTransmitMode::MainOnly;
    }
}

void QuickNotes::RenderLiveEditorTab() {
    bool isLive = (m_TransmitMode != QuickNoteTransmitMode::Off);

    // ── Barra de herramientas rápida ─────────────────────────────────────
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));

        if (ImGui::Button("Guardar en Biblioteca")) {
            m_ShowSaveModal = true;
            m_SaveCategoryIdx = 0;
            std::string text(m_TextBuffer.data());
            std::string defTitle = text.substr(0, std::min<size_t>(text.size(), 24));
            std::snprintf(m_SaveTitleBuf, sizeof(m_SaveTitleBuf), "%s", defTitle.c_str());
            m_SaveCustomCategoryBuf[0] = '\0';
        }

        ImGui::SameLine();
        if (ImGui::Button("Copiar")) {
            ImGui::SetClipboardText(m_TextBuffer.data());
            m_FeedbackMessage = "Texto copiado al portapapeles";
            m_FeedbackTime = ImGui::GetTime();
        }

        ImGui::SameLine();
        if (ImGui::Button("MAYÚSCULAS")) {
            for (auto& c : m_TextBuffer)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            PersistNow();
        }

        ImGui::SameLine();
        if (ImGui::Button("Minúsculas")) {
            for (auto& c : m_TextBuffer)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            PersistNow();
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.55f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.45f, 0.70f, 0.95f));
        if (ImGui::Button("+ Banner")) {
            std::string text(m_TextBuffer.data());
            if (!text.empty()) {
                if (auto* ann = Core::PresentationCore::Get().GetAnnouncementsRef()) {
                    ann->AddMessage(text, "Anuncios", true);
                    m_FeedbackMessage = "Texto enviado al banner de Anuncios";
                    m_FeedbackTime = ImGui::GetTime();
                }
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.15f, 0.15f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.2f, 0.2f, 0.9f));
        if (ImGui::Button("Borrar")) {
            m_TextBuffer.fill('\0');
            ClearFromCore();
            PersistNow();
            m_FeedbackMessage = "Texto borrado";
            m_FeedbackTime = ImGui::GetTime();
        }
        ImGui::PopStyleColor(2);

        ImGui::PopStyleVar(2);
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Caja de Entrada de Texto con borde reactivo ──────────────────────
    ImVec4 baseInputBg = ImGui::ColorConvertU32ToFloat4(DS::GlassFillBot);
    ImVec4 inputBg = isLive ? Brighten(baseInputBg, 0.04f) : baseInputBg;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        inputBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Brighten(inputBg, 0.02f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  inputBg);
    ImGui::PushStyleColor(ImGuiCol_Border,         ImGui::ColorConvertU32ToFloat4(isLive ? DS::SuccessColor : DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isLive ? 1.5f : 1.0f);

    float inputH = 140.0f;
    bool textChanged = ImGui::InputTextMultiline(
        "##QuickNoteInput",
        m_TextBuffer.data(),
        m_TextBuffer.size(),
        ImVec2(-FLT_MIN, inputH),
        ImGuiInputTextFlags_AllowTabInput);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // ── Estadísticas de texto e info ─────────────────────────────────────
    {
        size_t len = std::strlen(m_TextBuffer.data());
        int words = CountWords(m_TextBuffer.data());
        int readTimeSec = std::max(1, static_cast<int>(words / 3.0f)); // aprox 180 palabras por minuto

        char statsBuf[128];
        std::snprintf(statsBuf, sizeof(statsBuf), "%zu caracteres  •  %d palabras  •  ~%ds lectura",
            len, words, readTimeSec);

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextUnformatted(statsBuf);

        double sinceSave = ImGui::GetTime() - m_LastPersistTime;
        const char* saveStatus = (sinceSave < 1.5) ? "Guardado en disco" : "Autoguardado activo";
        const ImVec2 saveSz = ImGui::CalcTextSize(saveStatus);
        ImGui::SameLine(ImGui::GetWindowWidth() - saveSz.x - 20.0f);
        ImGui::TextUnformatted(saveStatus);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // ── Selector de Estilo y Tarjetas de Transmisión ─────────────────────
    RenderStyleSelector();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "Destino de transmisión en vivo:");
    RenderTransmitCards();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // ── Botones de Acción Rápida (Transmitir / Ocultar) ───────────────────
    {
        const float availW = ImGui::GetContentRegionAvail().x;
        const float btnW = (availW - 10.0f) * 0.5f;
        const float btnH = 38.0f;

        // Botón Transmitir / Actualizar
        if (isLive) {
            if (DS::GlassButton("Actualizar en Pantalla (F5)", ImVec2(btnW, btnH), DS::SuccessColor)) {
                SyncTransmission();
                m_FeedbackMessage = "Transmisión actualizada en pantalla";
                m_FeedbackTime = ImGui::GetTime();
            }
        } else {
            if (DS::GlassButton("Proyectar Ahora (F5)", ImVec2(btnW, btnH), DS::AccentColor)) {
                m_TransmitMode = QuickNoteTransmitMode::MainOnly;
                SyncTransmission();
                m_FeedbackMessage = "Proyectando nota rápida";
                m_FeedbackTime = ImGui::GetTime();
            }
        }

        ImGui::SameLine(0.0f, 10.0f);

        // Botón Ocultar de Pantalla
        if (DS::GlassButton("Ocultar de Pantalla (Esc)", ImVec2(btnW, btnH), DS::DangerColor)) {
            ClearFromCore();
            m_FeedbackMessage = "Nota oculta de la pantalla";
            m_FeedbackTime = ImGui::GetTime();
        }
    }

    if (textChanged)
        SyncTransmission();

    if (textChanged && (ImGui::GetTime() - m_LastPersistTime) > 1.5)
        PersistNow();
}

void QuickNotes::RenderLibraryTab() {
    // ── Fila de Búsqueda y Botón Nueva Nota ──────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);

        const float newBtnW = 120.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - newBtnW - 8.0f);
        ImGui::InputTextWithHint("##qnSearch", "Buscar por título o contenido...",
            m_SearchFilter, sizeof(m_SearchFilter));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 8.0f);

        // Botón Nueva Nota
        if (DS::GlassButton("+ Nueva Nota", ImVec2(newBtnW, 28.0f), DS::AccentColor)) {
            m_TextBuffer.fill('\0');
            m_CurrentTab = QuickNotesTab::LiveEditor;
            PersistNow();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Filtros por Categoría ────────────────────────────────────────────
    {
        const std::vector<std::string> categories = { "Todos", "Favoritos", "Anuncios", "Avisos", "Urgente", "Culto", "General" };

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

        for (const auto& cat : categories) {
            bool selected = (m_SelectedCategoryFilter == cat);

            ImVec4 bg = selected
                ? ImGui::ColorConvertU32ToFloat4(DS::AccentColor)
                : ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);

            ImGui::PushStyleColor(ImGuiCol_Button, bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4(1,1,1,1) : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));

            if (ImGui::Button(cat.c_str()))
                m_SelectedCategoryFilter = cat;

            ImGui::PopStyleColor(3);
            ImGui::SameLine();
        }
        ImGui::NewLine();

        ImGui::PopStyleVar(2);
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Lista de Tarjetas de Notas ───────────────────────────────────────
    float listH = ImGui::GetContentRegionAvail().y - 10.0f;
    ImGui::BeginChild("##qnCardsList", ImVec2(0.0f, listH), false);

    std::string searchLower = m_SearchFilter;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    // Filtrar y ordenar (Favoritos primero)
    std::vector<QuickNoteItem> filtered;
    for (const auto& note : m_SavedNotes) {
        if (m_SelectedCategoryFilter == "Favoritos" && !note.isFavorite) continue;
        if (m_SelectedCategoryFilter != "Todos" && m_SelectedCategoryFilter != "Favoritos" && note.category != m_SelectedCategoryFilter) continue;

        if (!searchLower.empty()) {
            std::string tLower = note.title;
            std::string cLower = note.content;
            std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
            std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
            if (tLower.find(searchLower) == std::string::npos && cLower.find(searchLower) == std::string::npos)
                continue;
        }
        filtered.push_back(note);
    }

    std::stable_sort(filtered.begin(), filtered.end(), [](const QuickNoteItem& a, const QuickNoteItem& b) {
        if (a.isFavorite != b.isFavorite) return a.isFavorite > b.isFavorite;
        return false;
    });

    if (filtered.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 30.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        const char* emptyMsg1 = m_SavedNotes.empty()
            ? "No tienes notas guardadas en la biblioteca."
            : "No se encontraron notas con el criterio de búsqueda.";
        const char* emptyMsg2 = m_SavedNotes.empty()
            ? "Escribe una nota en el editor y haz clic en 'Guardar' para almacenarla aquí."
            : "Prueba seleccionando otra categoría o limpiando el filtro.";

        ImVec2 msg1Sz = ImGui::CalcTextSize(emptyMsg1);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - msg1Sz.x) * 0.5f);
        ImGui::TextUnformatted(emptyMsg1);

        ImVec2 msg2Sz = ImGui::CalcTextSize(emptyMsg2);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - msg2Sz.x) * 0.5f);
        ImGui::TextUnformatted(emptyMsg2);

        ImGui::PopStyleColor();
    } else {
        std::string noteToDelete;

        for (const auto& note : filtered) {
            ImGui::PushID(note.id.c_str());

            const float cardW = ImGui::GetContentRegionAvail().x;
            const float cardH = 92.0f;

            ImVec2 pMin = ImGui::GetCursorScreenPos();
            ImVec2 pMax = ImVec2(pMin.x + cardW, pMin.y + cardH);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pMin, pMax, DS::GlassFillBot, DS::RadiusMedium);
            dl->AddRect(pMin, pMax, DS::GlassBorder, DS::RadiusMedium);

            // Borde lateral con color de categoría
            ImU32 catCol = GetCategoryColor(note.category);
            dl->AddRectFilled(pMin, ImVec2(pMin.x + 4.0f, pMax.y), catCol, DS::RadiusMedium, ImDrawFlags_RoundCornersLeft);

            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 12.0f, ImGui::GetCursorPosY() + 8.0f));
            ImGui::BeginGroup();

            // Fila 1: Badge de Categoría + Título + Botón Favorito
            {
                // Pill de Categoría
                ImVec2 catSz = ImGui::CalcTextSize(note.category.c_str());
                ImVec2 catPos = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(ImVec2(catPos.x, catPos.y - 1.0f), ImVec2(catPos.x + catSz.x + 10.0f, catPos.y + catSz.y + 3.0f),
                    (catCol & 0x00FFFFFF) | 0x35000000, DS::RadiusSmall);

                ImGui::SetCursorScreenPos(ImVec2(catPos.x + 5.0f, catPos.y));
                ImGui::PushStyleColor(ImGuiCol_Text, catCol);
                ImGui::SetWindowFontScale(0.80f);
                ImGui::TextUnformatted(note.category.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::SameLine(0.0f, 10.0f);

                // Título en negrita
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
                ImGui::SetWindowFontScale(1.05f);
                ImGui::TextUnformatted(note.title.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                // Botón de Favorito
                ImGui::SameLine(cardW - 44.0f);
                if (note.isFavorite) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.50f, 0.10f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                }
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
                if (ImGui::Button("Fav", ImVec2(34.0f, 20.0f))) {
                    ToggleFavorite(note.id);
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            }

            // Fila 2: Snippet de contenido
            {
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
                ImGui::PushTextWrapPos(pMin.x + cardW - 20.0f);

                std::string snippet = note.content;
                if (snippet.size() > 140) {
                    snippet = snippet.substr(0, 135) + "...";
                }
                ImGui::TextWrapped("%s", snippet.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            // Fila 3: Botones de Acción
            {
                ImGui::Dummy(ImVec2(0.0f, 2.0f));

                if (!note.updatedAt.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                    ImGui::SetWindowFontScale(0.80f);
                    ImGui::TextUnformatted(note.updatedAt.c_str());
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                }

                ImGui::SameLine(cardW - 300.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);

                // Cargar en editor
                if (ImGui::Button("Cargar", ImVec2(60.0f, 22.0f))) {
                    LoadFromLibrary(note, false);
                }

                ImGui::SameLine(0.0f, 4.0f);

                // Proyectar ahora
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.35f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.45f, 0.95f));
                if (ImGui::Button("Proyectar", ImVec2(74.0f, 22.0f))) {
                    LoadFromLibrary(note, true);
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(0.0f, 4.0f);

                // Enviar a Banner de Anuncios
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.55f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.45f, 0.70f, 0.95f));
                if (ImGui::Button("+ Banner", ImVec2(66.0f, 22.0f))) {
                    if (auto* ann = Core::PresentationCore::Get().GetAnnouncementsRef()) {
                        ann->AddMessage(note.content, note.category.empty() ? "Anuncios" : note.category, true);
                        m_FeedbackMessage = "Nota añadida al banner de Anuncios";
                        m_FeedbackTime = ImGui::GetTime();
                    }
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(0.0f, 4.0f);

                // Borrar
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.15f, 0.15f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.2f, 0.2f, 0.9f));
                if (ImGui::Button("Borrar", ImVec2(48.0f, 22.0f))) {
                    noteToDelete = note.id;
                }
                ImGui::PopStyleColor(2);

                ImGui::PopStyleVar();
            }

            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::PopID();
        }

        if (!noteToDelete.empty()) {
            DeleteFromLibrary(noteToDelete);
        }
    }

    ImGui::EndChild();
}

void QuickNotes::RenderSaveModal() {
    ImGui::OpenPopup("Guardar Nota en Biblioteca");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 290.0f), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillBot));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImGui::ColorConvertU32ToFloat4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(20.0f, 18.0f));

    if (ImGui::BeginPopupModal("Guardar Nota en Biblioteca", &m_ShowSaveModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Título o descripción de la nota:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##saveNoteTitle", m_SaveTitleBuf, sizeof(m_SaveTitleBuf));

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Categoría / Etiqueta:");

        const char* catOptions[] = { "Anuncios", "Avisos", "Urgente", "Culto", "General", "Personalizado" };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        for (int i = 0; i < 6; ++i) {
            bool sel = (m_SaveCategoryIdx == i);
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            }

            if (ImGui::Button(catOptions[i]))
                m_SaveCategoryIdx = i;

            ImGui::PopStyleColor(2);
            if (i < 5) ImGui::SameLine(0.0f, 6.0f);
        }
        ImGui::PopStyleVar();

        if (m_SaveCategoryIdx == 5) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##customCat", "Escribe categoría personalizada...",
                m_SaveCustomCategoryBuf, sizeof(m_SaveCustomCategoryBuf));
        }

        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        DS::GlassSeparator();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        float btnW = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;

        if (DS::GlassButton("Guardar Nota", ImVec2(btnW, 34.0f), DS::SuccessColor)) {
            std::string finalCat = "General";
            if (m_SaveCategoryIdx >= 0 && m_SaveCategoryIdx < 5) {
                finalCat = catOptions[m_SaveCategoryIdx];
            } else if (m_SaveCategoryIdx == 5 && m_SaveCustomCategoryBuf[0] != '\0') {
                finalCat = m_SaveCustomCategoryBuf;
            }

            SaveCurrentToLibrary(m_SaveTitleBuf, finalCat);
            m_ShowSaveModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine(0.0f, 10.0f);

        if (DS::GlassButton("Cancelar", ImVec2(btnW, 34.0f), DS::TextHint)) {
            m_ShowSaveModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void QuickNotes::Render() {
    // Atajos de teclado globales dentro de la ventana
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // F5 o Ctrl+Enter: Enviar a pantalla
        if ((ImGui::IsKeyPressed(ImGuiKey_F5) || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)))
            && m_TransmitMode == QuickNoteTransmitMode::Off)
        {
            m_TransmitMode = QuickNoteTransmitMode::MainOnly;
            SyncTransmission();
            m_FeedbackMessage = "Nota proyectada en vivo";
            m_FeedbackTime = ImGui::GetTime();
        }

        // Escape: Ocultar si está en vivo
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && m_TransmitMode != QuickNoteTransmitMode::Off) {
            ClearFromCore();
            m_FeedbackMessage = "Nota oculta de la pantalla";
            m_FeedbackTime = ImGui::GetTime();
        }
    }

    RenderHeaderBar();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (m_CurrentTab == QuickNotesTab::LiveEditor) {
        RenderLiveEditorTab();
    } else {
        RenderLibraryTab();
    }

    if (m_ShowSaveModal) {
        RenderSaveModal();
    }

    // Feedback Toast flotante
    if (!m_FeedbackMessage.empty() && (ImGui::GetTime() - m_FeedbackTime) < 2.5) {
        float alpha = 1.0f;
        double elapsed = ImGui::GetTime() - m_FeedbackTime;
        if (elapsed > 1.8) alpha = static_cast<float>((2.5 - elapsed) / 0.7);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winP = ImGui::GetWindowPos();
        ImVec2 winSz = ImGui::GetWindowSize();
        ImVec2 textSz = ImGui::CalcTextSize(m_FeedbackMessage.c_str());

        const float padX = 16.0f, padY = 6.0f;
        ImVec2 toastMin(winP.x + (winSz.x - textSz.x - padX * 2.0f) * 0.5f, winP.y + winSz.y - 38.0f);
        ImVec2 toastMax(toastMin.x + textSz.x + padX * 2.0f, toastMin.y + textSz.y + padY * 2.0f);

        ImU32 toastBg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.14f, 0.18f, 0.95f * alpha));
        ImU32 toastBord = ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.75f, 0.55f, 0.8f * alpha));
        ImU32 toastText = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.95f, 0.98f, 1.0f * alpha));

        dl->AddRectFilled(toastMin, toastMax, toastBg, DS::RadiusMedium);
        dl->AddRect(toastMin, toastMax, toastBord, DS::RadiusMedium, 0, 1.2f);
        dl->AddText(ImVec2(toastMin.x + padX, toastMin.y + padY), toastText, m_FeedbackMessage.c_str());
    }
}

} // namespace ProyecThor::UI

