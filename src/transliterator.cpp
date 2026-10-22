#include "abk/transliterator.h"
#include "abk/dictionary.h"
#include "abk/utf8_util.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>

namespace abk {

Transliterator::Transliterator() {
    std::vector<std::string> search_paths = {
        "data/phonetic_rules.tsv",
        "../data/phonetic_rules.tsv",
        "/usr/local/share/amar-bangla-kotha/phonetic_rules.tsv"
    };

    bool loaded = false;
    for (const auto& path : search_paths) {
        if (load_rules_from_file(path)) {
            loaded = true;
            break;
        }
    }

    if (!loaded || rules.empty()) {
        init_default_rules();
    }
}

Transliterator::Transliterator(const std::string& rules_filepath) {
    if (!load_rules_from_file(rules_filepath) || rules.empty()) {
        init_default_rules();
    }
}

RuleContext Transliterator::parse_context(const std::string& str) {
    if (str == "START") return RuleContext::START;
    if (str == "AFTER_CONSONANT") return RuleContext::AFTER_CONSONANT;
    if (str == "AFTER_VOWEL") return RuleContext::AFTER_VOWEL;
    if (str == "END") return RuleContext::END;
    return RuleContext::ANY;
}

OutputCategory Transliterator::parse_category(const std::string& str) {
    if (str == "CONSONANT") return OutputCategory::CONSONANT;
    if (str == "INDEPENDENT_VOWEL") return OutputCategory::INDEPENDENT_VOWEL;
    if (str == "KAR") return OutputCategory::KAR;
    if (str == "CONJUNCT") return OutputCategory::CONJUNCT;
    if (str == "MODIFIER") return OutputCategory::MODIFIER;
    if (str == "DIPHTHONG_SPLIT") return OutputCategory::DIPHTHONG_SPLIT;
    if (str == "DIGIT") return OutputCategory::DIGIT;
    if (str == "PUNCTUATION") return OutputCategory::PUNCTUATION;
    return OutputCategory::OTHER;
}

void Transliterator::parse_rule_line(const std::string& line) {
    std::string trimmed = utf8::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;

    std::stringstream ss(trimmed);
    std::string pattern, output, context_str, category_str, desc;
    double priority = 100.0;

    if (std::getline(ss, pattern, '\t') &&
        std::getline(ss, output, '\t') &&
        std::getline(ss, context_str, '\t') &&
        std::getline(ss, category_str, '\t')) {

        std::string prio_str;
        if (std::getline(ss, prio_str, '\t')) {
            try {
                priority = std::stod(prio_str);
            } catch (...) {
                priority = 100.0;
            }
        }
        std::getline(ss, desc, '\t');

        PhoneticRule rule;
        rule.pattern = pattern;
        rule.output = utf8::normalize_nfc(output);
        rule.context = parse_context(context_str);
        rule.category = parse_category(category_str);
        rule.priority = priority;
        rule.description = desc;

        rules.push_back(rule);
    }
}

bool Transliterator::load_rules_from_file(const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) return false;

    rules.clear();
    std::string line;
    while (std::getline(infile, line)) {
        parse_rule_line(line);
    }

    std::stable_sort(rules.begin(), rules.end(), [](const PhoneticRule& a, const PhoneticRule& b) {
        if (a.pattern.length() != b.pattern.length()) {
            return a.pattern.length() > b.pattern.length();
        }
        return a.priority > b.priority;
    });

    return !rules.empty();
}

