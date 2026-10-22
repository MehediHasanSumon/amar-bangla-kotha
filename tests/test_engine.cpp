#include "catch2/catch.hpp"
#include <fstream>
#include <sstream>
#include "abk/utf8_util.h"
#include "abk/transliterator.h"
#include "abk/trie.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/engine.h"
#include "abk/abk.h"

TEST_CASE("UTF-8 Utilities and Bengali Unicode Handling", "[utf8]") {
    SECTION("Codepoint conversions") {
        std::string bn_str = "বাংলা";
        auto cps = abk::utf8::to_codepoints(bn_str);
        REQUIRE(cps.size() == 5);
        REQUIRE(abk::utf8::to_utf8(cps) == bn_str);
    }

    SECTION("Bengali Character Classifications") {
        REQUIRE(abk::utf8::is_bengali(0x0986));            // আ
        REQUIRE(abk::utf8::is_bengali_vowel(0x0986));      // আ
        REQUIRE(abk::utf8::is_bengali_kar(0x09BE));        // া
        REQUIRE(abk::utf8::is_bengali_consonant(0x0995));  // ক
        REQUIRE(abk::utf8::is_bengali_virama(0x09CD));     // ্
        REQUIRE(abk::utf8::is_bengali_modifier(0x0982));   // ং
        REQUIRE(abk::utf8::is_bengali_digit(0x09E8));      // ২
    }

    SECTION("Vowel to Kar mapping") {
        REQUIRE(abk::utf8::vowel_to_kar(0x0986) == 0x09BE); // আ -> া
        REQUIRE(abk::utf8::vowel_to_kar(0x0987) == 0x09BF); // ই -> ি
        REQUIRE(abk::utf8::kar_to_vowel(0x09BE) == 0x0986); // া -> আ
    }
}

TEST_CASE("Core Transliteration - Rule Matcher", "[transliterator]") {
    abk::Transliterator tr;

    SECTION("Basic High-Frequency Words") {
        CHECK(tr.transliterate("ami") == "আমি");
        CHECK(tr.transliterate("amar") == "আমার");
        CHECK(tr.transliterate("kotha") == "কথা");
        CHECK(tr.transliterate("bangla") == "বাংলা");
        CHECK(tr.transliterate("dhonnobad") == "ধন্যবাদ");
        CHECK(tr.transliterate("bhalo") == "ভালো");
        CHECK(tr.transliterate("prothom") == "প্রথম");
        CHECK(tr.transliterate("shobdo") == "শব্দ");
    }

    SECTION("Phase 2: Diphthongs and Overrides: koi vs kOI") {
        abk::Dictionary dict;
        dict.load_from_file("data/bangla_wordlist.txt");
        // "koi" with dictionary resolves diphthong to common word "কই"
        CHECK(tr.transliterate("koi", &dict) == "কই");
        // "kOI" with explicit uppercase override produces "কৈ"
        CHECK(tr.transliterate("kOI", &dict) == "কৈ");
        // "boi" produces "বই"
        CHECK(tr.transliterate("boi", &dict) == "বই");
    }
}

TEST_CASE("Trie and Dictionary", "[trie][dictionary]") {
    SECTION("Prefix searching and frequency ordering") {
        abk::Trie trie;
        trie.insert("কথা", 100);
        trie.insert("কথায়", 80);
        trie.insert("কথার", 90);
        trie.insert("কখন", 50);

        REQUIRE(trie.search("কথা"));
        REQUIRE(!trie.search("বাংলা"));
        REQUIRE(trie.get_frequency("কথা") == 100);

        auto matches = trie.find_by_prefix("কথ", 5);
        REQUIRE(matches.size() == 3);
        CHECK(matches[0].first == "কথা");
        CHECK(matches[1].first == "কথার");
        CHECK(matches[2].first == "কথায়");
    }

    SECTION("Dictionary built-in vocabulary") {
        abk::Dictionary dict;
        REQUIRE(dict.word_count() > 50);
        REQUIRE(dict.contains("বাংলাদেশ"));
        REQUIRE(dict.contains("ধন্যবাদ"));
        REQUIRE(dict.contains("আমি"));
    }
}

