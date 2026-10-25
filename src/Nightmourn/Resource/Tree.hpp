// =============================================================================
// Resource/Tree.hpp
//
// A tree: a Nezumi resource that holds wood. Like every EntityResource it
// never ticks, so a forest costs nothing per tick; it changes only when
// something mines it (World::mine_entity), and it is removed once its wood
// runs out.
//
// Decorative for now: nothing mines trees yet. Once there is an item and
// storage system, a gatherer finds one with
//
//   surface.find_nearest_entity(pos, radius,
//       Nezumi::EntitySearchFilter::resources(EntityTypes::Tree));
//
// mines it, and OnEntityMined says how much wood to put in its inventory.
//
// No neutral faction or health: units ignore trees, and they cannot be cut
// down by attacking them.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityResource.hpp"
#include "Nightmourn/Resource/TreeConfig.hpp"
#include "Nightmourn/World/EntityTypes.hpp"

namespace Nightmourn {

class Tree : public Nezumi::EntityResource {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr Nezumi::EntityTypeID TYPE_ID = EntityTypes::Tree;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the tree
    explicit Tree(const TreeConfig& config)
        : Nezumi::EntityResource(config.max_wood)
        , config_(&config)
    {
        // Not Tickable or Active: it sleeps in its chunk's inactive list
        this->set_drawable();
        this->set_collidable();
        this->set_selectable();

        collision_bounds = config.collision_bounds;
    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const TreeConfig& config() const { return *config_; }

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const TreeConfig* config_ = nullptr;
};

} // namespace Nightmourn
