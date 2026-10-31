// =============================================================================
// ChunkState.hpp
// =============================================================================

#pragma once
#include <cstdint> // uint8_t

namespace Nightmourn {

enum class ChunkState : uint8_t {
    Null,     // Chunk not initialized
    Inactive, // Chunk not ticking (no active entities)
    Active,   // Chunk ticking (has at least one active entity)
};

} // namespace Nightmourn
