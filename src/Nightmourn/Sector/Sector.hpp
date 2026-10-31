// =============================================================================
// Sector.hpp
//
// A 2D spatial subsection of a surface used for entity collision queries.
// Collidable entities register in every sector their collision box touches.
//
// Empty sectors are not erased immediately; Surface sweeps them periodically
// to avoid create/destroy churn when entities hover on sector borders.
// =============================================================================

#pragma once
#include "Nezumi/Object/Handle.hpp"
#include "Nightmourn/Sector/SectorID.hpp"
#include <cstddef> // size_t
#include <vector>  // vector

namespace Nightmourn {

class Entity;

class Sector {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit Sector(SectorID id) : id(id) {}

    // Non-copyable
    Sector(const Sector&)            = delete;
    Sector& operator=(const Sector&) = delete;

    // -------------------------------------------------------------------------
    // Entity Functions
    // -------------------------------------------------------------------------

    void insert_entity(Entity* entity);
    void remove_entity(Entity* entity);

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    size_t size()  const { return entities.size();  }
    bool   empty() const { return entities.empty(); }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    SectorID             id       = {}; //  8 B
    Nezumi::Handle<Sector>       handle   = {}; //  8 B  slot in parent Surface's SlotMap
    std::vector<Entity*> entities = {}; // 24 B
};

} // namespace Nightmourn
