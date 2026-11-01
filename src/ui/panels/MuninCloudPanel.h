#pragma once

#include "core/MuninCloudManager.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <imgui.h>

namespace ProyecThor::UI {

class MuninCloudPanel {
public:
    MuninCloudPanel();
    ~MuninCloudPanel();

    void Render(bool* pOpen);

private:
    ImTextureID GetThumbnailTexture(const Core::MuninResource& res);
    void ClearThumbnailCache();

    char m_SearchFilter[64] = "";
    Core::MuninResourceType m_CurrentTab = Core::MuninResourceType::All;
    int m_SelectedCategoryIdx = 0;
    std::string m_ToastMessage;
    float m_ToastTimer = 0.0f;

    std::unordered_map<std::string, ImTextureID> m_ThumbTextures;
    std::unordered_map<std::string, bool> m_FetchingThumbs;
    std::mutex m_ThumbMutex;
};

} // namespace ProyecThor::UI
