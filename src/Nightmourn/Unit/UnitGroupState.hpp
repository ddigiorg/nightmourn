// =============================================================================
// UnitGroupState.hpp
//
// What a UnitGroup's formation is doing. Chosen by the group every tick and
// read by its members (through UnitGroup::Plan) and by renderers.
// =============================================================================

#pragma once
#include <cstdint> // uint8_t

namespace Nightmourn {

enum class UnitGroupState : std::uint8_t {
    Formed,    // members hold their slots; the anchor marches or stands
    Engaged,   // formation broken: members pick and fight their own targets
    Reforming, // anchor waits while members walk back into their slots
};

} // namespace Nightmourn
