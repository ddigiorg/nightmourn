// =============================================================================
// Input/Actions.cpp
// =============================================================================

#include "Nightmourn/Input/Actions.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include <cstdint> // uint32_t

namespace Nightmourn {

void bind_default_actions(Nezumi::InputMap& input) {
    using Nezumi::ScanCode;

    input.bind(Action::MoveLeft,  ScanCode::A);
    input.bind(Action::MoveRight, ScanCode::D);
    input.bind(Action::MoveDown,  ScanCode::S);
    input.bind(Action::MoveUp,    ScanCode::W);

    // F1..F12 are consecutive scancodes
    static_assert(DEBUG_FLAG_COUNT <= 12, "More debug flags than F keys");
    for (std::size_t i = 0; i < DEBUG_FLAG_COUNT; i++) {
        const auto key = static_cast<ScanCode>(static_cast<std::uint32_t>(ScanCode::F1) + i);
        input.bind(debug_toggle_action(static_cast<DebugFlag>(i)), key);
    }
}

void toggle_debug_flags(const Nezumi::InputMap& input, DebugFlags& flags) {
    for (std::size_t i = 0; i < DEBUG_FLAG_COUNT; i++) {
        const auto flag = static_cast<DebugFlag>(i);
        if (input.was_pressed(debug_toggle_action(flag))) flags.toggle(flag);
    }
}

} // namespace Nightmourn
