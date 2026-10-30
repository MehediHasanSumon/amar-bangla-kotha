#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/utf8_util.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdlib>

namespace abk {

namespace {

enum class PrevState : uint8_t {
    START = 0,
    AFTER_VOWEL = 1,
    AFTER_CONSONANT = 2,
    AFTER_Y_CONSONANT = 3,
    AFTER_PHALA = 4,
    AFTER_REPH = 5,
    AFTER_SEPARATOR = 6,
    OTHER = 7
};

struct Hypothesis {
    std::string text;
    size_t pos = 0;
    double phonetic_score = 0.0;
    double final_score = 0.0;
    PrevState state = PrevState::START;
    char32_t last_consonant = 0;
    bool used_variant = false;
    bool overrides_explicit = false;

    bool same_state_as(const Hypothesis& other) const {
        return pos == other.pos &&
               state == other.state &&
               last_consonant == other.last_consonant &&
               used_variant == other.used_variant &&
               overrides_explicit == other.overrides_explicit &&
               text == other.text;
    }
};

// Strip Assamese Ra (U+09F0) and Wa (U+09F1) if ever encountered in external rule files
std::string sanitize_bengali_codepoints(const std::string& s) {
    auto cps = utf8::to_codepoints(s);
    bool changed = false;
    for (char32_t& cp : cps) {
        if (cp == 0x09F0 || cp == 0x09F1) {
            cp = 0x09B0; // Standard Bengali Ra (র)
            changed = true;
        }
    }
    return changed ? utf8::to_utf8(cps) : s;
}

bool is_base_bengali_consonant(char32_t cp) {
    return (cp >= 0x0995 && cp <= 0x09B9);
}

char32_t extract_first_base_consonant(const std::string& utf8_str) {
    auto cps = utf8::to_codepoints(utf8_str);
    for (size_t i = 0; i < cps.size(); ++i) {
        if (is_base_bengali_consonant(cps[i])) {
            if (i + 1 < cps.size() && cps[i + 1] == 0x09BC) {
                continue; // Nukta consonant (ড়, ঢ়, য়, জ়)
            }
            return cps[i];
        }
    }
    return 0;
}

char32_t extract_last_base_consonant(const std::string& utf8_str) {
    auto cps = utf8::to_codepoints(utf8_str);
    char32_t last = 0;
    for (size_t i = 0; i < cps.size(); ++i) {
        if (is_base_bengali_consonant(cps[i])) {
            if (i + 1 < cps.size() && cps[i + 1] == 0x09BC) {
                continue;
            }
            last = cps[i];
        }
    }
    return last;
}

bool has_trie_prefix(const Trie& trie, const std::vector<char32_t>& cps) {
    if (cps.empty() || trie.node_count() == 0) return false;
    uint32_t node = trie.get_root_index();
    for (char32_t cp : cps) {
        node = trie.find_child(node, cp);
        if (node == 0) return false;
    }
    return true;
}

bool has_dictionary_prefix(const Dictionary* dict, const std::string& text) {
    if (!dict || text.empty()) return false;
    auto cps = utf8::to_codepoints(text);
    if (cps.empty()) return false;
    return has_trie_prefix(dict->get_system_trie(), cps) ||
           has_trie_prefix(dict->get_user_trie(), cps);
}

bool matches_left_context(RuleContext ctx, PrevState state, size_t pos, const std::string& input) {
    switch (ctx) {
        case RuleContext::ANY:
            return true;
        case RuleContext::START:
            return state == PrevState::START;
        case RuleContext::AFTER_CONSONANT:
            return state == PrevState::AFTER_CONSONANT;
        case RuleContext::AFTER_CONSONANT_LIKE:
            return state == PrevState::AFTER_CONSONANT ||
                   state == PrevState::AFTER_Y_CONSONANT ||
                   state == PrevState::AFTER_PHALA;
        case RuleContext::AFTER_VOWEL:
            return state == PrevState::AFTER_VOWEL;
        case RuleContext::AFTER_VOWEL_OR_SEPARATOR:
            return state == PrevState::AFTER_VOWEL ||
                   state == PrevState::AFTER_SEPARATOR ||
                   state == PrevState::OTHER;
        case RuleContext::START_OR_SEPARATOR:
            return state == PrevState::START ||
                   state == PrevState::AFTER_SEPARATOR ||
                   state == PrevState::OTHER;
        case RuleContext::NOT_AFTER_CONSONANT:
            return state == PrevState::START ||
                   state == PrevState::AFTER_VOWEL ||
                   state == PrevState::AFTER_SEPARATOR ||
                   state == PrevState::OTHER;
        case RuleContext::AFTER_REPH:
            return state == PrevState::AFTER_REPH;
        case RuleContext::END:
            return pos == input.length() ||
                   (pos < input.length() && std::isspace(static_cast<unsigned char>(input[pos])));
    }
    return true;
}

bool is_non_avro_uppercase(const std::string& str, size_t idx) {
    char c = str[idx];
    switch (c) {
        case 'A': case 'B': case 'C': case 'E': case 'F':
        case 'H': case 'K': case 'L': case 'M': case 'P':
        case 'Q': case 'V': case 'W': case 'X':
            return true;
        case 'G':
            // 'G' is only an Avro key in 'NG'
            return !(idx > 0 && str[idx - 1] == 'N');
        default:
            return false;
    }
}

bool is_url_or_email(const std::string& str) {
    if (str.rfind("http://", 0) == 0 || str.rfind("https://", 0) == 0 ||
        str.rfind("ftp://", 0) == 0 || str.rfind("file://", 0) == 0 ||
        str.rfind("www.", 0) == 0) {
        return true;
    }
    size_t at_pos = str.find('@');
    if (at_pos != std::string::npos && at_pos > 0 && at_pos + 1 < str.length()) {
        size_t dot_pos = str.find('.', at_pos + 1);
        if (dot_pos != std::string::npos && dot_pos > at_pos + 1 && dot_pos + 1 < str.length()) {
            return true;
        }
    }
    static const char* const tlds[] = {
        ".com", ".org", ".net", ".edu", ".gov", ".mil", ".int",
        ".bd", ".io", ".co", ".info", ".me", ".dev", ".app", ".ai", ".tech", ".xyz", ".uk"
    };
    std::string lower = utf8::to_lower_ascii(str);
    for (const char* tld : tlds) {
        size_t pos = lower.find(tld);
        if (pos != std::string::npos && pos > 0) {
            size_t after = pos + std::char_traits<char>::length(tld);
            if (after == lower.length() || lower[after] == '/' || lower[after] == '.' ||
                lower[after] == '?' || lower[after] == ':' || lower[after] == '#') {
                return true;
            }
        }
    }
    return false;
}

bool is_file_path_span(const std::string& str) {
    if (str.length() < 2) return false;

    // 1. Unix root/home/relative paths: /usr/bin, ~/docs, ./run.sh, ../file
    if (str.rfind("~/", 0) == 0 || str.rfind("./", 0) == 0 || str.rfind("../", 0) == 0) {
        return true;
    }
    if (str[0] == '/' && std::isalnum(static_cast<unsigned char>(str[1]))) {
        return true;
    }

    // 2. Windows drive path: C:\... or C:/...
    if (str.length() >= 3 && std::isalpha(static_cast<unsigned char>(str[0])) &&
        str[1] == ':' && (str[2] == '\\' || str[2] == '/')) {
        return true;
    }

    // 3. Backslash path or multi-slash path (dir1/dir2/file)
    if (str.find('\\') != std::string::npos) {
        bool has_alnum = false;
        for (unsigned char uc : str) {
            if (std::isalnum(uc)) { has_alnum = true; break; }
        }
        if (has_alnum) return true;
    }
    size_t first_slash = str.find('/');
    if (first_slash != std::string::npos && first_slash > 0 && first_slash + 1 < str.length()) {
        if (str.find('/', first_slash + 1) != std::string::npos || str.find('.') != std::string::npos) {
            return true;
        }
    }

    // 4. Filename with recognized extension (e.g. main.cpp, config.toml, report.pdf)
    size_t last_dot = str.rfind('.');
    if (last_dot != std::string::npos && last_dot > 0 && last_dot + 1 < str.length()) {
        std::string ext = utf8::to_lower_ascii(str.substr(last_dot + 1));
        static const char* const exts[] = {
            "cpp", "cc", "cxx", "c", "h", "hpp", "py", "js", "ts", "jsx", "tsx",
            "html", "htm", "css", "scss", "json", "xml", "yaml", "yml", "toml",
            "ini", "cfg", "conf", "md", "txt", "pdf", "doc", "docx", "xls", "xlsx",
            "ppt", "pptx", "png", "jpg", "jpeg", "gif", "svg", "webp", "ico",
            "zip", "tar", "gz", "bz2", "xz", "7z", "sh", "bash", "zsh", "bat",
            "ps1", "exe", "dll", "so", "dylib", "deb", "rpm", "apk", "tsv", "csv",
            "sql", "db", "log", "bak", "tmp", "lock"
        };
        for (const char* known : exts) {
            if (ext == known) return true;
        }
    }
    return false;
}

bool is_abbreviation_span(const std::string& str) {
    if (str.length() < 2 || str.back() != '.') return false;

    // 1. Dotted initialisms: e.g., i.e., B.A., M.A., U.S.A., U.K., Ph.D., B.Sc., M.Sc.
    if (str == "Ph.D." || str == "B.Sc." || str == "M.Sc." || str == "LL.B." || str == "LL.M.") {
        return true;
    }
    if (str.length() >= 4 && (str.length() % 2 == 0)) {
        bool all_pairs = true;
        for (size_t i = 0; i < str.length(); i += 2) {
            if (!std::isalpha(static_cast<unsigned char>(str[i])) || str[i + 1] != '.') {
                all_pairs = false;
                break;
            }
        }
        if (all_pairs) return true;
    }

    // 2. Common single-dot titles & scholarly abbreviations
    std::string stem = str.substr(0, str.length() - 1);
    static const char* const abbrevs[] = {
        "Dr", "Mr", "Mrs", "Ms", "Prof", "Jr", "Sr", "St", "Capt", "Col", "Gen", "Lt", "Rev",
        "Inc", "Ltd", "Co", "Corp", "Dept", "Univ", "Fig", "Vol", "No", "vs", "etc", "approx", "cf", "viz"
    };
    for (const char* a : abbrevs) {
        if (stem == a) return true;
    }
    return false;
}

bool is_protected_dollar(const std::string& input, size_t pos) {
    const size_t n = input.length();
    if (pos >= n || input[pos] != '$') return false;

    // Explicit escape $` is handled by its own 2-char rule; protect lone '$' in code/shell/math
    if (pos + 1 < n) {
        unsigned char next = static_cast<unsigned char>(input[pos + 1]);
        if (next == '`') return false; // let "$`" rule match
        if (std::isalpha(next) || next == '_' || next == '{' || next == '(' ||
            next == '[' || next == '?' || next == '@' || next == '#' ||
            next == '*' || next == '$' || next == '!') {
            return true;
        }
    }
    if (pos > 0) {
        unsigned char prev = static_cast<unsigned char>(input[pos - 1]);
        if (std::isalpha(prev) || prev == '_' || prev == '\\' || prev == '$') {
            return true;
        }
    }
    return false;
}

bool is_protected_dollar_span(const std::string& input) {
    size_t dpos = input.find('$');
    if (dpos == std::string::npos) return false;
    if (dpos + 1 < input.length() && input[dpos + 1] == '`') return false;
    return is_protected_dollar(input, dpos);
}

bool is_uppercase_acronym(const std::string& str) {
    if (str.length() < 2 || str.length() > 12) return false;
    int upper_count = 0;
    bool has_non_avro_upper = false;
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char uc = static_cast<unsigned char>(str[i]);
        if (std::isupper(uc)) {
            upper_count++;
            if (is_non_avro_uppercase(str, i)) {
                has_non_avro_upper = true;
            }
        } else if (str[i] != '-' && !std::isdigit(uc)) {
            return false;
        }
    }
    return upper_count >= 2 && has_non_avro_upper;
}

bool is_protected_dot(const std::string& input, size_t pos) {
    const size_t n = input.length();
    if (pos >= n || input[pos] != '.') return false;

    // 1. Inside numbers (e.g. 3.5, 3.14159)
    if (pos > 0 && pos + 1 < n &&
        std::isdigit(static_cast<unsigned char>(input[pos - 1])) &&
        std::isdigit(static_cast<unsigned char>(input[pos + 1]))) {
        return true;
    }

    // 2. Ellipsis or consecutive dots (.., ...)
    if ((pos > 0 && input[pos - 1] == '.') || (pos + 1 < n && input[pos + 1] == '.')) {
        return true;
    }

    // 3. Initialisms like e.g., i.e., B.A., M.A., U.S.A.
    if (pos >= 1 && std::isalpha(static_cast<unsigned char>(input[pos - 1])) &&
        (pos == 1 || !std::isalpha(static_cast<unsigned char>(input[pos - 2])))) {
        if (pos + 2 < n && std::isalpha(static_cast<unsigned char>(input[pos + 1])) &&
            input[pos + 2] == '.') {
            return true;
        }
        if (pos >= 3 && input[pos - 2] == '.' &&
            std::isalpha(static_cast<unsigned char>(input[pos - 3]))) {
            return true;
        }
    }

    // 4. Common abbreviations (Dr., Mr., Mrs., Ms., Prof., Jr., Sr., vs., etc.)
    size_t word_end = pos;
    size_t word_start = pos;
    while (word_start > 0 && std::isalpha(static_cast<unsigned char>(input[word_start - 1]))) {
        word_start--;
    }
    if (word_end > word_start) {
        std::string word = input.substr(word_start, word_end - word_start);
        if (word == "Dr" || word == "Mr" || word == "Mrs" || word == "Ms" ||
            word == "Prof" || word == "Jr" || word == "Sr" ||
            word == "vs" || word == "etc") {
            return true;
        }
    }

    return false;
}

// Strip comments outside double-quoted TOML strings
std::string strip_toml_comment(const std::string& line) {
    bool in_quotes = false;
    bool escaped = false;
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && in_quotes) {
            escaped = true;
            continue;
        }
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c == '#' && !in_quotes) {
            return line.substr(0, i);
        }
    }
    return line;
}

