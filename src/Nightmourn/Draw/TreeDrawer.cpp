// =============================================================================
// Draw/TreeDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/TreeDrawer.hpp"
#include "Nezumi/Render/Atlas/Atlas.hpp"
#include "Nezumi/World/Physics/TileCoord.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nightmourn/Resource/Tree.hpp"
#include <algorithm> // min
#include <cassert>   // assert
#include <cstdint>   // uint32_t

namespace Nightmourn {

// Which look a tree on `tile` wears: a hash of the tile, so neighbors differ
// and a tree keeps its look wherever and whenever it is drawn
static std::size_t look_index(Nezumi::TileCoord tile, std::size_t looks) {
    std::uint32_t h = static_cast<std::uint32_t>(tile.x) * 0x9E3779B1u
                    ^ static_cast<std::uint32_t>(tile.y) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0xC2B2AE3Du;
    h ^= h >> 13;
    return h % looks;
}

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

TreeDrawer::TreeDrawer(const Config& config)
    : atlas_(config.atlas)
    , looks_(std::min(config.looks, MAX_LOOKS))
{
    assert(atlas_ && "TreeDrawer: config.atlas is null");
    assert(looks_ > 0 && "TreeDrawer: config.looks is 0");

    // Frames are inset by half a texel, so linear filtering at a sprite's
    // edge never samples its neighbor in the atlas (the unit rows to the
    // left would draw a dark line down the side of the first look). The
    // frames' outer pixels are transparent, so nothing visible is lost.
    for (std::size_t i = 0; i < looks_; i++) {
        const glm::vec2 min = config.origin + glm::vec2{ static_cast<float>(i) * config.frame_size.x, 0.0f };
        tex_boxes_[i] = atlas_->tex_box({ min + 0.5f, min + config.frame_size - 0.5f });
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void TreeDrawer::draw(DrawContext& ctx, const Tree& tree) const {
    const glm::vec2   pos  = tree.position.to_vec2();
    const std::size_t look = look_index(Nezumi::TileCoord::from_position(tree.position), looks_);

    draw_sprite_at(ctx, *atlas_, tex_boxes_[look], pos, TREE_SPRITE_BOUNDS);

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, tree);
}

} // namespace Nightmourn
