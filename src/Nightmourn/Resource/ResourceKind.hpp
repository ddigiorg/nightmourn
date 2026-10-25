// =============================================================================
// Resource/ResourceKind.hpp
//
// What a resource holds (AoE2's four), and the item mining it yields. A
// resource's kind and its subtype are separate: trees hold wood, but so could
// a fallen log; berry bushes and farms both hold food.
// =============================================================================

#pragma once
#include "Nightmourn/Item/ItemID.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <cstdint>     // uint8_t
#include <string_view> // string_view

namespace Nightmourn {

enum class ResourceKind : std::uint8_t {
    Wood,
    Stone,
    Gold,
    Food,

    Count
};

namespace detail {
    struct ResourceKindInfo {
        std::string_view name;
        ItemID           item;
    };

    inline constexpr std::array<ResourceKindInfo, static_cast<std::size_t>(ResourceKind::Count)> RESOURCE_KINDS = {{
        { "wood",  Items::Wood  },
        { "stone", Items::Stone },
        { "gold",  Items::Gold  },
        { "food",  Items::Food  },
    }};
}

// Lowercase name, e.g. "wood"
constexpr std::string_view name(ResourceKind kind) {
    return detail::RESOURCE_KINDS[static_cast<std::size_t>(kind)].name;
}

// The item one unit of this kind becomes when mined
constexpr ItemID item(ResourceKind kind) {
    return detail::RESOURCE_KINDS[static_cast<std::size_t>(kind)].item;
}

} // namespace Nightmourn
