// =============================================================================
// Surface.cpp
// =============================================================================

#include "Nightmourn/Surface/Surface.hpp"
#include "Nightmourn/Events/OnChunkCreated.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <algorithm> // sort, unique, erase_if
#include <cassert>   // assert

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

Surface::Surface(World& world, SurfaceID id, const SurfaceConfig& config)
    : world_(world)
    , id_   (id)
    , name_ (config.name)
{
    // Initial chunks are created by World after OnSurfaceCreated is
    // published, so listeners see the surface before its chunks.
}

// -----------------------------------------------------------------------------
// Chunks
// -----------------------------------------------------------------------------

Chunk* Surface::create_chunk(ChunkID id) {
    assert(id && "ChunkID is not valid");

    if (auto it = chunk_lookup_.find(id); it != chunk_lookup_.end())
        return it->second;

    Chunk* chunk = chunks_.create(id).object;
    chunk_lookup_.emplace(id, chunk);
    link_neighbours_(chunk);

    OnChunkCreated event;
    event.surface_id = id_;
    event.chunk_id  = id;
    event.tick      = world_.get_tick();
    world_.publish_(event);

    return chunk;
}

void Surface::create_chunks(ChunkSpan span) {
    if (!span) return;
    span.each([&](ChunkID id) { create_chunk(id); });
}

Chunk* Surface::get_chunk(ChunkID id) {
    auto it = chunk_lookup_.find(id);
    return it != chunk_lookup_.end() ? it->second : nullptr;
}

const Chunk* Surface::get_chunk(ChunkID id) const {
    auto it = chunk_lookup_.find(id);
    return it != chunk_lookup_.end() ? it->second : nullptr;
}

void Surface::link_neighbours_(Chunk* chunk) {
    const ChunkID id = chunk->id();
    for (std::size_t i = 0; i < NEIGHBOUR_COUNT; i++) {
        const int x = id.x + NEIGHBOUR_OFFSETS[i].x;
        const int y = id.y + NEIGHBOUR_OFFSETS[i].y;
        if (x < CHUNK_MIN_X || x > CHUNK_MAX_X || y < CHUNK_MIN_Y || y > CHUNK_MAX_Y) continue;

        Chunk* other = get_chunk(ChunkID{ static_cast<ChunkID::value_t>(x), static_cast<ChunkID::value_t>(y) });
        if (!other) continue;

        chunk->neighbours_[i]                     = other;
        other->neighbours_[opposite_neighbour(i)] = chunk;
    }
}

// -----------------------------------------------------------------------------
// Pheromones
// -----------------------------------------------------------------------------

PheromoneValue Surface::pheromone_at(Position2D position, FactionIndex faction) const {
    const Chunk* chunk = get_chunk(ChunkID::pos_to_id(position));
    if (!chunk) return 0;
    return chunk->pheromone(faction, world_.pheromone_epoch(), world_.pheromone_decay());
}

ChunkOffset Surface::pheromone_gradient(Position2D position, FactionIndex faction) const {
    const Chunk* chunk = get_chunk(ChunkID::pos_to_id(position));
    if (!chunk) return {};

    const PheromoneEpoch  now   = world_.pheromone_epoch();
    const PheromoneDecay& decay = world_.pheromone_decay();

    const int best = chunk->strongest_neighbour(faction, now, decay);
    if (best < 0) return {};

    const std::size_t i = static_cast<std::size_t>(best);
    if (chunk->neighbour(i)->pheromone(faction, now, decay) <= chunk->pheromone(faction, now, decay))
        return {};
    return NEIGHBOUR_OFFSETS[i];
}

// -----------------------------------------------------------------------------
// Sectors
// -----------------------------------------------------------------------------

const Sector* Surface::get_sector(SectorID id) const {
    auto it = sector_lookup_.find(id);
    return it != sector_lookup_.end() ? it->second : nullptr;
}

Sector* Surface::get_or_create_sector_(SectorID id) {
    assert(id && "SectorID is not valid");

    if (auto it = sector_lookup_.find(id); it != sector_lookup_.end())
        return it->second;

    auto [handle, sector] = sectors_.create(id);
    sector->handle = handle;
    sector_lookup_.emplace(id, sector);
    return sector;
}

void Surface::insert_into_sectors_(Entity* entity, SectorSpan span) {
    span.each([&](SectorID id) {
        get_or_create_sector_(id)->insert_entity(entity);
    });
}

