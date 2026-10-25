// =============================================================================
// Resource/Resource.hpp
//
// Something in the world that can be mined: trees for now, later stone and
// gold mines, berry bushes, ... One C++ type for all of them; the config says
// which (see ResourceIDs.hpp and ResourceConfig.hpp).
//
// Like every Nezumi EntityResource it never ticks, so a forest costs nothing
// per tick. It changes only when something mines it (World::mine_entity),
// and it is removed once it runs out.
//
// Decorative for now: nothing mines resources yet. Once there is a storage
// system, a gatherer finds one with
//
//   surface.find_nearest_entity(pos, radius,
//       Nezumi::EntitySearchFilter::resources(EntityTypes::Resource));
//
// (checking subtype_id for the kind of resource it wants), mines it, and adds
// OnEntityMined::amount of item(kind()) to its storage.
//
// No faction or health: units ignore resources, and attacking one does
// nothing.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityResource.hpp"
#include "Nightmourn/Resource/ResourceConfig.hpp"
#include "Nightmourn/World/EntityTypes.hpp"

namespace Nightmourn {

class Resource : public Nezumi::EntityResource {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr Nezumi::EntityTypeID TYPE_ID = EntityTypes::Resource;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the resource
    explicit Resource(const ResourceConfig& config)
        : Nezumi::EntityResource(config.max_amount)
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

    const ResourceConfig& config () const { return *config_;        }
    ResourceVarietyID     variety() const { return config_->variety; }
    ResourceKind          kind   () const { return config_->kind;    }
    ItemID                item   () const { return Nightmourn::item(config_->kind); }

    bool is_tree() const { return subtype_id == ResourceSubtypes::Tree; }

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const ResourceConfig* config_ = nullptr;
};

} // namespace Nightmourn
