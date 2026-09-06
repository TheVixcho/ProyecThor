#include "WebBrowserPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/ui/LoadingSpinner.h"
#include "frontend/ui/FilePicker.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "external/tools/OpenURL.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

namespace {

static std::string GetCurrentTimestampStr() {
    std::time_t t = std::time(nullptr);
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

static std::string FormatFileSize(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)::tolower(c); });
    return s;
}

static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}

}

WebBrowserPanel::WebBrowserPanel()
{
    LoadLibraryFromDisk();
}

void WebBrowserPanel::Hide()
{
    m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void WebBrowserPanel::Go(const std::string& customUrl)
{
    std::string target = customUrl.empty() ? std::string(m_UrlBuf) : customUrl;
    if (target.empty()) return;
    NavigateToPathOrUrl(target);
}

void WebBrowserPanel::NavigateToPathOrUrl(const std::string& input)
{
    if (input.empty()) return;

    std::string target = input;
    bool isLocal = false;

    if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0) {
        isLocal = false;
    } else if (target.rfind("file:///", 0) == 0 || target.rfind("file://", 0) == 0) {
        isLocal = true;
    } else {
        std::error_code ec;
        fs::path p(target);
        if (fs::exists(p, ec)) {
            isLocal = true;
            std::string gen = fs::absolute(p, ec).generic_string();
            if (!gen.empty() && gen[0] == '/')
                target = "file://" + gen;
            else
                target = "file:///" + gen;
        } else if (target.size() >= 2 && target[1] == ':') {
            isLocal = true;
            std::string pStr = target;
            std::replace(pStr.begin(), pStr.end(), '\\', '/');
            target = "file:///" + pStr;
        } else {
            isLocal = false;
            target = "https://" + target;
        }
    }

    std::strncpy(m_UrlBuf, target.c_str(), sizeof(m_UrlBuf) - 1);
    m_UrlBuf[sizeof(m_UrlBuf) - 1] = '\0';

    m_Navigated = true;

    if (!isLocal) {
        auto it = std::find_if(m_VisitedWebs.begin(), m_VisitedWebs.end(),
            [&](const VisitedWebItem& v) { return v.url == target; });
        if (it != m_VisitedWebs.end()) {
            it->visitedAt = GetCurrentTimestampStr();
            VisitedWebItem item = *it;
            m_VisitedWebs.erase(it);
            m_VisitedWebs.insert(m_VisitedWebs.begin(), item);
        } else {
            VisitedWebItem item;
            item.url = target;
            item.title = "";
            item.visitedAt = GetCurrentTimestampStr();
            item.isFavorite = false;
            m_VisitedWebs.insert(m_VisitedWebs.begin(), item);
            if (m_VisitedWebs.size() > 100)
                m_VisitedWebs.pop_back();
        }
        SaveLibraryToDisk();
    }

    if (m_WebView.IsAvailable()) {
        m_WebView.NavigateTo(target);
    } else {
        ProyecThor::External::OpenURL(target);
    }
}

void WebBrowserPanel::StartSendToPublic()
{
    m_SendingToPublic = true;
    Core::PresentationCore::Get().SetProjecting(true);
}

void WebBrowserPanel::StopSendToPublic()
{
    m_SendingToPublic = false;
#ifdef _WIN32
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        HWND mainHwnd = glfwGetWin32Window(win);
        m_WebView.Reparent(mainHwnd);
    }
