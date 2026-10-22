#include "abk/token_classifier.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <cctype>

namespace abk {

TokenClassifier::TokenClassifier() {
    // 1. High-frequency Banglish tokens that must NEVER be falsely classified as English
    banglish_stop_words = {
        "ami", "tumi", "amar", "tomar", "apni", "apnar", "kotha", "valo", "bhalo",
        "dhonnobad", "kemon", "achho", "achhen", "shob", "desh", "bangla", "bangladesh",
        "ekhon", "kobe", "kothay", "jani", "bolun", "korun", "hobe", "hoyeche", "onek",
        "shobdo", "shundor", "bhai", "bon", "ma", "baba", "naam", "bari", "pani",
        "khabar", "kichu", "chai", "dorkar", "shobai", "amader", "shomoy", "gaan",
        "din", "raat", "boi", "koi", "chhele", "meye", "prothom", "shesh", "kaj",
        "manush", "bhasha", "bondhu", "shokol", "desher", "matri", "bhashar", "shikkha",
        "shathe", "theke", "porjonto", "shudhu", "karon", "tai", "jodi", "tobe", "ebong",
        "othoba", "kintu", "ar", "aar", "she", "tara", "ora", "era", "tader", "oder",
        "amar", "amar", "khub", "beshi", "kom", "notun", "puraton", "sobai", "sob", "baje"
    };

    // 2. High-confidence English words (frequent words without Banglish overlap)
    english_vocabulary = {
        "the", "and", "for", "with", "from", "have", "this", "that", "you", "not",
        "but", "are", "all", "can", "will", "about", "which", "get", "like", "make",
        "time", "just", "know", "take", "into", "year", "your", "good", "some", "could",
        "them", "other", "than", "then", "now", "look", "only", "come", "its", "over",
        "think", "also", "back", "after", "use", "how", "our", "work", "first", "well",
        "way", "even", "new", "want", "because", "any", "these", "give", "day", "most",
        "people", "see", "two", "need", "should", "please", "thanks", "welcome",
        "software", "hardware", "project", "system", "file", "version", "test", "build",
        "computer", "network", "internet", "browser", "windows", "linux", "macos",
        "google", "microsoft", "apple", "github", "terminal", "server", "client",
        "database", "function", "variable", "string", "number", "boolean", "object",
        "class", "method", "public", "private", "return", "import", "include", "export",
        "create", "delete", "update", "select", "insert", "where", "while", "break"
    };

    // 3. Common technical and document file extensions
    common_file_extensions = {
        "cpp", "h", "c", "hpp", "py", "js", "ts", "jsx", "tsx", "html", "css", "scss",
        "json", "xml", "yaml", "yml", "md", "txt", "pdf", "doc", "docx", "xls", "xlsx",
        "png", "jpg", "jpeg", "gif", "svg", "webp", "ico", "zip", "tar", "gz", "7z",
        "sh", "bat", "ps1", "exe", "dll", "so", "dylib", "apk", "tsv", "csv", "log"
    };

    // 4. Common Internet top-level domains
    common_tlds = {
        "com", "org", "net", "edu", "gov", "mil", "int", "io", "bd", "co", "info",
        "me", "dev", "app", "ai", "tech", "xyz", "online", "site", "uk", "ca", "de",
        "jp", "au", "in", "fr", "ru", "cn", "global"
    };
}

bool TokenClassifier::is_url(const std::string& token) const {
    if (token.empty()) return false;

    // Check URL scheme prefix
    if (token.rfind("http://", 0) == 0 ||
        token.rfind("https://", 0) == 0 ||
        token.rfind("ftp://", 0) == 0 ||
        token.rfind("file://", 0) == 0 ||
        token.rfind("www.", 0) == 0) {
        return true;
    }

    // Check domain with dot and known TLD (e.g. google.com, prothomalo.com, bangla.gov.bd)
    size_t last_dot = token.rfind('.');
    if (last_dot != std::string::npos && last_dot + 1 < token.length() && last_dot > 0) {
        std::string suffix = token.substr(last_dot + 1);
        std::string lower_suffix = utf8::to_lower_ascii(suffix);
        if (common_tlds.find(lower_suffix) != common_tlds.end()) {
            // Check character validity in domain
            bool valid_domain = true;
            for (size_t i = 0; i < last_dot; ++i) {
                char c = token[i];
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '.') {
                    valid_domain = false;
                    break;
                }
            }
            if (valid_domain) return true;
        }
    }

