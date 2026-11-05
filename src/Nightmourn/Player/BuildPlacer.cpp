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

float BuildPlacer::distance_to(glm::vec2 p, const TileRect& rect) {
    const glm::vec2 lo = { static_cast<float>(rect.min.x), static_cast<float>(rect.min.y) };
    const glm::vec2 hi = { static_cast<float>(rect.max.x + 1), static_cast<float>(rect.max.y + 1) };
    return glm::length(glm::max(glm::max(lo - p, p - hi), glm::vec2(0.0f)));
}

BuildPlacer::Blocker BuildPlacer::blocker_at_(TileCoord at, const Builder* builder) const {
    if (!builder) return Blocker::NoBuilder;
    if (distance_to(builder->position, TileRect::from_size(at, building_->footprint)) > builder->reach)
        return Blocker::OutOfReach;
    if (building_->type != BuildingType::Banner && !builder->in_settlement) return Blocker::NotInSettlement;
    return Blocker::None;
}

void BuildPlacer::update(World& world, SurfaceID surface,
                         std::span<Inventory* const> items_from, const Builder* builder,
                         EntityFactionsMask factions, std::optional<glm::vec2> cursor,
                         const Nezumi::InputMap& input)
{
    if (!building_) return;
    if (input.was_pressed(config_.cancel)) { cancel(); return; }

    has_cursor_ = cursor.has_value();
    if (!has_cursor_) { painted_.reset(); return; }

    origin_  = origin_at(*building_, *cursor);
    result_  = world.check_placement(surface, *building_, origin_, factions);
    blocker_ = blocker_at_(origin_, builder);

    // A click places; holding paints walls, once per tile
    const bool held    = input.is_held(config_.place);
    const bool clicked = input.was_pressed(config_.place);
    if (!held && !clicked) { painted_.reset(); return; }
    if (!clicked && !building_->connects) return;

    // The item is spent only once the building is down
    const ItemAmount one[] = { { building_->item, 1 } };
    const auto place_at = [&](TileCoord at) {
        if (count(items_from, building_->item) == 0) return;
        if (blocker_at_(at, builder) != Blocker::None) return;
        if (!world.place_building(surface, *building_, at, factions)) return;
        pay(one, items_from);
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

    result_    = world.check_placement(surface, *building_, origin_, factions);
    blocker_ = blocker_at_(origin_, builder);
    if (count(items_from, building_->item) == 0) cancel();
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void BuildPlacer::draw(DrawContext& ctx, const BuildingDrawer& drawer, SurfaceID surface) const {
    if (!building_ || !has_cursor_) return;

    const bool      ok   = can_place();
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
