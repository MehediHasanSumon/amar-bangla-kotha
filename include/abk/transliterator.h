#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>

namespace abk {

class Dictionary;

enum class RuleContext {
    ANY,
    START,                    // At word start (or after whitespace)
    AFTER_CONSONANT,          // Immediately following a base consonant (before any vowel/kar)
    AFTER_CONSONANT_LIKE,     // Following a base consonant, Y/nukta consonant, or phala
    AFTER_VOWEL,              // Immediately following an independent vowel, kar, or inherent 'o'
    AFTER_VOWEL_OR_SEPARATOR, // Following a vowel, separator (`), modifier, or non-consonant
    START_OR_SEPARATOR,       // At word start, after separator (`), or after modifier/other
    NOT_AFTER_CONSONANT,      // Anywhere except after a consonant-like unit
    AFTER_REPH,               // Immediately following Reph (র্)
    END                       // At word end (legacy TSV compatibility)
};

enum class RightContext {
    ANY,
    BEFORE_CONSONANT,         // Next input position matches a consonant
    END                       // At end of token or before whitespace
};

enum class OutputCategory {
    CONSONANT,          // Base consonant eligible for C1 + ্ + C2 conjuncts
    Y_CONSONANT,        // Nukta/semivowel consonant (ড়, ঢ়, য়, ইয়, জ়): takes kars, no C1+্+C2
    INDEPENDENT_VOWEL,  // Full vowel letter (অ, আ, ই, ঈ, উ, ঊ, ঋ, এ, ঐ, ও, ঔ)
    INHERENT_VOWEL,     // Inherent 'o' after consonant (outputs nothing, enters vowel state)
    KAR,                // Dependent vowel sign (া, ি, ী, ু, ূ, ৃ, ে, ৈ, ো, ৌ)
    VOWEL_YA,           // Vowel-following য়া or অ্যা sequence
    PHALA,              // Post-consonantal phala (্য, ্ব, র‍্য)
    REPH,               // Pre-consonantal Reph (র্)
    HASANTA,            // Explicit virama (,, -> ্)
    SEPARATOR,          // Explicit backtick separator (` -> "")
    CONJUNCT,           // Pre-built conjunct (legacy TSV compatibility)
    MODIFIER,           // ং, ঃ, ঁ, ৎ
    DIPHTHONG_SPLIT,    // Legacy TSV compatibility
    PUNCTUATION,
    DIGIT,
    OTHER
};

enum class PipeMode {
    PASSTHROUGH,        // '|' passes through as '|'
    LITERAL_DOT,        // '|' outputs '.' (Avro "Use pipe for dot" option)
    BENGALI_DARI        // '|' outputs '।'
};

struct TransliteratorConfig {
    bool enable_bangla_digits = true;  // 0-9 -> ০-৯
    bool enable_nukta_ja = true;       // Shift+J ('J') -> জ়
    bool enable_taka_sign = true;      // '$' -> ৳
    PipeMode pipe_mode = PipeMode::PASSTHROUGH;
};

struct PhoneticRule {
    std::string pattern;
    std::string output;
    RuleContext context = RuleContext::ANY;
    RightContext right_context = RightContext::ANY;
    OutputCategory category = OutputCategory::OTHER;
    double priority = 100.0;
    bool explicit_rule = false;
    bool variant = false;
    bool overrides_explicit = false;
    std::string setting;
    std::string description;
};

struct LatticeEdge {
    size_t start_pos = 0;
    size_t end_pos = 0;
    std::string bangla;
    RuleContext context = RuleContext::ANY;
    RightContext right_context = RightContext::ANY;
    OutputCategory category = OutputCategory::OTHER;
    double priority = 100.0;
    bool explicit_rule = false;
    bool variant = false;
    bool overrides_explicit = false;
};

class Transliterator {
public:
    Transliterator();
    explicit Transliterator(const std::string& rules_filepath);
    ~Transliterator() = default;

    // Load data-driven rules from external file (.toml or .tsv)
    bool load_rules_from_file(const std::string& filepath);
    bool load_rules_from_toml(const std::string& filepath);

    // Configuration accessors
    void set_config(const TransliteratorConfig& cfg) { config = cfg; }
    const TransliteratorConfig& get_config() const { return config; }

    void set_bangla_digits(bool enabled) { config.enable_bangla_digits = enabled; }
    bool is_bangla_digits_enabled() const { return config.enable_bangla_digits; }

    void set_nukta_ja(bool enabled) { config.enable_nukta_ja = enabled; }
    bool is_nukta_ja_enabled() const { return config.enable_nukta_ja; }

    void set_taka_sign(bool enabled) { config.enable_taka_sign = enabled; }
    bool is_taka_sign_enabled() const { return config.enable_taka_sign; }

    void set_pipe_mode(PipeMode mode) { config.pipe_mode = mode; }
    PipeMode get_pipe_mode() const { return config.pipe_mode; }

    // Check whether two Bengali base consonants form an automatic C1 + ্ + C2 conjunct
    bool can_form_conjunct(char32_t c1, char32_t c2) const;

    // Primary transliteration (powered by deterministic lattice beam search)
    std::string transliterate(const std::string& input, const Dictionary* dict = nullptr) const;

    // Full beam search returning top-N scored hypotheses (text, score)
    std::vector<std::pair<std::string, double>> transliterate_beam(
        const std::string& input,
        const Dictionary* dict = nullptr,
        size_t beam_width = 24
    ) const;

    // Generate possible phonetic alternatives from the beam
    std::vector<std::string> get_phonetic_variants(const std::string& input, size_t max_variants = 4) const;

    // Count of loaded rules
    size_t rule_count() const { return rules.size(); }

private:
    std::vector<PhoneticRule> rules;
    std::unordered_map<char32_t, std::vector<char32_t>> phonotactics_map;
    TransliteratorConfig config;

    void init_default_rules();
    void init_default_phonotactics();
    void sort_rules();
    void parse_rule_line(const std::string& line);
    bool is_rule_enabled(const PhoneticRule& r) const;
    bool matches_before_consonant(const std::string& input, size_t pos) const;
    std::vector<LatticeEdge> find_matching_edges(const std::string& input, size_t pos) const;

    std::vector<std::pair<std::string, double>> transliterate_chunk_beam(
        const std::string& input,
        const Dictionary* dict,
        size_t beam_width
    ) const;

    static RuleContext parse_context(const std::string& str);
    static RightContext parse_right_context(const std::string& str);
    static OutputCategory parse_category(const std::string& str);
};

} // namespace abk
