// =============================================================================
// Lighting/LightSource.hpp
//
// One light in the world, as the game describes it: where it stands (world
// tiles), how big and bright it is, whether it flickers, and an optional
// glow (the flame or spell core itself, drawn unlit so darkness never dims
// it). LightDrawer turns these into engine PointLights each frame.
//
// Colors are LINEAR and intensity may exceed 1; over about 1 in the lit image
// it blooms. The tuned looks (torch, campfire, lantern, wisps) are "light"
// configs in data/base/configs/lights.json (Data/Configs.hpp): copy one and
// set its position and seed.
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

} // namespace Nightmourn
