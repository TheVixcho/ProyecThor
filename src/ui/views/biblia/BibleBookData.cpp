#include "BibleBookData.h"
#include "BibleTextUtils.h"

#include <algorithm>

namespace ProyecThor::UI::BibleBooks {

namespace {

const char* kBookNames[] = {
    "Génesis","Éxodo","Levítico","Números","Deuteronomio",
    "Josué","Jueces","Rut","1 Samuel","2 Samuel","1 Reyes","2 Reyes",
    "1 Crónicas","2 Crónicas","Esdras","Nehemías","Ester","Job","Salmos",
    "Proverbios","Eclesiastés","Cantares","Isaías","Jeremías","Lamentaciones",
    "Ezequiel","Daniel","Oseas","Joel","Amós","Abdías","Jonás","Miqueas",
    "Nahúm","Habacuc","Sofonías","Hageo","Zacarías","Malaquías",
    "Mateo","Marcos","Lucas","Juan","Hechos","Romanos","1 Corintios",
    "2 Corintios","Gálatas","Efesios","Filipenses","Colosenses","1 Tesalonicenses",
    "2 Tesalonicenses","1 Timoteo","2 Timoteo","Tito","Filemón","Hebreos",
    "Santiago","1 Pedro","2 Pedro","1 Juan","2 Juan","3 Juan","Judas","Apocalipsis"
};

constexpr int kBookCount = sizeof(kBookNames) / sizeof(kBookNames[0]);

struct AbbrevEntry {
    const char* text;
    int         canonicalNumber;
};

const AbbrevEntry kAbbrevTable[] = {
    {"genesis",1},{"gen",1},{"gn",1},
    {"exodo",2},{"exo",2},{"ex",2},
    {"levitico",3},{"lev",3},{"lv",3},
    {"numeros",4},{"num",4},{"nm",4},
    {"deuteronomio",5},{"deut",5},{"dt",5},
    {"josue",6},{"jos",6},
    {"jueces",7},{"jue",7},{"jc",7},
    {"rut",8},{"rt",8},
    {"1samuel",9},{"1sam",9},{"1s",9},
    {"2samuel",10},{"2sam",10},{"2s",10},
    {"1reyes",11},{"1re",11},{"1r",11},
    {"2reyes",12},{"2re",12},{"2r",12},
    {"1cronicas",13},{"1cro",13},{"1cr",13},
    {"2cronicas",14},{"2cro",14},{"2cr",14},
    {"esdras",15},{"esd",15},
    {"nehemias",16},{"neh",16},
    {"ester",17},{"est",17},
    {"job",18},
    {"salmos",19},{"sal",19},{"ps",19},{"sl",19},
    {"proverbios",20},{"prov",20},{"pr",20},
    {"eclesiastes",21},{"ecl",21},{"qo",21},
    {"cantares",22},{"cnt",22},{"ct",22},{"can",22},
    {"isaias",23},{"isa",23},{"is",23},
    {"jeremias",24},{"jer",24},{"jr",24},
    {"lamentaciones",25},{"lam",25},
    {"ezequiel",26},{"eze",26},{"ez",26},
    {"daniel",27},{"dan",27},{"dn",27},
    {"oseas",28},{"ose",28},{"os",28},
    {"joel",29},{"jl",29},
    {"amos",30},{"am",30},
    {"abdias",31},{"abd",31},{"ab",31},
    {"jonas",32},{"jon",32},
    {"miqueas",33},{"miq",33},{"mi",33},
    {"nahum",34},{"nah",34},
    {"habacuc",35},{"hab",35},
    {"sofonias",36},{"sof",36},
    {"hageo",37},{"hag",37},
    {"zacarias",38},{"zac",38},
    {"malaquias",39},{"mal",39},
    {"mateo",40},{"mat",40},{"mt",40},
    {"marcos",41},{"mar",41},{"mc",41},{"mr",41},
    {"lucas",42},{"luc",42},{"lc",42},
    {"juan",43},{"jn",43},{"jua",43},
    {"hechos",44},{"hch",44},{"hec",44},{"act",44},
    {"romanos",45},{"rom",45},{"ro",45},
    {"1corintios",46},{"1cor",46},{"1co",46},
    {"2corintios",47},{"2cor",47},{"2co",47},
    {"galatas",48},{"gal",48},{"ga",48},
    {"efesios",49},{"efe",49},{"ef",49},
    {"filipenses",50},{"fil",50},{"php",50},
    {"colosenses",51},{"col",51},
    {"1tesalonicenses",52},{"1tes",52},{"1ts",52},
    {"2tesalonicenses",53},{"2tes",53},{"2ts",53},
    {"1timoteo",54},{"1tim",54},{"1ti",54},
    {"2timoteo",55},{"2tim",55},{"2ti",55},
    {"tito",56},{"tit",56},
    {"filemon",57},{"flm",57},{"fm",57},
    {"hebreos",58},{"heb",58},
    {"santiago",59},{"sant",59},{"stg",59},{"sg",59},
    {"1pedro",60},{"1ped",60},{"1pe",60},
    {"2pedro",61},{"2ped",61},{"2pe",61},
    {"1juan",62},{"1jn",62},
    {"2juan",63},{"2jn",63},
    {"3juan",64},{"3jn",64},
    {"judas",65},{"jud",65},
    {"apocalipsis",66},{"apo",66},{"ap",66},{"rev",66},
};

constexpr int kAbbrevCount = sizeof(kAbbrevTable) / sizeof(kAbbrevTable[0]);

const char* kBookShortAbbrevs[] = {
    "Gén","Éxo","Lev","Núm","Deut",
    "Jos","Jue","Rut","1 Sam","2 Sam","1 Rey","2 Rey",
    "1 Cr","2 Cr","Esd","Neh","Est","Job","Sal",
    "Prov","Ecl","Cant","Isa","Jer","Lam",
    "Eze","Dan","Os","Joel","Amós","Abd","Jon","Miq",
    "Nah","Hab","Sof","Hag","Zac","Mal",
    "Mat","Mar","Luc","Juan","Hech","Rom","1 Cor",
    "2 Cor","Gál","Ef","Fil","Col","1 Tes",
    "2 Tes","1 Tim","2 Tim","Tito","Filem","Heb",
    "Sant","1 Pe","2 Pe","1 Jn","2 Jn","3 Jn","Jud","Apoc"
};

bool StartsWith(const std::string& str, const std::string& prefix) {
    return prefix.size() <= str.size()
        && str.compare(0, prefix.size(), prefix) == 0;
}

}

const char* GetCanonicalBookName(int canonicalNumber) {
    if (canonicalNumber < 1 || canonicalNumber > kBookCount) return nullptr;
    return kBookNames[canonicalNumber - 1];
}

const char* GetBookShortAbbrev(int canonicalNumber) {
    if (canonicalNumber < 1 || canonicalNumber > kBookCount) return nullptr;
    return kBookShortAbbrevs[canonicalNumber - 1];
}

int GetBookCount() {
    return kBookCount;
}

int ResolveAbbrev(const std::string& normalizedText) {
    for (int i = 0; i < kAbbrevCount; i++)
        if (normalizedText == kAbbrevTable[i].text) return kAbbrevTable[i].canonicalNumber;
    return 0;
}

std::vector<int> FindBookCandidates(const std::string& normalizedPrefix) {
    std::vector<int> result;
    if (normalizedPrefix.empty()) return result;

    for (int n = 1; n <= kBookCount; n++) {
        std::string normName = TextUtils::Normalize(kBookNames[n - 1]);
        if (StartsWith(normName, normalizedPrefix)) {
            result.push_back(n);
            continue;
        }
        for (int a = 0; a < kAbbrevCount; a++) {
            if (kAbbrevTable[a].canonicalNumber != n) continue;
            if (StartsWith(kAbbrevTable[a].text, normalizedPrefix)) {
                result.push_back(n);
                break;
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

BibleSection GetBookSection(int n) {
    if (n >= 1  && n <= 5)  return BibleSection::Pentateuch;
    if (n >= 6  && n <= 17) return BibleSection::HistoricalOT;
    if (n >= 18 && n <= 22) return BibleSection::Wisdom;
    if (n >= 23 && n <= 27) return BibleSection::MajorProphets;
    if (n >= 28 && n <= 39) return BibleSection::MinorProphets;
    if (n >= 40 && n <= 43) return BibleSection::Gospels;
    if (n == 44)             return BibleSection::Acts;
    if (n >= 45 && n <= 57) return BibleSection::PaulineEpistles;
    if (n >= 58 && n <= 65) return BibleSection::GeneralEpistles;
    return BibleSection::Apocalypse;
}

void GetSectionColor(BibleSection section, float& r, float& g, float& b) {
    switch (section) {
        case BibleSection::Pentateuch:      r=0.95f; g=0.75f; b=0.35f; break;
        case BibleSection::HistoricalOT:    r=0.55f; g=0.80f; b=0.45f; break;
        case BibleSection::Wisdom:          r=0.95f; g=0.85f; b=0.30f; break;
        case BibleSection::MajorProphets:   r=0.75f; g=0.50f; b=0.95f; break;
        case BibleSection::MinorProphets:   r=0.50f; g=0.65f; b=0.95f; break;
        case BibleSection::Gospels:         r=0.30f; g=0.80f; b=0.85f; break;
        case BibleSection::Acts:            r=0.45f; g=0.88f; b=0.60f; break;
        case BibleSection::PaulineEpistles: r=0.95f; g=0.58f; b=0.35f; break;
        case BibleSection::GeneralEpistles: r=0.90f; g=0.50f; b=0.70f; break;
        case BibleSection::Apocalypse:      r=0.95f; g=0.35f; b=0.35f; break;
        default:                            r=0.70f; g=0.70f; b=0.70f; break;
    }
}

}
