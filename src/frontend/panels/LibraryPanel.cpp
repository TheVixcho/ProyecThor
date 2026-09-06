#include "LibraryPanel.h"

#include "biblio/LibraryHelpers.h"
#include "biblio/LibrarySidebar.h"
#include "biblio/LibrarySongs.h"
#include "biblio/LibraryVideos.h"
#include "biblio/LibraryDocuments.h"
#include "biblio/LibraryModals.h"
#include "frontend/views/audio/AudioHelpers.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#endif
#include <imgui.h>
#include <imgui_internal.h>

#include "backend/core/PresentationCore.h"
#include "backend/core/FileDeletionManager.h"
#include "UIStrings.h"
#include "frontend/ui/UIManager.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/FilePicker.h"
#include "ui/DesignSystem.h"
#include "biblio/LibraryPlaylists.h"
#include "frontend/panels/model3d/Model3DPanel.h"
#include "PanelPickerFullscreen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <iterator>
#include <thread>
#include <chrono>
#include <system_error>
#include <algorithm>

namespace fs = std::filesystem;

using namespace ProyecThor::Library;

static const std::string k_StreamURLsFile = "/stream_urls.txt";

namespace ProyecThor::UI {

namespace {

bool TryRemoveWithRetry(const fs::path& target, int maxAttempts = 8, int delayMs = 200)
{
    std::error_code ec;
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        fs::remove_all(target, ec);
        if (!ec)
            return true;

        if (attempt < maxAttempts - 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return false;
}

// -----------------------------------------------------------------------------
//  ImportSelectedFileToLibrary
//  Logica de copia compartida entre la rama Windows y la rama Linux de
//  ImportFile(). Recibe la ruta ya seleccionada por el usuario (via el dialogo
//  nativo en Windows o via zenity en Linux) y la copia a la carpeta que
//  corresponda segun la categoria actual de la biblioteca.
// -----------------------------------------------------------------------------
void ImportSelectedFileToLibrary(const fs::path& src, LibraryCategory category, const std::string& base)
{
    try {
        if (category == LibraryCategory::Documents) {
#ifdef _WIN32
            std::string docName = WideToUtf8(src.stem().wstring());
#else
            std::string docName = src.stem().string();
#endif
            fs::path docDir = U8Path(base + "/documents") / U8Path(docName);
            fs::create_directories(docDir);
            fs::copy(src, docDir / src.filename(),
                     fs::copy_options::overwrite_existing);
        } else if (category == LibraryCategory::Multimedia) {
            // El destino se decide por la extension del archivo elegido, no
            // por un filtro previo -- el dialogo de "Importar" en Multimedia
            // acepta cualquier tipo de los 3 (ver ImportFile()).
            std::string ext = src.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            static const std::vector<std::string> kVideoExts = { ".mp4", ".mkv", ".avi", ".mov" };
            static const std::vector<std::string> kImageExts = { ".jpg", ".jpeg", ".png" };

            std::string destFolder;
            if (std::find(kVideoExts.begin(), kVideoExts.end(), ext) != kVideoExts.end())
                destFolder = base + "/videos";
            else if (std::find(kImageExts.begin(), kImageExts.end(), ext) != kImageExts.end())
                destFolder = base + "/images";
            else
                destFolder = ProyecThor::Audio::GetAudioPath();

            fs::copy(src, U8Path(destFolder) / src.filename(),
                     fs::copy_options::overwrite_existing);
        } else {
            std::string destFolder;
            switch (category) {
                case LibraryCategory::Songs:  destFolder = base + "/songs";  break;
                case LibraryCategory::Videos: destFolder = base + "/videos"; break;
                case LibraryCategory::Images: destFolder = base + "/images"; break;
                case LibraryCategory::Bibles: destFolder = base + "/bibles"; break;
                default:                      destFolder = base + "/audio";  break;
            }
            fs::copy(src, U8Path(destFolder) / src.filename(),
                     fs::copy_options::overwrite_existing);
        }
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Error al importar: " << e.what() << '\n';
    }
}

// -----------------------------------------------------------------------------
//  SeedDefaultLibraryContent
//  Primer arranque (o biblioteca vaciada del todo): en vez de dejar Canciones
//  y Biblias completamente vacias, se copia ahi el contenido por defecto que
//  se distribuye con la app en bin/assets/<categoria> (relativo al ejecutable
//  — mismo criterio que main.cpp usa para iconos/fuentes bundleados, ver
//  "bin/assets/icons/ui/..." ahi). Solo copia si el destino esta VACIO: si el
//  operador ya tiene sus propias canciones/biblias, esto no toca nada; si las
//  borro todas a proposito, se vuelve a sembrar el default en el proximo
//  arranque (mismo comportamiento esperable que "restaurar contenido de
//  fabrica" al vaciar la carpeta).
// -----------------------------------------------------------------------------
void SeedDefaultLibraryContent(const std::string& base)
{
    static const char* kCategories[] = { "songs", "bibles" };

    for (const char* category : kCategories) {
        std::error_code ec;
        fs::path destDir = U8Path(base + "/" + category);
        if (!fs::is_empty(destDir, ec) || ec) continue; // tiene contenido (o no se pudo leer): no tocar

        fs::path srcDir = U8Path(std::string("bin/assets/") + category);
        if (!fs::exists(srcDir, ec) || !fs::is_directory(srcDir, ec)) continue;

        for (const auto& entry : fs::directory_iterator(srcDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            std::error_code copyEc;
            fs::copy_file(entry.path(), destDir / entry.path().filename(),
                          fs::copy_options::skip_existing, copyEc);
        }
    }
}

} // namespace

// =============================================================================
//  BuildContext
// =============================================================================
Library::LibraryContext LibraryPanel::BuildContext()
{
    return Library::LibraryContext{
        reinterpret_cast<int&>(m_CurrentCategory),
        reinterpret_cast<int&>(m_SideMode),
        m_Items,
        m_SelectedIndex,
        m_SearchBuffer,
        static_cast<int>(sizeof(m_SearchBuffer)),
        m_StreamURLs,
        m_SelectedURLIndex,
        m_URLInputBuffer,
        static_cast<int>(sizeof(m_URLInputBuffer)),
        m_ShowSongEditor,
        m_EditTitle,
        m_EditContent,
        m_EditAuthor,
        m_ShowRenameModal,
        m_RenameOldName,
        m_RenameExtension,
        m_RenameBuffer,
        m_RenameIsURL,
        m_RenameURLIndex,
        m_LoadedDocPath,
        m_MonitorRef,
        [this]() { RefreshList(); },
        [this]() { DeleteSelectedItem(); },
        [this]() { ImportFile(); },
        [this]() { CreateNewSong(); },
        [this](const std::string& t, const std::string& c, const std::string& a) { SaveSong(t, c, a); },
        [this]() { LoadStreamURLs(); },
        [this]() { SaveStreamURLs(); },
        [this](const std::string& f) { return LoadSongVerses(f); },
        [](const std::string& styleName) {
            Core::PresentationCore::Get().ApplyStyleByName(styleName);
        },
        m_ShowPlaylistsTab,
        m_ActivePlaylistName,
        m_ActivePlaylistIndex,
        []() { return Library::ListPlaylists(); },
        [](const std::string& name) { return Library::LoadPlaylist(name).songs; },
        [](const std::string& name) { return Library::CreatePlaylist(name); },
        [](const std::string& name) { Library::DeletePlaylist(name); },
        [](const std::string& a, const std::string& b) { return Library::RenamePlaylist(a, b); },
        [](const std::string& pl, const std::string& song) { Library::AddSongToPlaylist(pl, song); },
        [](const std::string& pl, int idx) { Library::RemoveSongFromPlaylist(pl, idx); },
        [](const std::string& pl, int idx, int delta) { Library::MovePlaylistSong(pl, idx, delta); },
        [this](const std::string& pl, int idx) { SelectPlaylistSong(pl, idx); },
        m_EditTags,
        [](const std::string& f) { return Library::GetSongTags(f); },
        [](const std::string& f, const std::vector<std::string>& t) { Library::SetSongTags(f, t); },
        [this]() { OpenPanelPickerFullscreen(); }
    };
}

// =============================================================================
//  Constructor
// =============================================================================
LibraryPanel::LibraryPanel()
{
    // Mudado desde ViewToolsPanel — ver PresentationCore::SetOClockRef y el
    // comentario de m_OClock en LibraryPanel.h.
    Core::PresentationCore::Get().SetOClockRef(&m_OClock);

    try {
        const std::string& base = GetAssetsPath();
        fs::create_directories(U8Path(base + "/songs"));
        fs::create_directories(U8Path(base + "/videos"));
        fs::create_directories(U8Path(base + "/images"));
        fs::create_directories(U8Path(base + "/bibles"));
        fs::create_directories(U8Path(base + "/documents"));
        fs::create_directories(U8Path(base + "/audio"));

        SeedDefaultLibraryContent(base);
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Advertencia IO: " << e.what() << '\n';
    }
    RefreshList();
    LoadStreamURLs();
}

LibraryPanel::~LibraryPanel() = default;

// El editor de Overlays necesita UIManager (para pedirle el modo pantalla
// completa, ver UIManager::EnterFullscreenEditor) -- se crea aca en vez de
// en el constructor porque SetUIManager corre despues (ver main.cpp).
void LibraryPanel::SetUIManager(UIManager* manager)
{
    m_UIManagerRef = manager;
    if (!m_OverlayTab && m_UIManagerRef)
        m_OverlayTab = std::make_unique<OverlayLibraryTab>(m_UIManagerRef);
    if (!m_WebBrowserPanel)
        m_WebBrowserPanel = std::make_unique<WebBrowserPanel>();
    if (!m_Model3DPanel) {
        m_Model3DPanel = std::make_unique<Model3DPanel>();
        m_Model3DPanel->SetUIManager(m_UIManagerRef);
    }
    if (!m_LabPanel) {
        m_LabPanel = std::make_unique<LabPanel>();
        m_LabPanel->SetUIManager(m_UIManagerRef);
    }
    if (!m_PanelPicker && m_UIManagerRef) {
        m_PanelPicker = std::make_unique<PanelPickerFullscreen>(this, m_UIManagerRef);
    }
}

void LibraryPanel::SetMediaOnlyMode(bool v)
{
    m_MediaOnlyMode = v;
    if (v)
    {
        m_CurrentCategory = LibraryCategory::Multimedia;
        m_PrevCategory    = LibraryCategory::Multimedia;
        m_SideMode        = LibrarySideMode::Categories;
    }
}

void LibraryPanel::SetRenderOnlyMode(bool v)
{
    m_RenderOnlyMode = v;
    if (v)
        m_SideMode = LibrarySideMode::Render;
}

void LibraryPanel::SelectCategory(LibraryCategory cat)
{
    m_CurrentCategory = cat;
    m_SideMode        = LibrarySideMode::Categories;
    m_SelectedIndex   = -1;
    RefreshList();
}

void LibraryPanel::SelectSideMode(LibrarySideMode mode)
{
    m_SideMode = mode;
}

void LibraryPanel::OpenPanelPickerFullscreen()
{
    if (m_UIManagerRef && m_PanelPicker) {
        m_PanelPicker->Open();
        m_UIManagerRef->EnterFullscreenEditor([this]() {
            if (m_PanelPicker) {
                m_PanelPicker->Render();
            }
        }, true);
    }
}

// =============================================================================
//  IO — URLs de streaming
// =============================================================================
void LibraryPanel::LoadStreamURLs()
{
    m_StreamURLs.clear();
    std::ifstream f(U8Path(GetAssetsPath() + k_StreamURLsFile));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.rfind("http", 0) == 0)
            m_StreamURLs.push_back(line);
    }
}

void LibraryPanel::SaveStreamURLs()
{
    std::ofstream f(U8Path(GetAssetsPath() + k_StreamURLsFile));
    if (!f.is_open()) return;
    for (const auto& u : m_StreamURLs) f << u << '\n';
}

// =============================================================================
//  RefreshList
// =============================================================================
void LibraryPanel::RefreshList()
{
    if (m_CurrentCategory == LibraryCategory::Multimedia) {
        m_Items.clear();
        Library::RefreshMultimediaLists();
        ForceListUpdate() = true;
        return;
    }

    if (m_CurrentCategory == LibraryCategory::Audio) {
        // Audio maneja su propio escaneo (AudioPanel::RefreshLibrary) en vez
        // de usar m_Items.
        m_Items.clear();
        ForceListUpdate() = true;
        return;
    }

    m_Items.clear();
    const std::string& base = GetAssetsPath();
    std::string path;
    switch (m_CurrentCategory) {
        case LibraryCategory::Songs:     path = base + "/songs";     break;
        case LibraryCategory::Videos:    path = base + "/videos";    break;
        case LibraryCategory::Images:    path = base + "/images";    break;
        case LibraryCategory::Bibles:    path = base + "/bibles";    break;
        case LibraryCategory::Documents: path = base + "/documents"; break;
        default: break;
    }

    try {
        fs::path fsPath = U8Path(path);
        if (fs::exists(fsPath)) {
            for (const auto& entry : fs::directory_iterator(fsPath)) {
#ifdef _WIN32
                std::string name = WideToUtf8(entry.path().filename().wstring());
#else
                std::string name = entry.path().filename().string();
#endif
                if (m_CurrentCategory == LibraryCategory::Documents) {
                    if (entry.is_directory()) m_Items.push_back(name);
                } else {
                    if (entry.is_regular_file()) m_Items.push_back(name);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[LibraryPanel] Error IO: " << e.what() << '\n';
    }

    if (m_Items.empty() && m_CurrentCategory == LibraryCategory::Songs)
        m_Items = { "Cuan_Grande_es_El.txt", "Gracia_Sublime.txt" };

    if (m_CurrentCategory == LibraryCategory::Videos)
        LoadStreamURLs();

    ForceListUpdate() = true;
}

// =============================================================================
//  DeleteSelectedItem
// =============================================================================
void LibraryPanel::DeleteSelectedItem()
{
    if (m_SelectedIndex < 0 || m_SelectedIndex >= (int)m_Items.size())
        return;

    const std::string& base = GetAssetsPath();
    std::string folder;
    switch (m_CurrentCategory) {
        case LibraryCategory::Songs:     folder = base + "/songs/";     break;
        case LibraryCategory::Videos:    folder = base + "/videos/";    break;
        case LibraryCategory::Images:    folder = base + "/images/";    break;
        case LibraryCategory::Documents: folder = base + "/documents/"; break;
        case LibraryCategory::Audio:     folder = base + "/audio/";     break;
        default:                         folder = base + "/bibles/";    break;
    }

    const std::string itemName = m_Items[m_SelectedIndex];
    const std::string fullPath = folder + itemName;

    auto& core = Core::PresentationCore::Get();
    auto  currentSelection = core.PeekSelection();

    const bool isDocumentInUse =
        (m_CurrentCategory == LibraryCategory::Documents) &&
        (!m_LoadedDocPath.empty()) &&
        (m_LoadedDocPath.rfind(fullPath, 0) == 0);

    const bool isCurrentlySelected =
        (currentSelection.title == itemName) || isDocumentInUse;

    if (isCurrentlySelected)
    {
        core.SetProjecting(false);
        core.ClearLayer2();
    }

    if (m_CurrentCategory == LibraryCategory::Documents)
        m_LoadedDocPath.clear();

    bool removed = false;
    if (m_CurrentCategory == LibraryCategory::Documents) {
        removed = Core::FileDeletionManager::ForceDeleteDirectory(fullPath);
    } else {
        removed = Core::FileDeletionManager::ForceDeleteFile(fullPath);
    }

    if (!removed)
    {
        std::cerr << "[LibraryPanel] No se pudo eliminar el archivo: "
                  << fullPath << '\n';
        ShowFileInUseToast(itemName);
        return;
    }

    m_SelectedIndex = -1;
    if (m_CurrentCategory == LibraryCategory::Documents)
        m_LoadedDocPath.clear();

    RefreshList();
}

// =============================================================================
//  ShowFileInUseToast — arma el aviso temporal
// =============================================================================
void LibraryPanel::ShowFileInUseToast(const std::string& fileName)
{
    m_ShowFileInUseToast  = true;
    m_FileInUseToastName  = fileName;
    m_FileInUseToastTimer = 3.5f;
}

// =============================================================================
//  RenderFileInUseToast — dibuja y hace desvanecer el aviso
// =============================================================================
void LibraryPanel::RenderFileInUseToast()
{
    if (!m_ShowFileInUseToast)
        return;

    m_FileInUseToastTimer -= ImGui::GetIO().DeltaTime;
    if (m_FileInUseToastTimer <= 0.0f)
    {
        m_ShowFileInUseToast = false;
        m_FileInUseToastName.clear();
        return;
    }

    const float k_FadeInOut = 0.4f;
    float alpha = 1.0f;
    if (m_FileInUseToastTimer < k_FadeInOut)
        alpha = m_FileInUseToastTimer / k_FadeInOut;

    std::string message = "No se pudo eliminar \"" + m_FileInUseToastName + "\": el archivo esta en uso.";

    ImGuiIO& io = ImGui::GetIO();
    ImVec2   displaySize = io.DisplaySize;

    ImFont* font = ImGui::GetFont();
    // Usamos ImGui::GetFontSize() en lugar de intentar obtenerlo del objeto font
    ImVec2 textSize = font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, message.c_str());

    const float padX = 16.0f;
    const float padY = 10.0f;
    const float boxW = textSize.x + padX * 2.0f;
    const float boxH = textSize.y + padY * 2.0f;
    const float marginBottom = 32.0f;

    ImVec2 boxMin(
        (displaySize.x - boxW) * 0.5f,
        displaySize.y - marginBottom - boxH
    );
    ImVec2 boxMax(boxMin.x + boxW, boxMin.y + boxH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImU32 bgColor   = IM_COL32(35, 15, 15, static_cast<int>(230 * alpha));
    ImU32 borderCol = IM_COL32(200, 70, 70, static_cast<int>(200 * alpha));
    ImU32 textCol   = IM_COL32(255, 220, 220, static_cast<int>(255 * alpha));

    dl->AddRectFilled(boxMin, boxMax, bgColor, 8.0f);
    dl->AddRect(boxMin, boxMax, borderCol, 8.0f, 0, 1.5f);

    ImVec2 textPos(boxMin.x + padX, boxMin.y + padY);
    dl->AddText(textPos, textCol, message.c_str());
}

// =============================================================================
//  Render — ahora envuelto en DS::BeginGlassPanel/EndGlassPanel
// =============================================================================

void LibraryPanel::Render()
{
    // ── Pump incondicional ──────────────────────────────────────────────────
    // Mudado desde ViewToolsPanel junto con m_OClock: debe seguir corriendo
    // aunque el operador este mirando otra categoria de Biblioteca (Reloj
    // alimenta LAN/pantalla), sin importar si el grupo Reloj esta activo
    // ahora.
    m_OClock.Update();

    // Alt Gr + 1: si Biblioteca esta colapsada (o pasando el punto medio de
    // la animacion), no dibujar la ventana ni su toolbar/sidebar -- el pump
    // de arriba ya corrio, asi que el Reloj sigue alimentando LAN/pantalla
    // igual que si el panel estuviera visible.
    if (m_UIManagerRef && m_UIManagerRef->IsPanelCollapsedForRender(GetName()))
        return;

    // Un archivo pudo haber cambiado de nombre en disco desde un lugar sin
    // acceso directo a este ctx (ver SongEditView::FlushIfDirty /
    // RenameNewSongToTitleIfApplicable) -- reescanea de verdad (RefreshList)
    // en vez de solo reordenar lo ya cargado, y sigue apuntando m_SelectedIndex
    // a la cancion actualmente seleccionada bajo su nombre nuevo.
    if (ForceLibraryRescan())
    {
        ForceLibraryRescan() = false;
        RefreshList();

        const std::string currentTitle = Core::PresentationCore::Get().PeekSelection().title;
        auto it = std::find(m_Items.begin(), m_Items.end(), currentTitle);
        m_SelectedIndex = (it != m_Items.end()) ? (int)std::distance(m_Items.begin(), it) : -1;
    }

    const auto& str = ProyecThor::UI::GetUIStrings();

    if (m_CurrentCategory != m_PrevCategory)
    {
        m_AudioSelectionSet = false;
        m_PrevCategory      = m_CurrentCategory;
        m_SearchBuffer[0]   = '\0';
        RefreshList();
    }
    
    ImGuiIO& io = ImGui::GetIO();
    // Los presets "Biblioteca"/"Render" bloquean la categoria/side-mode --
    // sin este guard, Shift+1..6 seguiria dejando saltar a Canciones/Video/
    // etc. en esos workspaces reducidos (ver SetMediaOnlyMode/SetRenderOnlyMode).
    if (io.KeyShift && !m_MediaOnlyMode && !m_RenderOnlyMode) // Solo si Shift está presionado
    {
        // Revisamos teclas del 1 al 6 (código ASCII '1' a '6')
        for (int i = 0; i < 6; ++i)
        {
            // FIX: Casteamos el entero resultante de vuelta a ImGuiKey
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)))
            {
                // Convertimos el índice 0-5 a tu enum LibraryCategory
                m_CurrentCategory = static_cast<LibraryCategory>(i);
                m_SideMode = LibrarySideMode::Categories;

                // Opcional: limpiar selección o refrescar al cambiar
                m_SelectedIndex = -1;
                RefreshList();
                break;
            }
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F8) && !io.KeyCtrl && !io.KeyAlt)
    {
        OpenPanelPickerFullscreen();
    }
    
    bool visible = false;

    if (m_UIManagerRef)
    {
        visible = DS::BeginGlassPanel(str.library, m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        visible = ImGui::Begin(str.library);
        ImGui::PopStyleVar();
    }

    if (!visible)
    {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        RenderFileInUseToast();
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && m_UIManagerRef)
        m_UIManagerRef->SetActiveLeftPanel(ActiveLeftPanel::Library);

    const float k_SidebarW = IconRailThickness(true);
    const float     totalH     = ImGui::GetContentRegionAvail().y;

    // Presets "Biblioteca"/"Render" (ver SetMediaOnlyMode/SetRenderOnlyMode):
    // sin sidebar de categorias -- solo hay una opcion posible, no tiene
    // sentido un selector. El contenido de abajo (ancho 0 = todo lo
    // disponible) ocupa automaticamente el espacio que el sidebar+divisor
    // hubieran usado.
    if (!m_MediaOnlyMode && !m_RenderOnlyMode)
    {
    // ── Sidebar izquierdo ──────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::BeginChild("##sidebar", ImVec2(k_SidebarW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    {
        Library::LibraryContext ctx = BuildContext();
        Library::RenderCategoryButtons(ctx);
    }

    ImGui::EndChild();

    // ── Divisor vertical con gradiente ────────────────────────────────────
    ImGui::SameLine(0.f, 0.f);
    {
        ImVec2      p  = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 colTop   = IM_COL32(60, 80, 160,  0);
        ImU32 colMid   = IM_COL32(60, 80, 160, 80);
        ImU32 colBot   = IM_COL32(60, 80, 160,  0);
        float midY     = p.y + totalH * 0.5f;
        dl->AddRectFilledMultiColor(
            p,              { p.x + 1.f, midY },
            colTop, colTop, colMid, colMid);
        dl->AddRectFilledMultiColor(
            { p.x, midY },  { p.x + 1.f, p.y + totalH },
            colMid, colMid, colBot, colBot);
    }
    ImGui::SameLine(0.f, 1.0f);
    }

    // ── Panel de contenido derecho ─────────────────────────────────────────
    // Margen unificado para TODAS las categorias (Canciones, Video, Documentos,
    // Audio). Centralizado aca para que ningun sub-panel (por ejemplo el grid
    // de Canciones/Playlists, que resetea su propio WindowPadding a 0 para
    // alinear columnas) pueda "comerse" el margen exterior del panel.
    constexpr float kContentMarginX = 18.0f;
    constexpr float kContentMarginY = 16.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kContentMarginX, kContentMarginY));
    ImGui::BeginChild("##content", ImVec2(0.f, totalH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    {
        Library::LibraryContext ctx = BuildContext();

        if (m_SideMode != LibrarySideMode::Web && m_WebBrowserPanel)
        {
            m_WebBrowserPanel->Hide();
        }

        if (m_SideMode == LibrarySideMode::Render)
        {
            RenderConverterSection();
        }
        else if (m_SideMode == LibrarySideMode::Overlay)
        {
            if (m_OverlayTab) m_OverlayTab->Render();
        }
        else if (m_SideMode == LibrarySideMode::Web)
        {
            if (m_WebBrowserPanel) m_WebBrowserPanel->Render();
        }
        else if (m_SideMode == LibrarySideMode::Model3D)
        {
            if (m_Model3DPanel) m_Model3DPanel->Render();
        }
        else if (m_SideMode == LibrarySideMode::Lab)
        {
            if (m_LabPanel) m_LabPanel->Render();
        }
        else if (m_CurrentCategory == LibraryCategory::Audio)
        {
            if (!m_AudioSelectionSet)
            {
                Core::LibrarySelection audioSel;
                audioSel.type  = Core::ItemType::Audio;
                audioSel.title = "Audio";
                Core::PresentationCore::Get().SetSelection(audioSel);
                m_AudioSelectionSet = true;
            }

            m_AudioPanel.RenderLibraryList();
        }
        else if (m_CurrentCategory == LibraryCategory::Multimedia)
        {
            Library::RenderMultimediaSection(ctx, m_MultimediaFilter);
        }
        else if (m_CurrentCategory == LibraryCategory::Videos)
        {
            Library::RenderVideoSection(ctx);
        }
        else if (m_CurrentCategory == LibraryCategory::Documents)
        {
            Library::RenderDocumentSection(ctx, m_DocumentView);
        }
        else
        {
            Library::RenderSideList(ctx);
        }

        Library::RenderRenameModal(ctx);
    }

    ImGui::EndChild();

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();

    RenderFileInUseToast();
}

// =============================================================================
//  Helpers — canciones
// =============================================================================
// Rework del editor: ya no abre un popup modal para pedir titulo/autor/
// contenido antes de crear el archivo (RenderSongEditor, retirado). En vez
// de eso, crea de una un archivo vacio con un nombre unico, lo selecciona, y
// pide (via el cue "consumir una vez" de PresentationCore) que SongView
// entre directo al editor unificado apenas la seleccion coincida —
// SongEditView permite renombrar el titulo visible desde adentro.
void LibraryPanel::CreateNewSong()
{
    const std::string base = "Nueva canción";
    std::string filename = base + ".txt";
    int suffix = 2;
    while (fs::exists(U8Path(GetAssetsPath() + "/songs/" + filename))) {
        filename = base + " (" + std::to_string(suffix) + ").txt";
        ++suffix;
    }

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open())
        f << "\xEF\xBB\xBF";
    f.close();

    RefreshList();

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = LoadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    auto it = std::find(m_Items.begin(), m_Items.end(), filename);
    if (it != m_Items.end())
        m_SelectedIndex = (int)std::distance(m_Items.begin(), it);

    Core::PresentationCore::Get().RequestSongEditorOpen(filename);
}

