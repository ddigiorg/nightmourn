// =============================================================================
// Input/Actions.cpp
// =============================================================================

#include "Nightmourn/Input/Actions.hpp"
#include "Nezumi/Input/InputMap.hpp"
#include <iterator> // size

namespace Nightmourn {

void bind_default_actions(Nezumi::InputMap& input) {
    using Nezumi::ScanCode;

    input.bind(Action::MoveLeft,  ScanCode::A);
    input.bind(Action::MoveRight, ScanCode::D);
    input.bind(Action::MoveDown,  ScanCode::S);
    input.bind(Action::MoveUp,    ScanCode::W);

    input.bind(Action::ToggleDebugMenu,     ScanCode::Grave);
    input.bind(Action::ToggleSkillsMenu,    ScanCode::K);
    input.bind(Action::ToggleEquipmentMenu, ScanCode::C);
    input.bind(Action::ToggleInventoryMenu, ScanCode::B);
    input.bind(Action::ToggleItemsMenu,     ScanCode::I);
    input.bind(Action::ToggleOptionsMenu,   ScanCode::O);
    input.bind(Action::ToggleMainMenu,      ScanCode::Escape);

    // 1, 2, ... 9, 0: the number row, in its order on the keyboard
    constexpr ScanCode SLOT_KEYS[] = {
        ScanCode::Num1, ScanCode::Num2, ScanCode::Num3, ScanCode::Num4, ScanCode::Num5,
        ScanCode::Num6, ScanCode::Num7, ScanCode::Num8, ScanCode::Num9, ScanCode::Num0,
    };
    for (std::size_t i = 0; i < std::size(SLOT_KEYS); i++)
        input.bind(Action::use_slot(i), SLOT_KEYS[i]);
}

} // namespace Nightmourn
