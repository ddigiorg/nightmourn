// =============================================================================
// Events/OnEntityDamaged.hpp
//
// Published when a damage command is applied to an entity. When `health` is
// 0 the entity was killed and is removed at the next command phase
// (OnEntityRemoved follows).
//
// Sim notification. World enqueues it on the EventBus (EventPhase::TickPost)
// and it is dispatched when the game loop flushes that phase. Listeners are
// outside the simulation: read sim state freely, but change it only through
// World commands.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nezumi/Event/IEvent.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct OnEntityDamaged : Nezumi::IEvent {
    EntityHandle target = {};
    EntityHandle source = {}; // may be null or stale
    uint16_t     amount = 0;  // damage actually applied
    uint16_t     health = 0;  // target health after damage
    Nezumi::Tick         tick   = 0;
};

} // namespace Nightmourn
