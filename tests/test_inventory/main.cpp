// =============================================================================
// tests/test_inventory/main.cpp
//
// Headless checks for Item/Inventory: stacks moving within one inventory and
// between two (dragging between the player's and a settlement's), sending a
// stack across, and paying and refunding a cost out of several inventories
// in order, all or nothing.
// =============================================================================

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

// Wood stacks to 10, stone to 5, so a few slots fill up fast
static void set_test_items() {
    set_items({ { "wood", 10 }, { "stone", 5 }, { "gold", 1 }, { "food", 1 } });
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_move_between() {
    std::printf("move between inventories\n");
    Inventory a(4), b(4);
    a.add(Items::Wood, 7);  // a: [7 wood]
    b.add(Items::Wood, 6);  // b: [6 wood]
    b.add(Items::Stone, 2); // b: [6 wood][2 stone]

    // Onto the same item: as much as fits, the rest stays
    Inventory::move(a, 0, b, 0);
    CHECK(b.slot(0)->count == 10 && a.slot(0)->count == 3);

    // Onto a different item: swapped
    Inventory::move(a, 0, b, 1);
    CHECK(a.slot(0)->item == Items::Stone && a.slot(0)->count == 2);
    CHECK(b.slot(1)->item == Items::Wood  && b.slot(1)->count == 3);

    // Into an empty slot
    Inventory::move(a, 0, b, 3);
    CHECK(!a.slot(0) && b.slot(3)->item == Items::Stone);

    // Out of range or from an empty slot: nothing
    Inventory::move(a, 0, b, 2);
    Inventory::move(b, 9, a, 0);
    CHECK(!a.slot(0) && !b.slot(2) && b.count(Items::Wood) == 13);
}

static void test_transfer() {
    std::printf("transfer a stack\n");
    Inventory from(2), to(2);
    from.add(Items::Wood, 10);
    to.add(Items::Stone, 5);
    to.add(Items::Wood, 8); // to: [5 stone][8 wood], so 2 wood fit

    CHECK(Inventory::transfer(from, 0, to) == 2);
    CHECK(from.slot(0)->count == 8 && to.count(Items::Wood) == 10);

    // Half of what's left, to an inventory with room
    Inventory big(4);
    CHECK(Inventory::transfer(from, 0, big, 4) == 4 && from.slot(0)->count == 4);
    CHECK(Inventory::transfer(from, 0, big) == 4 && !from.slot(0));
    CHECK(Inventory::transfer(from, 0, big) == 0);
    CHECK(Inventory::transfer(big, 0, big) == 0); // not onto itself
}

static void test_pay() {
    std::printf("pay a cost, settlement first\n");
    Inventory stock(4), carried(4);
    stock.add(Items::Wood, 3);
    carried.add(Items::Wood, 5);
    Inventory* const from[] = { &stock, &carried };

    const std::array house = { ItemAmount{ Items::Wood, 4 } };
    CHECK(!shortfall(house, from));
    CHECK(pay(house, from));
    CHECK(stock.count(Items::Wood) == 0 && carried.count(Items::Wood) == 4);

    // Short: says by how much, and takes nothing
    const std::array crafter = { ItemAmount{ Items::Wood, 9 } };
    const auto need = shortfall(crafter, from);
    CHECK(need && need->item == Items::Wood && need->count == 5);
    CHECK(!pay(crafter, from) && carried.count(Items::Wood) == 4);

    // Two items: all or nothing
    const std::array both = { ItemAmount{ Items::Wood, 2 }, ItemAmount{ Items::Stone, 1 } };
    CHECK(!pay(both, from) && carried.count(Items::Wood) == 4);

    // A missing (out of reach) inventory is skipped
    Inventory* const only_carried[] = { nullptr, &carried };
    CHECK(pay(std::array{ ItemAmount{ Items::Wood, 1 } }, only_carried) && carried.count(Items::Wood) == 3);
    CHECK(pay(std::span<const ItemAmount>{}, from)); // free
}

static void test_give() {
    std::printf("give, overflowing into the next\n");
    Inventory stock(1), carried(2);
    Inventory* const to[] = { &stock, &carried };

    // 10 fit in the stock's one slot, the rest overflow to the hero
    CHECK(give(std::array{ ItemAmount{ Items::Wood, 14 } }, to));
    CHECK(stock.count(Items::Wood) == 10 && carried.count(Items::Wood) == 4);

    // 6 more wood tops up the hero's stack, but 10 stone need two slots and
    // only one is free: nothing given
    const std::array too_much = { ItemAmount{ Items::Wood, 6 }, ItemAmount{ Items::Stone, 10 } };
    CHECK(!give(too_much, to));
    CHECK(stock.count(Items::Wood) == 10 && carried.count(Items::Wood) == 4 && carried.count(Items::Stone) == 0);

    CHECK(give(std::array{ ItemAmount{ Items::Wood, 6 } }, to) && carried.count(Items::Wood) == 10);
}

int main() {
    set_test_items();
    test_move_between();
    test_transfer();
    test_pay();
    test_give();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
