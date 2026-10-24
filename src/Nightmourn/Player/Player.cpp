// =============================================================================
// Player/Player.cpp
// =============================================================================

#include "Nightmourn/Player/Player.hpp"
#include "Nezumi/Input/InputMap.hpp"
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
                    const Nezumi::Render::Projection& proj)
{
    Character* c = character(world);
    if (!c) return;

    // Screen direction of the keys, as a world direction
    const glm::vec2 screen_dir = input.axis2d(
        Action::MoveLeft, Action::MoveRight, Action::MoveDown, Action::MoveUp);
    glm::vec2 dir = proj.dir_to_world(screen_dir);
    if (dir != glm::vec2(0.0f)) dir = glm::normalize(dir);

    c->issue(CharacterCommand{ .move_dir = { dir.x, dir.y } });
}

// -----------------------------------------------------------------------------
// View
// -----------------------------------------------------------------------------

std::optional<glm::vec2> Player::view_target(const Nezumi::World& world,
                                             const Nezumi::Render::Projection& proj) const
{
    const Character* c = character(world);
    if (!c) return std::nullopt;
    return proj.to_view(c->position.to_vec2());
}

} // namespace Nightmourn
