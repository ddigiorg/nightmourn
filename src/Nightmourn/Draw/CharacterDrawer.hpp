// =============================================================================
// Draw/CharacterDrawer.hpp
//
// Draws Characters (the hero, later NPCs): the sprite for the way the
// character faces, its healthbar once hurt, and the debug overlays:
//
//   ShowCollisionRectangles  collision box
//   ShowFacingArrows         arrow along facing()
//
// One sprite row for every character for now. When characters get their own
// looks, map CharacterConfig (or an appearance id on it) to rows here.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <glm/glm.hpp> // vec2

namespace Nezumi::Render {
    class Atlas;
}

namespace Nightmourn {

class Character;

class CharacterDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::Atlas* atlas = nullptr;

        // Size of one frame in the atlas (pixels), and the top-left pixel of
        // the characters' row of frames (see FigureFrames)
        glm::vec2 frame_size = { 64.0f, 128.0f };
        glm::vec2 row        = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.atlas` must outlive the drawer
    explicit CharacterDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Character& character) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const Nezumi::Render::Atlas* atlas_  = nullptr;
    FigureFrames                 frames_ = {};
};

} // namespace Nightmourn
