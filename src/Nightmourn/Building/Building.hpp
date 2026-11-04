// =============================================================================
// Building/Building.hpp
//
// A structure the player places in a settlement: banners, walls, houses,
// crafters. One C++ type for all of them, like Resource; its BuildingConfig
// says which (BuildingType, footprint, health, ...).
//
// Buildings snap to the tile grid. `footprint` is the tiles one covers, and
// its position is the footprint's center, so a 1x1 wall stands at a tile
// center and a 2x2 house on a tile corner. Its collision box is the whole
// footprint.
//
// Placing one goes through World::place_building, never create_entity: the
// world checks the footprint against the settlement's build area and
// occupancy grid, registers the building with its settlement, founds the
// settlement for a banner, and connects walls (`connections`). Removing one
// (World::remove_entity, or health reaching 0) frees its tiles again; a
// banner's removal disbands its settlement.
//
// Buildings don't tick yet, so they sleep in their chunk's inactive list and
// cost nothing per tick. A building type that does something each tick (a
// crafter working from the settlement's inventory) derives from Building,
// overrides tick() and sets Tickable and Active in its constructor. It keeps
// Building's TYPE_ID, so drawers and queries still see a Building; tell it
// apart by subtype_id (BuildingType).
// =============================================================================

#pragma once
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Entity/EntityDamageable.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityTypes.hpp"
#include "Nightmourn/Geometry/TileRect.hpp"
#include "Nightmourn/Settlement/SettlementID.hpp"
#include <cstdint> // uint8_t

namespace Nightmourn {

// Sides of a tile, as bits of Building::connections
namespace TileSide {
    inline constexpr std::uint8_t North = 1u << 0; // +y
    inline constexpr std::uint8_t East  = 1u << 1; // +x
    inline constexpr std::uint8_t South = 1u << 2; // -y
    inline constexpr std::uint8_t West  = 1u << 3; // -x
}

class Building : public EntityDamageable {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr EntityTypeID TYPE_ID = EntityTypes::Building;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    //
    // Use World::place_building, which constructs it through create_entity.
    // `config` must outlive the building.
    // -------------------------------------------------------------------------

    Building(const BuildingConfig& config, TileCoord origin,
             EntityFactionsMask factions, SettlementID settlement)
        : EntityDamageable(config.max_health)
        , settlement_id(settlement)
        , origin_(origin)
        , config_(&config)
    {
        // Not Tickable or Active: it sleeps in its chunk's inactive list
        this->set_drawable();
        this->set_collidable();
        this->set_selectable();

        subtype_id    = to_subtype(config.type);
        factions_mask = factions;

        // The whole footprint, relative to its center
        const Position2D half = Position2D(static_cast<float>(config.footprint.x),
                                           static_cast<float>(config.footprint.y)) * 0.5f;
        collision_bounds = Area2D(-half, half);
    }

    // Static buildings have nothing to do. Ticking ones override this.
    void tick(WorldContext&) override {}

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const BuildingConfig& config() const { return *config_; }
    BuildingType          type  () const { return config_->type; }
    bool is(BuildingType t) const { return config_->type == t; }

    // Tiles it covers
    TileRect footprint() const { return TileRect::from_size(origin_, config_->footprint); }

    // World position of a footprint whose bottom-left tile is `origin`: its
    // center, where the building stands
    static Position2D center_of(const BuildingConfig& config, TileCoord origin) {
        return TileRect::from_size(origin, config.footprint).center();
    }

    // -------------------------------------------------------------------------
    // Members
    //
    // With EntityDamageable's 4 B: 25 B of the 32 B left in Entity's second
    // cache line.
    // -------------------------------------------------------------------------

    SettlementID settlement_id = {}; // 4 B  the settlement it belongs to (World)
    std::uint8_t connections   = 0;  // 1 B  TileSide bits: connected walls next to it (World)

private:
    TileCoord             origin_ = {};      // 8 B  bottom-left footprint tile
    const BuildingConfig* config_ = nullptr; // 8 B
};

static_assert(sizeof(Building) <= 128, "Building should fit in Entity's 2 cache lines");

} // namespace Nightmourn
