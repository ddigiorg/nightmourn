// =============================================================================
// Entity/EntityTypes.hpp
//
// Nightmourn's entity type IDs: the single place they are assigned, so two
// types can never share one. Each entity class sets its TYPE_ID from here.
// =============================================================================

#pragma once
#include "Nightmourn/Entity/EntityID.hpp"

namespace Nightmourn::EntityTypes {

inline constexpr EntityTypeID Unit     { 1 };
inline constexpr EntityTypeID UnitGroup{ 2 };
inline constexpr EntityTypeID Resource { 3 };
inline constexpr EntityTypeID Character{ 4 };

} // namespace Nightmourn::EntityTypes
