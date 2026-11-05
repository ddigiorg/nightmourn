// =============================================================================
// Data/Configs.cpp
// =============================================================================

#include "Nightmourn/Data/Configs.hpp"

#include "Nezumi/Core/BasePath.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include "Nightmourn/World/Constants.hpp"
#include <algorithm>   // find_if, sort
#include <cmath>       // floor, lround
#include <filesystem>  // directory_iterator, path, recursive_directory_iterator
#include <fstream>     // ifstream
#include <limits>      // numeric_limits
#include <map>         // map
#include <nlohmann/json.hpp>
#include <set>         // set
#include <stdexcept>   // runtime_error
#include <type_traits> // is_integral_v, is_same_v

namespace Nightmourn {

namespace {

namespace fs = std::filesystem;
using Json   = nlohmann::json;
using Mods   = std::vector<std::pair<std::string, fs::path>>;

// Where the config being read came from, for error messages
struct Source {
    std::string file;
    std::string what; // "unit 'warrior'"
};

[[noreturn]] void fail(const Source& src, const std::string& what) {
    throw std::runtime_error("Configs: " + src.file + (src.what.empty() ? "" : ": " + src.what) + ": " + what);
}

// "__<mod>__/rest" -> "<mod dir>/rest"
std::string resolve(const Source& src, const Mods& mods, const std::string& filename) {
    if (filename.rfind("__", 0) != 0) fail(src, "file name '" + filename + "' must start with __<mod>__/");
    const std::size_t end = filename.find("__/", 2);
    if (end == std::string::npos) fail(src, "file name '" + filename + "' must start with __<mod>__/");

    const std::string mod = filename.substr(2, end - 2);
    const auto it = std::find_if(mods.begin(), mods.end(), [&](const auto& m) { return m.first == mod; });
    if (it == mods.end()) fail(src, "file name '" + filename + "' names an unknown mod '" + mod + "'");
    return (it->second / filename.substr(end + 3)).string();
}

// -----------------------------------------------------------------------------
// Values
// -----------------------------------------------------------------------------

// `v`, the value of field `key`, as a T, or fails saying what it should be
template <typename T>
T as(const Source& src, const std::string& key, const Json& v) {
    if constexpr (std::is_same_v<T, bool>) {
        if (!v.is_boolean()) fail(src, "'" + key + "' must be true or false");
        return v.get<bool>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!v.is_string()) fail(src, "'" + key + "' must be a string");
        return v.get<std::string>();
    } else if constexpr (std::is_integral_v<T>) {
        constexpr auto MAX = std::numeric_limits<T>::max();
        if (!v.is_number_unsigned() || v.get<std::uint64_t>() > MAX)
            fail(src, "'" + key + "' must be a whole number from 0 to " + std::to_string(MAX));
        return static_cast<T>(v.get<std::uint64_t>());
    } else {
        if (!v.is_number()) fail(src, "'" + key + "' must be a number");
        return v.get<T>();
    }
}

// `v` as N numbers: [x, y], [r, g, b], ...
template <int N>
glm::vec<N, float> as_vec(const Source& src, const std::string& key, const Json& v, const char* shape) {
    if (!v.is_array() || v.size() != N) fail(src, "'" + key + "' must be " + shape);
    glm::vec<N, float> out;
    for (int i = 0; i < N; i++) out[i] = as<float>(src, key, v[static_cast<std::size_t>(i)]);
    return out;
}

// [[min_x, min_y], [max_x, max_y]], tiles from the entity's feet
Area2D as_box(const Source& src, const std::string& key, const Json& v) {
    constexpr const char* SHAPE = "[[min_x, min_y], [max_x, max_y]]";
    if (!v.is_array() || v.size() != 2) fail(src, "'" + key + "' must be " + SHAPE);
    const glm::vec2 lo = as_vec<2>(src, key, v[0], SHAPE);
    const glm::vec2 hi = as_vec<2>(src, key, v[1], SHAPE);
    if (lo.x > hi.x || lo.y > hi.y) fail(src, "'" + key + "' must have its min below its max");
    return Area2D(lo.x, lo.y, hi.x, hi.y);
}

// -----------------------------------------------------------------------------
// Fields
//
// One config's fields: reads them by key and, once the config is read
// (done()), fails on any it didn't ask for, so a misspelled field is an
// error rather than a silent default.
// -----------------------------------------------------------------------------

class Fields {
public:
    Fields(const Source& src, const Json& j) : src_(src), j_(j) {
        if (!j_.is_object()) fail(src_, "must be an object");
    }

    const Source& source() const { return src_; }

