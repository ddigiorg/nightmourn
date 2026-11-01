// =============================================================================
// Draw/DebugFlags.cpp
// =============================================================================

#include "Nightmourn/Draw/DebugFlags.hpp"

namespace Nightmourn {

std::string_view DebugFlags::name(DebugFlag flag) {
    switch (flag) {
        case DebugFlag::ShowInfo:                return "show-info";
        case DebugFlag::ShowWorldGrid:           return "show-world-grid";
        case DebugFlag::ShowCollisionRectangles: return "show-collision-rectangles";
        case DebugFlag::ShowFacingArrows:        return "show-facing-arrows";
        case DebugFlag::ShowUnitBehaviorInfo:    return "show-unit-behavior-info";
        case DebugFlag::ShowUnitGroupInfo:       return "show-unit-group-info";
        case DebugFlag::ShowPheromones:          return "show-pheromones";
        case DebugFlag::UseTopDownView:          return "use-top-down-view";
        case DebugFlag::Count:                   break;
    }
    return "unknown";
}

} // namespace Nightmourn
