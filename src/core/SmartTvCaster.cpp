#include "SmartTvCaster.h"
#include "httplib.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <set>
#include <sstream>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   pragma comment(lib, "ws2_32.lib")
using SocketHandle = SOCKET;
static constexpr SocketHandle kInvalidSock = INVALID_SOCKET;
static void CloseSocket(SocketHandle s) { closesocket(s); }
#else
#   include <arpa/inet.h>
#   include <ifaddrs.h>
#   include <netinet/in.h>
#   include <sys/select.h>
#   include <sys/socket.h>
#   include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle kInvalidSock = -1;
static void CloseSocket(SocketHandle s) { close(s); }
#endif

namespace ProyecThor::Core {

namespace {

std::string ToLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string TrimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string ExtractXmlTag(const std::string& xml, const std::string& tag) {
    std::string open  = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    auto p1 = xml.find(open);
    if (p1 == std::string::npos) {
        // Try case-insensitive search
        std::string lowXml = ToLowerStr(xml);
        p1 = lowXml.find(ToLowerStr(open));
        if (p1 == std::string::npos) return "";
        auto p2 = lowXml.find(ToLowerStr(close), p1 + open.size());
        if (p2 == std::string::npos) return "";
        return TrimStr(xml.substr(p1 + open.size(), p2 - (p1 + open.size())));
    }
    auto p2 = xml.find(close, p1 + open.size());
    if (p2 == std::string::npos) return "";
    return TrimStr(xml.substr(p1 + open.size(), p2 - (p1 + open.size())));
}

bool ParseHttpUrl(const std::string& url, std::string& host, int& port, std::string& path) {
    std::string u = TrimStr(url);
    const std::string prefix = "http://";
    if (u.rfind(prefix, 0) != 0) return false;
    u = u.substr(prefix.size());

    auto slash = u.find('/');
    std::string hostPort = (slash == std::string::npos) ? u : u.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : u.substr(slash);

    auto colon = hostPort.find(':');
    if (colon == std::string::npos) {
        host = hostPort;
        port = 80;
    } else {
        host = hostPort.substr(0, colon);
        try { port = std::stoi(hostPort.substr(colon + 1)); } catch (...) { port = 80; }
    }
    return !host.empty() && port > 0;
}

std::string XmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (char c : in) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}

bool FetchDeviceDescription(const std::string& locationUrl, SmartTvDevice& outDev) {
    std::string host, path;
    int port = 80;
    if (!ParseHttpUrl(locationUrl, host, port, path)) return false;

    httplib::Client cli(host, port);
    cli.set_connection_timeout(1, 500000);
    cli.set_read_timeout(2, 0);

    auto res = cli.Get(path);
    if (!res || res->status < 200 || res->status >= 300) return false;

    const std::string& xml = res->body;
    // Find AVTransport service block
    std::string lowXml = ToLowerStr(xml);
    auto avPos = lowXml.find("avtransport");
    if (avPos == std::string::npos) return false;

    // Locate enclosing <service>...</service> around AVTransport
    size_t svcStart = lowXml.rfind("<service>", avPos);
    size_t svcEnd   = lowXml.find("</service>", avPos);
    if (svcStart == std::string::npos || svcEnd == std::string::npos) return false;

    std::string svcXml = xml.substr(svcStart, svcEnd - svcStart);
    std::string ctrlUrl = ExtractXmlTag(svcXml, "controlURL");
    if (ctrlUrl.empty()) return false;

    // Normalize controlURL relative to root
    if (ctrlUrl.rfind("http://", 0) == 0) {
        std::string ch, cp;
        int cport = port;
        if (ParseHttpUrl(ctrlUrl, ch, cport, cp)) {
            host = ch;
            port = cport;
            ctrlUrl = cp;
        }
    } else if (!ctrlUrl.empty() && ctrlUrl[0] != '/') {
        ctrlUrl = "/" + ctrlUrl;
    }

    outDev.name         = ExtractXmlTag(xml, "friendlyName");
    outDev.manufacturer = ExtractXmlTag(xml, "manufacturer");
    if (outDev.name.empty()) outDev.name = "Smart TV (" + host + ")";
    outDev.ip                    = host;
    outDev.port                  = port;
    outDev.locationUrl           = locationUrl;
    outDev.avTransportControlUrl = ctrlUrl;
    return true;
}

std::vector<std::string> SendSsdpSearch(const std::string& targetIp, int timeoutMs) {
    std::vector<std::string> locations;
    std::set<std::string> seen;

#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSock) return locations;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(1900);
    inet_pton(AF_INET, targetIp.c_str(), &dest.sin_addr);

    static const char* kSearchTargets[] = {
        "urn:schemas-upnp-org:device:MediaRenderer:1",
        "urn:schemas-upnp-org:service:AVTransport:1"
    };

    for (const char* st : kSearchTargets) {
        std::ostringstream req;
        req << "M-SEARCH * HTTP/1.1\r\n"
            << "HOST: " << targetIp << ":1900\r\n"
            << "MAN: \"ssdp:discover\"\r\n"
            << "MX: 1\r\n"
            << "ST: " << st << "\r\n\r\n";
        std::string payload = req.str();
        sendto(sock, payload.data(), static_cast<int>(payload.size()), 0,
               reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    char buf[2048];
    while (std::chrono::steady_clock::now() < deadline) {
        auto remMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remMs <= 0) break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        timeval tv{};
        tv.tv_sec  = static_cast<long>(remMs / 1000);
        tv.tv_usec = static_cast<long>((remMs % 1000) * 1000);

        int ready = select(static_cast<int>(sock) + 1, &rfds, nullptr, nullptr, &tv);
        if (ready <= 0) break;

        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        int n = static_cast<int>(recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                          reinterpret_cast<sockaddr*>(&from), &fromLen));
        if (n <= 0) continue;
        buf[n] = '\0';

        std::istringstream iss(buf);
        std::string line;
        while (std::getline(iss, line)) {
            std::string low = ToLowerStr(line);
            if (low.rfind("location:", 0) == 0) {
                std::string loc = TrimStr(line.substr(9));
                if (!loc.empty() && seen.insert(loc).second) {
                    locations.push_back(loc);
                }
            }
        }
    }

    CloseSocket(sock);
    return locations;
}

bool SendSoapAction(const SmartTvDevice& tv,
                    const std::string& action,
                    const std::string& bodyXml,
                    std::string& outErr) {
    if (tv.ip.empty() || tv.port <= 0 || tv.avTransportControlUrl.empty()) {
        outErr = "Dispositivo Smart TV inválido o sin servicio AVTransport.";
        return false;
    }

    httplib::Client cli(tv.ip, tv.port);
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(4, 0);

    std::ostringstream env;
    env << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        << "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        << "<s:Body>" << bodyXml << "</s:Body></s:Envelope>";

    httplib::Headers hdrs = {
        { "SOAPACTION", "\"urn:schemas-upnp-org:service:AVTransport:1#" + action + "\"" },
        { "Connection", "close" }
    };

    auto res = cli.Post(tv.avTransportControlUrl, hdrs, env.str(), "text/xml; charset=\"utf-8\"");
    if (!res) {
        outErr = "No se pudo conectar con " + tv.ip + ":" + std::to_string(tv.port);
        return false;
    }
    if (res->status < 200 || res->status >= 300) {
        outErr = "El TV respondió HTTP " + std::to_string(res->status) + " en " + action;
        return false;
    }
    return true;
}

} // namespace

