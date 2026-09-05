#pragma once

#include <string>
#include "BibleTypes.h"

namespace ProyecThor::UI::XmlIO {

bool LoadBible(const std::string& path, BibleData& outBible);

bool SaveBible(const std::string& path, const BibleData& bible);

}
