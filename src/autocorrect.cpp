#include "abk/autocorrect.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace abk {

namespace {
std::string sanitize_bengali_nfc(const std::string& s) {
    auto cps = utf8::to_codepoints(s);
    std::vector<char32_t> clean;
    clean.reserve(cps.size());
    for (char32_t cp : cps) {
        if (cp == 0x09F0) clean.push_back(0x09B0);      // Never emit Assamese ৰ
        else if (cp == 0x09F1) clean.push_back(0x09AC); // Never emit Assamese ৱ
        else clean.push_back(cp);
    }
    return utf8::normalize_nfc(utf8::to_utf8(clean));
}
} // namespace

AutocorrectTable::AutocorrectTable() {
    std::vector<std::string> paths = {
        "data/autocorrect_default.tsv",
        "../data/autocorrect_default.tsv",
        "../../data/autocorrect_default.tsv",
        "/usr/local/share/amar-bangla-kotha/autocorrect_default.tsv",
        "/usr/share/amar-bangla-kotha/autocorrect_default.tsv"
    };
    bool loaded = false;
    for (const auto& p : paths) {
        if (load_defaults(p)) {
            loaded = true;
            break;
        }
    }
    if (!loaded || entries_.empty()) {
        init_builtin_defaults();
    }
}

AutocorrectTable::AutocorrectTable(const std::string& defaults_filepath) {
    if (!load_defaults(defaults_filepath) || entries_.empty()) {
        init_builtin_defaults();
    }
}

bool AutocorrectTable::has_explicit_control_keys(const std::string& input) {
    if (input.empty()) return false;
    if (input.find('`') != std::string::npos || input.find(",,") != std::string::npos) {
        return true;
    }
    for (unsigned char uc : input) {
        if (uc >= 'A' && uc <= 'Z') {
            return true;
        }
    }
    return false;
}

void AutocorrectTable::init_builtin_defaults() {
    entries_.clear();
    const std::vector<AutocorrectEntry> defaults = {
        {"amra", "আমরা", "conjunct_exception", false},
        {"tomra", "তোমরা", "conjunct_exception", false},
        {"apnara", "আপনারা", "conjunct_exception", false},
        {"amader", "আমাদের", "conjunct_exception", false},
        {"tomader", "তোমাদের", "conjunct_exception", false},
        {"apnader", "আপনাদের", "conjunct_exception", false},
        {"ekta", "একটা", "conjunct_exception", false},
        {"ekti", "একটি", "conjunct_exception", false},
        {"ektu", "একটু", "conjunct_exception", false},
        {"aglano", "আগলানো", "conjunct_exception", false},
        {"korbo", "করব", "conjunct_exception", false},
        {"bolbo", "বলব", "conjunct_exception", false},
        {"dekhbo", "দেখব", "conjunct_exception", false},
        {"korte", "করতে", "conjunct_exception", false},
        {"bolte", "বলতে", "conjunct_exception", false},
        {"dekhte", "দেখতে", "conjunct_exception", false},
        {"dorkar", "দরকার", "conjunct_exception", false},
        {"sorkar", "সরকার", "conjunct_exception", false},
        {"shorkar", "সরকার", "conjunct_exception", false},
        {"tarpor", "তারপর", "conjunct_exception", false},
        {"ajke", "আজকে", "conjunct_exception", false},
        {"kalke", "কালকে", "conjunct_exception", false},
        {"byakti", "ব্যক্তি", "conjunct_exception", false},
        {"lokkhmi", "লক্ষ্মী", "conjunct_exception", false},
        {"software", "সফটওয়্যার", "loanword", false},
        {"doctor", "ডাক্তার", "loanword", false},
        {"school", "স্কুল", "loanword", false},
        {"college", "কলেজ", "loanword", false},
        {"office", "অফিস", "loanword", false},
        {"mobile", "মোবাইল", "loanword", false},
        {"phone", "ফোন", "loanword", false},
        {"hospital", "হাসপাতাল", "loanword", false},
        {"police", "পুলিশ", "loanword", false},
        {"bank", "ব্যাংক", "loanword", false},
        {"video", "ভিডিও", "loanword", false},
        {"internet", "ইন্টারনেট", "loanword", false},
        {"university", "ইউনিভার্সিটি", "loanword", false},
        {"bd", "বাংলাদেশ", "shortcut", false},
        {"dhk", "ঢাকা", "shortcut", false}
    };
    for (const auto& e : defaults) {
        entries_[e.typed] = AutocorrectEntry(e.typed, sanitize_bengali_nfc(e.replacement), e.category, false);
    }
}

