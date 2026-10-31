// =============================================================================
// Chunk.cpp
// =============================================================================

#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <bit>     // has_single_bit
#include <cassert> // assert
#include <cstdint> // uint16_t

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

// -----------------------------------------------------------------------------
// Pheromones
// -----------------------------------------------------------------------------

int Chunk::strongest_neighbour(FactionIndex faction, PheromoneEpoch now, const PheromoneDecay& decay) const {
    int            best       = -1;
    PheromoneValue best_value = 0;
    for (std::size_t i = 0; i < NEIGHBOUR_COUNT; i++) {
        const Chunk* chunk = neighbours_[i];
        if (!chunk) continue;
        const PheromoneValue value = chunk->pheromone(faction, now, decay);
        if (value > best_value) {
            best       = static_cast<int>(i);
            best_value = value;
        }
    }
    return best;
}

void Chunk::deposit_pheromones_(Nezumi::Tick tick, Nezumi::Tick interval, PheromoneValue amount,
                                PheromoneEpoch now, const PheromoneDecay& decay)
{
    visit_list_(active_entities_, [&](const Entity& entity) {
        if (!entity.is_alive() || !entity.emits_pheromone()) return true;
        if (!std::has_single_bit(static_cast<std::uint16_t>(entity.factions_mask))) return true;
        if ((tick + entity.handle.index()) % interval != 0) return true;

        pheromones_.deposit(entity.faction_index(), amount, now, decay);
        return true;
    });
}

} // namespace Nightmourn
