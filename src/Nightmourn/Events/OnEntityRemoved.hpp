// =============================================================================
// OnEntityRemoved.hpp
//
// Published just before a destroyed entity is removed. The handle is
// already stale when listeners run.
//
// Sim notification. World enqueues it on the EventBus (EventPhase::TickPost)
// and it is dispatched when the game loop flushes that phase. Listeners are
// outside the simulation: read sim state freely, but change it only through
// World commands.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nezumi/Event/IEvent.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include "Nightmourn/Entity/EntityID.hpp"

namespace Nightmourn {

struct OnEntityRemoved : Nezumi::IEvent {
    EntityHandle handle    = {};
    EntityTypeID type_id   = {};
    SurfaceID     surface_id = {};
    Nezumi::Tick         tick      = 0;
};

} // namespace Nightmourn
