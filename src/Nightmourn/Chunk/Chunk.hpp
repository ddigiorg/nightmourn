// =============================================================================
// Chunk.hpp
//
// A 2D spatial subsection of a surface used for simulation. Owns two intrusive
// entity lists: active (ticked) and inactive (sleeping).
//
// List membership is authoritative: an entity is in the active list if and
// only if its Active flag is set. Only set_entity_active() changes both.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityList.hpp"
#include "Nightmourn/Chunk/ChunkID.hpp"
#include "Nightmourn/Chunk/ChunkState.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include <cstddef>

namespace Nightmourn {

class  Surface;
struct WorldContext;

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
};

} // namespace Nightmourn
