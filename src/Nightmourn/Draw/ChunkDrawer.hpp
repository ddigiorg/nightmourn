// =============================================================================
// Draw/ChunkDrawer.hpp
//
// Draws a chunk's ground: its tiles, and the world grid when show-world-grid
// is on (Factorio's tile grid look, with chunk borders).
//
// Tiles are square textures drawn as Quads through the projection: squares
// top-down, 2:1 diamonds in iso, from the same art.
//
// Ground textures are world-space, the way Age of Empires II: DE does it: one
// seamless texture covers N x N tiles, and each tile draws the part of it
// that matches its position (tile x, y uses cell x mod N, y mod N). The
// ground repeats every N tiles instead of every tile, so it reads as one
// continuous meadow rather than a checkerboard.
//
// The grid costs 31 + 31 lines per chunk, not one outline per tile: each
// interior line spans the whole chunk. Chunk edges are 4 more, thicker lines.
// Line widths are in screen pixels, so the grid looks the same at every zoom.
//
// Every tile is grass until surfaces store tile types; then the tile type
// picks the texture here, and nothing outside this class changes.
//
// show-pheromones draws, for every faction with pheromone in the chunk, a
// ring around the chunk's center in the faction's color, as wide as the
// strength (a full-strength ring touches the chunk's edges), and an arrow
// toward the stronger neighbour when the gradient points away.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include <array>       // array
#include <cstdint>     // int32_t, uint32_t
#include <glm/glm.hpp> // vec4
#include <vector>      // vector

namespace Nezumi::Render {
    class SpriteSheet;
}

namespace Nightmourn {

class Chunk;

class ChunkDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        // Ground textures, a sheet created with
        // SpriteSheetConfig::address_mode = Repeat so filtering and mips
        // wrap across its edges
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        // The grass texture: a whole page (layer) of the sheet, so Repeat
        // wraps it onto itself, covering `grass_tiles` x `grass_tiles`
        // tiles. Square and seamless. WorldSheets::drawer_config() fills
        // both from the "grass" tile config.
        std::uint32_t grass_layer = 0;
        std::uint32_t grass_tiles = 8;

        // Grid (Factorio's: plain black)
        glm::vec4 grid_color           = { 0.0f, 0.0f, 0.0f, 1.0f };
        float     tile_line_pixels     = 1.0f;
        float     chunk_line_pixels    = 3.0f;

        // Below this many screen pixels per tile the tile lines would cover
        // most of the ground, so only the chunk edges are drawn
        float     min_tile_grid_pixels = 8.0f;

        // Pheromone overlay (show-pheromones): one color per faction; a color
        // left at alpha 0 draws white
        std::array<glm::vec4, MAX_FACTIONS> faction_colors = {};
        float     pheromone_line_pixels = 2.0f;
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer
    explicit ChunkDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Chunk& chunk) const;

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    void draw_tiles_(DrawContext& ctx, const Chunk& chunk) const;
    void draw_grid_ (DrawContext& ctx, const Chunk& chunk) const;
    void draw_pheromones_(DrawContext& ctx, const Chunk& chunk) const;

    // Texture box of the grass cell tile (x, y) draws (see the header comment)
    glm::vec4 grass_cell_(std::int32_t x, std::int32_t y) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    Config                 config_      = {};
    std::vector<glm::vec4> grass_cells_ = {}; // grass_tiles^2 texture boxes, row-major from cell (0, 0)
    std::uint32_t          grass_layer_ = 0;  // the grass sprite's page
};

} // namespace Nightmourn
