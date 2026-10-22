#include "abk/bigram_model.h"
#include "abk/utf8_util.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace abk {

BigramModel::BigramModel() : max_capacity(50000), total_bigram_pairs(0), total_trigram_triplets(0), enable_trigram(false) {}

BigramModel::BigramModel(size_t max_entries)
    : max_capacity(max_entries), total_bigram_pairs(0), total_trigram_triplets(0), enable_trigram(false) {}

void BigramModel::add_bigram(const std::string& raw_w1, const std::string& raw_w2, uint32_t count) {
    if (raw_w1.empty() || raw_w2.empty() || count == 0) return;

    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    std::string w2 = utf8::normalize_nfc(utf8::trim(raw_w2));
    if (w1.empty() || w2.empty()) return;

    auto& list = bigrams[w1];
    bool found = false;
    for (auto& entry : list) {
        if (entry.word == w2) {
            entry.count += count;
            found = true;
            break;
        }
    }

    if (!found) {
        list.emplace_back(w2, count);
        total_bigram_pairs++;
    }

    context_totals[w1] += count;

    if (total_bigram_pairs > max_capacity) {
        prune_if_needed();
    }
}

void BigramModel::add_trigram(const std::string& raw_w1, const std::string& raw_w2, const std::string& raw_w3, uint32_t count) {
    if (!enable_trigram || raw_w1.empty() || raw_w2.empty() || raw_w3.empty() || count == 0) return;

    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    std::string w2 = utf8::normalize_nfc(utf8::trim(raw_w2));
    std::string w3 = utf8::normalize_nfc(utf8::trim(raw_w3));
    if (w1.empty() || w2.empty() || w3.empty()) return;

    std::string key = w1 + "\t" + w2;
    auto& list = trigrams[key];
    bool found = false;
    for (auto& entry : list) {
        if (entry.word == w3) {
            entry.count += count;
            found = true;
            break;
        }
    }

    if (!found) {
        list.emplace_back(w3, count);
        total_trigram_triplets++;
    }

    trigram_context_totals[key] += count;
}

std::vector<std::pair<std::string, double>> BigramModel::predict_next_words(const std::string& raw_prev_word, size_t limit) const {
    std::vector<std::pair<std::string, double>> results;
    if (raw_prev_word.empty()) return results;

    std::string prev = utf8::normalize_nfc(utf8::trim(raw_prev_word));
    auto it = bigrams.find(prev);
    if (it == bigrams.end() || it->second.empty()) {
        return results;
    }

    uint64_t total = 0;
    auto tot_it = context_totals.find(prev);
    if (tot_it != context_totals.end() && tot_it->second > 0) {
        total = tot_it->second;
    } else {
        for (const auto& entry : it->second) total += entry.count;
    }

    std::vector<BigramEntry> sorted_entries = it->second;
    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const BigramEntry& a, const BigramEntry& b) {
        return a.count > b.count;
    });

    double denom = static_cast<double>(total > 0 ? total : 1);
    for (size_t i = 0; i < sorted_entries.size() && i < limit; ++i) {
        double prob = static_cast<double>(sorted_entries[i].count) / denom;
        results.emplace_back(sorted_entries[i].word, prob);
    }

    return results;
}

std::vector<std::pair<std::string, double>> BigramModel::predict_trigram_next_words(
    const std::string& raw_w1, const std::string& raw_w2, size_t limit) const {
    if (!enable_trigram) {
        return predict_next_words(raw_w2, limit);
    }

    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    std::string w2 = utf8::normalize_nfc(utf8::trim(raw_w2));
    if (w1.empty() || w2.empty()) {
        return predict_next_words(raw_w2, limit);
    }

    std::string key = w1 + "\t" + w2;
    auto it = trigrams.find(key);
    if (it == trigrams.end() || it->second.empty()) {
        // Fallback to bigram on w2
        return predict_next_words(raw_w2, limit);
    }

    uint64_t total = 0;
    auto tot_it = trigram_context_totals.find(key);
    if (tot_it != trigram_context_totals.end() && tot_it->second > 0) {
        total = tot_it->second;
    } else {
        for (const auto& entry : it->second) total += entry.count;
    }

    std::vector<BigramEntry> sorted_entries = it->second;
    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const BigramEntry& a, const BigramEntry& b) {
        return a.count > b.count;
    });

    std::vector<std::pair<std::string, double>> results;
    double denom = static_cast<double>(total > 0 ? total : 1);
    for (size_t i = 0; i < sorted_entries.size() && i < limit; ++i) {
        double prob = static_cast<double>(sorted_entries[i].count) / denom;
        results.emplace_back(sorted_entries[i].word, prob);
    }

    return results;
}