    return false;
}

bool TokenClassifier::is_email(const std::string& token) const {
    if (token.empty()) return false;

    size_t at_pos = token.find('@');
    if (at_pos == std::string::npos || at_pos == 0 || at_pos + 1 >= token.length()) {
        return false;
    }

    // Must have at least one dot after '@'
    size_t dot_pos = token.find('.', at_pos);
    if (dot_pos == std::string::npos || dot_pos == at_pos + 1 || dot_pos + 1 >= token.length()) {
        return false;
    }

    // Only one '@' allowed
    if (token.find('@', at_pos + 1) != std::string::npos) {
        return false;
    }

    // Check characters before '@'
    for (size_t i = 0; i < at_pos; ++i) {
        char c = token[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-' && c != '+') {
            return false;
        }
    }

    return true;
}

bool TokenClassifier::is_file_path(const std::string& token) const {
    if (token.empty()) return false;

    // Unix absolute or home path: /usr/bin, ~/Documents, ./file, ../file
    if (token[0] == '/' || token.rfind("~/", 0) == 0 ||
        token.rfind("./", 0) == 0 || token.rfind("../", 0) == 0) {
        return true;
    }

    // Windows drive path: C:\, D:\, C:/
    if (token.length() >= 3 && std::isalpha(static_cast<unsigned char>(token[0])) &&
        token[1] == ':' && (token[2] == '\\' || token[2] == '/')) {
        return true;
    }

    // Internal path separator: dir/file.ext or dir\file.ext
    if (token.find('/') != std::string::npos || token.find('\\') != std::string::npos) {
        return true;
    }

    // Filename with recognized technical extension: main.cpp, script.py, config.json
    size_t last_dot = token.rfind('.');
    if (last_dot != std::string::npos && last_dot > 0 && last_dot + 1 < token.length()) {
        std::string ext = utf8::to_lower_ascii(token.substr(last_dot + 1));
        if (common_file_extensions.find(ext) != common_file_extensions.end()) {
            return true;
        }
    }

    return false;
}

