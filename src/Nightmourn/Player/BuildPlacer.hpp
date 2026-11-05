// =============================================================================
// Player/BuildPlacer.hpp
//
// Placing buildings from the player's inventory: Factorio's cursor stack,
// AoE2's build mode. Select a building (e.g. its item's action bar slot) and
// its ghost follows the cursor, snapped to the tile grid and centered on it:
// green where World::check_placement says it fits, red with the reason where
// it doesn't. The place action puts it down and spends one of its item, from
// a list of inventories in order (the player's, then the settlement's while
// it's in reach): putting a building up is free, crafting its item is what
// costs (GUI/ItemsMenu). For walls, holding it paints a wall along the
// cursor's path. Cancel (Q, like
// Factorio's clearing the cursor), or running out of the item, ends it; the
// item stays in the inventory until a building is actually put down, so
// there's nothing to give back.
//
// Someone has to do the building: the Builder (the hero) must stand within
// its reach of the footprint and, for anything but a banner, inside its
// settlement's build area. The ghost is red with the reason otherwise
// (blocker()).
//
//   Inventory* items_from[] = { &player.inventory(), settlement_in_reach };
//   on_tick:  placer_.update(world, surface, items_from, &builder, factions, cursor, input);
//   on_draw:  placer_.draw(ctx, world_drawer.buildings(), surface);
//
// While a banner is selected the ghost also shows the build area it would
// found, and while anything else is, the faction's build area is filled
// (draw_settlements with SettlementDrawStyle::fill) so it's clear where it
// can go.
//
// Not an entity and not part of the simulation: it reads input between
// ticks and issues World::place_building like a player's command.
// =============================================================================

#pragma once
#include "Nezumi/Input/ActionID.hpp"
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Building/BuildingPlacement.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Geometry/TileRect.hpp"
#include "Nightmourn/Surface/SurfaceID.hpp"
#include <cstdint>     // uint8_t
#include <glm/glm.hpp> // vec2, vec4
#include <optional>    // optional
#include <span>        // span

namespace Nezumi {
    class InputMap;
}

namespace Nightmourn {

class BuildingDrawer;
class Inventory;
class World;
struct DrawContext;

class BuildPlacer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        Nezumi::ActionID place  = {}; // put the ghost down (held: paint walls)
        Nezumi::ActionID cancel = {}; // stop placing

        glm::vec4 valid_tint   = { 0.45f, 1.00f, 0.45f, 0.70f };
        glm::vec4 invalid_tint = { 1.00f, 0.35f, 0.30f, 0.70f };
    };

    explicit BuildPlacer(const Config& config) : config_(config) {}

    // Who is building, this update
    struct Builder {
        glm::vec2 position      = {};    // world, the character's feet
        float     reach         = 0.0f;  // tiles, to the footprint's nearest edge
        bool      in_settlement = false; // stands in its faction's build area
    };

    // Why a building that fits the ground still can't go down
    enum class Blocker : std::uint8_t {
        None,
        NoBuilder,     // no one to build it (the hero is down)
        OutOfReach,    // further than the builder's reach
        NotInSettlement, // the builder is outside the build area
    };

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    // Start placing `building` (null stops); selecting the one being placed
    // again stops too, as pressing its slot again does in most games
    void toggle(const BuildingConfig* building);
    void cancel() { building_ = nullptr; }

    bool                  active  () const { return building_ != nullptr; }
    const BuildingConfig* building() const { return building_; }

    // -------------------------------------------------------------------------
    // Update (between ticks)
    //
    // `cursor` is the world position under the mouse (none while the UI has
    // it), `builder` who builds (null: no one, nothing goes down). Places
    // through World::place_building, spending one of the building's item
    // from `items_from` (the first that has one; null entries are skipped);
    // stops when none has any left.
    // -------------------------------------------------------------------------

    void update(World& world, SurfaceID surface,
                std::span<Inventory* const> items_from, const Builder* builder,
                EntityFactionsMask factions, std::optional<glm::vec2> cursor,
                const Nezumi::InputMap& input);

    // Where the ghost is (bottom-left footprint tile) and whether it fits
    TileCoord       origin() const { return origin_; }
    PlacementResult result() const { return result_; }
    bool            has_cursor() const { return has_cursor_; }

    // What else stops it, checked in this order
    Blocker blocker() const { return blocker_; }

    // It fits and someone can build it there: the ghost is green
    bool can_place() const { return is_ok(result_) && blocker_ == Blocker::None; }

    // Tiles from `p` to the nearest point of `rect`'s tiles (0 inside)
    static float distance_to(glm::vec2 p, const TileRect& rect);

    // Bottom-left footprint tile of `building` centered on world `cursor`
    static TileCoord origin_at(const BuildingConfig& building, glm::vec2 cursor);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    // The ghost, and for a banner the build area it would found
    void draw(DrawContext& ctx, const BuildingDrawer& drawer, SurfaceID surface) const;

private:
    Config                config_     = {};
    const BuildingConfig* building_   = nullptr;
    TileCoord             origin_     = {};
    PlacementResult       result_     = PlacementResult::Ok;
    bool                  has_cursor_ = false;
    Blocker               blocker_    = Blocker::None;

    Blocker blocker_at_(TileCoord at, const Builder* builder) const;

    // The last tile a held place action put a wall on, so painting places
    // once per tile
    std::optional<TileCoord> painted_ = {};
};

} // namespace Nightmourn
