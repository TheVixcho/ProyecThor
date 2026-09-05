#pragma once

#include <string>
#include <vector>
#include "BibleTypes.h"

namespace ProyecThor::UI::BibleBooks {

const char* GetCanonicalBookName(int canonicalNumber);

const char* GetBookShortAbbrev(int canonicalNumber);

int GetBookCount();

int ResolveAbbrev(const std::string& normalizedText);

std::vector<int> FindBookCandidates(const std::string& normalizedPrefix);

BibleSection GetBookSection(int canonicalNumber);
void GetSectionColor(BibleSection section, float& r, float& g, float& b);

}
