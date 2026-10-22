#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace abk {
namespace utf8 {

// Convert UTF-8 encoded std::string to std::vector of char32_t code points
std::vector<char32_t> to_codepoints(const std::string& utf8_str);

// Convert std::vector of char32_t code points to UTF-8 encoded std::string
std::string to_utf8(const std::vector<char32_t>& codepoints);

// Convert single char32_t code point to UTF-8 string
std::string to_utf8(char32_t cp);

// Unicode character classifications
bool is_bengali(char32_t cp);
bool is_bengali_vowel(char32_t cp);
bool is_bengali_kar(char32_t cp);
bool is_bengali_consonant(char32_t cp);
bool is_bengali_virama(char32_t cp);
bool is_bengali_modifier(char32_t cp);
bool is_bengali_digit(char32_t cp);
bool is_zwj(char32_t cp);
bool is_zwnj(char32_t cp);

// Converts independent vowel to corresponding kar sign (e.g. আ -> া, ই -> ি)
char32_t vowel_to_kar(char32_t vowel_cp);

// Converts kar sign to corresponding independent vowel (e.g. া -> আ, ি -> ই)
char32_t kar_to_vowel(char32_t kar_cp);

// String utilities
size_t codepoint_length(const std::string& utf8_str);
std::string to_lower_ascii(const std::string& str);
std::string trim(const std::string& str);

// Phase 3: Unicode Normalization (NFC Enforcement)
// Normalizes Bengali strings into canonical NFC form (UAX #15):
// - Composes two-part vowel signs (e.g. \u09C7 + \u09BE -> \u09CB ো)
// - Canonically decomposes Nukta (ড় -> ড+়, ঢ় -> ঢ+়, য় -> য+়) per Unicode NFC spec
// - Canonical Combining Class (CCC) sorting for modifiers & nukta
std::string normalize_nfc(const std::string& utf8_str);

// Phase 3: Grapheme Cluster Handling (UAX #29)
// Returns list of user-perceived grapheme clusters
std::vector<std::string> get_grapheme_clusters(const std::string& utf8_str);
size_t grapheme_count(const std::string& utf8_str);

// Backspace deletion of logical units:
// If trailing character is a Kar (ি, ে, ৈ, ো etc.) or modifier, deletes ONLY that Kar/modifier,
// leaving the base consonant / conjunct intact!
std::string pop_logical_unit(const std::string& utf8_str);

// Backspace deletion of complete grapheme cluster (UAX #29):
// Deletes entire user-perceived visual cluster (e.g. কি -> "", ক্ত -> "")
std::string pop_grapheme_cluster(const std::string& utf8_str);

// Bengali Unicode constants
constexpr char32_t BN_VIRAMA       = 0x09CD; // ্ (Hasant)
constexpr char32_t BN_KHANDA_TA    = 0x09CE; // ৎ
constexpr char32_t BN_ANUSVARA     = 0x0982; // ং
constexpr char32_t BN_VISARGA      = 0x0983; // ঃ
constexpr char32_t BN_CHANDRABINDU = 0x0981; // ঁ
constexpr char32_t BN_NUKTA        = 0x09BC; // ়
constexpr char32_t BN_RA           = 0x09B0; // র
constexpr char32_t BN_DDA          = 0x09A1; // ড
constexpr char32_t BN_DDHA         = 0x09A2; // ঢ
constexpr char32_t BN_YA           = 0x09AF; // য
constexpr char32_t BN_RRA          = 0x09DC; // ড়
constexpr char32_t BN_RHA          = 0x09DD; // ঢ়
constexpr char32_t BN_YAA          = 0x09DF; // য়
constexpr char32_t ZWJ             = 0x200D; // Zero Width Joiner
constexpr char32_t ZWNJ            = 0x200C; // Zero Width Non-Joiner

// Two-part vowel signs
constexpr char32_t BN_KAR_E        = 0x09C7; // ে
constexpr char32_t BN_KAR_AA       = 0x09BE; // া
constexpr char32_t BN_KAR_O        = 0x09CB; // ো
constexpr char32_t BN_KAR_AU       = 0x09CC; // ৌ
constexpr char32_t BN_AU_LENGTH    = 0x09D7; // ৗ

} // namespace utf8
} // namespace abk
