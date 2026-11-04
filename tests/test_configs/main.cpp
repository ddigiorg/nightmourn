// =============================================================================
// tests/test_configs/main.cpp
//
// Headless checks for Data/Configs: the shipped data/ loads, names every
// config the game looks up, and points only at files that exist; a mod
// replaces a base config by type and name; configs name each other across
// mods; and broken configs throw with their file and name. Test mods are
// written to a temp folder.
// =============================================================================

#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include <cstdio>     // printf
#include <cstdlib>    // EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem> // exists, path, temp_directory_path
#include <fstream>    // ofstream
#include <stdexcept>  // runtime_error
#include <string>     // string

using namespace Nightmourn;
namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Harness
// -----------------------------------------------------------------------------

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

// The error Configs::load throws, or "" if it doesn't
static std::string load_error(const fs::path& dir) {
    try { Configs::load(dir.string()); } catch (const std::runtime_error& e) { return e.what(); }
    return "";
}

static void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_shipped_data() {
    std::printf("shipped data\n");
    const Configs p = Configs::load(NIGHTMOURN_DATA_DIR);

    // What the drawers and UI look up by default
    {
        const UnitSpriteConfig* u = p.find_unit_sprite("figure");
        CHECK(u != nullptr);
        if (u) for (const char* anim : { "idle", "walk", "attack", "death" }) CHECK(u->animations.contains(anim));
    }
    CHECK(p.find_tree("oak") && p.find_tree("pine"));

    // What the demos spawn, linked up
    {
        const UnitPrototype* warrior = p.find_unit("warrior");
        const UnitPrototype* archer  = p.find_unit("archer");
        CHECK(warrior && archer);
        if (warrior && archer) {
            CHECK(warrior->config.name == "warrior" && warrior->sprite == "figure");
            CHECK(!warrior->config.projectile);
            CHECK(archer->config.projectile && archer->config.projectile->name == "arrow");
            CHECK(warrior->config.subtype != archer->config.subtype);
        }
        CHECK(p.find_unit_group("block") && p.find_unit_group("block")->config.columns == 10);
        CHECK(p.find_character("hero") && p.find_character("hero")->config.max_health == 300);
        const TreeConfig& oak = p.tree("oak");
        CHECK(oak.resource.item == Items::Wood && oak.resource.subtype == to_subtype(ResourceType::Tree));
        CHECK(oak.resource.variety != p.tree("pine").resource.variety);
        for (const char* light : { "torch", "campfire", "lantern", "wisp-frost", "wisp-arcane" })
            CHECK(p.find_light(light) != nullptr);
    }

    // Items, and what inventories now go by
    {
        const ItemPrototype* wood = p.find_item("wood");
        CHECK(wood && wood->id == Items::Wood && !wood->icon.empty());
        CHECK(p.find_item("food") && p.find_item("food")->id == Items::Food);
        CHECK(item_name(Items::Gold) == "gold");
        CHECK(item_stack_size(Items::Wood) == 100 && item_stack_size(Items::Food) == 20);
    }
    // Buildings and the settlement a banner founds
    {
        const BuildingPrototype& banner = p.building("banner");
        const BuildingPrototype& wall   = p.building("wall");
        CHECK(banner.config.type == BuildingType::Banner && banner.config.settlement);
        CHECK(banner.config.settlement && banner.config.settlement->name == "player");
        CHECK(wall.config.connects && wall.picture.frame_count == 16);
        CHECK(p.building("house").config.footprint == TileCoord({ 2, 2 }));
        CHECK(p.building("crafter").config.footprint == TileCoord({ 3, 3 }));
        CHECK(p.building_for_item(p.find_item("house")->id) == p.find_building("house"));
        CHECK(!p.building_for_item(Items::Wood));
        CHECK(p.building("torch").config.type == BuildingType::Torch && p.building("torch").light == "torch");
        CHECK(p.building("barracks").config.type == BuildingType::Barracks);
        CHECK(p.building("barracks").config.footprint == TileCoord({ 3, 3 }));
        CHECK(p.building_for_item(p.find_item("barracks")->id) == p.find_building("barracks"));
    }
    CHECK(p.find_tile("grass") && p.find_tile("grass")->tiles == 8);
    for (const char* sprite : { "eagle", "plate", "slot", "rim", "button", "divider", "fill",
                                "menu/skills", "menu/main" }) {
        CHECK(p.find_sprite(sprite) != nullptr);
    }
    CHECK(p.find_sprite("plate")->border == glm::vec4(8.0f));

    // Every file a config names is there
    for (const std::string& file : p.filenames()) {
        const bool found = fs::exists(file);
        if (!found) std::printf("  missing %s\n", file.c_str());
        CHECK(found);
    }
}

