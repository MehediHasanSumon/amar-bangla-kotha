#include "abk/autocorrect.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <cmath>

namespace abk {

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
