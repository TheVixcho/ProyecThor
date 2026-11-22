#include "ui/framework/FilePicker.h"
#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include "core/PresentationCore.h"
#include "core/settings/SettingsManager.h"
#include "ui/views/Announcements.h"
#include "ui/views/Audio.h"
#include "ui/views/OClock.h"
#include "capture/CapturePanel.h"
#include "TeamChatPanel.h"
#include "MonitorTheme.h"
#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"
#include "ui/framework/LoadingSpinner.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/home/HomeIcons.h"
#include "ui/framework/IconRail.h"
#include "ui/framework/LiveContentRenderer.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include "ui/panels/overlay/OverlayLibraryTab.h"
#include "ui/panels/overlay/OverlayRecipeIO.h"
#include "ui/panels/overlay/OverlayTypes.h"
#include "core/AppPaths.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <string>
#include <iostream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include "stb_image.h"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace ProyecThor::UI {

namespace {

inline const char* TrVp(const char* es, const char* en, const char* pt) {
    auto lang = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language;
    if (lang == ProyecThor::Settings::Language::English) return en;
    if (lang == ProyecThor::Settings::Language::Portuguese) return pt;
    return es;
}

inline float SafeClamp(float v, float lo, float hi)
{
    if (std::isnan(v)) return lo;
    if (hi < lo) std::swap(lo, hi);
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

inline float CalcPanelPadX(float w)
{
    return w * 0.025f;
}

inline float CalcPanelGap(float w)
{
    return w * 0.016f;
}

struct StdStringCbData { std::string* str; };

static int StdStringResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* cb = static_cast<StdStringCbData*>(data->UserData);
        std::string* str = cb->str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

static bool InputTextMultilineStd(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0)
{
    if (str->capacity() == 0) {
        str->reserve(64);
    }
    flags |= ImGuiInputTextFlags_CallbackResize;
    StdStringCbData cb{ str };
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, StdStringResizeCallback, &cb);
}

namespace MT = MonitorTheme;
namespace fs = std::filesystem;
using namespace Design;
using namespace Components;

struct OverlayThumbEntry { std::string name, pngPath; };

fs::path OverlaysDirReadOnly() {
    return fs::path(ProyecThor::OverlaysPath());
}

std::vector<OverlayThumbEntry> ListSavedOverlays() {
    std::vector<OverlayThumbEntry> out;
    std::error_code ec;
    fs::path dir = OverlaysDirReadOnly();
    if (!fs::exists(dir, ec)) return out;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".overlay") continue;
        std::string name = e.path().stem().string();
        fs::path png = dir / (name + ".png");
        if (fs::exists(png)) out.push_back({ name, png.string() });
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

ImVec4 ToVec4(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }
ImVec4 Brighten(const ImVec4& c, float amount)
{
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

void DrawTransportGlyph(ImDrawList* dl, ImVec2 center, float sz, const std::string& key, ImU32 col)
{
    const float r = sz * 0.5f;
    const float t = std::max(1.3f, sz * 0.11f);
    if (key == "play") {
        dl->AddTriangleFilled(
            { center.x - r * 0.45f, center.y - r * 0.65f },
            { center.x - r * 0.45f, center.y + r * 0.65f },
            { center.x + r * 0.68f, center.y }, col);
    } else if (key == "pause") {
        const float bw = std::max(2.0f, r * 0.28f);
        const float bh = r * 0.62f;
        dl->AddRectFilled({ center.x - r * 0.48f, center.y - bh }, { center.x - r * 0.48f + bw, center.y + bh }, col, 1.0f);
        dl->AddRectFilled({ center.x + r * 0.48f - bw, center.y - bh }, { center.x + r * 0.48f, center.y + bh }, col, 1.0f);
    } else if (key == "stop") {
        const float hs = r * 0.52f;
        dl->AddRectFilled({ center.x - hs, center.y - hs }, { center.x + hs, center.y + hs }, col, 1.5f);
    } else if (key == "replay_10") {
        dl->PathArcTo(center, r * 0.68f, -2.2f, 3.3f, 16);
        dl->PathStroke(col, 0, t);
        ImVec2 tip = { center.x + std::cos(-2.2f) * r * 0.68f, center.y + std::sin(-2.2f) * r * 0.68f };
        dl->AddTriangleFilled(
            { tip.x - r * 0.30f, tip.y + r * 0.10f },
            { tip.x + r * 0.22f, tip.y - r * 0.25f },
            { tip.x + r * 0.18f, tip.y + r * 0.30f }, col);
    } else if (key == "forward_10") {
        dl->PathArcTo(center, r * 0.68f, -0.94f, 5.34f, 16);
        dl->PathStroke(col, 0, t);
        ImVec2 tip = { center.x + std::cos(-0.94f) * r * 0.68f, center.y + std::sin(-0.94f) * r * 0.68f };
        dl->AddTriangleFilled(
            { tip.x + r * 0.30f, tip.y + r * 0.10f },
            { tip.x - r * 0.22f, tip.y - r * 0.25f },
            { tip.x - r * 0.18f, tip.y + r * 0.30f }, col);
    } else if (key == "skip_prev" || key == "arrow_back" || key == "arrow_left") {
        dl->AddLine({ center.x - r * 0.56f, center.y - r * 0.56f },
                    { center.x - r * 0.56f, center.y + r * 0.56f }, col, t);
        dl->AddTriangleFilled(
            { center.x + r * 0.56f, center.y - r * 0.56f },
            { center.x + r * 0.56f, center.y + r * 0.56f },
            { center.x - r * 0.38f, center.y }, col);
    } else if (key == "skip_next" || key == "arrow_forward" || key == "arrow_right") {
        dl->AddLine({ center.x + r * 0.56f, center.y - r * 0.56f },
                    { center.x + r * 0.56f, center.y + r * 0.56f }, col, t);
        dl->AddTriangleFilled(
            { center.x - r * 0.56f, center.y - r * 0.56f },
            { center.x - r * 0.56f, center.y + r * 0.56f },
            { center.x + r * 0.38f, center.y }, col);
    }
}

bool DrawPadButton(const char* iconName, float iconSize, ImVec4 /*padColor*/, ImVec2 btnSize, bool lit,
                   DrawIconFn vectorIcon = nullptr)
{
    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::InvisibleButton("##tpBtn", btnSize);
    bool   hovered = ImGui::IsItemHovered();
    bool   isHeld  = ImGui::IsItemActive();
    ImVec2 p1      = { p0.x + btnSize.x, p0.y + btnSize.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bgCol = DS::BtnDefaultFill;
    if (isHeld) {
        bgCol = IM_COL32(54, 56, 60, 255);
    } else if (lit) {
        ImVec4 ac = ToVec4(DS::AccentLight);
        ImVec4 bf = ToVec4(DS::BtnDefaultFill);
        float mix = hovered ? 0.22f : 0.14f;
        bgCol = ImGui::ColorConvertFloat4ToU32({
            bf.x + (ac.x - bf.x) * mix,
            bf.y + (ac.y - bf.y) * mix,
            bf.z + (ac.z - bf.z) * mix, 1.0f });
    } else if (hovered) {
        bgCol = DS::BtnHoverFill;
    }

    ImU32 bordCol = lit
        ? DS::AccentLight
        : (hovered ? IM_COL32(255, 255, 255, 48) : IM_COL32(255, 255, 255, 22));

    dl->AddRectFilled(p0, p1, bgCol, DS::RadiusMedium);
    dl->AddRect(p0, p1, bordCol, DS::RadiusMedium, 0, lit ? 1.3f : 1.0f);

    ImVec2 center = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
    ImU32 iconTint = (lit || hovered) ? DS::TextPrimary : DS::TextSecondary;

    std::string key = iconName ? iconName : "";
    if (key == "arrow_left") key = "arrow_back";
    else if (key == "arrow_right") key = "arrow_forward";
    else if (key == "skip_previous") key = "skip_prev";

    if (vectorIcon) {
        ImVec2 origin = { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f };
        vectorIcon(dl, origin, iconSize, iconTint);
    } else {
        auto it = StyleGeneralApp::Icons.find(key);
        if (it == StyleGeneralApp::Icons.end() && iconName)
            it = StyleGeneralApp::Icons.find(iconName);
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr) {
            dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
                { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f },
                { center.x + iconSize * 0.5f, center.y + iconSize * 0.5f },
                ImVec2(0, 0), ImVec2(1, 1), iconTint);
        } else {
            DrawTransportGlyph(dl, center, iconSize, key, iconTint);
        }
    }

    return pressed;
}

bool HorizontalFader(const char* id, float* value, float lo, float hi, ImVec2 size,
                      ImU32 trackCol, ImU32 fillCol, ImU32 capCol)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;

    const float capW       = 14.0f;
    const float trackLeft  = pos.x + capW * 0.5f;
    const float trackRight = pos.x + size.x - capW * 0.5f;
    const float trackWpx   = std::max(1.0f, trackRight - trackLeft);

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hi > lo)
    {
        float t = std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / trackWpx, 0.0f, 1.0f);
        float newVal = lo + t * (hi - lo);
        if (newVal != *value) { *value = newVal; changed = true; }
    }

    float frac = (hi > lo) ? std::clamp((*value - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    float capX = trackLeft + frac * trackWpx;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    const float trackH  = 6.0f;
    float       cy      = pos.y + size.y * 0.5f;

    dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { trackRight, cy + trackH * 0.5f },
                       trackCol, trackH * 0.5f);

    if (capX - trackLeft > 0.5f)
        dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { capX, cy + trackH * 0.5f },
                           fillCol, trackH * 0.5f);

    for (int i = 0; i <= 4; i++)
    {
        float mx = trackLeft + trackWpx * (float)i / 4.0f;
        dl->AddLine({ mx, cy - size.y * 0.30f }, { mx, cy - trackH * 0.7f },
                     IM_COL32(255, 255, 255, 35), 1.0f);
    }

    float  capHalfH = size.y * 0.40f;
    ImVec2 capMin   = { capX - capW * 0.5f, cy - capHalfH };
    ImVec2 capMax   = { capX + capW * 0.5f, cy + capHalfH };
    ImU32  capBody  = (hovered || active)
        ? ImGui::GetColorU32(Brighten(ToVec4(capCol), 0.10f))
        : capCol;

    dl->AddRectFilled(capMin, capMax, capBody, 3.0f);
    dl->AddRect(capMin, capMax, IM_COL32(0, 0, 0, 110), 3.0f, 0, 1.2f);
    dl->AddLine({ capX, capMin.y + 4.0f }, { capX, capMax.y - 4.0f }, IM_COL32(0, 0, 0, 130), 1.5f);

    return changed;
}

bool HybridVolumeFader(const char* id, float* value, float lo, float hi, ImVec2 size,
                       bool muted, bool enabled)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    bool changed = false;

    const float capW       = size.y * 0.38f;
    const float trackLeft  = pos.x + capW * 0.5f;
    const float trackRight = pos.x + size.x - capW * 0.5f;
    const float trackWpx   = std::max(1.0f, trackRight - trackLeft);

    if (enabled && hi > lo)
    {
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            float unity = SafeClamp(1.0f, lo, hi);
            if (*value != unity) { *value = unity; changed = true; }
        }
        else if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float t = SafeClamp((ImGui::GetIO().MousePos.x - trackLeft) / trackWpx, 0.0f, 1.0f);
            float newVal = lo + t * (hi - lo);
            if (std::abs(newVal - 1.0f) < 0.022f) newVal = 1.0f;
            if (newVal != *value) { *value = newVal; changed = true; }
        }
    }

    const float shownVal  = enabled ? *value : 0.0f;
    const float frac      = (hi > lo) ? SafeClamp((shownVal - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    const float unityFrac = (hi > lo) ? SafeClamp((1.0f - lo) / (hi - lo), 0.0f, 1.0f) : 0.5f;
    const float capX      = trackLeft + frac * trackWpx;
    const float unityX    = trackLeft + unityFrac * trackWpx;

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    const float trackH = size.y * 0.30f;
    const float cy     = pos.y + size.y * 0.56f;
    const float trkTop = cy - trackH * 0.5f;
    const float trkBot = cy + trackH * 0.5f;
    const float trkRad = trackH * 0.5f;

    // Escala clásica de calibración proporcional a la altura del control
    const float tickGap = size.y * 0.055f;
    for (int i = 0; i <= 8; ++i)
    {
        const float mx      = trackLeft + trackWpx * static_cast<float>(i) / 8.0f;
        const bool  isUnity = (i == 4);
        const bool  isMajor = (i % 2 == 0);
        const float tickLen = size.y * (isUnity ? 0.16f : (isMajor ? 0.11f : 0.065f));
        const ImU32 tickCol = isUnity
            ? IM_COL32(255, 255, 255, enabled ? 96 : 28)
            : IM_COL32(255, 255, 255, enabled ? (isMajor ? 40 : 22) : 12);
        dl->AddLine({ mx, trkTop - tickGap - tickLen }, { mx, trkTop - tickGap * 0.6f },
                    tickCol, isUnity ? 1.4f : 1.0f);
    }

    // Riel hundido estilo consola + borde fino minimalista
    dl->AddRectFilled({ trackLeft, trkTop }, { trackRight, trkBot },
                      IM_COL32(16, 17, 20, 255), trkRad);
    dl->AddRect({ trackLeft, trkTop }, { trackRight, trkBot },
                IM_COL32(255, 255, 255, (hovered && enabled) ? 44 : 24), trkRad);

    // Relleno de nivel: verde estudio hasta 100%, segmento coral de ganancia extra (>100%)
    if (enabled && capX - trackLeft > 0.5f)
    {
        const bool  isBoost = (shownVal > 1.005f);
        const float baseEnd = isBoost ? std::min(capX, unityX) : capX;
        const ImU32 baseCol = muted
            ? IM_COL32(92, 98, 108, 145)
            : IM_COL32(64, 176, 126, 235);

        if (baseEnd - trackLeft > 0.5f)
        {
            dl->AddRectFilled({ trackLeft, trkTop }, { baseEnd, trkBot }, baseCol, trkRad);
        }
        if (isBoost && capX - unityX > 0.5f)
        {
            const ImU32 boostCol = muted
                ? IM_COL32(138, 80, 80, 155)
                : IM_COL32(224, 84, 84, 240);
            dl->AddRectFilled({ std::max(trackLeft, unityX - 1.0f), trkTop },
                              { capX, trkBot }, boostCol, trkRad);
        }
    }

    // Línea divisoria sutil en 100% dentro del riel
    dl->AddLine({ unityX, trkTop + 1.0f }, { unityX, trkBot - 1.0f },
                IM_COL32(255, 255, 255, enabled ? 36 : 14), 1.0f);

    // Capuchón táctil híbrido proporcional (perfil de fader clásico con acabado mate minimalista)
    const float capHalfH = size.y * 0.40f;
    const float capRad   = capW * 0.22f;
    const ImVec2 capMin  = { capX - capW * 0.5f, cy - capHalfH };
    const ImVec2 capMax  = { capX + capW * 0.5f, cy + capHalfH };

    ImU32 capBody = IM_COL32(82, 86, 94, 185);
    if (enabled)
    {
        if (muted) {
            capBody = (hovered || active) ? IM_COL32(160, 165, 174, 250) : IM_COL32(138, 143, 152, 245);
        } else if (shownVal > 1.005f) {
            capBody = (hovered || active) ? IM_COL32(250, 118, 118, 255) : IM_COL32(235, 96, 96, 255);
        } else {
            capBody = (hovered || active) ? IM_COL32(238, 241, 247, 255) : IM_COL32(212, 216, 224, 255);
        }
    }

    dl->AddRectFilled(capMin, capMax, capBody, capRad);
    dl->AddRect(capMin, capMax, IM_COL32(10, 11, 14, 195), capRad, 0, 1.1f);
    const float gripInset = capHalfH * 0.34f;
    dl->AddLine({ capX, capMin.y + gripInset }, { capX, capMax.y - gripInset },
                IM_COL32(20, 22, 26, enabled ? 175 : 100), std::max(1.2f, capW * 0.09f));

    return changed;
}

void DrawSpeakerShape(ImDrawList* dl, ImVec2 o, float sz, ImU32 col, bool muted)
{
    ImVec2 c = { o.x + sz * 0.5f, o.y + sz * 0.5f };

    float  boxHalfH = sz * 0.16f;
    ImVec2 boxMin   = { c.x - sz * 0.42f, c.y - boxHalfH };
    ImVec2 boxMax   = { c.x - sz * 0.16f, c.y + boxHalfH };
    dl->AddRectFilled(boxMin, boxMax, col, 1.0f);

    ImVec2 apex    = { boxMax.x, c.y };
    ImVec2 baseTop = { c.x + sz * 0.16f, c.y - sz * 0.34f };
    ImVec2 baseBot = { c.x + sz * 0.16f, c.y + sz * 0.34f };
    dl->AddTriangleFilled(apex, baseTop, baseBot, col);

    if (muted)
    {
        dl->AddLine({ o.x + sz * 0.06f, o.y + sz * 0.94f },
                    { o.x + sz * 0.94f, o.y + sz * 0.06f }, col, sz * 0.09f);
    }
    else
    {
        for (int i = 1; i <= 2; i++)
        {
            float r = sz * (0.14f + 0.13f * (float)i);
            dl->PathArcTo({ c.x + sz * 0.10f, c.y }, r, -0.62f, 0.62f, 10);
            dl->PathStroke(col, 0, sz * 0.055f);
        }
    }
}
void DrawIcon_SpeakerOn(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)    { DrawSpeakerShape(dl, o, sz, col, false); }
void DrawIcon_SpeakerMuted(ImDrawList* dl, ImVec2 o, float sz, ImU32 col) { DrawSpeakerShape(dl, o, sz, col, true);  }

void DrawIcon_Disc(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    ImVec2 center = { o.x + sz * 0.5f, o.y + sz * 0.5f };
    dl->AddCircle(center, sz * 0.40f, col, 24, sz * 0.06f);
    dl->AddCircle(center, sz * 0.24f, col, 20, sz * 0.035f);
    dl->AddCircleFilled(center, sz * 0.07f, col, 12);
}

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

bool QuickActionButton(const char* id, const char* iconKey, DrawIconFn vectorIcon,
                       const char* fallbackGlyph, const char* tooltip, ImVec2 size,
                       ImVec4 bgColor, ImVec4 hoverColor, ImVec4 activeColor, ImVec4 tint,
                       bool toggledOn)
{
    constexpr float rounding = 6.0f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 bMin   = cursor;
    ImVec2 bMax   = { cursor.x + size.x, cursor.y + size.y };

    ImGui::SetCursorScreenPos(bMin);
    const std::string btnId = std::string("##btn_") + id;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), size);
    bool hovered = ImGui::IsItemHovered();
    float t = hovered ? 1.0f : 0.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (toggledOn) {
        ImVec4 ac = activeColor;
        ac.w = 0.18f;
        dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        ac.w = 0.35f;
        dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);

        float barW = std::max(size.x - 12.0f, 10.0f);
        float barX0 = cursor.x + (size.x - barW) * 0.5f;
        dl->AddRectFilled({ barX0, bMax.y - 2.5f }, { barX0 + barW, bMax.y },
                          ImGui::ColorConvertFloat4ToU32(tint), 1.5f);
    } else {
        if (hovered) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 18), rounding);
        } else {
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(bgColor), rounding);
        }
    }

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    bool hasIcon    = hasTexture || vectorIcon != nullptr;

    ImVec4 textPriV = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
    ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
    float  brightT  = toggledOn ? 1.0f : t;
    ImVec4 icF = {
        Lerp(textDimV.x, textPriV.x, brightT),
        Lerp(textDimV.y, textPriV.y, brightT),
        Lerp(textDimV.z, textPriV.z, brightT),
        1.0f
    };
    if (toggledOn) {
        icF.x = Lerp(icF.x, tint.x, 0.35f);
        icF.y = Lerp(icF.y, tint.y, 0.35f);
        icF.z = Lerp(icF.z, tint.z, 0.35f);
        icF.w = 1.0f;
    }
    ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(icF);

    if (hasTexture)
    {
        const float iconSide = std::min(size.x, size.y) * 0.44f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f - (toggledOn ? 1.0f : 0.0f) };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        dl->AddImage((ImTextureID)(intptr_t)it->second.textureID, pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1), iconColor);
    }
    else if (vectorIcon)
    {
        const float iconSide = std::min(size.x, size.y) * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f - (toggledOn ? 1.0f : 0.0f) };
        const ImVec2 origin  = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        vectorIcon(dl, origin, iconSide, iconColor);
    }
    else if (fallbackGlyph && fallbackGlyph[0] != '\0')
    {
        ImFont* font = ImGui::GetFont();
        float fontSz = std::min(size.x, size.y) * 0.52f;
        ImVec2 glyphSz = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, fallbackGlyph);
        ImVec2 glyphPos = { (bMin.x + bMax.x - glyphSz.x) * 0.5f, (bMin.y + bMax.y - glyphSz.y) * 0.5f - (toggledOn ? 1.0f : 0.0f) };
        dl->AddText(font, fontSz, glyphPos, iconColor, fallbackGlyph);
    }

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

