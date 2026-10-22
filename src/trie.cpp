#include "abk/trie.h"
#include "abk/utf8_util.h"
#include <algorithm>
#include <queue>

namespace abk {

Trie::Trie() : word_count(0) {
    // Root is node index 0
    nodes.emplace_back();
}

uint32_t Trie::find_child(uint32_t node_idx, char32_t cp) const {
    if (node_idx >= nodes.size()) return 0;
    const auto& children = nodes[node_idx].children;
    auto it = std::lower_bound(children.begin(), children.end(), TrieChild{cp, 0});
    if (it != children.end() && it->cp == cp) {
        return it->node_index;
    }
    return 0;
}

void Trie::insert(const std::string& word, uint32_t frequency) {
    if (word.empty()) return;
    std::string norm_word = utf8::normalize_nfc(word);
    std::vector<char32_t> codepoints = utf8::to_codepoints(norm_word);
    uint32_t current_idx = 0;

    for (char32_t cp : codepoints) {
        uint32_t next_idx = find_child(current_idx, cp);
        if (next_idx != 0) {
            current_idx = next_idx;
        } else {
            uint32_t new_node_idx = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();

            // Re-fetch reference since emplace_back might reallocate vector
            auto& curr_children = nodes[current_idx].children;
            auto ins_it = std::lower_bound(curr_children.begin(), curr_children.end(), TrieChild{cp, 0});
            curr_children.insert(ins_it, TrieChild{cp, new_node_idx});
            current_idx = new_node_idx;
        }
    }

    if (!nodes[current_idx].is_terminal) {
        nodes[current_idx].is_terminal = true;
        word_count++;
    }
    nodes[current_idx].frequency = std::max(nodes[current_idx].frequency, frequency);
    nodes[current_idx].word = norm_word;
}

bool Trie::search(const std::string& word, uint32_t* out_frequency) const {
    if (word.empty() || nodes.empty()) return false;
    std::string norm_word = utf8::normalize_nfc(word);
    std::vector<char32_t> codepoints = utf8::to_codepoints(norm_word);
    uint32_t current_idx = 0;

    for (char32_t cp : codepoints) {
        current_idx = find_child(current_idx, cp);
        if (current_idx == 0) return false;
    }

    if (nodes[current_idx].is_terminal) {
        if (out_frequency) {
            *out_frequency = nodes[current_idx].frequency;
        }
        return true;
    }
    return false;
}

uint32_t Trie::get_frequency(const std::string& word) const {
    uint32_t freq = 0;
    if (search(word, &freq)) {
        return freq;
    }
    return 0;
}

void Trie::collect_prefix_bounded(uint32_t node_idx,
                                 std::vector<std::pair<std::string, uint32_t>>& results,
                                 size_t limit) const {
    if (node_idx >= nodes.size()) return;

    // BFS with priority to visit nodes and collect words
    std::queue<uint32_t> q;
    q.push(node_idx);

    while (!q.empty()) {
        uint32_t curr = q.front();
        q.pop();

        if (nodes[curr].is_terminal) {
            results.push_back({nodes[curr].word, nodes[curr].frequency});
            if (results.size() >= limit * 2) {
                // Saturated search bound
                break;
            }
        }

        for (const auto& child : nodes[curr].children) {
            q.push(child.node_index);
        }
    }
}

std::vector<std::pair<std::string, uint32_t>> Trie::find_by_prefix(const std::string& prefix, size_t limit) const {
    std::vector<std::pair<std::string, uint32_t>> results;
    if (prefix.empty() || nodes.empty()) return results;

    std::string norm_prefix = utf8::normalize_nfc(prefix);
    std::vector<char32_t> codepoints = utf8::to_codepoints(norm_prefix);
    uint32_t current_idx = 0;

    for (char32_t cp : codepoints) {
        current_idx = find_child(current_idx, cp);
        if (current_idx == 0) return results;
    }

    collect_prefix_bounded(current_idx, results, limit * 4);

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

size_t Trie::memory_bytes() const {
    size_t bytes = sizeof(Trie);
    bytes += nodes.capacity() * sizeof(CompactTrieNode);
    for (const auto& node : nodes) {
        bytes += node.children.capacity() * sizeof(TrieChild);
        bytes += node.word.capacity();
    }
    return bytes;
}

} // namespace abk
