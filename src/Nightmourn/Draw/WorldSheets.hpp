// =============================================================================
// Draw/WorldSheets.hpp
//
// The sprite sheets the world drawers draw from, created once at startup:
//
//   sprites  assets/graphics/sprites.json: units, characters and trees,
//            packed from assets-src/sprites by tools/pack_sprites.py
//   ground   assets/graphics/meadow.json: the seamless meadow
//            (tools/gen_meadow.py), sampled with Repeat so it wraps
//
// Both are BC7 KTX2 pages with their mips baked in: about 1.3 MB of VRAM per
// 1024 px page, a quarter of RGBA8. Options::low_res drops each sheet's
// largest level, for a quarter of that again.
//
//   const WorldSheets sheets = WorldSheets::load(*engine.renderer2d());
//   WorldDrawer::Config config = {
//       .chunks     = { .sheet = sheets.ground  },
//       .units      = { .sheet = sheets.sprites },
//       .characters = { .sheet = sheets.sprites },
//       .resources  = { .sheet = sheets.sprites },
//   };
// =============================================================================

#pragma once
#include <string> // string

namespace Nezumi::Render {
    class Renderer2D;
    class SpriteSheet;
}

namespace Nightmourn {

struct WorldSheets {
    const Nezumi::Render::SpriteSheet* sprites = nullptr;
    const Nezumi::Render::SpriteSheet* ground  = nullptr;

    struct Options {
        // Skip the largest mip level (SpriteSheetConfig::skip_levels): half
        // the texture resolution, a quarter of the VRAM. For small GPUs.
        bool low_res = false;

        // From the environment, until there's a settings menu:
        // NIGHTMOURN_LOW_RES=1 turns low_res on
        static Options from_env();
    };

    // Creates both through the renderer, which owns them. Throws if a
    // manifest or page can't be loaded.
    static WorldSheets load(Nezumi::Render::Renderer2D& renderer, const Options& options);
    static WorldSheets load(Nezumi::Render::Renderer2D& renderer) { return load(renderer, Options{}); }

    // One line about how the sheets were loaded (levels, low res, whether
    // BC7 had to be decoded on the CPU), for the log
    std::string describe() const;
};

} // namespace Nightmourn
