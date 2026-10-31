// =============================================================================
// EntityList.hpp
//
// Circular intrusive doubly-linked list for storing entity object pointers.
// =============================================================================

#pragma once
#include "Nezumi/Core/IntrusiveList.hpp"
#include "Nezumi/Core/IntrusiveListNode.hpp"

namespace Nightmourn {

class Entity;

using EntityListNode = Nezumi::IntrusiveListNode<Entity>;
using EntityList     = Nezumi::IntrusiveList    <Entity>;

} // namespace Nightmourn
