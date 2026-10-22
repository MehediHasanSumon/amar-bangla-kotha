#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace abk {

struct TrieChild {
    char32_t cp;
    uint32_t node_index;

    bool operator<(const TrieChild& other) const {
        return cp < other.cp;
    }
};

struct CompactTrieNode {
    std::vector<TrieChild> children; // Sorted by cp for fast binary search in L1 cache
    uint32_t frequency = 0;
    bool is_terminal = false;
    std::string word; // Stored only at terminal nodes
};

class Trie {
public:
    Trie();
    ~Trie() = default;

    // Insert a Bengali word with frequency
    void insert(const std::string& word, uint32_t frequency = 1);

    // Exact search: returns true if word is present
    bool search(const std::string& word, uint32_t* out_frequency = nullptr) const;

    // Get frequency of a word (0 if not found)
    uint32_t get_frequency(const std::string& word) const;

    // Find words starting with given prefix, sorted by frequency descending
    std::vector<std::pair<std::string, uint32_t>> find_by_prefix(const std::string& prefix, size_t limit = 10) const;

    // Total words in trie
    size_t size() const { return word_count; }

    // Total nodes in pool
    size_t node_count() const { return nodes.size(); }

    // Approximate memory footprint of the trie in bytes
    size_t memory_bytes() const;

    // Node accessors for fast fuzzy search (Autocorrect)
    const CompactTrieNode& get_node(uint32_t idx) const { return nodes[idx]; }
    uint32_t get_root_index() const { return 0; }
    const CompactTrieNode* get_root() const { return nodes.empty() ? nullptr : &nodes[0]; }

    // Fast child lookup via binary search
    uint32_t find_child(uint32_t node_idx, char32_t cp) const;

private:
    std::vector<CompactTrieNode> nodes; // Contiguous flat node pool (no heap pointer indirection)
    size_t word_count;

    void collect_prefix_bounded(uint32_t node_idx,
                                std::vector<std::pair<std::string, uint32_t>>& results,
                                size_t limit) const;
};

} // namespace abk
