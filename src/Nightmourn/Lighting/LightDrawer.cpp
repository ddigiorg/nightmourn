// =============================================================================
// Lighting/LightDrawer.cpp
// =============================================================================

#include "Nightmourn/Lighting/LightDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include <cmath> // floor

namespace Nightmourn {

namespace {
    // Value noise, 0..1, smooth between integers
    float hash(std::uint32_t n) {
        n = n * 374761393u + 668265263u;
        n = (n ^ (n >> 13)) * 1274126177u;
        return static_cast<float>((n ^ (n >> 16)) & 0xFFFFu) / 65535.0f;
    }

    float noise(float x, std::uint32_t seed) {
        const float         f = std::floor(x);
        const std::uint32_t i = static_cast<std::uint32_t>(static_cast<std::int64_t>(f)) + seed * 7919u;
        const float         t = x - f;
        const float         s = t * t * (3.0f - 2.0f * t);
        return hash(i) + (hash(i + 1u) - hash(i)) * s;
    }

    // 1 on average; flicker 1 swings intensity about +/- 20%
    float flicker_at(float seconds, const LightSource& light) {
        if (light.flicker <= 0.0f) return 1.0f;
        const float n = 0.65f * noise(seconds * 7.0f, light.seed)
                      + 0.35f * noise(seconds * 17.0f, light.seed + 101u);
        return 1.0f + light.flicker * 0.4f * (n - 0.5f);
    }
}

void LightDrawer::draw(DrawContext& ctx, std::span<const LightSource> lights, float seconds,
                       float strength) const
{
    for (const LightSource& light : lights) draw(ctx, light, seconds, strength);
}

void LightDrawer::draw(DrawContext& ctx, const LightSource& light, float seconds,
                       float strength) const
{
    using Nezumi::Render::PointLight;
    using Nezumi::Render::View;

    const float f = flicker_at(seconds, light);
    const float r = light.radius * (1.0f + 0.15f * (f - 1.0f)); // the pool breathes a little
    const bool  pool = strength > 0.0f;

    // Cull by the light's projected square
    const glm::vec2 p   = light.position;
    const glm::vec4 box = { p.x - r, p.y - r, p.x + r, p.y + r };
    const glm::vec4 vb  = ctx.proj.box_to_view_bounds(box);
    if (!Area2D(vb.x, vb.y, vb.z, vb.w).overlaps(ctx.view)) return;

    if (pool) ctx.renderer.draw_light(
        PointLight{
            .corners = ctx.proj.box_to_view(box),
            .color   = { light.color, light.intensity * f * strength },
            .falloff = { light.core, light.softness, 0.0f, 0.0f },
        },
        { .view = View::Lights });

    if (light.glow_radius <= 0.0f) return;

    // The glow: upright above the light, screen-aligned
    const float     scale = ctx.upright_scale();
    const glm::vec2 at    = ctx.to_view(p) + glm::vec2(0.0f, light.glow_height * scale);
    ctx.renderer.draw_light(
        PointLight::circle(at, light.glow_radius * scale,
                           { light.color, light.glow_intensity * f },
                           { 2.0f, 0.5f, 0.25f, 0.0f }),
        { .view = View::Emissive });
}

} // namespace Nightmourn
