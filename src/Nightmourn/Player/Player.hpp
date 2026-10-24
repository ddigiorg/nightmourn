// =============================================================================
// Player/Player.hpp
//
// The link between a human and the game: reads their input actions and
// controls the Character they possess. A Player is not an entity. The game
// owns one per human, and the character it drives lives in the World like any
// other entity.
//
// Each tick, before World::tick (i.e. in on_tick), update() turns the move
// actions into a CharacterCommand. Each frame, the game asks view_target()
// where the camera should look:
//
//   void on_tick(Engine& engine) override {
//       player_.update(engine.world(), engine.input(), projection);
//   }
//
//   void on_draw(Engine& engine) override {
//       if (auto target = player_.view_target(engine.world(), projection))
//           camera_controller_->set_follow_target(*target);
//       camera_controller_->update(engine.clock().get_real_dt());
//   }
//
// Moves are screen-relative: W walks up the screen whatever the projection,
// which in iso is world (+1, +1). The projection only turns the keys into a
// world direction; the command itself is in world space like the rest of the
// sim.
//
// Possession is just a handle: possess() another character to switch, and a
// character that dies (World removes it) leaves the player with nothing to
// control until the game possesses a new one.
// =============================================================================

#pragma once
#include "Nezumi/Render/View/Projection.hpp"
#include "Nezumi/World/Entity/EntityID.hpp"
#include <glm/glm.hpp> // vec2
#include <optional>    // optional

namespace Nezumi {
    class InputMap;
    class World;
}

namespace Nightmourn {

class Character;

class Player {
public:

    // -------------------------------------------------------------------------
    // Possession
    // -------------------------------------------------------------------------

    void possess(Nezumi::EntityHandle character) { character_ = character; }
    void release()                               { character_ = {};        }

    Nezumi::EntityHandle character_handle() const { return character_; }

    // The possessed character, or nullptr if there is none or it is gone
    Character*       character(Nezumi::World& world) const;
    const Character* character(const Nezumi::World& world) const;

    // -------------------------------------------------------------------------
    // Update (between ticks)
    //
    // Reads the move actions and commands the possessed character: 8
    // directions on screen, diagonals as fast as straight lines.
    // -------------------------------------------------------------------------

    void update(Nezumi::World& world, const Nezumi::InputMap& input,
                const Nezumi::Render::Projection& proj);

    // -------------------------------------------------------------------------
    // View
    // -------------------------------------------------------------------------

    // Where the camera should look (view space): the possessed character's
    // projected position, if any
    std::optional<glm::vec2> view_target(const Nezumi::World& world,
                                         const Nezumi::Render::Projection& proj) const;

private:
    Nezumi::EntityHandle character_ = {};
};

} // namespace Nightmourn
