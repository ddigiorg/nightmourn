// =============================================================================
// Data/Configs.hpp
//
// Everything the game reads from data/: what each kind of thing looks like
// (and, later, its stats), the way Factorio's prototypes work. Adding a unit
// look, a tree or a terrain is a PNG and a few lines of JSON, with no code
// change or build step.
//
// ## Layout
//
//   data/
//     base/                       the game's own mod; later mods sit beside it
//       graphics/                 PNGs (straight alpha, as editors save them)
//         entity/figure/red/idle.png
//         entity/tree/oak-01.png
//         terrain/meadow.png
//         gui/eagle.png
//       configs/                  JSON, any file names and folders
//         entity/figures.json
//         entity/trees.json
//         tiles.json
//         gui.json
//
// Every .json under a mod's configs/ holds an array of configs, each
// with a "type" and a "name". Mods load in order (base first, then the rest
// alphabetically, files sorted by path), and a later config with the same
// type and name replaces the earlier one, so a mod can restyle the base game.
// File names start with "__<mod>__/", the mod's folder.
//
// ## Types
//
//   figure   an upright figure (units, characters): its animations by name,
//            one sheet PNG each, a column per frame and a row per stored
//            direction (see Nezumi's SpritePacker.hpp)
//
//     { "type": "figure", "name": "red",
//       "animations": {
//         "idle": { "filename": "__base__/graphics/entity/figure/red/idle.png",
//                   "width": 64, "height": 128,     // one frame, px
//                   "frame_count": 1,               // columns (default 1)
//                   "direction_count": 5,           // rows (default 1): down,
//                   "headings": 8,                  //   down-right, right, up-right,
//                                                   //   up; the rest mirrored
//                   "fps": 0, "loop": true,         // (defaults)
//                   "pivot": [32, 96] } } }         // feet, px in a frame, y down
//
//   tree     a tree variety and its looks, one PNG each; a tree picks one
//
//     { "type": "tree", "name": "oak",
//       "pictures": [ { "filename": "...", "pivot": [128, 384] }, ... ] }
//
//   tile     a ground texture covering `tiles` x `tiles` tiles, seamless
//
//     { "type": "tile", "name": "grass",
//       "texture": { "filename": "__base__/graphics/terrain/meadow.png", "tiles": 8 } }
//
//   sprite   a lone image, e.g. a GUI piece, drawn at its full size;
//            "border" keeps that many UI points unstretched when it is drawn
//            as a nine-slice (left, top, right, bottom)
//
//     { "type": "sprite", "name": "plate",
//       "filename": "__base__/graphics/gui/plate.png", "border": [8, 8, 8, 8] }
//
// "pivot" defaults to the bottom center of the frame.
// =============================================================================

#pragma once
#include <cstdint>     // uint32_t
#include <glm/glm.hpp> // vec2, vec4
#include <map>         // map
#include <optional>    // optional
#include <string>      // string
#include <string_view> // string_view
#include <vector>      // vector

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Pieces
// -----------------------------------------------------------------------------

struct AnimationConfig {
    std::string              filename;            // resolved (see Configs::load)
    std::uint32_t            width           = 0; // frame size, px
    std::uint32_t            height          = 0;
    std::uint32_t            frame_count     = 1;
    std::uint32_t            direction_count = 1;
    std::uint32_t            headings        = 0; // 0 = direction_count
    float                    fps             = 0.0f;
    bool                     loop            = true;
    std::optional<glm::vec2> pivot           = {};
};

struct PictureConfig {
    std::string              filename;
    std::optional<glm::vec2> pivot = {};
};

// -----------------------------------------------------------------------------
// Configs by type
// -----------------------------------------------------------------------------

struct FigureConfig {
    std::string                               name;
    std::map<std::string, AnimationConfig> animations;
};

struct TreeConfig {
    std::string                   name;
    std::vector<PictureConfig> pictures;
};

struct TileConfig {
    std::string   name;
    std::string   filename;
    std::uint32_t tiles = 1;
};

struct SpriteConfig {
    std::string              name;
    std::string              filename;
    std::optional<glm::vec2> pivot  = {};
    glm::vec4                border = {}; // UI points: left, top, right, bottom
};

// -----------------------------------------------------------------------------
// Configs
// -----------------------------------------------------------------------------

class Configs {
public:

    // Loads every mod under `data_dir`, relative to the executable's folder
    // (or absolute). File names come out as paths the same way:
    // "__base__/graphics/x.png" -> "<data_dir>/base/graphics/x.png". Throws
    // naming the file and config on any error: bad JSON, an unknown type,
    // a missing field, an unknown mod in a file name.
    static Configs load(std::string_view data_dir = "data");

    // In load order; a replaced config keeps its first place
    std::vector<FigureConfig> figures;
    std::vector<TreeConfig>   trees;
    std::vector<TileConfig>   tiles;
    std::vector<SpriteConfig> sprites;

    // Null if there's none by that name
    const FigureConfig* find_figure(std::string_view name) const;
    const TreeConfig*   find_tree  (std::string_view name) const;
    const TileConfig*   find_tile  (std::string_view name) const;
    const SpriteConfig* find_sprite(std::string_view name) const;

    // Every file name the configs use, for checks
    std::vector<std::string> filenames() const;
};

} // namespace Nightmourn
