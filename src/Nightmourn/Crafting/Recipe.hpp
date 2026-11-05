// =============================================================================
// Crafting/Recipe.hpp
//
// How one thing is made: the items it takes, the items it makes, how long it
// takes, and where it can be made. A "recipe" config in data/ (Data/
// Configs.hpp), or an item's inline "recipe", which makes one of that item
// and is named after it.
//
//   { "type": "recipe", "name": "wooden-arrow", "category": "crafter",
//     "ingredients": { "wood": 1 }, "results": { "wooden-arrow": 10 },
//     "time": 0.5 }
//
// Where: every recipe has one category (a "recipe-category" config: hand,
// settlement, crafter, ...). Whoever crafts offers a set of categories, a
// CraftCategoryMask, and can make the recipes whose category is in it: the
// player offers "hand" anywhere, "settlement" inside one of their
// settlements, plus whatever that settlement's buildings unlock (a workshop
// unlocks "crafter"). Categories are bits, so the check is one AND.
//
// A recipe with no ingredients makes something from nothing, given time and
// a building that offers its category (a shrine's essence, later).
//
// Recipes live in Configs (Configs::recipes); RecipeIDs are their place in
// load order. A Recipe's spans point into Configs, which must outlive them.
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include "Nightmourn/Item/ItemAmount.hpp"
#include <cstddef>     // size_t
#include <cstdint>     // uint16_t, uint32_t
#include <span>        // span
#include <string_view> // string_view

namespace Nightmourn {

struct RecipeTag;

using RecipeID = Nezumi::ID<RecipeTag, std::uint16_t>;

// Bit i: the i-th "recipe-category" config, in load order
using CraftCategoryMask = std::uint32_t;

inline constexpr std::size_t MAX_CRAFT_CATEGORIES = 32;

struct Recipe {
    std::string_view            name        = {};
    RecipeID                    id          = {};
    std::span<const ItemAmount> ingredients = {}; // may be empty
    std::span<const ItemAmount> results     = {}; // at least one
    std::uint32_t               ticks       = 30; // its "time", in ticks (at least 1)
    CraftCategoryMask           category    = 0;  // exactly one bit

    // True if `offered` includes where it's made
    bool can_craft_in(CraftCategoryMask offered) const { return (category & offered) != 0; }

    // How many of `item` one craft makes
    std::uint32_t makes(ItemID item) const {
        std::uint32_t n = 0;
        for (const ItemAmount& r : results) if (r.item == item) n += r.count;
        return n;
    }
};

} // namespace Nightmourn
