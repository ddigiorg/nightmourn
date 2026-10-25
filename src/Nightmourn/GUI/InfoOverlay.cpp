// =============================================================================
// GUI/InfoOverlay.cpp
// =============================================================================

#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/Render/View/Camera.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Input/Picking.hpp"
#include <cmath>  // lround
#include <cstdio> // snprintf
#include <string> // string

namespace Nightmourn {

// Same margin and look as the demo HUD it sits above
static constexpr float MARGIN = 8.0f; // screen units from the corner

static constexpr Nezumi::Render::TextStyle STYLE = {
    .size          = 16.0f,
    .color         = { 1.0f, 1.0f, 1.0f, 1.0f },
    .outline_color = { 0.0f, 0.0f, 0.0f, 1.0f },
    .outline_width = 3.0f,
    .align         = Nezumi::Render::TextAlign::Left,
    .valign        = Nezumi::Render::TextVAlign::Top,
};

static constexpr Nezumi::Render::DrawParams PARAMS = {
    .view  = Nezumi::Render::View::Screen,
    .layer = DrawLayer::Hud,
};

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------

void InfoOverlay::update(Nezumi::Engine& engine) {
    const Nezumi::Engine::LoopStats stats = engine.get_stats();

    if (!started_) {
        started_    = true;
        tick_count_ = stats.tick_count;
        draw_count_ = stats.draw_count;
        window_.start();
        return;
    }

    // draw_ms is the previous draw's: this one is still running
    tick_ms_ += stats.tick_ms;
    draw_ms_ += stats.draw_ms;
    samples_++;

    const float elapsed = window_.get_s();
    if (elapsed < WINDOW_S) return;

    shown_ = {
        .tick_ms = tick_ms_ / static_cast<float>(samples_),
        .draw_ms = draw_ms_ / static_cast<float>(samples_),
        .tps     = static_cast<float>(stats.tick_count - tick_count_) / elapsed,
        .dps     = static_cast<float>(stats.draw_count - draw_count_) / elapsed,
    };

    tick_count_ = stats.tick_count;
    draw_count_ = stats.draw_count;
    tick_ms_    = 0.0f;
    draw_ms_    = 0.0f;
    samples_    = 0;
    window_.start();
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

float InfoOverlay::draw(Nezumi::Engine&                   engine,
                        const Nezumi::Render::Camera&     camera,
                        const Nezumi::Render::Projection& proj,
                        std::optional<glm::vec2>          cursor) const
{
    Nezumi::Render::Renderer* r      = engine.renderer();
    Nezumi::Render::Camera*   screen = r->get_view_camera(Nezumi::Render::View::Screen);
    const Nezumi::Render::Font* font = engine.ui()->font();
    if (!screen || !font) return 0.0f;

    std::string text;
    char line[128];

    // Values start in the same column on every line (the font is monospaced)
    text += "performance\n";
    std::snprintf(line, sizeof line, "  tick   %7.2f ms  %5.0f tps\n", shown_.tick_ms, shown_.tps);
    text += line;
    std::snprintf(line, sizeof line, "  draw   %7.2f ms  %5.0f dps\n", shown_.draw_ms, shown_.dps);
    text += line;

    text += "cursor\n";
    if (cursor) {
        const glm::vec2 world = screen_to_world(camera, proj, *cursor);
        std::snprintf(line, sizeof line, "  world  %7.2f, %.2f tiles\n", world.x, world.y);
        text += line;
        std::snprintf(line, sizeof line, "  screen %7.0f, %.0f px\n", cursor->x, cursor->y);
        text += line;
    } else {
        text += "  world        --\n";
        text += "  screen       --\n";
    }

    // The screen camera is sized to the backbuffer (real pixels) and draws
    // in points: get_scale() pixels each, the display scale
    const glm::vec2 resolution    = screen->get_size();
    const float     display_scale = screen->get_scale();

    text += "view\n";
    std::snprintf(line, sizeof line, "  size   %7ld x %ld px\n",
        std::lround(resolution.x), std::lround(resolution.y));
    text += line;
    if (display_scale != 1.0f) {
        std::snprintf(line, sizeof line, "  scale  %7.2fx\n", display_scale);
        text += line;
    }
    std::snprintf(line, sizeof line, "  zoom   %7.2fx", camera.get_zoom());
    text += line;

    const glm::vec2 position = { MARGIN, resolution.y / display_scale - MARGIN };
    r->draw_text(text, position, *font, STYLE, PARAMS);

    return Nezumi::Render::measure_text(*font, text, position, STYLE).y;
}

} // namespace Nightmourn
