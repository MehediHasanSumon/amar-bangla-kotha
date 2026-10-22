#pragma once

#include <string>
#include <vector>

namespace abk {

/**
 * @brief Legacy Font (Bijoy / SutonnyMJ ANSI) <-> Unicode UTF-8 Converter.
 * 
 * Provides robust bidirectional conversion between legacy ANSI-encoded
 * Bengali DTP text (SutonnyMJ, Bijoy 2000, Bayanno) and standard Unicode UTF-8.
 * Handles visual-to-logical reordering of pre-vowel kars, reph (র্),
 * conjuncts (যুক্তাক্ষর), and compound vowels.
 */
class LegacyConverter {
public:
    /**
     * @brief Converts Bijoy / SutonnyMJ ANSI text to standard Unicode (UTF-8).
     * 
     * @param ansi_text Input ANSI string (CP1252 / ISO-8859-1 / ASCII text).
     * @return std::string Clean Unicode UTF-8 string.
     */
    static std::string convert_ansi_to_unicode(const std::string& ansi_text);

    /**
     * @brief Converts standard Unicode (UTF-8) text to Bijoy / SutonnyMJ ANSI text.
     * 
     * @param unicode_text Input Unicode UTF-8 Bengali string.
     * @return std::string Bijoy ANSI string suitable for legacy printing / SutonnyMJ.
     */
    static std::string convert_unicode_to_ansi(const std::string& unicode_text);

    /**
     * @brief Heuristic detection if a string contains legacy Bijoy/SutonnyMJ ANSI text.
     * 
     * @param text Input string.
     * @return true if the string appears to be encoded in Bijoy ANSI.
     */
    static bool is_likely_legacy_ansi(const std::string& text);
};

} // namespace abk
