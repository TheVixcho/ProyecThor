#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include "core/PresentationCore.h"
#include "core/settings/SettingsManager.h"
#include "ui/views/Announcements.h"
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
#include "ui/framework/UIStrings.h"
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
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    return fs::path(buf) / "ProyecThor" / "assets" / "overlays";
#else
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".local" / "share" / "ProyecThor" / "assets" / "overlays";
#endif
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

ImTextureID LoadOverlayThumbTex(const char* path) {
    int w, h, n;
    unsigned char* d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    return (ImTextureID)(intptr_t)tex;
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
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   std::min(btnSize.x, btnSize.y) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.3f);

    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::Button("", btnSize);
    bool   isHeld  = ImGui::IsItemActive();
    ImVec2 p1      = { p0.x + btnSize.x, p0.y + btnSize.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (lit)
    {
        ImVec2 center = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
        dl->AddCircleFilled(center, btnSize.y * 0.55f,
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)), 24);
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

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID hovId = ImGui::GetID(id);
    float*  pT    = storage->GetFloatRef(hovId ^ 0x9876FEDCu, 0.0f);
    bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
    *pT += ((hovered ? 1.0f : 0.0f) - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float t = *pT;

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
        if (t > 0.01f) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 18.0f)), rounding);
        } else {
            dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(bgColor), rounding);
        }
    }

    ImGui::SetCursorScreenPos(bMin);
    const std::string btnId = std::string("##btn_") + id;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), size);

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

using PadIconDrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);
struct PadIconEntry { const char* name; PadIconDrawFn draw; };