void Surface::remove_from_sectors_(Entity* entity, SectorSpan span) {
    span.each([&](SectorID id) {
        auto it = sector_lookup_.find(id);
        assert(it != sector_lookup_.end() && "Sector is not valid");
        it->second->remove_entity(entity);
        // Empty sectors are kept until the next sweep.
    });
}

// -----------------------------------------------------------------------------
// Spatial Queries
// -----------------------------------------------------------------------------

void Surface::find_entities_in(const Area2D&              area,
                               const EntitySearchFilter&  filter,
                               std::vector<EntityHandle>& results) const
{
    visit_entities_in(area, [&](const Entity& entity) {
        if (filter.matches(entity)) results.push_back(entity.handle);
        return true;
    }, filter.include_inactive);
}

void Surface::find_entities_near(Position2D                 center,
                                 float                      radius,
                                 const EntitySearchFilter&  filter,
                                 std::vector<EntityHandle>& results) const
{
    const float radius2 = radius * radius;

    visit_entities_in(Area2D::around(center, radius), [&](const Entity& entity) {
        if (center.distance2(entity.position) > radius2) return true;
        if (filter.matches(entity)) results.push_back(entity.handle);
        return true;
    }, filter.include_inactive);
}

EntityHandle Surface::find_nearest_entity(Position2D                center,
                                          float                     radius,
                                          const EntitySearchFilter& filter) const
{
    EntityHandle nearest   = {};
    float        min_dist2 = radius * radius;

    visit_entities_in(Area2D::around(center, radius), [&](const Entity& entity) {
        const float d2 = center.distance2(entity.position);
        if (d2 > min_dist2) return true;
        if (d2 == min_dist2 && nearest && entity.handle.index() > nearest.index()) return true;
        if (!filter.matches(entity)) return true;

        min_dist2 = d2;
        nearest   = entity.handle;
        return true;
    }, filter.include_inactive);

    return nearest;
}

void Surface::find_colliding(const Area2D&              box,
                             EntityHandle               exclude,
                             std::vector<EntityHandle>& results) const
{
    const size_t first = results.size();

    SectorSpan::span_from_box(box).each([&](SectorID id) {
        const Sector* sector = get_sector(id);
        if (!sector) return;

        for (const Entity* entity : sector->entities) {
            if (entity->handle == exclude)              continue;
            if (!entity->is_alive())                    continue;
            if (!entity->collision_box().overlaps(box)) continue;
            results.push_back(entity->handle);
        }
    });

    // An entity spanning several sectors is found once per sector
    const auto by_raw = [](EntityHandle a, EntityHandle b) { return a.raw() < b.raw(); };
    std::sort(results.begin() + first, results.end(), by_raw);
    results.erase(std::unique(results.begin() + first, results.end()), results.end());
}

// -----------------------------------------------------------------------------
// Tick Phases
// -----------------------------------------------------------------------------

void Surface::tick_entities_(WorldContext& ctx) {
    // active_chunks_ cannot change during this phase: activation updates
    // happen only in World's activation phases.
    for (Chunk* chunk : active_chunks_) {
        chunk->tick(ctx);
    }
}

void Surface::integrate_movement_(float dt, std::vector<Entity*>& moved) {
    for (Chunk* chunk : active_chunks_) {
        chunk->visit_active_entities([&](Entity& entity) {
            if (entity.is_pending_remove()) return true;
            if (entity.velocity.is_zero())  return true;

            entity.position += entity.velocity * dt;
            moved.push_back(&entity);
            return true;
        });
    }
}

void Surface::update_chunk_activation_() {
    if (dirty_chunks_.empty()) return;

    bool order_changed = false;

    for (Chunk* chunk : dirty_chunks_) {
        const bool want_active = chunk->active_count() > 0;

        if (want_active != chunk->is_active()) {
            chunk->state_ = want_active ? ChunkState::Active : ChunkState::Inactive;
            if (want_active) active_chunks_.push_back(chunk);
            order_changed = true;
        }
        chunk->dirty_ = false;
    }
    dirty_chunks_.clear();

    if (!order_changed) return;

    std::erase_if(active_chunks_, [](const Chunk* c) { return !c->is_active(); });

    // Canonical tick order: independent of activation history, so a game
    // loaded from a save ticks in the same order as one that never stopped.
    std::sort(active_chunks_.begin(), active_chunks_.end(),
              [](const Chunk* a, const Chunk* b) { return a->id() < b->id(); });
}

