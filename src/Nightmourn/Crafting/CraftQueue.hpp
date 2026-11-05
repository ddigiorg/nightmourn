// =============================================================================
// Crafting/CraftQueue.hpp
//
// The player's crafting queue (Factorio's): recipes queued from the Items
// menu, made one at a time, in order. The Player owns one, like its
// inventory, so it survives the character dying.
//
//   CraftQueue queue;
//   const CraftPlace here = { .player = &bag, .settlement = stock, .home = home,
//                             .categories = categories };
//   queue.enqueue(recipe, 5, here);   // pays for all 5 now
//   ...each tick...
//   queue.tick(here, speed);          // the front one advances; done: into the bag
//
// Rules:
//
// - Paid when queued. Queuing takes the ingredients at once, from the
//   player's inventory first and then the settlement's (if in reach), so a
//   queued craft can never run short, and two queued crafts can't count the
//   same wood. A craft is only queued if it can be paid for now, its
//   category is offered here (CraftPlace::categories), and the player's
//   inventory has room for what it makes on top of what's already queued.
// - Made in order, the front one only, at the crafter's speed. Its results
//   go into the player's inventory only. With no room by then, it waits at
//   the front, blocked, and finishes as soon as there's room (it looks again
//   only when the inventory changes).
// - Cancelling refunds: to the player's inventory first, then the
//   settlement's (in reach, else the player's home settlement). Whatever
//   can't be refunded for lack of room stays queued.
// - A craft whose category stops being offered (the player walked out of
//   the settlement, or its last workshop came down) is cancelled and
//   refunded the same way, on the tick it happens.
//
// Plain data with no clock of its own: tick() advances it one tick, called
// once per World tick from the game's on_tick, so it's deterministic.
// =============================================================================

#pragma once
#include "Nightmourn/Crafting/Recipe.hpp"
#include "Nightmourn/Item/ItemAmount.hpp"
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t, uint32_t
#include <limits>   // numeric_limits
#include <optional> // optional
#include <span>     // span
#include <vector>   // vector

namespace Nightmourn {

class Inventory;

// Where the player is crafting from, this tick
struct CraftPlace {
    Inventory*        player     = nullptr; // pays first; gets what's made
    Inventory*        settlement = nullptr; // in reach: pays after the player
    Inventory*        home       = nullptr; // the player's settlement, in reach or not: refunds that don't fit the player
    CraftCategoryMask categories = 0;       // what can be made here
};

// Why enqueue() stopped short
enum class CraftRefusal : std::uint8_t {
    None,       // queued all of them
    WrongPlace, // its category isn't offered here
    CantAfford, // short of an ingredient (CraftOutcome::short_of)
    NoRoom,     // the player's inventory couldn't take what it makes
};

struct CraftOutcome {
    std::uint32_t             queued   = 0;
    CraftRefusal              refusal  = CraftRefusal::None;
    std::optional<ItemAmount> short_of = {}; // CantAfford: what, and how many short
};

// What happened during one tick()
struct CraftReport {
    std::uint32_t made      = 0;     // crafts finished
    std::uint32_t cancelled = 0;     // crafts cancelled because their place was left
    bool          blocked   = false; // the front craft just found no room for its results
};

class CraftQueue {
public:

    // -------------------------------------------------------------------------
    // Statics
    // -------------------------------------------------------------------------

    // As many as can be paid for (shift + click)
    static constexpr std::uint32_t ALL = std::numeric_limits<std::uint32_t>::max();

    // Queued crafts of one recipe in a row; the queue merges a craft into
    // the last entry when it's the same recipe
    struct Entry {
        const Recipe* recipe = nullptr;
        std::uint32_t count  = 0;
    };

    // -------------------------------------------------------------------------
    // Queue
    // -------------------------------------------------------------------------

    // Queues up to `count` crafts of `recipe` (ALL: as many as it can),
    // paying for each as it goes. Stops at the first it can't queue and
    // says why.
    CraftOutcome enqueue(const Recipe& recipe, std::uint32_t count, const CraftPlace& place);

    // Can one more craft of `recipe` be queued here? Why not, if not.
    CraftRefusal can_enqueue(const Recipe& recipe, const CraftPlace& place,
                             std::optional<ItemAmount>* short_of = nullptr) const;

    // Cancels up to `count` crafts of entry `entry`, the last queued first,
    // refunding each. Returns how many it cancelled (fewer if refunds ran
    // out of room).
    std::uint32_t cancel(std::size_t entry, std::uint32_t count, const CraftPlace& place);

    // -------------------------------------------------------------------------
    // Tick
    //
    // Advances the front craft by one tick at `speed` (the crafter's: 2 is
    // twice as fast). First cancels whatever can't be made here any more.
    // -------------------------------------------------------------------------

    CraftReport tick(const CraftPlace& place, float speed = 1.0f);

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    std::span<const Entry> entries() const { return entries_; }
    bool                   empty  () const { return entries_.empty(); }

    // Crafts queued, over every entry
    std::uint32_t size() const;

    // The front craft: how far along (0..1), ticks left, and whether it's
    // done but waiting for room
    float         progress    () const;
    std::uint32_t ticks_left  () const { return front_ticks_ > front_done_ ? front_ticks_ - front_done_ : 0; }
    bool          blocked     () const { return blocked_; }

    // Ticks until the whole queue is done at `speed`, from now
    std::uint32_t total_ticks_left(float speed = 1.0f) const;

    // How many of `item` the queued crafts will make
    std::uint32_t pending(ItemID item) const;

    // Ticks one craft of `recipe` takes at `speed`
    static std::uint32_t ticks_for(const Recipe& recipe, float speed);

private:
    bool refund_(const Recipe& recipe, const CraftPlace& place);
    void cancel_unoffered_(const CraftPlace& place, CraftReport& report);
    void reset_front_();

    std::vector<Entry> entries_ = {};

    // The front craft
    std::uint32_t front_done_    = 0;     // ticks worked
    std::uint32_t front_ticks_   = 0;     // ticks it takes (set when it starts)
    bool          blocked_       = false; // done, no room for its results yet
    std::uint32_t blocked_on_    = 0;     // the player inventory's version() when it last found no room
};

} // namespace Nightmourn