bool parse_toml_quoted_string(const std::string& raw, std::string& out) {
    std::string s = utf8::trim(raw);
    if (s.length() < 2 || s.front() != '"' || s.back() != '"') {
        return false;
    }
    out.clear();
    for (size_t i = 1; i + 1 < s.length(); ++i) {
        char c = s[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i + 1 >= s.length() - 1) {
            return false;
        }
        char next = s[++i];
        switch (next) {
            case '"':  out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            case 't':  out.push_back('\t'); break;
            case 'u': {
                if (i + 4 >= s.length() - 1) return false;
                std::string hex = s.substr(i + 1, 4);
                char* endptr = nullptr;
                unsigned long cp = std::strtoul(hex.c_str(), &endptr, 16);
                if (!endptr || *endptr != '\0') return false;
                out += utf8::to_utf8({static_cast<char32_t>(cp)});
                i += 4;
                break;
            }
            case 'U': {
                if (i + 8 >= s.length() - 1) return false;
                std::string hex = s.substr(i + 1, 8);
                char* endptr = nullptr;
                unsigned long cp = std::strtoul(hex.c_str(), &endptr, 16);
                if (!endptr || *endptr != '\0') return false;
                out += utf8::to_utf8({static_cast<char32_t>(cp)});
                i += 8;
                break;
            }
            default:
                out.push_back(next);
                break;
        }
    }
    return true;
}

