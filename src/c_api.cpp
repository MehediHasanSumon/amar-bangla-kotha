#include "abk/abk.h"
#include "abk/engine.h"
#include "abk/utf8_util.h"
#include <cstring>
#include <cstdlib>
#include <exception>

static char* duplicate_str(const std::string& str) {
    char* copy = static_cast<char*>(malloc(str.length() + 1));
    if (copy) {
        std::memcpy(copy, str.c_str(), str.length() + 1);
    }
    return copy;
}

abk_engine_t abk_engine_create(void) {
    try {
        auto* engine = new abk::AmarBanglaKothaEngine();
        return static_cast<abk_engine_t>(engine);
    } catch (...) {
        return nullptr;
    }
}

void abk_engine_destroy(abk_engine_t handle) {
    try {
        if (handle) {
            auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
            delete engine;
        }
    } catch (...) {
        // Suppress any destruction exceptions
    }
}

int abk_transliterate_fast(abk_engine_t handle, const char* latin_input, char* out_buf, size_t buf_size) {
    if (!latin_input || !out_buf || buf_size == 0) return -1;

    try {
        if (!handle) {
            // Fallback passthrough
            size_t in_len = std::strlen(latin_input);
            if (in_len + 1 > buf_size) return -1;
            std::memcpy(out_buf, latin_input, in_len + 1);
            return static_cast<int>(in_len);
        }

        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        std::string res = engine->transliterate_fast(latin_input);

        if (res.length() + 1 > buf_size) {
            return -1; // Buffer too small
        }

        std::memcpy(out_buf, res.c_str(), res.length() + 1);
        return static_cast<int>(res.length());
    } catch (...) {
        // Defensive exception fallback: passthrough input
        size_t in_len = std::strlen(latin_input);
        if (in_len + 1 <= buf_size) {
            std::memcpy(out_buf, latin_input, in_len + 1);
            return static_cast<int>(in_len);
        }
        return -1;
    }
}

abk_suggestion_list_t* abk_get_suggestions(abk_engine_t handle, const char* latin_input) {
    if (!latin_input) return nullptr;

    try {
        if (!handle) {
            // Fallback: create single candidate with raw latin input
            auto* fallback_list = static_cast<abk_suggestion_list_t*>(malloc(sizeof(abk_suggestion_list_t)));
            if (!fallback_list) return nullptr;
            fallback_list->raw_input = duplicate_str(latin_input);
            fallback_list->primary_transliteration = duplicate_str(latin_input);
            fallback_list->candidate_count = 1;
            fallback_list->candidates = static_cast<abk_candidate_t*>(malloc(sizeof(abk_candidate_t)));
            if (fallback_list->candidates) {
                fallback_list->candidates[0].word = duplicate_str(latin_input);
                fallback_list->candidates[0].score = 1.0;
                fallback_list->candidates[0].candidate_type = 0;
                fallback_list->candidates[0].explanation = duplicate_str("Passthrough");
            }
            return fallback_list;
        }

        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        abk::SuggestionResult cpp_res = engine->process(latin_input);

        auto* list = static_cast<abk_suggestion_list_t*>(malloc(sizeof(abk_suggestion_list_t)));
        if (!list) return nullptr;

        list->raw_input = duplicate_str(cpp_res.raw_input);
        list->primary_transliteration = duplicate_str(cpp_res.primary_transliteration);
        list->candidate_count = cpp_res.candidates.size();

        if (list->candidate_count > 0) {
            list->candidates = static_cast<abk_candidate_t*>(malloc(sizeof(abk_candidate_t) * list->candidate_count));
            if (!list->candidates) {
                list->candidate_count = 0;
            } else {
                for (size_t i = 0; i < list->candidate_count; i++) {
                    list->candidates[i].word = duplicate_str(cpp_res.candidates[i].word);
                    list->candidates[i].score = cpp_res.candidates[i].score;
                    list->candidates[i].candidate_type = static_cast<int>(cpp_res.candidates[i].type);
                    list->candidates[i].explanation = duplicate_str(cpp_res.candidates[i].explanation);
                }
            }
        } else {
            list->candidates = nullptr;
        }

        return list;
    } catch (...) {
        // Defensive fallback: Return passthrough list
        auto* err_list = static_cast<abk_suggestion_list_t*>(malloc(sizeof(abk_suggestion_list_t)));
        if (!err_list) return nullptr;
        err_list->raw_input = duplicate_str(latin_input);
        err_list->primary_transliteration = duplicate_str(latin_input);
        err_list->candidate_count = 0;
        err_list->candidates = nullptr;
        return err_list;
    }
}

