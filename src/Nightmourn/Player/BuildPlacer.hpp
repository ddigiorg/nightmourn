// =============================================================================
// Player/BuildPlacer.hpp
//
// Placing buildings from the player's inventory: Factorio's cursor stack,
// AoE2's build mode. Select a building (e.g. its item's action bar slot) and
// its ghost follows the cursor, snapped to the tile grid and centered on it:
// green where World::check_placement says it fits, red with the reason where
// it doesn't. The place action puts it down and spends one of its item; for
// walls, holding it paints a wall along the cursor's path. Cancel, or running
// out of the item, ends it.
//
//   on_tick:  if (placer_.active()) placer_.update(world, surface, inventory, factions, cursor, input);
//   on_draw:  placer_.draw(ctx, world_drawer.buildings());
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
#include "Nightmourn/Surface/SurfaceID.hpp"
#include <glm/glm.hpp> // vec2, vec4
#include <optional>    // optional

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
    // it). Places through World::place_building, spending the building's item
    // from `inventory`; stops when there's none left.
    // -------------------------------------------------------------------------

    void update(World& world, SurfaceID surface, Inventory& inventory,
                EntityFactionsMask factions, std::optional<glm::vec2> cursor,
                const Nezumi::InputMap& input);

    // Where the ghost is (bottom-left footprint tile) and whether it fits
    TileCoord       origin() const { return origin_; }
    PlacementResult result() const { return result_; }
    bool            has_cursor() const { return has_cursor_; }

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

    // The last tile a held place action put a wall on, so painting places
    // once per tile
    std::optional<TileCoord> painted_ = {};
};

} // namespace Nightmourn
