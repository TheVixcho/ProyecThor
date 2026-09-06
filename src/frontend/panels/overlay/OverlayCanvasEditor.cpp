#include "OverlayCanvasEditor.h"
#include "OverlayExportService.h"
#include "OverlayLayerRender.h"
#include "styles/CanvaStyleEditor.h"
#include "layers/LayersTheme.h"
#include "frontend/ui/IconRail.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <GL/gl.h>
#include "stb_image.h"

namespace ProyecThor::UI {

OverlayCanvasEditor::OverlayCanvasEditor(std::vector<std::string>* fontList,
                                         ResolvePngPathFn resolvePngPath,
                                         ListBgImagesFn listBgImages,
                                         ImportImageFn importImage,
                                         ImportSvgFn importSvg,
                                         ImportSvgSingleFn importSvgSingle)
    : m_FontList(fontList)
    , m_ResolvePngPath(std::move(resolvePngPath))
    , m_ListBgImages(std::move(listBgImages))
    , m_ImportImage(std::move(importImage))
    , m_ImportSvg(std::move(importSvg))
    , m_ImportSvgSingle(std::move(importSvgSingle))
{}

ImTextureID OverlayCanvasEditor::GetImageTexture(const std::string& path) {
    if (path.empty()) return 0;
    auto it = m_ImageTexCache.find(path);
    if (it != m_ImageTexCache.end()) return it->second;

    int w, h, n;
    unsigned char* d = stbi_load(path.c_str(), &w, &h, &n, 4);
    ImTextureID tex = 0;
    if (d) {
        GLuint id; glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
        stbi_image_free(d);
        tex = (ImTextureID)(intptr_t)id;
    }
    m_ImageTexCache[path] = tex;
    return tex;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apertura del editor
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::OpenNew(const OverlayDoc& defaults) {
    m_IsEditingExisting = false;
    m_IsOpen            = true;
    m_Doc                = defaults;
    m_SelectedLayer      = m_Doc.layers.empty() ? -1 : 0;
    m_SaveFailed         = false;
    memset(m_Name, 0, sizeof(m_Name));
}

void OverlayCanvasEditor::OpenEdit(const std::string& existingName, const OverlayDoc& existingDoc) {
    m_IsEditingExisting = true;
    m_IsOpen            = true;
    m_Doc                = existingDoc;
    m_SelectedLayer      = m_Doc.layers.empty() ? -1 : 0;
    m_SaveFailed         = false;

    size_t len = std::min(existingName.size(), sizeof(m_Name) - 1);
    memcpy(m_Name, existingName.c_str(), len);
    m_Name[len] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal — SIEMPRE a pantalla completa (debajo de la toolbar de
//  modos, que UIManager deja dibujada aparte -- ver
//  UIManager::EnterFullscreenEditor). El llamador ya se encargo de ocultar
//  el resto de los paneles para este frame.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::Render(OnSaveCallback onSave, OnCancelCallback onClose) {
    if (!m_IsOpen) return;

    ImGuiViewport* vp    = ImGui::GetMainViewport();
    float          railH = IconRailThickness(false);

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + railH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - railH));
    ImGui::SetNextWindowViewport(vp->ID);

    // NoScrollbar/NoScrollWithMouse son clave aca: el header/footer se
    // dibujan en coordenadas de PANTALLA (winPos + offset fijo) pero el
    // resto del contenido (canvas/sidebar) usa SetCursorPos, que es relativo
    // al scroll de la ventana -- si esta ventana llegaba a scrollear (ej.
    // contenido mas alto que la pantalla), el footer/header quedaban fijos
    // en pantalla mientras los widgets reales (input de nombre, botones)
    // se desplazaban con el scroll, separandose del fondo que los acompaña.
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoDocking         |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("##ovCanvasFullscreen", nullptr, kFlags);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      winPos  = ImGui::GetWindowPos();
    ImVec2      winSize = ImGui::GetWindowSize();

    RenderHeader(dl, winPos, winSize);

    constexpr float kHeaderH      = 40.0f;
    constexpr float kToolsBarH    = 44.0f;
    constexpr float kFooterH      = 52.0f;
    constexpr float kPadH         = 14.0f;
    constexpr float kLayersPanelW = 200.0f;
    constexpr float kPropsPanelW  = 280.0f;
    constexpr float kGap          = 14.0f;

    float contentH = winSize.y - kHeaderH - kToolsBarH - kFooterH - kPadH * 2.0f;
    float canvasW  = std::max(200.0f, winSize.x - kLayersPanelW - kPropsPanelW - kGap * 2.0f - kPadH * 2.0f);

    // Izquierda: Capas -- centro: Canvas -- derecha: Propiedades. Antes
    // Capas y Propiedades vivian apiladas en una unica columna a la
    // derecha; separarlas en dos paneles propios (cada uno con su propio
    // recuadro, ver RenderLayersPanel/RenderPropertiesPanel) da mas orden y
    // dedica todo el ancho de cada uno a lo que realmente muestra.
    ImGui::SetCursorPos(ImVec2(kPadH, kHeaderH + kPadH));
    ImGui::BeginGroup();
    RenderLayersPanel(kLayersPanelW, contentH);
    ImGui::EndGroup();

    ImGui::SetCursorPos(ImVec2(kPadH + kLayersPanelW + kGap, kHeaderH + kPadH));
    ImGui::BeginGroup();
    RenderCanvas(canvasW, contentH);
    ImGui::EndGroup();

    ImGui::SetCursorPos(ImVec2(kPadH + kLayersPanelW + kGap + canvasW + kGap, kHeaderH + kPadH));
    ImGui::BeginGroup();
    RenderPropertiesPanel(kPropsPanelW, contentH);
    ImGui::EndGroup();

    // Toolbar de herramientas -- banda propia ARRIBA del footer (nunca lo
    // solapa, tiene su propio espacio reservado): Mover/Seleccion multiple/
    // Borrador/Degradado, ver RenderBottomToolbar.
    ImGui::SetCursorPos(ImVec2(kPadH, kHeaderH + kPadH + contentH));
    ImGui::BeginGroup();
    RenderBottomToolbar(winSize.x - kPadH * 2.0f);
    ImGui::EndGroup();

    RenderFooter(winPos, winSize, onSave, onClose);

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderHeader
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderHeader(ImDrawList* dl, ImVec2 winPos, ImVec2 winSize) {
    constexpr float kHeaderH = 40.0f;

    // Fill plano (sin degradado) — mas minimalista, y no compite visualmente
    // con el canvas/preview del overlay que se edita.
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
                      CanvaPalette::ToU32(CanvaPalette::Surface1));

    float dotY = winPos.y + kHeaderH * 0.5f;
    dl->AddCircleFilled(ImVec2(winPos.x + 22.0f, dotY), 5.0f, CanvaPalette::ToU32(CanvaPalette::Pink));

    std::string title = m_IsEditingExisting
        ? (std::string("Editar overlay — ") + m_Name)
        : "Nuevo overlay";

    dl->AddText(ImGui::GetFont(), 14.0f,
        ImVec2(winPos.x + 36.0f, winPos.y + (kHeaderH - 14.0f) * 0.5f),
        CanvaPalette::ToU32(CanvaPalette::Text), title.c_str());

    dl->AddLine(
        ImVec2(winPos.x, winPos.y + kHeaderH),
        ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);

    ImGui::Dummy(ImVec2(0.0f, kHeaderH));
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFloatingToolbar — pastilla flotante arriba del canvas: Texto/Forma/
//  Imagen/Eliminar. "Eliminar" actua sobre la capa seleccionada (seleccionar
//  una capa se hace haciendo click en ella dentro del canvas o en la lista
//  de capas del sidebar).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderFloatingToolbar(ImVec2 canvasPos, ImVec2 canvasSize) {
    constexpr float kBtnSz = 34.0f;
    constexpr float kGap   = 6.0f;
    constexpr float kPad   = 8.0f;
    const int       kCount = 5;

    float barW = kPad * 2.0f + kBtnSz * kCount + kGap * (kCount - 1);
    ImVec2 barPos = ImVec2(canvasPos.x + (canvasSize.x - barW) * 0.5f, canvasPos.y - kBtnSz - 18.0f);

    ImGui::SetCursorScreenPos(barPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(CanvaPalette::Surface1.x, CanvaPalette::Surface1.y, CanvaPalette::Surface1.z, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPad, kPad));
    ImGui::BeginChild("##ovFloatingToolbar", ImVec2(barW, kBtnSz + kPad * 2.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    bool hasSelection = (m_SelectedLayer >= 0 && m_SelectedLayer < (int)m_Doc.layers.size());
    bool hasClock     = (FindClockLayer(m_Doc) != nullptr);

    if (LPCornerIconBtn("##ovAddText", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddLine({c.x - r*0.6f, c.y - r*0.55f}, {c.x + r*0.6f, c.y - r*0.55f}, col, 2.0f);
            dl->AddLine({c.x, c.y - r*0.55f}, {c.x, c.y + r*0.6f}, col, 2.0f);
        }, "Anadir texto", {kBtnSz, kBtnSz})) {
        OverlayLayer nl;
        nl.kind = OverlayLayerKind::Text;
        m_Doc.layers.push_back(nl);
        m_SelectedLayer = (int)m_Doc.layers.size() - 1;
    }
    ImGui::SameLine(0.0f, kGap);

    if (LPCornerIconBtn("##ovAddShape", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.65f, c.y - r*0.5f}, {c.x + r*0.05f, c.y + r*0.15f}, col, 2.0f, 0, 1.6f);
            dl->AddCircle({c.x + r*0.28f, c.y + r*0.1f}, r*0.32f, col, 0, 1.6f);
        }, "Anadir forma", {kBtnSz, kBtnSz}))
        ImGui::OpenPopup("##ovAddShapePop");
    ImGui::SameLine(0.0f, kGap);

    if (LPCornerIconBtn("##ovAddImage", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.7f, c.y - r*0.55f}, {c.x + r*0.7f, c.y + r*0.55f}, col, 2.0f, 0, 1.6f);
            dl->AddCircleFilled({c.x - r*0.32f, c.y - r*0.2f}, r*0.16f, col);
            dl->AddTriangleFilled({c.x - r*0.55f, c.y + r*0.5f}, {c.x - r*0.05f, c.y}, {c.x + r*0.55f, c.y + r*0.5f}, col);
        }, "Anadir imagen", {kBtnSz, kBtnSz}))
        ImGui::OpenPopup("##ovAddImagePop");
    ImGui::SameLine(0.0f, kGap);

    if (hasClock) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
    bool addClockClicked = LPCornerIconBtn("##ovAddClock", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddCircle(c, r * 0.62f, col, 0, 1.6f);
            dl->AddLine(c, {c.x, c.y - r * 0.4f}, col, 1.6f);
            dl->AddLine(c, {c.x + r * 0.3f, c.y}, col, 1.6f);
        }, hasClock ? "Ya hay un cuadro de reloj" : "Anadir cuadro de reloj", {kBtnSz, kBtnSz});
    if (hasClock) ImGui::PopStyleVar();
    if (addClockClicked && !hasClock) {
        OverlayLayer nl;
        nl.kind = OverlayLayerKind::Clock;
        nl.text = "00:00:00";
        m_Doc.layers.push_back(nl);
        m_SelectedLayer = (int)m_Doc.layers.size() - 1;
    }
    ImGui::SameLine(0.0f, kGap);

    if (!hasSelection) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
    bool delClicked = LPCornerIconBtn("##ovDelSel", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            dl->AddRect({c.x - r*0.45f, c.y - r*0.25f}, {c.x + r*0.45f, c.y + r*0.6f}, col, 1.5f, 0, 1.6f);
            dl->AddLine({c.x - r*0.65f, c.y - r*0.4f}, {c.x + r*0.65f, c.y - r*0.4f}, col, 1.6f);
            dl->AddLine({c.x - r*0.2f, c.y - r*0.4f}, {c.x - r*0.2f, c.y - r*0.6f}, col, 1.6f);
            dl->AddLine({c.x + r*0.2f, c.y - r*0.4f}, {c.x + r*0.2f, c.y - r*0.6f}, col, 1.6f);
        }, "Eliminar seleccionado", {kBtnSz, kBtnSz});
    if (!hasSelection) ImGui::PopStyleVar();
    if (delClicked && hasSelection) {
        m_Doc.layers.erase(m_Doc.layers.begin() + m_SelectedLayer);
        m_SelectedLayer = -1;
    }

    if (ImGui::BeginPopup("##ovAddShapePop")) {
        if (ImGui::Selectable("Rectangulo")) {
            OverlayLayer nl;
            nl.kind      = OverlayLayerKind::Shape;
            nl.shapeKind = OverlayShapeKind::Rectangle;
            nl.color[0] = CanvaPalette::Accent.x; nl.color[1] = CanvaPalette::Accent.y;
            nl.color[2] = CanvaPalette::Accent.z; nl.color[3] = 0.85f;
            m_Doc.layers.push_back(nl);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        if (ImGui::Selectable("Elipse")) {
            OverlayLayer nl;
            nl.kind      = OverlayLayerKind::Shape;
            nl.shapeKind = OverlayShapeKind::Ellipse;
            nl.color[0] = CanvaPalette::Accent.x; nl.color[1] = CanvaPalette::Accent.y;
            nl.color[2] = CanvaPalette::Accent.z; nl.color[3] = 0.85f;
            m_Doc.layers.push_back(nl);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##ovAddImagePop")) {
        AddImageLayerFromMenu();
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void OverlayCanvasEditor::AddImageLayerFromMenu() {
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::TextUnformatted("Anadir imagen");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::BeginMenu("Desde Fondos")) {
        std::vector<std::string> imgs = m_ListBgImages ? m_ListBgImages() : std::vector<std::string>{};
        if (imgs.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
            ImGui::TextUnformatted("No hay imágenes en Fondos.");
            ImGui::PopStyleColor();
        }
        for (const auto& path : imgs) {
            std::string fname = path;
            if (auto pos = fname.find_last_of("/\\"); pos != std::string::npos)
                fname = fname.substr(pos + 1);
            if (ImGui::Selectable(fname.c_str())) {
                OverlayLayer nl;
                nl.kind      = OverlayLayerKind::Image;
                nl.imagePath = path;
                m_Doc.layers.push_back(nl);
                m_SelectedLayer = (int)m_Doc.layers.size() - 1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::Selectable("Importar archivo...")) {
        std::string path = m_ImportImage ? m_ImportImage() : std::string();
        if (!path.empty()) {
            OverlayLayer nl;
            nl.kind      = OverlayLayerKind::Image;
            nl.imagePath = path;
            m_Doc.layers.push_back(nl);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Selectable("Importar SVG (una imagen)")) {
        // Rasteriza el SVG completo como UNA sola capa -- garantiza verse
        // igual al archivo original. Recomendado para SVG de Canva y
        // similares, que no agrupan de forma util para separar en capas
        // (ver comentario largo en "por capas" mas abajo).
        OverlayLayer single = m_ImportSvgSingle ? m_ImportSvgSingle(m_Doc.canvasW, m_Doc.canvasH) : OverlayLayer{};
        if (!single.imagePath.empty()) {
            m_Doc.layers.push_back(single);
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Selectable("Importar SVG (por capas)...")) {
        // Cada grupo de primer nivel del SVG (<g id="...">) entra como su
        // propia capa Image ya rasterizada -- permite traer un diseño de
        // Illustrator/Figma/Inkscape y seguir moviendo cada parte por
        // separado, en vez de una sola imagen plana. Ver OverlaySvgImport.h.
        // ADVERTENCIA: Canva (y herramientas similares) exportan cada
        // forma/glifo suelto como su propio grupo de primer nivel sin
        // jerarquia real de "capas de diseño" -- en esos archivos este modo
        // termina fragmentando en decenas de pedazos irreconocibles; usa
        // "una imagen" arriba para esos casos.
        std::vector<OverlayLayer> svgLayers =
            m_ImportSvg ? m_ImportSvg(m_Doc.canvasW, m_Doc.canvasH) : std::vector<OverlayLayer>{};
        if (!svgLayers.empty()) {
            for (auto& l : svgLayers) m_Doc.layers.push_back(std::move(l));
            m_SelectedLayer = (int)m_Doc.layers.size() - 1;
        }
        ImGui::CloseCurrentPopup();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCanvas — area central: fondo transparente + capas arrastrables
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderCanvas(float availW, float availH) {
    constexpr float kToolbarReserve = 62.0f; // espacio para la toolbar flotante arriba

    float aspect = (float)m_Doc.canvasW / (float)std::max(1, m_Doc.canvasH);
    float availCanvasH = availH - kToolbarReserve;
    float cw = availW;
    float ch = cw / aspect;
    if (ch > availCanvasH) { ch = availCanvasH; cw = ch * aspect; }
    cw = std::max(cw, 100.0f);
    ch = std::max(ch, 60.0f);

    float offsetX = std::max(0.0f, (availW - cw) * 0.5f);

    ImGui::Dummy(ImVec2(0.0f, kToolbarReserve));
    if (offsetX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##ovCanvasArea", ImVec2(cw, ch), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Capturado para el export (ver RenderFooter / OverlayExportService).
    m_CanvasWindowThisFrame = ImGui::GetCurrentWindow();
    m_CanvasScreenPos       = ImGui::GetWindowPos();
    m_CanvasScreenSize      = ImGui::GetWindowSize();

    // "dl" es el draw list de ESTA child — es exactamente lo que
    // OverlayExportService captura y guarda como PNG, asi que solo debe
    // contener el DISEÑO real (fondo + capas). Cualquier chrome de edicion
    // (marco de seleccion, handles) va en el foreground draw list, que nunca
    // se incluye en la exportacion.
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImDrawList* fgDl = ImGui::GetForegroundDrawList();
    ImVec2 p0 = m_CanvasScreenPos;
    ImVec2 p1 = ImVec2(p0.x + m_CanvasScreenSize.x, p0.y + m_CanvasScreenSize.y);

    // Fondo a cuadros (checkerboard) SOLO como guia visual de "sin fondo",
    // igual que Photoshop/Canva. Se dibuja en "dl" -- la EXPORTACION ya NO
    // lee este draw list en absoluto (ver DrawLayersForExport/RenderFooter):
    // arma su propio ImDrawList aparte con solo las capas reales, asi que el
    // cuadriculado puede vivir aca (visible, con el z-order correcto: por
    // debajo de las capas que se dibujan despues en esta misma lista) sin
    // ningun riesgo de terminar horneado en el PNG.
    {
        const float cell = 14.0f;
        ImU32 c1 = IM_COL32(38, 38, 44, 255), c2 = IM_COL32(30, 30, 35, 255);
        int cols = (int)std::ceil(m_CanvasScreenSize.x / cell);
        int rows = (int)std::ceil(m_CanvasScreenSize.y / cell);
        dl->PushClipRect(p0, p1, true);
        for (int ry = 0; ry < rows; ry++)
            for (int rx = 0; rx < cols; rx++) {
                ImVec2 a = { p0.x + rx * cell, p0.y + ry * cell };
                ImVec2 b = { std::min(p1.x, a.x + cell), std::min(p1.y, a.y + cell) };
                dl->AddRectFilled(a, b, ((rx + ry) % 2 == 0) ? c1 : c2);
            }
        dl->PopClipRect();
    }

    if (m_Doc.bgColor[3] > 0.001f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(m_Doc.bgColor[0], m_Doc.bgColor[1], m_Doc.bgColor[2], m_Doc.bgColor[3]));
        dl->AddRectFilled(p0, p1, bg);
    }
    fgDl->PushClipRect(p0, p1, true);

    // Click en area vacia = deseleccionar. Va ANTES que las capas para que
    // estas, dibujadas despues, le "roben" el hover en su propia zona — sin
    // AllowOverlap, ImGui le da el hover de toda la zona al primer item
    // sometido (este), y ninguna capa por encima llegaria a recibirlo nunca.
    auto&  core  = Core::PresentationCore::Get();
    float  scale = m_CanvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);

    // Bbox en pantalla de una capa, misma formula que el loop de dibujo de
    // abajo -- factorizado para reusar en el recuadro de seleccion (ver
    // mas abajo), que necesita testear TODAS las capas de una sola vez al
    // soltar el mouse, antes de que el loop principal las haya recorrido
    // este frame.
    auto ComputeLayerBounds = [&](int idx, ImVec2& outTl, ImVec2& outBr) {
        const auto& l = m_Doc.layers[idx];
        ImVec2 bsz;
        if (l.kind == OverlayLayerKind::Text || l.kind == OverlayLayerKind::Clock) {
            ImFont* f = core.GetImGuiFont(l.fontName, l.fontSize);
            if (!f) f = ImGui::GetFont();
            float ds = std::max(4.0f, l.fontSize * scale);
            bsz = f->CalcTextSizeA(ds, FLT_MAX, FLT_MAX, l.text.c_str());
        } else {
            bsz = ImVec2(std::max(4.0f, l.sizeW * m_CanvasScreenSize.x),
                        std::max(4.0f, l.sizeH * m_CanvasScreenSize.y));
        }
        ImVec2 c = ImVec2(p0.x + l.posX * m_CanvasScreenSize.x, p0.y + l.posY * m_CanvasScreenSize.y);
        outTl = ImVec2(c.x - bsz.x * 0.5f, c.y - bsz.y * 0.5f);
        outBr = ImVec2(outTl.x + bsz.x, outTl.y + bsz.y);
    };

    ImGui::SetCursorScreenPos(p0);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##ovCanvasBg", m_CanvasScreenSize);

    if (m_ActiveTool == OverlayTool::Move) {
        if (ImGui::IsItemActivated()) {
            m_RubberBandActive = true;
            m_RubberBandStart  = ImGui::GetIO().MousePos;
            if (!ImGui::GetIO().KeyCtrl) { m_SelectedLayer = -1; m_MultiSelected.clear(); }
        }
        if (m_RubberBandActive && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 cur = ImGui::GetIO().MousePos;
            ImVec2 rMin(std::min(m_RubberBandStart.x, cur.x), std::min(m_RubberBandStart.y, cur.y));
            ImVec2 rMax(std::max(m_RubberBandStart.x, cur.x), std::max(m_RubberBandStart.y, cur.y));
            fgDl->AddRectFilled(rMin, rMax, IM_COL32(90, 150, 255, 40));
            fgDl->AddRect(rMin, rMax, IM_COL32(130, 175, 255, 220), 0.0f, 0, 1.5f);
        }
        if (ImGui::IsItemDeactivated()) {
            if (m_RubberBandActive) {
                ImVec2 cur = ImGui::GetIO().MousePos;
                ImVec2 rMin(std::min(m_RubberBandStart.x, cur.x), std::min(m_RubberBandStart.y, cur.y));
                ImVec2 rMax(std::max(m_RubberBandStart.x, cur.x), std::max(m_RubberBandStart.y, cur.y));
                bool didDrag = (rMax.x - rMin.x > 3.0f || rMax.y - rMin.y > 3.0f);
                if (didDrag) {
                    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
                        ImVec2 ltl, lbr;
                        ComputeLayerBounds(i, ltl, lbr);
                        bool intersects = !(ltl.x > rMax.x || lbr.x < rMin.x || ltl.y > rMax.y || lbr.y < rMin.y);
                        if (intersects && !IsMultiSelected(i)) m_MultiSelected.push_back(i);
                    }
                    if (!m_MultiSelected.empty()) m_SelectedLayer = m_MultiSelected.back();
                }
                m_RubberBandActive = false;
            }
            m_DraggingLayer = -1;
        }
    }

    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
        auto& layer = m_Doc.layers[i];
        bool  isText  = (layer.kind == OverlayLayerKind::Text);
        bool  isImage = (layer.kind == OverlayLayerKind::Image);
        bool  isShape = (layer.kind == OverlayLayerKind::Shape);
        bool  isClock = (layer.kind == OverlayLayerKind::Clock);

        ImFont* font = nullptr;
        float   displaySize = 0.0f;
        ImVec2  blockSz;

        if (isText || isClock) {
            font = core.GetImGuiFont(layer.fontName, layer.fontSize);
            if (!font) font = ImGui::GetFont();
            displaySize = std::max(4.0f, layer.fontSize * scale);
            blockSz = font->CalcTextSizeA(displaySize, FLT_MAX, FLT_MAX, layer.text.c_str());
        } else {
            blockSz = ImVec2(std::max(4.0f, layer.sizeW * m_CanvasScreenSize.x),
                              std::max(4.0f, layer.sizeH * m_CanvasScreenSize.y));
        }

        ImVec2 lcenter = ImVec2(p0.x + layer.posX * m_CanvasScreenSize.x,
                                 p0.y + layer.posY * m_CanvasScreenSize.y);
        ImVec2 tl = ImVec2(lcenter.x - blockSz.x * 0.5f, lcenter.y - blockSz.y * 0.5f);
        ImVec2 br = ImVec2(tl.x + blockSz.x, tl.y + blockSz.y);

        if (isText) {
            DrawOverlayLayerStyledText(dl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);
        } else if (isClock) {
            // Cuadro-flag: se previsualiza en el editor (placeholder + marco
            // punteado) pero SOLO en el foreground draw list -- nunca en
            // "dl", así que nunca queda horneado en el PNG exportado. En
            // vivo, el reloj real se dibuja en esta misma posicion/estilo
            // sobre el overlay ya proyectado (ver LiveContentRenderer.cpp/
            // UIManager.cpp), no sobre el PNG.
            DrawOverlayLayerStyledText(fgDl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);

            constexpr float kDash = 5.0f;
            ImU32 dashCol = CanvaPalette::ToU32(CanvaPalette::Accent);
            for (float x = tl.x; x < br.x; x += kDash * 2.0f) {
                fgDl->AddLine({x, tl.y}, {std::min(x + kDash, br.x), tl.y}, dashCol, 1.5f);
                fgDl->AddLine({x, br.y}, {std::min(x + kDash, br.x), br.y}, dashCol, 1.5f);
            }
            for (float y = tl.y; y < br.y; y += kDash * 2.0f) {
                fgDl->AddLine({tl.x, y}, {tl.x, std::min(y + kDash, br.y)}, dashCol, 1.5f);
                fgDl->AddLine({br.x, y}, {br.x, std::min(y + kDash, br.y)}, dashCol, 1.5f);
            }
            const char* tag = "RELOJ";
            ImVec2 tagSz = ImGui::CalcTextSize(tag);
            fgDl->AddRectFilled({tl.x, tl.y - tagSz.y - 4.0f}, {tl.x + tagSz.x + 8.0f, tl.y - 2.0f},
                                dashCol, 3.0f);
            fgDl->AddText({tl.x + 4.0f, tl.y - tagSz.y - 2.0f}, IM_COL32(20, 20, 24, 255), tag);
        } else if (isImage) {
            ImTextureID tex = GetImageTexture(layer.imagePath);
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float rotRad  = layer.rotation * (float)M_PI / 180.0f;
            float cs = cosf(rotRad), sn = sinf(rotRad);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            auto Rot = [&](float lx, float ly) {
                return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
            };
            ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
            ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, std::clamp(layer.opacity, 0.0f, 1.0f)));
            if (tex) {
                dl->AddImageQuad(tex, qTL, qTR, qBR, qBL, ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), tint);
            } else {
                dl->AddQuadFilled(qTL, qTR, qBR, qBL, IM_COL32(40, 40, 46, 255));
                dl->AddQuad(qTL, qTR, qBR, qBL, IM_COL32(150, 70, 70, 255));
            }
        } else { // isShape
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            float rotRad = layer.rotation * (float)M_PI / 180.0f;
            float op = std::clamp(layer.opacity, 0.0f, 1.0f);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3] * op));
            ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3] * op));
            float ow = std::max(0.5f, layer.outlineWidth * scale);

            if (layer.shapeKind == OverlayShapeKind::Ellipse) {
                if (layer.shapeFilled) dl->AddEllipseFilled(center, ImVec2(hw, hh), fillCol, rotRad, 0);
                if (layer.outlineEnabled) dl->AddEllipse(center, ImVec2(hw, hh), strokeCol, rotRad, 0, ow);
            } else if (std::fabs(layer.rotation) < 0.01f) {
                float rounding = layer.shapeRounding * scale;
                if (layer.shapeFilled) dl->AddRectFilled(tl, br, fillCol, rounding);
                if (layer.outlineEnabled) dl->AddRect(tl, br, strokeCol, rounding, 0, ow);
            } else {
                float cs = cosf(rotRad), sn = sinf(rotRad);
                auto Rot = [&](float lx, float ly) {
                    return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
                };
                ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
                if (layer.shapeFilled) dl->AddQuadFilled(qTL, qTR, qBR, qBL, fillCol);
                if (layer.outlineEnabled) dl->AddQuad(qTL, qTR, qBR, qBL, strokeCol, ow);
            }
        }

        ImGui::PushID(i);
        ImGui::SetNextItemAllowOverlap();
        ImGui::SetCursorScreenPos(ImVec2(tl.x - 4.0f, tl.y - 4.0f));
        ImGui::InvisibleButton("##ovLayerHit", ImVec2(blockSz.x + 8.0f, blockSz.y + 8.0f));

        bool isMoveTool = (m_ActiveTool == OverlayTool::Move);
        bool isSel  = isMoveTool ? IsMultiSelected(i) : (m_SelectedLayer == i);
        bool isHov  = ImGui::IsItemHovered();

        // Chrome de edicion (marco de seleccion / hover) — solo en el
        // foreground draw list, nunca en "dl" (lo que se exporta a PNG).
        if (isSel) {
            fgDl->AddRect(ImVec2(tl.x - 4.0f, tl.y - 4.0f),
                          ImVec2(tl.x + blockSz.x + 4.0f, tl.y + blockSz.y + 4.0f),
                          CanvaPalette::ToU32(CanvaPalette::Accent), 3.0f, 0, 1.5f);
        } else if (isHov) {
            fgDl->AddRect(ImVec2(tl.x - 4.0f, tl.y - 4.0f),
                          ImVec2(tl.x + blockSz.x + 4.0f, tl.y + blockSz.y + 4.0f),
                          IM_COL32(255, 255, 255, 90), 3.0f, 0, 1.0f);
        }
        if (isHov && m_ActiveTool == OverlayTool::Move) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        if (m_ActiveTool == OverlayTool::Eraser || m_ActiveTool == OverlayTool::Gradient) {
            // Estas herramientas no mueven capas -- clickear solo selecciona
            // cual es el objetivo (ver RenderBottomToolbar).
            if (ImGui::IsItemActivated()) m_SelectedLayer = i;
        } else { // Move -- siempre se puede mover presionando el objeto; Ctrl+click
                 // suma/saca esta capa de la seleccion multiple, y agarrar
                 // cualquier capa de una seleccion multiple existente mueve
                 // a todo el grupo junto (ver comentario del enum OverlayTool).
            if (ImGui::IsItemActivated()) {
                bool ctrl = ImGui::GetIO().KeyCtrl;
                bool alreadyInGroup = IsMultiSelected(i) && m_MultiSelected.size() > 1;

                if (ctrl) {
                    ToggleMultiSelected(i);
                } else if (!alreadyInGroup) {
                    // Click normal sobre una capa que no es parte de una
                    // seleccion multiple existente: selecciona solo esta.
                    m_MultiSelected.clear();
                    m_MultiSelected.push_back(i);
                }
                // Si ctrl==false y alreadyInGroup==true, se mantiene la
                // seleccion multiple tal cual estaba (para poder arrastrar
                // el grupo entero agarrando cualquiera de sus miembros).

                m_SelectedLayer = IsMultiSelected(i) ? i
                                : (m_MultiSelected.empty() ? -1 : m_MultiSelected.back());

                if (IsMultiSelected(i)) {
                    m_DraggingLayer  = i;
                    m_DragStartMouse = ImGui::GetIO().MousePos;
                    m_DragStartPosX  = layer.posX;
                    m_DragStartPosY  = layer.posY;
                    m_MultiDragStartMouse = ImGui::GetIO().MousePos;
                    m_MultiDragStartPos.clear();
                    for (int idx : m_MultiSelected)
                        m_MultiDragStartPos.push_back(ImVec2(m_Doc.layers[idx].posX, m_Doc.layers[idx].posY));
                } else {
                    // Ctrl+click que acaba de SACAR esta capa de la seleccion:
                    // no arrastra nada (el usuario la esta deseleccionando).
                    m_DraggingLayer = -1;
                }
            }

            bool groupDrag = m_MultiSelected.size() > 1 && IsMultiSelected(i);

            if (m_DraggingLayer == i && ImGui::IsItemActive() &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                if (groupDrag) {
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float dx = (mouse.x - m_MultiDragStartMouse.x) / m_CanvasScreenSize.x;
                    float dy = (mouse.y - m_MultiDragStartMouse.y) / m_CanvasScreenSize.y;
                    for (size_t k = 0; k < m_MultiSelected.size() && k < m_MultiDragStartPos.size(); k++) {
                        auto& mLayer = m_Doc.layers[m_MultiSelected[k]];
                        mLayer.posX = std::clamp(m_MultiDragStartPos[k].x + dx, 0.0f, 1.0f);
                        mLayer.posY = std::clamp(m_MultiDragStartPos[k].y + dy, 0.0f, 1.0f);
                    }
                } else {
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float dx = (mouse.x - m_DragStartMouse.x) / m_CanvasScreenSize.x;
                    float dy = (mouse.y - m_DragStartMouse.y) / m_CanvasScreenSize.y;
                    float newX = std::clamp(m_DragStartPosX + dx, 0.0f, 1.0f);
                    float newY = std::clamp(m_DragStartPosY + dy, 0.0f, 1.0f);

                    // Guias de alineacion: al arrastrar, si el centro de la capa
                    // queda cerca del centro o los tercios del canvas ("zonas
                    // especiales" tipicas de composicion), se ajusta exacto a esa
                    // posicion y se resalta una guia -- asi el usuario ve clarito
                    // donde esta la mitad sin tener que calcularlo el mismo.
                    // Son las UNICAS guias visibles: no hay lineas permanentes,
                    // solo aparecen mientras se arrastra cerca de una de estas
                    // posiciones especiales.
                    static const float kSnapCandidates[5] = { 0.0f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 1.0f };
                    constexpr float kSnapTol = 0.012f;
                    ImU32 guideCol = CanvaPalette::ToU32(CanvaPalette::Accent);
                    for (float c : kSnapCandidates) {
                        if (std::fabs(newX - c) < kSnapTol) {
                            newX = c;
                            float gx = p0.x + c * m_CanvasScreenSize.x;
                            fgDl->AddLine(ImVec2(gx, p0.y), ImVec2(gx, p0.y + m_CanvasScreenSize.y), guideCol, 1.5f);
                            break;
                        }
                    }
                    for (float c : kSnapCandidates) {
                        if (std::fabs(newY - c) < kSnapTol) {
                            newY = c;
                            float gy = p0.y + c * m_CanvasScreenSize.y;
                            fgDl->AddLine(ImVec2(p0.x, gy), ImVec2(p0.x + m_CanvasScreenSize.x, gy), guideCol, 1.5f);
                            break;
                        }
                    }

                    layer.posX = newX;
                    layer.posY = newY;
                }

                // Medidor de tamano/posicion en px reales del canvas (ver pedido
                // de "medidor de px de los bordes y altura de cada imagen") --
                // se muestra junto al cursor mientras se arrastra, sea grupo o
                // capa individual.
                char dimBuf[96];
                int  pxX = (int)std::lround(layer.posX * m_Doc.canvasW);
                int  pxY = (int)std::lround(layer.posY * m_Doc.canvasH);
                int  pxW = (int)std::lround(blockSz.x / std::max(0.0001f, scale));
                int  pxH = (int)std::lround(blockSz.y / std::max(0.0001f, scale));
                snprintf(dimBuf, sizeof(dimBuf), "%d, %d  •  %d x %d px", pxX, pxY, pxW, pxH);
                ImVec2 dimPos = ImVec2(br.x + 10.0f, tl.y);
                ImVec2 txtSz  = ImGui::CalcTextSize(dimBuf);
                fgDl->AddRectFilled(ImVec2(dimPos.x - 5.0f, dimPos.y - 3.0f),
                                    ImVec2(dimPos.x + txtSz.x + 5.0f, dimPos.y + txtSz.y + 3.0f),
                                    IM_COL32(20, 20, 24, 220), 4.0f);
                fgDl->AddText(dimPos, IM_COL32(255, 255, 255, 255), dimBuf);
            }
            if (ImGui::IsItemDeactivated()) m_DraggingLayer = -1;
        }

        // Handles de redimension y rotacion — capas de imagen/forma, y solo
        // si esta seleccionada (para no saturar el canvas de agarres). Los
        // handles en si se mantienen sin rotar (ejes del bounding box) para
        // no complicar el hit-testing; solo el contenido visual rota.
        if (!isText && !isClock && isSel && m_ActiveTool == OverlayTool::Move &&
            m_MultiSelected.size() <= 1) {
            RenderResizeHandle(i, layer, 0, ImVec2(tl.x, tl.y), fgDl);
            RenderResizeHandle(i, layer, 1, ImVec2(br.x, tl.y), fgDl);
            RenderResizeHandle(i, layer, 2, ImVec2(tl.x, br.y), fgDl);
            RenderResizeHandle(i, layer, 3, ImVec2(br.x, br.y), fgDl);
            RenderRotateHandle(i, layer, ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f),
                              blockSz.y * 0.5f, fgDl);
        }

        ImGui::PopID();
    }

    // Herramienta Borrador: superficie propia sobre TODO el canvas (encima
    // de las capas, para capturar el arrastre sin competir con el hit-test
    // de cada una) -- solo activa si hay una capa Image seleccionada.
    if (m_ActiveTool == OverlayTool::Eraser && m_SelectedLayer >= 0 &&
        m_SelectedLayer < (int)m_Doc.layers.size() &&
        m_Doc.layers[m_SelectedLayer].kind == OverlayLayerKind::Image) {
        EnsurePixelEditBuffer(m_SelectedLayer);

        ImGui::SetCursorScreenPos(p0);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##ovEraserSurface", m_CanvasScreenSize);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        if (ImGui::IsItemHovered()) {
            float scalePx = m_CanvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);
            fgDl->AddCircle(mouse, m_BrushRadiusPx * scalePx, CanvaPalette::ToU32(CanvaPalette::Accent), 32, 1.5f);
        }
        if (ImGui::IsItemActive())
            ApplyEraserStroke(mouse, m_CanvasScreenSize);
        if (ImGui::IsItemDeactivated())
            SavePixelEditToDisk();
    }

    fgDl->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // La toolbar flotante se dibuja DESPUES del canvas (usa su posicion) pero
    // visualmente queda arriba porque RenderCanvas reservo kToolbarReserve.
    RenderFloatingToolbar(ImVec2(p0.x, p0.y - kToolbarReserve + 4.0f), ImVec2(cw, kToolbarReserve));

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::Text("%d x %d — sin fondo (transparente). Arrastra una capa para posicionarla",
               m_Doc.canvasW, m_Doc.canvasH);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  DrawLayersForExport — misma geometria/estilo que el canvas en vivo (ver
//  RenderCanvas) pero SOLO el contenido real: sin cuadriculado, sin chrome
//  de edicion, sin capas Clock (esas nunca se hornean). Usado exclusivamente
//  al exportar (ver RenderFooter), en un ImDrawList propio que no comparte
//  nada con lo que se ve en pantalla ese mismo frame.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::DrawLayersForExport(ImDrawList* dl, ImVec2 p0, ImVec2 canvasScreenSize) {
    if (m_Doc.bgColor[3] > 0.001f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(m_Doc.bgColor[0], m_Doc.bgColor[1], m_Doc.bgColor[2], m_Doc.bgColor[3]));
        dl->AddRectFilled(p0, ImVec2(p0.x + canvasScreenSize.x, p0.y + canvasScreenSize.y), bg);
    }

    auto&  core  = Core::PresentationCore::Get();
    float  scale = canvasScreenSize.x / (float)std::max(1, m_Doc.canvasW);

    for (const auto& layer : m_Doc.layers) {
        if (layer.kind == OverlayLayerKind::Clock) continue; // cuadro-flag, nunca se hornea

        bool isText  = (layer.kind == OverlayLayerKind::Text);
        bool isImage = (layer.kind == OverlayLayerKind::Image);

        ImFont* font = nullptr;
        float   displaySize = 0.0f;
        ImVec2  blockSz;

        if (isText) {
            font = core.GetImGuiFont(layer.fontName, layer.fontSize);
            if (!font) font = ImGui::GetFont();
            displaySize = std::max(4.0f, layer.fontSize * scale);
            blockSz = font->CalcTextSizeA(displaySize, FLT_MAX, FLT_MAX, layer.text.c_str());
        } else {
            blockSz = ImVec2(std::max(4.0f, layer.sizeW * canvasScreenSize.x),
                              std::max(4.0f, layer.sizeH * canvasScreenSize.y));
        }

        ImVec2 lcenter = ImVec2(p0.x + layer.posX * canvasScreenSize.x,
                                 p0.y + layer.posY * canvasScreenSize.y);
        ImVec2 tl = ImVec2(lcenter.x - blockSz.x * 0.5f, lcenter.y - blockSz.y * 0.5f);
        ImVec2 br = ImVec2(tl.x + blockSz.x, tl.y + blockSz.y);

        if (isText) {
            DrawOverlayLayerStyledText(dl, font, displaySize, tl, blockSz, layer, layer.text.c_str(), scale);
        } else if (isImage) {
            ImTextureID tex = GetImageTexture(layer.imagePath);
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float rotRad  = layer.rotation * (float)M_PI / 180.0f;
            float cs = cosf(rotRad), sn = sinf(rotRad);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            auto Rot = [&](float lx, float ly) {
                return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
            };
            ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
            if (tex) {
                ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, std::clamp(layer.opacity, 0.0f, 1.0f)));
                dl->AddImageQuad(tex, qTL, qTR, qBR, qBL, ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), tint);
            }
        } else { // Shape
            ImVec2 center = ImVec2((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
            float hw = blockSz.x * 0.5f, hh = blockSz.y * 0.5f;
            float rotRad = layer.rotation * (float)M_PI / 180.0f;
            float op = std::clamp(layer.opacity, 0.0f, 1.0f);
            ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3] * op));
            ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3] * op));
            float ow = std::max(0.5f, layer.outlineWidth * scale);

            if (layer.shapeKind == OverlayShapeKind::Ellipse) {
                if (layer.shapeFilled) dl->AddEllipseFilled(center, ImVec2(hw, hh), fillCol, rotRad, 0);
                if (layer.outlineEnabled) dl->AddEllipse(center, ImVec2(hw, hh), strokeCol, rotRad, 0, ow);
            } else if (std::fabs(layer.rotation) < 0.01f) {
                float rounding = layer.shapeRounding * scale;
                if (layer.shapeFilled) dl->AddRectFilled(tl, br, fillCol, rounding);
                if (layer.outlineEnabled) dl->AddRect(tl, br, strokeCol, rounding, 0, ow);
            } else {
                float cs = cosf(rotRad), sn = sinf(rotRad);
                auto Rot = [&](float lx, float ly) {
                    return ImVec2(center.x + lx * cs - ly * sn, center.y + lx * sn + ly * cs);
                };
                ImVec2 qTL = Rot(-hw, -hh), qTR = Rot(hw, -hh), qBR = Rot(hw, hh), qBL = Rot(-hw, hh);
                if (layer.shapeFilled) dl->AddQuadFilled(qTL, qTR, qBR, qBL, fillCol);
                if (layer.outlineEnabled) dl->AddQuad(qTL, qTR, qBR, qBL, strokeCol, ow);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderResizeHandle — agarre en una esquina de una capa de imagen/forma. Al
//  arrastrar, la esquina OPUESTA queda fija y size/posicion se recalculan en
//  espacio normalizado (0..1) del canvas, igual que posX/posY.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderResizeHandle(int layerIdx, OverlayLayer& layer, int corner,
                                             ImVec2 handlePos, ImDrawList* fgDl) {
    constexpr float kHandleR = 5.0f;
    constexpr float kHitR    = 9.0f;

    ImGui::PushID(corner + 100);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(ImVec2(handlePos.x - kHitR, handlePos.y - kHitR));
    ImGui::InvisibleButton("##ovResizeHandle", ImVec2(kHitR * 2.0f, kHitR * 2.0f));

    bool isHov = ImGui::IsItemHovered();
    if (isHov) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

    fgDl->AddCircleFilled(handlePos, kHandleR, CanvaPalette::ToU32(CanvaPalette::Accent));
    fgDl->AddCircle(handlePos, kHandleR, IM_COL32(20, 20, 24, 255), 0, 1.5f);

    if (ImGui::IsItemActivated()) {
        m_ResizeStartMouse  = ImGui::GetIO().MousePos;
        m_ResizeStartPosX   = layer.posX;
        m_ResizeStartPosY   = layer.posY;
        m_ResizeStartSizeW  = layer.sizeW;
        m_ResizeStartSizeH  = layer.sizeH;
        m_ResizingLayer     = layerIdx;
        m_ResizeCorner      = corner;
        m_SelectedLayer     = layerIdx;
    }
    if (m_ResizingLayer == layerIdx && m_ResizeCorner == corner && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = (mouse.x - m_ResizeStartMouse.x) / m_CanvasScreenSize.x;
        float dy = (mouse.y - m_ResizeStartMouse.y) / m_CanvasScreenSize.y;

        float hw = m_ResizeStartSizeW * 0.5f, hh = m_ResizeStartSizeH * 0.5f;
        float startTLx = m_ResizeStartPosX - hw, startTLy = m_ResizeStartPosY - hh;
        float startBRx = m_ResizeStartPosX + hw, startBRy = m_ResizeStartPosY + hh;

        // Esquina que se mueve con el mouse vs. esquina opuesta, que queda fija.
        float movX = (corner == 1 || corner == 3) ? startBRx + dx : startTLx + dx;
        float movY = (corner == 2 || corner == 3) ? startBRy + dy : startTLy + dy;
        float fixX = (corner == 1 || corner == 3) ? startTLx      : startBRx;
        float fixY = (corner == 2 || corner == 3) ? startTLy      : startBRy;

        constexpr float kMinSize = 0.02f;
        float newW = std::max(kMinSize, std::abs(movX - fixX));
        float newH = std::max(kMinSize, std::abs(movY - fixY));
        layer.sizeW = newW;
        layer.sizeH = newH;
        layer.posX  = std::clamp((movX + fixX) * 0.5f, 0.0f, 1.0f);
        layer.posY  = std::clamp((movY + fixY) * 0.5f, 0.0f, 1.0f);
    }
    if (ImGui::IsItemDeactivated() && m_ResizingLayer == layerIdx && m_ResizeCorner == corner)
        m_ResizingLayer = -1;

    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderRotateHandle — agarre flotante arriba de la capa (capas de imagen/
//  forma); al arrastrar, gira alrededor de su propio centro. El angulo se
//  mide como atan2(dx, -dy) para que 0° = arriba, coherente con la rotacion
//  aplicada al dibujar (ver RenderCanvas).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderRotateHandle(int layerIdx, OverlayLayer& layer, ImVec2 center,
                                             float halfH, ImDrawList* fgDl) {
    constexpr float kDist = 26.0f;
    constexpr float kR    = 6.0f;
    constexpr float kHitR = 10.0f;

    float rotRad = layer.rotation * (float)M_PI / 180.0f;
    float cs = cosf(rotRad), sn = sinf(rotRad);
    auto Rot = [&](ImVec2 p) {
        return ImVec2(center.x + p.x * cs - p.y * sn, center.y + p.x * sn + p.y * cs);
    };
    ImVec2 topPt    = Rot(ImVec2(0.0f, -halfH));
    ImVec2 handlePt = Rot(ImVec2(0.0f, -(halfH + kDist)));

    fgDl->AddLine(topPt, handlePt, IM_COL32(255, 255, 255, 140), 1.5f);

    ImGui::PushID(200);
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorScreenPos(ImVec2(handlePt.x - kHitR, handlePt.y - kHitR));
    ImGui::InvisibleButton("##ovRotateHandle", ImVec2(kHitR * 2.0f, kHitR * 2.0f));

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    fgDl->AddCircleFilled(handlePt, kR, CanvaPalette::ToU32(CanvaPalette::Accent));
    fgDl->AddCircle(handlePt, kR, IM_COL32(20, 20, 24, 255), 0, 1.5f);

    if (ImGui::IsItemActivated()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        m_RotateStartAngle    = atan2f(m.x - center.x, -(m.y - center.y));
        m_RotateStartRotation = layer.rotation;
        m_RotatingLayer       = layerIdx;
    }
    if (m_RotatingLayer == layerIdx && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 m = ImGui::GetIO().MousePos;
        float angNow   = atan2f(m.x - center.x, -(m.y - center.y));
        float deltaDeg = (angNow - m_RotateStartAngle) * 180.0f / (float)M_PI;
        layer.rotation = m_RotateStartRotation + deltaDeg;
    }
    if (ImGui::IsItemDeactivated() && m_RotatingLayer == layerIdx)
        m_RotatingLayer = -1;

    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderLayersPanel — lista de capas (objetos), panel propio a la
//  izquierda del canvas. Anadir capas vive en la toolbar flotante (ver
//  RenderFloatingToolbar) -- este panel solo lista/reordena/selecciona lo
//  que ya existe.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderLayersPanel(float w, float h) {
    CanvaStyleEditor::SectionLabel("CAPAS");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // Padding explicito -- sin esto, los controles (dimensionados a "w",
    // el ancho TOTAL del panel) se salian contra el borde interno de este
    // child bordeado, ya que su area de contenido real es mas chica que
    // "w" (le resta el padding/borde propios).
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::BeginChild("##ovLayerList", ImVec2(w, h - 32.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
    w = ImGui::GetContentRegionAvail().x;
    if (m_Doc.layers.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Sin capas todavia. Usa la toolbar de arriba del canvas para anadir texto, formas, imágenes o un reloj.");
        ImGui::PopStyleColor();
    }
    // Filas estilo Photoshop: mas altura, cuadradas con padding, y agarrables
    // (drag and drop) para reordenar -- ademas de los botones ^/v, que se
    // mantienen para quien prefiera clicks precisos.
    constexpr float kRowH   = 52.0f;
    constexpr float kRowGap = 6.0f;
    constexpr float kRowPad = 10.0f;
    ImDrawList* rowsDl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)m_Doc.layers.size(); i++) {
        ImGui::PushID(i);
        bool isSel = (m_SelectedLayer == i);
        const auto& l = m_Doc.layers[i];

        std::string label;
        const char* tag = "[T] ";
        if (l.kind == OverlayLayerKind::Text) {
            label = l.text.empty() ? "(vacio)" : l.text;
            for (auto& c : label) if (c == '\n') c = ' ';
            tag = "[T] ";
        } else if (l.kind == OverlayLayerKind::Image) {
            label = l.imagePath;
            if (auto pos = label.find_last_of("/\\"); pos != std::string::npos)
                label = label.substr(pos + 1);
            if (label.empty()) label = "(imagen)";
            tag = "[I] ";
        } else if (l.kind == OverlayLayerKind::Clock) {
            label = "Reloj";
            tag = "[R] ";
        } else {
            label = (l.shapeKind == OverlayShapeKind::Ellipse) ? "Elipse" : "Rectangulo";
            tag = "[F] ";
        }
        if (label.size() > 18) label = label.substr(0, 15) + "...";
        label = std::string(tag) + label;

        ImVec2 rowP0 = ImGui::GetCursorScreenPos();
        ImVec2 rowP1 = ImVec2(rowP0.x + w, rowP0.y + kRowH);

        // Item unico que cubre toda la fila -- selecciona al clickear y es
        // la fuente/destino del drag and drop. AllowOverlap para que los
        // botones dibujados encima (^/v/x) sigan siendo clickeables.
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##ovLayerRow", ImVec2(w, kRowH));
        bool rowHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) m_SelectedLayer = i;

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
            ImGui::SetDragDropPayload("OVERLAY_LAYER_ROW", &i, sizeof(int));
            ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::Accent);
            ImGui::TextUnformatted(label.c_str());
            ImGui::PopStyleColor();
            m_SelectedLayer = i;
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OVERLAY_LAYER_ROW")) {
                int srcIdx = *(const int*)payload->Data;
                if (srcIdx != i && srcIdx >= 0 && srcIdx < (int)m_Doc.layers.size()) {
                    OverlayLayer moved = m_Doc.layers[srcIdx];
                    m_Doc.layers.erase(m_Doc.layers.begin() + srcIdx);
                    int destIdx = (srcIdx < i) ? (i - 1) : i;
                    m_Doc.layers.insert(m_Doc.layers.begin() + destIdx, moved);
                    m_SelectedLayer = destIdx;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImU32 rowBg = isSel ? ImGui::ColorConvertFloat4ToU32(ImVec4(
                          CanvaPalette::Accent.x * 0.30f, CanvaPalette::Accent.y * 0.30f,
                          CanvaPalette::Accent.z * 0.58f, 1.0f))
                    : rowHovered ? CanvaPalette::ToU32(CanvaPalette::Surface2)
                                 : CanvaPalette::ToU32(CanvaPalette::Surface1);
        rowsDl->AddRectFilled(rowP0, rowP1, rowBg, 8.0f);
        if (isSel)
            rowsDl->AddRect(rowP0, rowP1, CanvaPalette::ToU32(CanvaPalette::Accent), 8.0f, 0, 1.5f);

        float textH = ImGui::GetTextLineHeight();
        rowsDl->AddText(ImVec2(rowP0.x + kRowPad, rowP0.y + (kRowH - textH) * 0.5f),
                        CanvaPalette::ToU32(CanvaPalette::Text), label.c_str());

        // Reordenar (subir/bajar en z-order) -- swap con el vecino, no
        // cambia la cantidad de capas asi que es seguro seguir iterando.
        float btnY = rowP0.y + (kRowH - 22.0f) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(rowP1.x - kRowPad - 90.0f, btnY));
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("^")) {
            std::swap(m_Doc.layers[i], m_Doc.layers[i - 1]);
            if      (m_SelectedLayer == i)     m_SelectedLayer = i - 1;
            else if (m_SelectedLayer == i - 1) m_SelectedLayer = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 4.0f);
        ImGui::BeginDisabled(i == (int)m_Doc.layers.size() - 1);
        if (ImGui::SmallButton("v")) {
            std::swap(m_Doc.layers[i], m_Doc.layers[i + 1]);
            if      (m_SelectedLayer == i)     m_SelectedLayer = i + 1;
            else if (m_SelectedLayer == i + 1) m_SelectedLayer = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::Red);
        bool removeClicked = ImGui::SmallButton("x");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(rowP0.x, rowP1.y + kRowGap));
        ImGui::PopID();

        if (removeClicked) {
            m_Doc.layers.erase(m_Doc.layers.begin() + i);
            if (m_SelectedLayer == i)      m_SelectedLayer = -1;
            else if (m_SelectedLayer > i)  m_SelectedLayer--;
            break; // los indices cambiaron: no seguir iterando este frame
        }
    }
    // FIX (warning de Dear ImGui "Code uses SetCursorPos()/SetCursorScreenPos()
    // to extend window/parent boundaries. Please submit an item... afterwards"):
    // el SetCursorScreenPos() de arriba, en la ULTIMA fila, empuja el cursor
    // kRowGap por debajo de esa fila para "cerrar" la lista, pero nunca se
    // somete ningun item ahi -- todo lo demas de la fila (fondo/texto) se
    // dibuja directo con ImDrawList, no cuenta como item real para que este
    // child sepa que ese espacio es contenido de verdad. Un Dummy() invisible
    // en la posicion final confirma el limite inferior real.
    ImGui::Dummy(ImVec2(w, 0.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderPropertiesPanel — propiedades de la capa seleccionada, panel propio
//  a la derecha del canvas (separado de Capas, ver comentario en Render()).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderPropertiesPanel(float w, float h) {
    CanvaStyleEditor::SectionLabel("PROPIEDADES");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // Scrolleable -- esta seccion no tiene altura acotada (sombra/contorno/
    // fondo/etc pueden no entrar todos) y la ventana ya no scrollea como
    // conjunto (ver kFlags en Render()), asi que necesita su propio scroll
    // interno para no cortar controles contra el borde inferior.
    float propsH = std::max(80.0f, h - 32.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CanvaPalette::Surface1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::BeginChild("##ovPropsScroll", ImVec2(w, propsH), true);
    // Padding explicito -- ver mismo comentario en RenderLayersPanel: sin
    // esto los controles (dimensionados al "w" de afuera) se salian contra
    // el borde interno de este child bordeado.
    w = ImGui::GetContentRegionAvail().x;

    if (m_SelectedLayer < 0 || m_SelectedLayer >= (int)m_Doc.layers.size()) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Selecciona o crea una capa para editar sus propiedades.");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    auto& layer = m_Doc.layers[m_SelectedLayer];

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    constexpr ImGuiColorEditFlags kSwatchFlags = ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_AlphaPreviewHalf;

    // Opacidad general de la capa -- comun a las 4, independiente del alpha
    // de cada color propio (permite desvanecer un texto entero con sombra+
    // contorno+fondo, o una imagen, con un solo control).
    ImGui::TextUnformatted("Opacidad");
    ImGui::SetNextItemWidth(w);
    float opacityPct = layer.opacity * 100.0f;
    if (ImGui::DragFloat("##ovLayerOpacity", &opacityPct, 0.5f, 0.0f, 100.0f, "%.0f%%"))
        layer.opacity = std::clamp(opacityPct / 100.0f, 0.0f, 1.0f);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (layer.kind == OverlayLayerKind::Text) {
        char textBuf[512];
        size_t len = std::min(layer.text.size(), sizeof(textBuf) - 1);
        memcpy(textBuf, layer.text.c_str(), len);
        textBuf[len] = '\0';
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputTextMultiline("##ovLayerText", textBuf, sizeof(textBuf), ImVec2(w, 54.0f)))
            layer.text = textBuf;
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        RenderTextStyleProperties(layer, w);
    } else if (layer.kind == OverlayLayerKind::Clock) {
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Se reemplaza en vivo por el reloj/contador activo (ver panel Contadores).");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        RenderTextStyleProperties(layer, w);
    } else if (layer.kind == OverlayLayerKind::Image) {
        std::string fname = layer.imagePath;
        if (auto pos = fname.find_last_of("/\\"); pos != std::string::npos)
            fname = fname.substr(pos + 1);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("%s", fname.c_str());
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Tamano");
        ImGui::SetNextItemWidth(w);
        float sizePct[2] = { layer.sizeW * 100.0f, layer.sizeH * 100.0f };
        if (ImGui::DragFloat2("##ovLayerImgSize", sizePct, 0.5f, 2.0f, 100.0f, "%.0f%%")) {
            layer.sizeW = sizePct[0] / 100.0f;
            layer.sizeH = sizePct[1] / 100.0f;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Rotación");
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovLayerRotation", &layer.rotation, 0.5f, -180.0f, 180.0f, "%.0f grados");

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
        ImGui::TextWrapped("Tambien podes arrastrar las esquinas (tamano) o el "
                           "handle de arriba (rotación) en el canvas.");
        ImGui::PopStyleColor();
    } else { // Shape
        ImGui::TextUnformatted("Tipo de forma");
        ImGui::SetNextItemWidth(w);
        int shapeIdx = (layer.shapeKind == OverlayShapeKind::Ellipse) ? 1 : 0;
        const char* shapeNames[] = { "Rectangulo", "Elipse" };
        if (ImGui::BeginCombo("##ovShapeKind", shapeNames[shapeIdx])) {
            for (int s = 0; s < 2; s++) {
                bool sel = (shapeIdx == s);
                if (ImGui::Selectable(shapeNames[s], sel))
                    layer.shapeKind = (s == 1) ? OverlayShapeKind::Ellipse : OverlayShapeKind::Rectangle;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Tamano");
        ImGui::SetNextItemWidth(w);
        float sizePct[2] = { layer.sizeW * 100.0f, layer.sizeH * 100.0f };
        if (ImGui::DragFloat2("##ovShapeSize", sizePct, 0.5f, 2.0f, 100.0f, "%.0f%%")) {
            layer.sizeW = sizePct[0] / 100.0f;
            layer.sizeH = sizePct[1] / 100.0f;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextUnformatted("Rotación");
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovShapeRotation", &layer.rotation, 0.5f, -180.0f, 180.0f, "%.0f grados");

        // Selector rápido: cubre el caso pedido explicitamente ("solo el
        // borde con color y transparente adentro") en un click, en vez de
        // tener que descubrir que apagar Relleno + prender Borde por
        // separado logra lo mismo (los checkboxes de abajo siguen ahi para
        // ajustar color/ancho una vez elegido el estilo).
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::TextUnformatted("Estilo");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        {
            struct StyleOpt { const char* label; bool fill; bool outline; };
            const StyleOpt opts[3] = {
                { "Relleno",      true,  false },
                { "Solo borde",   false, true  },
                { "Ambos",        true,  true  },
            };
            float gap  = 6.0f;
            float btnW = (w - gap * 2.0f) / 3.0f;
            for (int i = 0; i < 3; i++) {
                bool active = (layer.shapeFilled == opts[i].fill && layer.outlineEnabled == opts[i].outline);
                if (i > 0) ImGui::SameLine(0.0f, gap);
                if (!active) ImGui::PushStyleColor(ImGuiCol_Button, CanvaPalette::Surface1);
                if (ImGui::Button(opts[i].label, ImVec2(btnW, 26.0f))) {
                    layer.shapeFilled    = opts[i].fill;
                    layer.outlineEnabled = opts[i].outline;
                    if (opts[i].outline && layer.outlineWidth <= 0.0f) layer.outlineWidth = 2.0f;
                }
                if (!active) ImGui::PopStyleColor();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Relleno", &layer.shapeFilled);
        if (layer.shapeFilled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovShapeFillColor", layer.color, kSwatchFlags);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Checkbox("Borde", &layer.outlineEnabled);
        if (layer.outlineEnabled) {
            ImGui::SameLine(w - 26.0f);
            ImGui::ColorEdit4("##ovShapeStrokeColor", layer.outlineColor, kSwatchFlags);
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovShapeStrokeWidth", &layer.outlineWidth, 0.2f, 0.5f, 20.0f, "%.1f px");
        }

        if (layer.shapeKind == OverlayShapeKind::Rectangle) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextUnformatted("Redondeo de esquinas");
            ImGui::SetNextItemWidth(w);
            ImGui::DragFloat("##ovShapeRounding", &layer.shapeRounding, 0.2f, 0.0f, 200.0f, "%.0f px");
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderTextStyleProperties — font/tamano/color/sombra/contorno/fondo,
//  compartido entre capas Text y Clock (ver RenderSidebar).
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderTextStyleProperties(OverlayLayer& layer, float w) {
    constexpr ImGuiColorEditFlags kSwatchFlags = ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_AlphaPreviewHalf;

    ImGui::SetNextItemWidth(w);
    if (ImGui::BeginCombo("##ovLayerFont", layer.fontName.c_str())) {
        if (m_FontList) {
            for (const auto& f : *m_FontList) {
                bool sel = (layer.fontName == f);
                if (ImGui::Selectable(f.c_str(), sel)) layer.fontName = f;
                if (sel) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::SetNextItemWidth(w);
    ImGui::DragFloat("##ovLayerSize", &layer.fontSize, 1.0f, 10.0f, 400.0f, "%.0f px");

    // Swatch de color "a lo Estilos": sin sliders RGBA inline, solo el
    // cuadradito que abre el picker completo en un popup al clickear.
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Color");
    ImGui::SameLine(w - 26.0f);
    ImGui::ColorEdit4("##ovLayerColor", layer.color, kSwatchFlags);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Checkbox("Sombra", &layer.shadowEnabled);
    if (layer.shadowEnabled) {
        ImGui::SameLine(w - 26.0f);
        ImGui::ColorEdit4("##ovShadowColor", layer.shadowColor, kSwatchFlags);
        ImGui::SetNextItemWidth(w);
        float shOff[2] = { layer.shadowOffsetX, layer.shadowOffsetY };
        if (ImGui::DragFloat2("##ovShadowOffset", shOff, 0.2f, -20.0f, 20.0f, "%.1f px")) {
            layer.shadowOffsetX = shOff[0];
            layer.shadowOffsetY = shOff[1];
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Checkbox("Contorno", &layer.outlineEnabled);
    if (layer.outlineEnabled) {
        ImGui::SameLine(w - 26.0f);
        ImGui::ColorEdit4("##ovOutlineColor", layer.outlineColor, kSwatchFlags);
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovOutlineWidth", &layer.outlineWidth, 0.2f, 0.5f, 20.0f, "%.1f px");
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Checkbox("Fondo", &layer.bgEnabled);
    if (layer.bgEnabled) {
        ImGui::SameLine(w - 26.0f);
        ImGui::ColorEdit4("##ovBgColor", layer.bgColor, kSwatchFlags);
        ImGui::SetNextItemWidth(w);
        float pad[2] = { layer.bgPaddingX, layer.bgPaddingY };
        if (ImGui::DragFloat2("##ovBgPadding", pad, 0.2f, 0.0f, 60.0f, "%.0f px")) {
            layer.bgPaddingX = pad[0];
            layer.bgPaddingY = pad[1];
        }
        ImGui::SetNextItemWidth(w);
        ImGui::DragFloat("##ovBgRounding", &layer.bgRounding, 0.2f, 0.0f, 40.0f, "%.0f redondeo");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderFooter — nombre + Cancelar/Guardar (dispara la captura a PNG)
// ─────────────────────────────────────────────────────────────────────────────
void OverlayCanvasEditor::RenderFooter(ImVec2 winPos, ImVec2 winSize,
                                       OnSaveCallback& onSave, OnCancelCallback& onClose) {
    constexpr float kFooterH = 52.0f;
    float footerY = winSize.y - kFooterH;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + footerY),
        CanvaPalette::ToU32(CanvaPalette::Border), 1.0f);
    dl->AddRectFilled(
        ImVec2(winPos.x, winPos.y + footerY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        CanvaPalette::ToU32(CanvaPalette::Surface1));

    ImGui::SetCursorPos(ImVec2(20.0f, footerY + (kFooterH - 36.0f) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::TextMuted);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Nombre del overlay:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);

    bool nameEmpty = (strlen(m_Name) == 0);
    if (nameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.90f, 0.30f, 0.30f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        CanvaPalette::Surface1);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, CanvaPalette::Surface2);
    ImGui::PushStyleColor(ImGuiCol_Text,           CanvaPalette::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("##ovNameInput", m_Name, sizeof(m_Name));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (nameEmpty) { ImGui::PopStyleVar(); ImGui::PopStyleColor(); }

    if (m_SaveFailed) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, CanvaPalette::Red);
        ImGui::Text("No se pudo guardar. Intenta de nuevo.");
        ImGui::PopStyleColor();
    } else if (nameEmpty) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::Text("El nombre es obligatorio");
        ImGui::PopStyleColor();
    }

    float btnGroupW = 110.0f + 8.0f + 170.0f;
    ImGui::SameLine(winSize.x - btnGroupW - 20.0f);

    if (CanvaStyleEditor::GhostButton("  Cancelar  ", ImVec2(110.0f, 36.0f))) {
        m_IsOpen = false;
        if (onClose) onClose();
    }

    ImGui::SameLine(0.0f, 8.0f);

    bool canSave = !nameEmpty;
    if (!canSave)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

    bool clickedSave = CanvaStyleEditor::PrimaryButton("  Guardar overlay  ", ImVec2(170.0f, 36.0f));

    if (!canSave)
        ImGui::PopStyleVar();

    if (clickedSave && canSave && m_CanvasWindowThisFrame) {
        m_SaveFailed = false;

        std::string name    = m_Name;
        std::string pngPath = m_ResolvePngPath ? m_ResolvePngPath(name) : std::string();
        OverlayDoc  docCopy = m_Doc;
        int         expW    = m_Doc.canvasW;
        int         expH    = m_Doc.canvasH;

        // Ventana invisible dedicada SOLO para conseguir un ImDrawList
        // correctamente inicializado via la API publica de ImGui (Begin/
        // GetWindowDrawList), en vez de armar uno a mano con internals
        // (fragil entre versiones de ImGui) -- se posiciona en el MISMO
        // lugar/tamano que el canvas real (necesario para que su clip rect
        // no recorte nada), pero sin fondo/inputs y solo dura este frame
        // (Guardar cierra el editor de inmediato despues).
        ImGui::SetNextWindowPos(m_CanvasScreenPos);
        ImGui::SetNextWindowSize(m_CanvasScreenSize);
        ImGui::Begin("##ovExportCapture", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        DrawLayersForExport(ImGui::GetWindowDrawList(), m_CanvasScreenPos, m_CanvasScreenSize);
        auto exportDl = std::make_shared<ImDrawList>(*ImGui::GetWindowDrawList());
        ImGui::End();

        // Captura por VALOR (no por referencia): onSave/onClose/name/docCopy
        // deben sobrevivir hasta ProcessPending() mas adelante en este mismo
        // frame, momento en el que este Render() ya retorno.
        OverlayExportService::Get().RequestCapture(
            exportDl, m_CanvasScreenPos, m_CanvasScreenSize,
            pngPath, expW, expH,
            [this, name, docCopy, onSave, onClose](bool ok) {
                if (ok) {
                    if (onSave) onSave(name, docCopy);
                    m_IsOpen = false;
                    if (onClose) onClose();
                } else {
                    m_SaveFailed = true;
                }
            });
    }
}

} // namespace ProyecThor::UI