std::vector<SmartTvDevice> SmartTvCaster::DiscoverDevices(int timeoutMs) {
    std::vector<SmartTvDevice> results;
    std::set<std::string> seenIps;
    auto locs = SendSsdpSearch("239.255.255.250", timeoutMs);
    for (const auto& loc : locs) {
        SmartTvDevice dev;
        if (FetchDeviceDescription(loc, dev)) {
            std::string key = dev.ip + ":" + std::to_string(dev.port);
            if (seenIps.insert(key).second) {
                results.push_back(std::move(dev));
            }
        }
    }
    return results;
}

bool SmartTvCaster::ProbeManualIp(const std::string& rawIp, SmartTvDevice& outDev, std::string& outErr) {
    std::string ip = TrimStr(rawIp);
    if (ip.empty()) {
        outErr = "Ingresa la dirección IP de tu Smart TV (ej. 192.168.1.50).";
        return false;
    }

    // 1. Try direct unicast SSDP M-SEARCH to <ip>:1900
    auto locs = SendSsdpSearch(ip, 1200);
    for (const auto& loc : locs) {
        if (FetchDeviceDescription(loc, outDev)) {
            return true;
        }
    }

    // 2. Fallback: multicast search and match by IP (some TVs ignore unicast SSDP)
    auto multi = DiscoverDevices(1400);
    for (const auto& d : multi) {
        if (d.ip == ip) {
            outDev = d;
            return true;
        }
    }

    outErr = "No se encontró un receptor DLNA/UPnP activo en " + ip +
             ". Si tu TV usa navegador web, abre la URL LAN directamente desde el navegador del TV.";
    return false;
}

