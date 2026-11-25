#include "MediaCollections.h"
#include "core/AppPaths.h"
#include "core/PresentationCore.h"
#include "media/player/MPVBasePlayer.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/views/Audio.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace ProyecThor::Core {

static std::string CollectionsFilePath()
{
    return GetAssetsPath() + "/library_collections.json";
}

MediaCollectionManager& MediaCollectionManager::Get()
{
    static MediaCollectionManager s_Instance;
    if (!s_Instance.m_Initialized)
    {
        s_Instance.Initialize();
    }
    return s_Instance;
}

void MediaCollectionManager::Initialize()
{
    m_Initialized = true;
    Load();
}

std::vector<MediaCollection>& MediaCollectionManager::GetListForType(CollectionMediaType type)
{
    switch (type) {
        case CollectionMediaType::Video:    return m_VideoCollections;
        case CollectionMediaType::Image:    return m_ImageCollections;
        case CollectionMediaType::Audio:    return m_AudioCollections;
        case CollectionMediaType::Song:     return m_SongCollections;
        case CollectionMediaType::Document: return m_DocumentCollections;
    }
    return m_VideoCollections;
}

const std::vector<MediaCollection>& MediaCollectionManager::GetListForType(CollectionMediaType type) const
{
    switch (type) {
        case CollectionMediaType::Video:    return m_VideoCollections;
        case CollectionMediaType::Image:    return m_ImageCollections;
        case CollectionMediaType::Audio:    return m_AudioCollections;
        case CollectionMediaType::Song:     return m_SongCollections;
        case CollectionMediaType::Document: return m_DocumentCollections;
    }
    return m_VideoCollections;
}

std::vector<std::string> MediaCollectionManager::GetCollectionNames(CollectionMediaType type) const
{
    const auto& list = GetListForType(type);
    std::vector<std::string> names;
    names.reserve(list.size());
    for (const auto& c : list) {
        names.push_back(c.name);
    }
    return names;
}

const MediaCollection* MediaCollectionManager::GetCollection(CollectionMediaType type, const std::string& name) const
{
    const auto& list = GetListForType(type);
    for (const auto& c : list) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

MediaCollection* MediaCollectionManager::GetCollectionMutable(CollectionMediaType type, const std::string& name)
{
    auto& list = GetListForType(type);
    for (auto& c : list) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

bool MediaCollectionManager::CreateCollection(CollectionMediaType type, const std::string& name, const std::string& targetRoot)
{
    if (name.empty()) return false;
    auto& list = GetListForType(type);
    for (const auto& c : list) {
        if (c.name == name) return false;
    }
    MediaCollection newCol;
    newCol.name = name;
    newCol.slideDuration = 5.0f;
    newCol.loop = true;
    newCol.targetRoot = targetRoot.empty() ? "ProyecThor" : targetRoot;
    list.push_back(newCol);
    Save();
    return true;
}

std::string MediaCollectionManager::GetTargetRoot(CollectionMediaType type, const std::string& name) const
{
    const auto* col = GetCollection(type, name);
    return col ? col->targetRoot : "ProyecThor";
}

bool MediaCollectionManager::DeleteCollection(CollectionMediaType type, const std::string& name)
{
    auto& list = GetListForType(type);
    auto it = std::remove_if(list.begin(), list.end(), [&](const MediaCollection& c) {
        return c.name == name;
    });
    if (it != list.end()) {
        list.erase(it, list.end());
        if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == name) {
            StopQueue();
        }
        Save();
        return true;
    }
    return false;
}

bool MediaCollectionManager::RenameCollection(CollectionMediaType type, const std::string& oldName, const std::string& newName)
{
    if (newName.empty() || oldName == newName) return false;
    auto* col = GetCollectionMutable(type, oldName);
    if (!col) return false;
    if (GetCollection(type, newName) != nullptr) return false; // Name collision

    col->name = newName;
    if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == oldName) {
        m_QueueCollectionName = newName;
    }
    if (GetActiveCollection(type) == oldName) {
        SetActiveCollection(type, newName);
    }
    Save();
    return true;
}

