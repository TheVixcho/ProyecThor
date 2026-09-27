#pragma once

#ifdef _WIN32
    #include <windows.h>
#endif
#include <GL/glew.h>
#include <string>
#include <cstdint>
#include "ImageView.h"

namespace ProyecThor::Core {
    class VLCBasePlayer;
}

namespace ProyecThor::UI {

    class MediaView {
    public:
        MediaView() = default;
        ~MediaView();

        void Render(Core::VLCBasePlayer* previewPlayer);
        std::string FormatTime(int64_t ms);

    private:
        std::string m_LastSelectedFile = "";
        bool m_IsPlayingPreview = false;

        ImageView m_ImageView;
    };

}
