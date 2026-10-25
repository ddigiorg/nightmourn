// =============================================================================
// Item/Items.hpp
//
// Nightmourn's item ids (Nezumi::ItemID): the single place they are
// assigned, so two items can never share one. Only the raw materials exist
// so far; a storage system will count items by these.
// =============================================================================

#pragma once
#include "Nezumi/World/Item/ItemID.hpp"
#include <array>       // array
#include <string_view> // string_view

namespace Nightmourn {

namespace Items {
    inline constexpr Nezumi::ItemID Wood { 0 };
    inline constexpr Nezumi::ItemID Stone{ 1 };
    inline constexpr Nezumi::ItemID Gold { 2 };
    inline constexpr Nezumi::ItemID Food { 3 };
}

// Lowercase name of `item` ("wood"), or "item" for one without a name yet
constexpr std::string_view item_name(Nezumi::ItemID item) {
    constexpr std::array<std::string_view, 4> NAMES = { "wood", "stone", "gold", "food" };
    return item.value() < NAMES.size() ? NAMES[item.value()] : "item";
}

} // namespace Nightmourn
