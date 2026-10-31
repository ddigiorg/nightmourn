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

    // Pheromones (Chunk/Pheromones.hpp). Every entity with EmitsPheromone
    // and exactly one faction adds `pheromone_deposit_amount` to its chunk
    // every `pheromone_deposit_interval` ticks (0 disables deposits), so one
    // unit standing still settles near amount * half life / (interval * ln 2)
    // = 64 * 3600 / (30 * 0.693) ~ 11,000 of the 65,535 cap.
    Nezumi::Tick   pheromone_deposit_interval  = 30u; // 0.5 s at 60 UPS
    std::uint16_t  pheromone_deposit_amount    = 64u;

    // Strengths decay once every `pheromone_decay_period` ticks and halve
    // every `pheromone_half_life_epochs` of those periods.
    Nezumi::Tick   pheromone_decay_period      = 60u; // 1 s at 60 UPS
    std::uint32_t  pheromone_half_life_epochs  = 60u; // half life 60 s
};

} // namespace Nightmourn
