// =============================================================================
// Draw/WorldSheets.cpp
// =============================================================================

#include "Nightmourn/Draw/WorldSheets.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"

namespace Nightmourn {

WorldSheets WorldSheets::load(Nezumi::Render::Renderer2D& renderer) {
    WorldSheets sheets;

    // Mips for zooming out: pack_sprites.py places sprites on an 8 px grid
    // with 8 px gaps, so 4 levels never average neighbors together
    sheets.sprites = renderer.create_sprite_sheet({
        .name       = "sprites",
        .manifest   = "assets/graphics/sprites.json",
        .mip_levels = 4,
    }).object;

    // The full mip chain, wrapping (Repeat). Iso squashes the ground 2:1, so
    // anisotropy keeps it from blurring, and a small negative bias keeps the
    // grass detail crisp.
    sheets.ground = renderer.create_sprite_sheet({
        .name           = "ground",
        .manifest       = "assets/graphics/meadow.json",
        .address_mode   = Nezumi::RHI::SamplerAddressMode::Repeat,
        .mip_levels     = 0,
        .max_anisotropy = 4.0f,
        .lod_bias       = -0.5f,
    }).object;

    return sheets;
}

} // namespace Nightmourn
