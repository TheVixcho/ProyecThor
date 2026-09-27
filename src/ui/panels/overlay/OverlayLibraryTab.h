#pragma once
#include "OverlayTypes.h"
#include "OverlayCanvasEditor.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace ProyecThor::UI { class UIManager; }

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayLibraryTab — galeria de Overlays guardados + editor (ver
//  OverlayCanvasEditor). Vive en el sidebar izquierdo de Biblioteca (ver
//  LibrarySideMode::Overlay en LibraryPanel.h), no en el hub de Diseño.
//
//  Al abrir el editor (nuevo o existente) le pide a UIManager que oculte el
//  resto de los paneles y muestre el editor a pantalla completa (ver
//  UIManager::EnterFullscreenEditor); al Guardar/Cancelar se los devuelve.
// ─────────────────────────────────────────────────────────────────────────────
class OverlayLibraryTab {
public:
    explicit OverlayLibraryTab(UIManager* uiManager);
    ~OverlayLibraryTab() = default;

    void Render();

private:
    struct OverlayEntry {
        std::string name, pngPath;
        bool        hasClock = false;
        OverlayLayer clockLayer;
        int         canvasW = 1920, canvasH = 1080;
    };

    void ReloadList();
    void SeedDefaultOverlaysIfEmpty();
    void LoadFontsList();
    std::string ResolvePngPath(const std::string& name);
    std::vector<std::string> ListBgImages();
    std::string ImportOverlayImage();
    std::vector<OverlayLayer> ImportOverlaySvgAsLayers(int canvasW, int canvasH);
    OverlayLayer ImportOverlaySvgSingle(int canvasW, int canvasH);
    std::string PickSvgFile();

    bool DeleteOverlay(const std::string& name);

    void RenderTopBar();
    void RenderGallery();
    void RenderCard(const OverlayEntry& e, float W, float H, int col, int cols);
    void RenderRow(const OverlayEntry& e, float W, float rowH);
    void OpenEditorFullscreen(bool isNew, const std::string& name, const OverlayDoc& doc);

    ImTextureID GetThumbnail(const std::string& path);

    UIManager* m_UIManager = nullptr;

    std::vector<OverlayEntry> m_Overlays;
    std::vector<std::string>  m_AvailableFonts;
    std::unordered_map<std::string, ImTextureID> m_ThumbnailCache;

    bool  m_GridMode  = true;
    float m_ThumbZoom = 1.0f;

    std::unique_ptr<OverlayCanvasEditor> m_Editor;
};

} // namespace ProyecThor::UI
