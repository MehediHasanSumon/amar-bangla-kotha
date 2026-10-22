#include "abk/legacy_converter.h"
#include "abk/utf8_util.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>

namespace abk {

namespace {

struct MappingEntry {
    std::string ansi;
    std::string unicode;
};

// Master table mapping Bijoy / SutonnyMJ ANSI sequences to Unicode UTF-8
static const std::vector<MappingEntry>& get_ansi_to_unicode_table() {
    static const std::vector<MappingEntry> table = {
        // Multi-character sequences first
        {"Av", "আ"},
        {"„„", "ৃ"},
        {"yy", "ু"},
        {"vv", "া"},
        {"¯’", "স্থ"},
        {"š’", "ন্থ"},
        {"¯—", "স্ত"},
        {"¯‘", "স্তু"},
        {"¯¿", "স্ত্র"},
        {"š¿", "ন্ত্র"},
        {"š—", "ন্ত"},
        {"›`", "ন্দ"},
        {"›U", "ন্ট"},
        {"›Ø", "ন্দ্ব"},
        {"bœ", "ন্ন"},
        {"b¥", "ন্ম"},
        {"cœ", "প্ন"},
        {"gœ", "ম্ন"},
        {"kœ", "শ্ন"},
        {"k¦", "শ্ব"},
        {"k¥", "শ্ম"},
        {"k­", "শ্ল"},
        {"nè", "হ্ণ"},
        {"nŸ", "হ্ব"},
        {"n¬", "হ্ল"},

        // Vowels
        {"A", "অ"},
        {"B", "ই"},
        {"C", "ঈ"},
        {"D", "উ"},
        {"E", "ঊ"},
        {"F", "ঋ"},
        {"G", "এ"},
        {"H", "ঐ"},
        {"I", "ও"},
        {"J", "ঔ"},

        // Consonants
        {"K", "ক"},
        {"L", "খ"},
        {"M", "গ"},
        {"N", "ঘ"},
        {"O", "ঙ"},
        {"P", "চ"},
        {"Q", "ছ"},
        {"R", "জ"},
        {"S", "ঝ"},
        {"T", "ঞ"},
        {"U", "ট"},
        {"V", "ঠ"},
        {"W", "ড"},
        {"X", "ঢ"},
        {"Y", "ণ"},
        {"Z", "ত"},
        {"_", "থ"},
        {"`", "দ"},
        {"a", "ধ"},
        {"b", "ন"},
        {"c", "প"},
        {"d", "ফ"},
        {"e", "ব"},
        {"f", "ভ"},
        {"g", "ম"},
        {"h", "য"},
        {"i", "র"},
        {"j", "ল"},
        {"k", "শ"},
        {"l", "ষ"},
        {"m", "স"},
        {"n", "হ"},
        {"o", "ড়"},
        {"p", "ঢ়"},
        {"q", "য়"},
        {"r", "ৎ"},
        {"s", "ং"},
        {"t", "ঃ"},
        {"u", "ঁ"},

        // Digits
        {"0", "০"},
        {"1", "১"},
        {"2", "২"},
        {"3", "৩"},
        {"4", "৪"},
        {"5", "৫"},
        {"6", "৬"},
        {"7", "৭"},
        {"8", "৮"},
        {"9", "৯"},

        // Kars & Modifiers
        {"v", "া"},
        {"w", "ি"},
        {"x", "ী"},
        {"y", "ু"},
        {"z", "ু"},
        {"“", "ু"},
        {"–", "ু"},
        {"~", "ূ"},
        {"ƒ", "ূ"},
        {"‚", "ূ"},
        {"„", "ৃ"},
        {"†", "ে"},
        {"‡", "ে"},
        {"ˆ", "ৈ"},
        {"‰", "ৈ"},
        {"Š", "ৗ"},
        {"&", "্"},
        {"•", "ঙ্"},
        {"|", "।"},

        // Common SutonnyMJ Conjuncts & Ligatures
        {"^", "্ব"},
        {"Ÿ", "্ব"},
        {"¡", "্ব"},
        {"¦", "্ব"},
        {"‘", "্তু"},
        {"’", "্থ"},
        {"‹", "্ক"},
        {"Œ", "্ক্র"},
        {"”", "চ্"},
        {"—", "্ত"},
        {"˜", "দ্"},
        {"™", "দ্"},
        {"š", "ন্"},
        {"›", "ন্"},
        {"œ", "্ন"},
        {"è", "্ন"},
        {"¢", "্ভ"},
        {"£", "্ভ্র"},
        {"¤", "ম্"},
        {"¥", "্ম"},
        {"§", "্ম"},
        {"¨", "্য"},
        {"©", "র্"}, // Reph marker
        {"ª", "্র"},
        {"«", "্র"},
        {"Ö", "্র"},
        {"¬", "্ল"},
        {"­", "্ল"},
        {"®", "ষ্"},
        {"¯", "স্"},
        {"°", "ক্ক"},
        {"±", "ক্ট"},
        {"²", "ক্ষ্ণ"},
        {"³", "ক্ত"},
        {"´", "ক্ম"},
        {"µ", "ক্র"},
        {"¶", "ক্ষ"},
        {"·", "ক্স"},
        {"¸", "গু"},
        {"¹", "জ্ঞ"},
        {"Á", "জ্ঞ"},
        {"º", "গ্দ"},
        {"»", "গ্ধ"},
        {"¼", "ঙ্ক"},
        {"½", "ঙ্গ"},
        {"¾", "জ্জ"},
        {"¿", "্ত্র"},
        {"À", "জ্ঝ"},
        {"Â", "ঞ্চ"},
        {"Ã", "ঞ্ছ"},
        {"Ä", "ঞ্জ"},
        {"Å", "ঞ্ঝ"},
        {"Æ", "ট্ট"},
        {"Ç", "ড্ড"},
        {"È", "ণ্ট"},
        {"É", "ণ্ঠ"},
        {"Ê", "ণ্ড"},
        {"Ë", "ত্ত"},
        {"Ì", "ত্থ"},
        {"Í", "ত্ম"},
        {"Î", "ত্র"},
        {"Ï", "দ্দ"},
        {"Ð", "-"},
        {"Ñ", "-"},
        {"Ò", "\""},
        {"Ó", "\""},
        {"Ô", "'"},
        {"Õ", "'"},
        {"×", "দ্ধ"},
        {"Ø", "দ্ব"},
        {"Ù", "দ্ম"},
        {"Ú", "ন্ঠ"},
        {"Û", "ন্ড"},
        {"Ü", "ন্ধ"},
        {"Ý", "ন্স"},
        {"Þ", "প্ট"},
        {"ß", "প্ত"},
        {"à", "প্প"},
        {"á", "প্স"},
        {"â", "ব্জ"},
        {"ã", "ব্দ"},
        {"ä", "ব্ধ"},
        {"å", "ভ্র"},
        {"æ", "ম্ন"},
        {"ç", "ম্ফ"},
        {"é", "ল্ক"},
        {"ê", "ল্গ"},
        {"ë", "ল্ট"},
        {"ì", "ল্ড"},
        {"í", "ল্প"},
        {"î", "ল্ফ"},
        {"ï", "শু"},
        {"ð", "শ্চ"},
        {"ñ", "শ্ছ"},
        {"ò", "ষ্ণ"},
        {"ó", "ষ্ট"},
        {"ô", "ষ্ঠ"},
        {"õ", "ষ্ফ"},
        {"ö", "স্খ"},
        {"÷", "স্ট"},
        {"ø", "স্ন"},
        {"ù", "স্ফ"},
        {"ú", "্প"},
        {"û", "হু"},
        {"ü", "হৃ"},
        {"ý", "হ্ন"},
        {"þ", "হ্ম"}
    };
    return table;
}

// Master reverse table mapping Unicode components back to Bijoy ANSI
static const std::vector<MappingEntry>& get_unicode_to_ansi_table() {
    static const std::vector<MappingEntry> rev_table = {
        // Punctuation
        {"।", "|"},
        {"‘", "Ô"},
        {"’", "Õ"},
        {"“", "Ò"},
        {"”", "Ó"},

        // Conjuncts with ra-phala / ya-phala
        {"্র্য", "ª¨"},
        {"ম্প্র", "¤cÖ"},
        {"র‌্য", "i¨"},
        {"ক্ষ্ম", "²"},
        {"ক্ক", "°"},
        {"ক্ট", "±"},
        {"ক্ত", "³"},
        {"ক্ব", "K¡"},
        {"স্ক্র", "¯Œ"},
        {"ক্র", "µ"},
        {"ক্ল", "K¬"},
        {"ক্ষ", "¶"},
        {"ক্স", "·"},
        {"গু", "¸"},
        {"গ্ধ", "»"},
        {"গ্ন", "Mœ"},
        {"গ্ম", "M¥"},
        {"গ্ল", "M¬"},
        {"ঙ্ক", "¼"},
        {"ঙ্ক্ষ", "•¶"},
        {"ঙ্খ", "•L"},
        {"ঙ্গ", "½"},
        {"ঙ্ঘ", "•N"},
        {"চ্চ", "”P"},
        {"চ্ছ", "”Q"},
        {"চ্ছ্ব", "”Q^"},
        {"চ্ঞ", "”T"},
        {"জ্জ", "¾"},
        {"জ্জ্ব", "¾^"},
        {"জ্ঝ", "À"},
        {"জ্ঞ", "Á"},
        {"জ্ব", "R¡"},
        {"ঞ্চ", "Â"},
        {"ঞ্ছ", "Ã"},
        {"ঞ্জ", "Ä"},
        {"ঞ্ঝ", "Å"},
        {"ট্ট", "Æ"},
        {"ট্ব", "U¡"},
        {"ট্ম", "U¥"},
        {"ঠ্ঠ", "V&V"},
        {"ড্ড", "Ç"},
        {"ণ্ট", "È"},
        {"ণ্ঠ", "É"},
        {"ণ্ড", "Ê"},
        {"ণ্ড্ব", "Ê^"},
        {"ণ্ণ", "Yœ"},
        {"ণ্ব", "Y^"},
        {"ত্ম", "Í"},
        {"ত্র", "Î"},
        {"ত্ত", "Ë"},
        {"ত্থ", "Ì"},
        {"দ্দ", "Ï"},
        {"দ্ধ", "×"},
        {"দ্ব", "Ø"},
        {"দ্ভ", "™¢"},
        {"দ্ম", "Ù"},
        {"দ্রু", "`ª“"},
        {"ধ্ব", "aŸ"},
        {"ধ্ম", "a¥"},
        {"ন্ট", "›U"},
        {"ন্ঠ", "Ú"},
        {"ন্ড", "Û"},
        {"ন্ত্র", "š¿"},
        {"ন্ত", "š—"},
        {"স্ত্র", "¯¿"},
        {"ন্থ", "š’"},
        {"ন্দ", "›`"},
        {"ন্দ্ব", "›Ø"},
        {"ন্ধ", "Ü"},
        {"ন্ন", "bœ"},
        {"ন্ব", "š^"},
        {"ন্ম", "b¥"},
        {"প্ট", "Þ"},
        {"প্ত", "ß"},
        {"প্ন", "cœ"},
        {"প্প", "à"},
        {"প্ল", "c­"},
        {"প্স", "á"},
        {"ফ্ল", "d¬"},
        {"ব্জ", "â"},
        {"ব্দ", "ã"},
        {"ব্ধ", "ä"},
        {"ব্ব", "eŸ"},
        {"ব্ল", "e­"},
        {"ভ্র", "å"},
        {"ম্ন", "gœ"},
        {"ম্প", "¤ú"},
        {"ম্ফ", "ç"},
        {"ম্ব", "¤^"},
        {"ম্ভ", "¤¢"},
        {"ম্ভ্র", "¤£"},
        {"ম্ম", "¤§"},
        {"ম্ল", "¤­"},
        {"্র", "ª"},
        {"রু", "i“"},
        {"রূ", "iƒ"},
        {"ল্ক", "é"},
        {"ল্গ", "ê"},
        {"ল্ট", "ë"},
        {"ল্ড", "ì"},
        {"ল্প", "í"},
        {"ল্ফ", "î"},
        {"ল্ব", "j¦"},
        {"ল্ম", "j¥"},
        {"ল্ল", "j­"},
        {"শু", "ï"},
        {"শ্চ", "ð"},
        {"শ্ন", "kœ"},
        {"শ্ব", "k¦"},
        {"শ্ম", "k¥"},
        {"শ্ল", "k­"},
        {"ষ্ক", "®‹"},
        {"ষ্ক্র", "®Œ"},
        {"ষ্ট", "ó"},
        {"ষ্ঠ", "ô"},
        {"ষ্ণ", "ò"},
        {"ষ্প", "®ú"},
        {"ষ্ফ", "õ"},
        {"ষ্ম", "®§"},
        {"স্ক", "¯‹"},
        {"স্ট", "÷"},
        {"স্খ", "ö"},
        {"স্ত", "¯—"},
        {"স্তু", "¯‘"},
        {"স্থ", "¯’"},
        {"স্ন", "ø"},
        {"স্প", "¯ú"},
        {"স্ফ", "ù"},
        {"স্ব", "¯^"},
        {"স্ম", "¯§"},
        {"স্ল", "¯­"},
        {"হু", "û"},
        {"হ্ণ", "nè"},
        {"হ্ব", "nŸ"},
        {"হ্ন", "ý"},
        {"হ্ম", "þ"},
        {"হ্ল", "n¬"},
        {"হৃ", "ü"},

        // Modifiers & vowels
        {"র্", "©"},
        {"্য", "¨"},
        {"্", "&"},
        {"আ", "Av"},
        {"অ", "A"},
        {"ই", "B"},
        {"ঈ", "C"},
        {"উ", "D"},
        {"ঊ", "E"},
        {"ঋ", "F"},
        {"এ", "G"},
        {"ঐ", "H"},
        {"ও", "I"},
        {"ঔ", "J"},
        {"ক", "K"},
        {"খ", "L"},
        {"গ", "M"},
        {"ঘ", "N"},
        {"ঙ", "O"},
        {"চ", "P"},
        {"ছ", "Q"},
        {"জ", "R"},
        {"ঝ", "S"},
        {"ঞ", "T"},
        {"ট", "U"},
        {"ঠ", "V"},
        {"ড", "W"},
        {"ঢ", "X"},
        {"ণ", "Y"},
        {"ত", "Z"},
        {"থ", "_"},
        {"দ", "`"},
        {"ধ", "a"},
        {"ন", "b"},
        {"প", "c"},
        {"ফ", "d"},
        {"ব", "e"},
        {"ভ", "f"},
        {"ম", "g"},
        {"য", "h"},
        {"র", "i"},
        {"ল", "j"},
        {"শ", "k"},
        {"ষ", "l"},
        {"স", "m"},
        {"হ", "n"},
        {"ড়", "o"},
        {"ঢ়", "p"},
        {"য়", "q"},
        {"ৎ", "r"},
        {"০", "0"},
        {"১", "1"},
        {"২", "2"},
        {"৩", "3"},
        {"৪", "4"},
        {"৫", "5"},
        {"৬", "6"},
        {"৭", "7"},
        {"৮", "8"},
        {"৯", "9"},
        {"া", "v"},
        {"ি", "w"},
        {"ী", "x"},
        {"ু", "y"},
        {"ূ", "~"},
        {"ৃ", "„"},
        {"ে", "‡"},
        {"ৈ", "‰"},
        {"ৗ", "Š"},
        {"ং", "s"},
        {"ঃ", "t"},
        {"ঁ", "u"}
    };
    return rev_table;
}

// Convert raw Windows-1252 / ISO-8859-1 single-byte string to UTF-8
std::string sanitize_ansi_input(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c >= 0x80) {
            // Check if already valid multi-byte UTF-8
            if ((c & 0xE0) == 0xC0 && i + 1 < input.size() && (static_cast<unsigned char>(input[i+1]) & 0xC0) == 0x80) {
                out.push_back(input[i]);
                out.push_back(input[i+1]);
                i += 1;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size() &&
                       (static_cast<unsigned char>(input[i+1]) & 0xC0) == 0x80 &&
                       (static_cast<unsigned char>(input[i+2]) & 0xC0) == 0x80) {
                out.push_back(input[i]);
                out.push_back(input[i+1]);
                out.push_back(input[i+2]);
                i += 2;
            } else {
                // Map CP1252 byte to UTF-8 character
                switch (c) {
                    case 0x82: out += "‚"; break;
                    case 0x84: out += "„"; break;
                    case 0x85: out += "…"; break;
                    case 0x86: out += "†"; break;
                    case 0x87: out += "‡"; break;
                    case 0x88: out += "ˆ"; break;
                    case 0x89: out += "‰"; break;
                    case 0x8A: out += "Š"; break;
                    case 0x8B: out += "‹"; break;
                    case 0x8C: out += "Œ"; break;
                    case 0x91: out += "‘"; break;
                    case 0x92: out += "’"; break;
                    case 0x93: out += "“"; break;
                    case 0x94: out += "”"; break;
                    case 0x95: out += "•"; break;
                    case 0x96: out += "–"; break;
                    case 0x97: out += "—"; break;
                    case 0x98: out += "˜"; break;
                    case 0x99: out += "™"; break;
                    case 0x9A: out += "š"; break;
                    case 0x9B: out += "›"; break;
                    case 0x9C: out += "œ"; break;
                    case 0x9F: out += "Ÿ"; break;
                    case 0xA9: out += "©"; break;
                    case 0xAB: out += "«"; break;
                    case 0xAE: out += "®"; break;
                    case 0xB0: out += "°"; break;
                    case 0xB1: out += "±"; break;
                    case 0xB2: out += "²"; break;
                    case 0xB3: out += "³"; break;
                    case 0xB5: out += "µ"; break;
                    case 0xB6: out += "¶"; break;
                    case 0xB7: out += "·"; break;
                    default:
                        // General Latin-1 byte to UTF-8
                        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
                        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                        break;
                }
            }
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

// Pre-conversion string replacements for common typing anomalies
std::string apply_pre_replacements(const std::string& input) {
    std::string s = input;
    // Replace "yy" -> "y", "vv" -> "v"
    size_t pos = 0;
    while ((pos = s.find("yy", pos)) != std::string::npos) {
        s.replace(pos, 2, "y");
        pos += 1;
    }
    pos = 0;
    while ((pos = s.find("vv", pos)) != std::string::npos) {
        s.replace(pos, 2, "v");
        pos += 1;
    }
    pos = 0;
    while ((pos = s.find("y&", pos)) != std::string::npos) {
        s.replace(pos, 2, "y");
        pos += 1;
    }
    return s;
}

// Helper: check if a codepoint is a pre-kar in Bengali (ি, ে, ৈ)
bool is_pre_kar(char32_t cp) {
    return (cp == 0x09BF || cp == 0x09C7 || cp == 0x09C8);
}

// Helper: check if codepoint is consonant or consonant-like
bool is_consonant(char32_t cp) {
    return utf8::is_bengali_consonant(cp) || cp == utf8::BN_KHANDA_TA;
}

// Reorders pre-kars (ি, ে, ৈ) that visually precede the consonant cluster in ANSI
std::vector<char32_t> reorder_pre_kars(const std::vector<char32_t>& input) {
    std::vector<char32_t> out;
    out.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        if (is_pre_kar(input[i]) && i + 1 < input.size() && input[i+1] != ' ') {
            char32_t pre_kar = input[i];
            size_t j = i + 1;

            // Find full consonant cluster following pre-kar:
            // C (+ Virama + C)* (+ ZWJ/ZWNJ)? (+ Ya-phala)?
            if (is_consonant(input[j])) {
                while (j < input.size()) {
                    if (is_consonant(input[j])) {
                        if (j + 2 < input.size() && input[j+1] == utf8::BN_VIRAMA && is_consonant(input[j+2])) {
                            j += 2;
                        } else if (j + 1 < input.size() && input[j+1] == utf8::BN_NUKTA) {
                            j += 1;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }

                // Check for Ya-phala (্ + য / ্য)
                if (j + 2 < input.size() && input[j+1] == utf8::BN_VIRAMA && input[j+2] == utf8::BN_YA) {
                    j += 2;
                }
                // Check for Ba-phala (্ + ব) or Ra-phala (্ + র)
                if (j + 2 < input.size() && input[j+1] == utf8::BN_VIRAMA && 
                    (input[j+2] == 0x09AC || input[j+2] == utf8::BN_RA)) {
                    j += 2;
                }

                // Append the consonant cluster first
                for (size_t k = i + 1; k <= j; ++k) {
                    out.push_back(input[k]);
                }

                // Handle compound vowels:
                // If pre-kar is 'ে' (0x09C7) and followed by 'া' (0x09BE) -> 'ো' (0x09CB)
                // If pre-kar is 'ে' (0x09C7) and followed by 'ৗ' (0x09D7) -> 'ৌ' (0x09CC)
                if (pre_kar == 0x09C7 && j + 1 < input.size() && input[j+1] == 0x09BE) {
                    out.push_back(0x09CB); // ো
                    i = j + 2;
                    continue;
                } else if (pre_kar == 0x09C7 && j + 1 < input.size() && input[j+1] == 0x09D7) {
                    out.push_back(0x09CC); // ৌ
                    i = j + 2;
                    continue;
                } else {
                    out.push_back(pre_kar);
                    i = j + 1;
                    continue;
                }
            }
        }
        out.push_back(input[i]);
        i++;
    }
    return out;
}

// Reorders Reph (র্ = র + ্) that was typed after the consonant cluster in Bijoy
std::vector<char32_t> reorder_reph(const std::vector<char32_t>& input) {
    std::vector<char32_t> out = input;

    // Scan for 'র' + '্' (0x09B0, 0x09CD)
    for (size_t i = 1; i < out.size(); ++i) {
        if (out[i] == utf8::BN_VIRAMA && out[i-1] == utf8::BN_RA) {
            // Check if this 'র' + '্' is Reph (following a consonant cluster rather than preceding one)
            // If it is preceded by a consonant or kar:
            if (i >= 2 && (is_consonant(out[i-2]) || utf8::is_bengali_kar(out[i-2]) || utf8::is_bengali_modifier(out[i-2]))) {
                // Find start of the preceding cluster:
                long long start = static_cast<long long>(i) - 2;

                // Skip any trailing kars/modifiers
                while (start >= 0 && (utf8::is_bengali_kar(out[start]) || utf8::is_bengali_modifier(out[start]) || out[start] == utf8::BN_NUKTA)) {
                    start--;
                }

                // Trace back through consonant conjuncts (C + virama + C)
                while (start >= 0 && is_consonant(out[start])) {
                    if (start >= 2 && out[start-1] == utf8::BN_VIRAMA && is_consonant(out[start-2])) {
                        start -= 2;
                    } else {
                        break;
                    }
                }

                if (start >= 0 && static_cast<size_t>(start) < i - 1) {
                    // Extract 'র' + '্'
                    char32_t r = out[i-1];
                    char32_t v = out[i];
                    out.erase(out.begin() + (i - 1), out.begin() + (i + 1));
                    out.insert(out.begin() + start, {r, v});
                    // Re-scan from current position
                }
            }
        }
    }
    return out;
}

} // anonymous namespace

std::string LegacyConverter::convert_ansi_to_unicode(const std::string& ansi_text) {
    if (ansi_text.empty()) return "";

    // 1. Sanitize raw CP1252 / extended bytes to valid UTF-8
    std::string clean_ansi = sanitize_ansi_input(ansi_text);
    clean_ansi = apply_pre_replacements(clean_ansi);

    // 2. Map ANSI sequences to intermediate Unicode UTF-8
    const auto& table = get_ansi_to_unicode_table();
    std::string mapped;
    mapped.reserve(clean_ansi.size() * 2);

    size_t i = 0;
    while (i < clean_ansi.size()) {
        bool matched = false;
        // Search table for matching prefix (longest entries are first)
        for (const auto& entry : table) {
            size_t len = entry.ansi.size();
            if (i + len <= clean_ansi.size() && clean_ansi.compare(i, len, entry.ansi) == 0) {
                mapped += entry.unicode;
                i += len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            mapped.push_back(clean_ansi[i]);
            i++;
        }
    }

    // 3. Convert to codepoints for orthographic reordering
    std::vector<char32_t> cps = utf8::to_codepoints(mapped);

    // 4. Reorder pre-kars (ি, ে, ৈ) and handle compound vowels (ো, ৌ)
    cps = reorder_pre_kars(cps);

    // 5. Reorder Reph (র্) to precede consonant clusters
    cps = reorder_reph(cps);

    // 6. Convert back to UTF-8
    std::string result = utf8::to_utf8(cps);

    // Collapse duplicate halants (্ + ্ -> ্)
    size_t dpos = 0;
    while ((dpos = result.find("্্", dpos)) != std::string::npos) {
        result.replace(dpos, std::string("্্").size(), "্");
        dpos += std::string("্").size();
    }

    // 7. Post-conversion normalization:
    // Replace colon typos ('০ঃ' -> '০:', ' ঃ' -> ' :')
    for (char d = '0'; d <= '9'; ++d) {
        std::string target;
        target.push_back(d);
        target += "ঃ";
        size_t p = 0;
        while ((p = result.find(target, p)) != std::string::npos) {
            result.replace(p, target.size(), std::string(1, d) + ":");
            p += 2;
        }
    }

    // Final canonical NFC normalization
    return utf8::normalize_nfc(result);
}

std::string LegacyConverter::convert_unicode_to_ansi(const std::string& unicode_text) {
    if (unicode_text.empty()) return "";

    // 1. Normalize NFC first
    std::string norm = utf8::normalize_nfc(unicode_text);
    std::vector<char32_t> cps = utf8::to_codepoints(norm);

    // 2. Decompose compound kars into separate components for Bijoy typing:
    // 'ো' (0x09CB) -> 'ে' (0x09C7) + 'া' (0x09BE)
    // 'ৌ' (0x09CC) -> 'ে' (0x09C7) + 'ৗ' (0x09D7)
    std::vector<char32_t> decomp;
    decomp.reserve(cps.size() * 2);
    for (char32_t cp : cps) {
        if (cp == 0x09CB) {
            decomp.push_back(0x09C7); // ে
            decomp.push_back(0x09BE); // া
        } else if (cp == 0x09CC) {
            decomp.push_back(0x09C7); // ে
            decomp.push_back(0x09D7); // ৗ
        } else {
            decomp.push_back(cp);
        }
    }

    // 3. Reverse pre-kars: In Unicode, pre-kars follow consonant cluster.
    // In Bijoy visual order, pre-kars must precede the consonant cluster!
    std::vector<char32_t> reordered;
    reordered.reserve(decomp.size());

    size_t i = 0;
    while (i < decomp.size()) {
        // If current character is a pre-kar that was placed logically after a cluster
        if (is_pre_kar(decomp[i]) && !reordered.empty()) {
            char32_t pre_kar = decomp[i];
            // Find start of previous consonant cluster in reordered
            long long start = static_cast<long long>(reordered.size()) - 1;
            while (start >= 0 && (utf8::is_bengali_kar(reordered[start]) || reordered[start] == utf8::BN_NUKTA)) {
                start--;
            }
            while (start >= 0 && is_consonant(reordered[start])) {
                if (start >= 2 && reordered[start-1] == utf8::BN_VIRAMA && is_consonant(reordered[start-2])) {
                    start -= 2;
                } else {
                    break;
                }
            }
            if (start >= 0) {
                reordered.insert(reordered.begin() + start, pre_kar);
                i++;
                continue;
            }
        }
        reordered.push_back(decomp[i]);
        i++;
    }

    // 4. Reverse Reph: In Unicode, Reph ('র' + '্') precedes consonant cluster.
    // In Bijoy visual order, Reph ('©') follows the consonant cluster!
    std::vector<char32_t> final_cps;
    final_cps.reserve(reordered.size());
    i = 0;
    while (i < reordered.size()) {
        if (reordered[i] == utf8::BN_RA && i + 1 < reordered.size() && reordered[i+1] == utf8::BN_VIRAMA) {
            // Check if followed by a consonant cluster
            size_t j = i + 2;
            if (j < reordered.size() && is_consonant(reordered[j])) {
                while (j < reordered.size()) {
                    if (is_consonant(reordered[j])) {
                        if (j + 2 < reordered.size() && reordered[j+1] == utf8::BN_VIRAMA && is_consonant(reordered[j+2])) {
                            j += 2;
                        } else if (j + 1 < reordered.size() && (utf8::is_bengali_kar(reordered[j+1]) || reordered[j+1] == utf8::BN_NUKTA)) {
                            j += 1;
                        } else {
                            break;
                        }
                    } else if (utf8::is_bengali_kar(reordered[j])) {
                        j++;
                    } else {
                        break;
                    }
                }
                // Append cluster first
                for (size_t k = i + 2; k <= j && k < reordered.size(); ++k) {
                    final_cps.push_back(reordered[k]);
                }
                // Append reph (র + ্) after cluster
                final_cps.push_back(utf8::BN_RA);
                final_cps.push_back(utf8::BN_VIRAMA);
                i = j + 1;
                continue;
            }
        }
        final_cps.push_back(reordered[i]);
        i++;
    }

    std::string ready_text = utf8::to_utf8(final_cps);

    // 5. Map Unicode components to Bijoy ANSI string
    const auto& rev_table = get_unicode_to_ansi_table();
    std::string ansi_out;
    ansi_out.reserve(ready_text.size());

    i = 0;
    while (i < ready_text.size()) {
        bool matched = false;
        for (const auto& entry : rev_table) {
            size_t len = entry.ansi.size(); // In rev_table, 'ansi' field holds unicode source
            if (i + len <= ready_text.size() && ready_text.compare(i, len, entry.ansi) == 0) {
                ansi_out += entry.unicode;   // and 'unicode' field holds ANSI target
                i += len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            ansi_out.push_back(ready_text[i]);
            i++;
        }
    }

    return ansi_out;
}

bool LegacyConverter::is_likely_legacy_ansi(const std::string& text) {
    if (text.empty()) return false;

    // 1. If text already contains Bengali Unicode block (0x0980..0x09FF), it is NOT legacy ANSI
    auto cps = utf8::to_codepoints(text);
    size_t unicode_bn_count = 0;
    for (char32_t cp : cps) {
        if (utf8::is_bengali(cp)) {
            unicode_bn_count++;
        }
    }
    if (unicode_bn_count > 0) {
        return false;
    }

    // 2. High-value Bijoy specific token indicators
    static const std::vector<std::string> bijoy_indicators = {
        "Av", "‡", "†", "‰", "ˆ", "©", "®", "µ", "¶", "·", "¸", "¹", "Á",
        "¨", "¯", "’", "‘", "“", "”", "•", "–", "—", "˜", "™", "š", "›", "œ",
        "Í", "Î", "Ï", "Ö", "×", "Ø", "Ù", "Ú", "Û", "Ü", "Ý", "Þ", "ß",
        "à", "á", "â", "ã", "ä", "å", "æ", "ç", "è", "é", "ê", "ë", "ì",
        "í", "î", "ï", "ð", "ñ", "ò", "ó", "ô", "õ", "ö", "÷", "ø", "ù",
        "ú", "û", "ü", "ý", "þ", "evsjv", "Avgvi", "‡mvbvi", "fv‡jv"
    };

    size_t indicator_hits = 0;
    for (const auto& ind : bijoy_indicators) {
        if (text.find(ind) != std::string::npos) {
            indicator_hits++;
        }
    }

    // Check ratio of extended characters or indicators
    size_t non_ascii = 0;
    for (unsigned char c : text) {
        if (c >= 0x80) non_ascii++;
    }

    if (indicator_hits >= 2) return true;
    if (indicator_hits >= 1 && non_ascii > 0) return true;
    if (non_ascii * 10 > text.size() && indicator_hits >= 1) return true;

    return false;
}

} // namespace abk
