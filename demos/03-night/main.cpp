// =============================================================================
// demos/03-night/main.cpp
//
// Night falls on a camp in the meadow. Shows Nightmourn's lighting: the
// world drawn into a scene target, lights into a light target, composited
// over a day/night ambient, with bloom on the bright parts (see
// Lighting/GameLighting.hpp).
//
// - A campfire ringed by four torches, one torch alone out in the dark, two
//   magic wisps circling among the trees, and the hero's own light.
// - The day lasts 12 minutes and starts at dusk; T speeds time up (1x, 20x,
//   paused), N jumps to midnight, M to noon.
// - L upgrades the hero's lantern (none, small, large): at night, without
//   one, the hero sees almost nothing outside the firelight.
// - Healthbars, outlines and debug shapes are drawn after the lighting, so
//   they stay readable.
// - The debug menu (tilde) adds show-light-map, show-bloom and
//   disable-lighting.
//
// Controls: W/A/S/D move, mouse wheel zooms, T time speed, N midnight,
// M noon, L lantern, tilde (~) the debug menu, Escape the main menu.
// =============================================================================

#include "Nezumi/Control/CameraController.hpp"
#include "Nezumi/Core/Events/OnGameExited.hpp"
#include "Nezumi/Engine.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Draw/DebugFlags.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/GUI/GameMenus.hpp"
#include "Nightmourn/GUI/InfoOverlay.hpp"
#include "Nightmourn/GUI/Theme.hpp"
#include "Nightmourn/Input/Actions.hpp"
#include "Nightmourn/Item/Items.hpp"
#include "Nightmourn/Lighting/DayNight.hpp"
#include "Nightmourn/Lighting/GameLighting.hpp"
#include "Nightmourn/Lighting/LightDrawer.hpp"
#include "Nightmourn/Lighting/LightSource.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/Resource/ResourceTypes.hpp"
#include "Nightmourn/World/World.hpp"
#include <array>       // array
#include <cmath>       // sin, cos
#include <cstdio>      // snprintf
#include <cstdlib>     // EXIT_FAILURE, EXIT_SUCCESS
#include <exception>   // exception
#include <glm/glm.hpp> // vec2, vec4
#include <iostream>    // cerr
#include <memory>      // unique_ptr, make_unique
#include <optional>    // optional, nullopt
#include <string>      // string
#include <vector>      // vector

using namespace Nezumi;
using namespace Nightmourn;

// -----------------------------------------------------------------------------
// Demo Actions
// -----------------------------------------------------------------------------