bool MediaCollectionManager::MoveCollection(CollectionMediaType type, int fromIndex, int toIndex)
{
    auto& list = (type == CollectionMediaType::Video) ? m_VideoCollections :
                 (type == CollectionMediaType::Image) ? m_ImageCollections :
                                                        m_AudioCollections;
    if (fromIndex < 0 || fromIndex >= (int)list.size() ||
        toIndex < 0 || toIndex >= (int)list.size() ||
        fromIndex == toIndex) return false;

    auto item = std::move(list[fromIndex]);
    list.erase(list.begin() + fromIndex);
    list.insert(list.begin() + toIndex, std::move(item));
    Save();
    return true;
}

std::string MediaCollectionManager::GetActiveCollection(CollectionMediaType type) const
{
    auto it = m_ActiveCollections.find(static_cast<int>(type));
    return (it != m_ActiveCollections.end()) ? it->second : "";
}

void MediaCollectionManager::SetActiveCollection(CollectionMediaType type, const std::string& name)
{
    m_ActiveCollections[static_cast<int>(type)] = name;
}

bool MediaCollectionManager::AddItemToCollection(CollectionMediaType type, const std::string& collName, const std::string& filename)
{
    if (filename.empty()) return false;
    auto* col = GetCollectionMutable(type, collName);
    if (!col) return false;

    // Check if item already exists in collection
    for (const auto& it : col->itemFilenames) {
        if (it == filename) return false;
    }
    col->itemFilenames.push_back(filename);
    Save();
    return true;
}

bool MediaCollectionManager::RemoveItemFromCollection(CollectionMediaType type, const std::string& collName, int index)
{
    auto* col = GetCollectionMutable(type, collName);
    if (!col || index < 0 || index >= (int)col->itemFilenames.size()) return false;

    col->itemFilenames.erase(col->itemFilenames.begin() + index);
    if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == collName) {
        m_QueueItems = col->itemFilenames;
        if (m_QueueItems.empty()) {
            StopQueue();
        } else if (m_QueueCurrentIndex >= (int)m_QueueItems.size()) {
            m_QueueCurrentIndex = 0;
            PlayQueueIndex(0);
        }
    }
    Save();
    return true;
}

bool MediaCollectionManager::RemoveItemFromCollectionByPath(CollectionMediaType type, const std::string& collName, const std::string& filename)
{
    auto* col = GetCollectionMutable(type, collName);
    if (!col) return false;

    auto it = std::find(col->itemFilenames.begin(), col->itemFilenames.end(), filename);
    if (it != col->itemFilenames.end()) {
        col->itemFilenames.erase(it);
        if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == collName) {
            m_QueueItems = col->itemFilenames;
            if (m_QueueItems.empty()) {
                StopQueue();
            } else if (m_QueueCurrentIndex >= (int)m_QueueItems.size()) {
                m_QueueCurrentIndex = 0;
                PlayQueueIndex(0);
            }
        }
        Save();
        return true;
    }
    return false;
}

bool MediaCollectionManager::MoveItemInCollection(CollectionMediaType type, const std::string& collName, int from, int to)
{
    auto* col = GetCollectionMutable(type, collName);
    if (!col) return false;
    if (from < 0 || from >= (int)col->itemFilenames.size()) return false;
    if (to < 0 || to >= (int)col->itemFilenames.size()) return false;
    if (from == to) return true;

    std::string item = col->itemFilenames[from];
    col->itemFilenames.erase(col->itemFilenames.begin() + from);
    col->itemFilenames.insert(col->itemFilenames.begin() + to, item);
    if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == collName) {
        m_QueueItems = col->itemFilenames;
    }
    Save();
    return true;
}

bool MediaCollectionManager::MoveItemBetweenCollections(CollectionMediaType type, const std::string& fromCol, const std::string& toCol, const std::string& filename)
{
    if (fromCol == toCol) return true;
    if (!fromCol.empty()) {
        RemoveItemFromCollectionByPath(type, fromCol, filename);
    }
    return AddItemToCollection(type, toCol, filename);
}

float MediaCollectionManager::GetSlideDuration(const std::string& collName) const
{
    if (collName.empty()) return m_DefaultSlideDuration;
    const auto* col = GetCollection(CollectionMediaType::Image, collName);
    return col ? col->slideDuration : m_DefaultSlideDuration;
}

