#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "core/SystemStats.h"
#include "core/MemoryOptimizer.h"
#include "ui/framework/DesignSystem.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI::Settings {

namespace {

// ── Iconos vectoriales minimalistas de precisión ─────────────────────────────
enum class PerfBtnIcon {
    Standard,
    EcoBattery,
    Chip,
    NoBlur,
    PauseDisk,
    Gauge30,
    Gauge60,
    GaugeMax,
    CleanRam,
    Check
};

void DrawPerfVectorIcon(ImDrawList* dl, ImVec2 center, float r, PerfBtnIcon icon, ImU32 col) {
    const float t = std::max(1.3f, r * 0.16f);
    switch (icon) {
        case PerfBtnIcon::Standard: {
            dl->AddRect({center.x - r * 0.85f, center.y - r * 0.65f},
                        {center.x + r * 0.85f, center.y + r * 0.38f}, col, 2.0f, 0, t);
            dl->AddLine({center.x, center.y + r * 0.38f}, {center.x, center.y + r * 0.72f}, col, t);
            dl->AddLine({center.x - r * 0.45f, center.y + r * 0.72f},
                        {center.x + r * 0.45f, center.y + r * 0.72f}, col, t);
            break;
        }
        case PerfBtnIcon::EcoBattery: {
            // Cuerpo de batería + hoja/rayo de ahorro energético
            ImVec2 b0 = {center.x - r * 0.86f, center.y - r * 0.48f};
            ImVec2 b1 = {center.x + r * 0.70f, center.y + r * 0.48f};
            dl->AddRect(b0, b1, col, 2.2f, 0, t);
            dl->AddRectFilled({b1.x + 1.0f, center.y - r * 0.22f},
                              {b1.x + r * 0.20f, center.y + r * 0.22f}, col, 1.0f);
            ImVec2 bolt[4] = {
                {center.x + r * 0.04f, center.y - r * 0.30f},
                {center.x - r * 0.28f, center.y + r * 0.02f},
                {center.x + r * 0.08f, center.y + r * 0.02f},
                {center.x - r * 0.20f, center.y + r * 0.30f}
            };
            dl->AddPolyline(bolt, 4, col, 0, t * 0.95f);
            break;
        }
        case PerfBtnIcon::Chip: {
            dl->AddRect({center.x - r * 0.60f, center.y - r * 0.60f},
                        {center.x + r * 0.60f, center.y + r * 0.60f}, col, 2.0f, 0, t);
            dl->AddRectFilled({center.x - r * 0.25f, center.y - r * 0.25f},
                              {center.x + r * 0.25f, center.y + r * 0.25f}, col, 1.0f);
            for (int i = -1; i <= 1; i += 2) {
                float off = i * r * 0.30f;
                dl->AddLine({center.x + off, center.y - r * 0.92f}, {center.x + off, center.y - r * 0.60f}, col, t * 0.85f);
                dl->AddLine({center.x + off, center.y + r * 0.60f}, {center.x + off, center.y + r * 0.92f}, col, t * 0.85f);
                dl->AddLine({center.x - r * 0.92f, center.y + off}, {center.x - r * 0.60f, center.y + off}, col, t * 0.85f);
                dl->AddLine({center.x + r * 0.60f, center.y + off}, {center.x + r * 0.92f, center.y + off}, col, t * 0.85f);
            }
            break;
        }
        case PerfBtnIcon::NoBlur: {
            dl->AddCircle(center, r * 0.72f, col, 18, t);
            dl->AddLine({center.x - r * 0.56f, center.y + r * 0.56f},
                        {center.x + r * 0.56f, center.y - r * 0.56f}, col, t);
            break;
        }
        case PerfBtnIcon::PauseDisk: {
            dl->AddRect({center.x - r * 0.78f, center.y - r * 0.65f},
                        {center.x + r * 0.78f, center.y + r * 0.65f}, col, 2.0f, 0, t);
            dl->AddLine({center.x - r * 0.22f, center.y - r * 0.28f},
                        {center.x - r * 0.22f, center.y + r * 0.28f}, col, t * 1.15f);
            dl->AddLine({center.x + r * 0.22f, center.y - r * 0.28f},
                        {center.x + r * 0.22f, center.y + r * 0.28f}, col, t * 1.15f);
            break;
        }
        case PerfBtnIcon::Gauge30:
        case PerfBtnIcon::Gauge60:
        case PerfBtnIcon::GaugeMax: {
            dl->PathArcTo({center.x, center.y + r * 0.22f}, r * 0.78f, 3.14159f, 6.28318f, 16);
            dl->PathStroke(col, 0, t);
            float ang = (icon == PerfBtnIcon::Gauge30) ? -2.35f : (icon == PerfBtnIcon::Gauge60 ? -1.57f : -0.78f);
            ImVec2 base = {center.x, center.y + r * 0.22f};
            ImVec2 tip  = {base.x + std::cos(ang) * r * 0.58f, base.y + std::sin(ang) * r * 0.58f};
            dl->AddLine(base, tip, col, t);
            dl->AddCircleFilled(base, r * 0.14f, col, 8);
            break;
        }
        case PerfBtnIcon::CleanRam: {
            dl->AddCircle(center, r * 0.72f, col, 16, t);
            dl->AddLine({center.x, center.y - r * 0.36f}, {center.x, center.y + r * 0.30f}, col, t);
            dl->AddLine({center.x - r * 0.28f, center.y + r * 0.04f}, {center.x, center.y + r * 0.34f}, col, t);
            dl->AddLine({center.x + r * 0.28f, center.y + r * 0.04f}, {center.x, center.y + r * 0.34f}, col, t);
            break;
        }
        case PerfBtnIcon::Check: {
            ImVec2 pts[3] = {
                {center.x - r * 0.65f, center.y + r * 0.04f},
                {center.x - r * 0.15f, center.y + r * 0.52f},
                {center.x + r * 0.70f, center.y - r * 0.45f}
            };
            dl->AddPolyline(pts, 3, col, 0, t * 1.25f);
            break;
        }
    }
}

bool StudioPerfButton(const char* id,
                      const char* label,
                      PerfBtnIcon icon,
                      ImVec2      size,
                      bool        active,
                      bool        verticalLayout,
                      ImU32       activeAccent = 0)
{
    size.x = std::max(24.0f, size.x);
    size.y = std::max(22.0f, size.y);

    ImGui::PushID(id);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##btn", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held    = ImGui::IsItemActive();
    const ImVec2 p1    = { p0.x + size.x, p0.y + size.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 accent = (activeAccent != 0) ? activeAccent : DS::AccentLight;

    ImU32 bgCol = DS::BtnDefaultFill;
    if (held) {
        bgCol = IM_COL32(54, 56, 60, 255);
    } else if (active) {
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accent);
        ImVec4 bf = ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        const float mix = hovered ? 0.24f : 0.16f;
        bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
            bf.x + (ac.x - bf.x) * mix,
            bf.y + (ac.y - bf.y) * mix,
            bf.z + (ac.z - bf.z) * mix,
            1.0f));
    } else if (hovered) {
        bgCol = DS::BtnHoverFill;
    }

    ImU32 bordCol = active
        ? accent
        : (hovered ? IM_COL32(255, 255, 255, 48) : IM_COL32(255, 255, 255, 22));

    dl->AddRectFilled(p0, p1, bgCol, DS::RadiusMedium);
    dl->AddRect(p0, p1, bordCol, DS::RadiusMedium, 0, active ? 1.4f : 1.0f);

    const ImU32 fgCol = active
        ? DS::TextPrimary
        : (hovered ? DS::TextPrimary : DS::TextSecondary);
    const ImU32 icCol = active ? accent : fgCol;

    const ImVec2 txtSz = ImGui::CalcTextSize(label);
    dl->PushClipRect({ p0.x + 4.0f, p0.y + 2.0f }, { p1.x - 4.0f, p1.y - 2.0f }, true);
    if (verticalLayout) {
        const float iconR = 10.5f;
        const float totalH = iconR * 2.0f + 8.0f + txtSz.y;
        const float startY = p0.y + (size.y - totalH) * 0.5f;
        DrawPerfVectorIcon(dl, {p0.x + size.x * 0.5f, startY + iconR}, iconR, icon, icCol);
        dl->AddText({p0.x + (size.x - txtSz.x) * 0.5f, startY + iconR * 2.0f + 8.0f}, fgCol, label);

        if (active) {
            dl->AddCircleFilled({p1.x - 10.0f, p0.y + 10.0f}, 3.0f, accent, 10);
        }
    } else {
        const float iconR = 7.5f;
        const float gap   = 8.0f;
        const float totalW = iconR * 2.0f + gap + txtSz.x;
        const float startX = p0.x + std::max(6.0f, (size.x - totalW) * 0.5f);
        const float midY   = (p0.y + p1.y) * 0.5f;
        DrawPerfVectorIcon(dl, {startX + iconR, midY}, iconR, icon, icCol);
        dl->AddText({startX + iconR * 2.0f + gap, midY - txtSz.y * 0.5f}, fgCol, label);
    }
    dl->PopClipRect();

    ImGui::PopID();
    return clicked;
}

