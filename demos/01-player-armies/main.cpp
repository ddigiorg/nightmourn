// =============================================================================
// demos/01-player-armies/main.cpp
//
// The player walks a hero around a battlefield while red and blue unit groups
// fight each other.
//
// - The hero is a Nightmourn::Character on the red side, controlled by a
//   Nightmourn::Player: W/A/S/D move in 8 directions and the camera follows.
// - Red and blue groups (Nezumi Units and UnitGroups) attack-move across the
//   field and fight. Groups that finish their route attack the nearest enemy
//   group. Blue units treat the hero like any other red target, so walking
//   into a blue group gets the hero hurt and, eventually, killed.
// - A dead hero respawns at the red camp after a few seconds.
//
// Controls: W/A/S/D move, mouse wheel zooms, R restarts the battle. Press
// Escape (or close the window) to quit.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/World/Entities/Unit.hpp"
#include "Nezumi/World/Entities/UnitGroup.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nezumi/World/Physics/TileCoord.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Player/Player.hpp"
#include <algorithm>   // erase_if, max, min
#include <array>       // array
#include <cstdint>     // int32_t, uint16_t
#include <cstdio>      // snprintf
#include <cstdlib>     // EXIT_FAILURE, EXIT_SUCCESS
#include <exception>   // exception
#include <glm/glm.hpp> // vec2, vec4
#include <iostream>    // cerr
#include <memory>      // unique_ptr, make_unique
#include <string>      // string, to_string
#include <utility>     // swap
#include <vector>      // vector

using namespace Nezumi;
using Nightmourn::Character;
using Nightmourn::CharacterConfig;
using Nightmourn::Player;

// -----------------------------------------------------------------------------
// Demo Actions
//
// On top of the game's own (Nightmourn/Input/Actions.hpp), with ids well clear
// of them.
// -----------------------------------------------------------------------------

namespace DemoAction {
    inline constexpr ActionID Restart = 1000;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

// Screen pixels per tile at zoom 1.0
static constexpr float TILE_PIXELS = 32.0f;

// Each side spawns one group per lane, SPAWN_X tiles from the middle (see the
// Nezumi example 04-unit-group-combat)
static constexpr size_t SIDE_COUNT      = 2;
static constexpr float  SPAWN_X         = 44.0f;
static constexpr float  SPAWN_HALF_SIZE = 5.0f;

struct SideSetup {
    std::array<float, 4> lanes_y;
    size_t               lane_count;
    size_t               soldiers; // per group
    size_t               archers;  // per group
};

static constexpr std::array<SideSetup, SIDE_COUNT> SETUPS = {{
    { { -27.0f, -9.0f, 9.0f, 27.0f }, 4, 30, 12 }, // red:  4 x 42
    { { -18.0f,  0.0f, 18.0f, 0.0f }, 3, 40, 16 }, // blue: 3 x 56
}};

// Ground pre-created in [-MAP_HALF, MAP_HALF)
static constexpr glm::vec2 MAP_HALF = { 80.0f, 48.0f };

// Idle groups (route done, nothing in sight) attack the nearest enemy group
static constexpr uint16_t IDLE_RETARGET_TICKS = 120; // 2 s at 60 UPS

// The hero starts in the red camp, between two red lanes
static constexpr Position2D HERO_SPAWN    = { -SPAWN_X - 6.0f, 0.0f };
static constexpr uint16_t   RESPAWN_TICKS = 180; // 3 s at 60 UPS

// -----------------------------------------------------------------------------
// Configs
//
// Static storage, so they outlive every entity that points at them.
// -----------------------------------------------------------------------------

static constexpr CharacterConfig HERO = {
    .name       = "hero",
    .max_health = 300,
    .move_speed = 5.0f, // faster than any unit, so the hero can run away
};

static constexpr UnitConfig SOLDIER = {
    .name            = "soldier",
    .subtype         = EntitySubtypeID{ 0 },
    .max_health      = 100,
    .move_speed      = 2.5f,
    .attack_damage   = 10,
    .attack_interval = 60,
    .attack_range    = 0.75f,
    .aggro_radius    = 6.0f,
};

static constexpr UnitConfig ARCHER = {
    .name            = "archer",
    .subtype         = EntitySubtypeID{ 1 },
    .max_health      = 60,
    .move_speed      = 2.2f,
    .attack_damage   = 6,
    .attack_interval = 45,
    .attack_range    = 4.0f,
    .aggro_radius    = 7.0f,
};

static constexpr UnitGroupConfig BLOCK = {
    .name    = "block",
    .spacing = 0.9f,
    .aspect  = 2.0f,
};

// -----------------------------------------------------------------------------
// Atlas
//
// Pixel boxes in assets/graphics/atlas.png { x0, y0, x1, y1 }, y down.
// -----------------------------------------------------------------------------

namespace AtlasPixels {
    // 64x64 px ground tiles, drawn 1x1 tile
    constexpr glm::vec4 GrassDark  = {  64.0f, 256.0f, 128.0f, 320.0f };
    constexpr glm::vec4 GrassLight = {  64.0f, 320.0f, 128.0f, 384.0f };