void MediaCollectionManager::SetSlideDuration(const std::string& collName, float duration)
{
    float val = std::clamp(duration, 1.0f, 120.0f);
    if (collName.empty()) {
        m_DefaultSlideDuration = val;
        if (m_QueuePlaying && m_QueueType == CollectionMediaType::Image) {
            m_CurrentSlideDuration = val;
        }
        Save();
        return;
    }

    auto* col = GetCollectionMutable(CollectionMediaType::Image, collName);
    if (col) {
        col->slideDuration = val;
        if (m_QueuePlaying && m_QueueType == CollectionMediaType::Image && m_QueueCollectionName == collName) {
            m_CurrentSlideDuration = val;
        }
        Save();
    }
}

bool MediaCollectionManager::GetLoop(CollectionMediaType type, const std::string& collName) const
{
    const auto* col = GetCollection(type, collName);
    return col ? col->loop : true;
}

void MediaCollectionManager::SetLoop(CollectionMediaType type, const std::string& collName, bool loop)
{
    auto* col = GetCollectionMutable(type, collName);
    if (col) {
        col->loop = loop;
        if (m_QueuePlaying && m_QueueType == type && m_QueueCollectionName == collName) {
            m_CurrentLoop = loop;
        }
        Save();
    }
}

std::string MediaCollectionManager::ResolveFullPath(CollectionMediaType type, const std::string& filename) const
{
    if (filename.empty()) return "";
    fs::path p = Library::U8Path(filename);
    if (p.is_absolute() && fs::exists(p)) return filename;

    // Check in multimedia scanned lists (handles external files with customFullPath)
    if (type == CollectionMediaType::Video) {
        for (const auto& it : Library::GetVideoList()) {
            if (it.filename == filename) return Library::GetMMItemFullPath(it);
        }
        return Library::GetVideoLibraryPath() + "/" + filename;
    } else if (type == CollectionMediaType::Image) {
        for (const auto& it : Library::GetImageList()) {
            if (it.filename == filename) return Library::GetMMItemFullPath(it);
        }
        return GetAssetsPath() + "/images/" + filename;
    } else if (type == CollectionMediaType::Audio) {
        for (const auto& it : Library::GetAudioList()) {
            if (it.filename == filename) return Library::GetMMItemFullPath(it);
        }
        return GetAssetsPath() + "/audio/" + filename;
    } else if (type == CollectionMediaType::Song) {
        return GetAssetsPath() + "/songs/" + filename;
    } else if (type == CollectionMediaType::Document) {
        return GetAssetsPath() + "/documents/" + filename;
    }
    return filename;
}

std::string MediaCollectionManager::GetQueueCurrentItem() const
{
    if (!m_QueuePlaying || m_QueueCurrentIndex < 0 || m_QueueCurrentIndex >= (int)m_QueueItems.size())
        return "";
    return m_QueueItems[m_QueueCurrentIndex];
}

std::string MediaCollectionManager::GetQueueItem(int index) const
{
    if (index >= 0 && index < (int)m_QueueItems.size())
        return m_QueueItems[index];
    return "";
}

void MediaCollectionManager::StartQueue(CollectionMediaType type, const std::string& collName,
                                        const std::vector<std::string>& allItemsFallback,
                                        int startIndex)
{
    m_QueueType           = type;
    m_QueueCollectionName = collName;
    m_QueueItems.clear();

    if (!collName.empty()) {
        const auto* col = GetCollection(type, collName);
        if (col) {
            m_QueueItems           = col->itemFilenames;
            m_CurrentSlideDuration = col->slideDuration;
            m_CurrentLoop          = col->loop;
        }
    } else {
        m_QueueItems           = allItemsFallback;
        m_CurrentSlideDuration = m_DefaultSlideDuration;
        m_CurrentLoop          = true;
    }

    if (m_QueueItems.empty()) {
        m_QueuePlaying      = false;
        m_QueueCurrentIndex = -1;
        return;
    }

    m_QueuePlaying      = true;
    m_SlideTimer        = 0.0f;
    m_ItemPlayTimer     = 0.0f;
    m_PrefetchedCurrent = false;
    PlayQueueIndex(std::clamp(startIndex, 0, (int)m_QueueItems.size() - 1));
}

void MediaCollectionManager::StopQueue()
{
    if (m_QueueType == CollectionMediaType::Audio) {
        if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
            ap->Stop();
        }
    } else if (m_QueueType == CollectionMediaType::Video) {
        Core::PresentationCore::Get().StopBackgroundMedia();
    }

    m_QueuePlaying      = false;
    m_QueueCurrentIndex = -1;
    m_SlideTimer        = 0.0f;
    m_ItemPlayTimer     = 0.0f;
    m_PrefetchedCurrent = false;
}

