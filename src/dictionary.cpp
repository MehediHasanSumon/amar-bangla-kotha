#include "abk/dictionary.h"
#include "abk/utf8_util.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace abk {

Dictionary::Dictionary() {
    load_builtin_words();
}

void Dictionary::load_builtin_words() {
    const std::vector<std::pair<std::string, uint32_t>> builtin_corpus = {
        {"আমি", 100000}, {"আমার", 98000}, {"আমাকে", 85000}, {"আমরা", 82000}, {"আমাদের", 84000},
        {"তুমি", 92000}, {"তোমার", 89000}, {"তোমাকে", 79000}, {"তোমরা", 71000}, {"তোমাদের", 73000},
        {"আপনি", 87000}, {"আপনার", 88000}, {"আপনাকে", 81000}, {"আপনারা", 67000}, {"আপনাদের", 69000},
        {"সে", 91000}, {"তার", 93000}, {"তাকে", 86000}, {"তারা", 81000}, {"তাদের", 83000},
        {"তিনি", 77000}, {"তাঁর", 78000}, {"তাঁকে", 74000}, {"তাঁরা", 62000}, {"তাঁদের", 64000},
        {"এই", 94000}, {"এইটা", 62000}, {"এটি", 67000}, {"এগুলো", 57000}, {"এর", 82000},
        {"ওই", 62000}, {"সেটা", 67000}, {"ওটা", 57000},
        {"কে", 87000}, {"কার", 72000}, {"কাকে", 67000}, {"কারা", 57000},
        {"কী", 91000}, {"কি", 96000}, {"কেন", 84000}, {"কোথায়", 80000}, {"কখন", 77000},
        {"কিভাবে", 74000}, {"কেমন", 82000}, {"কত", 88000}, {"কতো", 65000},

        {"এবং", 96000}, {"ও", 92000}, {"কিন্তু", 87000}, {"অথবা", 72000}, {"বা", 77000},
        {"তবে", 67000}, {"তাই", 74000}, {"কারণ", 94000}, {"কারন", 60000}, {"যেন", 62000},
        {"যদি", 82000}, {"তো", 84000}, {"না", 99000}, {"হ্যাঁ", 87000}, {"আর", 92000},
        {"রে", 89000}, {"জন্য", 90000}, {"সাথে", 87000}, {"নিয়ে", 78000},
        {"এখন", 87000}, {"তখন", 72000}, {"শুধু", 80000}, {"খুব", 92000}, {"অনেক", 89000},
        {"ভালো", 94000}, {"ভাল", 15000}, {"সুন্দর", 88000}, {"নতুন", 86000},

        {"কই", 92000}, {"কৈ", 15000}, {"বই", 84000}, {"দই", 82000}, {"মই", 75000},
        {"কথা", 92000}, {"শব্দ", 77000}, {"ভাষা", 87000}, {"বাংলা", 99500}, {"বাংলাদেশ", 99000},
        {"মানুষ", 91000}, {"দেশ", 88000}, {"দিন", 87000}, {"রাত", 82000},
        {"বাড়ি", 86000}, {"পানি", 86000}, {"শিক্ষা", 84000}, {"জীবন", 89000},
        {"ধন্যবাদ", 96000}, {"প্রথম", 87000}, {"শেষ", 91000}, {"কাজ", 88000},
        {"করব", 95000}, {"করবো", 94000}, {"হবে", 87000}, {"আছে", 93000}
    };

    for (const auto& item : builtin_corpus) {
        system_trie.insert(item.first, item.second);
        index_skeleton_word(item.first, item.second);
    }
}

