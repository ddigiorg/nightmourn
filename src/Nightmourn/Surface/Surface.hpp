// =============================================================================
// Surface.hpp
//
// A 2D canvas, dimension, or layer of the world.
//
// Owns chunk and sector storage (SlotMaps, stable addresses) with hash maps
// used for LOOKUP ONLY. The simulation never iterates a hash map: ticking
// walks `active_chunks_`, which is kept sorted by ChunkID (row-major), and
// spatial queries walk spans in row-major order. Both orders are identical
// on every platform and independent of insertion history.
//
// Public API is read-only (const queries) plus chunk creation. Entity
// membership changes come only from World's tick phases.
// =============================================================================

#pragma once
#include "Nezumi/Object/SlotMap.hpp"
#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/Chunk/ChunkID.hpp"
#include "Nightmourn/Chunk/ChunkSpan.hpp"
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Entity/EntitySearchFilter.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Sector/Sector.hpp"
#include "Nightmourn/Sector/SectorID.hpp"
#include "Nightmourn/Sector/SectorSpan.hpp"
#include "Nightmourn/Surface/SurfaceConfig.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include <string>        // string
#include <unordered_map> // unordered_map
#include <vector>        // vector

namespace Nightmourn {

class  World;
struct WorldContext;

class Surface {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    Surface(World& world, SurfaceID id, const SurfaceConfig& config = {});

    // Non-copyable
    Surface(const Surface&)            = delete;
    Surface& operator=(const Surface&) = delete;

    // -------------------------------------------------------------------------
    // Surface
    // -------------------------------------------------------------------------

    SurfaceID            id() const { return id_;   }
    const std::string& name() const { return name_; }

    // -------------------------------------------------------------------------
    // Chunks
    //
    // Chunks are created on demand when an entity enters them, or explicitly
    // here (e.g. to pre-create a region). Chunks are never unloaded yet.
    // -------------------------------------------------------------------------

    Chunk*       create_chunk (ChunkID   id  );
    void         create_chunks(ChunkSpan span);
    Chunk*       get_chunk    (ChunkID   id  );
    const Chunk* get_chunk    (ChunkID   id  ) const;

    size_t chunk_count()        const { return chunks_.count();       }
    size_t active_chunk_count() const { return active_chunks_.size(); }

    // Active chunks in tick order (sorted by ChunkID).
    const std::vector<Chunk*>& active_chunks() const { return active_chunks_; }

    // Every chunk, in storage order. Fine for rendering and debug overlays;
    // do not let simulation results depend on this order.
    template <typename Fn>
    void each_chunk(Fn&& fn) const { chunks_.for_each(fn); }

    // Existing chunks within a span, row-major. Typical renderer entry point:
    // compute the camera's span, then visit its chunks and their entities.
    template <typename Fn>
    void each_chunk_in(ChunkSpan span, Fn&& fn) const {
        span.each([&](ChunkID id) {
            if (const Chunk* chunk = get_chunk(id)) fn(*chunk);
        });
    }

    // -------------------------------------------------------------------------
    // Sectors
    // -------------------------------------------------------------------------

    const Sector* get_sector  (SectorID id) const;
    size_t        sector_count() const { return sectors_.count(); }

    template <typename Fn>
    void each_sector(Fn&& fn) const { sectors_.for_each(fn); }

    // -------------------------------------------------------------------------
    // Spatial Queries (const, deterministic order)
    //
    // Entities are matched by position (their feet), except find_colliding,
    // which matches collision boxes through the sector grid. Results never
    // depend on hash map or allocation order: chunks are walked row-major,
    // then in list order within each chunk.
    //
    // Safe to call while ticking: positions and chunk lists only change in
    // World's movement and command phases.
    // -------------------------------------------------------------------------

    // Visit live entities whose position lies in `area`. `fn(const Entity&)`
    // returns true to continue, false to stop. Returns false if stopped.
    template <typename Fn>
    bool visit_entities_in(const Area2D& area, Fn&& fn, bool include_inactive = true) const;

    // Append handles of entities matching `filter` whose position lies in
    // `area` (or within `radius` of `center`) to `results`.
    void find_entities_in  (const Area2D&              area,
                            const EntitySearchFilter&  filter,
                            std::vector<EntityHandle>& results) const;

    void find_entities_near(Position2D                 center,
                            float                      radius,
                            const EntitySearchFilter&  filter,
                            std::vector<EntityHandle>& results) const;

    // Nearest entity matching `filter` within `radius` of `center`, or a null
    // handle. Equal distances go to the lower handle index, so the result
    // never depends on iteration order.
    EntityHandle find_nearest_entity(Position2D                center,
                                     float                     radius,
                                     const EntitySearchFilter& filter) const;

    // Append handles of collidable entities whose collision box overlaps
    // `box`, deduplicated and sorted by handle.
    void find_colliding(const Area2D&              box,
                        EntityHandle               exclude,
                        std::vector<EntityHandle>& results) const;

private:
    friend class World;

    // -------------------------------------------------------------------------
    // Tick Phases (World only)
    // -------------------------------------------------------------------------

    void tick_entities_          (WorldContext& ctx);
    void integrate_movement_     (float dt, std::vector<Entity*>& moved);
    void update_chunk_activation_();
    void sweep_empty_sectors_    ();

    // -------------------------------------------------------------------------
    // Entity Membership (World only)
    // -------------------------------------------------------------------------

    void insert_entity_      (Entity* entity);
    void remove_entity_      (Entity* entity);
    void update_entity_      (Entity* entity); // after position changed
    void set_entity_active_  (Entity* entity, bool active);

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Sector* get_or_create_sector_(SectorID id);
    void    insert_into_sectors_ (Entity* entity, SectorSpan span);
    void    remove_from_sectors_ (Entity* entity, SectorSpan span);
    void    mark_dirty_          (Chunk* chunk);

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    World&      world_;
    SurfaceID   id_   = {};
    std::string name_ = {};

    Nezumi::SlotMap<Chunk>                        chunks_        = {};
    std::unordered_map<ChunkID, Chunk*>   chunk_lookup_  = {};
    std::vector<Chunk*>                   active_chunks_ = {}; // sorted by ID
    std::vector<Chunk*>                   dirty_chunks_  = {}; // pending activation check

    Nezumi::SlotMap<Sector>                       sectors_       = {};
    std::unordered_map<SectorID, Sector*> sector_lookup_ = {};
};

// -----------------------------------------------------------------------------
// Template Definitions
// -----------------------------------------------------------------------------

template <typename Fn>
bool Surface::visit_entities_in(const Area2D& area, Fn&& fn, bool include_inactive) const {
    const ChunkSpan span = ChunkSpan::span_from_box(area);

    ChunkID id;
    for (id.y = span.min.y; id.y <= span.max.y; id.y++) {
        for (id.x = span.min.x; id.x <= span.max.x; id.x++) {
            const Chunk* chunk = get_chunk(id);
            if (!chunk) continue;

            const bool keep_going = chunk->visit_entities(
                [&](const Entity& entity) {
                    if (!entity.is_alive())              return true;
                    if (!area.contains(entity.position)) return true;
                    return static_cast<bool>(fn(entity));
                },
                include_inactive);

            if (!keep_going) return false;
        }
    }
    return true;
}

} // namespace Nightmourn
