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
#include "Nightmourn/Command/CommandRegistry.hpp"
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Surface/Surface.hpp"
#include "Nightmourn/Surface/SurfaceConfig.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include "Nightmourn/World/WorldConfig.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <cassert> // assert
#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <vector>  // vector

namespace Nezumi {
    class Clock;
}

namespace Nightmourn {

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
    // 4. Interaction  apply damage, then mine commands, each in issue order
    // 5. Movement     integrate velocity, then update chunk/sector membership
    // 6. Activation   update chunk active/inactive state again
    // 7. Sweep        drop empty sectors every `sector_sweep_interval` ticks
    // -------------------------------------------------------------------------

    WorldContext make_context_();

    void apply_commands_     ();
    void update_activation_  ();
    void tick_entities_      (WorldContext& ctx);
    void apply_interactions_ ();
    void apply_damage_       ();
    void apply_mining_       ();
    void integrate_movement_ (float dt);
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
