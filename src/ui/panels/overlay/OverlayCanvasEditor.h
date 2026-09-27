#pragma once
#include "OverlayTypes.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

struct ImGuiWindow; // ver imgui_internal.h

namespace ProyecThor::UI {

// Herramienta activa en la toolbar inferior (ver RenderBottomToolbar).
//   Mover      -> click+arrastre normal (siempre disponible, no hace falta
//                 cambiar de herramienta para mover una capa). Ademas:
//                 Ctrl+click agrega/saca una capa de la seleccion multiple,
//                 y arrastrar sobre area vacia dibuja un recuadro tipo
//                 Windows Explorer que selecciona todo lo que toque: en
//                 cualquiera de los dos casos, arrastrar cualquier capa de
//                 la seleccion multiple mueve a TODAS juntas.
//   Borrador   -> pincel circular que borra pixeles (alpha=0) de la capa
//                 Image seleccionada, sobre una copia privada del archivo.
//   Degradado  -> desvanece la capa Image seleccionada en un angulo/fuerza
//                 elegidos, a transparencia o a un color solido.
enum class OverlayTool { Move, Eraser, Gradient };

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayCanvasEditor — editor tipo Canva: canvas central con capas de
//  texto, forma o imagen arrastrables (y redimensionables/rotables, en el
//  caso de imagenes/formas) + toolbar flotante arriba (Texto/Forma/Imagen/
//  Eliminar) + panel lateral de capas y propiedades. Se dibuja SIEMPRE a
//  pantalla completa (ver Render()) — el llamador es responsable de pedirle
//  a UIManager que oculte el resto de los paneles mientras este abierto
//  (UIManager::EnterFullscreenEditor/ExitFullscreenEditor).
//
//  Al guardar, rasteriza el canvas a PNG con transparencia (ver
//  OverlayExportService) y solo entonces invoca el callback de guardado con
//  el nombre + la receta editable (OverlayDoc), para que el llamador la
//  persista en disco.
// ─────────────────────────────────────────────────────────────────────────────
class OverlayCanvasEditor {
public:
    using OnSaveCallback    = std::function<void(const std::string& name, const OverlayDoc& doc)>;
    using OnCancelCallback  = std::function<void()>;
    using ResolvePngPathFn  = std::function<std::string(const std::string& name)>;
    using ListBgImagesFn    = std::function<std::vector<std::string>()>;
    using ImportImageFn     = std::function<std::string()>; // abre dialogo, copia el archivo, devuelve ruta (o "" si cancela)

    // Abre dialogo para elegir un .svg, lo separa en capas (una por grupo
    // <g id="..."> de primer nivel) y devuelve un OverlayLayer Image ya
    // rasterizado por cada una, listas para agregar a m_Doc.layers -- ver
    // OverlaySvgImport.h. Vacio si el usuario cancela o el archivo no pudo
    // leerse. canvasW/canvasH: tamano del overlay activo, para ubicar el
    // SVG proporcionalmente dentro de el. Ideal para archivos de
    // Illustrator/Figma/Inkscape que agrupan de forma sensata; Canva (y
    // similares) exportan cada forma/glifo suelto como su propio grupo, asi
    // que este modo termina fragmentando de mas en esos casos -- ver
    // ImportSvgSingleFn para esos.
    using ImportSvgFn = std::function<std::vector<OverlayLayer>(int canvasW, int canvasH)>;

    // Misma idea, pero rasteriza el SVG entero como UNA sola capa Image
    // (garantiza verse igual al archivo original). imagePath vacio en el
    // layer devuelto = cancelado/fallo.
    using ImportSvgSingleFn = std::function<OverlayLayer(int canvasW, int canvasH)>;

    OverlayCanvasEditor(std::vector<std::string>* fontList, ResolvePngPathFn resolvePngPath,
                        ListBgImagesFn listBgImages, ImportImageFn importImage,
                        ImportSvgFn importSvg, ImportSvgSingleFn importSvgSingle);
    ~OverlayCanvasEditor() = default;

    void OpenNew(const OverlayDoc& defaults = {});
    void OpenEdit(const std::string& existingName, const OverlayDoc& existingDoc);

    // onCancel se dispara ademas al Guardar (el editor se cierra en ambos
    // casos) -- el llamador lo usa para salir del modo pantalla completa
    // (ver UIManager::ExitFullscreenEditor) sin duplicar esa llamada en cada
    // callback de guardado.
    void Render(OnSaveCallback onSave, OnCancelCallback onClose);

