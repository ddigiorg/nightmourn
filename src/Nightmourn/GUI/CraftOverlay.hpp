// =============================================================================
// GUI/CraftOverlay.hpp
//
// An icon over each crafting building, Factorio's "alt mode"
// (crafting-design/DESIGN.md §6.3): what its job makes, shaded for the time
// it has left, and red when it's done but the settlement has no room for
// it. Working and blocked machines always show one; holding Alt
// (Action::ShowCraftIcons) shows the idle ones too, as an empty socket.
//
//   craft_overlay_.draw(ui, world, s->crafting, world.get_tick(), camera, proj, show_all);
//
// Drawn in the UI pass from the site's dense machine vector, so it costs
// nothing for buildings that don't craft. One see-through panel the size of
// the screen that takes no clicks, drawn before the menus so they cover it.
// =============================================================================

#pragma once
#include "Nightmourn/Geometry/Projection.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nezumi/Core/Tick.hpp"
#include <array>       // array
#include <glm/glm.hpp> // vec2

namespace Nezumi::Render {
    class Camera2D;
}

namespace Nezumi::UI {
    class Context;
}

namespace Nightmourn {

class CraftSite;
class World;

class CraftOverlay {
public:
    static constexpr glm::vec2 ICON = { 28.0f, 28.0f };

    // `sheet` must outlive it
    explicit CraftOverlay(const UISheet& sheet) : sheet_(&sheet) {}

    // Call from on_draw, before the menus. `camera` and `proj` are the
    // world's, as it was drawn this frame.
    void draw(Nezumi::UI::Context& ui, const World& world, const CraftSite& site, Nezumi::Tick now,
              const Nezumi::Render::Camera2D& camera, const Projection& proj, bool show_all);

private:
    const UISheet* sheet_ = nullptr;
};

} // namespace Nightmourn
