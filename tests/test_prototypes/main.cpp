// =============================================================================
// tests/test_prototypes/main.cpp
//
// Headless checks for Data/Prototypes: the shipped data/ loads, names every
// prototype the game looks up, and points only at files that exist; a mod
// replaces a base prototype by type and name; and broken prototypes throw
// with their file and name. Test mods are written to a temp folder.
// =============================================================================

#include "Nightmourn/Data/Prototypes.hpp"
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

// The error Prototypes::load throws, or "" if it doesn't
static std::string load_error(const fs::path& dir) {
    try { Prototypes::load(dir.string()); } catch (const std::runtime_error& e) { return e.what(); }
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
    const Prototypes p = Prototypes::load(NIGHTMOURN_DATA_DIR);

    // What the drawers and UI look up by default
    for (const char* figure : { "red", "blue", "purple", "cyan" }) {
        const FigurePrototype* f = p.find_figure(figure);
        CHECK(f && f->animations.contains("idle"));
    }
    CHECK(p.find_tree("oak") && p.find_tree("pine"));
    CHECK(p.find_tile("grass") && p.find_tile("grass")->tiles == 8);
    for (const char* sprite : { "eagle", "plate", "slot", "rim", "button", "divider", "fill",
                                "menu/skills", "menu/main", "item/wood", "item/food" }) {
        CHECK(p.find_sprite(sprite) != nullptr);
    }
    CHECK(p.find_sprite("plate")->border == glm::vec4(8.0f));

    // Every file a prototype names is there
    for (const std::string& file : p.filenames()) {
        const bool found = fs::exists(file);
        if (!found) std::printf("  missing %s\n", file.c_str());
        CHECK(found);
    }
}

static void test_mods() {
    std::printf("mods\n");
    const fs::path dir = fs::temp_directory_path() / "nightmourn_test_prototypes";
    fs::remove_all(dir);

    write(dir / "base/prototypes/a.json", R"([
        { "type": "tile", "name": "grass", "texture": { "filename": "__base__/grass.png", "tiles": 4 } },
        { "type": "tree", "name": "oak", "pictures": [ { "filename": "__base__/oak.png" } ] }
    ])");
    write(dir / "zmod/prototypes/sub/b.json", R"([
        { "type": "tile", "name": "grass", "texture": { "filename": "__zmod__/lush.png", "tiles": 2 } },
        { "type": "figure", "name": "x", "animations": {
            "walk": { "filename": "__base__/x.png", "width": 8, "height": 16, "frame_count": 4,
                      "direction_count": 5, "headings": 8, "fps": 10, "pivot": [4, 12] } } }
    ])");

    const Prototypes p = Prototypes::load(dir.string());
    CHECK(p.tiles.size() == 1);
    CHECK(p.tiles[0].tiles == 2);
    CHECK(p.tiles[0].filename == (dir / "zmod" / "lush.png").string());
    CHECK(p.trees.size() == 1 && !p.trees[0].pictures[0].pivot);

    const AnimationPrototype& walk = p.find_figure("x")->animations.at("walk");
    CHECK(walk.filename == (dir / "base" / "x.png").string());
    CHECK(walk.width == 8 && walk.height == 16 && walk.frame_count == 4);
    CHECK(walk.direction_count == 5 && walk.headings == 8 && walk.fps == 10.0f && walk.loop);
    CHECK(walk.pivot && *walk.pivot == glm::vec2(4.0f, 12.0f));

    fs::remove_all(dir);
}

static void test_errors() {
    std::printf("errors\n");
    const fs::path dir = fs::temp_directory_path() / "nightmourn_test_prototypes";
    const auto broken = [&](const std::string& json) {
        fs::remove_all(dir);
        write(dir / "base/prototypes/bad.json", json);
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
    CHECK(broken(R"([ { "type": "figure", "name": "f", "animations": { "idle": { "filename": "__base__/f.png" } } } ])")
              .find("figure 'f'") != std::string::npos); // no width
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
