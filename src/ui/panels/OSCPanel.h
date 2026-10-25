#pragma once
#include "core/OSCReceiver.h"
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ── OSCPanel ──────────────────────────────────────────────────────────────
// Enviar/recibir mensajes OSC (luces/controladores externos). Vivia dentro
// del rail de Conexiones (YggdrasilPanel, retirado del todo); ahora es una
// subcategoria de Ajustes > Proyeccion (ver CategoryProjection.cpp), junto
// a Red/Mobile/Streaming.
//
// Igual criterio que StreamingPanel/TeamChatPanel/BroadcastPanel/SyncPanel:
// no es un IPanel/modo propio, UIManager es dueño de la unica instancia y le
// llama Update() incondicionalmente cada frame (para seguir recibiendo y
// aplicando mensajes OSC aunque Ajustes este cerrado), y expone un puntero a
// quien necesite dibujar su contenido (aca, SettingsPanel).
class OSCPanel {
public:
    OSCPanel();
    ~OSCPanel();

    // Llamar UNA VEZ POR FRAME sin importar si Ajustes > Conexiones esta a
    // la vista: aplica los mensajes OSC recibidos a los parametros en vivo
    // vinculados, igual que StreamingPanel::Update()/TeamChatPanel::Update().
    void Update();

    void RenderContent();

private:
    struct BindableParam {
        std::string                name;  // debe matchear Settings::OSCBinding::paramName
        std::function<float()>     get;
        std::function<void(float)> set;
    };

    void BuildParamRegistry();
    void ApplyReceivedMessages();

    void RenderConnectionSection();
    void RenderControlListSection();
    void RenderSendSection();

    Core::OSCReceiver           m_Receiver;
    std::vector<BindableParam>  m_Params;
    std::vector<Core::OSCReceivedMessage> m_DrainBuffer; // reusado cada frame

    int         m_LearningIndex = -1; // indice en m_Params en modo "Aprender"
    std::string m_ListenStatus;       // texto de estado de la escucha (error o "Escuchando en puerto N")
};

} // namespace ProyecThor::UI
