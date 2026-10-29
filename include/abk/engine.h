#pragma once

#include "abk/types.h"
#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/token_classifier.h"
#include "abk/bigram_model.h"
#include "abk/personal_dictionary.h"
#include "abk/domain_packs.h"
#include "abk/bangladesh_helpers.h"
#include "abk/quality_of_life.h"
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
    bool browse_tab();
    bool close_suggestion_window();
    std::string commit_selected_candidate(bool append_space = false);
    std::string get_selected_candidate_word() const;
    void set_suggestions_visible(bool visible);
    void reset_suggestion_state();

    // Section 8: Suggestion Mode, Window Pinning & "Remember My Choice"
    void set_suggestion_mode(SuggestionMode mode);
    SuggestionMode get_suggestion_mode() const { return config.suggestion_mode; }
    void set_tab_browses_candidates(bool enable) { config.tab_browses_candidates = enable; }
    bool is_tab_browses_candidates() const { return config.tab_browses_candidates; }
    void set_window_pinned(bool pinned, int x = 0, int y = 0);
    bool is_window_pinned() const { return suggestion_state.pinned; }
    std::pair<int, int> get_pinned_position() const { return {suggestion_state.pinned_x, suggestion_state.pinned_y}; }

    void remember_candidate_choice(const std::string& raw_input, const std::string& chosen_word);
    void reset_remembered_choices();

    // Section 7: Editable Autocorrect Replacement Table API
    const AutocorrectTable& get_autocorrect_table() const { return autocorrect_table; }
    AutocorrectTable& get_autocorrect_table() { return autocorrect_table; }
    void set_autocorrect_enabled(bool enable);
    bool is_autocorrect_enabled() const { return config.enable_autocorrect && autocorrect_table.is_enabled(); }
    bool add_autocorrect_entry(const std::string& typed_text, const std::string& replacement);
    bool remove_autocorrect_entry(const std::string& typed_text);
    bool import_autocorrect_table(const std::string& filepath);
    bool export_autocorrect_table(const std::string& filepath) const;

    // Workstream D: Transliterator Punctuation & Digit Settings Forwarding
    void set_bangla_digits(bool enable) { transliterator.set_bangla_digits(enable); }
    bool is_bangla_digits_enabled() const { return transliterator.is_bangla_digits_enabled(); }
    void set_nukta_ja(bool enable) { transliterator.set_nukta_ja(enable); }
    bool is_nukta_ja_enabled() const { return transliterator.is_nukta_ja_enabled(); }
    void set_taka_sign(bool enable) { transliterator.set_taka_sign(enable); }
    bool is_taka_sign_enabled() const { return transliterator.is_taka_sign_enabled(); }
    void set_pipe_mode(PipeMode mode) { transliterator.set_pipe_mode(mode); }
    PipeMode get_pipe_mode() const { return transliterator.get_pipe_mode(); }

    // Workstream 2: Safety, Privacy & Secure Fields (Password auto-bypass)
    void set_secure_mode(bool secure);
    bool is_secure_mode() const { return secure_mode; }

    void set_clipboard_paste_mode(bool enable) { clipboard_paste_mode = enable; }
    bool is_clipboard_paste_mode() const { return clipboard_paste_mode; }

    // Workstream 3 & Section 7: Undo on Backspace after Autocorrect or Auto-Commit
    void record_commit(const std::string& raw_input, const std::string& committed_text, bool was_autocorrected = false);
    bool can_undo_commit() const { return last_commit.can_undo; }
    const UndoRecord& get_last_commit() const { return last_commit; }
    void clear_undo() { last_commit = UndoRecord(); suppressed_autocorrect_input.clear(); }
    std::pair<std::string, std::string> undo_commit();
    std::string revert_autocorrect_on_backspace();

    // Workstream 3: Token Classification & Non-Bengali Bypass
    const TokenClassifier& get_token_classifier() const { return token_classifier; }
    bool should_bypass_token(const std::string& token) const;

    // Workstream 4: Next-Word Prediction & Personalization
    bool load_bigram_file(const std::string& filepath);
    bool load_personal_dictionary_file(const std::string& filepath);
    bool save_personal_dictionary_file(const std::string& filepath) const;

    std::vector<Candidate> get_next_word_predictions(size_t limit = 5) const;
    std::vector<Candidate> get_next_word_predictions_for(const std::string& prev_word, size_t limit = 5) const;

    void add_personal_shortcut(const std::string& shortcut, const std::string& expansion);
    bool remove_personal_shortcut(const std::string& shortcut);
    bool has_personal_shortcut(const std::string& shortcut) const;
    std::string get_personal_shortcut_expansion(const std::string& shortcut) const;

    void clear_personal_data();
    bool export_personal_data(const std::string& filepath) const;
    bool import_personal_data(const std::string& filepath);

    const BigramModel& get_bigram_model() const { return bigram_model; }
    BigramModel& get_bigram_model() { return bigram_model; }

    const PersonalDictionary& get_personal_dictionary() const { return personal_dictionary; }
    PersonalDictionary& get_personal_dictionary() { return personal_dictionary; }

    const std::string& get_last_committed_word() const { return last_committed_word; }
    void set_last_committed_word(const std::string& word) { last_committed_word = word; }

    // Workstream 7: Domain Dictionary Packs
    bool load_domain_pack(DomainPackType type, const std::string& filepath);
    void set_domain_pack_enabled(DomainPackType type, bool enabled);
    bool is_domain_pack_enabled(DomainPackType type) const;
    const DomainPackManager& get_domain_pack_manager() const { return domain_pack_manager; }
    DomainPackManager& get_domain_pack_manager() { return domain_pack_manager; }

    // Workstream 8: Quality of Life & Correction Logger
    const CorrectionLogger& get_correction_logger() const { return correction_logger; }
    CorrectionLogger& get_correction_logger() { return correction_logger; }
    void set_correction_logging(bool enable) { correction_logger.set_enabled(enable); }
    bool is_correction_logging_enabled() const { return correction_logger.is_enabled(); }

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
    AutocorrectTable autocorrect_table;
    SuggestionState suggestion_state;
    TokenClassifier token_classifier;
    UndoRecord last_commit;
    std::string suppressed_autocorrect_input;
    BigramModel bigram_model;
    PersonalDictionary personal_dictionary;
    DomainPackManager domain_pack_manager;
    CorrectionLogger correction_logger;
    std::string last_committed_word;
    std::string second_last_committed_word;
    bool debug_logging = false;
    bool secure_mode = false;
    bool clipboard_paste_mode = false;

    void add_candidate_unique(std::vector<Candidate>& list, const Candidate& candidate);
};

} // namespace abk
