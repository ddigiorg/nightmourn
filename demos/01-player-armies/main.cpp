// =============================================================================
// demos/01-player-armies/main.cpp
//
// The player walks a hero around a battlefield while a friendly blue army and
// an enemy red army fight each other.
//
// - The hero (cyan) is a Nightmourn::Character on the blue side, controlled
//   by a Nightmourn::Player: W/A/S/D move in 8 directions and the camera
//   follows.
// - Each army is a front line of 5 soldier groups with 3 archer groups behind
//   it, every group a block of 50 (5 ranks of 10 files). The groups (Nezumi
//   Units and UnitGroups) attack-move across the field and fight. Groups that
//   finish their route attack the nearest enemy group. Red units treat the
//   hero like any other blue target, so walking into a red group gets the
//   hero hurt and, eventually, killed.
// - A dead hero respawns at the blue camp, behind the archers, after a few
//   seconds.
// - The world is drawn 2:1 isometric (use-top-down-view in the debug menu
//   switches to top-down). The sim is the same either way: only drawing,
//   the move keys and mouse picking go through the projection.
// - Forests line the top and bottom of the field, with groves behind each
//   camp and lone trees scattered between: oaks, with stands of pines.
//   Trees are Nezumi Resources (subtype ResourceType::Tree, holding wood):
//   they never tick, so they cost nothing per tick, and for now they are
//   only decoration (nothing mines them, and figures walk through them). R
//   restarts the battle but keeps the trees.
// - The tile under the mouse is outlined; clicking a figure or a tree
//   selects it.
// - The action bar (src/Nightmourn/GUI/ActionBar) runs along the bottom: the
//   hero's hp, ten item slots (the first four hold test items; drag them
//   around, or off the bar to empty a slot) and the menu buttons.
//
// Drawing goes through Nightmourn::WorldDrawer (src/Nightmourn/Draw/); the
// demo only sets up its atlas rows and draws the HUD. The debug overlays
// (show-info among them: timings, cursor and view above the HUD) are
// switched in the debug menu (src/Nightmourn/GUI/DebugMenu), drawn by
// Nezumi's UI.
//
// Controls: W/A/S/D move (screen-relative), mouse wheel zooms, left click
// selects, R restarts the battle, 1-9 and 0 use the action bar's slots,
// K / C / B / I / O open the skill, equipment, inventory, items and options
// menus, and tilde (~) the debug menu. Escape closes open menus, or opens
// the main menu, whose Quit button (or closing the window) quits.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Core/Events/OnGameExited.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/Input/Events/OnMouseButtonDown.hpp"
#include "Nezumi/Input/Events/OnMouseMotion.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/Geometry/Area2D.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nightmourn/GUI/Theme.hpp"
#include "Nightmourn/GUI/UIAtlas.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Input/Picking.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include "Nightmourn/World/World.hpp"
#include <algorithm>   // erase_if, max, min
#include <array>       // array
#include <cstdint>     // int32_t, uint16_t, uint32_t
#include <cmath>       // abs, exp
#include <cstdio>      // snprintf
#include <cstdlib>     // EXIT_FAILURE, EXIT_SUCCESS
#include <exception>   // exception
#include <glm/glm.hpp> // vec2, vec4
#include <iostream>    // cerr
#include <memory>      // unique_ptr, make_unique
#include <optional>    // optional, nullopt
#include <string>      // string, to_string
#include <vector>      // vector

using namespace Nezumi;
using namespace Nightmourn;

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

// Screen pixels per view unit at zoom 1.0: 32x32 px tiles top-down, 64x32 px
// diamonds in iso (a tile is 2 view units wide there)
static constexpr float TILE_PIXELS = 32.0f;

// Each army spawns a front line of soldier groups FRONT_X tiles from the
// middle, and a line of archer groups ARCHER_BACK tiles behind it. Every
// group is a block of BLOCK_RANKS x BLOCK_FILES units. The lanes are the
// groups' y; groups march straight across the field along them.
static constexpr size_t SIDE_COUNT  = 2;
static constexpr float  FRONT_X     = 40.0f;
static constexpr float  ARCHER_BACK = 6.0f; // a block is ~4 tiles deep

