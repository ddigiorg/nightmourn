// =============================================================================
// Draw/DrawUtils.hpp
//
// Small pieces the entity drawers share: drawing sheet sprites at an
// entity's feet, picking a figure's heading from the way it faces, healthbars, and the debug shapes (collision boxes, facing
// arrows, outlines, ground circles).
//
// Positions passed in are WORLD positions; these project them (DrawContext).
// Ground shapes (outlines, collision boxes, ground circles) project every
// point, so they come out as diamonds and ellipses in iso. Figures and
// healthbars are upright: screen-aligned boxes around a projected anchor.
// =============================================================================

#pragma once
#include "Nezumi/Render/2D/Sprite/SpriteSheet.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Geometry/Position2D.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include <array>       // array
#include <cstdint>     // uint8_t, uint32_t
#include <glm/glm.hpp> // vec2, vec4
#include <string_view> // string_view

namespace Nightmourn {

class Entity;
class EntityDamageable;

// -----------------------------------------------------------------------------
// Sprites
//
// Sprites come from a Nezumi SpriteSheet (WorldSheets, packed at startup from
// the configs in data/) and are drawn upright at their pivot, which is
// the entity's feet. Art is painted at SPRITE_PIXELS_PER_TILE: a 64x128 px
// figure frame is 0.5x1 tile, a 256x512 px tree 2x4.
// -----------------------------------------------------------------------------

inline constexpr float SPRITE_PIXELS_PER_TILE = 128.0f;

// Which of an animation's `headings` (Nezumi::Render::Animation) a figure
// with `heading` (see Projection::dir_to_heading) shows: 0 faces down the
// screen, counting counter-clockwise (down-right, right, ...), in equal
// sectors centered on each heading
std::uint32_t heading_index(glm::vec2 heading, std::uint32_t headings);

// Pick boxes around an entity's feet (its position), in tiles of upright
// size: the frame each sprite is painted in. A tree is 6x a figure's size
// each way (AoE2's trees tower over its villagers). The tallest one sets
// DrawContext::VIEW_OVERHANG.
inline constexpr glm::vec4 FIGURE_SPRITE_BOUNDS = { -0.25f, -0.25f, 0.25f, 0.75f };
inline constexpr glm::vec4 TREE_SPRITE_BOUNDS   = { -1.50f, -1.50f, 1.50f, 4.50f };

// Sprite bounds of a drawable entity (above), or nullptr for types without a
// sprite. Picking (Input/Picking) asks here, so a click hits what is drawn.
const glm::vec4* sprite_bounds(const Entity& entity);

// View-space box of a sprite with `bounds` whose feet are at world `pos`
glm::vec4 sprite_view_box(const Projection&         proj, glm::vec2 pos, glm::vec4 bounds);

// View-space sprite box of a figure standing at world `pos`
inline glm::vec4 figure_view_box(const Projection&         proj, glm::vec2 pos) {
    return sprite_view_box(proj, pos, FIGURE_SPRITE_BOUNDS);
}

// Draws `sprite` of `sheet` with its pivot at world `pos`, y-sorted on
// DrawLayer::Entities, multiplied by `tint`
void draw_sprite_at(DrawContext& ctx, const Nezumi::Render::SpriteSheet& sheet,
                    Nezumi::Render::SpriteID sprite, glm::vec2 pos, bool mirrored = false,
                    glm::vec4 tint = glm::vec4(1.0f));

// Draws the frame of `animation` for a figure at world `pos` facing `facing`,
// `seconds` into the animation, y-sorted on DrawLayer::Entities. `color`
// tints it: a mask sheet's white takes the color, its black stays black.
void draw_figure(DrawContext& ctx, const Nezumi::Render::SpriteSheet& sheet,
                 Nezumi::Render::AnimationID animation, glm::vec2 pos, Position2D facing,
                 float seconds = 0.0f, glm::vec4 color = glm::vec4(1.0f));

// Seconds per simulation tick (60 UPS), for timing animations by ticks
inline constexpr float TICK_SECONDS = 1.0f / 60.0f;

// A figure's animations in the sheet, packed from its unit-sprite config as
// "unit/<name>/<animation>". Any but idle may be missing, and then plays
// idle. Throws if there's no idle.
struct FigureAnimations {
    Nezumi::Render::AnimationID idle   = 0;
    Nezumi::Render::AnimationID walk   = 0;
    Nezumi::Render::AnimationID attack = 0; // frame 0 is the hit
    Nezumi::Render::AnimationID death  = 0;

    static FigureAnimations find(const Nezumi::Render::SpriteSheet& sheet, std::string_view figure);
};

// Seconds into a looping animation for something `seconds` into it, offset
// by a per-entity phase (`seed`, e.g. a handle index) so a crowd doesn't
// step in lockstep
float figure_seconds(const Nezumi::Render::SpriteSheet& sheet, Nezumi::Render::AnimationID animation,
                     float seconds, std::uint32_t seed);

// -----------------------------------------------------------------------------
// Healthbars
// -----------------------------------------------------------------------------

// Healthbar above a figure's sprite, only once it is hurt
void draw_healthbar(DrawContext& ctx, const EntityDamageable& entity);

// -----------------------------------------------------------------------------
// Debug Shapes
// -----------------------------------------------------------------------------

// 1 px outline of the entity's collision box (ShowCollisionRectangles)
void draw_collision_box(DrawContext& ctx, const Entity& entity);

// 1 px arrow centered on `pos`, pointing along `facing` on screen
// (ShowFacingArrows)
void draw_facing_arrow(DrawContext& ctx, glm::vec2 pos, Position2D facing,
                       glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Outline of the world box `box` on the ground, with lines `pixels` wide:
// a rectangle top-down, a diamond in iso
void draw_outline(DrawContext& ctx, const Area2D& box, glm::vec4 color,
                  float pixels, std::uint8_t layer);

// Outline of the VIEW-space box `box` { x0, y0, x1, y1 } (e.g. a
// sprite_view_box), with lines `pixels` wide: upright, screen-aligned
void draw_view_outline(DrawContext& ctx, glm::vec4 box, glm::vec4 color,
                       float pixels, std::uint8_t layer);

// Circle of `radius` tiles around world `center` on the ground, with a line
// `pixels` wide: a circle top-down, a 2:1 ellipse in iso
void draw_ground_circle(DrawContext& ctx, glm::vec2 center, float radius, glm::vec4 color,
                        float pixels, std::uint8_t layer);

} // namespace Nightmourn