bool split_toml_kv(const std::string& line, std::string& key_out, std::string& val_out) {
    bool in_quotes = false;
    bool escaped = false;
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && in_quotes) {
            escaped = true;
            continue;
        }
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c == '=' && !in_quotes) {
            key_out = utf8::trim(line.substr(0, i));
            val_out = utf8::trim(line.substr(i + 1));
            return !key_out.empty();
        }
    }
    return false;
}

} // namespace

Transliterator::Transliterator() {
    std::vector<std::string> search_paths = {
        "data/keymap_phonetic.toml",
        "../data/keymap_phonetic.toml",
        "../../data/keymap_phonetic.toml",
        "/usr/local/share/amar-bangla-kotha/keymap_phonetic.toml",
        "/usr/share/amar-bangla-kotha/keymap_phonetic.toml"
    };

    bool loaded = false;
    for (const auto& path : search_paths) {
        if (load_rules_from_file(path)) {
            loaded = true;
            break;
        }
    }

    if (!loaded || rules.empty()) {
        init_default_rules();
    }
}

Transliterator::Transliterator(const std::string& rules_filepath) {
    if (!load_rules_from_file(rules_filepath) || rules.empty()) {
        init_default_rules();
    }
}

RuleContext Transliterator::parse_context(const std::string& str) {
    if (str == "START") return RuleContext::START;
    if (str == "AFTER_CONSONANT") return RuleContext::AFTER_CONSONANT;
    if (str == "AFTER_CONSONANT_LIKE") return RuleContext::AFTER_CONSONANT_LIKE;
    if (str == "AFTER_VOWEL") return RuleContext::AFTER_VOWEL;
    if (str == "AFTER_VOWEL_OR_SEPARATOR") return RuleContext::AFTER_VOWEL_OR_SEPARATOR;
    if (str == "START_OR_SEPARATOR") return RuleContext::START_OR_SEPARATOR;
    if (str == "NOT_AFTER_CONSONANT") return RuleContext::NOT_AFTER_CONSONANT;
    if (str == "AFTER_REPH") return RuleContext::AFTER_REPH;
    if (str == "END") return RuleContext::END;
    return RuleContext::ANY;
}

RightContext Transliterator::parse_right_context(const std::string& str) {
    if (str == "BEFORE_CONSONANT") return RightContext::BEFORE_CONSONANT;
    if (str == "END") return RightContext::END;
    return RightContext::ANY;
}

OutputCategory Transliterator::parse_category(const std::string& str) {
    if (str == "CONSONANT") return OutputCategory::CONSONANT;
    if (str == "Y_CONSONANT") return OutputCategory::Y_CONSONANT;
    if (str == "INDEPENDENT_VOWEL") return OutputCategory::INDEPENDENT_VOWEL;
    if (str == "INHERENT_VOWEL") return OutputCategory::INHERENT_VOWEL;
    if (str == "KAR") return OutputCategory::KAR;
    if (str == "VOWEL_YA") return OutputCategory::VOWEL_YA;
    if (str == "PHALA") return OutputCategory::PHALA;
    if (str == "REPH") return OutputCategory::REPH;
    if (str == "HASANTA") return OutputCategory::HASANTA;
    if (str == "SEPARATOR") return OutputCategory::SEPARATOR;
    if (str == "CONJUNCT") return OutputCategory::CONJUNCT;
    if (str == "MODIFIER") return OutputCategory::MODIFIER;
    if (str == "DIPHTHONG_SPLIT") return OutputCategory::DIPHTHONG_SPLIT;
    if (str == "DIGIT") return OutputCategory::DIGIT;
    if (str == "PUNCTUATION") return OutputCategory::PUNCTUATION;
    return OutputCategory::OTHER;
}

