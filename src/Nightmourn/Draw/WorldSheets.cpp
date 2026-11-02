// =============================================================================
// Draw/WorldSheets.cpp
// =============================================================================

#include "Nightmourn/Draw/WorldSheets.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include <cstdint> // uint32_t
#include <cstdlib> // getenv
#include <string>  // string, to_string

namespace Nightmourn {

WorldSheets::Options WorldSheets::Options::from_env() {
    const char* v = std::getenv("NIGHTMOURN_LOW_RES");
    return { .low_res = v && v[0] && v[0] != '0' };
}

std::string WorldSheets::describe() const {
    const auto one = [](const char* name, const Nezumi::Render::SpriteSheet* s) {
        if (!s) return std::string(name) + " (none)";
        return std::string(name) + " " + std::to_string(s->page_width()) + "x" + std::to_string(s->page_height()) +
               " x" + std::to_string(s->pages()) + ", " + std::to_string(s->mip_levels()) + " levels" +
               (s->decoded_on_cpu() ? ", BC7 decoded on the CPU (no BPTC)" : "");
    };
    return "Sheets: " + one("sprites", sprites) + "; " + one("ground", ground);
}

WorldSheets WorldSheets::load(Nezumi::Render::Renderer2D& renderer, const Options& options) {
    WorldSheets sheets;
    const std::uint32_t skip = options.low_res ? 1u : 0u;

    // BC7 KTX2 pages with 4 mip levels baked in by pack_sprites.py, which
    // places sprites on an 8 px grid with 8 px gaps so no level averages
    // neighbors together
    sheets.sprites = renderer.create_sprite_sheet({
        .name        = "sprites",
        .manifest    = "assets/graphics/sprites.json",
        .skip_levels = skip,
    }).object;

    // BC7 with the full mip chain (gen_meadow.py), wrapping (Repeat). Iso
    // squashes the ground 2:1, so anisotropy keeps it from blurring, and a
    // small negative bias keeps the grass detail crisp.
    sheets.ground = renderer.create_sprite_sheet({
        .name           = "ground",
        .manifest       = "assets/graphics/meadow.json",
        .address_mode   = Nezumi::RHI::SamplerAddressMode::Repeat,
        .skip_levels    = skip,
        .max_anisotropy = 4.0f,
        .lod_bias       = -0.5f,
    }).object;

    return sheets;
}

} // namespace Nightmourn
