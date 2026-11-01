#include "ui/framework/FilePicker.h"
#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include "core/PresentationCore.h"
#include "core/AuthLicenseManager.h"
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
#include <algorithm>
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

static constexpr float kQuickActionsRailW = 40.0f;
static constexpr float kConfigStripH = 34.0f;

namespace {

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

bool DrawPadButton(const char* iconName, float iconSize, ImVec4 padColor, ImVec2 btnSize, bool lit,
                    DrawIconFn vectorIcon = nullptr)
{
    ImVec4 baseCol = padColor;
    ImVec4 hovCol  = Brighten(baseCol, 0.12f);
    ImVec4 actCol  = Brighten(padColor, -0.10f);

    ImGui::PushStyleColor(ImGuiCol_Button,        baseCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  hovCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   actCol);
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, lit ? 0.40f : 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::Button("", btnSize);
    bool   isHeld  = ImGui::IsItemActive();
    ImVec2 p1      = { p0.x + btnSize.x, p0.y + btnSize.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (lit)
    {
        dl->AddRectFilled(p0, p1,
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)), DS::RadiusMedium);
    }

    std::string key = iconName ? iconName : "";
    if (key == "arrow_left") key = "arrow_back";
    else if (key == "arrow_right") key = "arrow_forward";
    else if (key == "skip_previous") key = "skip_prev";

