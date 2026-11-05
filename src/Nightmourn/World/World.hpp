// =============================================================================
// World/World.hpp
//
// Simulates 2D world objects and advances them one fixed tick at a time.
// =============================================================================

#pragma once
#include "Nezumi/Core/Random.hpp"
#include "Nezumi/Core/Tick.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nezumi/Object/SlotMap.hpp"
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Building/BuildingPlacement.hpp"
#include "Nightmourn/Chunk/Pheromones.hpp"
#include "Nightmourn/Command/CommandRegistry.hpp"
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Projectile/Projectile.hpp"
#include "Nightmourn/Projectile/ProjectileConfig.hpp"
#include "Nightmourn/Settlement/Settlement.hpp"
#include "Nightmourn/Settlement/SettlementID.hpp"
#include "Nightmourn/Surface/Surface.hpp"
#include "Nightmourn/Surface/SurfaceConfig.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include "Nightmourn/World/WorldConfig.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <cassert> // assert
#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <memory>  // unique_ptr
#include <vector>  // vector

namespace Nezumi {
    class Clock;
}

namespace Nightmourn {

class Building;

class World {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit World(Nezumi::Clock& clock, Nezumi::EventBus& event_bus, const WorldConfig& config = {});
    ~World();

    // Non-copyable and non-movable
    World(const World&)            = delete;
    World& operator=(const World&) = delete;
    World(World&&)                 = delete;
    World& operator=(World&&)      = delete;

    // -------------------------------------------------------------------------
    // Simulation
    // -------------------------------------------------------------------------

    // Advance exactly one fixed tick. Call once per Clock fixed step: the
    // game owns its World, so this goes at the end of IApplication::on_tick.
    void tick();

    Nezumi::Tick get_tick() const { return tick_; }

    // -------------------------------------------------------------------------
    // Pheromones
    //
    // The decay curve every chunk shares, and the epoch reads use now.
    // Surface::pheromone_at and pheromone_gradient pass both for you.
    // -------------------------------------------------------------------------

    const PheromoneDecay& pheromone_decay() const { return pheromone_decay_; }
    PheromoneEpoch        pheromone_epoch() const { return pheromone_decay_.epoch(tick_); }

    // -------------------------------------------------------------------------
    // Surfaces
    // -------------------------------------------------------------------------

    SurfaceID      create_surface(const SurfaceConfig& config = {});
    Surface*       get_surface   (SurfaceID id);
    const Surface* get_surface   (SurfaceID id) const;
    size_t         surface_count () const { return surfaces_.count(); }

    // Every surface in SlotMap order.
    template <typename Fn>
    void each_surface(Fn&& fn) const { surfaces_.for_each(fn); }

    // -------------------------------------------------------------------------
    // Entities
    // -------------------------------------------------------------------------

    // Constructs the entity now and returns its handle.  It joins the world
    // (chunks, collision sectors, queries, ticking) at the next command phase.
    // Construct a T with `args` and schedule it to join `surface` at
    // `position`. Derived constructors set entity members (flags_mask, etc).
    template <EntityType T, typename... Args>
    EntityHandle create_entity(
        SurfaceID  surface,
        Position2D position,
        Args&&...  args);

    // Marks the entity for removal now (queries and ticks skip it). It is
    // removed at the next command phase.
    void remove_entity(EntityHandle handle);

    void set_entity_active(EntityHandle handle, bool active);

    // Deal `amount` damage to `target` in the next interaction phase (this
    // tick's if called while ticking). Ignored if the target is gone or not
    // damageable by then. A target whose health reaches 0 is removed.
    void damage_entity(
        EntityHandle target,
        uint16_t     amount,
        EntityHandle source = {});

    // Take up to `amount` from resource `target` in the next interaction
    // phase (this tick's if called while ticking). Ignored if the target is
    // gone or not a resource by then. OnEntityMined reports what was
    // actually taken. A resource that runs out is removed.
    void mine_entity(
        EntityHandle target,
        uint16_t     amount,
        EntityHandle source = {});

    // Fire a projectile from `source` at `target`. It joins `source`'s
    // surface in the next projectile phase (this tick's if called while
    // ticking) and deals `damage` where it lands (see ProjectileConfig.hpp).
    // Flight time is the ground distance over config.speed, at least 1 tick.
    void launch_projectile(
        const ProjectileConfig& config,
        const Entity&           source,
        const Entity&           target,
        uint16_t                damage);

    Entity* get_entity(EntityHandle handle) {
        return entities_.get(handle);
    }

    const Entity* get_entity(EntityHandle handle) const {
        return entities_.get(handle);
    }

    template <EntityType T>
    T* get_entity_as(EntityHandle handle) {
        Entity* entity = get_entity(handle);
        return entity && entity->is_type<T>() ? static_cast<T*>(entity) : nullptr;
    }

    template <EntityType T>
    const T* get_entity_as(EntityHandle handle) const {
        const Entity* entity = get_entity(handle);
        return entity && entity->is_type<T>() ? static_cast<const T*>(entity) : nullptr;
    }

    size_t entity_count() const { return entities_.count(); }

    // Every entity in SlotMap order, including spawning/pending ones.
    template <typename Fn>
    void each_entity(Fn&& fn) const { entities_.for_each(fn); }