void LibraryPanel::SaveSong(const std::string& title, const std::string& content, const std::string& /*author*/)
{
    if (title.empty()) return;
    std::string filename = title;
    if (filename.find(".txt") == std::string::npos) filename += ".txt";

    std::ofstream f(U8Path(GetAssetsPath() + "/songs/" + filename));
    if (f.is_open()) {
        f << "\xEF\xBB\xBF";
        f << content;
        RefreshList();
    }
}

// =============================================================================
//  LoadSongVerses — delega en Library::LoadSongVerses (LibrarySongs.cpp), que
//  es la unica implementacion real (antes estaba duplicada aca). Se mantiene
//  este metodo (en vez de que los llamadores usen la funcion libre
//  directamente) para no tocar el wiring existente de ctx.loadSongVerses ni
//  la llamada de SelectPlaylistSong mas abajo.
// =============================================================================
std::vector<std::string> LibraryPanel::LoadSongVerses(const std::string& filename)
{
    return Library::LoadSongVerses(filename);
}

// =============================================================================
//  ImportFile
//  Multiplataforma: en Windows abre el dialogo nativo (OPENFILENAMEW). En
//  Linux invoca "zenity --file-selection" (requiere tener zenity instalado
//  en el sistema). En ambos casos, una vez elegido el archivo, la copia a la
//  carpeta correspondiente se hace con ImportSelectedFileToLibrary, que es
//  identica para las dos plataformas.
// =============================================================================
void LibraryPanel::SelectPlaylistSong(const std::string& playlistName, int index)
{
    Library::Playlist pl = Library::LoadPlaylist(playlistName);
    if (index < 0 || index >= (int)pl.songs.size()) return;

    const std::string& filename = pl.songs[index];

    Core::LibrarySelection s;
    s.title       = filename;
    s.type        = Core::ItemType::Song;
    s.contentData = LoadSongVerses(filename);
    Core::PresentationCore::Get().SetSelection(s);

    std::string defaultStyle =
        Core::PresentationCore::Get().GetCategoryDefaultStyle(Core::ItemType::Song);
    if (!defaultStyle.empty())
        Core::PresentationCore::Get().ApplyStyleByName(defaultStyle);

    m_ActivePlaylistName  = playlistName;
    m_ActivePlaylistIndex = index;

    auto it = std::find(m_Items.begin(), m_Items.end(), filename);
    if (it != m_Items.end())
        m_SelectedIndex = (int)std::distance(m_Items.begin(), it);
}