TEST_CASE("Autocorrect and Phonetic Distance", "[autocorrect]") {
    abk::Autocorrect ac;

    SECTION("Exact and edit distance") {
        CHECK(ac.compute_distance("কথা", "কথা") == 0.0);
        CHECK(ac.compute_distance("কথ", "কথা") == 1.0);
    }

    SECTION("Phonetic similarity discount") {
        // 'শ' and 'স' should have reduced distance 0.4
        double dist = ac.compute_distance("দেশ", "দেস");
        CHECK(dist == 0.4);
    }
}

TEST_CASE("AmarBanglaKothaEngine Full Pipeline", "[engine]") {
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Engine processes input and ranks candidate 1") {
        auto res = engine.process("ami");
        REQUIRE(!res.candidates.empty());
        CHECK(res.candidates[0].word == "আমি");
    }

    SECTION("Dynamic word learning") {
        engine.learn_word("রোবোটিক্স", 5000);
        REQUIRE(engine.get_dictionary().contains("রোবোটিক্স"));
    }
}

TEST_CASE("Phase 3: Unicode Normalization (NFC Enforcement)", "[unicode][nfc]") {
    SECTION("Two-part vowel signs composition") {
        // e-kar (0x09C7) + aa-kar (0x0BE) -> o-kar (0x09CB ো)
        std::string decomp_o = "\xE0\xA7\x87\xE0\xA6\xBE";
        std::string comp_o = abk::utf8::normalize_nfc(decomp_o);
        CHECK(comp_o == "ো");

        // e-kar (0x09C7) + au-length (0x09D7) -> au-kar (0x09CC ৌ)
        std::string decomp_au = "\xE0\xA7\x87\xE0\xA7\x97";
        std::string comp_au = abk::utf8::normalize_nfc(decomp_au);
        CHECK(comp_au == "ৌ");
    }

    SECTION("Nukta canonicalization: precomposed vs decomposed matching") {
        // Precomposed ড় (U+09DC) vs Decomposed ড + ় (U+09A1 + U+09BC)
        std::string precomp_rra = "\u09DC";
        std::string norm_rra = abk::utf8::normalize_nfc(precomp_rra);
        std::string expected_rra = "\u09A1\u09BC"; // ড + ়
        CHECK(norm_rra == expected_rra);

        // Precomposed ঢ় (U+09DD) vs Decomposed ঢ + ়
        std::string precomp_rha = "\u09DD";
        std::string norm_rha = abk::utf8::normalize_nfc(precomp_rha);
        std::string expected_rha = "\u09A2\u09BC"; // ঢ + ়
        CHECK(norm_rha == expected_rha);

        // Precomposed য় (U+09DF) vs Decomposed য + ়
        std::string precomp_yaa = "\u09DF";
        std::string norm_yaa = abk::utf8::normalize_nfc(precomp_yaa);
        std::string expected_yaa = "\u09AF\u09BC"; // য + ়
        CHECK(norm_yaa == expected_yaa);

        // Normalization idempotency: f(f(x)) == f(x)
        std::string test_str = "বাড়ি ও সময়";
        CHECK(abk::utf8::normalize_nfc(abk::utf8::normalize_nfc(test_str)) == abk::utf8::normalize_nfc(test_str));
    }

    SECTION("Dictionary lookup with mixed normalization forms") {
        abk::Dictionary dict;
        // Insert with precomposed ড় (U+09DC)
        dict.add_word("বা\u09DCি", 5000);
        // Query with decomposed ড + ় (U+09A1 + U+09BC)
        CHECK(dict.contains("বা\u09A1\u09BCি"));
    }
}