    // -------------------------------------------------------------------------
    // Buildings & Settlements
    //
    // Buildings snap to the tile grid: `origin` is the bottom-left tile of
    // the footprint. A banner founds a settlement for `factions` (one per
    // faction for now) with its build area centered on the banner; every
    // other building goes wholly inside its faction's build area, on tiles no
    // other building or collidable entity (a tree) covers.
    //
    // place_building checks, then creates the building (it joins the world
    // at the next command phase, like create_entity) and registers it with
    // its settlement at once, so a second placement on the same tiles before
    // the next tick is refused. Item costs are the caller's (the Player
    // spends the item). Removing a building frees its tiles in the next
    // command phase; removing a banner disbands its settlement, leaving its
    // buildings standing with no settlement until a new banner adopts them.
    // -------------------------------------------------------------------------

    PlacementResult check_placement(
        SurfaceID             surface,
        const BuildingConfig& config,
        TileCoord             origin,
        EntityFactionsMask    factions) const;

    // The new building's handle, or a null one if it can't go there (and
    // `result` says why)
    EntityHandle place_building(
        SurfaceID             surface,
        const BuildingConfig& config,
        TileCoord             origin,
        EntityFactionsMask    factions,
        PlacementResult*      result = nullptr);

    Settlement*       get_settlement(SettlementID id);
    const Settlement* get_settlement(SettlementID id) const;

    // The settlement whose build area holds `tile`, or nullptr
    const Settlement* settlement_at(SurfaceID surface, TileCoord tile) const;

    // The settlement of a faction in `factions`, or nullptr
    Settlement*       settlement_of(EntityFactionsMask factions);
    const Settlement* settlement_of(EntityFactionsMask factions) const;

    // The building standing on `tile` in a settlement's build area, or a
    // null handle
    EntityHandle building_at(SurfaceID surface, TileCoord tile) const;

    // True if enemies may not spawn at `position`: inside some settlement's
    // spawn area (its build area and spawn_margin around it)
    bool is_spawn_blocked(SurfaceID surface, Position2D position) const;

    // Every settlement, oldest first
    template <typename Fn>
    void each_settlement(Fn&& fn) const {
        for (const auto& s : settlements_) if (s) fn(*s);
    }

private:
    friend class Surface; // publish_

    // -------------------------------------------------------------------------
    // Tick Phases
    //
    // The simulation tick is processed in phases:
    //
    // 1. Commands     apply deferred commands.
    // 2. Activation   update chunk active/inactive state
    // 3. Entity tick  active chunks in ChunkID order; entities write only
    //                 themselves and issue commands
    // 4. Projectiles  land the ones due this tick (queueing their damage),
    //                 steer tracking ones, then add this tick's launches
    // 5. Interaction  apply damage, then mine commands, each in issue order
    // 6. Crafting     each settlement's machines, in SettlementID order:
    //                 deliver the jobs due, start new ones (CraftSite)
    // 7. Movement     integrate velocity, then update chunk/sector membership
    // 8. Activation   update chunk active/inactive state again
    // 9. Pheromones   active emitters deposit into their chunk (staggered)
    // 10. Sweep       drop empty sectors every `sector_sweep_interval` ticks
    // -------------------------------------------------------------------------

    WorldContext make_context_();

    void apply_commands_     ();
    void update_activation_  ();
    void tick_entities_      (WorldContext& ctx);
    void update_projectiles_ ();
    void apply_interactions_ ();
    void apply_damage_       ();
    void apply_mining_       ();
    void tick_crafting_      ();
    void integrate_movement_ (float dt);
    void deposit_pheromones_ ();
    void sweep_sectors_      ();

    // -------------------------------------------------------------------------
    // Notifications
    //
    // World events are enqueued on the EventBus in EventPhase::TickPost.
    // The engine flushes that phase after IApplication::on_tick returns, so
    // tick the world from on_tick and its events go out the same tick.
    // -------------------------------------------------------------------------

    template <typename E>
    void publish_(E event) {
        event_bus_.enqueue(std::move(event), Nezumi::EventPhase::TickPost);
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    // Engine subsystems (not owned)
    Nezumi::Clock&    clock_;
    Nezumi::EventBus& event_bus_;

    // Simulation state
    WorldConfig      config_   = {};
    Nezumi::Random           random_;  // sim-only RNG
    Nezumi::Tick             tick_     = 0;
    bool             in_tick_  = false;
    Nezumi::SlotMap<Entity>  entities_ = {};
    Nezumi::SlotMap<Surface> surfaces_ = {};
    CommandRegistry  commands_ = {};
    PheromoneDecay   pheromone_decay_;

    // Projectiles launched since the last projectile phase, in launch order
    // (each knows its surface through its source)
    struct Launch {
        SurfaceID  surface;
        Projectile projectile;
    };
    std::vector<Launch> launches_ = {};

    // Settlements by SettlementID (index); a disbanded one leaves a null
    // slot, so ids are never reused
    std::vector<std::unique_ptr<Settlement>> settlements_ = {};

    // A building leaving the world: frees its tiles, disconnects its walls,
    // disbands its settlement if it was the banner
    void on_building_removed_(Building& building);

    // Scratch (reused every tick to avoid allocation)
    std::vector<Entity*> moved_ = {};
};

// -----------------------------------------------------------------------------
// Template Definitions
// -----------------------------------------------------------------------------

template <EntityType T, typename... Args>
EntityHandle World::create_entity(
    SurfaceID  surface,
    Position2D position,
    Args&&...  args)
{
    assert(get_surface(surface) && "SurfaceID is not valid");

    auto [handle, entity] = entities_.template create<T>(std::forward<Args>(args)...);

    entity->handle     = handle;
    entity->type_id    = T::TYPE_ID;
    entity->surface_id = surface;
    entity->position   = position;

    entity->set_pending_create();

    commands_.defer_entity_create({ handle });
    return handle;
}

} // namespace Nightmourn
