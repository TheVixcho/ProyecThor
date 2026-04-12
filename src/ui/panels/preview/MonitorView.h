#pragma once
#include "../../VLCBasePlayer.h"
#include <string>
#include <cstdint>

namespace ProyecThor::UI {

class MonitorView {
public:
    MonitorView()  = default;
    ~MonitorView() = default;

    void Render(Core::VLCBasePlayer* player);

private:
    bool  m_IsPlaying = false;
    float m_Volume    = 1.0f;

    static std::string FormatTime(int64_t ms);
};

} // namespace ProyecThor::UI