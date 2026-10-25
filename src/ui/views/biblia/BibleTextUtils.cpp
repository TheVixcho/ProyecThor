#include "BibleTextUtils.h"

#include <algorithm>
#include <cctype>

namespace ProyecThor::UI::TextUtils {

std::string ToLowerUTF8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out += static_cast<char>(std::tolower(c));
            i++;
        } else if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            if (c2 >= 0x80 && c2 <= 0x96) {
                out += static_cast<char>(0xC3);
                out += static_cast<char>(c2 + 0x20);
            } else if (c2 >= 0x98 && c2 <= 0x9E) {
                out += static_cast<char>(0xC3);
                out += static_cast<char>(c2 + 0x20);
            } else {
                out += static_cast<char>(c);
                out += static_cast<char>(c2);
            }
            i += 2;
        } else {
            out += static_cast<char>(c);
            i++;
        }
    }
    return out;
}

std::string StripAccents(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            i++;
        } else if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            char rep = 0;
            switch (n) {
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                    rep = 'a'; break;
                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                case 0x88: case 0x89: case 0x8A: case 0x8B:
                    rep = 'e'; break;
                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                    rep = 'i'; break;
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                    rep = 'o'; break;
                case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                case 0x99: case 0x9A: case 0x9B: case 0x9C:
                    rep = 'u'; break;
                case 0xB1: case 0x91:
                    rep = 'n'; break;
                case 0xA7: case 0x87:
                    rep = 'c'; break;
                default:
                    break;
            }
            if (rep != 0) {
                out += rep;
            } else {
                out += static_cast<char>(c);
                out += static_cast<char>(n);
            }
            i += 2;
        } else {
            if (c >= 0xE0 && c <= 0xE5) out += 'a';
            else if (c >= 0xE8 && c <= 0xEB) out += 'e';
            else if (c >= 0xEC && c <= 0xEF) out += 'i';
            else if (c >= 0xF2 && c <= 0xF6) out += 'o';
            else if (c >= 0xF9 && c <= 0xFC) out += 'u';
            else if (c == 0xF1) out += 'n';
            else if (c >= 0xC0 && c <= 0xC5) out += 'a';
            else if (c >= 0xC8 && c <= 0xCB) out += 'e';
            else if (c >= 0xCC && c <= 0xCF) out += 'i';
            else if (c >= 0xD2 && c <= 0xD6) out += 'o';
            else if (c >= 0xD9 && c <= 0xDC) out += 'u';
            else if (c == 0xD1) out += 'n';
            else out += static_cast<char>(c);
            i++;
        }
    }
    return out;
}

std::string Normalize(const std::string& s) {
    std::string r = StripAccents(ToLowerUTF8(s));
    r.erase(std::remove_if(r.begin(), r.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '.' || c == ',' || c == '-' || c == '_';
    }), r.end());
    return r;
}

std::string Utf8SafeSubstr(const std::string& s, size_t maxChars) {
    if (s.empty() || maxChars == 0) return "";
    size_t charCount = 0;
    size_t byteIdx = 0;
    while (byteIdx < s.size() && charCount < maxChars) {
        unsigned char c = static_cast<unsigned char>(s[byteIdx]);
        if (c < 0x80) byteIdx += 1;
        else if ((c & 0xE0) == 0xC0) byteIdx += std::min<size_t>(2, s.size() - byteIdx);
        else if ((c & 0xF0) == 0xE0) byteIdx += std::min<size_t>(3, s.size() - byteIdx);
        else if ((c & 0xF8) == 0xF0) byteIdx += std::min<size_t>(4, s.size() - byteIdx);
        else byteIdx += 1;
        charCount++;
    }
    return s.substr(0, byteIdx);
}

ImU32 Col(float r, float g, float b, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

}
