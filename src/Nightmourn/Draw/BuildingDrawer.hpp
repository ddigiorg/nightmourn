// =============================================================================
// Draw/BuildingDrawer.hpp
//
// Draws Buildings, and the ghost of one being placed.
//
// Each building config's picture is packed as the animation
// "building/<name>" (WorldSheets): one frame, or 16 for a connecting wall,
// whose frame is its Building::connections. Pictures are masks, like the
// unit figures: white takes the owner faction's color, black stays black.
// A picture is drawn with its pivot (bottom-center by default) on the
// footprint's bottom-center, y-sorted with the figures by that edge, so a
// figure south of a house stands in front of it and one north of it behind.
//
// Damaged buildings get a healthbar over their picture. Debug overlay:
//
//   ShowCollisionRectangles  the footprint
// =============================================================================

#pragma once
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

    const Nezumi::Render::SpriteSheet*  sheet_          = nullptr;
    std::vector<Frames>                 pictures_       = {};
    std::array<glm::vec4, MAX_FACTIONS> faction_colors_ = {};
};

} // namespace Nightmourn