bool AutocorrectTable::load_defaults(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;

    entries_.clear();
    loaded_defaults_path_ = filepath;

    std::string line;
    while (std::getline(f, line)) {
        std::string trimmed = utf8::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::stringstream ss(trimmed);
        std::string typed, repl, cat;
        if (std::getline(ss, typed, '\t') && std::getline(ss, repl, '\t')) {
            if (!std::getline(ss, cat, '\t') || cat.empty()) {
                cat = "default";
            }
            typed = utf8::trim(typed);
            repl = sanitize_bengali_nfc(utf8::trim(repl));
            if (!typed.empty() && !repl.empty()) {
                entries_[typed] = AutocorrectEntry(typed, repl, utf8::trim(cat), false);
            }
        }
    }
    return !entries_.empty();
}

void AutocorrectTable::reset_to_defaults() {
    if (!loaded_defaults_path_.empty() && load_defaults(loaded_defaults_path_)) {
        return;
    }
    init_builtin_defaults();
}

bool AutocorrectTable::add_or_update(const std::string& typed_text, const std::string& replacement, const std::string& category) {
    std::string key = utf8::trim(typed_text);
    std::string val = sanitize_bengali_nfc(utf8::trim(replacement));
    if (key.empty() || val.empty()) return false;
    // Never allow explicit control keys to be overridden silently; store exact key
    entries_[key] = AutocorrectEntry(key, val, category.empty() ? "user" : category, true);
    return true;
}

bool AutocorrectTable::remove(const std::string& typed_text) {
    std::string key = utf8::trim(typed_text);
    return entries_.erase(key) > 0;
}

void AutocorrectTable::clear() {
    entries_.clear();
}

bool AutocorrectTable::contains(const std::string& typed_text) const {
    if (!enabled_) return false;
    if (has_explicit_control_keys(typed_text)) return false;
    return entries_.find(typed_text) != entries_.end();
}

std::string AutocorrectTable::lookup(const std::string& typed_text) const {
    if (!enabled_) return "";
    if (has_explicit_control_keys(typed_text)) return "";
    auto it = entries_.find(typed_text);
    if (it == entries_.end()) return "";
    return it->second.replacement;
}

std::vector<AutocorrectEntry> AutocorrectTable::get_all_entries() const {
    std::vector<AutocorrectEntry> out;
    out.reserve(entries_.size());
    for (const auto& kv : entries_) {
        out.push_back(kv.second);
    }
    std::sort(out.begin(), out.end(), [](const AutocorrectEntry& a, const AutocorrectEntry& b) {
        return a.typed < b.typed;
    });
    return out;
}

bool AutocorrectTable::import_tsv(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;

    std::string line;
    size_t imported = 0;
    while (std::getline(f, line)) {
        std::string trimmed = utf8::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::stringstream ss(trimmed);
        std::string typed, repl, cat;
        if (std::getline(ss, typed, '\t') && std::getline(ss, repl, '\t')) {
            if (!std::getline(ss, cat, '\t') || cat.empty()) {
                cat = "user";
            }
            if (add_or_update(typed, repl, utf8::trim(cat))) {
                imported++;
            }
        }
    }
    return imported > 0 || f.eof();
}

