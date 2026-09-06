#pragma once
#include "Model3DTypes.h"
#include <GL/glew.h>
#include <imgui.h>

namespace ProyecThor::UI {

class Model3DRenderer {
public:
    Model3DRenderer();
    ~Model3DRenderer();

    // Inicializa shaders y recursos OpenGL
    bool Initialize();
    void Shutdown();

    // Actualiza animaciones (ej. auto-rotación turntable)
    void Update(float dt, Model3DRenderConfig& config);

    // Renderiza la malla 3D en un Framebuffer y retorna el ID de textura OpenGL
    unsigned int RenderToTexture(Model3DMesh& mesh, const Model3DRenderConfig& config, int width, int height);

    // Retorna la textura actual lista para ImGui::Image
    unsigned int GetTextureID() const { return m_ColorTex; }
    int GetWidth()  const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // Procesamiento de interacción con mouse en el viewport
    void ProcessMouseInput(Model3DRenderConfig& config, ImVec2 mouseDelta, bool isLeftDragging, bool isRightDragging, float wheelDelta);

private:
    void EnsureFBO(int width, int height);
    void InitShaders();
    void RenderGrid(const Mat4& view, const Mat4& proj);

    unsigned int m_FBO       = 0;
    unsigned int m_ColorTex  = 0;
    unsigned int m_DepthRBO  = 0;
    int          m_Width     = 0;
    int          m_Height    = 0;

    // Shaders
    unsigned int m_ShaderProgram = 0;
    unsigned int m_GridProgram   = 0;
    unsigned int m_GridVAO       = 0;
    unsigned int m_GridVBO       = 0;
    int          m_GridVertexCount = 0;

    bool         m_Initialized   = false;
};

} // namespace ProyecThor::UI

