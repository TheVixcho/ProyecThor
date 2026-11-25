#include "MemoryOptimizer.h"
#include "PresentationCore.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/panels/layers/LayersBgTab.h"
#include "ui/panels/overlay/OverlayLibraryTab.h"
#include <atomic>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <malloc.h>
#else
#include <cstdio>
#include <unistd.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif
#endif

namespace ProyecThor::Core {

namespace {
std::atomic<bool> s_PendingPurge{false};
std::atomic<bool> s_PendingPurgeThumbs{false};
std::atomic<bool> s_PendingTrimWorkingSet{false};
} // namespace

void MemoryOptimizer::RequestPurge(bool purgeThumbnails, bool trimWorkingSet) {
    if (purgeThumbnails)
        s_PendingPurgeThumbs.store(true, std::memory_order_relaxed);
    if (trimWorkingSet)
        s_PendingTrimWorkingSet.store(true, std::memory_order_relaxed);
    s_PendingPurge.store(true, std::memory_order_release);
}

void MemoryOptimizer::ProcessPending() {
    if (!s_PendingPurge.exchange(false, std::memory_order_acquire))
        return;

    const bool purgeThumbs = s_PendingPurgeThumbs.exchange(false, std::memory_order_relaxed);
    const bool trimWs      = s_PendingTrimWorkingSet.exchange(false, std::memory_order_relaxed);

    if (purgeThumbs) {
        ProyecThor::Library::ClearSongIndexCache();
        ProyecThor::UI::ClearSharedBgThumbnailCache();
        ProyecThor::Library::ClearMultimediaThumbnailCaches();
        ProyecThor::UI::OverlayLibraryTab::ClearSharedThumbnailCache();
    }

    PresentationCore::Get().PurgeMemoryCaches();

    if (trimWs) {
        TrimWorkingSet();
    }
}

void MemoryOptimizer::PurgeAll(bool trimWorkingSet) {
    ProyecThor::Library::ClearSongIndexCache();
    ProyecThor::UI::ClearSharedBgThumbnailCache();
    ProyecThor::Library::ClearMultimediaThumbnailCaches();
    ProyecThor::UI::OverlayLibraryTab::ClearSharedThumbnailCache();
    PresentationCore::Get().PurgeMemoryCaches();

    if (trimWorkingSet) {
        TrimWorkingSet();
    }
}

void MemoryOptimizer::TrimWorkingSet() {
#ifdef _WIN32
#if !defined(_DEBUG) && !defined(PT_DEBUG)
    _heapmin();
#endif
    HANDLE hProc = GetCurrentProcess();
    SetProcessWorkingSetSize(hProc, static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    K32EmptyWorkingSet(hProc);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}

float MemoryOptimizer::GetProcessRamMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc = {};
    pmc.cb = sizeof(pmc);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<float>(pmc.WorkingSetSize / (1024.0 * 1024.0));
    }
    return 0.0f;
#else
    if (FILE* f = std::fopen("/proc/self/statm", "r")) {
        unsigned long sizePages = 0, residentPages = 0;
        if (std::fscanf(f, "%lu %lu", &sizePages, &residentPages) == 2) {
            std::fclose(f);
            long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize > 0) {
                return static_cast<float>((static_cast<double>(residentPages) * pageSize) / (1024.0 * 1024.0));
            }
        } else {
            std::fclose(f);
        }
    }
    return 0.0f;
#endif
}

} // namespace ProyecThor::Core
