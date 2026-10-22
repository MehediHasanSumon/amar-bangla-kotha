#include "abk/utf8_util.h"
#include <algorithm>
#include <cctype>

namespace abk {
namespace utf8 {

std::vector<char32_t> to_codepoints(const std::string& utf8_str) {
    std::vector<char32_t> codepoints;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(utf8_str.data());
    size_t len = utf8_str.length();
    size_t i = 0;

    while (i < len) {
        unsigned char c = bytes[i];
        if (c < 0x80) {
            codepoints.push_back(static_cast<char32_t>(c));
            i += 1;
        } else if ((c >> 5) == 0x06) {
            if (i + 1 < len && (bytes[i + 1] & 0xC0) == 0x80 && c >= 0xC2) {
                char32_t cp = ((c & 0x1F) << 6) | (bytes[i + 1] & 0x3F);
                codepoints.push_back(cp);
                i += 2;
            } else {
                codepoints.push_back(0xFFFD);
                i += 1;
            }
        } else if ((c >> 4) == 0x0E) {
            if (i + 2 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80) {
                char32_t cp = ((c & 0x0F) << 12) |
                              ((bytes[i + 1] & 0x3F) << 6) |
                              (bytes[i + 2] & 0x3F);
                if ((c == 0xE0 && bytes[i + 1] < 0xA0) || (cp >= 0xD800 && cp <= 0xDFFF)) {
                    codepoints.push_back(0xFFFD);
                } else {
                    codepoints.push_back(cp);
                }
                i += 3;
            } else {
                codepoints.push_back(0xFFFD);
                i += 1;
            }
        } else if ((c >> 3) == 0x1E) {
            if (i + 3 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80 && (bytes[i + 3] & 0xC0) == 0x80) {
                char32_t cp = ((c & 0x07) << 18) |
                              ((bytes[i + 1] & 0x3F) << 12) |
                              ((bytes[i + 2] & 0x3F) << 6) |
                              (bytes[i + 3] & 0x3F);
                if ((c == 0xF0 && bytes[i + 1] < 0x90) || cp > 0x10FFFF) {
                    codepoints.push_back(0xFFFD);
                } else {
                    codepoints.push_back(cp);
                }
                i += 4;
            } else {
                codepoints.push_back(0xFFFD);
                i += 1;
            }
        } else {
            codepoints.push_back(0xFFFD);
            i += 1;
        }
    }
    return codepoints;
}

std::string to_utf8(char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
        out += "\xEF\xBF\xBD"; // U+FFFD Replacement Character
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x110000) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out += "\xEF\xBF\xBD";
    }
    return out;
}

std::string to_utf8(const std::vector<char32_t>& codepoints) {
    std::string out;
    for (char32_t cp : codepoints) {
        out += to_utf8(cp);
    }
    return out;
}

bool is_bengali(char32_t cp) {
    return (cp >= 0x0980 && cp <= 0x09FF);
}

bool is_bengali_vowel(char32_t cp) {
    switch (cp) {
        case 0x0985: // অ
        case 0x0986: // আ
        case 0x0987: // ই
        case 0x0988: // ঈ
        case 0x0989: // উ
        case 0x098A: // ঊ
        case 0x098B: // ঋ
        case 0x098C: // ঌ
        case 0x098F: // এ
        case 0x0990: // ঐ
        case 0x0993: // ও
        case 0x0994: // ঔ
            return true;
        default:
            return false;
    }
}

bool is_bengali_kar(char32_t cp) {
    switch (cp) {
        case 0x09BE: // া
        case 0x09BF: // ি
        case 0x09C0: // ী
        case 0x09C1: // ু
        case 0x09C2: // ূ
        case 0x09C3: // ৃ
        case 0x09C4: // ৄ
        case 0x09C7: // ে
        case 0x09C8: // ৈ
        case 0x09CB: // ো
        case 0x09CC: // ৌ
        case 0x09D7: // ৗ
            return true;
        default:
            return false;
    }
}

bool is_bengali_consonant(char32_t cp) {
    if (cp >= 0x0995 && cp <= 0x09B9) {
        if (cp == 0x09A9 || cp == 0x09B1 || cp == 0x09B3 || cp == 0x09B4 || cp == 0x09B5) {
            return false;
        }
        return true;
    }
    // ড়, ঢ়, য়, ৎ
    return (cp == 0x09DC || cp == 0x09DD || cp == 0x09DF || cp == BN_KHANDA_TA);
}

