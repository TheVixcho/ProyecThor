#pragma once
#include "IPanel.h"
#include <string>

namespace ProyecThor::UI {
    
    class UIManager;

    class ControlPanel : public IPanel {
    public:
        ControlPanel(UIManager* uiManager); 
        ~ControlPanel() override = default;

        void Render() override;
        std::string GetName() const override { return "Control"; }

    private:
        UIManager* m_UIManager = nullptr; 
        
        bool m_isProjecting = false;
        void ToggleSecondaryDisplay(bool active);
    };

} // namespace ProyecThor::UI