void MediaCollectionManager::PlayQueueIndex(int index)
{
    if (!m_QueuePlaying || m_QueueItems.empty()) return;
    if (index < 0 || index >= (int)m_QueueItems.size()) {
        if (m_CurrentLoop && !m_QueueItems.empty()) {
            index = 0;
        } else {
            StopQueue();
            return;
        }
    }

    m_QueueCurrentIndex = index;
    m_SlideTimer        = 0.0f;
    m_ItemPlayTimer     = 0.0f;
    m_PrefetchedCurrent = false;

    std::string item = m_QueueItems[m_QueueCurrentIndex];
    std::string fullPath = ResolveFullPath(m_QueueType, item);

    auto& core = Core::PresentationCore::Get();

    if (m_QueueType == CollectionMediaType::Video)
    {
        core.SetProjecting(true);
        core.CommitNextBackgroundMedia(fullPath, /*isVideo=*/true, /*allowAudio=*/true);

        auto currentSel = core.PeekSelection();
        if (currentSel.title != item) {
            core.RequestPreviewLoad(fullPath, /*loop=*/false, /*allowAudio=*/false);
        }

        Core::LibrarySelection sel;
        sel.title = item;
        sel.type  = Core::ItemType::Video;
        core.SetSelection(sel, /*fromQueue=*/true);
    }
    else if (m_QueueType == CollectionMediaType::Image)
    {
        core.SetProjecting(true);
        core.SetBackgroundMedia(fullPath, /*isVideo=*/false, /*allowAudio=*/false);
        core.RequestPreviewLoad(fullPath, /*loop=*/false, /*allowAudio=*/false);

        Core::LibrarySelection sel;
        sel.title = item;
        sel.type  = Core::ItemType::Image;
        core.SetSelection(sel, /*fromQueue=*/true);
    }
    else if (m_QueueType == CollectionMediaType::Audio)
    {
        if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
            ap->PlayFileLive(fullPath.empty() ? item : fullPath);
        }

        Core::LibrarySelection sel;
        sel.title = item;
        sel.type  = Core::ItemType::Audio;
        core.SetSelection(sel, /*fromQueue=*/true);
    }
}

void MediaCollectionManager::Next()
{
    if (!m_QueuePlaying || m_QueueItems.empty()) return;
    int nextIdx = m_QueueCurrentIndex + 1;
    if (nextIdx >= (int)m_QueueItems.size()) {
        if (m_CurrentLoop) {
            nextIdx = 0;
        } else {
            StopQueue();
            return;
        }
    }
    PlayQueueIndex(nextIdx);
}

void MediaCollectionManager::Previous()
{
    if (!m_QueuePlaying || m_QueueItems.empty()) return;
    int prevIdx = m_QueueCurrentIndex - 1;
    if (prevIdx < 0) {
        prevIdx = (int)m_QueueItems.size() - 1;
    }
    PlayQueueIndex(prevIdx);
}

