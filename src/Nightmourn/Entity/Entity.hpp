// =============================================================================
// Entity/Entity.hpp
//
// An interactable game object that exists in the world.
//
// Simulation only: no draw functions. Renderers switch on `type_id` (and
// `subtype_id`) and read entities through const access.
//
// Rules for derived tick() implementations:
//   - Write only your own state (velocity, timers, derived-class fields).
//   - Affect other entities and the world through World commands
//   - Never write `position` directly while ticking. Set `velocity`; the
//     movement phase integrates it after every entity has ticked.
//
// Derived types must declare a unique, explicit type ID:
//
//   class Unit : public Entity {
//   public:
//       static constexpr EntityTypeID TYPE_ID{ 1 };
//       ...
//   };
// =============================================================================

#pragma once
#include "Nightmourn/Chunk/ChunkID.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityFlagsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Entity/EntityList.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Sector/CollisionMask.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Sector/SectorSpan.hpp"
#include <cassert>  // assert
#include <concepts> // convertible_to, derived_from

namespace Nightmourn {

struct WorldContext;

class Entity : public EntityListNode {
public:
    Entity()          = default;
    virtual ~Entity() = default;

    // Entities live at a stable address in the SlotMap and are linked into
    // intrusive lists, so they must never be copied or moved.
    Entity(const Entity&)            = delete;
    Entity& operator=(const Entity&) = delete;

    // -------------------------------------------------------------------------
    // Simulation
    // -------------------------------------------------------------------------

    virtual void tick(WorldContext& ctx) = 0;

    // -------------------------------------------------------------------------
    // Entity Type Identity and Cast
    // -------------------------------------------------------------------------

    template <typename TEntity>
    bool is_type() const { return type_id == TEntity::TYPE_ID; }

    template <typename TEntity>
    TEntity* as_type() {
        assert(this->is_type<TEntity>());
        return static_cast<TEntity*>(this);
    }

    template <typename TEntity>
    const TEntity* as_type() const {
        assert(this->is_type<TEntity>());
        return static_cast<const TEntity*>(this);
    }

    // -------------------------------------------------------------------------
    // Flags
    //
    // Read freely. Gameplay flags may be set in derived constructors. After
    // spawning, change Active only through World::set_entity_active so the
    // flag and the chunk list membership always agree.
    // -------------------------------------------------------------------------

    // Query flags
    bool has_flags(EntityFlagsMask mask) const { return has_all(flags_mask, mask); }

    bool is_resource()       const { return has_all(flags_mask, EntityFlagsMask::Flag23); }
    bool is_damageable()     const { return has_all(flags_mask, EntityFlagsMask::Flag24); }
    bool is_active()         const { return has_all(flags_mask, EntityFlagsMask::Flag25); }
    bool is_tickable()       const { return has_all(flags_mask, EntityFlagsMask::Flag26); }
    bool is_drawable()       const { return has_all(flags_mask, EntityFlagsMask::Flag27); }
    bool is_collidable()     const { return has_all(flags_mask, EntityFlagsMask::Flag28); }
    bool is_selectable()     const { return has_all(flags_mask, EntityFlagsMask::Flag29); }
    bool is_pending_create() const { return has_all(flags_mask, EntityFlagsMask::Flag30); }
    bool is_pending_remove() const { return has_all(flags_mask, EntityFlagsMask::Flag31); }
    bool is_alive()          const { return !is_pending_create() && !is_pending_remove(); }

    // Set flags
    void set_flags(EntityFlagsMask mask) {
        // TODO: assert not reserved flag
        flags_mask |= mask;
    }

    void set_resource()       { flags_mask |= EntityFlagsMask::Flag23; }
    void set_damageable()     { flags_mask |= EntityFlagsMask::Flag24; }
    void set_active()         { flags_mask |= EntityFlagsMask::Flag25; }
    void set_tickable()       { flags_mask |= EntityFlagsMask::Flag26; }
    void set_drawable()       { flags_mask |= EntityFlagsMask::Flag27; }
    void set_collidable()     { flags_mask |= EntityFlagsMask::Flag28; }
    void set_selectable()     { flags_mask |= EntityFlagsMask::Flag29; }
    void set_pending_create() { flags_mask |= EntityFlagsMask::Flag30; }
    void set_pending_remove() { flags_mask |= EntityFlagsMask::Flag31; }

