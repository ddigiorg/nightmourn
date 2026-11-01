// =============================================================================
// Chunk.hpp
//
// A 2D spatial subsection of a surface used for simulation. Owns two intrusive
// entity lists: active (ticked) and inactive (sleeping).
//
// List membership is authoritative: an entity is in the active list if and
// only if its Active flag is set. Only set_entity_active() changes both.
//
// A chunk also links its 8 neighbours (Surface does it as chunks are
// created; chunks are never unloaded, so the pointers stay valid) and holds
// its pheromone field (see Chunk/Pheromones.hpp).
// =============================================================================

#pragma once
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityList.hpp"
#include "Nightmourn/Chunk/ChunkID.hpp"
#include "Nightmourn/Chunk/ChunkState.hpp"
#include "Nightmourn/Chunk/Pheromones.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include <array>   // array
#include <cstddef> // size_t
#include <cstdint> // int8_t

namespace Nightmourn {

class  Surface;
struct WorldContext;

// -----------------------------------------------------------------------------
// Neighbours
//
// Offsets of the 8 neighbours in row-major order (y, then x), the same order
// as ChunkID's. Neighbour i and NEIGHBOUR_COUNT - 1 - i are opposite.
// -----------------------------------------------------------------------------

struct ChunkOffset {
    std::int8_t x = 0;
    std::int8_t y = 0;
};

inline constexpr std::size_t NEIGHBOUR_COUNT = 8;

inline constexpr std::array<ChunkOffset, NEIGHBOUR_COUNT> NEIGHBOUR_OFFSETS = {{
    { -1, -1 }, { 0, -1 }, { 1, -1 },
    { -1,  0 },            { 1,  0 },
    { -1,  1 }, { 0,  1 }, { 1,  1 },
}};

inline constexpr std::size_t opposite_neighbour(std::size_t i) { return NEIGHBOUR_COUNT - 1 - i; }

class Chunk {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit Chunk(ChunkID id);

    // Non-copyable: holds intrusive list heads, so it is pinned in memory.
    Chunk(const Chunk&)            = delete;
    Chunk& operator=(const Chunk&) = delete;

    // -------------------------------------------------------------------------
    // Simulation
    // -------------------------------------------------------------------------

    // Ticks every live entity in the active list, in list order.
    void tick(WorldContext& ctx);

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    ChunkID       id()     const { return id_;     }
    ChunkState    state()  const { return state_;  }
    const Area2D& bounds() const { return bounds_; }

    bool is_active() const { return state_ == ChunkState::Active; }
    bool is_dirty()  const { return dirty_; }

    bool in_bounds(Position2D position) const {
        return bounds_.contains(position);
    }

    size_t active_count()   const { return active_entities_.count();   }
    size_t inactive_count() const { return inactive_entities_.count(); }
    size_t entity_count()   const { return active_count() + inactive_count(); }

    // -------------------------------------------------------------------------
    // Neighbours
    // -------------------------------------------------------------------------

    // Neighbour `i` (see NEIGHBOUR_OFFSETS), or nullptr if it was never created
    const Chunk* neighbour(std::size_t i) const { return neighbours_[i]; }

    // -------------------------------------------------------------------------
    // Pheromones
    //
    // `now` is World::pheromone_epoch() and `decay` World::pheromone_decay();
    // Surface::pheromone_at and pheromone_gradient pass both for you.
    // -------------------------------------------------------------------------

    const Pheromones& pheromones() const { return pheromones_; }

    PheromoneValue pheromone(FactionIndex faction, PheromoneEpoch now, const PheromoneDecay& decay) const {
        return pheromones_.get(faction, now, decay);
    }

    // Index of the neighbour with the most of `faction`'s pheromone, or -1 if
    // no neighbour has any. A missing neighbour counts as 0, and ties go to
    // the lower index, so the answer never depends on creation order.
    int strongest_neighbour(FactionIndex faction, PheromoneEpoch now, const PheromoneDecay& decay) const;

    // -------------------------------------------------------------------------
    // Iteration
    //
    // Visits entities in list order (active list first). `fn` returns true to
    // continue and false to stop early. Returns false if stopped early.
    // Lists must not be modified during iteration; all structural changes in
    // World are deferred, so this holds during every tick phase.
    // -------------------------------------------------------------------------

    template <typename Fn>
    bool visit_entities(Fn&& fn, bool include_inactive = true) const {
        if (!visit_list_(active_entities_, fn)) return false;
        if (include_inactive && !visit_list_(inactive_entities_, fn)) return false;
        return true;
    }

    template <typename Fn>
    bool visit_active_entities(Fn&& fn) const {
        return visit_list_(active_entities_, fn);
    }

private:
    friend class Surface;

    // -------------------------------------------------------------------------
    // Entity Membership (Surface only)
    // -------------------------------------------------------------------------

    void insert_entity_      (Entity* entity);
    void remove_entity_      (Entity* entity);
    void set_entity_active_  (Entity* entity, bool active);

    // -------------------------------------------------------------------------
    // Pheromones (Surface only, tick phase 7)
    // -------------------------------------------------------------------------

    // Every live, active entity that emits pheromone and has exactly one
    // faction deposits `amount`, staggered: entity e deposits on the ticks
    // where (tick + e's handle index) % interval == 0.
    void deposit_pheromones_(Nezumi::Tick tick, Nezumi::Tick interval, PheromoneValue amount,
                             PheromoneEpoch now, const PheromoneDecay& decay);

    EntityList& list_for_(const Entity* entity) {
        return entity->is_active() ? active_entities_ : inactive_entities_;
    }

    template <typename Fn>
    static bool visit_list_(const EntityList& list, Fn&& fn) {
        for (EntityListNode* node = list.begin(); node != list.end(); ) {
            EntityListNode* next = node->next;
            if (!fn(*static_cast<Entity*>(node))) return false;
            node = next;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    bool        dirty_             = false;            //  1 B
    ChunkState  state_             = ChunkState::Null; //  1 B
    ChunkID     id_                = {};               //  4 B
    Area2D      bounds_            = {};               // 16 B
    EntityList  active_entities_   = {};               // 24 B
    EntityList  inactive_entities_ = {};               // 24 B
    std::array<Chunk*, NEIGHBOUR_COUNT> neighbours_ = {}; // 64 B, set by Surface
    Pheromones  pheromones_        = {};               // 36 B
};

} // namespace Nightmourn