void DrawMetricPill(ImDrawList* dl, ImVec2 p0, ImVec2 size, const char* tag, const char* val, ImU32 valCol) {
    size.x = std::max(40.0f, size.x);
    size.y = std::max(22.0f, size.y);
    const ImVec2 p1 = {p0.x + size.x, p0.y + size.y};
    dl->AddRectFilled(p0, p1, DS::BtnDefaultFill, DS::RadiusMedium);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 18), DS::RadiusMedium, 0, 1.0f);

    const ImVec2 tagSz = ImGui::CalcTextSize(tag);
    const ImVec2 valSz = ImGui::CalcTextSize(val);
    const float midY   = (p0.y + p1.y) * 0.5f;
    dl->PushClipRect({ p0.x + 4.0f, p0.y + 2.0f }, { p1.x - 4.0f, p1.y - 2.0f }, true);
    dl->AddText({p0.x + 10.0f, midY - tagSz.y * 0.5f}, DS::TextSecondary, tag);
    dl->AddText({p1.x - 10.0f - valSz.x, midY - valSz.y * 0.5f}, valCol, val);
    dl->PopClipRect();
}

} // namespace

void SettingsPanel::RenderCategoryPerformance() {
    auto& mgr  = ProyecThor::Settings::SettingsManager::Get();
    auto& perf = mgr.GetSettings().performance;

    if (!SectionTitle("Ahorro de energía", "Ahorro de energía"))
        return;

    static float s_RamFreedTimer = 0.0f;
    if (s_RamFreedTimer > 0.0f) {
        s_RamFreedTimer = std::max(0.0f, s_RamFreedTimer - ImGui::GetIO().DeltaTime);
    }

    const float availW = std::max(260.0f, ImGui::GetContentRegionAvail().x);
    const float gap    = 8.0f;
    const float col3W  = std::max(80.0f, (availW - gap * 2.0f) / 3.0f);

    const ImU32 kAccentGreen = IM_COL32(72, 208, 148, 255);
    const ImU32 kAccentCyan  = IM_COL32(88, 178, 236, 255);

    // ── Fila 1: Preajustes de 1 clic ─────────────────────────────────────────
    const bool isMinRam    = perf.lowMemoryMode;
    const bool isPowerSave = perf.performanceMode && !perf.lowMemoryMode;
    const bool isStandard  = !perf.performanceMode && !perf.lowMemoryMode;

    if (StudioPerfButton("##presetStd", "Estándar", PerfBtnIcon::Standard, {col3W, 68.0f}, isStandard, true)) {
        perf.performanceMode     = false;
        perf.lowMemoryMode       = false;
        perf.disableGlassBlur    = false;
        perf.pauseBackgroundScan = false;
        perf.targetFPS           = 60;
        mgr.SaveSettings();
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##presetEco", "Ahorro de energía", PerfBtnIcon::EcoBattery, {col3W, 68.0f}, isPowerSave, true, kAccentGreen)) {
        perf.performanceMode     = true;
        perf.lowMemoryMode       = false;
        perf.disableGlassBlur    = true;
        perf.pauseBackgroundScan = true;
        perf.targetFPS           = 60;
        mgr.SaveSettings();
        ProyecThor::Core::MemoryOptimizer::RequestPurge(false, false);
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##presetMinRam", "RAM Mínima", PerfBtnIcon::Chip, {col3W, 68.0f}, isMinRam, true, kAccentCyan)) {
        perf.performanceMode     = true;
        perf.lowMemoryMode       = true;
        perf.disableGlassBlur    = true;
        perf.pauseBackgroundScan = true;
        perf.targetFPS           = 30;
        mgr.SaveSettings();
        ProyecThor::Core::MemoryOptimizer::RequestPurge(true, true);
        s_RamFreedTimer = 2.5f;
    }

    ImGui::Dummy({0.0f, 10.0f});

    // ── Fila 2: Interruptores individuales ───────────────────────────────────
    if (StudioPerfButton("##togBlur", "Sin Blur", PerfBtnIcon::NoBlur, {col3W, 38.0f}, perf.disableGlassBlur, false)) {
        perf.disableGlassBlur = !perf.disableGlassBlur;
        mgr.SaveSettings();
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##togLowMem", "Baja RAM", PerfBtnIcon::Chip, {col3W, 38.0f}, perf.lowMemoryMode, false, kAccentCyan)) {
        perf.lowMemoryMode = !perf.lowMemoryMode;
        if (perf.lowMemoryMode) {
            perf.performanceMode = true;
        }
        mgr.SaveSettings();
        if (perf.lowMemoryMode) {
            ProyecThor::Core::MemoryOptimizer::RequestPurge(true, true);
            s_RamFreedTimer = 2.5f;
        }
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##togNoScan", "Sin Escaneo", PerfBtnIcon::PauseDisk, {col3W, 38.0f}, perf.pauseBackgroundScan, false)) {
        perf.pauseBackgroundScan = !perf.pauseBackgroundScan;
        mgr.SaveSettings();
    }

    ImGui::Dummy({0.0f, 10.0f});

    // ── Fila 3: Límite de FPS ────────────────────────────────────────────────
    if (StudioPerfButton("##fps30", "30 FPS", PerfBtnIcon::Gauge30, {col3W, 36.0f}, perf.targetFPS == 30, false, kAccentGreen)) {
        perf.targetFPS = 30;
        mgr.SaveSettings();
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##fps60", "60 FPS", PerfBtnIcon::Gauge60, {col3W, 36.0f}, perf.targetFPS == 60, false)) {
        perf.targetFPS = 60;
        mgr.SaveSettings();
    }
    ImGui::SameLine(0.0f, gap);

    if (StudioPerfButton("##fpsMax", "Máx FPS", PerfBtnIcon::GaugeMax, {col3W, 36.0f}, perf.targetFPS == 0, false)) {
        perf.targetFPS = 0;
        mgr.SaveSettings();
    }

    ImGui::Dummy({0.0f, 14.0f});
    DS::GlassSeparator(1.0f);
    ImGui::Dummy({0.0f, 12.0f});

    // ── Fila 4: Indicadores compactos + Liberar RAM ──────────────────────────
    auto& sys = ProyecThor::Core::SystemStats::Get();
    const float fps    = ImGui::GetIO().Framerate;
    const float cpu    = sys.CpuPercent();
    const float appRam = ProyecThor::Core::MemoryOptimizer::GetProcessRamMB();

    char fpsBuf[24], cpuBuf[24], ramBuf[32];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f", fps);
    std::snprintf(cpuBuf, sizeof(cpuBuf), "%.0f%%", cpu);
    std::snprintf(ramBuf, sizeof(ramBuf), "%.0f MB", appRam);

    const float cleanBtnW = std::max(120.0f, std::min(168.0f, availW * 0.34f));
    const float pillsArea = std::max(130.0f, availW - cleanBtnW - gap);
    const float pillW     = std::max(40.0f, (pillsArea - gap * 2.0f) / 3.0f);
    const float rowH      = 38.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cur = ImGui::GetCursorScreenPos();

    const ImU32 fpsCol = (fps < 24.0f) ? DS::DangerColor : kAccentGreen;
    const ImU32 cpuCol = (cpu > 80.0f) ? DS::DangerColor : DS::TextPrimary;
    const ImU32 ramCol = (appRam > 650.0f) ? IM_COL32(242, 188, 72, 255) : kAccentCyan;

    DrawMetricPill(dl, cur, {pillW, rowH}, "FPS", fpsBuf, fpsCol);
    DrawMetricPill(dl, {cur.x + pillW + gap, cur.y}, {pillW, rowH}, "CPU", cpuBuf, cpuCol);
    DrawMetricPill(dl, {cur.x + (pillW + gap) * 2.0f, cur.y}, {pillW, rowH}, "RAM", ramBuf, ramCol);

    ImGui::Dummy({pillsArea, rowH});
    ImGui::SameLine(0.0f, gap);

    const bool justFreed = (s_RamFreedTimer > 0.0f);
    if (StudioPerfButton("##freeRamNow",
                         justFreed ? "RAM Liberada" : "Liberar RAM",
                         justFreed ? PerfBtnIcon::Check : PerfBtnIcon::CleanRam,
                         {cleanBtnW, rowH},
                         justFreed,
                         false,
                         kAccentGreen))
    {
        ProyecThor::Core::MemoryOptimizer::RequestPurge(true, true);
        s_RamFreedTimer = 2.5f;
    }
}

} // namespace ProyecThor::UI::Settings