namespace DemoAction {
    inline constexpr ActionID TimeSpeed = 1000;
    inline constexpr ActionID Midnight  = 1001;
    inline constexpr ActionID Noon      = 1002;
    inline constexpr ActionID Lantern   = 1003;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

static constexpr float TILE_PIXELS = 32.0f;
static constexpr float MAP_HALF    = 32.0f;

static constexpr Position2D HERO_SPAWN = { -1.5f, -3.0f };

// The camp: a fire with four torches around it, and one torch out alone
static constexpr glm::vec2 CAMPFIRE = { 0.0f, 0.0f };
static constexpr std::array<glm::vec2, 5> TORCHES = {{
    {  4.0f,  4.0f }, { -4.0f,  4.0f }, { -4.0f, -4.0f }, { 4.0f, -4.0f },
    { 14.0f, -9.0f },
}};

// Wisps circle these points among the trees
static constexpr glm::vec2 WISP_FROST  = { -11.0f,  7.0f };
static constexpr glm::vec2 WISP_ARCANE = {  10.0f, 10.0f };

// Lantern upgrades: the hero light's radius (tiles), 0 = none
static constexpr std::array<float, 3> LANTERNS = { 0.0f, 3.5f, 6.0f };

// Time speeds T cycles through
static constexpr std::array<float, 3> TIME_SPEEDS = { 1.0f, 20.0f, 0.0f };

struct TreeSpawn { Position2D position; bool pine; };
static constexpr std::array<TreeSpawn, 16> TREES = {{
    { {  7.5f,  1.0f }, false }, { {  9.0f,  6.5f }, true  }, { { 12.5f, 12.0f }, false },
    { {  6.0f, 11.0f }, true  }, { { -7.5f,  2.0f }, false }, { { -9.5f,  9.5f }, true  },
    { {-13.0f,  5.0f }, false }, { {-14.0f, 10.0f }, true  }, { { -6.5f, -8.0f }, false },
    { { -1.0f,-10.5f }, true  }, { {  5.5f, -9.5f }, false }, { { 10.0f, -4.5f }, true  },
    { { 17.0f, -6.0f }, false }, { { 16.0f,-12.0f }, true  }, { {-11.0f, -2.0f }, true  },
    { {  1.0f, 13.0f }, false },
}};

static constexpr CharacterConfig HERO = {
    .name       = "hero",
    .max_health = 300,
    .move_speed = 5.0f,
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

namespace AtlasPixels {
    constexpr glm::vec4     Meadow      = { 0.0f, 384.0f, 1024.0f, 1408.0f };
    constexpr std::uint32_t MeadowTiles = 8;

    constexpr glm::vec2 UnitSize   = { 64.0f, 128.0f };
    constexpr glm::vec2 HeroOrigin = { 320.0f, 128.0f };

    constexpr glm::vec2 TreeSize   = { 256.0f, 512.0f };
    constexpr glm::vec2 OakOrigin  = {   0.0f, 1408.0f };
    constexpr glm::vec2 PineOrigin = { 512.0f, 1408.0f };
}

static constexpr float HUD_MARGIN = 8.0f;
static constexpr float HUD_GAP    = 12.0f;

// -----------------------------------------------------------------------------
// NightDemo
// -----------------------------------------------------------------------------

class NightDemo final : public IApplication {
public:

    void on_init(Nezumi::Engine& engine) override {
        world_ = std::make_unique<Nightmourn::World>(engine.clock(), engine.event_bus());
        Render::Renderer2D* renderer = engine.renderer2d();

        InputMap& input = engine.input();
        Nightmourn::bind_default_actions(input);
        input.bind(DemoAction::TimeSpeed, ScanCode::T);
        input.bind(DemoAction::Midnight,  ScanCode::N);
        input.bind(DemoAction::Noon,      ScanCode::M);
        input.bind(DemoAction::Lantern,   ScanCode::L);

        world_camera_  = renderer->get_view_camera(Render::View::World );
        screen_camera_ = renderer->get_view_camera(Render::View::Screen);
        world_camera_->set_pixels_per_unit(TILE_PIXELS);

        camera_controller_ = std::make_unique<CameraController>(
            engine.event_bus(), input, *renderer, renderer->get_view(Render::View::World),
            CameraControllerConfig{
                .enable_drag_pan = false,
                .zoom_init       = 1.0f,
                .zoom_min        = 0.25f,
                .zoom_max        = 4.0f,
                .follow_rate     = 0.0f,
            });

        // Lighting replaces the engine's single pass
        lighting_ = std::make_unique<GameLighting>(*engine.render(), *renderer);

        atlas_ = renderer->create_atlas({ .name = "demo-atlas", .filepath = "assets/graphics/atlas.png" }).object;

        WorldDrawer::Config draw_config = {
            .chunks     = { .atlas = atlas_, .grass = AtlasPixels::Meadow, .grass_tiles = AtlasPixels::MeadowTiles },
            .units      = { .atlas = atlas_, .frame_size = AtlasPixels::UnitSize },
            .characters = { .atlas = atlas_, .frame_size = AtlasPixels::UnitSize, .row = AtlasPixels::HeroOrigin },
            .resources  = { .atlas = atlas_, .tree_frame_size = AtlasPixels::TreeSize },
        };
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Oak )] = { AtlasPixels::OakOrigin,  2 };
        draw_config.resources.trees[static_cast<size_t>(TreeVariety::Pine)] = { AtlasPixels::PineOrigin, 1 };
        world_drawer_ = std::make_unique<WorldDrawer>(draw_config);

        font_ = engine.ui()->font();
        Nightmourn::apply_ui_theme(engine.ui()->style());

        surface_id_ = world_->create_surface(SurfaceConfig{
            .name           = "camp",
            .initial_chunks = ChunkSpan::span_from_box(Area2D(-MAP_HALF, -MAP_HALF, MAP_HALF - 1.0f, MAP_HALF - 1.0f)),
        });

        for (const TreeSpawn& t : TREES)
            world_->create_entity<Resource>(surface_id_, t.position, t.pine ? PINE : OAK);

        player_.possess(world_->create_entity<Character>(surface_id_, HERO_SPAWN, HERO, faction_bit(0)));
        snap_camera_ = true;

        // Start at dusk, so the first minute shows night falling
        day_night_.set_time(0.76f);

        engine.logger().info("Night Demo");
        engine.logger().info("W/A/S/D: move. Wheel: zoom. T: time speed. N: midnight. M: noon. "
                             "L: lantern. ~: debug menu. Escape: main menu.");
    }