void LibraryPanel::ImportFile()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(65536, 0);
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;

    if      (m_CurrentCategory == LibraryCategory::Videos)
        ofn.lpstrFilter = L"Videos\0*.mp4;*.mkv;*.avi;*.mov\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Images)
        ofn.lpstrFilter = L"Imágenes\0*.jpg;*.png;*.jpeg\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Multimedia)
        ofn.lpstrFilter = L"Video, audio o imagen\0*.mp4;*.mkv;*.avi;*.mov;*.mp3;*.flac;*.wav;*.ogg;*.aac;*.m4a;*.wma;*.opus;*.aiff;*.jpg;*.jpeg;*.png\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Songs)
        ofn.lpstrFilter = L"Textos\0*.txt\0Todos\0*.*\0";
    else if (m_CurrentCategory == LibraryCategory::Documents)
        ofn.lpstrFilter = L"Documentos\0*.pdf;*.pptx;*.ppt;*.odp\0Todos\0*.*\0";
    else
        ofn.lpstrFilter = L"Todos los archivos\0*.*\0";

    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile  = static_cast<DWORD>(buffer.size());
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&ofn)) return;

    const wchar_t* p = buffer.data();
    std::wstring first(p);
    p += first.length() + 1;

    if (*p == 0) {
        // Solo un archivo seleccionado
        fs::path src(first);
        ImportSelectedFileToLibrary(src, m_CurrentCategory, GetAssetsPath());
    } else {
        // Multiples archivos: 'first' es el directorio base
        fs::path dir(first);
        while (*p != 0) {
            std::wstring filename(p);
            fs::path src = dir / filename;
            ImportSelectedFileToLibrary(src, m_CurrentCategory, GetAssetsPath());
            p += filename.length() + 1;
        }
    }
