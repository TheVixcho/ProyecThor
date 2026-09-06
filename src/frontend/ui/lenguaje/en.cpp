#include "../UIStrings.h"

namespace ProyecThor::UI {

extern const UIStrings kEnglish = {
    // Generales
    "ProyecThor", "Close", "Save", "Reset", "Cancel",
    "Edit", "Delete", "Import", "New", "Refresh", "No Signal",

    // Paneles
    "Library", "Preview", "Layers & Backgrounds Inspector", "Control",

    // Menu superior
    "File", "Exit", "Edit", "Settings", "View",
    "Reset Layout", "Help", "Documentation", "Donations",
    "About ProyecThor",

    // About
    "Professional software for projection management.",
    "Non-profit. We operate through donations\nfrom the development team and community.",

    // LibraryPanel
    "Songs", "Videos", "Images", "Bible", "Documents",
    "Search by name or lyrics", "No player available.",

    // BibleView
    "Bible:", "Books", "Chapters",
    "Search book, ch. or verse (Gen 1:1)", "Clear History",
    "Error: Could not load the Bible XML file.",
    "Edit", "Save to XML",
    "  BOOKS", "  CHAPTERS",

    // SongView
    "LYRICS DECK", "CLEAR SCREEN", "Edit this song",

    // MediaView
    "Select an item from the library.",
    "Project Image",
    "Video ready: %s",
    "Use the monitor controls to project.",

    // TransitionPanel
    "Transition Effects",
    "No transition",
    "Dissolve",
    "Zoom In",
    "Zoom Out",
    "Transition duration",
    "Tip: Transitions are applied when changing stanzas or projecting new content.",

    // DocumentView
    "The document has no generated pages.",
    "Document: %s",
    "Page %d of %d",
    "of",
    "<< Previous",
    "Next >>",
    "Project Current Page",

    // MonitorView
    "PREVIEW (Video Only)", "  PREVIEW  NO SIGNAL",
    "LIVE (Broadcast)",     "  ON AIR  NO SIGNAL",
    "BROADCAST##trans",
    "[ Audio disabled ]",
    "MUTE##lm", "Stp##p",

    // ControlPanel
    "Quick Controls",
    "REMOVE TEXT",
    "STOP VIDEO",
    "Only 1 screen detected. Connect a second monitor to project.",
    "Screens detected: %d",
    "Output: [%d] %s  (%dx%d)",
    "START PROJECTION",
    "STOP PROJECTOR",
    "Projecting actively",
    "Projector inactive",

    // LayersPanel
    "  Backgrounds  ", "  Letter Styles  ",
    "    Backgrounds & Videos", "    Reload    ",
    "Drag videos to assets/backgrounds",
    "  + New Style  ", "  Reload Fonts  ",
    "Create your first style with the button above",
    "  Quick Settings (unsaved)",
    "Font", "Text Color", "Size  %.0f px",
    "Horizontal alignment", "Vertical alignment",
    "Left", "Center", "Right",
    "Top", "Middle##v", "Bottom",
    "ACTIVE", "Save to XML",

    // OClock
    "Clock & Timers",
    "Countdown Setup",
    "Minutes",
    "Seconds",
    "START",
    "PAUSE/STOP",
    "Broadcast to Main Screen",

    // QuickNotes
    "Quick Notes",
    "Type a message to display instantly on screen.",
    "Show on Screen (F5)",
    "Hide Message (ESC)",
    "LIVE",

    // SettingsPanel
    "Select the user interface language.",
    "Interface Language",
    "Language change applies after saving and restarting.\nSome strings may require a full restart.",
    "String Preview",
    "Preferences",
    "Search (Abbrev. Book + 1:1)",
    "Clear History",

    // Hub
    "Start Projecting",
    "Open Settings",
    "Just the Library panel, Render included",
    "What's New",
    "v%s available — key N",
    "Download Subtitles",
    "Download them as .txt from a URL",
    "Quick Access",
    "Total Projections",
    "Average FPS",
    "Most Projected Song",
    "No data yet",
    "Most projected (%d)",
    "VERSION HISTORY",
    "Version v%s",

    // Hub: "Download Subtitles"
    "Paste a video link. Its subtitles are looked up (Spanish first, then English) and saved as a standalone .txt file — no song is created.",
    "Save to",
    "Ask every time",
    "Fixed folder",
    "Not chosen",
    "Choose",
    "Choose folder for downloaded subtitles",
    "Searching for subtitles",
    "Download",
    "Saved to: %s",
    "Could not write the file to that location.",

    // LibrarySongs (Songs + Playlists)
    "Playlists",
    "No playlists yet",
    "Rename",
    "< Back",
    "%d song",
    "%d songs",
    "This playlist has no songs yet",
    "+ Add Songs",
    "+ New Playlist",
    "Playlist name",
    "Create",
    "New name",
    "Add Songs",
    "Search by title or author",
    "Added",
    "Add to playlist",
    "No results",
    "Done",
    "%d song found",
    "%d songs found",
    "Assign Tag",
    "Remove All Tags",

    // LibrarySidebar
    "Lyrics",
    "Media",
    "Doc"
};

} // namespace ProyecThor::UI
