#include <catch2/catch.hpp>
#include "abk/legacy_converter.h"
#include "abk/abk.h"
#include "abk/utf8_util.h"
#include <fstream>
#include <sstream>
#include <vector>

struct LegacyVector {
    std::string ansi;
    std::string expected_unicode;
    std::string category;
};

static std::vector<LegacyVector> load_legacy_vectors(const std::string& filepath) {
    std::vector<LegacyVector> vectors;
    std::ifstream file(filepath);
    if (!file.is_open()) return vectors;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string ansi, expected, cat;
        if (std::getline(ss, ansi, '\t') && std::getline(ss, expected, '\t') && std::getline(ss, cat, '\t')) {
            vectors.push_back({ansi, expected, cat});
        }
    }
    return vectors;
}

TEST_CASE("Legacy Converter: Hand-Verified Vectors", "[legacy_converter]") {
    auto vectors = load_legacy_vectors("tests/data/legacy_vectors.tsv");
    REQUIRE(!vectors.empty());

    size_t passed = 0;
    for (const auto& vec : vectors) {
        std::string converted = abk::LegacyConverter::convert_ansi_to_unicode(vec.ansi);
        std::string expected_norm = abk::utf8::normalize_nfc(vec.expected_unicode);
        std::string converted_norm = abk::utf8::normalize_nfc(converted);

        INFO("Category: " << vec.category);
        INFO("ANSI Input: " << vec.ansi);
        INFO("Expected:   " << expected_norm);
        INFO("Converted:  " << converted_norm);

        CHECK(converted_norm == expected_norm);
        if (converted_norm == expected_norm) passed++;
    }
    CHECK(passed == vectors.size());
}

TEST_CASE("Legacy Converter: Bidirectional Round-Trip", "[legacy_converter]") {
    std::string unicode_sample = "আমার সোনার বাংলা, আমি তোমায় ভালোবাসি।";
    std::string ansi = abk::LegacyConverter::convert_unicode_to_ansi(unicode_sample);
    REQUIRE(!ansi.empty());

    std::string roundtrip = abk::LegacyConverter::convert_ansi_to_unicode(ansi);
    CHECK(abk::utf8::normalize_nfc(roundtrip) == abk::utf8::normalize_nfc(unicode_sample));
}

TEST_CASE("Legacy Converter: Detection Heuristic", "[legacy_converter]") {
    // Legacy Bijoy ANSI texts -> should detect as true
    CHECK(abk::LegacyConverter::is_likely_legacy_ansi("Avgvi †mvbvi evsjv"));
    CHECK(abk::LegacyConverter::is_likely_legacy_ansi("Dfq cv‡k av‡bi kx‡l"));
    CHECK(abk::LegacyConverter::is_likely_legacy_ansi("e¨e¯’v"));

    // Modern Unicode Bengali -> should detect as false (already Unicode)
    CHECK_FALSE(abk::LegacyConverter::is_likely_legacy_ansi("আমার সোনার বাংলা"));
    CHECK_FALSE(abk::LegacyConverter::is_likely_legacy_ansi("বাংলাদেশ"));

    // Plain English -> should detect as false
    CHECK_FALSE(abk::LegacyConverter::is_likely_legacy_ansi("The quick brown fox jumps over the lazy dog"));
    CHECK_FALSE(abk::LegacyConverter::is_likely_legacy_ansi("Welcome to Bangladesh!"));

    // Empty string
    CHECK_FALSE(abk::LegacyConverter::is_likely_legacy_ansi(""));
}

TEST_CASE("Legacy Converter: C API Integration", "[legacy_converter]") {
    const char* ansi = "Avgvi †mvbvi evsjv";
    char out_buf[256];
    int len = abk_ansi_to_unicode(ansi, out_buf, sizeof(out_buf));
    REQUIRE(len > 0);
    CHECK(std::string(out_buf) == "আমার সোনার বাংলা");

    // Detection via C API
    CHECK(abk_is_likely_legacy_ansi(ansi) == true);
    CHECK(abk_is_likely_legacy_ansi("আমার সোনার বাংলা") == false);

    // Unicode to ANSI
    char ansi_buf[256];
    int ansi_len = abk_unicode_to_ansi("আমার সোনার বাংলা", ansi_buf, sizeof(ansi_buf));
    REQUIRE(ansi_len > 0);
    CHECK(std::string(ansi_buf).find("Avgvi") != std::string::npos);
}
