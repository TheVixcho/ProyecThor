// ─────────────────────────────────────────────────────────────────────────────
//  OverlayCanvasEditorTools.cpp — herramientas de la toolbar inferior del
//  editor de Overlays: Mover (default, seleccion multiple incluida, ver
//  OverlayCanvasEditor.cpp), Borrador y Degradado. Separado de
//  OverlayCanvasEditor.cpp por tamaño, pero son metodos de la misma clase.
//
//  Borrador/Degradado editan PIXELES de una capa Image en vivo: la primera
//  vez que se tocan, el archivo se "bifurca" (copia a un PNG propio, ver
//  EnsurePixelEditBuffer) para nunca pisar un original que puede estar
//  compartido (Fondos, otro overlay). La textura editada se sube al mismo
//  cache que usa el render normal (m_ImageTexCache, clave = layer.imagePath
//  ya actualizado a la copia), asi el dibujo de la capa no necesita ningun
//  caso especial para mostrar los cambios en vivo.
// ─────────────────────────────────────────────────────────────────────────────
#include "OverlayCanvasEditor.h"
#include "styles/CanvaStyleEditor.h"
#include "core/PresentationCore.h"
#include <imgui.h>
#include <GL/gl.h>
#include "stb_image.h"
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <filesystem>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

bool OverlayCanvasEditor::IsMultiSelected(int idx) const {
    return std::find(m_MultiSelected.begin(), m_MultiSelected.end(), idx) != m_MultiSelected.end();
}

void OverlayCanvasEditor::ToggleMultiSelected(int idx) {
    auto it = std::find(m_MultiSelected.begin(), m_MultiSelected.end(), idx);
    if (it != m_MultiSelected.end()) m_MultiSelected.erase(it);
    else m_MultiSelected.push_back(idx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  EnsurePixelEditBuffer — carga (y bifurca si hace falta) el buffer RGBA
//  mutable de layerIdx. No hace nada si ya es el buffer activo.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::EnsurePixelEditBuffer(int layerIdx) {
    if (layerIdx < 0 || layerIdx >= (int)m_Doc.layers.size()) return;
    if (m_PixelEditLayer == layerIdx && !m_PixelEditPixels.empty()) return;

    // Si habia una edicion sin guardar de OTRA capa, se escribe antes de
    // soltar ese buffer -- no se pierde silenciosamente.
    if (m_PixelEditLayer >= 0 && m_PixelEditDirty) SavePixelEditToDisk();

    m_PixelEditLayer = -1;
    m_PixelEditPixels.clear();
    m_PixelEditOriginal.clear();
    m_PixelEditDirty = false;

    auto& layer = m_Doc.layers[layerIdx];
    if (layer.kind != OverlayLayerKind::Image || layer.imagePath.empty()) return;

    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(layer.imagePath.c_str(), &w, &h, &n, 4);
    if (!data) return;

    m_PixelEditW = w;
    m_PixelEditH = h;
    m_PixelEditPixels.assign(data, data + (size_t)w * h * 4);
    m_PixelEditOriginal = m_PixelEditPixels;
    stbi_image_free(data);
    m_PixelEditLayer = layerIdx;

    fs::path origPath(layer.imagePath);
    std::string forkName = origPath.stem().string() + "_edit" + std::to_string(layerIdx) +
                            origPath.extension().string();
    fs::path forkPath = origPath.parent_path() / forkName;

    // Si layer.imagePath YA es la bifurcacion (ediciones previas en esta
    // misma sesion), no hay que volver a copiar nada.
    if (origPath != forkPath) {
        std::error_code ec;
        if (stbi_write_png(forkPath.string().c_str(), w, h, 4, m_PixelEditPixels.data(), w * 4)) {
            m_ImageTexCache.erase(layer.imagePath);
            layer.imagePath = forkPath.string();
        }
    }

    UploadPixelEditTexture();
}

// ─────────────────────────────────────────────────────────────────────────────
//  UploadPixelEditTexture — sube m_PixelEditPixels a la MISMA textura GL que
//  usa el render normal de la capa (m_ImageTexCache[layer.imagePath]), asi
//  los cambios se ven en vivo sin tocar el codigo de dibujo de capas.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::UploadPixelEditTexture() {
    if (m_PixelEditLayer < 0 || m_PixelEditLayer >= (int)m_Doc.layers.size()) return;
    auto& layer = m_Doc.layers[m_PixelEditLayer];
    if (layer.imagePath.empty() || m_PixelEditPixels.empty()) return;

    auto it = m_ImageTexCache.find(layer.imagePath);
    GLuint tex;
    bool reuse = (it != m_ImageTexCache.end() && it->second != 0);
    if (reuse) {
        tex = (GLuint)(intptr_t)it->second;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_PixelEditW, m_PixelEditH,
                        GL_RGBA, GL_UNSIGNED_BYTE, m_PixelEditPixels.data());
    } else {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_PixelEditW, m_PixelEditH, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, m_PixelEditPixels.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    m_ImageTexCache[layer.imagePath] = (ImTextureID)(intptr_t)tex;
}

