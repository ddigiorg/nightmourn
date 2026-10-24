// =============================================================================
// Draw/DebugFlags.cpp
// =============================================================================

#include "Nightmourn/Draw/DebugFlags.hpp"

namespace Nightmourn {

std::string_view DebugFlags::name(DebugFlag flag) {
    switch (flag) {
        case DebugFlag::ShowTileGrid:            return "show-tile-grid";
        case DebugFlag::ShowChunkBorders:        return "show-chunk-borders";
        case DebugFlag::ShowCollisionRectangles: return "show-collision-rectangles";
        case DebugFlag::ShowFacingArrows:        return "show-facing-arrows";
        case DebugFlag::ShowUnitBehaviorInfo:    return "show-unit-behavior-info";
        case DebugFlag::ShowUnitGroupInfo:       return "show-unit-group-info";
        case DebugFlag::Count:                   break;
    }
    return "unknown";
}

} // namespace Nightmourn
