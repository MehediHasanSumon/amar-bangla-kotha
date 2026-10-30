#include "catch2/catch.hpp"
#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include "abk/engine.h"
#include "abk/utf8_util.h"
#include "abk/abk.h"

#include <fstream>
#include <sstream>
#include <random>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

bool is_strictly_valid_utf8(const std::string& s) {
    const size_t len = s.size();
    size_t i = 0;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            if ((c2 & 0xC0) != 0x80 || c < 0xC2) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(s[i + 2]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            if (c == 0xE0 && c2 < 0xA0) return false;
            if (c == 0xED && c2 >= 0xA0) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(s[i + 2]);
            unsigned char c4 = static_cast<unsigned char>(s[i + 3]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) return false;
            if (c == 0xF0 && c2 < 0x90) return false;
            if (c == 0xF4 && c2 >= 0x90) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool contains_forbidden_assamese_cp(const std::string& s) {
    auto cps = abk::utf8::to_codepoints(s);
    for (uint32_t cp : cps) {
        if (cp == 0x09F0 || cp == 0x09F1) return true;
    }
    return false;
}

bool beam_contains(const std::vector<std::pair<std::string, double>>& beam, const std::string& target) {
    std::string norm_target = abk::utf8::normalize_nfc(target);
    for (const auto& item : beam) {
        if (item.first == norm_target) return true;
    }
    return false;
}

bool candidates_contain(const std::vector<abk::Candidate>& cands, const std::string& target) {
    std::string norm_target = abk::utf8::normalize_nfc(target);
    for (const auto& c : cands) {
        if (c.word == norm_target) return true;
    }
    return false;
}

} // namespace

TEST_CASE("Avro Style Spec: TOML Keymap Loading and Unicode Hygiene", "[avro][toml]") {
    abk::Transliterator tr;
    REQUIRE(tr.load_rules_from_toml("data/keymap_phonetic.toml"));
    CHECK(tr.rule_count() >= 100);

    // Phonotactics sanity checks
    CHECK(tr.can_form_conjunct(0x0997, 0x09B2)); // গ + ল (গ্ল)
    CHECK(tr.can_form_conjunct(0x0995, 0x099F)); // ক + ট (ক্ট)
    CHECK(tr.can_form_conjunct(0x09AA, 0x09B0)); // প + র (প্র)
    CHECK_FALSE(tr.can_form_conjunct(0x09A1, 0x09AE)); // ড + ম blocked (অ্যাডমিন)
    CHECK_FALSE(tr.can_form_conjunct(0x09B0, 0x0995)); // Single র never forms Reph
}

TEST_CASE("Avro Style Spec: Vowels, Case Sensitivity, and Bidirectional koi/kOI & kou/kOU", "[avro][vowels]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");
    abk::AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Case-sensitive vowel keys never case-fold") {
        CHECK(tr.transliterate("i") == "ই");
        CHECK(tr.transliterate("I") == "ঈ");
        CHECK(tr.transliterate("u") == "উ");
        CHECK(tr.transliterate("U") == "ঊ");
        CHECK(tr.transliterate("o") == "অ");
        CHECK(tr.transliterate("O") == "ও");
        CHECK(tr.transliterate("oi") == "অই");
        CHECK(tr.transliterate("OI") == "ঐ");
        CHECK(tr.transliterate("ou") == "অউ");
        CHECK(tr.transliterate("OU") == "ঔ");
    }

    SECTION("Bidirectional koi/kOI and kou/kOU in beam and engine suggestions") {
        CHECK(tr.transliterate("koi") == "কই");
        CHECK(tr.transliterate("kOI") == "কৈ");
        CHECK(tr.transliterate("kou") == "কউ");
        CHECK(tr.transliterate("kOU") == "কৌ");

        auto beam_koi = tr.transliterate_beam("koi", nullptr, 12);
        CHECK(beam_contains(beam_koi, "কই"));
        CHECK(beam_contains(beam_koi, "কৈ"));

        auto beam_kOI = tr.transliterate_beam("kOI", nullptr, 12);
        CHECK(beam_contains(beam_kOI, "কৈ"));
        CHECK(beam_contains(beam_kOI, "কই"));

        auto beam_kou = tr.transliterate_beam("kou", nullptr, 12);
        CHECK(beam_contains(beam_kou, "কউ"));
        CHECK(beam_contains(beam_kou, "কৌ"));

        auto beam_kOU = tr.transliterate_beam("kOU", nullptr, 12);
        CHECK(beam_contains(beam_kOU, "কৌ"));
        CHECK(beam_contains(beam_kOU, "কউ"));

        auto res_koi = engine.process("koi");
        REQUIRE(!res_koi.candidates.empty());
        CHECK(res_koi.candidates[0].word == "কই");
        CHECK(candidates_contain(res_koi.candidates, "কৈ"));

        auto res_kOI = engine.process("kOI");
        REQUIRE(!res_kOI.candidates.empty());
        CHECK(res_kOI.candidates[0].word == "কৈ");
        CHECK(candidates_contain(res_kOI.candidates, "কই"));

        auto res_kou = engine.process("kou");
        REQUIRE(!res_kou.candidates.empty());
        CHECK(res_kou.candidates[0].word == "কউ");
        CHECK(candidates_contain(res_kou.candidates, "কৌ"));

        auto res_kOU = engine.process("kOU");
        REQUIRE(!res_kOU.candidates.empty());
        CHECK(res_kOU.candidates[0].word == "কৌ");
        CHECK(candidates_contain(res_kOU.candidates, "কউ"));

        // Explicit uppercase OI/OU must NEVER be overridden by high-frequency dictionary words
        auto res_bOI = engine.process("bOI");
        REQUIRE(!res_bOI.candidates.empty());
        CHECK(res_bOI.candidates[0].word == "বৈ");
        CHECK(candidates_contain(res_bOI.candidates, "বই"));

        auto res_mOI = engine.process("mOI");
        REQUIRE(!res_mOI.candidates.empty());
        CHECK(res_mOI.candidates[0].word == "মৈ");
        CHECK(candidates_contain(res_mOI.candidates, "মই"));

        auto res_bOU = engine.process("bOU");
        REQUIRE(!res_bOU.candidates.empty());
        CHECK(res_bOU.candidates[0].word == "বৌ");
        CHECK(candidates_contain(res_bOU.candidates, "বউ"));
    }
}