    void on_tick(Nezumi::Engine& engine) override {
        InputMap& input = engine.input();

        menus_.handle_input(input);
        if (menus_.quit_requested()) engine.event_bus().dispatch(OnGameExited{});

        if (input.was_pressed(DemoAction::TimeSpeed)) {
            speed_index_ = (speed_index_ + 1) % TIME_SPEEDS.size();
            day_night_.set_speed(TIME_SPEEDS[speed_index_]);
        }
        if (input.was_pressed(DemoAction::Midnight)) day_night_.set_time(0.0f);
        if (input.was_pressed(DemoAction::Noon))     day_night_.set_time(0.5f);
        if (input.was_pressed(DemoAction::Lantern))  lantern_ = (lantern_ + 1) % LANTERNS.size();

        player_.update(*world_, input, projection_(), {});
        day_night_.tick();
        world_->tick();
    }

    void on_draw(Nezumi::Engine& engine) override {
        Render::Renderer2D* r = engine.renderer2d();
        const Nightmourn::World& world = *world_;
        seconds_ += engine.clock().get_real_dt();

        const Projection proj = projection_();
        if (proj != last_projection_) { last_projection_ = proj; snap_camera_ = true; }
        if (auto target = player_.view_target(world, proj)) {
            camera_controller_->set_follow_target(*target, snap_camera_);
            snap_camera_ = false;
        }
        camera_controller_->update(engine.clock().get_real_dt());

        lighting_->update(day_night_.ambient(), debug_flags_);

        if (const Nightmourn::Surface* surface = world.get_surface(surface_id_)) {
            DrawContext ctx = DrawContext::make(*r, world, *world_camera_, proj, debug_flags_);
            ctx.overlay_view = GameLighting::OVERLAY_VIEW;

            world_drawer_->draw(ctx, *surface);
            draw_props_(ctx);
            light_drawer_.draw(ctx, collect_lights_(world), seconds_, day_night_.light_strength());
        }

        info_overlay_.update(engine);
        float hud_top = screen_top_() - HUD_MARGIN;
        if (debug_flags_.is_on(DebugFlag::ShowInfo))
            hud_top = info_overlay_.draw(engine, *world_camera_, proj, std::nullopt) - HUD_GAP;
        draw_hud_(r, hud_top);

        menus_.draw(*engine.ui(), debug_flags_);
    }

    void on_exit(Nezumi::Engine& engine) override {
        camera_controller_.reset();
        world_drawer_.reset();
        lighting_.reset();
        engine.logger().info("Goodbye!");
    }

private:

    // -------------------------------------------------------------------------
    // Lights
    // -------------------------------------------------------------------------

    std::vector<LightSource> collect_lights_(const Nightmourn::World& world) {
        lights_.clear();

        lights_.push_back(Lights::campfire(CAMPFIRE, 1));
        for (std::size_t i = 0; i < TORCHES.size(); i++)
            lights_.push_back(Lights::torch(TORCHES[i], static_cast<std::uint32_t>(10 + i)));

        // Wisps drift around their points, bobbing
        const auto wisp_at = [&](glm::vec2 center, float phase) {
            const float a = seconds_ * 0.5f + phase;
            return center + glm::vec2(2.5f * std::cos(a), 1.5f * std::sin(a * 1.3f));
        };
        const float bob = 0.15f * std::sin(seconds_ * 2.0f);
        lights_.push_back(Lights::wisp(wisp_at(WISP_FROST,  0.0f), { 0.30f, 0.60f, 1.00f }, 1.1f + bob));
        lights_.push_back(Lights::wisp(wisp_at(WISP_ARCANE, 2.0f), { 0.75f, 0.30f, 1.00f }, 1.1f - bob));

        if (const Character* hero = player_.character(world); hero && LANTERNS[lantern_] > 0.0f)
            lights_.push_back(Lights::hero_light(hero->position.to_vec2(), LANTERNS[lantern_]));

        return lights_;
    }

