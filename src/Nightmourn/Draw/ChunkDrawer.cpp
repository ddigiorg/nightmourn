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
#include <cmath>     // ceil, floor
#include <cstdint>   // int32_t
#include <utility>   // swap

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

    if (ctx.show(DebugFlag::ShowWorldGrid))
        draw_grid_(ctx, chunk);
}

// Columns [first, last] of tile row `y` whose projected tile overlaps the
// view rectangle on one view axis. `ax` and `by` are where world +x and +y
// land on that axis, `vmin`/`vmax` the rectangle's extent along it. A tile
// spans x..x+1 by y..y+1, so on this axis it covers
// ax*x + by*y + [lo, hi]. Returns false if no column of the row can overlap.
static bool overlap_columns(float ax, float by, float vmin, float vmax, int32_t y,
                            int32_t& first, int32_t& last)
{
    const float lo  = std::min(ax, 0.0f) + std::min(by, 0.0f);
    const float hi  = std::max(ax, 0.0f) + std::max(by, 0.0f);
    const float row = by * static_cast<float>(y);

    // Overlap: ax*x + row + hi >= vmin and ax*x + row + lo <= vmax
    const float from = vmin - hi - row; // ax*x >= from
    const float to   = vmax - lo - row; // ax*x <= to

    if (ax == 0.0f) return from <= 0.0f && 0.0f <= to; // the row is in or out whole

    float x0 = from / ax;
    float x1 = to   / ax;
    if (ax < 0.0f) std::swap(x0, x1);

    first = std::max(first, static_cast<int32_t>(std::ceil (x0)));
    last  = std::min(last,  static_cast<int32_t>(std::floor(x1)));
    return true;
}

// Only the tiles whose projected diamond (or square, top-down) overlaps the
// view: a chunk on the edge of the screen is mostly off it, and in iso even
// the world box of the view holds twice the tiles on screen. Rows are culled
// exactly, one span per row.
void ChunkDrawer::draw_tiles_(DrawContext& ctx, const Nezumi::Chunk& chunk) const {
    const Nezumi::Area2D& bounds = chunk.bounds();
    const Nezumi::Area2D& world  = ctx.world_view;
    const Nezumi::Area2D& view   = ctx.view;

    const Nezumi::TileCoord min = Nezumi::TileCoord::from_position({
        std::max(bounds.min.x, world.min.x),
        std::max(bounds.min.y, world.min.y),
    });
    const Nezumi::TileCoord max = Nezumi::TileCoord::from_position({
        std::min(bounds.max.x, world.max.x) - 0.001f,
        std::min(bounds.max.y, world.max.y) - 0.001f,
    });

    // Where world +x and +y land in view space
    const glm::vec2 ax = ctx.proj.dir_to_view({ 1.0f, 0.0f });
    const glm::vec2 by = ctx.proj.dir_to_view({ 0.0f, 1.0f });

    Nezumi::Render::DrawParams params = world_layer(DrawLayer::Ground);
    params.material = config_.atlas->material();

    Nezumi::TileCoord tile;
    for (tile.y = min.y; tile.y <= max.y; tile.y++) {
        int32_t first = min.x;
        int32_t last  = max.x;
        if (!overlap_columns(ax.x, by.x, view.min.x, view.max.x, tile.y, first, last)) continue;
        if (!overlap_columns(ax.y, by.y, view.min.y, view.max.y, tile.y, first, last)) continue;

        for (tile.x = first; tile.x <= last; tile.x++) {
            ctx.renderer.draw_quad(
                Nezumi::Render::Quad{
                    .corners = ctx.proj.box_to_view(tile.bounds().to_vec4()),
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
    // length, projected (straight lines stay straight). Skipped when tiles
    // are too small on screen to read: a tile is tile_size().y view units
    // tall in either projection.
    const float tile_pixels = ctx.proj.tile_size().y / ctx.pixel_size;
    if (tile_pixels >= config_.min_tile_grid_pixels) {
        const float width = config_.tile_line_pixels * ctx.pixel_size;

        for (uint32_t i = 1; i < Nezumi::CHUNK_TILE_COUNT.x; i++) {
            const float x = bounds.min.x + static_cast<float>(i);
            ctx.renderer.draw_line(
                Nezumi::Render::Line{
                    .position_a = ctx.to_view({ x, bounds.min.y }),
                    .position_b = ctx.to_view({ x, bounds.max.y }),
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
                    .position_a = ctx.to_view({ bounds.min.x, y }),
                    .position_b = ctx.to_view({ bounds.max.x, y }),
                    .color      = config_.grid_color,
                    .width      = width,
                },
                params
            );
        }
    }

    // Chunk edges: thicker than the tile lines, and drawn at every zoom.
    // Neighbors both draw their shared edge; it's 4 lines per chunk, and
    // drawing all 4 keeps the edge of the map closed.
    draw_outline(ctx, bounds, config_.grid_color, config_.chunk_line_pixels,
                 DrawLayer::GroundOverlay);
}

} // namespace Nightmourn
