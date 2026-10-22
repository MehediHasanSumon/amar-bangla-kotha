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
    START,            // At word start
    AFTER_CONSONANT,  // Immediately following a consonant
    AFTER_VOWEL,      // Following a vowel or word start
    END               // At word end
};

enum class OutputCategory {
    CONSONANT,
    INDEPENDENT_VOWEL,
    KAR,
    CONJUNCT,
    MODIFIER,
    DIPHTHONG_SPLIT,  // Inherent 'o' + independent vowel (e.g. "koi" -> 'ক' + 'ই')
    PUNCTUATION,
    DIGIT,
    OTHER
};

struct PhoneticRule {
    std::string pattern;
    std::string output;
    RuleContext context = RuleContext::ANY;
    OutputCategory category = OutputCategory::OTHER;
    double priority = 100.0;
    std::string description;
};

struct LatticeEdge {
    size_t start_pos;
    size_t end_pos;
    std::string bangla;
    RuleContext context;
    OutputCategory category;
    double priority;
};

class Transliterator {
public:
    Transliterator();
    explicit Transliterator(const std::string& rules_filepath);
    ~Transliterator() = default;

    // Load data-driven rules from external file (TSV format)
    bool load_rules_from_file(const std::string& filepath);

    // Primary transliteration (backward compatible, powered by beam search)
    std::string transliterate(const std::string& input, const Dictionary* dict = nullptr) const;

    // Full beam search returning top-N scored hypotheses (text, score)
    std::vector<std::pair<std::string, double>> transliterate_beam(
        const std::string& input,
        const Dictionary* dict = nullptr,
        size_t beam_width = 24
    ) const;

    // Generate possible phonetic alternatives
    std::vector<std::string> get_phonetic_variants(const std::string& input, size_t max_variants = 4) const;

    // Count of loaded rules
    size_t rule_count() const { return rules.size(); }

private:
    std::vector<PhoneticRule> rules;

    void init_default_rules();
    void parse_rule_line(const std::string& line);
    std::vector<LatticeEdge> find_matching_edges(const std::string& input, size_t pos) const;

    static RuleContext parse_context(const std::string& str);
    static OutputCategory parse_category(const std::string& str);
};

} // namespace abk