bool is_bengali_virama(char32_t cp) {
    return cp == BN_VIRAMA;
}

bool is_bengali_modifier(char32_t cp) {
    return (cp == BN_CHANDRABINDU || cp == BN_ANUSVARA || cp == BN_VISARGA || cp == BN_NUKTA);
}

bool is_bengali_digit(char32_t cp) {
    return (cp >= 0x09E6 && cp <= 0x09EF);
}

bool is_zwj(char32_t cp) {
    return cp == ZWJ;
}

bool is_zwnj(char32_t cp) {
    return cp == ZWNJ;
}

char32_t vowel_to_kar(char32_t vowel_cp) {
    switch (vowel_cp) {
        case 0x0986: return 0x09BE; // আ -> া
        case 0x0987: return 0x09BF; // ই -> ি
        case 0x0988: return 0x09C0; // ঈ -> ী
        case 0x0989: return 0x09C1; // উ -> ু
        case 0x098A: return 0x09C2; // ঊ -> ূ
        case 0x098B: return 0x09C3; // ঋ -> ৃ
        case 0x098F: return 0x09C7; // এ -> ে
        case 0x0990: return 0x09C8; // ঐ -> ৈ
        case 0x0993: return 0x09CB; // ও -> ো
        case 0x0994: return 0x09CC; // ঔ -> ৌ
        default: return 0;
    }
}

char32_t kar_to_vowel(char32_t kar_cp) {
    switch (kar_cp) {
        case 0x09BE: return 0x0986; // া -> আ
        case 0x09BF: return 0x0987; // ি -> ই
        case 0x09C0: return 0x0988; // ী -> ঈ
        case 0x09C1: return 0x0989; // ু -> উ
        case 0x09C2: return 0x098A; // ূ -> ঊ
        case 0x09C3: return 0x098B; // ৃ -> ঋ
        case 0x09C7: return 0x098F; // ে -> এ
        case 0x09C8: return 0x0990; // ৈ -> ঐ
        case 0x09CB: return 0x0993; // ো -> ও
        case 0x09CC: return 0x0994; // ৌ -> ঔ
        default: return 0;
    }
}

size_t codepoint_length(const std::string& utf8_str) {
    return to_codepoints(utf8_str).size();
}

