#include "abk/abk.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cassert>
#include <cstring>

// Strict UTF-8 Validator
static bool is_valid_utf8(const char* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c <= 0x7F) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(data[i + 1]);
            if ((c2 & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false; // Overlong
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(data[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[i + 2]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            if (c == 0xE0 && c2 < 0xA0) return false; // Overlong
            if (c == 0xED && c2 >= 0xA0) return false; // Surrogate
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(data[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(data[i + 2]);
            unsigned char c4 = static_cast<unsigned char>(data[i + 3]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) return false;
            if (c == 0xF0 && c2 < 0x90) return false; // Overlong
            if (c == 0xF4 && c2 >= 0x90) return false; // Out of range (> U+10FFFF)
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << " Amar Bangla Kotha - Fuzz Testing Suite (ASan / UBSan)   \n";
    std::cout << "=========================================================\n";

    abk_engine_t engine = abk_engine_create();
    assert(engine != nullptr);

    // Load dictionary
    abk_load_dictionary(engine, "data/bangla_wordlist.txt");

    std::mt19937 rng(1337); // Deterministic seed for reproducible fuzzing

    // Character pools
    const std::string valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789^:` .,!?;:-()";
    std::vector<uint8_t> corrupted_bytes = {
        0x80, 0xBF, 0xC0, 0xC1, 0xF5, 0xFF, 0xFE, 0x00, 0x7F, 0xE0, 0xED, 0xA0
    };

    std::uniform_int_distribution<int> action_dist(0, 6);
    std::uniform_int_distribution<int> char_dist(0, valid_chars.size() - 1);
    std::uniform_int_distribution<int> corrupt_dist(0, corrupted_bytes.size() - 1);
    std::uniform_int_distribution<int> length_dist(1, 100);

    const int TOTAL_ITERATIONS = 100000;
    std::string current_buffer;
    char out_buf[1024];

    std::cout << "Running " << TOTAL_ITERATIONS << " fuzz iterations...\n";

    for (int iter = 0; iter < TOTAL_ITERATIONS; ++iter) {
        int action = action_dist(rng);

        if (action == 0 || action == 1) {
            // Append random valid phonetic character
            current_buffer += valid_chars[char_dist(rng)];
            if (current_buffer.length() > 60) {
                current_buffer.clear();
            }
        } else if (action == 2) {
            // Append corrupted byte sequence (stress invalid UTF-8 handling)
            current_buffer += static_cast<char>(corrupted_bytes[corrupt_dist(rng)]);
            if (current_buffer.length() > 60) {
                current_buffer.clear();
            }
        } else if (action == 3) {
            // Backspace / pop logical unit
            if (!current_buffer.empty()) {
                current_buffer.pop_back();
            }
        } else if (action == 4) {
            // Learn a word
            if (!current_buffer.empty()) {
                abk_learn_word(engine, current_buffer.c_str(), 100);
            }
        } else if (action == 5) {
            // Workstream 2: Toggle secure mode & clipboard paste fallback mode
            abk_engine_set_secure_mode(engine, rng() % 2);
            abk_engine_set_clipboard_mode(engine, rng() % 2);
        } else {
            // Clear buffer
            current_buffer.clear();
        }

        // Test 1: Suggestions query
        abk_suggestion_list_t* res = abk_get_suggestions(engine, current_buffer.c_str());
        if (res) {
            // Exercise candidate selection navigation
            abk_suggestion_select_next(engine);
            abk_suggestion_select_prev(engine);
            abk_suggestion_select_index(engine, rng() % 10);
            char sel_word[256];
            abk_suggestion_get_selected_word(engine, sel_word, sizeof(sel_word));
            abk_suggestion_get_selected_word_ptr(engine);

            if (res->primary_transliteration) {
                size_t p_len = std::strlen(res->primary_transliteration);
                if (!is_valid_utf8(res->primary_transliteration, p_len)) {
                    std::cerr << "FAILED at iter " << iter << "\n";
                    std::cerr << "Input buffer (hex): ";
                    for (unsigned char c : current_buffer) {
                        std::cerr << std::hex << (int)c << " ";
                    }
                    std::cerr << "\nOutput primary (hex): ";
                    for (size_t k = 0; k < p_len; ++k) {
                        std::cerr << std::hex << (int)(unsigned char)res->primary_transliteration[k] << " ";
                    }
                    std::cerr << "\n";
                    assert(false);
                }
            }
            for (size_t i = 0; i < res->candidate_count; ++i) {
                if (res->candidates[i].word) {
                    size_t w_len = std::strlen(res->candidates[i].word);
                    assert(is_valid_utf8(res->candidates[i].word, w_len));
                }
            }
            abk_free_suggestions(res);
        }

        // Test 2: Fast transliteration
        int fast_ret = abk_transliterate_fast(engine, current_buffer.c_str(), out_buf, sizeof(out_buf));
        if (fast_ret >= 0) {
            assert(is_valid_utf8(out_buf, fast_ret));
        }

        // Test 3: Normalization & Grapheme utilities
        int norm_ret = abk_normalize_nfc(current_buffer.c_str(), out_buf, sizeof(out_buf));
        if (norm_ret >= 0) {
            assert(is_valid_utf8(out_buf, norm_ret));
        }

        int pop_ret = abk_pop_logical_unit(current_buffer.c_str(), out_buf, sizeof(out_buf));
        if (pop_ret >= 0) {
            assert(is_valid_utf8(out_buf, pop_ret));
        }

        abk_grapheme_count(current_buffer.c_str());

        if ((iter + 1) % 10000 == 0) {
            std::cout << "  Passed " << (iter + 1) << " iterations...\n";
        }
    }

    abk_engine_destroy(engine);

    std::cout << "=========================================================\n";
    std::cout << " Fuzz testing completed: ZERO crashes, ZERO invalid UTF-8!\n";
    std::cout << "=========================================================\n";

    return 0;
}
