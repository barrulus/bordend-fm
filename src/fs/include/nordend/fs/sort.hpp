#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "nordend/fs/entry.hpp"

namespace nordend::fs {

// Natural name order, as people read file names: runs of digits compare by value
// ("IMG_2" < "IMG_10"), letters ignore ASCII case. Bytes outside ASCII compare as bytes, so
// accented letters sort after z (no locale collation yet). Ties fall back to plain byte order, so
// the order is total and stable across runs.
[[nodiscard]] int natural_compare(std::string_view a, std::string_view b) noexcept;

enum class SortKey : std::uint8_t { name, size, modified, type };

struct SortOrder {
    SortKey key{SortKey::name};
    bool descending{false};
    bool directories_first{true};  // directories (and links to them) before everything else
    // For SortKey::type: the type names, alphabetically ("image/jpeg" < "text/plain"); entries
    // whose name does not tell their type come last. Without it, types group in no set order.
    const MimeDatabase* mime{nullptr};

    // Strict weak order on entries; ties on size, date or type fall back to the name. By size,
    // directories compare by name among themselves.
    [[nodiscard]] bool operator()(const Entry& a, const Entry& b) const noexcept;
};

// A directory's entries kept sorted while they arrive in chunks.
class Listing {
public:
    explicit Listing(SortOrder order = {}) : order_{order} {}

    // Sorts the chunk and merges it in: O(chunk log chunk + size).
    void add(std::vector<Entry> chunk);

    // Removes the entries named in remove and those named like an entry of insert, then merges
    // insert in: one pass for a whole batch of changes.
    void replace(const std::unordered_set<std::string>& remove, std::vector<Entry> insert);

    // Re-sorts everything with a new order.
    void set_order(SortOrder order);

    void clear() { entries_.clear(); }

    [[nodiscard]] const SortOrder& order() const noexcept { return order_; }

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }

private:
    SortOrder order_;
    std::vector<Entry> entries_;
};

}  // namespace nordend::fs