    // Programmer art: torch posts and the campfire's logs, so the flames
    // have something to stand on
    void draw_props_(DrawContext& ctx) const {
        const float scale = ctx.upright_scale();

        for (const glm::vec2 torch : TORCHES) {
            const glm::vec2 feet = ctx.to_view(torch);
            Render::DrawParams params = ctx.layer(DrawLayer::Entities);
            params.order = y_sort_order(feet.y);
            ctx.renderer.draw_rectangle(
                Render::Rectangle{ .pos_box = { feet.x - 0.04f * scale, feet.y, feet.x + 0.04f * scale, feet.y + 1.1f * scale },
                                   .color   = { 0.22f, 0.14f, 0.08f, 1.0f } },
                params);
            ctx.renderer.draw_rectangle(
                Render::Rectangle{ .pos_box = { feet.x - 0.07f * scale, feet.y + 1.0f * scale, feet.x + 0.07f * scale, feet.y + 1.12f * scale },
                                   .color   = { 0.10f, 0.07f, 0.05f, 1.0f } },
                params);
        }

        // Two crossed logs on the ground
        const Render::DrawParams ground = ctx.layer(DrawLayer::GroundOverlay);
        const float width = 0.16f * scale;
        const glm::vec4 log = { 0.25f, 0.15f, 0.08f, 1.0f };
        ctx.renderer.draw_line({ .position_a = ctx.to_view(CAMPFIRE + glm::vec2(-0.6f, -0.2f)),
                                 .position_b = ctx.to_view(CAMPFIRE + glm::vec2( 0.6f,  0.2f)),
                                 .color = log, .width = width }, ground);
        ctx.renderer.draw_line({ .position_a = ctx.to_view(CAMPFIRE + glm::vec2(-0.2f,  0.6f)),
                                 .position_b = ctx.to_view(CAMPFIRE + glm::vec2( 0.2f, -0.6f)),
                                 .color = log, .width = width }, ground);
    }

    // -------------------------------------------------------------------------
    // HUD
    // -------------------------------------------------------------------------

    Projection projection_() const { return DrawContext::projection(debug_flags_); }

    float screen_top_() const { return screen_camera_->get_size().y / screen_camera_->get_scale(); }

    void draw_hud_(Render::Renderer2D* r, float top) const {
        static constexpr const char* LANTERN_NAMES[] = { "none", "small", "large" };

        char clock[16];
        day_night_.format_clock(clock, sizeof clock);

        char text[256];
        std::snprintf(text, sizeof text,
            "%s  %s  (time x%.0f)\nlantern  %s\n\nW/A/S/D: move  T: time speed  N: midnight  M: noon  L: lantern  ~: debug\n",
            clock, day_night_.is_night() ? "night" : "day", day_night_.get_speed(), LANTERN_NAMES[lantern_]);

        r->draw_text(text, { HUD_MARGIN, top }, *font_,
                     Render::TextStyle{
                         .size          = 16.0f,
                         .color         = { 1.0f, 1.0f, 1.0f, 1.0f },
                         .outline_color = { 0.0f, 0.0f, 0.0f, 1.0f },
                         .outline_width = 3.0f,
                         .align         = Render::TextAlign::Left,
                         .valign        = Render::TextVAlign::Top,
                     },
                     { .view = Render::View::Screen, .layer = DrawLayer::Hud });
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    std::unique_ptr<Nightmourn::World> world_ = nullptr;

    Render::Camera2D* world_camera_  = nullptr;
    Render::Camera2D* screen_camera_ = nullptr;
    Render::Atlas*    atlas_         = nullptr;
    Render::Font*     font_          = nullptr;

    std::unique_ptr<CameraController> camera_controller_ = nullptr;
    std::unique_ptr<WorldDrawer>      world_drawer_      = nullptr;
    std::unique_ptr<GameLighting>     lighting_          = nullptr;
    LightDrawer                       light_drawer_      = {};
    std::vector<LightSource>          lights_            = {};
    DayNight                          day_night_;
    DebugFlags                        debug_flags_       = {};
    GameMenus                         menus_             = {};
    InfoOverlay                       info_overlay_      = {};

    Projection  last_projection_ = {};
    Player      player_          = {};
    bool        snap_camera_     = false;
    std::size_t speed_index_     = 0;
    std::size_t lantern_         = 1;
    float       seconds_         = 0.0f;

    SurfaceID surface_id_ = {};
};

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title  = "Nightmourn: 03-night";
        config.window.width  = 1280;
        config.window.height = 720;
        config.escape_exits  = false; // Escape opens the main menu

        Nezumi::Engine engine(config);

        NightDemo app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
