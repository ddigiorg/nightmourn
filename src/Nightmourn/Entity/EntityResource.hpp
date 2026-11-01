// =============================================================================
// Entity/EntityResource.hpp
//
// Base for entities that can be mined (trees, ore, stone, ...).
//
// Resources never tick. They are not Tickable, so World keeps them in their
// chunk's inactive list: a chunk that holds only resources stays asleep and
// costs nothing per tick (Factorio's ore and trees work the same way). A
// resource changes only when something interacts with it.
//
// Mining is never applied directly while ticking. Issue it through
// World::mine_entity(); World applies it in the interaction phase (with
// damage, after every entity has ticked), in issue order, publishes
// OnEntityMined with the amount actually taken, and removes resources that
// run out. Two miners on the same tick take from the resource in issue
// order, and never more than it holds between them.
//
// The constructor sets the Resource flag, which is how World
// recognises a resource without RTTI or a virtual call:
//
//   if (entity->is_resource())
//       static_cast<EntityResource*>(entity)->take(amount);
//
// `item` is what mining yields (wood, gold, ...): one item per unit of
// amount. World reports it in OnEntityMined, so whoever credits the miner
// needs no lookup. See Resource/Resource for the ready-made
// resource.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <algorithm> // min
#include <cstdint>   // uint16_t

namespace Nightmourn {

class EntityResource : public Entity {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit EntityResource(uint16_t max_amount, ItemID item = {})
        : amount(max_amount), max_amount(max_amount), item(item)
    {
        this->set_resource();
    }

    // -------------------------------------------------------------------------
    // Simulation
    //
    // Never called: resources are not Tickable. Derived types should not
    // override it; anything a resource does happens when it is mined.
    // -------------------------------------------------------------------------

    void tick(WorldContext&) final {}

    // -------------------------------------------------------------------------
    // Amount
    // -------------------------------------------------------------------------

    bool is_depleted() const { return amount == 0;          }
    bool is_full()     const { return amount == max_amount; }

    // Amount left as a fraction of max amount, in [0, 1]. Render use only:
    // the simulation compares integers.
    float amount_ratio() const {
        return max_amount
            ? static_cast<float>(amount) / static_cast<float>(max_amount)
            : 0.0f;
    }

    // Take up to `request` and return what was actually taken (never more
    // than the amount left). World only.
    uint16_t take(uint16_t request) {
        const uint16_t taken = std::min(request, amount);
        amount = static_cast<uint16_t>(amount - taken);
        return taken;
    }

    // -------------------------------------------------------------------------
    // Members
    //
    // 6 B of the 32 B left in Entity's second cache line.
    // -------------------------------------------------------------------------

    uint16_t amount     = 0u; // 2 B
    uint16_t max_amount = 0u; // 2 B
    ItemID   item       = {}; // 2 B  what mining yields
};

} // namespace Nightmourn
