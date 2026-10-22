#include "abk/personal_dictionary.h"
#include "abk/utf8_util.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>

namespace abk {

PersonalDictionary::PersonalDictionary() : max_user_words(10000) {}

PersonalDictionary::PersonalDictionary(size_t max_words) : max_user_words(max_words) {}

void PersonalDictionary::learn_word(const std::string& raw_word, uint32_t frequency) {
    if (raw_word.empty()) return;
    std::string word = utf8::normalize_nfc(utf8::trim(raw_word));
    if (word.empty()) return;

    uint32_t existing_freq = 0;
    if (user_trie.search(word, &existing_freq)) {
        user_trie.insert(word, existing_freq + frequency);
    } else {
        user_trie.insert(word, frequency);
    }
}

bool PersonalDictionary::contains_word(const std::string& raw_word) const {
    if (raw_word.empty()) return false;
    std::string word = utf8::normalize_nfc(utf8::trim(raw_word));
    return user_trie.search(word);
}

uint32_t PersonalDictionary::get_word_frequency(const std::string& raw_word) const {
    if (raw_word.empty()) return 0;
    std::string word = utf8::normalize_nfc(utf8::trim(raw_word));
    uint32_t freq = 0;
    if (user_trie.search(word, &freq)) {
        return freq;
    }
    return 0;
}

std::vector<std::pair<std::string, uint32_t>> PersonalDictionary::find_words_by_prefix(
    const std::string& prefix, size_t limit) const {
    return user_trie.find_by_prefix(prefix, limit);
}

void PersonalDictionary::add_shortcut(const std::string& raw_shortcut, const std::string& raw_expansion) {
    if (raw_shortcut.empty() || raw_expansion.empty()) return;
    std::string shortcut = utf8::normalize_nfc(utf8::trim(raw_shortcut));
    std::string expansion = utf8::normalize_nfc(utf8::trim(raw_expansion));
    if (shortcut.empty() || expansion.empty()) return;

    shortcuts[shortcut] = UserShortcut(shortcut, expansion, 0);
}

bool PersonalDictionary::remove_shortcut(const std::string& raw_shortcut) {
    std::string shortcut = utf8::normalize_nfc(utf8::trim(raw_shortcut));
    return shortcuts.erase(shortcut) > 0;
}

bool PersonalDictionary::has_shortcut(const std::string& raw_shortcut) const {
    if (raw_shortcut.empty()) return false;
    std::string shortcut = utf8::normalize_nfc(utf8::trim(raw_shortcut));
    return shortcuts.find(shortcut) != shortcuts.end();
}

std::string PersonalDictionary::get_expansion(const std::string& raw_shortcut) const {
    if (raw_shortcut.empty()) return "";
    std::string shortcut = utf8::normalize_nfc(utf8::trim(raw_shortcut));
    auto it = shortcuts.find(shortcut);
    if (it != shortcuts.end()) {
        return it->second.expansion;
    }
    return "";
}

std::vector<UserShortcut> PersonalDictionary::get_all_shortcuts() const {
    std::vector<UserShortcut> list;
    list.reserve(shortcuts.size());
    for (const auto& pair : shortcuts) {
        list.push_back(pair.second);
    }
    std::sort(list.begin(), list.end(), [](const UserShortcut& a, const UserShortcut& b) {
        return a.shortcut < b.shortcut;
    });
    return list;
}

bool PersonalDictionary::parse_line(const std::string& line) {
    std::string trimmed = utf8::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return true; // valid comment or blank

    if (line.find('\t') != std::string::npos) {
        std::stringstream ss(trimmed);
        std::string type;
        if (!std::getline(ss, type, '\t')) return false;

        if (type == "WORD") {
            std::string word, freq_str;
            if (std::getline(ss, word, '\t')) {
                uint32_t freq = 100;
                if (std::getline(ss, freq_str, '\t') && !freq_str.empty()) {
                    try {
                        freq = static_cast<uint32_t>(std::stoul(freq_str));
                    } catch (...) {
                        freq = 100;
                    }
                }
                learn_word(word, freq);
                return true;
            }
        } else if (type == "SHORTCUT") {
            std::string shortcut, expansion;
            if (std::getline(ss, shortcut, '\t') && std::getline(ss, expansion, '\t')) {
                add_shortcut(shortcut, expansion);
                return true;
            }
        }
        return false;
    } else {
        // Fallback for legacy format: "word frequency"
        std::stringstream legacy_ss(trimmed);
        std::string w, f_str;
        if (legacy_ss >> w) {
            uint32_t f = 100;
            if (legacy_ss >> f_str) {
                try { f = static_cast<uint32_t>(std::stoul(f_str)); } catch (...) { return false; }
            }
            learn_word(w, f);
            return true;
        }
    }

    return false;
}

bool PersonalDictionary::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    std::vector<std::string> raw_lines;
    size_t invalid_lines = 0;

