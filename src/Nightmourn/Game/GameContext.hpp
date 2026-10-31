// =============================================================================
// Game/GameContext.hpp
//
// Everything game code may reach, in one place: the engine's services
// (clock, event bus, input, renderer, UI) through `engine`, and the game's own
// state beside them. The game owns the World; the engine never sees it.
//
//   void on_init(Nezumi::Engine& engine) override {
//       world_ = std::make_unique<World>(engine.clock(), engine.event_bus());
//       ctx_   = { .engine = &engine.context(), .world = world_.get(), .player = &player_ };
//   }
//
// Narrower views stay as they are: WorldContext is what entities see while
// ticking, and DrawContext is what drawers see each frame.
// =============================================================================

#pragma once

namespace Nezumi {
    class EngineContext;
}

namespace Nightmourn {

class Player;
class World;

struct GameContext {
    Nezumi::EngineContext* engine = nullptr; // engine services (not owned)
    World*                 world  = nullptr; // owned by the game
    Player*                player = nullptr; // owned by the game
};

} // namespace Nightmourn