TEST_CASE("Avro Style Spec: Backtick Separator and Explicit Escapes", "[avro][accent]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");

    CHECK(tr.transliterate("eki") == "একি");
    CHECK(tr.transliterate("ek`i") == "একই");
    CHECK(tr.transliterate("samia") == "সামিয়া");
    CHECK(tr.transliterate("sami`a") == "সামিআ");
    CHECK(tr.transliterate("a`") == "া");
    CHECK(tr.transliterate("i`") == "ি");
    CHECK(tr.transliterate("I`") == "ী");
    CHECK(tr.transliterate("o`") == "");
    CHECK(tr.transliterate("`") == "");
    CHECK(tr.transliterate("ek`") == "এক");
    CHECK(tr.transliterate("ek`Ta") == "একটা");
    CHECK(tr.transliterate("am`ra") == "আমরা");
    CHECK(tr.transliterate("t``") == "ৎ");
    CHECK(tr.transliterate(".`") == ".");
    CHECK(tr.transliterate(":`") == ":");
}

TEST_CASE("Avro Style Spec: Configurable Digits, Nukta-Ja, Pipe Mode, and Punctuation Guards", "[avro][settings]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");

    SECTION("Bangla digits toggle") {
        CHECK(tr.is_bangla_digits_enabled());
        CHECK(tr.transliterate("2026") == "২০২৬");
        CHECK(tr.transliterate("3.5") == "৩.৫");

        tr.set_bangla_digits(false);
        CHECK_FALSE(tr.is_bangla_digits_enabled());
        CHECK(tr.transliterate("2026") == "2026");
        CHECK(tr.transliterate("3.5") == "3.5");
        tr.set_bangla_digits(true);
    }

    SECTION("Shift+J Nukta-Ja toggle") {
        CHECK(tr.is_nukta_ja_enabled());
        CHECK(tr.transliterate("J") == abk::utf8::normalize_nfc("জ়"));

        tr.set_nukta_ja(false);
        CHECK_FALSE(tr.is_nukta_ja_enabled());
        CHECK(tr.transliterate("J") == "J");
        tr.set_nukta_ja(true);
    }

    SECTION("Pipe '|' mode configuration") {
        CHECK(tr.get_pipe_mode() == abk::PipeMode::PASSTHROUGH);
        CHECK(tr.transliterate("|") == "|");

        tr.set_pipe_mode(abk::PipeMode::LITERAL_DOT);
        CHECK(tr.transliterate("|") == ".");

        tr.set_pipe_mode(abk::PipeMode::BENGALI_DARI);
        CHECK(tr.transliterate("|") == "।");

        tr.set_pipe_mode(abk::PipeMode::PASSTHROUGH);
    }

    SECTION("Dot '.' never turns into Dari inside decimals, URLs, emails, or abbreviations") {
        CHECK(tr.transliterate(".") == "।");
        CHECK(tr.transliterate(".`") == ".");
        CHECK(tr.transliterate("3.5") == "৩.৫");
        CHECK(tr.transliterate("https://bangladesh.gov.bd") == "https://bangladesh.gov.bd");
        CHECK(tr.transliterate("test@example.com") == "test@example.com");
        CHECK(tr.transliterate("Dr.") == "Dr.");
        CHECK(tr.transliterate("...") == "...");
    }

    SECTION("Chandrabindu with 'qq' and '^' supports prefix, infix, and standalone notation") {
        std::string expected_dari = abk::utf8::normalize_nfc("দাঁড়ি");
        CHECK(tr.transliterate("qqdaRi") == expected_dari);
        CHECK(tr.transliterate("daqqRi") == expected_dari);
        CHECK(tr.transliterate("dqqaRi") == expected_dari);
        CHECK(tr.transliterate("qqcad") == abk::utf8::normalize_nfc("চাঁদ"));
        CHECK(tr.transliterate("caqqd") == abk::utf8::normalize_nfc("চাঁদ"));
        CHECK(tr.transliterate("cqqad") == abk::utf8::normalize_nfc("চাঁদ"));
        CHECK(tr.transliterate("qqidur") == abk::utf8::normalize_nfc("ইঁদুর"));
        CHECK(tr.transliterate("qq") == abk::utf8::normalize_nfc("ঁ"));
        CHECK(tr.transliterate("QQ") == abk::utf8::normalize_nfc("ঁ"));
        CHECK(tr.transliterate("qqdaRi.") == abk::utf8::normalize_nfc("দাঁড়ি।"));
    }
}

