#pragma once
#include <string>
#include <cstdint>

namespace ProyecThor {
    namespace Core {
        class VLCBasePlayer;
    }
}

namespace ProyecThor::UI {

    class MediaView {
    public:
        MediaView() = default;
        ~MediaView() = default;

        void Render(Core::VLCBasePlayer* previewPlayer);

    private:
        std::string FormatTime(int64_t ms);

        std::string m_LastSelectedFile;
        bool        m_IsPlayingPreview = false;
    };

} // namespace ProyecThor::UI