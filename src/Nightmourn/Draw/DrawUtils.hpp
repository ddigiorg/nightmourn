// =============================================================================
// Draw/DrawUtils.hpp
//
// Small pieces the entity drawers share: picking a figure's frame from the
// way it faces, healthbars, and the debug shapes (collision boxes, facing
// arrows, outlines, ground circles).
//
// Positions passed in are WORLD positions; these project them (DrawContext).
// Ground shapes (outlines, collision boxes, ground circles) project every
// point, so they come out as diamonds and ellipses in iso. Figures and
// healthbars are upright: screen-aligned boxes around a projected anchor.
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

    // Texture box for a figure with heading `heading` (unit length; see
    // Projection::dir_to_heading), mirrored when it faces left. 8 equal
    // sectors of 45 degrees: (0, -1) faces down the screen.
    glm::vec4 pick(glm::vec2 heading) const;
};

// Sprite box around a figure's feet (its position), in tiles of upright size
inline constexpr glm::vec4 FIGURE_SPRITE_BOUNDS = { -0.25f, -0.25f, 0.25f, 0.75f };

// View-space sprite box of a figure standing at world `pos`. Drawing and
// picking (Input/Picking) share it, so a click hits what is drawn.
glm::vec4 figure_view_box(const Nezumi::Render::Projection& proj, glm::vec2 pos);

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

// 1 px arrow centered on `pos`, pointing along `facing` on screen
// (ShowFacingArrows)
void draw_facing_arrow(DrawContext& ctx, glm::vec2 pos, Nezumi::Position2D facing,
                       glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Outline of the world box `box` on the ground, with lines `pixels` wide:
// a rectangle top-down, a diamond in iso
void draw_outline(DrawContext& ctx, const Nezumi::Area2D& box, glm::vec4 color,
                  float pixels, std::uint8_t layer);

// Circle of `radius` tiles around world `center` on the ground, with a line
// `pixels` wide: a circle top-down, a 2:1 ellipse in iso
void draw_ground_circle(DrawContext& ctx, glm::vec2 center, float radius, glm::vec4 color,
                        float pixels, std::uint8_t layer);

} // namespace Nightmourn
