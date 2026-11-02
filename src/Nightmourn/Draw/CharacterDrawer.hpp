// =============================================================================
// Draw/CharacterDrawer.hpp
//
// Draws Characters (the hero, later NPCs): its figure's animation for the
// way the character faces (attack for a moment after each swing, walk while
// it moves, otherwise idle), its healthbar once hurt, and the debug overlays:
//
//   ShowCollisionRectangles  collision box
//   ShowFacingArrows         arrow along facing()
//
// One figure for every character for now. When characters get their own
// looks, map CharacterConfig (or an appearance id on it) to figures here.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"

namespace Nezumi::Render {
    class SpriteSheet;
}

namespace Nightmourn {

class Character;

class CharacterDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        // The characters' figure (a figure config's name)
        const char* figure = "hero";
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer. Throws if `config.figure`
    // isn't in the sheet.
    explicit CharacterDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Character& character) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const Nezumi::Render::SpriteSheet* sheet_  = nullptr;
    FigureAnimations                   figure_ = {};
};

} // namespace Nightmourn
