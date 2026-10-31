// =============================================================================
// Draw/ChunkDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/ChunkDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/World/Constants.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/Geometry/TileCoord.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <algorithm> // max, min
#include <cassert>   // assert
#include <cmath>     // ceil, floor
#include <cstdint>   // int32_t
#include <utility>   // swap

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

ChunkDrawer::ChunkDrawer(const Config& config)
    : config_(config)
{
    assert(config_.atlas      && "ChunkDrawer: config.atlas is null");
    assert(config_.grass_tiles && "ChunkDrawer: config.grass_tiles must be > 0");

    // Cut the grass texture into its cells, row-major with cell (0, 0) at the
    // texture's bottom-left: world y goes up, image y goes down. Neighboring
    // cells are neighbors in the texture too, so linear filtering across a
    // tile edge samples the right pixels. Only the texture's outer edges are
    // inset by half a texel, so filtering never reaches past them into the
    // next sprite in the atlas (the wrap there is invisible on seamless art).
    const std::uint32_t n    = config_.grass_tiles;
    const float         cell = (config_.grass.z - config_.grass.x) / static_cast<float>(n);

    grass_cells_.resize(static_cast<std::size_t>(n) * n);
    for (std::uint32_t cy = 0; cy < n; cy++) {
        for (std::uint32_t cx = 0; cx < n; cx++) {
            const float x0 = config_.grass.x + cell * static_cast<float>(cx);
            const float y0 = config_.grass.y + cell * static_cast<float>(n - 1 - cy);

            glm::vec4 pixels = { x0, y0, x0 + cell, y0 + cell };
            if (cx == 0)     pixels.x += 0.5f;
            if (cx == n - 1) pixels.z -= 0.5f;
            if (cy == n - 1) pixels.y += 0.5f; // top row of the image
            if (cy == 0)     pixels.w -= 0.5f; // bottom row of the image

            grass_cells_[cy * n + cx] = config_.atlas->tex_box(pixels);
        }
    }
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

glm::vec4 ChunkDrawer::grass_cell_(std::int32_t x, std::int32_t y) const {
    // Floor modulo, so negative tiles continue the pattern instead of
    // mirroring it
    const std::int32_t n  = static_cast<std::int32_t>(config_.grass_tiles);
    const std::int32_t cx = ((x % n) + n) % n;
    const std::int32_t cy = ((y % n) + n) % n;
    return grass_cells_[static_cast<std::size_t>(cy * n + cx)];
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void ChunkDrawer::draw(DrawContext& ctx, const Chunk& chunk) const {
    draw_tiles_(ctx, chunk);

    if (ctx.show(DebugFlag::ShowWorldGrid))
        draw_grid_(ctx, chunk);

    if (ctx.show(DebugFlag::ShowPheromones))
        draw_pheromones_(ctx, chunk);
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
void ChunkDrawer::draw_tiles_(DrawContext& ctx, const Chunk& chunk) const {
    const Area2D& bounds = chunk.bounds();
    const Area2D& world  = ctx.world_view;
    const Area2D& view   = ctx.view;

    const TileCoord min = TileCoord::from_position({
        std::max(bounds.min.x, world.min.x),
        std::max(bounds.min.y, world.min.y),
    });
    const TileCoord max = TileCoord::from_position({
        std::min(bounds.max.x, world.max.x) - 0.001f,
        std::min(bounds.max.y, world.max.y) - 0.001f,
    });

    // Where world +x and +y land in view space
    const glm::vec2 ax = ctx.proj.dir_to_view({ 1.0f, 0.0f });
    const glm::vec2 by = ctx.proj.dir_to_view({ 0.0f, 1.0f });

    Nezumi::Render::DrawParams params = world_layer(DrawLayer::Ground);
    params.material = config_.atlas->material();

    TileCoord tile;
    for (tile.y = min.y; tile.y <= max.y; tile.y++) {
        int32_t first = min.x;
        int32_t last  = max.x;
        if (!overlap_columns(ax.x, by.x, view.min.x, view.max.x, tile.y, first, last)) continue;
        if (!overlap_columns(ax.y, by.y, view.min.y, view.max.y, tile.y, first, last)) continue;

        for (tile.x = first; tile.x <= last; tile.x++) {
            ctx.renderer.draw_quad(
                Nezumi::Render::Quad{
                    .corners = ctx.proj.box_to_view(tile.bounds().to_vec4()),
                    .tex_box = grass_cell_(tile.x, tile.y),
                },
                params
            );
        }
    }
}

void ChunkDrawer::draw_grid_(DrawContext& ctx, const Chunk& chunk) const {
    const Area2D& bounds = chunk.bounds();
    const Nezumi::Render::DrawParams params = world_layer(DrawLayer::GroundOverlay);

    // Tile lines: the 31 interior lines on each axis, each the chunk's full
    // length, projected (straight lines stay straight). Skipped when tiles
    // are too small on screen to read: a tile is tile_size().y view units
    // tall in either projection.
    const float tile_pixels = ctx.proj.tile_size().y / ctx.pixel_size;
    if (tile_pixels >= config_.min_tile_grid_pixels) {
        const float width = config_.tile_line_pixels * ctx.pixel_size;

        for (uint32_t i = 1; i < CHUNK_TILE_COUNT.x; i++) {
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

        for (uint32_t i = 1; i < CHUNK_TILE_COUNT.y; i++) {
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

void ChunkDrawer::draw_pheromones_(DrawContext& ctx, const Chunk& chunk) const {
    const PheromoneEpoch  now   = ctx.world.pheromone_epoch();
    const PheromoneDecay& decay = ctx.world.pheromone_decay();

    const Area2D&   bounds = chunk.bounds();
    const glm::vec2 center = (bounds.min.to_vec2() + bounds.max.to_vec2()) * 0.5f;
    const float     reach  = CHUNK_SIZE.x * 0.5f;

    for (FactionIndex f = 0; f < MAX_FACTIONS; f++) {
        const PheromoneValue value = chunk.pheromone(f, now, decay);
        if (value == 0) continue;

        const glm::vec4 color  = config_.faction_colors[f].a > 0.0f ? config_.faction_colors[f]
                                                                    : glm::vec4(1.0f); // unset: white
        const float     radius = reach * static_cast<float>(value) / static_cast<float>(PHEROMONE_MAX);
        draw_ground_circle(ctx, center, std::max(radius, 0.25f), color,
                           config_.pheromone_line_pixels, DrawLayer::GroundOverlay);

        const int best = chunk.strongest_neighbour(f, now, decay);
        if (best < 0) continue;
        const std::size_t i = static_cast<std::size_t>(best);
        if (chunk.neighbour(i)->pheromone(f, now, decay) <= value) continue;

        const ChunkOffset o = NEIGHBOUR_OFFSETS[i];
        draw_facing_arrow(ctx, center, Position2D{ static_cast<float>(o.x), static_cast<float>(o.y) }, color);
    }
}

} // namespace Nightmourn
