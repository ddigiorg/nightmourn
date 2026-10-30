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
// Mining: pass update() the entity under the cursor. While the mine action
// is held (or was clicked) over a resource, the character swings at it (see
// Character), unless the inventory has no room for what it yields. Credit
// the player from the game's OnEntityMined listener:
//
//   events.subscribe<OnEntityMined>([this](const OnEntityMined& e) {
//       player_.on_entity_mined(e);
//   });
//
// The inventory belongs to the player, not the character, so it survives
// the character dying and respawning.
//
// Possession is just a handle: possess() another character to switch, and a
// character that dies (World removes it) leaves the player with nothing to
// control until the game possesses a new one.
// =============================================================================

#pragma once
#include "Nightmourn/Item/Inventory.hpp"
#include "Nezumi/Render/View/Projection.hpp"
#include "Nezumi/World/Entity/EntityID.hpp"
#include <cstddef>     // size_t
#include <glm/glm.hpp> // vec2
#include <optional>    // optional

namespace Nezumi {
    class InputMap;
    class World;
    struct OnEntityMined;
}

namespace Nightmourn {

class Character;

class Player {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    // The inventory grid: 10 wide, 4 high
    static constexpr std::size_t INVENTORY_COLUMNS = 10;
    static constexpr std::size_t INVENTORY_ROWS    = 4;

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
    // directions on screen, diagonals as fast as straight lines. `hovered`
    // is the entity under the cursor (null for none, or while the UI has the
    // mouse): a resource there is mined while the mine action is held.
    // -------------------------------------------------------------------------

    void update(Nezumi::World& world, const Nezumi::InputMap& input,
                const Nezumi::Render::Projection& proj,
                Nezumi::EntityHandle hovered = {});

    // -------------------------------------------------------------------------
    // Inventory
    // -------------------------------------------------------------------------

    Inventory&       inventory()       { return inventory_; }
    const Inventory& inventory() const { return inventory_; }

    // Credits a mine by the possessed character: adds what it took to the
    // inventory. Mines by anyone else are ignored.
    void on_entity_mined(const Nezumi::OnEntityMined& event);

    // -------------------------------------------------------------------------
    // View
    // -------------------------------------------------------------------------

    // Where the camera should look (view space): the possessed character's
    // projected position, if any
    std::optional<glm::vec2> view_target(const Nezumi::World& world,
                                         const Nezumi::Render::Projection& proj) const;

private:
    Nezumi::EntityHandle character_ = {};
    Inventory            inventory_ { INVENTORY_COLUMNS * INVENTORY_ROWS };
};

} // namespace Nightmourn
