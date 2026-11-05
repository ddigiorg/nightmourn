// =============================================================================
// Settlement/Settlement.hpp
//
// A base: what a banner founds. It has an owner (its faction), a build area
// (a square of tiles centered on the banner), the buildings inside it, and
// its own inventory, apart from any player's.
//
// Not an entity: settlements don't stand anywhere or tick. World owns them
// (World::place_building founds one for a banner, and the banner's removal
// disbands it) and they live as long as their banner. Entities name one by
// SettlementID (Building::settlement_id).
//
// The settlement keeps an occupancy grid of its build area: which building
// stands on each tile. Placement checks, wall connections and "what's on this
// tile" are lookups in it, and it knows about buildings placed this tick that
// haven't joined their chunk yet.
//
//   if (const Settlement* s = world.settlement_at(surface, tile))
//       if (EntityHandle b = s->building_at(tile)) ...
//
// Enemy spawning asks World::is_spawn_blocked, which checks every
// settlement's spawn_area().
//
// Crafting: unlocked_categories() is what its buildings let the player
// craft while they stand in it (a workshop: "crafter"), kept up to date as
// buildings join and leave, so asking costs nothing. Its crafting buildings
// are also its machines (`crafting`, a CraftSite): they make what its rules
// ask for out of its inventory, ticked by World in its crafting phase.
// =============================================================================

#pragma once
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Crafting/CraftSite.hpp"
#include "Nightmourn/Crafting/Recipe.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/TileRect.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Settlement/SettlementConfig.hpp"
#include "Nightmourn/Settlement/SettlementID.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include <array>   // array
#include <bit>     // countr_zero
#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <vector>  // vector

namespace Nightmourn {

class Settlement {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    //
    // World only (World::place_building, for a banner). `config` must outlive
    // the settlement.
    // -------------------------------------------------------------------------

    Settlement(SettlementID id, const SettlementConfig& config, SurfaceID surface,
               TileCoord center, EntityFactionsMask factions, EntityHandle banner)
        : inventory(config.inventory_slots)
        , id_(id), surface_(surface), factions_(factions), banner_(banner)
        , center_(center), build_area_(TileRect::around(center, config.build_radius))
        , config_(&config)
        , occupancy_(static_cast<std::size_t>(build_area_.width()) *
                     static_cast<std::size_t>(build_area_.height()))
    {}

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    SettlementID            id      () const { return id_;       }
    SurfaceID               surface () const { return surface_;  }
    EntityFactionsMask      factions() const { return factions_; }
    EntityHandle            banner  () const { return banner_;   }
    TileCoord               center  () const { return center_;   } // the banner's tile
    const SettlementConfig& config  () const { return *config_;  }

    // Where its buildings may go
    const TileRect& build_area() const { return build_area_; }

    // Where enemies may not spawn: the build area and spawn_margin around it
    TileRect spawn_area() const { return build_area_.grow(config_->spawn_margin); }

    // Buildings standing in it (the banner included)
    std::size_t building_count() const { return building_count_; }

    // Recipe categories its buildings let the player craft while in it
    CraftCategoryMask unlocked_categories() const { return unlocked_; }

    // -------------------------------------------------------------------------
    // Occupancy
    // -------------------------------------------------------------------------

    // The building standing on `tile`, or a null handle (also outside the
    // build area)
    EntityHandle building_at(TileCoord tile) const {
        return build_area_.contains(tile) ? occupancy_[index_(tile)] : EntityHandle{};
    }

    // True if every tile of `rect` is in the build area with nothing on it
    bool is_free(const TileRect& rect) const {
        if (!build_area_.contains(rect)) return false;
        bool free = true;
        rect.each([&](TileCoord t) { if (occupancy_[index_(t)]) free = false; });
        return free;
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    // The settlement's stock: what its crafters draw from and deliver to.
    // Changed outside the tick for now (the game deposits into it).
    Inventory inventory;

    // Its machines and the player's rules for them. World adds and removes
    // the machines; the rules are the player's to set.
    CraftSite crafting;

private:
    friend class World;

    // World only: a building joins or leaves it
    void add_building_(EntityHandle building, const BuildingConfig& config) {
        building_count_++;
        crafting.add_machine(building, config);
        for_each_bit_(config.player_categories(), [&](std::size_t i) {
            if (unlock_counts_[i]++ == 0) unlocked_ |= CraftCategoryMask{ 1 } << i;
        });
    }
    void remove_building_(EntityHandle building, const BuildingConfig& config) {
        building_count_--;
        crafting.remove_machine(building);
        for_each_bit_(config.player_categories(), [&](std::size_t i) {
            if (--unlock_counts_[i] == 0) unlocked_ &= ~(CraftCategoryMask{ 1 } << i);
        });
    }

    template <typename Fn>
    static void for_each_bit_(CraftCategoryMask mask, Fn&& fn) {
        while (mask) {
            fn(static_cast<std::size_t>(std::countr_zero(mask)));
            mask &= mask - 1;
        }
    }

    // World only: marks `rect`'s tiles (clipped to the build area) as taken
    // by `building`, or frees them with a null handle
    void set_occupant_(const TileRect& rect, EntityHandle building) {
        rect.each([&](TileCoord t) {
            if (build_area_.contains(t)) occupancy_[index_(t)] = building;
        });
    }

    std::size_t index_(TileCoord t) const {
        return static_cast<std::size_t>(t.y - build_area_.min.y) * static_cast<std::size_t>(build_area_.width()) +
               static_cast<std::size_t>(t.x - build_area_.min.x);
    }

    SettlementID            id_             = {};
    SurfaceID               surface_        = {};
    EntityFactionsMask      factions_       = {};
    EntityHandle            banner_         = {};
    TileCoord               center_         = {};
    TileRect                build_area_     = {};
    const SettlementConfig* config_         = nullptr;
    std::vector<EntityHandle> occupancy_    = {}; // row-major over build_area_
    std::size_t             building_count_ = 0;

    CraftCategoryMask                                   unlocked_      = 0;
    std::array<std::uint16_t, MAX_CRAFT_CATEGORIES>     unlock_counts_ = {}; // buildings unlocking each
};

} // namespace Nightmourn
