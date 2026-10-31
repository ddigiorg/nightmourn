// =============================================================================
// demos/02-mining/main.cpp
//
// The player walks a hero to a tree and chops it for wood, which goes into
// the player's inventory.
//
// - Four chunks of meadow with a handful of trees (oaks and pines) around
//   the hero. Each tree holds 10 wood (a Nezumi Resource with max_amount 10).
// - The hero (a Nightmourn::Character, controlled by a Nightmourn::Player)
//   walks with W/A/S/D; the camera follows.
// - The tree under the mouse is outlined in white: solid when the hero can
//   reach it, faint when it's too far away.
// - Left click (or hold) swings at the hovered tree, once every 0.5 s
//   (CharacterConfig::mine_interval). Each swing takes 1 wood from the tree
//   (World::mine_entity) and the player's inventory gains it
//   (OnEntityMined -> Player::on_entity_mined). A "+1 wood" floats up from
//   the tree. A tree with no wood left is removed.
// - B (or the bag button on the action bar) opens the inventory: 10 x 4
//   slots, stacks showing their size in the corner. Drag stacks to rearrange
//   or merge them, or onto the action bar to make a shortcut there: the
//   slot shows how many of that item the player carries, dimmed at 0. The
//   inventory starts with some stone and gold, to have something to drag.
//
// Controls: W/A/S/D move, mouse wheel zooms, left click mines, 1-9 and 0 use
// the action bar's slots, B the inventory, K / C / I / O the other menus,
// tilde (~) the debug menu, R regrows the trees. Escape closes open menus, or
// opens the main menu, whose Quit button (or closing the window) quits.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Core/Events/OnGameExited.hpp"
#include "Nezumi/Engine.hpp"
#include "Nezumi/Input/Events/OnMouseMotion.hpp"
#include "Nezumi/World/Entities/Resource.hpp"
#include "Nezumi/World/Events/OnEntityMined.hpp"
#include "Nezumi/World/Physics/Area2D.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/GUI/ActionBar.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nightmourn/GUI/InventoryMenu.hpp"
#include "Nightmourn/GUI/Theme.hpp"
#include "Nightmourn/GUI/UIAtlas.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Input/Picking.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include <algorithm>   // erase_if
#include <array>       // array
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
using Nightmourn::Character;
using Nightmourn::CharacterConfig;
using Nightmourn::DebugFlags;
using Nightmourn::DrawContext;
using Nightmourn::GameMenus;
using Nightmourn::InfoOverlay;
using Nightmourn::Player;
using Nightmourn::ResourceType;
using Nightmourn::TreeVariety;
using Nightmourn::WorldDrawer;

// -----------------------------------------------------------------------------
// Demo Actions
//
// On top of the game's own (Nightmourn/Input/Actions.hpp), with ids well clear
// of them.
// -----------------------------------------------------------------------------

