#pragma once

#include <string>
#include <unordered_set>

namespace abk {

enum class TokenType {
    BANGLA_PHONETIC,
    URL,
    EMAIL,
    FILE_PATH,
    CODE_TOKEN,
    EXPLICIT_OVERRIDE,
    ENGLISH_WORD,
    NUMERIC_OR_SYMBOL
};

struct TokenClassification {
    TokenType type = TokenType::BANGLA_PHONETIC;
    bool should_bypass_transliteration = false;
    std::string reason;
};

class TokenClassifier {
public:
    TokenClassifier();

    TokenClassification classify(const std::string& token) const;
    bool should_bypass(const std::string& token) const;

    // Specific detector methods
    bool is_url(const std::string& token) const;
    bool is_email(const std::string& token) const;
    bool is_file_path(const std::string& token) const;
    bool is_code_token(const std::string& token) const;
    bool is_explicit_override(const std::string& token) const;
    bool is_english_word(const std::string& token) const;

private:
    std::unordered_set<std::string> english_vocabulary;
    std::unordered_set<std::string> banglish_stop_words; // Common Banglish tokens that must NEVER be classified as English
    std::unordered_set<std::string> common_file_extensions;
    std::unordered_set<std::string> common_tlds;
};

} // namespace abk
