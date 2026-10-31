// =============================================================================
// UnitGroupConfig.hpp
//
// Shared, data-driven settings for one kind of UnitGroup. Groups keep a
// pointer to their config, so a config must outlive every group built from it
// (static storage or a registry owned by the game), like UnitConfig.
//
// Distances are in tiles, rates per second, and durations in ticks.
// =============================================================================

#pragma once
#include <cstdint>     // uint8_t, uint16_t
#include <string_view> // string_view

namespace Nightmourn {

enum class UnitFormation : std::uint8_t {
    Block, // rectangle of rows, filled front to back; a short last row is centered
};

struct UnitGroupConfig {
    std::string_view name = {};

    // -------------------------------------------------------------------------
    // Formation layout
    // -------------------------------------------------------------------------

    UnitFormation formation = UnitFormation::Block;

    float    spacing = 1.0f; // tiles between neighboring slots
    uint16_t columns = 0;    // slots per row (0: from `aspect`)
    float    aspect  = 2.0f; // width / depth when columns is 0

    // Short-ranged members take the front rows, long-ranged ones the back
    bool ranged_in_back = true;

    uint16_t max_members = 512;

    // -------------------------------------------------------------------------
    // Marching
    // -------------------------------------------------------------------------

    // The anchor marches at the slowest member's move_speed times this. The
    // headroom below 1.0 is what lets members that fell behind catch up.
    float march_speed_factor = 0.85f;

    // How hard a member steers back toward its slot (1/s). Its velocity is
    // the group's plus `catch_up_gain * (slot - position)`, capped at its own
    // move_speed, so the error decays with a time constant of 1/gain.
    float catch_up_gain = 4.0f;

    // A member farther than this from its slot counts as a straggler
    float straggler_distance = 1.5f;

    // Straggler slowdown: the march slows as the members' lag (RMS distance
    // from their slots, see UnitGroup::lag) grows from `slowdown_lag` to
    // `stop_lag`, down to `min_speed_factor`. Never all the way to zero, so
    // one stuck member can't freeze its group.
    float slowdown_lag     = 0.25f;
    float stop_lag         = 2.0f;
    float min_speed_factor = 0.2f;

    // -------------------------------------------------------------------------
    // Combat
    // -------------------------------------------------------------------------

    // The group looks for enemies around its members every N ticks (staggered
    // by handle index). One search per group replaces one per member.
    uint16_t scan_interval = 15;

    // Engaged groups reform after this long without seeing an enemy
    uint16_t disengage_ticks = 90;

    // A reforming group marches on once its lag drops below `reform_lag`,
    // or after `reform_timeout` ticks
    float    reform_lag     = 0.3f;
    uint16_t reform_timeout = 300;
};

} // namespace Nightmourn
