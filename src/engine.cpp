#include "abk/engine.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace abk {

AmarBanglaKothaEngine::AmarBanglaKothaEngine(const EngineConfig& cfg)
    : config(cfg) {
    if (!config.dictionary_file_path.empty()) {
        dictionary.load_from_file(config.dictionary_file_path);
    }
    if (!config.user_dictionary_file_path.empty()) {
        dictionary.load_user_dictionary(config.user_dictionary_file_path);
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
    if (trimmed_input.empty()) {
        reset_suggestion_state();
        return result;
    }

    // 1. Beam-search transliteration powered by lattice
    auto beam_candidates = transliterator.transliterate_beam(trimmed_input, &dictionary, config.max_suggestions * 2);

    std::string primary = "";
    if (!beam_candidates.empty()) {
        primary = beam_candidates[0].first;
    } else {
        primary = transliterator.transliterate(trimmed_input, &dictionary);
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
            // Log-linear combination: Transliteration confidence + LM unigram + Rank discount
            double final_score = 8500.0 * trans_conf + 500.0 * lm_log_score - (i * 35.0);
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::DICTIONARY_EXACT, "Dictionary match"));
        } else {
            double final_score = 7500.0 * trans_conf - (i * 45.0);
            add_candidate_unique(result.candidates, Candidate(cand_word, final_score, CandidateType::EXACT_TRANSLITERATION, "Phonetic transliteration"));
        }
    }

    // 3. Phonetic Alternatives
    std::vector<std::string> variants = transliterator.get_phonetic_variants(trimmed_input, 4);
    for (const auto& var : variants) {
        if (var != primary) {
            if (dictionary.contains(var)) {
                uint32_t freq = dictionary.get_frequency(var);
                double lm_log_score = std::log10(static_cast<double>(freq + 1));
                double score = 5200.0 + 400.0 * lm_log_score;
                add_candidate_unique(result.candidates, Candidate(var, score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation (in dictionary)"));
            } else {
                double score = 2600.0;
                add_candidate_unique(result.candidates, Candidate(var, score, CandidateType::PHONETIC_ALTERNATIVE, "Phonetic variation"));
            }
        }
    }

    // 4. Prefix Completion from Dictionary (Future word completions)
    if (config.enable_prefix_completion && !primary.empty()) {
        auto prefix_matches = dictionary.get_prefix_matches(primary, config.max_suggestions * 2);
        size_t prim_len = utf8::codepoint_length(primary);

        for (const auto& match : prefix_matches) {
            if (match.first == primary) continue;
            size_t word_len = utf8::codepoint_length(match.first);
            double len_diff = (word_len >= prim_len) ? static_cast<double>(word_len - prim_len) : 0.0;
            double lm_log_score = std::log10(static_cast<double>(match.second + 1));

            // Log-linear: Base + LM score - length distance penalty
            double score = 3200.0 + 350.0 * lm_log_score - (len_diff * 80.0);
            add_candidate_unique(result.candidates, Candidate(match.first, score, CandidateType::PREFIX_COMPLETION, "Prefix completion"));
        }
    }

    // 5. Autocorrect / Typo Correction (Bounded fast fuzzy trie search)
    if (config.enable_autocorrect && utf8::codepoint_length(primary) >= 2) {
        auto corrections = autocorrect.suggest_corrections(
            dictionary.get_system_trie(),
            primary,
            config.max_edit_distance,
            config.max_suggestions
        );

        for (const auto& match : corrections) {
            if (match.word == primary) continue;
            double lm_log_score = std::log10(static_cast<double>(match.frequency + 1));
            // Log-linear: Base + LM score - Edit distance penalty
            double score = 3000.0 + 400.0 * lm_log_score - (match.edit_distance * 700.0);
            add_candidate_unique(result.candidates, Candidate(match.word, score, CandidateType::AUTOCORRECT, "Autocorrect"));
        }
    }

    // 6. Final sort descending by score
    std::sort(result.candidates.begin(), result.candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    if (result.candidates.size() > config.max_suggestions) {
        result.candidates.resize(config.max_suggestions);
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
    suggestion_state.visible = true;
    if (debug_logging) {
        log_event("CLICK_RECEIVED", "Selected index: " + std::to_string(index) +
                  " ('" + suggestion_state.items[index].word + "')");
    }
    return true;
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

bool AmarBanglaKothaEngine::load_dictionary_file(const std::string& filepath) {
    return dictionary.load_from_file(filepath);
}

bool AmarBanglaKothaEngine::load_user_dictionary_file(const std::string& filepath) {
    return dictionary.load_user_dictionary(filepath);
}

} // namespace abk
