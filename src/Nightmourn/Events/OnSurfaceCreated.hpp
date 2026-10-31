// =============================================================================
// OnSurfaceCreated.hpp
//
// Published after a surface is created.
//
// Sim notification. World enqueues it on the EventBus (EventPhase::TickPost)
// and it is dispatched when the game loop flushes that phase. Listeners are
// outside the simulation: read sim state freely, but change it only through
// World commands.
//
// SurfaceID is a plain index because surfaces are never removed yet. Switch
// to a Handle<Surface> when surface removal lands so stale IDs are detectable.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nezumi/Event/IEvent.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"

namespace Nightmourn {

struct OnSurfaceCreated : Nezumi::IEvent {
    SurfaceID surface_id = {};
    Nezumi::Tick      tick       = 0;
};

} // namespace Nightmourn
