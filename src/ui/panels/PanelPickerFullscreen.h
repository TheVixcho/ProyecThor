#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <imgui.h>

namespace ProyecThor::UI {

class LibraryPanel;
class UIManager;

enum class PickerCategory {
    All = 0,
    Content,
    Tools
};

struct PickerItem {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string categoryLabel;
    PickerCategory category;
    ImU32 badgeColor;
    void (*drawIcon)(ImDrawList*, ImVec2, float, ImU32);
    std::function<void()> action;
    std::string keywords;
};

class PanelPickerFullscreen {
public:
    PanelPickerFullscreen(LibraryPanel* library, UIManager* uiManager);
    ~PanelPickerFullscreen() = default;

    void Open();
    void Close();
    bool IsOpen() const { return m_IsOpen; }

    void Render();

private:
    void InitItems();
    bool MatchesSearch(const PickerItem& item, const std::string& query) const;

    LibraryPanel* m_Library = nullptr;
    UIManager* m_UIManager = nullptr;

    bool m_IsOpen = false;
    bool m_JustOpened = false;
    char m_SearchBuffer[128]{};
    PickerCategory m_SelectedCategory = PickerCategory::All;

    std::vector<PickerItem> m_Items;
};

} // namespace ProyecThor::UI

