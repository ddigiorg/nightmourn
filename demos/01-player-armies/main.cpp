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
// Drawing goes through Nightmourn::WorldDrawer (src/Nightmourn/Draw/); the
// demo only sets up its atlas rows and draws the HUD. The debug overlays are
// switched in the debug menu (src/Nightmourn/GUI/DebugMenu), drawn by
// Nezumi's UI.
//
// Controls: W/A/S/D move, mouse wheel zooms, R restarts the battle, tilde (~)
// opens and closes the debug menu. Press Escape (or close the window) to quit.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/World/Entities/Unit.hpp"
#include "Nezumi/World/Entities/UnitGroup.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/GUI/DebugMenu.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Player/Player.hpp"
#include <algorithm>   // erase_if
#include <array>       // array
#include <cstdint>     // int32_t, uint16_t
#include <cstdio>      // snprintf
#include <cstdlib>     // EXIT_FAILURE, EXIT_SUCCESS
#include <exception>   // exception
#include <glm/glm.hpp> // vec2, vec4
#include <iostream>    // cerr
#include <memory>      // unique_ptr, make_unique
#include <string>      // string, to_string
#include <vector>      // vector

using namespace Nezumi;
using Nightmourn::Character;
using Nightmourn::CharacterConfig;
using Nightmourn::DebugFlags;
using Nightmourn::DebugMenu;
using Nightmourn::DrawContext;
using Nightmourn::Player;
using Nightmourn::WorldDrawer;

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
    glm::vec4   color;        // group routes and slots
    float       direction;    // +1 marches right, -1 left
};

static constexpr std::array<SideStyle, SIDE_COUNT> SIDES = {{
    { "red",  { 0.0f,   0.0f }, { 1.0f, 0.25f, 0.2f, 1.0f },  1.0f },
    { "blue", { 0.0f, 128.0f }, { 0.3f, 0.5f,  1.0f, 1.0f }, -1.0f },
}};

static constexpr FactionIndex RED = 0;

constexpr Render::DrawParams DRAW_PARAMS_HUD = {
    .view  = Render::View::Screen,
    .layer = Nightmourn::DrawLayer::Hud,
};

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

        WorldDrawer::Config draw_config = {
            .chunks = {
                .atlas       = atlas_,
                .grass_dark  = AtlasPixels::GrassDark,
                .grass_light = AtlasPixels::GrassLight,
            },
            .units = {
                .atlas      = atlas_,
                .frame_size = AtlasPixels::UnitSize,
            },
            .characters = {
                .atlas      = atlas_,
                .frame_size = AtlasPixels::UnitSize,
                .row        = AtlasPixels::HeroOrigin,
            },
        };
        for (FactionIndex f = 0; f < SIDE_COUNT; f++) {
            draw_config.units.faction_rows[f]         = SIDES[f].atlas_origin;
            draw_config.unit_groups.faction_colors[f] = SIDES[f].color;
        }
        world_drawer_ = std::make_unique<WorldDrawer>(draw_config);

        // The HUD shares the UI's font (loading a font takes about a second)
        font_ = engine.ui()->font();

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
        engine.logger().info("W/A/S/D: move. Mouse wheel: zoom. R: restart. ~: debug menu. Escape: quit.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    //
    // Runs before World::tick, so commands issued here take effect this tick.
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nezumi::World& world = engine.world();

        if (engine.input().was_pressed(Nightmourn::Action::ToggleDebugMenu))
            debug_menu_.toggle();

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

        if (const Nezumi::Surface* surface = world.get_surface(surface_id_)) {
            DrawContext ctx = DrawContext::make(*r, world, *world_camera_, debug_flags_);
            world_drawer_->draw(ctx, *surface);
        }

        draw_hud_(r, engine);
        debug_menu_.draw(*engine.ui(), debug_flags_);
    }

    // -------------------------------------------------------------------------
    // OnExit
    // -------------------------------------------------------------------------

    void on_exit(Nezumi::Engine& engine) override {
        camera_controller_.reset();
        world_drawer_.reset();

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

        text += "\nW/A/S/D: move  wheel: zoom  R: restart  ~: debug menu\n";

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
            DRAW_PARAMS_HUD
        );
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    Render::Camera* world_camera_  = nullptr;
    Render::Camera* screen_camera_ = nullptr;
    Render::Atlas*  atlas_         = nullptr;
    Render::Font*   font_          = nullptr;

    std::unique_ptr<CameraController> camera_controller_ = nullptr;
    std::unique_ptr<WorldDrawer>      world_drawer_      = nullptr;
    DebugFlags                        debug_flags_       = {};
    DebugMenu                         debug_menu_        = {};

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