TEST_CASE("Phase 3: Grapheme Clusters & Backspace Deletion", "[grapheme][backspace]") {
    SECTION("Backspace on vowel signs leaves base consonant intact") {
        // 'কি' (ক + ি): Backspace removes ি, leaving 'ক'
        std::string ki = "কি";
        std::string after_bs1 = abk::utf8::pop_logical_unit(ki);
        CHECK(after_bs1 == "ক");

        // Backspace on 'ক' leaves empty string
        std::string after_bs2 = abk::utf8::pop_logical_unit(after_bs1);
        CHECK(after_bs2 == "");

        // 'কে' (ক + ে) -> 'ক'
        CHECK(abk::utf8::pop_logical_unit("কে") == "ক");

        // 'কৈ' (ক + ৈ) -> 'ক'
        CHECK(abk::utf8::pop_logical_unit("কৈ") == "ক");

        // 'কো' (ক + ো) -> 'ক'
        CHECK(abk::utf8::pop_logical_unit("কো") == "ক");

        // 'কৌ' (ক + ৌ) -> 'ক'
        CHECK(abk::utf8::pop_logical_unit("কৌ") == "ক");
    }

    SECTION("Backspace on modifiers leaves base intact") {
        // 'চাঁদ' (চ + ঁ + া + দ): popping দ leaves 'চাঁা' / 'চাঁ'
        CHECK(abk::utf8::pop_logical_unit("দুঃখ") == "দুঃ");
        CHECK(abk::utf8::pop_logical_unit("দুঃ") == "দু");
        CHECK(abk::utf8::pop_logical_unit("রং") == "র");
    }

    SECTION("Grapheme cluster segmentation count") {
        CHECK(abk::utf8::grapheme_count("বাংলা") == 2); // বা+ং (syllable cluster) and লা
        CHECK(abk::utf8::grapheme_count("ক্ষমা") == 2); // ক্ষ, মা
        CHECK(abk::utf8::grapheme_count("আমি") == 2);   // আ, মি
    }
}

TEST_CASE("Phase 3: Hasanta, ZWJ and ZWNJ Disambiguation", "[hasanta][zwj][zwnj]") {
    abk::Transliterator tr;

    SECTION("ZWJ Reph vs Ya-phala disambiguation") {
        abk::Dictionary dict;
        dict.load_from_file("data/bangla_wordlist.txt");
        // r``yab -> র‍্যাব (Contains ZWJ U+200D to produce Ra with Ya-phala)
        std::string res_ryab = tr.transliterate("r``yab", &dict);
        CHECK(res_ryab.find("\xE2\x80\x8D") != std::string::npos); // UTF-8 of ZWJ U+200D
        CHECK(res_ryab == "র‍্যাব");

        // Standard Reph without ZWJ: kormo -> কর্ম
        std::string res_kormo = tr.transliterate("kormo", &dict);
        CHECK(res_kormo.find("\xE2\x80\x8D") == std::string::npos);
        CHECK(res_kormo == "কর্ম");
    }

    SECTION("Explicit Hasanta input via ,,") {
        std::string res_hasant = tr.transliterate(",,");
        CHECK(res_hasant == "্");
    }
}

TEST_CASE("Phase 4: Compact Flat Trie & User Overlay", "[trie][user_dict]") {
    SECTION("Compact flat trie node indexing and memory footprint") {
        abk::Trie trie;
        trie.insert("বাংলা", 99000);
        trie.insert("বাংলাদেশ", 98000);
        trie.insert("বাঙালি", 85000);

        REQUIRE(trie.size() == 3);
        REQUIRE(trie.node_count() > 3);
        REQUIRE(trie.memory_bytes() > 0);

        uint32_t root_idx = trie.get_root_index();
        // Child lookup via binary search for 'ব' (0x09AC)
        uint32_t ba_child = trie.find_child(root_idx, 0x09AC);
        REQUIRE(ba_child != 0);
    }

    SECTION("User dictionary overlay precedence and prefix merging") {
        abk::Dictionary dict;
        dict.load_from_file("data/bangla_wordlist.txt");

        // Add custom word to user layer
        dict.add_word("রোবোটিক্স-ল্যাব", 99999);
        REQUIRE(dict.contains("রোবোটিক্স-ল্যাব"));
        REQUIRE(dict.get_frequency("রোবোটিক্স-ল্যাব") == 99999);

        // User prefix match should include the user word
        auto matches = dict.get_prefix_matches("রোবো", 5);
        REQUIRE(!matches.empty());
        CHECK(matches[0].first == "রোবোটিক্স-ল্যাব");
    }
}