    // 64x128 px unit frames, drawn 0.5x1 tile. One row of 5 headings per
    // color: down, down-right, right, up-right, up (left ones are mirrored).
    constexpr glm::vec2 UnitSize   = { 64.0f, 128.0f };
    constexpr glm::vec2 HeroOrigin = { 320.0f, 0.0f }; // purple row
}

struct SideStyle {
    const char* name;
    glm::vec2   atlas_origin; // first unit frame in the atlas
    float       direction;    // +1 marches right, -1 left
};

static constexpr std::array<SideStyle, SIDE_COUNT> SIDES = {{
    { "red",  { 0.0f,   0.0f },  1.0f },
    { "blue", { 0.0f, 128.0f }, -1.0f },
}};

static constexpr FactionIndex RED = 0;

constexpr glm::vec4 inset_half_texel(glm::vec4 pixel_box) {
    return pixel_box + glm::vec4{ 0.5f, 0.5f, -0.5f, -0.5f };
}

// Sprite size in tiles, anchored on the feet (the entity's position)
static constexpr glm::vec4 UNIT_SPRITE_BOUNDS = { -0.25f, -0.25f, 0.25f, 0.75f };

// Healthbar above the sprite, drawn only once hurt
static constexpr glm::vec4 HEALTHBAR_BOUNDS = { -0.3f, 0.82f, 0.3f, 0.97f };
static constexpr float     HEALTHBAR_BORDER = 0.04f;

// -----------------------------------------------------------------------------
// Draw Params
// -----------------------------------------------------------------------------

constexpr Render::DrawParams DRAW_PARAMS_GROUND     = { .view = Render::View::World,  .layer = 0 };
constexpr Render::DrawParams DRAW_PARAMS_UNITS      = { .view = Render::View::World,  .layer = 1 };
constexpr Render::DrawParams DRAW_PARAMS_HEALTHBARS = { .view = Render::View::World,  .layer = 2 };
constexpr Render::DrawParams DRAW_PARAMS_UI         = { .view = Render::View::Screen, .layer = 0 };

// -----------------------------------------------------------------------------
// PlayerArmiesDemo
// -----------------------------------------------------------------------------

class PlayerArmiesDemo final : public IApplication {
public:

    // -------------------------------------------------------------------------
    // OnInit
    // -------------------------------------------------------------------------

