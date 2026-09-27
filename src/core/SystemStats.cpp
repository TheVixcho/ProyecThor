#include "SystemStats.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <cstring>
#endif

namespace ProyecThor::Core {

namespace {
    constexpr double kSampleIntervalSeconds = 1.0;
}

void SystemStats::EnsureGpuName()
{
    if (!m_GpuName.empty()) return;
    const GLubyte* renderer = glGetString(GL_RENDERER);
    m_GpuName = renderer ? reinterpret_cast<const char*>(renderer) : "Desconocida";

    const GLubyte* vendor = glGetString(GL_VENDOR);
    std::string vendorStr = vendor ? reinterpret_cast<const char*>(vendor) : "";
    m_IsNvidiaGpu = vendorStr.find("NVIDIA") != std::string::npos
                 || m_GpuName.find("NVIDIA") != std::string::npos; // fallback si algun driver no lo repite en vendor
}

void SystemStats::Update()
{
    EnsureGpuName();

    double now = glfwGetTime();
    if (m_LastSampleTime != 0.0 && (now - m_LastSampleTime) < kSampleIntervalSeconds)
        return; // throttle: la lectura real del SO cuesta bastante mas que un getter

    m_LastSampleTime = now;
    SampleCpuAndRam();
}

#ifdef _WIN32

static unsigned long long FileTimeToU64(const FILETIME& ft)
{
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void SystemStats::SampleCpuAndRam()
{
    FILETIME idleFt, kernelFt, userFt;
    if (GetSystemTimes(&idleFt, &kernelFt, &userFt))
    {
        unsigned long long idle   = FileTimeToU64(idleFt);
        unsigned long long kernel = FileTimeToU64(kernelFt); // incluye el tiempo idle
        unsigned long long user   = FileTimeToU64(userFt);

        if (m_HasPrevCpuSample)
        {
            unsigned long long idleDelta  = idle   - m_PrevIdle;
            unsigned long long totalDelta = (kernel - m_PrevKernel) + (user - m_PrevUser);

            m_CpuPercent = totalDelta > 0
                ? static_cast<float>(100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta)))
                : 0.0f;
        }

        m_PrevIdle   = idle;
        m_PrevKernel = kernel;
        m_PrevUser   = user;
        m_HasPrevCpuSample = true;
    }

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem))
    {
        m_RamTotalMB = static_cast<float>(mem.ullTotalPhys / (1024.0 * 1024.0));
        m_RamUsedMB  = m_RamTotalMB - static_cast<float>(mem.ullAvailPhys / (1024.0 * 1024.0));
    }
}

#else // Linux

void SystemStats::SampleCpuAndRam()
{
    // Primera linea de /proc/stat: "cpu  user nice system idle iowait irq softirq steal guest guest_nice"
    if (FILE* f = fopen("/proc/stat", "r"))
    {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        int n = fscanf(f, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
                        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        fclose(f);

        if (n == 8)
        {
            unsigned long long idleAll = idle + iowait;
            unsigned long long total   = user + nice + system + idleAll + irq + softirq + steal;

            if (m_HasPrevCpuSample)
            {
                unsigned long long idleDelta  = idleAll - m_PrevIdle;
                unsigned long long totalDelta = total   - m_PrevTotal;

                m_CpuPercent = totalDelta > 0
                    ? static_cast<float>(100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta)))
                    : 0.0f;
            }

            m_PrevIdle  = idleAll;
            m_PrevTotal = total;
            m_HasPrevCpuSample = true;
        }
    }

    // /proc/meminfo: MemTotal/MemAvailable en kB
    if (FILE* f = fopen("/proc/meminfo", "r"))
    {
        char line[256];
        unsigned long long memTotalKb = 0, memAvailKb = 0;
        while (fgets(line, sizeof(line), f))
        {
            if (std::strncmp(line, "MemTotal:", 9) == 0)
                sscanf(line + 9, "%llu", &memTotalKb);
            else if (std::strncmp(line, "MemAvailable:", 13) == 0)
                sscanf(line + 13, "%llu", &memAvailKb);
        }
        fclose(f);

        m_RamTotalMB = static_cast<float>(memTotalKb / 1024.0);
        m_RamUsedMB  = static_cast<float>((memTotalKb - memAvailKb) / 1024.0);
    }
}

#endif

} // namespace ProyecThor::Core