namespace DemoAction {
    inline constexpr ActionID RegrowTrees = 1000;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

// Screen pixels per view unit at zoom 1.0 (see 01-player-armies)
static constexpr float TILE_PIXELS = 32.0f;

// Ground: 2 x 2 chunks of 32 x 32 tiles, centered on the hero's spawn
static constexpr float MAP_HALF = 32.0f;

static constexpr Position2D HERO_SPAWN = { 0.0f, 0.0f };

// A few trees around the spawn, a short walk apart
struct TreeSpawn { Position2D position; bool pine; };
static constexpr std::array<TreeSpawn, 8> TREES = {{
    { {  3.5f,  4.0f }, false },
    { {  6.0f,  1.5f }, false },
    { { -4.0f,  3.5f }, true  },
    { { -6.5f, -1.0f }, false },
    { { -2.5f, -5.5f }, false },
    { {  2.0f, -6.0f }, true  },
    { {  8.5f, -4.0f }, false },
    { {  9.0f,  7.5f }, true  },
}};

// Hovered tree outline: white, faint while out of reach
static constexpr glm::vec4 HOVER_REACH   = { 1.0f, 1.0f, 1.0f, 1.0f  };
static constexpr glm::vec4 HOVER_TOO_FAR = { 1.0f, 1.0f, 1.0f, 0.35f };
static constexpr float     HOVER_PIXELS  = 2.0f;

// The outline hugs the trunk and crown rather than the whole frame (a tree
// sprite has shadow and air around it): TREE_SPRITE_BOUNDS, shrunk
static constexpr glm::vec4 TREE_HOVER_BOUNDS = { -0.80f, -0.35f, 0.80f, 2.85f };

// "+1 wood" over a chopped tree: rises this far (tiles) over its lifetime
static constexpr Tick  POPUP_TICKS = 50;
static constexpr float POPUP_RISE  = 0.8f;
static constexpr float POPUP_START = 3.2f; // above the tree's foot, over its outline

// HUD text: distance from the screen's corner, and gap below show-info
static constexpr float HUD_MARGIN = 8.0f;
static constexpr float HUD_GAP    = 12.0f;

constexpr Render::DrawParams DRAW_PARAMS_HUD = {
    .view  = Render::View::Screen,
    .layer = Nightmourn::DrawLayer::Hud,
};

// -----------------------------------------------------------------------------
// Configs
//
// Static storage, so they outlive every entity that points at them.
// -----------------------------------------------------------------------------

static constexpr CharacterConfig HERO = {
    .name          = "hero",
    .max_health    = 300,
    .move_speed    = 5.0f,
    .mine_interval = 30, // a swing every 0.5 s
    .mine_amount   = 1,
    .mine_reach    = 2.0f,
};

static constexpr ResourceConfig OAK = {
    .name       = "oak",
    .subtype    = Nightmourn::to_subtype(ResourceType::Tree),
    .variety    = Nightmourn::to_variety(TreeVariety::Oak),
    .item       = Nightmourn::Items::Wood,
    .max_amount = 10,
};

static constexpr ResourceConfig PINE = {
    .name       = "pine",
    .subtype    = Nightmourn::to_subtype(ResourceType::Tree),
    .variety    = Nightmourn::to_variety(TreeVariety::Pine),
    .item       = Nightmourn::Items::Wood,
    .max_amount = 10,
};

// -----------------------------------------------------------------------------
// Atlas
//
// Pixel boxes in assets/graphics/atlas.png { x0, y0, x1, y1 }, y down. Same
// as 01-player-armies.
// -----------------------------------------------------------------------------

namespace AtlasPixels {
    constexpr glm::vec4     Meadow      = { 0.0f, 384.0f, 1024.0f, 1408.0f };
    constexpr std::uint32_t MeadowTiles = 8;

    constexpr glm::vec2 UnitSize   = { 64.0f, 128.0f };
    constexpr glm::vec2 HeroOrigin = { 320.0f, 128.0f }; // cyan row

    constexpr glm::vec2 TreeSize   = { 256.0f, 512.0f };
    constexpr glm::vec2 OakOrigin  = {   0.0f, 1408.0f };
    constexpr glm::vec2 PineOrigin = { 512.0f, 1408.0f };
}

// -----------------------------------------------------------------------------
// MiningDemo
// -----------------------------------------------------------------------------

class MiningDemo final : public IApplication {
public:

    // -------------------------------------------------------------------------
    // OnInit
    // -------------------------------------------------------------------------

