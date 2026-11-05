// =============================================================================
// Crafting/CraftQueue.cpp
// =============================================================================

#include "Nightmourn/Crafting/CraftQueue.hpp"
#include "Nightmourn/Item/Inventory.hpp" // give, pay, shortfall
#include <algorithm> // max, min
#include <array>     // array
#include <cmath>     // ceil

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Queue
// -----------------------------------------------------------------------------

CraftRefusal CraftQueue::can_enqueue(const Recipe& recipe, const CraftPlace& place,
                                     std::optional<ItemAmount>* short_of) const
{
    if (!place.player || !recipe.can_craft_in(place.categories)) return CraftRefusal::WrongPlace;

    Inventory* const from[] = { place.player, place.settlement };
    if (const std::optional<ItemAmount> s = shortfall(recipe.ingredients, from)) {
        if (short_of) *short_of = s;
        return CraftRefusal::CantAfford;
    }

    // Room for what it makes, on top of what the queue will make already
    for (const ItemAmount& r : recipe.results)
        if (place.player->room_for(r.item) < pending(r.item) + recipe.makes(r.item)) return CraftRefusal::NoRoom;
    return CraftRefusal::None;
}

CraftOutcome CraftQueue::enqueue(const Recipe& recipe, std::uint32_t count, const CraftPlace& place) {
    CraftOutcome out;
    Inventory* const from[] = { place.player, place.settlement };
    while (out.queued < count) {
        out.refusal = can_enqueue(recipe, place, &out.short_of);
        if (out.refusal != CraftRefusal::None) break;
        pay(recipe.ingredients, from);

        if (!entries_.empty() && entries_.back().recipe == &recipe) entries_.back().count++;
        else                                                         entries_.push_back({ &recipe, 1 });
        out.queued++;
    }
    return out;
}

std::uint32_t CraftQueue::cancel(std::size_t entry, std::uint32_t count, const CraftPlace& place) {
    if (entry >= entries_.size()) return 0;

    std::uint32_t cancelled = 0;
    while (cancelled < count && entries_[entry].count > 0) {
        if (!refund_(*entries_[entry].recipe, place)) break;
        entries_[entry].count--;
        cancelled++;
    }
    if (entries_[entry].count > 0) return cancelled;

    // The entry's gone: the front craft (if it was this one) starts over
    // with the next, and neighbours of one recipe join up again
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(entry));
    if (entry == 0) reset_front_();
    if (entry > 0 && entry < entries_.size() && entries_[entry - 1].recipe == entries_[entry].recipe) {
        entries_[entry - 1].count += entries_[entry].count;
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(entry));
    }
    return cancelled;
}

bool CraftQueue::refund_(const Recipe& recipe, const CraftPlace& place) {
    if (recipe.ingredients.empty()) return true;
    Inventory* const to[] = { place.player, place.settlement ? place.settlement : place.home };
    return give(recipe.ingredients, to);
}

void CraftQueue::reset_front_() {
    front_done_  = 0;
    front_ticks_ = 0;
    blocked_     = false;
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void CraftQueue::cancel_unoffered_(const CraftPlace& place, CraftReport& report) {
    for (std::size_t i = entries_.size(); i-- > 0;) {
        if (i >= entries_.size() || entries_[i].recipe->can_craft_in(place.categories)) continue;
        report.cancelled += cancel(i, entries_[i].count, place);
    }
}

CraftReport CraftQueue::tick(const CraftPlace& place, float speed) {
    CraftReport report;
    if (!place.player) return report;

    cancel_unoffered_(place, report);
    if (entries_.empty()) return report;

    // One that couldn't be refunded (no room) can't be made either; it
    // waits until the place offers it again or the refund fits
    Entry& front = entries_.front();
    if (!front.recipe->can_craft_in(place.categories)) return report;

    if (front_ticks_ == 0) front_ticks_ = ticks_for(*front.recipe, speed);

    if (!blocked_) {
        if (++front_done_ < front_ticks_) return report;
    } else if (place.player->version() == blocked_on_) {
        return report; // nothing's changed since there was no room
    }

    // Done: its results go to the player, all or nothing
    Inventory* const to[] = { place.player };
    if (!give(front.recipe->results, to)) {
        report.blocked = !blocked_;
        blocked_       = true;
        blocked_on_    = place.player->version();
        return report;
    }
    report.made++;
    if (--front.count == 0) entries_.erase(entries_.begin());
    reset_front_();
    return report;
}

// -----------------------------------------------------------------------------
// Observers
// -----------------------------------------------------------------------------

std::uint32_t CraftQueue::size() const {
    std::uint32_t n = 0;
    for (const Entry& e : entries_) n += e.count;
    return n;
}

float CraftQueue::progress() const {
    if (blocked_) return 1.0f;
    if (front_ticks_ == 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(front_done_) / static_cast<float>(front_ticks_));
}

std::uint32_t CraftQueue::total_ticks_left(float speed) const {
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < entries_.size(); i++) {
        const std::uint32_t each = ticks_for(*entries_[i].recipe, speed);
        if (i > 0) { total += each * entries_[i].count; continue; }
        total += (front_ticks_ > 0 ? ticks_left() : each) + each * (entries_[i].count - 1);
    }
    return total;
}

std::uint32_t CraftQueue::pending(ItemID item) const {
    std::uint32_t n = 0;
    for (const Entry& e : entries_) n += e.recipe->makes(item) * e.count;
    return n;
}

std::uint32_t CraftQueue::ticks_for(const Recipe& recipe, float speed) {
    const float ticks = static_cast<float>(recipe.ticks) / std::max(speed, 0.01f);
    return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::ceil(ticks)));
}

} // namespace Nightmourn