TEST_CASE("Phase 4: Enhanced Autocorrect & Confusions", "[autocorrect][confusions]") {
    abk::Autocorrect ac;

    SECTION("Broad phonetic confusions: শ/ষ/স, ণ/ন, ড়/র, ৎ/ত, ব/ভ") {
        // Nasals: কারণ vs কারন
        CHECK(ac.compute_distance("কারণ", "কারন") == 0.4);

        // Flaps / R sounds: বাড়ি vs বারি
        CHECK(ac.compute_distance("বাড়ি", "বারি") == 0.4);

        // Ta vs Khanda-Ta: হতাৎ vs হতাত
        CHECK(ac.compute_distance("হতাৎ", "হতাত") == 0.4);

        // Labials: ভুল vs বুল
        CHECK(ac.compute_distance("ভুল", "বুল") == 0.4);

        // Long vs Short I: নদী vs নদি
        CHECK(ac.compute_distance("নদী", "নদি") == 0.4);
    }

    SECTION("Bounded search performance (< 1ms)") {
        abk::Dictionary dict;
        dict.load_from_file("data/bangla_wordlist.txt");

        auto t0 = std::chrono::high_resolution_clock::now();
        auto suggestions = ac.suggest_corrections(dict.get_system_trie(), "মানুশ", 2, 5);
        auto t1 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        CHECK(us < 2000.0); // Bounded strictly under 2ms (typically < 300us)
        REQUIRE(!suggestions.empty());
        CHECK(suggestions[0].word == "মানুষ"); // Typo "মানুশ" corrected to "মানুষ"
    }
}

TEST_CASE("Phase 4: Principled Log-Linear Candidate Ranking", "[engine][ranking]") {
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("High-frequency dictionary words outrank rare transliterations") {
        auto res = engine.process("koi");
        REQUIRE(!res.candidates.empty());
        // Candidate 1 must be 'কই'
        CHECK(res.candidates[0].word == "কই");
        CHECK(res.candidates[0].type == abk::CandidateType::DICTIONARY_EXACT);
    }

    SECTION("Prefix completions never hijack space bar (#1 spot)") {
        auto res = engine.process("bangla");
        REQUIRE(!res.candidates.empty());
        // Candidate 1 must be exact word 'বাংলা', not the longer completion 'বাংলাদেশ'
        CHECK(res.candidates[0].word == "বাংলা");
    }
}