static constexpr uint16_t BLOCK_RANKS = 5;  // rows, front to back
static constexpr uint16_t BLOCK_FILES = 10; // units per rank
static constexpr size_t   BLOCK_UNITS = BLOCK_RANKS * BLOCK_FILES;

static constexpr std::array<float, 5> SOLDIER_LANES = { -20.0f, -10.0f, 0.0f, 10.0f, 20.0f };
static constexpr std::array<float, 3> ARCHER_LANES  = {  -10.0f,   0.0f, 10.0f };

// Spawned units stand up to this far off their slot, so the blocks look
// formed but not drilled
static constexpr float SPAWN_JITTER = 0.15f;

// Ground pre-created in [-MAP_HALF, MAP_HALF)
static constexpr glm::vec2 MAP_HALF = { 80.0f, 48.0f };

// Idle groups (route done, nothing in sight) attack the nearest enemy group
static constexpr uint16_t IDLE_RETARGET_TICKS = 120; // 2 s at 60 UPS

// Trees: forests along the top and bottom edges, from FOREST_EDGE tiles off
// the middle out (the lanes and blocks stay within ~25), groves behind each
// camp, and a few lone trees everywhere else. At most one tree per
// TREE_SPACING square cell, somewhere near its center. Pines grow in stands
// where a second noise field is high. A fixed seed, so every run grows the
// same forest.
static constexpr uint64_t TREE_SEED         = 7;
static constexpr float    TREE_SPACING      = 2.25f; // tiles; a tree is 2 wide
static constexpr float    TREE_JITTER       = 0.45f;
static constexpr float    FOREST_EDGE       = 29.0f;
static constexpr float    FOREST_DEPTH      = 10.0f; // tiles until it is densest
static constexpr float    LONE_TREE_DENSITY = 0.03f;
static constexpr float    PINE_STANDS       = 0.62f; // pine noise above this: pines

struct Grove { glm::vec2 center; float radius; };
static constexpr std::array<Grove, 4> GROVES = {{
    { { -70.0f, -15.0f }, 7.0f },
    { { -68.0f,  17.0f }, 5.5f },
    { {  70.0f,  14.0f }, 7.0f },
    { {  67.0f, -18.0f }, 5.0f },
}};

// The hero starts in the blue camp, behind the archers
static constexpr Position2D HERO_SPAWN    = { -FRONT_X - ARCHER_BACK - 8.0f, 0.0f };
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

static constexpr ResourceConfig OAK = {
    .name       = "oak",
    .subtype    = Nightmourn::to_subtype(ResourceType::Tree),
    .variety    = Nightmourn::to_variety(TreeVariety::Oak),
    .item       = Nightmourn::Items::Wood,
    .max_amount = 100,
};

static constexpr ResourceConfig PINE = {
    .name       = "pine",
    .subtype    = Nightmourn::to_subtype(ResourceType::Tree),
    .variety    = Nightmourn::to_variety(TreeVariety::Pine),
    .item       = Nightmourn::Items::Wood,
    .max_amount = 100,
};

static constexpr UnitGroupConfig BLOCK = {
    .name    = "block",
    .spacing = 0.9f,
    .columns = BLOCK_FILES,
};

// -----------------------------------------------------------------------------
// Atlas
//
// Pixel boxes in assets/graphics/atlas.png { x0, y0, x1, y1 }, y down.
// -----------------------------------------------------------------------------

namespace AtlasPixels {
    // Seamless 1024x1024 px meadow covering 8x8 tiles (128 px per tile),
    // drawn world-space (see ChunkDrawer). Made by tools/gen_meadow.py.
    constexpr glm::vec4     Meadow      = { 0.0f, 384.0f, 1024.0f, 1408.0f };
    constexpr std::uint32_t MeadowTiles = 8;