static const PadIconEntry kPadIcons[] = {
    { "Mixer",      AppIcons::DrawIcon_Mixer     },
    { "Monitor",    AppIcons::DrawIcon_Monitor   },
    { "Capas",      AppIcons::DrawIcon_Layers    },
    { "Paleta",     AppIcons::DrawIcon_Palette   },
    { "Overlay",    AppIcons::DrawIcon_Overlay   },
    { "Tipografia", AppIcons::DrawIcon_TextAa    },
    { "Transición", AppIcons::DrawIcon_Swap      },
    { "Shader",     AppIcons::DrawIcon_Shader    },
    { "Home",       HomeIcons::DrawIcon_Home     },
    { "Reloj",      HomeIcons::DrawIcon_Clock    },
    { "Anuncios",   HomeIcons::DrawIcon_Megaphone},
    { "Notas",      HomeIcons::DrawIcon_Notepad  },
    { "Camara",     HomeIcons::DrawIcon_Camera   },
    { "Red",        HomeIcons::DrawIcon_Broadcast},
    { "Chat",       HomeIcons::DrawIcon_Chat     },
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
    const float cellSz  = 34.0f;
    const float spacing = 6.0f;

    for (int i = 0; i < kPadIconCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        const bool sel = (i == iconIndex);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? MT::k_PrevBtn : ImVec4(1,1,1,0.04f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_PrevBtnAct);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        bool clicked = ImGui::Button("##padIcon", ImVec2(cellSz, cellSz));
        ImVec2 p = ImGui::GetItemRectMin();
        ImVec2 s = ImGui::GetItemRectSize();
        float  iconSz = cellSz * 0.55f;
        kPadIcons[i].draw(ImGui::GetWindowDrawList(),
                          { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, ImGui::GetColorU32(ImVec4(1,1,1,0.92f)));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", kPadIcons[i].name);

        if (clicked) { iconIndex = i; changed = true; }
        ImGui::PopID();
    }
    return changed;
}

using PadSettings = ProyecThor::Settings::PadSettings;

void SavePad(PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();

    if (auto* cap = core.GetCapturePanelRef())
        pad.hasCapture = cap->SnapshotCurrentCapture(pad.capture);
    else
        pad.hasCapture = false;

    auto state = core.GetState();
    pad.hasStyle       = true;
    pad.styleSize      = state.textSize;
    for (int c = 0; c < 4; c++) pad.styleColor[c] = state.textColor[c];
    pad.styleHAlign    = state.textAlignment;
    pad.styleVAlign    = state.vAlignment;
    for (int c = 0; c < 4; c++) pad.styleMargins[c] = state.margins[c];
    pad.styleAutoScale = state.autoScale;
    pad.styleFontName  = state.selectedFont;
    pad.bgType = (int)state.bgType;
    pad.bgPath = state.bgPath;
    for (int c = 0; c < 3; c++) pad.bgColor[c] = state.bgColor[c];

    pad.assigned = pad.hasCapture || pad.hasStyle;
    ProyecThor::Settings::SettingsManager::Get().Save();
}

void ApplyPad(const PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();
    using BgType = Core::PresentationState::BackgroundType;

    if (pad.hasCapture) {
        if (auto* cap = core.GetCapturePanelRef())
            cap->ApplyCaptureScene(pad.capture);
    }

    if (pad.hasStyle) {
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

    const auto& str = ProyecThor::UI::GetUIStrings();
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
        visible = ImGui::Begin(Loc("Vista en Vivo - Ventana Independiente", "Live View - Detached Window", "Visualização ao Vivo - Janela Independente"), &open, flags);
        if (!open) {
            SetDetached(false);
        }
    }
    else
    {
        visible = ImGui::Begin(str.panelLiveView);
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
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }

        const bool  showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                            .GetSettings().general.showViewQuickActions;
        const bool  narrowColumn = avail.x < 340.0f;
        const bool  showClearRail = showQuickActions && !narrowColumn;
        const float railW      = showClearRail ? kQuickActionsRailW : 0.0f;
        const float stripH     = showQuickActions ? kConfigStripH : 0.0f;
        const float topAreaH   = avail.y;
        const float contentW   = std::max(0.0f, avail.x - railW);

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

            if (inlineToolH > 8.0f)
            {
                ImGui::SetCursorPosY(topReservedH + videoH + transportH + stripH);
                RenderInlineTool(contentW, inlineToolH);
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
    bool overlayLive = core.HasOverlay();

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
        { "vaClearText", "cleaning_services", nullptr, "\xF0\x9F\x93\x9D", Loc("Limpiar texto", "Clear text", "Limpar texto"),
          hoverClear, activeContent, showText,  showText  ? tintOnYellow : textPrimary },
        { "vaClearDisc", "stop", nullptr, "\xF0\x9F\x92\xBF", Loc("Detener disco en vivo", "Stop live disc", "Parar disco ao vivo"),
          hoverClear, activeContent, discLive,  discLive  ? tintOnYellow : textPrimary },
        { "vaClearBg",   "original_screen", nullptr, "\xF0\x9F\x96\xBC", Loc("Quitar fondo", "Remove background", "Remover plano de fundo"),
          hoverClear, activeContent, bgLive,    bgLive    ? tintOnYellow : textPrimary },
        { "vaClearOverlay", "fit_screen", nullptr, "\xF0\x9F\x93\x91", Loc("Quitar overlay", "Remove overlay", "Remover overlay"),
          hoverClear, activeContent, overlayLive, overlayLive ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "volume_up", nullptr, "\xF0\x9F\x93\xA2", Loc("Detener anuncios", "Stop announcements", "Parar anúncios"),
          hoverClear, activeContent, annLive,   annLive   ? tintOnYellow : textPrimary },
        { "vaClearClock","", nullptr, "\xE2\x8F\xB0", Loc("Quitar reloj", "Remove clock", "Remover relógio"),
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "motion_play", nullptr, "\xF0\x9F\x93\xB9", Loc("Detener captura", "Stop capture", "Parar captura"),
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
            else if (i == 2) core.StopBackgroundMedia();
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

    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeStretch= ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);

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

    const bool previewingAlt = (m_PreviewSource != PreviewSource::Publico);
    const char* previewSourceLabel[4] = {
        Loc("Público", "Live", "Público"),
        "Stage",
        Loc("Transmisión", "Broadcast", "Transmissão"),
        Loc("Inalámbrica", "Wireless", "Sem fio")
    };
    int previewSourceIdx = static_cast<int>(m_PreviewSource);
    int nextPreviewSourceIdx = (previewSourceIdx + 1) % 4;
    char previewSourceTooltip[128];
    snprintf(previewSourceTooltip, sizeof(previewSourceTooltip),
             Loc("Viendo: %s (clic para ver %s)", "Viewing: %s (click for %s)", "Visualizando: %s (clique para ver %s)"),
             previewSourceLabel[previewSourceIdx], previewSourceLabel[nextPreviewSourceIdx]);

    const bool overlaysOn = (m_ActiveTool == InlineTool::Overlays);
    const bool chatOn     = (m_ActiveTool == InlineTool::Chat);
    const bool padsOn     = (m_ActiveTool == InlineTool::Pads);
    const bool clockOn    = (m_ActiveTool == InlineTool::Clock);

    const bool detachedOn = m_IsDetached;
    const char* detachTip = m_IsDetached ?
        Loc("Acoplar panel al entorno (Ctrl+Shift+D)", "Dock panel to workspace (Ctrl+Shift+D)", "Acoplar painel ao espaço (Ctrl+Shift+D)") :
        Loc("Poner en ventana aparte (Ctrl+Shift+D)", "Detach into separate window (Ctrl+Shift+D)", "Destacar em janela separada (Ctrl+Shift+D)");

    ActionDef actions[7] = {
        { "vaStretch",   "fit_screen", nullptr, "\xF0\x9F\x93\x90",
          Loc("Alternar proporción", "Toggle aspect ratio", "Alternar proporção"), hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaClock",     "", nullptr, "\xE2\x8F\xB0", Loc("Reloj", "Clock", "Relógio"),
          hoverClear, activeStretch, clockOn, textPrimary },
        { "vaPreviewSource", "repeat", nullptr, "\xF0\x9F\x94\x84",
          previewSourceTooltip,
          hoverClear, activeStretch, previewingAlt, textPrimary },
        { "vaOverlays",  "fit_screen", nullptr, "\xF0\x9F\x93\x91", "Overlays",
          hoverClear, activeStretch, overlaysOn, textPrimary },
        { "vaChat",      "", nullptr, "\xF0\x9F\x92\xAC", "Chat",
          hoverClear, activeStretch, chatOn, textPrimary },
        { "vaPads",      "", nullptr, "\xF0\x9F\x8E\x9B", "Pads",
          hoverClear, activeStretch, padsOn, textPrimary },
        { "vaDetach",    "", nullptr, m_IsDetached ? "\xE2\x87\xB1" : "\xE2\x87\xB2",
          detachTip,
          hoverClear, activeStretch, detachedOn, textPrimary },
    };

    constexpr int kCount = 7;
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
            if (i == 0)      core.SetStretchToFill(!stretchOn);
            else if (i == 1) m_ActiveTool = clockOn    ? InlineTool::None : InlineTool::Clock;
            else if (i == 2) m_PreviewSource = static_cast<PreviewSource>(nextPreviewSourceIdx);
            else if (i == 3) m_ActiveTool = overlaysOn ? InlineTool::None : InlineTool::Overlays;
            else if (i == 4) m_ActiveTool = chatOn     ? InlineTool::None : InlineTool::Chat;
            else if (i == 5) m_ActiveTool = padsOn     ? InlineTool::None : InlineTool::Pads;
            else if (i == 6) SetDetached(!m_IsDetached);
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
        ImGui::SetCursorPosX(std::max(0.0f, (leftW - kQuickActionsRailW) * 0.5f));
        ImGui::BeginGroup();
        RenderQuickActionsClear(kQuickActionsRailW);
        ImGui::EndGroup();

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
        default: break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
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

        auto it = m_OverlayThumbCache.find(ov.pngPath);
        if (it == m_OverlayThumbCache.end())
            it = m_OverlayThumbCache.emplace(ov.pngPath, LoadOverlayThumbTex(ov.pngPath.c_str())).first;
        ImTextureID thumb = it->second;

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
            Core::PresentationCore::Get().SetOverlayMedia(ov.pngPath);
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

void ViewPanel::RenderPadsContent()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("PADS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 290.0f);
    ImGui::TextWrapped("Click: aplicar. Click derecho: guardar lo que hay en pantalla "
                       "(captura + estilo/fondo + overlay activo, no la letra) o elegir icono.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    auto& padsArr = ProyecThor::Settings::SettingsManager::Get().GetSettings().pads.pads;

    const int   cols    = 4;
    const float btnSize = 56.0f;
    const float spacing = 10.0f;

    for (int i = 0; i < ProyecThor::Settings::kPadCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        auto& pad = padsArr[i];
        const auto& icon = PadIconFor(pad.iconIndex);

        ImVec4 fillCol = pad.assigned ? MT::k_PrevBtn : ImVec4(MT::k_PrevBtn.x, MT::k_PrevBtn.y, MT::k_PrevBtn.z, 0.12f);
        ImVec4 bordCol = pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.35f) : MT::k_BorderSubtle;

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        fillCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   MT::k_PrevBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Border,         bordCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);

        bool clicked = ImGui::Button("##pad", ImVec2(btnSize, btnSize));

        ImVec2 p       = ImGui::GetItemRectMin();
        ImVec2 s       = ImGui::GetItemRectSize();
        float  iconSz  = btnSize * 0.42f;
        ImU32  iconCol = ImGui::GetColorU32(pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.92f) : MT::k_TextDim);
        icon.draw(ImGui::GetWindowDrawList(),
                  { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, iconCol);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (clicked && pad.assigned) ApplyPad(pad);

        if (ImGui::BeginPopupContextItem("##padCtx")) {
            if (ImGui::MenuItem(pad.assigned ? "Guardar aquí (reemplazar)" : "Guardar aquí"))
                SavePad(pad);

            if (ImGui::BeginMenu("Elegir icono")) {
                if (RenderPadIconGrid(pad.iconIndex))
                    ProyecThor::Settings::SettingsManager::Get().Save();
                ImGui::EndMenu();
            }

            if (pad.assigned) {
                ImGui::Separator();
                if (ImGui::MenuItem("Borrar pad")) {
                    pad = PadSettings{};
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
            }
            ImGui::EndPopup();
        }

        if (pad.assigned && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            std::string tip = "Pad " + std::to_string(i + 1);
            if (pad.hasCapture) tip += "\n- Captura";
            if (pad.hasStyle)   tip += "\n- Estilo y fondo";
            ImGui::SetTooltip("%s", tip.c_str());
        }

        ImGui::PopID();
    }

    if (auto* cap = Core::PresentationCore::Get().GetCapturePanelRef())
        cap->RenderSceneButtons();
}

void ViewPanel::RenderLiveTransport(float w, float h)
{
    auto& core = Core::PresentationCore::Get();
    Core::VLCBasePlayer* bg = core.GetBackgroundPlayer();

    m_LiveMuted   = core.GetLiveMute();
    m_LiveVolume  = static_cast<float>(core.GetLiveVolume()) * 0.01f;
    m_LivePlaying = bg && !bg->IsPaused();

    int64_t liveLenMs = bg ? bg->GetLength() : 0;

    if (m_LivePlaying && bg && liveLenMs > 0)
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

    int64_t liveCurMs = bg ? bg->GetTime()   : 0;
    int64_t liveLen   = bg ? bg->GetLength() : 0;
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
        core.SetLivePosition(displayPos);
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
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Retroceder / Anterior (Flecha Izquierda)");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_replay");
    if (DrawPadButton("replay_10", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::max(0.0f, np));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Atrasar 10 segundos");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* mainIcon = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("vp_pad_main");
    if (DrawPadButton(mainIcon, padSize * 0.40f, m_LivePlaying ? kLive : kNeutral, { padSize, padSize }, m_LivePlaying)) {
        if (bg) {
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
        float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::min(1.0f, np));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Adelantar 10 segundos");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_stop");
    if (DrawPadButton("stop", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        core.SetLivePosition(0.0f);
        if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Detener video");
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_nav_next");
    if (DrawPadButton("arrow_forward", padSize * 0.44f, kNeutral, { padSize, padSize }, false)) {
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
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Avanzar / Siguiente (Flecha Derecha)");
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
        core.SetLiveMute(m_LiveMuted);
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
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
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
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
        UI::DrawStageContent(dl, p0, p1);
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

