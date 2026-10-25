// =============================================================================
// Draw/TreeDrawer.hpp
//
// Draws Trees: one of a few looks, chosen from the tree's tile so a forest
// doesn't repeat yet a tree never changes look (no sim state for it), and the
// debug overlay:
//
//   ShowCollisionRectangles  collision box around the trunk
//
// Each look is one atlas frame of 2x a figure's size, with its ground shadow
// painted in (tools/gen_trees.py). Trees are y-sorted with the figures, so a
// unit walks in front of a tree, or behind it.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <glm/glm.hpp> // vec2, vec4

namespace Nezumi::Render {
    class Atlas;
}

namespace Nightmourn {

class Tree;

class TreeDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    static constexpr std::size_t MAX_LOOKS = 4;

    struct Config {
        const Nezumi::Render::Atlas* atlas = nullptr;

        // Size of one frame in the atlas (pixels), the top-left pixel of the
        // first one, and how many sit side by side from there
        glm::vec2   frame_size = { 128.0f, 256.0f };
        glm::vec2   origin     = {};
        std::size_t looks      = 1;
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.atlas` must outlive the drawer
    explicit TreeDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Tree& tree) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const Nezumi::Render::Atlas*     atlas_     = nullptr;
    std::array<glm::vec4, MAX_LOOKS> tex_boxes_ = {};
    std::size_t                      looks_     = 0;
};

} // namespace Nightmourn
