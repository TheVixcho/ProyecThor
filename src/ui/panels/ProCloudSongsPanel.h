#pragma once

#include "ui/framework/GlassRenderer.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

class ProCloudSongsPanel {
public:
    ProCloudSongsPanel() = default;
    ~ProCloudSongsPanel() = default;

    void Render(bool* pOpen, GlassRenderer& glass);

private:
    char m_SearchFilter[128] = "";
    int  m_SelectedSongIndex = -1;

    // Estado de subida / creación
    bool m_ShowNewSongModal = false;
    char m_NewTitleBuf[128] = "";
    char m_NewContentBuf[4096] = "";

    bool m_ShowLocalPickerModal = false;
    std::string m_StatusFeedback = "";
    bool m_StatusIsSuccess = true;
};

} // namespace ProyecThor::UI
