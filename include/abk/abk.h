#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
    #ifdef ABK_EXPORTS
        #define ABK_API __declspec(dllexport)
    #else
        #define ABK_API __declspec(dllimport)
    #endif
#else
    #define ABK_API __attribute__((visibility("default")))
#endif

// Opaque engine pointer
typedef void* abk_engine_t;

// Candidate structure in C
typedef struct {
    char* word;
    double score;
    int candidate_type; // 0: Exact transliteration, 1: Dict exact, 2: Prefix, 3: Autocorrect, 4: Phonetic
    char* explanation;
} abk_candidate_t;

// Suggestion list in C
typedef struct {
    char* raw_input;
    char* primary_transliteration;
    size_t candidate_count;
    abk_candidate_t* candidates;
} abk_suggestion_list_t;

// Create and destroy engine instance
ABK_API abk_engine_t abk_engine_create(void);
ABK_API void abk_engine_destroy(abk_engine_t engine);

// Direct phonetic transliteration (fast)
// Returns number of bytes written, or -1 if buffer is too small
ABK_API int abk_transliterate_fast(abk_engine_t engine, const char* latin_input, char* out_buf, size_t buf_size);

// Get ranked suggestion list (Prefix completion, autocorrect, dictionary ranking)
ABK_API abk_suggestion_list_t* abk_get_suggestions(abk_engine_t engine, const char* latin_input);

// Free suggestion list memory
ABK_API void abk_free_suggestions(abk_suggestion_list_t* suggestions);

// Learn a new Bengali word into user dictionary
ABK_API int abk_learn_word(abk_engine_t engine, const char* bangla_word, uint32_t frequency);

// Load external dictionary file
ABK_API int abk_load_dictionary(abk_engine_t engine, const char* filepath);

// Phase 3: Unicode Normalization and Grapheme utilities for platform frontends
ABK_API int abk_normalize_nfc(const char* utf8_in, char* out_buf, size_t buf_size);
ABK_API int abk_pop_logical_unit(const char* utf8_in, char* out_buf, size_t buf_size);
ABK_API int abk_pop_grapheme_cluster(const char* utf8_in, char* out_buf, size_t buf_size);
ABK_API size_t abk_grapheme_count(const char* utf8_in);

// Workstream 1: Suggestion Panel Navigation & State Machine APIs (Single Source of Truth)
ABK_API int abk_suggestion_select_next(abk_engine_t engine);
ABK_API int abk_suggestion_select_prev(abk_engine_t engine);
ABK_API int abk_suggestion_select_index(abk_engine_t engine, int index);
ABK_API int abk_suggestion_get_selected_index(abk_engine_t engine);
ABK_API int abk_suggestion_get_selected_word(abk_engine_t engine, char* out_buf, size_t buf_size);
ABK_API const char* abk_suggestion_get_selected_word_ptr(abk_engine_t engine);
ABK_API int abk_suggestion_is_visible(abk_engine_t engine);
ABK_API void abk_suggestion_set_visible(abk_engine_t engine, int visible);
ABK_API void abk_suggestion_reset(abk_engine_t engine);
ABK_API void abk_set_debug_logging(abk_engine_t engine, int enable);
ABK_API void abk_log_event(abk_engine_t engine, const char* event_type, const char* details);

// Workstream 2: Security, Privacy & Secure Fields (Password auto-bypass & Clipboard fallback)
ABK_API void abk_engine_set_secure_mode(abk_engine_t engine, int is_secure);
ABK_API int abk_engine_is_secure_mode(abk_engine_t engine);
ABK_API void abk_engine_set_clipboard_mode(abk_engine_t engine, int enable);
ABK_API int abk_engine_is_clipboard_mode(abk_engine_t engine);

// Workstream 3: Undo on Backspace after Autocorrect/Auto-Commit & Token Bypass
ABK_API void abk_engine_record_commit(abk_engine_t engine, const char* raw_input, const char* committed_text, int was_autocorrected);
ABK_API int abk_engine_can_undo_commit(abk_engine_t engine);
ABK_API int abk_engine_get_undo_info(abk_engine_t engine, char* out_committed, size_t comm_size, char* out_raw, size_t raw_size);
ABK_API void abk_engine_clear_undo(abk_engine_t engine);
ABK_API int abk_token_should_bypass(abk_engine_t engine, const char* token);

// Get engine version
ABK_API const char* abk_get_version(void);

#ifdef __cplusplus
}
#endif