#endif
    m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void WebBrowserPanel::UpdateSendToPublicBounds()
{
    void* hwnd = Core::PresentationCore::Get().GetProjectorNativeWindow();
    if (!hwnd) return;

    m_WebView.Reparent(hwnd);

    auto state = Core::PresentationCore::Get().GetState();
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (!monitors || monitorCount == 0) return;
    int idx = std::clamp(state.targetMonitorIndex, 0, monitorCount - 1);

    int mx = 0, my = 0;
    glfwGetMonitorPos(monitors[idx], &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(monitors[idx]);
    if (!mode) return;

    m_WebView.UpdateBounds(mx, my, mode->width, mode->height, true);
}

void WebBrowserPanel::LoadLibraryFromDisk()
{
    m_ImportedHtmls.clear();
    m_VisitedWebs.clear();

    fs::path p = fs::path(ProyecThor::GetAppDataRoot()) / "web_library.json";
    std::error_code ec;
    if (!fs::exists(p, ec)) return;

    std::ifstream in(p);
    if (!in.is_open()) return;

    try {
        nlohmann::json j;
        in >> j;

        if (j.contains("history") && j["history"].is_array()) {
            for (const auto& item : j["history"]) {
                VisitedWebItem w;
                w.url = item.value("url", "");
                w.title = item.value("title", "");
                w.visitedAt = item.value("visitedAt", "");
                w.isFavorite = item.value("isFavorite", false);
                if (!w.url.empty())
                    m_VisitedWebs.push_back(w);
            }
        }

        if (j.contains("importedHtml") && j["importedHtml"].is_array()) {
            for (const auto& item : j["importedHtml"]) {
                ImportedHtmlItem h;
                h.id = item.value("id", "");
                h.title = item.value("title", "");
                h.filePath = item.value("filePath", "");
                h.importedAt = item.value("importedAt", "");
                h.fileSize = item.value("fileSize", (uintmax_t)0);
                h.isFavorite = item.value("isFavorite", false);
                if (!h.filePath.empty()) {
                    if (fs::exists(h.filePath, ec)) {
                        h.fileSize = fs::file_size(h.filePath, ec);
                    }
                    m_ImportedHtmls.push_back(h);
                }
            }
        }
    } catch (...) {}
}

void WebBrowserPanel::SaveLibraryToDisk()
{
    fs::path p = fs::path(ProyecThor::GetAppDataRoot()) / "web_library.json";
    nlohmann::json j;

    nlohmann::json histArr = nlohmann::json::array();
    for (const auto& w : m_VisitedWebs) {
        nlohmann::json item;
        item["url"] = w.url;
        item["title"] = w.title;
        item["visitedAt"] = w.visitedAt;
        item["isFavorite"] = w.isFavorite;
        histArr.push_back(item);
    }
    j["history"] = histArr;

    nlohmann::json htmlArr = nlohmann::json::array();
    for (const auto& h : m_ImportedHtmls) {
        nlohmann::json item;
        item["id"] = h.id;
        item["title"] = h.title;
        item["filePath"] = h.filePath;
        item["importedAt"] = h.importedAt;
        item["fileSize"] = h.fileSize;
        item["isFavorite"] = h.isFavorite;
        htmlArr.push_back(item);
    }
    j["importedHtml"] = htmlArr;

    std::ofstream out(p);
    if (out.is_open()) {
        out << j.dump(2);
    }
}

void WebBrowserPanel::ImportHtmlDialog()
{
    std::string selected = PickHtmlFile();
    if (!selected.empty()) {
        ImportHtmlFile(selected);
    }
}

bool WebBrowserPanel::ImportHtmlFile(const std::string& srcFilePath)
{
    std::error_code ec;
    fs::path src(srcFilePath);
    if (!fs::exists(src, ec) || !fs::is_regular_file(src, ec))
        return false;

    fs::path baseDir = fs::path(ProyecThor::WebPath()) / "imported";
    fs::create_directories(baseDir, ec);

    std::string stem = src.stem().string();
    fs::path siteDir = baseDir / stem;
    fs::create_directories(siteDir, ec);

    fs::path destHtml = siteDir / src.filename();
    fs::copy_file(src, destHtml, fs::copy_options::overwrite_existing, ec);

    fs::path parentSrc = src.parent_path();
    if (fs::exists(parentSrc, ec) && parentSrc != siteDir) {
        for (const auto& entry : fs::directory_iterator(parentSrc, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)::tolower(c); });
            if (ext == ".css" || ext == ".js" || ext == ".png" || ext == ".jpg" ||
                ext == ".jpeg" || ext == ".gif" || ext == ".svg" || ext == ".webp" ||
                ext == ".json" || ext == ".woff" || ext == ".woff2" || ext == ".ttf" ||
                ext == ".mp4" || ext == ".webm") {
                fs::path destAsset = siteDir / entry.path().filename();
                fs::copy_file(entry.path(), destAsset, fs::copy_options::overwrite_existing, ec);
            }
        }
    }

    uintmax_t size = fs::file_size(destHtml, ec);

    ImportedHtmlItem item;
    item.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    item.title = stem;
    item.filePath = destHtml.generic_string();
    item.importedAt = GetCurrentTimestampStr();
    item.fileSize = size;
    item.isFavorite = false;

    auto it = std::find_if(m_ImportedHtmls.begin(), m_ImportedHtmls.end(),
        [&](const ImportedHtmlItem& x) { return x.filePath == item.filePath; });
    if (it != m_ImportedHtmls.end()) {
        *it = item;
    } else {
        m_ImportedHtmls.insert(m_ImportedHtmls.begin(), item);
    }

    SaveLibraryToDisk();
    NavigateToPathOrUrl(item.filePath);
    return true;
}