#else
    std::string filter;
    switch (m_CurrentCategory) {
        case LibraryCategory::Videos:
            filter = "--file-filter=Videos | *.mp4 *.mkv *.avi *.mov";
            break;
        case LibraryCategory::Images:
            filter = "--file-filter=Imágenes | *.jpg *.jpeg *.png";
            break;
        case LibraryCategory::Multimedia:
            filter = "--file-filter=Video, audio o imagen | *.mp4 *.mkv *.avi *.mov "
                     "*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma *.opus *.aiff *.jpg *.jpeg *.png";
            break;
        case LibraryCategory::Songs:
            filter = "--file-filter=Textos | *.txt";
            break;
        case LibraryCategory::Documents:
            filter = "--file-filter=Documentos | *.pdf *.pptx *.ppt *.odp";
            break;
        default:
            filter = "--file-filter=Todos | *";
            break;
    }

    std::string command = "zenity --file-selection --multiple --separator=\"|\" --title=\"Importar archivos\" \"" +
                          filter + "\" 2>/dev/null";

    std::string result;
    char buffer[4096];
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "[LibraryPanel] No se pudo abrir el selector de archivos (zenity).\n";
        return;
    }
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;
    int status = pclose(pipe);

    if (status != 0 || result.empty()) return;
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    if (result.empty()) return;

    size_t start = 0, end = 0;
    while ((end = result.find('|', start)) != std::string::npos) {
        std::string pathStr = result.substr(start, end - start);
        if (!pathStr.empty()) {
            fs::path src(pathStr);
            ImportSelectedFileToLibrary(src, m_CurrentCategory, GetAssetsPath());
        }
        start = end + 1;
    }
    if (start < result.size()) {
        std::string pathStr = result.substr(start);
        if (!pathStr.empty()) {
            fs::path src(pathStr);
            ImportSelectedFileToLibrary(src, m_CurrentCategory, GetAssetsPath());
        }
    }