    // 64x128 px unit frames, drawn 0.5x1 tile. One row of 5 headings per
    // color: down, down-right, right, up-right, up (left ones are mirrored).
    constexpr glm::vec2 UnitSize   = { 64.0f, 128.0f };
    constexpr glm::vec2 HeroOrigin = { 320.0f, 128.0f }; // cyan row

    // 256x512 px tree frames (4x a unit frame each way), drawn 2x4 tiles,
    // below the meadow: two oaks, then a pine. Made by tools/gen_trees.py.
    constexpr glm::vec2 TreeSize   = { 256.0f, 512.0f };
    constexpr glm::vec2 OakOrigin  = {   0.0f, 1408.0f };
    constexpr glm::vec2 PineOrigin = { 512.0f, 1408.0f };
}

struct SideStyle {
    const char* name;
    glm::vec2   atlas_origin; // first unit frame in the atlas
    glm::vec4   color;        // group routes and slots
    float       direction;    // +1 marches right, -1 left
};

static constexpr std::array<SideStyle, SIDE_COUNT> SIDES = {{
    { "blue", { 0.0f, 128.0f }, { 0.3f, 0.5f,  1.0f, 1.0f },  1.0f },
    { "red",  { 0.0f,   0.0f }, { 1.0f, 0.25f, 0.2f, 1.0f }, -1.0f },
}};

// The hero's side
static constexpr FactionIndex BLUE = 0;

// Hovered tile outline and selection ring
static constexpr glm::vec4 HOVER_COLOR      = { 1.0f, 0.9f, 0.4f, 0.9f };
static constexpr glm::vec4 SELECTION_COLOR  = { 0.4f, 1.0f, 0.5f, 1.0f };
static constexpr float     SELECTION_RADIUS = 0.5f;  // tiles, wider than a figure