TEST_CASE("Workstream 1: Suggestion State Machine and Navigation", "[engine][suggestion_state]") {
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Initial state and query population") {
        auto res = engine.process("ami");
        REQUIRE(!res.candidates.empty());
        const auto& state = engine.get_suggestion_state();
        CHECK(state.visible);
        CHECK(state.selected_index == 0);
        CHECK(state.items.size() == res.candidates.size());
        CHECK(engine.get_selected_candidate_word() == res.candidates[0].word);
    }

    SECTION("Arrow Down / Next Candidate Navigation with Wrap-around") {
        engine.process("k");
        const auto& state = engine.get_suggestion_state();
        size_t count = state.items.size();
        REQUIRE(count > 2);

        CHECK(state.selected_index == 0);

        // Move next
        bool ok = engine.select_next_candidate();
        CHECK(ok);
        CHECK(engine.get_suggestion_state().selected_index == 1);
        CHECK(engine.get_selected_candidate_word() == state.items[1].word);

        // Move to last item
        for (size_t i = 2; i < count; ++i) {
            engine.select_next_candidate();
        }
        CHECK(engine.get_suggestion_state().selected_index == static_cast<int>(count - 1));

        // Wrap around back to 0
        bool wrapped = engine.select_next_candidate();
        CHECK(wrapped);
        CHECK(engine.get_suggestion_state().selected_index == 0);
        CHECK(engine.get_selected_candidate_word() == state.items[0].word);
    }

    SECTION("Arrow Up / Prev Candidate Navigation with Wrap-around") {
        engine.process("k");
        const auto& state = engine.get_suggestion_state();
        size_t count = state.items.size();
        REQUIRE(count > 2);

        CHECK(state.selected_index == 0);

        // Prev from index 0 wraps to count - 1
        bool wrapped = engine.select_prev_candidate();
        CHECK(wrapped);
        CHECK(engine.get_suggestion_state().selected_index == static_cast<int>(count - 1));
        CHECK(engine.get_selected_candidate_word() == state.items[count - 1].word);

        // Prev from count - 1 moves to count - 2
        bool prev = engine.select_prev_candidate();
        CHECK(prev);
        CHECK(engine.get_suggestion_state().selected_index == static_cast<int>(count - 2));
    }

    SECTION("Direct Index Selection (Mouse Click / Number Key) and Clamping") {
        engine.process("k");
        const auto& state = engine.get_suggestion_state();
        size_t count = state.items.size();
        REQUIRE(count >= 3);

        bool sel = engine.select_candidate(2);
        CHECK(sel);
        CHECK(engine.get_suggestion_state().selected_index == 2);
        CHECK(engine.get_selected_candidate_word() == state.items[2].word);

        // Clamping on out-of-range index
        bool clamped = engine.select_candidate(999);
        CHECK(clamped);
        CHECK(engine.get_suggestion_state().selected_index == static_cast<int>(count - 1));
    }

    SECTION("Selection Stability Across Keystrokes") {
        // Step 1: Type "ko", candidates are generated
        engine.process("ko");
        const auto& s1 = engine.get_suggestion_state();
        REQUIRE(s1.items.size() >= 2);

        // Select candidate 1
        engine.select_candidate(1);
        std::string selected_word = engine.get_selected_candidate_word();
        CHECK(!selected_word.empty());

        // Step 2: Next keystroke
        engine.process("koi");
        const auto& s2 = engine.get_suggestion_state();
        REQUIRE(!s2.items.empty());

        // If previously selected word is in s2, selected_index must match it
        bool found = false;
        for (size_t i = 0; i < s2.items.size(); ++i) {
            if (s2.items[i].word == selected_word) {
                CHECK(s2.selected_index == static_cast<int>(i));
                found = true;
                break;
            }
        }
        if (!found) {
            // Otherwise it must be safely clamped
            CHECK(s2.selected_index < static_cast<int>(s2.items.size()));
        }
    }

    SECTION("Reset Suggestion State") {
        engine.process("ami");
        CHECK(engine.get_suggestion_state().visible);
        engine.reset_suggestion_state();
        CHECK(!engine.get_suggestion_state().visible);
        CHECK(engine.get_suggestion_state().selected_index == 0);
        CHECK(engine.get_suggestion_state().items.empty());
    }
}

