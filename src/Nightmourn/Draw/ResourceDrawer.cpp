// =============================================================================
// Draw/ResourceDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/ResourceDrawer.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
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
    : sheet_(config.sheet)
{
    assert(sheet_ && "ResourceDrawer: config.sheet is null");

    for (std::size_t v = 0; v < trees_.size(); v++) {
        if (!config.trees[v]) continue;
        const Nezumi::Render::AnimationID* id = sheet_->find_animation(config.trees[v]);
        if (!id) continue;
        const Nezumi::Render::Animation& looks = sheet_->anim(*id);
        trees_[v] = { looks.first, looks.frames * looks.directions };
    }
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ResourceDrawer::draw(DrawContext& ctx, const Resource& resource) const {
    if (!is_tree(resource)) return; // no art for other resources yet

    const std::size_t variety = resource.variety_id.value();
    if (variety >= trees_.size() || trees_[variety].count == 0) return;

    const Looks&      looks = trees_[variety];
    const glm::vec2   pos   = resource.position.to_vec2();
    const std::size_t look  = look_index(TileCoord::from_position(resource.position), looks.count);

    draw_sprite_at(ctx, *sheet_, looks.first + static_cast<std::uint32_t>(look), pos);

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, resource);
}

} // namespace Nightmourn
