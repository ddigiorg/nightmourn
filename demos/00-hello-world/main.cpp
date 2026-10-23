#include "Nezumi/Engine.hpp"
#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <exception> // exception
#include <iostream>  // cerr

class HelloWorldDemo final : public Nezumi::IApplication {
public:
    void on_init(Nezumi::Engine& engine) override {
        engine.logger().info("Hello from a Nightmourn demo!");
        engine.logger().info("Press Escape or close the window to exit.");
    }

    void on_tick(Nezumi::Engine&) override {}

    void on_draw(Nezumi::Engine&) override {}

    void on_exit(Nezumi::Engine& engine) override {
        engine.logger().info("Goodbye!");
    }
};

int main() {
    try {
        Nezumi::EngineConfig config = {};
        config.window.title = "Nightmourn: 00-hello-world";
        config.escape_exits = true;

        Nezumi::Engine engine(config);

        HelloWorldDemo app;
        engine.run(app);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