double BigramModel::get_bigram_prob(const std::string& raw_w1, const std::string& raw_w2) const {
    if (raw_w1.empty() || raw_w2.empty()) return 0.0;

    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    std::string w2 = utf8::normalize_nfc(utf8::trim(raw_w2));

    auto it = bigrams.find(w1);
    if (it == bigrams.end() || it->second.empty()) return 0.0;

    uint32_t count = 0;
    for (const auto& entry : it->second) {
        if (entry.word == w2) {
            count = entry.count;
            break;
        }
    }

    if (count == 0) return 0.0;

    uint64_t total = 1;
    auto tot_it = context_totals.find(w1);
    if (tot_it != context_totals.end() && tot_it->second > 0) {
        total = tot_it->second;
    }

    return static_cast<double>(count) / static_cast<double>(total);
}

double BigramModel::get_trigram_prob(const std::string& raw_w1, const std::string& raw_w2, const std::string& raw_w3) const {
    if (!enable_trigram) return get_bigram_prob(raw_w2, raw_w3);

    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    std::string w2 = utf8::normalize_nfc(utf8::trim(raw_w2));
    std::string w3 = utf8::normalize_nfc(utf8::trim(raw_w3));

    std::string key = w1 + "\t" + w2;
    auto it = trigrams.find(key);
    if (it == trigrams.end() || it->second.empty()) {
        // Fallback to bigram
        return get_bigram_prob(raw_w2, raw_w3);
    }

    uint32_t count = 0;
    for (const auto& entry : it->second) {
        if (entry.word == w3) {
            count = entry.count;
            break;
        }
    }

    if (count == 0) return get_bigram_prob(raw_w2, raw_w3);

    uint64_t total = 1;
    auto tot_it = trigram_context_totals.find(key);
    if (tot_it != trigram_context_totals.end() && tot_it->second > 0) {
        total = tot_it->second;
    }

    return static_cast<double>(count) / static_cast<double>(total);
}

bool BigramModel::has_context(const std::string& raw_w1) const {
    if (raw_w1.empty()) return false;
    std::string w1 = utf8::normalize_nfc(utf8::trim(raw_w1));
    auto it = bigrams.find(w1);
    return it != bigrams.end() && !it->second.empty();
}

bool BigramModel::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = utf8::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::stringstream ss(trimmed);
        std::string w1, w2, count_str;
        if (std::getline(ss, w1, '\t') && std::getline(ss, w2, '\t')) {
            uint32_t count = 1;
            if (std::getline(ss, count_str, '\t') && !count_str.empty()) {
                try {
                    count = static_cast<uint32_t>(std::stoul(count_str));
                } catch (...) {
                    count = 1;
                }
            }
            add_bigram(w1, w2, count);
        }
    }

    return true;
}

bool BigramModel::save_to_file(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "# Amar Bangla Kotha Bigram Language Model\n";
    file << "# Format: word1<TAB>word2<TAB>count\n";

    for (const auto& pair : bigrams) {
        const std::string& w1 = pair.first;
        for (const auto& entry : pair.second) {
            file << w1 << "\t" << entry.word << "\t" << entry.count << "\n";
        }
    }

    return true;
}

void BigramModel::clear() {
    bigrams.clear();
    context_totals.clear();
    trigrams.clear();
    trigram_context_totals.clear();
    total_bigram_pairs = 0;
    total_trigram_triplets = 0;
}

void BigramModel::prune_if_needed() {
    if (total_bigram_pairs <= max_capacity) return;

    // Prune entries with count <= 1 first
    uint32_t threshold = 1;
    while (total_bigram_pairs > (max_capacity * 9) / 10) {
        for (auto it = bigrams.begin(); it != bigrams.end();) {
            auto& list = it->second;
            size_t before_sz = list.size();

            list.erase(std::remove_if(list.begin(), list.end(), [threshold](const BigramEntry& e) {
                return e.count <= threshold;
            }), list.end());

            size_t after_sz = list.size();
            total_bigram_pairs -= (before_sz - after_sz);

            if (list.empty()) {
                context_totals.erase(it->first);
                it = bigrams.erase(it);
            } else {
                ++it;
            }
        }
        threshold++;
        if (threshold > 10) break; // Safety bound
    }
}

size_t BigramModel::memory_bytes() const {
    size_t bytes = sizeof(*this);
    for (const auto& pair : bigrams) {
        bytes += pair.first.capacity();
        bytes += pair.second.capacity() * sizeof(BigramEntry);
        for (const auto& entry : pair.second) {
            bytes += entry.word.capacity();
        }
    }
    bytes += context_totals.size() * (sizeof(std::string) + sizeof(uint64_t));
    return bytes;
}

} // namespace abk
