// =============================================================================
// Lighting/GameLighting.hpp
//
// Turns the engine's Renderer2D into Nightmourn's lit pipeline: makes the
// scene and light targets and a PostProcess, and replaces the frame recipe:
//
//   scene     View::World           -> scene target (the art as drawn)
//   lights    View::Lights          -> light target (half size, linear)
//   composite scene x (ambient + light)  -> lit target (linear HDR)
//   emissive  View::Emissive        -> lit target (flames, spell cores)
//   bloom + tonemap                 -> backbuffer
//   overlay   GameView::Overlay     -> backbuffer (healthbars, outlines,
//                                      debug shapes: unlit, readable at night)
//   ui        View::Screen          -> backbuffer
//
// Each frame, update() takes the ambient (DayNight) and the debug flags
// (show-light-map, show-bloom, disable-lighting). Drawers that make a
// DrawContext set ctx.overlay_view = GameLighting::OVERLAY_VIEW.
//
// Construct after the world camera is set up, keep it alive while the game
// draws, and destroy it before the engine.
// =============================================================================

#pragma once
#include "Nezumi/Render/Core/Post/PostProcess.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include <glm/glm.hpp> // vec3
#include <memory>      // unique_ptr

namespace Nezumi::Render {
    class RenderCore;
    class Renderer2D;
}

namespace Nightmourn {

class GameLighting {
public:

    static constexpr Nezumi::Render::ViewID OVERLAY_VIEW = GameView::Overlay;

    GameLighting(Nezumi::Render::RenderCore& core, Nezumi::Render::Renderer2D& renderer);
    ~GameLighting();

    GameLighting(const GameLighting&)            = delete;
    GameLighting& operator=(const GameLighting&) = delete;

    // Once per frame, before drawing
    void update(glm::vec3 ambient, const DebugFlags& debug);

    // Bloom, exposure, tonemap and the rest, for tuning
    Nezumi::Render::PostSettings& settings() { return post_->settings(); }

private:
    Nezumi::Render::RenderCore&                  core_;
    Nezumi::Render::Renderer2D&                  renderer_;
    std::unique_ptr<Nezumi::Render::PostProcess> post_;
};

} // namespace Nightmourn
