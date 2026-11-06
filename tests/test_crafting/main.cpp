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
#include "Nightmourn/Crafting/CraftSite.hpp"
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


// -----------------------------------------------------------------------------
// CraftSite
// -----------------------------------------------------------------------------

namespace {

// A workshop: makes hand and crafter recipes (planks, swords), at speed 1
BuildingConfig workshop(float speed = 1.0f) {
    BuildingConfig c;
    c.name                = "workshop";
    c.type                = BuildingType::Crafter;
    c.crafting_categories = HAND | CRAFTER;
    c.crafting_speed      = speed;
    return c;
}

const EntityHandle A{ 1, 1 };
const EntityHandle B{ 2, 1 };

// Ticks the site from `from` to `to` inclusive, summing its reports
CraftSite::Report run(CraftSite& site, Inventory& stock, Nezumi::Tick from, Nezumi::Tick to) {
    CraftSite::Report sum;
    for (Nezumi::Tick t = from; t <= to; t++) {
        const CraftSite::Report r = site.tick(t, stock);
        sum.started += r.started;
        sum.made    += r.made;
    }
    return sum;
}

} // namespace

static void test_site_target() {
    std::printf("site: makes up to its target\n");
    Inventory stock(4);
    stock.add(Items::Wood, 10);
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    CHECK(site.machine_count() == 1 && site.categories() == (HAND | CRAFTER));

    // No rules: nothing
    CHECK(site.tick(0, stock).started == 0);

    site.set_max(SWORDS, SWORD, 2);
    CHECK(site.tick(0, stock).started == 1);
    CHECK(stock.count(Items::Wood) == 7 && site.in_progress(SWORD) == 1);
    CHECK(site.machine(A)->state == MachineState::Working && site.machine(A)->progress(10) == 0.5f);

    // 20 ticks a sword; the second starts as the first lands
    CHECK(run(site, stock, 1, 19).made == 0);
    const CraftSite::Report r = site.tick(20, stock);
    CHECK(r.made == 1 && r.started == 1 && stock.count(SWORD) == 1);

    // Two in store: it stops
    const CraftSite::Report rest = run(site, stock, 21, 100);
    CHECK(rest.made == 1 && rest.started == 0 && stock.count(SWORD) == 2 && stock.count(Items::Wood) == 4);
    CHECK(site.machine(A)->state == MachineState::Idle && site.in_progress(SWORD) == 0);

    // Using one up: it makes another
    stock.remove(SWORD, 1);
    CHECK(site.tick(101, stock).started == 1);

    // Target 0 drops the rule; the job under way still finishes
    site.set_max(SWORDS, SWORD, 0);
    CHECK(site.rules().empty() && site.limits(SWORD).max == 0);
    CHECK(run(site, stock, 102, 200).made == 1 && stock.count(SWORD) == 2);
}

static void test_site_floor() {
    std::printf("site: keeps ingredients above their floors\n");
    Inventory stock(4);
    stock.add(Items::Wood, 7);
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    site.set_min(Items::Wood, 5);
    site.set_max(SWORDS, SWORD, 10);
    CHECK(site.limits(Items::Wood).min == 5);

    // 7 - 3 would leave 4, under 5
    CHECK(site.tick(0, stock).started == 0 && stock.count(Items::Wood) == 7);

    // 8 - 3 leaves 5: one, then no more
    stock.add(Items::Wood, 1);
    CHECK(site.tick(1, stock).started == 1 && stock.count(Items::Wood) == 5);
    CHECK(run(site, stock, 2, 100).started == 0 && stock.count(SWORD) == 1);

    // Floor 0: it uses the rest
    site.set_min(Items::Wood, 0);
    CHECK(run(site, stock, 101, 200).made == 1 && stock.count(Items::Wood) == 2);
}

static void test_site_machines_share() {
    std::printf("site: machines don't pile on\n");
    Inventory stock(4);
    stock.add(Items::Wood, 20);
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    site.add_machine(B, ws);

    // One sword wanted: one machine makes it
    site.set_max(SWORDS, SWORD, 1);
    CHECK(site.tick(0, stock).started == 1 && stock.count(Items::Wood) == 17);

    // Two more: both work, in parallel
    site.set_max(SWORDS, SWORD, 3);
    CHECK(site.tick(1, stock).started == 1);
    CHECK(site.machine(A)->state == MachineState::Working && site.machine(B)->state == MachineState::Working);
    CHECK(site.in_progress(SWORD) == 2);
    run(site, stock, 2, 21);
    CHECK(stock.count(SWORD) == 2 && site.in_progress(SWORD) == 1); // A's done, and started the third
}

