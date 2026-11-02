// =============================================================================
// Lighting/LightSource.hpp
//
// One light in the world, as the game describes it: where it stands (world
// tiles), how big and bright it is, whether it flickers, and an optional
// glow (the flame or spell core itself, drawn unlit so darkness never dims
// it). LightDrawer turns these into engine PointLights each frame.
//
// Colors are LINEAR and intensity may exceed 1; over about 1 in the lit image
// it blooms. Presets below are the tuned starting points.
//
// No light lives in the engine: a muzzle flash or spell impact is a
// LightSource drawn for a few frames.
// =============================================================================

#pragma once
#include <cstdint>     // uint32_t
#include <glm/glm.hpp> // vec2, vec3

namespace Nightmourn {

struct LightSource {
    glm::vec2 position  = {};                    // world tiles, on the ground
    glm::vec3 color     = { 1.0f, 1.0f, 1.0f };  // linear
    float     intensity = 1.0f;                  // peak = color * intensity
    float     radius    = 5.0f;                  // tiles: the light is 0 beyond
    float     core      = 8.0f;                  // falloff: how hot the center is
    float     softness  = 0.0f;                  // 0 hot center .. 1 even aura
    float     flicker   = 0.0f;                  // 0 steady .. 1 a guttering torch
    std::uint32_t seed  = 0;                     // decorrelates flicker between lights

    // Glow: an unlit halo at `glow_height` tiles above the position (upright,
    // like a figure), or none when glow_radius is 0
    float     glow_height    = 0.0f;
    float     glow_radius    = 0.0f;   // tiles of upright size
    float     glow_intensity = 10.0f;
};

namespace Lights {

    // Wall or post torch: warm, guttering, a hot flame at head height
    inline LightSource torch(glm::vec2 position, std::uint32_t seed = 0) {
        return {
            .position       = position,
            .color          = { 1.00f, 0.56f, 0.24f },
            .intensity      = 1.8f,
            .radius         = 5.5f,
            .core           = 14.0f,
            .softness       = 0.0f,
            .flicker        = 0.8f,
            .seed           = seed,
            .glow_height    = 1.15f,
            .glow_radius    = 0.12f,
            .glow_intensity = 4.0f,
        };
    }

    // Campfire: bigger, redder, the flame low to the ground
    inline LightSource campfire(glm::vec2 position, std::uint32_t seed = 0) {
        return {
            .position       = position,
            .color          = { 1.00f, 0.45f, 0.18f },
            .intensity      = 2.4f,
            .radius         = 7.5f,
            .core           = 12.0f,
            .softness       = 0.1f,
            .flicker        = 1.0f,
            .seed           = seed,
            .glow_height    = 0.25f,
            .glow_radius    = 0.30f,
            .glow_intensity = 3.5f,
        };
    }

    // What the hero carries: a soft, neutral pool so the player can see.
    // `radius` is the lantern upgrade's reach.
    inline LightSource hero_light(glm::vec2 position, float radius = 4.0f) {
        return {
            .position  = position,
            .color     = { 1.00f, 0.90f, 0.75f },
            .intensity = 1.1f,
            .radius    = radius,
            .core      = 4.0f,
            .softness  = 0.5f,
        };
    }

    // A floating magic light: saturated, bright enough to bloom
    inline LightSource wisp(glm::vec2 position, glm::vec3 color, float height = 1.0f) {
        return {
            .position       = position,
            .color          = color,
            .intensity      = 1.8f,
            .radius         = 4.5f,
            .core           = 6.0f,
            .softness       = 0.4f,
            .glow_height    = height,
            .glow_radius    = 0.14f,
            .glow_intensity = 6.0f,
        };
    }

} // namespace Lights

} // namespace Nightmourn