    auto it = StyleGeneralApp::Icons.find(key);
    if (it == StyleGeneralApp::Icons.end() && iconName)
        it = StyleGeneralApp::Icons.find(iconName);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);

    float  offsetY = isHeld ? 2.0f : 0.0f;
    ImVec2 center  = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f + offsetY };

    const float iconLuma = 0.299f * baseCol.x + 0.587f * baseCol.y + 0.114f * baseCol.z;
    const ImU32 iconTint = (iconLuma > 0.55f) ? IM_COL32(20, 20, 24, 255) : IM_COL32(255, 255, 255, 255);

    const char* symbolGlyph = nullptr;
    std::string iName = key;
    if (iName == "replay_10")            symbolGlyph = "\xE2\x8F\xAA";
    else if (iName == "play")            symbolGlyph = "\xE2\x96\xB6";
    else if (iName == "pause")           symbolGlyph = "\xE2\x8F\xB8";
    else if (iName == "forward_10")      symbolGlyph = "\xE2\x8F\xA9";
    else if (iName == "stop")            symbolGlyph = "\xE2\x8F\xB9";
    else if (iName == "volume_up")       symbolGlyph = "\xF0\x9F\x94\x8A";
    else if (iName == "no_sound")        symbolGlyph = "\xF0\x9F\x94\x87";
    else if (iName == "arrow_back")      symbolGlyph = "\xE2\x8F\xAE";
    else if (iName == "arrow_forward")   symbolGlyph = "\xE2\x8F\xAD";

    if (hasTexture)
    {
        dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
            { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f },
            { center.x + iconSize * 0.5f, center.y + iconSize * 0.5f },
            ImVec2(0, 0), ImVec2(1, 1),
            iconTint);
    }
    else if (vectorIcon)
    {
        ImVec2 origin = { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f };
        vectorIcon(dl, origin, iconSize, iconTint);
    }
    else if (iName == "arrow_back" || iName == "arrow_left")
    {
        float chW = iconSize * 0.65f;
        float chH = iconSize * 0.90f;
        ImVec2 pts[3] = {
            { center.x + chW * 0.25f, center.y - chH * 0.45f },
            { center.x - chW * 0.35f, center.y },
            { center.x + chW * 0.25f, center.y + chH * 0.45f }
        };
        dl->AddPolyline(pts, 3, iconTint, 0, 1.8f);
    }
    else if (iName == "arrow_forward" || iName == "arrow_right")
    {
        float chW = iconSize * 0.65f;
        float chH = iconSize * 0.90f;
        ImVec2 pts[3] = {
            { center.x - chW * 0.25f, center.y - chH * 0.45f },
            { center.x + chW * 0.35f, center.y },
            { center.x - chW * 0.25f, center.y + chH * 0.45f }
        };
        dl->AddPolyline(pts, 3, iconTint, 0, 1.8f);
    }
    else if (symbolGlyph)
    {
        ImFont* font = ImGui::GetFont();
        float fontSz = iconSize * 1.15f;
        ImVec2 glyphSz = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, symbolGlyph);
        ImVec2 glyphPos = { center.x - glyphSz.x * 0.5f, center.y - glyphSz.y * 0.5f };
        dl->AddText(font, fontSz, glyphPos, iconTint, symbolGlyph);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
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
        visible = ImGui::Begin("Vista en Vivo");
    }

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        const bool wideShort = avail.x > avail.y * 1.8f && avail.y > 8.0f;

        if (wideShort)
        {
            const bool showQuickActionsCompact = ProyecThor::Settings::SettingsManager::Get()
                                                        .GetSettings().general.showViewQuickActions;
            RenderCompactWide(avail, showQuickActionsCompact);
            RenderPadEditorModal();
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }

        const bool  showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                            .GetSettings().general.showViewQuickActions;
        const bool  narrowColumn = avail.x < 340.0f;
        const bool  showClearRail = false; // Rail derecho movido hacia abajo reemplazando la barra inferior
        const float railW      = 0.0f;
        const float stripH     = showQuickActions ? kConfigStripH : 0.0f;
        const float topAreaH   = avail.y;
        const float contentW   = avail.x;

        if (contentW > 8.0f && topAreaH > 8.0f)
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

            const float dotsH = 0.0f;

            const float topReservedH = dotsH;

            const float remain2   = std::max(0.0f, topAreaH - topReservedH - stripH);
            const float minVideoH = 40.0f;

            float naturalVideoH = contentW / std::max(0.1f, srcAspect);
            float videoH     = std::clamp(naturalVideoH, minVideoH, std::max(minVideoH, remain2 * 0.65f));
            const bool  toolActive   = (m_ActiveTool != InlineTool::None);
            const float sobrante     = std::max(0.0f, remain2 - videoH);
            float transportH = std::min(kLiveTransportMinH, sobrante);
            float inlineToolH = toolActive ? std::max(0.0f, sobrante - transportH) : 0.0f;

            ImGui::SetCursorPosY(topReservedH);
            RenderContent(contentW, videoH);

            ImGui::SetCursorPosY(topReservedH + videoH);
            RenderLiveTransport(contentW, transportH);

            if (showQuickActions)
            {
                ImGui::SetCursorPosY(topReservedH + videoH + transportH);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
                ImGui::BeginChild("##viewQuickActionsConfigStrip", ImVec2(contentW, stripH), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                RenderQuickActionsConfig(stripH);
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }

            const float bottomAreaH = std::max(0.0f, sobrante - transportH);
            if (bottomAreaH > 15.0f)
            {
                ImGui::SetCursorPosY(topReservedH + videoH + transportH + stripH);
                if (m_ActiveTool != InlineTool::None && inlineToolH > 8.0f)
                {
                    RenderInlineTool(contentW, bottomAreaH);
                }
                else
                {
                    RenderPadsContent(contentW, bottomAreaH);
                }
            }

            ImGui::EndChild();
        }

        if (showClearRail)
        {
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
            ImGui::BeginChild("##viewQuickActions", ImVec2(railW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActionsClear(railW);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // ── Loop de Carpeta de Videos en Fondo si está activa ──
        if (!m_FolderPlaylistVideos.empty()) {
            auto& core = Core::PresentationCore::Get();
            if (Core::VLCBasePlayer* bg = core.GetBackgroundPlayer()) {
                if (bg->ConsumeEndReached()) {
                    m_FolderPlaylistIndex = (m_FolderPlaylistIndex + 1) % m_FolderPlaylistVideos.size();
                    core.SetBackgroundMedia(m_FolderPlaylistVideos[m_FolderPlaylistIndex], true, false);
                }
            }
        }

        // ── Modal de Edición de Escenas ──
        RenderPadEditorModal();

        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActionsClear(float railW)
{
    auto& core = Core::PresentationCore::Get();

    auto* announcements= core.GetAnnouncementsRef();
    auto* oclock       = core.GetOClockRef();
    auto* capturePanel = core.GetCapturePanelRef();

    bool showText  = core.GetState().showText;
    bool discLive  = core.GetState().bgType == Core::PresentationState::BackgroundType::Audio;
    bool bgLive    = core.GetState().bgType != Core::PresentationState::BackgroundType::SolidColor;
    bool annLive   = announcements && announcements->IsLive();
    bool clockLive = oclock && oclock->IsLive();
    bool capLive   = capturePanel && capturePanel->IsLive();
    bool overlayLive = core.HasOverlay() || core.HasOverlayClockLayer();

    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeContent= Design::k_EQ_Yellow;
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);
    ImVec4 tintOnYellow = ImVec4(0.10f, 0.09f, 0.06f, 1.0f);
    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);

    struct ActionDef {
        const char* id;
        const char* icon;
        DrawIconFn  vectorIcon;
        const char* fallbackGlyph;
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    ActionDef actions[7] = {
        { "vaClearText", "cleaning_services", nullptr, "\xF0\x9F\x93\x9D", "Limpiar texto",
          hoverClear, activeContent, showText,  showText  ? tintOnYellow : textPrimary },
        { "vaClearDisc", "stop", nullptr, "\xF0\x9F\x92\xBF", "Detener disco en vivo",
          hoverClear, activeContent, discLive,  discLive  ? tintOnYellow : textPrimary },
        { "vaClearBg",   "original_screen", nullptr, "\xF0\x9F\x96\xBC", "Quitar fondo",
          hoverClear, activeContent, bgLive,    bgLive    ? tintOnYellow : textPrimary },
        { "vaClearOverlay", "fit_screen", nullptr, "\xF0\x9F\x93\x91", "Quitar overlay",
          hoverClear, activeContent, overlayLive, overlayLive ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "volume_up", nullptr, "\xF0\x9F\x93\xA2", "Detener anuncios",
          hoverClear, activeContent, annLive,   annLive   ? tintOnYellow : textPrimary },
        { "vaClearClock","", nullptr, "\xE2\x8F\xB0", "Quitar reloj",
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "motion_play", nullptr, "\xF0\x9F\x93\xB9", "Detener captura",
          hoverClear, activeContent, capLive,   capLive   ? tintOnYellow : textPrimary },
    };

    const float  cellPad = 4.0f;
    const ImVec2 cellSize(railW - cellPad * 2.0f, railW - cellPad * 2.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
    ImGui::Dummy(ImVec2(railW, 4.0f));

    for (int i = 0; i < 7; i++)
    {
        ImGui::SetCursorPosX(cellPad);
        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) {
                core.StopBackgroundMedia();
                if (!core.GetState().currentText.empty())
                    core.SetProjecting(true);
            }
            else if (i == 3) core.ClearOverlay();
            else if (i == 4 && announcements) announcements->SetLive(false);
            else if (i == 5 && oclock)        oclock->StopTransmitting();
            else if (i == 6 && capturePanel)  capturePanel->Stop();
        }
    }

    ImGui::PopStyleVar();
}

void ViewPanel::RenderQuickActionsConfig(float stripH)
{
    auto& core = Core::PresentationCore::Get();
    bool stretchOn = core.GetStretchToFill();

    auto* announcements= core.GetAnnouncementsRef();
    auto* oclock       = core.GetOClockRef();
    auto* capturePanel = core.GetCapturePanelRef();

    bool showText    = core.GetState().showText;
    bool discLive    = core.GetState().bgType == Core::PresentationState::BackgroundType::Audio;
    bool bgLive      = core.GetState().bgType != Core::PresentationState::BackgroundType::SolidColor;
    bool annLive     = announcements && announcements->IsLive();
    bool clockLive   = oclock && oclock->IsLive();
    bool capLive     = capturePanel && capturePanel->IsLive();
    bool overlayLive = core.HasOverlay() || core.HasOverlayClockLayer();

    ImVec4 baseFill      = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear    = ToVec4(DS::AccentColorDim);
    ImVec4 activeContent = Design::k_EQ_Yellow;
    ImVec4 activeStretch = ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 textPrimary   = ToVec4(DS::TextPrimary);
    ImVec4 tintOnYellow  = ImVec4(0.10f, 0.09f, 0.06f, 1.0f);

    struct ActionDef {
        const char* id;
        const char* icon;
        DrawIconFn  vectorIcon;
        const char* fallbackGlyph;
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    const bool isPro = ProyecThor::Core::AuthLicenseManager::Get().IsPro();
    if (!isPro && m_PreviewSource == PreviewSource::Stage) {
        m_PreviewSource = PreviewSource::Publico;
    }

    std::vector<PreviewSource> availableSources;
    availableSources.push_back(PreviewSource::Publico);
    if (isPro) {
        availableSources.push_back(PreviewSource::Stage);
    }
    availableSources.push_back(PreviewSource::Transmision);
    availableSources.push_back(PreviewSource::Lan);

    auto getPreviewSourceLabel = [](PreviewSource s) -> const char* {
        switch (s) {
            case PreviewSource::Publico:     return "Público";
            case PreviewSource::Stage:       return "Stage";
            case PreviewSource::Transmision: return "Transmisión";
            case PreviewSource::Lan:         return "Inalámbrica";
            default:                         return "Público";
        }
    };

    int curSourceIdx = 0;
    for (size_t idx = 0; idx < availableSources.size(); ++idx) {
        if (availableSources[idx] == m_PreviewSource) {
            curSourceIdx = (int)idx;
            break;
        }
    }
    int nextSourceIdx = (curSourceIdx + 1) % (int)availableSources.size();

    const bool previewingAlt = (m_PreviewSource != PreviewSource::Publico);
    char previewSourceTooltip[96];
    snprintf(previewSourceTooltip, sizeof(previewSourceTooltip), "Viendo: %s (click para ver %s)",
             getPreviewSourceLabel(availableSources[curSourceIdx]),
             getPreviewSourceLabel(availableSources[nextSourceIdx]));

    const bool detachedOn = m_IsDetached;
    const char* detachTip = m_IsDetached ? "Acoplar panel al entorno (Ctrl+Shift+D)" : "Poner en ventana aparte (Ctrl+Shift+D)";

    ActionDef actions[10] = {
        { "vaClearText", "cleaning_services", AppIcons::DrawIcon_Broom, "\xF0\x9F\x93\x9D", "Limpiar texto (Ctrl+L)",
          hoverClear, activeContent, showText, showText ? tintOnYellow : textPrimary },
        { "vaClearDisc", "stop", AppIcons::DrawIcon_Disc, "\xF0\x9F\x92\xBF", "Detener disco en vivo",
          hoverClear, activeContent, discLive, discLive ? tintOnYellow : textPrimary },
        { "vaClearBg",   "original_screen", AppIcons::DrawIcon_Image, "\xF0\x9F\x96\xBC", "Quitar fondo",
          hoverClear, activeContent, bgLive, bgLive ? tintOnYellow : textPrimary },
        { "vaClearOverlay", "fit_screen", AppIcons::DrawIcon_Overlay, "\xF0\x9F\x93\x91", "Quitar overlay",
          hoverClear, activeContent, overlayLive, overlayLive ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "volume_up", HomeIcons::DrawIcon_Megaphone, "\xF0\x9F\x93\xA2", "Detener anuncios",
          hoverClear, activeContent, annLive, annLive ? tintOnYellow : textPrimary },
        { "vaClearClock","", HomeIcons::DrawIcon_Clock, "\xE2\x8F\xB0", "Quitar reloj",
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "motion_play", HomeIcons::DrawIcon_Camera, "\xF0\x9F\x93\xB9", "Detener captura",
          hoverClear, activeContent, capLive, capLive ? tintOnYellow : textPrimary },
        { "vaPreviewSource", "repeat", AppIcons::DrawIcon_Swap, "\xF0\x9F\x94\x84", previewSourceTooltip,
          hoverClear, activeStretch, previewingAlt, textPrimary },
        { "vaStretch",   "fit_screen", AppIcons::DrawIcon_Monitor, "\xF0\x9F\x93\x90", "Alternar proporción",
          hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaDetach",    "", AppIcons::DrawIcon_Popout, m_IsDetached ? "\xE2\x87\xB1" : "\xE2\x87\xB2", detachTip,
          hoverClear, activeStretch, detachedOn, textPrimary },
    };

    constexpr int kCount = 10;
    constexpr float kGap = 4.0f;
    const float   btnH   = std::min(stripH - 4.0f, 30.0f);
    const float   totalW = ImGui::GetContentRegionAvail().x;
    const float   cellW  = (totalW - kGap * (kCount - 1) - 8.0f) / (float)kCount;
    const ImVec2  cellSize(cellW, btnH);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::SetCursorPosX(4.0f);
    ImGui::SetCursorPosY((stripH - btnH) * 0.5f);

    for (int i = 0; i < kCount; i++)
    {
        if (i > 0) ImGui::SameLine(0.0f, kGap);

        bool clicked = QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn);

        if (clicked)
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) {
                core.StopBackgroundMedia();
                if (!core.GetState().currentText.empty())
                    core.SetProjecting(true);
            }
            else if (i == 3) core.ClearOverlay();
            else if (i == 4 && announcements) announcements->SetLive(false);
            else if (i == 5 && oclock)        oclock->StopTransmitting();
            else if (i == 6 && capturePanel)  capturePanel->Stop();
            else if (i == 7) m_PreviewSource = availableSources[nextSourceIdx];
            else if (i == 8) core.SetStretchToFill(!stretchOn);
            else if (i == 9) SetDetached(!m_IsDetached);
        }
    }

    ImGui::PopStyleVar();
}

