// =============================================================================
// World/EntityTypes.hpp
//
// Nightmourn's entity type IDs: the single place they are assigned, so two
// types can never share one.
//
// Nezumi's own types (Unit 1, UnitGroup 2) sit below GAME_TYPE_BASE. Game
// types start there, leaving the engine room to grow.
// =============================================================================

#pragma once
#include "Nezumi/World/Entity/EntityID.hpp"

namespace Nightmourn::EntityTypes {

inline constexpr Nezumi::EntityTypeID::value_t GAME_TYPE_BASE = 64;

inline constexpr Nezumi::EntityTypeID Character{ GAME_TYPE_BASE + 0 };

} // namespace Nightmourn::EntityTypes
