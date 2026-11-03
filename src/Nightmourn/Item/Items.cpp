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

std::uint16_t item_stack_size(ItemID item) {
    const ItemInfo* i = info(item);
    return i ? i->stack_size : 1;
}

} // namespace Nightmourn
