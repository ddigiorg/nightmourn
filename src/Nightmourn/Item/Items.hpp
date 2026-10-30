// =============================================================================
// Item/Items.hpp
//
// Nightmourn's item ids (Nezumi::ItemID): the single place they are
// assigned, so two items can never share one. Only the raw materials exist
// so far. Inventories (Item/Inventory) count items by these, in stacks of up
// to item_stack_size().
// =============================================================================

#pragma once
#include "Nezumi/World/Item/ItemID.hpp"
#include <array>       // array
#include <cstdint>     // uint16_t
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

// Most of `item` one inventory slot holds (Factorio's stack size): 100 wood
// or stone, 50 gold, 20 food. 1 for an item without a stack size yet.
constexpr std::uint16_t item_stack_size(Nezumi::ItemID item) {
    constexpr std::array<std::uint16_t, 4> STACKS = { 100, 100, 50, 20 };
    return item.value() < STACKS.size() ? STACKS[item.value()] : 1;
}

} // namespace Nightmourn
