#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>

// Forward declaration de GLFW para no incluir el header completo aquí
struct GLFWwindow;
struct GLFWmonitor;

namespace ProyecThor::Core {

    class VLCBasePlayer;

    enum class ItemType { None = -1, Video = 0, Image = 1, Song = 2, Bible = 3 };

    struct LibrarySelection {
        std::string title;
        ItemType type = ItemType::None;
        std::vector<std::string> contentData;
    };

    struct PresentationState {
        enum class BackgroundType { SolidColor, Video };
        BackgroundType bgType = BackgroundType::SolidColor;
        std::string bgPath;
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };

        std::string overlayPath;
        bool isProjecting = false;
        int targetMonitorIndex = 0;

        std::string currentText;
        bool showText = false;
        float textSize = 60.0f;
        float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        int textAlignment = 1;
        float margins[4] = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool autoScale = true;
    };

    class PresentationCoreImpl;

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }

        PresentationCore();
        ~PresentationCore();

        void Update();
        void RenderProjectorWindow();

        PresentationState GetState();
        void* GetBackgroundTexture();
        void* GetOverlayTexture();

        VLCBasePlayer* GetBackgroundPlayer();
        VLCBasePlayer* GetOverlayPlayer();

        // SetSelection guarda la selección.
        // GetSelection la consume (limpia title tras leerla) — usar solo en MediaView.
        // PeekSelection la lee SIN consumirla — usar en paneles de UI que solo leen.
        void SetSelection(const LibrarySelection& selection);
        LibrarySelection GetSelection();
        LibrarySelection PeekSelection();

        void SetBackgroundMedia(const std::string& path, bool isVideo);
        void StopBackgroundMedia();
        void SetLayer0_Color(float r, float g, float b);

        void SetOverlayMedia(const std::string& path);
        void StopOverlayMedia();

        void SetLayer2_Text(const std::string& text);
        void UpdateTextStyle(float size, const float color[4], int align,
                             const float margins[4], bool autoScale);
        void ClearLayer2();

        void SetProjecting(bool state);
        void SetTargetMonitor(int index);
        void SetProjectorSize(int w, int h);

        // Ventana de proyección en monitor secundario
        void CreateProjectorWindow();
        void DestroyProjectorWindow();
        GLFWwindow* GetProjectorWindow() const;

    private:
        std::unique_ptr<PresentationCoreImpl> m_Impl;
        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;
        std::mutex        m_Mutex;

        int m_ProjectorWidth  = 1920;
        int m_ProjectorHeight = 1080;

        GLFWwindow* m_ProjectorWindow = nullptr;
    };

} // namespace ProyecThor::Core