TEST_CASE("Avro Style Spec: Golden TSV Suite Across All Categories", "[avro][tsv]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");
    abk::Dictionary dict;
    REQUIRE(dict.load_from_file("data/bangla_wordlist.txt"));

    abk::EngineConfig cfg;
    cfg.dictionary_file_path = "data/bangla_wordlist.txt";
    cfg.enable_autocorrect = true;
    abk::AmarBanglaKothaEngine engine(cfg);

    std::ifstream file("tests/data/avro_style.tsv");
    REQUIRE(file.is_open());

    std::map<std::string, int> cat_total;
    std::map<std::string, int> cat_passed;

    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
        line_no++;
        if (line.empty() || line[0] == '#') continue;

        std::vector<std::string> cols;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '\t')) {
            cols.push_back(item);
        }
        if (cols.size() < 4) continue;

        const std::string& category = cols[0];
        const std::string& input = cols[1];
        const std::string expected = abk::utf8::normalize_nfc(cols[2]);
        const std::string& mode = cols[3];

        cat_total[category]++;

        INFO("Line " << line_no << " [" << category << "] input='" << input << "' mode=" << mode);
        CHECK_FALSE(contains_forbidden_assamese_cp(expected));
        CHECK(abk::utf8::normalize_nfc(expected) == expected);

        if (mode == "strict") {
            std::string actual = tr.transliterate(input, nullptr);
            CHECK(is_strictly_valid_utf8(actual));
            CHECK_FALSE(contains_forbidden_assamese_cp(actual));
            CHECK(abk::utf8::normalize_nfc(actual) == actual);
            CHECK(actual == expected);
            if (actual == expected) cat_passed[category]++;
        } else if (mode == "dictionary") {
            std::string actual_dict = tr.transliterate(input, &dict);
            auto beam = tr.transliterate_beam(input, nullptr, 24);
            CHECK(beam_contains(beam, expected));
            CHECK(actual_dict == expected);
            if (actual_dict == expected) cat_passed[category]++;
        } else if (mode == "engine_autocorrect") {
            auto res = engine.process(input);
            REQUIRE(!res.candidates.empty());
            CHECK(is_strictly_valid_utf8(res.candidates[0].word));
            CHECK_FALSE(contains_forbidden_assamese_cp(res.candidates[0].word));
            CHECK(abk::utf8::normalize_nfc(res.candidates[0].word) == res.candidates[0].word);
            CHECK(res.candidates[0].word == expected);
            CHECK(res.candidates[0].type == abk::CandidateType::AUTOCORRECT);
            if (res.candidates[0].word == expected) cat_passed[category]++;
        } else if (mode == "autocorrect") {
            // Guide entries with explicit uppercase/backtick control keys that Section 7 forbids
            // overriding automatically; verify secondary beam / suggestion list still contains them.
            auto beam = tr.transliterate_beam(input, nullptr, 32);
            bool in_beam = beam_contains(beam, expected);
            CHECK(in_beam);
            auto res = engine.process(input);
            CHECK(candidates_contain(res.candidates, expected));
            if (in_beam) cat_passed[category]++;
        }
    }

    const char* required_cats[] = {
        "vowel", "consonant", "accent", "phala", "reph",
        "conjunct", "punctuation", "y-rules", "autocorrect"
    };
    for (const char* c : required_cats) {
        INFO("Category: " << c);
        CHECK(cat_total[c] > 0);
        CHECK(cat_passed[c] == cat_total[c]);
    }
}

TEST_CASE("Avro Style Spec: Code-Point Conjunct Validation Suite (251 Clusters)", "[avro][conjuncts]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");

    std::ifstream file("tests/data/conjuncts_avro_style.tsv");
    REQUIRE(file.is_open());

    int total_clusters = 0;
    int total_key_variants = 0;
    std::string line;
    int line_no = 0;

    while (std::getline(file, line)) {
        line_no++;
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string expected_raw, keys_col, cps_col, status_col;
        if (!std::getline(ss, expected_raw, '\t') ||
            !std::getline(ss, keys_col, '\t') ||
            !std::getline(ss, cps_col, '\t') ||
            !std::getline(ss, status_col, '\t')) {
            continue;
        }

        total_clusters++;
        std::string expected = abk::utf8::normalize_nfc(expected_raw);
        CHECK_FALSE(contains_forbidden_assamese_cp(expected));
        CHECK(abk::utf8::normalize_nfc(expected) == expected);
        CHECK(status_col == "NEEDS_NATIVE_REVIEW");

        std::stringstream kss(keys_col);
        std::string key;
        while (std::getline(kss, key, '/')) {
            total_key_variants++;
            std::string actual = tr.transliterate(key, nullptr);
            INFO("Line " << line_no << ": key='" << key << "' expected='" << expected << "' (" << cps_col << ") got='" << actual << "'");
            CHECK(is_strictly_valid_utf8(actual));
            CHECK_FALSE(contains_forbidden_assamese_cp(actual));
            CHECK(abk::utf8::normalize_nfc(actual) == actual);
            CHECK(actual == expected);
        }
    }

    CHECK(total_clusters >= 240);
    CHECK(total_key_variants >= 270);
}

