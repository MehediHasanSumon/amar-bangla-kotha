#pragma once

#include "abk/types.h"
#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/token_classifier.h"
#include <string>
#include <vector>
#include <memory>

namespace abk {

class AmarBanglaKothaEngine {
public:
    explicit AmarBanglaKothaEngine(const EngineConfig& config = EngineConfig());
    ~AmarBanglaKothaEngine() = default;

    // Full processing pipeline: transliterate, suggest, prefix complete & autocorrect
    SuggestionResult process(const std::string& latin_input);

    // Fast transliteration only (no suggestion overhead)
    std::string transliterate_fast(const std::string& latin_input) const;

    // Word learning: add a word to dynamic memory and user dictionary
    void learn_word(const std::string& bangla_word, uint32_t frequency = 100);

    // Load external or user dictionary file
    bool load_dictionary_file(const std::string& filepath);
    bool load_user_dictionary_file(const std::string& filepath);

    // Workstream 1: Suggestion State Machine (Single source of truth)
    const SuggestionState& get_suggestion_state() const { return suggestion_state; }
    SuggestionState& get_suggestion_state() { return suggestion_state; }

    bool select_next_candidate();
    bool select_prev_candidate();
    bool select_candidate(int index);
    std::string get_selected_candidate_word() const;
    void set_suggestions_visible(bool visible);
    void reset_suggestion_state();

    // Workstream 2: Safety, Privacy & Secure Fields (Password auto-bypass)
    void set_secure_mode(bool secure);
    bool is_secure_mode() const { return secure_mode; }

    void set_clipboard_paste_mode(bool enable) { clipboard_paste_mode = enable; }
    bool is_clipboard_paste_mode() const { return clipboard_paste_mode; }

    // Workstream 3: Undo on Backspace after Autocorrect or Auto-Commit
    void record_commit(const std::string& raw_input, const std::string& committed_text, bool was_autocorrected = false);
    bool can_undo_commit() const { return last_commit.can_undo; }
    const UndoRecord& get_last_commit() const { return last_commit; }
    void clear_undo() { last_commit = UndoRecord(); }
    std::pair<std::string, std::string> undo_commit();

    // Workstream 3: Token Classification & Non-Bengali Bypass
    const TokenClassifier& get_token_classifier() const { return token_classifier; }
    bool should_bypass_token(const std::string& token) const;

    // Workstream 1: Per-event diagnostic logging
    void set_debug_logging(bool enable) { debug_logging = enable; }
    bool is_debug_logging() const { return debug_logging; }
    void log_event(const std::string& event_type, const std::string& details) const;

    // Engine accessors
    const EngineConfig& get_config() const { return config; }
    void set_config(const EngineConfig& cfg) { config = cfg; }

    const Dictionary& get_dictionary() const { return dictionary; }
    Dictionary& get_dictionary() { return dictionary; }

    const Transliterator& get_transliterator() const { return transliterator; }
    const Autocorrect& get_autocorrect() const { return autocorrect; }

private:
    EngineConfig config;
    Transliterator transliterator;
    Dictionary dictionary;
    Autocorrect autocorrect;
    SuggestionState suggestion_state;
    TokenClassifier token_classifier;
    UndoRecord last_commit;
    bool debug_logging = false;
    bool secure_mode = false;
    bool clipboard_paste_mode = false;

    void add_candidate_unique(std::vector<Candidate>& list, const Candidate& candidate);
};

} // namespace abk
