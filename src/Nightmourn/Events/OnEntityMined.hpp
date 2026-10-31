// =============================================================================
// Events/OnEntityMined.hpp
//
// Published when a mine command takes from a resource. `amount` is what was
// actually taken, so the miner should be credited with `amount` of `item`. When
// `remaining` is 0 the resource ran out and is removed at the next command
// phase (OnEntityRemoved follows).
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
#include "Nightmourn/Item/ItemID.hpp"
#include <cstdint> // uint16_t

namespace Nightmourn {

struct OnEntityMined : Nezumi::IEvent {
    EntityHandle    target     = {};
    EntityHandle    source     = {}; // may be null or stale
    EntityTypeID    type_id    = {}; // target's type and subtype: what was
    EntitySubtypeID subtype_id = {}; // mined, without a lookup
    ItemID          item       = {}; // what mining yields (may be null)
    uint16_t        amount     = 0;  // amount actually taken
    uint16_t        remaining  = 0;  // target amount after mining
    Nezumi::Tick            tick       = 0;
};

} // namespace Nightmourn
