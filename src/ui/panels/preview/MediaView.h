#pragma once
#include <string>
#include <cstdint> // Mantenemos este include igual que en MonitorView

// 1. Usamos la declaración adelantada 100% segura, idéntica a MonitorView
namespace ProyecThor {
    namespace Core {
        class VLCVideoLayer;
    }
}

namespace ProyecThor::UI {

    class MediaView {
    public:
        MediaView() = default;
        ~MediaView() = default;

        // 2. Usamos la ruta completa para el puntero
        void Render(ProyecThor::Core::VLCVideoLayer* previewPlayer);

    private:
        std::string FormatTime(int64_t ms);

        std::string m_LastSelectedFile;
        bool m_IsPlayingPreview = false;
    };

} // namespace ProyecThor::UI