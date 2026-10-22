#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace abk {

enum class CandidateType {
    EXACT_TRANSLITERATION,   // Exact rule-based transliteration
    DICTIONARY_EXACT,       // Direct match in the Bengali dictionary
    PREFIX_COMPLETION,      // Next letters / prefix completion from dictionary
    AUTOCORRECT,            // Typo-corrected suggestion via edit distance
    PHONETIC_ALTERNATIVE   // Phonetic variant (e.g. sh/s, i/ee variations)
};

struct Candidate {
    std::string word;
    double score;
    CandidateType type;
    std::string explanation;

    Candidate() : score(0.0), type(CandidateType::EXACT_TRANSLITERATION) {}
    Candidate(std::string w, double s, CandidateType t, std::string expl = "")
        : word(std::move(w)), score(s), type(t), explanation(std::move(expl)) {}
};

struct SuggestionResult {
    std::string raw_input;
    std::string primary_transliteration;
    std::vector<Candidate> candidates;
};

struct EngineConfig {
    size_t max_suggestions = 8;
    bool enable_autocorrect = true;
    bool enable_prefix_completion = true;
    size_t max_edit_distance = 2;
    std::string dictionary_file_path = "";
    std::string user_dictionary_file_path = "";
};

} // namespace abk
