#pragma once

#include "abk/trie.h"
#include <string>
#include <vector>
#include <cstdint>

namespace abk {

class Dictionary {
public:
    Dictionary();
    ~Dictionary() = default;

    // Load built-in default vocabulary
    void load_builtin_words();

    // Load words from a text file (Format: "word <optional_frequency>" per line)
    bool load_from_file(const std::string& filepath);

    // Load or append user learned words into custom user dictionary layer
    bool load_user_dictionary(const std::string& filepath);
    bool save_user_word(const std::string& filepath, const std::string& word, uint32_t frequency = 100);

    // Add single word to user dictionary memory
    void add_word(const std::string& word, uint32_t frequency);

    // Queries: transparently search user layer first, then system layer
    bool contains(const std::string& word) const;
    uint32_t get_frequency(const std::string& word) const;
    std::vector<std::pair<std::string, uint32_t>> get_prefix_matches(const std::string& prefix, size_t limit = 10) const;

    // Total words across system and user dictionaries
    size_t word_count() const { return system_trie.size() + user_trie.size(); }
    size_t memory_bytes() const { return system_trie.memory_bytes() + user_trie.memory_bytes(); }

    const Trie& get_system_trie() const { return system_trie; }
    const Trie& get_user_trie() const { return user_trie; }
    const Trie& get_trie() const { return system_trie; } // Backward compatibility

private:
    Trie system_trie; // Preloaded foundational and file vocabulary
    Trie user_trie;   // Custom user dictionary overlay

    void parse_line_and_insert(const std::string& line, Trie& target_trie);
};

} // namespace abk