void OverlayCanvasEditor::SavePixelEditToDisk() {
    if (m_PixelEditLayer < 0 || m_PixelEditLayer >= (int)m_Doc.layers.size()) return;
    auto& layer = m_Doc.layers[m_PixelEditLayer];
    if (layer.imagePath.empty() || m_PixelEditPixels.empty()) return;
    if (stbi_write_png(layer.imagePath.c_str(), m_PixelEditW, m_PixelEditH, 4,
                       m_PixelEditPixels.data(), m_PixelEditW * 4)) {
        m_PixelEditDirty = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  ApplyEraserStroke — borra (alpha hacia 0, con caida suave) un circulo
//  alrededor de mousePos, en el espacio LOCAL de la capa en edicion. No
//  contempla rotacion (limitacion conocida: el pincel asume la capa sin
//  rotar) para mantener la conversion de coordenadas simple.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::ApplyEraserStroke(ImVec2 mousePos, ImVec2 canvasScreenSize) {
    if (m_PixelEditLayer < 0 || m_PixelEditLayer >= (int)m_Doc.layers.size()) return;
    auto& layer = m_Doc.layers[m_PixelEditLayer];

    float layerScreenW = layer.sizeW * canvasScreenSize.x;
    float layerScreenH = layer.sizeH * canvasScreenSize.y;
    if (layerScreenW <= 1.0f || layerScreenH <= 1.0f) return;

    float centerX = m_CanvasScreenPos.x + layer.posX * canvasScreenSize.x;
    float centerY = m_CanvasScreenPos.y + layer.posY * canvasScreenSize.y;
    float leftX = centerX - layerScreenW * 0.5f;
    float topY  = centerY - layerScreenH * 0.5f;

    float px = (mousePos.x - leftX) / layerScreenW * m_PixelEditW;
    float py = (mousePos.y - topY)  / layerScreenH * m_PixelEditH;

    float scalePx       = canvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);
    float brushScreenR  = m_BrushRadiusPx * scalePx;
    float brushBufR      = std::max(1.0f, brushScreenR / layerScreenW * m_PixelEditW);

    int minX = std::max(0, (int)(px - brushBufR));
    int maxX = std::min(m_PixelEditW - 1, (int)(px + brushBufR));
    int minY = std::max(0, (int)(py - brushBufR));
    int maxY = std::min(m_PixelEditH - 1, (int)(py + brushBufR));

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float dx = (x + 0.5f) - px, dy = (y + 0.5f) - py;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > brushBufR) continue;
            float falloff = 1.0f - (dist / brushBufR);
            unsigned char& a = m_PixelEditPixels[((size_t)y * m_PixelEditW + x) * 4 + 3];
            a = (unsigned char)std::clamp((int)std::lround(a * (1.0f - falloff)), 0, 255);
        }
    }

    m_PixelEditDirty = true;
    UploadPixelEditTexture();
}

