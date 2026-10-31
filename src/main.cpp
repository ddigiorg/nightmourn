#include "Nezumi/Engine.hpp"
#include "Nightmourn/Game/GameContext.hpp"
#include "Nightmourn/Player/Player.hpp"
#include "Nightmourn/World/World.hpp"
#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <exception> // exception
#include <iostream>  // cerr
#include <memory>    // unique_ptr, make_unique

class Game final : public Nezumi::IApplication {
public:
    void on_init(Nezumi::Engine& engine) override {
        engine.logger().info("Starting Nightmourn");
        engine.logger().info("Press Escape or close the window to exit.");

        // The game owns its world; the engine only supplies the clock and bus
        world_ = std::make_unique<Nightmourn::World>(engine.clock(), engine.event_bus());
        world_->create_surface();

        ctx_ = {
            .engine = &engine.context(),
            .world  = world_.get(),
            .player = &player_,
        };
    }

    void on_tick(Nezumi::Engine&) override {
        ctx_.world->tick();
    }

    void on_draw(Nezumi::Engine&) override {}

    void on_exit(Nezumi::Engine& engine) override {
        engine.logger().info("Goodbye!");
    }

private:
    std::unique_ptr<Nightmourn::World> world_  = nullptr;
    Nightmourn::Player                 player_ = {};
    Nightmourn::GameContext            ctx_    = {};
};

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title = "Nightmourn";
        config.escape_exits = true;

        Nezumi::Engine engine(config);

        Game app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