void WebBrowserPanel::DeleteImportedHtml(size_t index)
{
    if (index >= m_ImportedHtmls.size()) return;
    std::string pathToRemove = m_ImportedHtmls[index].filePath;
    m_ImportedHtmls.erase(m_ImportedHtmls.begin() + index);
    SaveLibraryToDisk();

    std::error_code ec;
    fs::path p(pathToRemove);
    fs::path baseImported = fs::path(ProyecThor::WebPath()) / "imported";
    if (fs::exists(p, ec) && p.string().find(baseImported.string()) != std::string::npos) {
        fs::remove(p, ec);
        fs::path parent = p.parent_path();
        if (fs::exists(parent, ec) && parent != baseImported && fs::is_empty(parent, ec)) {
            fs::remove(parent, ec);
        }
    }
}

void WebBrowserPanel::DeleteVisitedWeb(size_t index)
{
    if (index >= m_VisitedWebs.size()) return;
    m_VisitedWebs.erase(m_VisitedWebs.begin() + index);
    SaveLibraryToDisk();
}

void WebBrowserPanel::ClearHistory()
{
    m_VisitedWebs.clear();
    SaveLibraryToDisk();
}

void WebBrowserPanel::RenderHeader(float availW)
{
    (void)availW;
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("WEB & HTML LOCAL");
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void WebBrowserPanel::RenderNavigationControls(float availW)
{
    const float btnSz = 26.0f;
    const float goBtnW = 28.0f;
    const float gap = 3.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));

    if (ImGui::Button("<", ImVec2(btnSz, btnSz))) {
        m_WebView.GoBack();
    }
    ImGui::SameLine(0.0f, gap);
    if (ImGui::Button(">", ImVec2(btnSz, btnSz))) {
        m_WebView.GoForward();
    }
    ImGui::SameLine(0.0f, gap);
    if (ImGui::Button(reinterpret_cast<const char*>(u8"\u27F3"), ImVec2(btnSz, btnSz))) {
        m_WebView.Reload();
    }
    ImGui::SameLine(0.0f, gap);

    float fixedW = btnSz * 3.0f + goBtnW + gap * 4.0f;
    float urlW = std::max(60.0f, availW - fixedW);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));

    ImGui::SetNextItemWidth(urlW);
    bool enterPressed = ImGui::InputTextWithHint("##webUrl", "URL o ruta...", m_UrlBuf, sizeof(m_UrlBuf), ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleColor(4);
    ImGui::SameLine(0.0f, gap);

    bool goClicked = DS::GlassButton(reinterpret_cast<const char*>(u8"\u279C"), ImVec2(goBtnW, btnSz));
    if (enterPressed || goClicked) Go();

    ImGui::PopStyleVar(2);
}

