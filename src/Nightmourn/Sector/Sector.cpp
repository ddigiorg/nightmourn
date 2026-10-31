// =============================================================================
// Sector.cpp
// =============================================================================

#include "Nightmourn/Sector/Sector.hpp"
#include <cassert> // assert

namespace Nightmourn {

void Sector::insert_entity(Entity* entity) {
    assert(entity && "Entity is not valid");
    entities.push_back(entity);
}

// Swap-and-pop. Changes element order, but deterministically: the same
// sequence of inserts and removes always yields the same order.
void Sector::remove_entity(Entity* entity) {
    for (size_t i = 0; i < entities.size(); i++) {
        if (entities[i] == entity) {
            entities[i] = entities.back();
            entities.pop_back();
            return;
        }
    }
    assert(false && "Entity not found in sector");
}

} // namespace Nightmourn