    void on_init(Nezumi::Engine& engine) override {
        Nezumi::World&    world    = engine.world();
        Render::Renderer2D* renderer = engine.renderer2d();

        InputMap& input = engine.input();
        Nightmourn::bind_default_actions(input);
        input.bind(DemoAction::RegrowTrees, ScanCode::R);

        // Cameras: locked on the hero, as in 01-player-armies
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
                .zoom_init       = 1.0f,
                .zoom_min        = 0.25f,
                .zoom_max        = 4.0f,
                .follow_rate     = 0.0f,
            }
        );

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
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Oak )] = { AtlasPixels::OakOrigin,  2 };
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Pine)] = { AtlasPixels::PineOrigin, 1 };
        world_drawer_ = std::make_unique<WorldDrawer>(draw_config);

        // Mouse: only its position; mining reads the Mine action
        listeners_.push_back(engine.event_bus().subscribe<OnMouseMotion>(
            [this](const OnMouseMotion& e) {
                cursor_     = { e.pos_x, e.pos_y };
                has_cursor_ = true;
            }
        ));

        // Credit the player for what the hero chops, and float the amount
        // up from the tree
        listeners_.push_back(engine.event_bus().subscribe<OnEntityMined>(
            [this, &engine](const OnEntityMined& e) {
                player_.on_entity_mined(e);
                if (e.source != player_.character_handle() || e.amount == 0) return;

                const Nezumi::Entity* tree = engine.world().get_entity(e.target);
                if (tree) popups_.push_back({ tree->position.to_vec2(), e.item, e.amount, e.tick });
            }
        ));

        font_ = engine.ui()->font();
        Nightmourn::apply_ui_theme(engine.ui()->style());

        // Action bar and inventory, both over the player's inventory
        ui_atlas_       = Nightmourn::UIAtlas::load(*renderer);
        action_bar_     = std::make_unique<Nightmourn::ActionBar>(ui_atlas_);
        inventory_menu_ = std::make_unique<Nightmourn::InventoryMenu>(ui_atlas_, player_.inventory());

        // Something besides wood to drag around
        player_.inventory().add(Nightmourn::Items::Stone, 37);
        player_.inventory().add(Nightmourn::Items::Gold,  12);

        surface_id_ = world.create_surface(
            SurfaceConfig{
                .name           = "clearing",
                .initial_chunks = ChunkSpan::span_from_box(
                    Area2D(-MAP_HALF, -MAP_HALF, MAP_HALF - 1.0f, MAP_HALF - 1.0f)),
            }
        );

        spawn_trees_(world);
        spawn_hero_(world);

        engine.logger().info("Mining Demo");
        engine.logger().info("W/A/S/D: move. Mouse wheel: zoom. Left click (hold): chop the hovered tree. "
                             "B: inventory. 1-0: slots. R: regrow trees. Escape: main menu.");
    }

    // -------------------------------------------------------------------------
    // OnTick
    //
    // Runs before World::tick, so commands issued here take effect this tick.
    // -------------------------------------------------------------------------

    void on_tick(Nezumi::Engine& engine) override {
        Nezumi::World& world = engine.world();

        menus_.handle_input(engine.input());
        if (menus_.quit_requested()) engine.event_bus().dispatch(OnGameExited{});

        // Items do nothing yet: using one just says so
        action_bar_->handle_input(engine.input());
        for (std::size_t slot : action_bar_->take_used()) {
            if (const auto item = action_bar_->slot(slot))
                engine.logger().info("Used slot " + std::to_string(slot + 1) + ": " +
                                     std::string(Nightmourn::item_name(*item)) + " (" +
                                     std::to_string(player_.inventory().count(*item)) + " carried)");
        }

        if (engine.input().was_pressed(DemoAction::RegrowTrees)) regrow_trees_(world);

        player_.update(world, engine.input(), projection_(), hovered_tree_(engine));

        std::erase_if(popups_, [&](const Popup& p) { return world.get_tick() - p.tick > POPUP_TICKS; });
    }

    // -------------------------------------------------------------------------
    // OnDraw
    // -------------------------------------------------------------------------

    void on_draw(Nezumi::Engine& engine) override {
        Render::Renderer2D* r = engine.renderer2d();
        const Nezumi::World& world = engine.world();

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

        if (const Nezumi::Surface* surface = world.get_surface(surface_id_)) {
            DrawContext ctx = DrawContext::make(*r, world, *world_camera_, proj, debug_flags_);
            world_drawer_->draw(ctx, *surface);
            draw_hover_(ctx, engine);
            draw_popups_(ctx);
        }

        info_overlay_.update(engine);
        float hud_top = screen_top_() - HUD_MARGIN;
        if (debug_flags_.is_on(Nightmourn::DebugFlag::ShowInfo)) {
            const std::optional<glm::vec2> cursor = has_cursor_ ? std::optional(cursor_) : std::nullopt;
            hud_top = info_overlay_.draw(engine, *world_camera_, proj, cursor) - HUD_GAP;
        }
        draw_hud_(r, engine, hud_top);

        const Nightmourn::Inventory& inventory = player_.inventory();
        action_bar_->draw(*engine.ui(), engine.input(), menus_, player_.character(world), &inventory);
        menus_.draw(*engine.ui(), debug_flags_, inventory_menu_.get());
    }

    // -------------------------------------------------------------------------
    // OnExit
    // -------------------------------------------------------------------------

    void on_exit(Nezumi::Engine& engine) override {
        for (EventListenerID id : listeners_) engine.event_bus().unsubscribe(id);
        camera_controller_.reset();
        world_drawer_.reset();
        action_bar_.reset();
        inventory_menu_.reset();

        engine.logger().info("Chopped " + std::to_string(player_.inventory().count(Nightmourn::Items::Wood)) + " wood.");
        engine.logger().info("Goodbye!");
    }

