// =============================================================================
// Projectile.hpp
//
// One projectile in flight. Not an Entity: nothing searches for it, collides
// with it or targets it, so it lives in a flat vector on its Surface
// (Surface::projectiles) instead of chunk lists and sectors, and World moves
// it in its own tick phase (World.hpp, phase 4).
//
// The flight is analytic. Where the projectile is at any tick follows from
// origin, destination and its two ticks, so nothing is integrated and
// drawers read the same numbers the sim does:
//
//   t         = (tick - launch_tick) / flight_ticks      0 at launch, 1 on landing
//   ground(t) = origin + t * (destination - origin)
//   height(t) = (1 - t) * launch_height + 4 * peak_height * t * (1 - t)
//
// Tracking projectiles move `destination` onto their target every tick, so
// their ground path bends toward it.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Projectile/ProjectileConfig.hpp"
#include <algorithm> // clamp
#include <cstdint>   // uint16_t

namespace Nightmourn {

struct Projectile {

    // -------------------------------------------------------------------------
    // Flight
    // -------------------------------------------------------------------------

    Nezumi::Tick land_tick() const { return launch_tick + flight_ticks; }

    // Flight progress at `tick` plus `alpha` of the next one, in [0, 1]
    float progress(Nezumi::Tick tick, float alpha = 0.0f) const {
        const float flown = static_cast<float>(tick - launch_tick) + alpha;
        return std::clamp(flown / static_cast<float>(flight_ticks), 0.0f, 1.0f);
    }

    Position2D ground_at(float t) const {
        return origin + (destination - origin) * t;
    }

    // Tiles above the ground
    float height_at(float t) const {
        return (1.0f - t) * launch_height + 4.0f * peak_height * t * (1.0f - t);
    }

    // d height / d t (tiles per whole flight): positive while rising
    float climb_at(float t) const {
        return 4.0f * peak_height * (1.0f - 2.0f * t) - launch_height;
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const ProjectileConfig* config          = nullptr; //  8 B
    Position2D              origin          = {};      //  8 B  shooter's feet at launch
    Position2D              destination     = {};      //  8 B  landing point
    EntityHandle            source          = {};      //  8 B  shooter (may be gone)
    EntityHandle            target          = {};      //  8 B  intended target
    Nezumi::Tick            launch_tick     = 0;       //  8 B
    float                   peak_height     = 0.0f;    //  4 B  tiles above the line
    float                   launch_height   = 0.0f;    //  4 B
    uint16_t                flight_ticks    = 1;       //  2 B  >= 1
    uint16_t                damage          = 0;       //  2 B
    EntityFactionsMask      source_factions = {};      //  2 B  for friendly fire
};

} // namespace Nightmourn
