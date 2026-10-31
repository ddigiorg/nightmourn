// =============================================================================
// Draw/ResourceDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/ResourceDrawer.hpp"
#include "Nezumi/Render/2D/Atlas/Atlas.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <algorithm> // min
#include <cassert>   // assert
#include <cstdint>   // uint32_t

namespace Nightmourn {

// Which of `looks` a resource on `tile` wears: a hash of the tile, so
// neighbors differ and a resource keeps its look wherever and whenever it is
// drawn
static std::size_t look_index(TileCoord tile, std::size_t looks) {
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

ResourceDrawer::ResourceDrawer(const Config& config)
    : atlas_(config.atlas)
{
    assert(atlas_ && "ResourceDrawer: config.atlas is null");

    // Frames are inset by half a texel, so linear filtering at a sprite's
    // edge never samples its neighbor in the atlas. The frames' outer pixels
    // are transparent, so nothing visible is lost.
    const glm::vec2 size = config.tree_frame_size;
    for (std::size_t v = 0; v < trees_.size(); v++) {
        const Looks& looks = config.trees[v];
        trees_[v].count = std::min(looks.count, MAX_LOOKS);
        for (std::size_t i = 0; i < trees_[v].count; i++) {
            const glm::vec2 min = looks.origin + glm::vec2{ static_cast<float>(i) * size.x, 0.0f };
            trees_[v].tex_boxes[i] = atlas_->tex_box({ min + 0.5f, min + size - 0.5f });
        }
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ResourceDrawer::draw(DrawContext& ctx, const Resource& resource) const {
    if (!is_tree(resource)) return; // no art for other resources yet

    const std::size_t variety = resource.variety_id.value();
    if (variety >= trees_.size() || trees_[variety].count == 0) return;

    const VarietyFrames& frames = trees_[variety];
    const glm::vec2      pos    = resource.position.to_vec2();
    const std::size_t    look   = look_index(TileCoord::from_position(resource.position), frames.count);

    draw_sprite_at(ctx, *atlas_, frames.tex_boxes[look], pos, TREE_SPRITE_BOUNDS);

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, resource);
}

} // namespace Nightmourn
