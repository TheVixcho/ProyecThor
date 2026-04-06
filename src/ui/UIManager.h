#pragma once

#include <vector>
#include <memory>
#include <GLFW/glfw3.h> 
#include "IPanel.h"
#include "../toolbar/ConfigPanel.h"

namespace ProyecThor::UI {

    class UIManager {
    public:
        UIManager();
        ~UIManager();

        bool Initialize(GLFWwindow* window); 
        
        void AddPanel(std::shared_ptr<IPanel> panel);
        void RenderAll();
        void Shutdown();

    private:
        void BeginDockspace();
        void EndDockspace();
        void ApplyProfessionalTheme(); 

        GLFWwindow* m_Window = nullptr;
        std::vector<std::shared_ptr<IPanel>> m_Panels;
        
        bool m_ShowConfig = false;
        ConfigPanel m_ConfigPanel; 
    };

}