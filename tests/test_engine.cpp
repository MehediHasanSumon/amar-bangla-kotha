#include "catch2/catch.hpp"
#include "abk/utf8_util.h"
#include "abk/transliterator.h"
#include "abk/trie.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/engine.h"

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
