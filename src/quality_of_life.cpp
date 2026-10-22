#include "abk/quality_of_life.h"
#include "abk/transliterator.h"
#include "abk/token_classifier.h"
#include "abk/utf8_util.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace abk {

namespace {

static std::string get_system_os_info() {
#if defined(__linux__) || defined(__APPLE__)
    struct utsname un;
    if (uname(&un) == 0) {
        return std::string(un.sysname) + " " + un.release + " (" + un.machine + ")";
    }
#elif defined(_WIN32)
    return "Microsoft Windows";
#endif
    return "Unknown OS";
}

static std::string get_current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(_WIN32)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

static std::string get_default_config_dir() {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return std::string(appdata) + "\\AmarBanglaKotha";
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config/amar-bangla-kotha";
    return ".";
#endif
}

} // anonymous namespace

// ----------------------------------------------------------------------
// CorrectionLogger Implementation
// ----------------------------------------------------------------------

CorrectionLogger::CorrectionLogger(const std::string& filepath) {
    if (!filepath.empty()) {
        filepath_ = filepath;
    } else {
        filepath_ = default_log_path();
    }
}

std::string CorrectionLogger::default_log_path() const {
    return get_default_config_dir() + "/correction_log.jsonl";
}

void CorrectionLogger::set_enabled(bool enable) {
    enabled_ = enable;
}

void CorrectionLogger::record_correction(const std::string& raw_input, const std::string& corrected_word, bool is_secure_mode) {
    // STRICT RULE: Never record anything in secure/password mode!
    if (is_secure_mode) return;
    if (!enabled_) return;
    if (raw_input.empty() || corrected_word.empty()) return;

    CorrectionRecord rec;
    rec.timestamp = get_current_iso_timestamp();
    rec.raw_input = raw_input;
    rec.corrected_word = corrected_word;

    memory_records_.push_back(rec);
    if (memory_records_.size() > 1000) {
        memory_records_.erase(memory_records_.begin());
    }

    append_to_disk(rec);
}

void CorrectionLogger::append_to_disk(const CorrectionRecord& record) {
    if (filepath_.empty()) return;
    std::ofstream f(filepath_, std::ios::app);
    if (!f.is_open()) return;

    // Simple JSON Lines format
    f << "{\"timestamp\":\"" << record.timestamp
      << "\",\"raw_input\":\"" << record.raw_input
      << "\",\"corrected_word\":\"" << record.corrected_word
      << "\"}\n";
}

std::vector<CorrectionRecord> CorrectionLogger::get_records() const {
    load_from_disk();
    return memory_records_;
}

void CorrectionLogger::load_from_disk() const {
    if (filepath_.empty()) return;
    std::ifstream f(filepath_);
    if (!f.is_open()) return;

    memory_records_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '{') continue;
        CorrectionRecord rec;
        // Parse simple JSON fields
        auto parse_field = [](const std::string& l, const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            size_t p = l.find(needle);
            if (p == std::string::npos) return "";
            p += needle.size();
            size_t end = l.find("\"", p);
            if (end == std::string::npos) return "";
            return l.substr(p, end - p);
        };
        rec.timestamp = parse_field(line, "timestamp");
        rec.raw_input = parse_field(line, "raw_input");
        rec.corrected_word = parse_field(line, "corrected_word");
        if (!rec.raw_input.empty() && !rec.corrected_word.empty()) {
            memory_records_.push_back(rec);
        }
    }
}

void CorrectionLogger::clear_records() {
    memory_records_.clear();
    if (!filepath_.empty()) {
        std::ofstream f(filepath_, std::ios::trunc);
    }
}

size_t CorrectionLogger::record_count() const {
    load_from_disk();
    return memory_records_.size();
}

// ----------------------------------------------------------------------
// QualityOfLife Implementation
// ----------------------------------------------------------------------

