// =============================================================================
// Item/Items.hpp
//
// Nightmourn's items. What each item is (its name, icon and stack size) is
// data: an "item" config in data/ (Data/Configs.hpp), with ItemIDs given by
// load order. Configs::load hands the items to set_items(), and from then on
// item_name() and item_stack_size() (and so inventories, Item/Inventory)
// go by them.
//
// The base items the code names come first, in this order, which
// Configs::load checks:
// =============================================================================

#pragma once
#include "Nightmourn/Item/ItemID.hpp"
#include <cstdint>     // uint16_t
#include <string>      // string
#include <string_view> // string_view
#include <vector>      // vector

namespace Nightmourn {

namespace Items {
    inline constexpr ItemID Wood { 0 };
    inline constexpr ItemID Stone{ 1 };
    inline constexpr ItemID Gold { 2 };
    inline constexpr ItemID Food { 3 };
}

// What the simulation needs to know about an item, by ItemID
struct ItemInfo {
    std::string   name;
    std::uint16_t stack_size = 1;
};

// The items, indexed by ItemID; replaces any set before. Not thread safe:
// call it at load, before anything asks.
void set_items(std::vector<ItemInfo> items);

// Lowercase name of `item` ("wood"), or "item" for one that isn't loaded
std::string_view item_name(ItemID item);

// Most of `item` one inventory slot holds (Factorio's stack size), or 1
// for an item that isn't loaded
std::uint16_t item_stack_size(ItemID item);

} // namespace Nightmourn
