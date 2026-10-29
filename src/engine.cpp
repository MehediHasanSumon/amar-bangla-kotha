#include "abk/engine.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace abk {

AmarBanglaKothaEngine::AmarBanglaKothaEngine(const EngineConfig& cfg)
    : config(cfg), bigram_model(cfg.max_bigrams), personal_dictionary(cfg.max_user_words) {
    if (!config.dictionary_file_path.empty()) {
        dictionary.load_from_file(config.dictionary_file_path);
    }
    if (!config.user_dictionary_file_path.empty()) {
        dictionary.load_user_dictionary(config.user_dictionary_file_path);
    }
    if (!config.bigram_model_file_path.empty()) {
        bigram_model.load_from_file(config.bigram_model_file_path);
    } else {
        bigram_model.load_from_file("data/bangla_bigrams.tsv");
    }
    if (!config.personal_dictionary_file_path.empty()) {
        personal_dictionary.load_from_file(config.personal_dictionary_file_path);
    }
    personal_dictionary.set_max_remembered_choices(config.max_remembered_choices);
    autocorrect_table.set_enabled(config.enable_autocorrect);
    suggestion_state.pinned = config.window_pinned;
    suggestion_state.pinned_x = config.window_pinned_x;
    suggestion_state.pinned_y = config.window_pinned_y;
    bigram_model.set_enable_trigram(config.enable_trigram);

    // Workstream 7: Initialize built-in Bangladeshi shortcuts (Currency 'tk' -> '৳', greetings)
    for (const auto& sc : BangladeshHelpers::get_builtin_shortcuts()) {
        if (!personal_dictionary.has_shortcut(sc.first)) {
            personal_dictionary.add_shortcut(sc.first, sc.second);
        }
    }

    // Load domain dictionary packs from standard paths
    const std::vector<std::pair<DomainPackType, std::vector<std::string>>> default_packs = {
        {DomainPackType::BANGLADESH_PLACES, {"data/domains/bangladesh_places.tsv", "/usr/share/amar-bangla-kotha/domains/bangladesh_places.tsv"}},
        {DomainPackType::ISLAMIC_TERMS, {"data/domains/islamic_terms.tsv", "/usr/share/amar-bangla-kotha/domains/islamic_terms.tsv"}},
        {DomainPackType::LEGAL_MEDICAL_BANKING, {"data/domains/legal_medical_banking.tsv", "/usr/share/amar-bangla-kotha/domains/legal_medical_banking.tsv"}}
    };
    for (const auto& dp : default_packs) {
        for (const auto& p : dp.second) {
            if (domain_pack_manager.load_pack(dp.first, p)) {
                break;
            }
        }
    }
}

void AmarBanglaKothaEngine::add_candidate_unique(std::vector<Candidate>& list, const Candidate& candidate) {
    if (candidate.word.empty()) return;

    Candidate norm_cand = candidate;
    norm_cand.word = utf8::normalize_nfc(candidate.word);

    for (auto& existing : list) {
        if (existing.word == norm_cand.word) {
            if (norm_cand.score > existing.score) {
                existing = norm_cand;
            }
            return;
        }
    }
    list.push_back(norm_cand);
}

std::string AmarBanglaKothaEngine::transliterate_fast(const std::string& latin_input) const {
    if (secure_mode) return utf8::normalize_nfc(latin_input);
    if (should_bypass_token(latin_input)) return utf8::normalize_nfc(latin_input);
    if (config.suggestion_mode == SuggestionMode::CLASSIC_PHONETIC) {
        return transliterator.transliterate(latin_input, nullptr);
    }
    std::string trimmed = utf8::trim(latin_input);
    if (config.enable_autocorrect && autocorrect_table.is_enabled() && trimmed != suppressed_autocorrect_input) {
        std::string ac = autocorrect_table.lookup(trimmed);
        if (!ac.empty()) return ac;
    }
    return transliterator.transliterate(latin_input, &dictionary);
}

