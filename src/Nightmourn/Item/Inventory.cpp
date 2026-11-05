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

void Inventory::counted_(ItemID item, std::uint32_t added, std::uint32_t removed) {
    if (added == removed) return;
    if (item.value() >= totals_.size()) totals_.resize(static_cast<std::size_t>(item.value()) + 1, 0);
    totals_[item.value()] = totals_[item.value()] + added - removed;
    version_++;
}

std::uint32_t Inventory::room_for(ItemID item) const {
    const std::uint32_t stack = item_stack_size(item);
    std::uint32_t room = 0;
    for (const auto& s : slots_) {
        if (!s)                   room += stack;
        else if (s->item == item) room += stack - s->count;
    }
    return room;
}

std::uint32_t Inventory::add(ItemID item, std::uint32_t amount) {
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
    counted_(item, amount - left, 0);
    return amount - left;
}

std::uint32_t Inventory::remove(ItemID item, std::uint32_t amount) {
    std::uint32_t left = amount;
    for (auto it = slots_.rbegin(); it != slots_.rend() && left > 0; ++it) {
        auto& s = *it;
        if (!s || s->item != item) continue;
        const std::uint32_t take = std::min<std::uint32_t>(left, s->count);
        s->count = static_cast<std::uint16_t>(s->count - take);
        left -= take;
        if (s->count == 0) s.reset();
    }
    counted_(item, 0, amount - left);
    return amount - left;
}

// -----------------------------------------------------------------------------
// Rearranging
// -----------------------------------------------------------------------------

void Inventory::move(Inventory& from, std::size_t from_slot, Inventory& to, std::size_t to_slot) {
    if (&from == &to && from_slot == to_slot) return;
    if (from_slot >= from.slots_.size() || to_slot >= to.slots_.size()) return;
    auto& src = from.slots_[from_slot];
    auto& dst = to.slots_[to_slot];
    if (!src) return;

    if (dst && dst->item == src->item) {
        const ItemID        item  = src->item;
        const std::uint16_t stack = item_stack_size(item);
        const std::uint16_t put   = std::min<std::uint16_t>(src->count, static_cast<std::uint16_t>(stack - std::min(dst->count, stack)));
        dst->count = static_cast<std::uint16_t>(dst->count + put);
        src->count = static_cast<std::uint16_t>(src->count - put);
        if (src->count == 0) src.reset();
        if (&from != &to) {
            from.counted_(item, 0, put);
            to.counted_(item, put, 0);
        } else if (put > 0) {
            from.version_++;
        }
        return;
    }

    // A swap: within one inventory the totals stay, between two each loses
    // what it gave and gains what it got
    const std::optional<ItemStack> gave = src, got = dst;
    std::swap(src, dst);
    if (&from == &to) {
        from.version_++;
        return;
    }
    from.counted_(gave->item, 0, gave->count);
    to.counted_(gave->item, gave->count, 0);
    if (got) {
        to.counted_(got->item, 0, got->count);
        from.counted_(got->item, got->count, 0);
    }
}

std::uint32_t Inventory::transfer(Inventory& from, std::size_t slot, Inventory& to, std::uint32_t amount) {
    if (&from == &to || slot >= from.slots_.size()) return 0;
    auto& src = from.slots_[slot];
    if (!src) return 0;

    const ItemID        item  = src->item;
    const std::uint32_t moved = to.add(item, std::min<std::uint32_t>(amount, src->count));
    src->count = static_cast<std::uint16_t>(src->count - moved);
    if (src->count == 0) src.reset();
    from.counted_(item, 0, moved);
    return moved;
}

// -----------------------------------------------------------------------------
// Several inventories at once
// -----------------------------------------------------------------------------

std::uint32_t count(std::span<Inventory* const> from, ItemID item) {
    std::uint32_t total = 0;
    for (const Inventory* inv : from)
        if (inv) total += inv->count(item);
    return total;
}

std::optional<ItemAmount> shortfall(std::span<const ItemAmount> cost, std::span<Inventory* const> from) {
    for (const ItemAmount& c : cost) {
        // The same item could be listed twice; count it once in total
        std::uint32_t need = 0;
        for (const ItemAmount& d : cost)
            if (d.item == c.item) need += d.count;
        const std::uint32_t have = count(from, c.item);
        if (have < need) return ItemAmount{ c.item, need - have };
    }
    return std::nullopt;
}

bool pay(std::span<const ItemAmount> cost, std::span<Inventory* const> from) {
    if (shortfall(cost, from)) return false;
    for (const ItemAmount& c : cost) {
        std::uint32_t left = c.count;
        for (Inventory* inv : from)
            if (inv && left > 0) left -= inv->remove(c.item, left);
    }
    return true;
}

bool give(std::span<const ItemAmount> items, std::span<Inventory* const> to) {
    // Try it on copies first (a few dozen slots each), so it's all or nothing
    // however the items share out the free slots
    std::vector<Inventory> trial;
    trial.reserve(to.size());
    for (const Inventory* inv : to) trial.push_back(inv ? *inv : Inventory(0));

    for (const ItemAmount& it : items) {
        std::uint32_t left = it.count;
        for (std::size_t i = 0; i < trial.size() && left > 0; i++)
            if (to[i]) left -= trial[i].add(it.item, left);
        if (left > 0) return false;
    }
    for (std::size_t i = 0; i < to.size(); i++)
        if (to[i]) *to[i] = std::move(trial[i]);
    return true;
}

} // namespace Nightmourn
