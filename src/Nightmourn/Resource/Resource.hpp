// =============================================================================
// Resource.hpp
//
// Something in the world that can be mined: trees, gold and stone mines,
// berry bushes, ... One C++ type for all of them; its ResourceConfig says
// which (subtype, variety, item and amount). The game assigns the subtype,
// variety and item ids.
//
// Like every EntityResource it never ticks, so a forest costs nothing per
// tick: it sleeps in its chunk's inactive list. It changes only when
// something mines it (World::mine_entity), and it is removed once it runs
// out.
//
// A gatherer finds one with
//
//   surface.find_nearest_entity(pos, radius,
//       EntitySearchFilter::resources(Resource::TYPE_ID));
//
// (checking subtype_id for what it wants), mines it, and credits
// OnEntityMined::amount of OnEntityMined::item.
//
// No faction or health: units ignore resources, and attacking one does
// nothing.
// =============================================================================

#pragma once
#include "Nightmourn/Resource/ResourceConfig.hpp"
#include "Nightmourn/Entity/EntityTypes.hpp"
#include "Nightmourn/Entity/EntityResource.hpp"

namespace Nightmourn {

class Resource : public EntityResource {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr EntityTypeID TYPE_ID = EntityTypes::Resource;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the resource
    explicit Resource(const ResourceConfig& config)
        : EntityResource(config.max_amount, config.item)
        , variety_id(config.variety)
        , config_(&config)
    {
        // Not Tickable or Active: it sleeps in its chunk's inactive list
        this->set_drawable();
        this->set_collidable();
        this->set_selectable();

        subtype_id       = config.subtype;
        collision_bounds = config.collision_bounds;
    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const ResourceConfig& config() const { return *config_; }

    // -------------------------------------------------------------------------
    // Members
    //
    // With EntityResource's 6 B: 16 B of the 32 B left in Entity's second
    // cache line.
    // -------------------------------------------------------------------------

    ResourceVarietyID variety_id = {}; // 1 B  which one of its subtype (oak, pine)

private:
    const ResourceConfig* config_ = nullptr; // 8 B
};

static_assert(sizeof(Resource) <= 128, "Resource should fit in Entity's 2 cache lines");

} // namespace Nightmourn