bool AutocorrectTable::export_tsv(const std::string& filepath) const {
    if (filepath.empty()) return false;
    std::string tmp_path = filepath + ".tmp";
    {
        std::ofstream f(tmp_path, std::ios::trunc);
        if (!f.is_open()) return false;
        f << "# Amar Bangla Kotha — User-Editable Autocorrect Table\n";
        f << "# Format: typed<TAB>replacement<TAB>category\n";
        auto sorted = get_all_entries();
        for (const auto& e : sorted) {
            f << e.typed << "\t" << e.replacement << "\t" << e.category << "\n";
        }
        f.flush();
    }
    if (std::rename(tmp_path.c_str(), filepath.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }
    return true;
}

Autocorrect::Autocorrect() {}

bool Autocorrect::is_phonetically_similar(char32_t a, char32_t b) const {
    if (a == b) return true;

    // 1. Sibilants (শ, ষ, স)
    if ((a == 0x09B6 || a == 0x09B7 || a == 0x09B8) &&
        (b == 0x09B6 || b == 0x09B7 || b == 0x09B8)) return true;

    // 2. Nasals (ন, ণ)
    if ((a == 0x09A8 || a == 0x09A3) &&
        (b == 0x09A8 || b == 0x09A3)) return true;

    // 3. Short and Long I-kar (ি, ী)
    if ((a == 0x09BF || a == 0x09C0) &&
        (b == 0x09BF || b == 0x09C0)) return true;

    // 4. Short and Long U-kar (ু, ূ)
    if ((a == 0x09C1 || a == 0x09C2) &&
        (b == 0x09C1 || b == 0x09C2)) return true;

    // 5. Independent Vowels (ই, ঈ)
    if ((a == 0x0987 || a == 0x0988) &&
        (b == 0x0987 || b == 0x0988)) return true;

    // 6. Independent Vowels (উ, ঊ)
    if ((a == 0x0989 || a == 0x098A) &&
        (b == 0x0989 || b == 0x098A)) return true;

    // 7. R sounds and Flaps (র, ড়, ঢ়, ড, ঢ)
    if ((a == 0x09B0 || a == 0x09DC || a == 0x09DD || a == 0x09A1 || a == 0x09A2) &&
        (b == 0x09B0 || b == 0x09DC || b == 0x09DD || b == 0x09A1 || b == 0x09A2)) return true;

    // 8. J & Y sounds (জ, য, য়)
    if ((a == 0x099C || a == 0x09AF || a == 0x09DF) &&
        (b == 0x099C || b == 0x09AF || b == 0x09DF)) return true;

    // 9. Ta, Tha, Khanda-Ta, Retroflex (ত, থ, ট, ঠ, ৎ)
    if ((a == 0x09CE || a == 0x09A4 || a == 0x09A5 || a == 0x099F || a == 0x09A0) &&
        (b == 0x09CE || b == 0x09A4 || b == 0x09A5 || b == 0x099F || b == 0x09A0)) return true;

    // 10. Labials (ব, ভ)
    if ((a == 0x09AC || a == 0x09AD) &&
        (b == 0x09AC || b == 0x09AD)) return true;

    // 11. Modifiers (ং, ঁ, ঙ)
    if ((a == 0x0982 || a == 0x0981 || a == 0x0999) &&
        (b == 0x0982 || b == 0x0981 || b == 0x0999)) return true;

    return false;
}

double Autocorrect::substitution_cost(char32_t a, char32_t b) const {
    if (a == b) return 0.0;
    if (is_phonetically_similar(a, b)) return 0.4; // Reduced penalty for common phonetic confusions
    return 1.0;
}

double Autocorrect::compute_distance(const std::string& target, const std::string& candidate) const {
    std::string norm_target = utf8::normalize_nfc(target);
    std::string norm_cand = utf8::normalize_nfc(candidate);

    std::vector<char32_t> s1 = utf8::to_codepoints(norm_target);
    std::vector<char32_t> s2 = utf8::to_codepoints(norm_cand);

    size_t n = s1.size();
    size_t m = s2.size();

    if (n == 0) return static_cast<double>(m);
    if (m == 0) return static_cast<double>(n);

    std::vector<std::vector<double>> d(n + 1, std::vector<double>(m + 1, 0.0));

    for (size_t i = 0; i <= n; i++) d[i][0] = static_cast<double>(i);
    for (size_t j = 0; j <= m; j++) d[0][j] = static_cast<double>(j);

    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            double cost = substitution_cost(s1[i - 1], s2[j - 1]);

            double deletion = d[i - 1][j] + 1.0;
            double insertion = d[i][j - 1] + 1.0;
            double substitution = d[i - 1][j - 1] + cost;

            d[i][j] = std::min({deletion, insertion, substitution});

            // Base + Nukta composite transition (e.g. ড + ় -> র or vice-versa)
            if (i > 1 && s1[i - 1] == utf8::BN_NUKTA) {
                double comp_cost = substitution_cost(s1[i - 2], s2[j - 1]);
                d[i][j] = std::min(d[i][j], d[i - 2][j - 1] + comp_cost);
            }
            if (j > 1 && s2[j - 1] == utf8::BN_NUKTA) {
                double comp_cost = substitution_cost(s1[i - 1], s2[j - 2]);
                d[i][j] = std::min(d[i][j], d[i - 1][j - 2] + comp_cost);
            }

            // Damerau transposition check
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 0.75);
            }
        }
    }

    return d[n][m];
}

