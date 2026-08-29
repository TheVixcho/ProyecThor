#pragma once
#include "Model3DTypes.h"
#include "Model3DLoader.h"
#include "Model3DRenderer.h"
#include <string>
#include <vector>
#include <memory>

namespace ProyecThor::UI {

class UIManager;

class Model3DPanel {
public:
    Model3DPanel();
    ~Model3DPanel();

    void SetUIManager(UIManager* uiManager) { m_UIManager = uiManager; }

    // Renderiza el panel principal de Recursos y Visor 3D
    void Render();

    // Estado y textura para la proyección en vivo
    bool IsProjectingLive() const { return m_IsProjectingLive; }
    void SetProjectingLive(bool live);
    unsigned int GetLiveTextureID();

    // Carga un modelo por ruta o nombre integrado
    void LoadAsset(const Model3DAsset& asset);

private:
    void RenderTopBar();
    void RenderModelGallery(float w, float h);
    void Render3DViewport(float w, float h);
    void RenderViewportControls(ImVec2 vpMin, ImVec2 vpMax);
    void RefreshFolder();

    UIManager*                  m_UIManager = nullptr;
    std::unique_ptr<Model3DRenderer> m_Renderer;

    std::string                 m_Folder = "assets/models";
    char                        m_SearchBuf[128]{};
    std::vector<Model3DAsset>   m_Assets;
    int                         m_SelectedAssetIndex = 0;

    Model3DMesh                 m_CurrentMesh;
    Model3DAsset                m_CurrentAsset;
    Model3DRenderConfig         m_Config;

    bool                        m_IsProjectingLive = false;
    bool                        m_SplitView        = true;
    float                       m_GalleryWidthFrac = 0.32f;

    // Recursos FBO dedicados para la proyección en vivo a resolución completa
    std::unique_ptr<Model3DRenderer> m_LiveRenderer;
};

} // namespace ProyecThor::UI