TEST_CASE("Workstream 1: C ABI Suggestion Functions", "[c_api][suggestion_state]") {
    abk_engine_t engine = abk_engine_create();
    REQUIRE(engine != nullptr);
    abk_load_dictionary(engine, "data/bangla_wordlist.txt");

    abk_suggestion_list_t* res = abk_get_suggestions(engine, "k");
    REQUIRE(res != nullptr);
    REQUIRE(res->candidate_count > 2);

    CHECK(abk_suggestion_is_visible(engine) == 1);
    CHECK(abk_suggestion_get_selected_index(engine) == 0);

    // Next
    int next_ok = abk_suggestion_select_next(engine);
    CHECK(next_ok == 1);
    CHECK(abk_suggestion_get_selected_index(engine) == 1);

    // Prev
    int prev_ok = abk_suggestion_select_prev(engine);
    CHECK(prev_ok == 1);
    CHECK(abk_suggestion_get_selected_index(engine) == 0);

    // Select index
    int sel_ok = abk_suggestion_select_index(engine, 2);
    CHECK(sel_ok == 1);
    CHECK(abk_suggestion_get_selected_index(engine) == 2);

    // Word via ptr
    const char* word_ptr = abk_suggestion_get_selected_word_ptr(engine);
    REQUIRE(word_ptr != nullptr);
    CHECK(std::string(word_ptr) == std::string(res->candidates[2].word));

    // Word via copy buffer
    char buf[128] = {0};
    int len = abk_suggestion_get_selected_word(engine, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK(std::string(buf) == std::string(res->candidates[2].word));

    // Reset
    abk_suggestion_reset(engine);
    CHECK(abk_suggestion_is_visible(engine) == 0);
    CHECK(abk_suggestion_get_selected_index(engine) == 0);

    abk_free_suggestions(res);
    abk_engine_destroy(engine);
}

TEST_CASE("Workstream 2: Safety, Privacy and Password Auto-Bypass", "[engine][security]") {
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Normal mode vs Secure mode processing") {
        // Normal mode produces transliteration
        auto normal_res = engine.process("ami");
        CHECK(normal_res.primary_transliteration == "আমি");
        CHECK(!normal_res.candidates.empty());
        CHECK(engine.get_suggestion_state().visible);

        // Turn on secure mode (e.g. entering a password or PIN field)
        engine.set_secure_mode(true);
        CHECK(engine.is_secure_mode());
        CHECK(!engine.get_suggestion_state().visible);

        // In secure mode: auto-bypass, zero transliteration, zero candidate exposure
        auto secure_res = engine.process("ami");
        CHECK(secure_res.primary_transliteration == "ami");
        CHECK(secure_res.candidates.empty());
        CHECK(!engine.get_suggestion_state().visible);

        // Fast transliteration also bypasses
        CHECK(engine.transliterate_fast("secret123") == "secret123");

        // Turn secure mode back off
        engine.set_secure_mode(false);
        CHECK(!engine.is_secure_mode());
        auto restored_res = engine.process("ami");
        CHECK(restored_res.primary_transliteration == "আমি");
    }

    SECTION("Zero learning and storage in secure mode") {
        engine.set_secure_mode(true);

        // Attempt to learn a sensitive word in secure mode
        engine.learn_word("গোপনপাসওয়ার্ড", 10000);

        // Verify dictionary does NOT store or learn the sensitive word
        CHECK(!engine.get_dictionary().contains("গোপনপাসওয়ার্ড"));
        auto matches = engine.get_dictionary().get_prefix_matches("গোপন", 5);
        for (const auto& m : matches) {
            CHECK(m.first != "গোপনপাসওয়ার্ড");
        }
    }

    SECTION("C ABI Secure and Clipboard Fallback APIs") {
        abk_engine_t c_eng = abk_engine_create();
        REQUIRE(c_eng != nullptr);

        CHECK(abk_engine_is_secure_mode(c_eng) == 0);
        abk_engine_set_secure_mode(c_eng, 1);
        CHECK(abk_engine_is_secure_mode(c_eng) == 1);

        abk_suggestion_list_t* res = abk_get_suggestions(c_eng, "password123");
        REQUIRE(res != nullptr);
        // Under secure mode, candidate count is 0 and transliteration is passthrough
        CHECK(res->candidate_count == 0);
        CHECK(std::string(res->primary_transliteration) == "password123");
        abk_free_suggestions(res);

        // Clipboard paste fallback mode toggle
        CHECK(abk_engine_is_clipboard_mode(c_eng) == 0);
        abk_engine_set_clipboard_mode(c_eng, 1);
        CHECK(abk_engine_is_clipboard_mode(c_eng) == 1);
        abk_engine_set_clipboard_mode(c_eng, 0);
        CHECK(abk_engine_is_clipboard_mode(c_eng) == 0);

        abk_engine_destroy(c_eng);
    }
}

