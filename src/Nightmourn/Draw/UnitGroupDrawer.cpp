// =============================================================================
// Draw/UnitGroupDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/UnitGroupDrawer.hpp"
#include "Nezumi/Render/Renderer.hpp"
#include "Nezumi/World/Entities/Unit.hpp"
#include "Nezumi/World/Entities/UnitGroup.hpp"
#include "Nezumi/World/World.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <cstdint> // uint16_t
#include <vector>  // vector

namespace Nightmourn {

// By UnitGroupState
static constexpr std::array<glm::vec4, 3> STATE_COLORS = {{
    { 1.0f, 1.0f, 1.0f, 0.8f }, // Formed
    { 1.0f, 0.3f, 0.1f, 0.9f }, // Engaged
    { 1.0f, 0.9f, 0.2f, 0.9f }, // Reforming
}};

static constexpr glm::vec4 STRAGGLER_COLOR = { 1.0f, 0.9f, 0.2f, 0.9f };

// Line widths in pixels, sizes in tiles
static constexpr float BOUNDS_PIXELS    = 2.0f;
static constexpr float BOUNDS_PADDING   = 0.6f; // so members on the edge sit inside
static constexpr float ROUTE_PIXELS     = 2.0f;
static constexpr float FACING_PIXELS    = 3.0f;
static constexpr float FACING_LENGTH    = 1.5f;
static constexpr float LINK_PIXELS      = 1.0f; // engage and straggler lines
static constexpr float ANCHOR_RADIUS    = 0.2f;
static constexpr float WAYPOINT_RADIUS  = 0.35f;
static constexpr float SLOT_RADIUS      = 0.22f;

void UnitGroupDrawer::draw(DrawContext& ctx, const Nezumi::UnitGroup& group) const {
    if (!ctx.show(DebugFlag::ShowUnitGroupInfo)) return;

    const Nezumi::Area2D frame = Nezumi::Area2D(group.collision_box()).grow(BOUNDS_PADDING);
    if (!frame.overlaps(ctx.view)) return;

    Nezumi::Render::Renderer& r = ctx.renderer;
    const float px = ctx.pixel_size;

    const glm::vec4 side_color  = config_.faction_colors[group.faction_index()];
    const glm::vec4 state_color = STATE_COLORS[static_cast<std::size_t>(group.state())];
    const Nezumi::UnitGroup::Plan& plan = group.plan(ctx.tick);

    const Nezumi::Render::DrawParams ground = world_layer(DrawLayer::GroundOverlay);
    const Nezumi::Render::DrawParams debug  = world_layer(DrawLayer::Debug);

    draw_outline(ctx, frame, state_color, BOUNDS_PIXELS, DrawLayer::Debug);

    // Route: anchor -> waypoints
    glm::vec2 from = group.position.to_vec2();
    for (Nezumi::Position2D waypoint : group.waypoints()) {
        const glm::vec2 to = waypoint.to_vec2();
        r.draw_line(Nezumi::Render::Line{ .position_a = from, .position_b = to,
                                  .color = side_color * glm::vec4(1, 1, 1, 0.6f), .width = ROUTE_PIXELS * px },
                    ground);
        r.draw_circle(Nezumi::Render::Circle{ .position = to, .color = side_color,
                                      .radius = WAYPOINT_RADIUS, .width = ROUTE_PIXELS * px },
                      ground, /*.filled = */false);
        from = to;
    }

    // Anchor and facing
    const glm::vec2 anchor = group.position.to_vec2();
    r.draw_line(Nezumi::Render::Line{ .position_a = anchor, .position_b = anchor + plan.facing.to_vec2() * FACING_LENGTH,
                              .color = state_color, .width = FACING_PIXELS * px },
                debug);
    r.draw_circle(Nezumi::Render::Circle{ .position = anchor, .color = state_color, .radius = ANCHOR_RADIUS }, debug);

    if (group.state() == Nezumi::UnitGroupState::Engaged) {
        r.draw_line(Nezumi::Render::Line{ .position_a = anchor, .position_b = plan.engage_position.to_vec2(),
                                  .color = state_color * glm::vec4(1, 1, 1, 0.5f), .width = LINK_PIXELS * px },
                    debug);
        return; // nobody holds a slot while engaged
    }

    // Slots, and stragglers' lines to theirs
    const float straggle2 = group.config().straggler_distance * group.config().straggler_distance;
    const std::vector<Nezumi::EntityHandle>& members = group.members();

    for (std::size_t i = 0; i < members.size(); i++) {
        const Nezumi::Unit* unit = ctx.world.get_entity_as<Nezumi::Unit>(members[i]);
        if (!unit || unit->group() != group.handle) continue;

        const glm::vec2 slot = group.slot_position(ctx.tick, static_cast<uint16_t>(i)).to_vec2();
        r.draw_circle(Nezumi::Render::Circle{ .position = slot, .color = side_color * glm::vec4(1, 1, 1, 0.5f),
                                      .radius = SLOT_RADIUS, .width = LINK_PIXELS * px },
                      ground, /*.filled = */false);

        const glm::vec2 lag = slot - unit->position.to_vec2();
        if (lag.x * lag.x + lag.y * lag.y > straggle2) {
            r.draw_line(Nezumi::Render::Line{ .position_a = unit->position.to_vec2(), .position_b = slot,
                                      .color = STRAGGLER_COLOR, .width = LINK_PIXELS * px },
                        debug);
        }
    }
}

} // namespace Nightmourn