void WebBrowserPanel::RenderActionButtons(float availW)
{
    const float gap = 4.0f;
    float btnW = std::floor((availW - gap) * 0.5f);

    if (DS::GlassButton("+ Montar HTML", ImVec2(btnW, 26.0f), DS::AccentColor)) {
        ImportHtmlDialog();
    }
    ImGui::SameLine(0.0f, gap);

    bool canSend = m_Navigated && m_WebView.IsAvailable();
    ImGui::BeginDisabled(!canSend);
    if (m_SendingToPublic) {
        if (DS::GlassButton(reinterpret_cast<const char*>(u8"\u23F9 Detener"), ImVec2(btnW, 26.0f), DS::DangerColor))
            StopSendToPublic();
    } else {
        if (DS::GlassButton(reinterpret_cast<const char*>(u8"\u25B6 En Vivo"), ImVec2(btnW, 26.0f), DS::SuccessColor))
            StartSendToPublic();
    }
    ImGui::EndDisabled();
}

void WebBrowserPanel::RenderViewModeBar(float availW)
{
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float stripH = 22.0f;
    ImVec2 p1(p0.x + availW, p0.y + stripH);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, IM_COL32(18, 20, 26, 255), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(40, 44, 56, 160), 4.0f);

    ImU32 dotCol = m_SendingToPublic ? DS::SuccessColor : DS::AccentColor;
    dl->AddCircleFilled(ImVec2(p0.x + 10.0f, p0.y + stripH * 0.5f), 3.5f, dotCol);

    const char* statusTxt = m_SendingToPublic ? "EN PANTALLA PÚBLICA" : "NAVEGADOR ACTIVO";
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 18.0f, p0.y + 3.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(m_SendingToPublic ? DS::SuccessColor : DS::TextSecondary));
    ImGui::TextUnformatted(statusTxt);
    ImGui::PopStyleColor();

    float toggleW = 54.0f;
    ImGui::SetCursorScreenPos(ImVec2(p1.x - toggleW - 4.0f, p0.y + 1.0f));
    const char* toggleLbl = m_FullWebView ? "[ ↕ Div ]" : "[ ⛶ Max ]";
    if (DS::GlassButton(toggleLbl, ImVec2(toggleW, 20.0f), m_FullWebView ? DS::AccentColorDim : DS::BtnDefaultFill)) {
        m_FullWebView = !m_FullWebView;
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + stripH));
    ImGui::Dummy(ImVec2(availW, 2.0f));
}

