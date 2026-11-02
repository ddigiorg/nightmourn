// =============================================================================
// Draw/CharacterDrawer.hpp
//
// Draws Characters (the hero, later NPCs): the sprite for the way the
// character faces, its healthbar once hurt, a swing while it mines (a
// placeholder slash in front of it until there are swing frames), and the
// debug overlays:
//
//   ShowCollisionRectangles  collision box
//   ShowFacingArrows         arrow along facing()
//
// One figure animation for every character for now. When characters get
// their own looks, map CharacterConfig (or an appearance id on it) to
// animations here.
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

        // The characters' figure animation in the sheet
        const char* figure = "units/cyan/idle";
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
    Nezumi::Render::AnimationID        figure_ = 0;
};

} // namespace Nightmourn
