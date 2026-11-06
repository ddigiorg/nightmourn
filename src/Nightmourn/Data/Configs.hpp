// =============================================================================
// Data/Configs.hpp
//
// Everything the game reads from data/: what each kind of thing is, how it
// plays and how it looks, the way Factorio's prototypes work. Adding a unit,
// a tree, an item or a light is a few lines of JSON (and a PNG if it needs
// one), with no code change or build step.
//
// ## Layout
//
//   data/
//     base/                       the game's own mod; later mods sit beside it
//       graphics/                 PNGs (straight alpha, as editors save them)
//         entity/unit/figure/idle_mask.png
//         entity/tree/oak-01.png
//         terrain/meadow.png
//         gui/eagle.png
//       configs/                  JSON, any file names and folders
//         entity/units.json         unit         warrior, archer
//         entity/unit-sprites.json  unit-sprite  figure
//         entity/unit-groups.json   unit-group   block
//         entity/projectiles.json   projectile   arrow
//         entity/characters.json    character    hero
//         entity/trees.json         tree         oak, pine
//         entity/buildings.json     building     banner, wall, gate, torch, house, workshop, barracks
//         settlements.json          settlement   player
//         raids.json                raid         night
//         items.json                item         wood, stone, gold, food, ... (and their recipes)
//         recipes.json              recipe-category  hand, settlement, crafter
//         lights.json               light        torch, campfire, lantern, ...
//         tiles.json                tile         grass
//         gui.json                  sprite       plate, slot, menu/...
//
// Every .json under a mod's configs/ holds an array of configs, each
// with a "type" and a "name". Mods load in order (base first, then the rest
// alphabetically, files sorted by path), and a later config with the same
// type and name replaces the earlier one, so a mod can retune or restyle the
// base game. File names start with "__<mod>__/", the mod's folder. A config
// names another by its name ("projectile": "arrow"), and the names are
// checked once every mod has loaded, so a mod can point at a base config and
// the other way round.
//
// Any field but "type", "name" and the file names may be left out for the
// C++ default (UnitConfig.hpp and friends document every field). A field
// the type doesn't have is an error, so a typo can't go unnoticed.
//
// Units and durations are the C++ ones: distances in tiles, speeds in tiles
// per second, durations in ticks (60 per second), except where a field says
// seconds (a recipe's "time", a raid's "interval"). Format the files with
// tools/format_json.py.
//
// ## Types
//
//   unit         a kind of unit (Unit/UnitConfig.hpp): its stats, the
//                "sprite" (a unit-sprite) it is drawn with, and for a ranged
//                one the "projectile" it fires
//
//     { "type": "unit", "name": "archer", "sprite": "figure",
//       "max_health": 60, "move_speed": 2.5,
//       "attack_damage": 6, "attack_interval": 120, "attack_windup": 25,
//       "attack_range": 10.0, "aggro_radius": 16.0,
//       "projectile": "arrow",                     // none: melee
//       "collision_box": [[-0.25, -0.25], [0.25, 0.25]] }   // [min, max]
//
//   unit-sprite  an upright figure's look (units, characters): its
//                animations by name, one sheet PNG each, a column per frame
//                and a row per stored direction (see Nezumi's
//                SpritePacker.hpp). The drawers tint it, so a "_mask" sheet
//                (white where the color goes) serves every side
//
//     { "type": "unit-sprite", "name": "figure",
//       "animations": {
//         "idle": { "filename": "__base__/graphics/entity/unit/figure/idle_mask.png",
//                   "width": 64, "height": 128,     // one frame, px
//                   "frame_count": 8,               // columns (default 1)
//                   "direction_count": 8,           // rows (default 1): down,
//                   "headings": 8,                  //   down-right, right, ...
//                                                   //   counter-clockwise; with
//                                                   //   fewer rows than headings
//                                                   //   the rest are mirrored
//                   "fps": 6, "loop": true,         // (defaults 0, true)
//                   "pivot": [32, 96] } } }         // feet, px in a frame, y down
//
//                Animations the drawers play: idle, walk, attack (frame 0 is
//                the hit) and death; any but idle may be left out
//
//   unit-group   a kind of formation (Unit/UnitGroupConfig.hpp)
//
//     { "type": "unit-group", "name": "block", "formation": "block",
//       "spacing": 0.9, "columns": 10, ... }
//
//   projectile   a kind of projectile (Projectile/ProjectileConfig.hpp);
//                "guidance" is "ground" or "tracking"
//
//     { "type": "projectile", "name": "arrow", "guidance": "ground",
//       "speed": 8.0, "arc": 0.25, ... }
//
//   character    a kind of character, e.g. the player's hero
//                (Character/CharacterConfig.hpp), and its "sprite"
//
//     { "type": "character", "name": "hero", "sprite": "figure",
//       "max_health": 300, "move_speed": 5.0, "mine_interval": 30,
//       "attack_damage": 25, ... }
//
//   tree         a tree variety: what mining it yields ("item", up to
//                "max_amount" of it; Resource/ResourceConfig.hpp) and its
//                looks, one PNG each; a tree picks one
//
//     { "type": "tree", "name": "oak", "item": "wood", "max_amount": 10,
//       "collision_box": [[-0.4, -0.4], [0.4, 0.4]],
//       "pictures": [ { "filename": "...", "pivot": [128, 384] }, ... ] }
//
//   building     a kind of building (Building/BuildingConfig.hpp): what it
//                does ("kind": "banner", "wall", "gate", "torch", "house",
//                "crafter" or "barracks"), its footprint in tiles, the item
//                placing it costs, the "light" it gives off (a light's name,
//                optional), and its "picture": a sheet like a unit-sprite
//                animation's whose "pivot" is the footprint's center in the
//                image (tools/gen_buildings.py prints them). A connecting
//                wall has 16 frames, one per set of connected sides (bits
//                north 1, east 2, south 4, west 8). A banner names the
//                "settlement" it founds. "solid": false lets figures walk
//                through it (a torch). What it costs is its item's "recipe".
//                A building that crafts, or lets the player craft more while
//                it stands in their settlement, has a "crafting" block
//
//     { "type": "building", "name": "house", "kind": "house",
//       "size": [2, 2], "max_health": 400, "item": "house",
//       "picture": { "filename": "__base__/graphics/entity/building/house_mask.png",
//                    "width": 268, "height": 250, "pivot": [134, 180] } }
//
//     { "type": "building", "name": "workshop", "kind": "crafter", ...,
//       "crafting": { "categories": ["crafter"],     // what it makes
//                     "speed": 1.0,                  // time multiplier (later)
//                     "unlocks_for_player": true } } // the player may make them too
//
//   settlement   what a banner's settlement gets (Settlement/SettlementConfig.hpp)
//
//     { "type": "settlement", "name": "player", "build_radius": 16,
//       "spawn_margin": 8, "min_gap": 4, "inventory_slots": 40 }
//
//   raid         night raids on settlements (Settlement/RaidConfig.hpp):
//                which "unit" spawns, every "interval" seconds while
//                darkness is at least "min_darkness", up to "max_alive"
//
//     { "type": "raid", "name": "night", "unit": "raider", "interval": 40,
//       "max_alive": 2, "min_distance": 2, "max_distance": 8,
//       "min_darkness": 0.5 }
//
//   item         a kind of item (Item/Items.hpp): its icon and stack size,
//                and optionally an inline "recipe" making one of it, named
//                after the item (a "recipe" config of that name replaces it)
//
//     { "type": "item", "name": "house", "icon": "__base__/graphics/gui/item/house.png",
//       "stack_size": 10,
//       "recipe": { "ingredients": { "wood": 4 }, "time": 2, "category": "settlement",
//                   "count": 1 } }          // how many one craft makes (default 1)
//
//   recipe       how something is made (Crafting/Recipe.hpp): what it takes
//                (none: from nothing), what it makes, how long in seconds
//                (default 0.5) and its "category" (default "hand")
//
//     { "type": "recipe", "name": "wooden-arrow", "category": "crafter",
//       "ingredients": { "wood": 1 }, "results": { "wooden-arrow": 10 },
//       "time": 0.5 }
//
//   recipe-category  where recipes are made (at most 32). The code knows
//                "hand" (the player, anywhere) and "settlement" (the player,
//                in one of their settlements); buildings offer the rest. The
//                "description" finishes "Made ..." in tooltips
//
//     { "type": "recipe-category", "name": "crafter",
//       "description": "in a settlement with a workshop" }
//
//   item         something an inventory holds, its icon and how many fit
//                in one slot. "recipe" (optional) is what crafting one
//                takes, by item name: a building's item is crafted from wood
//                in the Crafting menu, and the building is then free to place
//
//     { "type": "item", "name": "wood",
//       "icon": "__base__/graphics/gui/item/wood.png", "stack_size": 100 }
//     { "type": "item", "name": "house", "icon": "...", "stack_size": 10,
//       "recipe": { "wood": 4 } }
//
//   light        a light's look (Lighting/LightSource.hpp); the game places
//                it. "color" is linear RGB
//
//     { "type": "light", "name": "torch", "color": [1.0, 0.56, 0.24],
//       "intensity": 1.8, "radius": 5.5, "flicker": 0.8, ... }
//
//   tile         a ground texture covering `tiles` x `tiles` tiles, seamless
//
//     { "type": "tile", "name": "grass",
//       "texture": { "filename": "__base__/graphics/terrain/meadow.png", "tiles": 8 } }
//
//   sprite       a lone image, e.g. a GUI piece, drawn at its full size;
//                "border" keeps that many UI points unstretched when it is
//                drawn as a nine-slice (left, top, right, bottom)
//
//     { "type": "sprite", "name": "plate",
//       "filename": "__base__/graphics/gui/plate.png", "border": [8, 8, 8, 8] }
//
// "pivot" defaults to the bottom center of the frame.
//
// ## Ids
//
// The simulation's ids come from load order, so they are the same on every
// machine that loads the same mods: a unit's subtype is its place among the
// units, a tree's variety its place among the trees, and an item's ItemID
// its place among the items. The base items the code names (Items::Wood,
// Stone, Gold, Food) must come first and in that order, so a mod adds items
// after them and never reorders them.
//
// ## Lifetime
//
// Entities keep pointers to their configs (a Unit to its UnitConfig, and
// that to its ProjectileConfig), so keep the Configs alive, and in one
// place, as long as the world is. Configs can be moved (the configs inside
// stay where they are) but not copied.
// =============================================================================

