#pragma once

#include <string>
#include "BibleTypes.h"

namespace ProyecThor::UI::Search {

bool ParseSmartQuery(const BibleData& bible, const std::string& rawQuery,
                      int& outBook, int& outChap, int& outVerse);

}
