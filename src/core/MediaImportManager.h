#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace ProyecThor::Core {

enum class MediaImportDest {
    Multimedia = 0,
    Background = 1
};

enum class CandidateMediaKind {
    Video = 0,
    Image = 1,
    Audio = 2
};

struct ImportCandidate {
    std::string        fullPath;
    std::string        filename;
    std::string        parentFolderLabel;
    uint64_t           fileSizeBytes       = 0;
    CandidateMediaKind kind                = CandidateMediaKind::Video;
    MediaImportDest    destination         = MediaImportDest::Multimedia;
    bool               selected            = true;
    bool               alreadyInMultimedia = false;
    bool               alreadyInBackground = false;
};

class MediaImportManager {
public:
    static MediaImportManager& Get();

    // Llamar una vez por frame desde el hilo principal (UIManager::RenderAll).
    // Dispara escaneos asincronos livianos cada pocos segundos sin bloquear la UI.
    void Update();

    // Fuerza un escaneo inmediato en segundo plano de Descargas y carpetas vinculadas.
    void TriggerImmediateScan();

    // Abre el selector nativo de archivos multiples en un hilo separado y, al
    // elegir archivos, muestra el menu de seleccion (Multimedia vs Background).
    void OpenFilePickerForImport(MediaImportDest defaultDest, const std::string& targetBgFolder = "");

    // Encola rutas soltadas por drag & drop o seleccionadas manualmente para
    // presentarlas en el menu de seleccion sin duplicarlas en disco.
    void EnqueueManualFiles(const std::vector<std::string>& rawPaths,
                            MediaImportDest defaultDest,
                            const std::string& targetBgFolder = "");

    // Vincula directamente un unico archivo externo (ej. "Abrir con ProyecThor")
    // sin copiarlo y evitando duplicados de nombre.
    bool LinkSingleExternalFileDirect(const std::string& rawPath, MediaImportDest dest);

    // ── Estado del modal interactivo (hilo principal) ────────────────────────
    bool HasPendingPrompt() const { return m_ModalOpen; }
    bool IsAutoDetectedPrompt() const { return m_ModalIsAutoDetected; }
    const std::string& GetTargetBgFolder() const { return m_ModalTargetBgFolder; }

    std::vector<ImportCandidate>&       GetCandidates()       { return m_Candidates; }
    const std::vector<ImportCandidate>& GetCandidates() const { return m_Candidates; }

    // Recalcula las banderas alreadyInMultimedia / alreadyInBackground de los candidatos actuales.
    void RefreshCandidateDuplicateFlags();

    // Confirma ("Sí"): vincula los archivos seleccionados en su ubicacion original
    // (ej. Descargas) o los copia solo si copyToAppFolder == true, evitando siempre
    // duplicar archivos con el mismo nombre.
    int CommitPendingImport(bool copyToAppFolder = false);

    // Rechaza ("No"): cierra el modal y registra los archivos detectados para no
    // volver a preguntar por ellos ni importarlos automaticamente.
    void DismissPendingImport();

    // ── Utilidades de desvinculacion, movimiento y deduplicacion ─────────────
    static std::string NormalizePathKey(const std::string& utf8Path);
    static std::string NormalizeFilenameKey(const std::string& utf8Filename);
    static void        ParseExternalBgEntry(const std::string& rawEntry,
                                            std::string& outFolder,
                                            std::string& outFullPath);
    static std::string FormatExternalBgEntry(const std::string& folder,
                                             const std::string& fullPath);

    bool IsExternalBackgroundPath(const std::string& utf8Path) const;
    bool IsExternalMultimediaPath(const std::string& utf8Path) const;

    bool UnlinkExternalMultimedia(const std::string& utf8Path);
    bool UnlinkExternalBackground(const std::string& utf8Path);

    // Mueve un archivo externo entre Multimedia <-> Background sin copiar ni mover en disco.
    bool MoveExternalToBackground(const std::string& utf8Path, const std::string& bgFolder = "");
    bool MoveExternalToMultimedia(const std::string& utf8Path);

    // Verifica si ya existe un archivo con el mismo nombre (case-insensitive) en Multimedia o Background.
    bool ExistsInMultimediaByName(const std::string& filename) const;
    bool ExistsInBackgroundByName(const std::string& filename) const;

    static std::string GetSystemDownloadsFolderUtf8();

private:
    MediaImportManager() = default;

    struct PendingBatch {
        std::vector<ImportCandidate> items;
        bool                         isAutoDetected = false;
        MediaImportDest              defaultDest    = MediaImportDest::Multimedia;
        std::string                  targetBgFolder;
    };

    void StartAsyncWatcherScan();
    static bool ClassifyExtension(const std::string& lowerExt, CandidateMediaKind& outKind);
    static MediaImportDest GuessSmartDestination(const std::string& filename,
                                                 CandidateMediaKind kind,
                                                 MediaImportDest fallback);

    void CollectExistingSets(std::unordered_set<std::string>& mmPaths,
                             std::unordered_set<std::string>& mmNames,
                             std::unordered_set<std::string>& bgPaths,
                             std::unordered_set<std::string>& bgNames) const;

    std::atomic<bool>      m_ScanRunning{false};
    std::atomic<bool>      m_PickerRunning{false};
    std::atomic<bool>      m_ForceImmediateScan{false};
    double                 m_LastScanTimeSec = -10.0;

    mutable std::mutex     m_PendingMutex;
    std::vector<PendingBatch> m_PendingBatches;

    // Estado activo en el hilo de UI
    bool                         m_ModalOpen           = false;
    bool                         m_ModalIsAutoDetected = false;
    std::string                  m_ModalTargetBgFolder;
    std::vector<ImportCandidate> m_Candidates;
};

} // namespace ProyecThor::Core
