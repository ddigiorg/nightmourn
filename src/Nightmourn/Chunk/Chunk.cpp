// =============================================================================
// Chunk.cpp
// =============================================================================

#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <cassert> // assert

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

Chunk::Chunk(ChunkID id) {
    assert(id.is_valid() && "ChunkID is not valid");

    id_    = id;
    state_ = ChunkState::Inactive;

    const Position2D pos = ChunkID::id_to_pos(id);

    bounds_ = {
        /*.min = */pos,
        /*.max = */{ pos.x + CHUNK_SIZE.x, pos.y + CHUNK_SIZE.y }
    };
}

// -------------------------------------------------------------------------
// Simulation
// -------------------------------------------------------------------------

void Chunk::tick(WorldContext& ctx) {
    visit_list_(active_entities_, [&](Entity& entity) {
        if (!entity.is_pending_remove()) entity.tick(ctx);
        return true;
    });
}

// -------------------------------------------------------------------------
// Entity Membership (Surface only)
// -------------------------------------------------------------------------

void Chunk::insert_entity_(Entity* entity) {
    assert(entity && "Entity is not valid");
    assert(!entity->chunk_id && "Entity is already in a chunk");

    list_for_(entity).insert(entity);
    entity->chunk_id = id_;
}

void Chunk::remove_entity_(Entity* entity) {
    assert(entity && "Entity is not valid");
    assert(entity->chunk_id == id_ && "Entity is not in this chunk");

    list_for_(entity).remove(entity);
    entity->chunk_id = ChunkID::null();
}

void Chunk::set_entity_active_(Entity* entity, bool active) {
    assert(entity && "Entity is not valid");
    assert(entity->chunk_id == id_ && "Entity is not in this chunk");

    if (entity->is_active() == active) return;

    list_for_(entity).remove(entity);
    (active)
        ? entity->set_active()
        : entity->clear_active();
    list_for_(entity).insert(entity);
}

} // namespace Nightmourn
