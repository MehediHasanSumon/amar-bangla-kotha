#include "abk/bangladesh_helpers.h"
#include "abk/utf8_util.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace abk {

namespace {

static const char* WORDS_0_TO_99[100] = {
    "শূন্য", "এক", "দুই", "তিন", "চার", "পাঁচ", "ছয়", "সাত", "আট", "নয়", "দশ",
    "এগারো", "বারো", "তেরো", "চৌদ্দ", "পনেরো", "ষোলো", "সতেরো", "আঠারো", "উনিশ", "বিশ",
    "একুশ", "বাইশ", "তেইশ", "চব্বিশ", "পঁচিশ", "ছাব্বিশ", "সাতাশ", "আঠাশ", "উনত্রিশ", "ত্রিশ",
    "একত্রিশ", "বত্রিশ", "তেত্রিশ", "চৌত্রিশ", "পঁয়ত্রিশ", "ছত্রিশ", "সাঁইত্রিশ", "আটত্রিশ", "উনচল্লিশ", "চল্লিশ",
    "একচল্লিশ", "বিয়াল্লিশ", "তেতাল্লিশ", "চুয়াল্লিশ", "পঁয়তাল্লিশ", "ছেচল্লিশ", "সাতচল্লিশ", "আটচল্লিশ", "উনপঞ্চাশ", "পঞ্চাশ",
    "একান্ন", "বায়ান্ন", "তিপ্পান্ন", "চুয়ান্ন", "পঞ্চান্ন", "ছাপ্পান্ন", "সাতান্ন", "আটান্ন", "উনষাট", "ষাট",
    "একষট্টি", "বাষট্টি", "তেষট্টি", "চৌষট্টি", "পঁয়ষট্টি", "ছেষট্টি", "সাতষট্টি", "আটষট্টি", "উনসত্তর", "সত্তর",
    "একাত্তর", "বাহাত্তর", "তিয়াত্তর", "চুয়াত্তর", "পঁচাত্তর", "ছিয়াত্তর", "সাতাত্তর", "আটাত্তর", "উনাশি", "আশি",
    "একাশি", "বিরাশি", "তিরাশি", "চৌরাশি", "পঁচাশি", "ছিয়াশি", "সাতাশি", "আটাশি", "ঊননব্বই", "নব্বই",
    "একানব্বই", "বানব্বই", "তিরানব্বই", "চুরানব্বই", "পঁচানব্বই", "ছিয়ানব্বই", "সাতানব্বই", "আটানব্বই", "নিরানব্বই"
};

static const char* BANGABDA_MONTHS[12] = {
    "বৈশাখ", "জ্যৈষ্ঠ", "আষাঢ়", "শ্রাবণ", "ভাদ্র", "আশ্বিন",
    "কার্তিক", "অগ্রহায়ণ", "পৌষ", "মাঘ", "ফাল্গুন", "চৈত্র"
};

std::string to_bengali_digits(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            // Bengali digits U+09E6 to U+09EF
            char32_t cp = 0x09E6 + (c - '0');
            out += utf8::to_utf8(cp);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool is_gregorian_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

bool is_delimiter_or_punct(const std::string& text, size_t pos, size_t& char_len) {
    if (pos >= text.size()) return false;
    unsigned char c = static_cast<unsigned char>(text[pos]);
    if (c <= ' ' || c == ',' || c == '.' || c == '|' || c == '?' || c == '!' || c == ';' || c == ':' || c == '"' || c == '\'') {
        char_len = 1;
        return true;
    }
    // Check for Bengali Dari '।' (U+0964 -> 0xE0 0xA5 0xA4)
    if (c == 0xE0 && pos + 2 < text.size() &&
        static_cast<unsigned char>(text[pos+1]) == 0xA5 &&
        static_cast<unsigned char>(text[pos+2]) == 0xA4) {
        char_len = 3;
        return true;
    }
    // Check for Bengali Double Dari '॥' (U+0965 -> 0xE0 0xA5 0xA5)
    if (c == 0xE0 && pos + 2 < text.size() &&
        static_cast<unsigned char>(text[pos+1]) == 0xA5 &&
        static_cast<unsigned char>(text[pos+2]) == 0xA5) {
        char_len = 3;
        return true;
    }
    char_len = 0;
    return false;
}

int days_in_gregorian_month(int year, int month) {
    static const int days_common[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_gregorian_leap_year(year)) return 29;
    return (month >= 1 && month <= 12) ? days_common[month] : 30;
}

// Days from Jan 1, 1970 to (y, m, d)
long long date_to_days(int y, int m, int d) {
    long long days = 0;
    for (int yr = 1970; yr < y; ++yr) {
        days += is_gregorian_leap_year(yr) ? 366 : 365;
    }
    for (int mo = 1; mo < m; ++mo) {
        days += days_in_gregorian_month(y, mo);
    }
    days += (d - 1);
    return days;
}

} // anonymous namespace

std::string BangladeshHelpers::format_lakh_crore(long long number, bool bengali_digits) {
    if (number == 0) {
        return bengali_digits ? "০" : "0";
    }

    bool is_negative = number < 0;
    unsigned long long abs_n = (number < 0) ? -static_cast<unsigned long long>(number) : static_cast<unsigned long long>(number);
    std::string digits = std::to_string(abs_n);

    std::string formatted;
    size_t len = digits.size();

    if (len <= 3) {
        formatted = digits;
    } else {
        std::string last3 = digits.substr(len - 3);
        std::string prefix = digits.substr(0, len - 3);

        std::string rev_prefix;
        size_t count = 0;
        for (long long i = static_cast<long long>(prefix.size()) - 1; i >= 0; --i) {
            if (count == 2) {
                rev_prefix.push_back(',');
                count = 0;
            }
            rev_prefix.push_back(prefix[i]);
            count++;
        }
        std::reverse(rev_prefix.begin(), rev_prefix.end());
        formatted = rev_prefix + "," + last3;
    }

    if (bengali_digits) {
        formatted = to_bengali_digits(formatted);
    }

    if (is_negative) {
        formatted = "-" + formatted;
    }

    return formatted;
}

std::string BangladeshHelpers::number_to_bangla_words(long long number) {
    if (number == 0) return WORDS_0_TO_99[0];
    if (number < 0) {
        return "ঋণাত্মক " + number_to_bangla_words(-number);
    }

    std::string result;

    if (number >= 10000000LL) { // 1 Crore (১ কোটি)
        long long crore = number / 10000000LL;
        long long rem = number % 10000000LL;
        result = number_to_bangla_words(crore) + " কোটি";
        if (rem > 0) {
            result += " " + number_to_bangla_words(rem);
        }
        return result;
    }

    if (number >= 100000LL) { // 1 Lakh (১ লক্ষ)
        long long lakh = number / 100000LL;
        long long rem = number % 100000LL;
        result = number_to_bangla_words(lakh) + " লক্ষ";
        if (rem > 0) {
            result += " " + number_to_bangla_words(rem);
        }
        return result;
    }

    if (number >= 1000LL) { // 1 Thousand (১ হাজার)
        long long thousand = number / 1000LL;
        long long rem = number % 1000LL;
        result = number_to_bangla_words(thousand) + " হাজার";
        if (rem > 0) {
            result += " " + number_to_bangla_words(rem);
        }
        return result;
    }

    if (number >= 100LL) { // 1 Hundred (১ শত)
        long long hundred = number / 100LL;
        long long rem = number % 100LL;
        if (hundred == 1) {
            result = "একশত";
        } else {
            result = std::string(WORDS_0_TO_99[hundred]) + "শত";
        }
        if (rem > 0) {
            result += " " + number_to_bangla_words(rem);
        }
        return result;
    }

    return WORDS_0_TO_99[number];
}

std::string BangladeshHelpers::get_bangla_ordinal(int day) {
    std::string day_str = to_bengali_digits(std::to_string(day));
    if (day == 1) return day_str + "লা";
    if (day == 2 || day == 3) return day_str + "রা";
    if (day == 4) return day_str + "ঠা";
    if (day >= 5 && day <= 18) return day_str + "ই";
    if (day >= 19 && day <= 31) return day_str + "শে";
    return day_str;
}

BangabdaDate BangladeshHelpers::gregorian_to_bangabda(int year, int month, int day) {
    BangabdaDate b_date;

    // Check if on or after April 14
    bool is_on_or_after_pahela_boishakh = (month > 4) || (month == 4 && day >= 14);

    int b_year = is_on_or_after_pahela_boishakh ? (year - 593) : (year - 594);
    int ref_year = is_on_or_after_pahela_boishakh ? year : (year - 1);

    long long current_days = date_to_days(year, month, day);
    long long pahela_boishakh_days = date_to_days(ref_year, 4, 14);

    long long elapsed_days = current_days - pahela_boishakh_days;

    // Bangla Academy official revised month days:
    // Months 1-6 (Boishakh to Ashwin) = 31 days
    // Months 7-10 (Kartik to Magh) = 30 days
    // Month 11 (Falgun) = 29 days (30 in leap year)
    // Month 12 (Chaitra) = 30 days
    bool falgun_leap = is_gregorian_leap_year(ref_year + 1);
    int bangabda_month_days[12] = {
        31, 31, 31, 31, 31, 31,
        30, 30, 30, 30, (falgun_leap ? 30 : 29), 30
    };

    int b_month = 1;
    long long rem_days = elapsed_days;
    for (int m = 0; m < 12; ++m) {
        if (rem_days < bangabda_month_days[m]) {
            b_month = m + 1;
            break;
        }
        rem_days -= bangabda_month_days[m];
    }

    int b_day = static_cast<int>(rem_days) + 1;

    b_date.year = b_year;
    b_date.month = b_month;
    b_date.month_name = BANGABDA_MONTHS[b_month - 1];
    b_date.day = b_day;
    b_date.ordinal = get_bangla_ordinal(b_day);

    std::string b_year_bn = to_bengali_digits(std::to_string(b_year));
    b_date.formatted = b_date.ordinal + " " + b_date.month_name + " " + b_year_bn + " বঙ্গাব্দ";

    return b_date;
}

SadhuCholitoResult BangladeshHelpers::convert_sadhu_to_cholito(const std::string& text) {
    SadhuCholitoResult res;

    static const std::unordered_map<std::string, std::string> PRONOUN_MAP = {
        {"তাহার", "তার"},
        {"তাহারা", "তারা"},
        {"তাহাদের", "তাদের"},
        {"তাহাকে", "তাকে"},
        {"তাহাদিগকে", "তাদেরকে"},
        {"তাহাতে", "তাতে"},
        {"যাহার", "যার"},
        {"যাহারা", "যারা"},
        {"যাহাদের", "যাদের"},
        {"যাহাকে", "যাকে"},
        {"যাহাতে", "যাতে"},
        {"যাহা", "যা"},
        {"ইহা", "এটা"},
        {"ইহাতে", "এতে"},
        {"ইহাদের", "এদের"},
        {"ইহাকে", "একে"},
        {"উহা", "ওটা"},
        {"উহাতে", "ওতে"},
        {"উহাদের", "ওদের"},
        {"উহাকে", "ওকে"},
        {"কাহার", "কার"},
        {"কাহারা", "কারা"},
        {"কাহাকে", "কাকে"},
        {"সহিত", "সাথে"}
    };

    static const std::unordered_map<std::string, std::string> IRREGULAR_VERBS = {
        {"যাইতেছে", "যাচ্ছে"},
        {"যাইতেছেন", "যাচ্ছেন"},
        {"যাইতেছিল", "যাচ্ছিল"},
        {"যাইতেছিলেন", "যাচ্ছিলেন"},
        {"যাইব", "যাব"},
        {"যাইবেন", "যাবেন"},
        {"যাইয়া", "গিয়ে"},
        {"গিয়াছিল", "গিয়েছিল"},
        {"খাইতেছে", "খাচ্ছে"},
        {"খাইতেছেন", "খাচ্ছেন"},
        {"খাইতেছিল", "খাচ্ছিল"},
        {"খাইব", "খাব"},
        {"খাইয়া", "খেয়ে"},
        {"হইয়া", "হয়ে"},
        {"হইয়াছে", "হয়েছে"},
        {"হইয়াছিল", "হয়েছিল"},
        {"হইবেন", "হবেন"},
        {"হইবে", "হবে"},
        {"আসিয়া", "এসে"},
        {"আসিবে", "আসবে"},
        {"আসিয়াছিল", "এসেছিল"}
    };

    std::string out;

    size_t i = 0;
    while (i < text.size()) {
        size_t delim_len = 0;
        if (is_delimiter_or_punct(text, i, delim_len)) {
            out.append(text, i, delim_len);
            i += delim_len;
            continue;
        }

        size_t start = i;
        while (i < text.size()) {
            size_t dlen = 0;
            if (is_delimiter_or_punct(text, i, dlen)) {
                break;
            }
            i++;
        }

        std::string word = text.substr(start, i - start);
        std::string converted = word;

        // 1. Direct pronoun lookup
        auto it = PRONOUN_MAP.find(word);
        if (it != PRONOUN_MAP.end()) {
            converted = it->second;
            res.flags.push_back({start, word, converted, "Standard pronoun transformation"});
        } else {
            // 2. Irregular verb lookup
            auto it_irr = IRREGULAR_VERBS.find(word);
            if (it_irr != IRREGULAR_VERBS.end()) {
                converted = it_irr->second;
                res.flags.push_back({start, word, converted, "Irregular verb transformation"});
            } else {
                // 3. Regular suffix transformations
                static const std::vector<std::pair<std::string, std::string>> SUFFIX_MAP = {
                    {"িতেছেন", "ছেন"},
                    {"িতেছে", "ছে"},
                    {"িতেছিলেন", "ছিলেন"},
                    {"িতেছিল", "ছিল"},
                    {"িয়াছিলেন", "েছিলেন"},
                    {"িয়াছিল", "েছিল"},
                    {"িয়াছেন", "েছেন"},
                    {"িয়াছে", "েছে"},
                    {"িবার", "ার"},
                    {"িবেন", "বেন"},
                    {"িবে", "বে"},
                    {"িলেক", "ল"},
                    {"িলে", "লে"},
                    {"িয়া", "ে"},
                    {"িল", "ল"},
                    {"িব", "ব"},
                    {"ইতেছেন", "চ্ছেন"},
                    {"ইতেছে", "চ্ছে"},
                    {"িতেছিলেন", "চ্ছিলেন"},
                    {"িতেছিল", "চ্ছিল"},
                    {"িব", "ব"}
                };

                for (const auto& [suffix, repl] : SUFFIX_MAP) {
                    if (word.size() > suffix.size() && word.rfind(suffix) == word.size() - suffix.size()) {
                        std::string root = word.substr(0, word.size() - suffix.size());
                        converted = root + repl;
                        res.flags.push_back({start, word, converted, "Regular verb suffix: " + suffix + " -> " + repl});
                        break;
                    }
                }
            }
        }

        out += converted;
    }

    res.text = out;
    return res;
}

SadhuCholitoResult BangladeshHelpers::convert_cholito_to_sadhu(const std::string& text) {
    SadhuCholitoResult res;

    static const std::unordered_map<std::string, std::string> REV_PRONOUN_MAP = {
        {"তার", "তাহার"},
        {"তারা", "তাহারা"},
        {"তাদের", "তাহাদের"},
        {"তাকে", "তাহাকে"},
        {"তাদেরকে", "তাহাদিগকে"},
        {"তাতে", "তাহাতে"},
        {"যার", "যাহার"},
        {"যারা", "যাহারা"},
        {"যাদের", "যাহাদের"},
        {"যাকে", "যাহাকে"},
        {"যাতে", "যাহাতে"},
        {"যা", "যাহা"},
        {"এটা", "ইহা"},
        {"এতে", "ইহাতে"},
        {"এদের", "ইহাদের"},
        {"একে", "ইহাকে"},
        {"ওটা", "উহা"},
        {"ওতে", "উহাতে"},
        {"ওদের", "উহাদের"},
        {"ওকে", "উহাকে"},
        {"কার", "কাহার"},
        {"কারা", "কাহারা"},
        {"কাকে", "কাহাকে"},
        {"সাথে", "সহিত"}
    };

    static const std::unordered_map<std::string, std::string> REV_IRREGULAR_VERBS = {
        {"যাচ্ছে", "যাইতেছে"},
        {"যাচ্ছেন", "যাইতেছেন"},
        {"যাচ্ছিল", "যাইতেছিল"},
        {"যাচ্ছিলেন", "যাইতেছিলেন"},
        {"যাব", "যাইব"},
        {"যাবেন", "যাইবেন"},
        {"গিয়ে", "যাইয়া"},
        {"গিয়েছিল", "গিয়াছিল"},
        {"খাচ্ছে", "খাইতেছে"},
        {"খাচ্ছেন", "খাইতেছেন"},
        {"খাচ্ছিল", "খাইতেছিল"},
        {"খাব", "খাইব"},
        {"খেয়ে", "খাইয়া"},
        {"হয়ে", "হইয়া"},
        {"হয়েছে", "হইয়াছে"},
        {"হয়েছিল", "হইয়াছিল"},
        {"হবেন", "হইবেন"},
        {"হবে", "হইবে"},
        {"এসে", "আসিয়া"},
        {"আসবে", "আসিবে"},
        {"এসেছিল", "আসিয়াছিল"}
    };

    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t delim_len = 0;
        if (is_delimiter_or_punct(text, i, delim_len)) {
            out.append(text, i, delim_len);
            i += delim_len;
            continue;
        }

        size_t start = i;
        while (i < text.size()) {
            size_t dlen = 0;
            if (is_delimiter_or_punct(text, i, dlen)) {
                break;
            }
            i++;
        }

        std::string word = text.substr(start, i - start);
        std::string converted = word;

        auto it = REV_PRONOUN_MAP.find(word);
        if (it != REV_PRONOUN_MAP.end()) {
            converted = it->second;
            res.flags.push_back({start, word, converted, "Standard pronoun transformation (Cholito -> Sadhu)"});
        } else {
            auto it_irr = REV_IRREGULAR_VERBS.find(word);
            if (it_irr != REV_IRREGULAR_VERBS.end()) {
                converted = it_irr->second;
                res.flags.push_back({start, word, converted, "Irregular verb (Cholito -> Sadhu)"});
            } else {
                static const std::vector<std::pair<std::string, std::string>> REV_SUFFIX_MAP = {
                    {"চ্ছেন", "ইতেছেন"},
                    {"চ্ছে", "ইতেছে"},
                    {"চ্ছিলেন", "িতেছিলেন"},
                    {"চ্ছিল", "িতেছিল"},
                    {"েছিলেন", "িয়াছিলেন"},
                    {"েছিল", "িয়াছিল"},
                    {"েছেন", "িয়াছেন"},
                    {"েছে", "িয়াছে"},
                    {"বেন", "িবেন"}
                };

                for (const auto& [suffix, repl] : REV_SUFFIX_MAP) {
                    if (word.size() > suffix.size() && word.rfind(suffix) == word.size() - suffix.size()) {
                        std::string root = word.substr(0, word.size() - suffix.size());
                        converted = root + repl;
                        res.flags.push_back({start, word, converted, "Regular verb suffix (Cholito -> Sadhu): " + suffix + " -> " + repl});
                        break;
                    }
                }
            }
        }

        out += converted;
    }

    res.text = out;
    return res;
}

std::vector<std::pair<std::string, std::string>> BangladeshHelpers::get_builtin_shortcuts() {
    return {
        {"tk", "৳"},
        {"taka", "৳"},
        {"tk.", "৳"},
        {"taka.", "৳"},
        {"salam", "আসসালামু আলাইকুম"},
        {"assalamu", "আসসালামু আলাইকুম"},
        {"walaikum", "ওয়ালাইকুম আসসালাম"},
        {"inshallah", "ইনশাআল্লাহ"},
        {"inshaallah", "ইনশাআল্লাহ"},
        {"alhamdulillah", "আলহামদুলিল্লাহ"},
        {"bismillah", "বিসমিল্লাহ"},
        {"subhanallah", "সুবহানাল্লাহ"},
        {"mashallah", "মাশাআল্লাহ"},
        {"jazakallah", "জাজাকাল্লাহ"},
        {"allahhafez", "আল্লাহ হাফেজ"}
    };
}

} // namespace abk
