#pragma once

#include <string>
#include <vector>

namespace ProyecThor::Core {

struct SmartTvDevice {
    std::string name;
    std::string manufacturer;
    std::string ip;
    int         port = 0;
    std::string locationUrl;
    std::string avTransportControlUrl; // relative or absolute path on ip:port
};

class SmartTvCaster {
public:
    // Sends SSDP M-SEARCH on LAN and parses UPnP MediaRenderer devices (Samsung, LG, Sony, Kodi, etc.)
    static std::vector<SmartTvDevice> DiscoverDevices(int timeoutMs = 1600);

    // Direct unicast SSDP probe to a specific Smart TV IP address
    static bool ProbeManualIp(const std::string& ip, SmartTvDevice& outDev, std::string& outErr);

    // Casts an HTTP MJPEG / JPEG stream URL to a DLNA/UPnP Smart TV via AVTransport SetAVTransportURI + Play
    static bool CastStreamToTv(const SmartTvDevice& tv,
                               const std::string& streamUrl,
                               const std::string& title,
                               bool isMjpeg,
                               std::string& outErr);

    // Sends AVTransport Stop to the Smart TV
    static bool StopCastOnTv(const SmartTvDevice& tv, std::string& outErr);

    // Lists all non-loopback local IPv4 addresses on this machine
    static std::vector<std::string> GetLocalIPv4Addresses();
};

} // namespace ProyecThor::Core