    while (std::getline(file, line)) {
        raw_lines.push_back(line);
        if (!parse_line(line)) {
            invalid_lines++;
        }
    }
    file.close();

    // Corruption Recovery: If corrupt lines were found, back up original and rewrite clean
    if (invalid_lines > 0) {
        std::string bak_path = filepath + ".bak";
        std::ofstream bak_file(bak_path);
        if (bak_file.is_open()) {
            for (const auto& l : raw_lines) {
                bak_file << l << "\n";
            }
            bak_file.close();
        }
        // Rewrite recovered state atomically
        save_atomic(filepath);
    }

    return true;
}

bool PersonalDictionary::save_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "# Amar Bangla Kotha - Personal Dictionary & User Shortcuts\n";
    file << "# Format: WORD<TAB>word<TAB>frequency OR SHORTCUT<TAB>shortcut<TAB>expansion\n";

    // Write words from user_trie
    auto words = user_trie.find_by_prefix("", max_user_words);
    for (const auto& w : words) {
        file << "WORD\t" << w.first << "\t" << w.second << "\n";
    }

    // Write shortcuts
    for (const auto& pair : shortcuts) {
        file << "SHORTCUT\t" << pair.second.shortcut << "\t" << pair.second.expansion << "\n";
    }

    return true;
}

bool PersonalDictionary::save_atomic(const std::string& filepath) const {
    std::string tmp_path = filepath + ".tmp";

    {
        std::ofstream file(tmp_path, std::ios::trunc);
        if (!file.is_open()) return false;

        file << "# Amar Bangla Kotha - Personal Dictionary & User Shortcuts\n";
        file << "# Format: WORD<TAB>word<TAB>frequency OR SHORTCUT<TAB>shortcut<TAB>expansion\n";

        auto words = user_trie.find_by_prefix("", max_user_words);
        for (const auto& w : words) {
            file << "WORD\t" << w.first << "\t" << w.second << "\n";
        }

        for (const auto& pair : shortcuts) {
            file << "SHORTCUT\t" << pair.second.shortcut << "\t" << pair.second.expansion << "\n";
        }
        file.flush();
    }

    // Atomic replacement via rename
    if (std::rename(tmp_path.c_str(), filepath.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }

    return true;
}

bool PersonalDictionary::export_to_tsv(const std::string& filepath) const {
    return save_to_file(filepath);
}

bool PersonalDictionary::import_from_tsv(const std::string& filepath) {
    return load_from_file(filepath);
}

void PersonalDictionary::clear() {
    user_trie.clear();
    shortcuts.clear();
}

bool PersonalDictionary::reset_storage(const std::string& filepath) {
    clear();
    if (!filepath.empty()) {
        std::remove(filepath.c_str());
        std::string bak_path = filepath + ".bak";
        std::remove(bak_path.c_str());
        std::string tmp_path = filepath + ".tmp";
        std::remove(tmp_path.c_str());
        return true;
    }
    return false;
}

size_t PersonalDictionary::memory_bytes() const {
    size_t bytes = sizeof(*this);
    bytes += user_trie.memory_bytes();
    for (const auto& pair : shortcuts) {
        bytes += pair.first.capacity();
        bytes += pair.second.shortcut.capacity();
        bytes += pair.second.expansion.capacity();
        bytes += sizeof(UserShortcut);
    }
    return bytes;
}

} // namespace abk
