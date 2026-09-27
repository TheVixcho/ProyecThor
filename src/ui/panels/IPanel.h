#pragma once
#include <string>

namespace ProyecThor::UI {

    class IPanel {
    public:
        virtual ~IPanel() = default;
        
        // Método principal que dibujará el contenido del panel
        virtual void Render() = 0;
        
        // Útil para identificar el panel o mostrar su nombre en debug
        virtual std::string GetName() const = 0;
    };

}