#pragma once

#include "abk/trie.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace abk {

struct CorrectionMatch {
    std::string word;
    double edit_distance;
    uint32_t frequency;
};

struct AutocorrectEntry {
    std::string typed;
    std::string replacement;
    std::string category;
    bool is_user_defined = false;

    AutocorrectEntry() = default;
    AutocorrectEntry(std::string t, std::string r, std::string cat = "custom", bool user_def = false)
        : typed(std::move(t)), replacement(std::move(r)), category(std::move(cat)), is_user_defined(user_def) {}
};

// Section 7: User-editable replacement table ("typed text -> replacement")
class AutocorrectTable {
public:
    AutocorrectTable();
    explicit AutocorrectTable(const std::string& defaults_filepath);

    // Load default clean-room table from TSV
    bool load_defaults(const std::string& filepath = "data/autocorrect_default.tsv");
    void init_builtin_defaults();
    void reset_to_defaults();

    // Global enable / disable
    void set_enabled(bool enable) { enabled_ = enable; }
    bool is_enabled() const { return enabled_; }

    // Editable CRUD operations: add, change, delete, clear
    bool add_or_update(const std::string& typed_text, const std::string& replacement, const std::string& category = "user");
    bool remove(const std::string& typed_text);
    void clear();

    // Import & Export user-editable TSV
    bool import_tsv(const std::string& filepath);
    bool export_tsv(const std::string& filepath) const;

    // Lookup respecting global toggle and explicit user control keys (uppercase, `, ,,)
    bool contains(const std::string& typed_text) const;
    std::string lookup(const std::string& typed_text) const;
    std::vector<AutocorrectEntry> get_all_entries() const;
    size_t size() const { return entries_.size(); }

    // True if input contains explicit user control keys: uppercase A-Z, backtick `, or double-comma ,,
    static bool has_explicit_control_keys(const std::string& input);

private:
    bool enabled_ = true;
    std::string loaded_defaults_path_;
    std::unordered_map<std::string, AutocorrectEntry> entries_;
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