void Transliterator::init_default_rules() {
    rules.clear();
    rules = {
        {"OI", "ৈ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 450, "oi_kar_override"},
        {"OU", "ৌ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 450, "ou_kar_override"},
        {"OI", "ঐ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 450, "oi_initial_override"},
        {"OU", "ঔ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 450, "ou_initial_override"},
        {"r``y", "র‍্য", RuleContext::ANY, OutputCategory::CONJUNCT, 500, "zwj_reph_ya"},
        {"r_y", "র‍্য", RuleContext::ANY, OutputCategory::CONJUNCT, 500, "zwj_reph_ya_alt"},
        {"t`", "ৎ", RuleContext::ANY, OutputCategory::MODIFIER, 450, "khanda_ta"},
        {"^", "ঁ", RuleContext::ANY, OutputCategory::MODIFIER, 300, "chandrabindu"},
        {":", "ঃ", RuleContext::ANY, OutputCategory::MODIFIER, 300, "visarga"},
        {",,", "্", RuleContext::ANY, OutputCategory::MODIFIER, 300, "virama"},

        {"0", "০", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_0"},
        {"1", "১", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_1"},
        {"2", "২", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_2"},
        {"3", "৩", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_3"},
        {"4", "৪", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_4"},
        {"5", "৫", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_5"},
        {"6", "৬", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_6"},
        {"7", "৭", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_7"},
        {"8", "৮", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_8"},
        {"9", "৯", RuleContext::ANY, OutputCategory::DIGIT, 150, "digit_9"},

        {".", "।", RuleContext::END, OutputCategory::PUNCTUATION, 180, "dari"},

        {"oi", "ই", RuleContext::AFTER_CONSONANT, OutputCategory::DIPHTHONG_SPLIT, 250, "inherent_plus_i"},
        {"oi", "ৈ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 220, "oi_kar"},
        {"oi", "ঐ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 260, "oi_init"},
        {"ou", "উ", RuleContext::AFTER_CONSONANT, OutputCategory::DIPHTHONG_SPLIT, 250, "inherent_plus_u"},
        {"ou", "ৌ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 220, "ou_kar"},
        {"ou", "ঔ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 260, "ou_init"},

        {"dhonnobad", "ধন্যবাদ", RuleContext::START, OutputCategory::CONJUNCT, 1200, "dhonnobad"},
        {"dhonno", "ধন্য", RuleContext::START, OutputCategory::CONJUNCT, 800, "dhonno"},
        {"kkh", "ক্ষ", RuleContext::ANY, OutputCategory::CONJUNCT, 360, "kkh"},
        {"jny", "জ্ঞ", RuleContext::ANY, OutputCategory::CONJUNCT, 360, "jny"},
        {"gyan", "জ্ঞান", RuleContext::START, OutputCategory::CONJUNCT, 480, "gyan"},
        {"gy", "জ্ঞ", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "gy"},
        {"ngk", "ঙ্ক", RuleContext::ANY, OutputCategory::CONJUNCT, 360, "ngk"},
        {"ngg", "ঙ্গ", RuleContext::ANY, OutputCategory::CONJUNCT, 360, "ngg"},
        {"cch", "চ্ছ", RuleContext::ANY, OutputCategory::CONJUNCT, 360, "cch"},
        {"tt", "ত্ত", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "tt"},
        {"TT", "ট্ট", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "TT"},
        {"nt", "ন্ত", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "nt"},
        {"nd", "ন্দ", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "nd"},
        {"nth", "ন্থ", RuleContext::ANY, OutputCategory::CONJUNCT, 350, "nth"},
        {"ndh", "ন্ধ", RuleContext::ANY, OutputCategory::CONJUNCT, 350, "ndh"},
        {"bd", "ব্দ", RuleContext::ANY, OutputCategory::CONJUNCT, 260, "bd"},

        {"a", "া", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 100, "akar"},
        {"aa", "া", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 230, "aakar"},
        {"i", "ি", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 100, "ikar"},
        {"I", "ী", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 120, "Ikar"},
        {"ee", "ী", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 230, "eekar"},
        {"u", "ু", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 100, "ukar"},
        {"U", "ূ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 120, "Ukar"},
        {"oo", "ূ", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 230, "ookar"},
        {"e", "ে", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 100, "ekar"},
        {"O", "ো", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 120, "Okar"},
        {"o", "ো", RuleContext::AFTER_CONSONANT, OutputCategory::KAR, 95, "okar"},
        {"t", "ৎ", RuleContext::END, OutputCategory::MODIFIER, 120, "terminal_t"},

        {"a", "আ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 100, "init_a"},
        {"aa", "আ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 230, "init_aa"},
        {"o", "অ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 100, "init_o"},
        {"i", "ই", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 100, "init_i"},
        {"I", "ঈ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 120, "init_I"},
        {"ee", "ঈ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 230, "init_ee"},
        {"u", "উ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 100, "init_u"},
        {"U", "ঊ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 120, "init_U"},
        {"oo", "ঊ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 230, "init_oo"},
        {"e", "এ", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 100, "init_e"},
        {"O", "ও", RuleContext::START, OutputCategory::INDEPENDENT_VOWEL, 120, "init_O"},

        {"kh", "খ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "kh"},
        {"gh", "ঘ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "gh"},
        {"Ng", "ঙ", RuleContext::ANY, OutputCategory::CONSONANT, 260, "Ng"},
        {"ng", "ং", RuleContext::ANY, OutputCategory::MODIFIER, 240, "ng"},
        {"ch", "চ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "ch"},
        {"chh", "ছ", RuleContext::ANY, OutputCategory::CONSONANT, 260, "chh"},
        {"jh", "ঝ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "jh"},
        {"Th", "ঠ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "Th"},
        {"th", "থ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "th"},
        {"Dh", "ঢ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "Dh"},
        {"dh", "ধ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "dh"},
        {"ph", "ফ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "ph"},
        {"bh", "ভ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "bh"},
        {"sh", "শ", RuleContext::ANY, OutputCategory::CONSONANT, 250, "sh"},
        {"Sh", "ষ", RuleContext::ANY, OutputCategory::CONSONANT, 255, "Sh"},
        {"Rh", "ঢ়", RuleContext::ANY, OutputCategory::CONSONANT, 260, "Rh"},
        {"k", "ক", RuleContext::ANY, OutputCategory::CONSONANT, 100, "k"},
        {"g", "গ", RuleContext::ANY, OutputCategory::CONSONANT, 100, "g"},
        {"j", "জ", RuleContext::ANY, OutputCategory::CONSONANT, 100, "j"},
        {"T", "ট", RuleContext::ANY, OutputCategory::CONSONANT, 105, "T"},
        {"t", "ত", RuleContext::ANY, OutputCategory::CONSONANT, 100, "t"},
        {"D", "ড", RuleContext::ANY, OutputCategory::CONSONANT, 105, "D"},
        {"d", "দ", RuleContext::ANY, OutputCategory::CONSONANT, 100, "d"},
        {"N", "ণ", RuleContext::ANY, OutputCategory::CONSONANT, 105, "N"},
        {"n", "ন", RuleContext::ANY, OutputCategory::CONSONANT, 100, "n"},
        {"p", "প", RuleContext::ANY, OutputCategory::CONSONANT, 100, "p"},
        {"b", "ব", RuleContext::ANY, OutputCategory::CONSONANT, 100, "b"},
        {"m", "ম", RuleContext::ANY, OutputCategory::CONSONANT, 100, "m"},
        {"r", "র", RuleContext::ANY, OutputCategory::CONSONANT, 100, "r"},
        {"r", "ড়", RuleContext::ANY, OutputCategory::CONSONANT, 85, "r_alt"},
        {"R", "ড়", RuleContext::ANY, OutputCategory::CONSONANT, 110, "R"},
        {"l", "ল", RuleContext::ANY, OutputCategory::CONSONANT, 100, "l"},
        {"s", "স", RuleContext::ANY, OutputCategory::CONSONANT, 100, "s"},
        {"h", "হ", RuleContext::ANY, OutputCategory::CONSONANT, 100, "h"},
        {"y", "য়", RuleContext::ANY, OutputCategory::CONSONANT, 100, "y"},
        {"y", "্য", RuleContext::AFTER_CONSONANT, OutputCategory::CONJUNCT, 220, "y_phala"}
    };

    std::stable_sort(rules.begin(), rules.end(), [](const PhoneticRule& a, const PhoneticRule& b) {
        if (a.pattern.length() != b.pattern.length()) {
            return a.pattern.length() > b.pattern.length();
        }
        return a.priority > b.priority;
    });
}

std::vector<LatticeEdge> Transliterator::find_matching_edges(const std::string& input, size_t pos) const {
    std::vector<LatticeEdge> edges;
    size_t in_len = input.length();

    for (const auto& r : rules) {
        size_t rlen = r.pattern.length();
        if (pos + rlen <= in_len) {
            if (input.compare(pos, rlen, r.pattern) == 0) {
                LatticeEdge edge;
                edge.start_pos = pos;
                edge.end_pos = pos + rlen;
                edge.bangla = r.output;
                edge.context = r.context;
                edge.category = r.category;
                edge.priority = r.priority;
                edges.push_back(edge);
            }
        }
    }
    return edges;
}

namespace {
struct Hypothesis {
    std::string text;
    size_t pos = 0;
    double score = 0.0;
    bool last_was_consonant = false;
    bool has_inherent_vowel = false;
    int consonant_chain = 0;

    bool operator==(const Hypothesis& other) const {
        return pos == other.pos && text == other.text &&
               last_was_consonant == other.last_was_consonant &&
               has_inherent_vowel == other.has_inherent_vowel;
    }
};

bool is_url_or_email(const std::string& str) {
    if (str.find("http://") == 0 || str.find("https://") == 0 || str.find("www.") == 0) return true;
    if (str.find('@') != std::string::npos && str.find('.') != std::string::npos) return true;
    if (str.find(".com") != std::string::npos || str.find(".org") != std::string::npos || str.find(".net") != std::string::npos) return true;
    return false;
}

bool is_uppercase_acronym(const std::string& str) {
    if (str.length() < 2 || str.length() > 10) return false;
    bool has_upper = false;
    for (char c : str) {
        if (std::isupper(static_cast<unsigned char>(c))) {
            has_upper = true;
        } else if (c != '-' && !std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return has_upper;
}
} // namespace

std::vector<std::pair<std::string, double>> Transliterator::transliterate_beam(
    const std::string& input,
    const Dictionary* dict,
    size_t beam_width
) const {
    std::vector<std::pair<std::string, double>> results;
    if (input.empty()) return results;

    if (is_url_or_email(input) || is_uppercase_acronym(input)) {
        return {{input, 10000.0}};
    }

    size_t n = input.length();
    std::vector<std::vector<Hypothesis>> beam(n + 1);

    Hypothesis initial;
    initial.text = "";
    initial.pos = 0;
    initial.score = 0.0;
    initial.last_was_consonant = false;
    initial.has_inherent_vowel = false;
    initial.consonant_chain = 0;
    beam[0].push_back(initial);

    for (size_t pos = 0; pos < n; pos++) {
        if (beam[pos].empty()) continue;

        std::sort(beam[pos].begin(), beam[pos].end(), [](const Hypothesis& a, const Hypothesis& b) {
            return a.score > b.score;
        });
        if (beam[pos].size() > beam_width) {
            beam[pos].resize(beam_width);
        }

        std::vector<LatticeEdge> edges = find_matching_edges(input, pos);

        // Decimal point check: e.g. "3.5"
        if (pos > 0 && pos + 1 < n && input[pos] == '.' &&
            std::isdigit(static_cast<unsigned char>(input[pos - 1])) &&
            std::isdigit(static_cast<unsigned char>(input[pos + 1]))) {
            LatticeEdge decimal_edge;
            decimal_edge.start_pos = pos;
            decimal_edge.end_pos = pos + 1;
            decimal_edge.bangla = ".";
            decimal_edge.context = RuleContext::ANY;
            decimal_edge.category = OutputCategory::PUNCTUATION;
            decimal_edge.priority = 400.0;
            edges.push_back(decimal_edge);
        }

        // Ro-fola: consonant + 'r' (e.g. "pr" -> প্র)
        for (const auto& hyp : beam[pos]) {
            if (hyp.last_was_consonant && !hyp.has_inherent_vowel && input[pos] == 'r' &&
                (pos + 1 == n || input[pos + 1] != 'r')) {
                LatticeEdge ro_fola;
                ro_fola.start_pos = pos;
                ro_fola.end_pos = pos + 1;
                ro_fola.bangla = "\xE0\xA7\x8D\xE0\xA6\xB0"; // ্ + র
                ro_fola.context = RuleContext::AFTER_CONSONANT;
                ro_fola.category = OutputCategory::CONJUNCT;
                ro_fola.priority = 180.0;
                edges.push_back(ro_fola);
                break;
            }
        }

        // Always provide a safe UTF-8 fallback edge so the beam never reaches a dead end
        LatticeEdge fallback;
        fallback.start_pos = pos;

        unsigned char c = static_cast<unsigned char>(input[pos]);
        size_t seq_len = 1;
        bool valid = true;
        if (c < 0x80) {
            seq_len = 1;
        } else if ((c >> 5) == 0x06) {
            seq_len = 2;
            if (pos + 1 >= n || (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 || c < 0xC2) {
                valid = false;
            }
        } else if ((c >> 4) == 0x0E) {
            seq_len = 3;
            if (pos + 2 >= n ||
                (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 2]) & 0xC0) != 0x80) {
                valid = false;
            } else {
                unsigned char c2 = static_cast<unsigned char>(input[pos + 1]);
                if (c == 0xE0 && c2 < 0xA0) valid = false;
                if (c == 0xED && c2 >= 0xA0) valid = false;
            }
        } else if ((c >> 3) == 0x1E) {
            seq_len = 4;
            if (pos + 3 >= n ||
                (static_cast<unsigned char>(input[pos + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(input[pos + 3]) & 0xC0) != 0x80) {
                valid = false;
            } else {
                unsigned char c2 = static_cast<unsigned char>(input[pos + 1]);
                if (c == 0xF0 && c2 < 0x90) valid = false;
                if (c == 0xF4 && c2 >= 0x90) valid = false;
            }
        } else {
            valid = false;
        }

        if (!valid) {
            fallback.end_pos = pos + 1;
            fallback.bangla = "\xEF\xBF\xBD"; // U+FFFD (Replacement Character)
        } else {
            fallback.end_pos = pos + seq_len;
            fallback.bangla = input.substr(pos, seq_len);
        }
        fallback.context = RuleContext::ANY;
        fallback.category = OutputCategory::OTHER;
        fallback.priority = 1.0; // Low baseline priority so real rules always win
        edges.push_back(fallback);

        for (const auto& hyp : beam[pos]) {
            for (const auto& edge : edges) {
                // Context validation
                if (edge.context == RuleContext::START && pos != 0) {
                    if (pos > 0 && !std::isspace(static_cast<unsigned char>(input[pos - 1]))) {
                        continue;
                    }
                }
                if (edge.context == RuleContext::END && edge.end_pos != n) {
                    if (edge.end_pos < n && !std::isspace(static_cast<unsigned char>(input[edge.end_pos]))) {
                        continue;
                    }
                }
                if (edge.context == RuleContext::AFTER_CONSONANT) {
                    if (!hyp.last_was_consonant || hyp.has_inherent_vowel) {
                        continue;
                    }
                }
                if (edge.context == RuleContext::AFTER_VOWEL) {
                    if (hyp.last_was_consonant && !hyp.has_inherent_vowel && pos != 0) {
                        continue;
                    }
                }

                Hypothesis next_hyp;
                next_hyp.pos = edge.end_pos;
                next_hyp.text = hyp.text;
                next_hyp.score = hyp.score + edge.priority;
                next_hyp.consonant_chain = hyp.consonant_chain;

                if (edge.category == OutputCategory::CONSONANT) {
                    if (hyp.last_was_consonant && !hyp.has_inherent_vowel) {
                        // In Bengali phonotactics, 'হ' (h) never follows an un-aspirated stop in a virama cluster
                        if (edge.bangla == "\xE0\xA6\xB9") {
                            next_hyp.score -= 200.0; // Prohibit unmerged t+h, b+h, etc.
                        }
                        next_hyp.text += "\xE0\xA7\x8D"; // ্

                        // Also fork a hypothesis without virama (implicit inherent vowel, e.g. "korbo" -> "করব")
                        Hypothesis no_virama = hyp;
                        no_virama.pos = edge.end_pos;
                        no_virama.text += edge.bangla;
                        no_virama.score = hyp.score + edge.priority - 20.0;
                        no_virama.last_was_consonant = true;
                        no_virama.has_inherent_vowel = false;
                        no_virama.consonant_chain = 1;
                        beam[edge.end_pos].push_back(no_virama);
                    }
                    next_hyp.text += edge.bangla;
                    next_hyp.last_was_consonant = true;
                    next_hyp.has_inherent_vowel = false;
                    next_hyp.consonant_chain = hyp.consonant_chain + 1;
                }
                else if (edge.category == OutputCategory::KAR) {
                    if (edge.end_pos - edge.start_pos == 1 && input[edge.start_pos] == 'o') {
                        bool is_terminal = (edge.end_pos == n);
                        if (is_terminal) {
                            if (hyp.consonant_chain >= 2) {
                                // Final inherent 'o' after conjunct (e.g. "shobdo" -> "শব্দ")
                                next_hyp.last_was_consonant = false;
                                next_hyp.has_inherent_vowel = true;
                                next_hyp.consonant_chain = 0;
                            } else {
                                // Final 'o' after single consonant emits okar (e.g. "bhalo" -> "ভালো")
                                next_hyp.text += edge.bangla;
                                next_hyp.last_was_consonant = false;
                                next_hyp.has_inherent_vowel = false;
                                next_hyp.consonant_chain = 0;

                                // Also fork an alternative with inherent 'o' (e.g. "কত")
                                Hypothesis inherent_terminal = hyp;
                                inherent_terminal.pos = edge.end_pos;
                                inherent_terminal.score = hyp.score + edge.priority - 10.0;
                                inherent_terminal.last_was_consonant = false;
                                inherent_terminal.has_inherent_vowel = true;
                                inherent_terminal.consonant_chain = 0;
                                beam[edge.end_pos].push_back(inherent_terminal);
                            }
                        } else {
                            // Medial inherent 'o' (e.g. "kotha" -> কথা)
                            next_hyp.last_was_consonant = false;
                            next_hyp.has_inherent_vowel = true;
                            next_hyp.consonant_chain = 0;

                            // Also fork an alternative with O-kar (e.g. "কোথাও")
                            Hypothesis okar_medial = hyp;
                            okar_medial.pos = edge.end_pos;
                            okar_medial.text += edge.bangla;
                            okar_medial.score = hyp.score + edge.priority - 20.0;
                            okar_medial.last_was_consonant = false;
                            okar_medial.has_inherent_vowel = false;
                            okar_medial.consonant_chain = 0;
                            beam[edge.end_pos].push_back(okar_medial);
                        }
                    } else {
                        next_hyp.text += edge.bangla;
                        next_hyp.last_was_consonant = false;
                        next_hyp.has_inherent_vowel = false;
                        next_hyp.consonant_chain = 0;
                    }
                }
                else if (edge.category == OutputCategory::DIPHTHONG_SPLIT) {
                    next_hyp.text += edge.bangla;
                    next_hyp.last_was_consonant = false;
                    next_hyp.has_inherent_vowel = false;
                    next_hyp.consonant_chain = 0;
                }
                else {
                    next_hyp.text += edge.bangla;
                    if (edge.category == OutputCategory::INDEPENDENT_VOWEL) {
                        next_hyp.last_was_consonant = false;
                        next_hyp.has_inherent_vowel = false;
                        next_hyp.consonant_chain = 0;
                    } else if (edge.category == OutputCategory::CONJUNCT) {
                        next_hyp.last_was_consonant = true;
                        next_hyp.has_inherent_vowel = false;
                        next_hyp.consonant_chain = hyp.consonant_chain + 2;
                    } else {
                        next_hyp.last_was_consonant = false;
                        next_hyp.has_inherent_vowel = false;
                        next_hyp.consonant_chain = 0;
                    }
                }

                // Intermediate Dictionary Prefix Guidance
                if (dict && !next_hyp.text.empty() && next_hyp.text.length() >= 3) {
                    if (dict->contains(next_hyp.text)) {
                        uint32_t freq = dict->get_frequency(next_hyp.text);
                        next_hyp.score += 80.0 + 20.0 * std::log10(static_cast<double>(freq + 10));
                    }
                }

                bool exists = false;
                for (auto& existing : beam[edge.end_pos]) {
                    if (existing == next_hyp) {
                        if (next_hyp.score > existing.score) {
                            existing.score = next_hyp.score;
                        }
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    beam[edge.end_pos].push_back(next_hyp);
                }
            }
        }
    }

    auto& final_beam = beam[n];
    if (final_beam.empty()) return results;

    for (auto& hyp : final_beam) {
        if (dict && dict->contains(hyp.text)) {
            uint32_t freq = dict->get_frequency(hyp.text);
            hyp.score += 350.0 + 50.0 * std::log10(static_cast<double>(freq + 1));
        }
    }

    std::sort(final_beam.begin(), final_beam.end(), [](const Hypothesis& a, const Hypothesis& b) {
        return a.score > b.score;
    });

    for (const auto& hyp : final_beam) {
        if (hyp.text.empty()) continue;
        std::string norm_text = utf8::normalize_nfc(hyp.text);
        bool seen = false;
        for (const auto& r : results) {
            if (r.first == norm_text) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            results.push_back({norm_text, hyp.score});
            if (results.size() >= beam_width) break;
        }
    }

    return results;
}

std::string Transliterator::transliterate(const std::string& input, const Dictionary* dict) const {
    auto beam_results = transliterate_beam(input, dict, 24);
    if (!beam_results.empty()) {
        return beam_results[0].first;
    }
    return utf8::normalize_nfc(input);
}

std::vector<std::string> Transliterator::get_phonetic_variants(const std::string& input, size_t max_variants) const {
    std::vector<std::string> variants;
    auto beam_results = transliterate_beam(input, nullptr, max_variants * 2);
    for (const auto& r : beam_results) {
        variants.push_back(r.first);
        if (variants.size() >= max_variants) break;
    }
    return variants;
}

} // namespace abk
