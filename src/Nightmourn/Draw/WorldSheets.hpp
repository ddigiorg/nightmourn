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
//   const WorldSheets sheets = WorldSheets::load(*engine.renderer2d());
//   WorldDrawer::Config config = {
//       .chunks     = { .sheet = sheets.ground  },
//       .units      = { .sheet = sheets.sprites },
//       .characters = { .sheet = sheets.sprites },
//       .resources  = { .sheet = sheets.sprites },
//   };
// =============================================================================

#pragma once

namespace Nezumi::Render {
    class Renderer2D;
    class SpriteSheet;
}

namespace Nightmourn {

struct WorldSheets {
    const Nezumi::Render::SpriteSheet* sprites = nullptr;
    const Nezumi::Render::SpriteSheet* ground  = nullptr;

    // Creates both through the renderer, which owns them. Throws if a
    // manifest or page can't be loaded.
    static WorldSheets load(Nezumi::Render::Renderer2D& renderer);
};

} // namespace Nightmourn