#pragma once
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Character/CharacterConfig.hpp"
#include "Nightmourn/Crafting/Recipe.hpp"
#include "Nightmourn/Item/ItemAmount.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include "Nightmourn/Lighting/LightSource.hpp"
#include "Nightmourn/Projectile/ProjectileConfig.hpp"
#include "Nightmourn/Resource/ResourceConfig.hpp"
#include "Nightmourn/Settlement/RaidConfig.hpp"
#include "Nightmourn/Settlement/SettlementConfig.hpp"
#include "Nightmourn/Unit/UnitConfig.hpp"
#include "Nightmourn/Unit/UnitGroupConfig.hpp"
#include <cstdint>     // uint16_t, uint32_t
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
//
// The ones the simulation uses wrap its config (`config`), whose name views
// `name` and whose ids and pointers are filled in once every mod has loaded.
// -----------------------------------------------------------------------------

// "unit"
struct UnitPrototype {
    std::string name;
    std::string sprite;     // a unit-sprite's name, or empty
    std::string projectile; // a projectile's name, or empty for melee
    UnitConfig  config;     // subtype: place among the units
};

// "unit-sprite": how a figure looks
struct UnitSpriteConfig {
    std::string                            name;
    std::map<std::string, AnimationConfig> animations;
};

// "unit-group"
struct UnitGroupPrototype {
    std::string     name;
    UnitGroupConfig config;
};

