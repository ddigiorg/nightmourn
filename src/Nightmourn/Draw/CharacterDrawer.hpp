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
// One look for every character for now. When characters get their own
// looks, map CharacterConfig (or an appearance id on it) to looks here.
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

        // The characters' look: a unit-sprite config's name, and the color its
        // (mask) sprites are tinted with
        const char* unit  = "figure";
        glm::vec4   color = glm::vec4(1.0f);
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer. Throws if `config.unit` has
    // no idle animation in the sheet.
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
    glm::vec4                          color_  = glm::vec4(1.0f);
};

} // namespace Nightmourn
