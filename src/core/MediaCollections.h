#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace ProyecThor::Core {

enum class CollectionMediaType {
    Video = 0,
    Image = 1,
    Audio = 2,
    Song = 3,
    Document = 4
};

struct MediaCollection {
    std::string              name;
    std::vector<std::string> itemFilenames; // Filenames or paths in order
    float                    slideDuration = 5.0f; // Seconds for image slideshows
    bool                     loop          = true;
    std::string              targetRoot    = "ProyecThor"; // "ProyecThor" or name/path of external folder
};

class MediaCollectionManager {
public:
    static MediaCollectionManager& Get();

    void Initialize();
    void Load();
    void Save();

    // ── Collection management ─────────────────────────────────────────────
    std::vector<std::string> GetCollectionNames(CollectionMediaType type) const;
    const MediaCollection*   GetCollection(CollectionMediaType type, const std::string& name) const;
    MediaCollection*         GetCollectionMutable(CollectionMediaType type, const std::string& name);

    bool CreateCollection(CollectionMediaType type, const std::string& name, const std::string& targetRoot = "ProyecThor");
    bool DeleteCollection(CollectionMediaType type, const std::string& name);
    bool RenameCollection(CollectionMediaType type, const std::string& oldName, const std::string& newName);
    bool MoveCollection(CollectionMediaType type, int fromIndex, int toIndex);

    std::string GetTargetRoot(CollectionMediaType type, const std::string& name) const;
    std::string GetActiveCollection(CollectionMediaType type) const;
    void        SetActiveCollection(CollectionMediaType type, const std::string& name);

    bool AddItemToCollection(CollectionMediaType type, const std::string& collName, const std::string& filename);
    bool RemoveItemFromCollection(CollectionMediaType type, const std::string& collName, int index);
    bool RemoveItemFromCollectionByPath(CollectionMediaType type, const std::string& collName, const std::string& filename);
    bool MoveItemInCollection(CollectionMediaType type, const std::string& collName, int from, int to);
    bool MoveItemBetweenCollections(CollectionMediaType type, const std::string& fromCol, const std::string& toCol, const std::string& filename);

    float GetSlideDuration(const std::string& collName) const;
    void  SetSlideDuration(const std::string& collName, float duration);

    bool GetLoop(CollectionMediaType type, const std::string& collName) const;
    void SetLoop(CollectionMediaType type, const std::string& collName, bool loop);

    // ── Sequential Queue Playback ("Reproducir en Cola") ───────────────────
    void StartQueue(CollectionMediaType type, const std::string& collName,
                    const std::vector<std::string>& allItemsFallback = {},
                    int startIndex = 0);
    void StopQueue();
    bool IsQueuePlaying() const { return m_QueuePlaying; }

    CollectionMediaType GetQueueType() const { return m_QueueType; }
    const std::string&  GetQueueCollectionName() const { return m_QueueCollectionName; }
    int                 GetQueueCurrentIndex() const { return m_QueueCurrentIndex; }
    int                 GetQueueTotalItems() const { return (int)m_QueueItems.size(); }
    std::string         GetQueueCurrentItem() const;
    std::string         GetQueueItem(int index) const;
    float               GetSlideTimer() const { return m_SlideTimer; }
    float               GetCurrentSlideDuration() const { return m_CurrentSlideDuration; }

    void Next();
    void Previous();
    void PlayQueueIndex(int index);

    // Must be pumped every frame (calculates image slide timer & video EOF)
    void Update(float dt);

    std::string ResolveFullPath(CollectionMediaType type, const std::string& filename) const;

private:
    MediaCollectionManager() = default;
    ~MediaCollectionManager() = default;

    std::vector<MediaCollection>&       GetListForType(CollectionMediaType type);
    const std::vector<MediaCollection>& GetListForType(CollectionMediaType type) const;

    std::vector<MediaCollection> m_VideoCollections;
    std::vector<MediaCollection> m_ImageCollections;
    std::vector<MediaCollection> m_AudioCollections;
    std::vector<MediaCollection> m_SongCollections;
    std::vector<MediaCollection> m_DocumentCollections;

    // Queue State
    bool                m_QueuePlaying = false;
    CollectionMediaType m_QueueType    = CollectionMediaType::Video;
    std::string         m_QueueCollectionName;
    std::vector<std::string> m_QueueItems;
    int                 m_QueueCurrentIndex   = -1;
    float               m_SlideTimer          = 0.0f;
    float               m_CurrentSlideDuration = 5.0f;
    float               m_DefaultSlideDuration = 5.0f;
    float               m_ItemPlayTimer       = 0.0f;
    bool                m_PrefetchedCurrent   = false;
    bool                m_CurrentLoop         = true;
    bool                m_Initialized         = false;
    std::unordered_map<int, std::string> m_ActiveCollections;
};

} // namespace ProyecThor::Core