    // The field's value, or null if it's left out
    const Json* find(const char* key) {
        used_.insert(key);
        const auto it = j_.find(key);
        return it == j_.end() ? nullptr : &*it;
    }

    const Json& need(const char* key) {
        const Json* v = find(key);
        if (!v) fail(src_, std::string("needs '") + key + "'");
        return *v;
    }

    // Sets `out` if the field is there; leaves the default otherwise
    template <typename T>
    void get(const char* key, T& out) {
        if (const Json* v = find(key)) out = as<T>(src_, key, *v);
    }

    template <typename T>
    T get(const char* key) { return as<T>(src_, key, need(key)); }

    void get_box(const char* key, Area2D& out) {
        if (const Json* v = find(key)) out = as_box(src_, key, *v);
    }

    std::optional<glm::vec2> pivot() {
        const Json* v = find("pivot");
        if (!v) return std::nullopt;
        return as_vec<2>(src_, "pivot", *v, "[x, y]");
    }

    void done() const {
        for (const auto& [key, _] : j_.items()) {
            if (!used_.contains(key)) fail(src_, "unknown field '" + key + "'");
        }
    }

private:
    Source                src_;
    const Json&           j_;
    std::set<std::string> used_;
};

// -----------------------------------------------------------------------------
// Lists
// -----------------------------------------------------------------------------

// Adds or replaces the config named `p.name`
template <typename T>
void put(std::vector<T>& list, T p) {
    const auto it = std::find_if(list.begin(), list.end(), [&](const T& q) { return q.name == p.name; });
    if (it == list.end()) list.push_back(std::move(p));
    else                  *it = std::move(p);
}

template <typename T>
const T* find(const std::vector<T>& list, std::string_view name) {
    const auto it = std::find_if(list.begin(), list.end(), [&](const T& p) { return p.name == name; });
    return it == list.end() ? nullptr : &*it;
}

template <typename T>
const T& need(const std::vector<T>& list, std::string_view type, std::string_view name) {
    if (const T* p = find(list, name)) return *p;
    throw std::runtime_error("Configs: no " + std::string(type) + " '" + std::string(name) + "'");
}

// -----------------------------------------------------------------------------
// Readers, one per type
// -----------------------------------------------------------------------------

UnitPrototype read_unit(Fields& f, const std::string& name) {
    UnitPrototype p{ .name = name, .sprite = {}, .projectile = {}, .config = {} };
    UnitConfig& c = p.config;
    f.get("sprite",          p.sprite);
    f.get("projectile",      p.projectile);
    f.get("max_health",      c.max_health);
    f.get("move_speed",      c.move_speed);
    f.get("attack_damage",   c.attack_damage);
    f.get("attack_interval", c.attack_interval);
    f.get("attack_windup",   c.attack_windup);
    f.get("attack_range",    c.attack_range);
    f.get("aggro_radius",    c.aggro_radius);
    f.get_box("collision_box", c.collision_bounds);

    if (c.attack_interval > 0 && c.attack_windup >= c.attack_interval)
        fail(f.source(), "'attack_windup' must be less than 'attack_interval'");
    return p;
}

AnimationConfig read_animation(Fields& f, const auto& file_of) {
    AnimationConfig a;
    a.filename = file_of(f);
    a.width    = f.get<std::uint32_t>("width");
    a.height   = f.get<std::uint32_t>("height");
    f.get("frame_count",     a.frame_count);
    f.get("direction_count", a.direction_count);
    f.get("headings",        a.headings);
    f.get("fps",             a.fps);
    f.get("loop",            a.loop);
    a.pivot = f.pivot();
    return a;
}

UnitGroupPrototype read_unit_group(Fields& f, const std::string& name) {
    UnitGroupPrototype p{ .name = name, .config = {} };
    UnitGroupConfig& c = p.config;

    if (const Json* v = f.find("formation")) {
        const std::string formation = as<std::string>(f.source(), "formation", *v);
        if (formation == "block") c.formation = UnitFormation::Block;
        else fail(f.source(), "'formation' must be \"block\"");
    }
    f.get("spacing",            c.spacing);
    f.get("columns",            c.columns);
    f.get("aspect",             c.aspect);
    f.get("ranged_in_back",     c.ranged_in_back);
    f.get("max_members",        c.max_members);
    f.get("march_speed_factor", c.march_speed_factor);
    f.get("catch_up_gain",      c.catch_up_gain);
    f.get("straggler_distance", c.straggler_distance);
    f.get("slowdown_lag",       c.slowdown_lag);
    f.get("stop_lag",           c.stop_lag);
    f.get("min_speed_factor",   c.min_speed_factor);
    f.get("scan_interval",      c.scan_interval);
    f.get("disengage_ticks",    c.disengage_ticks);
    f.get("reform_lag",         c.reform_lag);
    f.get("reform_timeout",     c.reform_timeout);

    if (c.spacing <= 0.0f)     fail(f.source(), "'spacing' must be more than 0");
    if (c.scan_interval == 0)  fail(f.source(), "'scan_interval' must be at least 1");
    return p;
}

ProjectilePrototype read_projectile(Fields& f, const std::string& name) {
    ProjectilePrototype p{ .name = name, .config = {} };
    ProjectileConfig& c = p.config;

    if (const Json* v = f.find("guidance")) {
        const std::string guidance = as<std::string>(f.source(), "guidance", *v);
        if      (guidance == "ground")   c.guidance = ProjectileGuidance::Ground;
        else if (guidance == "tracking") c.guidance = ProjectileGuidance::Tracking;
        else fail(f.source(), "'guidance' must be \"ground\" or \"tracking\"");
    }
    f.get("look",          c.look);
    f.get("speed",         c.speed);
    f.get("arc",           c.arc);
    f.get("max_height",    c.max_height);
    f.get("launch_height", c.launch_height);
    f.get("lead_target",   c.lead_target);
    f.get("hit_radius",    c.hit_radius);
    f.get("friendly_fire", c.friendly_fire);

    if (c.speed <= 0.0f) fail(f.source(), "'speed' must be more than 0");
    return p;
}

CharacterPrototype read_character(Fields& f, const std::string& name) {
    CharacterPrototype p{ .name = name, .sprite = {}, .config = {} };
    CharacterConfig& c = p.config;
    f.get("sprite",        p.sprite);
    f.get("max_health",    c.max_health);
    f.get("move_speed",    c.move_speed);
    f.get("mine_interval", c.mine_interval);
    f.get("mine_amount",   c.mine_amount);
    f.get("mine_reach",    c.mine_reach);
    f.get("build_reach",   c.build_reach);
    f.get("craft_speed",   c.craft_speed);
    f.get("attack_interval", c.attack_interval);
    f.get("attack_damage",   c.attack_damage);
    f.get("attack_reach",    c.attack_reach);
    f.get_box("collision_box", c.collision_bounds);
    if (c.craft_speed <= 0.0f) fail(f.source(), "'craft_speed' must be more than 0");
    return p;
}

BuildingPrototype read_building(Fields& f, const std::string& name, const auto& file_of) {
    BuildingPrototype p{ .name = name, .item = {}, .settlement = {}, .light = {}, .config = {}, .picture = {} };
    BuildingConfig& c = p.config;

    const std::string kind = f.get<std::string>("kind");
    if      (kind == "banner")   c.type = BuildingType::Banner;
    else if (kind == "wall")     c.type = BuildingType::Wall;
    else if (kind == "house")    c.type = BuildingType::House;
    else if (kind == "crafter")  c.type = BuildingType::Crafter;
    else if (kind == "torch")    c.type = BuildingType::Torch;
    else if (kind == "barracks") c.type = BuildingType::Barracks;
    else if (kind == "gate")     c.type = BuildingType::Gate;
    else fail(f.source(), "'kind' must be \"banner\", \"wall\", \"torch\", \"house\", \"crafter\" or \"barracks\"");

    if (const Json* v = f.find("size")) {
        const glm::vec2 size = as_vec<2>(f.source(), "size", *v, "[x, y]");
        if (size.x < 1.0f || size.y < 1.0f || size.x != std::floor(size.x) || size.y != std::floor(size.y) ||
            size.x > 16.0f || size.y > 16.0f)
            fail(f.source(), "'size' must be whole tiles from 1 to 16");
        c.footprint = { static_cast<std::int32_t>(size.x), static_cast<std::int32_t>(size.y) };
    }
    f.get("max_health", c.max_health);
    f.get("connects",   c.connects);
    f.get("solid",      c.solid);
    f.get("item",       p.item);
    f.get("settlement", p.settlement);
    f.get("light",      p.light);

    Fields pf({ f.source().file, f.source().what + ": picture" }, f.need("picture"));
    p.picture = read_animation(pf, file_of);
    pf.done();

    // "crafting": { "categories": ["crafter"], "speed": 1.0, "unlocks_for_player": true }
    if (const Json* v = f.find("crafting")) {
        Fields cf({ f.source().file, f.source().what + ": crafting" }, *v);
        const Json& cats = cf.need("categories");
        if (!cats.is_array() || cats.empty()) fail(cf.source(), "'categories' must be a list of recipe-category names");
        for (const Json& cat : cats) p.crafting_categories.push_back(as<std::string>(cf.source(), "categories", cat));
        cf.get("speed",              c.crafting_speed);
        cf.get("unlocks_for_player", c.unlocks_for_player);
        cf.done();
        if (c.crafting_speed <= 0.0f) fail(cf.source(), "'speed' must be more than 0");
    }

    if (c.max_health == 0) fail(f.source(), "'max_health' must be at least 1");
    if (c.connects && c.footprint != TileCoord{ 1, 1 })
        fail(f.source(), "only a 1 x 1 building can connect");
    if (c.connects && p.picture.frame_count != 16)
        fail(f.source(), "a connecting building's picture needs 16 frames, one per set of connected sides");
    if ((c.type == BuildingType::Banner) != !p.settlement.empty())
        fail(f.source(), "a banner, and only a banner, names its 'settlement'");
    return p;
}

SettlementPrototype read_settlement(Fields& f, const std::string& name) {
    SettlementPrototype p{ .name = name, .config = {} };
    SettlementConfig& c = p.config;
    f.get("build_radius",    c.build_radius);
    f.get("spawn_margin",    c.spawn_margin);
    f.get("min_gap",         c.min_gap);
    f.get("inventory_slots", c.inventory_slots);
    if (c.build_radius < 1 || c.build_radius > 256) fail(f.source(), "'build_radius' must be from 1 to 256");
    if (c.spawn_margin < 0) fail(f.source(), "'spawn_margin' must be 0 or more");
    if (c.min_gap < 0)      fail(f.source(), "'min_gap' must be 0 or more");
    return p;
}

RaidPrototype read_raid(Fields& f, const std::string& name) {
    RaidPrototype p{ .name = name, .unit = f.get<std::string>("unit"), .config = {} };
    RaidConfig& c = p.config;
    f.get("interval",     c.interval);
    f.get("max_alive",    c.max_alive);
    f.get("min_distance", c.min_distance);
    f.get("max_distance", c.max_distance);
    f.get("min_darkness", c.min_darkness);
    if (c.interval <= 0.0f) fail(f.source(), "'interval' must be more than 0");
    if (c.min_distance < 0.0f || c.max_distance < c.min_distance)
        fail(f.source(), "'min_distance' must be 0 or more, and 'max_distance' at least 'min_distance'");
    if (c.min_darkness < 0.0f || c.min_darkness > 1.0f) fail(f.source(), "'min_darkness' must be from 0 to 1");
    return p;
}

LightPrototype read_light(Fields& f, const std::string& name) {
    LightPrototype p{ .name = name, .light = {} };
    LightSource& l = p.light;
    if (const Json* v = f.find("color")) l.color = as_vec<3>(f.source(), "color", *v, "[r, g, b]");
    f.get("intensity",      l.intensity);
    f.get("radius",         l.radius);
    f.get("core",           l.core);
    f.get("softness",       l.softness);
    f.get("flicker",        l.flicker);
    f.get("glow_height",    l.glow_height);
    f.get("glow_radius",    l.glow_radius);
    f.get("glow_intensity", l.glow_intensity);
    return p;
}

// The name of item `id` among `out`'s items, for errors
std::string_view item_name_of(const Configs& out, ItemID id) {
    const ItemPrototype* p = out.item(id);
    return p ? std::string_view(p->name) : std::string_view("item");
}

// { "wood": 4, "stone": 1 }: item names and counts of at least 1
ItemAmountNames read_amounts(const Source& src, const std::string& key, const Json& v) {
    if (!v.is_object()) fail(src, "'" + key + "' must be an object of item names and counts");
    ItemAmountNames out;
    for (const auto& [item, n] : v.items()) {
        if (!n.is_number_unsigned() || n.get<std::uint64_t>() == 0 ||
            n.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
            fail(src, "'" + key + "' counts must be whole numbers of at least 1");
        out.emplace_back(item, n.get<std::uint32_t>());
    }
    return out;
}

// What a "recipe" config and an item's inline "recipe" share
void read_recipe_common(Fields& f, RecipePrototype& p) {
    if (const Json* v = f.find("ingredients")) p.ingredients = read_amounts(f.source(), "ingredients", *v);
    f.get("time",     p.time);
    f.get("category", p.category);
    if (!(p.time > 0.0f)) fail(f.source(), "'time' must be more than 0 seconds");
}

RecipePrototype read_recipe(Fields& f, const std::string& name) {
    RecipePrototype p{ .name = name };
    read_recipe_common(f, p);
    p.results = read_amounts(f.source(), "results", f.need("results"));
    if (p.results.empty()) fail(f.source(), "'results' must name at least one item");
    return p;
}

// An item's "recipe": makes "count" (default 1) of the item, and is named
// after it
RecipePrototype read_inline_recipe(Fields& f, const std::string& item) {
    RecipePrototype p{ .name = item };
    read_recipe_common(f, p);
    std::uint32_t n = 1;
    f.get("count", n);
    if (n == 0) fail(f.source(), "'count' must be at least 1");
    p.results.emplace_back(item, n);
    return p;
}

} // namespace

// -----------------------------------------------------------------------------
// Load
// -----------------------------------------------------------------------------

Configs Configs::load(std::string_view data_dir) {
    const fs::path root = Nezumi::BasePath::get(data_dir);
    if (!fs::is_directory(root)) throw std::runtime_error("Configs: no data folder at '" + root.string() + "'");

    // Mods: base first, then the rest by name
    Mods mods;
    for (const fs::directory_entry& e : fs::directory_iterator(root)) {
        if (e.is_directory()) mods.emplace_back(e.path().filename().string(), e.path());
    }
    std::sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) {
        if ((a.first == "base") != (b.first == "base")) return a.first == "base";
        return a.first < b.first;
    });

    // Where each config was last defined, by "type 'name'", for the
    // errors of the linking below
    std::map<std::string, std::string> defined_in;

    Configs out;
    for (const auto& [mod, mod_dir] : mods) {
        const fs::path dir = mod_dir / "configs";
        if (!fs::is_directory(dir)) continue;

        std::vector<fs::path> files;
        for (const fs::directory_entry& e : fs::recursive_directory_iterator(dir)) {
            if (e.is_regular_file() && e.path().extension() == ".json") files.push_back(e.path());
        }
        std::sort(files.begin(), files.end());

        for (const fs::path& file : files) {
            Source src{ file.string(), {} };

            Json doc;
            {
                std::ifstream in(file);
                if (!in) fail(src, "can't open");
                try { doc = Json::parse(in); }
                catch (const Json::exception& e) { fail(src, e.what()); }
            }
            if (!doc.is_array()) fail(src, "must hold an array of configs");

            for (const Json& j : doc) {
                src.what.clear();
                try {
                    if (!j.is_object()) fail(src, "each config must be an object");
                    if (!j.contains("type")) fail(src, "a config needs a 'type'");
                    if (!j.contains("name")) fail(src, "a config needs a 'name'");
                    const std::string type = as<std::string>(src, "type", j["type"]);
                    const std::string name = as<std::string>(src, "name", j["name"]);
                    if (name.empty()) fail(src, "'name' must not be empty");
                    src.what = type + " '" + name + "'";
                    defined_in[src.what] = src.file;

                    Fields f(src, j);
                    f.find("type");
                    f.find("name");

                    const auto file_of = [&](Fields& o) {
                        return resolve(o.source(), mods, o.get<std::string>("filename"));
                    };

                    if (type == "unit") {
                        put(out.units, read_unit(f, name));
                    } else if (type == "unit-sprite") {
                        UnitSpriteConfig p{ .name = name, .animations = {} };
                        const Json& anims = f.need("animations");
                        if (!anims.is_object()) fail(src, "'animations' must be an object");
                        for (const auto& [anim_name, a] : anims.items()) {
                            Fields af({ src.file, src.what + ": animation '" + anim_name + "'" }, a);
                            p.animations.emplace(anim_name, read_animation(af, file_of));
                            af.done();
                        }
                        if (p.animations.empty()) fail(src, "has no animations");
                        put(out.unit_sprites, std::move(p));
                    } else if (type == "unit-group") {
                        put(out.unit_groups, read_unit_group(f, name));
                    } else if (type == "projectile") {
                        put(out.projectiles, read_projectile(f, name));
                    } else if (type == "character") {
                        put(out.characters, read_character(f, name));
                    } else if (type == "tree") {
                        TreeConfig p{ .name = name, .item = {}, .resource = {}, .pictures = {} };
                        f.get("item",       p.item);
                        f.get("max_amount", p.resource.max_amount);
                        f.get_box("collision_box", p.resource.collision_bounds);
                        const Json& pics = f.need("pictures");
                        if (!pics.is_array()) fail(src, "'pictures' must be an array");
                        for (const Json& pic : pics) {
                            Fields pf({ src.file, src.what + ": picture" }, pic);
                            p.pictures.push_back({ .filename = file_of(pf), .pivot = pf.pivot() });
                            pf.done();
                        }
                        if (p.pictures.empty()) fail(src, "has no pictures");
                        put(out.trees, std::move(p));
                    } else if (type == "building") {
                        put(out.buildings, read_building(f, name, file_of));
                    } else if (type == "settlement") {
                        put(out.settlements, read_settlement(f, name));
                    } else if (type == "raid") {
                        put(out.raids, read_raid(f, name));
                    } else if (type == "item") {
                        ItemPrototype p{ .name = name, .id = {}, .icon = {}, .stack_size = 1, .recipe = nullptr, .inline_recipe = {} };
                        if (f.find("icon")) p.icon = resolve(src, mods, f.get<std::string>("icon"));
                        f.get("stack_size", p.stack_size);
                        if (p.stack_size == 0) fail(src, "'stack_size' must be at least 1");

                        // "recipe": { "ingredients": { "wood": 4 }, "time": 2, ... },
                        // linked once every item is read
                        if (const Json* v = f.find("recipe")) {
                            Fields rf({ src.file, src.what + ": recipe" }, *v);
                            p.inline_recipe = read_inline_recipe(rf, name);
                            rf.done();
                        }
                        put(out.items, std::move(p));
                    } else if (type == "recipe") {
                        put(out.recipes, read_recipe(f, name));
                    } else if (type == "recipe-category") {
                        RecipeCategoryPrototype p{ .name = name, .description = {}, .bit = 0 };
                        f.get("description", p.description);
                        put(out.recipe_categories, std::move(p));
                    } else if (type == "light") {
                        put(out.lights, read_light(f, name));
                    } else if (type == "tile") {
                        Fields tf({ src.file, src.what + ": texture" }, f.need("texture"));
                        TileConfig p{ .name = name, .filename = file_of(tf), .tiles = 1 };
                        tf.get("tiles", p.tiles);
                        tf.done();
                        if (p.tiles == 0) fail(src, "'tiles' must be at least 1");
                        put(out.tiles, std::move(p));
                    } else if (type == "sprite") {
                        SpriteConfig p{ .name = name, .filename = file_of(f), .pivot = f.pivot(), .border = {} };
                        if (const Json* b = f.find("border"))
                            p.border = as_vec<4>(src, "border", *b, "[left, top, right, bottom]");
                        put(out.sprites, std::move(p));
                    } else {
                        fail(src, "unknown type");
                    }
                    f.done();
                } catch (const Json::exception& e) {
                    fail(src, e.what());
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Link: now that every mod has loaded, give the simulation's configs
    // their names, ids and pointers, and check the names configs use
    // -------------------------------------------------------------------------

    const auto where = [&](std::string_view type, const std::string& name) {
        const std::string what = std::string(type) + " '" + name + "'";
        return Source{ defined_in[what], what };
    };
    const auto check_sprite = [&](std::string_view type, const std::string& name, const std::string& sprite) {
        if (!sprite.empty() && !out.find_unit_sprite(sprite))
            fail(where(type, name), "'sprite' names no unit-sprite '" + sprite + "'");
    };

    for (std::size_t i = 0; i < out.items.size(); i++) {
        if (i > std::numeric_limits<std::uint16_t>::max() - 1u) fail(where("item", out.items[i].name), "too many items");
        out.items[i].id = ItemID{ static_cast<std::uint16_t>(i) };
    }
    const std::pair<ItemID, const char*> NAMED_ITEMS[] = {
        { Items::Wood, "wood" }, { Items::Stone, "stone" }, { Items::Gold, "gold" }, { Items::Food, "food" },
    };
    for (const auto& [id, name] : NAMED_ITEMS) {
        const ItemPrototype* item = out.find_item(name);
        if (item && item->id != id) {
            fail(where("item", name), "must be item number " + std::to_string(id.value() + 1) + ": the items "
                 "start with wood, stone, gold and food, in that order (Item/Items.hpp names them by place)");
        }
    }

    for (ProjectilePrototype& p : out.projectiles) p.config.name = p.name;
    for (UnitGroupPrototype&  p : out.unit_groups) p.config.name = p.name;

    for (std::size_t i = 0; i < out.units.size(); i++) {
        UnitPrototype& p = out.units[i];
        p.config.name    = p.name;
        p.config.subtype = EntitySubtypeID{ static_cast<std::uint16_t>(i) };
        check_sprite("unit", p.name, p.sprite);
        if (!p.projectile.empty()) {
            const ProjectilePrototype* projectile = out.find_projectile(p.projectile);
            if (!projectile) fail(where("unit", p.name), "'projectile' names no projectile '" + p.projectile + "'");
            p.config.projectile = &projectile->config;
        }
    }

    for (CharacterPrototype& p : out.characters) {
        p.config.name = p.name;
        check_sprite("character", p.name, p.sprite);
    }

    for (std::size_t i = 0; i < out.trees.size(); i++) {
        TreeConfig& p = out.trees[i];
        if (i > std::numeric_limits<std::uint8_t>::max() - 1u) fail(where("tree", p.name), "too many trees");
        p.resource.name    = p.name;
        p.resource.subtype = to_subtype(ResourceType::Tree);
        p.resource.variety = ResourceVarietyID{ static_cast<std::uint8_t>(i) };
        if (!p.item.empty()) {
            const ItemPrototype* item = out.find_item(p.item);
            if (!item) fail(where("tree", p.name), "'item' names no item '" + p.item + "'");
            p.resource.item = item->id;
        }
    }

    for (SettlementPrototype& p : out.settlements) p.config.name = p.name;

    for (RaidPrototype& p : out.raids) {
        p.config.name = p.name;
        const UnitPrototype* unit = out.find_unit(p.unit);
        if (!unit) fail(where("raid", p.name), "'unit' names no unit '" + p.unit + "'");
        p.config.unit = &unit->config;
    }

    // Recipe categories: a bit each
    if (out.recipe_categories.size() > MAX_CRAFT_CATEGORIES)
        fail(where("recipe-category", out.recipe_categories[MAX_CRAFT_CATEGORIES].name),
             "too many recipe categories (at most " + std::to_string(MAX_CRAFT_CATEGORIES) + ")");
    for (std::size_t i = 0; i < out.recipe_categories.size(); i++)
        out.recipe_categories[i].bit = CraftCategoryMask{ 1 } << i;
    const auto category_bit = [&](const Source& src, const std::string& name) {
        const RecipeCategoryPrototype* c = out.find_recipe_category(name);
        if (!c) fail(src, "names no recipe-category '" + name + "'");
        return c->bit;
    };
    if (const RecipeCategoryPrototype* c = out.find_recipe_category("hand"))       out.hand_category       = c->bit;
    if (const RecipeCategoryPrototype* c = out.find_recipe_category("settlement")) out.settlement_category = c->bit;

    // Recipes: the explicit ones, then each item's inline one unless a
    // recipe config of its name replaces it
    for (const ItemPrototype& item : out.items) {
        if (item.inline_recipe && !out.find_recipe(item.name)) {
            out.recipes.push_back(*item.inline_recipe);
            defined_in["recipe '" + item.name + "'"] = defined_in["item '" + item.name + "'"];
        }
    }
    if (out.recipes.size() > std::numeric_limits<std::uint16_t>::max() - 1u)
        fail(where("recipe", out.recipes.back().name), "too many recipes");

    // Their items, back to back in recipe_amounts. Sized first, so the
    // spans into it stay put.
    std::size_t amounts = 0;
    for (const RecipePrototype& p : out.recipes) amounts += p.ingredients.size() + p.results.size();
    out.recipe_amounts.clear();
    out.recipe_amounts.reserve(amounts);

    const auto link_amounts = [&](const RecipePrototype& p, const ItemAmountNames& names, const char* key) {
        const std::size_t first = out.recipe_amounts.size();
        for (const auto& [item_name, n] : names) {
            const ItemPrototype* item = out.find_item(item_name);
            if (!item) fail(where("recipe", p.name), std::string("'") + key + "' names no item '" + item_name + "'");
            out.recipe_amounts.push_back({ item->id, n });
        }
        return std::span<const ItemAmount>(out.recipe_amounts.data() + first, names.size());
    };

    for (std::size_t i = 0; i < out.recipes.size(); i++) {
        RecipePrototype& p = out.recipes[i];
        Recipe&          r = p.recipe;
        r.name        = p.name;
        r.id          = RecipeID{ static_cast<std::uint16_t>(i) };
        r.ingredients = link_amounts(p, p.ingredients, "ingredients");
        r.results     = link_amounts(p, p.results,     "results");
        r.ticks       = static_cast<std::uint32_t>(std::max(1L, std::lround(p.time * static_cast<float>(TICKS_PER_SECOND))));
        r.category    = category_bit(where("recipe", p.name), p.category);
        for (const ItemAmount& in : r.ingredients)
            if (r.makes(in.item) > 0)
                fail(where("recipe", p.name), "can't take '" + std::string(item_name_of(out, in.item)) + "', which it makes");
    }

    // Each item's own recipe: the one named after it, else the first that
    // makes it
    for (ItemPrototype& item : out.items) {
        item.recipe = nullptr;
        if (const RecipePrototype* own = out.find_recipe(item.name); own && own->recipe.makes(item.id) > 0) {
            item.recipe = &own->recipe;
            continue;
        }
        for (const RecipePrototype& p : out.recipes) {
            if (p.recipe.makes(item.id) == 0) continue;
            item.recipe = &p.recipe;
            break;
        }
    }

    for (std::size_t i = 0; i < out.buildings.size(); i++) {
        BuildingPrototype& p = out.buildings[i];
        p.config.name  = p.name;
        p.config.index = static_cast<std::uint16_t>(i);
        if (!p.item.empty()) {
            const ItemPrototype* item = out.find_item(p.item);
            if (!item) fail(where("building", p.name), "'item' names no item '" + p.item + "'");
            p.config.item = item->id;
        }
        if (!p.settlement.empty()) {
            const SettlementPrototype* s = out.find_settlement(p.settlement);
            if (!s) fail(where("building", p.name), "'settlement' names no settlement '" + p.settlement + "'");
            p.config.settlement = &s->config;
        }
        if (!p.light.empty() && !out.find_light(p.light))
            fail(where("building", p.name), "'light' names no light '" + p.light + "'");
        p.config.crafting_categories = 0;
        for (const std::string& cat : p.crafting_categories)
            p.config.crafting_categories |= category_bit(where("building", p.name), cat);
    }

    // What inventories go by
    std::vector<ItemInfo> table;
    for (const ItemPrototype& item : out.items) table.push_back({ item.name, item.stack_size });
    set_items(std::move(table));

    return out;
}

// -----------------------------------------------------------------------------
// Lookup
// -----------------------------------------------------------------------------

const UnitPrototype*       Configs::find_unit       (std::string_view n) const { return find(units,        n); }
const UnitSpriteConfig*    Configs::find_unit_sprite(std::string_view n) const { return find(unit_sprites, n); }
const UnitGroupPrototype*  Configs::find_unit_group (std::string_view n) const { return find(unit_groups,  n); }
const ProjectilePrototype* Configs::find_projectile (std::string_view n) const { return find(projectiles,  n); }
const CharacterPrototype*  Configs::find_character  (std::string_view n) const { return find(characters,   n); }
const TreeConfig*          Configs::find_tree       (std::string_view n) const { return find(trees,        n); }
const BuildingPrototype*   Configs::find_building   (std::string_view n) const { return find(buildings,    n); }
const SettlementPrototype* Configs::find_settlement (std::string_view n) const { return find(settlements,  n); }
const RaidPrototype*       Configs::find_raid       (std::string_view n) const { return find(raids,        n); }
const ItemPrototype*       Configs::find_item       (std::string_view n) const { return find(items,        n); }
const RecipePrototype*     Configs::find_recipe     (std::string_view n) const { return find(recipes,      n); }
const RecipeCategoryPrototype* Configs::find_recipe_category(std::string_view n) const { return find(recipe_categories, n); }
const LightPrototype*      Configs::find_light      (std::string_view n) const { return find(lights,       n); }
const TileConfig*          Configs::find_tile       (std::string_view n) const { return find(tiles,        n); }
const SpriteConfig*        Configs::find_sprite     (std::string_view n) const { return find(sprites,      n); }

const UnitPrototype&      Configs::unit      (std::string_view n) const { return need(units,       "unit",       n); }
const UnitGroupPrototype& Configs::unit_group(std::string_view n) const { return need(unit_groups, "unit-group", n); }
const CharacterPrototype& Configs::character (std::string_view n) const { return need(characters,  "character",  n); }
const TreeConfig&         Configs::tree      (std::string_view n) const { return need(trees,       "tree",       n); }
const BuildingPrototype&  Configs::building  (std::string_view n) const { return need(buildings,   "building",   n); }
const LightPrototype&     Configs::light     (std::string_view n) const { return need(lights,      "light",      n); }
const RaidPrototype&      Configs::raid      (std::string_view n) const { return need(raids,       "raid",       n); }

const BuildingPrototype* Configs::building_for_item(ItemID item) const {
    if (!item) return nullptr;
    for (const BuildingPrototype& b : buildings)
        if (b.config.item == item) return &b;
    return nullptr;
}

const RecipeCategoryPrototype* Configs::recipe_category(CraftCategoryMask bit) const {
    for (const RecipeCategoryPrototype& c : recipe_categories)
        if (c.bit & bit) return &c;
    return nullptr;
}

std::vector<std::string> Configs::filenames() const {
    std::vector<std::string> out;
    for (const UnitSpriteConfig& u : unit_sprites) for (const auto& [_, a] : u.animations) out.push_back(a.filename);
    for (const TreeConfig&    t : trees)   for (const PictureConfig& p : t.pictures) out.push_back(p.filename);
    for (const BuildingPrototype& b : buildings) out.push_back(b.picture.filename);
    for (const ItemPrototype& i : items)   if (!i.icon.empty()) out.push_back(i.icon);
    for (const TileConfig&    t : tiles)   out.push_back(t.filename);
    for (const SpriteConfig&  s : sprites) out.push_back(s.filename);
    return out;
}

} // namespace Nightmourn
