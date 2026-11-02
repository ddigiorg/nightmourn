// =============================================================================
// Lighting/GameLighting.cpp
// =============================================================================

#include "Nightmourn/Lighting/GameLighting.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nezumi/Render/Core/RenderCore.hpp"

namespace Nightmourn {

namespace R = Nezumi::Render;

GameLighting::GameLighting(R::RenderCore& core, R::Renderer2D& renderer)
    : core_    (core)
    , renderer_(renderer)
{
    // The world at full size; the light at half (it is soft, and many large
    // lights overlap at night)
    const R::TargetID scene = core.create_target({ .name = "nightmourn-scene" });
    const R::TargetID light = core.create_target({ .name = "nightmourn-light", .scale = 0.5f });

    post_ = std::make_unique<R::PostProcess>(core, R::PostProcessConfig{
        .scene = scene,
        .light = light,
    });

    R::PostSettings& s = post_->settings();
    s.bloom_threshold = 1.0f;
    s.bloom_intensity = 0.9f;
    s.tonemap         = R::Tonemap::Shoulder;

    // Overlays draw through the world camera, after the lighting
    renderer.set_view(OVERLAY_VIEW, renderer.get_view(R::View::World));

    using R::view_bit;
    R::FrameRecipe recipe;
    recipe.passes.push_back({ .name = "scene", .target = scene,
                              .clear_color = { 0.0f, 0.0f, 0.0f, 1.0f },
                              .features = { { &renderer, view_bit(R::View::World) } } });
    recipe.passes.push_back({ .name = "lights", .target = light,
                              .clear_color = { 0.0f, 0.0f, 0.0f, 0.0f },
                              .features = { { &renderer, view_bit(R::View::Lights) } } });
    recipe.passes.push_back(post_->composite_pass());
    recipe.passes.push_back({ .name = "emissive", .target = post_->lit_target(), .clear = false,
                              .features = { { &renderer, view_bit(R::View::Emissive) } } });
    for (R::PassConfig& pass : post_->output_passes()) recipe.passes.push_back(std::move(pass));
    recipe.passes.push_back({ .name = "overlay", .clear = false,
                              .features = { { &renderer, view_bit(OVERLAY_VIEW) } } });
    recipe.passes.push_back({ .name = "ui", .clear = false,
                              .features = { { &renderer, view_bit(R::View::Screen) } } });
    core.set_recipe(std::move(recipe));
}

GameLighting::~GameLighting() {
    // Back to one unlit pass, as the Engine set it up, so nothing reads the
    // targets PostProcess leaves behind
    core_.set_recipe({ .passes = { { .name = "main",
                                     .features = { { .feature = &renderer_ } } } } });
    post_.reset();
    renderer_.clear_view(OVERLAY_VIEW);
}

void GameLighting::update(glm::vec3 ambient, const DebugFlags& debug) {
    R::PostSettings& s = post_->settings();
    s.ambient  = ambient;
    s.lighting = !debug.is_on(DebugFlag::DisableLighting);
    s.debug    = debug.is_on(DebugFlag::ShowLightMap) ? R::PostDebugView::Light
               : debug.is_on(DebugFlag::ShowBloom)    ? R::PostDebugView::Bloom
                                                      : R::PostDebugView::None;
}

} // namespace Nightmourn
