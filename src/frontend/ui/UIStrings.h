#pragma once
#include "SettingsManager.h"

namespace ProyecThor::UI {

    struct UIStrings {
        // ── Generales ────────────────────────────────────────────────────────
        const char* appTitle;
        const char* close;
        const char* save;
        const char* reset;
        const char* cancel;
        const char* edit;
        const char* deleteLabel;
        const char* importLabel;
        const char* newLabel;
        const char* refresh;
        const char* noSignal;

        // ── Paneles (Docking) ─────────────────────────────────────────────────
        const char* library;
        const char* preview;
        const char* inspector;
        const char* control;

        // ── Menú Superior ─────────────────────────────────────────────────────
        const char* menuFile;
        const char* menuExit;
        const char* menuEdit;
        const char* menuPrefs;
        const char* menuView;
        const char* menuResetLayout;
        const char* menuHelp;
        const char* menuDocs;
        const char* menuDonations;
        const char* menuAbout;

        // ── Modal "Acerca de" ─────────────────────────────────────────────────
        const char* aboutDesc;
        const char* aboutNonProfit;

        // ── LibraryPanel ──────────────────────────────────────────────────────
        const char* libCatSongs;
        const char* libCatVideos;
        const char* libCatImages;
        const char* libCatBible;
        const char* libCatDocuments;
        const char* libSearchHint;
        const char* libNoPlayer;

        // ── BibleView ─────────────────────────────────────────────────────────
        const char* bibleLabel;
        const char* bibleBooks;
        const char* bibleChapters;
        const char* bibleSearchHint;
        const char* bibleClearHistory;
        const char* bibleNoFile;
        const char* bibleEditVerse;
        const char* bibleSaveXML;
        const char* bibleSection_books;
        const char* bibleSection_chapters;

        // ── SongView ──────────────────────────────────────────────────────────
        const char* songLyricsDeck;
        const char* songClearScreen;
        const char* songEditSong;

        // ── MediaView ─────────────────────────────────────────────────────────
        const char* mediaNoSelection;
        const char* mediaProjectImage;
        const char* mediaVideoReady;
        const char* mediaVideoHint;

        // ── TransitionPanel ───────────────────────────────────────────────────
        const char* transTitle;
        const char* transNone;
        const char* transFade;
        const char* transZoomIn;
        const char* transZoomOut;
        const char* transDuration;
        const char* transTip;

        // ── DocumentView ──────────────────────────────────────────────────────
        const char* docNoPages;
        const char* docTitle;
        const char* docPage;
        const char* docOf;
        const char* docPrev;
        const char* docNext;
        const char* docProject;

        // ── MonitorView ───────────────────────────────────────────────────────
        const char* monPreviewLabel;
        const char* monPreviewNoSignal;
        const char* monLiveLabel;
        const char* monLiveNoSignal;
        const char* monTransmit;
        const char* monPreviewAudioOff;
        const char* monMute;
        const char* monStop;

        // ── ControlPanel ──────────────────────────────────────────────────────
        const char* ctrlQuickControls;
        const char* ctrlRemoveText;
        const char* ctrlStopVideo;
        const char* ctrlOneScreen;
        const char* ctrlScreenCount;
        const char* ctrlOutput;
        const char* ctrlStartProjection;
        const char* ctrlStopProjection;
        const char* ctrlProjecting;
        const char* ctrlInactive;

        // ── LayersPanel ───────────────────────────────────────────────────────
        const char* layersBgTab;
        const char* layersStyleTab;
        const char* layersBgTitle;
        const char* layersBgReload;
        const char* layersBgEmpty;
        const char* layersNewStyle;
        const char* layersReloadFonts;
        const char* layersNoStyles;
        const char* layersQuickSettings;
        const char* layersFont;
        const char* layersTextColor;
        const char* layersTextSize;
        const char* layersHAlign;
        const char* layersVAlign;
        const char* layersAlignLeft;
        const char* layersAlignCenter;
        const char* layersAlignRight;
        const char* layersAlignTop;
        const char* layersAlignMiddle;
        const char* layersAlignBottom;
        const char* layersThemeActive;
        const char* layersSaveXML;

        // ── OClock ────────────────────────────────────────────────────────────
        const char* oclockTitle;
        const char* oclockSetup;
        const char* minutes;
        const char* seconds;
        const char* start;
        const char* pause;
        const char* transmitLive;

        // ── QuickNotes ────────────────────────────────────────────────────────
        const char* quickNotesTitle;
        const char* quickNotesDesc;
        const char* showOnScreen;
        const char* hideMessage;
        const char* liveIndicator;

        // ── SettingsPanel (CategoryLanguage) ──────────────────────────────────
        const char* settingsLangHint;
        const char* settingsLangTitle;
        const char* settingsLangNote;
        const char* settingsLangPreview;
        const char* preferences;
        const char* search;
        const char* clearHistory;

        // ── Hub (pantalla de inicio) ────────────────────────────────────────────
        const char* hubStartProjecting;
        const char* hubOpenSettings;
        const char* hubLibraryCardDesc;
        const char* hubNovedadesTitle;
        const char* hubNovedadesCardSubFmt;   // "v%s disponible ..." -- %s = version
        const char* hubDownloadSubsTitle;
        const char* hubDownloadSubsCardDesc;
        const char* hubQuickAccess;
        const char* hubStatProjections;
        const char* hubStatFps;
        const char* hubStatTopSong;
        const char* hubStatNoData;
        const char* hubStatTopSongFmt;         // "Más proyectada (%d)" -- %d = veces
        const char* hubVersionHistory;
        const char* hubVersionFmt;             // "Versión v%s" -- %s = versión

        // ── Hub: "Descargar subtitulos" ──────────────────────────────────────────
        const char* hubDlSubsHint;
        const char* hubDlSubsSaveIn;
        const char* hubDlSubsAskEachTime;
        const char* hubDlSubsFixedFolder;
        const char* hubDlSubsNoFolder;
        const char* hubDlSubsChoose;
        const char* hubDlSubsChooseDialogTitle;
        const char* hubDlSubsSearching;
        const char* hubDlSubsDownloadBtn;
        const char* hubDlSubsSavedFmt;        // "Guardado en: %s" -- %s = ruta
        const char* hubDlSubsWriteError;

        // ── LibrarySongs (Canciones + Playlists) ─────────────────────────────────
        const char* libPlaylistsTab;
        const char* libNoPlaylistsYet;
        const char* libRename;
        const char* libBackShort;
        const char* libSongCountOneFmt;     // "%d canción" -- %d = 1
        const char* libSongCountManyFmt;    // "%d canciones" -- %d = cantidad
        const char* libNoSongsInPlaylist;
        const char* libAddSongsBtn;
        const char* libNewPlaylistBtn;
        const char* libPlaylistNameLabel;
        const char* libCreateBtn;
        const char* libNewNameLabel;
        const char* libAddSongsTitle;
        const char* libAddSongsSearchHint;
        const char* libAddedBadge;
        const char* libAddToPlaylistTooltip;
        const char* libNoResults;
        const char* libDoneBtn;
        const char* libSongsFoundOneFmt;    // "%d canción encontrada" -- %d = 1
        const char* libSongsFoundManyFmt;   // "%d canciones encontradas" -- %d = cantidad
        const char* libAssignTag;
        const char* libRemoveAllTags;

        // ── LibrarySidebar (riel de categorías, icono + label corto) ─────────────
        const char* libRailSongs;
        const char* libRailMultimedia;
        const char* libRailDocs;
    };

    const UIStrings& GetUIStrings();

} // namespace ProyecThor::UI
