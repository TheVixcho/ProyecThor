#pragma once

#include <string>
#include <cstdio>
#include <cctype>
#include <vector>

#include <imgui.h>
#include <GL/gl.h>

#include "stb_image.h"

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <commdlg.h>
#endif

#include "backend/core/AppPaths.h"

namespace ProyecThor::Audio {

#ifdef _WIN32

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), size);
    return wide;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                   nullptr, 0, nullptr, nullptr);
    std::string utf8(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                        utf8.data(), size, nullptr, nullptr);
    return utf8;
}

inline std::string PathToVLCUri(const std::string& utf8path) {
    std::string uri = "file:///";
    for (unsigned char c : utf8path) {
        if (c == '\\') {
            uri += '/';
        } else if (std::isalnum(c) || c == '/' || c == '.'
                || c == '-' || c == '_' || c == ':') {
            uri += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            uri += buf;
        }
    }
    return uri;
}

#else

inline std::string WideToUtf8(const std::string& s) { return s; }
inline std::string Utf8ToWide(const std::string& s)  { return s; }

#endif

inline ImTextureID LoadTextureFromMemory(const unsigned char* data, int size) {
    if (!data || size <= 0) return (ImTextureID)0;

    int width, height, channels;
    unsigned char* image_data = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!image_data) return (ImTextureID)0;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    stbi_image_free(image_data);

    return (ImTextureID)(intptr_t)texture_id;
}

inline const std::string& GetAudioPath() {
    static std::string s_Path;
    if (s_Path.empty())
        s_Path = ProyecThor::GetAssetsPath() + "/audio";
    return s_Path;
}

}
