// =============================================================================
// Item/ItemAmount.hpp
//
// Some number of one item: "4 wood". What a building costs to put up (its
// config's "cost"), and later what a recipe takes and makes.
// =============================================================================

#pragma once
#include "Nightmourn/Item/ItemID.hpp"
#include <cstdint> // uint32_t

namespace Nightmourn {

struct ItemAmount {
    ItemID        item  = {};
    std::uint32_t count = 0;
};

} // namespace Nightmourn
