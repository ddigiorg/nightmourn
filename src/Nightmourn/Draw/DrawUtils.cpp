// =============================================================================
// Draw/DrawUtils.cpp
// =============================================================================

#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nezumi/Render/Renderer.hpp"
#include "Nezumi/World/Entity/EntityDamageable.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include <cmath>   // atan2, cos, sin
#include <utility> // swap

namespace Nightmourn {

// -----------------------------------------------------------------------------
// FigureFrames
// -----------------------------------------------------------------------------

FigureFrames FigureFrames::from_row(const Nezumi::Render::Atlas& atlas,
                                    glm::vec2 row_origin, glm::vec2 frame_size)
{
    FigureFrames frames;
    for (std::size_t i = 0; i < HEADINGS; i++) {
        const glm::vec2 min = row_origin + glm::vec2{ static_cast<float>(i) * frame_size.x, 0.0f };
        frames.tex_boxes[i] = atlas.tex_box({ min, min + frame_size });
    }
    return frames;
}

glm::vec4 FigureFrames::pick(glm::vec2 heading) const {
    // Column: 0 down, 1 down-right, 2 right, 3 up-right, 4 up.
    // 0.383 = sin(22.5 deg) splits the circle into 8 equal sectors.
    const int dx = (heading.x > 0.383f) - (heading.x < -0.383f);
    const int dy = (heading.y > 0.383f) - (heading.y < -0.383f);
    const int column = (dx == 0) ? (dy > 0 ? 4 : 0) : (dy + 2);

    glm::vec4 tex_box = tex_boxes[static_cast<std::size_t>(column)];
    if (dx < 0) std::swap(tex_box.x, tex_box.z); // mirror
    return tex_box;
}

glm::vec4 figure_view_box(const Nezumi::Render::Projection& proj, glm::vec2 pos) {
    const glm::vec2 feet = proj.to_view(pos);
    return glm::vec4{ feet, feet } + FIGURE_SPRITE_BOUNDS * proj.tile_size().x;
}

void draw_figure(DrawContext& ctx, const Nezumi::Render::Atlas& atlas,
                 const FigureFrames& frames, glm::vec2 pos, Nezumi::Position2D facing)
{
    // Sorted by the feet's view y: further up the screen draws first
    Nezumi::Render::DrawParams params = world_layer(DrawLayer::Entities);
    params.material = atlas.material();
    params.order    = y_sort_order(ctx.to_view(pos).y);

    ctx.renderer.draw_sprite(
        Nezumi::Render::Sprite{
            .pos_box = figure_view_box(ctx.proj, pos),
            .tex_box = frames.pick(ctx.proj.dir_to_heading(facing.to_vec2())),
        },
        params
    );
}

// -----------------------------------------------------------------------------
// Healthbars
// -----------------------------------------------------------------------------

static constexpr glm::vec4 HEALTHBAR_BOUNDS = { -0.3f, 0.82f, 0.3f, 0.97f };
static constexpr float     HEALTHBAR_BORDER = 0.04f;

void draw_healthbar(DrawContext& ctx, const Nezumi::EntityDamageable& entity) {
    if (entity.is_full_health()) return;

    // Upright, like the figure it floats over
    const glm::vec2 feet   = ctx.to_view(entity.position.to_vec2());
    const float     scale  = ctx.upright_scale();
    const float     border = HEALTHBAR_BORDER * scale;
    const glm::vec4 back   = glm::vec4{ feet, feet } + HEALTHBAR_BOUNDS * scale;

    glm::vec4 fill = back + glm::vec4{ border, border, -border, -border };
    fill.z = fill.x + (fill.z - fill.x) * entity.health_ratio();

    constexpr Nezumi::Render::DrawParams params = world_layer(DrawLayer::EntityOverlay);
    ctx.renderer.draw_rectangle(Nezumi::Render::Rectangle{ .pos_box = back, .color = { 0.0f, 0.0f, 0.0f, 1.0f } }, params);
    ctx.renderer.draw_rectangle(Nezumi::Render::Rectangle{ .pos_box = fill, .color = { 1.0f, 1.0f, 1.0f, 1.0f } }, params);
}

// -----------------------------------------------------------------------------
// Debug Shapes
// -----------------------------------------------------------------------------

static constexpr glm::vec4 COLLISION_BOX_COLOR    = { 0.2f, 1.0f, 0.2f, 1.0f };
static constexpr float     FACING_ARROW_SIZE      = 0.6f; // tiles
static constexpr int       GROUND_CIRCLE_SEGMENTS = 24;

void draw_collision_box(DrawContext& ctx, const Nezumi::Entity& entity) {
    draw_outline(ctx, entity.collision_box(), COLLISION_BOX_COLOR, 1.0f, DrawLayer::Debug);
}

void draw_facing_arrow(DrawContext& ctx, glm::vec2 pos, Nezumi::Position2D facing, glm::vec4 color) {
    // The arrow lies on the ground, so it points the way the projected
    // facing points on screen
    const glm::vec2 dir = ctx.proj.dir_to_view(facing.to_vec2());

    ctx.renderer.draw_arrow(
        Nezumi::Render::Arrow{
            .position = ctx.to_view(pos),
            .size     = FACING_ARROW_SIZE * ctx.upright_scale(),
            .angle    = std::atan2(dir.y, dir.x),
            .color    = color,
        },
        world_layer(DrawLayer::Debug)
    );
}

void draw_outline(DrawContext& ctx, const Nezumi::Area2D& box, glm::vec4 color,
                  float pixels, std::uint8_t layer)
{
    const float width = pixels * ctx.pixel_size;
    const Nezumi::Render::DrawParams params = world_layer(layer);
    const std::array<glm::vec2, 4> corners = ctx.proj.box_to_view(box.to_vec4());

    // Each edge runs past both corners by half a width, so the corners are
    // closed instead of notched, at any angle
    for (std::size_t i = 0; i < corners.size(); i++) {
        const glm::vec2 a   = corners[i];
        const glm::vec2 b   = corners[(i + 1) % corners.size()];
        if (a == b) continue; // an empty box has no edges
        const glm::vec2 ext = glm::normalize(b - a) * (width * 0.5f);
        ctx.renderer.draw_line(
            Nezumi::Render::Line{ .position_a = a - ext, .position_b = b + ext, .color = color, .width = width },
            params);
    }
}

void draw_ground_circle(DrawContext& ctx, glm::vec2 center, float radius, glm::vec4 color,
                        float pixels, std::uint8_t layer)
{
    const float width = pixels * ctx.pixel_size;
    const Nezumi::Render::DrawParams params = world_layer(layer);

    // Top-down the circle kind is exact; a projected circle is an ellipse,
    // drawn as a ring of segments
    if (ctx.proj.is_top_down()) {
        ctx.renderer.draw_circle(
            Nezumi::Render::Circle{ .position = center, .color = color, .radius = radius, .width = width },
            params, /*.filled = */false);
        return;
    }

    constexpr float STEP = 6.28318531f / static_cast<float>(GROUND_CIRCLE_SEGMENTS);
    glm::vec2 prev = ctx.to_view(center + glm::vec2{ radius, 0.0f });
    for (int i = 1; i <= GROUND_CIRCLE_SEGMENTS; i++) {
        const float     angle = STEP * static_cast<float>(i);
        const glm::vec2 next  = ctx.to_view(center + radius * glm::vec2{ std::cos(angle), std::sin(angle) });
        ctx.renderer.draw_line(
            Nezumi::Render::Line{ .position_a = prev, .position_b = next, .color = color, .width = width },
            params);
        prev = next;
    }
}

} // namespace Nightmourn