// "projectile"
struct ProjectilePrototype {
    std::string      name;
    ProjectileConfig config;
};

// "character"
struct CharacterPrototype {
    std::string     name;
    std::string     sprite; // a unit-sprite's name, or empty
    CharacterConfig config;
};

// "tree": a Resource of subtype ResourceType::Tree
struct TreeConfig {
    std::string                name;
    std::string                item;     // an item's name, or empty for none
    ResourceConfig             resource; // variety: place among the trees
    std::vector<PictureConfig> pictures;
};

// "building"
struct BuildingPrototype {
    std::string     name;
    std::string     item;       // an item's name, or empty for none
    std::string     settlement; // banners: a settlement's name
    std::string     light;      // a light's name, or empty for none
    BuildingConfig  config;     // index: place among the buildings
    AnimationConfig picture;    // frames: 1, or 16 for a connecting wall

    std::vector<std::string> crafting_categories = {}; // as read; linked into config
};

// "settlement"
struct SettlementPrototype {
    std::string      name;
    SettlementConfig config;
};

// "raid"
struct RaidPrototype {
    std::string name;
    std::string unit;   // a unit's name: what raids
    RaidConfig  config; // unit: linked
};

// Some number of an item by name, as read (linked into ItemAmounts)
using ItemAmountNames = std::vector<std::pair<std::string, std::uint32_t>>;

// "recipe", and an item's inline "recipe"
struct RecipePrototype {
    std::string     name;
    ItemAmountNames ingredients = {}; // as read
    ItemAmountNames results     = {}; // as read
    std::string     category    = "hand";
    float           time        = 0.5f; // seconds
    Recipe          recipe      = {};   // linked: spans into Configs::recipe_amounts
};

// "recipe-category"
struct RecipeCategoryPrototype {
    std::string       name;
    std::string       description; // "in a settlement with a workshop"
    CraftCategoryMask bit = 0;
};

// "item"
struct ItemPrototype {
    std::string   name;
    ItemID        id         = {}; // place among the items
    std::string   icon;            // resolved file name, or empty
    std::uint16_t stack_size = 1;

