// =============================================================================
// tests/test_crafting/main.cpp
//
// Headless checks for Crafting/CraftQueue, the player's crafting queue:
// paying when queued (player first, then the settlement), refusing what
// can't be made here, afforded or fit; making the front craft in its time
// at the crafter's speed; waiting, blocked, for room and finishing once
// there is some; cancelling with refunds; and cancelling what can't be made
// any more when the player leaves the settlement.
// =============================================================================

#include "Nightmourn/Crafting/CraftQueue.hpp"
#include "Nightmourn/Item/Inventory.hpp"
#include "Nightmourn/Item/Items.hpp"
#include <array>   // array
#include <cstdio>  // printf
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS

using namespace Nightmourn;

// -----------------------------------------------------------------------------
// Harness
// -----------------------------------------------------------------------------

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

// -----------------------------------------------------------------------------
// Fixture
//
// Wood stacks to 10, planks to 5 (4 per craft), a sword to 1. Categories:
// hand 1, settlement 2, crafter 4.
// -----------------------------------------------------------------------------

namespace {

constexpr ItemID PLANK{ 4 };
constexpr ItemID SWORD{ 5 };

constexpr CraftCategoryMask HAND       = 1u << 0;
constexpr CraftCategoryMask SETTLEMENT = 1u << 1;
constexpr CraftCategoryMask CRAFTER    = 1u << 2;

const std::array PLANK_IN  = { ItemAmount{ Items::Wood, 2 } };
const std::array PLANK_OUT = { ItemAmount{ PLANK, 4 } };
const std::array SWORD_IN  = { ItemAmount{ Items::Wood, 3 } };
const std::array SWORD_OUT = { ItemAmount{ SWORD, 1 } };

// 10 ticks by hand; 20 ticks, only at a workshop
const Recipe PLANKS = { .name = "plank", .id = RecipeID{ 0 }, .ingredients = PLANK_IN, .results = PLANK_OUT, .ticks = 10, .category = HAND };
const Recipe SWORDS = { .name = "sword", .id = RecipeID{ 1 }, .ingredients = SWORD_IN, .results = SWORD_OUT, .ticks = 20, .category = CRAFTER };

void set_test_items() {
    set_items({ { "wood", 10 }, { "stone", 10 }, { "gold", 10 }, { "food", 10 }, { "plank", 5 }, { "sword", 1 } });
}

// Ticks until the queue reports something made, up to `limit`
int ticks_until_made(CraftQueue& q, const CraftPlace& here, float speed = 1.0f, int limit = 1000) {
    for (int t = 1; t <= limit; t++)
        if (q.tick(here, speed).made > 0) return t;
    return -1;
}

} // namespace

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_enqueue_pays() {
    std::printf("queuing pays, player first\n");
    Inventory player(4), stock(4);
    player.add(Items::Wood, 3);
    stock.add(Items::Wood, 10);
    const CraftPlace here = { .player = &player, .settlement = &stock, .home = &stock, .categories = HAND | SETTLEMENT };

    CraftQueue q;
    const CraftOutcome o = q.enqueue(PLANKS, 2, here);
    CHECK(o.queued == 2 && o.refusal == CraftRefusal::None);
    CHECK(player.count(Items::Wood) == 0 && stock.count(Items::Wood) == 9);
    CHECK(q.entries().size() == 1 && q.entries()[0].count == 2 && q.size() == 2); // merged
    CHECK(q.pending(PLANK) == 8);

    // ALL: as many as can be paid for (and fit: 4 slots of 5 planks)
    const CraftOutcome all = q.enqueue(PLANKS, CraftQueue::ALL, here);
    CHECK(all.queued == 3 && all.refusal == CraftRefusal::NoRoom);
    CHECK(stock.count(Items::Wood) == 3 && q.size() == 5 && q.pending(PLANK) == 20);
}

static void test_refusals() {
    std::printf("refusals: place, cost, room\n");
    Inventory player(2), stock(4);
    player.add(Items::Wood, 5);
    CraftQueue q;

    // A sword needs a workshop's category
    const CraftPlace outside = { .player = &player, .settlement = nullptr, .home = &stock, .categories = HAND };
    CHECK(q.enqueue(SWORDS, 1, outside).refusal == CraftRefusal::WrongPlace);
    CHECK(player.count(Items::Wood) == 5 && q.empty());

    // Short: says of what and by how much, and takes nothing
    const CraftPlace inside = { .player = &player, .settlement = &stock, .home = &stock, .categories = HAND | CRAFTER };
    q.enqueue(SWORDS, 1, inside); // 3 wood: leaves 2
    const CraftOutcome o = q.enqueue(SWORDS, 1, inside);
    CHECK(o.queued == 0 && o.refusal == CraftRefusal::CantAfford);
    CHECK(o.short_of && o.short_of->item == Items::Wood && o.short_of->count == 1);
    CHECK(player.count(Items::Wood) == 2);

    // No room: one slot is wood, and the queued sword has the other
    stock.add(Items::Wood, 10);
    CHECK(q.can_enqueue(SWORDS, inside) == CraftRefusal::NoRoom);
    CHECK(q.can_enqueue(PLANKS, inside) == CraftRefusal::None); // 5 planks fit the empty slot
}

