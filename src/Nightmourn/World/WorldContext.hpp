// =============================================================================
// World/WorldContext.hpp
//
// Everything simulation code may touch, passed to Entity::tick().
//
// Deliberately narrow: no renderer, window, event bus, wall clock or engine
// RNG. If entity code cannot reach them, it cannot accidentally make the
// simulation depend on them.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"

namespace Nezumi {
    class Random;
}

namespace Nightmourn {

class World;

struct WorldContext {
    World*          world  = nullptr; // commands + queries
    Nezumi::Random* random = nullptr; // simulation RNG (owned by World, seeded)
    Nezumi::Tick    tick   = 0;       // simulation tick being processed
    float           dt     = 0.0f;    // fixed timestep, seconds
};

} // namespace Nightmourn
