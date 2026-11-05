// =============================================================================
// Item/Inventory.hpp
//
// A fixed number of item slots, each empty or holding one stack: one kind of
// item, 1 to item_stack_size() of it (Factorio's inventories). The player's
// is 10 x 4; chests and carts can use the same class with their own size.
//
//   Inventory bag(40);
//   bag.add(Items::Wood, 1);            // tops up a wood stack, else a free slot
//   bag.count(Items::Wood);             // over every slot
//   bag.move(from, to);                 // merges, or swaps
//
// The player's inventory and a settlement's are both Inventories, so moving
// stacks between two of them (Inventory::move, Inventory::transfer) and
// paying a cost out of several in turn (pay, give) work for any pair: a
// chest, a cart, a barracks' stock later.
//
//   Inventory* from[] = { &settlement.inventory, &player.inventory() };
//   if (pay(building.cost, from)) ...place it...  // settlement first
//
// Plain data, not an entity: whoever owns it (the Player) changes it outside
// the World's tick, e.g. when OnEntityMined credits the player's character.
//
// count() is O(1): each inventory keeps a running total per item next to
// its slots. version() changes whenever anything in it does, so something
// waiting on it (a craft waiting for room, later a sleeping crafter) can
// look again only when there's something new to see.
// =============================================================================

#pragma once
#include "Nightmourn/Item/ItemAmount.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <cstddef>  // size_t
#include <cstdint>  // uint16_t, uint32_t
#include <limits>   // numeric_limits
#include <optional> // optional
#include <span>     // span
#include <vector>   // vector

namespace Nightmourn {

struct ItemStack {
    ItemID item  = {};
    std::uint16_t  count = 0; // 1 .. item_stack_size(item)
};

class Inventory {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit Inventory(std::size_t slot_count) : slots_(slot_count) {}

    // -------------------------------------------------------------------------
    // Slots
    // -------------------------------------------------------------------------

    std::size_t size() const { return slots_.size(); }

    const std::optional<ItemStack>& slot(std::size_t i) const { return slots_[i]; }

    // -------------------------------------------------------------------------
    // Items
    // -------------------------------------------------------------------------

    // Total of `item` over every slot, O(1)
    std::uint32_t count(ItemID item) const {
        return item && item.value() < totals_.size() ? totals_[item.value()] : 0;
    }

    // Changes whenever a slot does (never goes back to an earlier value)
    std::uint32_t version() const { return version_; }

    // How many of `item` would fit: room on its stacks plus empty slots
    std::uint32_t room_for(ItemID item) const;
    bool          can_add (ItemID item) const { return room_for(item) > 0; }

    // Adds up to `amount` of `item`: first onto its stacks that aren't full,
    // in slot order, then into empty slots. Returns how many fit.
    std::uint32_t add(ItemID item, std::uint32_t amount);

    // Takes up to `amount` of `item`, from the last slots first (so the
    // stacks at the top stay put). Returns how many were taken.
    std::uint32_t remove(ItemID item, std::uint32_t amount);

    // -------------------------------------------------------------------------
    // Rearranging
    // -------------------------------------------------------------------------

    // Moves slot `from` onto slot `to`, the way dragging one onto the other
    // does: into an empty slot, onto a stack of the same item (as much as
    // fits; the rest stays behind), or swapped with a different item
    void move(std::size_t from, std::size_t to) { move(*this, from, *this, to); }

    // The same between two inventories (or within one): slot `from_slot` of
    // `from` onto slot `to_slot` of `to`
    static void move(Inventory& from, std::size_t from_slot, Inventory& to, std::size_t to_slot);

    // Sends up to `amount` of the stack in `from`'s slot `slot` to `to`, the
    // way add() puts it there (topping up its stacks, then empty slots).
    // Whatever doesn't fit stays in the slot. Returns how many moved.
    static std::uint32_t transfer(Inventory& from, std::size_t slot, Inventory& to,
                                  std::uint32_t amount = std::numeric_limits<std::uint32_t>::max());

private:
    // Keep totals_ and version_ in step with a slot change
    void counted_(ItemID item, std::uint32_t added, std::uint32_t removed);

    std::vector<std::optional<ItemStack>> slots_;
    std::vector<std::uint32_t>            totals_  = {}; // by ItemID, grown as items arrive
    std::uint32_t                         version_ = 0;
};

// -----------------------------------------------------------------------------
// Several inventories at once
//
// `from` / `to` are tried in order, so the first is drawn on (or filled)
// first: a building's cost comes out of the settlement before the player.
// Null entries are skipped, for an inventory that's out of reach.
// -----------------------------------------------------------------------------

// Total of `item` over all of them
std::uint32_t count(std::span<Inventory* const> from, ItemID item);

// The first part of `cost` they don't have enough of, and how many short
// they are; none if they can pay all of it
std::optional<ItemAmount> shortfall(std::span<const ItemAmount> cost, std::span<Inventory* const> from);

// Takes all of `cost`, or nothing if they can't pay it all. True if paid.
bool pay(std::span<const ItemAmount> cost, std::span<Inventory* const> from);

// Adds all of `items`, overflowing from one inventory into the next, or
// nothing if they don't all fit. True if given.
bool give(std::span<const ItemAmount> items, std::span<Inventory* const> to);

} // namespace Nightmourn