    bool IsOpen() const { return m_IsOpen; }

private:
    void RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize);
    void RenderFloatingToolbar(ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderCanvas(float w, float h);
    void RenderLayersPanel(float w, float h);
    void RenderPropertiesPanel(float w, float h);
    void RenderFooter(ImVec2 winPos, ImVec2 winSize, OnSaveCallback& onSave, OnCancelCallback& onClose);

    void RenderResizeHandle(int layerIdx, OverlayLayer& layer, int corner,
                            ImVec2 handlePos, ImDrawList* fgDl);
    void RenderRotateHandle(int layerIdx, OverlayLayer& layer, ImVec2 center, float halfH,
                            ImDrawList* fgDl);
    void AddImageLayerFromMenu();
    ImTextureID GetImageTexture(const std::string& path);

    // Herramientas de la toolbar inferior (ver OverlayTool) -- viven en su
    // propio archivo (OverlayCanvasEditorTools.cpp) por tamaño, pero son
    // metodos de esta misma clase (acceden a m_Doc/m_ImageTexCache/etc).
    void RenderBottomToolbar(float w);
    bool IsMultiSelected(int idx) const;
    void ToggleMultiSelected(int idx);
    void EnsurePixelEditBuffer(int layerIdx);
    void UploadPixelEditTexture();
    void SavePixelEditToDisk();
    void ApplyEraserStroke(ImVec2 canvasPos, ImVec2 canvasScreenSize);
    void ApplyGradientPreview();

    // Dibuja SOLO el contenido real de las capas (fondo del doc + Text/
    // Image/Shape; Clock se saltea siempre, nunca se hornea) en un
    // ImDrawList aparte, propio para exportar -- ver RenderFooter. Nunca
    // incluye el cuadriculado "sin fondo" ni chrome de edicion (eso solo
    // existe en el canvas en vivo, ver RenderCanvas).
    void DrawLayersForExport(ImDrawList* dl, ImVec2 p0, ImVec2 canvasScreenSize);

    // Controles de font/tamano/color/sombra/contorno/fondo -- compartidos
    // entre capas Text y Clock (la capa Clock es un cuadro con el mismo
    // estilo de texto, solo que sin contenido editable, ver RenderSidebar).
    void RenderTextStyleProperties(OverlayLayer& layer, float w);

    std::vector<std::string>* m_FontList;
    ResolvePngPathFn           m_ResolvePngPath;
    ListBgImagesFn             m_ListBgImages;
    ImportImageFn              m_ImportImage;
    ImportSvgFn                m_ImportSvg;
    ImportSvgSingleFn          m_ImportSvgSingle;

    OverlayDoc m_Doc;
    char       m_Name[128] = {};
    bool       m_IsOpen            = false;
    bool       m_IsEditingExisting = false;
    int        m_SelectedLayer     = -1;
    bool       m_SaveFailed        = false;

    // Estado de arrastre de una capa (mover)
    int    m_DraggingLayer = -1;
    ImVec2 m_DragStartMouse{};
    float  m_DragStartPosX = 0.0f;
    float  m_DragStartPosY = 0.0f;

    // Estado de arrastre de una esquina (redimensionar capas de imagen/forma)
    int    m_ResizingLayer   = -1;
    int    m_ResizeCorner    = -1; // 0=TL, 1=TR, 2=BL, 3=BR
    ImVec2 m_ResizeStartMouse{};
    float  m_ResizeStartPosX  = 0.0f;
    float  m_ResizeStartPosY  = 0.0f;
    float  m_ResizeStartSizeW = 0.0f;
    float  m_ResizeStartSizeH = 0.0f;

    // Estado de arrastre del handle de rotacion (capas de imagen/forma)
    int   m_RotatingLayer      = -1;
    float m_RotateStartAngle   = 0.0f; // radianes, angulo mouse-centro al empezar
    float m_RotateStartRotation = 0.0f; // layer.rotation al empezar (grados)

    // Cache de texturas GL de imagenes usadas como capa (clave = ruta)
    std::unordered_map<std::string, ImTextureID> m_ImageTexCache;

    // ── Herramienta activa + seleccion multiple ──────────────────────────
    OverlayTool      m_ActiveTool = OverlayTool::Move;
    std::vector<int> m_MultiSelected;      // indices en m_Doc.layers, seleccionados a la vez
    ImVec2           m_MultiDragStartMouse{};
    std::vector<ImVec2> m_MultiDragStartPos; // posX/posY de cada capa en m_MultiSelected, al iniciar el arrastre

    // Recuadro de seleccion tipo Windows Explorer: arrastrar sobre area
    // vacia del canvas selecciona todo lo que el recuadro toque.
    bool   m_RubberBandActive = false;
    ImVec2 m_RubberBandStart{};

    // ── Edicion de pixeles (Borrador/Degradado) ──────────────────────────
    // Buffer RGBA mutable de la capa Image actualmente en edicion -- se
    // "bifurca" (copia a un archivo propio) la PRIMERA vez que se edita esa
    // capa, para nunca pisar un PNG que pueda estar compartido (Fondos,
    // otro overlay, etc). La textura editada se sube al MISMO
    // m_ImageTexCache que ya usa el render normal (clave = layer.imagePath
    // ya actualizado a la copia), asi el dibujo de la capa no necesita
    // ningun caso especial.
    int                        m_PixelEditLayer = -1;
    int                        m_PixelEditW = 0, m_PixelEditH = 0;
    std::vector<unsigned char> m_PixelEditPixels;      // RGBA, se modifica en vivo (Borrador) o se recalcula desde m_PixelEditOriginal (Degradado)
    std::vector<unsigned char> m_PixelEditOriginal;    // copia intacta, solo para poder recalcular el Degradado sin acumular
    bool                       m_PixelEditDirty = false; // true = hay cambios sin escribir a disco

    float m_BrushRadiusPx   = 40.0f;  // px referidos a un canvas 1920x1080
    float m_GradientAngle   = 0.0f;   // grados
    float m_GradientStrength = 1.0f;  // 0..1
    bool  m_GradientToColor  = false; // false = desvanece a transparente, true = a m_GradientColor
    float m_GradientColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    // Capturado cada frame al dibujar el canvas — usado al presionar Guardar
    // para pedirle a OverlayExportService que rasterice exactamente ese
    // contenido (ver RenderCanvas / RenderFooter).
    ImGuiWindow* m_CanvasWindowThisFrame = nullptr;
    ImVec2       m_CanvasScreenPos{};
    ImVec2       m_CanvasScreenSize{};
};

} // namespace ProyecThor::UI