TEST_CASE("Avro Style Spec: Property Tests (UTF-8, Zero U+09F0/U+09F1, NFC, Determinism)", "[avro][properties]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");
    abk::Dictionary dict;
    dict.load_from_file("data/bangla_wordlist.txt");

    std::mt19937 rng(20261026);
    const std::string alphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`.,:^$|;?!()[]{} /\\+-*%";
    const uint8_t corrupt_bytes[] = {0x80, 0xBF, 0xC0, 0xC1, 0xE0, 0xED, 0xF5, 0xFF};

    std::uniform_int_distribution<size_t> alpha_dist(0, alphabet.size() - 1);
    std::uniform_int_distribution<size_t> corrupt_dist(0, sizeof(corrupt_bytes) - 1);
    std::uniform_int_distribution<int> len_dist(1, 80);

    for (int iter = 0; iter < 2000; ++iter) {
        int len = (iter % 50 == 0) ? 450 : len_dist(rng); // Also test >256-byte chunked inputs
        std::string input;
        input.reserve(static_cast<size_t>(len));
        for (int j = 0; j < len; ++j) {
            if (iter % 15 == 0 && j % 11 == 0) {
                input.push_back(static_cast<char>(corrupt_bytes[corrupt_dist(rng)]));
            } else {
                input.push_back(alphabet[alpha_dist(rng)]);
            }
        }

        auto beam1 = tr.transliterate_beam(input, (iter % 2 == 0) ? &dict : nullptr, 16);
        auto beam2 = tr.transliterate_beam(input, (iter % 2 == 0) ? &dict : nullptr, 16);

        // Determinism: identical call produces identical beam order and scores
        REQUIRE(beam1.size() == beam2.size());
        for (size_t k = 0; k < beam1.size(); ++k) {
            CHECK(beam1[k].first == beam2[k].first);
            CHECK(beam1[k].second == Approx(beam2[k].second));
            CHECK(is_strictly_valid_utf8(beam1[k].first));
            CHECK_FALSE(contains_forbidden_assamese_cp(beam1[k].first));
            CHECK(abk::utf8::normalize_nfc(beam1[k].first) == beam1[k].first);
        }

        std::string out = tr.transliterate(input, (iter % 2 == 0) ? &dict : nullptr);
        CHECK(is_strictly_valid_utf8(out));
        CHECK_FALSE(contains_forbidden_assamese_cp(out));
        CHECK(abk::utf8::normalize_nfc(out) == out);
    }
}

TEST_CASE("Avro Style Spec: Workstream C (Section 7) — Autocorrect Table, Control-Key Non-Override & First-Backspace Revert", "[avro][autocorrect]") {
    abk::EngineConfig cfg;
    cfg.dictionary_file_path = "data/bangla_wordlist.txt";
    cfg.enable_autocorrect = true;
    abk::AmarBanglaKothaEngine engine(cfg);

    SECTION("Clean-room default entries load from data/autocorrect_default.tsv") {
        CHECK(engine.get_autocorrect_table().size() >= 60);

        // (a) Conjunct-rule exceptions without needing backtick
        auto r_amra = engine.process("amra");
        REQUIRE(!r_amra.candidates.empty());
        CHECK(r_amra.candidates[0].word == abk::utf8::normalize_nfc("আমরা"));
        CHECK(r_amra.candidates[0].type == abk::CandidateType::AUTOCORRECT);
        // Classic phonetic আম্রা remains #2 in candidate list
        REQUIRE(r_amra.candidates.size() >= 2);
        CHECK(r_amra.candidates[1].word == abk::utf8::normalize_nfc("আম্রা"));

        auto r_ekta = engine.process("ekta");
        REQUIRE(!r_ekta.candidates.empty());
        CHECK(r_ekta.candidates[0].word == abk::utf8::normalize_nfc("একটা"));

        auto r_aglano = engine.process("aglano");
        REQUIRE(!r_aglano.candidates.empty());
        CHECK(r_aglano.candidates[0].word == abk::utf8::normalize_nfc("আগলানো"));

        // (b) English-spelling loanwords override generic English word bypass
        auto r_sw = engine.process("software");
        REQUIRE(!r_sw.candidates.empty());
        CHECK(r_sw.candidates[0].word == abk::utf8::normalize_nfc("সফটওয়্যার"));
        CHECK(r_sw.candidates[0].type == abk::CandidateType::AUTOCORRECT);
    }

    SECTION("Editable: add, update, delete, import/export TSV, and global enable/disable") {
        // By default "computer" bypasses as English; adding autocorrect entry overrides it
        CHECK(engine.process("computer").primary_transliteration == "computer");
        CHECK(engine.add_autocorrect_entry("computer", "কম্পিউটার"));
        auto r_comp = engine.process("computer");
        REQUIRE(!r_comp.candidates.empty());
        CHECK(r_comp.candidates[0].word == abk::utf8::normalize_nfc("কম্পিউটার"));

        // Update existing entry
        CHECK(engine.add_autocorrect_entry("computer", "কম্পিউটার-যন্ত্র"));
        CHECK(engine.process("computer").candidates[0].word == abk::utf8::normalize_nfc("কম্পিউটার-যন্ত্র"));

        // Export & import TSV round-trip
        const std::string tmp_tsv = "/tmp/abk_test_autocorrect_io.tsv";
        std::remove(tmp_tsv.c_str());
        REQUIRE(engine.export_autocorrect_table(tmp_tsv));

        abk::AutocorrectTable imported;
        imported.clear();
        REQUIRE(imported.size() == 0);
        REQUIRE(imported.import_tsv(tmp_tsv));
        CHECK(imported.size() >= 60);
        CHECK(imported.lookup("computer") == abk::utf8::normalize_nfc("কম্পিউটার-যন্ত্র"));
        std::remove(tmp_tsv.c_str());

        // Delete entry
        CHECK(engine.remove_autocorrect_entry("computer"));
        CHECK_FALSE(engine.remove_autocorrect_entry("computer"));
        CHECK(engine.process("computer").primary_transliteration == "computer");

        // Global disable / re-enable
        engine.set_autocorrect_enabled(false);
        CHECK_FALSE(engine.is_autocorrect_enabled());
        auto r_disabled = engine.process("amra");
        REQUIRE(!r_disabled.candidates.empty());
        CHECK(r_disabled.candidates[0].word == abk::utf8::normalize_nfc("আম্রা"));

        engine.set_autocorrect_enabled(true);
        CHECK(engine.is_autocorrect_enabled());
        CHECK(engine.process("amra").candidates[0].word == abk::utf8::normalize_nfc("আমরা"));
    }

    SECTION("Autocorrect NEVER overrides explicit user control keys (uppercase, backtick, double-comma)") {
        CHECK_FALSE(abk::AutocorrectTable::has_explicit_control_keys("amra"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("amRa"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("Software"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("am`ra"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("am,,ra"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("kOI"));
        CHECK(abk::AutocorrectTable::has_explicit_control_keys("aZromeTik"));

        // Even if someone manually inserts an entry containing control keys, lookup/engine refuses to fire it
        engine.add_autocorrect_entry("amRa", "আমরা");
        engine.add_autocorrect_entry("am`ra", "ভুল");
        engine.add_autocorrect_entry("am,,ra", "ভুল");
        CHECK_FALSE(engine.get_autocorrect_table().contains("amRa"));
        CHECK(engine.get_autocorrect_table().lookup("amRa").empty());
        CHECK(engine.get_autocorrect_table().lookup("am`ra").empty());
        CHECK(engine.get_autocorrect_table().lookup("am,,ra").empty());

        auto r_upper = engine.process("amRa");
        REQUIRE(!r_upper.candidates.empty());
        CHECK(r_upper.candidates[0].type != abk::CandidateType::AUTOCORRECT);
        CHECK(r_upper.candidates[0].word == abk::utf8::normalize_nfc("আমড়া"));

        auto r_bt = engine.process("am`ra");
        REQUIRE(!r_bt.candidates.empty());
        CHECK(r_bt.candidates[0].type != abk::CandidateType::AUTOCORRECT);
        CHECK(r_bt.candidates[0].word == abk::utf8::normalize_nfc("আমরা"));

        auto r_dc = engine.process("am,,ra");
        REQUIRE(!r_dc.candidates.empty());
        CHECK(r_dc.candidates[0].type != abk::CandidateType::AUTOCORRECT);
        CHECK(r_dc.candidates[0].word == abk::utf8::normalize_nfc("আম্রা"));
    }

    SECTION("First backspace after an autocorrect reverts to what the user typed without re-forcing autocorrect") {
        auto r1 = engine.process("amra");
        REQUIRE(!r1.candidates.empty());
        CHECK(r1.candidates[0].word == abk::utf8::normalize_nfc("আমরা"));
        CHECK(r1.candidates[0].type == abk::CandidateType::AUTOCORRECT);

        // User presses Space to commit the autocorrected top candidate
        std::string committed = engine.commit_selected_candidate(true);
        CHECK(committed == abk::utf8::normalize_nfc("আমরা "));
        CHECK(engine.can_undo_commit());

        // First Backspace immediately reverts to raw typed "amra"
        std::string reverted_raw = engine.revert_autocorrect_on_backspace();
        CHECK(reverted_raw == "amra");
        CHECK_FALSE(engine.can_undo_commit());

        // Re-processing the reverted "amra" buffer must NOT re-force autocorrect on top!
        auto r_after_revert = engine.process("amra");
        REQUIRE(!r_after_revert.candidates.empty());
        CHECK(r_after_revert.candidates[0].word == abk::utf8::normalize_nfc("আম্রা"));
        CHECK(r_after_revert.candidates[0].type != abk::CandidateType::AUTOCORRECT);
        CHECK(candidates_contain(r_after_revert.candidates, "আমরা"));

        // Once the user edits/moves to another word, normal autocorrect resumes
        engine.process("ekta");
        auto r_resumed = engine.process("amra");
        CHECK(r_resumed.candidates[0].word == abk::utf8::normalize_nfc("আমরা"));
    }
}

