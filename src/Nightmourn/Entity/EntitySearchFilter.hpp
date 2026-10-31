// =============================================================================
// Entity/EntitySearchFilter.hpp
//
// Which entities a Surface spatial query accepts. The query's shape (box,
// radius) is passed separately, so one filter serves every query:
//
//   const auto enemies = EntitySearchFilter::enemies_of(*this);
//   surface->find_nearest_entity(position, aggro_radius, enemies);
//
// Default-constructed, the filter accepts every live entity.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/Entity.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityFlagsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"

namespace Nightmourn {

struct EntitySearchFilter {

    // -------------------------------------------------------------------------
    // Presets
    // -------------------------------------------------------------------------

    // Damageable entities that share no faction with `self`. Neutral entities
    // (no faction) are skipped.
    static EntitySearchFilter enemies_of(const Entity& self) {
        return {
            .require_flags    = EntityFlagsMask::Flag24, // Damageable
            .any_factions     = EntityFactionsMask::All,
            .exclude_factions = self.factions_mask,
            .exclude          = self.handle,
        };
    }

    // Resources (any faction or none), e.g. for a gatherer looking for the
    // nearest tree. Pass `type` to look for one kind only.
    static EntitySearchFilter resources(EntityTypeID type = {}) {
        return {
            .type          = type,
            .require_flags = EntityFlagsMask::Flag23, // Resource
        };
    }

    // Entities that share at least one faction with `self`, except `self`.
    static EntitySearchFilter friends_of(const Entity& self) {
        return {
            .any_factions = self.factions_mask,
            .exclude      = self.handle,
        };
    }

    // -------------------------------------------------------------------------
    // Matching
    // -------------------------------------------------------------------------

    bool matches(const Entity& entity) const {
        if (!entity.is_alive())                                        return false;
        if (entity.handle == exclude)                                  return false;
        if (type && entity.type_id != type)                            return false;
        if (!has_all(entity.flags_mask, require_flags))                return false;
        if (any_factions != EntityFactionsMask::None &&
            !has_any(entity.factions_mask, any_factions))              return false;
        if (has_any(entity.factions_mask, exclude_factions))           return false;
        return true;
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    EntityTypeID       type             = {};                       // null: any type
    EntityFlagsMask    require_flags    = EntityFlagsMask::None;    // must have all
    EntityFactionsMask any_factions     = EntityFactionsMask::None; // must share one (None: no check)
    EntityFactionsMask exclude_factions = EntityFactionsMask::None; // must share none
    EntityHandle       exclude          = {};                       // usually the searcher
    bool               include_inactive = true;                     // visit sleeping entities too
};

} // namespace Nightmourn