// HUD text: distance from the screen's corner, and gap below show-info
static constexpr float HUD_MARGIN = 8.0f;
static constexpr float HUD_GAP    = 12.0f;

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
        // The game owns its world; the engine only supplies the clock and bus
        world_ = std::make_unique<Nightmourn::World>(engine.clock(), engine.event_bus());

        Nightmourn::World&    world    = *world_;
        Render::Renderer2D* renderer = engine.renderer2d();

        // Input: the game's default bindings, plus the demo's own
        InputMap& input = engine.input();
        Nightmourn::bind_default_actions(input);
        input.bind(DemoAction::Restart, ScanCode::R);

        // Cameras. No pan actions: the camera follows the hero, locked on
        // with no easing or dead zone (Diablo / Path of Exile style).
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
                .follow_rate      = 0.0f, // snap to the hero every frame
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
                .grass       = AtlasPixels::Meadow,
                .grass_tiles = AtlasPixels::MeadowTiles,
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
            .resources = {
                .atlas           = atlas_,
                .tree_frame_size = AtlasPixels::TreeSize,
            },
        };
        for (FactionIndex f = 0; f < SIDE_COUNT; f++) {
            draw_config.units.faction_rows[f]         = SIDES[f].atlas_origin;
            draw_config.unit_groups.faction_colors[f] = SIDES[f].color;
        }
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Oak )] = { AtlasPixels::OakOrigin,  2 };
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Pine)] = { AtlasPixels::PineOrigin, 1 };
        world_drawer_ = std::make_unique<WorldDrawer>(draw_config);

        // Mouse: remember the cursor for hover, select on left click (not
        // when the click lands on the debug menu)
        mouse_listeners_[0] = engine.event_bus().subscribe<OnMouseMotion>(
            [this](const OnMouseMotion& e) {
                cursor_     = { e.pos_x, e.pos_y };
                has_cursor_ = true;
            }
        );
        mouse_listeners_[1] = engine.event_bus().subscribe<OnMouseButtonDown>(
            [this, &engine](const OnMouseButtonDown& e) {
                if (e.button != MouseButton::Left || !has_cursor_) return;
                if (engine.input().is_mouse_captured()) return;
                if (const Nightmourn::Surface* surface = world_->get_surface(surface_id_))
                    selected_ = Nightmourn::pick_entity(*surface, *world_camera_, projection_(), cursor_);
            }
        );

        // The HUD shares the UI's font (loading a font takes about a second),
        // and the UI wears Nightmourn's colors
        font_ = engine.ui()->font();
        Nightmourn::apply_ui_theme(engine.ui()->style());

        // Action bar, with a few test items to use and drag around
        ui_atlas_   = Nightmourn::UIAtlas::load(*renderer);
        action_bar_ = std::make_unique<Nightmourn::ActionBar>(ui_atlas_);
        action_bar_->set_slot(0, Nightmourn::Items::Wood);
        action_bar_->set_slot(1, Nightmourn::Items::Stone);
        action_bar_->set_slot(2, Nightmourn::Items::Gold);
        action_bar_->set_slot(3, Nightmourn::Items::Food);

        surface_id_ = world.create_surface(
            SurfaceConfig{
                .name           = "battlefield",
                .initial_chunks = ChunkSpan::span_from_box(
                    Area2D(-MAP_HALF.x, -MAP_HALF.y, MAP_HALF.x, MAP_HALF.y)),
            }
        );

        spawn_trees_(world);
        spawn_sides_(engine);
        spawn_hero_(world);

        engine.logger().info("Player Armies Demo");
        engine.logger().info("W/A/S/D: move. Mouse wheel: zoom. Click: select. R: restart. 1-0: slots. "
                             "K/C/B/I/O/~: menus. Escape: main menu.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    //
    // Runs before World::tick, so commands issued here take effect this tick.
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nightmourn::World& world = *world_;

        menus_.handle_input(engine.input());
        if (menus_.quit_requested()) engine.event_bus().dispatch(OnGameExited{});

        // Items do nothing yet: using one just says so
        action_bar_->handle_input(engine.input());
        for (std::size_t slot : action_bar_->take_used()) {
            if (const auto item = action_bar_->slot(slot))
                engine.logger().info("Used slot " + std::to_string(slot + 1) + ": " +
                                     std::string(Nightmourn::item_name(*item)));
        }

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

        player_.update(world, engine.input(), projection_());

        prune_groups_(world);
        retarget_idle_groups_(world);

        // Advance the simulation once, after the player has issued commands
        world.tick();
    }

    // -------------------------------------------------------------------------
    // OnDraw
    // -------------------------------------------------------------------------

    void on_draw(Nezumi::Engine& engine) override {
        Render::Renderer2D* r = engine.renderer2d();
        const Nightmourn::World& world = *world_;

        // The camera looks at view space, so switching projections moves
        // everything on screen: jump to the hero instead of sliding there
        const Projection proj = projection_();
        if (proj != last_projection_) {
            last_projection_ = proj;
            snap_camera_     = true;
        }

        // Follow the hero; while it's dead the camera stays where it fell
        if (auto target = player_.view_target(world, proj)) {
            camera_controller_->set_follow_target(*target, snap_camera_);
            snap_camera_ = false;
        }
        camera_controller_->update(engine.clock().get_real_dt());

        if (const Nightmourn::Surface* surface = world.get_surface(surface_id_)) {
            DrawContext ctx = DrawContext::make(*r, world, *world_camera_, proj, debug_flags_);
            world_drawer_->draw(ctx, *surface);
            draw_pointer_(ctx, !engine.input().is_mouse_captured());
        }

        // show-info takes the top-left corner; the HUD goes below it
        info_overlay_.update(engine);
        float hud_top = screen_top_() - HUD_MARGIN;
        if (debug_flags_.is_on(Nightmourn::DebugFlag::ShowInfo)) {
            const std::optional<glm::vec2> cursor = has_cursor_ ? std::optional(cursor_) : std::nullopt;
            hud_top = info_overlay_.draw(engine, *world_camera_, proj, cursor) - HUD_GAP;
        }

        draw_hud_(r, hud_top);
        action_bar_->draw(*engine.ui(), engine.input(), menus_, player_.character(world));
        menus_.draw(*engine.ui(), debug_flags_);
    }

    // -------------------------------------------------------------------------
    // OnExit
    // -------------------------------------------------------------------------

    void on_exit(Nezumi::Engine& engine) override {
        for (EventListenerID id : mouse_listeners_) engine.event_bus().unsubscribe(id);
        camera_controller_.reset();
        world_drawer_.reset();
        action_bar_.reset();

        engine.logger().info("Ran " + std::to_string(engine.get_stats().tick_count) + " ticks.");
        engine.logger().info("Goodbye!");
    }

