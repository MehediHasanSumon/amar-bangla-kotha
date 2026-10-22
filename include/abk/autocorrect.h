#pragma once

#include "abk/trie.h"
#include <string>
#include <vector>
#include <cstdint>

namespace abk {

struct CorrectionMatch {
    std::string word;
    double edit_distance;
    uint32_t frequency;
};

class Autocorrect {
public:
    Autocorrect();
    ~Autocorrect() = default;

    // Direct Damerau-Levenshtein distance between two Unicode strings
    double compute_distance(const std::string& target, const std::string& candidate) const;

    // Search the Trie for all dictionary words within max_distance of target (bounded < 1ms)
    std::vector<CorrectionMatch> suggest_corrections(const Trie& trie,
                                                     const std::string& target,
                                                     size_t max_distance = 2,
                                                     size_t limit = 5) const;

    // Phonetic confusion cost
    double substitution_cost(char32_t a, char32_t b) const;
    bool is_phonetically_similar(char32_t a, char32_t b) const;

private:
    void search_trie_recursive(const Trie& trie,
                               uint32_t current_node_idx,
                               char32_t current_cp,
                               const std::vector<char32_t>& target_cps,
                               const std::vector<double>& previous_row,
                               std::vector<CorrectionMatch>& results,
                               double max_distance,
                               int& step_count) const;
};

} // namespace abk
