#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include "../BackgroundsPanel.h"
#include "backend/core/ThumbnailWorker.h"

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  LayersBgTab — toda la lógica del tab "Fondos"
//  Layout tipo ProPresenter: carpetas en una columna angosta a la izquierda,
//  contenido de la carpeta seleccionada en el area central. Toolbar compacta
//  arriba (solo iconos, sin titulos) con importar / nueva carpeta / zoom /
//  grid-lista.
// ─────────────────────────────────────────────────────────────────────────────
class LayersBgTab {
public:
    LayersBgTab();
    ~LayersBgTab() = default;

    // Punto de entrada: renderiza el contenido del tab (sin la barra de tabs)
    void Render();

    // Llamar externamente si se necesita forzar recarga
    void ReloadList();

private:
    // ── Datos ─────────────────────────────────────────────────────────────────
    std::vector<BgEntry>     m_AllBackgrounds;
    std::vector<std::string> m_BgFolders;
    std::string              m_CurrentBgFolder; // "" = raiz / "Todos"

    // Previene autoclick al cambiar de seleccion en el sidebar: se activa al
    // cambiar y se limpia tras el primer frame renderizado en la nueva vista
    bool  m_JustEnteredFolder = false;
    // Fade-in suave del contenido central al cambiar de carpeta (0..1)
    float m_ContentFade = 1.0f;

    // ── Thumbnails ────────────────────────────────────────────────────────────
    // Las miniaturas de video se generan en 2do plano (ThumbnailWorker, sin
    // abrir ninguna ventana) y se cachean en disco — solo se regeneran la
    // primera vez que se ve cada video, nunca en sesiones siguientes.
    std::unordered_map<std::string, ImTextureID> m_ThumbnailCache;
    Core::ThumbnailWorker                        m_ThumbWorker;
    ImTextureID GetThumbnail(const std::string& path, bool isVideo);
    void        DrainThumbnailResults(); // llamar una vez por frame desde Render()

    // ── Preview al mantener presionado ───────────────────────────────────────
    // Mientras el usuario mantiene el click sobre una tarjeta/fila, se
    // muestra en grande (sin aplicarlo) para que pueda verlo antes de
    // soltar. Se recalcula cada frame en RenderContentArea (se limpia al
    // empezar y la tarjeta/fila activa lo vuelve a fijar si sigue presionada).
    std::string m_HeldPreviewPath;
    bool        m_HeldPreviewIsVideo = false;
    void RenderHoldPreview();

    // ── Vistas ────────────────────────────────────────────────────────────────
    bool  m_GridMode  = true;
    float m_ThumbZoom = 1.0f; // 0.6 .. 1.8 — controla el tamano de las tarjetas

    // ── Estado de renombrado ──────────────────────────────────────────────────
    bool        m_RenamingBg      = false;
    std::string m_RenameOldPath;
    char        m_RenameBuf[256]  = {};

    bool        m_RenamingFolder  = false;
    std::string m_RenameFolderOld;
    char        m_RenameFolderBuf[128] = {};

    // ── Estado de creacion de carpeta ─────────────────────────────────────────
    bool m_CreatingFolder = false;
    char m_NewFolderBuf[128] = {};

    // ── Render helpers ────────────────────────────────────────────────────────
    void RenderTopBar();                          // toolbar compacta (icon-only) + zoom + grid/lista
    void RenderFolderSidebar(float w, float h);    // columna izquierda: "Todos" + carpetas
    void RenderSidebarItem(const std::string& label, const std::string& folderKey,
                           int count, bool selected, float w);
    // Alternativa a RenderFolderSidebar para columnas angostas (ver Render):
    // fila de chips que envuelve, en vez de una columna fija de 116px que le
    // resta ancho al contenido.
    void RenderFolderChips();
    void RenderContentArea(float w, float h);      // grid/lista de la seleccion actual

    void RenderBgCard(const BgEntry& e, float cardW, float cardH, int col, int cols);
    void RenderBgRow(const BgEntry& e, float panelW, float rowH);

    void SelectFolder(const std::string& folderKey);

    void BgContextMenu(const BgEntry& entry);
    void FolderContextMenu(const std::string& folderName);

    // Modales
    void RenderCreateFolderModal();
    void RenderRenameBgModal();
    void RenderRenameFolderModal();

    // ── Operaciones de disco ─────────────────────────────────────────────────
    bool ImportBackground();
    bool CreateBgFolder(const std::string& name);
    bool RenameBgFile(const std::string& oldPath, const std::string& newName);
    bool RenameBgFolder(const std::string& oldName, const std::string& newName);
    bool DeleteBgFile(const std::string& fullPath);
    bool DeleteBgFolder(const std::string& folderName);
    bool MoveBgToFolder(const std::string& srcFullPath, const std::string& destFolder);
};

} // namespace ProyecThor::UI
