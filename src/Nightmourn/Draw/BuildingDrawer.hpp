// =============================================================================
// Draw/BuildingDrawer.hpp
//
// Draws Buildings, and the ghost of one being placed.
//
// Each building config's picture is packed as the animation
// "building/<name>" (WorldSheets): one frame, or 16 for a connecting wall,
// whose frame is its Building::connections. Pictures are masks, like the
// unit figures: white takes the owner faction's color, black stays black.
// The art is top-down (tools/gen_buildings.py: squares filling the
// footprint, poles standing on its center), drawn with its pivot on the
// footprint's center and y-sorted with the figures by the footprint's
// bottom edge. In the isometric debug view a building bigger than 1 x 1 is
// drawn in vertical strips instead, each sorted by the footprint's front
// edge in its column.
//
// fill_ground() fills a world box on the ground (a diamond in iso) with a
// color, for ghosts and the settlement's build area.
//
// Damaged buildings get a healthbar over their picture. Debug overlay:
//
//   ShowCollisionRectangles  the footprint
// =============================================================================

#pragma once
#include "Nezumi/Render/2D/Sprite/SpriteID.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include <array>       // array
#include <cstdint>     // uint32_t
#include <glm/glm.hpp> // vec4
#include <string>      // string
#include <vector>      // vector

namespace Nezumi::Render {
    class SpriteSheet;
}

namespace Nightmourn {

class BuildingDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Picture {
        std::string animation = {};   // in the sheet ("building/<name>")
        float       top       = 1.0f; // tiles from the pivot up to the image's top
    };

    struct Config {
        const Nezumi::Render::SpriteSheet* sheet = nullptr;

        // Each building config's picture, by BuildingConfig::index
        // (WorldSheets::drawer_config fills it). One left empty, or not in
        // the sheet, isn't drawn.
        std::vector<Picture> pictures = {};

        // Tint of each faction's buildings (the mask's white); a faction
        // left at zero alpha draws white
        std::array<glm::vec4, MAX_FACTIONS> faction_colors = {};

        // A plain white sprite in the sheet, for fill_ground (none: no fills)
        std::string fill_sprite = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config.sheet` must outlive the drawer
    explicit BuildingDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Building& building) const;

    // The building `config` would be at `origin`, see-through and tinted
    // `tint` (green where it fits, red where it doesn't), over its footprint
    // filled with the same color. Drawn over the world (EntityOverlay).
    // `connections` picks a wall's frame.
    void draw_ghost(DrawContext& ctx, const BuildingConfig& config, TileCoord origin,
                    glm::vec4 tint, std::uint8_t connections = 0) const;

    // Fills world box `box` on the ground with `color`: a rectangle top-down,
    // a diamond in iso
    void fill_ground(DrawContext& ctx, const Area2D& box, glm::vec4 color, std::uint8_t layer) const;

    // The owner color of `factions`' buildings
    glm::vec4 faction_color(EntityFactionsMask factions) const;

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    struct Frames {
        std::uint32_t first = 0;
        std::uint32_t count = 0;
        float         top   = 1.0f;
    };

    const Frames* frames_(const BuildingConfig& config) const;

    // A multi-tile building's picture in strips, each sorted by its own
    // footprint depth
    void draw_sliced_(DrawContext& ctx, Nezumi::Render::SpriteID sprite, const TileRect& footprint,
                      glm::vec4 color) const;

    const Nezumi::Render::SpriteSheet*  sheet_          = nullptr;
    std::vector<Frames>                 pictures_       = {};
    std::array<glm::vec4, MAX_FACTIONS> faction_colors_ = {};
    bool                                has_fill_       = false;
    glm::vec4                           fill_tex_       = {};
    std::uint32_t                       fill_layer_     = 0;
};

} // namespace Nightmourn
