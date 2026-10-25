#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace ProyecThor::UI::BibleNames {

struct BibleDisplayInfo {
    std::string shortCode;
    std::string fullTitle;
    std::string langCode;
};

inline std::string ToUpperAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

inline BibleDisplayInfo ParseBibleName(const std::string& rawName) {
    if (rawName.empty()) {
        return { "", "Sin Biblia", "" };
    }

    std::string stem = rawName;
    size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos)
        stem = stem.substr(slash + 1);

    if (stem.size() > 4) {
        std::string ext = stem.substr(stem.size() - 4);
        if (ToUpperAscii(ext) == ".XML")
            stem = stem.substr(0, stem.size() - 4);
    }

    std::string upper = ToUpperAscii(stem);

    // Known standard versions (Beblia / Zefania / OpenSong conventions)
    if (upper == "SPANISHRVR1960BIBLE" || upper == "RVR1960" || upper == "RVR60")
        return { "RVR1960", "Reina-Valera 1960", "ES" };
    if (upper == "SPANISHRVR1995BIBLE" || upper == "RVR1995")
        return { "RVR1995", "Reina-Valera 1995", "ES" };
    if (upper == "SPANISHRVA2015BIBLE" || upper == "SPANISHRVABIBLE" || upper == "RVA")
        return { "RVA", "Reina-Valera Actualizada", "ES" };
    if (upper == "SPANISHLBLABIBLE" || upper == "LBLA")
        return { "LBLA", "La Biblia de las Américas", "ES" };
    if (upper == "SPANISHPDTBIBLE" || upper == "PDT")
        return { "PDT", "Palabra de Dios para Todos", "ES" };
    if (upper == "SPANISHTLABIBLE" || upper == "TLA")
        return { "TLA", "Traducción en Lenguaje Actual", "ES" };
    if (upper == "SPANISHNVIBIBLE" || upper == "NVI")
        return { "NVI", "Nueva Versión Internacional", "ES" };
    if (upper == "SPANISHNTVBIBLE" || upper == "NTV")
        return { "NTV", "Nueva Traducción Viviente", "ES" };
    if (upper == "SPANISHDHHBIBLE" || upper == "DHH")
        return { "DHH", "Dios Habla Hoy", "ES" };

    if (upper == "ENGLISHNIVBIBLE" || upper == "NIV")
        return { "NIV", "New International Version", "EN" };
    if (upper == "ENGLISHTLBIBLE" || upper == "TLB")
        return { "TLB", "The Living Bible", "EN" };
    if (upper == "ENGLISHKJVBIBLE" || upper == "KJV")
        return { "KJV", "King James Version", "EN" };
    if (upper == "ENGLISHNKJVBIBLE" || upper == "NKJV")
        return { "NKJV", "New King James Version", "EN" };
    if (upper == "ENGLISHESVBIBLE" || upper == "ESV")
        return { "ESV", "English Standard Version", "EN" };
    if (upper == "ENGLISHNASBBIBLE" || upper == "NASB")
        return { "NASB", "New American Standard", "EN" };

    if (upper == "PORTUGUESE1969BIBLE" || upper == "ARC1969")
        return { "ARC 1969", "Almeida Rev. e Corrigida", "PT" };
    if (upper == "PORTUGUESEBLTBIBLE" || upper == "BLT")
        return { "BLT", "Bíblia Livre para Todos", "PT" };
    if (upper == "PORTUGUESENVIBIBLE")
        return { "NVI", "Nova Versão Internacional", "PT" };

    // Generic cleanup for custom imported XML Bibles
    std::string lang;
    std::string cleaned = stem;

    auto stripPrefixCI = [&](const char* prefix, const char* code) {
        size_t len = std::char_traits<char>::length(prefix);
        if (cleaned.size() > len && ToUpperAscii(cleaned.substr(0, len)) == ToUpperAscii(prefix)) {
            cleaned = cleaned.substr(len);
            lang = code;
            return true;
        }
        return false;
    };

    if (!stripPrefixCI("Spanish", "ES"))
    if (!stripPrefixCI("Espanol", "ES"))
    if (!stripPrefixCI("English", "EN"))
    if (!stripPrefixCI("Portuguese", "PT"))
    if (!stripPrefixCI("French", "FR"))
    if (!stripPrefixCI("German", "DE"))
        stripPrefixCI("Italian", "IT");

    if (cleaned.size() > 5 && ToUpperAscii(cleaned.substr(cleaned.size() - 5)) == "BIBLE") {
        cleaned = cleaned.substr(0, cleaned.size() - 5);
    }

    for (char& c : cleaned) {
        if (c == '_' || c == '-') c = ' ';
    }

    // Trim spaces
    while (!cleaned.empty() && cleaned.front() == ' ') cleaned.erase(cleaned.begin());
    while (!cleaned.empty() && cleaned.back() == ' ')  cleaned.pop_back();

    if (cleaned.empty()) cleaned = stem;

    std::string code = cleaned;
    if (code.size() > 14) code = code.substr(0, 14);

    return { code, cleaned, lang };
}

} // namespace ProyecThor::UI::BibleNames
