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
ABK_API size_t abk_grapheme_count(const char* utf8_in);

// Get engine version
ABK_API const char* abk_get_version(void);

#ifdef __cplusplus
}
#endif
