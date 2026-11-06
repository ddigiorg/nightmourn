// =============================================================================
// demos/04-settlement/main.cpp
//
// The player founds a settlement and lays out a base, seen top-down,
// through the day and into the night.
//
// - 7 x 7 chunks of meadow, forests around the edges and a few trees near
//   the hero, who starts at 0, 0 with only a banner (the action bar's slot
//   1). Chop trees for wood (left click one, as in 02-mining), and craft
//   from it in the Crafting menu (C): click an item to queue one, right click
//   for five, Shift + click for all you can afford. Its "recipe" (items.json:
//   a wall, torch or barracks 1 wood, a gate 2, a house 4, a workshop 9, a
//   banner 5) is paid when queued, from the hero's inventory first, then
//   the settlement's while in reach, and each craft takes its time (0.5 to
//   3 s). The queue shows over the action bar's left end, the one being made
//   shaded for its time left; click an icon to cancel one (right click: 5)
//   and get its wood back. Torches and banners are made anywhere; buildings
//   only inside the settlement, and wooden arrows only in a settlement with
//   a workshop. Walking out cancels and refunds those. With no room for
//   what's made, the craft waits, red, until there is.
// - The banner and workshops also craft for the settlement on their own,
//   from its inventory into it: the banner torches and buildings, workshops
//   arrows too. What they make is set on the Settlement menu's Crafting tab
//   (left click a workshop, or V and the tab), a row per item: drag the
//   slider's two grabs or type the numbers. Min: never used as an
//   ingredient below it. Max: made while there are fewer (0: not made).
//   Priority: 0 to 255, higher first. Each machine makes one thing at a
//   time, the highest priority it can afford, taking equals in turn. An
//   icon over a working one shows what it's making, shaded for the time
//   left, red when the stores have no room for it; hold Alt to see the idle
//   ones too. Taking one down refunds its job's wood to the settlement.
// - Pick a building to place with its action bar slot (key or click), or by
//   clicking its stack in either inventory, and its ghost follows the
//   cursor, snapped to the tile grid: green where it fits, red where it
//   doesn't, with the reason by the cursor. Left click places it, free,
//   spending one of its item (the hero's first); hold left click to paint
//   a wall. Q (or the slot again) stops, Factorio's clearing the cursor:
//   the item never left the inventory. Only the banner can go down with the
//   hero outside the build area, and nothing further than the hero's
//   build_reach (10 tiles, characters.json) from the hero.
// - Right click a building within reach to take it down and get its item
//   back (to the hero, or the settlement if the hero's full). A banner only
//   goes once it stands alone.
// - Only a banner goes down first. It founds the settlement (World::
//   place_building) and its build area, a square of tiles around it outlined
//   in the player's blue; while placing anything else the area is filled, and
//   every other building has to fit inside it. Walls join up with the walls
//   next to them, and gates join walls too. Buildings are solid: the hero
//   (and any figure) slides along their footprints and around tree trunks
//   instead of walking through, except a gate, which lets its own side
//   through.
// - Night raids: after dark a red raider comes every 40 s (2 at most) from
//   outside the no-spawn area, attack-moving on the banner and breaking
//   walls in its way (data/base/configs/raids.json). Left click one to hit
//   it; the hero gets up at the banner 5 s after falling.
// - The day lasts 12 minutes and starts late in the afternoon; T speeds it
//   up (1x, 20x, paused), N jumps to midnight and M to noon. Torches light
//   the base at night, and the hero carries a small lantern.
// - Two inventories, the same InventoryMenu: the Inventory (X) and
//   the Settlement menu's Inventory tab (V, or left click the banner), each
//   with an action bar button, 10 wide and scrolling (the settlement's 80
//   slots, 6 rows in view). The settlement's opens and closes when the
//   player says; while the hero is outside the build area it says its
//   stores are out of reach. With both open and in reach, drag stacks
//   between them, Ctrl +
//   click one to send the whole stack across, or Ctrl + right click to send
//   half. Shift + click a stack of yours to put it on the action bar (only
//   the player's items go there). Rows past the fourth scroll, a row at a
//   time, by wheel or scrollbar. F sends everything chopped.
// - Enemies can't spawn in the build area or spawn_margin tiles around it.
//   P scatters 900 would-be spawn points over the map, red where a spawn is
//   allowed and none inside the no-spawn area (also outlined, dashed, with
//   show-settlement-areas in the debug menu).
// - Hover a building to see its name and health. H hits it for 50, to see
//   buildings take damage and fall.
//
// Tuning is data: data/base/configs/entity/buildings.json (footprints,
// health, lights, pictures), data/base/configs/items.json (recipes) and data/base/configs/settlements.json
// (build radius, spawn margin, inventory size).
//
// Controls: W/A/S/D move, mouse wheel zooms, 1-0 slots, left click place /
// chop, Q stop placing, right click take a building down, B player
// inventory, V settlement (inventory and crafting tabs), Alt crafting
// icons, F deposit, H hit, P spawn points, T
// time speed, N midnight, M noon, tilde (~) debug menu, Escape main menu.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Core/Events/OnGameExited.hpp"
#include "Nezumi/Core/Random.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/Input/Events/OnMouseMotion.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Data/Configs.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nightmourn/Draw/SettlementDrawer.hpp"
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/Draw/WorldSheets.hpp"
#include "Nightmourn/Events/OnEntityMined.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/GUI/CraftOverlay.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/ItemsMenu.hpp"
#include "Nightmourn/GUI/SettlementMenu.hpp"
#include "Nightmourn/GUI/Theme.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Input/Picking.hpp"
#include "Nightmourn/Crafting/CraftQueue.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Lighting/BuildingLights.hpp"
#include "Nightmourn/Lighting/DayNight.hpp"
#include "Nightmourn/Lighting/GameLighting.hpp"
#include "Nightmourn/Lighting/LightDrawer.hpp"
#include "Nightmourn/Lighting/LightSource.hpp"
#include "Nightmourn/Player/BuildPlacer.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/Settlement/RaidSpawner.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/World/World.hpp"
#include <array>       // array
#include <cmath>       // hypot
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
    inline constexpr ActionID CancelBuild    = 1000;
    inline constexpr ActionID Deposit        = 1001;
    inline constexpr ActionID RemoveBuilding = 1002;
    inline constexpr ActionID HitBuilding  = 1003;
    inline constexpr ActionID SpawnPoints  = 1004;
    inline constexpr ActionID TimeSpeed    = 1005;
    inline constexpr ActionID Midnight     = 1006;
    inline constexpr ActionID Noon         = 1007;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

