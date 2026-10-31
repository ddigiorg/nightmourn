// =============================================================================
// Character/Character.hpp
//
// A person in the world: the hero the player controls, and later NPCs. It is
// a damageable entity with a faction, so Units see it as a
// target like any other enemy, and World's damage phase hurts and removes it.
//
// A character never reads input. Its controller (see Player) issues a
// CharacterCommand between ticks, the same rule Units follow, and the
// character turns that into velocity while ticking.
//
// Mining: while the command names a resource in reach, the character faces
// it and swings at it every CharacterConfig::mine_interval ticks, each swing
// a World::mine_entity() of mine_amount. The hit is immediate for now (no
// wind-up). Whoever credits the items listens for OnEntityMined with the
// character as its source.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityDamageable.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include "Nightmourn/Character/CharacterCommand.hpp"
#include "Nightmourn/Character/CharacterConfig.hpp"
#include "Nightmourn/Entity/EntityTypes.hpp"
#include "Nezumi/Core/Tick.hpp"

namespace Nightmourn {

struct WorldContext;

class Character : public EntityDamageable {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    static constexpr EntityTypeID TYPE_ID = EntityTypes::Character;

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // `config` must outlive the character
    Character(const CharacterConfig& config, EntityFactionsMask factions);

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    void tick(WorldContext& ctx) override;

    // -------------------------------------------------------------------------
    // Commands (between ticks only)
    // -------------------------------------------------------------------------

    void issue(const CharacterCommand& command) { command_ = command; }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    const CharacterConfig&  config () const { return *config_; }
    const CharacterCommand& command() const { return command_; }

    // Unit vector the character faces (last movement direction, or the
    // resource it is mining)
    Position2D facing() const { return facing_; }

    // -------------------------------------------------------------------------
    // Mining
    // -------------------------------------------------------------------------

    // True if `resource` (any entity) is close enough to swing at: its
    // collision box within mine_reach of the feet
    bool can_reach(const Entity& resource) const;

    // Ticks until the next swing may land (0: ready)
    uint16_t mine_cooldown() const { return mine_cooldown_; }

    // Tick the last swing landed on, and whether there has been one. Drawing
    // uses it to animate the swing.
    Nezumi::Tick last_swing_tick() const { return last_swing_tick_; }
    bool         has_swung()       const { return has_swung_;       }

private:

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    const CharacterConfig* config_  = nullptr;
    CharacterCommand       command_ = {};
    Position2D     facing_  = { 0.0f, -1.0f }; // down

    uint16_t     mine_cooldown_   = 0;
    Nezumi::Tick last_swing_tick_ = 0;
    bool         has_swung_       = false;

    void mine_(WorldContext& ctx);
};

} // namespace Nightmourn