private:

    // -------------------------------------------------------------------------
    // World
    // -------------------------------------------------------------------------

    std::unique_ptr<Nightmourn::World> world_ = nullptr;

    // -------------------------------------------------------------------------
    // Spawn
    // -------------------------------------------------------------------------

    void spawn_hero_(Nightmourn::World& world) {
        const EntityHandle hero = world.create_entity<Character>(
            /*.surface  = */surface_id_,
            /*.position = */HERO_SPAWN,
            /*.config   = */HERO,
            /*.factions = */faction_bit(BLUE)
        );
        player_.possess(hero);
        respawn_ticks_ = 0;
        snap_camera_   = true;
    }

    // One tree per cell at most, with a chance that grows into the edge
    // forests and the groves. The forests' inner edge is broken up by coarse
    // value noise, so it reads as a treeline rather than a ruler.
    void spawn_trees_(Nightmourn::World& world) {
        Random rng(TREE_SEED);

        // Value noise on a grid of 6-tile cells, in [0, 1]: one field for the
        // treeline, one for the pine stands
        constexpr int CELL = 6;
        constexpr int GX = static_cast<int>(2.0f * MAP_HALF.x) / CELL + 2;
        constexpr int GY = static_cast<int>(2.0f * MAP_HALF.y) / CELL + 2;
        using Grid = std::array<float, GX * GY>;
        Grid edge_grid, pine_grid;
        for (float& v : edge_grid) v = rng.get<float>(0.0f, 1.0f);
        for (float& v : pine_grid) v = rng.get<float>(0.0f, 1.0f);

        const auto noise = [&](const Grid& grid, float x, float y) {
            const float gx = (x + MAP_HALF.x) / CELL, gy = (y + MAP_HALF.y) / CELL;
            const int   ix = static_cast<int>(gx),    iy = static_cast<int>(gy);
            const float fx = gx - ix,                 fy = gy - iy;
            const auto  at = [&](int i, int j) { return grid[static_cast<size_t>(j * GX + i)]; };
            const float a  = at(ix, iy    ) + (at(ix + 1, iy    ) - at(ix, iy    )) * fx;
            const float b  = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * fx;
            return a + (b - a) * fy;
        };

        constexpr float HALF = TREE_SPACING * 0.5f;
        for (float y = -MAP_HALF.y; y + TREE_SPACING <= MAP_HALF.y; y += TREE_SPACING) {
            for (float x = -MAP_HALF.x; x + TREE_SPACING <= MAP_HALF.x; x += TREE_SPACING) {
                const glm::vec2 center = { x + HALF, y + HALF };

                // Edge forests: denser further out, ragged on the inside
                const float depth = (std::abs(center.y) - FOREST_EDGE) / FOREST_DEPTH
                                  + (noise(edge_grid, center.x, center.y) - 0.5f) * 0.8f;
                float chance = depth > 0.0f ? 0.25f + 0.5f * std::min(depth, 1.0f) : LONE_TREE_DENSITY;

                // Groves: dense at the middle, thinning out
                for (const Grove& grove : GROVES) {
                    const glm::vec2 d = (center - grove.center) / grove.radius;
                    chance = std::max(chance, 0.8f * std::exp(-glm::dot(d, d)));
                }

                if (rng.get<float>(0.0f, 1.0f) >= chance) continue;

                const Position2D position = {
                    center.x + rng.get<float>(-TREE_JITTER, TREE_JITTER),
                    center.y + rng.get<float>(-TREE_JITTER, TREE_JITTER),
                };
                const bool pine = noise(pine_grid, center.x, center.y) > PINE_STANDS;
                world.create_entity<Resource>(surface_id_, position, pine ? PINE : OAK);
            }
        }
    }

    // Per side: soldier groups in front, archer groups behind them
    void spawn_sides_(Nezumi::Engine& engine) {
        for (FactionIndex f = 0; f < SIDE_COUNT; f++) {
            for (float lane_y : SOLDIER_LANES) spawn_group_(engine, f, SOLDIER, FRONT_X,               lane_y);
            for (float lane_y : ARCHER_LANES)  spawn_group_(engine, f, ARCHER,  FRONT_X + ARCHER_BACK, lane_y);
        }
    }

    // A block of `config` units `back` tiles from the middle on side `f`,
    // attack-moving along its lane to the enemy's front line. The units
    // start in their ranks, facing the march.
    void spawn_group_(Nezumi::Engine& engine, FactionIndex f, const UnitConfig& config,
                      float back, float lane_y) {
        Nightmourn::World& world = *world_;

        const float      dir         = SIDES[f].direction;
        const Position2D center      = { -dir * back,    lane_y };
        const Position2D destination = {  dir * FRONT_X, lane_y };

        const EntityHandle group_handle = world.create_entity<UnitGroup>(
            surface_id_, center, BLOCK, faction_bit(f));
        UnitGroup* group = world.get_entity_as<UnitGroup>(group_handle);

        for (size_t i = 0; i < BLOCK_UNITS; i++) {
            // Rank 0 is the front; the block is centered on `center`
            const float rank = static_cast<float>(i / BLOCK_FILES);
            const float file = static_cast<float>(i % BLOCK_FILES);
            const float forward = ((BLOCK_RANKS - 1) * 0.5f - rank) * BLOCK.spacing;
            const float lateral = (file - (BLOCK_FILES - 1) * 0.5f) * BLOCK.spacing;

            const Position2D position = {
                center.x + dir * forward + engine.random().get<float>(-SPAWN_JITTER, SPAWN_JITTER),
                center.y +       lateral + engine.random().get<float>(-SPAWN_JITTER, SPAWN_JITTER),
            };

            const EntityHandle handle = world.create_entity<Unit>(
                surface_id_, position, config, faction_bit(f));
            group->add_member(*world.get_entity_as<Unit>(handle));
        }

        group->attack_move(destination);
        groups_.push_back(group_handle);
    }

    // Everything but the trees
    void clear_world_(Nightmourn::World& world) {
        std::vector<EntityHandle> doomed;
        world.each_entity([&](const Nightmourn::Entity& entity) {
            if (!entity.is_pending_remove() && !entity.is_type<Resource>()) doomed.push_back(entity.handle);
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
    void retarget_idle_groups_(Nightmourn::World& world) {
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
    void prune_groups_(Nightmourn::World& world) {
        std::erase_if(groups_, [&](EntityHandle handle) {
            const UnitGroup* group = world.get_entity_as<UnitGroup>(handle);
            return !group || group->is_pending_remove();
        });
    }

    template <typename Fn>
    void for_each_group_(Nightmourn::World& world, Fn&& fn) {
        for (EntityHandle handle : groups_) {
            UnitGroup* group = world.get_entity_as<UnitGroup>(handle);
            if (group && !group->is_pending_remove()) fn(*group);
        }
    }

    // -------------------------------------------------------------------------
    // Projection
    // -------------------------------------------------------------------------

    // Iso unless use-top-down-view is on. Drawing, the move keys and picking
    // all ask here, so they always agree.
    Projection projection_() const { return DrawContext::projection(debug_flags_); }

    // -------------------------------------------------------------------------
    // Draw Pointer
    //
    // The tile under the mouse (a diamond in iso) and a ring under the
    // selected figure, both on the ground below the sprites
    // -------------------------------------------------------------------------

    void draw_pointer_(DrawContext& ctx, bool show_hover) const {
        using Nightmourn::DrawLayer::GroundOverlay;

        if (has_cursor_ && show_hover) {
            const TileCoord tile = Nightmourn::tile_at(*world_camera_, ctx.proj, cursor_);
            Nightmourn::draw_outline(ctx, tile.bounds(), HOVER_COLOR, 2.0f, GroundOverlay);
        }

        if (const Nightmourn::Entity* entity = ctx.world.get_entity(selected_); entity && entity->is_alive()) {
            Nightmourn::draw_ground_circle(ctx, entity->position.to_vec2(), SELECTION_RADIUS,
                                           SELECTION_COLOR, 2.0f, GroundOverlay);
        }
    }

    // -------------------------------------------------------------------------
    // Draw HUD
    // -------------------------------------------------------------------------

    // Top edge of the screen view (points, y up). Its camera is sized in
    // pixels, get_scale() of them per point.
    float screen_top_() const { return screen_camera_->get_size().y / screen_camera_->get_scale(); }

    // `top`: screen-view y of the HUD's first line
    void draw_hud_(Render::Renderer2D* r, float top) {
        const Nightmourn::World& world = *world_;

        std::array<size_t, SIDE_COUNT> units = {};
        size_t                         trees = 0; // every resource is a tree for now
        world.each_entity([&](const Nightmourn::Entity& entity) {
            if (!entity.is_alive()) return;
            if      (entity.is_type<Unit>()) units[entity.faction_index()]++;
            else if (entity.is_type<Resource>()) trees++;
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
        std::snprintf(line, sizeof line, "trees %4zu\n", trees);
        text += line;

        const Projection proj = projection_();
        if (has_cursor_) {
            const TileCoord tile = Nightmourn::tile_at(*world_camera_, proj, cursor_);
            std::snprintf(line, sizeof line, "\ntile  %d, %d  (%s view)\n",
                static_cast<int>(tile.x), static_cast<int>(tile.y), proj.is_top_down() ? "top-down" : "iso");
            text += line;
        }

        if (const Nightmourn::Entity* entity = world.get_entity(selected_); entity && entity->is_alive()) {
            std::string_view name = "entity";
            if (entity->is_type<Unit>())      name = entity->as_type<Unit>()->config().name;
            if (entity->is_type<Character>()) name = entity->as_type<Character>()->config().name;
            if (entity->is_type<Resource>())  name = entity->as_type<Resource>()->config().name;
            std::snprintf(line, sizeof line, "selected  %.*s at %.1f, %.1f\n",
                static_cast<int>(name.size()), name.data(), entity->position.x, entity->position.y);
            text += line;

            if (entity->is_type<Resource>()) {
                const Resource*        resource = entity->as_type<Resource>();
                const std::string_view kind     = Nightmourn::item_name(resource->item);
                std::snprintf(line, sizeof line, "          %.*s %u / %u\n",
                    static_cast<int>(kind.size()), kind.data(),
                    static_cast<unsigned>(resource->amount), static_cast<unsigned>(resource->max_amount));
                text += line;
            }
        }

        text += "\nW/A/S/D: move  wheel: zoom  click: select  R: restart  1-0: slots  Esc: menu\n";

        r->draw_text(
            text,
            { HUD_MARGIN, top },
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

    Render::Camera2D* world_camera_  = nullptr;
    Render::Camera2D* screen_camera_ = nullptr;
    Render::Atlas*  atlas_         = nullptr;
    Render::Font*   font_          = nullptr;

    std::unique_ptr<CameraController> camera_controller_ = nullptr;
    std::unique_ptr<WorldDrawer>      world_drawer_      = nullptr;
    DebugFlags                        debug_flags_       = {};
    GameMenus                         menus_             = {};
    Nightmourn::UIAtlas               ui_atlas_          = {};
    std::unique_ptr<Nightmourn::ActionBar> action_bar_   = nullptr;
    InfoOverlay                       info_overlay_      = {};

    // Mouse
    std::array<EventListenerID, 2> mouse_listeners_ = {};
    glm::vec2                      cursor_          = {}; // window pixels
    bool                           has_cursor_      = false;
    EntityHandle                   selected_        = {};
    Projection                     last_projection_ = {};

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
        config.escape_exits  = false; // Escape opens the main menu

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
