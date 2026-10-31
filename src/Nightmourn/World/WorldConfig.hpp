// =============================================================================
// World/WorldConfig.hpp
//
// Config of a World.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include <cstdint>

namespace Nightmourn {

struct WorldConfig {
    // Seed for the simulation RNG. Every lockstep peer must use the same one.
    uint64_t seed = 0u;

    // Empty sectors are swept every N ticks (0 disables sweeping).
    Nezumi::Tick sector_sweep_interval = 600u; // 10 s at 60 UPS

    // Publish OnEntityDamaged for every applied hit. Each event is a heap
    // allocation on the EventBus, so turn this off for very large battles if
    // nothing listens.
    bool publish_damage_events = true;

    // Publish OnEntityMined for every applied mine command. Without it
    // nothing outside the sim learns what a miner took, so leave it on unless
    // the game credits mining some other way.
    bool publish_mine_events = true;
};

} // namespace Nightmourn