void MediaCollectionManager::Update(float dt)
{
    if (!m_QueuePlaying || m_QueueItems.empty())
        return;

    if (m_QueueCurrentIndex < 0 || m_QueueCurrentIndex >= (int)m_QueueItems.size()) {
        StopQueue();
        return;
    }

    m_ItemPlayTimer += dt;

    if (m_QueueType == CollectionMediaType::Video)
    {
        auto& core = Core::PresentationCore::Get();
        auto* bgPlayer = core.GetBackgroundPlayer();

        // Precarga anticipada del siguiente video de la cola para corte instantáneo
        if (!m_PrefetchedCurrent && !core.IsBackgroundSwapPending() && m_QueueItems.size() > 1)
        {
            int nextIdx = m_QueueCurrentIndex + 1;
            if (nextIdx >= (int)m_QueueItems.size() && m_CurrentLoop) nextIdx = 0;
            if (nextIdx >= 0 && nextIdx < (int)m_QueueItems.size() && nextIdx != m_QueueCurrentIndex)
            {
                std::string nextPath = ResolveFullPath(m_QueueType, m_QueueItems[nextIdx]);
                if (!nextPath.empty()) {
                    core.PreloadNextBackgroundMedia(nextPath, /*allowAudio=*/true);
                    m_PrefetchedCurrent = true;
                }
            }
        }
        else if (m_QueueItems.size() <= 1)
        {
            m_PrefetchedCurrent = true;
        }

        if (bgPlayer && m_ItemPlayTimer > 0.3f)
        {
            if (bgPlayer->ConsumeEndReached())
            {
                Next();
            }
            else if (bgPlayer->ConsumeHadError())
            {
                Next();
            }
        }
    }
    else if (m_QueueType == CollectionMediaType::Image)
    {
        m_SlideTimer += dt;
        if (m_SlideTimer >= m_CurrentSlideDuration)
        {
            m_SlideTimer = 0.0f;
            Next();
        }
    }
    else if (m_QueueType == CollectionMediaType::Audio)
    {
        // El fin de cada pista lo notifica AudioPanel::Update() (Next()).
        // Vigilante: si la musica ya no esta activa (el usuario la detuvo o el
        // fondo en vivo cambio a otra cosa), la cola se da por terminada en
        // vez de quedar "fantasma" y robarse los controles del ViewPanel.
        if (m_ItemPlayTimer > 3.0f) {
            auto* ap = UI::AudioPanel::GetActiveInstance();
            if (!ap || (!ap->IsMusicActive() && !ap->GetIsPlaying())) {
                m_QueuePlaying      = false;
                m_QueueCurrentIndex = -1;
                m_ItemPlayTimer     = 0.0f;
            }
        }
    }
}

void MediaCollectionManager::Load()
{
    std::string filePath = CollectionsFilePath();
    std::error_code ec;
    if (!fs::exists(Library::U8Path(filePath), ec)) return;

    try {
        std::ifstream f(Library::U8Path(filePath));
        if (!f.is_open()) return;

        json j;
        f >> j;

        auto LoadCategory = [&](const std::string& catKey, std::vector<MediaCollection>& outList) {
            outList.clear();
            if (!j.contains(catKey) || !j[catKey].is_array()) return;
            for (const auto& item : j[catKey]) {
                MediaCollection c;
                c.name = item.value("name", "");
                if (c.name.empty()) continue;
                c.slideDuration = item.value("slideDuration", 5.0f);
                c.loop          = item.value("loop", true);
                c.targetRoot    = item.value("targetRoot", "ProyecThor");
                if (item.contains("items") && item["items"].is_array()) {
                    for (const auto& strItem : item["items"]) {
                        if (strItem.is_string()) c.itemFilenames.push_back(strItem.get<std::string>());
                    }
                }
                outList.push_back(c);
            }
        };

        LoadCategory("videos",    m_VideoCollections);
        LoadCategory("images",    m_ImageCollections);
        LoadCategory("audio",     m_AudioCollections);
        LoadCategory("songs",     m_SongCollections);
        LoadCategory("documents", m_DocumentCollections);

        m_DefaultSlideDuration = j.value("defaultSlideDuration", 5.0f);
    } catch (const std::exception& e) {
        std::cerr << "[MediaCollectionManager] Error al cargar colecciones: " << e.what() << '\n';
    }
}

void MediaCollectionManager::Save()
{
    std::string filePath = CollectionsFilePath();
    try {
        fs::path p = Library::U8Path(filePath);
        fs::create_directories(p.parent_path());

        json j;

        auto SaveCategory = [&](const std::string& catKey, const std::vector<MediaCollection>& inList) {
            j[catKey] = json::array();
            for (const auto& c : inList) {
                json item;
                item["name"]          = c.name;
                item["slideDuration"] = c.slideDuration;
                item["loop"]          = c.loop;
                item["targetRoot"]    = c.targetRoot;
                item["items"]         = c.itemFilenames;
                j[catKey].push_back(item);
            }
        };

        SaveCategory("videos",    m_VideoCollections);
        SaveCategory("images",    m_ImageCollections);
        SaveCategory("audio",     m_AudioCollections);
        SaveCategory("songs",     m_SongCollections);
        SaveCategory("documents", m_DocumentCollections);
        j["defaultSlideDuration"] = m_DefaultSlideDuration;

        std::ofstream f(p);
        if (f.is_open()) {
            f << j.dump(2);
        }
    } catch (const std::exception& e) {
        std::cerr << "[MediaCollectionManager] Error al guardar colecciones: " << e.what() << '\n';
    }
}

} // namespace ProyecThor::Core