bool SmartTvCaster::CastStreamToTv(const SmartTvDevice& tv,
                                   const std::string& streamUrl,
                                   const std::string& title,
                                   bool isMjpeg,
                                   std::string& outErr) {
    std::string mime = isMjpeg ? "video/x-motion-jpeg" : "image/jpeg";
    std::string upnpClass = isMjpeg ? "object.item.videoItem" : "object.item.imageItem.photo";
    std::string protoInfo = "http-get:*:" + mime + ":DLNA.ORG_OP=00;DLNA.ORG_CI=0";

    std::ostringstream didl;
    didl << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
         << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
         << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\">"
         << "<item id=\"0\" parentID=\"-1\" restricted=\"1\">"
         << "<dc:title>" << XmlEscape(title) << "</dc:title>"
         << "<upnp:class>" << upnpClass << "</upnp:class>"
         << "<res protocolInfo=\"" << protoInfo << "\">" << XmlEscape(streamUrl) << "</res>"
         << "</item></DIDL-Lite>";

    std::ostringstream setUriBody;
    setUriBody << "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
               << "<InstanceID>0</InstanceID>"
               << "<CurrentURI>" << XmlEscape(streamUrl) << "</CurrentURI>"
               << "<CurrentURIMetaData>" << XmlEscape(didl.str()) << "</CurrentURIMetaData>"
               << "</u:SetAVTransportURI>";

    if (!SendSoapAction(tv, "SetAVTransportURI", setUriBody.str(), outErr)) {
        return false;
    }

    std::string playBody =
        "<u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
        "<InstanceID>0</InstanceID><Speed>1</Speed></u:Play>";
    return SendSoapAction(tv, "Play", playBody, outErr);
}

bool SmartTvCaster::StopCastOnTv(const SmartTvDevice& tv, std::string& outErr) {
    std::string stopBody =
        "<u:Stop xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
        "<InstanceID>0</InstanceID></u:Stop>";
    return SendSoapAction(tv, "Stop", stopBody, outErr);
}

std::vector<std::string> SmartTvCaster::GetLocalIPv4Addresses() {
    std::vector<std::string> ips;
#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &res) == 0 && res) {
            for (addrinfo* p = res; p; p = p->ai_next) {
                char ip[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr, ip, sizeof(ip));
                std::string s(ip);
                if (!s.empty() && s != "127.0.0.1" && s.rfind("169.254.", 0) != 0) {
                    if (std::find(ips.begin(), ips.end(), s) == ips.end()) ips.push_back(s);
                }
            }
            freeaddrinfo(res);
        }
    }
#else
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
            std::string s(ip);
            if (!s.empty() && s != "127.0.0.1" && s.rfind("169.254.", 0) != 0) {
                if (std::find(ips.begin(), ips.end(), s) == ips.end()) ips.push_back(s);
            }
        }
        freeifaddrs(ifaddr);
    }
#endif
    if (ips.empty()) ips.push_back("127.0.0.1");
    return ips;
}

} // namespace ProyecThor::Core