private:

    // -------------------------------------------------------------------------
    // Spawn
    // -------------------------------------------------------------------------

    void spawn_hero_(Nezumi::World& world) {
        player_.possess(world.create_entity<Character>(
            /*.surface  = */surface_id_,
            /*.position = */HERO_SPAWN,
            /*.config   = */HERO,
            /*.factions = */faction_bit(0)
        ));
        snap_camera_ = true;
    }

    void spawn_trees_(Nezumi::World& world) {
        for (const TreeSpawn& t : TREES)
            world.create_entity<Resource>(surface_id_, t.position, t.pine ? PINE : OAK);
    }

    // Removes what is left of every tree and plants them all again, full
    void regrow_trees_(Nezumi::World& world) {
        std::vector<EntityHandle> doomed;
        world.each_entity([&](const Nezumi::Entity& entity) {
            if (entity.is_alive() && entity.is_type<Resource>()) doomed.push_back(entity.handle);
        });
        for (EntityHandle handle : doomed) world.remove_entity(handle);
        spawn_trees_(world);
    }

    // -------------------------------------------------------------------------
    // Hover
    // -------------------------------------------------------------------------

    // The tree under the mouse, picked through the hero if it stands in
    // front; none while the UI has the mouse
    EntityHandle hovered_tree_(Nezumi::Engine& engine) const {
        if (!has_cursor_ || engine.input().is_mouse_captured()) return {};
        const Nezumi::Surface* surface = engine.world().get_surface(surface_id_);
        if (!surface) return {};
        return Nightmourn::pick_entity(*surface, *world_camera_, projection_(), cursor_,
                                       [](const Nezumi::Entity& e) { return e.is_resource(); });
    }

    // White box around the hovered tree, faint while it's out of reach
    void draw_hover_(DrawContext& ctx, Nezumi::Engine& engine) const {
        const Nezumi::Entity* tree = ctx.world.get_entity(hovered_tree_(engine));
        if (!tree || !tree->is_alive()) return;

        const Character* hero  = player_.character(ctx.world);
        const bool       reach = hero && hero->can_reach(*tree);

        const glm::vec4 box = Nightmourn::sprite_view_box(ctx.proj, tree->position.to_vec2(), TREE_HOVER_BOUNDS);
        Nightmourn::draw_view_outline(ctx, box, reach ? HOVER_REACH : HOVER_TOO_FAR,
                                      HOVER_PIXELS, Nightmourn::DrawLayer::EntityOverlay);
    }

    // -------------------------------------------------------------------------
    // Popups
    // -------------------------------------------------------------------------

    struct Popup {
        glm::vec2 position = {}; // the tree's foot, world
        ItemID    item     = {};
        uint16_t  amount   = 0;
        Tick      tick     = 0;  // mined on
    };

    void draw_popups_(DrawContext& ctx) const {
        for (const Popup& p : popups_) {
            const float t = static_cast<float>(ctx.tick - p.tick) / static_cast<float>(POPUP_TICKS);
            if (t < 0.0f || t > 1.0f) continue;

            const std::string_view name = Nightmourn::item_name(p.item);
            char text[32];
            std::snprintf(text, sizeof text, "+%u %.*s", static_cast<unsigned>(p.amount),
                          static_cast<int>(name.size()), name.data());

            const float     scale = ctx.upright_scale();
            const glm::vec2 at    = ctx.to_view(p.position) + glm::vec2(0.0f, (POPUP_START + POPUP_RISE * t) * scale);
            const float     alpha = t < 0.6f ? 1.0f : 1.0f - (t - 0.6f) / 0.4f;

            ctx.renderer.draw_text(
                text, at, *font_,
                Render::TextStyle{
                    .size          = 16.0f * ctx.pixel_size,
                    .color         = { 1.0f, 0.92f, 0.70f, alpha },
                    .outline_color = { 0.0f, 0.0f, 0.0f, alpha },
                    .outline_width = 3.0f,
                    .align         = Render::TextAlign::Center,
                    .valign        = Render::TextVAlign::Baseline,
                },
                Nightmourn::world_layer(Nightmourn::DrawLayer::EntityOverlay)
            );
        }
    }

    // -------------------------------------------------------------------------
    // Projection
    // -------------------------------------------------------------------------

    Projection projection_() const { return DrawContext::projection(debug_flags_); }

    // -------------------------------------------------------------------------
    // Draw HUD
    // -------------------------------------------------------------------------

    float screen_top_() const { return screen_camera_->get_size().y / screen_camera_->get_scale(); }

    void draw_hud_(Render::Renderer2D* r, Nezumi::Engine& engine, float top) {
        const Nezumi::World& world = engine.world();

        size_t trees = 0;
        world.each_entity([&](const Nezumi::Entity& entity) {
            if (entity.is_alive() && entity.is_type<Resource>()) trees++;
        });

        std::string text;
        char line[128];

        std::snprintf(line, sizeof line, "wood  %u\ntrees %zu\n",
                      static_cast<unsigned>(player_.inventory().count(Nightmourn::Items::Wood)), trees);
        text += line;

        if (const Character* hero = player_.character(world)) {
            const float cooldown = static_cast<float>(hero->mine_cooldown()) / 60.0f;
            std::snprintf(line, sizeof line, "swing %s\n", cooldown > 0.0f ? "cooling down" : "ready");
            text += line;

            if (const Resource* tree = world.get_entity_as<Resource>(hovered_tree_(engine))) {
                std::snprintf(line, sizeof line, "\n%.*s  %u / %u wood  %s\n",
                    static_cast<int>(tree->config().name.size()), tree->config().name.data(),
                    static_cast<unsigned>(tree->amount), static_cast<unsigned>(tree->max_amount),
                    hero->can_reach(*tree) ? "(in reach)" : "(too far)");
                text += line;
            }
        }

        text += "\nW/A/S/D: move  click (hold): chop  B: inventory  1-0: slots  R: regrow  Esc: menu\n";

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

    std::unique_ptr<CameraController>          camera_controller_ = nullptr;
    std::unique_ptr<WorldDrawer>               world_drawer_      = nullptr;
    DebugFlags                                 debug_flags_       = {};
    GameMenus                                  menus_             = {};
    Nightmourn::UIAtlas                        ui_atlas_          = {};
    std::unique_ptr<Nightmourn::ActionBar>     action_bar_        = nullptr;
    std::unique_ptr<Nightmourn::InventoryMenu> inventory_menu_    = nullptr;
    InfoOverlay                                info_overlay_      = {};

    std::vector<EventListenerID> listeners_       = {};
    glm::vec2                    cursor_          = {}; // window pixels
    bool                         has_cursor_      = false;
    Projection                   last_projection_ = {};
    std::vector<Popup>           popups_          = {};

    Player player_      = {};
    bool   snap_camera_ = false;

    SurfaceID surface_id_ = {};
};

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title  = "Nightmourn: 02-mining";
        config.window.width  = 1280;
        config.window.height = 720;
        config.escape_exits  = false; // Escape opens the main menu

        Nezumi::Engine engine(config);

        MiningDemo app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