static void test_mods() {
    std::printf("mods\n");
    const fs::path dir = fs::temp_directory_path() / "nightmourn_test_configs";
    fs::remove_all(dir);

    write(dir / "base/configs/a.json", R"([
        { "type": "tile", "name": "grass", "texture": { "filename": "__base__/grass.png", "tiles": 4 } },
        { "type": "tree", "name": "oak", "pictures": [ { "filename": "__base__/oak.png" } ] },
        { "type": "unit", "name": "archer", "max_health": 60, "projectile": "bolt" },
        { "type": "item", "name": "wood", "stack_size": 100 }
    ])");
    write(dir / "zmod/configs/sub/b.json", R"([
        { "type": "tile", "name": "grass", "texture": { "filename": "__zmod__/lush.png", "tiles": 2 } },
        { "type": "unit-sprite", "name": "x", "animations": {
            "walk": { "filename": "__base__/x.png", "width": 8, "height": 16, "frame_count": 4,
                      "direction_count": 5, "headings": 8, "fps": 10, "pivot": [4, 12] } } },
        { "type": "unit", "name": "archer", "max_health": 80, "sprite": "x", "projectile": "bolt" },
        { "type": "projectile", "name": "bolt", "guidance": "tracking" },
        { "type": "item", "name": "rope", "stack_size": 5 },
        { "type": "tree", "name": "birch", "item": "rope", "pictures": [ { "filename": "__zmod__/b.png" } ] }
    ])");

    const Configs p = Configs::load(dir.string());
    CHECK(p.tiles.size() == 1);
    CHECK(p.tiles[0].tiles == 2);
    CHECK(p.tiles[0].filename == (dir / "zmod" / "lush.png").string());
    CHECK(p.trees.size() == 2 && !p.trees[0].pictures[0].pivot);

    // The mod's archer replaces base's whole, and fires the mod's bolt
    const UnitPrototype& archer = p.unit("archer");
    CHECK(p.units.size() == 1 && archer.config.max_health == 80 && archer.sprite == "x");
    CHECK(archer.config.projectile && archer.config.projectile->guidance == ProjectileGuidance::Tracking);
    CHECK(archer.config.attack_damage == UnitConfig{}.attack_damage); // left out: the C++ default

    // Items and trees get ids by place; a mod's item comes after base's
    CHECK(p.find_item("rope") && p.find_item("rope")->id == ItemID{ 1 });
    CHECK(p.tree("birch").resource.item == ItemID{ 1 } && p.tree("birch").resource.variety == ResourceVarietyID{ 1 });
    CHECK(item_name(ItemID{ 1 }) == "rope" && item_stack_size(ItemID{ 1 }) == 5);

    const AnimationConfig& walk = p.find_unit_sprite("x")->animations.at("walk");
    CHECK(walk.filename == (dir / "base" / "x.png").string());
    CHECK(walk.width == 8 && walk.height == 16 && walk.frame_count == 4);
    CHECK(walk.direction_count == 5 && walk.headings == 8 && walk.fps == 10.0f && walk.loop);
    CHECK(walk.pivot && *walk.pivot == glm::vec2(4.0f, 12.0f));

    fs::remove_all(dir);
}

static void test_errors() {
    std::printf("errors\n");
    const fs::path dir = fs::temp_directory_path() / "nightmourn_test_configs";
    const auto broken = [&](const std::string& json) {
        fs::remove_all(dir);
        write(dir / "base/configs/bad.json", json);
        return load_error(dir);
    };

    CHECK(load_error(dir / "nowhere").find("no data folder") != std::string::npos);
    CHECK(broken("{").find("bad.json") != std::string::npos);
    CHECK(broken(R"({ "type": "tile" })").find("array") != std::string::npos);
    CHECK(broken(R"([ { "type": "dragon", "name": "d" } ])").find("dragon 'd': unknown type") != std::string::npos);
    CHECK(broken(R"([ { "type": "tree", "name": "t", "pictures": [ { "filename": "oak.png" } ] } ])")
              .find("__<mod>__") != std::string::npos);
    CHECK(broken(R"([ { "type": "tree", "name": "t", "pictures": [ { "filename": "__nope__/oak.png" } ] } ])")
              .find("unknown mod 'nope'") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit-sprite", "name": "f", "animations": { "idle": { "filename": "__base__/f.png" } } } ])")
              .find("unit-sprite 'f': animation 'idle': needs 'width'") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "max_heatlh": 5 } ])")
              .find("unit 'u': unknown field 'max_heatlh'") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "max_health": -5 } ])")
              .find("'max_health' must be a whole number from 0 to 65535") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "move_speed": "fast" } ])")
              .find("'move_speed' must be a number") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "attack_interval": 10, "attack_windup": 10 } ])")
              .find("'attack_windup' must be less than") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "projectile": "rock" } ])")
              .find("bad.json: unit 'u': 'projectile' names no projectile 'rock'") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "sprite": "ghost" } ])")
              .find("names no unit-sprite 'ghost'") != std::string::npos);
    CHECK(broken(R"([ { "type": "unit", "name": "u", "collision_box": [[1, 0], [0, 1]] } ])")
              .find("'collision_box' must have its min below its max") != std::string::npos);
    CHECK(broken(R"([ { "type": "projectile", "name": "p", "guidance": "homing" } ])")
              .find("'guidance' must be") != std::string::npos);
    CHECK(broken(R"([ { "type": "item", "name": "stone" } ])")
              .find("item 'stone': must be item number 2") != std::string::npos);
    CHECK(broken(R"([ { "type": "tree", "name": "t", "item": "gem", "pictures": [ { "filename": "__base__/t.png" } ] } ])")
              .find("'item' names no item 'gem'") != std::string::npos);
    CHECK(broken(R"([ { "type": "sprite", "name": "s", "filename": "__base__/s.png", "pivot": [1] } ])")
              .find("'pivot' must be [x, y]") != std::string::npos);
    fs::remove_all(dir);
}

int main() {
    test_shipped_data();
    test_mods();
    test_errors();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