void Autocorrect::search_trie_recursive(const Trie& trie,
                                        uint32_t current_node_idx,
                                        char32_t current_cp,
                                        const std::vector<char32_t>& target_cps,
                                        const std::vector<double>& previous_row,
                                        std::vector<CorrectionMatch>& results,
                                        double max_distance,
                                        int& step_count) const {
    if (++step_count > 2500) return; // Bounded search guard (< 1ms)

    size_t cols = target_cps.size() + 1;
    std::vector<double> current_row(cols, 0.0);
    current_row[0] = previous_row[0] + 1.0;

    double min_in_row = current_row[0];

    for (size_t c = 1; c < cols; c++) {
        double insert_or_del = std::min(current_row[c - 1] + 1.0, previous_row[c] + 1.0);
        double cost = substitution_cost(target_cps[c - 1], current_cp);
        double replace = previous_row[c - 1] + cost;

        current_row[c] = std::min(insert_or_del, replace);
        if (current_row[c] < min_in_row) {
            min_in_row = current_row[c];
        }
    }

    const auto& node = trie.get_node(current_node_idx);

    if (current_row.back() <= max_distance && node.is_terminal) {
        results.push_back({node.word, current_row.back(), node.frequency});
    }

    if (min_in_row <= max_distance) {
        for (const auto& child : node.children) {
            search_trie_recursive(trie, child.node_index, child.cp, target_cps, current_row, results, max_distance, step_count);
            if (step_count > 2500) break;
        }
    }
}

std::vector<CorrectionMatch> Autocorrect::suggest_corrections(const Trie& trie,
                                                             const std::string& target,
                                                             size_t max_distance,
                                                             size_t limit) const {
    std::vector<CorrectionMatch> results;
    if (target.empty() || trie.size() == 0) return results;

    std::string norm_target = utf8::normalize_nfc(target);
    std::vector<char32_t> target_cps = utf8::to_codepoints(norm_target);
    size_t cols = target_cps.size() + 1;

    std::vector<double> initial_row(cols, 0.0);
    for (size_t i = 0; i < cols; i++) {
        initial_row[i] = static_cast<double>(i);
    }

    int step_count = 0;
    const auto& root = trie.get_node(trie.get_root_index());

    for (const auto& child : root.children) {
        search_trie_recursive(trie, child.node_index, child.cp, target_cps, initial_row, results, static_cast<double>(max_distance), step_count);
        if (step_count > 2500) break;
    }

    std::sort(results.begin(), results.end(), [](const CorrectionMatch& a, const CorrectionMatch& b) {
        if (std::fabs(a.edit_distance - b.edit_distance) > 0.05) {
            return a.edit_distance < b.edit_distance;
        }
        return a.frequency > b.frequency;
    });

    std::vector<CorrectionMatch> unique_results;
    for (const auto& m : results) {
        bool seen = false;
        for (const auto& u : unique_results) {
            if (u.word == m.word) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            unique_results.push_back(m);
            if (unique_results.size() >= limit) break;
        }
    }

    return unique_results;
}

} // namespace abk