std::string QualityOfLife::convert_banglish_to_bangla(const std::string& text, const Dictionary* dict) {
    if (text.empty()) return "";

    Transliterator trans;
    TokenClassifier classifier;
    std::string out;

    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c <= ' ') {
            out.push_back(text[i]);
            i++;
            continue;
        }

        // Collect chunk until whitespace
        size_t start = i;
        while (i < text.size() && static_cast<unsigned char>(text[i]) > ' ') {
            i++;
        }
        std::string chunk = text.substr(start, i - start);

        // Check if chunk is URL, email, file path or code
        if (classifier.should_bypass(chunk)) {
            out += chunk;
            continue;
        }

        // Separate leading punctuation (e.g. '(', '"', '[')
        size_t prefix_len = 0;
        while (prefix_len < chunk.size() && (chunk[prefix_len] == '(' || chunk[prefix_len] == '[' ||
               chunk[prefix_len] == '{' || chunk[prefix_len] == '"' || chunk[prefix_len] == '\'')) {
            prefix_len++;
        }

        // Separate trailing punctuation (e.g. '.', ',', '?', '!', ';', ':', ')', ']')
        size_t suffix_len = 0;
        while (suffix_len < (chunk.size() - prefix_len)) {
            char ch = chunk[chunk.size() - 1 - suffix_len];
            if (ch == '.' || ch == ',' || ch == '?' || ch == '!' || ch == ';' || ch == ':' ||
                ch == ')' || ch == ']' || ch == '}' || ch == '"' || ch == '\'') {
                suffix_len++;
            } else {
                break;
            }
        }

        std::string prefix = chunk.substr(0, prefix_len);
        std::string core = chunk.substr(prefix_len, chunk.size() - prefix_len - suffix_len);
        std::string suffix = chunk.substr(chunk.size() - suffix_len);

        out += prefix;
        if (!core.empty()) {
            if (classifier.should_bypass(core)) {
                out += core;
            } else {
                out += trans.transliterate(core, dict);
            }
        }
        out += suffix;
    }

    return out;
}

std::string QualityOfLife::convert_bangla_to_banglish(const std::string& text) {
    if (text.empty()) return "";

    auto cps = utf8::to_codepoints(text);
    std::string out;

    for (size_t i = 0; i < cps.size(); ++i) {
        char32_t cp = cps[i];

        // Vowels
        switch (cp) {
            case 0x0985: out += "o"; continue;
            case 0x0986: out += "a"; continue;
            case 0x0987: out += "i"; continue;
            case 0x0988: out += "ee"; continue;
            case 0x0989: out += "u"; continue;
            case 0x098A: out += "oo"; continue;
            case 0x098B: out += "rri"; continue;
            case 0x098F: out += "e"; continue;
            case 0x0990: out += "oi"; continue;
            case 0x0993: out += "o"; continue;
            case 0x0994: out += "ou"; continue;

            // Kars
            case 0x09BE: out += "a"; continue;
            case 0x09BF: out += "i"; continue;
            case 0x09C0: out += "ee"; continue;
            case 0x09C1: out += "u"; continue;
            case 0x09C2: out += "oo"; continue;
            case 0x09C3: out += "ri"; continue;
            case 0x09C7: out += "e"; continue;
            case 0x09C8: out += "oi"; continue;
            case 0x09CB: out += "o"; continue;
            case 0x09CC: out += "ou"; continue;

            // Consonants
            case 0x0995: out += "k"; break;
            case 0x0996: out += "kh"; break;
            case 0x0997: out += "g"; break;
            case 0x0998: out += "gh"; break;
            case 0x0999: out += "ng"; break;
            case 0x099A: out += "ch"; break;
            case 0x099B: out += "chh"; break;
            case 0x099C: out += "j"; break;
            case 0x099D: out += "jh"; break;
            case 0x099E: out += "n"; break;
            case 0x099F: out += "T"; break;
            case 0x09A0: out += "Th"; break;
            case 0x09A1: out += "D"; break;
            case 0x09A2: out += "Dh"; break;
            case 0x09A3: out += "N"; break;
            case 0x09A4: out += "t"; break;
            case 0x09A5: out += "th"; break;
            case 0x09A6: out += "d"; break;
            case 0x09A7: out += "dh"; break;
            case 0x09A8: out += "n"; break;
            case 0x09AA: out += "p"; break;
            case 0x09AB: out += "f"; break;
            case 0x09AC: out += "b"; break;
            case 0x09AD: out += "bh"; break;
            case 0x09AE: out += "m"; break;
            case 0x09AF: out += "z"; break;
            case 0x09B0: out += "r"; break;
            case 0x09B2: out += "l"; break;
            case 0x09B6: out += "sh"; break;
            case 0x09B7: out += "sh"; break;
            case 0x09B8: out += "s"; break;
            case 0x09B9: out += "h"; break;
            case 0x09DC: out += "R"; break;
            case 0x09DD: out += "Rh"; break;
            case 0x09DF: out += "y"; break;
            case 0x09CE: out += "t"; continue;
            case 0x0982: out += "ng"; continue;
            case 0x0983: out += ":"; continue;
            case 0x0981: out += "^"; continue;
            case 0x09CD: continue; // Virama joins consonants

            // Numerals
            case 0x09E6: out += "0"; continue;
            case 0x09E7: out += "1"; continue;
            case 0x09E8: out += "2"; continue;
            case 0x09E9: out += "3"; continue;
            case 0x09EA: out += "4"; continue;
            case 0x09EB: out += "5"; continue;
            case 0x09EC: out += "6"; continue;
            case 0x09ED: out += "7"; continue;
            case 0x09EE: out += "8"; continue;
            case 0x09EF: out += "9"; continue;

            // Punctuation
            case 0x0964: out += "."; continue;
            case 0x0965: out += ".."; continue;

            default:
                out += utf8::to_utf8(cp);
                continue;
        }

        // If consonant was matched, check if next is a kar or virama or space
        if (i + 1 < cps.size()) {
            char32_t next_cp = cps[i + 1];
            if (utf8::is_bengali_kar(next_cp) || next_cp == utf8::BN_VIRAMA ||
                next_cp == 0x0982 || next_cp == 0x0983 || next_cp == 0x0981) {
                // Next kar will supply the vowel
            } else if (utf8::is_bengali_consonant(next_cp)) {
                // Inherent 'o'
                out += "o";
            }
        }
    }

    return out;
}