static void test_site_round_robin() {
    std::printf("site: rules take turns\n");
    Inventory stock(8);
    stock.add(Items::Wood, 50);
    CraftSite site;
    const BuildingConfig ws = workshop(2.0f); // planks 5 ticks, swords 10
    site.add_machine(A, ws);
    site.set_max(PLANKS, PLANK, 100);
    site.set_max(SWORDS, SWORD, 100);

    site.tick(0, stock);
    CHECK(site.machine(A)->recipe == &PLANKS && site.machine(A)->finish == 5);
    run(site, stock, 1, 5);
    CHECK(site.machine(A)->recipe == &SWORDS && site.machine(A)->finish == 15);
    run(site, stock, 6, 15);
    CHECK(site.machine(A)->recipe == &PLANKS && stock.count(PLANK) == 4 && stock.count(SWORD) == 1);

    // A machine only takes rules it offers
    CraftSite hands;
    BuildingConfig bench = workshop();
    bench.crafting_categories = HAND;
    hands.add_machine(A, bench);
    hands.set_max(SWORDS, SWORD, 5);
    CHECK(hands.tick(0, stock).started == 0);
}

static void test_site_priority() {
    std::printf("site: higher priority first\n");
    Inventory stock(8);
    stock.add(Items::Wood, 50);
    CraftSite site;
    const BuildingConfig ws = workshop(2.0f); // planks 5 ticks, swords 10
    site.add_machine(A, ws);
    site.set_max(PLANKS, PLANK, 100);
    site.set_max(SWORDS, SWORD, 100);
    site.set_priority(SWORD, 5);
    CHECK(site.limits(SWORD).priority == 5 && site.limits(PLANK).priority == 0);

    // Swords for as long as they're wanted, though planks came first
    site.tick(0, stock);
    CHECK(site.machine(A)->recipe == &SWORDS);
    run(site, stock, 1, 10);
    CHECK(site.machine(A)->recipe == &SWORDS);

    // Enough swords: planks get their turn
    site.set_max(SWORDS, SWORD, 2);
    run(site, stock, 11, 20);
    CHECK(stock.count(SWORD) == 2 && site.machine(A)->recipe == &PLANKS);
}

static void test_site_sleeps() {
    std::printf("site: sleeps until the storage changes\n");
    Inventory stock(4);
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    site.set_max(SWORDS, SWORD, 1);
    CHECK(site.tick(0, stock).started == 0);
    CHECK(run(site, stock, 1, 50).started == 0);

    // Wood arrives: it wakes and starts
    stock.add(Items::Wood, 3);
    CHECK(site.tick(51, stock).started == 1 && stock.count(Items::Wood) == 0);
}

static void test_site_output_full() {
    std::printf("site: waits for room\n");
    Inventory stock(2);
    stock.add(Items::Wood, 10);
    stock.add(Items::Stone, 10); // both slots taken
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    site.set_max(SWORDS, SWORD, 1);
    CHECK(site.tick(0, stock).started == 1);

    CHECK(run(site, stock, 1, 30).made == 0);
    CHECK(site.machine(A)->state == MachineState::OutputFull && site.machine(A)->progress(30) == 1.0f);

    // Room: delivered on the next tick
    stock.remove(Items::Stone, 10);
    CHECK(site.tick(31, stock).made == 1 && stock.count(SWORD) == 1);
    CHECK(site.machine(A)->state == MachineState::Idle);
}

static void test_site_machine_leaves() {
    std::printf("site: a machine that goes loses or refunds its job\n");
    Inventory stock(4);
    stock.add(Items::Wood, 10);
    CraftSite site;
    const BuildingConfig ws = workshop();
    site.add_machine(A, ws);
    site.set_max(SWORDS, SWORD, 5);
    site.tick(0, stock);
    CHECK(stock.count(Items::Wood) == 7);

    // Deconstructed: the wood comes back, and it starts over (a new job)
    CHECK(site.refund_job(A, stock) && stock.count(Items::Wood) == 10 && site.in_progress(SWORD) == 0);
    CHECK(site.tick(5, stock).started == 1 && site.machine(A)->finish == 25);
    CHECK(run(site, stock, 6, 24).made == 0); // the first job's heap entry is stale
    CHECK(site.tick(25, stock).made == 1);

    // Destroyed: the job's lost, the slot freed and reused
    site.remove_machine(A);
    CHECK(site.machine_count() == 0 && !site.machine(A) && site.in_progress(SWORD) == 0);
    CHECK(stock.count(Items::Wood) == 4);
    CHECK(run(site, stock, 26, 100).made == 0);
    site.add_machine(B, ws);
    CHECK(site.machines().size() == 1 && site.tick(101, stock).started == 1);

    // A refund that doesn't fit keeps the job
    Inventory full(1);
    full.add(Items::Stone, 10);
    CHECK(!site.refund_job(B, full) && site.machine(B)->state == MachineState::Working);
    CHECK(site.refund_job(EntityHandle{ 9, 1 }, full)); // no such machine: nothing to refund
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
    test_site_target();
    test_site_floor();
    test_site_machines_share();
    test_site_round_robin();
    test_site_priority();
    test_site_sleeps();
    test_site_output_full();
    test_site_machine_leaves();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
