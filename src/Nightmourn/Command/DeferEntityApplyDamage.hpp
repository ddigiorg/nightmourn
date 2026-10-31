// =============================================================================
// DeferEntityApplyDamage.hpp
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct DeferEntityApplyDamage {
    EntityHandle target = {};
    EntityHandle source = {};
    uint16_t     amount = 0;
};

} // namespace Nightmourn