TEST_CASE("Avro Style Spec: Workstream C (Section 8) — Suggestion Modes, Skeleton Lookup, Navigation, Remember Choice & Pin", "[avro][suggestions]") {
    abk::EngineConfig cfg;
    cfg.dictionary_file_path = "data/bangla_wordlist.txt";
    cfg.enable_autocorrect = true;
    abk::AmarBanglaKothaEngine engine(cfg);

    SECTION("Collapsed Bengali skeleton lookup resolves ত/ট/ৎ, শ/ষ/স, ই/ঈ, উ/ঊ, ন/ণ, র/ড়/ঢ় inconsistencies") {
        const auto& dict = engine.get_dictionary();
        CHECK(abk::Dictionary::compute_skeleton("বাড়ি") == abk::Dictionary::compute_skeleton("বারি"));
        CHECK(abk::Dictionary::compute_skeleton("কারণ") == abk::Dictionary::compute_skeleton("কারন"));
        CHECK(abk::Dictionary::compute_skeleton("মানুষ") == abk::Dictionary::compute_skeleton("মানুস"));
        CHECK(abk::Dictionary::compute_skeleton("নদী") == abk::Dictionary::compute_skeleton("নদি"));
        CHECK(abk::Dictionary::compute_skeleton("উৎসব") == abk::Dictionary::compute_skeleton("উতসব"));
        CHECK(!dict.lookup_by_skeleton("বারি", 5).empty());

        CHECK(candidates_contain(engine.process("bari").candidates, "বাড়ি"));
        CHECK(candidates_contain(engine.process("karon").candidates, "কারণ"));
        CHECK(candidates_contain(engine.process("manus").candidates, "মানুষ"));
        CHECK(candidates_contain(engine.process("nodi").candidates, "নদী"));
        CHECK(candidates_contain(engine.process("utsob").candidates, "উৎসব"));
    }

    SECTION("Suggestion modes: Character-first (default), Dictionary-first, and Classic phonetic (no window)") {
        CHECK(engine.get_suggestion_mode() == abk::SuggestionMode::CHARACTER_FIRST);

        // 1. Character-first: classic phonetic বারি comes before dictionary/skeleton বাড়ি
        auto char_res = engine.process("bari");
        REQUIRE(!char_res.candidates.empty());
        CHECK(engine.get_suggestion_state().visible);
        CHECK(char_res.candidates[0].word == abk::utf8::normalize_nfc("বারি"));
        CHECK(candidates_contain(char_res.candidates, "বাড়ি"));

        // 2. Dictionary-first: higher-frequency dictionary word মানুষ / বাড়ি ranks #1
        engine.set_suggestion_mode(abk::SuggestionMode::DICTIONARY_FIRST);
        CHECK(engine.get_suggestion_mode() == abk::SuggestionMode::DICTIONARY_FIRST);
        auto dict_res = engine.process("manus");
        REQUIRE(!dict_res.candidates.empty());
        CHECK(engine.get_suggestion_state().visible);
        CHECK(dict_res.candidates[0].word == abk::utf8::normalize_nfc("মানুষ"));

        // Explicit uppercase keys (kOI -> কৈ, bOI -> বৈ) STILL stay #1 even in Dictionary-first mode
        CHECK(engine.process("kOI").candidates[0].word == abk::utf8::normalize_nfc("কৈ"));
        CHECK(engine.process("bOI").candidates[0].word == abk::utf8::normalize_nfc("বৈ"));

        // 3. Classic phonetic with no suggestion window
        engine.set_suggestion_mode(abk::SuggestionMode::CLASSIC_PHONETIC);
        CHECK(engine.get_suggestion_mode() == abk::SuggestionMode::CLASSIC_PHONETIC);
        auto classic_res = engine.process("amra");
        CHECK_FALSE(engine.get_suggestion_state().visible);
        REQUIRE(classic_res.candidates.size() == 1);
        CHECK(classic_res.candidates[0].word == abk::utf8::normalize_nfc("আম্রা"));

        engine.set_suggestion_mode(abk::SuggestionMode::CHARACTER_FIRST);
    }

    SECTION("Navigation: Down arrow, optional Tab setting, 1-9 direct select, Space/Enter commit, Esc close") {
        auto res = engine.process("bari");
        REQUIRE(res.candidates.size() >= 2);
        CHECK(engine.get_suggestion_state().selected_index == 0);
        CHECK(engine.get_suggestion_state().visible);

        // Down arrow advances selection
        CHECK(engine.select_next_candidate());
        CHECK(engine.get_suggestion_state().selected_index == 1);
        // Tab advances selection when tab_browses_candidates is enabled (default)
        CHECK(engine.is_tab_browses_candidates());
        CHECK(engine.browse_tab());

        // Optional setting: disable Tab browsing for spreadsheets/tables
        engine.set_tab_browses_candidates(false);
        CHECK_FALSE(engine.is_tab_browses_candidates());
        int cur_idx = engine.get_suggestion_state().selected_index;
        CHECK_FALSE(engine.browse_tab());
        CHECK(engine.get_suggestion_state().selected_index == cur_idx);
        engine.set_tab_browses_candidates(true);

        // Number key direct selection (0-based index)
        CHECK(engine.select_candidate(1));
        CHECK(engine.get_suggestion_state().selected_index == 1);

        // Esc closes the suggestion window without destroying primary transliteration
        CHECK(engine.close_suggestion_window());
        CHECK_FALSE(engine.get_suggestion_state().visible);
    }

    SECTION("Remember my choice: non-default pick ranks #1 next time, size-capped, and resettable") {
        engine.reset_remembered_choices();
        auto r1 = engine.process("bari");
        REQUIRE(r1.candidates.size() >= 2);
        CHECK(r1.candidates[0].word == abk::utf8::normalize_nfc("বারি"));

        // Find বাড়ি at a non-default index and commit it
        int bari_idx = -1;
        for (size_t i = 0; i < r1.candidates.size(); ++i) {
            if (r1.candidates[i].word == abk::utf8::normalize_nfc("বাড়ি")) {
                bari_idx = static_cast<int>(i);
                break;
            }
        }
        REQUIRE(bari_idx > 0);
        CHECK(engine.select_candidate(bari_idx));
        std::string committed = engine.commit_selected_candidate(false);
        CHECK(committed == abk::utf8::normalize_nfc("বাড়ি"));

        // Clear last_committed_word to isolate single-word ranking: বাড়ি must now be #1!
        engine.set_last_committed_word("");
        auto r2 = engine.process("bari");
        REQUIRE(!r2.candidates.empty());
        CHECK(r2.candidates[0].word == abk::utf8::normalize_nfc("বাড়ি"));
        CHECK(r2.candidates[0].type == abk::CandidateType::REMEMBERED_CHOICE);

        // Resettable
        engine.reset_remembered_choices();
        auto r3 = engine.process("bari");
        CHECK(r3.candidates[0].word == abk::utf8::normalize_nfc("বারি"));

        // Size-cap FIFO eviction check on PersonalDictionary
        abk::PersonalDictionary pdict(100);
        pdict.set_max_remembered_choices(3);
        pdict.remember_choice("k1", "ক১");
        pdict.remember_choice("k2", "ক২");
        pdict.remember_choice("k3", "ক৩");
        CHECK(pdict.remembered_choice_count() == 3);
        pdict.remember_choice("k4", "ক৪");
        CHECK(pdict.remembered_choice_count() == 3);
        CHECK_FALSE(pdict.has_remembered_choice("k1"));
        CHECK(pdict.get_remembered_choice("k4") == abk::utf8::normalize_nfc("ক৪"));
    }

    SECTION("Caret follow vs pinned window coordinates and C ABI round-trip") {
        CHECK_FALSE(engine.is_window_pinned());
        engine.set_window_pinned(true, 320, 480);
        CHECK(engine.is_window_pinned());
        auto pos = engine.get_pinned_position();
        CHECK(pos.first == 320);
        CHECK(pos.second == 480);
        engine.process("ami");
        CHECK(engine.get_suggestion_state().pinned);
        CHECK(engine.get_suggestion_state().pinned_x == 320);
        CHECK(engine.get_suggestion_state().pinned_y == 480);

        // C ABI verification
        abk_engine_t c_eng = abk_engine_create();
        REQUIRE(c_eng != nullptr);
        abk_load_dictionary(c_eng, "data/bangla_wordlist.txt");

        CHECK(abk_engine_get_suggestion_mode(c_eng) == 0);
        abk_engine_set_suggestion_mode(c_eng, 1);
        CHECK(abk_engine_get_suggestion_mode(c_eng) == 1);
        abk_engine_set_suggestion_mode(c_eng, 0);

        abk_engine_set_window_pinned(c_eng, 1, 150, 250);
        int c_x = 0, c_y = 0;
        CHECK(abk_engine_is_window_pinned(c_eng) == 1);
        CHECK(abk_engine_get_pinned_position(c_eng, &c_x, &c_y) == 1);
        CHECK(c_x == 150);
        CHECK(c_y == 250);

        CHECK(abk_autocorrect_add(c_eng, "laptop", "ল্যাপটপ") == 1);
        abk_suggestion_list_t* s = abk_get_suggestions(c_eng, "laptop");
        REQUIRE(s != nullptr);
        REQUIRE(s->candidate_count >= 1);
        CHECK(std::string(s->candidates[0].word) == abk::utf8::normalize_nfc("ল্যাপটপ"));
        abk_free_suggestions(s);

        abk_engine_record_commit(c_eng, "laptop", "ল্যাপটপ ", 1);
        char rev_buf[128];
        REQUIRE(abk_engine_revert_autocorrect(c_eng, rev_buf, sizeof(rev_buf)) > 0);
        CHECK(std::string(rev_buf) == "laptop");

        abk_engine_destroy(c_eng);
    }
}

