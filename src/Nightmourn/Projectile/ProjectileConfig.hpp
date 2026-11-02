// =============================================================================
// ProjectileConfig.hpp
//
// Shared, data-driven stats for one kind of projectile (arrow, bolt, rock).
// Projectiles keep a pointer to their config, so a config must outlive every
// projectile fired with it (static storage, like UnitConfig).
//
// Distances are in tiles and speeds in tiles per second (World/Constants.hpp).
//
// Guidance (Factorio's "projectile" vs a homing one, Age of Empires' arrows
// before and after Ballistics):
//
//   Ground     Flies to a point picked at launch and hits whatever stands
//              there when it lands. A target that walks away dodges it,
//              unless `lead_target` aims where the target is heading.
//   Tracking   Follows its target and always hits it if it is still alive
//              on landing. If the target dies first, it lands where the
//              target was last seen, like a Ground projectile.
// =============================================================================

#pragma once
#include <cstdint>     // uint8_t
#include <string_view> // string_view

namespace Nightmourn {

enum class ProjectileGuidance : std::uint8_t {
    Ground,
    Tracking,
};

struct ProjectileConfig {
    std::string_view   name     = {};
    std::uint8_t       look     = 0; // drawers pick the look by this
    ProjectileGuidance guidance = ProjectileGuidance::Ground;

    // Flight
    float speed         = 8.0f;  // tiles per second, along the ground
    float arc           = 0.25f; // peak height above the line, per tile flown
    float max_height    = 3.0f;  // tiles; cap on the peak for long shots
    float launch_height = 0.5f;  // tiles above the shooter's feet

    // Aim (Ground only): aim where the target will be on landing, from its
    // velocity at launch. AoE2's Ballistics.
    bool lead_target = false;

    // Hit (Ground, and Tracking whose target died): a box of this half-size
    // around the landing point is tested against collision boxes. The
    // intended target wins if it is there; otherwise the lowest-handle
    // enemy, or any entity but the shooter if `friendly_fire` is on.
    float hit_radius    = 0.15f;
    bool  friendly_fire = false;
};

} // namespace Nightmourn
