#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace abk {

enum class CandidateType {
    EXACT_TRANSLITERATION,   // Exact rule-based transliteration
    DICTIONARY_EXACT,       // Direct match in the Bengali dictionary
    PREFIX_COMPLETION,      // Next letters / prefix completion from dictionary
    AUTOCORRECT,            // Autocorrect replacement table or typo-corrected suggestion
    PHONETIC_ALTERNATIVE,   // Phonetic variant (e.g. sh/s, i/ee variations)
    SHORTCUT_EXPANSION,     // User defined shortcut / snippet (e.g. "amr" -> "আমার")
    NEXT_WORD_PREDICTION,   // Contextual next-word prediction from bigram model
    EMOJI_SUGGESTION,       // Quality of life: Emoji and symbol suggestion
    REMEMBERED_CHOICE       // Section 8: User's remembered non-default candidate choice
};

// Section 8: Suggestion / preview behavior modes
enum class SuggestionMode {
    CHARACTER_FIRST = 0,    // Default: Autocorrect -> Classic phonetic -> Dictionary/Skeleton
    DICTIONARY_FIRST = 1,   // Autocorrect -> Dictionary/Skeleton -> Classic phonetic
    CLASSIC_PHONETIC = 2    // Classic phonetic only with no suggestion window
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
    bool pinned = false;
    int pinned_x = 0;
    int pinned_y = 0;
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

    // Section 8: Suggestion / Preview Modes, Navigation & Window Placement
    SuggestionMode suggestion_mode = SuggestionMode::CHARACTER_FIRST;
    bool tab_browses_candidates = true;
    bool enable_number_selection = true;
    bool enable_remember_choice = true;
    size_t max_remembered_choices = 2048;
    bool window_pinned = false;
    int window_pinned_x = 0;
    int window_pinned_y = 0;

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
