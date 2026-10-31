// =============================================================================
// GUI/InfoOverlay.hpp
//
// The show-info debug overlay: engine timings, the cursor and the view, as
// white outlined text in the top-left corner of the screen.
//
//   performance
//     tick     0.42 ms     60 tps
//     draw     3.10 ms     60 dps
//   cursor
//     world   -12.34, 5.67 tiles
//     screen     640, 360 px
//   view
//     size    1280 x 720 px
//     zoom    0.75x
//
// Timings and rates are averaged over half a second, so the numbers are
// readable instead of flickering every frame. The font is monospaced, so the
// columns line up.
//
//   on_draw:  info_overlay_.update(engine);
//             if (debug_flags_.is_on(DebugFlag::ShowInfo))
//                 hud_top = info_overlay_.draw(engine, *world_camera_, proj, cursor);
// =============================================================================

#pragma once
#include "Nezumi/Core/Stopwatch.hpp"
#include "Nezumi/World/Projection.hpp"
#include <cstdint>     // uint64_t
#include <glm/glm.hpp> // vec2
#include <optional>    // optional

namespace Nezumi {
    class Engine;
}

namespace Nezumi::Render {
    class Camera2D;
}

namespace Nightmourn {

class InfoOverlay {
public:

    // -------------------------------------------------------------------------
    // Update
    //
    // Call once per frame from on_draw, shown or not, so the numbers are
    // already current when the overlay is switched on.
    // -------------------------------------------------------------------------

    void update(Nezumi::Engine& engine);

    // -------------------------------------------------------------------------
    // Draw
    //
    // Call from on_draw. `camera` and `proj` are the ones the world is drawn
    // with; `cursor` is the mouse in window pixels (y down), or nullopt
    // before it has moved. Returns the screen-space y of the overlay's
    // bottom edge (y up), for HUD text that goes below it.
    // -------------------------------------------------------------------------

    float draw(Nezumi::Engine&                   engine,
               const Nezumi::Render::Camera2D&   camera,
               const Nezumi::Projection&         proj,
               std::optional<glm::vec2>          cursor) const;

private:

    // Averaging window
    static constexpr float WINDOW_S = 0.5f;

    // Shown values, refreshed once per window
    struct Averages {
        float tick_ms = 0.0f;
        float draw_ms = 0.0f;
        float tps     = 0.0f; // ticks per second
        float dps     = 0.0f; // draws per second
    };

    Averages shown_ = {};

    // Current window
    Nezumi::Stopwatch window_     = {};
    std::uint64_t     tick_count_ = 0;   // engine counts at the window's start
    std::uint64_t     draw_count_ = 0;
    float             tick_ms_    = 0.0f; // sums of per-frame samples
    float             draw_ms_    = 0.0f;
    std::uint32_t     samples_    = 0;
    bool              started_    = false;
};

} // namespace Nightmourn
