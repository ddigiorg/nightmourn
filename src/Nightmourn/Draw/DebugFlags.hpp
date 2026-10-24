// =============================================================================
// Draw/DebugFlags.hpp
//
// Visual debug options, like Factorio's F4 menu (show-tile-grid,
// show-collision-rectangles, ...). Each flag turns on one overlay in the
// drawers; none of them changes the simulation.
//
// The game owns one DebugFlags and hands it to the drawers through the
// DrawContext, so there is no global state for a future draw thread to race
// on. Flags change between frames (see toggle_debug_flags in Input/Actions).
//
// To add a flag: add it to DebugFlag above Count and give it a name in
// DebugFlags.cpp. It gets the next F key automatically.
// =============================================================================

#pragma once
#include <cstddef>     // size_t
#include <cstdint>     // uint8_t, uint32_t
#include <string_view> // string_view

namespace Nightmourn {

enum class DebugFlag : std::uint8_t {
    ShowTileGrid,            // 1 px lines between tiles (and chunk edges)
    ShowChunkBorders,        // 3 px lines on chunk edges
    ShowCollisionRectangles, // collision box of every collidable entity
    ShowFacingArrows,        // arrow on units and characters, the way they face
    ShowUnitBehaviorInfo,    // unit state dot, line to its target
    ShowUnitGroupInfo,       // group bounds, route, slots and stragglers

    Count
};

inline constexpr std::size_t DEBUG_FLAG_COUNT = static_cast<std::size_t>(DebugFlag::Count);

class DebugFlags {
public:

    // -------------------------------------------------------------------------
    // Flags
    // -------------------------------------------------------------------------

    bool is_on(DebugFlag flag) const { return (bits_ & bit_(flag)) != 0; }
    bool any()                 const { return bits_ != 0; }

    void set(DebugFlag flag, bool on) {
        if (on) bits_ |=  bit_(flag);
        else    bits_ &= ~bit_(flag);
    }

    void toggle(DebugFlag flag) { bits_ ^= bit_(flag); }
    void clear ()               { bits_ = 0;           }

    // -------------------------------------------------------------------------
    // Names
    // -------------------------------------------------------------------------

    // Factorio-style kebab-case name, e.g. "show-tile-grid"
    static std::string_view name(DebugFlag flag);

private:
    static_assert(DEBUG_FLAG_COUNT <= 32, "DebugFlags holds 32 flags");

    static constexpr std::uint32_t bit_(DebugFlag flag) {
        return 1u << static_cast<std::uint32_t>(flag);
    }

    std::uint32_t bits_ = 0;
};

} // namespace Nightmourn
