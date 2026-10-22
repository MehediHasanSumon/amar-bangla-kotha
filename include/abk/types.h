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
    PHONETIC_ALTERNATIVE,  // Phonetic variant (e.g. sh/s, i/ee variations)
    SHORTCUT_EXPANSION,     // User defined shortcut / snippet (e.g. "amr" -> "আমার")
    NEXT_WORD_PREDICTION,   // Contextual next-word prediction from bigram model
    EMOJI_SUGGESTION        // Quality of life: Emoji and symbol suggestion
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

// Phase 1 / Workstream 1: Single source of truth for suggestion panel state
struct SuggestionState {
    std::vector<Candidate> items;
    int selected_index = 0;
    bool visible = false;
    std::string raw_input;
    std::string primary_transliteration;
};

// Workstream 3: Undo on Backspace Record
struct UndoRecord {
    std::string raw_input;
    std::string committed_text;
    bool was_autocorrected = false;
    bool can_undo = false;
};

struct EngineConfig {
    size_t max_suggestions = 8;
    bool enable_autocorrect = true;
    bool enable_prefix_completion = true;
    size_t max_edit_distance = 2;
    bool auto_bypass_password_fields = true;
    bool clipboard_paste_fallback = false;

    // Workstream 3: Token Classification & Per-App Profiles
    bool enable_english_bypass = true;
    bool enable_code_bypass = true;
    bool is_ide_or_terminal = false;

    // Workstream 4: Next-word prediction & Personalization
    bool enable_next_word_prediction = true;
    bool enable_shortcut_expansion = true;
    bool enable_dynamic_learning = true;
    bool enable_trigram = false;
    size_t max_bigrams = 50000;
    size_t max_user_words = 10000;
    std::string personal_dictionary_file_path = "";
    std::string bigram_model_file_path = "";

    std::string dictionary_file_path = "";
    std::string user_dictionary_file_path = "";
};

} // namespace abk
