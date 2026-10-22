#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <cstddef>

namespace abk {

struct BigramEntry {
    std::string word;
    uint32_t count = 0;

    BigramEntry() = default;
    BigramEntry(std::string w, uint32_t c) : word(std::move(w)), count(c) {}
};

class BigramModel {
public:
    BigramModel();
    explicit BigramModel(size_t max_entries);

    // Load bigram counts from disk (Format: word1\tword2\tcount per line)
    bool load_from_file(const std::string& filepath);
    bool save_to_file(const std::string& filepath) const;

    // Add or increment bigram observation
    void add_bigram(const std::string& w1, const std::string& w2, uint32_t count = 1);

    // Optional trigram observation
    void add_trigram(const std::string& w1, const std::string& w2, const std::string& w3, uint32_t count = 1);

    // Predict top-N next words given previous word w1
    std::vector<std::pair<std::string, double>> predict_next_words(const std::string& prev_word, size_t limit = 5) const;

    // Predict top-N next words given w1 and w2 (trigram with bigram fallback)
    std::vector<std::pair<std::string, double>> predict_trigram_next_words(const std::string& w1, const std::string& w2, size_t limit = 5) const;

    // Get smoothed conditional probability P(w2 | w1)
    double get_bigram_prob(const std::string& w1, const std::string& w2) const;

    // Get smoothed trigram probability P(w3 | w1, w2)
    double get_trigram_prob(const std::string& w1, const std::string& w2, const std::string& w3) const;

    // Check if bigram model has any context for w1
    bool has_context(const std::string& w1) const;

    // Clear all entries and reset stats
    void clear();

    // Capacity and size management
    size_t entry_count() const { return total_bigram_pairs; }
    size_t memory_bytes() const;
    void set_max_entries(size_t max_entries) { max_capacity = max_entries; }
    size_t get_max_entries() const { return max_capacity; }

    void set_enable_trigram(bool enable) { enable_trigram = enable; }
    bool is_trigram_enabled() const { return enable_trigram; }

private:
    size_t max_capacity = 50000;
    size_t total_bigram_pairs = 0;
    size_t total_trigram_triplets = 0;
    bool enable_trigram = false;

    // Bigram storage: w1 -> list of (w2, count)
    std::unordered_map<std::string, std::vector<BigramEntry>> bigrams;
    // Context marginal totals: w1 -> total count
    std::unordered_map<std::string, uint64_t> context_totals;

    // Trigram storage: "w1\tw2" -> list of (w3, count)
    std::unordered_map<std::string, std::vector<BigramEntry>> trigrams;
    std::unordered_map<std::string, uint64_t> trigram_context_totals;

    void prune_if_needed();
};

} // namespace abk
