// =============================================================================
// Draw/WorldSheets.hpp
//
// The sprite sheets the world drawers draw from, built once at startup from
// the configs (Data/Configs.hpp) and their PNGs in data/:
//
//   sprites  every figure animation, as "figure/<name>/<animation>"
//            ("figure/red/idle"), and every tree's pictures, as the
//            animation "tree/<name>" (one frame per look), packed at load
//            (Nezumi's SpritePacker) with mips
//   ground   one page per tile config's texture, sampled with Repeat so
//            each wraps onto itself
//
//   const Configs  protos = Configs::load();
//   const WorldSheets sheets = WorldSheets::load(*engine.renderer2d(), protos);
//   WorldDrawer::Config config = sheets.drawer_config();
// =============================================================================

#pragma once
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include <cstdint>     // uint32_t
#include <string>      // string
#include <string_view> // string_view
#include <utility>     // pair
#include <vector>      // vector

namespace Nezumi::Render {
    class Renderer2D;
    class SpriteSheet;
}

namespace Nightmourn {

class Configs;

struct WorldSheets {

    // A tile config's texture in `ground`
    struct GroundTexture {
        std::uint32_t layer = 0; // page
        std::uint32_t tiles = 1; // covers tiles x tiles
    };

    const Nezumi::Render::SpriteSheet* sprites = nullptr;
    const Nezumi::Render::SpriteSheet* ground  = nullptr;

    std::vector<std::pair<std::string, GroundTexture>> ground_textures; // by tile name

    // Throws if there's no tile config by that name
    const GroundTexture& ground_texture(std::string_view tile) const;

    // A drawer config using these sheets, with the default figure and tree
    // animation names and the "grass" tile's texture; change what differs
    WorldDrawer::Config drawer_config() const;

    // Creates both through the renderer, which owns them. Throws if a PNG
    // can't be loaded or doesn't match its config, or if the ground
    // textures differ in size.
    static WorldSheets load(Nezumi::Render::Renderer2D& renderer, const Configs& configs);
};

} // namespace Nightmourn
