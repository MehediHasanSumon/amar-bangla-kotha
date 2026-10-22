#include "abk/domain_packs.h"
#include <fstream>
#include <sstream>

namespace abk {

DomainPackManager::DomainPackManager() {
    enabled_status_[DomainPackType::BANGLADESH_PLACES] = true;
    enabled_status_[DomainPackType::ISLAMIC_TERMS] = true;
    enabled_status_[DomainPackType::LEGAL_MEDICAL_BANKING] = true;
}

bool DomainPackManager::load_pack(DomainPackType type, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::vector<DomainEntry> entries;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string key, word, freq_str, cat;
        if (std::getline(ss, key, '\t') && std::getline(ss, word, '\t')) {
            int freq = 1000;
            if (std::getline(ss, freq_str, '\t') && !freq_str.empty()) {
                try {
                    freq = std::stoi(freq_str);
                } catch (...) {
                    freq = 1000;
                }
            }
            std::getline(ss, cat, '\t');
            entries.push_back({key, word, freq, cat});
        }
    }

    packs_[type] = std::move(entries);
    return true;
}

void DomainPackManager::set_pack_enabled(DomainPackType type, bool enabled) {
    enabled_status_[type] = enabled;
}

bool DomainPackManager::is_pack_enabled(DomainPackType type) const {
    auto it = enabled_status_.find(type);
    return it != enabled_status_.end() ? it->second : false;
}

std::vector<DomainEntry> DomainPackManager::get_active_entries() const {
    std::vector<DomainEntry> active;
    for (const auto& [type, is_en] : enabled_status_) {
        if (is_en) {
            auto it = packs_.find(type);
            if (it != packs_.end()) {
                active.insert(active.end(), it->second.begin(), it->second.end());
            }
        }
    }
    return active;
}

const std::vector<DomainEntry>& DomainPackManager::get_pack_entries(DomainPackType type) const {
    static const std::vector<DomainEntry> empty_entries;
    auto it = packs_.find(type);
    return it != packs_.end() ? it->second : empty_entries;
}

size_t DomainPackManager::get_pack_size(DomainPackType type) const {
    auto it = packs_.find(type);
    return it != packs_.end() ? it->second.size() : 0;
}

} // namespace abk
