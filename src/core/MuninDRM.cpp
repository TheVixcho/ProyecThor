#include "MuninDRM.h"
#include "core/AuthLicenseManager.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <random>
#include <chrono>
#include <mutex>
#include <thread>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::Core::MuninDRM {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Implementación Criptográfica Autónoma (SHA-256 & CRC-32)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

static inline uint32_t RightRotate(uint32_t val, uint32_t bits) {
    return (val >> bits) | (val << (32 - bits));
}

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

static void Sha256Transform(uint32_t state[8], const uint8_t buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t w[64];

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = RightRotate(w[i - 15], 7) ^ RightRotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = RightRotate(w[i - 2], 17) ^ RightRotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = RightRotate(e, 6) ^ RightRotate(e, 11) ^ RightRotate(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = RightRotate(a, 2) ^ RightRotate(a, 13) ^ RightRotate(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void Sha256Init(Sha256Ctx& ctx) {
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.count = 0;
}

static void Sha256Update(Sha256Ctx& ctx, const uint8_t* data, size_t len) {
    size_t index = (size_t)(ctx.count & 63);
    ctx.count += len;
    size_t partLen = 64 - index;
    size_t i = 0;

    if (len >= partLen) {
        std::memcpy(&ctx.buffer[index], data, partLen);
        Sha256Transform(ctx.state, ctx.buffer);
        for (i = partLen; i + 63 < len; i += 64) {
            Sha256Transform(ctx.state, &data[i]);
        }
        index = 0;
    }
    std::memcpy(&ctx.buffer[index], &data[i], len - i);
}

static void Sha256Final(Sha256Ctx& ctx, uint8_t digest[32]) {
    uint8_t bits[8];
    uint64_t bitCount = ctx.count * 8;
    for (int i = 0; i < 8; i++) {
        bits[7 - i] = (uint8_t)((bitCount >> (i * 8)) & 0xff);
    }

    size_t index = (size_t)(ctx.count & 63);
    size_t padLen = (index < 56) ? (56 - index) : (120 - index);
    static const uint8_t padding[64] = { 0x80 };
    Sha256Update(ctx, padding, padLen);
    Sha256Update(ctx, bits, 8);

    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)((ctx.state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = (uint8_t)((ctx.state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = (uint8_t)((ctx.state[i] >> 8)  & 0xff);
        digest[i * 4 + 3] = (uint8_t)(ctx.state[i] & 0xff);
    }
}

static std::vector<uint8_t> CalculateSha256(const uint8_t* data, size_t len) {
    Sha256Ctx ctx;
    Sha256Init(ctx);
    Sha256Update(ctx, data, len);
    std::vector<uint8_t> digest(32);
    Sha256Final(ctx, digest.data());
    return digest;
}

static uint32_t CalculateCRC32(const uint8_t* data, size_t len) {
    static uint32_t crcTable[256];
    static bool tableInit = false;
    if (!tableInit) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
            }
            crcTable[i] = c;
        }
        tableInit = true;
    }

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  2. Gestión de Claves y Anti-Ingeniería Inversa (Ofuscación y Firebase)
// ─────────────────────────────────────────────────────────────────────────────
static std::mutex s_KeyMutex;
static std::string s_RemoteFirebaseKey = "";

void SetRemoteKey(const std::string& key) {
    std::lock_guard<std::mutex> lk(s_KeyMutex);
    s_RemoteFirebaseKey = key;
}

std::string GetRemoteKey() {
    std::lock_guard<std::mutex> lk(s_KeyMutex);
    return s_RemoteFirebaseKey;
}

// Semillas ofuscadas no legibles como cadenas de texto en el binario compilado
static const uint8_t kSeedA[32] = {
    0x9A, 0x12, 0xF4, 0x55, 0xC3, 0x7E, 0x08, 0x31,
    0xBB, 0x4D, 0x6E, 0x89, 0x2A, 0xD7, 0x1C, 0x4F,
    0x58, 0x73, 0xE1, 0x90, 0x64, 0x3B, 0xA2, 0x8C,
    0x0D, 0xF9, 0x25, 0x66, 0x7B, 0x48, 0x1E, 0x82
};

static const uint8_t kSeedB[32] = {
    0xDF, 0x77, 0x8A, 0x14, 0xAF, 0x2B, 0x63, 0x5D,
    0xCA, 0x39, 0x01, 0xE8, 0x4F, 0x93, 0x72, 0x26,
    0x31, 0x1A, 0x88, 0xE4, 0x5D, 0x0C, 0xC7, 0xEF,
    0x6E, 0x92, 0x44, 0x07, 0x1F, 0x29, 0x73, 0x3B
};

static std::vector<uint8_t> DeriveMasterKey() {
    std::vector<uint8_t> blended(64);
    for (size_t i = 0; i < 32; i++) {
        blended[i] = kSeedA[i] ^ kSeedB[i];
    }

    std::string remote = GetRemoteKey();
    if (!remote.empty()) {
        auto rHash = CalculateSha256(reinterpret_cast<const uint8_t*>(remote.data()), remote.size());
        for (size_t i = 0; i < 32; i++) {
            blended[i] ^= rHash[i];
        }
    }

    // Mezclar con la instancia de hardware del usuario si está registrada
    std::string instId = AuthLicenseManager::Get().GetLicenseInfo().instanceId;
    if (instId.empty()) instId = "PT_MUNIN_DRM_V2";
    auto instHash = CalculateSha256(reinterpret_cast<const uint8_t*>(instId.data()), instId.size());
    for (size_t i = 0; i < 32; i++) {
        blended[32 + i] = instHash[i];
    }

    return CalculateSha256(blended.data(), blended.size());
}

// Consulta asíncrona a Firebase Firestore para la clave remota
void SyncWithFirebaseAsync() {
    std::thread([]() {
        auto& auth = AuthLicenseManager::Get();
        std::string apiKey = auth.GetFirebaseApiKey();
        if (apiKey.empty()) return;

#ifdef _WIN32
        HINTERNET hSession = WinHttpOpen(L"ProyecThor-MuninDRM/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return;

        HINTERNET hConnect = WinHttpConnect(hSession, L"firestore.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            std::string path = "/v1/projects/proyecthor/databases/(default)/documents/system/drm?key=" + apiKey;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
            std::wstring wpath(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                                   NULL, NULL,
                                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD dwSize = 0;
                    std::string response;
                    do {
                        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
                        std::vector<char> buf(dwSize + 1);
                        DWORD dwDownloaded = 0;
                        if (WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded)) {
                            response.append(buf.data(), dwDownloaded);
                        }
                    } while (dwSize > 0);

                    // Buscar el campo drm_key si existe en el JSON devuelto
                    size_t p = response.find("\"drm_key\"");
                    if (p != std::string::npos) {
                        size_t strVal = response.find("\"stringValue\":", p);
                        if (strVal != std::string::npos) {
                            size_t q1 = response.find("\"", strVal + 14);
                            if (q1 != std::string::npos) {
                                size_t q2 = response.find("\"", q1 + 1);
                                if (q2 != std::string::npos) {
                                    std::string key = response.substr(q1 + 1, q2 - q1 - 1);
                                    SetRemoteKey(key);
                                }
                            }
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
#endif
    }).detach();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Cifrado y Descifrado PTENC
// ─────────────────────────────────────────────────────────────────────────────
constexpr size_t kHeaderSize = 6 + 16 + 4 + 4; // Magic (6) + Salt (16) + Size (4) + CRC (4) = 30 bytes

bool IsEncrypted(const uint8_t* data, size_t size) {
    if (!data || size < kHeaderSize) return false;
    return (std::memcmp(data, kMagic, sizeof(kMagic)) == 0);
}

bool IsEncryptedFile(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return false;
    char magic[sizeof(kMagic)];
    f.read(magic, sizeof(magic));
    return (f.gcount() == sizeof(kMagic) && std::memcmp(magic, kMagic, sizeof(kMagic)) == 0);
}

bool EncryptBuffer(const std::vector<uint8_t>& plainData, std::vector<uint8_t>& outEncrypted) {
    if (plainData.empty()) return false;

    outEncrypted.clear();
    outEncrypted.resize(kHeaderSize + plainData.size());

    // 1. Magic
    std::memcpy(&outEncrypted[0], kMagic, sizeof(kMagic));

    // 2. Salt aleatorio único por cada archivo cifrado
    uint8_t salt[16];
    {
        std::random_device rd;
        std::mt19937_64 gen(rd() ^ std::chrono::high_resolution_clock::now().time_since_epoch().count());
        uint64_t r1 = gen();
        uint64_t r2 = gen();
        std::memcpy(&salt[0], &r1, 8);
        std::memcpy(&salt[8], &r2, 8);
    }
    std::memcpy(&outEncrypted[6], salt, 16);

    // 3. Tamaño original (uint32_t little-endian)
    uint32_t plainLen = static_cast<uint32_t>(plainData.size());
    std::memcpy(&outEncrypted[22], &plainLen, 4);

    // 4. Checksum CRC-32 de los datos originales
    uint32_t crc = CalculateCRC32(plainData.data(), plainData.size());
    std::memcpy(&outEncrypted[26], &crc, 4);

    // 5. Cifrado mediante flujo de bloques SHA-256 (Counter Mode CTR)
    auto masterKey = DeriveMasterKey();

    std::vector<uint8_t> blockInput(masterKey.size() + 16 + sizeof(uint64_t));
    std::memcpy(&blockInput[0], masterKey.data(), masterKey.size());
    std::memcpy(&blockInput[masterKey.size()], salt, 16);

    size_t processed = 0;
    uint64_t blockIndex = 0;

    while (processed < plainData.size()) {
        std::memcpy(&blockInput[masterKey.size() + 16], &blockIndex, sizeof(uint64_t));
        auto keystream = CalculateSha256(blockInput.data(), blockInput.size());

        size_t chunk = std::min<size_t>(32, plainData.size() - processed);
        for (size_t i = 0; i < chunk; i++) {
            outEncrypted[kHeaderSize + processed + i] = plainData[processed + i] ^ keystream[i];
        }

        processed += chunk;
        blockIndex++;
    }

    return true;
}

bool EncryptFile(const std::string& sourcePlainFile, const std::string& destEncryptedFile) {
    std::ifstream in(sourcePlainFile, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    std::streamsize sz = in.tellg();
    if (sz <= 0) return false;
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> plain(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(plain.data()), sz)) return false;
    in.close();

    std::vector<uint8_t> encrypted;
    if (!EncryptBuffer(plain, encrypted)) return false;

    std::ofstream out(destEncryptedFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    return out.good();
}

bool DecryptBuffer(const uint8_t* encData, size_t encSize, std::vector<uint8_t>& outPlainData) {
    if (!IsEncrypted(encData, encSize)) return false;

    uint8_t salt[16];
    std::memcpy(salt, &encData[6], 16);

    uint32_t plainLen = 0;
    std::memcpy(&plainLen, &encData[22], 4);

    uint32_t expectedCrc = 0;
    std::memcpy(&expectedCrc, &encData[26], 4);

    if (encSize < kHeaderSize + plainLen) return false;

    outPlainData.resize(plainLen);
    auto masterKey = DeriveMasterKey();

    std::vector<uint8_t> blockInput(masterKey.size() + 16 + sizeof(uint64_t));
    std::memcpy(&blockInput[0], masterKey.data(), masterKey.size());
    std::memcpy(&blockInput[masterKey.size()], salt, 16);

    size_t processed = 0;
    uint64_t blockIndex = 0;

    while (processed < plainLen) {
        std::memcpy(&blockInput[masterKey.size() + 16], &blockIndex, sizeof(uint64_t));
        auto keystream = CalculateSha256(blockInput.data(), blockInput.size());

        size_t chunk = std::min<size_t>(32, plainLen - processed);
        for (size_t i = 0; i < chunk; i++) {
            outPlainData[processed + i] = encData[kHeaderSize + processed + i] ^ keystream[i];
        }

        processed += chunk;
        blockIndex++;
    }

    // Verificación de integridad por CRC-32
    uint32_t actualCrc = CalculateCRC32(outPlainData.data(), outPlainData.size());
    if (actualCrc != expectedCrc) {
        outPlainData.clear();
        return false;
    }

    return true;
}

bool DecryptFileToMemory(const std::string& encFilePath, std::vector<uint8_t>& outPlainData) {
    std::ifstream in(encFilePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    std::streamsize sz = in.tellg();
    if (sz < static_cast<std::streamsize>(kHeaderSize)) return false;
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> enc(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(enc.data()), sz)) return false;

    return DecryptBuffer(enc.data(), enc.size(), outPlainData);
}

} // namespace ProyecThor::Core::MuninDRM