std::string Dictionary::compute_skeleton(const std::string& bangla_word) {
    std::string norm = utf8::normalize_nfc(bangla_word);
    auto cps = utf8::to_codepoints(norm);
    std::vector<char32_t> skel;
    skel.reserve(cps.size());

    for (size_t i = 0; i < cps.size(); ) {
        char32_t cp = cps[i];
        bool next_nukta = (i + 1 < cps.size() && cps[i + 1] == utf8::BN_NUKTA);
        if (next_nukta) {
            if (cp == 0x09A1 || cp == 0x09A2) {
                skel.push_back(0x09B0); // ড়, ঢ় -> র
            } else if (cp == 0x09AF) {
                skel.push_back(0x09DF); // য়
            } else if (cp == 0x099C) {
                skel.push_back(0x099C); // জ় -> জ
            } else {
                skel.push_back(cp);
            }
            i += 2;
            continue;
        }

        switch (cp) {
            case 0x09DC: case 0x09DD: case 0x09B0:
                skel.push_back(0x09B0); // র / ড় / ঢ়
                break;
            case 0x09DF:
                skel.push_back(0x09DF); // য়
                break;
            case 0x099C: case 0x09AF:
                skel.push_back(0x099C); // জ / য
                break;
            case 0x09B6: case 0x09B7: case 0x09B8:
                skel.push_back(0x09B6); // শ / ষ / স
                break;
            case 0x09A4: case 0x099F: case 0x09CE:
                skel.push_back(0x09A4); // ত / ট / ৎ
                break;
            case 0x09A5: case 0x09A0:
                skel.push_back(0x09A5); // থ / ঠ
                break;
            case 0x09A6: case 0x09A1:
                skel.push_back(0x09A6); // দ / ড
                break;
            case 0x09A7: case 0x09A2:
                skel.push_back(0x09A7); // ধ / ঢ
                break;
            case 0x09A8: case 0x09A3:
                skel.push_back(0x09A8); // ন / ণ
                break;
            case 0x0999: case 0x0982:
                skel.push_back(0x0982); // ঙ / ং
                break;
            case 0x0987: case 0x0988:
                skel.push_back(0x0987); // ই / ঈ
                break;
            case 0x09BF: case 0x09C0:
                skel.push_back(0x09BF); // ি / ী
                break;
            case 0x0989: case 0x098A:
                skel.push_back(0x0989); // উ / ঊ
                break;
            case 0x09C1: case 0x09C2:
                skel.push_back(0x09C1); // ু / ূ
                break;
            case 0x09CB: // ো (collapsed with inherent 'o')
            case 0x0981: // ঁ (optional nasalization)
            case 0x200C: // ZWNJ
            case 0x200D: // ZWJ
                break;
            default:
                skel.push_back(cp);
                break;
        }
        ++i;
    }

    return utf8::to_utf8(skel);
}

void Dictionary::index_skeleton_word(const std::string& word, uint32_t frequency) {
    std::string norm = utf8::normalize_nfc(word);
    if (norm.empty()) return;
    std::string skel = compute_skeleton(norm);
    if (skel.empty()) return;

    auto& bucket = skeleton_index[skel];
    for (auto& entry : bucket) {
        if (entry.first == norm) {
            if (frequency > entry.second) {
                entry.second = frequency;
            }
            return;
        }
    }
    bucket.emplace_back(norm, frequency);
}

std::vector<std::pair<std::string, uint32_t>> Dictionary::lookup_by_skeleton(const std::string& bangla_query, size_t limit) const {
    std::vector<std::pair<std::string, uint32_t>> out;
    if (bangla_query.empty() || limit == 0) return out;

    std::string skel = compute_skeleton(bangla_query);
    auto it = skeleton_index.find(skel);
    if (it == skeleton_index.end()) return out;

    out = it->second;
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    if (out.size() > limit) {
        out.resize(limit);
    }
    return out;
}

void Dictionary::parse_line_and_insert(const std::string& line, Trie& target_trie) {
    std::string trimmed = utf8::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;

    std::stringstream ss(trimmed);
    std::string word;
    uint32_t freq = 100;

    ss >> word;
    if (ss >> freq) {
        // frequency provided
    }
    if (!word.empty()) {
        std::string norm = utf8::normalize_nfc(word);
        target_trie.insert(norm, freq);
        index_skeleton_word(norm, freq);
    }
}

bool Dictionary::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        parse_line_and_insert(line, system_trie);
    }
    return true;
}

bool Dictionary::load_user_dictionary(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        parse_line_and_insert(line, user_trie);
    }
    return true;
}

bool Dictionary::save_user_word(const std::string& filepath, const std::string& word, uint32_t frequency) {
    std::string norm = utf8::normalize_nfc(word);
    user_trie.insert(norm, frequency);
    index_skeleton_word(norm, frequency);
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) return false;

    file << norm << " " << frequency << "\n";
    return true;
}

void Dictionary::add_word(const std::string& word, uint32_t frequency) {
    std::string norm = utf8::normalize_nfc(word);
    user_trie.insert(norm, frequency);
    index_skeleton_word(norm, frequency);
}

bool Dictionary::contains(const std::string& word) const {
    if (user_trie.search(word)) return true;
    return system_trie.search(word);
}

uint32_t Dictionary::get_frequency(const std::string& word) const {
    uint32_t freq = 0;
    if (user_trie.search(word, &freq)) {
        return freq;
    }
    if (system_trie.search(word, &freq)) {
        return freq;
    }
    return 0;
}

std::vector<std::pair<std::string, uint32_t>> Dictionary::get_prefix_matches(const std::string& prefix, size_t limit) const {
    std::vector<std::pair<std::string, uint32_t>> results;

    // Collect from user trie first (user words get precedence)
    auto user_matches = user_trie.find_by_prefix(prefix, limit);
    for (auto& match : user_matches) {
        results.push_back(match);
    }

    // Collect from system trie
    auto sys_matches = system_trie.find_by_prefix(prefix, limit * 2);
    for (const auto& match : sys_matches) {
        bool already_present = false;
        for (const auto& existing : results) {
            if (existing.first == match.first) {
                already_present = true;
                break;
            }
        }
        if (!already_present) {
            results.push_back(match);
        }
        if (results.size() >= limit * 2) break;
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

} // namespace abk