void abk_free_suggestions(abk_suggestion_list_t* list) {
    if (!list) return;

    try {
        if (list->raw_input) free(list->raw_input);
        if (list->primary_transliteration) free(list->primary_transliteration);

        if (list->candidates) {
            for (size_t i = 0; i < list->candidate_count; i++) {
                if (list->candidates[i].word) free(list->candidates[i].word);
                if (list->candidates[i].explanation) free(list->candidates[i].explanation);
            }
            free(list->candidates);
        }

        free(list);
    } catch (...) {
        // Suppress any freeing exceptions
    }
}

int abk_learn_word(abk_engine_t handle, const char* bangla_word, uint32_t frequency) {
    if (!handle || !bangla_word) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->learn_word(bangla_word, frequency);
        return 1;
    } catch (...) {
        return 0;
    }
}

int abk_load_dictionary(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->load_dictionary_file(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_normalize_nfc(const char* utf8_in, char* out_buf, size_t buf_size) {
    if (!utf8_in || !out_buf || buf_size == 0) return -1;
    try {
        std::string res = abk::utf8::normalize_nfc(utf8_in);
        if (res.length() + 1 > buf_size) return -1;
        std::memcpy(out_buf, res.c_str(), res.length() + 1);
        return static_cast<int>(res.length());
    } catch (...) {
        size_t len = std::strlen(utf8_in);
        if (len + 1 > buf_size) return -1;
        std::memcpy(out_buf, utf8_in, len + 1);
        return static_cast<int>(len);
    }
}

int abk_pop_logical_unit(const char* utf8_in, char* out_buf, size_t buf_size) {
    if (!utf8_in || !out_buf || buf_size == 0) return -1;
    try {
        std::string res = abk::utf8::pop_logical_unit(utf8_in);
        if (res.length() + 1 > buf_size) return -1;
        std::memcpy(out_buf, res.c_str(), res.length() + 1);
        return static_cast<int>(res.length());
    } catch (...) {
        size_t len = std::strlen(utf8_in);
        if (len > 0) len--; // Basic byte pop fallback
        if (len + 1 > buf_size) return -1;
        std::memcpy(out_buf, utf8_in, len);
        out_buf[len] = '\0';
        return static_cast<int>(len);
    }
}

int abk_pop_grapheme_cluster(const char* utf8_in, char* out_buf, size_t buf_size) {
    if (!utf8_in || !out_buf || buf_size == 0) return -1;
    try {
        std::string res = abk::utf8::pop_grapheme_cluster(utf8_in);
        if (res.length() + 1 > buf_size) return -1;
        std::memcpy(out_buf, res.c_str(), res.length() + 1);
        return static_cast<int>(res.length());
    } catch (...) {
        size_t len = std::strlen(utf8_in);
        if (len > 0) len--;
        if (len + 1 > buf_size) return -1;
        std::memcpy(out_buf, utf8_in, len);
        out_buf[len] = '\0';
        return static_cast<int>(len);
    }
}

size_t abk_grapheme_count(const char* utf8_in) {
    if (!utf8_in) return 0;
    try {
        return abk::utf8::grapheme_count(utf8_in);
    } catch (...) {
        return std::strlen(utf8_in);
    }
}

int abk_suggestion_select_next(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->select_next_candidate() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_suggestion_select_prev(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->select_prev_candidate() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_suggestion_select_index(abk_engine_t handle, int index) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->select_candidate(index) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_suggestion_get_selected_index(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->get_suggestion_state().selected_index;
    } catch (...) {
        return 0;
    }
}

int abk_suggestion_get_selected_word(abk_engine_t handle, char* out_buf, size_t buf_size) {
    if (!handle || !out_buf || buf_size == 0) return -1;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        std::string w = engine->get_selected_candidate_word();
        if (w.length() + 1 > buf_size) return -1;
        std::memcpy(out_buf, w.c_str(), w.length() + 1);
        return static_cast<int>(w.length());
    } catch (...) {
        return -1;
    }
}

const char* abk_suggestion_get_selected_word_ptr(abk_engine_t handle) {
    if (!handle) return "";
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        const auto& state = engine->get_suggestion_state();
        if (state.selected_index >= 0 && state.selected_index < static_cast<int>(state.items.size())) {
            return state.items[state.selected_index].word.c_str();
        }
        return state.primary_transliteration.c_str();
    } catch (...) {
        return "";
    }
}

int abk_suggestion_is_visible(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->get_suggestion_state().visible ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

void abk_suggestion_set_visible(abk_engine_t handle, int visible) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->set_suggestions_visible(visible != 0);
    } catch (...) {}
}

