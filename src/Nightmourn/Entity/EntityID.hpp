// =============================================================================
// EntityID.hpp
//
// Entity identification.
// =============================================================================

#pragma once
#include "Nezumi/Object/Handle.hpp"
#include "Nezumi/Object/ID.hpp"
#include <cstdint> // uint8_t, uint16_t

namespace Nightmourn {

class  Entity;
struct EntityTypeTag;
struct EntitySubtypeTag;

// Index + generation into World's entity SlotMap. This is the only safe way
// to hold a reference to another entity across ticks, and doubles as a network
// and replay ID because every lockstep peer allocates handles identically.
using EntityHandle = Nezumi::Handle<Entity>;

// Concrete C++ type (Unit, Building, ...). Assigned explicitly by each derived
// class as `TYPE_ID`.
using EntityTypeID = Nezumi::ID<EntityTypeTag, std::uint8_t>;

// Data-driven variant within a type (Knight, Tree, ...).
using EntitySubtypeID = Nezumi::ID<EntitySubtypeTag, std::uint16_t>;

} // namespace Nightmourn
