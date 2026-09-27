#pragma once

#include <string>
#include <imgui.h>

namespace ProyecThor::UI::TextUtils {

std::string ToLowerUTF8(const std::string& s);

std::string StripAccents(const std::string& s);

std::string Normalize(const std::string& s);

std::string Utf8SafeSubstr(const std::string& s, size_t maxChars);

ImU32 Col(float r, float g, float b, float a = 1.0f);

}
