// =============================================================================
// Draw/WorldSheets.cpp
// =============================================================================

#include "Nightmourn/Draw/WorldSheets.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nezumi/Render/2D/Sprite/SpritePacker.hpp"
#include "Nezumi/Render/Core/Image/Image.hpp"
#include <stdexcept> // runtime_error

namespace Nightmourn {

namespace NR = Nezumi::Render;

const WorldSheets::GroundTexture& WorldSheets::ground_texture(std::string_view tile) const {
    for (const auto& [name, texture] : ground_textures) {
        if (name == tile) return texture;
    }
    throw std::runtime_error("WorldSheets: no tile config '" + std::string(tile) + "'");
}

WorldDrawer::Config WorldSheets::drawer_config() const {
    const GroundTexture& grass = ground_texture("grass");
    return {
        .chunks     = { .sheet = ground, .grass_layer = grass.layer, .grass_tiles = grass.tiles },
        .units      = { .sheet = sprites },
        .characters = { .sheet = sprites },
        .resources  = { .sheet = sprites, .trees = tree_animations },
        .buildings  = { .sheet = sprites, .pictures = building_pictures, .faction_colors = {} },
    };
}

WorldSheets WorldSheets::load(NR::Renderer2D& renderer, const Configs& configs) {
    WorldSheets sheets;

    // Figures and trees, packed into one sheet so they batch together
    NR::SpritePacker packer;
    for (const UnitSpriteConfig& figure : configs.unit_sprites) {
        for (const auto& [name, a] : figure.animations) {
            packer.add_animation("unit/" + figure.name + "/" + name, NR::load_image(a.filename), {
                .frame_width  = a.width,
                .frame_height = a.height,
                .frames       = a.frame_count,
                .directions   = a.direction_count,
                .headings     = a.headings,
                .fps          = a.fps,
                .loop         = a.loop,
                .pivot        = a.pivot,
            });
        }
    }
    for (const TreeConfig& tree : configs.trees) {
        const auto first = static_cast<NR::SpriteID>(packer.sprite_count());
        for (std::size_t i = 0; i < tree.pictures.size(); i++) {
            const PictureConfig& pic = tree.pictures[i];
            packer.add_sprite("tree/" + tree.name + "/" + std::to_string(i), NR::load_image(pic.filename),
                              { .pivot = pic.pivot });
        }
        sheets.tree_animations.push_back("tree/" + tree.name);
        packer.add_animation("tree/" + tree.name, NR::Animation{
            .first  = first,
            .frames = static_cast<std::uint32_t>(tree.pictures.size()),
            .fps    = 0.0f,
            .loop   = false,
        });
    }

    for (const BuildingPrototype& b : configs.buildings) {
        const AnimationConfig& a = b.picture;
        packer.add_animation("building/" + b.name, NR::load_image(a.filename), {
            .frame_width  = a.width,
            .frame_height = a.height,
            .frames       = a.frame_count,
            .directions   = a.direction_count,
            .headings     = a.headings,
            .fps          = a.fps,
            .loop         = a.loop,
            .pivot        = a.pivot,
        });
        const float pivot_y = a.pivot ? a.pivot->y : static_cast<float>(a.height);
        sheets.building_pictures.push_back({ "building/" + b.name, pivot_y / SPRITE_PIXELS_PER_TILE });
    }

    // Mips for zooming out: 4 levels, so the packer leaves an 8 px grid and
    // gaps and no level averages neighbors together
    sheets.sprites = renderer.create_sprite_sheet({ .name = "sprites", .mip_levels = 4 }, packer).object;

    // Ground: one page per tile, the full mip chain, wrapping (Repeat). Iso
    // squashes the ground 2:1, so anisotropy keeps it from blurring, and a
    // small negative bias keeps the grass detail crisp.
    std::vector<NR::Image> pages;
    for (const TileConfig& tile : configs.tiles) {
        sheets.ground_textures.push_back({ tile.name, { static_cast<std::uint32_t>(pages.size()), tile.tiles } });
        pages.push_back(NR::load_image(tile.filename));
    }
    sheets.ground = renderer.create_sprite_sheet({
        .name           = "ground",
        .address_mode   = Nezumi::RHI::SamplerAddressMode::Repeat,
        .mip_levels     = 0,
        .max_anisotropy = 4.0f,
        .lod_bias       = -0.5f,
    }, pages).object;

    return sheets;
}

} // namespace Nightmourn
