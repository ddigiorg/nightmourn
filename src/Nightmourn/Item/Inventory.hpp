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
// Plain data, not an entity: whoever owns it (the Player) changes it outside
// the World's tick, e.g. when OnEntityMined credits the player's character.
// =============================================================================

#pragma once
#include "Nightmourn/Item/ItemID.hpp"
#include <cstddef>  // size_t
#include <cstdint>  // uint16_t, uint32_t
#include <optional> // optional
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

    // Total of `item` over every slot
    std::uint32_t count(ItemID item) const;

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
    void move(std::size_t from, std::size_t to);

private:
    std::vector<std::optional<ItemStack>> slots_;
};

} // namespace Nightmourn