std::string QualityOfLife::apply_smart_punctuation(const std::string& text) {
    if (text.empty()) return "";

    std::string s = text;

    // 1. Triple dot '...' -> '…' (U+2026)
    size_t pos = 0;
    while ((pos = s.find("...", pos)) != std::string::npos) {
        s.replace(pos, 3, "\xE2\x80\xA6"); // …
        pos += 3;
    }

    // 2. Smart quotes: replace paired '"' with “ and ”
    bool open_quote = true;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') {
            if (open_quote) {
                s.replace(i, 1, "\xE2\x80\x9C"); // “
                open_quote = false;
                i += 2;
            } else {
                s.replace(i, 1, "\xE2\x80\x9D"); // ”
                open_quote = true;
                i += 2;
            }
        }
    }

    // 3. Smart period '.' -> '।' (Bengali Dari)
    // Only convert '.' if:
    //  - NOT between digits (e.g. "3.14")
    //  - NOT inside URLs (e.g. "google.com")
    //  - NOT following known abbreviations ("e.g.", "i.e.", "ডা.", "ড.")
    static const std::vector<std::string> ABBREVIATIONS = {
        "e.g", "i.e", "etc", "vs", "dr", "mr", "mrs", "ms", "prof", "ডা", "ড"
    };

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '.') {
            bool bypass = false;

            // Check preceding and following digits
            if (i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1])) &&
                i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1]))) {
                bypass = true;
            }

            // Check URL/email domain extension: letters both sides without space
            if (i > 0 && std::isalpha(static_cast<unsigned char>(s[i - 1])) &&
                i + 1 < s.size() && std::isalpha(static_cast<unsigned char>(s[i + 1]))) {
                bypass = true;
            }

            // Check preceding word for abbreviations
            if (!bypass && i > 0) {
                long long start = static_cast<long long>(i) - 1;
                while (start >= 0 && s[start] != ' ' && s[start] != '\n') {
                    start--;
                }
                std::string word = s.substr(start + 1, i - (start + 1));
                std::string lower_word = utf8::to_lower_ascii(word);
                for (const auto& abbr : ABBREVIATIONS) {
                    if (lower_word == abbr) {
                        bypass = true;
                        break;
                    }
                }
            }

            if (!bypass) {
                s.replace(i, 1, "।"); // U+0964 (3 bytes in UTF-8: 0xE0 0xA5 0xA4)
                i += 3;
                continue;
            }
        }
        i++;
    }

    return s;
}

