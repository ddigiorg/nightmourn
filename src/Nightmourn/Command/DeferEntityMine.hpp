// =============================================================================
// DeferEntityMine.hpp
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct DeferEntityMine {
    EntityHandle target = {};
    EntityHandle source = {};
    uint16_t     amount = 0;
};

} // namespace Nightmourn
