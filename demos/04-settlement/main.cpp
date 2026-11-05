// =============================================================================
// demos/04-settlement/main.cpp
//
// The player founds a settlement and lays out a base, seen top-down,
// through the day and into the night.
//
// - 7 x 7 chunks of meadow, forests around the edges and a few trees near
//   the hero, who starts at 0, 0 with a banner, 100 walls, 10
//   gates, 10 torches, 5 houses, 2 crafters and 2 barracks (the action
//   bar's slots 1-7), and no wood: chop the trees for it (left click one,
//   as in 02-mining).
// - Pick a building with its slot (key or click) and its ghost follows the
//   cursor, snapped to the tile grid: green where it fits, red where it
//   doesn't, with the reason by the cursor. Left click places it and spends
//   one of its item and its wood (buildings.json "cost": a wall or torch 1,
//   a gate 2, a house 4, a crafter 9, a barracks 1), from the settlement's
//   inventory first, then the hero's; hold left click to paint a wall. Q
//   (or the slot again) stops, Factorio's clearing the cursor: the item
//   never left the inventory. Only the banner can go down with the hero
//   outside the build area, and nothing further than the hero's
//   build_reach (10 tiles, characters.json) from the hero.
// - Right click a building within reach to take it down: its item and wood
//   come back (the item to the hero, the wood to the settlement first), if
//   there's room. A banner only goes once it stands alone.
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
// - Two inventories, the same InventoryMenu: the Player Inventory (B) and
//   the Settlement Inventory (V, or left click the banner), 10 x 4 each and
//   scrolling once they grow. The settlement's only opens while the hero is
//   in its build area; walking out closes it, and walking back in opens it
//   again if it was open. With both open, drag stacks between them, Ctrl +
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
// health, costs, lights, pictures) and data/base/configs/settlements.json
// (build radius, spawn margin, inventory size).
//
// Controls: W/A/S/D move, mouse wheel zooms, 1-0 slots, left click place /
// chop, Q stop placing, right click take a building down, B player
// inventory, V settlement inventory, F deposit, H hit, P spawn points, T
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
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/Theme.hpp"
#include "Nightmourn/GUI/UISheet.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Input/Picking.hpp"
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
static constexpr std::array<StartItem, 7> START_ITEMS = {{
    { "banner", 1 }, { "wall", 100 }, { "gate", 10 }, { "torch", 10 }, { "house", 5 }, { "crafter", 2 },
    { "barracks", 2 },
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
            InventoryMenu::Config{ .title = "Player Inventory", .anchor = { 0.505f, 0.66f }, .pivot = { 0.0f, 0.5f } });
        settlement_menu_ = std::make_unique<Nightmourn::InventoryMenu>(ui_sheet_, nullptr,
            InventoryMenu::Config{ .title = "Settlement Inventory", .anchor = { 0.495f, 0.66f }, .pivot = { 1.0f, 0.5f },
                                   .shortcuts = false });

        // Lighting replaces the engine's single pass
        lighting_ = std::make_unique<GameLighting>(*engine.render(), *renderer);
        day_night_.set_time(START_TIME);

        // The kit, on slots 1-7, and wood after it
        for (std::size_t i = 0; i < START_ITEMS.size(); i++) {
            const ItemPrototype* item = configs_.find_item(START_ITEMS[i].item);
            if (!item) continue;
            player_.inventory().add(item->id, START_ITEMS[i].count);
            action_bar_->set_slot(i, item->id);
        }
        action_bar_->set_slot(START_ITEMS.size(), Nightmourn::Items::Wood);

        surface_id_ = world.create_surface(SurfaceConfig{ .name = "frontier", .initial_chunks = ChunkSpan::span_from_box(MAP_AREA) });

        spawn_trees_(world);
        spawn_hero_(world, HERO_SPAWN);
        snap_camera_ = true;

        player_menu_->set_action_bar(action_bar_.get());

        engine.logger().info("Settlement Demo");
        engine.logger().info("1-7: banner, wall, gate, torch, house, crafter, barracks. Left click: place (hold for walls), chop, or hit a raider. "
                             "Q: stop placing. Right click: take a building down. B / V: player / settlement inventory. "
                             "F: deposit. H: hit. P: spawn points.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nightmourn::World& world = *world_;
        const InputMap&    input = engine.input();

        // The settlement's inventory only while the hero stands in its area
        Settlement* stock = settlement_in_reach_(world);
        menus_.set_settlement_available(stock != nullptr);
        settlement_menu_->set_inventory(stock ? &stock->inventory : nullptr);

        menus_.handle_input(engine.input());
        if (menus_.quit_requested()) engine.event_bus().dispatch(OnGameExited{});

        // A slot holding a building's item picks that building to place
        action_bar_->handle_input(engine.input());
        for (std::size_t slot : action_bar_->take_used()) {
            const auto item = action_bar_->slot(slot);
            if (!item) continue;
            const BuildingPrototype* b = configs_.building_for_item(*item);
            if (b && player_.inventory().count(*item) > 0) placer_.toggle(&b->config);
        }

        // Buildings are paid for out of the settlement's stock first
        Inventory* const pay_from[] = { stock ? &stock->inventory : nullptr, &player_.inventory() };
        const std::optional<glm::vec2> cursor = cursor_world_(engine);
        const std::optional<BuildPlacer::Builder> builder = builder_(world);
        placer_.update(world, surface_id_, player_.inventory(), pay_from, builder ? &*builder : nullptr,
                       faction_bit(PLAYER_FACTION), cursor, input);

        // Clicking the banner opens the settlement's inventory
        if (!placer_.active() && input.was_pressed(Action::Mine)) {
            const Building* b = hovered_building_(engine);
            if (b && b->is(BuildingType::Banner) && stock && b->settlement_id == stock->id()) menus_.toggle_settlement();
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

        // The settlement may have fallen since the tick
        Settlement* stock = settlement_in_reach_(*world_);
        menus_.set_settlement_available(stock != nullptr);
        settlement_menu_->set_inventory(stock ? &stock->inventory : nullptr);

        const Nightmourn::Inventory& inventory = player_.inventory();
        action_bar_->draw(*engine.ui(), engine.input(), menus_, player_.character(world), &inventory);
        menus_.draw(*engine.ui(), debug_flags_, player_menu_.get(), settlement_menu_.get());
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

    // The player's settlement if the hero stands in its build area (where
    // its inventory can be reached), else null
    Settlement* settlement_in_reach_(Nightmourn::World& world) const {
        Settlement*      s    = world.settlement_of(faction_bit(PLAYER_FACTION));
        const Character* hero = player_.character(world);
        if (!s || !hero || !s->build_area().contains(TileCoord::from_position(hero->position))) return nullptr;
        return s;
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
    // of it or nothing: with no room, it stays up. A banner only goes once
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

        Settlement* stock      = settlement_in_reach_(world);
        Inventory*  stock_inv  = stock ? &stock->inventory : nullptr;
        Inventory&  carried    = player_.inventory();
        Inventory* const item_to[] = { &carried, stock_inv };
        Inventory* const cost_to[] = { stock_inv, &carried };

        // Tried on copies, so a refund that doesn't all fit changes nothing
        Inventory carried_before = carried;
        std::optional<Inventory> stock_before = stock_inv ? std::optional(*stock_inv) : std::nullopt;
        const bool item_ok = !b->config().item ||
                             give(std::array{ ItemAmount{ b->config().item, 1 } }, item_to);
        if (!item_ok || !give(b->config().cost, cost_to)) {
            carried = std::move(carried_before);
            if (stock_inv) *stock_inv = std::move(*stock_before);
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
                      menus_.settlement_available() ? "   (in the settlement: V opens its inventory)" : "");
        text += line;

        if (placer_.active()) {
            const BuildingConfig&  c = *placer_.building();
            std::snprintf(line, sizeof line, "placing %.*s (%u left)", static_cast<int>(c.name.size()), c.name.data(),
                          static_cast<unsigned>(player_.inventory().count(c.item)));
            text += line;
            for (const ItemAmount& cost : c.cost) {
                const std::string_view item = item_name(cost.item);
                std::snprintf(line, sizeof line, "  costs %u %.*s", static_cast<unsigned>(cost.count),
                              static_cast<int>(item.size()), item.data());
                text += line;
            }
            text += "  (Q: stop)\n";
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

        text += "\n1-7: pick a building  click: place (hold: wall)  Q: stop  right click: take down  P: spawn points\n"
                "click a tree: chop  click an enemy: attack  B / V: inventories  F: deposit  T: time speed  N: midnight  M: noon\n";

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
                case BuildPlacer::Blocker::Cost: {
                    const ItemAmount       need = *placer_.shortfall();
                    const std::string_view item = item_name(need.item);
                    char line[64];
                    std::snprintf(line, sizeof line, "needs %u more %.*s", static_cast<unsigned>(need.count),
                                  static_cast<int>(item.size()), item.data());
                    why = line;
                    break;
                }
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
    std::unique_ptr<Nightmourn::InventoryMenu> settlement_menu_   = nullptr;
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