void DrawIcon_FolderLoop(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.07f);
    ImVec2 p0 = AppIcons::IcPt(o, sz, 0.12f, 0.26f);
    ImVec2 p1 = AppIcons::IcPt(o, sz, 0.88f, 0.82f);
    // Pestaña superior de carpeta
    ImVec2 tab[4] = {
        AppIcons::IcPt(o, sz, 0.12f, 0.28f),
        AppIcons::IcPt(o, sz, 0.18f, 0.16f),
        AppIcons::IcPt(o, sz, 0.42f, 0.16f),
        AppIcons::IcPt(o, sz, 0.48f, 0.26f)
    };
    dl->AddPolyline(tab, 4, col, ImDrawFlags_None, thick);
    dl->AddRect(p0, p1, col, sz * 0.06f, ImDrawFlags_RoundCornersAll, thick);

    // Triángulo Play en el interior
    dl->AddTriangleFilled(
        AppIcons::IcPt(o, sz, 0.43f, 0.41f),
        AppIcons::IcPt(o, sz, 0.43f, 0.67f),
        AppIcons::IcPt(o, sz, 0.64f, 0.54f), col);
}

void DrawIcon_PlusKey(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.4f, sz * 0.10f);
    ImVec2 c = { o.x + sz * 0.5f, o.y + sz * 0.5f };
    float r = sz * 0.34f;
    dl->AddLine({ c.x - r, c.y }, { c.x + r, c.y }, col, thick);
    dl->AddLine({ c.x, c.y - r }, { c.x, c.y + r }, col, thick);
}

void DrawIcon_CaptureSnap(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.08f);
    float m = 0.14f, b = 0.32f;
    // Esquinas tipo visor broadcast [ + ]
    dl->AddLine(AppIcons::IcPt(o, sz, m, b), AppIcons::IcPt(o, sz, m, m), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, m, m), AppIcons::IcPt(o, sz, b, m), col, thick);

    dl->AddLine(AppIcons::IcPt(o, sz, 1.0f - b, m), AppIcons::IcPt(o, sz, 1.0f - m, m), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 1.0f - m, m), AppIcons::IcPt(o, sz, 1.0f - m, b), col, thick);

    dl->AddLine(AppIcons::IcPt(o, sz, m, 1.0f - b), AppIcons::IcPt(o, sz, m, 1.0f - m), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, m, 1.0f - m), AppIcons::IcPt(o, sz, b, 1.0f - m), col, thick);

    dl->AddLine(AppIcons::IcPt(o, sz, 1.0f - b, 1.0f - m), AppIcons::IcPt(o, sz, 1.0f - m, 1.0f - m), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 1.0f - m, 1.0f - m), AppIcons::IcPt(o, sz, 1.0f - m, 1.0f - b), col, thick);

    // Cruz central
    dl->AddLine(AppIcons::IcPt(o, sz, 0.36f, 0.50f), AppIcons::IcPt(o, sz, 0.64f, 0.50f), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.50f, 0.36f), AppIcons::IcPt(o, sz, 0.50f, 0.64f), col, thick);
}

void DrawIcon_Trash(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.2f, sz * 0.075f);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.18f, 0.26f), AppIcons::IcPt(o, sz, 0.82f, 0.26f), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.38f, 0.16f), AppIcons::IcPt(o, sz, 0.62f, 0.16f), col, thick);
    dl->AddRect(AppIcons::IcPt(o, sz, 0.26f, 0.26f), AppIcons::IcPt(o, sz, 0.74f, 0.84f),
                col, sz * 0.05f, ImDrawFlags_RoundCornersBottom, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.42f, 0.40f), AppIcons::IcPt(o, sz, 0.42f, 0.70f), col, thick * 0.85f);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.58f, 0.40f), AppIcons::IcPt(o, sz, 0.58f, 0.70f), col, thick * 0.85f);
}

void DrawIcon_PlayTri(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    dl->AddTriangleFilled(
        AppIcons::IcPt(o, sz, 0.26f, 0.18f),
        AppIcons::IcPt(o, sz, 0.26f, 0.82f),
        AppIcons::IcPt(o, sz, 0.80f, 0.50f), col);
}

void DrawIcon_CloseX(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    float thick = std::max(1.3f, sz * 0.09f);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.24f, 0.24f), AppIcons::IcPt(o, sz, 0.76f, 0.76f), col, thick);
    dl->AddLine(AppIcons::IcPt(o, sz, 0.76f, 0.24f), AppIcons::IcPt(o, sz, 0.24f, 0.76f), col, thick);
}

bool DrawToggleSwitch(const char* id, bool* v, const ImVec4& accent)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = 30.0f;
    const float h = 16.0f;
    const float r = h * 0.5f;

    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    if (clicked) *v = !(*v);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 trackCol = *v
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, hovered ? 0.95f : 0.82f))
        : (hovered ? IM_COL32(62, 68, 84, 255) : IM_COL32(42, 46, 58, 255));
    dl->AddRectFilled(p, { p.x + w, p.y + h }, trackCol, r);
    dl->AddRect(p, { p.x + w, p.y + h }, IM_COL32(255, 255, 255, *v ? 45 : 22), r);

    float knobX = *v ? (p.x + w - r) : (p.x + r);
    dl->AddCircleFilled({ knobX, p.y + r }, r - 2.2f, IM_COL32(250, 252, 255, 255), 16);
    return clicked;
}

std::string TruncateUTF8ToWidth(ImFont* font, float fontSize, const std::string& s, float maxW)
{
    if (s.empty() || maxW <= 4.0f) return "";
    if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, s.c_str()).x <= maxW) return s;

    const char* ell = "..";
    float ellW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ell).x;
    if (ellW >= maxW) return "";

    size_t pos = s.size();
    while (pos > 0) {
        --pos;
        while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) --pos;
        std::string cand = s.substr(0, pos) + ell;
        if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, cand.c_str()).x <= maxW) {
            return cand;
        }
    }
    return "";
}

static constexpr float kPadPalette[8][4] = {
    { 0.94f, 0.30f, 0.42f, 1.0f }, // Coral / Live
    { 0.26f, 0.56f, 0.98f, 1.0f }, // Azul
    { 0.18f, 0.80f, 0.52f, 1.0f }, // Esmeralda
    { 0.96f, 0.66f, 0.18f, 1.0f }, // Ámbar
    { 0.64f, 0.38f, 0.96f, 1.0f }, // Violeta
    { 0.14f, 0.76f, 0.88f, 1.0f }, // Cian
    { 0.90f, 0.34f, 0.78f, 1.0f }, // Magenta
    { 0.42f, 0.50f, 0.96f, 1.0f }, // Índigo
};

using PadIconDrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);
struct PadIconEntry { const char* name; PadIconDrawFn draw; };

static const PadIconEntry kPadIcons[] = {
    { "Mixer",      AppIcons::DrawIcon_Mixer     },
    { "Monitor",    AppIcons::DrawIcon_Monitor   },
    { "Capas",      AppIcons::DrawIcon_Layers    },
    { "Paleta",     AppIcons::DrawIcon_Palette   },
    { "Overlay",    AppIcons::DrawIcon_Overlay   },
    { "Tipografía", AppIcons::DrawIcon_TextAa    },
    { "Transición", AppIcons::DrawIcon_Swap      },
    { "Efecto",     AppIcons::DrawIcon_Shader    },
    { "Inicio",     HomeIcons::DrawIcon_Home     },
    { "Reloj",      HomeIcons::DrawIcon_Clock    },
    { "Anuncios",   HomeIcons::DrawIcon_Megaphone},
    { "Notas",      HomeIcons::DrawIcon_Notepad  },
    { "Cámara",     HomeIcons::DrawIcon_Camera   },
    { "Red",        HomeIcons::DrawIcon_Broadcast},
    { "Chat",       HomeIcons::DrawIcon_Chat     },
    { "Carpeta",    DrawIcon_FolderLoop          },
    { "Imagen",     AppIcons::DrawIcon_Image     },
    { "Destello",   HomeIcons::DrawIcon_Sparkle  },
    { "Audio",      AppIcons::DrawIcon_Disc      },
    { "Proyector",  AppIcons::DrawIcon_Projector },
};
static constexpr int kPadIconCount = (int)(sizeof(kPadIcons) / sizeof(kPadIcons[0]));

const PadIconEntry& PadIconFor(int index)
{
    return kPadIcons[std::clamp(index, 0, kPadIconCount - 1)];
}

bool RenderPadIconGrid(int& iconIndex)
{
    bool changed = false;
    const int   cols    = 5;
    const float cellSz  = 32.0f;
    const float spacing = 5.0f;

    for (int i = 0; i < kPadIconCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        const bool sel = (i == iconIndex);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? MT::k_PrevBtn : ImVec4(1, 1, 1, 0.05f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_PrevBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Border, sel ? ImVec4(0.45f, 0.72f, 1.0f, 0.85f) : ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool clicked = ImGui::Button("##padIcon", ImVec2(cellSz, cellSz));
        ImVec2 p = ImGui::GetItemRectMin();
        ImVec2 s = ImGui::GetItemRectSize();
        float  iconSz = cellSz * 0.54f;
        kPadIcons[i].draw(ImGui::GetWindowDrawList(),
                          { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f },
                          iconSz,
                          sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 222, 238, 225));

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", kPadIcons[i].name);

        if (clicked) { iconIndex = i; changed = true; }
        ImGui::PopID();
    }
    return changed;
}

using PadSettings = ProyecThor::Settings::PadSettings;

std::string CleanShortLabel(const std::string& raw)
{
    std::string line;
    for (char c : raw) {
        if (c == '\r' || c == '\n') {
            if (!line.empty()) break;
            continue;
        }
        line.push_back(c);
    }
    size_t a = line.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = line.find_last_not_of(" \t");
    line = line.substr(a, b - a + 1);
    // Limitar longitud segura en UTF-8 (~18 bytes)
    if (line.size() > 18) {
        size_t p = 18;
        while (p > 0 && ((unsigned char)line[p] & 0xC0) == 0x80) --p;
        line = line.substr(0, p);
    }
    return line;
}

void SavePad(PadSettings& pad, int padIndex = 0)
{
    auto& core = Core::PresentationCore::Get();
    auto state = core.GetState();
    const bool wasAssigned = pad.assigned;

    // 1. Fondo actual
    pad.hasBackground   = (state.bgType != Core::PresentationState::BackgroundType::SolidColor ||
                           state.bgColor[0] > 0.0f || state.bgColor[1] > 0.0f || state.bgColor[2] > 0.0f ||
                           !state.bgPath.empty());
    pad.applyBackground = pad.hasBackground;
    pad.bgType          = (int)state.bgType;
    pad.bgPath          = state.bgPath;
    for (int c = 0; c < 3; c++) pad.bgColor[c] = state.bgColor[c];

    if (!state.bgPath.empty()) {
        try {
            std::filesystem::path p = std::filesystem::u8path(state.bgPath);
            if (p.has_parent_path()) {
                pad.bgFolderName = p.parent_path().filename().u8string();
            }
        } catch (...) {}
    }

    // 2. Letra / Texto actual en vivo
    pad.hasText   = state.showText && !state.currentText.empty();
    pad.applyText = pad.hasText;
    pad.text      = state.currentText;
    auto sel      = core.PeekSelection();
    pad.textTitle = sel.title;

    // 3. Estilo tipográfico
    pad.hasStyle       = true;
    pad.applyStyle     = true;
    pad.styleSize      = state.textSize;
    for (int c = 0; c < 4; c++) pad.styleColor[c] = state.textColor[c];
    pad.styleHAlign    = state.textAlignment;
    pad.styleVAlign    = state.vAlignment;
    for (int c = 0; c < 4; c++) pad.styleMargins[c] = state.margins[c];
    pad.styleAutoScale = state.autoScale;
    pad.styleFontName  = state.selectedFont;

    // 4. Captura
    if (auto* cap = core.GetCapturePanelRef()) {
        pad.hasCapture   = cap->SnapshotCurrentCapture(pad.capture);
        pad.applyCapture = pad.hasCapture;
    } else {
        pad.hasCapture   = false;
        pad.applyCapture = false;
    }

    // Auto-identidad limpia al guardar en un pad vacío
    if (!wasAssigned) {
        const float* pal = kPadPalette[std::clamp(padIndex, 0, 7)];
        for (int c = 0; c < 4; c++) pad.color[c] = pal[c];

        if (pad.hasCapture)                            pad.iconIndex = 12; // Cámara
        else if (pad.bgType == 4)                      pad.iconIndex = 15; // Carpeta
        else if (pad.hasText && pad.hasBackground)     pad.iconIndex = 2;  // Capas
        else if (pad.hasText)                          pad.iconIndex = 5;  // Tipografía
        else if (pad.bgType == 2)                      pad.iconIndex = 18; // Audio
        else                                           pad.iconIndex = 16; // Imagen / Fondo

        if (pad.name.empty()) {
            std::string autoLbl;
            if (pad.hasText && !pad.text.empty()) {
                autoLbl = CleanShortLabel(pad.text);
            } else if (!pad.bgPath.empty()) {
                try {
                    autoLbl = CleanShortLabel(std::filesystem::u8path(pad.bgPath).stem().u8string());
                } catch (...) {}
            }
            pad.name = autoLbl;
        }
    }

    pad.assigned = true;
    ProyecThor::Settings::SettingsManager::Get().Save();
}

void ApplyPad(ViewPanel* panel, const PadSettings& pad)
{
    if (!pad.assigned) return;
    auto& core = Core::PresentationCore::Get();
    using BgType = Core::PresentationState::BackgroundType;

    core.SetProjecting(true);

    // 1. Captura
    if (pad.applyCapture && pad.hasCapture) {
        if (auto* cap = core.GetCapturePanelRef())
            cap->ApplyCaptureScene(pad.capture);
    }

    // 2. Estilo tipográfico
    if (pad.applyStyle && pad.hasStyle) {
        Core::TextBoxStyle box;
        box.sizeW    = std::max(0.02f, (1920.0f - pad.styleMargins[0] - pad.styleMargins[2]) / 1920.0f);
        box.sizeH    = std::max(0.02f, (1080.0f - pad.styleMargins[1] - pad.styleMargins[3]) / 1080.0f);
        box.posX     = pad.styleMargins[0] / 1920.0f + box.sizeW * 0.5f;
        box.posY     = pad.styleMargins[1] / 1080.0f + box.sizeH * 0.5f;
        box.textSize = pad.styleSize;
        for (int c = 0; c < 4; c++) box.color[c] = pad.styleColor[c];
        box.hAlign    = pad.styleHAlign;
        box.vAlign    = pad.styleVAlign;
        box.autoScale = pad.styleAutoScale;
        box.fontName  = pad.styleFontName;

        Core::SavedStyle snap;
        snap.lyrics = box;
        core.ApplyStyleSnapshot(snap);
    }

    // 3. Fondo (Video, Imagen, Color sólido o Carpeta de videos en bucle)
    if (pad.applyBackground && pad.hasBackground) {
        if (pad.bgType == 4) { // VideoFolder
            std::vector<std::string> vids;
            std::filesystem::path folderPath;

            if (!pad.bgPath.empty() && std::filesystem::exists(std::filesystem::u8path(pad.bgPath)) &&
                std::filesystem::is_directory(std::filesystem::u8path(pad.bgPath))) {
                folderPath = std::filesystem::u8path(pad.bgPath);
            } else if (!pad.bgFolderName.empty()) {
                std::filesystem::path root = std::filesystem::u8path(ProyecThor::GetAssetsPath()) / "backgrounds";
                if (std::filesystem::exists(root / pad.bgFolderName)) {
                    folderPath = root / pad.bgFolderName;
                } else if (pad.bgFolderName == "Todos") {
                    folderPath = root;
                }
            }

            if (!folderPath.empty() && std::filesystem::exists(folderPath)) {
                try {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
                        if (!entry.is_regular_file()) continue;
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                        if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".webm" || ext == ".avi" || ext == ".m4v") {
                            vids.push_back(entry.path().u8string());
                        }
                    }
                    std::sort(vids.begin(), vids.end());
                } catch (...) {}
            }

            if (panel) {
                panel->SetFolderPlaylist(vids);
            }

            if (!vids.empty()) {
                core.SetBackgroundMedia(vids[0], true, false);
            }
        } else {
            if (panel) {
                panel->ClearFolderPlaylist();
            }

            switch ((BgType)pad.bgType) {
                case BgType::SolidColor:
                    core.SetLayer0_Color(pad.bgColor[0], pad.bgColor[1], pad.bgColor[2]);
                    break;
                case BgType::Video:
                    core.SetBackgroundMedia(pad.bgPath, true, false);
                    break;
                case BgType::Audio:
                    core.SetBackgroundAudio();
                    break;
                default:
                    core.SetBackgroundMedia(pad.bgPath, false, false);
                    break;
            }
        }
    }

    // 4. Letra / Texto / Palabra fija
    if (pad.applyText && pad.hasText) {
        if (!pad.text.empty()) {
            core.SetLayer2_Text(pad.text, -1, false);
        } else {
            core.ClearLayer2();
        }
    }
}
}

ViewPanel::ViewPanel(UIManager* uiManager)
    : m_UIManager(uiManager)
{
    m_IsDetached = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.viewPanelDetached;
}

void ViewPanel::SetDetached(bool detached)
{
    m_IsDetached = detached;
    auto& gen = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    if (gen.viewPanelDetached != detached) {
        gen.viewPanelDetached = detached;
        ProyecThor::Settings::SettingsManager::Get().Save();
    }
}

bool ViewPanel::HasPlayableLiveMedia() const
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();

    UI::AudioPanel* musicPanel = core.GetAudioPanelRef();
    if (musicPanel && musicPanel->IsLiveBackground() &&
        state.bgType == Core::PresentationState::BackgroundType::Audio)
    {
        return true;
    }

    if (state.bgType == Core::PresentationState::BackgroundType::Video &&
        !state.bgPath.empty() &&
        !core.IsBackgroundStaticImage() &&
        core.GetBackgroundPlayer() != nullptr)
    {
        return true;
    }

    return false;
}

