// =============================================================================
// Item/Inventory.cpp
// =============================================================================

#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include <algorithm> // min
#include <utility>   // swap

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Items
// -----------------------------------------------------------------------------

std::uint32_t Inventory::count(Nezumi::ItemID item) const {
    std::uint32_t total = 0;
    for (const auto& s : slots_)
        if (s && s->item == item) total += s->count;
    return total;
}

std::uint32_t Inventory::room_for(Nezumi::ItemID item) const {
    const std::uint32_t stack = item_stack_size(item);
    std::uint32_t room = 0;
    for (const auto& s : slots_) {
        if (!s)                   room += stack;
        else if (s->item == item) room += stack - s->count;
    }
    return room;
}

std::uint32_t Inventory::add(Nezumi::ItemID item, std::uint32_t amount) {
    const std::uint16_t stack = item_stack_size(item);
    std::uint32_t left = amount;

    // Top up the stacks already there, then start new ones
    for (auto& s : slots_) {
        if (left == 0) break;
        if (!s || s->item != item || s->count >= stack) continue;
        const std::uint32_t put = std::min<std::uint32_t>(left, stack - s->count);
        s->count = static_cast<std::uint16_t>(s->count + put);
        left -= put;
    }
    for (auto& s : slots_) {
        if (left == 0) break;
        if (s) continue;
        const std::uint32_t put = std::min<std::uint32_t>(left, stack);
        s = ItemStack{ item, static_cast<std::uint16_t>(put) };
        left -= put;
    }
    return amount - left;
}

std::uint32_t Inventory::remove(Nezumi::ItemID item, std::uint32_t amount) {
    std::uint32_t left = amount;
    for (auto it = slots_.rbegin(); it != slots_.rend() && left > 0; ++it) {
        auto& s = *it;
        if (!s || s->item != item) continue;
        const std::uint32_t take = std::min<std::uint32_t>(left, s->count);
        s->count = static_cast<std::uint16_t>(s->count - take);
        left -= take;
        if (s->count == 0) s.reset();
    }
    return amount - left;
}

// -----------------------------------------------------------------------------
// Rearranging
// -----------------------------------------------------------------------------

void Inventory::move(std::size_t from, std::size_t to) {
    if (from == to || from >= slots_.size() || to >= slots_.size()) return;
    auto& src = slots_[from];
    auto& dst = slots_[to];
    if (!src) return;

    if (dst && dst->item == src->item) {
        const std::uint16_t stack = item_stack_size(src->item);
        const std::uint16_t put   = std::min<std::uint16_t>(src->count, static_cast<std::uint16_t>(stack - std::min(dst->count, stack)));
        dst->count = static_cast<std::uint16_t>(dst->count + put);
        src->count = static_cast<std::uint16_t>(src->count - put);
        if (src->count == 0) src.reset();
        return;
    }
    std::swap(src, dst);
}

} // namespace Nightmourn