#endif

    RefreshList();
}

// =============================================================================
//  Render (conversor de formato) — migrado tal cual desde LibraryManagerPanel
//  (sección "Biblioteca" del workspace, retirada del todo: ver LibrarySideMode
//  ::Render en LibraryPanel.h y el grupo "Red"/"Reloj"/"Render" del sidebar en
//  LibrarySidebar.cpp). Convierte Video/Audio ya importados a otro formato
//  aprovechando ffmpeg (ver MediaConverter.h) — Video vive en assets/videos,
//  Audio en la carpeta que devuelve ProyecThor::Audio::GetAudioPath().
// =============================================================================
void LibraryPanel::RefreshConvertibleItems()
{
    m_ConvertibleItems.clear();

    auto scan = [&](const std::string& dirPath, const std::vector<std::string>& exts, bool isVideo) {
        fs::path dir = U8Path(dirPath);
        std::error_code ec;
        if (!fs::exists(dir, ec) || ec) return;

        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string lowExt = entry.path().extension().string();
            std::transform(lowExt.begin(), lowExt.end(), lowExt.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (std::find(exts.begin(), exts.end(), lowExt) == exts.end()) continue;

            m_ConvertibleItems.push_back({ PathToUtf8(entry.path().filename()), isVideo });
        }
    };
    scan(GetAssetsPath() + "/videos/", { ".mp4", ".mkv", ".avi", ".mov" }, true);
    scan(ProyecThor::Audio::GetAudioPath() + "/",
        { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" }, false);

    std::sort(m_ConvertibleItems.begin(), m_ConvertibleItems.end(),
              [](const ConvertibleItem& a, const ConvertibleItem& b) { return a.filename < b.filename; });

    if (m_ConvertSourceIndex >= (int)m_ConvertibleItems.size()) m_ConvertSourceIndex = -1;
    m_ConvertibleNeedsRefresh = false;
}

// Estilo de combo/frame compartido con la barra de busqueda de Multimedia
// (ver RenderMultimediaSection en LibraryMultimedia.cpp) -- mismo look en
// toda la biblioteca en vez del combo gris por defecto de ImGui.
static void PushConverterFrameStyle() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}
static void PopConverterFrameStyle() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
}