SuggestionResult AmarBanglaKothaEngine::process(const std::string& raw_input) {
    SuggestionResult result;
    result.raw_input = raw_input;

    // Security & Privacy: Auto-bypass password and sensitive fields
    if (secure_mode) {
        reset_suggestion_state();
        result.primary_transliteration = utf8::normalize_nfc(raw_input);
        return result;
    }

    std::string trimmed_input = utf8::trim(raw_input);
    if (!suppressed_autocorrect_input.empty() && trimmed_input != suppressed_autocorrect_input) {
        suppressed_autocorrect_input.clear();
    }

    if (trimmed_input.empty()) {
        reset_suggestion_state();
        if (config.suggestion_mode != SuggestionMode::CLASSIC_PHONETIC &&
            config.enable_next_word_prediction && !last_committed_word.empty()) {
            auto predictions = get_next_word_predictions(config.max_suggestions);
            if (!predictions.empty()) {
                result.candidates = predictions;
                result.primary_transliteration = predictions[0].word;
                suggestion_state.items = predictions;
                suggestion_state.selected_index = 0;
                suggestion_state.visible = true;
                suggestion_state.raw_input = "";
                suggestion_state.primary_transliteration = predictions[0].word;
            }
        }
        return result;
    }

    // Workstream 3 & D: Check token classification (URLs, emails, file paths, code, English)
    if (should_bypass_token(trimmed_input)) {
        std::string safe_text = utf8::normalize_nfc(trimmed_input);
        suggestion_state.items.clear();
        suggestion_state.selected_index = 0;
        suggestion_state.visible = false;
        suggestion_state.raw_input = raw_input;
        suggestion_state.primary_transliteration = safe_text;
        result.primary_transliteration = safe_text;
        return result;
    }

    const bool has_explicit_ctrl = AutocorrectTable::has_explicit_control_keys(trimmed_input);
    const bool allow_autocorrect_for_token =
        config.enable_autocorrect && autocorrect_table.is_enabled() &&
        !has_explicit_ctrl && (trimmed_input != suppressed_autocorrect_input);

    // Section 8 Mode 3: Classic phonetic with no suggestion window
    if (config.suggestion_mode == SuggestionMode::CLASSIC_PHONETIC) {
        std::string out = transliterator.transliterate(trimmed_input, nullptr);
        result.primary_transliteration = out;
        result.candidates = {Candidate(out, 12000.0, CandidateType::EXACT_TRANSLITERATION, "Classic phonetic")};
        suggestion_state.items = result.candidates;
        suggestion_state.selected_index = 0;
        suggestion_state.visible = false; // No suggestion window in classic mode
        suggestion_state.raw_input = raw_input;
        suggestion_state.primary_transliteration = out;
        return result;
    }

    // Section 8: "Remember my choice" — rank previously selected non-default candidate first
    if (config.enable_remember_choice && trimmed_input != suppressed_autocorrect_input &&
        personal_dictionary.has_remembered_choice(trimmed_input)) {
        std::string rem = personal_dictionary.get_remembered_choice(trimmed_input);
        if (!rem.empty()) {
            add_candidate_unique(result.candidates,
                Candidate(rem, 22000.0, CandidateType::REMEMBERED_CHOICE, "Remembered user choice"));
        }
    }

    // Section 7: Editable Autocorrect Replacement Table (never overrides explicit control keys)
    if (allow_autocorrect_for_token) {
        std::string ac_repl = autocorrect_table.lookup(trimmed_input);
        if (!ac_repl.empty()) {
            add_candidate_unique(result.candidates,
                Candidate(ac_repl, 16500.0, CandidateType::AUTOCORRECT, "Autocorrect table"));
        }
    }

    // Workstream 4: Personal Shortcut / Snippet Expansion (e.g. "amr" -> "আমার")
    if (config.enable_shortcut_expansion && !has_explicit_ctrl &&
        trimmed_input != suppressed_autocorrect_input &&
        personal_dictionary.has_shortcut(trimmed_input)) {
        std::string expansion = personal_dictionary.get_expansion(trimmed_input);
        if (!expansion.empty()) {
            Candidate sc_cand(expansion, 15000.0, CandidateType::SHORTCUT_EXPANSION, "Personal shortcut");
            add_candidate_unique(result.candidates, sc_cand);
        }
    }

    // Classic strict character-first phonetic result
    std::string classic_phonetic = transliterator.transliterate(trimmed_input, nullptr);
    if (!classic_phonetic.empty() && config.suggestion_mode == SuggestionMode::CHARACTER_FIRST) {
        CandidateType ctype = dictionary.contains(classic_phonetic)
            ? CandidateType::DICTIONARY_EXACT
            : CandidateType::EXACT_TRANSLITERATION;
        add_candidate_unique(result.candidates,
            Candidate(classic_phonetic, 11800.0, ctype, "Classic phonetic"));
    }

    // Workstream 7: Domain Dictionary Pack matching
    for (const auto& d_entry : domain_pack_manager.get_active_entries()) {
        if (d_entry.key == trimmed_input) {
            add_candidate_unique(result.candidates, Candidate(d_entry.word, 11000.0, CandidateType::DICTIONARY_EXACT, "Domain Pack: " + d_entry.category));
        }
    }

    // Workstream 8: Emoji & Symbol Suggestions
    auto emojis = QualityOfLife::get_emoji_suggestions(trimmed_input);
    for (const auto& em : emojis) {
        add_candidate_unique(result.candidates, em);
    }

    // 1. Beam-search transliteration powered by lattice
    auto beam_candidates = transliterator.transliterate_beam(trimmed_input, &dictionary, config.max_suggestions * 2);

    std::string primary = "";
    if (!beam_candidates.empty()) {
        primary = beam_candidates[0].first;
    } else {
        primary = classic_phonetic;
    }
    result.primary_transliteration = primary;

    // 2. Principled Log-Linear Scoring for Transliteration Candidates
    double max_beam_score = 1.0;
    for (const auto& bc : beam_candidates) {
        if (bc.second > max_beam_score) max_beam_score = bc.second;
    }

    for (size_t i = 0; i < beam_candidates.size(); ++i) {
        const std::string& cand_word = beam_candidates[i].first;
        double beam_score = beam_candidates[i].second;
        double trans_conf = (max_beam_score > 0.0) ? std::max(0.2, beam_score / max_beam_score) : 1.0;

        bool in_dict = dictionary.contains(cand_word);
        if (in_dict) {
            uint32_t freq = dictionary.get_frequency(cand_word);
            double lm_log_score = std::log10(static_cast<double>(freq + 1));
            double final_score = 8500.0 * trans_conf + 500.0 * lm_log_score - (i * 35.0);
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::DICTIONARY_EXACT, "Dictionary match"));
        } else if (i == 0) {
            double final_score = 7500.0 * trans_conf;
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::EXACT_TRANSLITERATION, "Phonetic transliteration"));
        } else if (i <= 2) {
            double final_score = 6180.0 * trans_conf - (i * 25.0);
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation"));
        } else {
            double final_score = 2600.0 * trans_conf - (i * 15.0);
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation"));
        }
    }

    // 2b. Section 8: Skeleton / Collapsed Phonetic Key Dictionary Lookup
    // Resolves ত/ট/ৎ, শ/ষ/স, ই/ঈ, উ/ঊ, ন/ণ, র/ড়/ঢ়, and inherent o vs ো inconsistencies
    if (!classic_phonetic.empty()) {
        auto skel_matches = dictionary.lookup_by_skeleton(classic_phonetic, config.max_suggestions);
        for (size_t s_idx = 0; s_idx < skel_matches.size(); ++s_idx) {
            const auto& sm = skel_matches[s_idx];
            double lm_log_score = std::log10(static_cast<double>(sm.second + 1));
            double skel_score = (!has_explicit_ctrl && config.suggestion_mode == SuggestionMode::DICTIONARY_FIRST)
                ? (8200.0 + 500.0 * lm_log_score - (s_idx * 30.0))
                : (7800.0 + 450.0 * lm_log_score - (s_idx * 30.0));
            if (has_explicit_ctrl) {
                // Never allow skeleton collapse to outrank an explicit control key (#1)
                skel_score = std::min(skel_score, 7100.0 - (s_idx * 25.0));
            }
            add_candidate_unique(result.candidates,
                Candidate(sm.first, skel_score, CandidateType::DICTIONARY_EXACT, "Skeleton dictionary match"));
        }
    }

    // 3. Phonetic Alternatives
    std::vector<std::string> variants = transliterator.get_phonetic_variants(trimmed_input, 4);
    for (size_t v_idx = 0; v_idx < variants.size(); ++v_idx) {
        const auto& var = variants[v_idx];
        if (var != primary) {
            if (dictionary.contains(var)) {
                uint32_t freq = dictionary.get_frequency(var);
                double lm_log_score = std::log10(static_cast<double>(freq + 1));
                double score = 5200.0 + 400.0 * lm_log_score;
                add_candidate_unique(result.candidates, Candidate(var, score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation (in dictionary)"));
            } else {
                double score = 4900.0 - (v_idx * 80.0);
                add_candidate_unique(result.candidates, Candidate(var, score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation"));
            }
        }
    }

    // 4. Prefix Completion from Dictionary (Future word completions)
    const std::string& pfx_seed = (!classic_phonetic.empty() && config.suggestion_mode == SuggestionMode::CHARACTER_FIRST)
        ? classic_phonetic : primary;
    if (config.enable_prefix_completion && !pfx_seed.empty()) {
        auto prefix_matches = dictionary.get_prefix_matches(pfx_seed, config.max_suggestions * 2);
        size_t prim_len = utf8::codepoint_length(pfx_seed);

        for (const auto& match : prefix_matches) {
            if (match.first == pfx_seed) continue;
            size_t word_len = utf8::codepoint_length(match.first);
            double len_diff = (word_len >= prim_len) ? static_cast<double>(word_len - prim_len) : 0.0;
            double lm_log_score = std::log10(static_cast<double>(match.second + 1));

            double score = 3200.0 + 350.0 * lm_log_score - (len_diff * 80.0);
            add_candidate_unique(result.candidates, Candidate(match.first, score, CandidateType::PREFIX_COMPLETION, "Prefix completion"));
        }
    }

    // 5. Autocorrect / Typo Correction (Bounded fast fuzzy trie search — never overrides explicit control keys)
    if (allow_autocorrect_for_token && utf8::codepoint_length(primary) >= 2) {
        auto corrections = autocorrect.suggest_corrections(
            dictionary.get_system_trie(),
            primary,
            config.max_edit_distance,
            config.max_suggestions
        );

        for (const auto& match : corrections) {
            if (match.word == primary) continue;
            double lm_log_score = std::log10(static_cast<double>(match.frequency + 1));
            double score = 3000.0 + 400.0 * lm_log_score - (match.edit_distance * 700.0);
            add_candidate_unique(result.candidates, Candidate(match.word, score, CandidateType::AUTOCORRECT, "Fuzzy autocorrect"));
        }
    }

    // Workstream 4: Contextual Bigram Boosting and Personal Familiarity
    if (config.enable_next_word_prediction && !last_committed_word.empty()) {
        for (auto& cand : result.candidates) {
            double bi_prob = 0.0;
            if (config.enable_trigram && !second_last_committed_word.empty()) {
                bi_prob = bigram_model.get_trigram_prob(second_last_committed_word, last_committed_word, cand.word);
            } else {
                bi_prob = bigram_model.get_bigram_prob(last_committed_word, cand.word);
            }
            if (bi_prob > 0.0 && !has_explicit_ctrl) {
                cand.score += 2400.0 * std::log10(bi_prob * 1000.0 + 1.0);
            }
            if (personal_dictionary.contains_word(cand.word) && !has_explicit_ctrl) {
                cand.score += 800.0;
            }
        }
    } else if (!has_explicit_ctrl) {
        for (auto& cand : result.candidates) {
            if (personal_dictionary.contains_word(cand.word)) {
                cand.score += 800.0;
            }
        }
    }

    // 6. Final sort descending by score
    std::sort(result.candidates.begin(), result.candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    if (result.candidates.size() > config.max_suggestions) {
        result.candidates.resize(config.max_suggestions);
    }

    if (!result.candidates.empty()) {
        result.primary_transliteration = result.candidates[0].word;
    }

    // Workstream 1: Selection stability across keystrokes
    std::string prev_selected_word = "";
    if (suggestion_state.selected_index >= 0 &&
        suggestion_state.selected_index < static_cast<int>(suggestion_state.items.size())) {
        prev_selected_word = suggestion_state.items[suggestion_state.selected_index].word;
    }

    // Update internal single source of truth SuggestionState
    suggestion_state.items = result.candidates;
    suggestion_state.raw_input = raw_input;
    suggestion_state.primary_transliteration = result.primary_transliteration;
    suggestion_state.visible = !result.candidates.empty();

    int new_selected_index = 0;
    if (!prev_selected_word.empty()) {
        for (size_t i = 0; i < suggestion_state.items.size(); ++i) {
            if (suggestion_state.items[i].word == prev_selected_word) {
                new_selected_index = static_cast<int>(i);
                break;
            }
        }
    }
    suggestion_state.selected_index = new_selected_index;

    if (debug_logging) {
        log_event("SUGGESTIONS_UPDATED",
                  "Input: '" + raw_input + "', Total Candidates: " + std::to_string(result.candidates.size()) +
                  ", Selected Index: " + std::to_string(new_selected_index) +
                  " ('" + (suggestion_state.items.empty() ? "" : suggestion_state.items[new_selected_index].word) + "')");
    }

    return result;
}

bool AmarBanglaKothaEngine::select_next_candidate() {
    if (suggestion_state.items.empty()) {
        if (debug_logging) log_event("KEY_IGNORED", "Arrow/Tab next with empty candidate list");
        return false;
    }
    suggestion_state.selected_index = (suggestion_state.selected_index + 1) % static_cast<int>(suggestion_state.items.size());
    suggestion_state.visible = true;
    if (debug_logging) {
        log_event("KEY_CONSUMED", "Arrow/Tab next -> selected index: " + std::to_string(suggestion_state.selected_index) +
                  " ('" + suggestion_state.items[suggestion_state.selected_index].word + "')");
    }
    return true;
}

bool AmarBanglaKothaEngine::select_prev_candidate() {
    if (suggestion_state.items.empty()) {
        if (debug_logging) log_event("KEY_IGNORED", "Arrow/Tab prev with empty candidate list");
        return false;
    }
    int sz = static_cast<int>(suggestion_state.items.size());
    suggestion_state.selected_index = (suggestion_state.selected_index - 1 + sz) % sz;
    suggestion_state.visible = true;
    if (debug_logging) {
        log_event("KEY_CONSUMED", "Arrow/Tab prev -> selected index: " + std::to_string(suggestion_state.selected_index) +
                  " ('" + suggestion_state.items[suggestion_state.selected_index].word + "')");
    }
    return true;
}

bool AmarBanglaKothaEngine::select_candidate(int index) {
    if (suggestion_state.items.empty()) {
        suggestion_state.selected_index = 0;
        return false;
    }
    if (index < 0) {
        index = 0;
    } else if (index >= static_cast<int>(suggestion_state.items.size())) {
        index = static_cast<int>(suggestion_state.items.size() - 1);
    }
    suggestion_state.selected_index = index;
    suggestion_state.visible = (config.suggestion_mode != SuggestionMode::CLASSIC_PHONETIC);
    if (debug_logging) {
        log_event("CLICK_RECEIVED", "Selected index: " + std::to_string(index) +
                  " ('" + suggestion_state.items[index].word + "')");
    }
    return true;
}

bool AmarBanglaKothaEngine::browse_tab() {
    if (!config.tab_browses_candidates || !suggestion_state.visible || suggestion_state.items.empty()) {
        if (debug_logging) log_event("KEY_IGNORED", "Tab ignored (disabled or candidate window closed)");
        return false;
    }
    return select_next_candidate();
}

bool AmarBanglaKothaEngine::close_suggestion_window() {
    if (!suggestion_state.visible) {
        return false;
    }
    suggestion_state.visible = false;
    if (debug_logging) log_event("WINDOW_CLOSED", "Suggestion window closed via Esc");
    return true;
}

std::string AmarBanglaKothaEngine::commit_selected_candidate(bool append_space) {
    std::string raw_in = utf8::trim(suggestion_state.raw_input);
    std::string chosen = get_selected_candidate_word();
    int sel_idx = suggestion_state.selected_index;
    bool was_ac = false;

    if (sel_idx >= 0 && sel_idx < static_cast<int>(suggestion_state.items.size())) {
        auto ctype = suggestion_state.items[sel_idx].type;
        was_ac = (ctype == CandidateType::AUTOCORRECT || ctype == CandidateType::SHORTCUT_EXPANSION);
    }

    // Section 8: "Remember my choice" when user picks a non-default candidate (sel_idx > 0)
    if (config.enable_remember_choice && !secure_mode && sel_idx > 0 && !raw_in.empty() && !chosen.empty()) {
        personal_dictionary.remember_choice(raw_in, chosen);
    }

    suppressed_autocorrect_input.clear();
    std::string committed = utf8::normalize_nfc(chosen) + (append_space ? " " : "");
    if (!raw_in.empty() && !committed.empty()) {
        record_commit(raw_in, committed, was_ac);
    }
    reset_suggestion_state();
    return committed;
}

void AmarBanglaKothaEngine::set_suggestion_mode(SuggestionMode mode) {
    config.suggestion_mode = mode;
    if (mode == SuggestionMode::CLASSIC_PHONETIC) {
        suggestion_state.visible = false;
    }
}

void AmarBanglaKothaEngine::set_window_pinned(bool pinned, int x, int y) {
    config.window_pinned = pinned;
    config.window_pinned_x = x;
    config.window_pinned_y = y;
    suggestion_state.pinned = pinned;
    suggestion_state.pinned_x = x;
    suggestion_state.pinned_y = y;
}

void AmarBanglaKothaEngine::remember_candidate_choice(const std::string& raw_input, const std::string& chosen_word) {
    if (secure_mode || !config.enable_remember_choice) return;
    personal_dictionary.remember_choice(raw_input, chosen_word);
    if (!config.personal_dictionary_file_path.empty()) {
        personal_dictionary.save_atomic(config.personal_dictionary_file_path);
    }
}

void AmarBanglaKothaEngine::reset_remembered_choices() {
    personal_dictionary.clear_remembered_choices();
    if (!config.personal_dictionary_file_path.empty()) {
        personal_dictionary.save_atomic(config.personal_dictionary_file_path);
    }
}

void AmarBanglaKothaEngine::set_autocorrect_enabled(bool enable) {
    config.enable_autocorrect = enable;
    autocorrect_table.set_enabled(enable);
}

bool AmarBanglaKothaEngine::add_autocorrect_entry(const std::string& typed_text, const std::string& replacement) {
    return autocorrect_table.add_or_update(typed_text, replacement, "user");
}

bool AmarBanglaKothaEngine::remove_autocorrect_entry(const std::string& typed_text) {
    return autocorrect_table.remove(typed_text);
}

bool AmarBanglaKothaEngine::import_autocorrect_table(const std::string& filepath) {
    return autocorrect_table.import_tsv(filepath);
}

bool AmarBanglaKothaEngine::export_autocorrect_table(const std::string& filepath) const {
    return autocorrect_table.export_tsv(filepath);
}

std::string AmarBanglaKothaEngine::get_selected_candidate_word() const {
    if (suggestion_state.selected_index >= 0 &&
        suggestion_state.selected_index < static_cast<int>(suggestion_state.items.size())) {
        return suggestion_state.items[suggestion_state.selected_index].word;
    }
    if (!suggestion_state.primary_transliteration.empty()) {
        return suggestion_state.primary_transliteration;
    }
    return suggestion_state.raw_input;
}

void AmarBanglaKothaEngine::set_suggestions_visible(bool visible) {
    suggestion_state.visible = visible;
    if (debug_logging) {
        log_event("VISIBILITY_CHANGED", visible ? "true" : "false");
    }
}

void AmarBanglaKothaEngine::reset_suggestion_state() {
    suggestion_state.items.clear();
    suggestion_state.selected_index = 0;
    suggestion_state.visible = false;
    suggestion_state.raw_input.clear();
    suggestion_state.primary_transliteration.clear();
    if (debug_logging) {
        log_event("STATE_RESET", "Candidate list cleared, selection reset to 0, visible=false");
    }
}

void AmarBanglaKothaEngine::set_secure_mode(bool secure) {
    secure_mode = secure;
    if (secure) {
        reset_suggestion_state();
    }
}

void AmarBanglaKothaEngine::log_event(const std::string& event_type, const std::string& details) const {
    if (secure_mode) return; // Strict privacy: never log sensitive/password input
    std::cerr << "[ABK_EVENT_LOG] " << event_type << " | " << details << std::endl;
}

void AmarBanglaKothaEngine::learn_word(const std::string& bangla_word, uint32_t frequency) {
    if (secure_mode) return; // Strict privacy: never learn words typed in secure fields
    if (bangla_word.empty()) return;

    if (!config.user_dictionary_file_path.empty()) {
        dictionary.save_user_word(config.user_dictionary_file_path, bangla_word, frequency);
    } else {
        dictionary.add_word(bangla_word, frequency);
    }
}

void AmarBanglaKothaEngine::record_commit(const std::string& raw_input, const std::string& committed_text, bool was_autocorrected) {
    if (secure_mode) {
        clear_undo();
        last_committed_word.clear();
        second_last_committed_word.clear();
        return;
    }
    last_commit.raw_input = raw_input;
    last_commit.committed_text = committed_text;
    last_commit.was_autocorrected = was_autocorrected;
    last_commit.can_undo = !raw_input.empty() && !committed_text.empty();

    std::string clean_word = utf8::normalize_nfc(utf8::trim(committed_text));
    if (!clean_word.empty()) {
        // Workstream 8: Opt-in Correction Logger
        if (was_autocorrected || raw_input != clean_word) {
            correction_logger.record_correction(raw_input, clean_word, secure_mode);
        }

        if (config.enable_dynamic_learning) {
            learn_word(clean_word, 200);
            personal_dictionary.learn_word(clean_word, 200);
        }

        if (config.enable_next_word_prediction && !last_committed_word.empty()) {
            bigram_model.add_bigram(last_committed_word, clean_word, 1);
            if (config.enable_trigram && !second_last_committed_word.empty()) {
                bigram_model.add_trigram(second_last_committed_word, last_committed_word, clean_word, 1);
            }
        }

        second_last_committed_word = last_committed_word;
        last_committed_word = clean_word;

        if (!config.personal_dictionary_file_path.empty()) {
            personal_dictionary.save_atomic(config.personal_dictionary_file_path);
        }
    }
}

std::pair<std::string, std::string> AmarBanglaKothaEngine::undo_commit() {
    if (!last_commit.can_undo) {
        return {"", ""};
    }
    if (last_commit.was_autocorrected) {
        suppressed_autocorrect_input = utf8::trim(last_commit.raw_input);
    }
    std::pair<std::string, std::string> res = { last_commit.committed_text, last_commit.raw_input };
    last_commit.can_undo = false;
    return res;
}

std::string AmarBanglaKothaEngine::revert_autocorrect_on_backspace() {
    if (!last_commit.can_undo || !last_commit.was_autocorrected) {
        return "";
    }
    auto res = undo_commit();
    return res.second; // Exact text the user typed
}

bool AmarBanglaKothaEngine::should_bypass_token(const std::string& token) const {
    if (token.empty()) return false;

    auto classification = token_classifier.classify(token);
    if (!classification.should_bypass_transliteration) {
        return false;
    }

    switch (classification.type) {
        case TokenType::URL:
        case TokenType::EMAIL:
        case TokenType::FILE_PATH:
            return true;

        case TokenType::CODE_TOKEN:
            return config.enable_code_bypass || config.is_ide_or_terminal;

        case TokenType::ENGLISH_WORD:
            if (config.enable_autocorrect && autocorrect_table.is_enabled() &&
                token != suppressed_autocorrect_input && autocorrect_table.contains(token)) {
                return false;
            }
            if (config.enable_shortcut_expansion && personal_dictionary.has_shortcut(token)) {
                return false;
            }
            return config.enable_english_bypass;

        default:
            return false;
    }
}

bool AmarBanglaKothaEngine::load_dictionary_file(const std::string& filepath) {
    return dictionary.load_from_file(filepath);
}

bool AmarBanglaKothaEngine::load_user_dictionary_file(const std::string& filepath) {
    return dictionary.load_user_dictionary(filepath);
}

bool AmarBanglaKothaEngine::load_bigram_file(const std::string& filepath) {
    return bigram_model.load_from_file(filepath);
}

bool AmarBanglaKothaEngine::load_personal_dictionary_file(const std::string& filepath) {
    return personal_dictionary.load_from_file(filepath);
}

bool AmarBanglaKothaEngine::save_personal_dictionary_file(const std::string& filepath) const {
    return personal_dictionary.save_atomic(filepath);
}

std::vector<Candidate> AmarBanglaKothaEngine::get_next_word_predictions(size_t limit) const {
    if (last_committed_word.empty()) return {};
    return get_next_word_predictions_for(last_committed_word, limit);
}

std::vector<Candidate> AmarBanglaKothaEngine::get_next_word_predictions_for(const std::string& prev_word, size_t limit) const {
    std::vector<Candidate> candidates;
    if (prev_word.empty()) return candidates;

    std::vector<std::pair<std::string, double>> predictions;
    if (config.enable_trigram && !second_last_committed_word.empty() && prev_word == last_committed_word) {
        predictions = bigram_model.predict_trigram_next_words(second_last_committed_word, last_committed_word, limit);
    } else {
        predictions = bigram_model.predict_next_words(prev_word, limit);
    }

    for (const auto& p : predictions) {
        double score = 4000.0 + p.second * 1000.0;
        candidates.emplace_back(p.first, score, CandidateType::NEXT_WORD_PREDICTION, "Next-word prediction");
    }
    return candidates;
}

void AmarBanglaKothaEngine::add_personal_shortcut(const std::string& shortcut, const std::string& expansion) {
    personal_dictionary.add_shortcut(shortcut, expansion);
}

bool AmarBanglaKothaEngine::remove_personal_shortcut(const std::string& shortcut) {
    return personal_dictionary.remove_shortcut(shortcut);
}

bool AmarBanglaKothaEngine::has_personal_shortcut(const std::string& shortcut) const {
    return personal_dictionary.has_shortcut(shortcut);
}

std::string AmarBanglaKothaEngine::get_personal_shortcut_expansion(const std::string& shortcut) const {
    return personal_dictionary.get_expansion(shortcut);
}

void AmarBanglaKothaEngine::clear_personal_data() {
    personal_dictionary.clear();
    bigram_model.clear();
    last_committed_word.clear();
    second_last_committed_word.clear();
    if (!config.personal_dictionary_file_path.empty()) {
        personal_dictionary.reset_storage(config.personal_dictionary_file_path);
    }
}

bool AmarBanglaKothaEngine::export_personal_data(const std::string& filepath) const {
    return personal_dictionary.export_to_tsv(filepath);
}

bool AmarBanglaKothaEngine::import_personal_data(const std::string& filepath) {
    return personal_dictionary.import_from_tsv(filepath);
}

bool AmarBanglaKothaEngine::load_domain_pack(DomainPackType type, const std::string& filepath) {
    return domain_pack_manager.load_pack(type, filepath);
}

void AmarBanglaKothaEngine::set_domain_pack_enabled(DomainPackType type, bool enabled) {
    domain_pack_manager.set_pack_enabled(type, enabled);
}

bool AmarBanglaKothaEngine::is_domain_pack_enabled(DomainPackType type) const {
    return domain_pack_manager.is_pack_enabled(type);
}

} // namespace abk
