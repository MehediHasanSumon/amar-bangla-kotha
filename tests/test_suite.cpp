#include "abk/utf8_util.h"
#include "abk/transliterator.h"
#include "abk/trie.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/engine.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        passed_tests++; \
        std::cout << "  [PASS] " << msg << "\n"; \
    } else { \
        std::cerr << "  [FAIL] " << msg << " (Line " << __LINE__ << ")\n"; \
    } \
} while (0)

void test_utf8() {
    std::cout << "\n--- Testing UTF-8 and Bengali Unicode Utilities ---\n";

    std::string bn_str = "বাংলা";
    auto cps = abk::utf8::to_codepoints(bn_str);
    TEST_ASSERT(cps.size() == 5, "Codepoint count of 'বাংলা' should be 5");
    TEST_ASSERT(abk::utf8::to_utf8(cps) == bn_str, "Codepoint to UTF-8 roundtrip");

    // Codepoint classifications
    TEST_ASSERT(abk::utf8::is_bengali(0x0986), "U+0986 (আ) is Bengali");
    TEST_ASSERT(abk::utf8::is_bengali_vowel(0x0986), "U+0986 (আ) is Bengali vowel");
    TEST_ASSERT(abk::utf8::is_bengali_kar(0x09BE), "U+09BE (া) is Bengali kar");
    TEST_ASSERT(abk::utf8::is_bengali_consonant(0x0995), "U+0995 (ক) is Bengali consonant");
    TEST_ASSERT(abk::utf8::is_bengali_virama(0x09CD), "U+09CD (্) is Bengali virama");
}

void test_transliterator() {
    std::cout << "\n--- Testing Transliteration Engine ---\n";
    abk::Transliterator tr;

    TEST_ASSERT(tr.transliterate("ami") == "আমি", "Transliterate 'ami' -> 'আমি'");
    TEST_ASSERT(tr.transliterate("amar") == "আমার", "Transliterate 'amar' -> 'আমার'");
    TEST_ASSERT(tr.transliterate("kotha") == "কথা", "Transliterate 'kotha' -> 'কথা'");
    TEST_ASSERT(tr.transliterate("bangla") == "বাংলা", "Transliterate 'bangla' -> 'বাংলা'");
    TEST_ASSERT(tr.transliterate("dhonnobad") == "ধন্যবাদ", "Transliterate 'dhonnobad' -> 'ধন্যবাদ'");
    TEST_ASSERT(tr.transliterate("bhalo") == "ভালো", "Transliterate 'bhalo' -> 'ভালো'");
    TEST_ASSERT(tr.transliterate("prothom") == "প্রথম", "Transliterate 'prothom' -> 'প্রথম'");
    TEST_ASSERT(tr.transliterate("shobdo") == "শব্দ", "Transliterate 'shobdo' -> 'শব্দ'");
}

void test_trie_and_dictionary() {
    std::cout << "\n--- Testing Trie and Dictionary ---\n";
    abk::Trie trie;
    trie.insert("কথা", 100);
    trie.insert("কথায়", 80);
    trie.insert("কথার", 90);
    trie.insert("কখন", 50);

    TEST_ASSERT(trie.search("কথা"), "Trie finds 'কথা'");
    TEST_ASSERT(!trie.search("বাংলা"), "Trie correctly reports missing 'বাংলা'");

    auto prefix_results = trie.find_by_prefix("কথ");
    TEST_ASSERT(prefix_results.size() == 3, "Prefix 'কথ' returns exactly 3 words");
    TEST_ASSERT(prefix_results[0].first == "কথা", "Top prefix match is 'কথা' by frequency");

    abk::Dictionary dict;
    TEST_ASSERT(dict.word_count() > 50, "Built-in dictionary contains words");
    TEST_ASSERT(dict.contains("বাংলাদেশ"), "Dictionary contains 'বাংলাদেশ'");
    TEST_ASSERT(dict.contains("ধন্যবাদ"), "Dictionary contains 'ধন্যবাদ'");
}

void test_autocorrect() {
    std::cout << "\n--- Testing Autocorrect & Levenshtein ---\n";
    abk::Autocorrect ac;

    double d1 = ac.compute_distance("কথা", "কথা");
    TEST_ASSERT(d1 == 0.0, "Identical words distance is 0");

    double d2 = ac.compute_distance("কথ", "কথা");
    TEST_ASSERT(d2 == 1.0, "Single character insertion distance is 1");

    // Confusion penalty check (শ vs স)
    double d_conf = ac.compute_distance("দেশ", "দেস");
    TEST_ASSERT(d_conf == 0.5, "Phonetic confusion between 'শ' and 'স' has reduced penalty (0.5)");

    abk::Trie trie;
    trie.insert("কথা", 100);
    trie.insert("কখন", 50);
    trie.insert("কত", 70);

    auto suggestions = ac.suggest_corrections(trie, "কথ", 1);
    TEST_ASSERT(!suggestions.empty(), "Autocorrect suggestions found for 'কথ'");
    TEST_ASSERT(suggestions[0].word == "কথা", "Top suggestion for 'কথ' is 'কথা'");
}

void test_engine_pipeline() {
    std::cout << "\n--- Testing Unified AmarBanglaKothaEngine ---\n";
    abk::AmarBanglaKothaEngine engine;

    auto res = engine.process("kotha");
    TEST_ASSERT(!res.candidates.empty(), "Engine returns candidates for 'kotha'");
    TEST_ASSERT(res.candidates[0].word == "কথা", "Top candidate for 'kotha' is 'কথা'");

    auto res_ami = engine.process("ami");
    TEST_ASSERT(res_ami.candidates[0].word == "আমি", "Top candidate for 'ami' is 'আমি'");

    // Test learning
    engine.learn_word("রোবোটিক্স", 9999);
    TEST_ASSERT(engine.get_dictionary().contains("রোবোটিক্স"), "Engine learned custom word 'রোবোটিক্স'");
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Running Amar Bangla Kotha Engine Tests \n";
    std::cout << "========================================\n";

    test_utf8();
    test_transliterator();
    test_trie_and_dictionary();
    test_autocorrect();
    test_engine_pipeline();

    std::cout << "\n========================================\n";
    std::cout << " Test Summary: " << passed_tests << " / " << total_tests << " passed.\n";
    std::cout << "========================================\n";

    return (passed_tests == total_tests) ? 0 : 1;
}