// Estimacion GRUESA (no exacta -- no hay forma de saberlo sin codificar de
// verdad) de que fraccion del tamaño de entrada va a pesar el archivo
// convertido, segun el codec elegido y el % de compresion -- basado en
// relaciones tipicas de eficiencia entre codecs a calidad comparable
// (H.265/VP9 suelen pesar 30-40% menos que H.264 equivalente, AV1 otro
// 15-20% menos que esos) interpoladas contra el rango de CRF de cada uno
// (ver BuildVideoCodecArgs, MediaConverter.cpp). Sirve de referencia para
// que el usuario tenga una idea antes de convertir -- el tamaño real
// depende del contenido del video y puede variar bastante.
static float EstimateSizeRatio(Core::VideoCodec codec, int compression) {
    float t = std::clamp(compression, 0, 100) / 100.0f;
    switch (codec) {
        case Core::VideoCodec::H264: return 0.90f - t * (0.90f - 0.32f);
        case Core::VideoCodec::H265: return 0.72f - t * (0.72f - 0.24f);
        case Core::VideoCodec::VP9:  return 0.68f - t * (0.68f - 0.22f);
        case Core::VideoCodec::AV1:  return 0.58f - t * (0.58f - 0.18f);
        case Core::VideoCodec::Auto:
        default: return 1.0f;
    }
}

