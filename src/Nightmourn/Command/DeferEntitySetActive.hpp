// =============================================================================
// DeferEntitySetActive.hpp
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"

namespace Nightmourn {

struct DeferEntitySetActive {
    EntityHandle handle = {};
    bool         active = false;
};

} // namespace Nightmourn
