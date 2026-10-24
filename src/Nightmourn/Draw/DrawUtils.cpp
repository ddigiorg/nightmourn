// =============================================================================
// Draw/DrawUtils.cpp
// =============================================================================

#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nezumi/Render/Renderer.hpp"
#include "Nezumi/World/Entity/EntityDamageable.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include <cmath>   // atan2
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

glm::vec4 FigureFrames::pick(Nezumi::Position2D facing) const {
    // Column: 0 down, 1 down-right, 2 right, 3 up-right, 4 up.
    // 0.383 = sin(22.5 deg) splits the circle into 8 equal sectors.
    const int dx = (facing.x > 0.383f) - (facing.x < -0.383f);
    const int dy = (facing.y > 0.383f) - (facing.y < -0.383f);
    const int column = (dx == 0) ? (dy > 0 ? 4 : 0) : (dy + 2);

    glm::vec4 tex_box = tex_boxes[static_cast<std::size_t>(column)];
    if (dx < 0) std::swap(tex_box.x, tex_box.z); // mirror
    return tex_box;
}

void draw_figure(DrawContext& ctx, const Nezumi::Render::Atlas& atlas,
                 const FigureFrames& frames, glm::vec2 pos, Nezumi::Position2D facing)
{
    Nezumi::Render::DrawParams params = world_layer(DrawLayer::Entities);
    params.material = atlas.material();
    params.order    = y_sort_order(pos.y);

    ctx.renderer.draw_sprite(
        Nezumi::Render::Sprite{
            .pos_box = glm::vec4{ pos, pos } + FIGURE_SPRITE_BOUNDS,
            .tex_box = frames.pick(facing),
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

    const glm::vec2 pos  = entity.position.to_vec2();
    const glm::vec4 back = glm::vec4{ pos, pos } + HEALTHBAR_BOUNDS;

    glm::vec4 fill = back + glm::vec4{ HEALTHBAR_BORDER, HEALTHBAR_BORDER,
                                      -HEALTHBAR_BORDER, -HEALTHBAR_BORDER };
    fill.z = fill.x + (fill.z - fill.x) * entity.health_ratio();

    constexpr Nezumi::Render::DrawParams params = world_layer(DrawLayer::EntityOverlay);
    ctx.renderer.draw_rectangle(Nezumi::Render::Rectangle{ .pos_box = back, .color = { 0.0f, 0.0f, 0.0f, 1.0f } }, params);
    ctx.renderer.draw_rectangle(Nezumi::Render::Rectangle{ .pos_box = fill, .color = { 1.0f, 1.0f, 1.0f, 1.0f } }, params);
}

// -----------------------------------------------------------------------------
// Debug Shapes
// -----------------------------------------------------------------------------

static constexpr glm::vec4 COLLISION_BOX_COLOR = { 0.2f, 1.0f, 0.2f, 1.0f };
static constexpr float     FACING_ARROW_SIZE   = 0.6f; // tiles

void draw_collision_box(DrawContext& ctx, const Nezumi::Entity& entity) {
    ctx.renderer.draw_rectangle(
        Nezumi::Render::Rectangle{ .pos_box = entity.collision_box().to_vec4(), .color = COLLISION_BOX_COLOR },
        world_layer(DrawLayer::Debug),
        /*.filled = */false
    );
}

void draw_facing_arrow(DrawContext& ctx, glm::vec2 pos, Nezumi::Position2D facing, glm::vec4 color) {
    ctx.renderer.draw_arrow(
        Nezumi::Render::Arrow{
            .position = pos,
            .size     = FACING_ARROW_SIZE,
            .angle    = std::atan2(facing.y, facing.x),
            .color    = color,
        },
        world_layer(DrawLayer::Debug)
    );
}

void draw_outline(DrawContext& ctx, const Nezumi::Area2D& box, glm::vec4 color,
                  float pixels, std::uint8_t layer)
{
    // Horizontal edges run past the corners by half a width, so the corners
    // are square instead of notched
    const float width = pixels * ctx.pixel_size;
    const float half  = width * 0.5f;
    const Nezumi::Render::DrawParams params = world_layer(layer);

    const auto line = [&](glm::vec2 a, glm::vec2 b) {
        ctx.renderer.draw_line(Nezumi::Render::Line{ .position_a = a, .position_b = b, .color = color, .width = width },
                               params);
    };

    line({ box.min.x - half, box.min.y }, { box.max.x + half, box.min.y }); // bottom
    line({ box.min.x - half, box.max.y }, { box.max.x + half, box.max.y }); // top
    line({ box.min.x, box.min.y }, { box.min.x, box.max.y });               // left
    line({ box.max.x, box.min.y }, { box.max.x, box.max.y });               // right
}

} // namespace Nightmourn