std::string to_lower_ascii(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return out;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Phase 3: Unicode Normalization (NFC Enforcement)
std::string normalize_nfc(const std::string& utf8_str) {
    if (utf8_str.empty()) return "";

    std::vector<char32_t> raw_cps = to_codepoints(utf8_str);
    std::vector<char32_t> norm_cps;
    norm_cps.reserve(raw_cps.size());

    // 1. Canonical decomposition step for Bengali:
    // Nukta characters (ড়, ঢ়, য়) decompose to base + nukta in standard Unicode NFC
    for (char32_t cp : raw_cps) {
        if (cp == BN_RRA) { // ড় -> ড + ়
            norm_cps.push_back(BN_DDA);
            norm_cps.push_back(BN_NUKTA);
        } else if (cp == BN_RHA) { // ঢ় -> ঢ + ়
            norm_cps.push_back(BN_DDHA);
            norm_cps.push_back(BN_NUKTA);
        } else if (cp == BN_YAA) { // য় -> য + ়
            norm_cps.push_back(BN_YA);
            norm_cps.push_back(BN_NUKTA);
        } else {
            norm_cps.push_back(cp);
        }
    }

    // 2. Canonical composition step for Bengali:
    // Two-part vowel signs:
    // e-kar (0x09C7) + aa-kar (0x09BE) -> o-kar (0x09CB ো)
    // e-kar (0x09C7) + au-length (0x09D7) -> au-kar (0x09CC ৌ)
    std::vector<char32_t> comp_cps;
    comp_cps.reserve(norm_cps.size());

    size_t i = 0;
    size_t len = norm_cps.size();
    while (i < len) {
        if (norm_cps[i] == BN_KAR_E && i + 1 < len) {
            if (norm_cps[i + 1] == BN_KAR_AA) {
                comp_cps.push_back(BN_KAR_O); // ো
                i += 2;
                continue;
            } else if (norm_cps[i + 1] == BN_AU_LENGTH) {
                comp_cps.push_back(BN_KAR_AU); // ৌ
                i += 2;
                continue;
            }
        }
        comp_cps.push_back(norm_cps[i]);
        i++;
    }

    // 3. Reorder combining marks (Nukta CCC 7 precedes Virama CCC 9 and Kars)
    // If a Nukta was placed after a Kar (e.g. ক + ি + ়), reorder to ক + ় + ি
    for (size_t k = 1; k < comp_cps.size(); ++k) {
        if (comp_cps[k] == BN_NUKTA) {
            // Swap backwards past any Kars or Viramas to attach directly to consonant
            size_t curr = k;
            while (curr > 0 && (is_bengali_kar(comp_cps[curr - 1]) || is_bengali_virama(comp_cps[curr - 1]))) {
                std::swap(comp_cps[curr], comp_cps[curr - 1]);
                curr--;
            }
        }
    }

    return to_utf8(comp_cps);
}

// Phase 3: Grapheme Cluster Segmentation (UAX #29)
std::vector<std::string> get_grapheme_clusters(const std::string& utf8_str) {
    std::vector<std::string> clusters;
    if (utf8_str.empty()) return clusters;

    std::vector<char32_t> cps = to_codepoints(utf8_str);
    size_t n = cps.size();
    size_t start = 0;

    while (start < n) {
        size_t end = start + 1;

        // Absorb Nukta
        if (end < n && cps[end] == BN_NUKTA) {
            end++;
        }

        // Absorb Virama + Consonant sequences (Conjuncts)
        while (end < n && (cps[end] == BN_VIRAMA || cps[end] == ZWJ || cps[end] == ZWNJ)) {
            end++;
            if (end < n && (is_bengali_consonant(cps[end]) || cps[end] == BN_NUKTA)) {
                end++;
                if (end < n && cps[end] == BN_NUKTA) {
                    end++;
                }
            }
        }

        // Absorb Kar
        if (end < n && is_bengali_kar(cps[end])) {
            end++;
        }

        // Absorb Anusvara, Visarga, Chandrabindu
        while (end < n && is_bengali_modifier(cps[end])) {
            end++;
        }

        std::vector<char32_t> cluster_slice(cps.begin() + start, cps.begin() + end);
        clusters.push_back(to_utf8(cluster_slice));
        start = end;
    }

    return clusters;
}

size_t grapheme_count(const std::string& utf8_str) {
    return get_grapheme_clusters(utf8_str).size();
}

// Phase 3: Backspace Deletion of Logical Unit
std::string pop_logical_unit(const std::string& utf8_str) {
    if (utf8_str.empty()) return "";

    std::vector<char32_t> cps = to_codepoints(utf8_str);
    if (cps.empty()) return "";

    char32_t last_cp = cps.back();

    // Case 1: Trailing Kar sign (ি, ে, ৈ, ো etc.)
    // Deleting the Kar leaves the base consonant/conjunct untouched!
    if (is_bengali_kar(last_cp)) {
        cps.pop_back();
        return to_utf8(cps);
    }

    // Case 2: Trailing modifier (ঁ, ং, ঃ)
    if (is_bengali_modifier(last_cp)) {
        cps.pop_back();
        return to_utf8(cps);
    }

    // Case 3: Trailing ZWJ / ZWNJ
    if (is_zwj(last_cp) || is_zwnj(last_cp)) {
        cps.pop_back();
        if (!cps.empty() && cps.back() == BN_VIRAMA) {
            cps.pop_back();
        }
        return to_utf8(cps);
    }

    // Case 4: Trailing Virama (্)
    if (is_bengali_virama(last_cp)) {
        cps.pop_back();
        return to_utf8(cps);
    }

    // Case 5: Trailing Nukta + Base
    if (last_cp == BN_NUKTA) {
        cps.pop_back();
        if (!cps.empty()) {
            cps.pop_back();
        }
        return to_utf8(cps);
    }

    // Case 6: Regular character / grapheme
    cps.pop_back();
    return to_utf8(cps);
}

} // namespace utf8
} // namespace abk