// Screen pixels per view unit at zoom 1.0 (see 01-player-armies)
static constexpr float TILE_PIXELS = 32.0f;

// 7 x 7 chunks around the hero's spawn (chunks -3 .. 3: tiles -96 .. 127)
static constexpr Area2D MAP_AREA = { -96.0f, -96.0f, 128.0f, 128.0f };

static constexpr Position2D HERO_SPAWN = { 0.0f, 0.0f };

// The player's side: faction 0, drawn blue
static constexpr FactionIndex PLAYER_FACTION = 0;
static constexpr glm::vec4    PLAYER_COLOR   = { 0.42f, 0.62f, 1.00f, 1.0f };

// Night raiders: faction 1, drawn red
static constexpr FactionIndex RAID_FACTION = 1;
static constexpr glm::vec4    RAID_COLOR   = { 0.90f, 0.22f, 0.18f, 1.0f };

// A fallen hero gets up again at the banner (or the spawn) after this long
static constexpr float RESPAWN_SECONDS = 5.0f;

// What the hero starts with, in action bar order
struct StartItem { const char* item; std::uint32_t count; };
static constexpr std::array<StartItem, 1> START_ITEMS = {{
    { "banner", 1 },
}};

// Time speeds T cycles through, and the time of day it starts at (0
// midnight, 0.5 noon): late afternoon, so night falls a few minutes in
static constexpr std::array<float, 3> TIME_SPEEDS = { 1.0f, 20.0f, 0.0f };
static constexpr float                START_TIME  = 0.68f;

// The hero's lantern: a "lantern" light this wide (tiles)
static constexpr float LANTERN_RADIUS = 3.5f;

// Trees: a clear meadow of this radius around the spawn with a few trees
// to chop, then forest thickening toward the map's edge
static constexpr float         CLEARING_RADIUS = 22.0f;
static constexpr std::uint32_t FOREST_TRIES    = 2600;
static constexpr std::array<Position2D, 6> NEAR_TREES = {{
    { 6.5f, 9.0f }, { -9.5f, 7.0f }, { -12.0f, -4.5f }, { 11.0f, -8.0f }, { 3.5f, -13.0f }, { 15.5f, 3.5f },
}};

// Hovered building or tree: white outline, faint while out of reach
static constexpr glm::vec4 HOVER_COLOR   = { 1.0f, 1.0f, 1.0f, 1.0f  };
static constexpr glm::vec4 HOVER_TOO_FAR = { 1.0f, 1.0f, 1.0f, 0.35f };
static constexpr float     HOVER_PIXELS  = 2.0f;
static constexpr glm::vec4 TREE_HOVER_BOUNDS = { -1.20f, -0.50f, 1.20f, 4.30f };
static constexpr glm::vec4 FIGURE_HOVER_BOUNDS = { -0.45f, -0.20f, 0.45f, 1.30f };
static constexpr glm::vec4 ENEMY_HOVER       = { 1.0f, 0.35f, 0.30f, 1.0f  };
static constexpr glm::vec4 ENEMY_TOO_FAR     = { 1.0f, 0.35f, 0.30f, 0.35f };

// H hits for this much
static constexpr std::uint16_t HIT_DAMAGE = 50;

// P: how many would-be spawn points, and how long they show (ticks)
static constexpr std::uint32_t SPAWN_SAMPLES    = 900;
static constexpr Tick          SPAWN_SHOW_TICKS = 60 * 6;

// HUD text: distance from the screen's corner, and gap below show-info
static constexpr float HUD_MARGIN = 8.0f;

// Crafting notes: how long they show, and how far up the screen
static constexpr float NOTE_SECONDS = 3.0f;
static constexpr float NOTE_Y       = 130.0f;
static constexpr float HUD_GAP    = 12.0f;

constexpr Render::DrawParams DRAW_PARAMS_HUD = {
    .view  = Render::View::Screen,
    .layer = Nightmourn::DrawLayer::Hud,
};

// -----------------------------------------------------------------------------
// SettlementDemo
// -----------------------------------------------------------------------------

class SettlementDemo final : public IApplication {
public:

    // -------------------------------------------------------------------------
    // OnInit
    // -------------------------------------------------------------------------

