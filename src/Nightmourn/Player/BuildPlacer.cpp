// =============================================================================
// Player/BuildPlacer.cpp
// =============================================================================

#include "Nightmourn/Player/BuildPlacer.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Draw/BuildingDrawer.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/SettlementDrawer.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/World/World.hpp"
#include <cmath>   // floor
#include <cstdlib> // abs

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Selection
// -----------------------------------------------------------------------------

void BuildPlacer::toggle(const BuildingConfig* building) {
    building_ = (building == building_) ? nullptr : building;
    painted_.reset();
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------

TileCoord BuildPlacer::origin_at(const BuildingConfig& building, glm::vec2 cursor) {
    // Centered on the cursor: odd sizes on the tile under it, even ones on
    // the tile corner nearest it
    return {
        static_cast<std::int32_t>(std::floor(cursor.x - static_cast<float>(building.footprint.x) * 0.5f + 0.5f)),
        static_cast<std::int32_t>(std::floor(cursor.y - static_cast<float>(building.footprint.y) * 0.5f + 0.5f)),
    };
}

void BuildPlacer::update(World& world, SurfaceID surface, Inventory& inventory,
                         EntityFactionsMask factions, std::optional<glm::vec2> cursor,
                         const Nezumi::InputMap& input)
{
    if (!building_) return;
    if (input.was_pressed(config_.cancel)) { cancel(); return; }

    has_cursor_ = cursor.has_value();
    if (!has_cursor_) { painted_.reset(); return; }

    origin_ = origin_at(*building_, *cursor);
    result_ = world.check_placement(surface, *building_, origin_, factions);

    // A click places; holding paints walls, once per tile
    const bool held    = input.is_held(config_.place);
    const bool clicked = input.was_pressed(config_.place);
    if (!held && !clicked) { painted_.reset(); return; }
    if (!clicked && !building_->connects) return;

    const auto place_at = [&](TileCoord at) {
        if (inventory.count(building_->item) == 0) return;
        if (world.place_building(surface, *building_, at, factions)) inventory.remove(building_->item, 1);
    };

    if (building_->connects && painted_ && !clicked) {
        // Painting: every tile from the last one to the cursor, stepping one
        // axis at a time so the wall stays joined however fast the mouse moves
        TileCoord at = *painted_;
        while (at != origin_) {
            const std::int32_t dx = origin_.x - at.x;
            const std::int32_t dy = origin_.y - at.y;
            if (std::abs(dx) >= std::abs(dy)) at.x += dx > 0 ? 1 : -1;
            else                              at.y += dy > 0 ? 1 : -1;
            place_at(at);
        }
    } else if (!painted_ || *painted_ != origin_) {
        place_at(origin_);
    }
    painted_ = origin_;

    result_ = world.check_placement(surface, *building_, origin_, factions);
    if (inventory.count(building_->item) == 0) cancel();
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void BuildPlacer::draw(DrawContext& ctx, const BuildingDrawer& drawer, SurfaceID surface) const {
    if (!building_ || !has_cursor_) return;

    const bool      ok   = is_ok(result_);
    const glm::vec4 tint = ok ? config_.valid_tint : config_.invalid_tint;

    // A banner shows the base it would found
    if (building_->type == BuildingType::Banner && building_->settlement) {
        SettlementDrawStyle style;
        style.filler = &drawer;
        style.layer  = DrawLayer::EntityOverlay;
        style.color  = { tint.r, tint.g, tint.b, 0.9f };
        style.fill   = true;
        draw_build_area(ctx, TileRect::around(origin_, building_->settlement->build_radius), style);
    }

    // A wall shows how it would join the walls around it
    std::uint8_t connections = 0;
    if (building_->connects) {
        constexpr struct { std::uint8_t bit; std::int32_t dx, dy; } SIDES[4] = {
            { TileSide::North, 0, 1 }, { TileSide::East, 1, 0 }, { TileSide::South, 0, -1 }, { TileSide::West, -1, 0 },
        };
        for (const auto& side : SIDES) {
            const Building* n = ctx.world.get_entity_as<Building>(
                ctx.world.building_at(surface, { origin_.x + side.dx, origin_.y + side.dy }));
            if (n && n->is_alive() && n->config().connects) connections |= side.bit;
        }
    }

    drawer.draw_ghost(ctx, *building_, origin_, tint, connections);
}

} // namespace Nightmourn
