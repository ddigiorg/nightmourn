// =============================================================================
// Settlement/RaidSpawner.hpp
//
// Sends night raids at settlements (see RaidConfig). The game owns one per
// raiding faction and calls update() once per tick, between ticks, with how
// dark it is (Lighting's DayNight::darkness(), so the simulation itself
// doesn't depend on lighting):
//
//   raids_.update(*world_, surface_, day_night_.darkness(), clock.get_fixed_dt());
//
// Raiders spawn only where World::is_spawn_blocked allows, on a loaded
// chunk, clear of anything collidable. They attack-move to the banner. The
// spawner forgets a raider once it dies.
// =============================================================================

#pragma once
#include "Nezumi/Core/Random.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Settlement/RaidConfig.hpp"
#include "Nightmourn/Settlement/SettlementID.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include <cstddef>  // size_t
#include <cstdint>  // uint64_t
#include <optional> // optional
#include <vector>   // vector

namespace Nightmourn {

class World;
class Settlement;

class RaidSpawner {
public:
    // `config` (and its unit) must outlive the spawner. Raiders belong to
    // `raiders`, and attack every settlement not of that faction.
    RaidSpawner(const RaidConfig& config, EntityFactionsMask raiders, std::uint64_t seed = 1);

    // Once per tick: counts down while darkness >= min_darkness and spawns
    // when the count runs out. The first raider comes half an interval
    // after dark.
    void update(World& world, SurfaceID surface, float darkness, float dt);

    // Raiders alive (all settlements)
    std::size_t alive() const;

    // Where a raider would spawn now for `settlement`, if anywhere (tries a
    // few random spots on the ring around its spawn area)
    std::optional<Position2D> pick_spot(const World& world, const Settlement& settlement);

    const RaidConfig& config() const { return *config_; }

private:
    struct Raider {
        SettlementID settlement;
        EntityHandle unit;
    };

    struct Timer {
        SettlementID settlement;
        float        seconds;
    };

    const RaidConfig*   config_;
    EntityFactionsMask  raiders_;
    Nezumi::Random      random_;
    std::vector<Raider> alive_  = {};
    std::vector<Timer>  timers_ = {};

    float& timer_(SettlementID settlement);
};

} // namespace Nightmourn