    void on_init(Nezumi::Engine& engine) override {
        Nezumi::World&    world    = engine.world();
        Render::Renderer* renderer = engine.renderer();

        // Input: the game's default bindings, plus the demo's own
        InputMap& input = engine.input();
        Nightmourn::bind_default_actions(input);
        input.bind(DemoAction::Restart, ScanCode::R);

        // Cameras. No pan actions: the camera follows the hero.
        world_camera_  = renderer->get_view_camera(Render::View::World );
        screen_camera_ = renderer->get_view_camera(Render::View::Screen);
        world_camera_->set_pixels_per_unit(TILE_PIXELS);

        camera_controller_ = std::make_unique<CameraController>(
            /*.events   = */engine.event_bus(),
            /*.input    = */input,
            /*.renderer = */*renderer,
            /*.camera   = */renderer->get_view(Render::View::World),
            CameraControllerConfig{
                .enable_drag_pan  = false,
                .zoom_init        = 0.75f,
                .zoom_min         = 0.125f,
                .zoom_max         = 4.0f,
                .follow_rate      = 6.0f,
                .follow_dead_zone = { 1.5f, 1.0f }, // tiles
            }
        );

        // Atlas and font
        atlas_ = renderer->create_atlas(
            Render::AtlasConfig{
                .name     = "demo-atlas",
                .filepath = "assets/graphics/atlas.png",
            }
        ).object;

        tex_grass_dark_  = atlas_->tex_box(inset_half_texel(AtlasPixels::GrassDark ));
        tex_grass_light_ = atlas_->tex_box(inset_half_texel(AtlasPixels::GrassLight));

        font_ = renderer->create_font(
            Render::FontConfig{
                .name     = "demo-font",
                .filepath = "assets/fonts/CommitMono-1.143/CommitMono-400-Regular.otf"
            }
        ).object;

        surface_id_ = world.create_surface(
            SurfaceConfig{
                .name           = "battlefield",
                .initial_chunks = ChunkSpan::span_from_box(
                    Area2D(-MAP_HALF.x, -MAP_HALF.y, MAP_HALF.x, MAP_HALF.y)),
            }
        );

        spawn_sides_(engine);
        spawn_hero_(world);

        engine.logger().info("Player Armies Demo");
        engine.logger().info("W/A/S/D: move. Mouse wheel: zoom. R: restart. Escape: quit.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    //
    // Runs before World::tick, so commands issued here take effect this tick.
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nezumi::World& world = engine.world();

        if (engine.input().was_pressed(DemoAction::Restart)) {
            clear_world_(world);
            spawn_sides_(engine);
            spawn_hero_(world);
        }

        // Respawn a dead hero after a delay
        if (!player_.character(world)) {
            if (respawn_ticks_ == 0) respawn_ticks_ = RESPAWN_TICKS;
            else if (--respawn_ticks_ == 0) spawn_hero_(world);
        }

        player_.update(world, engine.input());

        prune_groups_(world);
        retarget_idle_groups_(world);
    }

    // -------------------------------------------------------------------------
    // OnDraw
    // -------------------------------------------------------------------------

    void on_draw(Nezumi::Engine& engine) override {
        Render::Renderer* r = engine.renderer();
        const Nezumi::World& world = engine.world();

        // Follow the hero; while it's dead the camera stays where it fell
        if (auto target = player_.view_target(world)) {
            camera_controller_->set_follow_target(*target, snap_camera_);
            snap_camera_ = false;
        }
        camera_controller_->update(engine.clock().get_real_dt());

        const Nezumi::Surface* surface = world.get_surface(surface_id_);
        if (!surface) return;

        const glm::vec4 view = world_camera_->world_bounds();
        const Area2D    view_area(view.x, view.y, view.z, view.w);

        surface->each_chunk_in(ChunkSpan::span_from_box(view_area), [&](const Chunk& chunk) {
            draw_ground_  (r, chunk, view_area);
            draw_entities_(r, chunk);
        });

        draw_hud_(r, engine);
    }

    // -------------------------------------------------------------------------
    // OnExit
    // -------------------------------------------------------------------------

    void on_exit(Nezumi::Engine& engine) override {
        camera_controller_.reset();

        engine.logger().info("Ran " + std::to_string(engine.get_stats().tick_count) + " ticks.");
        engine.logger().info("Goodbye!");
    }

private:

    // -------------------------------------------------------------------------
    // Spawn
    // -------------------------------------------------------------------------

    void spawn_hero_(Nezumi::World& world) {
        const EntityHandle hero = world.create_entity<Character>(
            /*.surface  = */surface_id_,
            /*.position = */HERO_SPAWN,
            /*.config   = */HERO,
            /*.factions = */faction_bit(RED)
        );
        player_.possess(hero);
        respawn_ticks_ = 0;
        snap_camera_   = true;
    }

    // One group per lane per side, attack-moving across the field
    void spawn_sides_(Nezumi::Engine& engine) {
        Nezumi::World& world = engine.world();

        for (FactionIndex f = 0; f < SIDE_COUNT; f++) {
            const float      dir   = SIDES[f].direction;
            const SideSetup& setup = SETUPS[f];

            for (size_t lane = 0; lane < setup.lane_count; lane++) {
                const float      lane_y      = setup.lanes_y[lane];
                const Position2D center      = { -dir * SPAWN_X, lane_y };
                const Position2D destination = {  dir * SPAWN_X, lane_y };

                const EntityHandle group_handle = world.create_entity<UnitGroup>(
                    surface_id_, center, BLOCK, faction_bit(f));
                UnitGroup* group = world.get_entity_as<UnitGroup>(group_handle);

                for (size_t i = 0; i < setup.soldiers + setup.archers; i++) {
                    const UnitConfig& config = (i < setup.soldiers) ? SOLDIER : ARCHER;

                    const Position2D position = {
                        center.x + engine.random().get<float>(-SPAWN_HALF_SIZE, SPAWN_HALF_SIZE),
                        center.y + engine.random().get<float>(-SPAWN_HALF_SIZE, SPAWN_HALF_SIZE),
                    };

                    const EntityHandle handle = world.create_entity<Unit>(
                        surface_id_, position, config, faction_bit(f));
                    group->add_member(*world.get_entity_as<Unit>(handle));
                }

                group->attack_move(destination);
                groups_.push_back(group_handle);
            }
        }
    }

    void clear_world_(Nezumi::World& world) {
        std::vector<EntityHandle> doomed;
        world.each_entity([&](const Nezumi::Entity& entity) {
            if (!entity.is_pending_remove()) doomed.push_back(entity.handle);
        });
        for (EntityHandle handle : doomed) world.remove_entity(handle);

        groups_.clear();
        player_.release();
    }

    // -------------------------------------------------------------------------
    // Groups
    // -------------------------------------------------------------------------

    // Groups that finished their route and found nobody attack the nearest
    // enemy group, wherever it is
    void retarget_idle_groups_(Nezumi::World& world) {
        for_each_group_(world, [&](UnitGroup& group) {
            if (group.state() != UnitGroupState::Formed)       return;
            if (group.command().type != UnitCommandType::Stop) return;
            if (group.state_ticks() < IDLE_RETARGET_TICKS)     return;

            const UnitGroup* nearest = nullptr;
            float            best    = 0.0f;
            for_each_group_(world, [&](UnitGroup& other) {
                if (!group.is_enemy(other)) return;
                const float d2 = group.distance2(other);
                if (!nearest || d2 < best) { nearest = &other; best = d2; }
            });

            if (nearest) group.attack(nearest->handle);
        });
    }

    // Drop groups that removed themselves (no members left)
    void prune_groups_(Nezumi::World& world) {
        std::erase_if(groups_, [&](EntityHandle handle) {
            const UnitGroup* group = world.get_entity_as<UnitGroup>(handle);
            return !group || group->is_pending_remove();
        });
    }

    template <typename Fn>
    void for_each_group_(Nezumi::World& world, Fn&& fn) {
        for (EntityHandle handle : groups_) {
            UnitGroup* group = world.get_entity_as<UnitGroup>(handle);
            if (group && !group->is_pending_remove()) fn(*group);
        }
    }

    // -------------------------------------------------------------------------
    // Draw Ground
    // -------------------------------------------------------------------------

    void draw_ground_(Render::Renderer* r, const Chunk& chunk, const Area2D& view) {
        const Area2D& bounds = chunk.bounds();

        const TileCoord min = TileCoord::from_position({
            std::max(bounds.min.x, view.min.x),
            std::max(bounds.min.y, view.min.y),
        });
        const TileCoord max = TileCoord::from_position({
            std::min(bounds.max.x, view.max.x) - 0.001f,
            std::min(bounds.max.y, view.max.y) - 0.001f,
        });

        Render::DrawParams params = DRAW_PARAMS_GROUND;
        params.material = atlas_->material();

        TileCoord tile;
        for (tile.y = min.y; tile.y <= max.y; tile.y++) {
            for (tile.x = min.x; tile.x <= max.x; tile.x++) {
                r->draw_sprite(
                    Render::Sprite{
                        .pos_box = tile.bounds().to_vec4(),
                        .tex_box = ((tile.x + tile.y) & 1) ? tex_grass_light_ : tex_grass_dark_,
                    },
                    params
                );
            }
        }
    }

    // -------------------------------------------------------------------------
    // Draw Entities
    //
    // Units and the hero share the frame layout; the hero uses the purple row.
    // Sorted by y, healthbars only once hurt.
    // -------------------------------------------------------------------------

    void draw_entities_(Render::Renderer* r, const Chunk& chunk) {
        chunk.visit_active_entities([&](const Nezumi::Entity& entity) {
            if (!entity.is_alive()) return true;

            if (entity.is_type<Unit>()) {
                const Unit& unit = *entity.as_type<Unit>();
                draw_figure_(r, unit, SIDES[unit.faction_index()].atlas_origin, unit.facing());
            }
            else if (entity.is_type<Character>()) {
                const Character& character = *entity.as_type<Character>();
                draw_figure_(r, character, AtlasPixels::HeroOrigin, character.facing());
            }
            return true;
        });
    }

    void draw_figure_(Render::Renderer* r, const EntityDamageable& entity,
                      glm::vec2 atlas_origin, Position2D facing)
    {
        const glm::vec2 pos = entity.position.to_vec2();

        // Heading column: 0 down, 1 down-right, 2 right, 3 up-right, 4 up.
        // 0.383 = sin(22.5 deg) splits the circle into 8 equal sectors.
        const int dx = (facing.x > 0.383f) - (facing.x < -0.383f);
        const int dy = (facing.y > 0.383f) - (facing.y < -0.383f);
        const int column = (dx == 0) ? (dy > 0 ? 4 : 0) : (dy + 2);

        const glm::vec2 origin = atlas_origin + glm::vec2{ column * AtlasPixels::UnitSize.x, 0.0f };
        glm::vec4 tex_box = atlas_->tex_box({ origin, origin + AtlasPixels::UnitSize });
        if (dx < 0) std::swap(tex_box.x, tex_box.z); // mirror

        Render::DrawParams params = DRAW_PARAMS_UNITS;
        params.material = atlas_->material();
        params.order    = -static_cast<int32_t>(pos.y * 16.0f);

        r->draw_sprite(
            Render::Sprite{
                .pos_box = glm::vec4{ pos, pos } + UNIT_SPRITE_BOUNDS,
                .tex_box = tex_box,
            },
            params
        );

        if (!entity.is_full_health()) draw_healthbar_(r, pos, entity.health_ratio());
    }

    void draw_healthbar_(Render::Renderer* r, glm::vec2 pos, float ratio) {
        const glm::vec4 back = glm::vec4{ pos, pos } + HEALTHBAR_BOUNDS;

        glm::vec4 fill = back + glm::vec4{ HEALTHBAR_BORDER, HEALTHBAR_BORDER,
                                          -HEALTHBAR_BORDER, -HEALTHBAR_BORDER };
        fill.z = fill.x + (fill.z - fill.x) * ratio;

        r->draw_rectangle(Render::Rectangle{ .pos_box = back, .color = { 0.0f, 0.0f, 0.0f, 1.0f } },
                          DRAW_PARAMS_HEALTHBARS);
        r->draw_rectangle(Render::Rectangle{ .pos_box = fill, .color = { 1.0f, 1.0f, 1.0f, 1.0f } },
                          DRAW_PARAMS_HEALTHBARS);
    }

    // -------------------------------------------------------------------------
    // Draw HUD
    // -------------------------------------------------------------------------

    void draw_hud_(Render::Renderer* r, Nezumi::Engine& engine) {
        const Nezumi::World& world = engine.world();

        std::array<size_t, SIDE_COUNT> units = {};
        world.each_entity([&](const Nezumi::Entity& entity) {
            if (entity.is_alive() && entity.is_type<Unit>()) units[entity.faction_index()]++;
        });

        std::string text;
        char line[128];

        if (const Character* hero = player_.character(world)) {
            std::snprintf(line, sizeof line, "hero  %u / %u hp  at %.1f, %.1f\n",
                static_cast<unsigned>(hero->health), static_cast<unsigned>(hero->max_health),
                hero->position.x, hero->position.y);
        } else {
            std::snprintf(line, sizeof line, "hero  fallen, respawning in %.1f s\n",
                static_cast<float>(respawn_ticks_) / 60.0f);
        }
        text += line;

        for (size_t f = 0; f < SIDE_COUNT; f++) {
            std::snprintf(line, sizeof line, "%-5s %4zu units\n", SIDES[f].name, units[f]);
            text += line;
        }

        text += "\nW/A/S/D: move  wheel: zoom  R: restart";

        r->draw_text(
            text,
            { 8.0f, screen_camera_->get_size().y - 8.0f },
            *font_,
            Render::TextStyle {
                .size          = 16.0f,
                .color         = { 1.0f, 1.0f, 1.0f, 1.0f },
                .outline_color = { 0.0f, 0.0f, 0.0f, 1.0f },
                .outline_width = 3.0f,
                .align         = Render::TextAlign::Left,
                .valign        = Render::TextVAlign::Top,
            },
            DRAW_PARAMS_UI
        );
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    Render::Camera* world_camera_  = nullptr;
    Render::Camera* screen_camera_ = nullptr;
    Render::Atlas*  atlas_         = nullptr;
    Render::Font*   font_          = nullptr;

    glm::vec4 tex_grass_dark_  = {};
    glm::vec4 tex_grass_light_ = {};

    std::unique_ptr<CameraController> camera_controller_ = nullptr;

    Player   player_        = {};
    uint16_t respawn_ticks_ = 0;     // counting down while the hero is dead
    bool     snap_camera_   = false; // jump (not ease) to the next follow target

    SurfaceID                 surface_id_ = {};
    std::vector<EntityHandle> groups_     = {};
};

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title  = "Nightmourn: 01-player-armies";
        config.window.width  = 1280;
        config.window.height = 720;
        config.escape_exits  = true;

        Nezumi::Engine engine(config);

        PlayerArmiesDemo app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
