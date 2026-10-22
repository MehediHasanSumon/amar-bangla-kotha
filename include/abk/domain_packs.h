#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace abk {

enum class DomainPackType {
    BANGLADESH_PLACES,
    ISLAMIC_TERMS,
    LEGAL_MEDICAL_BANKING
};

struct DomainEntry {
    std::string key;
    std::string word;
    int frequency = 0;
    std::string category;
};

/**
 * @brief Manages domain dictionary packs that can be enabled or disabled per user.
 */
class DomainPackManager {
public:
    DomainPackManager();

    /**
     * @brief Loads a domain pack file from disk.
     * 
     * @param type The type of pack.
     * @param filepath Path to the tab-delimited .tsv pack file.
     * @return true if successfully loaded.
     */
    bool load_pack(DomainPackType type, const std::string& filepath);

    /**
     * @brief Toggles a domain pack on or off.
     */
    void set_pack_enabled(DomainPackType type, bool enabled);

    /**
     * @brief Checks if a domain pack is currently enabled.
     */
    bool is_pack_enabled(DomainPackType type) const;

    /**
     * @brief Retrieves all active entries from enabled domain packs.
     */
    std::vector<DomainEntry> get_active_entries() const;

    /**
     * @brief Retrieves all entries belonging to a specific domain pack.
     */
    const std::vector<DomainEntry>& get_pack_entries(DomainPackType type) const;

    /**
     * @brief Returns total number of words in a domain pack.
     */
    size_t get_pack_size(DomainPackType type) const;

private:
    std::unordered_map<DomainPackType, bool> enabled_status_;
    std::unordered_map<DomainPackType, std::vector<DomainEntry>> packs_;
};

} // namespace abk