bool TokenClassifier::is_code_token(const std::string& token) const {
    if (token.empty()) return false;

    // 1. snake_case: Contains underscore between characters (e.g. abk_engine_create, my_var)
    if (token.find('_') != std::string::npos) {
        return true;
    }

    // 2. Syntax operators or code punctuation: ::, ->, [], (), {}, !=, ==, +=, ++, &&, ||, $, #, %
    const std::string code_operators[] = {
        "::", "->", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "++", "--",
        "&&", "||", "[]", "()", "{}"
    };
    for (const auto& op : code_operators) {
        if (token.find(op) != std::string::npos) {
            return true;
        }
    }

    // Check individual code symbols: $, #, @, %, &, ;
    for (char c : token) {
        if (c == '$' || c == '#' || c == '%' || c == ';' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == '<' || c == '>') {
            return true;
        }
    }

    // 3. camelCase: Starts with lower, has uppercase letter inside (e.g. myVariable, getUserInfo)
    if (std::islower(static_cast<unsigned char>(token[0])) && token.length() >= 4) {
        // Exclude Avro diphthongs "OI" and "OU" (e.g. kOI, kOU, dOUr)
        if (token.find("OI") == std::string::npos && token.find("OU") == std::string::npos) {
            for (size_t i = 1; i < token.length(); ++i) {
                char c = token[i];
                if (std::isupper(static_cast<unsigned char>(c))) {
                    // Non-Avro uppercase characters indicate programming identifiers:
                    if (c == 'V' || c == 'C' || c == 'K' || c == 'M' || c == 'P' ||
                        c == 'L' || c == 'W' || c == 'F' || c == 'G' || c == 'H' ||
                        c == 'X' || c == 'Q' || c == 'B' || c == 'E' || c == 'A' || c == 'I' || c == 'J') {
                        return true;
                    }
                    // For T, D, N, R, S, Y, Z, check if it's followed by lowercase and length is substantial
                    if (i + 1 < token.length() && std::islower(static_cast<unsigned char>(token[i + 1])) && token.length() >= 7) {
                        return true;
                    }
                }
            }
        }
    }

    // 4. PascalCase: Starts with capital, followed by lower and upper (e.g. AmarBanglaKotha, HTMLElement)
    if (std::isupper(static_cast<unsigned char>(token[0])) && token.length() >= 4) {
        if (token.find("OI") == std::string::npos && token.find("OU") == std::string::npos) {
            bool has_lower = false;
            for (size_t i = 1; i < token.length(); ++i) {
                if (std::islower(static_cast<unsigned char>(token[i]))) {
                    has_lower = true;
                } else if (std::isupper(static_cast<unsigned char>(token[i]))) {
                    if (has_lower) {
                        char c = token[i];
                        if (c == 'V' || c == 'C' || c == 'K' || c == 'M' || c == 'P' ||
                            c == 'L' || c == 'W' || c == 'F' || c == 'G' || c == 'H' ||
                            c == 'X' || c == 'Q' || c == 'B' || c == 'E' || c == 'A' || c == 'I' || c == 'J') {
                            return true;
                        } else if (i + 1 < token.length() && std::islower(static_cast<unsigned char>(token[i + 1])) && token.length() >= 7) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 5. Identifier with digits (e.g. utf8, x86_64, var1, h264)
    bool has_alpha = false;
    bool has_digit = false;
    for (char c : token) {
        if (std::isalpha(static_cast<unsigned char>(c))) has_alpha = true;
        if (std::isdigit(static_cast<unsigned char>(c))) has_digit = true;
    }
    if (has_alpha && has_digit) {
        // If it starts with letter and contains digits (e.g. var123, sha256), treat as code identifier
        return true;
    }

    return false;
}

bool TokenClassifier::is_explicit_override(const std::string& token) const {
    if (token.empty()) return false;

    // Explicit Avro-compatible escape override starts with comma ',' (e.g. ,k, ,o, ,i)
    if (token[0] == ',') {
        return true;
    }

    return false;
}

bool TokenClassifier::is_english_word(const std::string& token) const {
    if (token.empty()) return false;

    std::string lower = utf8::to_lower_ascii(token);

    // CRITICAL: Banglish stop words must NEVER be classified as English
    if (banglish_stop_words.find(lower) != banglish_stop_words.end()) {
        return false;
    }

    // Check vocabulary of unambiguous English terms
    if (english_vocabulary.find(lower) != english_vocabulary.end()) {
        return true;
    }

    return false;
}

TokenClassification TokenClassifier::classify(const std::string& token) const {
    TokenClassification result;

    if (token.empty()) {
        result.type = TokenType::BANGLA_PHONETIC;
        result.should_bypass_transliteration = false;
        return result;
    }

    // 1. Explicit override
    if (is_explicit_override(token)) {
        result.type = TokenType::EXPLICIT_OVERRIDE;
        result.should_bypass_transliteration = false; // Overrides are handled by phonetic engine
        result.reason = "explicit_override";
        return result;
    }

    // 2. URLs
    if (is_url(token)) {
        result.type = TokenType::URL;
        result.should_bypass_transliteration = true;
        result.reason = "url";
        return result;
    }

    // 3. Emails
    if (is_email(token)) {
        result.type = TokenType::EMAIL;
        result.should_bypass_transliteration = true;
        result.reason = "email";
        return result;
    }

    // 4. File Paths
    if (is_file_path(token)) {
        result.type = TokenType::FILE_PATH;
        result.should_bypass_transliteration = true;
        result.reason = "file_path";
        return result;
    }

    // 5. Code tokens (camelCase, snake_case, syntax)
    if (is_code_token(token)) {
        result.type = TokenType::CODE_TOKEN;
        result.should_bypass_transliteration = true;
        result.reason = "code_token";
        return result;
    }

    // 6. English words
    if (is_english_word(token)) {
        result.type = TokenType::ENGLISH_WORD;
        result.should_bypass_transliteration = true;
        result.reason = "english_word";
        return result;
    }

    // Default: Banglish phonetic input
    result.type = TokenType::BANGLA_PHONETIC;
    result.should_bypass_transliteration = false;
    result.reason = "bangla_phonetic";
    return result;
}

bool TokenClassifier::should_bypass(const std::string& token) const {
    return classify(token).should_bypass_transliteration;
}

} // namespace abk