TEST_CASE("Avro Style Spec: Workstream D — Punctuation, File Path, Abbreviation, Taka '$' & Digit Guards", "[avro][workstream-d]") {
    abk::Transliterator tr("data/keymap_phonetic.toml");

    SECTION("Dot '.' -> Dari '।' vs decimals, URLs, emails, file paths, and abbreviations") {
        CHECK(tr.transliterate(".") == "।");
        CHECK(tr.transliterate(".`") == ".");
        CHECK(tr.transliterate("ami bhalo achi.") == abk::utf8::normalize_nfc("আমি ভাল আছি।"));

        // Decimals
        CHECK(tr.transliterate("3.5") == "৩.৫");
        CHECK(tr.transliterate("0.05") == "০.০৫");
        CHECK(tr.transliterate("dam 12.50 taka.") == abk::utf8::normalize_nfc("দাম ১২.৫০ তাকা।"));

        // URLs & Emails (standalone and mid-sentence)
        CHECK(tr.transliterate("https://bangladesh.gov.bd/index.html") == "https://bangladesh.gov.bd/index.html");
        CHECK(tr.transliterate("www.prothomalo.com") == "www.prothomalo.com");
        CHECK(tr.transliterate("contact.us@domain.org.bd") == "contact.us@domain.org.bd");
        CHECK(tr.transliterate("dekho https://gov.bd ekhon.") == abk::utf8::normalize_nfc("দেখ https://gov.bd এখন।"));

        // File paths & code filenames
        CHECK(tr.transliterate("/usr/local/bin/abk") == "/usr/local/bin/abk");
        CHECK(tr.transliterate("~/Desktop/notes.txt") == "~/Desktop/notes.txt");
        CHECK(tr.transliterate("./scripts/run.sh") == "./scripts/run.sh");
        CHECK(tr.transliterate("../config/keymap.toml") == "../config/keymap.toml");
        CHECK(tr.transliterate("C:\\Windows\\System32\\cmd.exe") == "C:\\Windows\\System32\\cmd.exe");
        CHECK(tr.transliterate("main.cpp") == "main.cpp");

        // Abbreviations (standalone and mid-sentence)
        CHECK(tr.transliterate("Dr.") == "Dr.");
        CHECK(tr.transliterate("Mr.") == "Mr.");
        CHECK(tr.transliterate("Mrs.") == "Mrs.");
        CHECK(tr.transliterate("Prof.") == "Prof.");
        CHECK(tr.transliterate("e.g.") == "e.g.");
        CHECK(tr.transliterate("i.e.") == "i.e.");
        CHECK(tr.transliterate("etc.") == "etc.");
        CHECK(tr.transliterate("vs.") == "vs.");
        CHECK(tr.transliterate("Ph.D.") == "Ph.D.");
        CHECK(tr.transliterate("U.S.A.") == "U.S.A.");
        CHECK(tr.transliterate("Dr. korim ekhon asen.") == abk::utf8::normalize_nfc("Dr. করিম এখন আসেন।"));
    }

    SECTION("Dollar '$' -> Taka '৳' ONLY when intended (never inside shell/code variables)") {
        CHECK(tr.is_taka_sign_enabled());
        CHECK(tr.transliterate("$") == "৳");
        CHECK(tr.transliterate("$500") == "৳৫০০");
        CHECK(tr.transliterate("$3.50") == "৳৩.৫০");
        CHECK(tr.transliterate("500$") == "৫০০৳");
        CHECK(tr.transliterate("$ 100") == "৳ ১০০");
        CHECK(tr.transliterate("$`500") == "$৫০০");

        // Non-currency shell/code tokens stay literal '$'
        CHECK(tr.transliterate("$PATH") == "$PATH");
        CHECK(tr.transliterate("${HOME}") == "${HOME}");
        CHECK(tr.transliterate("$(pwd)") == "$(pwd)");
        CHECK(tr.transliterate("$my_var") == "$my_var");
        CHECK(tr.transliterate("$_private") == "$_private");
        CHECK(tr.transliterate("$$") == "$$");
        CHECK(tr.transliterate("$?") == "$?");
        CHECK(tr.transliterate("foo$bar") == "foo$bar");
        CHECK(tr.transliterate("echo $HOME ekhon.") == abk::utf8::normalize_nfc("এছ $HOME এখন।"));

        // Global Taka toggle
        tr.set_taka_sign(false);
        CHECK_FALSE(tr.is_taka_sign_enabled());
        CHECK(tr.transliterate("$500") == "$৫০০");
        tr.set_taka_sign(true);
    }

    SECTION("Digit mapping setting (0-9 <-> ০-৯) interacts cleanly with decimals and Taka") {
        tr.set_bangla_digits(false);
        CHECK(tr.transliterate("0123456789") == "0123456789");
        CHECK(tr.transliterate("3.5") == "3.5");
        CHECK(tr.transliterate("$500") == "৳500");
        tr.set_bangla_digits(true);
        CHECK(tr.transliterate("0123456789") == "০১২৩৪৫৬৭৮৯");
        CHECK(tr.transliterate("3.5") == "৩.৫");
        CHECK(tr.transliterate("$500") == "৳৫০০");
    }
}

