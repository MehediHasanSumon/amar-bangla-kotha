#pragma once

#include "abk/trie.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

namespace abk {

struct UserShortcut {
    std::string shortcut;
    std::string expansion;
    uint32_t use_count = 0;

    UserShortcut() = default;
    UserShortcut(std::string s, std::string e, uint32_t c = 0)
        : shortcut(std::move(s)), expansion(std::move(e)), use_count(c) {}
};

class PersonalDictionary {
public:
    PersonalDictionary();
    explicit PersonalDictionary(size_t max_words);

    // Persistence with atomic writes and corruption recovery
    bool load_from_file(const std::string& filepath);
    bool save_to_file(const std::string& filepath) const;
    bool save_atomic(const std::string& filepath) const;

    // Dynamic word learning (proper nouns, personal vocabulary)
    void learn_word(const std::string& word, uint32_t frequency = 100);
    bool contains_word(const std::string& word) const;
    uint32_t get_word_frequency(const std::string& word) const;
    std::vector<std::pair<std::string, uint32_t>> find_words_by_prefix(const std::string& prefix, size_t limit = 10) const;

    // Shortcut snippet expansion
    void add_shortcut(const std::string& shortcut, const std::string& expansion);
    bool remove_shortcut(const std::string& shortcut);
    bool has_shortcut(const std::string& shortcut) const;
    std::string get_expansion(const std::string& shortcut) const;
    std::vector<UserShortcut> get_all_shortcuts() const;

    // Export & Import
    bool export_to_tsv(const std::string& filepath) const;
    bool import_from_tsv(const std::string& filepath);

    // Reset and cleanup
    void clear();
    bool reset_storage(const std::string& filepath);

    // Capacity & memory
    size_t word_count() const { return user_trie.size(); }
    size_t shortcut_count() const { return shortcuts.size(); }
    size_t memory_bytes() const;
    void set_max_words(size_t max_words) { max_user_words = max_words; }
    size_t get_max_words() const { return max_user_words; }

    const Trie& get_user_trie() const { return user_trie; }

private:
    size_t max_user_words = 10000;
    Trie user_trie;
    std::unordered_map<std::string, UserShortcut> shortcuts;

    // Line parser handling both WORD records and SHORTCUT records
    bool parse_line(const std::string& line);
};

} // namespace abk