    // Clear flags
    void clear_flags(EntityFlagsMask mask) {
        // TODO: assert not reserved flag
        flags_mask &= ~mask;
    }

    void clear_active()         { flags_mask &= ~EntityFlagsMask::Flag25; }
    void clear_tickable()       { flags_mask &= ~EntityFlagsMask::Flag26; }
    void clear_drawable()       { flags_mask &= ~EntityFlagsMask::Flag27; }
    void clear_collidable()     { flags_mask &= ~EntityFlagsMask::Flag28; }
    void clear_selectable()     { flags_mask &= ~EntityFlagsMask::Flag29; }
    void clear_pending_create() { flags_mask &= ~EntityFlagsMask::Flag30; }
    void clear_pending_remove() { flags_mask &= ~EntityFlagsMask::Flag31; }

    // -------------------------------------------------------------------------
    // Factions
    //
    // Entities that share any faction are friends; entities that share none
    // are enemies. Entities with no faction are neutral (enemy of none).
    // -------------------------------------------------------------------------

    bool is_faction(EntityFactionsMask flag) const { return has_any (factions_mask, flag); }
    bool is_friend (EntityFactionsMask mask) const { return has_any (factions_mask, mask); }
    bool is_enemy  (EntityFactionsMask mask) const { return has_none(factions_mask, mask); }

    bool is_friend(const Entity& other) const { return is_friend(other.factions_mask); }
    bool is_enemy (const Entity& other) const {
        return factions_mask != EntityFactionsMask::None &&
               other.factions_mask != EntityFactionsMask::None &&
               is_enemy(other.factions_mask);
    }

    // Index of the entity's only faction, for per-faction arrays. Asserts the
    // entity has exactly one.
    FactionIndex faction_index() const { return Nightmourn::faction_index(factions_mask); }

    // -------------------------------------------------------------------------
    // Collision
    // -------------------------------------------------------------------------

    // TODO

    // -------------------------------------------------------------------------
    // Positional
    // -------------------------------------------------------------------------

    bool at(Position2D point) const {
        return position.approx_equal(point, 0.25f); // within 1/4 tile
    }

    float distance2(const Entity& other) const { return position.distance2(other.position); }

    // World-space collision box (local bounds offset by position).
    Area2D collision_box() const { return collision_bounds + position; }

    // -------------------------------------------------------------------------
    // Members
    //
    // Sizes assume a 16 B IntrusiveListNode (prev + next pointers).
    // -------------------------------------------------------------------------

    // Cache line 0 (hot: tick, movement, queries)
    // void*           vtable           = {};     //  8 B
    // EntityListNode  list_node        = {};     // 16 B
    EntityHandle       handle           = {};     //  8 B  set by World
    EntityTypeID       type_id          = {};     //  1 B  set by World
    SurfaceID          surface_id       = {};     //  1 B  set by World
    EntitySubtypeID    subtype_id       = {};     //  2 B
    EntityFlagsMask    flags_mask       = {};     //  4 B
    EntityFactionsMask factions_mask    = {};     //  2 B
    CollisionMask      collision_mask   = {};     //  2 B
    ChunkID            chunk_id         = {};     //  4 B  owned by Surface
    Position2D         position         = {};     //  8 B  written by movement phase
    Position2D         velocity         = {};     //  8 B  tiles per second

    // Cache line 1 (collision and gameplay)
    Area2D             collision_bounds = {};      // 16 B  local, relative to position
    SectorSpan         sector_span      = {};      // 16 B  owned by Surface

    // 32 B free: derived types start here (EntityDamageable adds 4 B,
    // EntityResource 6 B)
};

static_assert(sizeof(Entity) <= 128, "Entity should fit in 2 cache lines");

// -----------------------------------------------------------------------------
// EntityType concept
//
// A concrete entity type that World can create.
// -----------------------------------------------------------------------------

template <typename T>
concept EntityType =
    std::derived_from<T, Entity> &&
    !std::is_abstract_v<T> &&
    requires { { T::TYPE_ID } -> std::convertible_to<EntityTypeID>; };

} // namespace Nightmourn