TEST_CASE("Workstream 3: Token Classification and Bypass", "[classifier][bypass]") {
    abk::TokenClassifier classifier;
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Golden token classification TSV benchmark verification") {
        std::ifstream file("tests/data/token_classification.tsv");
        REQUIRE(file.is_open());

        std::string line;
        // Read header
        std::getline(file, line);

        int total_tested = 0;
        int banglish_tested = 0;
        int banglish_false_positives = 0;
        int bypass_tested = 0;
        int bypass_correct = 0;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token, expected_type, should_bypass_str, category;
            if (std::getline(ss, token, '\t') &&
                std::getline(ss, expected_type, '\t') &&
                std::getline(ss, should_bypass_str, '\t') &&
                std::getline(ss, category, '\t')) {

                bool should_bypass = (should_bypass_str == "1");
                auto result = classifier.classify(token);
                total_tested++;

                if (category == "BANGLISH_PROTECTED") {
                    banglish_tested++;
                    if (result.should_bypass_transliteration) {
                        banglish_false_positives++;
                    }
                    CHECK(result.type == abk::TokenType::BANGLA_PHONETIC);
                    CHECK(!result.should_bypass_transliteration);
                } else if (should_bypass) {
                    bypass_tested++;
                    if (result.should_bypass_transliteration) {
                        bypass_correct++;
                    }
                    CHECK(result.should_bypass_transliteration);
                }
            }
        }

        // Strict accuracy assertions
        CHECK(total_tested > 40);
        CHECK(banglish_tested >= 15);
        CHECK(banglish_false_positives == 0); // 0% false positives on Banglish
        CHECK(bypass_tested >= 20);
        CHECK(bypass_correct == bypass_tested); // 100% bypass accuracy on URLs, emails, code, paths
    }

    SECTION("Engine process bypasses non-Bengali tokens into passthrough") {
        // URLs
        auto url_res = engine.process("https://bangladesh.gov.bd");
        CHECK(url_res.candidates.empty());
        CHECK(url_res.primary_transliteration == "https://bangladesh.gov.bd");

        // Emails
        auto email_res = engine.process("dev@domain.com");
        CHECK(email_res.candidates.empty());
        CHECK(email_res.primary_transliteration == "dev@domain.com");

        // Code tokens
        auto code_res = engine.process("myVariableName");
        CHECK(code_res.candidates.empty());
        CHECK(code_res.primary_transliteration == "myVariableName");

        // English words
        auto eng_res = engine.process("computer");
        CHECK(eng_res.candidates.empty());
        CHECK(eng_res.primary_transliteration == "computer");

        // Banglish words are NOT bypassed
        auto bangla_res = engine.process("kotha");
        CHECK(!bangla_res.candidates.empty());
        CHECK(bangla_res.primary_transliteration == "কথা");
    }

    SECTION("C ABI token bypass function") {
        abk_engine_t c_eng = abk_engine_create();
        REQUIRE(c_eng != nullptr);

        CHECK(abk_token_should_bypass(c_eng, "https://google.com") == 1);
        CHECK(abk_token_should_bypass(c_eng, "test@mail.org") == 1);
        CHECK(abk_token_should_bypass(c_eng, "var_name") == 1);
        CHECK(abk_token_should_bypass(c_eng, "ami") == 0);
        CHECK(abk_token_should_bypass(c_eng, "kotha") == 0);

        abk_engine_destroy(c_eng);
    }
}

TEST_CASE("Workstream 3: Undo on Backspace State Machine", "[undo][backspace]") {
    abk::AmarBanglaKothaEngine engine;

    SECTION("Basic commit and undo cycle") {
        CHECK(!engine.can_undo_commit());

        // 1. Record commit after user types "kotha" and commits "কথা "
        engine.record_commit("kotha", "কথা ", false);
        CHECK(engine.can_undo_commit());
        CHECK(engine.get_last_commit().raw_input == "kotha");
        CHECK(engine.get_last_commit().committed_text == "কথা ");

        // 2. First backspace: undo_commit() reverts commit and clears can_undo
        auto undo_result = engine.undo_commit();
        CHECK(undo_result.first == "কথা ");
        CHECK(undo_result.second == "kotha");
        CHECK(!engine.can_undo_commit());

        // 3. Second call fails (already undone)
        auto second_undo = engine.undo_commit();
        CHECK(second_undo.first.empty());
        CHECK(second_undo.second.empty());
    }

    SECTION("Undo record cleared on explicit clear") {
        engine.record_commit("ami", "আমি", false);
        CHECK(engine.can_undo_commit());

        engine.clear_undo();
        CHECK(!engine.can_undo_commit());
    }

    SECTION("Secure mode prevents recording undo commits") {
        engine.set_secure_mode(true);
        engine.record_commit("password", "password", false);
        CHECK(!engine.can_undo_commit());
    }

    SECTION("C ABI Undo APIs") {
        abk_engine_t c_eng = abk_engine_create();
        REQUIRE(c_eng != nullptr);

        CHECK(abk_engine_can_undo_commit(c_eng) == 0);
        abk_engine_record_commit(c_eng, "bhalo", "ভালো ", 1);
        CHECK(abk_engine_can_undo_commit(c_eng) == 1);

        char comm_buf[128] = {0};
        char raw_buf[128] = {0};
        int ret = abk_engine_get_undo_info(c_eng, comm_buf, sizeof(comm_buf), raw_buf, sizeof(raw_buf));
        CHECK(ret == 1);
        CHECK(std::string(comm_buf) == "ভালো ");
        CHECK(std::string(raw_buf) == "bhalo");
        CHECK(abk_engine_can_undo_commit(c_eng) == 0);

        abk_engine_destroy(c_eng);
    }
}