bool is_phonetic_vowel_char(char c) {
    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return (lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u');
}

std::string match_vowel_token(const std::string& s, size_t pos) {
    static const char* VOWEL_TOKENS[] = {
        "oi", "OI", "ou", "OU", "ee", "oo", "rri",
        "a", "e", "i", "o", "u",
        "A", "E", "I", "O", "U"
    };
    for (const char* v : VOWEL_TOKENS) {
        size_t len = std::strlen(v);
        if (pos + len <= s.length() && s.compare(pos, len, v) == 0) {
            return std::string(v);
        }
    }
    return "";
}

// Normalizes Chandrabindu prefix and infix notation:
// e.g. qqdaRi -> da^Ri (দাঁড়ি), qqcad -> ca^d (চাঁদ), daqqRi -> da^Ri (দাঁড়ি)
// Standalone qq -> ^ (ঁ)
std::string normalize_chandrabindu_qq(const std::string& input) {
    const size_t n = input.length();
    if (n == 0) return input;

    bool has_marker = false;
    for (size_t k = 0; k < n; ++k) {
        if (input[k] == '^' || input[k] == 'q' || input[k] == 'Q') {
            has_marker = true;
            break;
        }
    }
    if (!has_marker) return input;

    std::string res;
    res.reserve(n + 4);
    size_t i = 0;

    while (i < n) {
        bool is_qq = (i + 1 < n &&
                      (input[i] == 'q' || input[i] == 'Q') &&
                      (input[i + 1] == 'q' || input[i + 1] == 'Q'));
        bool is_caret = (input[i] == '^');

        if (!is_qq && !is_caret) {
            res.push_back(input[i]);
            i++;
            continue;
        }

        size_t step = is_qq ? 2 : 1;

        // 1. Infix notation following consonant: e.g. dqqa -> da^
        if (!res.empty() && std::isalpha(static_cast<unsigned char>(res.back())) &&
            !is_phonetic_vowel_char(res.back())) {
            size_t j = i + step;
            std::string vt = (j < n) ? match_vowel_token(input, j) : "";
            if (!vt.empty()) {
                res.append(vt);
                res.push_back('^');
                i = j + vt.length();
                continue;
            } else {
                res.push_back('^');
                i += step;
                continue;
            }
        }

        // 2. Post-vowel notation: e.g. daqqRi -> da^Ri
        if (!res.empty() && std::isalpha(static_cast<unsigned char>(res.back()))) {
            res.push_back('^');
            i += step;
            continue;
        }

        // 3. Prefix notation: at beginning of token or after non-letter (e.g. qqdaRi, ^daRi, qqcad, qqidur)
        size_t j = i + step;
        if (j >= n) {
            res.push_back('^');
            i = j;
            continue;
        }

        // If next is another q, Q or ^, just emit ^ and let next iteration handle it (e.g. qqqq -> ^^)
        if (input[j] == 'q' || input[j] == 'Q' || input[j] == '^') {
            res.push_back('^');
            i = j;
            continue;
        }

        // Check if followed by standalone vowel: qq + vowel -> vowel + ^ (e.g. qqidur -> i^dur)
        std::string vt = match_vowel_token(input, j);
        if (!vt.empty()) {
            res.append(vt);
            res.push_back('^');
            i = j + vt.length();
            continue;
        }

        // Check if followed by consonant cluster: qq + C+ [V*] -> C+ [V*] + ^
        if (std::isalpha(static_cast<unsigned char>(input[j]))) {
            size_t c_start = j;
            while (j < n && std::isalpha(static_cast<unsigned char>(input[j]))) {
                if (input[j] == 'q' || input[j] == 'Q' || !match_vowel_token(input, j).empty()) {
                    break;
                }
                j++;
            }
            std::string consonants = input.substr(c_start, j - c_start);
            std::string fol_v = (j < n) ? match_vowel_token(input, j) : "";
            if (!fol_v.empty()) {
                j += fol_v.length();
                res.append(consonants);
                res.append(fol_v);
                res.push_back('^');
                i = j;
                continue;
            } else {
                res.append(consonants);
                res.push_back('^');
                i = j;
                continue;
            }
        } else {
            res.push_back('^');
            i = j;
            continue;
        }
    }

    return res;
}

void Transliterator::sort_rules() {
    std::stable_sort(rules.begin(), rules.end(), [](const PhoneticRule& a, const PhoneticRule& b) {
        if (a.pattern.length() != b.pattern.length()) {
            return a.pattern.length() > b.pattern.length();
        }
        if (a.variant != b.variant) {
            return !a.variant;
        }
        return a.priority > b.priority;
    });
}

void Transliterator::init_default_phonotactics() {
    phonotactics_map.clear();
    const std::pair<const char*, const char*> defaults[] = {
        {"ক", "কটতনবমযরলষস"}, {"খ", "বযর"}, {"গ", "গঘদধনবমযরল"}, {"ঘ", "নবযর"},
        {"ঙ", "কখগঘময"}, {"চ", "চছঞবয"}, {"ছ", "বযর"}, {"জ", "জঝঞবযর"},
        {"ঝ", "বযর"}, {"ঞ", "চছজঝয"}, {"ট", "টঠবমযর"}, {"ঠ", "বযর"},
        {"ড", "গডঢবযর"}, {"ঢ", "বযর"}, {"ণ", "টঠডঢণবময"}, {"ত", "তথনবমযর"},
        {"থ", "বযর"}, {"দ", "গঘদধবভমযর"}, {"ধ", "নবমযর"}, {"ন", "টঠডঢতথদধনবমযস"},
        {"প", "টতনপফবমযরলস"}, {"ফ", "টফবযরল"}, {"ব", "জদধবভযরল"}, {"ভ", "বভযরল"},
        {"ম", "নপফবভমযরল"}, {"য", "বযর"}, {"র", ""}, {"ল", "কগটডপফবভমযল"},
        {"শ", "চছতনবমযরল"}, {"ষ", "কটঠণপফবময"}, {"স", "কখটতথনপফবমযরল"}, {"হ", "ণনবমযরল"}
    };
    for (const auto& kv : defaults) {
        auto k_cps = utf8::to_codepoints(kv.first);
        auto v_cps = utf8::to_codepoints(kv.second);
        if (!k_cps.empty()) {
            std::sort(v_cps.begin(), v_cps.end());
            v_cps.erase(std::unique(v_cps.begin(), v_cps.end()), v_cps.end());
            phonotactics_map[k_cps[0]] = std::move(v_cps);
        }
    }
}

bool Transliterator::can_form_conjunct(char32_t c1, char32_t c2) const {
    if (c1 == 0 || c2 == 0) return false;
    auto it = phonotactics_map.find(c1);
    if (it == phonotactics_map.end()) return false;
    return std::binary_search(it->second.begin(), it->second.end(), c2);
}

bool Transliterator::load_rules_from_toml(const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;

    rules.clear();
    phonotactics_map.clear();

    enum class Section { NONE, METADATA, PHONOTACTICS, RULE };
    Section current_section = Section::NONE;
    PhoneticRule current_rule;
    bool has_active_rule = false;

    auto flush_rule = [&]() {
        if (has_active_rule && !current_rule.pattern.empty()) {
            current_rule.output = utf8::normalize_nfc(sanitize_bengali_codepoints(current_rule.output));
            rules.push_back(current_rule);
        }
        current_rule = PhoneticRule{};
        has_active_rule = false;
    };

    std::string raw_line;
    while (std::getline(infile, raw_line)) {
        std::string line = utf8::trim(strip_toml_comment(raw_line));
        if (line.empty()) continue;

        if (line == "[[rule]]") {
            flush_rule();
            current_section = Section::RULE;
            has_active_rule = true;
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            flush_rule();
            std::string sec_name = utf8::trim(line.substr(1, line.length() - 2));
            if (sec_name == "metadata") {
                current_section = Section::METADATA;
            } else if (sec_name == "phonotactics") {
                current_section = Section::PHONOTACTICS;
            } else {
                current_section = Section::NONE;
            }
            continue;
        }

        std::string key, val;
        if (!split_toml_kv(line, key, val)) continue;

        if (current_section == Section::PHONOTACTICS) {
            std::string c1_str, c2_list_str;
            if (!parse_toml_quoted_string(key, c1_str)) c1_str = key;
            if (!parse_toml_quoted_string(val, c2_list_str)) continue;
            auto k_cps = utf8::to_codepoints(utf8::normalize_nfc(c1_str));
            auto v_cps = utf8::to_codepoints(utf8::normalize_nfc(c2_list_str));
            if (!k_cps.empty()) {
                std::sort(v_cps.begin(), v_cps.end());
                v_cps.erase(std::unique(v_cps.begin(), v_cps.end()), v_cps.end());
                phonotactics_map[k_cps[0]] = std::move(v_cps);
            }
        } else if (current_section == Section::RULE && has_active_rule) {
            std::string str_val;
            if (key == "pattern" && parse_toml_quoted_string(val, str_val)) {
                current_rule.pattern = str_val;
            } else if (key == "output" && parse_toml_quoted_string(val, str_val)) {
                current_rule.output = str_val;
            } else if (key == "left" && parse_toml_quoted_string(val, str_val)) {
                current_rule.context = parse_context(str_val);
            } else if (key == "right" && parse_toml_quoted_string(val, str_val)) {
                current_rule.right_context = parse_right_context(str_val);
            } else if (key == "category" && parse_toml_quoted_string(val, str_val)) {
                current_rule.category = parse_category(str_val);
            } else if (key == "setting" && parse_toml_quoted_string(val, str_val)) {
                current_rule.setting = str_val;
            } else if (key == "description" && parse_toml_quoted_string(val, str_val)) {
                current_rule.description = str_val;
            } else if (key == "priority") {
                try {
                    current_rule.priority = std::stod(val);
                } catch (...) {
                    current_rule.priority = 100.0;
                }
            } else if (key == "explicit") {
                current_rule.explicit_rule = (val == "true");
            } else if (key == "variant") {
                current_rule.variant = (val == "true");
            } else if (key == "overrides_explicit") {
                current_rule.overrides_explicit = (val == "true");
            }
        }
    }
    flush_rule();

    if (phonotactics_map.empty()) {
        init_default_phonotactics();
    }
    sort_rules();
    return !rules.empty();
}

void Transliterator::parse_rule_line(const std::string& line) {
    std::string trimmed = utf8::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;

    std::stringstream ss(trimmed);
    std::string pattern, output, context_str, category_str, desc;
    double priority = 100.0;

    if (std::getline(ss, pattern, '\t') &&
        std::getline(ss, output, '\t') &&
        std::getline(ss, context_str, '\t') &&
        std::getline(ss, category_str, '\t')) {

        std::string prio_str;
        if (std::getline(ss, prio_str, '\t')) {
            try {
                priority = std::stod(prio_str);
            } catch (...) {
                priority = 100.0;
            }
        }
        std::getline(ss, desc, '\t');

        PhoneticRule rule;
        rule.pattern = pattern;
        rule.output = utf8::normalize_nfc(sanitize_bengali_codepoints(output));
        rule.context = parse_context(context_str);
        if (rule.context == RuleContext::END) {
            rule.context = RuleContext::ANY;
            rule.right_context = RightContext::END;
        }
        rule.category = parse_category(category_str);
        rule.priority = priority;
        rule.description = desc;

        rules.push_back(rule);
    }
}

bool Transliterator::load_rules_from_file(const std::string& filepath) {
    if (filepath.length() >= 5 && filepath.substr(filepath.length() - 5) == ".toml") {
        return load_rules_from_toml(filepath);
    }

    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;

    rules.clear();
    init_default_phonotactics();

    std::string line;
    while (std::getline(infile, line)) {
        parse_rule_line(line);
    }

    sort_rules();
    return !rules.empty();
}

void Transliterator::init_default_rules() {
    init_default_phonotactics();
    rules.clear();

    auto add = [&](const char* pat, const char* out, RuleContext left, RightContext right,
                   OutputCategory cat, double prio, bool expl = false, bool var = false,
                   bool over_expl = false, const char* setting = "") {
        PhoneticRule r;
        r.pattern = pat;
        r.output = utf8::normalize_nfc(out);
        r.context = left;
        r.right_context = right;
        r.category = cat;
        r.priority = prio;
        r.explicit_rule = expl;
        r.variant = var;
        r.overrides_explicit = over_expl;
        r.setting = setting;
        rules.push_back(std::move(r));
    };

    // Accent / escape rules
    add("t``", "ৎ", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 580, true);
    add("r``y", "র\xE2\x80\x8D\xE0\xA7\x8Dয", RuleContext::ANY, RightContext::ANY, OutputCategory::PHALA, 580, true);
    add(".`", ".", RuleContext::ANY, RightContext::ANY, OutputCategory::PUNCTUATION, 560, true);
    add(":`", ":", RuleContext::ANY, RightContext::ANY, OutputCategory::PUNCTUATION, 560, true);
    add("$`", "$", RuleContext::ANY, RightContext::ANY, OutputCategory::PUNCTUATION, 560, true);
    add(",,", "্", RuleContext::ANY, RightContext::ANY, OutputCategory::HASANTA, 560, true);
    add("`rri", "ঋ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 540, true);
    add("`ee", "ঈ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 530, true);
    add("`oo", "উ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 530, true);
    add("`OI", "ঐ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 530, true);
    add("`OU", "ঔ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 530, true);
    add("`o", "অ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`a", "আ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`i", "ই", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`I", "ঈ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`u", "উ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`U", "ঊ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`e", "এ", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);
    add("`O", "ও", RuleContext::ANY, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 520, true);

    add("rri`", "ৃ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 460, true);
    add("ee`", "ী", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 450, true);
    add("oo`", "ু", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 450, true);
    add("OI`", "ৈ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 450, true);
    add("OU`", "ৌ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 450, true);
    add("a`", "া", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("i`", "ি", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("I`", "ী", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("u`", "ু", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("U`", "ূ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("e`", "ে", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("O`", "ো", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::KAR, 440, true);
    add("o`", "", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INHERENT_VOWEL, 440, true);
    add("`", "", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::SEPARATOR, 420, true);
    add("`", "", RuleContext::ANY, RightContext::ANY, OutputCategory::SEPARATOR, 50, true);

    // Forced phala & reph
    add("rZ", "র\xE2\x80\x8D\xE0\xA7\x8Dয", RuleContext::ANY, RightContext::ANY, OutputCategory::PHALA, 560, true);
    add("aZ", "অ্যা", RuleContext::START_OR_SEPARATOR, RightContext::ANY, OutputCategory::VOWEL_YA, 500, true);
    add("Z", "্য", RuleContext::ANY, RightContext::ANY, OutputCategory::PHALA, 450, true);
    add("rr", "র্", RuleContext::NOT_AFTER_CONSONANT, RightContext::BEFORE_CONSONANT, OutputCategory::REPH, 360);
    add("w", "্ব", RuleContext::AFTER_CONSONANT, RightContext::ANY, OutputCategory::PHALA, 220);
    add("w", "ও", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);

    // Vowels & kars
    add("rri", "ৃ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 320);
    add("rri", "ঋ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 320);
    add("OI", "ৈ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 310, true);
    add("OI", "ঐ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 310, true);
    add("OU", "ৌ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 310, true);
    add("OU", "ঔ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 310, true);
    add("ee", "ী", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 300);
    add("ee", "ঈ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 300);
    add("oo", "ু", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 300);
    add("oo", "উ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 300);
    add("I", "ী", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 180, true);
    add("I", "ঈ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 180, true);
    add("U", "ূ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 180, true);
    add("U", "ঊ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 180, true);
    add("O", "ো", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 180, true);
    add("O", "ও", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 180, true);
    add("o", "", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::INHERENT_VOWEL, 150);
    add("o", "অ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);
    add("a", "া", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 150);
    add("a", "য়া", RuleContext::AFTER_VOWEL, RightContext::ANY, OutputCategory::VOWEL_YA, 160);
    add("a", "আ", RuleContext::START_OR_SEPARATOR, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);
    add("i", "ি", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 150);
    add("i", "ই", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);
    add("u", "ু", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 150);
    add("u", "উ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);
    add("e", "ে", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 150);
    add("e", "এ", RuleContext::NOT_AFTER_CONSONANT, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 150);

    // Multi-key consonants & roots
    add("kkh", "ক্ষ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 380);
    add("kx", "ক্ষ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 380);
    add("jNG", "জ্ঞ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 380, true);
    add("gg", "জ্ঞ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 370);
    add("nch", "ঞ্ছ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 370);
    add("njh", "ঞ্ঝ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 370);
    add("nc", "ঞ্চ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 350);
    add("nj", "ঞ্জ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 350);
    add("kh", "খ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("gh", "ঘ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("Ng", "ঙ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 400, true);
    add("ng", "ং", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 260);
    add("ch", "ছ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("jh", "ঝ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("NG", "ঞ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 275, true);
    add("Th", "ঠ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 275, true);
    add("th", "থ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("Dh", "ঢ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 275, true);
    add("dh", "ধ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("ph", "ফ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("bh", "ভ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("Sh", "ষ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 275, true);
    add("sh", "শ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 270);
    add("Rh", "ঢ়", RuleContext::ANY, RightContext::ANY, OutputCategory::Y_CONSONANT, 275, true);

    // Y / y rules
    add("Y", "য়", RuleContext::ANY, RightContext::ANY, OutputCategory::Y_CONSONANT, 220, true);
    add("y", "ইয়", RuleContext::START, RightContext::ANY, OutputCategory::Y_CONSONANT, 200);
    add("y", "য", RuleContext::AFTER_REPH, RightContext::ANY, OutputCategory::CONSONANT, 200);
    add("y", "্য", RuleContext::AFTER_CONSONANT, RightContext::ANY, OutputCategory::PHALA, 200);
    add("y", "য়", RuleContext::AFTER_VOWEL_OR_SEPARATOR, RightContext::ANY, OutputCategory::Y_CONSONANT, 150);

    // Single consonants
    add("k", "ক", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("g", "গ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("c", "চ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("j", "জ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("J", "জ়", RuleContext::ANY, RightContext::ANY, OutputCategory::Y_CONSONANT, 160, true, false, false, "nukta_ja");
    add("T", "ট", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 160, true);
    add("t", "ত", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("D", "ড", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 160, true);
    add("d", "দ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("N", "ণ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 160, true);
    add("n", "ন", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("p", "প", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("f", "ফ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("b", "ব", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("v", "ভ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("m", "ম", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("z", "য", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("r", "র", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("R", "ড়", RuleContext::ANY, RightContext::ANY, OutputCategory::Y_CONSONANT, 160, true);
    add("l", "ল", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("S", "শ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 160, true);
    add("s", "স", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);
    add("h", "হ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 150);

    // Modifiers, symbols, digits
    add("^", "ঁ", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 300);
    add("qq", "ঁ", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 320);
    add("QQ", "ঁ", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 320);
    add(":", "ঃ", RuleContext::ANY, RightContext::ANY, OutputCategory::MODIFIER, 300);
    add("$", "৳", RuleContext::ANY, RightContext::ANY, OutputCategory::PUNCTUATION, 300, false, false, false, "taka_sign");
    add(".", "।", RuleContext::ANY, RightContext::ANY, OutputCategory::PUNCTUATION, 200);
    const char* bn_digits[10] = {"০","১","২","৩","৪","৫","৬","৭","৮","৯"};
    for (int d = 0; d <= 9; ++d) {
        char pat[2] = {static_cast<char>('0' + d), '\0'};
        add(pat, bn_digits[d], RuleContext::ANY, RightContext::ANY, OutputCategory::DIGIT, 150, false, false, false, "bangla_digits");
    }

    // Secondary beam variants
    add("oi", "ৈ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 410, false, true);
    add("OI", "ই", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 160, false, true, true);
    add("ou", "ৌ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 410, false, true);
    add("OU", "উ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::INDEPENDENT_VOWEL, 160, false, true, true);
    add("o", "ো", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 195, false, true);
    add("i", "ী", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 195, false, true);
    add("u", "ূ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 195, false, true);
    add("oo", "ূ", RuleContext::AFTER_CONSONANT_LIKE, RightContext::ANY, OutputCategory::KAR, 525, false, true);
    add("ya", "্য", RuleContext::AFTER_CONSONANT, RightContext::ANY, OutputCategory::PHALA, 410, false, true);
    add("r", "র্", RuleContext::NOT_AFTER_CONSONANT, RightContext::BEFORE_CONSONANT, OutputCategory::REPH, 195, false, true);
    add("r", "ড়", RuleContext::ANY, RightContext::ANY, OutputCategory::Y_CONSONANT, 195, false, true);
    add("n", "ণ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 195, false, true);
    add("nn", "ন্য", RuleContext::ANY, RightContext::ANY, OutputCategory::PHALA, 410, false, true);
    add("s", "শ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 195, false, true);
    add("sh", "স", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 525, false, true);
    add("sh", "ষ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 525, false, true);
    add("ch", "চ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 525, false, true);
    add("chh", "ছ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 740, false, true);
    add("ngk", "ঙ্ক", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 740, false, true);
    add("ngg", "ঙ্গ", RuleContext::ANY, RightContext::ANY, OutputCategory::CONSONANT, 740, false, true);

    sort_rules();
}

bool Transliterator::is_rule_enabled(const PhoneticRule& r) const {
    if (r.setting.empty()) return true;
    if (r.setting == "bangla_digits") return config.enable_bangla_digits;
    if (r.setting == "nukta_ja") return config.enable_nukta_ja;
    if (r.setting == "taka_sign") return config.enable_taka_sign;
    return true;
}

bool Transliterator::matches_before_consonant(const std::string& input, size_t pos) const {
    size_t in_len = input.length();
    if (pos >= in_len) return false;

    for (const auto& r : rules) {
        if (r.variant) continue;
        if (!is_rule_enabled(r)) continue;
        if (r.category != OutputCategory::CONSONANT &&
            r.category != OutputCategory::Y_CONSONANT &&
            r.category != OutputCategory::CONJUNCT) {
            continue;
        }
        size_t rlen = r.pattern.length();
        if (pos + rlen <= in_len && input.compare(pos, rlen, r.pattern) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<LatticeEdge> Transliterator::find_matching_edges(const std::string& input, size_t pos) const {
    std::vector<LatticeEdge> edges;
    size_t in_len = input.length();
    bool dot_protected = (pos < in_len && input[pos] == '.' && is_protected_dot(input, pos));
    bool dollar_protected = (pos < in_len && input[pos] == '$' && is_protected_dollar(input, pos));

    if (pos < in_len && input[pos] == '|' && config.pipe_mode != PipeMode::PASSTHROUGH) {
        LatticeEdge pipe_edge;
        pipe_edge.start_pos = pos;
        pipe_edge.end_pos = pos + 1;
        pipe_edge.bangla = (config.pipe_mode == PipeMode::LITERAL_DOT) ? "." : "।";
        pipe_edge.context = RuleContext::ANY;
        pipe_edge.right_context = RightContext::ANY;
        pipe_edge.category = OutputCategory::PUNCTUATION;
        pipe_edge.priority = 300.0;
        edges.push_back(std::move(pipe_edge));
    }

    for (const auto& r : rules) {
        if (!is_rule_enabled(r)) continue;

        size_t rlen = r.pattern.length();
        if (pos + rlen > in_len) continue;
        if (input.compare(pos, rlen, r.pattern) != 0) continue;

        // Guard single '.' inside decimals, ellipses, and abbreviations
        if (dot_protected && r.pattern == ".") {
            LatticeEdge lit_dot;
            lit_dot.start_pos = pos;
            lit_dot.end_pos = pos + 1;
            lit_dot.bangla = ".";
            lit_dot.context = RuleContext::ANY;
            lit_dot.right_context = RightContext::ANY;
            lit_dot.category = OutputCategory::PUNCTUATION;
            lit_dot.priority = r.priority;
            edges.push_back(std::move(lit_dot));
            continue;
        }

        // Guard '$' inside shell/code variables ($PATH, ${HOME}, $(cmd), $var)
        if (dollar_protected && r.pattern == "$") {
            LatticeEdge lit_dollar;
            lit_dollar.start_pos = pos;
            lit_dollar.end_pos = pos + 1;
            lit_dollar.bangla = "$";
            lit_dollar.context = RuleContext::ANY;
            lit_dollar.right_context = RightContext::ANY;
            lit_dollar.category = OutputCategory::PUNCTUATION;
            lit_dollar.priority = r.priority;
            edges.push_back(std::move(lit_dollar));
            continue;
        }

        // Right-context check
        if (r.right_context == RightContext::BEFORE_CONSONANT) {
            if (!matches_before_consonant(input, pos + rlen)) continue;
        } else if (r.right_context == RightContext::END) {
            if (pos + rlen < in_len && !std::isspace(static_cast<unsigned char>(input[pos + rlen]))) {
                continue;
            }
        }

        LatticeEdge edge;
        edge.start_pos = pos;
        edge.end_pos = pos + rlen;
        edge.bangla = r.output;
        edge.context = r.context;
        edge.right_context = r.right_context;
        edge.category = r.category;
        edge.priority = r.priority;
        edge.explicit_rule = r.explicit_rule;
        edge.variant = r.variant;
        edge.overrides_explicit = r.overrides_explicit;
        edges.push_back(std::move(edge));
    }

    return edges;
}

std::vector<std::pair<std::string, double>> Transliterator::transliterate_chunk_beam(
    const std::string& raw_input,
    const Dictionary* dict,
    size_t beam_width
) const {
    std::vector<std::pair<std::string, double>> results;
    if (raw_input.empty()) return results;

    std::string input = normalize_chandrabindu_qq(raw_input);

    if (is_url_or_email(input) || is_file_path_span(input) ||
        is_abbreviation_span(input) || is_uppercase_acronym(input) ||
        is_protected_dollar_span(input)) {
        return {{utf8::normalize_nfc(sanitize_bengali_codepoints(input)), 10000.0}};
    }

    const size_t effective_beam = std::min<size_t>(std::max<size_t>(beam_width, 8), 64);
    const size_t n = input.length();
    std::vector<std::vector<Hypothesis>> beam(n + 1);

    Hypothesis initial;
    initial.pos = 0;
    initial.state = PrevState::START;
    beam[0].push_back(initial);

    auto push_or_merge = [](std::vector<Hypothesis>& bucket, Hypothesis&& candidate) {
        for (auto& existing : bucket) {
            if (existing.same_state_as(candidate)) {
                if (candidate.phonetic_score > existing.phonetic_score) {
                    existing.phonetic_score = candidate.phonetic_score;
                }
                return;
            }
        }
        bucket.push_back(std::move(candidate));
    };

    for (size_t pos = 0; pos < n; ++pos) {
        if (beam[pos].empty()) continue;

        if (beam[pos].size() > effective_beam) {
            for (auto& h : beam[pos]) {
                h.final_score = h.phonetic_score;
                if (dict && has_dictionary_prefix(dict, h.text)) {
                    h.final_score += 40.0;
                }
            }
            std::sort(beam[pos].begin(), beam[pos].end(), [dict](const Hypothesis& a, const Hypothesis& b) {
                if (a.overrides_explicit != b.overrides_explicit) {
                    return !a.overrides_explicit;
                }
                if (!dict && a.used_variant != b.used_variant) {
                    return !a.used_variant;
                }
                if (std::abs(a.final_score - b.final_score) > 1e-9) {
                    return a.final_score > b.final_score;
                }
                if (a.used_variant != b.used_variant) {
                    return !a.used_variant;
                }
                if (a.text != b.text) {
                    return a.text < b.text;
                }
                return static_cast<int>(a.state) < static_cast<int>(b.state);
            });
            beam[pos].resize(effective_beam);
        }

        std::vector<LatticeEdge> edges = find_matching_edges(input, pos);

        // Safe UTF-8 fallback edge so the beam never reaches a dead end on arbitrary bytes
        LatticeEdge fallback;
        fallback.start_pos = pos;
        unsigned char c = static_cast<unsigned char>(input[pos]);
        size_t seq_len = 1;
        bool valid = true;
        if (c < 0x80) {
            seq_len = 1;
        } else if ((c >> 5) == 0x06) {
            seq_len = 2;
            if (pos + 1 >= n || (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 || c < 0xC2) {
                valid = false;
            }
        } else if ((c >> 4) == 0x0E) {
            seq_len = 3;
            if (pos + 2 >= n ||
                (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 2]) & 0xC0) != 0x80) {
                valid = false;
            } else {
                unsigned char c2 = static_cast<unsigned char>(input[pos + 1]);
                if (c == 0xE0 && c2 < 0xA0) valid = false;
                if (c == 0xED && c2 >= 0xA0) valid = false;
            }
        } else if ((c >> 3) == 0x1E) {
            seq_len = 4;
            if (pos + 3 >= n ||
                (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 3]) & 0xC0) != 0x80) {
                valid = false;
            } else {
                unsigned char c2 = static_cast<unsigned char>(input[pos + 1]);
                if (c == 0xF0 && c2 < 0x90) valid = false;
                if (c == 0xF4 && c2 >= 0x90) valid = false;
            }
        } else {
            valid = false;
        }

        if (!valid) {
            fallback.end_pos = pos + 1;
            fallback.bangla = "\xEF\xBF\xBD"; // U+FFFD replacement character
        } else {
            fallback.end_pos = pos + seq_len;
            fallback.bangla = sanitize_bengali_codepoints(input.substr(pos, seq_len));
        }
        fallback.context = RuleContext::ANY;
        fallback.right_context = RightContext::ANY;
        fallback.category = OutputCategory::OTHER;
        fallback.priority = 1.0;

        for (const auto& hyp : beam[pos]) {
            bool matched_primary = false;
            for (const auto& edge : edges) {
                if (!matches_left_context(edge.context, hyp.state, pos, input)) {
                    continue;
                }
                if (!edge.variant) {
                    matched_primary = true;
                }

                Hypothesis next_hyp;
                next_hyp.pos = edge.end_pos;
                next_hyp.text = hyp.text;

                const double edge_len = static_cast<double>(edge.end_pos - edge.start_pos);
                double edge_score = edge.priority;
                if (!edge.variant && edge.priority > 1.0) {
                    // Longest-match per-character lattice law: an L-char valid rule strictly
                    // outscores any finer partition into shorter rules, while equal-partition
                    // ambiguities resolve via rule priority.
                    edge_score = 200.0 * edge_len + 120.0 * (edge_len - 1.0) + 0.1 * edge.priority;
                }

                next_hyp.phonetic_score = hyp.phonetic_score + edge_score;
                next_hyp.used_variant = hyp.used_variant || edge.variant;
                next_hyp.overrides_explicit = hyp.overrides_explicit || edge.overrides_explicit;
                next_hyp.last_consonant = 0;

                switch (edge.category) {
                    case OutputCategory::CONSONANT:
                    case OutputCategory::CONJUNCT: {
                        char32_t first_cp = extract_first_base_consonant(edge.bangla);
                        char32_t last_cp = extract_last_base_consonant(edge.bangla);

                        if (hyp.state == PrevState::AFTER_REPH) {
                            // Reph (র্) already ends with U+09CD
                            next_hyp.text += edge.bangla;
                            next_hyp.state = PrevState::AFTER_CONSONANT;
                            next_hyp.last_consonant = last_cp;
                        } else if (hyp.state == PrevState::AFTER_CONSONANT &&
                                   can_form_conjunct(hyp.last_consonant, first_cp)) {
                            // Also fork a secondary non-conjunct hypothesis for dictionary disambiguation (e.g. amra -> আমরা)
                            Hypothesis no_virama = next_hyp;
                            no_virama.text += edge.bangla;
                            no_virama.phonetic_score -= 15.0;
                            no_virama.used_variant = true;
                            no_virama.state = PrevState::AFTER_CONSONANT;
                            no_virama.last_consonant = last_cp;
                            push_or_merge(beam[edge.end_pos], std::move(no_virama));

                            // Primary rule: general C1 + ্ + C2 conjunct formation
                            next_hyp.text += "\xE0\xA7\x8D";
                            next_hyp.text += edge.bangla;
                            next_hyp.state = PrevState::AFTER_CONSONANT;
                            next_hyp.last_consonant = last_cp;
                        } else {
                            next_hyp.text += edge.bangla;
                            next_hyp.state = PrevState::AFTER_CONSONANT;
                            next_hyp.last_consonant = last_cp;
                        }
                        break;
                    }

                    case OutputCategory::Y_CONSONANT:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::AFTER_Y_CONSONANT;
                        break;

                    case OutputCategory::PHALA:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::AFTER_PHALA;
                        break;

                    case OutputCategory::REPH:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::AFTER_REPH;
                        break;

                    case OutputCategory::KAR:
                    case OutputCategory::INDEPENDENT_VOWEL:
                    case OutputCategory::INHERENT_VOWEL:
                    case OutputCategory::VOWEL_YA:
                    case OutputCategory::DIPHTHONG_SPLIT:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::AFTER_VOWEL;
                        break;

                    case OutputCategory::SEPARATOR:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::AFTER_SEPARATOR;
                        break;

                    case OutputCategory::HASANTA:
                        next_hyp.text += edge.bangla;
                        next_hyp.state = PrevState::OTHER;
                        break;

                    case OutputCategory::MODIFIER:
                        next_hyp.text += edge.bangla;
                        if (edge.bangla == "ঁ" && hyp.state == PrevState::AFTER_VOWEL) {
                            next_hyp.state = PrevState::AFTER_VOWEL;
                        } else {
                            next_hyp.state = PrevState::OTHER;
                        }
                        break;

                    case OutputCategory::PUNCTUATION:
                    case OutputCategory::DIGIT:
                    case OutputCategory::OTHER:
                        next_hyp.text += edge.bangla;
                        if (edge.bangla.length() == 1 &&
                            std::isspace(static_cast<unsigned char>(edge.bangla[0]))) {
                            next_hyp.state = PrevState::START;
                        } else {
                            next_hyp.state = PrevState::OTHER;
                        }
                        break;
                }

                push_or_merge(beam[edge.end_pos], std::move(next_hyp));
            }

            // Emit UTF-8 passthrough fallback only when no primary rule matched at this position
            if (!matched_primary) {
                Hypothesis fb_hyp;
                fb_hyp.pos = fallback.end_pos;
                fb_hyp.text = hyp.text + fallback.bangla;
                fb_hyp.phonetic_score = hyp.phonetic_score + fallback.priority;
                fb_hyp.used_variant = hyp.used_variant;
                fb_hyp.overrides_explicit = hyp.overrides_explicit;
                fb_hyp.last_consonant = 0;
                if (fallback.bangla.length() == 1 &&
                    std::isspace(static_cast<unsigned char>(fallback.bangla[0]))) {
                    fb_hyp.state = PrevState::START;
                } else {
                    fb_hyp.state = PrevState::OTHER;
                }
                push_or_merge(beam[fallback.end_pos], std::move(fb_hyp));
            }
        }
    }

    auto& final_beam = beam[n];
    if (final_beam.empty()) return results;

    double best_non_override_score = 0.0;
    for (auto& hyp : final_beam) {
        hyp.text = utf8::normalize_nfc(sanitize_bengali_codepoints(hyp.text));
        hyp.final_score = hyp.phonetic_score;
        if (dict && !hyp.text.empty() && dict->contains(hyp.text) && !hyp.overrides_explicit) {
            uint32_t freq = dict->get_frequency(hyp.text);
            hyp.final_score += 350.0 + 50.0 * std::log10(static_cast<double>(freq + 1));
        }
        if (!hyp.overrides_explicit && hyp.final_score > best_non_override_score) {
            best_non_override_score = hyp.final_score;
        }
    }
    if (best_non_override_score > 0.0) {
        for (auto& hyp : final_beam) {
            if (hyp.overrides_explicit) {
                hyp.final_score = std::min(hyp.final_score, best_non_override_score * 0.45);
            }
        }
    }

    std::sort(final_beam.begin(), final_beam.end(), [dict](const Hypothesis& a, const Hypothesis& b) {
        if (a.overrides_explicit != b.overrides_explicit) {
            return !a.overrides_explicit;
        }
        if (!dict && a.used_variant != b.used_variant) {
            return !a.used_variant;
        }
        if (std::abs(a.final_score - b.final_score) > 1e-9) {
            return a.final_score > b.final_score;
        }
        if (a.used_variant != b.used_variant) {
            return !a.used_variant;
        }
        return a.text < b.text;
    });

    for (const auto& hyp : final_beam) {
        bool seen = false;
        for (const auto& r : results) {
            if (r.first == hyp.text) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            results.push_back({hyp.text, hyp.final_score});
            if (results.size() >= beam_width) break;
        }
    }

    return results;
}

std::vector<std::pair<std::string, double>> Transliterator::transliterate_beam(
    const std::string& raw_input,
    const Dictionary* dict,
    size_t beam_width
) const {
    if (raw_input.empty()) return {};

    std::string input = normalize_chandrabindu_qq(raw_input);

    // Check if input contains whitespace; if it is a single token <= MAX_CHUNK_BYTES, run full beam directly
    constexpr size_t MAX_CHUNK_BYTES = 256;
    bool has_ws = false;
    for (unsigned char uc : input) {
        if (std::isspace(uc)) {
            has_ws = true;
            break;
        }
    }
    if (!has_ws && input.length() <= MAX_CHUNK_BYTES) {
        return transliterate_chunk_beam(input, dict, beam_width);
    }

    // Token-preserving bounded-memory decoding across whitespace-separated spans so URLs,
    // emails, file paths, abbreviations, decimals, and $VAR tokens are protected mid-sentence too
    std::string combined;
    combined.reserve(input.length() * 2);
    double total_score = 0.0;

    size_t offset = 0;
    const size_t n = input.length();
    while (offset < n) {
        if (std::isspace(static_cast<unsigned char>(input[offset]))) {
            size_t ws_end = offset + 1;
            while (ws_end < n && std::isspace(static_cast<unsigned char>(input[ws_end]))) {
                ws_end++;
            }
            combined.append(input, offset, ws_end - offset);
            offset = ws_end;
            continue;
        }

        size_t tok_end = offset + 1;
        while (tok_end < n && !std::isspace(static_cast<unsigned char>(input[tok_end]))) {
            tok_end++;
        }

        // Decode token (chunking safely at UTF-8 boundaries if a single token exceeds MAX_CHUNK_BYTES)
        size_t sub_off = offset;
        while (sub_off < tok_end) {
            size_t limit = std::min(tok_end, sub_off + MAX_CHUNK_BYTES);
            if (limit < tok_end) {
                while (limit > sub_off + 1 &&
                       (static_cast<unsigned char>(input[limit]) & 0xC0) == 0x80) {
                    limit--;
                }
            }
            auto chunk_res = transliterate_chunk_beam(input.substr(sub_off, limit - sub_off), dict, 8);
            if (!chunk_res.empty()) {
                combined += chunk_res[0].first;
                total_score += chunk_res[0].second;
            }
            sub_off = limit;
        }
        offset = tok_end;
    }

    return {{utf8::normalize_nfc(combined), total_score}};
}

std::string Transliterator::transliterate(const std::string& input, const Dictionary* dict) const {
    if (input.empty()) return "";
    auto beam_results = transliterate_beam(input, dict, 24);
    if (!beam_results.empty()) {
        return beam_results[0].first;
    }
    return utf8::normalize_nfc(sanitize_bengali_codepoints(input));
}

std::vector<std::string> Transliterator::get_phonetic_variants(const std::string& input, size_t max_variants) const {
    std::vector<std::string> variants;
    auto beam_results = transliterate_beam(input, nullptr, std::max<size_t>(max_variants * 3, 12));
    for (const auto& r : beam_results) {
        if (!r.first.empty() || variants.empty()) {
            variants.push_back(r.first);
            if (variants.size() >= max_variants) break;
        }
    }
    return variants;
}

} // namespace abk
