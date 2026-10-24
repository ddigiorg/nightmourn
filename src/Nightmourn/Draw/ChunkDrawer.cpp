// =============================================================================
// Draw/ChunkDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/ChunkDrawer.hpp"
#include "Nezumi/Render/Renderer.hpp"
#include "Nezumi/World/Chunk/Chunk.hpp"
#include "Nezumi/World/Constants.hpp"
#include "Nezumi/World/Physics/TileCoord.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <algorithm> // max, min
#include <cassert>   // assert

namespace Nightmourn {

// Shrinks a pixel box by half a texel on each side, so linear filtering never
// samples the neighboring sprite
static constexpr glm::vec4 inset_half_texel(glm::vec4 pixel_box) {
    return pixel_box + glm::vec4{ 0.5f, 0.5f, -0.5f, -0.5f };
}

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

ChunkDrawer::ChunkDrawer(const Config& config)
    : config_(config)
{
    assert(config_.atlas && "ChunkDrawer: config.atlas is null");
    tex_grass_dark_  = config_.atlas->tex_box(inset_half_texel(config_.grass_dark ));
    tex_grass_light_ = config_.atlas->tex_box(inset_half_texel(config_.grass_light));
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ChunkDrawer::draw(DrawContext& ctx, const Nezumi::Chunk& chunk) const {
    draw_tiles_(ctx, chunk);

    if (ctx.show(DebugFlag::ShowTileGrid) || ctx.show(DebugFlag::ShowChunkBorders))
        draw_grid_(ctx, chunk);
}

// Only the tiles inside the view: a chunk on the edge of the screen is mostly
// off it
void ChunkDrawer::draw_tiles_(DrawContext& ctx, const Nezumi::Chunk& chunk) const {
    const Nezumi::Area2D& bounds = chunk.bounds();
    const Nezumi::Area2D& view   = ctx.view;

    const Nezumi::TileCoord min = Nezumi::TileCoord::from_position({
        std::max(bounds.min.x, view.min.x),
        std::max(bounds.min.y, view.min.y),
    });
    const Nezumi::TileCoord max = Nezumi::TileCoord::from_position({
        std::min(bounds.max.x, view.max.x) - 0.001f,
        std::min(bounds.max.y, view.max.y) - 0.001f,
    });

    Nezumi::Render::DrawParams params = world_layer(DrawLayer::Ground);
    params.material = config_.atlas->material();

    Nezumi::TileCoord tile;
    for (tile.y = min.y; tile.y <= max.y; tile.y++) {
        for (tile.x = min.x; tile.x <= max.x; tile.x++) {
            ctx.renderer.draw_sprite(
                Nezumi::Render::Sprite{
                    .pos_box = tile.bounds().to_vec4(),
                    .tex_box = ((tile.x + tile.y) & 1) ? tex_grass_light_ : tex_grass_dark_,
                },
                params
            );
        }
    }
}

void ChunkDrawer::draw_grid_(DrawContext& ctx, const Nezumi::Chunk& chunk) const {
    const Nezumi::Area2D& bounds = chunk.bounds();
    const Nezumi::Render::DrawParams params = world_layer(DrawLayer::GroundOverlay);

    // Tile lines: the 31 interior lines on each axis, each the chunk's full
    // length. Skipped when tiles are too small on screen to read.
    const float tile_pixels = 1.0f / ctx.pixel_size;
    if (ctx.show(DebugFlag::ShowTileGrid) && tile_pixels >= config_.min_tile_grid_pixels) {
        const float width = config_.tile_line_pixels * ctx.pixel_size;

        for (uint32_t i = 1; i < Nezumi::CHUNK_TILE_COUNT.x; i++) {
            const float x = bounds.min.x + static_cast<float>(i);
            ctx.renderer.draw_line(
                Nezumi::Render::Line{
                    .position_a = { x, bounds.min.y },
                    .position_b = { x, bounds.max.y },
                    .color      = config_.grid_color,
                    .width      = width,
                },
                params
            );
        }

        for (uint32_t i = 1; i < Nezumi::CHUNK_TILE_COUNT.y; i++) {
            const float y = bounds.min.y + static_cast<float>(i);
            ctx.renderer.draw_line(
                Nezumi::Render::Line{
                    .position_a = { bounds.min.x, y },
                    .position_b = { bounds.max.x, y },
                    .color      = config_.grid_color,
                    .width      = width,
                },
                params
            );
        }
    }

    // Chunk edges: the tile grid's outer lines, thicker with chunk borders
    // on. Neighbors both draw their shared edge; it's 4 lines per chunk, and
    // drawing all 4 keeps the edge of the map closed.
    const float edge_pixels = ctx.show(DebugFlag::ShowChunkBorders)
        ? config_.chunk_line_pixels
        : config_.tile_line_pixels;

    draw_outline(ctx, bounds, config_.grid_color, edge_pixels, DrawLayer::GroundOverlay);
}

} // namespace Nightmourn
