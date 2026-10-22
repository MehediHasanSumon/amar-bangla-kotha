#pragma once

#include <string>
#include <vector>
#include <utility>

namespace abk {

/**
 * @brief Bangabda (Bengali Calendar) Representation.
 */
struct BangabdaDate {
    int year = 0;                  // e.g. 1433
    int month = 0;                 // 1 = Boishakh, 12 = Chaitra
    std::string month_name;        // e.g. "বৈশাখ", "আশ্বিন"
    int day = 0;                   // 1 to 31
    std::string ordinal;           // e.g. "১লা", "১৫ই", "২১শে"
    std::string formatted;         // e.g. "১লা বৈশাখ ১৪৩৩ বঙ্গাব্দ"
};

/**
 * @brief Sadhu to Cholito conversion diagnostic flag.
 */
struct ConversionFlag {
    size_t position = 0;
    std::string original_word;
    std::string converted_word;
    std::string explanation;
};

struct SadhuCholitoResult {
    std::string text;
    std::vector<ConversionFlag> flags;
};

/**
 * @brief Bangladesh-Specific Language & Linguistic Helpers.
 */
class BangladeshHelpers {
public:
    /**
     * @brief Formats an integer using South Asian Lakh/Crore grouping (3-2-2-2 style).
     * 
     * @param number The input integer (e.g. 1250000).
     * @param bengali_digits If true, converts digits to ০-৯ (e.g. "১২,৫০,০০০"). If false: "12,50,000".
     * @return std::string Formatted number string.
     */
    static std::string format_lakh_crore(long long number, bool bengali_digits = true);

    /**
     * @brief Converts an integer to words in standard Bengali (e.g. 1250 -> "এক হাজার দুইশত পঞ্চাশ").
     * 
     * Supports negative numbers ("ঋণাত্মক ..."), zero ("শূন্য"), and numbers up to hundreds of crores.
     * 
     * @param number Integer value to convert.
     * @return std::string Written Bengali representation.
     */
    static std::string number_to_bangla_words(long long number);

    /**
     * @brief Converts a Civil Gregorian date (Year, Month 1-12, Day 1-31) to the revised Bangladesh Bangabda calendar.
     * 
     * Follows Bangla Academy official revision (April 14 = 1st Boishakh; months 1-6 have 31 days).
     * 
     * @param year Gregorian year (e.g. 2026).
     * @param month Gregorian month (1 to 12).
     * @param day Gregorian day of month (1 to 31).
     * @return BangabdaDate Structured Bengali calendar date.
     */
    static BangabdaDate gregorian_to_bangabda(int year, int month, int day);

    /**
     * @brief Returns the Bengali ordinal suffix for a calendar day (1 to 31).
     * 
     * Examples: 1 -> "১লা", 2 -> "২রা", 3 -> "৩রা", 4 -> "৪ঠা", 15 -> "১৫ই", 21 -> "২১শে".
     * 
     * @param day Day of the month (1-31).
     * @return std::string Bengali ordinal string.
     */
    static std::string get_bangla_ordinal(int day);

    /**
     * @brief Converts Sadhu (সাধু রীতি) text to Cholito (চলিত রীতি) for verbs and standard pronouns.
     * 
     * Rule-based morphological parser that transforms verb endings (-ইতেছে -> -চ্ছে, -িয়াছিল -> -েছিল)
     * and pronouns (তাহার -> তার, যাহারা -> যারা). Flags ambiguous or irregular cases.
     * 
     * @param text Input Bengali text in Sadhu style.
     * @return SadhuCholitoResult Converted text and diagnostic flags.
     */
    static SadhuCholitoResult convert_sadhu_to_cholito(const std::string& text);

    /**
     * @brief Converts Cholito (চলিত রীতি) text to Sadhu (সাধু রীতি) for verbs and standard pronouns.
     * 
     * @param text Input Bengali text in Cholito style.
     * @return SadhuCholitoResult Converted text and diagnostic flags.
     */
    static SadhuCholitoResult convert_cholito_to_sadhu(const std::string& text);

    /**
     * @brief Returns built-in high-utility Bangladeshi shortcuts and phrase expansions.
     * 
     * Includes currency ("tk" -> "৳", "taka" -> "৳") and standard greetings.
     */
    static std::vector<std::pair<std::string, std::string>> get_builtin_shortcuts();
};

} // namespace abk
