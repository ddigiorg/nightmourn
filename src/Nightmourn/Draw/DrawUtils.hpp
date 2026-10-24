// =============================================================================
// Draw/DrawUtils.hpp
//
// Small pieces the entity drawers share: picking a figure's frame from the
// way it faces, healthbars, and the debug shapes (collision boxes, facing
// arrows, outlines).
// =============================================================================

#pragma once
#include "Nezumi/Render/Atlas/Atlas.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nezumi/World/Physics/Position2D.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include <array>       // array
#include <cstdint>     // uint8_t
#include <glm/glm.hpp> // vec2, vec4

namespace Nezumi {
    class Entity;
    class EntityDamageable;
}

namespace Nightmourn {

// -----------------------------------------------------------------------------
// FigureFrames
//
// One atlas row of a walking figure (unit, character): 5 frames facing down,
// down-right, right, up-right and up. The left-facing ones are the right ones
// mirrored. Texture boxes are computed once, not per draw.
// -----------------------------------------------------------------------------

struct FigureFrames {
    static constexpr std::size_t HEADINGS = 5;

    std::array<glm::vec4, HEADINGS> tex_boxes = {};

    // Frames of `frame_size` pixels, left to right from `row_origin` (the
    // top-left pixel of the "down" frame)
    static FigureFrames from_row(const Nezumi::Render::Atlas& atlas,
                                 glm::vec2 row_origin, glm::vec2 frame_size);

    // Texture box for a figure facing `facing` (any length), mirrored when
    // it faces left. 8 equal sectors of 45 degrees.
    glm::vec4 pick(Nezumi::Position2D facing) const;
};

// Sprite size in tiles around a figure's feet (its position)
inline constexpr glm::vec4 FIGURE_SPRITE_BOUNDS = { -0.25f, -0.25f, 0.25f, 0.75f };

// Draws a figure sprite at its feet, y-sorted on DrawLayer::Entities
void draw_figure(DrawContext& ctx, const Nezumi::Render::Atlas& atlas,
                 const FigureFrames& frames, glm::vec2 pos, Nezumi::Position2D facing);

// -----------------------------------------------------------------------------
// Healthbars
// -----------------------------------------------------------------------------

// Healthbar above a figure's sprite, only once it is hurt
void draw_healthbar(DrawContext& ctx, const Nezumi::EntityDamageable& entity);

// -----------------------------------------------------------------------------
// Debug Shapes
// -----------------------------------------------------------------------------

// 1 px outline of the entity's collision box (ShowCollisionRectangles)
void draw_collision_box(DrawContext& ctx, const Nezumi::Entity& entity);

// 1 px arrow centered on `pos`, pointing along `facing` (ShowFacingArrows)
void draw_facing_arrow(DrawContext& ctx, glm::vec2 pos, Nezumi::Position2D facing,
                       glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Outline of `box` with lines `pixels` wide (any width, unlike the 1 px
// unfilled rectangle)
void draw_outline(DrawContext& ctx, const Nezumi::Area2D& box, glm::vec4 color,
                  float pixels, std::uint8_t layer);

} // namespace Nightmourn
