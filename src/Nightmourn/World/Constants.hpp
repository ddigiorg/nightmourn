// =============================================================================
// World/Constants.hpp
//
// World units are TILE-SPACE: 1.0 is one tile. A position's integer part
// (floor) is the tile it is in and the fraction is how far into that tile:
//
//   x =  3.25  ->  tile  3, 0.25 of the way across
//   x = -0.25  ->  tile -1, 0.75 of the way across (floor, not truncation)
//
// Tile n covers [n, n + 1), so its center is n + 0.5. See TileCoord.hpp.
//
// The world knows nothing about pixels. How big a tile looks on screen is the
// camera's job (CameraConfig::pixels_per_unit and zoom), and how big a tile's
// sprite is in its texture is the atlas's job (Atlas::tex_box).
// =============================================================================

#pragma once
#include <cstdint>     // int16_t, int32_t
#include <glm/glm.hpp> // vec2, uvec2

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Chunk
// -----------------------------------------------------------------------------

inline constexpr int16_t CHUNK_MIN_X = -31250;
inline constexpr int16_t CHUNK_MIN_Y = -31250;
inline constexpr int16_t CHUNK_MAX_X =  31249; // 62,500 total columns
inline constexpr int16_t CHUNK_MAX_Y =  31249; // 62,500 total rows

inline constexpr glm::uvec2 CHUNK_TILE_COUNT = {
    /*.x = */32,
    /*.y = */32,
};

inline constexpr glm::vec2 CHUNK_SIZE = { // tiles
    /*.x = */static_cast<float>(CHUNK_TILE_COUNT.x),
    /*.y = */static_cast<float>(CHUNK_TILE_COUNT.y),
};

// -----------------------------------------------------------------------------
// Sector
// -----------------------------------------------------------------------------

inline constexpr int32_t SECTOR_MIN_X = -500000;
inline constexpr int32_t SECTOR_MIN_Y = -500000;
inline constexpr int32_t SECTOR_MAX_X =  499999; // 1,000,000 total columns
inline constexpr int32_t SECTOR_MAX_Y =  499999; // 1,000,000 total rows

inline constexpr glm::uvec2 SECTOR_TILE_COUNT = {
    /*.x = */2,
    /*.y = */2,
};

inline constexpr glm::vec2 SECTOR_SIZE = { // tiles
    /*.x = */static_cast<float>(SECTOR_TILE_COUNT.x),
    /*.y = */static_cast<float>(SECTOR_TILE_COUNT.y),
};

} // namespace Nightmourn
