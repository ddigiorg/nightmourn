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
// Sprites are one figure animation per faction for now (red, blue, ...).
// Once units have their own art, it comes from the unit's subtype as well.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <array>       // array

namespace Nezumi::Render {
    class SpriteSheet;
}

namespace Nightmourn {

class Unit;

class UnitDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        // Each faction's figure animation in the sheet. A faction left null
        // uses faction 0's, and so does a unit with no faction.
        std::array<const char*, MAX_FACTIONS> faction_figures = { "figure/red/idle" };
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer. Throws if an animation named
    // in the config isn't in the sheet.
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

    const Nezumi::Render::SpriteSheet*                    sheet_   = nullptr;
    std::array<Nezumi::Render::AnimationID, MAX_FACTIONS> figures_ = {};
};

} // namespace Nightmourn
