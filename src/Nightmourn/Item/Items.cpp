// =============================================================================
// Item/Items.cpp
// =============================================================================

#include "Nightmourn/Item/Items.hpp"

namespace Nightmourn {

namespace {

std::vector<ItemInfo>& items() {
    static std::vector<ItemInfo> table;
    return table;
}

const ItemInfo* info(ItemID item) {
    const std::vector<ItemInfo>& table = items();
    return item.is_valid() && item.value() < table.size() ? &table[item.value()] : nullptr;
}

} // namespace

void set_items(std::vector<ItemInfo> table) {
    items() = std::move(table);
}

std::string_view item_name(ItemID item) {
    const ItemInfo* i = info(item);
    return i ? std::string_view(i->name) : std::string_view("item");
}

std::string display_name(std::string_view name) {
    std::string s(name);
    for (char& c : s) if (c == '-' || c == '_') c = ' ';
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}

std::uint16_t item_stack_size(ItemID item) {
    const ItemInfo* i = info(item);
    return i ? i->stack_size : 1;
}

} // namespace Nightmourn