    void on_init(Nezumi::Engine& engine) override {
        world_ = std::make_unique<Nightmourn::World>(engine.clock(), engine.event_bus());

        Nightmourn::World&  world    = *world_;
        Render::Renderer2D* renderer = engine.renderer2d();

        InputMap& input = engine.input();
        Nightmourn::bind_default_actions(input);
        input.bind(DemoAction::CancelBuild,    ScanCode::Q);
        input.bind(DemoAction::Deposit,        ScanCode::F);
        input.bind(DemoAction::RemoveBuilding, MouseButton::Right);
        input.bind(DemoAction::HitBuilding, ScanCode::H);
        input.bind(DemoAction::SpawnPoints, ScanCode::P);
        input.bind(DemoAction::TimeSpeed,   ScanCode::T);
        input.bind(DemoAction::Midnight,    ScanCode::N);
        input.bind(DemoAction::Noon,        ScanCode::M);

        world_camera_  = renderer->get_view_camera(Render::View::World );
        screen_camera_ = renderer->get_view_camera(Render::View::Screen);
        world_camera_->set_pixels_per_unit(TILE_PIXELS);

        camera_controller_ = std::make_unique<CameraController>(
            /*.events   = */engine.event_bus(),
            /*.input    = */input,
            /*.renderer = */*renderer,
            /*.camera   = */renderer->get_view(Render::View::World),
            CameraControllerConfig{
                .enable_drag_pan = false,
                .zoom_init       = 0.8f,
                .zoom_min        = 0.2f,
                .zoom_max        = 4.0f,
                .follow_rate     = 0.0f,
            }
        );

        configs_ = Nightmourn::Configs::load();
        sheets_  = Nightmourn::WorldSheets::load(*renderer, configs_);

        WorldDrawer::Config draw_config = sheets_.drawer_config();
        draw_config.buildings.faction_colors[PLAYER_FACTION] = PLAYER_COLOR;
        draw_config.buildings.faction_colors[RAID_FACTION]   = RAID_COLOR;
        const UnitPrototype& raider = configs_.unit(configs_.raid("night").unit);
        draw_config.units.looks[RAID_FACTION][raider.config.subtype.value()] = { raider.sprite.c_str(), RAID_COLOR };
        raids_ = std::make_unique<RaidSpawner>(configs_.raid("night").config, faction_bit(RAID_FACTION), 11);
        world_drawer_ = std::make_unique<WorldDrawer>(draw_config);

        listeners_.push_back(engine.event_bus().subscribe<OnMouseMotion>(
            [this](const OnMouseMotion& e) {
                cursor_     = { e.pos_x, e.pos_y };
                has_cursor_ = true;
            }
        ));
        listeners_.push_back(engine.event_bus().subscribe<OnEntityMined>(
            [this](const OnEntityMined& e) { player_.on_entity_mined(e); }
        ));

        font_ = engine.ui()->font();
        Nightmourn::apply_ui_theme(engine.ui()->style());

        ui_sheet_       = Nightmourn::UISheet::load(*renderer, configs_);
        action_bar_     = std::make_unique<Nightmourn::ActionBar>(ui_sheet_);
        // Side by side, just either side of the screen's center
        player_menu_ = std::make_unique<Nightmourn::InventoryMenu>(ui_sheet_, player_.inventory(),
            InventoryMenu::Config{ .title = "Inventory", .anchor = { 0.505f, 0.66f }, .pivot = { 0.0f, 0.5f } });
        settlement_menu_ = std::make_unique<Nightmourn::SettlementMenu>(ui_sheet_, configs_);
        items_menu_ = std::make_unique<Nightmourn::ItemsMenu>(ui_sheet_, configs_);
        items_menu_->set_action_bar(action_bar_.get());
        craft_overlay_ = std::make_unique<Nightmourn::CraftOverlay>(ui_sheet_);

        // Lighting replaces the engine's single pass
        lighting_ = std::make_unique<GameLighting>(*engine.render(), *renderer);
        day_night_.set_time(START_TIME);

        // The kit, from slot 1 on
        for (std::size_t i = 0; i < START_ITEMS.size(); i++) {
            const ItemPrototype* item = configs_.find_item(START_ITEMS[i].item);
            if (!item) continue;
            player_.inventory().add(item->id, START_ITEMS[i].count);
            action_bar_->set_slot(i, item->id);
        }

        surface_id_ = world.create_surface(SurfaceConfig{ .name = "frontier", .initial_chunks = ChunkSpan::span_from_box(MAP_AREA) });

        spawn_trees_(world);
        spawn_hero_(world, HERO_SPAWN);
        snap_camera_ = true;

        player_menu_->set_action_bar(action_bar_.get());

        engine.logger().info("Settlement Demo");
        engine.logger().info("1: banner. C: crafting (click to craft). Left click: place (hold for walls), chop, or hit a raider. "
                             "Q: stop placing. Right click: take a building down. B: player inventory. V: settlement (inventory, crafting). "
                             "F: deposit. H: hit. P: spawn points.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nightmourn::World& world = *world_;
        const InputMap&    input = engine.input();

        Settlement* stock = sync_stock_(world);

        menus_.handle_input(engine.input());
        if (menus_.quit_requested()) engine.event_bus().dispatch(OnGameExited{});

        craft_(world, engine);

        // A slot holding a building's item picks that building to place
        action_bar_->handle_input(engine.input());
        for (std::size_t slot : action_bar_->take_used()) {
            const auto item = action_bar_->slot(slot);
            if (!item) continue;
            pick_building_(*item, world);
        }

        // A building's item comes from the hero first, then the settlement
        Inventory* const items_from[] = { &player_.inventory(), stock ? &stock->inventory : nullptr };
        const std::optional<glm::vec2> cursor = cursor_world_(engine);
        const std::optional<BuildPlacer::Builder> builder = builder_(world);
        placer_.update(world, surface_id_, items_from, builder ? &*builder : nullptr,
                       faction_bit(PLAYER_FACTION), cursor, input);

        // Clicking the banner opens the Settlement on its inventory, a
        // workshop on its crafting; again, or on the same tab, closes it
        if (!placer_.active() && input.was_pressed(Action::Mine)) {
            const Building* b = hovered_building_(engine);
            if (b && stock && b->settlement_id == stock->id()) {
                if      (b->is(BuildingType::Banner))      open_settlement_(engine, SettlementMenu::InventoryTab);
                else if (b->config().crafting_categories) open_settlement_(engine, SettlementMenu::CraftingTab);
            }
        }

        // Chopping and fighting only while not placing, so the place click
        // doesn't swing
        player_.update(world, input, projection_(), placer_.active() ? EntityHandle{} : hovered_target_(engine));

        // A fallen hero gets up at the banner after a while
        if (!player_.character(world)) {
            respawn_in_ -= engine.clock().get_fixed_dt();
            if (respawn_in_ <= 0.0f) {
                const Settlement* s = world.settlement_of(faction_bit(PLAYER_FACTION));
                const Entity*     b = s ? world.get_entity(s->banner()) : nullptr;
                spawn_hero_(world, b ? b->position + Position2D(0.0f, -1.0f) : HERO_SPAWN);
            }
        }

        if (input.was_pressed(DemoAction::Deposit))        deposit_(world);
        if (input.was_pressed(DemoAction::RemoveBuilding)) remove_building_(world, engine);
        if (input.was_pressed(DemoAction::HitBuilding)) {
            if (const Building* b = hovered_building_(engine)) world.damage_entity(b->handle, HIT_DAMAGE);
        }
        if (input.was_pressed(DemoAction::SpawnPoints)) sample_spawns_(world);

        if (input.was_pressed(DemoAction::TimeSpeed)) {
            speed_index_ = (speed_index_ + 1) % TIME_SPEEDS.size();
            day_night_.set_speed(TIME_SPEEDS[speed_index_]);
        }
        if (input.was_pressed(DemoAction::Midnight)) day_night_.set_time(0.0f);
        if (input.was_pressed(DemoAction::Noon))     day_night_.set_time(0.5f);
        day_night_.tick();
        raids_->update(world, surface_id_, day_night_.darkness(), engine.clock().get_fixed_dt());

        world.tick();
    }

    // -------------------------------------------------------------------------
    // OnDraw
    // -------------------------------------------------------------------------

    void on_draw(Nezumi::Engine& engine) override {
        Render::Renderer2D* r = engine.renderer2d();
        const Nightmourn::World& world = *world_;

        const Projection proj = projection_();
        if (proj != last_projection_) {
            last_projection_ = proj;
            snap_camera_     = true;
        }
        if (auto target = player_.view_target(world, proj)) {
            camera_controller_->set_follow_target(*target, snap_camera_);
            snap_camera_ = false;
        }
        camera_controller_->update(engine.clock().get_real_dt());
        seconds_ += engine.clock().get_real_dt();

        lighting_->update(day_night_.ambient(), debug_flags_);

        if (const Nightmourn::Surface* surface = world.get_surface(surface_id_)) {
            DrawContext ctx = DrawContext::make(*r, world, *world_camera_, proj, debug_flags_);
            ctx.overlay_view = GameLighting::OVERLAY_VIEW;

            // Territory on the ground; while placing, filled and drawn over
            // the dark so it's clear where things can go
            const bool placing = placer_.active() && placer_.building()->type != BuildingType::Banner;
            SettlementDrawStyle style;
            style.filler = &world_drawer_->buildings();
            style.layer  = placing ? DrawLayer::EntityOverlay : DrawLayer::GroundOverlay;
            style.color  = PLAYER_COLOR;
            style.fill   = placing;
            style.show_spawn_area = !spawn_points_.empty() && ctx.tick - spawn_shown_at_ <= SPAWN_SHOW_TICKS;
            draw_settlements(ctx, *surface, style);

            world_drawer_->draw(ctx, *surface);
            placer_.draw(ctx, world_drawer_->buildings(), surface_id_);
            if (!placer_.active()) draw_hover_(ctx, engine);
            draw_spawn_points_(ctx);

            // Torches, and the hero's lantern
            lights_.clear();
            append_building_lights(*surface, ctx.world_view, configs_, lights_);
            if (const Character* hero = player_.character(world)) {
                LightSource lantern = configs_.light("lantern").light;
                lantern.position = hero->position.to_vec2();
                lantern.radius   = LANTERN_RADIUS;
                lights_.push_back(lantern);
            }
            light_drawer_.draw(ctx, lights_, seconds_, day_night_.light_strength());
        }

        info_overlay_.update(engine);
        float hud_top = screen_top_() - HUD_MARGIN;
        if (debug_flags_.is_on(Nightmourn::DebugFlag::ShowInfo)) {
            const std::optional<glm::vec2> cursor = has_cursor_ ? std::optional(cursor_) : std::nullopt;
            hud_top = info_overlay_.draw(engine, *world_camera_, proj, cursor) - HUD_GAP;
        }
        draw_hud_(r, engine, hud_top);
        draw_placement_reason_(r);
        draw_note_(r);

        // The settlement may have fallen since the tick
        sync_stock_(*world_);

        // Over the world, under the action bar and menus
        if (const Settlement* home = world.settlement_of(faction_bit(PLAYER_FACTION)))
            craft_overlay_->draw(*engine.ui(), world, home->crafting, world.get_tick(), *world_camera_, proj,
                                 engine.input().is_held(Action::ShowCraftIcons));

        const Nightmourn::Inventory& inventory = player_.inventory();
        action_bar_->draw(*engine.ui(), engine.input(), menus_, player_.character(world), &inventory);
        menus_.draw(*engine.ui(), debug_flags_,
                    { .inventory = player_menu_.get(), .settlement = settlement_menu_.get(), .items = items_menu_.get() });

        // A building's stack clicked in either inventory picks it to place
        for (InventoryMenu* m : { player_menu_.get(), &settlement_menu_->inventory() })
            if (const auto item = m->take_picked()) pick_building_(*item, *world_);
    }

    // -------------------------------------------------------------------------
    // OnExit
    // -------------------------------------------------------------------------

    void on_exit(Nezumi::Engine& engine) override {
        for (EventListenerID id : listeners_) engine.event_bus().unsubscribe(id);
        camera_controller_.reset();
        world_drawer_.reset();
        lighting_.reset();
        raids_.reset();
        action_bar_.reset();
        player_menu_.reset();
        items_menu_.reset();
        craft_overlay_.reset();
        settlement_menu_.reset();
        engine.logger().info("Goodbye!");
    }

private:

    // -------------------------------------------------------------------------
    // World
    // -------------------------------------------------------------------------

    // Before the world, so it outlives every entity pointing into it
    Nightmourn::Configs configs_ = {};

    std::unique_ptr<Nightmourn::World> world_ = nullptr;

    void spawn_trees_(Nightmourn::World& world) {
        for (Position2D p : NEAR_TREES)
            world.create_entity<Resource>(surface_id_, p, configs_.tree("oak").resource);

        // Forest: denser the further from the spawn, on whole-tile spots so
        // they never straddle a building's tiles oddly
        Nezumi::Random random(7);
        const Area2D map = MAP_AREA;
        for (std::uint32_t i = 0; i < FOREST_TRIES; i++) {
            const float x = std::floor(random.get<float>(map.min.x + 1.0f, map.max.x - 2.0f)) + 0.5f;
            const float y = std::floor(random.get<float>(map.min.y + 1.0f, map.max.y - 2.0f)) + 0.5f;
            const float d = std::hypot(x, y);
            if (d < CLEARING_RADIUS) continue;
            if (random.get<float>(0.0f, 1.0f) > std::min(1.0f, (d - CLEARING_RADIUS) / 70.0f)) continue;
            const bool pine = random.get<int>(0, 2) == 0;
            world.create_entity<Resource>(surface_id_, Position2D(x, y), configs_.tree(pine ? "pine" : "oak").resource);
        }
    }

    void spawn_hero_(Nightmourn::World& world, Position2D at) {
        player_.possess(world.create_entity<Character>(
            surface_id_, at, configs_.character("hero").config, faction_bit(PLAYER_FACTION)));
        respawn_in_ = RESPAWN_SECONDS;
    }

    // Opens the Settlement menu on `tab`, or closes it if it's already
    // showing that tab
    void open_settlement_(Nezumi::Engine& engine, SettlementMenu::Tab tab) {
        if (menus_.is_open(Menu::Settlement) && settlement_menu_->tab() == tab) {
            menus_.set_open(Menu::Settlement, false);
            return;
        }
        settlement_menu_->show_tab(*engine.ui(), tab);
        menus_.set_open(Menu::Settlement, true);
    }

    // The player's settlement if the hero stands in its build area (where
    // its inventory can be reached), else null
    Settlement* settlement_in_reach_(Nightmourn::World& world) const {
        Settlement*      s    = world.settlement_of(faction_bit(PLAYER_FACTION));
        const Character* hero = player_.character(world);
        if (!s || !hero || !s->build_area().contains(TileCoord::from_position(hero->position))) return nullptr;
        return s;
    }

    // Hands the menus the settlement's stock while it's in reach (else a
    // note saying why it's not), and returns it
    Settlement* sync_stock_(Nightmourn::World& world) {
        Settlement* stock = settlement_in_reach_(world);
        settlement_menu_->inventory().set_inventory(stock ? &stock->inventory : nullptr);
        settlement_menu_->inventory().set_note(world.settlement_of(faction_bit(PLAYER_FACTION))
            ? "You're outside the settlement.\nStep back inside its borders\nto reach its stores."
            : "You have no settlement yet.\nPlace your banner to found one.");
        items_menu_->set_crafting(craft_place_(world), &player_.crafting());
        Settlement* home = world.settlement_of(faction_bit(PLAYER_FACTION));
        settlement_menu_->set_site(home ? &home->crafting : nullptr, home ? &home->inventory : nullptr, stock != nullptr);
        const Character* hero = player_.character(world);
        action_bar_->set_crafting(&player_.crafting(), hero ? hero->config().craft_speed : 1.0f);
        return stock;
    }

    // -------------------------------------------------------------------------
    // Crafting
    // -------------------------------------------------------------------------

    // Where the hero crafts from right now: their inventory, the
    // settlement's while in reach, and what can be made where they stand
    CraftPlace craft_place_(Nightmourn::World& world) {
        Settlement* stock = settlement_in_reach_(world);
        Settlement* home  = world.settlement_of(faction_bit(PLAYER_FACTION));
        return CraftPlace{
            .player     = &player_.inventory(),
            .settlement = stock ? &stock->inventory : nullptr,
            .home       = home ? &home->inventory : nullptr,
            .categories = Player::craft_categories(configs_, stock),
        };
    }

    // The Crafting menu's clicks queue crafts, the queue's clicks cancel them,
    // and the queue makes the front one: each tick, before the world's
    void craft_(Nightmourn::World& world, Nezumi::Engine& engine) {
        const CraftPlace here  = craft_place_(world);
        CraftQueue&      queue = player_.crafting();

        for (const CraftRequest& req : items_menu_->take_requests()) {
            const CraftOutcome o = queue.enqueue(*req.recipe, req.count, here);
            if (o.refusal == CraftRefusal::None || (req.count == CraftQueue::ALL && o.queued > 0)) continue;
            note_(engine, refusal_note_(o));
        }
        for (const CraftCancel& c : action_bar_->take_cancels()) queue.cancel(c.entry, c.count, here);

        // The settlement's limits, set on its Crafting tab (only while
        // inside it)
        Settlement* stock = settlement_in_reach_(world);
        for (const SettlementCraftEdit& e : settlement_menu_->take_edits())
            if (stock) e.apply(stock->crafting);

        const CraftReport r = player_.tick_crafting(world, here);
        if (r.blocked) note_(engine, "Inventory full: the craft waits for room");
        if (r.cancelled > 0) {
            char line[96];
            std::snprintf(line, sizeof line, "Left the settlement: %u craft%s cancelled and refunded",
                          static_cast<unsigned>(r.cancelled), r.cancelled == 1 ? "" : "s");
            note_(engine, line);
        }
    }

    static std::string refusal_note_(const CraftOutcome& o) {
        switch (o.refusal) {
            case CraftRefusal::None:       return {};
            case CraftRefusal::WrongPlace: return "You can't make that here";
            case CraftRefusal::NoRoom:     return "No room in your inventory";
            case CraftRefusal::CantAfford: {
                if (!o.short_of) return "Not enough to make that";
                const std::string_view n = item_name(o.short_of->item);
                return "Short " + std::to_string(o.short_of->count) + " " + std::string(n);
            }
        }
        return {};
    }

    // A line of text over the action bar for a few seconds (Factorio's
    // flying text), and in the log
    std::string note_text_  = {};
    float       note_until_ = 0.0f;

    void note_(Nezumi::Engine& engine, std::string text) {
        engine.logger().info(text);
        note_text_  = std::move(text);
        note_until_ = seconds_ + NOTE_SECONDS;
    }

    void draw_note_(Render::Renderer2D* r) const {
        if (note_text_.empty() || seconds_ > note_until_) return;
        const float     scale = screen_camera_->get_scale();
        const glm::vec2 at    = { screen_camera_->get_size().x / scale * 0.5f, NOTE_Y };
        r->draw_text(
            note_text_, at, *font_,
            Render::TextStyle {
                .size          = 18.0f,
                .color         = { 1.0f, 0.82f, 0.55f, 1.0f },
                .outline_color = { 0.0f, 0.0f, 0.0f, 1.0f },
                .outline_width = 3.0f,
                .align         = Render::TextAlign::Center,
                .valign        = Render::TextVAlign::Bottom,
            },
            DRAW_PARAMS_HUD
        );
    }

    // Starts placing the building `item` puts up, if there's one to place
    // (stops, if it's already being placed)
    void pick_building_(ItemID item, Nightmourn::World& world) {
        const BuildingPrototype* b = configs_.building_for_item(item);
        if (!b) return;
        Settlement* stock = settlement_in_reach_(world);
        Inventory* const from[] = { &player_.inventory(), stock ? &stock->inventory : nullptr };
        if (count(from, item) > 0) placer_.toggle(&b->config);
    }

    // The hero as the one building: where they stand, how far they reach,
    // and whether they're in the settlement's area. None while they're down.
    std::optional<BuildPlacer::Builder> builder_(Nightmourn::World& world) const {
        const Character* hero = player_.character(world);
        if (!hero) return std::nullopt;
        return BuildPlacer::Builder{
            .position      = hero->position.to_vec2(),
            .reach         = hero->config().build_reach,
            .in_settlement = settlement_in_reach_(world) != nullptr,
        };
    }

    // F: hand the settlement everything chopped, if the hero is in its area
    void deposit_(Nightmourn::World& world) {
        Settlement* s = settlement_in_reach_(world);
        if (!s) return;

        for (ItemID item : { Items::Wood, Items::Stone, Items::Gold, Items::Food }) {
            const std::uint32_t moved = s->inventory.add(item, player_.inventory().count(item));
            player_.inventory().remove(item, moved);
        }
    }

    // Right click: take the hovered building down and get back what it
    // cost: its item to the hero (or the settlement, if the hero's full),
    // its wood to the settlement (or the hero), both only within reach. All
    // of it or nothing: with no room, it stays up. A workshop's job under
    // way is refunded to its settlement first. A banner only goes once
    // nothing else stands in its settlement.
    void remove_building_(Nightmourn::World& world, Nezumi::Engine& engine) {
        const Building* b = hovered_building_(engine);
        if (!b || !has_any(b->factions_mask, faction_bit(PLAYER_FACTION))) return;
        const std::optional<BuildPlacer::Builder> builder = builder_(world);
        if (!builder || BuildPlacer::distance_to(builder->position, b->footprint()) > builder->reach) {
            engine.logger().info("Too far away to take it down.");
            return;
        }
        if (b->is(BuildingType::Banner)) {
            const Settlement* s = world.get_settlement(b->settlement_id);
            if (s && s->building_count() > 1) {
                engine.logger().info("Take the settlement's other buildings down first.");
                return;
            }
        }

        // A workshop's job under way: its ingredients back to the
        // settlement first, or it stays up (CraftSite::refund_job)
        if (Settlement* home = world.get_settlement(b->settlement_id)) {
            if (!home->crafting.refund_job(b->handle, home->inventory)) {
                note_(engine, "No room in the settlement's stores for its job's ingredients");
                return;
            }
        }

        // Its item back, to the hero or (full) the settlement; with no room
        // anywhere it stays up
        Settlement* stock = settlement_in_reach_(world);
        Inventory* const item_to[] = { &player_.inventory(), stock ? &stock->inventory : nullptr };
        if (b->config().item && !give(std::array{ ItemAmount{ b->config().item, 1 } }, item_to)) {
            engine.logger().info("No room to take it down.");
            return;
        }
        world.remove_entity(b->handle);
    }

    // -------------------------------------------------------------------------
    // Spawn points (P)
    //
    // Where night spawns could go: random spots over the map, kept only
    // where World::is_spawn_blocked allows. The empty square around the base
    // is the no-spawn area.
    // -------------------------------------------------------------------------

    std::vector<glm::vec2> spawn_points_    = {};
    Tick                   spawn_shown_at_ = 0;

    void sample_spawns_(Nightmourn::World& world) {
        Nezumi::Random random(world.get_tick() + 1);
        const Area2D map = MAP_AREA;
        spawn_points_.clear();
        for (std::uint32_t i = 0; i < SPAWN_SAMPLES; i++) {
            const Position2D p(random.get<float>(map.min.x, map.max.x), random.get<float>(map.min.y, map.max.y));
            if (!world.is_spawn_blocked(surface_id_, p)) spawn_points_.push_back(p.to_vec2());
        }
        spawn_shown_at_ = world.get_tick();
    }

    void draw_spawn_points_(DrawContext& ctx) const {
        if (spawn_points_.empty() || ctx.tick - spawn_shown_at_ > SPAWN_SHOW_TICKS) return;
        const Render::DrawParams params = ctx.layer(DrawLayer::EntityOverlay);
        for (glm::vec2 p : spawn_points_) {
            ctx.renderer.draw_circle(
                Render::Circle{ .position = ctx.to_view(p), .color = { 1.0f, 0.25f, 0.2f, 0.9f }, .radius = 0.9f * ctx.upright_scale() },
                params, /*.filled = */true);
        }
    }

    // -------------------------------------------------------------------------
    // Hover
    // -------------------------------------------------------------------------

    std::optional<glm::vec2> cursor_world_(Nezumi::Engine& engine) const {
        if (!has_cursor_ || engine.input().is_mouse_captured()) return std::nullopt;
        return Nightmourn::screen_to_world(*world_camera_, projection_(), cursor_);
    }

    const Building* hovered_building_(Nezumi::Engine& engine) const {
        const std::optional<glm::vec2> c = cursor_world_(engine);
        if (!c) return nullptr;
        const EntityHandle h = world_->building_at(surface_id_, TileCoord::from_position({ c->x, c->y }));
        const Building*    b = world_->get_entity_as<Building>(h);
        return b && b->is_alive() ? b : nullptr;
    }

    // The tree to chop or the enemy figure to hit under the cursor
    EntityHandle hovered_target_(Nezumi::Engine& engine) const {
        if (!has_cursor_ || engine.input().is_mouse_captured()) return {};
        const Nightmourn::Surface* surface = world_->get_surface(surface_id_);
        if (!surface) return {};
        return Nightmourn::pick_entity(*surface, *world_camera_, projection_(), cursor_,
                                       [](const Nightmourn::Entity& e) {
                                           return e.is_resource() || (e.is_type<Unit>() &&
                                                  has_any(e.factions_mask, faction_bit(RAID_FACTION)));
                                       });
    }

    void draw_hover_(DrawContext& ctx, Nezumi::Engine& engine) const {
        if (const Building* b = hovered_building_(engine)) {
            draw_outline(ctx, b->footprint().bounds(), HOVER_COLOR, HOVER_PIXELS, DrawLayer::EntityOverlay);
            return;
        }
        const Nightmourn::Entity* e = ctx.world.get_entity(hovered_target_(engine));
        if (!e || !e->is_alive()) return;
        const Character* hero = player_.character(ctx.world);
        if (e->is_resource()) {
            const bool      reach = hero && hero->can_reach(*e);
            const glm::vec4 box   = sprite_view_box(ctx.proj, e->position.to_vec2(), TREE_HOVER_BOUNDS);
            draw_view_outline(ctx, box, reach ? HOVER_COLOR : HOVER_TOO_FAR, HOVER_PIXELS, DrawLayer::EntityOverlay);
        } else {
            const bool      reach = hero && hero->can_hit(*e);
            const glm::vec4 box   = sprite_view_box(ctx.proj, e->position.to_vec2(), FIGURE_HOVER_BOUNDS);
            draw_view_outline(ctx, box, reach ? ENEMY_HOVER : ENEMY_TOO_FAR, HOVER_PIXELS, DrawLayer::EntityOverlay);
        }
    }

    // -------------------------------------------------------------------------
    // HUD
    // -------------------------------------------------------------------------

    Projection projection_() const { return DrawContext::projection(debug_flags_); }

    float screen_top_() const { return screen_camera_->get_size().y / screen_camera_->get_scale(); }

    void draw_hud_(Render::Renderer2D* r, Nezumi::Engine& engine, float top) {
        const Nightmourn::World& world = *world_;
        std::string text;
        char line[160];

        if (const Settlement* s = world.settlement_of(faction_bit(PLAYER_FACTION))) {
            const TileRect& a = s->build_area();
            std::snprintf(line, sizeof line, "settlement at %d, %d   build area %d x %d   buildings %zu\n",
                          s->center().x, s->center().y, a.width(), a.height(), s->building_count());
            text += line;
            text += "  stock:";
            bool any = false;
            for (std::size_t i = 0; i < s->inventory.size(); i++) {
                if (!s->inventory.slot(i)) continue;
                const ItemID item = s->inventory.slot(i)->item;
                bool seen = false;
                for (std::size_t j = 0; j < i; j++)
                    if (s->inventory.slot(j) && s->inventory.slot(j)->item == item) seen = true;
                if (seen) continue;
                const std::string_view n = item_name(item);
                std::snprintf(line, sizeof line, " %u %.*s", static_cast<unsigned>(s->inventory.count(item)),
                              static_cast<int>(n.size()), n.data());
                text += line;
                any = true;
            }
            text += any ? "\n" : " (empty: F inside the area deposits what you chopped)\n";
        } else {
            text += "no settlement yet: press 1 and place the banner\n";
        }

        std::snprintf(line, sizeof line, "carrying %u wood%s\n",
                      static_cast<unsigned>(player_.inventory().count(Items::Wood)),
                      settlement_in_reach_(*world_) ? "   (in the settlement: V opens its menu)" : "");
        text += line;

        if (placer_.active()) {
            const BuildingConfig&  c = *placer_.building();
            Settlement*      stock  = settlement_in_reach_(*world_);
            Inventory* const from[] = { &player_.inventory(), stock ? &stock->inventory : nullptr };
            std::snprintf(line, sizeof line, "placing %.*s (%u left)  (Q: stop)\n", static_cast<int>(c.name.size()),
                          c.name.data(), static_cast<unsigned>(count(from, c.item)));
            text += line;
        } else if (const Building* b = hovered_building_(engine)) {
            const std::string_view n = b->config().name;
            std::snprintf(line, sizeof line, "%.*s  %u / %u hp  (right click: take down, H: hit)\n",
                          static_cast<int>(n.size()), n.data(),
                          static_cast<unsigned>(b->health), static_cast<unsigned>(b->max_health));
            text += line;
        }

        char clock[16];
        day_night_.format_clock(clock, sizeof clock);
        std::snprintf(line, sizeof line, "%s  %s  x%.0f   raiders %zu\n", clock,
                      day_night_.is_night() ? "night" : "day", day_night_.get_speed(), raids_->alive());
        text += line;

        if (!player_.character(world)) {
            std::snprintf(line, sizeof line, "you fell: back up at the banner in %.0f s\n", std::ceil(respawn_in_));
            text += line;
        }

        text += "\nC: crafting (click: craft 1, right: 5, shift: all)  queue icon: cancel  slot or stack: pick a building  click: place (hold: wall)  Q: stop  right click: take down\n"
                "click a tree: chop  click an enemy: attack  Z: character  X: inventory  V: settlement  F: deposit  P: spawn points  T: time  N: midnight  M: noon\n";

        r->draw_text(
            text, { HUD_MARGIN, top }, *font_,
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

    // Why the ghost is red, next to the cursor
    void draw_placement_reason_(Render::Renderer2D* r) const {
        if (!placer_.active() || !placer_.has_cursor() || placer_.can_place() || !has_cursor_) return;
        const float     scale = screen_camera_->get_scale();
        const glm::vec2 at    = { cursor_.x / scale + 18.0f, screen_top_() - cursor_.y / scale - 18.0f };

        std::string why(placement_message(placer_.result()));
        if (is_ok(placer_.result())) {
            switch (placer_.blocker()) {
                case BuildPlacer::Blocker::None:            break;
                case BuildPlacer::Blocker::NoBuilder:       why = "no one to build it"; break;
                case BuildPlacer::Blocker::OutOfReach:      why = "out of reach"; break;
                case BuildPlacer::Blocker::NotInSettlement: why = "stand in the settlement to build"; break;
            }
        }
        r->draw_text(
            why, at, *font_,
            Render::TextStyle {
                .size          = 16.0f,
                .color         = { 1.0f, 0.55f, 0.50f, 1.0f },
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
    Render::Font*     font_          = nullptr;
    Nightmourn::WorldSheets sheets_  = {};

    std::unique_ptr<CameraController>          camera_controller_ = nullptr;
    std::unique_ptr<WorldDrawer>               world_drawer_      = nullptr;
    DebugFlags                                 debug_flags_       = {};
    GameMenus                                  menus_             = {};
    Nightmourn::UISheet                        ui_sheet_          = {};
    std::unique_ptr<Nightmourn::ActionBar>     action_bar_        = nullptr;
    std::unique_ptr<Nightmourn::InventoryMenu> player_menu_       = nullptr;
    std::unique_ptr<Nightmourn::SettlementMenu> settlement_menu_  = nullptr;
    std::unique_ptr<Nightmourn::ItemsMenu>     items_menu_        = nullptr;
    std::unique_ptr<Nightmourn::CraftOverlay>  craft_overlay_     = nullptr;
    InfoOverlay                                info_overlay_      = {};

    std::unique_ptr<RaidSpawner> raids_      = nullptr;
    float                        respawn_in_ = RESPAWN_SECONDS;

    std::vector<EventListenerID> listeners_       = {};
    glm::vec2                    cursor_          = {}; // window pixels
    bool                         has_cursor_      = false;
    Projection                   last_projection_ = {};

    Player      player_      = {};
    BuildPlacer placer_      { BuildPlacer::Config{ .place = Action::Mine, .cancel = DemoAction::CancelBuild } };

    std::unique_ptr<GameLighting> lighting_    = nullptr;
    LightDrawer                   light_drawer_ = {};
    std::vector<LightSource>      lights_       = {};
    DayNight                      day_night_;
    std::size_t                   speed_index_  = 0;
    float                         seconds_      = 0.0f;
    bool        snap_camera_ = false;

    SurfaceID surface_id_ = {};
};

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title  = "Nightmourn: 04-settlement";
        config.window.width  = 1280;
        config.window.height = 720;
        config.escape_exits  = false; // Escape opens the main menu

        Nezumi::Engine engine(config);

        SettlementDemo app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
