// =============================================================================
// Draw/UnitDrawer.hpp
//
// Draws Nezumi Units: the sprite for the way the unit faces, its healthbar
// once hurt, and the unit debug overlays:
//
//   ShowCollisionRectangles  collision box
//   ShowFacingArrows         arrow along facing()
//   ShowUnitBehaviorInfo     dot colored by UnitState, and a line to the
//                            unit's target (yellow chasing, red attacking)
//
// Sprites are one atlas row per faction for now (red, blue, ...). Once units
// have their own art, the row comes from the unit's subtype as well.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <array>       // array
#include <glm/glm.hpp> // vec2

namespace Nezumi::Render {
    class Atlas;
}

namespace Nightmourn {

class Unit;

class UnitDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::Atlas* atlas = nullptr;

        // Size of one frame in the atlas (pixels)
        glm::vec2 frame_size = { 64.0f, 128.0f };

        // Top-left pixel of each faction's row of frames (see FigureFrames).
        // A unit with no faction uses row 0.
        std::array<glm::vec2, MAX_FACTIONS> faction_rows = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.atlas` must outlive the drawer
    explicit UnitDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Unit& unit) const;

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    void draw_behavior_(DrawContext& ctx, const Unit& unit) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const Nezumi::Render::Atlas*                   atlas_  = nullptr;
    std::array<FigureFrames, MAX_FACTIONS> frames_ = {};
};

} // namespace Nightmourn
