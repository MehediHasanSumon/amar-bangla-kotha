#pragma once

#include "abk/types.h"
#include "abk/dictionary.h"
#include <string>
#include <vector>

namespace abk {

/**
 * @brief Feedback item for recording erroneous suggestions locally.
 */
struct FeedbackItem {
    std::string raw_input;
    std::string received_suggestion;
    std::string expected_word;
    std::string timestamp;
    std::string os_info;
    std::string app_name;
};

/**
 * @brief Record for opt-in user correction logging.
 */
struct CorrectionRecord {
    std::string timestamp;
    std::string raw_input;
    std::string corrected_word;
};

/**
 * @brief Opt-in local correction logger.
 * Privacy-first: Default OFF, size-capped, zero logging in secure/password mode.
 */
class CorrectionLogger {
public:
    CorrectionLogger(const std::string& filepath = "");

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enable);

    void record_correction(const std::string& raw_input, const std::string& corrected_word, bool is_secure_mode);
    std::vector<CorrectionRecord> get_records() const;
    void clear_records();

    size_t record_count() const;
    const std::string& get_filepath() const { return filepath_; }

private:
    bool enabled_ = false;
    std::string filepath_;
    mutable std::vector<CorrectionRecord> memory_records_;

    std::string default_log_path() const;
    void load_from_disk() const;
    void append_to_disk(const CorrectionRecord& record);
};

/**
 * @brief Quality of Life Utilities for Amar Bangla Kotha.
 */
class QualityOfLife {
public:
    /**
     * @brief Transliterates multi-word / paragraph Banglish text to Bangla.
     * Preserves whitespace, punctuation, URLs, emails, and code tokens.
     */
    static std::string convert_banglish_to_bangla(const std::string& text, const Dictionary* dict = nullptr);

    /**
     * @brief Converts standard Unicode Bangla text to readable Banglish Latin text.
     */
    static std::string convert_bangla_to_banglish(const std::string& text);

    /**
     * @brief Applies typographical smart punctuation rules:
     * - '.' -> '।' (bypassing decimals, URLs, emails, file extensions, abbreviations)
     * - '...' -> '…' (U+2026)
     * - '"' -> '“' and '”'
     */
    static std::string apply_smart_punctuation(const std::string& text);

    /**
     * @brief Generates emoji / symbol suggestions for relevant keywords.
     * e.g. "smile" -> 😊, "bhalobasha" / "love" -> ❤️, "fire" -> 🔥, "tk" -> ৳.
     */
    static std::vector<Candidate> get_emoji_suggestions(const std::string& keyword);

    /**
     * @brief Saves a user feedback report locally for inclusion in bug reports.
     */
    static bool save_feedback_locally(const FeedbackItem& item, const std::string& custom_path = "");

    /**
     * @brief Loads all locally recorded user feedback items.
     */
    static std::vector<FeedbackItem> load_local_feedback(const std::string& custom_path = "");

    /**
     * @brief Clears locally recorded feedback.
     */
    static void clear_local_feedback(const std::string& custom_path = "");
};

} // namespace abk
