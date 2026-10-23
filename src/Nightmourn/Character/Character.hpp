// =============================================================================
// Character/Character.hpp
//
// A person in the world: the hero the player controls, and later NPCs. It is
// a damageable entity with a faction, so the engine's Units see it as a
// target like any other enemy, and World's damage phase hurts and removes it.
//
// A character never reads input. Its controller (see Player) issues a
// CharacterCommand between ticks, the same rule Units follow, and the
// character turns that into velocity while ticking.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityDamageable.hpp"
#include "Nezumi/World/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Character/CharacterCommand.hpp"
#include "Nightmourn/Character/CharacterConfig.hpp"
#include "Nightmourn/World/EntityTypes.hpp"

namespace Nezumi {
    struct WorldContext;
}

namespace Nightmourn {

class Character : public Nezumi::EntityDamageable {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr Nezumi::EntityTypeID TYPE_ID = EntityTypes::Character;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the character
    Character(const CharacterConfig& config, Nezumi::EntityFactionsMask factions);

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    void tick(Nezumi::WorldContext& ctx) override;

    // -------------------------------------------------------------------------
    // Commands (between ticks only)
    // -------------------------------------------------------------------------

    void issue(const CharacterCommand& command) { command_ = command; }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const CharacterConfig&  config () const { return *config_; }
    const CharacterCommand& command() const { return command_; }

    // Unit vector the character faces (last movement direction)
    Nezumi::Position2D facing() const { return facing_; }

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const CharacterConfig* config_  = nullptr;
    CharacterCommand       command_ = {};
    Nezumi::Position2D     facing_  = { 0.0f, -1.0f }; // down
};

} // namespace Nightmourn
