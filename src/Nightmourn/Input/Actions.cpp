// =============================================================================
// Input/Actions.cpp
// =============================================================================

#include "Nightmourn/Input/Actions.hpp"
#include "Nezumi/Input/InputMap.hpp"

namespace Nightmourn {

void bind_default_actions(Nezumi::InputMap& input) {
    using Nezumi::ScanCode;

    input.bind(Action::MoveLeft,  ScanCode::A);
    input.bind(Action::MoveRight, ScanCode::D);
    input.bind(Action::MoveDown,  ScanCode::S);
    input.bind(Action::MoveUp,    ScanCode::W);

    input.bind(Action::ToggleDebugMenu, ScanCode::Grave);
}

} // namespace Nightmourn