    // How it's made: the recipe named after it if there is one, else the
    // first that makes it; null for an item only found in the world. Linked.
    const Recipe* recipe = nullptr;

    std::optional<RecipePrototype> inline_recipe = {}; // as read: its "recipe"
};

// "light": `light.position` and `light.seed` are the game's to set
struct LightPrototype {
    std::string name;
    LightSource light;
};

// "tile"
struct TileConfig {
    std::string   name;
    std::string   filename;
    std::uint32_t tiles = 1;
};

// "sprite"
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
    // naming the file and config on any error: bad JSON, an unknown type or
    // field, a missing field, a value out of range, an unknown mod in a file
    // name, or a name that doesn't match a config.
    //
    // Also makes the items the ones item_name() and item_stack_size() (and
    // so inventories) go by.
    static Configs load(std::string_view data_dir = "data");

    Configs()                          = default;
    Configs(Configs&&)                 = default;
    Configs& operator=(Configs&&)      = default;
    Configs(const Configs&)            = delete; // entities point into it
    Configs& operator=(const Configs&) = delete;

    // In load order; a replaced config keeps its first place
    std::vector<UnitPrototype>       units;
    std::vector<UnitSpriteConfig>    unit_sprites;
    std::vector<UnitGroupPrototype>  unit_groups;
    std::vector<ProjectilePrototype> projectiles;
    std::vector<CharacterPrototype>  characters;
    std::vector<TreeConfig>          trees;
    std::vector<BuildingPrototype>   buildings;
    std::vector<SettlementPrototype> settlements;
    std::vector<RaidPrototype>       raids;
    std::vector<ItemPrototype>       items;
    std::vector<RecipePrototype>     recipes;           // explicit ones, then the items' inline ones
    std::vector<RecipeCategoryPrototype> recipe_categories;
    std::vector<LightPrototype>      lights;
    std::vector<TileConfig>          tiles;
    std::vector<SpriteConfig>        sprites;

    // Every recipe's ingredients and results, back to back: Recipe's spans
    // point in here, so starting a craft reads one small contiguous block
    std::vector<ItemAmount> recipe_amounts;

    // The categories the code offers (0 if their config is missing)
    CraftCategoryMask hand_category       = 0; // the player, anywhere
    CraftCategoryMask settlement_category = 0; // the player, in their settlement

    // Null if there's none by that name
    const UnitPrototype*       find_unit       (std::string_view name) const;
    const UnitSpriteConfig*    find_unit_sprite(std::string_view name) const;
    const UnitGroupPrototype*  find_unit_group (std::string_view name) const;
    const ProjectilePrototype* find_projectile (std::string_view name) const;
    const CharacterPrototype*  find_character  (std::string_view name) const;
    const TreeConfig*          find_tree       (std::string_view name) const;
    const BuildingPrototype*   find_building   (std::string_view name) const;
    const SettlementPrototype* find_settlement (std::string_view name) const;
    const RaidPrototype*       find_raid       (std::string_view name) const;
    const ItemPrototype*       find_item       (std::string_view name) const;
    const RecipePrototype*     find_recipe     (std::string_view name) const;
    const RecipeCategoryPrototype* find_recipe_category(std::string_view name) const;
    const LightPrototype*      find_light      (std::string_view name) const;
    const TileConfig*          find_tile       (std::string_view name) const;
    const SpriteConfig*        find_sprite     (std::string_view name) const;

    // The same, but throws naming the type if there's none, for configs the
    // game can't do without
    const UnitPrototype&       unit      (std::string_view name) const;
    const UnitGroupPrototype&  unit_group(std::string_view name) const;
    const CharacterPrototype&  character (std::string_view name) const;
    const TreeConfig&          tree      (std::string_view name) const;
    const BuildingPrototype&   building  (std::string_view name) const;
    const LightPrototype&      light     (std::string_view name) const;
    const RaidPrototype&       raid      (std::string_view name) const;

    // The building that `item` places, or null if it places none
    const BuildingPrototype* building_for_item(ItemID item) const;

    // The recipe by id, or null
    const Recipe* recipe(RecipeID id) const {
        return id && id.value() < recipes.size() ? &recipes[id.value()].recipe : nullptr;
    }

    // The item config by id, or null
    const ItemPrototype* item(ItemID id) const {
        return id && id.value() < items.size() ? &items[id.value()] : nullptr;
    }

    // The category config of a recipe's (or any one-bit) mask, or null
    const RecipeCategoryPrototype* recipe_category(CraftCategoryMask bit) const;

    // Every file name the configs use, for checks
    std::vector<std::string> filenames() const;
};

} // namespace Nightmourn