static void test_tick_and_speed() {
    std::printf("made in its time, at the crafter's speed\n");
    Inventory player(4);
    player.add(Items::Wood, 6);
    const CraftPlace here = { .player = &player, .settlement = nullptr, .home = nullptr, .categories = HAND };

    CraftQueue q;
    q.enqueue(PLANKS, 3, here);
    CHECK(q.total_ticks_left() == 30);
    CHECK(ticks_until_made(q, here) == 10);
    CHECK(player.count(PLANK) == 4 && q.size() == 2 && q.progress() == 0.0f);

    q.tick(here);
    CHECK(q.progress() > 0.0f && q.ticks_left() == 9 && q.total_ticks_left() == 19);

    // Twice as fast: 5 ticks a craft (the one started keeps its length)
    CHECK(ticks_until_made(q, here, 1.0f) == 9);
    CHECK(ticks_until_made(q, here, 2.0f) == 5);
    CHECK(q.empty() && player.count(PLANK) == 12);
}

static void test_blocked() {
    std::printf("blocked until there's room\n");
    Inventory player(2);
    player.add(Items::Wood, 3);
    const CraftPlace here = { .player = &player, .settlement = nullptr, .home = nullptr, .categories = CRAFTER };

    CraftQueue q;
    CHECK(q.enqueue(SWORDS, 1, here).queued == 1);
    player.add(Items::Stone, 20); // both slots full by the time it's done

    CraftReport r;
    int blocked = 0;
    for (int t = 0; t < 25; t++) {
        r = q.tick(here);
        if (r.blocked) blocked++;
    }
    CHECK(blocked == 1 && q.blocked() && q.size() == 1 && player.count(SWORD) == 0);
    CHECK(q.progress() == 1.0f);

    // Room: it finishes on the next tick
    player.remove(Items::Stone, 10);
    r = q.tick(here);
    CHECK(r.made == 1 && q.empty() && player.count(SWORD) == 1);
}

static void test_cancel() {
    std::printf("cancelling refunds\n");
    Inventory player(4), stock(4);
    player.add(Items::Wood, 10);
    const CraftPlace here = { .player = &player, .settlement = &stock, .home = &stock, .categories = HAND };

    CraftQueue q;
    q.enqueue(PLANKS, 3, here); // 6 wood: player has 4 left
    q.tick(here);
    CHECK(q.progress() > 0.0f);

    // The last queued first; the one under way keeps going
    CHECK(q.cancel(0, 1, here) == 1 && q.size() == 2 && q.progress() > 0.0f);
    CHECK(player.count(Items::Wood) == 6);

    // Refunds that don't fit the player go to the settlement
    player.add(Items::Wood, 4);   // a full stack of 10
    player.add(Items::Stone, 30); // and the other slots
    CHECK(q.cancel(0, 5, here) == 2 && q.empty() && q.progress() == 0.0f);
    CHECK(stock.count(Items::Wood) == 4);

    // No room anywhere: it stays queued
    Inventory tiny(2), none(0);
    tiny.add(Items::Wood, 2);
    const CraftPlace cramped = { .player = &tiny, .settlement = nullptr, .home = &none, .categories = HAND };
    q.enqueue(PLANKS, 1, cramped);
    tiny.add(Items::Stone, 20);
    CHECK(q.cancel(0, 1, cramped) == 0 && q.size() == 1);
}

static void test_leaving() {
    std::printf("leaving the settlement cancels its crafts\n");
    Inventory player(6), stock(4);
    player.add(Items::Wood, 20);
    const CraftPlace inside  = { .player = &player, .settlement = &stock, .home = &stock, .categories = HAND | SETTLEMENT | CRAFTER };
    const CraftPlace outside = { .player = &player, .settlement = nullptr, .home = &stock, .categories = HAND };

    CraftQueue q;
    q.enqueue(SWORDS, 2, inside);
    q.enqueue(PLANKS, 1, inside);
    q.enqueue(SWORDS, 1, inside);
    CHECK(q.entries().size() == 3 && player.count(Items::Wood) == 9);
    q.tick(inside);

    // Out: the swords are refunded, the planks (by hand) carry on
    const CraftReport r = q.tick(outside);
    CHECK(r.cancelled == 3 && q.entries().size() == 1 && q.entries()[0].recipe == &PLANKS);
    CHECK(player.count(Items::Wood) == 18);
    CHECK(ticks_until_made(q, outside) == 9 && player.count(PLANK) == 4); // started on the way out
}

static void test_merge_after_cancel() {
    std::printf("same recipes join up again\n");
    Inventory player(6);
    player.add(Items::Wood, 20);
    const CraftPlace here = { .player = &player, .settlement = nullptr, .home = nullptr, .categories = HAND | CRAFTER };

    CraftQueue q;
    q.enqueue(PLANKS, 1, here);
    q.enqueue(SWORDS, 1, here);
    q.enqueue(PLANKS, 2, here);
    CHECK(q.entries().size() == 3);
    CHECK(q.cancel(1, 1, here) == 1);
    CHECK(q.entries().size() == 1 && q.entries()[0].count == 3);
}

int main() {
    set_test_items();
    test_enqueue_pays();
    test_refusals();
    test_tick_and_speed();
    test_blocked();
    test_cancel();
    test_leaving();
    test_merge_after_cancel();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
