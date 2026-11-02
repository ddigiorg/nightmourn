// =============================================================================
// Draw/ResourceDrawer.hpp
//
// Draws Nezumi Resources. Only trees have art so far: each tree variety (oak, pine)
// has one or more looks in the sprite sheet (its tree config's pictures,
// packed as the animation "tree/<name>": one frame per look, see
// WorldSheets.hpp), and a tree wears the one picked by a
// hash of its tile, so a forest doesn't repeat yet a tree never changes look
// (no sim state for it). Debug overlay:
//
//   ShowCollisionRectangles  collision box around the trunk
//
// Each look is one frame with its ground shadow painted in
// (tools/gen_trees.py). Trees are y-sorted with the figures, so a unit walks
// in front of a tree, or behind it.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include <array>       // array
#include <cstddef>     // size_t
#include <cstdint>     // uint32_t

namespace Nezumi::Render {
    class SpriteSheet;
}

namespace Nightmourn {

class ResourceDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    static constexpr std::size_t TREE_VARIETIES = static_cast<std::size_t>(TreeVariety::Count);

    struct Config {
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        // Looks animation of each tree variety in the sheet, indexed by
        // TreeVariety. A variety left null, or not in the sheet, isn't drawn.
        std::array<const char*, TREE_VARIETIES> trees = { "tree/oak", "tree/pine" };
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer
    explicit ResourceDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Resource& resource) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    // A variety's looks: sprites first .. first + count - 1
    struct Looks {
        std::uint32_t first = 0;
        std::uint32_t count = 0;
    };

    const Nezumi::Render::SpriteSheet*  sheet_ = nullptr;
    std::array<Looks, TREE_VARIETIES>   trees_ = {};
};

} // namespace Nightmourn
