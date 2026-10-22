#pragma once

#include "abk/types.h"
#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/autocorrect.h"
#include <string>
#include <vector>
#include <memory>

namespace abk {

class AmarBanglaKothaEngine {
public:
    explicit AmarBanglaKothaEngine(const EngineConfig& config = EngineConfig());
    ~AmarBanglaKothaEngine() = default;

    // Full processing pipeline: transliterate, suggest, prefix complete & autocorrect
    SuggestionResult process(const std::string& latin_input);

    // Fast transliteration only (no suggestion overhead)
    std::string transliterate_fast(const std::string& latin_input) const;

    // Word learning: add a word to dynamic memory and user dictionary
    void learn_word(const std::string& bangla_word, uint32_t frequency = 100);

    // Load external or user dictionary file
    bool load_dictionary_file(const std::string& filepath);
    bool load_user_dictionary_file(const std::string& filepath);

    // Engine accessors
    const EngineConfig& get_config() const { return config; }
    void set_config(const EngineConfig& cfg) { config = cfg; }

    const Dictionary& get_dictionary() const { return dictionary; }
    Dictionary& get_dictionary() { return dictionary; }

    const Transliterator& get_transliterator() const { return transliterator; }
    const Autocorrect& get_autocorrect() const { return autocorrect; }

private:
    EngineConfig config;
    Transliterator transliterator;
    Dictionary dictionary;
    Autocorrect autocorrect;

    void add_candidate_unique(std::vector<Candidate>& list, const Candidate& candidate);
};

} // namespace abk
