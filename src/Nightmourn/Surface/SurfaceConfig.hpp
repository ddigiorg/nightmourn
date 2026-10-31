// =============================================================================
// SurfaceConfig.hpp
//
// Config of a surface.
// =============================================================================

#pragma once
#include "Nightmourn/Chunk/ChunkSpan.hpp"
#include <string> // string

namespace Nightmourn {

struct SurfaceConfig {
    std::string name           = {};
    ChunkSpan   initial_chunks = {}; // optional: chunks created up front
};

} // namespace Nightmourn
