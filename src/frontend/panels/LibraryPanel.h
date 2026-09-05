#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <cstdint>
#include "frontend/views/Audio.h"
#include "frontend/views/DocumentView.h"
#include "frontend/views/OClock.h"
#include "IPanel.h"
#include "biblio/LibraryContext.h"
#include "biblio/LibraryMultimedia.h"
#include "backend/core/MediaConverter.h"
#include "overlay/OverlayLibraryTab.h"
#include "WebBrowserPanel.h"
#include "model3d/Model3DPanel.h"
#include "lab/LabPanel.h"
#include <memory>

namespace ProyecThor::UI { class UIManager; class MonitorView; }
enum class ActiveLeftPanel;

namespace ProyecThor::UI {

enum class LibraryCategory {
    Songs,
    Videos,
    Images,
    Bibles,
    Documents,
    Audio,
    // Vista unificada del sidebar: reemplaza los 3 botones Video/Imagen/Audio
    // por uno solo (ver LibraryMultimedia.h). Videos/Images/Audio de arriba
    // siguen existiendo para la resolucion de carpetas (RefreshList, import,
    // delete/rename) -- no son alcanzables desde el sidebar directamente.
    Multimedia
};

// ── Modos extra del sidebar de Biblioteca ────────────────────────────────────
// Items que antes vivian en secciones dedicadas del workspace o en paneles
// flotantes y que el operador pidió tener directamente en el rail izquierdo,
// separado de las categorias de contenido de arriba por una linea. No toca
// LibraryCategory/m_CurrentCategory -- es un modo de vista independiente.
enum class LibrarySideMode {
    Categories = 0,
    // Red y Mobile se mudaron a Ajustes > Conexiones (ver
    // CategoryConnections.cpp), junto con Streaming (RTMP) y OSC -- una
    // sola página para "todo lo que conecta ProyecThor con el exterior",
    // en vez de repartido entre aca y el rail de Conexiones (retirado).
    // "Reloj" (antes indice 2) se saco de aca -- ya vive en el toolbar
    // inline de ViewPanel (ver ViewPanel::InlineTool::Clock), duplicaba
    // el acceso.
    Render     = 3, // "Render" — conversor de formato (ver MediaConverter.h),
                     // mudado desde la sección "Biblioteca" del workspace
                     // (LibraryManagerPanel, retirada del todo).
    Overlay    = 4, // "Overlay" — galeria + editor de overlays PNG (ver
                     // OverlayLibraryTab), se abre a pantalla completa
                     // (UIManager::EnterFullscreenEditor) al crear/editar uno.
    Web        = 5, // "Web" — navegador embebido generico (ver WebBrowserPanel),
                     // con "Enviar a Público" para mostrar cualquier pagina en
                     // la salida real, no solo contenido de la Biblioteca.
    Model3D    = 6, // "3D" — visor y catálogo de modelos y recursos 3D (OBJ, STL, PLY, GLTF, GLB)
    Lab        = 7, // "Lab" — laboratorio matemático de fórmulas y graficador de funciones en vivo
};

class LibraryPanel : public IPanel {
public:
    LibraryPanel();
    ~LibraryPanel() override = default;

    std::string GetName() const override { return "Library"; }
    AudioPanel* GetAudioPanel() { return &m_AudioPanel; }
    void Render() override;
    void SetUIManager(UIManager* manager);
    void SetMonitorView(MonitorView* monitor) { m_MonitorRef = monitor; }

    // Preset de workspace "Biblioteca" (ver Settings::WorkspaceLayoutPreset::
    // Library / UIManager::BuildWorkspaceLayoutLibrary): bloquea Biblioteca
    // en la categoria Medios y oculta el selector de categorias del sidebar
    // (Canciones/Video/Biblia/Documentos/Reloj/Render/Overlay), asi el
    // operador solo ve la grilla de Medios -- nada mas para navegar a otro
    // lado por accidente en ese workspace reducido. UIManager la llama cada
    // frame segun el preset activo, no hace falta llamarla a mano.
    void SetMediaOnlyMode(bool v);

    // Preset de workspace "Render" (ver Settings::WorkspaceLayoutPreset::
    // Render / UIManager::BuildWorkspaceLayoutRender): bloquea Biblioteca en
    // el conversor de formato (LibrarySideMode::Render) y oculta el
    // sidebar -- pantalla completa dedicada solo a codificar/decodificar
    // video, sin nada mas para navegar a otro lado por accidente. UIManager
    // la llama cada frame segun el preset activo, no hace falta llamarla a
    // mano. Mutuamente excluyente con SetMediaOnlyMode (UIManager nunca
    // activa las dos a la vez, son presets distintos).
    void SetRenderOnlyMode(bool v);

    // Migrado tal cual desde LibraryManagerPanel (seccion "Biblioteca" del
    // workspace, retirada del todo) -- convierte Video/Audio ya importados a
    // otro formato aprovechando ffmpeg (ver MediaConverter.h). Publico
    // porque VideoEditorPanel lo llama directo como una de sus pestañas
    // (ver Settings::WorkspaceLayoutPreset::Video) -- el ex-preset "Render"
    // ya no bloquea toda la ventana de Biblioteca, ahora esto se embebe
    // inline en otro panel, misma instancia de LibraryPanel de siempre.
    void RenderConverterSection();

private:
    Library::LibraryContext BuildContext();

    void RefreshList();
    void CreateNewSong();
    void SaveSong(const std::string& title, const std::string& content, const std::string& author);
    void ImportFile();
    void DeleteSelectedItem();
    std::vector<std::string> LoadSongVerses(const std::string& filename);
    void LoadStreamURLs();
    void SaveStreamURLs();

