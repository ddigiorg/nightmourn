// =============================================================================
// Draw/ChunkDrawer.hpp
//
// Draws a chunk's ground: its tiles, and the world grid when show-world-grid
// is on (Factorio's tile grid look, with chunk borders).
//
// The grid costs 31 + 31 lines per chunk, not one outline per tile: each
// interior line spans the whole chunk. Chunk edges are 4 more, thicker lines.
// Line widths are in screen pixels, so the grid looks the same at every zoom.
//
// Tiles are a grass checkerboard until surfaces store tile types; then the
// tile type picks the sprite here, and nothing outside this class changes.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include <glm/glm.hpp> // vec4

namespace Nezumi {
    class Chunk;
}

namespace Nezumi::Render {
    class Atlas;
}

namespace Nightmourn {

class ChunkDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        const Nezumi::Render::Atlas* atlas = nullptr;

        // Ground tiles: pixel boxes in the atlas { x0, y0, x1, y1 }, y down
        glm::vec4 grass_dark  = {};
        glm::vec4 grass_light = {};

        // Grid (Factorio's: plain black)
        glm::vec4 grid_color           = { 0.0f, 0.0f, 0.0f, 1.0f };
        float     tile_line_pixels     = 1.0f;
        float     chunk_line_pixels    = 3.0f;

        // Below this many screen pixels per tile the tile lines would cover
        // most of the ground, so only the chunk edges are drawn
        float     min_tile_grid_pixels = 8.0f;
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.atlas` must outlive the drawer
    explicit ChunkDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Nezumi::Chunk& chunk) const;

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    void draw_tiles_(DrawContext& ctx, const Nezumi::Chunk& chunk) const;
    void draw_grid_ (DrawContext& ctx, const Nezumi::Chunk& chunk) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    Config    config_          = {};
    glm::vec4 tex_grass_dark_  = {}; // texture boxes, from config_'s pixel boxes
    glm::vec4 tex_grass_light_ = {};
};

} // namespace Nightmourn