TEST_CASE("Avro Style Spec: Workstream E — Bilingual Docs & Provenance Verification", "[avro][docs]") {
    std::ifstream keymap_doc("docs/KEYMAP.md");
    REQUIRE(keymap_doc.is_open());
    std::stringstream kss;
    kss << keymap_doc.rdbuf();
    std::string ktext = kss.str();
    CHECK(ktext.find("Amar Bangla Kotha") != std::string::npos);
    CHECK(ktext.find("আমার বাংলা কথা") != std::string::npos);
    CHECK(ktext.find("koi") != std::string::npos);
    CHECK(ktext.find("kOI") != std::string::npos);
    CHECK(ktext.find("t``") != std::string::npos);
    CHECK_FALSE(contains_forbidden_assamese_cp(ktext));

    std::ifstream prov_doc("docs/PROVENANCE.md");
    REQUIRE(prov_doc.is_open());
    std::stringstream pss;
    pss << prov_doc.rdbuf();
    std::string ptext = pss.str();
    CHECK(ptext.find("data/keymap_phonetic.toml") != std::string::npos);
    CHECK(ptext.find("data/autocorrect_default.tsv") != std::string::npos);
    CHECK(ptext.find("docs/KEYMAP.md") != std::string::npos);
    CHECK(ptext.find("NEEDS NATIVE REVIEW") != std::string::npos);
}