void ViewPanel::RenderCompactWide(ImVec2 avail, bool showQuickActions)
{
    const float leftW  = showQuickActions ? 220.0f : 0.0f;
    const float videoW = std::max(0.0f, avail.x - leftW);

    if (showQuickActions && leftW > 8.0f && avail.x > leftW + 40.0f)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
        ImGui::BeginChild("##viewCompactToolbar", ImVec2(leftW, avail.y), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        const float transportH = std::min(kLiveTransportMinH, std::max(60.0f, avail.y));
        RenderLiveTransport(leftW, transportH);

        ImGui::SetCursorPosY(transportH);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
        ImGui::BeginChild("##viewCompactConfig", ImVec2(leftW, kConfigStripH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderQuickActionsConfig(kConfigStripH);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(transportH + kConfigStripH + 6.0f);
        float compactRemainH = std::max(0.0f, avail.y - transportH - kConfigStripH - 8.0f);
        if (compactRemainH > 40.0f)
        {
            RenderPadsContent(leftW, compactRemainH);
        }
        else
        {
            ImGui::SetCursorPosX(std::max(0.0f, (leftW - kQuickActionsRailW) * 0.5f));
            ImGui::BeginGroup();
            RenderQuickActionsClear(kQuickActionsRailW);
            ImGui::EndGroup();
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
    const ImVec2 secOrigin = ImGui::GetWindowPos();
    const float padX = MT::k_PadLg;
    const float padInnerW = std::max(20.0f, availW - padX * 2.0f);
    const double nowTime = ImGui::GetTime();
    ImFont* font = ImGui::GetFont();

    // Línea divisoria sutil integrada con la consola superior
    dl->AddLine(
        { secOrigin.x + padX, secOrigin.y + 1.0f },
        { secOrigin.x + availW - padX, secOrigin.y + 1.0f },
        ImGui::GetColorU32(MT::k_BorderSubtle), 1.0f);

    // ── Barra superior minimalista estilo hardware ──
    const float topBarY = 5.0f;
    const float topBtnH = 20.0f;
    const float topBtnW = 22.0f;

    // Ícono + sello sutil "PADS"
    AppIcons::DrawIcon_Pads(dl, { secOrigin.x + padX, secOrigin.y + topBarY + 3.5f }, 13.0f,
                            IM_COL32(155, 165, 190, 210));
    dl->AddText(font, 11.0f,
                { secOrigin.x + padX + 17.0f, secOrigin.y + topBarY + 4.0f },
                ImGui::GetColorU32(MT::k_TextSecondary), "PADS");

    // Indicador compacto si hay bucle de carpeta activo
    if (!m_FolderPlaylistVideos.empty()) {
        char loopTxt[24];
        snprintf(loopTxt, sizeof(loopTxt), "%zu/%zu",
                 (m_FolderPlaylistIndex % m_FolderPlaylistVideos.size()) + 1,
                 m_FolderPlaylistVideos.size());
        ImVec2 lSz = font->CalcTextSizeA(10.5f, FLT_MAX, 0.0f, loopTxt);
        float pillX = secOrigin.x + padX + 54.0f;
        float pillY = secOrigin.y + topBarY + 2.0f;
        float pillW = 16.0f + lSz.x + 7.0f;
        if (pillX + pillW < secOrigin.x + availW - padX - (topBtnW * 2.0f + 10.0f)) {
            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + 16.0f },
                              IM_COL32(36, 115, 185, 55), 4.0f);
            DrawIcon_FolderLoop(dl, { pillX + 3.0f, pillY + 2.5f }, 11.0f, IM_COL32(110, 195, 255, 230));
            dl->AddText(font, 10.5f, { pillX + 16.0f, pillY + 2.5f }, IM_COL32(160, 215, 255, 230), loopTxt);
        }
    }

    // Dos micro-botones de consola a la derecha (sin texto, nunca se cortan)
    auto DrawMicroActionBtn = [&](const char* id, ImVec2 pos, PadIconDrawFn icFn, const char* tip) -> bool {
        ImGui::SetCursorPos(pos);
        ImVec2 sMin = ImGui::GetCursorScreenPos();
        ImVec2 sMax = { sMin.x + topBtnW, sMin.y + topBtnH };
        bool pr = ImGui::InvisibleButton(id, ImVec2(topBtnW, topBtnH));
        bool hv = ImGui::IsItemHovered();
        bool ac = ImGui::IsItemActive();

        ImU32 bgC = ac ? ImGui::GetColorU32(MT::k_NeutBtnAct)
                       : (hv ? ImGui::GetColorU32(MT::k_NeutBtnHov) : ImGui::GetColorU32(MT::k_NeutBtn));
        dl->AddRectFilled(sMin, sMax, bgC, 5.0f);
        dl->AddRect(sMin, sMax, IM_COL32(255, 255, 255, hv ? 40 : 18), 5.0f);
        float icS = 12.0f;
        icFn(dl, { sMin.x + (topBtnW - icS) * 0.5f, sMin.y + (topBtnH - icS) * 0.5f },
             icS, hv ? IM_COL32(245, 248, 255, 255) : IM_COL32(165, 175, 198, 225));
        if (hv && tip) ImGui::SetTooltip("%s", tip);
        return pr;
    };

    const float rightClusterX = availW - padX - (topBtnW * 2.0f + 4.0f);
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

    if (DrawMicroActionBtn("##padQuickCfg", { rightClusterX + topBtnW + 4.0f, topBarY }, AppIcons::DrawIcon_Settings, "Editar escenas"))
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

    // ── Grilla de Pads estilo Stream Deck / Launchpad ──
    const float gridStartY = topBarY + topBtnH + 6.0f;
    const float remainH = std::max(28.0f, availH - gridStartY - 6.0f);

    const int totalPads = ProyecThor::Settings::kPadCount;
    int cols = 4;
    if (padInnerW >= 500.0f && remainH < 105.0f) cols = 8;
    else if (padInnerW < 175.0f)                 cols = 2;
    else                                         cols = 4;

    const int   rows = (totalPads + cols - 1) / cols;
    const float gap  = 6.5f;
    const float padCellW = std::max(26.0f, (padInnerW - gap * (float)(cols - 1)) / (float)cols);
    const float idealMaxH = std::clamp(padCellW * 0.92f, 44.0f, 76.0f);
    const float padCellH = std::clamp((remainH - gap * (float)(rows - 1)) / (float)rows, 28.0f, idealMaxH);

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

        // Zona de engranaje en la esquina superior derecha al pasar el cursor
        ImRect gearRect({ pMax.x - 19.0f, pMin.y + 2.0f }, { pMax.x - 2.0f, pMin.y + 19.0f });
        bool gearHovered = hovered && (padCellW >= 42.0f) && gearRect.Contains(ImGui::GetIO().MousePos);

        const float rounding = 8.0f;
        char idxStr[8];
        snprintf(idxStr, sizeof(idxStr), "%d", i + 1);

        if (!pad.assigned)
        {
            // ── Pad vacío: tecla oscura empotrada, sin texto, glifo '+' central ──
            ImU32 bgCol = (hovered && !gearHovered)
                ? IM_COL32(28, 32, 42, 255)
                : IM_COL32(13, 15, 20, 235);
            dl->AddRectFilled(pMin, pMax, bgCol, rounding);
            dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, hovered ? 42 : 16), rounding, 0, 1.0f);

            // Índice discreto en esquina superior izquierda
            dl->AddText(font, 10.0f, { pMin.x + 6.0f, pMin.y + 4.0f },
                        IM_COL32(255, 255, 255, hovered ? 85 : 36), idxStr);

            // Símbolo '+' limpio en el centro
            ImVec2 center = { (pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f + (held && !gearHovered ? 1.0f : 0.0f) };
            if (hovered && !gearHovered) {
                dl->AddCircleFilled(center, std::min(padCellH, padCellW) * 0.22f, IM_COL32(255, 255, 255, 14), 16);
            }
            float plusSz = std::clamp(std::min(padCellW, padCellH) * 0.24f, 10.0f, 15.0f);
            DrawIcon_PlusKey(dl, { center.x - plusSz * 0.5f, center.y - plusSz * 0.5f }, plusSz,
                             IM_COL32(255, 255, 255, (hovered && !gearHovered) ? 195 : 42));

            // Engranaje sutil en hover para abrir el configurador
            if (hovered && padCellW >= 42.0f) {
                if (gearHovered) {
                    dl->AddRectFilled(gearRect.Min, gearRect.Max, IM_COL32(255, 255, 255, 28), 4.0f);
                }
                AppIcons::DrawIcon_Settings(dl, { gearRect.Min.x + 3.0f, gearRect.Min.y + 3.0f }, 11.0f,
                                            IM_COL32(255, 255, 255, gearHovered ? 240 : 115));
            }
        }
        else
        {
            // ── Pad asignado: tecla táctil iluminada estilo consola broadcast ──
            const ImVec4 accent(pad.color[0], pad.color[1], pad.color[2], 1.0f);
            const bool   isLivePad = (i == m_LastActivatedPadIndex && state.isProjecting);
            const double sinceTrig = nowTime - m_LastPadActivatedFeedbackTime;
            const float  trigFlash = (i == m_LastActivatedPadIndex && sinceTrig >= 0.0 && sinceTrig < 0.45)
                                        ? (float)(1.0 - sinceTrig / 0.45) : 0.0f;

            const float tintAmt = (isLivePad ? 0.28f : (hovered && !gearHovered ? 0.24f : 0.15f)) + trigFlash * 0.18f;
            ImVec4 keyBg = {
                Lerp(MT::k_NeutBtn.x, accent.x, tintAmt),
                Lerp(MT::k_NeutBtn.y, accent.y, tintAmt),
                Lerp(MT::k_NeutBtn.z, accent.z, tintAmt),
                1.0f
            };
            if (held && !gearHovered) keyBg = Brighten(keyBg, -0.05f);

            // Resplandor exterior si es la escena activa
            if (isLivePad || trigFlash > 0.01f) {
                float glowA = (isLivePad ? 0.26f : 0.0f) + trigFlash * 0.35f;
                dl->AddRect({ pMin.x - 1.0f, pMin.y - 1.0f }, { pMax.x + 1.0f, pMax.y + 1.0f },
                            ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, glowA)),
                            rounding + 1.0f, 0, 2.0f);
            }

            dl->AddRectFilled(pMin, pMax, ImGui::ColorConvertFloat4ToU32(keyBg), rounding);

            // Bisel superior táctil
            dl->AddRectFilled(pMin, { pMax.x, pMin.y + padCellH * 0.48f },
                              IM_COL32(255, 255, 255, hovered ? 16 : 9),
                              rounding, ImDrawFlags_RoundCornersTop);
            dl->AddLine({ pMin.x + 6.0f, pMin.y + 1.0f }, { pMax.x - 6.0f, pMin.y + 1.0f },
                        IM_COL32(255, 255, 255, hovered ? 42 : 22), 1.0f);

            // Borde nítido
            float bordAlpha = isLivePad ? 0.92f : (hovered ? 0.65f : 0.34f);
            dl->AddRect(pMin, pMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, bordAlpha)),
                        rounding, 0, isLivePad ? 1.5f : 1.0f);

            // Barra LED inferior
            float ledInset = std::min(10.0f, padCellW * 0.20f);
            dl->AddRectFilled(
                { pMin.x + ledInset, pMax.y - 3.5f },
                { pMax.x - ledInset, pMax.y - 1.0f },
                ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, isLivePad ? 1.0f : 0.72f)),
                1.5f);

            // Índice en esquina superior izquierda
            dl->AddText(font, 10.0f, { pMin.x + 6.0f, pMin.y + 4.0f },
                        IM_COL32(255, 255, 255, isLivePad ? 155 : 75), idxStr);

            // Esquina superior derecha: engranaje al pasar el cursor, o micro-iconos de capas activas
            if (hovered && padCellW >= 42.0f) {
                if (gearHovered) {
                    dl->AddRectFilled(gearRect.Min, gearRect.Max, IM_COL32(0, 0, 0, 110), 4.0f);
                }
                AppIcons::DrawIcon_Settings(dl, { gearRect.Min.x + 3.0f, gearRect.Min.y + 3.0f }, 11.0f,
                                            IM_COL32(255, 255, 255, gearHovered ? 250 : 155));
            } else if (padCellW >= 54.0f) {
                float miX = pMax.x - 5.0f;
                const float miY = pMin.y + 5.0f;
                const float miSz = 9.0f;
                const ImU32 miCol = IM_COL32(235, 240, 252, 125);
                if (pad.applyCapture && pad.hasCapture) {
                    miX -= miSz;
                    HomeIcons::DrawIcon_Camera(dl, { miX, miY }, miSz, miCol);
                    miX -= 2.5f;
                }
                if (pad.applyText && pad.hasText) {
                    miX -= miSz;
                    AppIcons::DrawIcon_TextAa(dl, { miX, miY }, miSz, miCol);
                    miX -= 2.5f;
                }
                if (pad.applyBackground && pad.hasBackground) {
                    miX -= miSz;
                    if (pad.bgType == 4) DrawIcon_FolderLoop(dl, { miX, miY }, miSz, miCol);
                    else                 AppIcons::DrawIcon_Image(dl, { miX, miY }, miSz, miCol);
                }
            }

            // Ícono central + etiqueta corta opcional
            const float pressY = (held && !gearHovered) ? 1.5f : 0.0f;
            const bool  showLabel = !pad.name.empty() && padCellH >= 44.0f;
            const float icSz = std::clamp(padCellH * (showLabel ? 0.35f : 0.44f), 14.0f, 25.0f);
            const float icCenterY = showLabel
                ? (pMin.y + (padCellH - 15.0f) * 0.50f + 2.0f + pressY)
                : (pMin.y + padCellH * 0.50f - 1.0f + pressY);

            ImVec4 icTintF = {
                Lerp(accent.x, 1.0f, isLivePad ? 0.82f : 0.68f),
                Lerp(accent.y, 1.0f, isLivePad ? 0.82f : 0.68f),
                Lerp(accent.z, 1.0f, isLivePad ? 0.82f : 0.68f),
                isLivePad ? 1.0f : 0.94f
            };
            const auto& iconEntry = PadIconFor(pad.iconIndex);
            iconEntry.draw(dl,
                           { (pMin.x + pMax.x - icSz) * 0.5f, icCenterY - icSz * 0.5f },
                           icSz,
                           ImGui::ColorConvertFloat4ToU32(icTintF));

            if (showLabel) {
                const float lblFontSz = 11.0f;
                std::string fitLbl = TruncateUTF8ToWidth(font, lblFontSz, pad.name, padCellW - 10.0f);
                if (!fitLbl.empty()) {
                    ImVec2 tSz = font->CalcTextSizeA(lblFontSz, FLT_MAX, 0.0f, fitLbl.c_str());
                    dl->AddText(font, lblFontSz,
                                { pMin.x + (padCellW - tSz.x) * 0.5f, pMax.y - 16.5f + pressY },
                                IM_COL32(236, 240, 250, isLivePad ? 255 : 225),
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
            if (ImGui::BeginMenu("Icono y color")) {
                // Puntos rápidos de color
                for (int cIdx = 0; cIdx < 8; cIdx++) {
                    if (cIdx > 0) ImGui::SameLine(0.0f, 6.0f);
                    ImGui::PushID(cIdx);
                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("##colDot", ImVec2(16.0f, 16.0f))) {
                        for (int k = 0; k < 4; k++) pad.color[k] = kPadPalette[cIdx][k];
                        ProyecThor::Settings::SettingsManager::Get().Save();
                    }
                    ImDrawList* pdl = ImGui::GetWindowDrawList();
                    ImVec2 cc = { cp.x + 8.0f, cp.y + 8.0f };
                    pdl->AddCircleFilled(cc, 6.5f, ImGui::ColorConvertFloat4ToU32(
                        ImVec4(kPadPalette[cIdx][0], kPadPalette[cIdx][1], kPadPalette[cIdx][2], 1.0f)), 16);
                    bool sameCol = std::fabs(pad.color[0] - kPadPalette[cIdx][0]) < 0.03f &&
                                   std::fabs(pad.color[1] - kPadPalette[cIdx][1]) < 0.03f &&
                                   std::fabs(pad.color[2] - kPadPalette[cIdx][2]) < 0.03f;
                    if (sameCol) pdl->AddCircle(cc, 8.0f, IM_COL32(255, 255, 255, 230), 16, 1.5f);
                    ImGui::PopID();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
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
        const ImVec4 accent(m_TempPadEdit.color[0], m_TempPadEdit.color[1], m_TempPadEdit.color[2], 1.0f);

        // ── Selector rápido de Pad 1..8 ──
        {
            const float stripW = ImGui::GetContentRegionAvail().x;
            const float sGap = 5.0f;
            const float pillW = (stripW - sGap * 7.0f) / 8.0f;
            const float pillH = 22.0f;
            for (int pIdx = 0; pIdx < ProyecThor::Settings::kPadCount; pIdx++) {
                if (pIdx > 0) ImGui::SameLine(0.0f, sGap);
                ImGui::PushID(1000 + pIdx);
                ImVec2 sMin = ImGui::GetCursorScreenPos();
                ImVec2 sMax = { sMin.x + pillW, sMin.y + pillH };
                bool pClk = ImGui::InvisibleButton("##selPadSlot", ImVec2(pillW, pillH));
                bool pHov = ImGui::IsItemHovered();
                const bool isCur = (pIdx == m_EditingPadIndex);
                const auto& slotRef = padsArr[pIdx];
                const ImVec4 slotCol(slotRef.color[0], slotRef.color[1], slotRef.color[2], 1.0f);

                if (isCur) {
                    dl->AddRectFilled(sMin, sMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.28f)), 5.0f);
                    dl->AddRect(sMin, sMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.85f)), 5.0f, 0, 1.2f);
                } else {
                    dl->AddRectFilled(sMin, sMax,
                        pHov ? IM_COL32(36, 40, 52, 255) : IM_COL32(18, 20, 27, 230), 5.0f);
                    dl->AddRect(sMin, sMax, IM_COL32(255, 255, 255, pHov ? 36 : 14), 5.0f);
                }

                char slotLbl[8];
                snprintf(slotLbl, sizeof(slotLbl), "%d", pIdx + 1);
                ImVec2 lSz = font->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, slotLbl);
                dl->AddText(font, 11.0f,
                            { sMin.x + (pillW - lSz.x) * 0.5f, sMin.y + (pillH - lSz.y) * 0.5f - (slotRef.assigned ? 1.0f : 0.0f) },
                            isCur ? IM_COL32(255, 255, 255, 255) : IM_COL32(165, 175, 195, 215),
                            slotLbl);

                if (slotRef.assigned) {
                    dl->AddRectFilled({ sMin.x + pillW * 0.28f, sMax.y - 3.0f },
                                      { sMax.x - pillW * 0.28f, sMax.y - 1.2f },
                                      ImGui::ColorConvertFloat4ToU32(slotCol), 1.0f);
                }

                if (pClk && !isCur) {
                    m_EditingPadIndex = pIdx;
                    m_TempPadEdit = padsArr[pIdx];
                    if (!m_TempPadEdit.assigned) {
                        const float* pal = kPadPalette[pIdx % 8];
                        for (int c = 0; c < 4; c++) m_TempPadEdit.color[c] = pal[c];
                        m_TempPadEdit.iconIndex = 2;
                    }
                }
                ImGui::PopID();
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // ── Cabecera Visual: Vista previa de tecla + Nombre + Colores + Captura rápida ──
        ImVec2 previewPos = ImGui::GetCursorScreenPos();
        const float keyPreviewSz = 48.0f;
        bool openIconPicker = ImGui::InvisibleButton("##padPreviewKeyBtn", ImVec2(keyPreviewSz, keyPreviewSz));
        bool prevHov = ImGui::IsItemHovered();
        {
            ImVec2 kMax = { previewPos.x + keyPreviewSz, previewPos.y + keyPreviewSz };
            ImVec4 kBg = {
                Lerp(MT::k_NeutBtn.x, accent.x, prevHov ? 0.30f : 0.22f),
                Lerp(MT::k_NeutBtn.y, accent.y, prevHov ? 0.30f : 0.22f),
                Lerp(MT::k_NeutBtn.z, accent.z, prevHov ? 0.30f : 0.22f),
                1.0f
            };
            dl->AddRectFilled(previewPos, kMax, ImGui::ColorConvertFloat4ToU32(kBg), 9.0f);
            dl->AddRectFilled(previewPos, { kMax.x, previewPos.y + keyPreviewSz * 0.48f },
                              IM_COL32(255, 255, 255, 14), 9.0f, ImDrawFlags_RoundCornersTop);
            dl->AddRect(previewPos, kMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, prevHov ? 0.90f : 0.60f)),
                        9.0f, 0, 1.3f);
            dl->AddRectFilled({ previewPos.x + 9.0f, kMax.y - 3.5f }, { kMax.x - 9.0f, kMax.y - 1.2f },
                              ImGui::ColorConvertFloat4ToU32(accent), 1.5f);

            const float icSz = 22.0f;
            PadIconFor(m_TempPadEdit.iconIndex).draw(
                dl,
                { previewPos.x + (keyPreviewSz - icSz) * 0.5f, previewPos.y + (keyPreviewSz - icSz) * 0.5f - 1.0f },
                icSz, IM_COL32(250, 252, 255, 250));

            char pNum[8];
            snprintf(pNum, sizeof(pNum), "%d", m_EditingPadIndex + 1);
            dl->AddText(font, 9.5f, { previewPos.x + 5.0f, previewPos.y + 3.0f }, IM_COL32(255, 255, 255, 120), pNum);
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

        // Columna derecha de la cabecera (2 filas compactas)
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginGroup();
        {
            const float rightAvail = ImGui::GetContentRegionAvail().x;
            const float snapBtnW = 94.0f;
            const float closeBtnW = 24.0f;
            const float nameW = std::max(100.0f, rightAvail - snapBtnW - closeBtnW - 12.0f);

            char nameBuf[64];
            strncpy(nameBuf, m_TempPadEdit.name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, MT::k_Bg3);
            ImGui::SetNextItemWidth(nameW);
            if (ImGui::InputTextWithHint("##padNameInp", "Nombre (opcional)...", nameBuf, sizeof(nameBuf))) {
                m_TempPadEdit.name = nameBuf;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            // Botón Capturar actual (icono vectorial + etiqueta breve)
            ImGui::SameLine(0.0f, 6.0f);
            {
                ImVec2 bMin = ImGui::GetCursorScreenPos();
                ImVec2 bMax = { bMin.x + snapBtnW, bMin.y + 24.0f };
                bool snapClk = ImGui::InvisibleButton("##snapLiveAll", ImVec2(snapBtnW, 24.0f));
                bool snapHov = ImGui::IsItemHovered();
                dl->AddRectFilled(bMin, bMax,
                                  ImGui::ColorConvertFloat4ToU32(snapHov ? MT::k_PrevBtnHov : MT::k_PrevBtn), 6.0f);
                dl->AddRect(bMin, bMax, IM_COL32(120, 185, 255, snapHov ? 140 : 70), 6.0f);
                DrawIcon_CaptureSnap(dl, { bMin.x + 7.0f, bMin.y + 5.0f }, 14.0f, IM_COL32(235, 245, 255, 255));
                dl->AddText(font, 11.5f, { bMin.x + 25.0f, bMin.y + 5.0f }, IM_COL32(235, 245, 255, 255), "Capturar");
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

            // Botón cerrar 'X'
            ImGui::SameLine(0.0f, 6.0f);
            {
                ImVec2 xMin = ImGui::GetCursorScreenPos();
                ImVec2 xMax = { xMin.x + closeBtnW, xMin.y + 24.0f };
                bool xClk = ImGui::InvisibleButton("##closePadModal", ImVec2(closeBtnW, 24.0f));
                bool xHov = ImGui::IsItemHovered();
                if (xHov) dl->AddRectFilled(xMin, xMax, IM_COL32(255, 255, 255, 22), 6.0f);
                DrawIcon_CloseX(dl, { xMin.x + 5.0f, xMin.y + 5.0f }, 14.0f,
                                xHov ? IM_COL32(255, 255, 255, 240) : IM_COL32(165, 172, 190, 200));
                if (xClk) {
                    m_ShowPadEditModal = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            // Fila 2: Swatches de color LED en 1 clic + selector libre
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            for (int cIdx = 0; cIdx < 8; cIdx++) {
                if (cIdx > 0) ImGui::SameLine(0.0f, 6.0f);
                ImGui::PushID(cIdx);
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##palDot", ImVec2(16.0f, 16.0f))) {
                    for (int k = 0; k < 4; k++) m_TempPadEdit.color[k] = kPadPalette[cIdx][k];
                }
                bool cHov = ImGui::IsItemHovered();
                ImVec2 cc = { cp.x + 8.0f, cp.y + 8.0f };
                dl->AddCircleFilled(cc, cHov ? 7.0f : 6.0f,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(kPadPalette[cIdx][0], kPadPalette[cIdx][1], kPadPalette[cIdx][2], 1.0f)), 16);
                bool selCol = std::fabs(m_TempPadEdit.color[0] - kPadPalette[cIdx][0]) < 0.03f &&
                              std::fabs(m_TempPadEdit.color[1] - kPadPalette[cIdx][1]) < 0.03f &&
                              std::fabs(m_TempPadEdit.color[2] - kPadPalette[cIdx][2]) < 0.03f;
                if (selCol) dl->AddCircle(cc, 8.0f, IM_COL32(255, 255, 255, 235), 16, 1.4f);
                ImGui::PopID();
            }
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::ColorEdit4("##customPadCol", m_TempPadEdit.color,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoBorder);
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
    auto& core = Core::PresentationCore::Get();
    Core::VLCBasePlayer* bg = core.GetBackgroundPlayer();

    // Si el fondo en vivo es MUSICA (AudioPanel), el transporte controla esa
    // misma reproduccion (play/pausa, seek, anterior/siguiente, volumen) en vez
    // del reproductor de video de fondo: una sola fuente de verdad.
    UI::AudioPanel* musicPanel = core.GetAudioPanelRef();
    const bool audioLive = musicPanel && musicPanel->IsLiveBackground() &&
                           core.GetState().bgType == Core::PresentationState::BackgroundType::Audio;

    if (audioLive) {
        m_LiveMuted   = musicPanel->IsMusicMuted();
        m_LiveVolume  = static_cast<float>(musicPanel->GetMusicVolume()) * 0.01f;
        m_LivePlaying = musicPanel->IsMusicActive() && !musicPanel->IsMusicPaused();
    } else {
        m_LiveMuted   = core.GetLiveMute();
        m_LiveVolume  = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        m_LivePlaying = bg && !bg->IsPaused();
    }

    int64_t liveLenMs = audioLive ? musicPanel->GetMusicLengthMs() : (bg ? bg->GetLength() : 0);

    if (!audioLive && m_LivePlaying && bg && liveLenMs > 0)
    {
        int64_t curMs = bg->GetTime();
        float   fpos  = (liveLenMs > 0)
            ? static_cast<float>(curMs) / static_cast<float>(liveLenMs)
            : 0.0f;

        if (fpos >= 0.995f && core.GetLiveLoop())
        {
            bg->SetPosition(0.0f);
            bg->SetPause(false);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::BeginChild("##viewLiveTransport", { w, h }, false, ImGuiWindowFlags_NoScrollbar);

    const float innerW = w - MT::k_PadLg * 2.0f;

    ImGui::Spacing();

    int64_t liveCurMs = audioLive ? musicPanel->GetMusicTimeMs()   : (bg ? bg->GetTime()   : 0);
    int64_t liveLen   = audioLive ? musicPanel->GetMusicLengthMs() : (bg ? bg->GetLength() : 0);
    float   livePos   = (liveLen > 0)
        ? std::clamp(static_cast<float>(liveCurMs) / static_cast<float>(liveLen), 0.0f, 1.0f)
        : 0.0f;

    ImGui::SetCursorPosX(MT::k_PadLg);
    float displayPos = livePos;
    if (BMSlider("##vp_tl_live", &displayPos, 0.0f, 1.0f, "",
                 MT::k_LiveTrack, MT::k_LiveGrab,
                 { MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f },
                 innerW))
    {
        if (audioLive) musicPanel->SeekNormalized(displayPos);
        else           core.SetLivePosition(displayPos);
        liveCurMs = static_cast<int64_t>(displayPos * static_cast<float>(liveLen));
    }

    DrawTimeRow(innerW, MT::k_PadLg, liveCurMs, liveLen);
    ImGui::Spacing();

    const float rowH      = std::clamp(ImGui::GetContentRegionAvail().y, 24.0f, 36.0f);
    const float gap       = MT::k_Gap * 1.5f;
    const float padSize   = std::clamp((innerW - gap * 5.0f) / 6.0f, 26.0f, 38.0f);

    const ImVec4 kNeutral = MT::k_NeutBtn;
    const ImVec4 kLive    = MT::k_LiveBtn;

    ImGui::SetCursorPosX(MT::k_PadLg);

    ImGui::PushID("vp_pad_nav_prev");
    if (DrawPadButton("arrow_back", padSize * 0.44f, kNeutral, { padSize, padSize }, false)) {
        if (audioLive) {
            musicPanel->Previous();
        } else {
            core.RequestNavigate(-1);
            auto sel = core.PeekSelection();
            if (sel.type != Core::ItemType::Song &&
                sel.type != Core::ItemType::Bible &&
                sel.type != Core::ItemType::Documents && bg && liveLen > 0)
            {
                float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
                core.SetLivePosition(std::max(0.0f, np));
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(audioLive ? "Pista anterior (reinicia si pasaron 3 s)" : "Retroceder / Anterior (Flecha Izquierda)");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_replay");
    if (DrawPadButton("replay_10", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        if (audioLive) {
            musicPanel->SkipBy(-10);
        } else {
            float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
            core.SetLivePosition(std::max(0.0f, np));
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Atrasar 10 segundos");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* mainIcon = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("vp_pad_main");
    if (DrawPadButton(mainIcon, padSize * 0.40f, m_LivePlaying ? kLive : kNeutral, { padSize, padSize }, m_LivePlaying)) {
        if (audioLive) {
            musicPanel->TogglePlayPause();
        } else if (bg) {
            if (m_LivePlaying) {
                bg->SetPause(true);
            } else {
                core.SetLiveMute(m_LiveMuted);
                core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
                bg->SetPause(false);
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_LivePlaying ? "Pausar" : "Reproducir");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_fwd");
    if (DrawPadButton("forward_10", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        if (audioLive) {
            musicPanel->SkipBy(10);
        } else {
            float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
            core.SetLivePosition(std::min(1.0f, np));
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adelantar 10 segundos");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_stop");
    if (DrawPadButton("stop", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        if (audioLive) {
            musicPanel->Stop();
        } else {
            core.SetLivePosition(0.0f);
            if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(audioLive ? "Detener música" : "Detener video");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_nav_next");
    if (DrawPadButton("arrow_forward", padSize * 0.44f, kNeutral, { padSize, padSize }, false)) {
        if (audioLive) {
            musicPanel->Next();
        } else {
            core.RequestNavigate(1);
            auto sel = core.PeekSelection();
            if (sel.type != Core::ItemType::Song &&
                sel.type != Core::ItemType::Bible &&
                sel.type != Core::ItemType::Documents && bg && liveLen > 0)
            {
                float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
                core.SetLivePosition(std::min(1.0f, np));
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(audioLive ? "Pista siguiente" : "Avanzar / Siguiente (Flecha Derecha)");
    ImGui::PopID();
    ImGui::Spacing();

    const float volRowH   = std::clamp(padSize * 0.85f, 26.0f, 32.0f);
    const float muteW     = volRowH;
    const float pctW      = 44.0f;
    const float faderW    = std::max(50.0f, innerW - muteW - pctW - gap * 2.0f);

    bool        isDanger  = (m_LiveVolume > 1.0f);
    DrawIconFn  speakerFn = m_LiveMuted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn;

    ImGui::SetCursorPosX(MT::k_PadLg);

    ImGui::PushID("vp_pad_mute");
    if (DrawPadButton(m_LiveMuted ? "no_sound" : "volume_up", muteW * 0.44f, m_LiveMuted ? kLive : kNeutral,
                      { muteW, volRowH }, m_LiveMuted, speakerFn)) {
        m_LiveMuted = !m_LiveMuted;
        if (audioLive) {
            musicPanel->SetMusicMuted(m_LiveMuted);
        } else {
            core.SetLiveMute(m_LiveMuted);
            core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(m_LiveMuted ? "Audio silenciado — click para activar" : "Silenciar audio");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImU32 trackCol = ImGui::GetColorU32(MT::k_NeutBtn);
    ImU32 fillCol  = isDanger ? IM_COL32(235, 70, 70, 255) : ImGui::GetColorU32(MT::k_LiveGrab);
    ImU32 capCol   = isDanger ? IM_COL32(255, 90, 90, 255) : IM_COL32(225, 228, 235, 255);

    if (HorizontalFader("##vp_vol_fader", &m_LiveVolume, 0.0f, 2.0f, { faderW, volRowH },
                         trackCol, fillCol, capCol)) {
        if (audioLive) musicPanel->SetMusicVolume(static_cast<int>(m_LiveVolume * 100.0f));
        else           core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Volumen en vivo: %d%%", static_cast<int>(m_LiveVolume * 100.0f));

    ImGui::SameLine(0.0f, gap);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (volRowH - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, isDanger ? IM_COL32(255, 100, 100, 255) : ImGui::GetColorU32(MT::k_TextDim));
    ImGui::Text("%d%%", static_cast<int>(m_LiveVolume * 100.0f));
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

void ViewPanel::RenderContent(float panelW, float panelH)
{
    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

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

    if (m_PreviewSource == PreviewSource::Publico)
    {
        UI::DrawPublicContent(dl, p0, p1, drawW, drawH, m_IsEditingLiveLyrics, true);
    }
    else if (m_PreviewSource == PreviewSource::Stage)
    {
        const bool isPro = ProyecThor::Core::AuthLicenseManager::Get().IsPro();
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
    else if (m_PreviewSource == PreviewSource::Lan)
    {
        auto lanMode = core.GetLanContentMode();
        if (lanMode == Core::OutputContentMode::Live)
        {
            UI::DrawPublicContent(dl, p0, p1, drawW, drawH, false, true);
        }
        else
        {
            dl->AddRectFilled(p0, p1, IM_COL32(10, 10, 12, 255));
            if (lanMode == Core::OutputContentMode::ClockOnly)
            {
                std::time_t now = std::time(nullptr);
                std::tm lt{};
#ifdef _WIN32
                localtime_s(&lt, &now);
#else
                localtime_r(&now, &lt);
#endif
                char buf[16];
                std::strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
                float fontSize = std::clamp(drawH * 0.20f, 24.0f, 160.0f);
                ImFont* f  = ImGui::GetFont();
                ImVec2  ts = f->CalcTextSizeA(fontSize, FLT_MAX, FLT_MAX, buf);
                dl->AddText(f, fontSize, { p0.x + (drawW - ts.x) * 0.5f, p0.y + (drawH - ts.y) * 0.5f },
                            IM_COL32(235, 235, 240, 255), buf);
            }
            else
            {
                const char* msg = "En blanco";
                ImVec2 ts = ImGui::CalcTextSize(msg);
                dl->AddText({ p0.x + (drawW - ts.x) * 0.5f, p0.y + (drawH - ts.y) * 0.5f },
                            IM_COL32(110, 110, 118, 255), msg);
            }
            dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);
        }

        {
            const char* pillLabel[3] = { "En vivo", "Solo reloj", "En blanco" };
            const float pillH = 24.0f, pillGap = 4.0f, pillPad = 8.0f;
            float pillY = p1.y - pillH - pillPad;
            float pillTotalW = drawW - pillPad * 2.0f;
            float pillW = (pillTotalW - pillGap * 2.0f) / 3.0f;
            for (int pi = 0; pi < 3; pi++)
            {
                bool active = (static_cast<int>(lanMode) == pi);
                ImVec2 pillPos = { p0.x + pillPad + pi * (pillW + pillGap), pillY };
                ImGui::SetCursorScreenPos(pillPos);
                ImGui::PushStyleColor(ImGuiCol_Button, active
                    ? ImVec4(0.35f, 0.55f, 0.95f, 0.85f) : ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active
                    ? ImVec4(0.40f, 0.60f, 1.00f, 0.90f) : ImVec4(0.0f, 0.0f, 0.0f, 0.70f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.50f, 0.90f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Text, active
                    ? ImVec4(1, 1, 1, 1) : ImVec4(0.75f, 0.76f, 0.80f, 1.0f));
                ImGui::PushID(pi);
                if (ImGui::Button(pillLabel[pi], { pillW, pillH }))
                    core.SetLanContentMode(static_cast<Core::OutputContentMode>(pi));
                ImGui::PopID();
                ImGui::PopStyleColor(4);
            }
        }
    }
    else
    {
        dl->AddRectFilled(p0, p1, IM_COL32(10, 10, 12, 255));
        const char* msg = "La Transmisión (RTMP) se controla desde su espacio de trabajo";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 msgPos = { p0.x + (drawW - ts.x) * 0.5f, p0.y + drawH * 0.44f - ts.y * 0.5f };
        dl->AddText(msgPos, IM_COL32(160, 160, 170, 255), msg);
        dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);

        const char* btnLabel = "Ir a Transmisión";
        ImVec2 btnSize = { 180.0f, 30.0f };
        ImGui::SetCursorScreenPos({ p0.x + (drawW - btnSize.x) * 0.5f, msgPos.y + ts.y + 14.0f });
        if (ImGui::Button(btnLabel, btnSize))
        {
            auto& workspace = ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace;
            workspace.layoutPreset = ProyecThor::Settings::WorkspaceLayoutPreset::Broadcast;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting)
    {
        Core::VLCBasePlayer* liveBg = core.GetBackgroundPlayer();
        bool liveMuted   = core.GetLiveMute();
        float liveVolume = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        bool livePlaying = liveBg && !liveBg->IsPaused();

        m_AudioMeters.Update(liveBg, true, livePlaying, liveMuted, liveVolume);

        const float meterW = 22.0f;
        const float meterPad = 6.0f;
        float meterH = std::min(drawH - meterPad * 2.0f, 110.0f);
        if (meterH > 20.0f)
        {
            ImVec2 meterPos = { p0.x + meterPad, p0.y + meterPad };
            m_AudioMeters.RenderVertical(dl, meterPos, meterW, meterH);
        }
    }

    if (m_PreviewSource == PreviewSource::Publico && core.IsBackgroundSwapPending())
    {
        const float spinR = 11.0f;
        ImVec2 spinCenter = { p1.x - spinR - 14.0f, p1.y - spinR - 14.0f };
        DrawLoadingSpinner(dl, spinCenter, spinR);

        char etaBuf[32];
        snprintf(etaBuf, sizeof(etaBuf), "~%.1fs", core.GetBackgroundSwapEta());
        ImVec2 etaSz = ImGui::CalcTextSize(etaBuf);
        ImVec2 etaPos = { spinCenter.x - spinR - 6.0f - etaSz.x, spinCenter.y - etaSz.y * 0.5f };
        dl->AddRectFilled({ etaPos.x - 5.0f, etaPos.y - 3.0f }, { etaPos.x + etaSz.x + 5.0f, etaPos.y + etaSz.y + 3.0f },
                          IM_COL32(0, 0, 0, 150), 4.0f);
        dl->AddText(etaPos, IM_COL32(230, 230, 235, 230), etaBuf);
    }

    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting && state.isStreamingNet)
    {
        const char* wifiStr = "online";
        ImVec2 wifiSize = ImGui::CalcTextSize(wifiStr);

        ImVec2 wifiPos = ImVec2(p1.x - wifiSize.x - 12.0f, p0.y + 8.0f);

        dl->AddRectFilled(
            ImVec2(wifiPos.x - 6.0f, wifiPos.y - 4.0f),
            ImVec2(wifiPos.x + wifiSize.x + 6.0f, wifiPos.y + wifiSize.y + 4.0f),
            IM_COL32(0, 0, 0, 160), 4.0f);

        dl->AddText(wifiPos, IM_COL32(0, 255, 100, 255), wifiStr);
    }

    // ── Botón flotante para alternar ventana aparte / acoplada ──
    {
        const char* popBtnLabel = m_IsDetached ? "[-] Acoplar" : "[+] Ventana aparte";
        ImVec2 popSz = ImGui::CalcTextSize(popBtnLabel);
        popSz.x += 16.0f;
        popSz.y += 8.0f;
        float rightMargin = (m_PreviewSource == PreviewSource::Publico && state.isProjecting && state.isStreamingNet) ? 75.0f : 8.0f;
        ImVec2 popPos = ImVec2(p1.x - popSz.x - rightMargin, p0.y + 6.0f);

        ImGui::SetCursorScreenPos(popPos);
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(20, 24, 32, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(40, 52, 75, 240));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 42, 65, 255));
        ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(215, 225, 240, 240));
        ImGui::PushStyleColor(ImGuiCol_Border,        IM_COL32(80, 110, 160, 150));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

        if (ImGui::Button(popBtnLabel)) {
            SetDetached(!m_IsDetached);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (Ctrl+Shift+D)", m_IsDetached ? "Volver a acoplar al panel principal" : "Separar Vista en Vivo en ventana independiente");
        }
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
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
                        dl->AddRect(clickMin, clickMax, IM_COL32(99, 130, 255, 180), 6.0f, 0, 1.5f);

                        const char* editBadge = "✏️ Clic para editar";
                        ImVec2 bSz = ImGui::CalcTextSize(editBadge);
                        ImVec2 bPos(clickMax.x - bSz.x - 10.0f, clickMin.y - bSz.y - 6.0f);
                        if (bPos.y < p0.y + 4.0f) bPos.y = clickMin.y + 4.0f;
                        dl->AddRectFilled(ImVec2(bPos.x - 6.0f, bPos.y - 3.0f), ImVec2(bPos.x + bSz.x + 6.0f, bPos.y + bSz.y + 3.0f),
                                          IM_COL32(20, 24, 36, 230), 4.0f);
                        dl->AddRect(ImVec2(bPos.x - 6.0f, bPos.y - 3.0f), ImVec2(bPos.x + bSz.x + 6.0f, bPos.y + bSz.y + 3.0f),
                                    IM_COL32(99, 130, 255, 200), 4.0f, 0, 1.0f);
                        dl->AddText(bPos, IM_COL32(220, 230, 255, 255), editBadge);

                        if (isSong)
                            ImGui::SetTooltip("Editar letra en tiempo real (se guardará en el .txt al terminar)");
                        else
                            ImGui::SetTooltip("Editar texto en vivo");
                    }

                    if (isClicked)
                    {
                        m_IsEditingLiveLyrics = true;
                        m_LiveLyricsEditBuf = state.currentText;
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
                    float editW = std::clamp(clickSize.x + 100.0f, 340.0f, drawW - 24.0f);
                    float editH = std::clamp(clickSize.y + 85.0f, 140.0f, drawH - 24.0f);
                    float editX = clickMin.x + clickSize.x * 0.5f - editW * 0.5f;
                    float editY = clickMin.y + clickSize.y * 0.5f - editH * 0.5f;
                    editX = std::clamp(editX, p0.x + 12.0f, p1.x - editW - 12.0f);
                    editY = std::clamp(editY, p0.y + 12.0f, p1.y - editH - 12.0f);

                    ImVec2 cardP0(editX, editY);
                    ImVec2 cardP1(editX + editW, editY + editH);

                    dl->AddRectFilled(ImVec2(cardP0.x - 3.0f, cardP0.y - 3.0f), ImVec2(cardP1.x + 3.0f, cardP1.y + 3.0f), IM_COL32(0, 0, 0, 90), 12.0f);
                    dl->AddRectFilled(cardP0, cardP1, IM_COL32(14, 16, 22, 245), 10.0f);
                    dl->AddRect(cardP0, cardP1, IM_COL32(99, 102, 241, 230), 10.0f, 0, 1.5f);

                    float headerH = 28.0f;
                    ImGui::SetCursorScreenPos(ImVec2(cardP0.x + 10.0f, cardP0.y + 6.0f));
                    ImGui::BeginGroup();

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 1.0f, 1.0f));
                    ImGui::TextUnformatted("✏️ EDICIÓN EN VIVO");
                    ImGui::PopStyleColor();

                    ImGui::SameLine(0.0f, 10.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.78f, 0.85f));
                    ImGui::TextUnformatted("Enter: Guardar | Shift+Enter: Línea | Esc: Cancelar");
                    ImGui::PopStyleColor();

                    float btnW = 68.0f;
                    float btnH = 22.0f;
                    float btnsRight = cardP1.x - 10.0f;

                    ImGui::SameLine();
                    ImGui::SetCursorScreenPos(ImVec2(btnsRight - btnW * 2.0f - 6.0f, cardP0.y + 4.0f));

                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.35f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.40f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.55f, 0.30f, 1.0f));
                    bool wantsSave = ImGui::Button("Guardar", ImVec2(btnW, btnH));
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.30f, 0.36f, 0.80f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.40f, 0.48f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.24f, 0.30f, 1.0f));
                    bool wantsCancel = ImGui::Button("Cancelar", ImVec2(btnW, btnH));
                    ImGui::PopStyleColor(3);

                    ImGui::PopStyleVar(2);
                    ImGui::EndGroup();

                    float inputY = cardP0.y + headerH + 6.0f;
                    float inputH = editH - headerH - 14.0f;
                    ImGui::SetCursorScreenPos(ImVec2(cardP0.x + 10.0f, inputY));

                    if (m_FocusLiveEditInput)
                    {
                        ImGui::SetKeyboardFocusHere();
                        m_FocusLiveEditInput = false;
                    }

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.07f, 0.10f, 0.90f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.09f, 0.13f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.09f, 0.10f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.50f, 0.85f, 0.70f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

                    ImGuiInputTextFlags editFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                    ImGuiInputTextFlags_CtrlEnterForNewLine;
                    bool committed = InputTextMultilineStd("##liveLyricsEditInput", &m_LiveLyricsEditBuf,
                                                          ImVec2(editW - 20.0f, inputH), editFlags);

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

        if (ImGui::GetTime() - m_LiveLyricsSavedFeedbackTime < 2.5)
        {
            std::string toast = "✔ Letra guardada en " + (!m_EditingSongFilename.empty() ? m_EditingSongFilename : "archivo");
            ImVec2 tSz = ImGui::CalcTextSize(toast.c_str());
            ImVec2 tPos(p0.x + (drawW - tSz.x) * 0.5f, p0.y + 12.0f);
            dl->AddRectFilled(ImVec2(tPos.x - 12.0f, tPos.y - 6.0f), ImVec2(tPos.x + tSz.x + 12.0f, tPos.y + tSz.y + 6.0f),
                              IM_COL32(18, 50, 28, 235), 6.0f);
            dl->AddRect(ImVec2(tPos.x - 12.0f, tPos.y - 6.0f), ImVec2(tPos.x + tSz.x + 12.0f, tPos.y + tSz.y + 6.0f),
                        IM_COL32(45, 180, 80, 220), 6.0f, 0, 1.0f);
            dl->AddText(tPos, IM_COL32(220, 255, 230, 255), toast.c_str());
        }
    }

    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(drawW, drawH));
}

}
