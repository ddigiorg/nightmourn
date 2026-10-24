// =============================================================================
// Draw/UnitGroupDrawer.hpp
//
// Draws Nezumi UnitGroups. Groups have no sprite (they are formations, not
// figures), so for now this is only the ShowUnitGroupInfo overlay:
//
//   - bounds around the members, colored by UnitGroupState
//   - the anchor and the formation's facing
//   - the remaining route (MoveTo, AttackMove)
//   - while Engaged: a line to the enemy the group saw
//   - otherwise: every member's slot, and a line from each straggler to its
//
// It reads the plan for World::get_tick(), the one members steer toward
// next. Later game-facing visuals (banners, selection, a command marker) go
// here too.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include <array>       // array
#include <glm/glm.hpp> // vec4

namespace Nezumi {
    class UnitGroup;
}

namespace Nightmourn {

class UnitGroupDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        // Route and slot color per faction
        std::array<glm::vec4, Nezumi::MAX_FACTIONS> faction_colors = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit UnitGroupDrawer(const Config& config) : config_(config) {}

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    // How far (tiles) a group's members can stand outside the chunks the
    // camera sees: the anchor is in one chunk, but the formation spreads
    // around it. WorldDrawer visits this much further out for groups.
    static constexpr float VIEW_MARGIN = 16.0f;

    void draw(DrawContext& ctx, const Nezumi::UnitGroup& group) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    Config config_ = {};
};

} // namespace Nightmourn
