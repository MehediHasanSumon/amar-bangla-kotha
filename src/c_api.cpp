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

size_t abk_grapheme_count(const char* utf8_in) {
    if (!utf8_in) return 0;
    try {
        return abk::utf8::grapheme_count(utf8_in);
    } catch (...) {
        return std::strlen(utf8_in);
    }
}

const char* abk_get_version(void) {
    return "Amar Bangla Kotha v1.0.0 (C++ Core Engine)";
}