// ─────────────────────────────────────────────────────────────────────────────
//  ApplyGradientPreview — recalcula SIEMPRE desde m_PixelEditOriginal (no
//  acumula), asi mover el slider de angulo/fuerza da un resultado
//  predecible en vez de ir degradando la imagen de a poco en cada cambio.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::ApplyGradientPreview() {
    if (m_PixelEditLayer < 0 || m_PixelEditOriginal.empty()) return;
    m_PixelEditPixels = m_PixelEditOriginal;

    float rad = m_GradientAngle * (float)M_PI / 180.0f;
    float dirX = cosf(rad), dirY = sinf(rad);

    float corners[4][2] = {
        { 0.0f, 0.0f }, { (float)m_PixelEditW, 0.0f },
        { 0.0f, (float)m_PixelEditH }, { (float)m_PixelEditW, (float)m_PixelEditH }
    };
    float minProj = FLT_MAX, maxProj = -FLT_MAX;
    for (auto& c : corners) {
        float p = c[0] * dirX + c[1] * dirY;
        minProj = std::min(minProj, p);
        maxProj = std::max(maxProj, p);
    }
    float span = std::max(1.0f, maxProj - minProj);
    float strength = std::clamp(m_GradientStrength, 0.0f, 1.0f);

    for (int y = 0; y < m_PixelEditH; y++) {
        for (int x = 0; x < m_PixelEditW; x++) {
            float proj = x * dirX + y * dirY;
            float t = std::clamp((proj - minProj) / span, 0.0f, 1.0f);
            float amount = t * strength;

            unsigned char* px = &m_PixelEditPixels[((size_t)y * m_PixelEditW + x) * 4];
            if (m_GradientToColor) {
                for (int c = 0; c < 3; c++) {
                    float target = m_GradientColor[c] * 255.0f;
                    px[c] = (unsigned char)std::clamp((int)std::lround(px[c] * (1.0f - amount) + target * amount), 0, 255);
                }
            } else {
                px[3] = (unsigned char)std::clamp((int)std::lround(px[3] * (1.0f - amount)), 0, 255);
            }
        }
    }

    m_PixelEditDirty = true;
    UploadPixelEditTexture();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderBottomToolbar — banda de herramientas arriba del footer (ver
//  Render()). Se dibuja sobre un fondo propio (para separarla claramente del
//  canvas/paneles arriba y del footer abajo). Mover siempre permite arrastrar
//  capas y seleccionar varias (Ctrl+click / recuadro, ver el loop de capas en
//  RenderCanvas); Borrador/Degradado muestran controles propios aca al lado
//  de los botones.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderBottomToolbar(float w) {
    ImVec2 barP0 = ImGui::GetCursorScreenPos();
    ImVec2 barP1 = ImVec2(barP0.x + w, barP0.y + 44.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(barP0, barP1, CanvaPalette::ToU32(CanvaPalette::Surface1), 8.0f);
    ImGui::GetWindowDrawList()->AddRect(barP0, barP1, CanvaPalette::ToU32(CanvaPalette::Border), 8.0f);
    ImGui::SetCursorScreenPos(ImVec2(barP0.x + 10.0f, barP0.y + 7.0f));

    struct ToolOpt { const char* label; OverlayTool tool; };
    static const ToolOpt opts[3] = {
        { "Mover",       OverlayTool::Move },
        { "Borrador",    OverlayTool::Eraser },
        { "Degradado",   OverlayTool::Gradient },
    };

    constexpr float kBtnW = 96.0f, kBtnH = 30.0f;

    for (int t = 0; t < 3; t++) {
        bool active = (m_ActiveTool == opts[t].tool);
        ImGui::PushStyleColor(ImGuiCol_Button, active
            ? ImVec4(CanvaPalette::Accent.x, CanvaPalette::Accent.y, CanvaPalette::Accent.z, 0.85f)
            : CanvaPalette::Surface2);
        if (ImGui::Button(opts[t].label, ImVec2(kBtnW, kBtnH))) {
            m_ActiveTool = opts[t].tool;
            if (opts[t].tool != OverlayTool::Move) m_MultiSelected.clear();
            bool selIsImage = m_SelectedLayer >= 0 && m_SelectedLayer < (int)m_Doc.layers.size() &&
                              m_Doc.layers[m_SelectedLayer].kind == OverlayLayerKind::Image;
            if ((opts[t].tool == OverlayTool::Eraser || opts[t].tool == OverlayTool::Gradient) && selIsImage)
                EnsurePixelEditBuffer(m_SelectedLayer);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 8.0f);
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::BeginGroup();

    bool selIsImage = m_SelectedLayer >= 0 && m_SelectedLayer < (int)m_Doc.layers.size() &&
                      m_Doc.layers[m_SelectedLayer].kind == OverlayLayerKind::Image;

    if (m_ActiveTool == OverlayTool::Eraser) {
        if (!selIsImage) {
            ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
            ImGui::TextUnformatted("Selecciona una capa de imagen para borrar sobre ella.");
            ImGui::PopStyleColor();
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Tamaño del pincel");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat("##ovBrushSize", &m_BrushRadiusPx, 1.0f, 4.0f, 300.0f, "%.0f px");
        }
    } else if (m_ActiveTool == OverlayTool::Gradient) {
        if (!selIsImage) {
            ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
            ImGui::TextUnformatted("Selecciona una capa de imagen para aplicar el degradado.");
            ImGui::PopStyleColor();
        } else {
            bool changed = false;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Angulo");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            changed |= ImGui::DragFloat("##ovGradAngle", &m_GradientAngle, 1.0f, -180.0f, 180.0f, "%.0f°");

            ImGui::SameLine(0.0f, 16.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Fuerza");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            changed |= ImGui::DragFloat("##ovGradStrength", &m_GradientStrength, 0.01f, 0.0f, 1.0f, "%.2f");

            ImGui::SameLine(0.0f, 16.0f);
            changed |= ImGui::Checkbox("A color", &m_GradientToColor);
            if (m_GradientToColor) {
                ImGui::SameLine();
                changed |= ImGui::ColorEdit3("##ovGradColor", m_GradientColor);
            }

            if (changed) {
                EnsurePixelEditBuffer(m_SelectedLayer);
                ApplyGradientPreview();
            }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextUnformatted(
            "Arrastra una capa para moverla. Ctrl+click o un recuadro sobre el "
            "canvas selecciona varias para moverlas juntas.");
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();
}

} // namespace ProyecThor::UI