void abk_suggestion_reset(abk_engine_t handle) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->reset_suggestion_state();
    } catch (...) {}
}

void abk_set_debug_logging(abk_engine_t handle, int enable) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->set_debug_logging(enable != 0);
    } catch (...) {}
}

void abk_log_event(abk_engine_t handle, const char* event_type, const char* details) {
    if (!handle || !event_type) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->log_event(event_type, details ? details : "");
    } catch (...) {}
}

void abk_engine_set_secure_mode(abk_engine_t handle, int is_secure) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->set_secure_mode(is_secure != 0);
    } catch (...) {}
}

int abk_engine_is_secure_mode(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->is_secure_mode() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

void abk_engine_set_clipboard_mode(abk_engine_t handle, int enable) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->set_clipboard_paste_mode(enable != 0);
    } catch (...) {}
}

int abk_engine_is_clipboard_mode(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->is_clipboard_paste_mode() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

void abk_engine_record_commit(abk_engine_t handle, const char* raw_input, const char* committed_text, int was_autocorrected) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->record_commit(raw_input ? raw_input : "", committed_text ? committed_text : "", was_autocorrected != 0);
    } catch (...) {}
}

int abk_engine_can_undo_commit(abk_engine_t handle) {
    if (!handle) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->can_undo_commit() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_engine_get_undo_info(abk_engine_t handle, char* out_committed, size_t comm_size, char* out_raw, size_t raw_size) {
    if (!handle || !out_committed || comm_size == 0 || !out_raw || raw_size == 0) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        auto undo = engine->undo_commit();
        if (undo.first.empty() && undo.second.empty()) return 0;
        if (undo.first.length() + 1 > comm_size || undo.second.length() + 1 > raw_size) return 0;
        std::memcpy(out_committed, undo.first.c_str(), undo.first.length() + 1);
        std::memcpy(out_raw, undo.second.c_str(), undo.second.length() + 1);
        return 1;
    } catch (...) {
        return 0;
    }
}

void abk_engine_clear_undo(abk_engine_t handle) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->clear_undo();
    } catch (...) {}
}

int abk_token_should_bypass(abk_engine_t handle, const char* token) {
    if (!handle || !token) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->should_bypass_token(token) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_engine_load_bigrams(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->load_bigram_file(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_engine_load_personal_dict(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->load_personal_dictionary_file(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_engine_save_personal_dict(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->save_personal_dictionary_file(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

void abk_engine_add_shortcut(abk_engine_t handle, const char* shortcut, const char* expansion) {
    if (!handle || !shortcut || !expansion) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->add_personal_shortcut(shortcut, expansion);
    } catch (...) {}
}

int abk_engine_remove_shortcut(abk_engine_t handle, const char* shortcut) {
    if (!handle || !shortcut) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->remove_personal_shortcut(shortcut) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

void abk_engine_clear_personal_data(abk_engine_t handle) {
    if (!handle) return;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        engine->clear_personal_data();
    } catch (...) {}
}

int abk_engine_export_personal_data(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->export_personal_data(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int abk_engine_import_personal_data(abk_engine_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        return engine->import_personal_data(filepath) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

abk_suggestion_list_t* abk_engine_get_next_word_predictions(abk_engine_t handle, const char* prev_word) {
    if (!handle) return nullptr;
    try {
        auto* engine = static_cast<abk::AmarBanglaKothaEngine*>(handle);
        std::vector<abk::Candidate> preds;
        if (prev_word && std::strlen(prev_word) > 0) {
            preds = engine->get_next_word_predictions_for(prev_word, engine->get_config().max_suggestions);
        } else {
            preds = engine->get_next_word_predictions(engine->get_config().max_suggestions);
        }

        auto* res = new abk_suggestion_list_t();
        res->raw_input = nullptr;
        res->primary_transliteration = preds.empty() ? nullptr : duplicate_str(preds[0].word);
        res->candidate_count = preds.size();
        res->candidates = new abk_candidate_t[preds.size()];

        for (size_t i = 0; i < preds.size(); ++i) {
            res->candidates[i].word = duplicate_str(preds[i].word);
            res->candidates[i].score = preds[i].score;
            res->candidates[i].candidate_type = static_cast<int>(preds[i].type);
            res->candidates[i].explanation = duplicate_str(preds[i].explanation);
        }
        return res;
    } catch (...) {
        return nullptr;
    }
}

const char* abk_get_version(void) {
    return "Amar Bangla Kotha v1.0.0 (C++ Core Engine)";
}
