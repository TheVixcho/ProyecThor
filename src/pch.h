#pragma once
#ifdef __cplusplus

// C++ Standard Library (Headers más pesados y comunes)
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <map>
#include <unordered_map>
#include <set>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <regex>

#if !defined(_WIN32)
#ifndef strncpy_s
inline void strncpy_s(char* dest, const char* src, size_t count) {
    if (!dest || count == 0) return;
    std::strncpy(dest, src ? src : "", count);
    dest[count] = '\0';
}
#endif
#endif

// ImGui, JSON y GLM
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#endif

