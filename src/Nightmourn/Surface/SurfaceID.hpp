// =============================================================================
// SurfaceID.hpp
//
// Surface identification.
//
// TODO: Surfaces are few and long-lived, so a plain index is enough. Upgrade to
// a Handle<Surface> once surfaces can be removed (see ROADMAP.md).
// =============================================================================

#pragma once
#include "Nezumi/Object/ID.hpp"
#include <cstdint> // uint8_t
class Surface;

namespace Nightmourn {

using SurfaceID = Nezumi::ID<Surface, uint8_t>;

} // namespace Nightmourn