std::vector<Candidate> QualityOfLife::get_emoji_suggestions(const std::string& keyword) {
    std::vector<Candidate> results;
    if (keyword.empty()) return results;

    std::string key = utf8::to_lower_ascii(keyword);

    struct EmojiEntry {
        std::vector<std::string> triggers;
        std::string emoji;
        std::string name;
    };

    static const std::vector<EmojiEntry> EMOJI_DB = {
        {{"smile", "hasi", "khushi", ":)"}, "😊", "Smiling Face"},
        {{"grin", "anondo", ":D"}, "😀", "Grinning Face"},
        {{"love", "bhalobasha", "valobasha", "prem", "<3"}, "❤️", "Red Heart"},
        {{"sparkle_heart", "heart", "ador"}, "💖", "Sparkling Heart"},
        {{"fire", "agun"}, "🔥", "Fire"},
        {{"flower", "ful"}, "🌸", "Cherry Blossom"},
        {{"rose", "golap"}, "🌹", "Rose"},
        {{"star", "tara"}, "⭐", "Star"},
        {{"sun", "shurjo", "rod"}, "☀️", "Sun"},
        {{"moon", "chad"}, "🌙", "Crescent Moon"},
        {{"pray", "namaste", "dua", "monajat"}, "🙏", "Folded Hands"},
        {{"check", "thik", "ok"}, "✔️", "Check Mark"},
        {{"thumbs_up", "like", "bhalo"}, "👍", "Thumbs Up"},
        {{"bd", "bangladesh"}, "🇧🇩", "Flag of Bangladesh"},
        {{"tk", "taka", "poisa"}, "৳", "Bangladeshi Taka"},
        {{"cry", "kanna", "sad"}, "😢", "Crying Face"},
        {{"party", "celebration", "utsob"}, "🎉", "Party Popper"},
        {{"hundred", "eksho", "100"}, "💯", "Hundred Points"},
        {{"handshake", "shongjukto"}, "🤝", "Handshake"}
    };

    for (const auto& entry : EMOJI_DB) {
        for (const auto& tr : entry.triggers) {
            if (tr == key || (key.size() >= 3 && tr.find(key) == 0)) {
                double score = 6200.0;
                // If it's a symbolic emoticon like :) or <3, give it high score
                if (!tr.empty() && (tr[0] == ':' || tr[0] == '<')) {
                    score = 9500.0;
                }
                results.push_back(Candidate(entry.emoji, score, CandidateType::EMOJI_SUGGESTION, "Emoji: " + entry.name));
                break;
            }
        }
    }

    return results;
}

bool QualityOfLife::save_feedback_locally(const FeedbackItem& item, const std::string& custom_path) {
    std::string path = custom_path.empty() ? (get_default_config_dir() + "/feedback.jsonl") : custom_path;
    std::ofstream f(path, std::ios::app);
    if (!f.is_open()) return false;

    std::string ts = item.timestamp.empty() ? get_current_iso_timestamp() : item.timestamp;
    std::string os = item.os_info.empty() ? get_system_os_info() : item.os_info;

    f << "{\"timestamp\":\"" << ts
      << "\",\"raw_input\":\"" << item.raw_input
      << "\",\"received\":\"" << item.received_suggestion
      << "\",\"expected\":\"" << item.expected_word
      << "\",\"os\":\"" << os
      << "\",\"app\":\"" << item.app_name
      << "\"}\n";

    return true;
}

std::vector<FeedbackItem> QualityOfLife::load_local_feedback(const std::string& custom_path) {
    std::string path = custom_path.empty() ? (get_default_config_dir() + "/feedback.jsonl") : custom_path;
    std::vector<FeedbackItem> items;
    std::ifstream f(path);
    if (!f.is_open()) return items;

    std::string line;
    auto parse_field = [](const std::string& l, const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\":\"";
        size_t p = l.find(needle);
        if (p == std::string::npos) return "";
        p += needle.size();
        size_t end = l.find("\"", p);
        if (end == std::string::npos) return "";
        return l.substr(p, end - p);
    };

    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '{') continue;
        FeedbackItem item;
        item.timestamp = parse_field(line, "timestamp");
        item.raw_input = parse_field(line, "raw_input");
        item.received_suggestion = parse_field(line, "received");
        item.expected_word = parse_field(line, "expected");
        item.os_info = parse_field(line, "os");
        item.app_name = parse_field(line, "app");
        items.push_back(item);
    }
    return items;
}

void QualityOfLife::clear_local_feedback(const std::string& custom_path) {
    std::string path = custom_path.empty() ? (get_default_config_dir() + "/feedback.jsonl") : custom_path;
    std::ofstream f(path, std::ios::trunc);
}

} // namespace abk