// Runs after the second activation phase, so every chunk that holds an
// active entity is in active_chunks_ (an entity that just walked into a
// sleeping chunk woke it). Each chunk only writes its own field.
void Surface::deposit_pheromones_(Nezumi::Tick tick, Nezumi::Tick interval, PheromoneValue amount) {
    const PheromoneEpoch  now   = world_.pheromone_epoch();
    const PheromoneDecay& decay = world_.pheromone_decay();
    for (Chunk* chunk : active_chunks_) {
        chunk->deposit_pheromones_(tick, interval, amount, now, decay);
    }
}

void Surface::sweep_empty_sectors_() {
    // Walk the SlotMap (index order, deterministic), not the hash map.
    std::vector<Nezumi::Handle<Sector>> empty;

    sectors_.for_each([&](const Sector& sector) {
        if (sector.empty()) empty.push_back(sector.handle);
    });

    for (Nezumi::Handle<Sector> handle : empty) {
        const Sector* sector = sectors_.get(handle);
        sector_lookup_.erase(sector->id);
        sectors_.remove(handle);
    }
}

// -----------------------------------------------------------------------------
// Entity Membership
// -----------------------------------------------------------------------------

void Surface::insert_entity_(Entity* entity) {
    assert(entity && "Entity is not valid");
    assert(entity->surface_id == id_ && "Entity belongs to another surface");

    Chunk* chunk = create_chunk(ChunkID::pos_to_id(entity->position));
    chunk->insert_entity_(entity);
    mark_dirty_(chunk);

    if (entity->is_collidable()) {
        assert(!entity->sector_span && "Entity is already in the sector grid");
        entity->sector_span = SectorSpan::span_from_box(entity->collision_box());
        insert_into_sectors_(entity, entity->sector_span);
    }
}

void Surface::remove_entity_(Entity* entity) {
    assert(entity && "Entity is not valid");

    if (entity->chunk_id) {
        Chunk* chunk = get_chunk(entity->chunk_id);
        assert(chunk && "Chunk is not valid");
        chunk->remove_entity_(entity);
        mark_dirty_(chunk);
    }

    if (entity->sector_span) {
        remove_from_sectors_(entity, entity->sector_span);
        entity->sector_span = SectorSpan::null();
    }
}

void Surface::update_entity_(Entity* entity) {
    assert(entity && "Entity is not valid");
    assert(entity->chunk_id && "Entity is not in the chunk grid");

    // Chunk membership
    const ChunkID src_chunk_id = entity->chunk_id;
    const ChunkID dst_chunk_id = ChunkID::pos_to_id(entity->position);

    if (dst_chunk_id != src_chunk_id) {
        Chunk* src = get_chunk(src_chunk_id);
        Chunk* dst = create_chunk(dst_chunk_id);
        assert(src && "Source chunk is not valid");

        src->remove_entity_(entity);
        dst->insert_entity_(entity);
        mark_dirty_(src);
        mark_dirty_(dst);
    }

    // Sector membership
    if (!entity->sector_span) return;

    const SectorSpan src_span = entity->sector_span;
    const SectorSpan dst_span = SectorSpan::span_from_box(entity->collision_box());
    if (dst_span == src_span) return;

    src_span.each([&](SectorID id) {
        if (dst_span.contains(id)) return;
        auto it = sector_lookup_.find(id);
        assert(it != sector_lookup_.end() && "Sector is not valid");
        it->second->remove_entity(entity);
    });

    dst_span.each([&](SectorID id) {
        if (src_span.contains(id)) return;
        get_or_create_sector_(id)->insert_entity(entity);
    });

    entity->sector_span = dst_span;
}

void Surface::set_entity_active_(Entity* entity, bool active) {
    assert(entity && "Entity is not valid");
    assert(entity->chunk_id && "Entity is not in the chunk grid");

    Chunk* chunk = get_chunk(entity->chunk_id);
    assert(chunk && "Chunk is not valid");

    chunk->set_entity_active_(entity, active);
    mark_dirty_(chunk);
}

void Surface::mark_dirty_(Chunk* chunk) {
    if (chunk->dirty_) return;
    chunk->dirty_ = true;
    dirty_chunks_.push_back(chunk);
}

} // namespace Nightmourn