void WebBrowserPanel::RenderWebArea(float availW, float h)
{
    if (m_SendingToPublic) {
        UpdateSendToPublicBounds();
        return;
    }

    if (!m_WebView.IsAvailable()) {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ToVec4(DS::TextSecondary), "Se abrió en tu navegador externo.");
        if (DS::GlassButton("Volver a abrir", ImVec2(availW, 26.0f)))
            ProyecThor::External::OpenURL(m_UrlBuf);
        return;
    }

    if (m_WebView.HasError()) {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ToVec4(DS::DangerColor), "%s", m_WebView.GetLastError().c_str());
        return;
    }

    ImGui::BeginChild("##webArea", ImVec2(availW, h), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool ready = m_WebView.IsReady();
    if (!ready) {
        ImVec2 childAvail = ImGui::GetContentRegionAvail();
        ImVec2 center = { childAvail.x * 0.5f, childAvail.y * 0.40f };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        DrawLoadingSpinner(dl, { origin.x + center.x, origin.y + center.y }, 14.0f);

        const char* msg = "Cargando página...";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos({ center.x - ts.x * 0.5f, center.y + 20.0f });
        ImGui::TextColored(ToVec4(DS::TextSecondary), "%s", msg);
    }
    ImVec2 areaPos  = ImGui::GetWindowPos();
    ImVec2 areaSize = ImGui::GetWindowSize();
    ImGui::EndChild();

    if (ready)
        m_WebView.UpdateBounds(static_cast<int>(areaPos.x), static_cast<int>(areaPos.y),
                                static_cast<int>(areaSize.x), static_cast<int>(areaSize.y), true);
    else
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void WebBrowserPanel::RenderLibrarySection(float availW, float h)
{
    ImGui::BeginChild("##webLibrary", ImVec2(availW, h), false);

    const float gap = 4.0f;
    float tabW = std::floor((availW - gap) * 0.5f);

    ImU32 t0Col = (m_ActiveTab == 0) ? DS::AccentColor : DS::BtnDefaultFill;
    ImU32 t1Col = (m_ActiveTab == 1) ? DS::AccentColor : DS::BtnDefaultFill;

    std::string t0Label = "HTMLs (" + std::to_string(m_ImportedHtmls.size()) + ")";
    std::string t1Label = "Historial (" + std::to_string(m_VisitedWebs.size()) + ")";

    if (DS::GlassButton(t0Label.c_str(), ImVec2(tabW, 26.0f), t0Col)) m_ActiveTab = 0;
    ImGui::SameLine(0.0f, gap);
    if (DS::GlassButton(t1Label.c_str(), ImVec2(tabW, 26.0f), t1Col)) m_ActiveTab = 1;

    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##webSearch", "Buscar en biblioteca...", m_SearchFilter, sizeof(m_SearchFilter));
    ImGui::PopStyleVar();

    ImGui::Spacing();

    std::string filterStr = ToLower(m_SearchFilter);

    if (m_ActiveTab == 0) {
        if (DS::GlassButton("➕ Montar Archivo HTML...", ImVec2(availW, 28.0f), DS::SuccessColor)) {
            ImportHtmlDialog();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_ImportedHtmls.empty()) {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float boxH = 80.0f;
            ImVec2 p1(p0.x + availW, p0.y + boxH);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, p1, IM_COL32(20, 22, 28, 255), 6.0f);
            dl->AddRect(p0, p1, IM_COL32(40, 45, 58, 140), 6.0f);

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 10.0f, p0.y + 10.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextPrimary));
            ImGui::TextUnformatted("Sin archivos HTML montados.");
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 10.0f, p0.y + 30.0f));
            ImGui::PushTextWrapPos(p0.x + availW - 10.0f);
            ImGui::TextColored(ToVec4(DS::TextHint),
                "Carga páginas de bienvenida, cronómetros u obras interactivas locales sin requerir servidor.");
            ImGui::PopTextWrapPos();

            ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + boxH + 4.0f));
        } else {
            ImGui::BeginChild("##htmlList", ImVec2(availW, 0.0f), false);
            for (size_t i = 0; i < m_ImportedHtmls.size(); ++i) {
                const auto& item = m_ImportedHtmls[i];
                if (!filterStr.empty() && ToLower(item.title).find(filterStr) == std::string::npos &&
                    ToLower(item.filePath).find(filterStr) == std::string::npos) {
                    continue;
                }

                ImGui::PushID(static_cast<int>(i));

                ImVec2 p0 = ImGui::GetCursorScreenPos();
                float cardW = availW;
                float cardH = 58.0f;
                ImVec2 p1(p0.x + cardW, p0.y + cardH);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, p1, IM_COL32(24, 26, 32, 255), 5.0f);
                dl->AddRect(p0, p1, IM_COL32(48, 54, 68, 180), 5.0f);

                ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 5.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextPrimary));
                std::string titleTxt = "[HTML] " + item.title;
                ImGui::TextUnformatted(titleTxt.c_str());
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 22.0f));
                std::string metaStr = FormatFileSize(item.fileSize) + "  " + item.importedAt;
                ImGui::TextColored(ToVec4(DS::TextHint), "%s", metaStr.c_str());

                float btnH = 22.0f;
                float g = 3.0f;
                float bDel = 24.0f;
                float remW = cardW - 16.0f - bDel - g * 2.0f;
                float bOpen = std::floor(remW * 0.46f);
                float bLive = remW - bOpen;

                ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 32.0f));
                if (DS::GlassButton("Abrir", ImVec2(bOpen, btnH))) {
                    NavigateToPathOrUrl(item.filePath);
                }
                ImGui::SameLine(0.0f, g);
                if (DS::GlassButton(reinterpret_cast<const char*>(u8"\u25B6 En Vivo"), ImVec2(bLive, btnH), DS::SuccessColor)) {
                    NavigateToPathOrUrl(item.filePath);
                    StartSendToPublic();
                }
                ImGui::SameLine(0.0f, g);
                if (DS::GlassButton("X", ImVec2(bDel, btnH), DS::DangerColor)) {
                    DeleteImportedHtml(i);
                    ImGui::PopID();
                    break;
                }

                ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 5.0f));
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    } else {
        if (!m_VisitedWebs.empty()) {
            if (DS::GlassButton("Limpiar Historial", ImVec2(availW, 24.0f), DS::DangerColor)) {
                ClearHistory();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        if (m_VisitedWebs.empty()) {
            ImGui::TextColored(ToVec4(DS::TextHint), "Sin historial de páginas visitadas.");
        } else {
            ImGui::BeginChild("##historyList", ImVec2(availW, 0.0f), false);
            for (size_t i = 0; i < m_VisitedWebs.size(); ++i) {
                const auto& item = m_VisitedWebs[i];
                if (!filterStr.empty() && ToLower(item.url).find(filterStr) == std::string::npos) {
                    continue;
                }

                ImGui::PushID(static_cast<int>(i));

                ImVec2 p0 = ImGui::GetCursorScreenPos();
                float cardW = availW;
                float cardH = 46.0f;
                ImVec2 p1(p0.x + cardW, p0.y + cardH);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, p1, IM_COL32(22, 24, 30, 255), 5.0f);
                dl->AddRect(p0, p1, IM_COL32(44, 48, 62, 160), 5.0f);

                std::string displayUrl = item.url;
                if (displayUrl.length() > 28) displayUrl = displayUrl.substr(0, 26) + "...";

                ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 4.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextPrimary));
                ImGui::TextUnformatted(displayUrl.c_str());
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y + 22.0f));
                ImGui::TextColored(ToVec4(DS::TextHint), "%s", item.visitedAt.c_str());

                float btnH = 20.0f;
                float g = 3.0f;
                float bDel = 22.0f;
                float bOpen = 42.0f;

                ImGui::SetCursorScreenPos(ImVec2(p0.x + cardW - bOpen - bDel - g - 6.0f, p0.y + 13.0f));
                if (DS::GlassButton("Abrir", ImVec2(bOpen, btnH))) {
                    NavigateToPathOrUrl(item.url);
                }
                ImGui::SameLine(0.0f, g);
                if (DS::GlassButton("X", ImVec2(bDel, btnH), DS::DangerColor)) {
                    DeleteVisitedWeb(i);
                    ImGui::PopID();
                    break;
                }

                ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 4.0f));
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }

    ImGui::EndChild();
}

void WebBrowserPanel::Render()
{
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y;

    RenderHeader(availW);
    ImGui::Spacing();

    RenderNavigationControls(availW);
    ImGui::Spacing();

    RenderActionButtons(availW);
    ImGui::Spacing();

    if (m_Navigated) {
        RenderViewModeBar(availW);
        ImGui::Spacing();
    }

    float remH = ImGui::GetContentRegionAvail().y;

    if (!m_Navigated) {
        RenderLibrarySection(availW, remH);
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
    } else if (m_SendingToPublic) {
        UpdateSendToPublicBounds();
        RenderLibrarySection(availW, remH);
    } else if (m_FullWebView) {
        RenderWebArea(availW, remH);
    } else {
        float webH = std::clamp(remH * 0.46f, 150.0f, 300.0f);
        RenderWebArea(availW, webH);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float libH = ImGui::GetContentRegionAvail().y;
        RenderLibrarySection(availW, libH);
    }
}

} // namespace ProyecThor::UI

