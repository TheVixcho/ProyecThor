
#include "AudioAlbumArt.h"
#include "audio/AudioHelpers.h"

#include "frontend/panels/stb_image.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace fs = std::filesystem;

namespace ProyecThor::Audio {

static std::ifstream OpenBinaryFile(const std::string& utf8Path)
{
#ifdef _WIN32
    std::wstring widePath = ProyecThor::Audio::Utf8ToWide(utf8Path);
    return std::ifstream(fs::path(widePath), std::ios::binary);
#else
    return std::ifstream(utf8Path, std::ios::binary);
#endif
}

static uint32_t ReadBE32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}

static uint32_t ReadLE32(const uint8_t* p)
{
    return  static_cast<uint32_t>(p[0])        |
           (static_cast<uint32_t>(p[1]) <<  8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static const std::string kB64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::vector<uint8_t> Base64Decode(const std::string& in)
{
    std::vector<uint8_t> out;
    out.reserve(in.size() * 3 / 4);
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        auto pos = kB64Chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static bool DecodeImageBytes(const uint8_t* data, size_t size, AlbumArt& out)
{
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &ch, 4);

    if (!pixels) {
        std::cerr << "[AlbumArt] Error stbi: " << stbi_failure_reason() << std::endl;
        return false;
    }

    out.width  = w;
    out.height = h;
    out.pixels.assign(pixels, pixels + (w * h * 4));
    stbi_image_free(pixels);
    return true;
}

static void DecodePictureBlock(const uint8_t* data, size_t size, AlbumArt& out)
{
    if (size < 32) return;

    const uint8_t* p   = data;
    const uint8_t* end = data + size;

    p += 4;
    if (p + 4 > end) return;

    uint32_t mimeLen = ReadBE32(p); p += 4;
    if (p + mimeLen > end) return;
    p += mimeLen;

    if (p + 4 > end) return;
    uint32_t descLen = ReadBE32(p); p += 4;
    if (p + descLen > end) return;
    p += descLen;

    p += 16;
    if (p + 4 > end) return;

    uint32_t dataLen = ReadBE32(p); p += 4;
    if (p + dataLen > end) return;

    DecodeImageBytes(p, dataLen, out);
}

static AlbumArt ExtractMP3Cover(const std::string& utf8Path)
{
    AlbumArt art;
    std::ifstream file = OpenBinaryFile(utf8Path);
    if (!file.is_open()) return art;

    uint8_t header[10];
    file.read(reinterpret_cast<char*>(header), 10);
    if (file.gcount() != 10) return art;
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') return art;

    uint8_t majorVersion = header[3];
    uint8_t flags         = header[5];
    bool    extendedHeader = (flags & 0x40) != 0;

    uint32_t tagSize =
        (static_cast<uint32_t>(header[6]) << 21) |
        (static_cast<uint32_t>(header[7]) << 14) |
        (static_cast<uint32_t>(header[8]) <<  7) |
         static_cast<uint32_t>(header[9]);

    std::vector<uint8_t> tagData(tagSize);
    file.read(reinterpret_cast<char*>(tagData.data()), tagSize);
    std::streamsize readBytes = file.gcount();
    if (readBytes < 0) return art;
    if (static_cast<uint32_t>(readBytes) != tagSize)
        tagData.resize(static_cast<size_t>(readBytes));

    size_t pos = 0;

    if (extendedHeader && pos + 4 <= tagData.size()) {
        uint32_t extSize =
            (static_cast<uint32_t>(tagData[pos]) << 21) |
            (static_cast<uint32_t>(tagData[pos + 1]) << 14) |
            (static_cast<uint32_t>(tagData[pos + 2]) <<  7) |
             static_cast<uint32_t>(tagData[pos + 3]);
        pos += extSize;
    }

    while (pos < tagData.size()) {
        std::string frameId;
        uint32_t    frameSize      = 0;
        size_t      frameHeaderLen = 0;

        if (majorVersion == 2) {
            if (pos + 6 > tagData.size()) break;
            frameId.assign(reinterpret_cast<const char*>(&tagData[pos]), 3);
            frameSize = (static_cast<uint32_t>(tagData[pos + 3]) << 16) |
                        (static_cast<uint32_t>(tagData[pos + 4]) <<  8) |
                         static_cast<uint32_t>(tagData[pos + 5]);
            frameHeaderLen = 6;
        } else {
            if (pos + 10 > tagData.size()) break;
            frameId.assign(reinterpret_cast<const char*>(&tagData[pos]), 4);
            if (frameId[0] == '\0') break;

            if (majorVersion >= 4) {
                frameSize =
                    (static_cast<uint32_t>(tagData[pos + 4]) << 21) |
                    (static_cast<uint32_t>(tagData[pos + 5]) << 14) |
                    (static_cast<uint32_t>(tagData[pos + 6]) <<  7) |
                     static_cast<uint32_t>(tagData[pos + 7]);
            } else {
                frameSize = ReadBE32(&tagData[pos + 4]);
            }
            frameHeaderLen = 10;
        }

        size_t contentStart = pos + frameHeaderLen;
        if (frameHeaderLen == 0 || contentStart + frameSize > tagData.size()) break;

        bool isPicture = (majorVersion == 2 && frameId == "PIC") ||
                         (majorVersion != 2 && frameId == "APIC");

        if (isPicture && frameSize > 0) {
            const uint8_t* p   = &tagData[contentStart];
            const uint8_t* end = p + frameSize;

            uint8_t textEncoding = p[0];
            p += 1;

            if (majorVersion == 2) {
                p += 3;
            } else {
                while (p < end && *p != 0x00) p++;
                if (p < end) p++;
            }

            if (p < end) p++;

            bool doubleNullTerminator = (textEncoding == 1 || textEncoding == 2);
            if (doubleNullTerminator) {
                while (p + 1 < end && !(p[0] == 0x00 && p[1] == 0x00)) p += 2;
                if (p + 1 < end) p += 2;
            } else {
                while (p < end && *p != 0x00) p++;
                if (p < end) p++;
            }

            if (p < end)
                DecodeImageBytes(p, static_cast<size_t>(end - p), art);
        }

        pos = contentStart + frameSize;
        if (art.HasData()) break;
    }

    return art;
}

static AlbumArt ExtractFLACCover(const std::string& utf8Path)
{
    AlbumArt art;
    std::ifstream file = OpenBinaryFile(utf8Path);
    if (!file.is_open()) return art;

    char magic[4];
    file.read(magic, 4);
    if (file.gcount() != 4 || std::memcmp(magic, "fLaC", 4) != 0) return art;

    const uint8_t kPictureBlockType = 6;

    while (file.good()) {
        uint8_t blockHeader[4];
        file.read(reinterpret_cast<char*>(blockHeader), 4);
        if (file.gcount() != 4) break;

        bool     isLast    = (blockHeader[0] & 0x80) != 0;
        uint8_t  blockType = blockHeader[0] & 0x7F;
        uint32_t blockSize = (static_cast<uint32_t>(blockHeader[1]) << 16) |
                              (static_cast<uint32_t>(blockHeader[2]) <<  8) |
                               static_cast<uint32_t>(blockHeader[3]);

        if (blockType == kPictureBlockType) {
            std::vector<uint8_t> block(blockSize);
            file.read(reinterpret_cast<char*>(block.data()), blockSize);
            if (static_cast<uint32_t>(file.gcount()) == blockSize)
                DecodePictureBlock(block.data(), block.size(), art);
            if (art.HasData()) break;
        } else {
            file.seekg(blockSize, std::ios::cur);
        }

        if (isLast) break;
    }

    return art;
}

static void ParseVorbisCommentForPicture(const uint8_t* data, size_t size, AlbumArt& out)
{
    if (size < 8) return;

    const uint8_t* p   = data;
    const uint8_t* end = data + size;

    uint32_t vendorLen = ReadLE32(p); p += 4;
    if (p + vendorLen > end) return;
    p += vendorLen;

    if (p + 4 > end) return;
    uint32_t commentCount = ReadLE32(p); p += 4;

    static const std::string kKey = "METADATA_BLOCK_PICTURE=";

    for (uint32_t i = 0; i < commentCount && p + 4 <= end; i++) {
        uint32_t commentLen = ReadLE32(p); p += 4;
        if (p + commentLen > end) break;

        if (commentLen > kKey.size() &&
            std::memcmp(p, kKey.data(), kKey.size()) == 0) {

            const char* valueStart = reinterpret_cast<const char*>(p) + kKey.size();
            size_t      valueLen   = commentLen - kKey.size();
            std::string base64Value(valueStart, valueLen);

            std::vector<uint8_t> binary = Base64Decode(base64Value);
            if (!binary.empty())
                DecodePictureBlock(binary.data(), binary.size(), out);

            if (out.HasData()) return;
        }

        p += commentLen;
    }
}

static AlbumArt ExtractOggCover(const std::string& utf8Path)
{
    AlbumArt art;
    std::ifstream file = OpenBinaryFile(utf8Path);
    if (!file.is_open()) return art;

    std::vector<uint8_t> packet;
    bool     packetStarted    = false;
    uint32_t trackedSerial    = 0;
    bool     serialKnown      = false;
    int      packetsChecked   = 0;
    const int kMaxPacketsToCheck = 8;

    while (file.good() && packetsChecked < kMaxPacketsToCheck) {
        uint8_t pageHeader[27];
        file.read(reinterpret_cast<char*>(pageHeader), 27);
        if (file.gcount() != 27) break;
        if (std::memcmp(pageHeader, "OggS", 4) != 0) break;

        uint32_t serial = static_cast<uint32_t>(pageHeader[14])        |
                          (static_cast<uint32_t>(pageHeader[15]) <<  8) |
                          (static_cast<uint32_t>(pageHeader[16]) << 16) |
                          (static_cast<uint32_t>(pageHeader[17]) << 24);

        uint8_t segmentCount = pageHeader[26];
        std::vector<uint8_t> segmentTable(segmentCount);
        if (segmentCount > 0) {
            file.read(reinterpret_cast<char*>(segmentTable.data()), segmentCount);
            if (static_cast<uint8_t>(file.gcount()) != segmentCount) break;
        }

        if (!serialKnown) { trackedSerial = serial; serialKnown = true; }
        bool belongsToTrackedStream = (serial == trackedSerial);

        bool readError = false;
        for (uint8_t seg : segmentTable) {
            std::vector<uint8_t> segData(seg);
            if (seg > 0) {
                file.read(reinterpret_cast<char*>(segData.data()), seg);
                if (static_cast<uint8_t>(file.gcount()) != seg) { readError = true; break; }
            }

            if (belongsToTrackedStream) {
                packet.insert(packet.end(), segData.begin(), segData.end());
                packetStarted = true;
            }

            if (seg < 255) {
                if (belongsToTrackedStream && packetStarted) {
                    packetsChecked++;

                    bool isVorbisComment = packet.size() > 7 &&
                        packet[0] == 0x03 &&
                        std::memcmp(&packet[1], "vorbis", 6) == 0;
                    bool isOpusComment = packet.size() > 8 &&
                        std::memcmp(packet.data(), "OpusTags", 8) == 0;

                    if (isVorbisComment || isOpusComment) {
                        size_t offset = isVorbisComment ? 7 : 8;
                        ParseVorbisCommentForPicture(packet.data() + offset,
                                                      packet.size() - offset, art);
                        if (art.HasData()) return art;
                    }
                }
                packet.clear();
                packetStarted = false;
            }
        }

        if (readError) break;
    }

    return art;
}

static bool ReadBoxHeader(std::ifstream& file, uint64_t& boxSize,
                          std::string& boxType, uint64_t& headerLen)
{
    uint8_t sizeBytes[4];
    file.read(reinterpret_cast<char*>(sizeBytes), 4);
    if (file.gcount() != 4) return false;

    char typeBytes[4];
    file.read(typeBytes, 4);
    if (file.gcount() != 4) return false;

    uint64_t size32 = ReadBE32(sizeBytes);
    boxType.assign(typeBytes, 4);
    headerLen = 8;

    if (size32 == 1) {
        uint8_t extended[8];
        file.read(reinterpret_cast<char*>(extended), 8);
        if (file.gcount() != 8) return false;
        boxSize = 0;
        for (int i = 0; i < 8; i++) boxSize = (boxSize << 8) | extended[i];
        headerLen += 8;
    } else {
        boxSize = size32;
    }
    return true;
}

static bool FindMP4CoverBox(std::ifstream& file, uint64_t rangeEnd, AlbumArt& out, int depth)
{
    if (depth > 8) return false;

    while (true) {
        uint64_t curPos = static_cast<uint64_t>(file.tellg());
        if (curPos >= rangeEnd) break;

        uint64_t    boxSize   = 0;
        uint64_t    headerLen = 0;
        std::string boxType;
        if (!ReadBoxHeader(file, boxSize, boxType, headerLen)) break;

        if (boxSize == 0) boxSize = rangeEnd - curPos;
        if (boxSize < headerLen) break;

        uint64_t boxEnd = curPos + boxSize;
        if (boxEnd > rangeEnd) boxEnd = rangeEnd;

        if (boxType == "moov" || boxType == "udta" || boxType == "ilst" || boxType == "covr") {
            if (FindMP4CoverBox(file, boxEnd, out, depth + 1)) return true;
        } else if (boxType == "meta") {
            file.seekg(static_cast<std::streamoff>(curPos + headerLen + 4), std::ios::beg);
            if (FindMP4CoverBox(file, boxEnd, out, depth + 1)) return true;
        } else if (boxType == "data") {
            uint64_t payloadStart = curPos + headerLen + 8;
            uint64_t payloadSize  = (boxEnd > payloadStart) ? (boxEnd - payloadStart) : 0;
            if (payloadSize > 0) {
                std::vector<uint8_t> imageData(payloadSize);
                file.seekg(static_cast<std::streamoff>(payloadStart), std::ios::beg);
                file.read(reinterpret_cast<char*>(imageData.data()),
                          static_cast<std::streamsize>(payloadSize));
                if (static_cast<uint64_t>(file.gcount()) == payloadSize) {
                    DecodeImageBytes(imageData.data(), imageData.size(), out);
                    if (out.HasData()) return true;
                }
            }
        }

        file.seekg(static_cast<std::streamoff>(boxEnd), std::ios::beg);
        if (out.HasData()) return true;
    }
    return false;
}

static AlbumArt ExtractMP4Cover(const std::string& utf8Path)
{
    AlbumArt art;
    std::ifstream file = OpenBinaryFile(utf8Path);
    if (!file.is_open()) return art;

    file.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    FindMP4CoverBox(file, fileSize, art, 0);
    return art;
}

AlbumArt ExtractAlbumArt(const std::string& utf8FilePath)
{
    AlbumArt art;
    std::string ext;

    auto pos = utf8FilePath.rfind('.');
    if (pos != std::string::npos) {
        ext = utf8FilePath.substr(pos + 1);
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    }

    try {
        if (ext == "mp3") {
            art = ExtractMP3Cover(utf8FilePath);
        } else if (ext == "flac") {
            art = ExtractFLACCover(utf8FilePath);
        } else if (ext == "ogg" || ext == "oga" || ext == "opus") {
            art = ExtractOggCover(utf8FilePath);
        } else if (ext == "m4a" || ext == "mp4") {
            art = ExtractMP4Cover(utf8FilePath);
        }
    } catch (const std::exception& e) {
        std::cerr << "[AlbumArt] Excepción al leer portada: " << e.what() << std::endl;
    }

    if (!art.HasData())
        std::cout << "[AlbumArt] No se pudo extraer portada de: " << utf8FilePath << std::endl;
    else
        std::cout << "[AlbumArt] Portada extraida con exito de: " << utf8FilePath << std::endl;

    return art;
}

void UploadAlbumArtToGL(AlbumArt& art)
{
    if (!art.HasData()) return;

    if (art.texID != 0) {
        glDeleteTextures(1, &art.texID);
        art.texID = 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 art.width, art.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, art.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    art.texID = tex;

    art.pixels.clear();
    art.pixels.shrink_to_fit();
}

void FreeAlbumArtTexture(AlbumArt& art)
{
    if (art.texID != 0) {
        glDeleteTextures(1, &art.texID);
        art.texID = 0;
    }
    art.pixels.clear();
    art.width  = 0;
    art.height = 0;
}

}
