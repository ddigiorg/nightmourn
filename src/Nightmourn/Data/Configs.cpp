// =============================================================================
// Data/Configs.cpp
// =============================================================================

#include "Nightmourn/Data/Configs.hpp"

#include "Nezumi/Core/BasePath.hpp"
#include <algorithm>  // find_if, sort
#include <filesystem> // directory_iterator, path, recursive_directory_iterator
#include <fstream>    // ifstream
#include <nlohmann/json.hpp>
#include <stdexcept>  // runtime_error

namespace Nightmourn {

namespace {

namespace fs = std::filesystem;
using Json   = nlohmann::json;

// Where the config being read came from, for error messages
struct Source {
    std::string file;
    std::string what; // "figure 'red'"
};

[[noreturn]] void fail(const Source& src, const std::string& what) {
    throw std::runtime_error("Configs: " + src.file + (src.what.empty() ? "" : ": " + src.what) + ": " + what);
}

// "__<mod>__/rest" -> "<mod dir>/rest"
std::string resolve(const Source& src, const std::vector<std::pair<std::string, fs::path>>& mods,
                    const std::string& filename)
{
    if (filename.rfind("__", 0) != 0) fail(src, "file name '" + filename + "' must start with __<mod>__/");
    const std::size_t end = filename.find("__/", 2);
    if (end == std::string::npos) fail(src, "file name '" + filename + "' must start with __<mod>__/");

    const std::string mod = filename.substr(2, end - 2);
    const auto it = std::find_if(mods.begin(), mods.end(), [&](const auto& m) { return m.first == mod; });
    if (it == mods.end()) fail(src, "file name '" + filename + "' names an unknown mod '" + mod + "'");
    return (it->second / filename.substr(end + 3)).string();
}

std::optional<glm::vec2> read_pivot(const Source& src, const Json& j) {
    if (!j.contains("pivot")) return std::nullopt;
    const Json& p = j["pivot"];
    if (!p.is_array() || p.size() != 2) fail(src, "'pivot' must be [x, y]");
    return glm::vec2{ p[0].get<float>(), p[1].get<float>() };
}

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

} // namespace

// -----------------------------------------------------------------------------
// Load
// -----------------------------------------------------------------------------

Configs Configs::load(std::string_view data_dir) {
    const fs::path root = Nezumi::BasePath::get(data_dir);
    if (!fs::is_directory(root)) throw std::runtime_error("Configs: no data folder at '" + root.string() + "'");

    // Mods: base first, then the rest by name
    std::vector<std::pair<std::string, fs::path>> mods;
    for (const fs::directory_entry& e : fs::directory_iterator(root)) {
        if (e.is_directory()) mods.emplace_back(e.path().filename().string(), e.path());
    }
    std::sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) {
        if ((a.first == "base") != (b.first == "base")) return a.first == "base";
        return a.first < b.first;
    });

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
                    const std::string type = j.at("type").get<std::string>();
                    const std::string name = j.at("name").get<std::string>();
                    src.what = type + " '" + name + "'";
                    const auto file_of = [&](const Json& o) {
                        return resolve(src, mods, o.at("filename").get<std::string>());
                    };

                    if (type == "figure") {
                        FigureConfig p{ .name = name, .animations = {} };
                        for (const auto& [anim_name, a] : j.at("animations").items()) {
                            AnimationConfig anim;
                            anim.filename        = file_of(a);
                            anim.width           = a.at("width").get<std::uint32_t>();
                            anim.height          = a.at("height").get<std::uint32_t>();
                            anim.frame_count     = a.value("frame_count", 1u);
                            anim.direction_count = a.value("direction_count", 1u);
                            anim.headings        = a.value("headings", 0u);
                            anim.fps             = a.value("fps", 0.0f);
                            anim.loop            = a.value("loop", true);
                            anim.pivot           = read_pivot(src, a);
                            p.animations.emplace(anim_name, std::move(anim));
                        }
                        if (p.animations.empty()) fail(src, "has no animations");
                        put(out.figures, std::move(p));
                    } else if (type == "tree") {
                        TreeConfig p{ .name = name, .pictures = {} };
                        for (const Json& pic : j.at("pictures")) {
                            p.pictures.push_back({ .filename = file_of(pic), .pivot = read_pivot(src, pic) });
                        }
                        if (p.pictures.empty()) fail(src, "has no pictures");
                        put(out.trees, std::move(p));
                    } else if (type == "tile") {
                        const Json& t = j.at("texture");
                        TileConfig p{ .name = name, .filename = file_of(t), .tiles = t.value("tiles", 1u) };
                        if (p.tiles == 0) fail(src, "'tiles' must be at least 1");
                        put(out.tiles, std::move(p));
                    } else if (type == "sprite") {
                        SpriteConfig p{ .name = name, .filename = file_of(j), .pivot = read_pivot(src, j) };
                        if (j.contains("border")) {
                            const Json& b = j["border"];
                            if (!b.is_array() || b.size() != 4) fail(src, "'border' must be [left, top, right, bottom]");
                            p.border = { b[0].get<float>(), b[1].get<float>(), b[2].get<float>(), b[3].get<float>() };
                        }
                        put(out.sprites, std::move(p));
                    } else {
                        fail(src, "unknown type");
                    }
                } catch (const Json::exception& e) {
                    fail(src, e.what());
                }
            }
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// Lookup
// -----------------------------------------------------------------------------

const FigureConfig* Configs::find_figure(std::string_view name) const { return find(figures, name); }
const TreeConfig*   Configs::find_tree  (std::string_view name) const { return find(trees,   name); }
const TileConfig*   Configs::find_tile  (std::string_view name) const { return find(tiles,   name); }
const SpriteConfig* Configs::find_sprite(std::string_view name) const { return find(sprites, name); }

std::vector<std::string> Configs::filenames() const {
    std::vector<std::string> out;
    for (const FigureConfig& f : figures) for (const auto& [_, a] : f.animations) out.push_back(a.filename);
    for (const TreeConfig&   t : trees)   for (const PictureConfig& p : t.pictures) out.push_back(p.filename);
    for (const TileConfig&   t : tiles)   out.push_back(t.filename);
    for (const SpriteConfig& s : sprites) out.push_back(s.filename);
    return out;
}

} // namespace Nightmourn
