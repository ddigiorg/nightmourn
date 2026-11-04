// =============================================================================
// Settlement/SettlementID.hpp
//
// Which settlement: an index into World's settlements, never reused, so a
// building that outlives its settlement (its banner fell) keeps an id that
// simply finds nothing. Null for "none".
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include <cstdint> // uint32_t

namespace Nightmourn {

struct SettlementTag;

using SettlementID = Nezumi::ID<SettlementTag, std::uint32_t>;

} // namespace Nightmourn
