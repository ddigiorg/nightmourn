// =============================================================================
// Draw/ResourceDrawer.hpp
//
// Draws Resources. Only trees have art so far: each tree variety (oak, pine)
// has one or more looks side by side in the atlas, and a tree wears the one
// picked by a hash of its tile, so a forest doesn't repeat yet a tree never
// changes look (no sim state for it). Debug overlay:
//
//   ShowCollisionRectangles  collision box around the trunk
//
// Each look is one atlas frame with its ground shadow painted in
// (tools/gen_trees.py). Trees are y-sorted with the figures, so a unit walks
// in front of a tree, or behind it.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Resource/ResourceIDs.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <glm/glm.hpp> // vec2, vec4

namespace Nezumi::Render {
    class Atlas;
}

namespace Nightmourn {

class Resource;

class ResourceDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    static constexpr std::size_t MAX_LOOKS = 4; // per variety

    // `count` frames of the drawer's frame size, left to right from `origin`
    // (the top-left pixel of the first)
    struct Looks {
        glm::vec2   origin = {};
        std::size_t count  = 0;
    };

    struct Config {
        const Nezumi::Render::Atlas* atlas = nullptr;

        // Size of one tree frame in the atlas (pixels)
        glm::vec2 tree_frame_size = { 256.0f, 512.0f };

        // Looks of each tree variety, indexed by TreeVarieties. A variety
        // with no looks isn't drawn.
        std::array<Looks, TreeVarieties::COUNT> trees = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.atlas` must outlive the drawer
    explicit ResourceDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Resource& resource) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    struct VarietyFrames {
        std::array<glm::vec4, MAX_LOOKS> tex_boxes = {};
        std::size_t                      count     = 0;
    };

    const Nezumi::Render::Atlas*                    atlas_ = nullptr;
    std::array<VarietyFrames, TreeVarieties::COUNT> trees_ = {};
};

} // namespace Nightmourn
