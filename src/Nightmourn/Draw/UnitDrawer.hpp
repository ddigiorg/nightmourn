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
// Each unit has a look picked by its faction and its subtype
// (UnitConfig::subtype): a unit config's sprites (data/base/configs/entity/
// units.json) and the color they are tinted with. It plays the animation for
// its UnitState: idle, walk, or attack, the attack timed so its frame 0 (the
// hit) lands as the unit's hit does.
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

    // Unit subtypes with a look of their own; higher ones use subtype 0's
    static constexpr std::size_t MAX_SUBTYPES = 4;

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        struct Look {
            const char* unit  = nullptr;          // a unit config's name
            glm::vec4   color = glm::vec4(1.0f);  // tints its (mask) sprites
        };

        // Looks by faction, then by unit subtype (its value; 0 for none). A
        // look with no unit falls back to the faction's subtype 0, then to
        // faction 0's; a unit with no faction uses faction 0's.
        std::array<std::array<Look, MAX_SUBTYPES>, MAX_FACTIONS> looks = {{ { Look{ "figure" } } }};
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
    struct Resolved {
        FigureAnimations anims = {};
        glm::vec4        color = glm::vec4(1.0f);
    };
    std::array<std::array<Resolved, MAX_SUBTYPES>, MAX_FACTIONS> looks_ = {};
};

} // namespace Nightmourn