static std::string FormatFileSize(uint64_t bytes) {
    static const char* kUnits[] = { "B", "KB", "MB", "GB" };
    double b = (double)bytes;
    int u = 0;
    while (b >= 1024.0 && u < 3) { b /= 1024.0; u++; }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1f %s", b, kUnits[u]);
    return buf;
}

void LibraryPanel::RenderConverterSection()
{
    if (m_ConvertibleNeedsRefresh) RefreshConvertibleItems();

    // Titulo + descripcion envuelta -- antes iban en la misma linea
    // (SameLine) y la descripcion se cortaba contra el borde del panel en
    // ventanas angostas (ver reporte del usuario, "esta cortado").
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted("Render");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX());
    ImGui::TextUnformatted("Convierte Video o Audio ya importados a otro formato.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    DS::GlassSeparator();
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Si termino una conversion desde el ultimo frame, actualizar estado.
    bool        finishedOk = false;
    std::string finishedMsg;
    if (m_Converter.PollFinished(finishedOk, finishedMsg)) {
        m_ConvertStatusIsError = !finishedOk;
        m_ConvertStatus        = finishedMsg;

        // Comparacion EXACTA (a diferencia de la estimacion previa a
        // convertir, esto ya es el archivo real) -- se agrega al mensaje
        // de exito si se pudo leer el tamaño de ambos archivos.
        if (finishedOk && m_ConvertLastInputSize > 0) {
            std::error_code sizeEc;
            uint64_t outSize = fs::file_size(U8Path(m_ConvertLastOutputPath), sizeEc);
            if (!sizeEc) {
                double pct = 100.0 * (1.0 - (double)outSize / (double)m_ConvertLastInputSize);
                char suffix[128];
                std::snprintf(suffix, sizeof(suffix), " (%s, %s%.0f%% vs %s)",
                              FormatFileSize(outSize).c_str(), pct >= 0 ? "-" : "+",
                              std::fabs(pct), FormatFileSize(m_ConvertLastInputSize).c_str());
                m_ConvertStatus += suffix;
            }
        }

        m_ConvertibleNeedsRefresh = true; // por si el archivo convertido cae en la misma carpeta
    }

    if (m_ConvertibleItems.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextUnformatted("Todavia no importaste ningun Video o Audio para convertir.");
        ImGui::PopStyleColor();
        return;
    }

    bool running = m_Converter.IsRunning();
    if (running) ImGui::BeginDisabled();

    // ── Origen ────────────────────────────────────────────────────────────
    DS::GlassSectionHeader("ARCHIVO DE ORIGEN");
    std::string sourcePreview = (m_ConvertSourceIndex >= 0 && m_ConvertSourceIndex < (int)m_ConvertibleItems.size())
        ? m_ConvertibleItems[m_ConvertSourceIndex].filename : "Elegi un archivo...";

    PushConverterFrameStyle();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##convertSource", sourcePreview.c_str())) {
        for (int i = 0; i < (int)m_ConvertibleItems.size(); i++) {
            const auto& item = m_ConvertibleItems[i];
            std::string label = std::string(item.isVideo ? "[Video] " : "[Audio] ") + item.filename;
            bool sel = (i == m_ConvertSourceIndex);
            if (ImGui::Selectable(label.c_str(), sel)) {
                m_ConvertSourceIndex = i;
                m_ConvertFormatIndex = 0;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    PopConverterFrameStyle();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // ── Formato de destino ───────────────────────────────────────────────
    static const char* kVideoFormats[] = { "mp4", "mkv", "webm", "avi", "mov" };
    static const char* kAudioFormats[] = { "mp3", "wav", "flac", "ogg", "aac", "m4a" };

    const char** formats     = kVideoFormats;
    int          formatCount = (int)(sizeof(kVideoFormats) / sizeof(kVideoFormats[0]));
    bool         haveSource  = (m_ConvertSourceIndex >= 0 && m_ConvertSourceIndex < (int)m_ConvertibleItems.size());
    if (haveSource && !m_ConvertibleItems[m_ConvertSourceIndex].isVideo) {
        formats     = kAudioFormats;
        formatCount = (int)(sizeof(kAudioFormats) / sizeof(kAudioFormats[0]));
    }
    if (m_ConvertFormatIndex >= formatCount) m_ConvertFormatIndex = 0;

    DS::GlassSectionHeader("FORMATO DE DESTINO");
    PushConverterFrameStyle();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##convertFormat", haveSource ? formats[m_ConvertFormatIndex] : "-")) {
        for (int i = 0; i < formatCount; i++) {
            bool sel = (i == m_ConvertFormatIndex);
            if (ImGui::Selectable(formats[i], sel)) m_ConvertFormatIndex = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    PopConverterFrameStyle();

    // ── Codec + compresion (solo tiene sentido para Video) ─────────────────
    bool isVideoSource = haveSource && m_ConvertibleItems[m_ConvertSourceIndex].isVideo;
    if (isVideoSource) {
        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        DS::GlassSectionHeader("CODEC");
        static const char* kCodecLabels[] = {
            "Automático (sin recodificar)", "H.264", "H.265 (mas compresion)", "VP9", "AV1 (mas compresion, mas lento)"
        };
        constexpr int kCodecCount = (int)(sizeof(kCodecLabels) / sizeof(kCodecLabels[0]));
        int codecIdx = (int)m_ConvertCodec;
        PushConverterFrameStyle();
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("##convertCodec", kCodecLabels[codecIdx])) {
            for (int i = 0; i < kCodecCount; i++) {
                bool sel = (i == codecIdx);
                if (ImGui::Selectable(kCodecLabels[i], sel)) m_ConvertCodec = (Core::VideoCodec)i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        PopConverterFrameStyle();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        DS::GlassSectionHeader("COMPRESION");
        bool codecIsAuto = (m_ConvertCodec == Core::VideoCodec::Auto);
        if (codecIsAuto) ImGui::BeginDisabled();
        PushConverterFrameStyle();
        ImGui::SetNextItemWidth(260.0f);
        ImGui::SliderInt("##convertCompression", &m_ConvertCompression, 0, 100, "%d%%");
        PopConverterFrameStyle();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX());
        ImGui::TextUnformatted("Menos = mejor calidad y archivo mas pesado. Mas = mas liviano y menor calidad.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Estimacion de peso -- se calcula sobre el tamaño REAL del archivo
        // de origen (ver EstimateSizeRatio, es aproximado a proposito).
        std::error_code sizeEc;
        fs::path srcPath = U8Path(GetAssetsPath() + "/videos/" + m_ConvertibleItems[m_ConvertSourceIndex].filename);
        uint64_t srcSize = fs::file_size(srcPath, sizeEc);
        if (!sizeEc && srcSize > 0) {
            float ratio = codecIsAuto ? 1.0f : EstimateSizeRatio(m_ConvertCodec, m_ConvertCompression);
            uint64_t estSize = (uint64_t)((double)srcSize * ratio);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            ImGui::Text("Peso estimado: ~%s (original: %s)",
                        FormatFileSize(estSize).c_str(), FormatFileSize(srcSize).c_str());
            ImGui::PopStyleColor();
        }
        if (codecIsAuto) ImGui::EndDisabled();
    }

    // ── Donde guardar ────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
    DS::GlassSectionHeader("GUARDAR EN");
    // Apilados verticalmente (no SameLine): con el panel angosto de
    // Biblioteca, "Preguntar cada vez" + "Carpeta fija" en una sola linea
    // no entraban y "Carpeta fija" quedaba cortado contra el borde.
    if (ImGui::RadioButton("Preguntar cada vez", m_ConvertAskEachTime)) m_ConvertAskEachTime = true;
    if (ImGui::RadioButton("Carpeta fija", !m_ConvertAskEachTime)) m_ConvertAskEachTime = false;

    if (!m_ConvertAskEachTime) {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        char folderBuf[512];
        std::snprintf(folderBuf, sizeof(folderBuf), "%s",
                       m_ConvertPresetFolder.empty() ? "Sin elegir..." : m_ConvertPresetFolder.c_str());
        PushConverterFrameStyle();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 96.0f);
        ImGui::InputText("##convertPresetFolder", folderBuf, sizeof(folderBuf), ImGuiInputTextFlags_ReadOnly);
        PopConverterFrameStyle();
        ImGui::SameLine();
        if (ImGui::Button("Elegir...", ImVec2(86.0f, 0.0f))) {
            std::string chosen = UI::PickFolder("Elegir carpeta de salida para Render");
            if (!chosen.empty()) m_ConvertPresetFolder = chosen;
        }
        if (m_ConvertPresetFolder.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextUnformatted("Elegi una carpeta -- si no, se pregunta igual al convertir.");
            ImGui::PopStyleColor();
        }
    }

    if (running) ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    if (!m_ConvertStatus.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX());
        ImGui::TextColored(m_ConvertStatusIsError ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.40f, 0.85f, 0.55f, 1.0f),
                            "%s", m_ConvertStatus.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
    }

    if (running) {
        float progress = m_Converter.GetProgress();
        if (progress >= 0.0f) {
            char overlay[32];
            std::snprintf(overlay, sizeof(overlay), "Convirtiendo... %.0f%%", progress * 100.0f);
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), overlay);
        } else {
            // Duracion total todavia desconocida (recien arrancando, o el
            // archivo no la reporta) -- idioma estandar de ImGui para una
            // barra indeterminada: fraccion negativa animada con el tiempo.
            ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1.0f, 0.0f), "Convirtiendo...");
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.58f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.68f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button("Cancelar", ImVec2(160.0f, DS::ButtonHeight))) m_Converter.Cancel();
        ImGui::PopStyleColor(3);
        return;
    }

    if (!haveSource) { ImGui::BeginDisabled(); }
    if (DS::GlassButton("Convertir", ImVec2(200.0f, DS::ButtonHeight + 6.0f)) && haveSource) {
        const auto& src     = m_ConvertibleItems[m_ConvertSourceIndex];
        std::string dirPath = src.isVideo ? (GetAssetsPath() + "/videos/") : (ProyecThor::Audio::GetAudioPath() + "/");
        std::string stem    = StripExtension(src.filename);
        std::string ext     = formats[m_ConvertFormatIndex];
        std::string inputPath = dirPath + src.filename;

        std::string outputPath;
        bool        cancelled = false;

        if (m_ConvertAskEachTime || m_ConvertPresetFolder.empty()) {
            // Dialogo nativo "Guardar como" (ver FilePicker::
            // PickSaveVideoPath) -- sugiere el mismo nombre/carpeta que
            // antes por defecto, pero el usuario puede elegir cualquier
            // otro destino. Sin carpeta fija elegida, este es tambien el
            // fallback (ver hint en la UI).
            std::string suggested = dirPath + stem + "." + ext;
            outputPath = UI::PickSaveVideoPath(suggested);
            cancelled  = outputPath.empty();
        } else {
            // Carpeta fija: mismo criterio de nombre único "nunca pisa un
            // archivo existente" que antes, pero resuelto contra esa
            // carpeta en vez de la carpeta de origen.
            fs::path    presetDir = U8Path(m_ConvertPresetFolder);
            std::string outName   = stem + "." + ext;
            int suffix = 2;
            std::error_code ec;
            while (fs::exists(presetDir / U8Path(outName), ec)) {
                outName = stem + " (" + std::to_string(suffix) + ")." + ext;
                suffix++;
            }
            outputPath = PathToUtf8(presetDir / U8Path(outName));
        }

        if (!cancelled) {
            // El codec/compresion elegidos solo aplican a conversiones de
            // Video -- para Audio se manda Auto (ffmpeg infiere, igual que
            // siempre).
            Core::VideoCodec codec = src.isVideo ? m_ConvertCodec : Core::VideoCodec::Auto;

            std::string err;
            if (m_Converter.Start(inputPath, outputPath, codec, m_ConvertCompression, &err)) {
                m_ConvertStatusIsError = false;
                m_ConvertStatus        = "Convirtiendo a " + PathToUtf8(U8Path(outputPath).filename()) + "...";

                std::error_code sizeEc;
                m_ConvertLastInputSize  = fs::file_size(U8Path(inputPath), sizeEc);
                if (sizeEc) m_ConvertLastInputSize = 0;
                m_ConvertLastOutputPath = outputPath;
            } else {
                m_ConvertStatusIsError = true;
                m_ConvertStatus        = err;
            }
        }
    }
    if (!haveSource) { ImGui::EndDisabled(); }
}

} // namespace ProyecThor::UI