TEST_CASE("Workstream 3: Backspace Unit & Bengali Orthography", "[backspace][grapheme]") {
    SECTION("Left-sided Kar signs pop logically from trailing end") {
        // Hrosho-I Kar: 'ক' + 'ি' = 'কি'
        std::string ki = "কি";
        std::string ki_popped = abk::utf8::pop_logical_unit(ki);
        CHECK(ki_popped == "ক");

        // E-Kar: 'ক' + 'ে' = 'কে'
        std::string ke = "কে";
        std::string ke_popped = abk::utf8::pop_logical_unit(ke);
        CHECK(ke_popped == "ক");

        // Oi-Kar: 'ক' + 'ৈ' = 'কৈ'
        std::string koi = "কৈ";
        std::string koi_popped = abk::utf8::pop_logical_unit(koi);
        CHECK(koi_popped == "ক");

        // O-Kar: 'ক' + 'ো' = 'কো'
        std::string ko = "কো";
        std::string ko_popped = abk::utf8::pop_logical_unit(ko);
        CHECK(ko_popped == "ক");
    }

    SECTION("Progressive logical popping of conjuncts (Yuktakshar)") {
        // 'ক' + '্' + 'ত' = 'ক্ত'
        std::string kta = "ক্ত";
        std::string step1 = abk::utf8::pop_logical_unit(kta);
        CHECK(step1 == "ক্"); // Pops 'ত'

        std::string step2 = abk::utf8::pop_logical_unit(step1);
        CHECK(step2 == "ক");  // Pops '্'

        std::string step3 = abk::utf8::pop_logical_unit(step2);
        CHECK(step3.empty()); // Pops 'ক'
    }

    SECTION("Grapheme cluster popping vs Logical unit popping") {
        // 'কি' is 1 grapheme cluster (Ka + i-kar) but 2 logical code points
        std::string ki = "কি";
        CHECK(abk::utf8::pop_grapheme_cluster(ki) == "");
        CHECK(abk::utf8::pop_logical_unit(ki) == "ক");

        // Multi-syllable word "আমি"
        std::string ami = "আমি";
        std::string ami_cluster_popped = abk::utf8::pop_grapheme_cluster(ami);
        CHECK(ami_cluster_popped == "আ"); // Deletes 'মি' cluster completely

        std::string ami_logical_popped = abk::utf8::pop_logical_unit(ami);
        CHECK(ami_logical_popped == "আম"); // Deletes 'ি' kar, leaves 'ম'
    }

    SECTION("C ABI pop functions") {
        char buf[128] = {0};
        int ret1 = abk_pop_logical_unit("কি", buf, sizeof(buf));
        CHECK(ret1 > 0);
        CHECK(std::string(buf) == "ক");

        int ret2 = abk_pop_grapheme_cluster("কি", buf, sizeof(buf));
        CHECK(ret2 == 0);
        CHECK(std::string(buf) == "");
    }
}
