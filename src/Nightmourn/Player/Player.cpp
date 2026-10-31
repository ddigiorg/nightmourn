// =============================================================================
// Player/Player.cpp
// =============================================================================

#include "Nightmourn/Player/Player.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include "Nezumi/World/Entity/EntityResource.hpp"
#include "Nezumi/World/Events/OnEntityMined.hpp"
#include "Nezumi/World/World.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Input/Actions.hpp"

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Possession
// -----------------------------------------------------------------------------

Character* Player::character(Nezumi::World& world) const {
    Character* c = world.get_entity_as<Character>(character_);
    return c && !c->is_pending_remove() ? c : nullptr;
}

const Character* Player::character(const Nezumi::World& world) const {
    const Character* c = world.get_entity_as<Character>(character_);
    return c && !c->is_pending_remove() ? c : nullptr;
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------

void Player::update(Nezumi::World& world, const Nezumi::InputMap& input,
                    const Nezumi::Projection&         proj,
                    Nezumi::EntityHandle hovered)
{
    Character* c = character(world);
    if (!c) return;

    // Screen direction of the keys, as a world direction
    const glm::vec2 screen_dir = input.axis2d(
        Action::MoveLeft, Action::MoveRight, Action::MoveDown, Action::MoveUp);
    glm::vec2 dir = proj.dir_to_world(screen_dir);
    if (dir != glm::vec2(0.0f)) dir = glm::normalize(dir);

    // A click between ticks counts too, so a quick tap still swings
    Nezumi::EntityHandle mine_target = {};
    if (input.is_held(Action::Mine) || input.was_pressed(Action::Mine)) {
        const Nezumi::Entity* e = world.get_entity(hovered);
        if (e && e->is_alive() && e->is_resource() &&
            inventory_.can_add(static_cast<const Nezumi::EntityResource*>(e)->item))
            mine_target = hovered;
    }

    c->issue(CharacterCommand{ .move_dir = { dir.x, dir.y }, .mine_target = mine_target });
}

// -----------------------------------------------------------------------------
// Inventory
// -----------------------------------------------------------------------------

void Player::on_entity_mined(const Nezumi::OnEntityMined& event) {
    if (!character_ || event.source != character_ || !event.item) return;

    // update() only mines what fits, so nothing is lost unless the inventory
    // filled up in between (then the rest is, until items drop on the ground)
    inventory_.add(event.item, event.amount);
}

// -----------------------------------------------------------------------------
// View
// -----------------------------------------------------------------------------

std::optional<glm::vec2> Player::view_target(const Nezumi::World& world,
                                             const Nezumi::Projection& proj) const
{
    const Character* c = character(world);
    if (!c) return std::nullopt;
    return proj.to_view(c->position.to_vec2());
}

} // namespace Nightmourn