    void ShowFileInUseToast(const std::string& fileName);
    void RenderFileInUseToast();

    // ── Render (conversor de formato, ver LibrarySideMode::Render) ───────
    // RenderConverterSection() ahora es publico, ver mas arriba.
    void RefreshConvertibleItems();

    LibraryCategory          m_CurrentCategory     = LibraryCategory::Multimedia;
    LibraryCategory          m_PrevCategory        = LibraryCategory::Multimedia;
    bool                     m_MediaOnlyMode       = false;
    bool                     m_RenderOnlyMode      = false;
    Library::MultimediaFilter m_MultimediaFilter   = Library::MultimediaFilter::All;
    // Flag: evita llamar SetSelection cada frame cuando estamos en Audio.
    // Solo se llama una vez al entrar a la categoria.
    bool                     m_AudioSelectionSet   = false;

    std::vector<std::string> m_Items;
    int                      m_SelectedIndex       = -1;
    char                     m_SearchBuffer[256]{};

    UIManager*               m_UIManagerRef        = nullptr;
    MonitorView*             m_MonitorRef          = nullptr;
    AudioPanel               m_AudioPanel;
    DocumentView              m_DocumentView;
    std::string              m_LoadedDocPath;

    // ── Grupo "Reloj" del sidebar (ver LibrarySideMode) ───────────────────
    // Mudado desde ViewToolsPanel: la propiedad de OClock (y el registro en
    // PresentationCore::SetOClockRef) se movio junto con el boton.
    LibrarySideMode  m_SideMode = LibrarySideMode::Categories;
    OClock           m_OClock;

    // ── Grupo "Overlay" del sidebar (ver LibrarySideMode) ─────────────────
    std::unique_ptr<OverlayLibraryTab> m_OverlayTab;

    // ── Grupo "Web" del sidebar (ver LibrarySideMode) ─────────────────────
    std::unique_ptr<WebBrowserPanel> m_WebBrowserPanel;

    // ── Grupo "3D" del sidebar (ver LibrarySideMode::Model3D) ────────────
    std::unique_ptr<Model3DPanel>    m_Model3DPanel;

    // ── Grupo "Lab" del sidebar (ver LibrarySideMode::Lab) ────────────────
    std::unique_ptr<LabPanel>         m_LabPanel;

    // ── Render (conversor de formato) ─────────────────────────────────────
    struct ConvertibleItem { std::string filename; bool isVideo; };
    std::vector<ConvertibleItem> m_ConvertibleItems; // Video + Audio juntos, para el combo de origen
    bool                          m_ConvertibleNeedsRefresh = true;

    int  m_ConvertSourceIndex = -1;
    int  m_ConvertFormatIndex = 0;

    // Codec forzado + compresion (0..100, ver MediaConverter::Start) --
    // solo aplica a conversiones de Video, se ignora para Audio. Arranca
    // en H264 (no Auto) para que la compresion sirva de entrada sin que el
    // usuario tenga que cambiar el codec primero.
    Core::VideoCodec m_ConvertCodec       = Core::VideoCodec::H264;
    int               m_ConvertCompression = 40;

    // Donde se guarda el archivo convertido: preguntar cada vez (dialogo
    // nativo "Guardar como", ver FilePicker::PickSaveVideoPath) o una
    // carpeta fija elegida una vez (FilePicker::PickFolder) y reusada sin
    // volver a preguntar -- el nombre de archivo se sigue auto-generando
    // (mismo criterio "nunca pisa un existente" de siempre) dentro de esa
    // carpeta.
    bool        m_ConvertAskEachTime  = true;
    std::string m_ConvertPresetFolder;

    Core::MediaConverter m_Converter;
    std::string          m_ConvertStatus;
    bool                 m_ConvertStatusIsError = false;

    // Tamaño de entrada/salida de la ULTIMA conversion arrancada -- para
    // poder mostrar "era X, quedo en Y" en el mensaje de estado una vez
    // termina (ver PollFinished en RenderConverterSection). 0 = desconocido.
    uint64_t    m_ConvertLastInputSize  = 0;
    std::string m_ConvertLastOutputPath;

    bool m_ShowSongEditor = false;
    char m_EditTitle  [256]{};
    char m_EditContent[8192]{};
    char m_EditAuthor [256]{};

    std::vector<std::string> m_StreamURLs;
    int                      m_SelectedURLIndex    = -1;
    char                     m_URLInputBuffer[512]{};

    bool        m_ShowRenameModal   = false;
    std::string m_RenameOldName;
    std::string m_RenameExtension;
    char        m_RenameBuffer[512]{};
    bool        m_RenameIsURL       = false;
    int         m_RenameURLIndex    = -1;
bool        m_ShowPlaylistsTab   = false;
    std::string m_ActivePlaylistName;
    int         m_ActivePlaylistIndex = -1;
    char        m_EditTags[256]{};

    void SelectPlaylistSong(const std::string& playlistName, int index);
    // ── Toast "archivo en uso" ───────────────────────────────────────────
    // Aviso temporal que aparece cuando DeleteSelectedItem() no logra
    // eliminar un archivo porque sigue bloqueado por otro subsistema
    // (reproduccion de video, documento cargado, etc.). Se dibuja con el
    // ForegroundDrawList directamente en Render(), asi que no depende de
    // estar dentro de ningun BeginChild/BeginWindow especifico y no
    // interfiere con el layout del panel.
    bool        m_ShowFileInUseToast  = false;
    std::string m_FileInUseToastName;
    float       m_FileInUseToastTimer = 0.0f;
};

} // namespace ProyecThor::UI