void ViewPanel::RenderMonitorHeader(float w, float h)
{
    if (w <= 0.0f || h <= 0.0f) return;

    auto& core      = Core::PresentationCore::Get();
    auto  state     = core.GetState();
    const bool isPro = true;
    if (!isPro && m_PreviewSource == PreviewSource::Stage) {
        m_PreviewSource = PreviewSource::Publico;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::BeginChild("##viewMonitorHeader", ImVec2(w, h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    ImFont*      font     = ImGui::GetFont();
    const float  baseFont = ImGui::GetFontSize();
    const ImVec2 org      = ImGui::GetWindowPos();
    const float  padX     = CalcPanelPadX(w);
    const float  gap      = CalcPanelGap(w) * 0.75f;
    const float  midY     = org.y + h * 0.5f;

    dl->AddLine({ org.x, org.y + h - 1.0f }, { org.x + w, org.y + h - 1.0f },
                IM_COL32(255, 255, 255, 16), 1.0f);

    // ── Indicador de estado en vivo a la izquierda ───────────────────────────
    const ImU32 dotCol = state.isProjecting
        ? IM_COL32(72, 208, 148, 255)
        : IM_COL32(118, 122, 132, 180);
    const float dotR = h * 0.12f;
    dl->AddCircleFilled({ org.x + padX + dotR, midY }, dotR, dotCol, 12);

    const char* statusTxt = state.isProjecting ? TrVp("EN VIVO", "LIVE", "EM DIRETO") : TrVp("EN ESPERA", "STANDBY", "EM ESPERA");
    const float stFontSz  = SafeClamp(h * 0.38f, baseFont * 0.78f, baseFont * 0.96f);
    ImVec2 stSz = font->CalcTextSizeA(stFontSz, FLT_MAX, 0.0f, statusTxt);
    const float statusX = padX + dotR * 2.0f + h * 0.22f;
    dl->AddText(font, stFontSz,
                { org.x + statusX, midY - stSz.y * 0.5f },
                state.isProjecting ? DS::TextPrimary : DS::TextSecondary,
                statusTxt);

    float leftEnd = statusX + stSz.x + gap;
    if (state.isProjecting && state.isStreamingNet && (leftEnd + h * 1.4f < w * 0.55f)) {
        const float lanH = h * 0.56f;
        const float lanW = lanH * 2.0f;
        ImVec2 lp0 = { org.x + leftEnd, midY - lanH * 0.5f };
        ImVec2 lp1 = { lp0.x + lanW, midY + lanH * 0.5f };
        dl->AddRectFilled(lp0, lp1, IM_COL32(72, 208, 148, 36), DS::RadiusSmall);
        dl->AddRect(lp0, lp1, IM_COL32(72, 208, 148, 140), DS::RadiusSmall);
        const float lanFontSz = stFontSz * 0.90f;
        ImVec2 lts = font->CalcTextSizeA(lanFontSz, FLT_MAX, 0.0f, "LAN");
        dl->AddText(font, lanFontSz, { lp0.x + (lanW - lts.x) * 0.5f, midY - lts.y * 0.5f },
                    IM_COL32(140, 238, 192, 255), "LAN");
    }

    // ── Controles minimalistas proporcionales a la derecha ───────────────────
    const float btnH   = h * 0.72f;
    const float btnY   = (h - btnH) * 0.5f;
    const float icBtnW = btnH * 1.15f;

    auto DrawHdrIconBtn = [&](const char* id, float rx, DrawIconFn icFn, bool active, const char* tip) -> bool {
        ImGui::SetCursorPos({ rx, btnY });
        ImVec2 p0  = ImGui::GetCursorScreenPos();
        ImVec2 p1  = { p0.x + icBtnW, p0.y + btnH };
        bool   clk = ImGui::InvisibleButton(id, { icBtnW, btnH });
        bool   hov = ImGui::IsItemHovered();

        ImU32 bg = active ? IM_COL32(52, 54, 58, 255) : (hov ? DS::BtnHoverFill : DS::BtnDefaultFill);
        ImU32 bd = active ? DS::AccentLight : (hov ? IM_COL32(255, 255, 255, 48) : IM_COL32(255, 255, 255, 20));
        dl->AddRectFilled(p0, p1, bg, DS::RadiusSmall);
        dl->AddRect(p0, p1, bd, DS::RadiusSmall);

        const float icSz = btnH * 0.56f;
        icFn(dl, { p0.x + (icBtnW - icSz) * 0.5f, p0.y + (btnH - icSz) * 0.5f },
             icSz, (active || hov) ? DS::TextPrimary : DS::TextSecondary);
        if (hov && tip) ImGui::SetTooltip("%s", tip);
        return clk;
    };

    float rightX = w - padX - icBtnW;
    if (DrawHdrIconBtn("##hdrDetach", rightX, AppIcons::DrawIcon_Popout, m_IsDetached,
                       m_IsDetached ? "Acoplar panel (Ctrl+Shift+D)" : "Ventana flotante (Ctrl+Shift+D)"))
    {
        SetDetached(!m_IsDetached);
    }

    rightX -= (icBtnW + gap);
    const bool stretchOn = core.GetStretchToFill();
    if (DrawHdrIconBtn("##hdrAspect", rightX, AppIcons::DrawIcon_Monitor, stretchOn,
                       stretchOn ? "Modo actual: Rellenar pantalla (clic para Ajustar)"
                                 : "Modo actual: Ajustar proporción (clic para Rellenar)"))
    {
        core.SetStretchToFill(!stretchOn);
    }

    const float segW = btnH * 2.4f;
    if (isPro && rightX > leftEnd + segW * 2.0f + gap * 3.0f) {
        rightX -= (segW * 2.0f + gap * 1.5f);

        auto DrawSrcTab = [&](const char* id, const char* lbl, float sx, PreviewSource src) {
            ImGui::SetCursorPos({ sx, btnY });
            ImVec2 p0  = ImGui::GetCursorScreenPos();
            ImVec2 p1  = { p0.x + segW, p0.y + btnH };
            bool   clk = ImGui::InvisibleButton(id, { segW, btnH });
            bool   hov = ImGui::IsItemHovered();
            bool   act = (m_PreviewSource == src);

            ImU32 bg = act ? IM_COL32(52, 54, 58, 255) : (hov ? DS::BtnHoverFill : DS::BtnDefaultFill);
            ImU32 bd = act ? DS::AccentLight : IM_COL32(255, 255, 255, hov ? 42 : 18);
            dl->AddRectFilled(p0, p1, bg, DS::RadiusSmall);
            dl->AddRect(p0, p1, bd, DS::RadiusSmall);

            ImVec2 ts = font->CalcTextSizeA(stFontSz * 0.95f, FLT_MAX, 0.0f, lbl);
            dl->AddText(font, stFontSz * 0.95f, { p0.x + (segW - ts.x) * 0.5f, p0.y + (btnH - ts.y) * 0.5f },
                        act ? DS::TextPrimary : DS::TextSecondary, lbl);
            if (clk) m_PreviewSource = src;
        };

        DrawSrcTab("##srcPub", "Público", rightX, PreviewSource::Publico);
        DrawSrcTab("##srcStg", "Stage",   rightX + segW + gap * 0.5f, PreviewSource::Stage);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ViewPanel::Render()
{
    if (m_UIManager && m_UIManager->IsPanelCollapsedForRender(GetName()) && !m_IsDetached)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, MT::k_Bg3);

    bool visible = false;
    if (m_IsDetached)
    {
        ImGuiWindowClass detachedClass;
        detachedClass.DockingAllowUnclassed = false;
        detachedClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&detachedClass);
        ImGui::SetNextWindowSize(ImVec2(880.0f, 620.0f), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;
        bool open = true;
        visible = ImGui::Begin("Vista en Vivo - Ventana Independiente", &open, flags);
        if (!open) {
            SetDetached(false);
        }
    }
    else
    {
        auto langVp = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language;
        const char* titleVp = (langVp == ProyecThor::Settings::Language::English) ? "Live View###Panel_LiveView"
            : ((langVp == ProyecThor::Settings::Language::Portuguese) ? "Em Direto###Panel_LiveView" : "Vista en Vivo###Panel_LiveView");
        visible = ImGui::Begin(titleVp);
    }

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        // Loop de Carpeta de Videos en Fondo si está activa
        if (!m_FolderPlaylistVideos.empty()) {
            auto& core = Core::PresentationCore::Get();
            if (Core::MPVBasePlayer* bg = core.GetBackgroundPlayer()) {
                if (bg->ConsumeEndReached()) {
                    m_FolderPlaylistIndex = (m_FolderPlaylistIndex + 1) % m_FolderPlaylistVideos.size();
                    core.SetBackgroundMedia(m_FolderPlaylistVideos[m_FolderPlaylistIndex], true, false);
                }
            }
        }

        const bool showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                          .GetSettings().general.showViewQuickActions;
        const bool wideShort = avail.x > avail.y * 1.85f && avail.y > 8.0f;

        if (wideShort)
        {
            RenderCompactWide(avail, showQuickActions);
            RenderPadEditorModal();
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }

        const float contentW  = avail.x;
        const float topAreaH  = avail.y;
        const float baseFont  = ImGui::GetFontSize();
        const bool  mediaLive = HasPlayableLiveMedia();

        if (contentW > baseFont && topAreaH > baseFont)
        {
            ImGui::BeginChild("##viewVideoArea", ImVec2(contentW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            float srcAspect = 1920.0f / 1080.0f;
            {
                int monitorCount = 0;
                GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
                auto state = Core::PresentationCore::Get().GetState();
                if (monitors && monitorCount > 0 &&
                    state.targetMonitorIndex >= 0 && state.targetMonitorIndex < monitorCount) {
                    if (const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
                        mode && mode->width > 0 && mode->height > 0) {
                        srcAspect = (float)mode->width / (float)mode->height;
                    }
                }
            }

            // Distribución vertical proporcional con límite superior estricto (no mayor a la toolbar izquierda: <= 32px)
            const float hdrH          = (topAreaH > baseFont * 5.0f) ? SafeClamp(topAreaH * 0.045f, 22.0f, 28.0f) : 0.0f;
            const float availBelowHdr = std::max(0.0f, topAreaH - hdrH);

            // Tiras de control con altura máxima acotada para que los botones nunca superen 32px
            const float stripH        = showQuickActions ? SafeClamp(availBelowHdr * 0.065f, 28.0f, 36.0f) : 0.0f;
            const float transportH    = mediaLive        ? SafeClamp(availBelowHdr * 0.160f, 72.0f, 92.0f) : 0.0f;

            // Espacio disponible para el monitor de video y la consola inferior (Pads / Live)
            const float remainForVideoAndPads = std::max(0.0f, availBelowHdr - stripH - transportH);
            const float minVideoH             = baseFont * 3.5f;
            const float naturalVideoH         = contentW / std::max(0.1f, srcAspect);
            const float idealPadsH            = SafeClamp(remainForVideoAndPads * 0.38f, 130.0f, 220.0f);
            const float maxAllowedVideoH      = std::max(minVideoH, remainForVideoAndPads - idealPadsH);
            const float videoH                = SafeClamp(naturalVideoH, minVideoH, maxAllowedVideoH);

            const float bottomAreaH           = std::max(0.0f, remainForVideoAndPads - videoH);

            float curY = 0.0f;
            if (hdrH > 0.0f) {
                ImGui::SetCursorPosY(curY);
                RenderMonitorHeader(contentW, hdrH);
                curY += hdrH;
            }

            ImGui::SetCursorPosY(curY);
            RenderContent(contentW, videoH);
            curY += videoH;

            if (showQuickActions && stripH > baseFont * 1.4f) {
                ImGui::SetCursorPosY(curY);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
                ImGui::BeginChild("##viewQuickActionsConfigStrip", ImVec2(contentW, stripH), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                RenderQuickActionsConfig(stripH);
                ImGui::EndChild();
                ImGui::PopStyleColor();
                curY += stripH;
            }

            if (mediaLive && transportH > baseFont * 2.0f) {
                ImGui::SetCursorPosY(curY);
                RenderLiveTransport(contentW, transportH);
                curY += transportH;
            }

            if (bottomAreaH > baseFont * 2.0f) {
                ImGui::SetCursorPosY(curY);
                if (m_ActiveTool != InlineTool::None) {
                    RenderInlineTool(contentW, bottomAreaH);
                } else {
                    RenderPadsContent(contentW, bottomAreaH);
                }
            }

            ImGui::EndChild();
        }

        RenderPadEditorModal();
        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActionsClear(float railW)
{
    RenderQuickActionsConfig(railW);
}

void ViewPanel::RenderQuickActionsConfig(float stripH)
{
    auto& core  = Core::PresentationCore::Get();
    auto  state = core.GetState();
    auto  sel   = core.PeekSelection();

    auto* announcements = core.GetAnnouncementsRef();
    auto* oclock        = core.GetOClockRef();
    auto* capturePanel  = core.GetCapturePanelRef();

    const bool textLive    = (state.showText && !state.currentText.empty()) || state.tiles.enabled;
    const bool audioBg     = state.bgType == Core::PresentationState::BackgroundType::Audio;
    const bool bgLive      = (state.bgType != Core::PresentationState::BackgroundType::SolidColor) || !state.bgPath.empty();
    const bool overlayLive = core.HasOverlay() || core.HasOverlayClockLayer();
    const bool annLive     = announcements && announcements->IsLive();
    const bool clockLive   = oclock && oclock->IsLive();
    const bool capLive     = capturePanel && capturePanel->IsLive();
    const bool model3DLive = core.IsLive3DModelActive();
    const bool labLive     = core.IsLiveLabActive();

    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    ImFont*      font     = ImGui::GetFont();
    const float  baseFont = ImGui::GetFontSize();
    const ImVec2 winPos   = ImGui::GetWindowPos();
    const float  totalW   = ImGui::GetWindowWidth();

    dl->AddLine({ winPos.x, winPos.y }, { winPos.x + totalW, winPos.y },
                IM_COL32(255, 255, 255, 16), 1.0f);
    dl->AddLine({ winPos.x, winPos.y + stripH - 1.0f }, { winPos.x + totalW, winPos.y + stripH - 1.0f },
                IM_COL32(255, 255, 255, 14), 1.0f);

    const float padX    = CalcPanelPadX(totalW);
    const float gap     = SafeClamp(CalcPanelGap(totalW), 3.0f, 6.0f);
    const float innerW  = std::max(1.0f, totalW - padX * 2.0f);

    // Límite máximo estricto: no mayor a los botones de la toolbar izquierda (32.0f)
    const float btnH    = SafeClamp(stripH - 6.0f, 22.0f, 30.0f);
    const float btnY    = (stripH - btnH) * 0.5f;
    const float icSz    = SafeClamp(btnH * 0.48f, 13.0f, 15.0f);
    const float fontSz  = SafeClamp(btnH * 0.38f, baseFont * 0.78f, baseFont * 0.94f);
    const float closeSz = icSz * 0.65f;

    struct ActiveLayerChip {
        const char* id;
        const char* label;
        const char* tooltip;
        DrawIconFn  icon;
        int         kind;
    };

    std::vector<ActiveLayerChip> liveChips;
    liveChips.reserve(8);
    if (textLive)
        liveChips.push_back({ "##clrTxt", "Texto", "Quitar texto en pantalla (Ctrl+L)", AppIcons::DrawIcon_TextAa, 0 });
    if (bgLive)
        liveChips.push_back({ "##clrBg", audioBg ? "Audio" : "Fondo",
                              audioBg ? "Detener audio de fondo" : "Quitar fondo activo",
                              audioBg ? AppIcons::DrawIcon_Disc : AppIcons::DrawIcon_Image, 1 });
    if (overlayLive)
        liveChips.push_back({ "##clrOvl", "Overlay", "Quitar overlay en pantalla", AppIcons::DrawIcon_Overlay, 2 });
    if (annLive)
        liveChips.push_back({ "##clrAnn", "Anuncio", "Detener anuncio en pantalla", HomeIcons::DrawIcon_Megaphone, 3 });
    if (clockLive)
        liveChips.push_back({ "##clrClk", "Reloj", "Ocultar reloj en pantalla", HomeIcons::DrawIcon_Clock, 4 });
    if (capLive)
        liveChips.push_back({ "##clrCap", "Cámara", "Detener cámara en pantalla", HomeIcons::DrawIcon_Camera, 5 });
    if (model3DLive)
        liveChips.push_back({ "##clr3D", "3D", "Ocultar modelo 3D", AppIcons::DrawIcon_Cube3D, 6 });
    if (labLive)
        liveChips.push_back({ "##clrLab", "Lab", "Ocultar vista Lab", AppIcons::DrawIcon_Formula, 7 });

    auto ClearSingleLayer = [&](int kind) {
        switch (kind) {
            case 0:
                core.ClearLayer2();
                if (state.tiles.enabled) core.ClearAllTiles();
                break;
            case 1:
                ClearFolderPlaylist();
                core.StopBackgroundMedia();
                if (state.showText && !state.currentText.empty())
                    core.SetProjecting(true);
                break;
            case 2: core.ClearOverlay(); break;
            case 3: if (announcements) announcements->SetLive(false); break;
            case 4: if (oclock)        oclock->StopTransmitting();    break;
            case 5: if (capturePanel)  capturePanel->Stop();          break;
            case 6: core.SetLive3DModelActive(false);                 break;
            case 7: core.SetLiveLabActive(false);                     break;
        }
    };

    auto ClearAllLiveLayers = [&]() {
        ClearFolderPlaylist();
        m_LastActivatedPadIndex = -1;
        core.ClearLayer2();
        if (state.tiles.enabled) core.ClearAllTiles();
        core.StopBackgroundMedia();
        core.ClearOverlay();
        if (announcements && announcements->IsLive()) announcements->SetLive(false);
        if (oclock && oclock->IsLive())               oclock->StopTransmitting();
        if (capturePanel && capturePanel->IsLive())   capturePanel->Stop();
        if (model3DLive) core.SetLive3DModelActive(false);
        if (labLive)     core.SetLiveLabActive(false);
    };

    const bool isMultiSlideType  = (sel.type == Core::ItemType::Song ||
                                    sel.type == Core::ItemType::Bible ||
                                    sel.type == Core::ItemType::Documents);
    const bool showSlideStepper  = isMultiSlideType && sel.contentData.size() > 1 && state.showText;
    const bool hasClearAll       = !liveChips.empty();

    // ── Cálculo del clúster derecho (Limpiar + Cortar/Proyectar) ───────────────
    const char* outLbl = state.isProjecting ? TrVp("Cortar", "Cut", "Cortar") : TrVp("Proyectar", "Project", "Projetar");
    ImVec2 outTextSz   = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, outLbl);
    const float outBtnW = SafeClamp(icSz + outTextSz.x + 18.0f, 32.0f, 76.0f);

    const char* clrLbl = TrVp("Limpiar", "Clear", "Limpar");
    ImVec2 clrTextSz   = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, clrLbl);
    const float clearBtnW = hasClearAll ? SafeClamp(icSz + clrTextSz.x + 18.0f, 32.0f, 74.0f) : 0.0f;

    const float rightClusterW = outBtnW + (hasClearAll ? (clearBtnW + gap) : 0.0f);
    const float rightTargetX  = totalW - padX - rightClusterW;

    // ── Clúster izquierdo: Stepper de diapositivas + Chips de capas activas ────
    float curX = padX;

    if (showSlideStepper) {
        int slideIdx = core.GetActiveSlideIndex();
        char cntBuf[24];
        if (slideIdx >= 0)
            std::snprintf(cntBuf, sizeof(cntBuf), "%d/%zu", slideIdx + 1, sel.contentData.size());
        else
            std::snprintf(cntBuf, sizeof(cntBuf), "%zu", sel.contentData.size());

        ImVec2 cSz       = font->CalcTextSizeA(fontSz * 0.94f, FLT_MAX, 0.0f, cntBuf);
        const float navW = SafeClamp(btnH * 0.82f, 20.0f, 26.0f);
        const float midW = cSz.x + 8.0f;
        const float stepGap = 2.0f;
        const float stepperTotalW = navW * 2.0f + midW + stepGap * 2.0f;

        auto DrawSlideStepBtn = [&](const char* id, float bx, bool isNext) -> bool {
            ImGui::SetCursorPos({ bx, btnY });
            ImVec2 p0  = ImGui::GetCursorScreenPos();
            ImVec2 p1  = { p0.x + navW, p0.y + btnH };
            bool   clk = ImGui::InvisibleButton(id, { navW, btnH });
            bool   hov = ImGui::IsItemHovered();

            dl->AddRectFilled(p0, p1, hov ? DS::BtnHoverFill : DS::BtnDefaultFill, DS::RadiusSmall);
            dl->AddRect(p0, p1, IM_COL32(255, 255, 255, hov ? 48 : 22), DS::RadiusSmall);

            ImVec2 c  = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
            float  ay = btnH * 0.16f;
            float  dx = (isNext ? 1.0f : -1.0f) * (ay * 0.55f);
            ImVec2 pts[3] = {
                { c.x - dx, c.y - ay },
                { c.x + dx, c.y },
                { c.x - dx, c.y + ay }
            };
            dl->AddPolyline(pts, 3, hov ? DS::TextPrimary : DS::TextSecondary, 0, 1.5f);
            if (hov) ImGui::SetTooltip(isNext ? "Siguiente diapositiva" : "Diapositiva anterior");
            return clk;
        };

        if (DrawSlideStepBtn("##vpSlidePrev", curX, false)) core.RequestNavigate(-1);

        dl->AddText(font, fontSz * 0.94f,
                    { winPos.x + curX + navW + stepGap + (midW - cSz.x) * 0.5f,
                      winPos.y + (stripH - cSz.y) * 0.5f },
                    DS::TextSecondary, cntBuf);

        if (DrawSlideStepBtn("##vpSlideNext", curX + navW + stepGap + midW + stepGap, true))
            core.RequestNavigate(1);

        curX += stepperTotalW + gap;
    }

    // Si no hay capas activas ni stepper, texto sutil en el lado izquierdo
    if (liveChips.empty() && !showSlideStepper) {
        const char* cleanTxt = TrVp("Pantalla limpia", "Clear screen", "Tela limpa");
        ImVec2 ts = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, cleanTxt);
        dl->AddText(font, fontSz,
                    { winPos.x + curX + 2.0f, winPos.y + (stripH - ts.y) * 0.5f },
                    DS::TextHint, cleanTxt);
        curX += ts.x + gap * 2.0f;
    }

    // Renderizado de chips de capas activas
    const float availForChips = std::max(0.0f, rightTargetX - gap - curX);
    const bool  compactChips  = (availForChips < static_cast<float>(liveChips.size()) * 56.0f);

    for (const auto& chip : liveChips) {
        ImVec2 lblSz = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, chip.label);
        const float fullNeed  = icSz + 6.0f + lblSz.x + 6.0f + closeSz + 10.0f;
        const float chipW     = compactChips
            ? SafeClamp(icSz + closeSz + 12.0f, 28.0f, 40.0f)
            : SafeClamp(fullNeed, 32.0f, 74.0f);

        if (curX + chipW > rightTargetX - gap * 0.5f && curX > padX + 30.0f) {
            // Evitar solapamiento con los botones de la derecha si el ancho es muy reducido
            break;
        }

        ImGui::SetCursorPos({ curX, btnY });
        ImVec2 p0  = ImGui::GetCursorScreenPos();
        ImVec2 p1  = { p0.x + chipW, p0.y + btnH };
        bool   clk = ImGui::InvisibleButton(chip.id, { chipW, btnH });
        bool   hov = ImGui::IsItemHovered();

        ImU32 bg = hov ? IM_COL32(58, 38, 42, 255) : DS::BtnDefaultFill;
        ImU32 bd = hov ? DS::DangerColor : DS::AccentColorDim;
        dl->AddRectFilled(p0, p1, bg, DS::RadiusSmall);
        dl->AddRect(p0, p1, bd, DS::RadiusSmall);

        if (!compactChips && chipW >= fullNeed * 0.90f) {
            const float contentWChip = icSz + 5.0f + lblSz.x + 6.0f + closeSz;
            const float sx           = p0.x + (chipW - contentWChip) * 0.5f;
            chip.icon(dl, { sx, p0.y + (btnH - icSz) * 0.5f }, icSz, DS::TextPrimary);
            dl->AddText(font, fontSz,
                        { sx + icSz + 5.0f, p0.y + (btnH - lblSz.y) * 0.5f },
                        DS::TextPrimary, chip.label);
            DrawIcon_CloseX(dl,
                            { sx + contentWChip - closeSz, p0.y + (btnH - closeSz) * 0.5f },
                            closeSz, hov ? DS::DangerColor : DS::TextSecondary);
        } else {
            if (hov) {
                DrawIcon_CloseX(dl, { p0.x + (chipW - icSz) * 0.5f, p0.y + (btnH - icSz) * 0.5f }, icSz, DS::DangerColor);
            } else {
                chip.icon(dl, { p0.x + (chipW - icSz) * 0.5f, p0.y + (btnH - icSz) * 0.5f }, icSz, DS::TextPrimary);
            }
        }

        if (hov && chip.tooltip) ImGui::SetTooltip("%s", chip.tooltip);
        if (clk) ClearSingleLayer(chip.kind);

        curX += chipW + gap;
    }

    // ── Renderizado del clúster derecho alineado a la derecha ─────────────────
    float rx = std::max(curX, rightTargetX);

    // Botón Limpiar Todo
    if (hasClearAll) {
        ImGui::SetCursorPos({ rx, btnY });
        ImVec2 p0  = ImGui::GetCursorScreenPos();
        ImVec2 p1  = { p0.x + clearBtnW, p0.y + btnH };
        bool   clk = ImGui::InvisibleButton("##masterClearAll", { clearBtnW, btnH });
        bool   hov = ImGui::IsItemHovered();

        ImU32 bg = hov ? IM_COL32(62, 36, 40, 255) : DS::BtnDefaultFill;
        ImU32 bd = hov ? DS::DangerColor : IM_COL32(255, 255, 255, 24);
        dl->AddRectFilled(p0, p1, bg, DS::RadiusSmall);
        dl->AddRect(p0, p1, bd, DS::RadiusSmall);

        const ImU32 icCol = hov ? DS::DangerColor : DS::TextSecondary;
        const char* lbl   = clrLbl;
        ImVec2 ts         = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, lbl);
        const float cw    = icSz + 5.0f + ts.x;

        if (clearBtnW >= cw + 8.0f) {
            const float sx = p0.x + (clearBtnW - cw) * 0.5f;
            AppIcons::DrawIcon_Broom(dl, { sx, p0.y + (btnH - icSz) * 0.5f }, icSz, icCol);
            dl->AddText(font, fontSz, { sx + icSz + 5.0f, p0.y + (btnH - ts.y) * 0.5f },
                        hov ? DS::TextPrimary : DS::TextSecondary, lbl);
        } else {
            AppIcons::DrawIcon_Broom(dl, { p0.x + (clearBtnW - icSz) * 0.5f, p0.y + (btnH - icSz) * 0.5f }, icSz, icCol);
        }
        if (hov) ImGui::SetTooltip("%s", TrVp("Limpiar todas las capas activas en pantalla", "Clear all active on-screen layers", "Limpar todas as camadas ativas no ecrã"));
        if (clk) ClearAllLiveLayers();

        rx += clearBtnW + gap;
    }

    // Botón Salida (Cortar / Proyectar)
    {
        ImGui::SetCursorPos({ rx, btnY });
        ImVec2 p0  = ImGui::GetCursorScreenPos();
        ImVec2 p1  = { p0.x + outBtnW, p0.y + btnH };
        bool   clk = ImGui::InvisibleButton("##masterOutToggle", { outBtnW, btnH });
        bool   hov = ImGui::IsItemHovered();

        const ImU32 liveGreen = IM_COL32(72, 208, 148, 255);
        ImU32 bg = DS::BtnDefaultFill;
        if (state.isProjecting) {
            bg = hov ? IM_COL32(36, 62, 50, 255) : IM_COL32(28, 48, 39, 255);
        } else if (hov) {
            bg = DS::BtnHoverFill;
        }
        ImU32 bd = state.isProjecting ? liveGreen : IM_COL32(255, 255, 255, hov ? 48 : 22);
        dl->AddRectFilled(p0, p1, bg, DS::RadiusSmall);
        dl->AddRect(p0, p1, bd, DS::RadiusSmall, 0, state.isProjecting ? 1.5f : 1.0f);

        const float cw = icSz + 5.0f + outTextSz.x;
        if (outBtnW >= cw + 8.0f) {
            const float sx = p0.x + (outBtnW - cw) * 0.5f;
            AppIcons::DrawIcon_Projector(dl, { sx, p0.y + (btnH - icSz) * 0.5f },
                                         icSz, state.isProjecting ? liveGreen : DS::TextSecondary);
            dl->AddText(font, fontSz, { sx + icSz + 5.0f, p0.y + (btnH - outTextSz.y) * 0.5f },
                        state.isProjecting ? liveGreen : DS::TextPrimary, outLbl);
        } else {
            AppIcons::DrawIcon_Projector(dl, { p0.x + (outBtnW - icSz) * 0.5f, p0.y + (btnH - icSz) * 0.5f },
                                         icSz, state.isProjecting ? liveGreen : DS::TextSecondary);
        }
        if (hov) {
            ImGui::SetTooltip("%s", state.isProjecting
                ? TrVp("Detener salida al proyector", "Stop output to projector", "Parar saída para o projetor")
                : TrVp("Activar salida al proyector", "Activate output to projector", "Ativar saída para o projetor"));
        }
        if (clk) {
            core.SetProjecting(!state.isProjecting);
        }
    }
}

void ViewPanel::RenderCompactWide(ImVec2 avail, bool showQuickActions)
{
    const float baseFont  = ImGui::GetFontSize();
    const float leftW     = showQuickActions ? avail.x * 0.34f : 0.0f;
    const float videoW    = std::max(0.0f, avail.x - leftW);
    const bool  mediaLive = HasPlayableLiveMedia();

    if (showQuickActions && leftW > baseFont * 4.0f && avail.x > leftW + baseFont * 3.0f)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
        ImGui::BeginChild("##viewCompactToolbar", ImVec2(leftW, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const float hdrH     = SafeClamp(avail.y * 0.06f, 22.0f, 28.0f);
        const float remainH  = std::max(0.0f, avail.y - hdrH);
        const float stripH   = showQuickActions ? SafeClamp(remainH * 0.12f, 28.0f, 36.0f) : 0.0f;
        const float transpH  = mediaLive        ? SafeClamp(remainH * 0.28f, 72.0f, 92.0f) : 0.0f;
        const float padsH    = std::max(0.0f, remainH - stripH - transpH);

        float curY = 0.0f;
        RenderMonitorHeader(leftW, hdrH);
        curY += hdrH;

        if (stripH > baseFont * 1.4f) {
            ImGui::SetCursorPosY(curY);
            ImGui::BeginChild("##viewCompactConfig", ImVec2(leftW, stripH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActionsConfig(stripH);
            ImGui::EndChild();
            curY += stripH;
        }

        if (mediaLive && transpH > baseFont * 2.0f) {
            ImGui::SetCursorPosY(curY);
            RenderLiveTransport(leftW, transpH);
            curY += transpH;
        }

        if (padsH > baseFont * 2.0f) {
            ImGui::SetCursorPosY(curY);
            RenderPadsContent(leftW, padsH);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 0.0f);
    }

    ImGui::BeginChild("##viewCompactVideo", ImVec2(videoW, avail.y), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderContent(videoW, avail.y);
    ImGui::EndChild();
}

void ViewPanel::RenderInlineTool(float w, float h)
{
    if (m_ActiveTool == InlineTool::None) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MT::k_PadLg, MT::k_Pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##viewInlineTool", { w, h }, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    switch (m_ActiveTool)
    {
        case InlineTool::Overlays: RenderOverlaysContent(); break;
        case InlineTool::Chat:     RenderChatContent();     break;
        case InlineTool::Pads:     RenderPadsContent();     break;
        case InlineTool::Clock:    RenderClockContent();    break;
        case InlineTool::Tiles:    RenderTilesContent();    break;
        default: break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void ViewPanel::RenderTilesContent()
{
    auto& core  = Core::PresentationCore::Get();
    auto  tiles = core.GetTilesConfig();
    auto  state = core.GetState();

    const float availW = ImGui::GetContentRegionAvail().x;
    const float itemSpacingX = ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("TILES — PANTALLA DIVIDIDA");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Toggle principal Activar Tiles
    bool enabled = tiles.enabled;
    if (ImGui::Checkbox("Activar pantalla dividida", &enabled))
    {
        core.SetTilesEnabled(enabled);
        tiles = core.GetTilesConfig();
    }

    ImGui::Spacing();

    // ── Distribución (Layout) en Combo de ancho 100% ──
    ImGui::TextColored(ToVec4(DS::TextSecondary), "Distribución:");
    const char* layoutOptions[3] = {
        "2 Columnas (Vertical)",
        "2 Filas (Horizontal)",
        "4 Cuadrantes (2x2)"
    };
    int curLayout = static_cast<int>(tiles.layout);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##TileLayout", &curLayout, layoutOptions, 3))
    {
        core.SetTileLayout(static_cast<Core::TileLayout>(curLayout));
        tiles = core.GetTilesConfig();
    }
    ImGui::Spacing();

    // ── Continuidad entre Pantallas (Flujo entre Pantalla 1 y 2) ──
    ImGui::TextColored(ToVec4(DS::TextSecondary), "Modo de Continuidad:");
    const char* continuityOptions[4] = {
        "Desactivado (Estándar)",
        "Historial / Previo (P1: Actual, P2: Anterior)",
        "Continuación / Siguiente (P1: Actual, P2: Siguiente)",
        "Flujo Inverso (P1: Anterior, P2: Actual)"
    };
    int curContinuity = static_cast<int>(tiles.continuityMode);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##TileContinuity", &curContinuity, continuityOptions, 4))
    {
        core.SetTileContinuityMode(static_cast<Core::TileContinuityMode>(curContinuity));
        tiles = core.GetTilesConfig();
    }

    if (tiles.continuityMode == Core::TileContinuityMode::PreviousOnTile2) {
        ImGui::TextDisabled("P1 muestra el actual. Al avanzar, P2 muestra lo anterior.");
    } else if (tiles.continuityMode == Core::TileContinuityMode::NextOnTile2) {
        ImGui::TextDisabled("P1 muestra el actual y P2 la continuación (como en Stage).");
    } else if (tiles.continuityMode == Core::TileContinuityMode::PreviousOnTile1) {
        ImGui::TextDisabled("P1 muestra el anterior y P2 muestra el actual.");
    }

    ImGui::Spacing();

    // ── Mini-Diagrama Visual adaptable 100% al ancho disponible ──
    const float diagW = std::max(availW, 60.0f);
    const float diagH = std::clamp(diagW * 0.50f, 36.0f, 85.0f);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = { p0.x + diagW, p0.y + diagH };
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, IM_COL32(18, 22, 30, 255), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(60, 70, 90, 200), 4.0f);

    int maxSlots = (tiles.layout == Core::TileLayout::QuadGrid) ? 4 : 2;
    for (int i = 0; i < maxSlots; i++)
    {
        ImVec2 sp0 = p0, sp1 = p1;
        if (tiles.layout == Core::TileLayout::SplitVertical) {
            float halfW = diagW * 0.5f;
            sp0 = { p0.x + i * halfW, p0.y };
            sp1 = { sp0.x + halfW, p1.y };
        } else if (tiles.layout == Core::TileLayout::SplitHorizontal) {
            float halfH = diagH * 0.5f;
            sp0 = { p0.x, p0.y + i * halfH };
            sp1 = { p1.x, sp0.y + halfH };
        } else {
            float halfW = diagW * 0.5f;
            float halfH = diagH * 0.5f;
            int col = i % 2;
            int row = i / 2;
            sp0 = { p0.x + col * halfW, p0.y + row * halfH };
            sp1 = { sp0.x + halfW, sp0.y + halfH };
        }

        bool isActive = (tiles.activeTileIndex == i);
        if (isActive) {
            dl->AddRectFilled(sp0, sp1, IM_COL32(50, 120, 200, 70));
            dl->AddRect(sp0, sp1, IM_COL32(80, 160, 240, 220), 0.0f, 0, 1.5f);
        } else {
            dl->AddRect(sp0, sp1, IM_COL32(50, 60, 80, 140), 0.0f, 0, 1.0f);
        }

        char tBadge[16];
        snprintf(tBadge, sizeof(tBadge), "T%d", i + 1);
        ImVec2 bSz = ImGui::CalcTextSize(tBadge);
        dl->AddText({ sp0.x + (sp1.x - sp0.x - bSz.x) * 0.5f, sp0.y + (sp1.y - sp0.y - bSz.y) * 0.5f },
                    isActive ? IM_COL32(100, 200, 255, 255) : IM_COL32(180, 190, 205, 180),
                    tBadge);
    }

    // El diagrama es interactivo: clic para seleccionar slot activo
    ImGui::SetCursorScreenPos(p0);
    if (ImGui::InvisibleButton("##diagClick", ImVec2(diagW, diagH)))
    {
        ImVec2 mpos = ImGui::GetIO().MousePos;
        float rx = std::clamp((mpos.x - p0.x) / diagW, 0.0f, 0.999f);
        float ry = std::clamp((mpos.y - p0.y) / diagH, 0.0f, 0.999f);
        int clickedSlot = 0;
        if (tiles.layout == Core::TileLayout::SplitVertical) {
            clickedSlot = (rx >= 0.5f) ? 1 : 0;
        } else if (tiles.layout == Core::TileLayout::SplitHorizontal) {
            clickedSlot = (ry >= 0.5f) ? 1 : 0;
        } else {
            int c = (rx >= 0.5f) ? 1 : 0;
            int r = (ry >= 0.5f) ? 1 : 0;
            clickedSlot = r * 2 + c;
        }
        if (clickedSlot >= 0 && clickedSlot < maxSlots) {
            core.SetActiveTileIndex(clickedSlot);
            tiles = core.GetTilesConfig();
        }
    }

    ImGui::Spacing();

    const bool isContinuity = (tiles.continuityMode != Core::TileContinuityMode::None);
    if (isContinuity) ImGui::BeginDisabled(true);

    // ── Tile receptor activo (botones proporcionales que dividen 100% de availW) ──
    ImGui::TextColored(ToVec4(DS::TextSecondary), isContinuity ? "Tile Receptor (Automático por continuidad):" : "Tile Receptor Activo:");
    const float slotBtnW = std::max(20.0f, (availW - itemSpacingX * (float)(maxSlots - 1)) / (float)maxSlots);
    for (int s = 0; s < maxSlots; s++)
    {
        if (s > 0) ImGui::SameLine();
        bool isActive = (tiles.activeTileIndex == s);
        char btnLabel[16];
        snprintf(btnLabel, sizeof(btnLabel), "%sT%d", isActive ? "* " : "", s + 1);
        if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.85f, 0.95f));
        if (ImGui::Button(btnLabel, ImVec2(slotBtnW, 24.0f)))
        {
            core.SetActiveTileIndex(s);
            tiles = core.GetTilesConfig();
        }
        if (isActive) ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // ── Opciones de comportamiento y aspecto ──
    bool autoAdv = tiles.autoAdvance;
    if (ImGui::Checkbox("Auto-alternar (1 -> 2)", &autoAdv))
    {
        core.SetTileAutoAdvance(autoAdv);
        tiles = core.GetTilesConfig();
    }

    if (isContinuity) ImGui::EndDisabled();

    bool showDiv = tiles.showDividers;
    if (ImGui::Checkbox("Mostrar línea divisoria", &showDiv))
    {
        core.SetTileShowDividers(showDiv);
        tiles = core.GetTilesConfig();
    }

    ImGui::Spacing();

    // ── Botones de acciones rápidas dividiendo el ancho 50/50 ──
    const float halfBtnW = std::max(20.0f, (availW - itemSpacingX) * 0.5f);
    if (ImGui::Button("Intercambiar", ImVec2(halfBtnW, 24.0f)))
    {
        core.SwapTileSlots(0, 1);
        tiles = core.GetTilesConfig();
    }
    ImGui::SameLine();
    if (ImGui::Button("Limpiar Todo", ImVec2(halfBtnW, 24.0f)))
    {
        core.ClearAllTiles();
        tiles = core.GetTilesConfig();
    }
    ImGui::Spacing();

    // ── Estilos por Pantalla / Cuadrante ──
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::AccentColor), "ESTILO POR PANTALLA");
    ImGui::TextDisabled("Aplica estilos diferentes a cada pantalla.");

    static int s_SelectedStyleSlot = 0;
    if (s_SelectedStyleSlot >= maxSlots) s_SelectedStyleSlot = 0;

    const float screenTabW = std::max(20.0f, (availW - itemSpacingX * (float)(maxSlots - 1)) / (float)maxSlots);
    for (int s = 0; s < maxSlots; s++)
    {
        if (s > 0) ImGui::SameLine();
        bool isSel = (s_SelectedStyleSlot == s);
        bool hasCustom = tiles.slots[s].hasCustomStyle;

        char tabLabel[32];
        if (hasCustom)
            snprintf(tabLabel, sizeof(tabLabel), "P%d *", s + 1);
        else
            snprintf(tabLabel, sizeof(tabLabel), "P%d", s + 1);

        if (isSel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.85f, 0.95f));
        if (ImGui::Button(tabLabel, ImVec2(screenTabW, 24.0f)))
        {
            s_SelectedStyleSlot = s;
        }
        if (isSel) ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    const auto& curSlot = tiles.slots[s_SelectedStyleSlot];

    // Selector de Estilo Guardado
    ImGui::TextColored(ToVec4(DS::TextSecondary), "Estilo Pantalla %d:", s_SelectedStyleSlot + 1);

    std::vector<std::string> savedStyles = core.GetSavedStyleNames();
    std::string previewName = curSlot.hasCustomStyle
        ? (!curSlot.styleName.empty() ? curSlot.styleName : "Personalizado")
        : "[Predeterminado (Global)]";

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##TileStyleCombo", previewName.c_str()))
    {
        bool isDefSel = !curSlot.hasCustomStyle;
        if (ImGui::Selectable("[Predeterminado (Global)]", isDefSel))
        {
            core.ResetTileSlotStyle(s_SelectedStyleSlot);
            tiles = core.GetTilesConfig();
        }
        if (isDefSel) ImGui::SetItemDefaultFocus();

        for (const auto& sName : savedStyles)
        {
            bool isSelected = (curSlot.hasCustomStyle && curSlot.styleName == sName);
            if (ImGui::Selectable(sName.c_str(), isSelected))
            {
                core.SetTileSlotSavedStyle(s_SelectedStyleSlot, sName);
                tiles = core.GetTilesConfig();
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (curSlot.hasCustomStyle)
    {
        ImGui::Spacing();

        // Color de texto
        ImGui::TextColored(ToVec4(DS::TextSecondary), "Color de texto:");
        float col[4] = { curSlot.lyricsBox.color[0], curSlot.lyricsBox.color[1],
                         curSlot.lyricsBox.color[2], curSlot.lyricsBox.color[3] };
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::ColorEdit4("##slotTxtColor", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            core.SetTileSlotTextColor(s_SelectedStyleSlot, col);
            tiles = core.GetTilesConfig();
        }

        // Tamaño de fuente
        ImGui::TextColored(ToVec4(DS::TextSecondary), "Tamano de fuente:");
        float fSize = curSlot.lyricsBox.textSize;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##slotTxtSize", &fSize, 20.0f, 160.0f, "%.0f px"))
        {
            core.SetTileSlotTextSize(s_SelectedStyleSlot, fSize);
            tiles = core.GetTilesConfig();
        }

        ImGui::Spacing();
        const float actionBtnW = std::max(20.0f, (availW - itemSpacingX) * 0.5f);
        if (ImGui::Button("Restablecer##resetSlotStyle", ImVec2(actionBtnW, 22.0f)))
        {
            core.ResetTileSlotStyle(s_SelectedStyleSlot);
            tiles = core.GetTilesConfig();
        }
        ImGui::SameLine();
        int otherSlot = (s_SelectedStyleSlot == 0) ? 1 : 0;
        char copyBtnLabel[32];
        snprintf(copyBtnLabel, sizeof(copyBtnLabel), "Copiar a P%d##copySlotStyle", otherSlot + 1);
        if (ImGui::Button(copyBtnLabel, ImVec2(actionBtnW, 22.0f)))
        {
            core.CopyTileSlotStyle(s_SelectedStyleSlot, otherSlot);
            tiles = core.GetTilesConfig();
        }
    }
    else
    {
        ImGui::Spacing();
        const float actionBtnW = std::max(20.0f, (availW - itemSpacingX) * 0.5f);
        if (ImGui::Button("Personalizar##customSlotBtn", ImVec2(actionBtnW, 22.0f)))
        {
            core.SetTileSlotCustom(s_SelectedStyleSlot, true);
            tiles = core.GetTilesConfig();
        }
        ImGui::SameLine();
        int otherSlot = (s_SelectedStyleSlot == 0) ? 1 : 0;
        char copyBtnLabel[32];
        snprintf(copyBtnLabel, sizeof(copyBtnLabel), "Copiar de P%d##copyFromSlot", otherSlot + 1);
        if (ImGui::Button(copyBtnLabel, ImVec2(actionBtnW, 22.0f)))
        {
            core.CopyTileSlotStyle(otherSlot, s_SelectedStyleSlot);
            tiles = core.GetTilesConfig();
        }
    }

    ImGui::Spacing();
}

void ViewPanel::RenderClockContent()
{
    auto* oclock = Core::PresentationCore::Get().GetOClockRef();
    if (oclock && m_UIManager)
        oclock->Render(m_UIManager->GetGlassRenderer());
    else
        ImGui::TextDisabled("Reloj no disponible.");
}

void ViewPanel::RenderOverlaysContent()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("OVERLAYS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    std::vector<OverlayThumbEntry> overlays = ListSavedOverlays();

    if (overlays.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextWrapped("Sin overlays guardados todavia. Creá uno desde Biblioteca > Overlay.");
        ImGui::PopStyleColor();
        return;
    }

    constexpr float kCardW = 84.0f, kCardH = 52.0f, kGap = 8.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const int   cols   = std::max(1, (int)((availW + kGap) / (kCardW + kGap)));
    int col = 0;

    for (const auto& ov : overlays)
    {
        ImGui::PushID(ov.name.c_str());

        ImTextureID thumb = OverlayLibraryTab::GetSharedThumbnail(ov.pngPath);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = { p0.x + kCardW, p0.y + kCardH };
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(MT::k_Bg2), MT::k_R);
        if (thumb) dl->AddImageRounded(thumb, p0, p1, { 0, 0 }, { 1, 1 }, IM_COL32_WHITE, MT::k_R);
        dl->AddRect(p0, p1, ImGui::GetColorU32(MT::k_BorderSubtle), MT::k_R);

        std::string dn = ov.name.length() > 12 ? ov.name.substr(0, 10) + "..." : ov.name;
        ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
        dl->AddRectFilled({ p0.x, p1.y - 16.0f }, p1, IM_COL32(0, 0, 0, 170), MT::k_R, ImDrawFlags_RoundCornersBottom);
        dl->AddText({ p0.x + (kCardW - ns.x) * 0.5f, p1.y - 15.0f }, ImGui::GetColorU32(MT::k_TextWhite), dn.c_str());

        if (ImGui::InvisibleButton("##ovApply", { kCardW, kCardH }))
        {
            auto& core = Core::PresentationCore::Get();
            core.SetOverlayMedia(ov.pngPath);
            OverlayDoc doc;
            if (ProyecThor::UI::LoadOverlayRecipe(OverlaysDirReadOnly(), ov.name, doc)) {
                if (const auto* cl = ProyecThor::UI::FindClockLayer(doc))
                    core.SetOverlayClockLayer(true, *cl, doc.canvasW, doc.canvasH);
                else
                    core.SetOverlayClockLayer(false, OverlayLayer{}, doc.canvasW, doc.canvasH);
            } else {
                core.SetOverlayClockLayer(false, OverlayLayer{}, 1920, 1080);
            }
            m_ActiveTool = InlineTool::None;
        }

        col++;
        if (col < cols) ImGui::SameLine(0.0f, kGap);
        else { col = 0; ImGui::Dummy(ImVec2(0.0f, kGap)); }

        ImGui::PopID();
    }
}

void ViewPanel::RenderChatContent()
{
    if (m_TeamChatPanelRef)
        m_TeamChatPanelRef->RenderContent();
    else
        ImGui::TextDisabled("Chat no disponible.");
}

void ViewPanel::RenderPadsContent(float w, float h)
{
    auto& padsArr = ProyecThor::Settings::SettingsManager::Get().GetSettings().pads.pads;
    const float availW = (w > 0.0f) ? w : ImGui::GetContentRegionAvail().x;
    const float availH = (h > 0.0f) ? h : ImGui::GetContentRegionAvail().y;

    if (availW < 16.0f || availH < 24.0f) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);

    ImGui::BeginChild("##viewPadsSection", ImVec2(availW, availH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    auto& core = Core::PresentationCore::Get();
    auto  state = core.GetState();
    auto* capPanel = core.GetCapturePanelRef();
    const bool hasLiveContent = state.isProjecting ||
                                state.bgType != Core::PresentationState::BackgroundType::SolidColor ||
                                !state.bgPath.empty() ||
                                (state.showText && !state.currentText.empty()) ||
                                (capPanel && capPanel->IsLive());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float baseFont = ImGui::GetFontSize();
    const ImVec2 secOrigin = ImGui::GetWindowPos();
    const float padX = CalcPanelPadX(availW);
    const float gap  = CalcPanelGap(availW);
    const float padInnerW = std::max(baseFont, availW - padX * 2.0f);
    const double nowTime = ImGui::GetTime();

    // Línea divisoria sutil integrada con la consola superior
    dl->AddLine(
        { secOrigin.x + padX, secOrigin.y + 1.0f },
        { secOrigin.x + availW - padX, secOrigin.y + 1.0f },
        ImGui::GetColorU32(MT::k_BorderSubtle), 1.0f);

    // ── Barra superior minimalista proporcional ──
    const float topBarY  = SafeClamp(availH * 0.020f, 2.0f, 8.0f);
    const float topBtnH  = SafeClamp(availH * 0.065f, 22.0f, 28.0f);
    const float topBtnW  = topBtnH;
    const float hdrFont  = SafeClamp(topBtnH * 0.48f, baseFont * 0.78f, baseFont * 0.94f);
    const float hdrIcSz  = topBtnH * 0.56f;

    // Ícono + sello sutil "PADS"
    AppIcons::DrawIcon_Pads(dl,
                            { secOrigin.x + padX, secOrigin.y + topBarY + (topBtnH - hdrIcSz) * 0.5f },
                            hdrIcSz, IM_COL32(155, 165, 190, 210));
    ImVec2 padsLblSz = font->CalcTextSizeA(hdrFont, FLT_MAX, 0.0f, "PADS");
    const float padsLblX = padX + hdrIcSz + topBtnH * 0.25f;
    dl->AddText(font, hdrFont,
                { secOrigin.x + padsLblX, secOrigin.y + topBarY + (topBtnH - padsLblSz.y) * 0.5f },
                ImGui::GetColorU32(MT::k_TextSecondary), "PADS");

    // Indicador compacto si hay bucle de carpeta activo
    if (!m_FolderPlaylistVideos.empty()) {
        char loopTxt[24];
        snprintf(loopTxt, sizeof(loopTxt), "%zu/%zu",
                 (m_FolderPlaylistIndex % m_FolderPlaylistVideos.size()) + 1,
                 m_FolderPlaylistVideos.size());
        ImVec2 lSz = font->CalcTextSizeA(hdrFont * 0.92f, FLT_MAX, 0.0f, loopTxt);
        const float pillH = topBtnH * 0.78f;
        const float pillX = secOrigin.x + padsLblX + padsLblSz.x + gap;
        const float pillY = secOrigin.y + topBarY + (topBtnH - pillH) * 0.5f;
        const float loopIcSz = pillH * 0.65f;
        const float pillW = loopIcSz + lSz.x + pillH * 0.65f;
        if (pillX + pillW < secOrigin.x + availW - padX - (topBtnW * 2.0f + gap * 2.0f)) {
            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                              IM_COL32(36, 115, 185, 55), DS::RadiusSmall);
            DrawIcon_FolderLoop(dl, { pillX + pillH * 0.20f, pillY + (pillH - loopIcSz) * 0.5f },
                                loopIcSz, IM_COL32(110, 195, 255, 230));
            dl->AddText(font, hdrFont * 0.92f,
                        { pillX + pillH * 0.30f + loopIcSz, pillY + (pillH - lSz.y) * 0.5f },
                        IM_COL32(160, 215, 255, 230), loopTxt);
        }
    }

    // Dos botones minimalistas a la derecha
    auto DrawMicroActionBtn = [&](const char* id, ImVec2 pos, PadIconDrawFn icFn, const char* tip) -> bool {
        ImGui::SetCursorPos(pos);
        ImVec2 sMin = ImGui::GetCursorScreenPos();
        ImVec2 sMax = { sMin.x + topBtnW, sMin.y + topBtnH };
        bool pr = ImGui::InvisibleButton(id, ImVec2(topBtnW, topBtnH));
        bool hv = ImGui::IsItemHovered();
        bool ac = ImGui::IsItemActive();

        ImU32 bgC = ac ? ImGui::GetColorU32(MT::k_NeutBtnAct)
                       : (hv ? ImGui::GetColorU32(MT::k_NeutBtnHov) : ImGui::GetColorU32(MT::k_NeutBtn));
        dl->AddRectFilled(sMin, sMax, bgC, DS::RadiusSmall);
        dl->AddRect(sMin, sMax, IM_COL32(255, 255, 255, hv ? 40 : 18), DS::RadiusSmall);
        const float icS = topBtnH * 0.56f;
        icFn(dl, { sMin.x + (topBtnW - icS) * 0.5f, sMin.y + (topBtnH - icS) * 0.5f },
             icS, hv ? IM_COL32(245, 248, 255, 255) : IM_COL32(165, 175, 198, 225));
        if (hv && tip) ImGui::SetTooltip("%s", tip);
        return pr;
    };

    const float hdrBtnGap = gap * 0.65f;
    const float rightClusterX = availW - padX - (topBtnW * 2.0f + hdrBtnGap);
    if (DrawMicroActionBtn("##padQuickCap", { rightClusterX, topBarY }, DrawIcon_CaptureSnap, "Capturar escena actual"))
    {
        int targetSlot = 0;
        bool foundEmpty = false;
        for (int pIdx = 0; pIdx < ProyecThor::Settings::kPadCount; pIdx++) {
            if (!padsArr[pIdx].assigned) { targetSlot = pIdx; foundEmpty = true; break; }
        }
        if (!foundEmpty && m_LastActivatedPadIndex >= 0 && m_LastActivatedPadIndex < ProyecThor::Settings::kPadCount)
            targetSlot = m_LastActivatedPadIndex;

        SavePad(padsArr[targetSlot], targetSlot);
        m_LastActivatedPadIndex = targetSlot;
        m_LastPadActivatedFeedbackTime = nowTime;
        m_ActiveFeedbackName = padsArr[targetSlot].name;
    }

    if (DrawMicroActionBtn("##padQuickCfg", { rightClusterX + topBtnW + hdrBtnGap, topBarY }, AppIcons::DrawIcon_Settings, "Editar escenas"))
    {
        int editIdx = (m_LastActivatedPadIndex >= 0 && m_LastActivatedPadIndex < ProyecThor::Settings::kPadCount)
                        ? m_LastActivatedPadIndex : 0;
        m_EditingPadIndex = editIdx;
        m_TempPadEdit = padsArr[editIdx];
        if (!m_TempPadEdit.assigned) {
            const float* pal = kPadPalette[editIdx % 8];
            for (int c = 0; c < 4; c++) m_TempPadEdit.color[c] = pal[c];
        }
        m_ShowPadEditModal = true;
    }

    // ── Grilla de Pads proporcional aprovechando todo el alto y ancho disponible ──
    const float vPad = availH * 0.03f;
    const float gridStartY = topBarY + topBtnH + vPad;
    const float remainH = std::max(baseFont * 1.5f, availH - gridStartY - vPad);

    const int totalPads = ProyecThor::Settings::kPadCount;
    int cols = 4;
    if (padInnerW > remainH * 4.5f)           cols = 8;
    else if (padInnerW < baseFont * 12.0f)    cols = 2;
    else                                      cols = 4;

    const int   rows = (totalPads + cols - 1) / cols;
    const float padCellW = std::max(baseFont, (padInnerW - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols));
    const float maxPadH  = 64.0f;
    const float padCellH = SafeClamp((remainH - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows),
                                     28.0f, maxPadH);

    auto OpenEditorFor = [&](int idx) {
        m_EditingPadIndex = idx;
        m_TempPadEdit = padsArr[idx];
        if (!m_TempPadEdit.assigned) {
            const float* pal = kPadPalette[idx % 8];
            for (int c = 0; c < 4; c++) m_TempPadEdit.color[c] = pal[c];
            m_TempPadEdit.iconIndex = 2;
        }
        m_ShowPadEditModal = true;
    };

    for (int i = 0; i < totalPads; i++)
    {
        const int colIdx = i % cols;
        const int rowIdx = i / cols;
        const float cellX = padX + (float)colIdx * (padCellW + gap);
        const float cellY = gridStartY + (float)rowIdx * (padCellH + gap);

        ImGui::SetCursorPos({ cellX, cellY });
        auto& pad = padsArr[i];
        ImGui::PushID(i);

        ImVec2 pMin = ImGui::GetCursorScreenPos();
        ImVec2 pMax = { pMin.x + padCellW, pMin.y + padCellH };

        bool clicked = ImGui::InvisibleButton("##padKey", ImVec2(padCellW, padCellH));
        bool hovered = ImGui::IsItemHovered();
        bool held    = ImGui::IsItemActive();

        // Zona de engranaje proporcional en la esquina superior derecha al pasar el cursor
        const float cellMinDim = std::min(padCellW, padCellH);
        const float gearBoxSz  = cellMinDim * 0.28f;
        const float gearInset  = cellMinDim * 0.04f;
        ImRect gearRect({ pMax.x - gearBoxSz - gearInset, pMin.y + gearInset },
                        { pMax.x - gearInset, pMin.y + gearBoxSz + gearInset });
        bool gearHovered = hovered && (padCellW >= baseFont * 2.8f) && gearRect.Contains(ImGui::GetIO().MousePos);
        const float gearIcSz = gearBoxSz * 0.65f;

        if (!pad.assigned)
        {
            // ── Pad vacío: botón plano minimalista igual al resto de la app ──
            ImU32 bgCol = (held && !gearHovered)
                ? IM_COL32(48, 50, 54, 255)
                : ((hovered && !gearHovered) ? DS::BtnHoverFill : DS::BtnDefaultFill);
            ImU32 bdCol = IM_COL32(255, 255, 255, hovered ? 44 : 18);

            dl->AddRectFilled(pMin, pMax, bgCol, DS::RadiusMedium);
            dl->AddRect(pMin, pMax, bdCol, DS::RadiusMedium, 0, 1.0f);

            ImVec2 center = { (pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f };
            float plusSz = cellMinDim * 0.22f;
            DrawIcon_PlusKey(dl, { center.x - plusSz * 0.5f, center.y - plusSz * 0.5f }, plusSz,
                             (hovered && !gearHovered) ? DS::TextSecondary : DS::TextHint);

            if (hovered && padCellW >= baseFont * 2.8f) {
                if (gearHovered) {
                    dl->AddRectFilled(gearRect.Min, gearRect.Max, IM_COL32(255, 255, 255, 24), DS::RadiusSmall);
                }
                AppIcons::DrawIcon_Settings(
                    dl,
                    { gearRect.Min.x + (gearBoxSz - gearIcSz) * 0.5f, gearRect.Min.y + (gearBoxSz - gearIcSz) * 0.5f },
                    gearIcSz, gearHovered ? DS::TextPrimary : DS::TextSecondary);
            }
        }
        else
        {
            // ── Pad asignado: botón plano minimalista, sin degradados ni efectos ──
            const bool isLivePad = (i == m_LastActivatedPadIndex && state.isProjecting);

            ImU32 bgCol = DS::BtnDefaultFill;
            if (held && !gearHovered) {
                bgCol = IM_COL32(54, 56, 60, 255);
            } else if (isLivePad) {
                ImVec4 ac = ToVec4(DS::AccentLight);
                ImVec4 bf = ToVec4(DS::BtnDefaultFill);
                const float mix = (hovered && !gearHovered) ? 0.20f : 0.14f;
                bgCol = ImGui::ColorConvertFloat4ToU32({
                    bf.x + (ac.x - bf.x) * mix,
                    bf.y + (ac.y - bf.y) * mix,
                    bf.z + (ac.z - bf.z) * mix, 1.0f });
            } else if (hovered && !gearHovered) {
                bgCol = DS::BtnHoverFill;
            }

            const ImU32 bdCol = isLivePad
                ? DS::AccentLight
                : IM_COL32(255, 255, 255, hovered ? 48 : 22);

            dl->AddRectFilled(pMin, pMax, bgCol, DS::RadiusMedium);
            dl->AddRect(pMin, pMax, bdCol, DS::RadiusMedium, 0, isLivePad ? 1.3f : 1.0f);

            if (isLivePad) {
                const float dotOff = cellMinDim * 0.11f;
                dl->AddCircleFilled({ pMin.x + dotOff, pMin.y + dotOff }, cellMinDim * 0.04f, DS::AccentLight, 10);
            }

            if (hovered && padCellW >= baseFont * 2.8f) {
                if (gearHovered) {
                    dl->AddRectFilled(gearRect.Min, gearRect.Max, IM_COL32(255, 255, 255, 24), DS::RadiusSmall);
                }
                AppIcons::DrawIcon_Settings(
                    dl,
                    { gearRect.Min.x + (gearBoxSz - gearIcSz) * 0.5f, gearRect.Min.y + (gearBoxSz - gearIcSz) * 0.5f },
                    gearIcSz, gearHovered ? DS::TextPrimary : DS::TextSecondary);
            }

            const bool  showLabel = !pad.name.empty() && padCellH >= baseFont * 3.0f;
            const float icSz      = cellMinDim * (showLabel ? 0.30f : 0.38f);
            const float lblFontSz = SafeClamp(padCellH * 0.15f, baseFont * 0.76f, baseFont * 0.98f);
            const float icCenterY = showLabel
                ? (pMin.y + padCellH * 0.41f)
                : (pMin.y + padCellH * 0.50f);

            const ImU32 icCol = (isLivePad || (hovered && !gearHovered)) ? DS::TextPrimary : DS::TextSecondary;
            const auto& iconEntry = PadIconFor(pad.iconIndex);
            iconEntry.draw(dl,
                           { (pMin.x + pMax.x - icSz) * 0.5f, icCenterY - icSz * 0.5f },
                           icSz, icCol);

            if (showLabel) {
                std::string fitLbl = TruncateUTF8ToWidth(font, lblFontSz, pad.name, padCellW * 0.86f);
                if (!fitLbl.empty()) {
                    ImVec2 tSz = font->CalcTextSizeA(lblFontSz, FLT_MAX, 0.0f, fitLbl.c_str());
                    dl->AddText(font, lblFontSz,
                                { pMin.x + (padCellW - tSz.x) * 0.5f, pMax.y - tSz.y - padCellH * 0.10f },
                                isLivePad ? DS::TextPrimary : DS::TextSecondary,
                                fitLbl.c_str());
                }
            }
        }

        // Tooltip limpio y conciso
        if (hovered && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        {
            if (gearHovered) {
                ImGui::SetTooltip("Editar Pad %d", i + 1);
            } else if (pad.assigned) {
                std::string tip = pad.name.empty() ? ("Pad " + std::to_string(i + 1)) : pad.name;
                if (pad.applyBackground && pad.hasBackground) {
                    if (pad.bgType == 4) {
                        tip += "\n• Carpeta: " + (!pad.bgFolderName.empty() ? pad.bgFolderName : "Videos");
                    } else if (!pad.bgPath.empty()) {
                        try {
                            tip += "\n• Fondo: " + std::filesystem::u8path(pad.bgPath).filename().u8string();
                        } catch (...) {}
                    } else {
                        tip += "\n• Color sólido";
                    }
                }
                if (pad.applyText && pad.hasText) {
                    if (!pad.text.empty()) tip += "\n• Texto: \"" + CleanShortLabel(pad.text) + "\"";
                    else                   tip += "\n• Texto: Limpiar pantalla";
                }
                if (pad.applyCapture && pad.hasCapture) {
                    tip += "\n• Cámara";
                }
                ImGui::SetTooltip("%s", tip.c_str());
            } else {
                ImGui::SetTooltip(hasLiveContent ? "Guardar escena actual" : "Configurar escena");
            }
        }

        // Acción de clic izquierdo
        if (clicked)
        {
            if (gearHovered) {
                OpenEditorFor(i);
            } else if (pad.assigned) {
                ApplyPad(this, pad);
                m_LastActivatedPadIndex = i;
                m_LastPadActivatedFeedbackTime = nowTime;
                m_ActiveFeedbackName = pad.name;
            } else if (hasLiveContent) {
                SavePad(pad, i);
                m_LastActivatedPadIndex = i;
                m_LastPadActivatedFeedbackTime = nowTime;
                m_ActiveFeedbackName = pad.name;
            } else {
                OpenEditorFor(i);
            }
        }

        // Menú contextual limpio (sin emojis rotos)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
        if (ImGui::BeginPopupContextItem("##padCtx"))
        {
            if (pad.assigned) {
                if (ImGui::MenuItem("Activar escena")) {
                    ApplyPad(this, pad);
                    m_LastActivatedPadIndex = i;
                    m_LastPadActivatedFeedbackTime = nowTime;
                    m_ActiveFeedbackName = pad.name;
                }
            }
            if (ImGui::MenuItem(pad.assigned ? "Reemplazar con actual" : "Capturar escena actual")) {
                SavePad(pad, i);
                m_LastActivatedPadIndex = i;
                m_LastPadActivatedFeedbackTime = nowTime;
                m_ActiveFeedbackName = pad.name;
            }
            if (ImGui::MenuItem("Editar...")) {
                OpenEditorFor(i);
            }
            if (ImGui::BeginMenu("Cambiar icono")) {
                if (RenderPadIconGrid(pad.iconIndex)) {
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
                ImGui::EndMenu();
            }
            if (pad.assigned) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, MT::k_BtnDelT);
                if (ImGui::MenuItem("Vaciar")) {
                    pad = ProyecThor::Settings::PadSettings{};
                    if (m_LastActivatedPadIndex == i) m_LastActivatedPadIndex = -1;
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ViewPanel::RenderPadEditorModal()
{
    if (m_ShowPadEditModal && !ImGui::IsPopupOpen("##padSceneEditorModal")) {
        ImGui::OpenPopup("##padSceneEditorModal");
    }

    ImGui::SetNextWindowSize(ImVec2(476.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));

    if (ImGui::BeginPopupModal("##padSceneEditorModal", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (m_EditingPadIndex < 0 || m_EditingPadIndex >= ProyecThor::Settings::kPadCount) {
            m_ShowPadEditModal = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_ShowPadEditModal = false;
            ImGui::CloseCurrentPopup();
        }

        auto& padsArr = ProyecThor::Settings::SettingsManager::Get().GetSettings().pads.pads;
        auto& core = Core::PresentationCore::Get();
        auto  state = core.GetState();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec4 accent = ToVec4(DS::AccentLight);

        // ── Selector minimalista de Pad 1..8 ──
        {
            const float stripW = ImGui::GetContentRegionAvail().x;
            const float sGap   = 5.0f;
            const float pillW  = (stripW - sGap * 7.0f) / 8.0f;
            const float pillH  = 22.0f;
            for (int pIdx = 0; pIdx < ProyecThor::Settings::kPadCount; pIdx++) {
                if (pIdx > 0) ImGui::SameLine(0.0f, sGap);
                ImGui::PushID(1000 + pIdx);
                ImVec2 sMin = ImGui::GetCursorScreenPos();
                ImVec2 sMax = { sMin.x + pillW, sMin.y + pillH };
                bool pClk = ImGui::InvisibleButton("##selPadSlot", ImVec2(pillW, pillH));
                bool pHov = ImGui::IsItemHovered();
                const bool isCur = (pIdx == m_EditingPadIndex);
                const auto& slotRef = padsArr[pIdx];

                ImU32 bg = isCur ? IM_COL32(52, 54, 58, 255) : (pHov ? DS::BtnHoverFill : DS::BtnDefaultFill);
                ImU32 bd = isCur ? DS::AccentLight : IM_COL32(255, 255, 255, pHov ? 44 : 18);
                dl->AddRectFilled(sMin, sMax, bg, DS::RadiusSmall);
                dl->AddRect(sMin, sMax, bd, DS::RadiusSmall, 0, isCur ? 1.2f : 1.0f);

                char slotLbl[8];
                snprintf(slotLbl, sizeof(slotLbl), "%d", pIdx + 1);
                ImVec2 lSz = font->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, slotLbl);
                dl->AddText(font, 11.0f,
                            { sMin.x + (pillW - lSz.x) * 0.5f, sMin.y + (pillH - lSz.y) * 0.5f },
                            isCur ? DS::TextPrimary : DS::TextSecondary,
                            slotLbl);

                if (slotRef.assigned) {
                    dl->AddCircleFilled({ sMax.x - 5.0f, sMin.y + 5.0f }, 2.0f, DS::AccentLight, 8);
                }

                if (pClk && !isCur) {
                    m_EditingPadIndex = pIdx;
                    m_TempPadEdit = padsArr[pIdx];
                    if (!m_TempPadEdit.assigned) {
                        m_TempPadEdit.iconIndex = 2;
                    }
                }
                ImGui::PopID();
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // ── Cabecera minimalista: Icono + Nombre + Capturar + Cerrar ──
        ImVec2 previewPos = ImGui::GetCursorScreenPos();
        const float keyPreviewSz = 32.0f;
        bool openIconPicker = ImGui::InvisibleButton("##padPreviewKeyBtn", ImVec2(keyPreviewSz, keyPreviewSz));
        bool prevHov = ImGui::IsItemHovered();
        {
            ImVec2 kMax = { previewPos.x + keyPreviewSz, previewPos.y + keyPreviewSz };
            dl->AddRectFilled(previewPos, kMax, prevHov ? DS::BtnHoverFill : DS::BtnDefaultFill, DS::RadiusMedium);
            dl->AddRect(previewPos, kMax, prevHov ? DS::AccentLight : IM_COL32(255, 255, 255, 28), DS::RadiusMedium);

            const float icSz = 16.0f;
            PadIconFor(m_TempPadEdit.iconIndex).draw(
                dl,
                { previewPos.x + (keyPreviewSz - icSz) * 0.5f, previewPos.y + (keyPreviewSz - icSz) * 0.5f },
                icSz, DS::TextPrimary);
        }
        if (prevHov) ImGui::SetTooltip("Cambiar icono");
        if (openIconPicker) ImGui::OpenPopup("##padIconPickerPop");

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
        if (ImGui::BeginPopup("##padIconPickerPop")) {
            if (RenderPadIconGrid(m_TempPadEdit.iconIndex)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        ImGui::SameLine(0.0f, 8.0f);
        ImGui::BeginGroup();
        {
            const float rightAvail = ImGui::GetContentRegionAvail().x;
            const float snapBtnW   = 94.0f;
            const float closeBtnW  = 28.0f;
            const float nameW      = std::max(100.0f, rightAvail - snapBtnW - closeBtnW - 12.0f);

            char nameBuf[64];
            strncpy(nameBuf, m_TempPadEdit.name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, MT::k_Bg3);
            ImGui::SetNextItemWidth(nameW);
            if (ImGui::InputTextWithHint("##padNameInp", "Nombre del botón...", nameBuf, sizeof(nameBuf))) {
                m_TempPadEdit.name = nameBuf;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            ImGui::SameLine(0.0f, 6.0f);
            {
                ImVec2 bMin = ImGui::GetCursorScreenPos();
                ImVec2 bMax = { bMin.x + snapBtnW, bMin.y + 32.0f };
                bool snapClk = ImGui::InvisibleButton("##snapLiveAll", ImVec2(snapBtnW, 32.0f));
                bool snapHov = ImGui::IsItemHovered();
                dl->AddRectFilled(bMin, bMax, snapHov ? DS::BtnHoverFill : DS::BtnDefaultFill, DS::RadiusMedium);
                dl->AddRect(bMin, bMax, IM_COL32(255, 255, 255, snapHov ? 52 : 24), DS::RadiusMedium);
                DrawIcon_CaptureSnap(dl, { bMin.x + 8.0f, bMin.y + 9.0f }, 14.0f, DS::TextPrimary);
                dl->AddText(font, 11.5f, { bMin.x + 26.0f, bMin.y + 9.0f }, DS::TextPrimary, "Capturar");
                if (snapHov) ImGui::SetTooltip("Tomar fondo, texto, estilo y cámara actuales");
                if (snapClk) {
                    m_TempPadEdit.hasBackground   = true;
                    m_TempPadEdit.applyBackground = true;
                    m_TempPadEdit.bgType          = (int)state.bgType;
                    m_TempPadEdit.bgPath          = state.bgPath;
                    for (int c = 0; c < 3; c++) m_TempPadEdit.bgColor[c] = state.bgColor[c];
                    if (!state.bgPath.empty()) {
                        try {
                            std::filesystem::path p = std::filesystem::u8path(state.bgPath);
                            if (p.has_parent_path()) m_TempPadEdit.bgFolderName = p.parent_path().filename().u8string();
                        } catch (...) {}
                    }
                    if (state.showText && !state.currentText.empty()) {
                        m_TempPadEdit.hasText   = true;
                        m_TempPadEdit.applyText = true;
                        m_TempPadEdit.text      = state.currentText;
                        m_TempPadEdit.textTitle = core.PeekSelection().title;
                    }
                    m_TempPadEdit.hasStyle       = true;
                    m_TempPadEdit.applyStyle     = true;
                    m_TempPadEdit.styleSize      = state.textSize;
                    for (int c = 0; c < 4; c++) m_TempPadEdit.styleColor[c] = state.textColor[c];
                    m_TempPadEdit.styleHAlign    = state.textAlignment;
                    m_TempPadEdit.styleVAlign    = state.vAlignment;
                    for (int c = 0; c < 4; c++) m_TempPadEdit.styleMargins[c] = state.margins[c];
                    m_TempPadEdit.styleAutoScale = state.autoScale;
                    m_TempPadEdit.styleFontName  = state.selectedFont;
                    if (auto* cap = core.GetCapturePanelRef()) {
                        m_TempPadEdit.hasCapture   = cap->SnapshotCurrentCapture(m_TempPadEdit.capture);
                        m_TempPadEdit.applyCapture = m_TempPadEdit.hasCapture;
                    }
                    if (m_TempPadEdit.name.empty()) {
                        if (m_TempPadEdit.applyText && !m_TempPadEdit.text.empty())
                            m_TempPadEdit.name = CleanShortLabel(m_TempPadEdit.text);
                        else if (!m_TempPadEdit.bgPath.empty()) {
                            try { m_TempPadEdit.name = CleanShortLabel(std::filesystem::u8path(m_TempPadEdit.bgPath).stem().u8string()); } catch (...) {}
                        }
                    }
                }
            }

            ImGui::SameLine(0.0f, 6.0f);
            {
                ImVec2 xMin = ImGui::GetCursorScreenPos();
                ImVec2 xMax = { xMin.x + closeBtnW, xMin.y + 32.0f };
                bool xClk = ImGui::InvisibleButton("##closePadModal", ImVec2(closeBtnW, 32.0f));
                bool xHov = ImGui::IsItemHovered();
                if (xHov) dl->AddRectFilled(xMin, xMax, DS::BtnHoverFill, DS::RadiusMedium);
                DrawIcon_CloseX(dl, { xMin.x + 7.0f, xMin.y + 9.0f }, 14.0f,
                                xHov ? DS::TextPrimary : DS::TextSecondary);
                if (xClk) {
                    m_ShowPadEditModal = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Helper para botones compactos secundarios dentro de tarjetas
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, MT::k_NeutBtnAct);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, MT::k_Bg3);

        const float cardW = ImGui::GetContentRegionAvail().x;

        // ══════════════════════════════════════════════════════════════════════
        // 1. TARJETA: FONDO (Archivo | Carpeta | Color)
        // ══════════════════════════════════════════════════════════════════════
        {
            ImVec2 cStart = ImGui::GetCursorScreenPos();
            const float cardH = m_TempPadEdit.applyBackground ? 68.0f : 34.0f;
            dl->AddRectFilled(cStart, { cStart.x + cardW, cStart.y + cardH },
                              ImGui::GetColorU32(m_TempPadEdit.applyBackground ? MT::k_Bg2 : MT::k_Bg3), 8.0f);
            dl->AddRect(cStart, { cStart.x + cardW, cStart.y + cardH },
                        ImGui::GetColorU32(m_TempPadEdit.applyBackground ? MT::k_BorderSubtle : ImVec4(1, 1, 1, 0.05f)), 8.0f);

            ImGui::BeginGroup();
            ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 9.0f });
            if (DrawToggleSwitch("##swBg", &m_TempPadEdit.applyBackground, accent)) {
                if (m_TempPadEdit.applyBackground) m_TempPadEdit.hasBackground = true;
            }

            ImU32 hdrCol = m_TempPadEdit.applyBackground ? IM_COL32(235, 240, 250, 255) : IM_COL32(130, 138, 158, 190);
            if (m_TempPadEdit.bgType == 4)
                DrawIcon_FolderLoop(dl, { cStart.x + 48.0f, cStart.y + 9.5f }, 15.0f, hdrCol);
            else
                AppIcons::DrawIcon_Image(dl, { cStart.x + 48.0f, cStart.y + 9.5f }, 15.0f, hdrCol);
            dl->AddText(font, 12.5f, { cStart.x + 69.0f, cStart.y + 9.5f }, hdrCol, "Fondo");

            if (m_TempPadEdit.applyBackground)
            {
                m_TempPadEdit.hasBackground = true;

                int bgMode = 0; // 0 = Archivo, 1 = Carpeta, 2 = Color
                if (m_TempPadEdit.bgType == 4) bgMode = 1;
                else if (m_TempPadEdit.bgType == 0 && m_TempPadEdit.bgPath.empty()) bgMode = 2;

                // Selector segmentado compacto a la derecha del encabezado
                const char* segLabels[3] = { "Archivo", "Carpeta", "Color" };
                const float segW = 60.0f;
                const float segTotalW = segW * 3.0f + 4.0f;
                ImVec2 segPos = { cStart.x + cardW - segTotalW - 10.0f, cStart.y + 6.0f };
                dl->AddRectFilled(segPos, { segPos.x + segTotalW, segPos.y + 22.0f }, IM_COL32(12, 14, 20, 220), 6.0f);

                for (int s = 0; s < 3; s++) {
                    ImVec2 sp0 = { segPos.x + 2.0f + s * segW, segPos.y + 2.0f };
                    ImVec2 sp1 = { sp0.x + segW, sp0.y + 18.0f };
                    ImGui::SetCursorScreenPos(sp0);
                    ImGui::PushID(s);
                    bool sClk = ImGui::InvisibleButton("##bgSeg", ImVec2(segW, 18.0f));
                    bool sHov = ImGui::IsItemHovered();
                    if (bgMode == s) {
                        dl->AddRectFilled(sp0, sp1,
                            ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.35f)), 4.5f);
                        dl->AddRect(sp0, sp1,
                            ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.70f)), 4.5f);
                    } else if (sHov) {
                        dl->AddRectFilled(sp0, sp1, IM_COL32(255, 255, 255, 16), 4.5f);
                    }
                    ImVec2 ts = font->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, segLabels[s]);
                    dl->AddText(font, 11.0f, { sp0.x + (segW - ts.x) * 0.5f, sp0.y + (18.0f - ts.y) * 0.5f },
                                bgMode == s ? IM_COL32(250, 252, 255, 255) : IM_COL32(155, 165, 185, 220), segLabels[s]);
                    if (sClk) {
                        if (s == 0) {
                            m_TempPadEdit.bgType = 1;
                            if (!m_TempPadEdit.bgPath.empty() &&
                                std::filesystem::is_directory(std::filesystem::u8path(m_TempPadEdit.bgPath))) {
                                m_TempPadEdit.bgPath.clear();
                            }
                        } else if (s == 1) {
                            m_TempPadEdit.bgType = 4;
                            if (m_TempPadEdit.bgFolderName.empty()) m_TempPadEdit.bgFolderName = "Todos";
                        } else {
                            m_TempPadEdit.bgType = 0;
                            m_TempPadEdit.bgPath.clear();
                        }
                        bgMode = s;
                    }
                    ImGui::PopID();
                }

                // Fila inferior de controles del modo activo
                ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 36.0f });
                const float rowInnerW = cardW - 20.0f;

                if (bgMode == 0) // Archivo único
                {
                    std::string fileLabel = "Sin archivo";
                    if (!m_TempPadEdit.bgPath.empty()) {
                        try { fileLabel = std::filesystem::u8path(m_TempPadEdit.bgPath).filename().u8string(); }
                        catch (...) { fileLabel = m_TempPadEdit.bgPath; }
                    }
                    const float btnPickW = 74.0f;
                    const float btnLiveW = 58.0f;
                    const float boxW = std::max(60.0f, rowInnerW - btnPickW - btnLiveW - 12.0f);

                    ImVec2 fMin = ImGui::GetCursorScreenPos();
                    ImVec2 fMax = { fMin.x + boxW, fMin.y + 22.0f };
                    dl->AddRectFilled(fMin, fMax, IM_COL32(12, 14, 20, 220), 5.0f);
                    dl->AddRect(fMin, fMax, IM_COL32(255, 255, 255, 18), 5.0f);
                    std::string truncF = TruncateUTF8ToWidth(font, 11.5f, fileLabel, boxW - 14.0f);
                    dl->AddText(font, 11.5f, { fMin.x + 7.0f, fMin.y + 4.0f },
                                m_TempPadEdit.bgPath.empty() ? IM_COL32(120, 128, 148, 200) : IM_COL32(220, 228, 242, 255),
                                truncF.c_str());
                    ImGui::Dummy(ImVec2(boxW, 22.0f));

                    ImGui::SameLine(0.0f, 6.0f);
                    if (ImGui::Button("Elegir...##bgPick", ImVec2(btnPickW, 22.0f))) {
                        std::string picked = PickImageOrVideoFile();
                        if (!picked.empty()) {
                            m_TempPadEdit.bgPath = picked;
                            m_TempPadEdit.bgType = LooksLikeVideoPath(picked) ? 1 : 3;
                        }
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (ImGui::Button("Actual##bgCur", ImVec2(btnLiveW, 22.0f))) {
                        m_TempPadEdit.bgPath = state.bgPath;
                        m_TempPadEdit.bgType = (int)state.bgType;
                    }
                }
                else if (bgMode == 1) // Carpeta en bucle
                {
                    std::vector<std::string> bgFolders;
                    bgFolders.push_back("Todos");
                    std::filesystem::path bgRoot = std::filesystem::u8path(ProyecThor::GetAssetsPath()) / "backgrounds";
                    if (std::filesystem::exists(bgRoot)) {
                        try {
                            for (const auto& e : std::filesystem::directory_iterator(bgRoot)) {
                                if (e.is_directory()) bgFolders.push_back(e.path().filename().u8string());
                            }
                        } catch (...) {}
                    }

                    int curFolderIdx = -1;
                    for (size_t fI = 0; fI < bgFolders.size(); fI++) {
                        if (bgFolders[fI] == m_TempPadEdit.bgFolderName) { curFolderIdx = (int)fI; break; }
                    }
                    std::string comboPreview = (curFolderIdx >= 0)
                        ? bgFolders[curFolderIdx]
                        : (!m_TempPadEdit.bgFolderName.empty() ? m_TempPadEdit.bgFolderName : "Todos");

                    const float extBtnW = 92.0f;
                    ImGui::SetNextItemWidth(std::max(100.0f, rowInnerW - extBtnW - 6.0f));
                    if (ImGui::BeginCombo("##fldCombo", comboPreview.c_str())) {
                        for (size_t fI = 0; fI < bgFolders.size(); fI++) {
                            bool isSel = (curFolderIdx == (int)fI);
                            if (ImGui::Selectable(bgFolders[fI].c_str(), isSel)) {
                                m_TempPadEdit.bgFolderName = bgFolders[fI];
                                m_TempPadEdit.bgPath = (fI == 0) ? bgRoot.u8string() : (bgRoot / bgFolders[fI]).u8string();
                            }
                            if (isSel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::SameLine(0.0f, 6.0f);
                    if (ImGui::Button("Otra...##pickExtFld", ImVec2(extBtnW, 22.0f))) {
                        std::string pickedFld = PickFolder("Seleccionar carpeta de videos");
                        if (!pickedFld.empty()) {
                            m_TempPadEdit.bgPath = pickedFld;
                            try { m_TempPadEdit.bgFolderName = std::filesystem::u8path(pickedFld).filename().u8string(); }
                            catch (...) { m_TempPadEdit.bgFolderName = pickedFld; }
                        }
                    }
                }
                else // Color sólido
                {
                    ImGui::ColorEdit3("##bgSolidPicker", m_TempPadEdit.bgColor, ImGuiColorEditFlags_NoInputs);
                    ImGui::SameLine(0.0f, 8.0f);
                    if (ImGui::Button("Negro##blkBg", ImVec2(64.0f, 22.0f))) {
                        m_TempPadEdit.bgColor[0] = 0.0f;
                        m_TempPadEdit.bgColor[1] = 0.0f;
                        m_TempPadEdit.bgColor[2] = 0.0f;
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (ImGui::Button("Actual##colCur", ImVec2(64.0f, 22.0f))) {
                        for (int c = 0; c < 3; c++) m_TempPadEdit.bgColor[c] = state.bgColor[c];
                    }
                }
            }

            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({ cStart.x, cStart.y + cardH });
            ImGui::Dummy(ImVec2(cardW, 6.0f));
        }

        // ══════════════════════════════════════════════════════════════════════
        // 2. TARJETA: TEXTO / LETRA
        // ══════════════════════════════════════════════════════════════════════
        {
            ImVec2 cStart = ImGui::GetCursorScreenPos();
            const float cardH = m_TempPadEdit.applyText ? 92.0f : 34.0f;
            dl->AddRectFilled(cStart, { cStart.x + cardW, cStart.y + cardH },
                              ImGui::GetColorU32(m_TempPadEdit.applyText ? MT::k_Bg2 : MT::k_Bg3), 8.0f);
            dl->AddRect(cStart, { cStart.x + cardW, cStart.y + cardH },
                        ImGui::GetColorU32(m_TempPadEdit.applyText ? MT::k_BorderSubtle : ImVec4(1, 1, 1, 0.05f)), 8.0f);

            ImGui::BeginGroup();
            ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 9.0f });
            if (DrawToggleSwitch("##swTxt", &m_TempPadEdit.applyText, accent)) {
                if (m_TempPadEdit.applyText) m_TempPadEdit.hasText = true;
            }

            ImU32 hdrCol = m_TempPadEdit.applyText ? IM_COL32(235, 240, 250, 255) : IM_COL32(130, 138, 158, 190);
            AppIcons::DrawIcon_TextAa(dl, { cStart.x + 48.0f, cStart.y + 9.5f }, 15.0f, hdrCol);
            dl->AddText(font, 12.5f, { cStart.x + 69.0f, cStart.y + 9.5f }, hdrCol, "Texto");

            if (m_TempPadEdit.applyText)
            {
                m_TempPadEdit.hasText = true;
                ImGui::SetCursorScreenPos({ cStart.x + cardW - 134.0f, cStart.y + 6.0f });
                if (ImGui::Button("Actual##cpTxt", ImVec2(58.0f, 21.0f))) {
                    m_TempPadEdit.text = state.currentText;
                    m_TempPadEdit.textTitle = core.PeekSelection().title;
                }
                ImGui::SameLine(0.0f, 6.0f);
                if (ImGui::Button("Limpiar##clrTxt", ImVec2(60.0f, 21.0f))) {
                    m_TempPadEdit.text.clear();
                }

                ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 34.0f });
                char lyricsBuf[2048];
                strncpy(lyricsBuf, m_TempPadEdit.text.c_str(), sizeof(lyricsBuf) - 1);
                lyricsBuf[sizeof(lyricsBuf) - 1] = '\0';
                ImVec2 inpMin = ImGui::GetCursorScreenPos();
                if (ImGui::InputTextMultiline("##padLyricsInput", lyricsBuf, sizeof(lyricsBuf),
                                              ImVec2(cardW - 20.0f, 48.0f))) {
                    m_TempPadEdit.text = lyricsBuf;
                }
                if (m_TempPadEdit.text.empty() && !ImGui::IsItemActive()) {
                    dl->AddText(font, 11.5f, { inpMin.x + 8.0f, inpMin.y + 6.0f },
                                IM_COL32(125, 132, 152, 165), "Palabra o letra (vacío = limpiar texto)...");
                }
            }

            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({ cStart.x, cStart.y + cardH });
            ImGui::Dummy(ImVec2(cardW, 6.0f));
        }

        // ══════════════════════════════════════════════════════════════════════
        // 3. TARJETA: ESTILO TIPOGRÁFICO (1 sola fila limpia)
        // ══════════════════════════════════════════════════════════════════════
        {
            ImVec2 cStart = ImGui::GetCursorScreenPos();
            const float cardH = 34.0f;
            dl->AddRectFilled(cStart, { cStart.x + cardW, cStart.y + cardH },
                              ImGui::GetColorU32(m_TempPadEdit.applyStyle ? MT::k_Bg2 : MT::k_Bg3), 8.0f);
            dl->AddRect(cStart, { cStart.x + cardW, cStart.y + cardH },
                        ImGui::GetColorU32(m_TempPadEdit.applyStyle ? MT::k_BorderSubtle : ImVec4(1, 1, 1, 0.05f)), 8.0f);

            ImGui::BeginGroup();
            ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 9.0f });
            if (DrawToggleSwitch("##swSty", &m_TempPadEdit.applyStyle, accent)) {
                if (m_TempPadEdit.applyStyle) m_TempPadEdit.hasStyle = true;
            }

            ImU32 hdrCol = m_TempPadEdit.applyStyle ? IM_COL32(235, 240, 250, 255) : IM_COL32(130, 138, 158, 190);
            AppIcons::DrawIcon_Palette(dl, { cStart.x + 48.0f, cStart.y + 9.5f }, 15.0f, hdrCol);
            dl->AddText(font, 12.5f, { cStart.x + 69.0f, cStart.y + 9.5f }, hdrCol, "Estilo");

            if (m_TempPadEdit.applyStyle)
            {
                m_TempPadEdit.hasStyle = true;
                std::string fntShort = TruncateUTF8ToWidth(font, 11.0f, m_TempPadEdit.styleFontName, 120.0f);
                dl->AddText(font, 11.0f, { cStart.x + 124.0f, cStart.y + 10.5f },
                            IM_COL32(165, 175, 196, 220), fntShort.c_str());

                ImGui::SetCursorScreenPos({ cStart.x + cardW - 156.0f, cStart.y + 6.0f });
                ImGui::ColorEdit4("##padTxtCol", m_TempPadEdit.styleColor,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::SetNextItemWidth(56.0f);
                ImGui::DragFloat("##padTxtSz", &m_TempPadEdit.styleSize, 1.0f, 18.0f, 180.0f, "%.0fpx");
                ImGui::SameLine(0.0f, 6.0f);
                if (ImGui::Button("Actual##cpSty", ImVec2(58.0f, 21.0f))) {
                    m_TempPadEdit.styleSize      = state.textSize;
                    for (int c = 0; c < 4; c++) m_TempPadEdit.styleColor[c] = state.textColor[c];
                    m_TempPadEdit.styleHAlign    = state.textAlignment;
                    m_TempPadEdit.styleVAlign    = state.vAlignment;
                    for (int c = 0; c < 4; c++) m_TempPadEdit.styleMargins[c] = state.margins[c];
                    m_TempPadEdit.styleAutoScale = state.autoScale;
                    m_TempPadEdit.styleFontName  = state.selectedFont;
                }
            }

            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({ cStart.x, cStart.y + cardH });
            ImGui::Dummy(ImVec2(cardW, 6.0f));
        }

        // ══════════════════════════════════════════════════════════════════════
        // 4. TARJETA: CÁMARA (1 sola fila limpia)
        // ══════════════════════════════════════════════════════════════════════
        {
            ImVec2 cStart = ImGui::GetCursorScreenPos();
            const float cardH = 34.0f;
            dl->AddRectFilled(cStart, { cStart.x + cardW, cStart.y + cardH },
                              ImGui::GetColorU32(m_TempPadEdit.applyCapture ? MT::k_Bg2 : MT::k_Bg3), 8.0f);
            dl->AddRect(cStart, { cStart.x + cardW, cStart.y + cardH },
                        ImGui::GetColorU32(m_TempPadEdit.applyCapture ? MT::k_BorderSubtle : ImVec4(1, 1, 1, 0.05f)), 8.0f);

            ImGui::BeginGroup();
            ImGui::SetCursorScreenPos({ cStart.x + 10.0f, cStart.y + 9.0f });
            if (DrawToggleSwitch("##swCam", &m_TempPadEdit.applyCapture, accent)) {
                if (m_TempPadEdit.applyCapture && !m_TempPadEdit.hasCapture) {
                    if (auto* cap = core.GetCapturePanelRef())
                        m_TempPadEdit.hasCapture = cap->SnapshotCurrentCapture(m_TempPadEdit.capture);
                }
            }

            ImU32 hdrCol = m_TempPadEdit.applyCapture ? IM_COL32(235, 240, 250, 255) : IM_COL32(130, 138, 158, 190);
            HomeIcons::DrawIcon_Camera(dl, { cStart.x + 48.0f, cStart.y + 9.5f }, 15.0f, hdrCol);
            dl->AddText(font, 12.5f, { cStart.x + 69.0f, cStart.y + 9.5f }, hdrCol, "Cámara");

            if (m_TempPadEdit.applyCapture)
            {
                std::string camLbl = !m_TempPadEdit.capture.sourceName.empty()
                    ? TruncateUTF8ToWidth(font, 11.0f, m_TempPadEdit.capture.sourceName, 180.0f)
                    : (m_TempPadEdit.hasCapture ? "Lista" : "Sin fuente activa");
                dl->AddText(font, 11.0f, { cStart.x + 132.0f, cStart.y + 10.5f },
                            IM_COL32(165, 175, 196, 220), camLbl.c_str());

                ImGui::SetCursorScreenPos({ cStart.x + cardW - 68.0f, cStart.y + 6.0f });
                if (ImGui::Button("Actual##cpCam", ImVec2(58.0f, 21.0f))) {
                    if (auto* cap = core.GetCapturePanelRef())
                        m_TempPadEdit.hasCapture = cap->SnapshotCurrentCapture(m_TempPadEdit.capture);
                }
            }

            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({ cStart.x, cStart.y + cardH });
            ImGui::Dummy(ImVec2(cardW, 8.0f));
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        ImGui::Separator();
        ImGui::Spacing();

        // ── Pie del modal: Vaciar (izq)  |  Cancelar · Guardar · Aplicar (der) ──
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        const float footerY = ImGui::GetCursorPosY();

        if (padsArr[m_EditingPadIndex].assigned)
        {
            ImVec2 dMin = ImGui::GetCursorScreenPos();
            ImVec2 dMax = { dMin.x + 76.0f, dMin.y + 28.0f };
            bool delClk = ImGui::InvisibleButton("##delPadScene", ImVec2(76.0f, 28.0f));
            bool delHov = ImGui::IsItemHovered();
            dl->AddRectFilled(dMin, dMax,
                              ImGui::GetColorU32(delHov ? MT::k_BtnDelH : MT::k_BtnDel), 6.0f);
            DrawIcon_Trash(dl, { dMin.x + 9.0f, dMin.y + 7.0f }, 14.0f, ImGui::GetColorU32(MT::k_BtnDelT));
            dl->AddText(font, 12.0f, { dMin.x + 27.0f, dMin.y + 7.0f }, ImGui::GetColorU32(MT::k_BtnDelT), "Vaciar");
            if (delClk) {
                padsArr[m_EditingPadIndex] = ProyecThor::Settings::PadSettings{};
                if (m_LastActivatedPadIndex == m_EditingPadIndex) m_LastActivatedPadIndex = -1;
                ProyecThor::Settings::SettingsManager::Get().Save();
                m_ShowPadEditModal = false;
                ImGui::CloseCurrentPopup();
            }
        }

        const float btnCancelW = 80.0f;
        const float btnSaveW   = 84.0f;
        const float btnApplyW  = 94.0f;
        const float rightBtnsW = btnCancelW + btnSaveW + btnApplyW + 14.0f;
        ImGui::SetCursorPos({ ImGui::GetWindowWidth() - 14.0f - rightBtnsW, footerY });

        ImGui::PushStyleColor(ImGuiCol_Button, MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, MT::k_NeutBtnAct);
        if (ImGui::Button("Cancelar", ImVec2(btnCancelW, 28.0f)))
        {
            m_ShowPadEditModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        auto CommitEditedPad = [&]() {
            m_TempPadEdit.assigned      = true;
            m_TempPadEdit.hasBackground = m_TempPadEdit.applyBackground;
            m_TempPadEdit.hasText       = m_TempPadEdit.applyText;
            m_TempPadEdit.hasStyle      = m_TempPadEdit.applyStyle;
            if (m_TempPadEdit.applyCapture) m_TempPadEdit.hasCapture = true;
            padsArr[m_EditingPadIndex] = m_TempPadEdit;
            ProyecThor::Settings::SettingsManager::Get().Save();
        };

        ImGui::SameLine(0.0f, 7.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, MT::k_BtnGreen);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_BtnGreenH);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, MT::k_BtnGreenA);
        if (ImGui::Button("Guardar", ImVec2(btnSaveW, 28.0f)))
        {
            CommitEditedPad();
            m_ShowPadEditModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 7.0f);
        {
            ImVec2 aMin = ImGui::GetCursorScreenPos();
            ImVec2 aMax = { aMin.x + btnApplyW, aMin.y + 28.0f };
            bool appClk = ImGui::InvisibleButton("##applyPadNow", ImVec2(btnApplyW, 28.0f));
            bool appHov = ImGui::IsItemHovered();
            dl->AddRectFilled(aMin, aMax,
                              ImGui::GetColorU32(appHov ? MT::k_PrevBtnHov : MT::k_PrevBtn), 6.0f);
            dl->AddRect(aMin, aMax, IM_COL32(130, 195, 255, appHov ? 180 : 95), 6.0f);
            DrawIcon_PlayTri(dl, { aMin.x + 12.0f, aMin.y + 8.0f }, 12.0f, IM_COL32(255, 255, 255, 255));
            dl->AddText(font, 12.0f, { aMin.x + 29.0f, aMin.y + 7.0f }, IM_COL32(255, 255, 255, 255), "Aplicar");
            if (appClk) {
                CommitEditedPad();
                ApplyPad(this, padsArr[m_EditingPadIndex]);
                m_LastActivatedPadIndex = m_EditingPadIndex;
                m_LastPadActivatedFeedbackTime = ImGui::GetTime();
                m_ActiveFeedbackName = m_TempPadEdit.name;
                m_ShowPadEditModal = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void ViewPanel::RenderLiveTransport(float w, float h)
{
    if (w < 40.0f || h < 28.0f) return;

    auto& core  = Core::PresentationCore::Get();
    const auto& state = core.GetState();
    Core::MPVBasePlayer* bg = core.GetBackgroundPlayer();

    UI::AudioPanel* musicPanel = core.GetAudioPanelRef();
    const bool audioLive = musicPanel && musicPanel->IsLiveBackground() &&
                           state.bgType == Core::PresentationState::BackgroundType::Audio;
    const bool hasAudibleAudio = audioLive || core.GetContentAllowsAudio();
    const bool hasPlaylistNav  = audioLive || (m_FolderPlaylistVideos.size() > 1);

    if (audioLive) {
        m_LiveMuted   = musicPanel->IsMusicMuted();
        m_LiveVolume  = static_cast<float>(musicPanel->GetMusicVolume()) * 0.01f;
        m_LivePlaying = musicPanel->IsMusicActive() && !musicPanel->IsMusicPaused();
    } else {
        m_LiveMuted   = core.GetLiveMute();
        m_LiveVolume  = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        m_LivePlaying = bg && !bg->IsPaused();
    }

    int64_t liveCurMs = audioLive ? musicPanel->GetMusicTimeMs()   : (bg ? bg->GetTime()   : 0);
    int64_t liveLen   = audioLive ? musicPanel->GetMusicLengthMs() : (bg ? bg->GetLength() : 0);

    if (!audioLive && m_LivePlaying && bg && liveLen > 0)
    {
        float fpos = static_cast<float>(liveCurMs) / static_cast<float>(liveLen);
        if (fpos >= 0.995f && core.GetLiveLoop())
        {
            bg->SetPosition(0.0f);
            bg->SetPause(false);
        }
    }

    float livePos = (liveLen > 0)
        ? std::clamp(static_cast<float>(liveCurMs) / static_cast<float>(liveLen), 0.0f, 1.0f)
        : 0.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::BeginChild("##viewLiveTransport", { w, h }, false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    ImFont*      font     = ImGui::GetFont();
    const float  baseFont = ImGui::GetFontSize();
    const ImVec2 org      = ImGui::GetWindowPos();
    dl->AddLine({ org.x, org.y }, { org.x + w, org.y }, IM_COL32(255, 255, 255, 16), 1.0f);

    const float padX   = CalcPanelPadX(w);
    const float gap    = CalcPanelGap(w);
    const float innerW = std::max(baseFont, w - padX * 2.0f);
    const ImVec4 kNeutral = MT::k_NeutBtn;
    const ImVec4 kLive    = MT::k_LiveBtn;

    auto DrawInlineScrubber = [&](float x, float y, float scrubW, float scrubH) {
        if (scrubW < baseFont) return;
        ImGui::SetCursorPos({ x, y });
        ImVec2 s0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##vp_tl_live", { scrubW, scrubH });
        bool sHov = ImGui::IsItemHovered();
        bool sAct = ImGui::IsItemActive();

        const float trkH   = scrubH * 0.36f;
        const float thumbR = trkH * ((sHov || sAct) ? 0.80f : 0.66f);
        const float trkL   = s0.x + thumbR;
        const float trkR   = s0.x + scrubW - thumbR;
        const float trkW   = std::max(1.0f, trkR - trkL);
        const float cy     = s0.y + scrubH * 0.5f;
        const float trkRad = trkH * 0.5f;

        if (sAct && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float newPos = SafeClamp((ImGui::GetIO().MousePos.x - trkL) / trkW, 0.0f, 1.0f);
            if (audioLive) musicPanel->SeekNormalized(newPos);
            else           core.SetLivePosition(newPos);
            livePos   = newPos;
            liveCurMs = static_cast<int64_t>(newPos * static_cast<float>(liveLen));
        }

        // Riel hundido proporcional + relleno en vivo + cabezal táctil
        dl->AddRectFilled({ trkL, cy - trkH * 0.5f }, { trkR, cy + trkH * 0.5f },
                          IM_COL32(16, 17, 20, 255), trkRad);
        dl->AddRect({ trkL, cy - trkH * 0.5f }, { trkR, cy + trkH * 0.5f },
                    IM_COL32(255, 255, 255, (sHov || sAct) ? 44 : 24), trkRad);

        float fillX = trkL + livePos * trkW;
        if (fillX > trkL + 0.5f) {
            dl->AddRectFilled({ trkL, cy - trkH * 0.5f }, { fillX, cy + trkH * 0.5f },
                              DS::AccentLight, trkRad);
        }

        const ImU32 thumbCol = (sHov || sAct) ? IM_COL32(242, 245, 250, 255) : IM_COL32(216, 220, 228, 255);
        dl->AddCircleFilled({ fillX, cy }, thumbR, thumbCol, 16);
        dl->AddCircle({ fillX, cy }, thumbR, IM_COL32(10, 11, 14, 185), 16, 1.1f);
    };

    const std::string curStr = FormatTime(liveCurMs);
    const std::string lenStr = FormatTime(liveLen);

    auto TriggerPrev = [&]() {
        if (audioLive) {
            musicPanel->Previous();
        } else if (!m_FolderPlaylistVideos.empty()) {
            m_FolderPlaylistIndex = (m_FolderPlaylistIndex + m_FolderPlaylistVideos.size() - 1) % m_FolderPlaylistVideos.size();
            core.SetBackgroundMedia(m_FolderPlaylistVideos[m_FolderPlaylistIndex], true, false);
        }
    };

    auto TriggerNext = [&]() {
        if (audioLive) {
            musicPanel->Next();
        } else if (!m_FolderPlaylistVideos.empty()) {
            m_FolderPlaylistIndex = (m_FolderPlaylistIndex + 1) % m_FolderPlaylistVideos.size();
            core.SetBackgroundMedia(m_FolderPlaylistVideos[m_FolderPlaylistIndex], true, false);
        }
    };

    auto TriggerSkip = [&](int deltaSec) {
        if (audioLive) {
            musicPanel->SkipBy(deltaSec);
        } else if (liveLen > 0) {
            float np = livePos + (static_cast<float>(deltaSec) * 1000.0f / static_cast<float>(liveLen));
            core.SetLivePosition(SafeClamp(np, 0.0f, 1.0f));
        }
    };

    auto TriggerPlayPause = [&]() {
        if (audioLive) {
            musicPanel->TogglePlayPause();
        } else if (bg) {
            if (m_LivePlaying) {
                bg->SetPause(true);
            } else {
                if (hasAudibleAudio) {
                    core.SetLiveMute(m_LiveMuted);
                    core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
                }
                bg->SetPause(false);
            }
        }
    };

    auto TriggerStop = [&]() {
        if (audioLive) {
            musicPanel->Stop();
        } else {
            core.SetLivePosition(0.0f);
            if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
        }
    };

    auto ToggleMute = [&]() {
        if (!hasAudibleAudio) return;
        m_LiveMuted = !m_LiveMuted;
        if (audioLive) {
            musicPanel->SetMusicMuted(m_LiveMuted);
        } else {
            core.SetLiveMute(m_LiveMuted);
            core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
        }
    };

    auto ApplyVolume = [&]() {
        if (!hasAudibleAudio) return;
        const int volPct = static_cast<int>(std::round(m_LiveVolume * 100.0f));
        if (audioLive) musicPanel->SetMusicVolume(volPct);
        else           core.SetLiveVolume(m_LiveMuted ? 0 : volPct);
    };

    if (h >= baseFont * 4.8f)
    {
        // Distribución vertical 100% proporcional (mismo alto de control que la barra superior)
        const float vPad         = SafeClamp(h * 0.045f, 3.0f, 6.0f);
        const float vGap         = SafeClamp(h * 0.040f, 3.0f, 5.0f);
        const float usableH      = std::max(baseFont, h - vPad * 2.0f - vGap * 2.0f);
        const float timelineRowH = SafeClamp(usableH * (0.65f / 2.65f), 14.0f, 18.0f);
        const float controlRowH  = SafeClamp(usableH * (1.00f / 2.65f), 22.0f, 30.0f);
        const float timeFontSz   = SafeClamp(timelineRowH * 0.42f, baseFont * 0.78f, baseFont * 1.00f);

        // ── Fila 1: Tiempo actual + línea de tiempo grande + duración total ───
        const float row1Y = vPad;
        ImVec2 curSz = font->CalcTextSizeA(timeFontSz, FLT_MAX, 0.0f, curStr.c_str());
        ImVec2 lenSz = font->CalcTextSizeA(timeFontSz, FLT_MAX, 0.0f, lenStr.c_str());
        const float timeColW = std::max(curSz.x, lenSz.x) + gap * 0.5f;

        dl->AddText(font, timeFontSz,
                    { org.x + padX, org.y + row1Y + (timelineRowH - curSz.y) * 0.5f },
                    DS::TextSecondary, curStr.c_str());

        dl->AddText(font, timeFontSz,
                    { org.x + w - padX - lenSz.x, org.y + row1Y + (timelineRowH - lenSz.y) * 0.5f },
                    DS::TextHint, lenStr.c_str());

        const float scrubX = padX + timeColW + gap * 0.5f;
        const float scrubW = w - scrubX * 2.0f;
        DrawInlineScrubber(scrubX, row1Y, scrubW, timelineRowH);

        // ── Fila 2: Botones de transporte TODOS del mismo tamaño (btnW × controlRowH) ──
        const float row2Y    = row1Y + timelineRowH + vGap;
        const int   btnCount = hasPlaylistNav ? 6 : 4;
        const float rawBtnW  = (innerW - gap * static_cast<float>(btnCount - 1)) / static_cast<float>(btnCount);
        const float btnW     = SafeClamp(rawBtnW, 26.0f, 44.0f);
        const float iconSz   = std::min(btnW, controlRowH) * 0.44f;
        const float totalBtnsW = static_cast<float>(btnCount) * btnW + gap * static_cast<float>(btnCount - 1);
        float curX           = (totalBtnsW <= innerW) ? (padX + (innerW - totalBtnsW) * 0.5f) : padX;

        if (hasPlaylistNav) {
            ImGui::SetCursorPos({ curX, row2Y });
            ImGui::PushID("vp_pad_nav_prev");
            if (DrawPadButton("skip_prev", iconSz, kNeutral, { btnW, controlRowH }, false)) {
                TriggerPrev();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(audioLive ? "Pista anterior" : "Video anterior");
            ImGui::PopID();
            curX += btnW + gap;
        }

        ImGui::SetCursorPos({ curX, row2Y });
        ImGui::PushID("vp_pad_replay");
        if (DrawPadButton("replay_10", iconSz, kNeutral, { btnW, controlRowH }, false)) {
            TriggerSkip(-10);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Retroceder 10 s");
        ImGui::PopID();
        curX += btnW + gap;

        ImGui::SetCursorPos({ curX, row2Y });
        ImGui::PushID("vp_pad_main");
        const char* mainIcon = m_LivePlaying ? "pause" : "play";
        if (DrawPadButton(mainIcon, iconSz, m_LivePlaying ? kLive : kNeutral, { btnW, controlRowH }, m_LivePlaying)) {
            TriggerPlayPause();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_LivePlaying ? "Pausar" : "Reproducir");
        ImGui::PopID();
        curX += btnW + gap;

        ImGui::SetCursorPos({ curX, row2Y });
        ImGui::PushID("vp_pad_fwd");
        if (DrawPadButton("forward_10", iconSz, kNeutral, { btnW, controlRowH }, false)) {
            TriggerSkip(10);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adelantar 10 s");
        ImGui::PopID();
        curX += btnW + gap;

        ImGui::SetCursorPos({ curX, row2Y });
        ImGui::PushID("vp_pad_stop");
        if (DrawPadButton("stop", iconSz * 0.94f, kNeutral, { btnW, controlRowH }, false)) {
            TriggerStop();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Detener");
        ImGui::PopID();
        curX += btnW + gap;

        if (hasPlaylistNav) {
            ImGui::SetCursorPos({ curX, row2Y });
            ImGui::PushID("vp_pad_nav_next");
            if (DrawPadButton("skip_next", iconSz, kNeutral, { btnW, controlRowH }, false)) {
                TriggerNext();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(audioLive ? "Pista siguiente" : "Video siguiente");
            ImGui::PopID();
        }

        // ── Fila 3 (debajo): Botón Mute del mismo tamaño + Barra de volumen híbrida grande ──
        const float row3Y     = row2Y + controlRowH + vGap;
        const bool  effMuted  = !hasAudibleAudio || m_LiveMuted;
        const bool  isDanger  = hasAudibleAudio && !m_LiveMuted && (m_LiveVolume > 1.005f);
        DrawIconFn  speakerFn = effMuted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn;

        char pctBuf[16];
        if (!hasAudibleAudio) {
            std::snprintf(pctBuf, sizeof(pctBuf), "Mudo");
        } else {
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(std::round(m_LiveVolume * 100.0f)));
        }
        const float pctFontSz = SafeClamp(controlRowH * 0.30f, baseFont * 0.80f, baseFont * 1.05f);
        ImVec2 samplePts      = font->CalcTextSizeA(pctFontSz, FLT_MAX, 0.0f, "200%");
        ImVec2 pts            = font->CalcTextSizeA(pctFontSz, FLT_MAX, 0.0f, pctBuf);
        const float pctW      = std::max(samplePts.x, pts.x) + gap * 0.5f;
        const float muteW     = btnW;
        const float faderW    = std::max(baseFont * 2.0f, innerW - muteW - pctW - gap * 2.0f);

        ImGui::SetCursorPos({ padX, row3Y });
        ImGui::PushID("vp_pad_mute");
        if (!hasAudibleAudio) ImGui::BeginDisabled(true);
        if (DrawPadButton(effMuted ? "no_sound" : "volume_up", iconSz,
                          (hasAudibleAudio && m_LiveMuted) ? kLive : kNeutral,
                          { muteW, controlRowH }, hasAudibleAudio && m_LiveMuted, speakerFn)) {
            ToggleMute();
        }
        if (!hasAudibleAudio) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!hasAudibleAudio) ImGui::SetTooltip("Fondo silencioso (sin pista de audio activa)");
            else                  ImGui::SetTooltip(m_LiveMuted ? "Activar audio" : "Silenciar audio");
        }
        ImGui::PopID();

        ImGui::SetCursorPos({ padX + muteW + gap, row3Y });
        if (HybridVolumeFader("##vp_vol_fader", &m_LiveVolume, 0.0f, 2.0f,
                              { faderW, controlRowH }, effMuted, hasAudibleAudio)) {
            ApplyVolume();
        }
        if (ImGui::IsItemHovered()) {
            if (!hasAudibleAudio)
                ImGui::SetTooltip("Fondo silencioso — reproduce desde Multimedia o Audio para sonido");
            else
                ImGui::SetTooltip("Volumen en vivo: %d%% (doble clic = 100%%)",
                                  static_cast<int>(std::round(m_LiveVolume * 100.0f)));
        }

        ImU32 pctCol = !hasAudibleAudio
            ? DS::TextHint
            : (isDanger ? IM_COL32(245, 110, 110, 255) : DS::TextSecondary);
        dl->AddText(font, pctFontSz,
                    { org.x + w - padX - pts.x, org.y + row3Y + (controlRowH - pts.y) * 0.5f },
                    pctCol, pctBuf);
    }
    else
    {
        // Modo compacto proporcional cuando la ventana se reduce al mínimo vertical
        const float rowH      = h * 0.76f;
        const float rowY      = (h - rowH) * 0.5f;
        const float btnW      = rowH * 1.15f;
        const float iconSz    = rowH * 0.44f;
        const float timeFont  = SafeClamp(rowH * 0.36f, baseFont * 0.75f, baseFont);
        float curX            = padX;

        ImGui::SetCursorPos({ curX, rowY });
        ImGui::PushID("vp_compact_main");
        if (DrawPadButton(m_LivePlaying ? "pause" : "play", iconSz,
                          m_LivePlaying ? kLive : kNeutral, { btnW, rowH }, m_LivePlaying)) {
            TriggerPlayPause();
        }
        ImGui::PopID();
        curX += btnW + gap;

        ImGui::SetCursorPos({ curX, rowY });
        ImGui::PushID("vp_compact_stop");
        if (DrawPadButton("stop", iconSz * 0.94f, kNeutral, { btnW, rowH }, false)) {
            TriggerStop();
        }
        ImGui::PopID();
        curX += btnW + gap;

        float rightEnd = w - padX;
        if (hasAudibleAudio && (rightEnd - curX) > btnW * 3.5f) {
            rightEnd -= (btnW + gap);
            ImGui::SetCursorPos({ rightEnd + gap, rowY });
            ImGui::PushID("vp_compact_mute");
            DrawIconFn speakerFn = m_LiveMuted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn;
            if (DrawPadButton(m_LiveMuted ? "no_sound" : "volume_up", iconSz,
                              m_LiveMuted ? kLive : kNeutral, { btnW, rowH }, m_LiveMuted, speakerFn)) {
                ToggleMute();
            }
            ImGui::PopID();
        }

        ImVec2 curSz = font->CalcTextSizeA(timeFont, FLT_MAX, 0.0f, curStr.c_str());
        if (rightEnd - curX > curSz.x + btnW * 2.0f) {
            rightEnd -= (curSz.x + gap);
            dl->AddText(font, timeFont,
                        { org.x + rightEnd + gap * 0.5f, org.y + (h - curSz.y) * 0.5f },
                        DS::TextSecondary, curStr.c_str());
        }

        DrawInlineScrubber(curX, rowY, std::max(baseFont, rightEnd - curX), rowH);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

void ViewPanel::RenderContent(float panelW, float panelH)
{
    if (panelW < 4.0f || panelH < 4.0f) return;

    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();
    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    float srcW = 1920.0f;
    float srcH = 1080.0f;

    if (monitors && monitorCount > 0 && state.targetMonitorIndex >= 0 &&
        state.targetMonitorIndex < monitorCount)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
        if (mode && mode->width > 0 && mode->height > 0)
        {
            srcW = (float)mode->width;
            srcH = (float)mode->height;
        }
    }

    float srcRatio = srcW / srcH;
    float drawW = panelW;
    float drawH = panelW / srcRatio;

    if (drawH > panelH)
    {
        drawH = panelH;
        drawW = panelH * srcRatio;
    }

    float offsetX = (panelW - drawW) * 0.5f;
    float offsetY = (panelH - drawH) * 0.5f;

    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }
    if (offsetY > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
    }

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + drawW, p0.y + drawH);

    if (m_PreviewSource == PreviewSource::Stage)
    {
        const bool isPro = true;
        if (!isPro)
        {
            m_PreviewSource = PreviewSource::Publico;
            UI::DrawPublicContent(dl, p0, p1, drawW, drawH, m_IsEditingLiveLyrics, true);
        }
        else
        {
            UI::DrawStageContent(dl, p0, p1);
        }
    }
    else
    {
        UI::DrawPublicContent(dl, p0, p1, drawW, drawH, m_IsEditingLiveLyrics, true);
    }

    // Marco sutil de monitor de estudio
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 24), 0.0f, 0, 1.0f);

    // Vúmetro de audio: solo cuando se proyecta video con audio real y fuera de Modo Rendimiento
    const bool showMeters = !perf.performanceMode &&
                            !perf.lowMemoryMode &&
                            m_PreviewSource == PreviewSource::Publico &&
                            state.isProjecting &&
                            state.bgType == Core::PresentationState::BackgroundType::Video &&
                            core.GetContentAllowsAudio() &&
                            core.GetBackgroundPlayer() != nullptr;
    if (showMeters)
    {
        Core::MPVBasePlayer* liveBg = core.GetBackgroundPlayer();
        bool liveMuted   = core.GetLiveMute();
        float liveVolume = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        bool livePlaying = liveBg && !liveBg->IsPaused();

        m_AudioMeters.Update(liveBg, true, livePlaying, liveMuted, liveVolume);

        const float meterW   = 20.0f;
        const float meterPad = 6.0f;
        float meterH = std::min(drawH - meterPad * 2.0f, 104.0f);
        if (meterH > 20.0f)
        {
            ImVec2 meterPos = { p0.x + meterPad, p0.y + meterPad };
            m_AudioMeters.RenderVertical(dl, meterPos, meterW, meterH);
        }
    }

    if (m_PreviewSource == PreviewSource::Publico && core.IsBackgroundSwapPending())
    {
        const float spinR = 10.0f;
        ImVec2 spinCenter = { p1.x - spinR - 12.0f, p1.y - spinR - 12.0f };
        DrawLoadingSpinner(dl, spinCenter, spinR);

        char etaBuf[32];
        snprintf(etaBuf, sizeof(etaBuf), "~%.1fs", core.GetBackgroundSwapEta());
        ImVec2 etaSz = ImGui::CalcTextSize(etaBuf);
        ImVec2 etaPos = { spinCenter.x - spinR - 6.0f - etaSz.x, spinCenter.y - etaSz.y * 0.5f };
        dl->AddRectFilled({ etaPos.x - 5.0f, etaPos.y - 3.0f }, { etaPos.x + etaSz.x + 5.0f, etaPos.y + etaSz.y + 3.0f },
                          IM_COL32(0, 0, 0, 165), 4.0f);
        dl->AddText(etaPos, IM_COL32(230, 230, 235, 230), etaBuf);
    }

    // ── Edición en vivo de letras al hacer clic ─────────────────────────────
    if (m_PreviewSource == PreviewSource::Publico)
    {
        bool hasLyrics = state.showText && !state.currentText.empty();

        if (hasLyrics)
        {
            ImVec2 boxMin, boxMax, textMin, textMax;
            if (UI::GetLyricsScreenBounds(state, p0, drawW, drawH, boxMin, boxMax, textMin, textMax))
            {
                float padX = 14.0f;
                float padY = 10.0f;
                ImVec2 clickMin(std::max(p0.x + 4.0f, textMin.x - padX), std::max(p0.y + 4.0f, textMin.y - padY));
                ImVec2 clickMax(std::min(p1.x - 4.0f, textMax.x + padX), std::min(p1.y - 4.0f, textMax.y + padY));
                ImVec2 clickSize(std::max(40.0f, clickMax.x - clickMin.x), std::max(30.0f, clickMax.y - clickMin.y));

                auto sel = core.PeekSelection();
                bool isSong = (sel.type == Core::ItemType::Song);

                if (!m_IsEditingLiveLyrics)
                {
                    ImGui::SetCursorScreenPos(clickMin);
                    ImGui::InvisibleButton("##liveLyricsClickArea", clickSize);
                    bool isHovered = ImGui::IsItemHovered();
                    bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

                    if (isHovered)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
                        dl->AddRect(clickMin, clickMax, DS::AccentLight, 5.0f, 0, 1.2f);

                        const char* editBadge = "Clic para editar";
                        ImVec2 bSz = ImGui::CalcTextSize(editBadge);
                        float bx = SafeClamp(clickMax.x - bSz.x - 10.0f, p0.x + 8.0f, p1.x - bSz.x - 8.0f);
                        float by = clickMin.y - bSz.y - 6.0f;
                        if (by < p0.y + 4.0f) by = SafeClamp(clickMin.y + 4.0f, p0.y + 4.0f, p1.y - bSz.y - 4.0f);
                        ImVec2 bPos(bx, by);
                        dl->AddRectFilled(ImVec2(bPos.x - 6.0f, bPos.y - 3.0f), ImVec2(bPos.x + bSz.x + 6.0f, bPos.y + bSz.y + 3.0f),
                                          IM_COL32(22, 24, 28, 235), 4.0f);
                        dl->AddRect(ImVec2(bPos.x - 6.0f, bPos.y - 3.0f), ImVec2(bPos.x + bSz.x + 6.0f, bPos.y + bSz.y + 3.0f),
                                    DS::AccentLight, 4.0f, 0, 1.0f);
                        dl->AddText(bPos, DS::TextPrimary, editBadge);

                        if (isSong)
                            ImGui::SetTooltip("Editar letra en tiempo real (se guardará en el .txt al terminar)");
                        else
                            ImGui::SetTooltip("Editar texto en vivo");
                    }

                    if (isClicked)
                    {
                        m_IsEditingLiveLyrics = true;
                        m_LiveLyricsEditBuf = state.currentText;
                        m_LiveLyricsEditBuf.reserve(std::max<size_t>(256, m_LiveLyricsEditBuf.size() + 64));
                        m_LastLiveLyricsEditBuf = state.currentText;
                        m_LiveLyricsOriginalText = state.currentText;
                        m_EditingSongFilename = isSong ? sel.title : "";
                        m_EditingStanzaIndex = core.GetActiveSlideIndex();
                        if (m_EditingStanzaIndex < 0 && isSong)
                        {
                            for (size_t i = 0; i < sel.contentData.size(); ++i)
                            {
                                if (sel.contentData[i] == state.currentText)
                                {
                                    m_EditingStanzaIndex = static_cast<int>(i);
                                    break;
                                }
                            }
                        }
                        m_FocusLiveEditInput = true;
                    }
                }
                else
                {
                    const float margin   = 6.0f;
                    const float maxEditW = std::max(120.0f, drawW - margin * 2.0f);
                    const float minEditW = std::min(250.0f, maxEditW);
                    const float editW    = SafeClamp(clickSize.x + 80.0f, minEditW, maxEditW);

                    const float maxEditH = std::max(84.0f, drawH - margin * 2.0f);
                    const float minEditH = std::min(126.0f, maxEditH);
                    const float editH    = SafeClamp(clickSize.y + 68.0f, minEditH, maxEditH);

                    float editX = clickMin.x + clickSize.x * 0.5f - editW * 0.5f;
                    float editY = clickMin.y + clickSize.y * 0.5f - editH * 0.5f;
                    editX = SafeClamp(editX, p0.x + margin, p1.x - editW - margin);
                    editY = SafeClamp(editY, p0.y + margin, p1.y - editH - margin);

                    ImVec2 cardP0(editX, editY);
                    ImVec2 cardP1(editX + editW, editY + editH);

                    dl->AddRectFilled(ImVec2(cardP0.x - 3.0f, cardP0.y - 3.0f), ImVec2(cardP1.x + 3.0f, cardP1.y + 3.0f), IM_COL32(0, 0, 0, 90), 8.0f);
                    dl->AddRectFilled(cardP0, cardP1, IM_COL32(20, 21, 24, 248), 6.0f);
                    dl->AddRect(cardP0, cardP1, DS::AccentLight, 6.0f, 0, 1.2f);

                    const float headerH   = 28.0f;
                    const float btnW      = (editW < 260.0f) ? 54.0f : 64.0f;
                    const float btnH      = 22.0f;
                    const float btnsStart = std::max(cardP0.x + 8.0f, cardP1.x - 8.0f - btnW * 2.0f - 6.0f);

                    const char* hdrTitle = "EDICIÓN EN VIVO";
                    ImVec2 hdrSz = ImGui::CalcTextSize(hdrTitle);
                    if (btnsStart - (cardP0.x + 10.0f) > hdrSz.x + 6.0f)
                    {
                        dl->AddText(ImVec2(cardP0.x + 10.0f, cardP0.y + (headerH - hdrSz.y) * 0.5f + 1.0f),
                                    DS::TextPrimary, hdrTitle);

                        const char* hdrHint = "Enter: Guardar · Ctrl+Enter: Línea · Esc: Cancelar";
                        ImVec2 hintSz = ImGui::CalcTextSize(hdrHint);
                        const float hintX = cardP0.x + 10.0f + hdrSz.x + 10.0f;
                        if (btnsStart - hintX > hintSz.x + 8.0f)
                        {
                            dl->AddText(ImVec2(hintX, cardP0.y + (headerH - hintSz.y) * 0.5f + 1.0f),
                                        DS::TextHint, hdrHint);
                        }
                    }

                    ImGui::SetCursorScreenPos(ImVec2(btnsStart, cardP0.y + 4.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.26f, 0.30f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.34f, 0.38f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.22f, 0.26f, 1.0f));
                    bool wantsSave = ImGui::Button("Guardar##liveEditSave", ImVec2(btnW, btnH));
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.17f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.25f, 0.28f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
                    bool wantsCancel = ImGui::Button("Cancelar##liveEditCancel", ImVec2(btnW, btnH));
                    ImGui::PopStyleColor(3);

                    ImGui::PopStyleVar(2);

                    const float inputY = cardP0.y + headerH + 4.0f;
                    const float inputW = std::max(80.0f, editW - 16.0f);
                    const float inputH = std::max(36.0f, editH - headerH - 12.0f);
                    ImGui::SetCursorScreenPos(ImVec2(cardP0.x + 8.0f, inputY));

                    if (m_FocusLiveEditInput)
                    {
                        ImGui::SetKeyboardFocusHere();
                        m_FocusLiveEditInput = false;
                    }

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.12f, 0.96f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.13f, 0.15f, 0.98f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.13f, 0.14f, 0.16f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.45f, 0.48f, 0.54f, 0.65f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

                    ImGuiInputTextFlags editFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                    ImGuiInputTextFlags_CtrlEnterForNewLine;
                    bool committed = InputTextMultilineStd("##liveLyricsEditInput", &m_LiveLyricsEditBuf,
                                                          ImVec2(inputW, inputH), editFlags);

                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(4);

                    if (m_LiveLyricsEditBuf != m_LastLiveLyricsEditBuf)
                    {
                        m_LastLiveLyricsEditBuf = m_LiveLyricsEditBuf;
                        core.SetLayer2_Text(m_LiveLyricsEditBuf, m_EditingStanzaIndex, false);
                    }

                    if (committed || wantsSave)
                    {
                        if (isSong && !m_EditingSongFilename.empty() && m_EditingStanzaIndex >= 0)
                        {
                            ProyecThor::Library::UpdateSongStanza(m_EditingSongFilename, m_EditingStanzaIndex, m_LiveLyricsEditBuf);
                        }
                        core.SetLayer2_Text(m_LiveLyricsEditBuf, m_EditingStanzaIndex, false);
                        m_IsEditingLiveLyrics = false;
                        m_LiveLyricsSavedFeedbackTime = ImGui::GetTime();
                    }
                    else if (wantsCancel || ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        core.SetLayer2_Text(m_LiveLyricsOriginalText, m_EditingStanzaIndex, false);
                        m_IsEditingLiveLyrics = false;
                    }
                }
            }
        }
        else if (m_IsEditingLiveLyrics)
        {
            m_IsEditingLiveLyrics = false;
        }

        if (ImGui::GetTime() - m_LiveLyricsSavedFeedbackTime < 2.2)
        {
            std::string toast = "Guardado: " + (!m_EditingSongFilename.empty() ? m_EditingSongFilename : "texto");
            ImVec2 tSz = ImGui::CalcTextSize(toast.c_str());
            ImVec2 tPos(p0.x + (drawW - tSz.x) * 0.5f, p0.y + 10.0f);
            dl->AddRectFilled(ImVec2(tPos.x - 10.0f, tPos.y - 5.0f), ImVec2(tPos.x + tSz.x + 10.0f, tPos.y + tSz.y + 5.0f),
                              IM_COL32(22, 36, 28, 235), 5.0f);
            dl->AddRect(ImVec2(tPos.x - 10.0f, tPos.y - 5.0f), ImVec2(tPos.x + tSz.x + 10.0f, tPos.y + tSz.y + 5.0f),
                        IM_COL32(72, 208, 148, 180), 5.0f, 0, 1.0f);
            dl->AddText(tPos, IM_COL32(225, 245, 232, 255), toast.c_str());
        }
    }

    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(drawW, drawH));
}

}
