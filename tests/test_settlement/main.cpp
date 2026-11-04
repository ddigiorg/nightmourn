// =============================================================================
// tests/test_settlement/main.cpp
//
// Headless checks for buildings and settlements (World::place_building and
// friends): a banner founds a settlement and its build area; other buildings
// go only inside it, on free tiles, clear of trees; walls join up and come
// apart; removing a building frees its tiles; a fallen banner disbands its
// settlement and a new one adopts what's left; spawning is blocked in the
// build area and its margin.
// =============================================================================

#include "Nezumi/Core/Clock.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/World/World.hpp"
#include <cstdio>  // printf
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS

using namespace Nezumi;
using namespace Nightmourn;

// -----------------------------------------------------------------------------
// Harness
// -----------------------------------------------------------------------------

static int g_failures = 0;

#define CHECK(...)                                                           \
    do {                                                                     \
        if (!(__VA_ARGS__)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__);    \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static const SettlementConfig SETTLEMENT = { .name = "test", .build_radius = 8, .spawn_margin = 4, .min_gap = 2, .inventory_slots = 10 };

static const BuildingConfig BANNER  = { .name = "banner",  .type = BuildingType::Banner,  .index = 0, .footprint = { 1, 1 }, .max_health = 100, .item = {}, .connects = false, .settlement = &SETTLEMENT };
static const BuildingConfig WALL    = { .name = "wall",    .type = BuildingType::Wall,    .index = 1, .footprint = { 1, 1 }, .max_health = 50,  .item = {}, .connects = true,  .settlement = nullptr };
static const BuildingConfig HOUSE   = { .name = "house",   .type = BuildingType::House,   .index = 2, .footprint = { 2, 2 }, .max_health = 80,  .item = {}, .connects = false, .settlement = nullptr };
static const BuildingConfig CRAFTER = { .name = "crafter", .type = BuildingType::Crafter, .index = 3, .footprint = { 3, 3 }, .max_health = 90,  .item = {}, .connects = false, .settlement = nullptr };

static const ResourceConfig TREE = { .name = "tree", .subtype = {}, .variety = {}, .item = {}, .max_amount = 5,
                                     .collision_bounds = { -0.4f, -0.4f, 0.4f, 0.4f } };

static const EntityFactionsMask BLUE = faction_bit(0);
static const EntityFactionsMask RED  = faction_bit(1);

struct Fixture {
    Clock     clock;
    EventBus  bus;
    World     world{ clock, bus };
    SurfaceID surface = world.create_surface();
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_founding() {
    std::printf("founding\n");
    Fixture f;
    World& w = f.world;

    CHECK(w.check_placement(f.surface, HOUSE, { 0, 0 }, BLUE) == PlacementResult::NoSettlement);

    const EntityHandle banner = w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    CHECK(banner);
    const Settlement* s = w.settlement_of(BLUE);
    CHECK(s && s->banner() == banner && s->center() == TileCoord{ 0, 0 });
    CHECK(s && s->build_area() == TileRect::around({ 0, 0 }, 8));
    CHECK(s && s->building_count() == 1 && s->inventory.size() == 10);
    CHECK(w.building_at(f.surface, { 0, 0 }) == banner);
    CHECK(w.settlement_at(f.surface, { 8, -8 }) == s && !w.settlement_at(f.surface, { 9, 0 }));

    // One per faction; another faction's must keep its distance
    CHECK(w.check_placement(f.surface, BANNER, { 30, 0 }, BLUE) == PlacementResult::HasSettlement);
    CHECK(w.check_placement(f.surface, BANNER, { 18, 0 }, RED)  == PlacementResult::TooCloseToOther); // 10..26 + gap
    CHECK(w.check_placement(f.surface, BANNER, { 19, 0 }, RED)  == PlacementResult::Ok);

    w.tick();
    const Building* b = w.get_entity_as<Building>(banner);
    CHECK(b && b->is(BuildingType::Banner) && b->is_alive() && b->settlement_id == s->id());
    CHECK(b && b->position == Position2D(0.5f, 0.5f) && b->health == 100);
}

static void test_placement() {
    std::printf("placement\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);

    // Inside the area, not on the banner, not over the edge
    CHECK(w.check_placement(f.surface, HOUSE, { 0, 0 },  BLUE) == PlacementResult::Occupied);
    CHECK(w.check_placement(f.surface, HOUSE, { 8, 8 },  BLUE) == PlacementResult::OutsideBuildArea);
    CHECK(w.check_placement(f.surface, HOUSE, { 6, 6 },  BLUE) == PlacementResult::Ok);
    CHECK(w.check_placement(f.surface, HOUSE, { 6, 6 },  RED)  == PlacementResult::NoSettlement);

    // A second building on the same tiles before the next tick is refused
    const EntityHandle house = w.place_building(f.surface, HOUSE, { 2, 2 }, BLUE);
    CHECK(house);
    PlacementResult why = PlacementResult::Ok;
    CHECK(!w.place_building(f.surface, CRAFTER, { 1, 1 }, BLUE, &why) && why == PlacementResult::Occupied);
    CHECK(w.building_at(f.surface, { 3, 3 }) == house && !w.building_at(f.surface, { 4, 4 }));

    // Footprint and position: a 2x2 on tiles 2..3 stands at their shared corner, 3, 3
    w.tick();
    const Building* h = w.get_entity_as<Building>(house);
    CHECK(h && h->footprint() == TileRect({ { 2, 2 }, { 3, 3 } }) && h->position == Position2D(3.0f, 3.0f));

    // Trees block, and only the tiles they cover
    w.create_entity<Resource>(f.surface, Position2D(-4.5f, -4.5f), TREE);
    w.tick();
    CHECK(w.check_placement(f.surface, WALL,  { -5, -5 }, BLUE) == PlacementResult::Blocked);
    CHECK(w.check_placement(f.surface, WALL,  { -4, -5 }, BLUE) == PlacementResult::Ok);
    CHECK(w.check_placement(f.surface, HOUSE, { -6, -6 }, BLUE) == PlacementResult::Blocked);

    // Removing frees the tiles at the next command phase
    w.remove_entity(house);
    w.tick();
    CHECK(!w.building_at(f.surface, { 3, 3 }));
    CHECK(w.check_placement(f.surface, CRAFTER, { 1, 1 }, BLUE) == PlacementResult::Ok);
    CHECK(w.settlement_of(BLUE)->building_count() == 1);
}

static void test_walls() {
    std::printf("walls\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);

    // An L: 2,2 - 3,2 - 3,3
    const EntityHandle a = w.place_building(f.surface, WALL, { 2, 2 }, BLUE);
    const EntityHandle b = w.place_building(f.surface, WALL, { 3, 2 }, BLUE);
    const EntityHandle c = w.place_building(f.surface, WALL, { 3, 3 }, BLUE);
    w.place_building(f.surface, HOUSE, { 4, 2 }, BLUE); // next to b, doesn't connect
    w.tick();

    const auto conn = [&](EntityHandle h) { return w.get_entity_as<Building>(h)->connections; };
    CHECK(conn(a) == TileSide::East);
    CHECK(conn(b) == (TileSide::West | TileSide::North));
    CHECK(conn(c) == TileSide::South);

    // Take the corner out: both ends lose it
    w.remove_entity(b);
    w.tick();
    CHECK(conn(a) == 0 && conn(c) == 0);
}

static void test_disband_and_adopt() {
    std::printf("disband and adopt\n");
    Fixture f;
    World& w = f.world;
    const EntityHandle banner = w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    const EntityHandle house  = w.place_building(f.surface, HOUSE,  { 3, 3 }, BLUE);
    w.tick();

    // The banner falls: the settlement goes, the house stands unowned
    w.damage_entity(banner, 1000);
    w.tick(); // damage: queued for removal
    w.tick(); // removed
    CHECK(!w.get_entity(banner));
    CHECK(!w.settlement_of(BLUE));
    const Building* h = w.get_entity_as<Building>(house);
    CHECK(h && !h->settlement_id);
    CHECK(w.check_placement(f.surface, HOUSE, { 6, 6 }, BLUE) == PlacementResult::NoSettlement);

    // Its tiles still block a new banner
    CHECK(w.check_placement(f.surface, BANNER, { 3, 3 }, BLUE) == PlacementResult::Blocked);

    // A new banner nearby adopts it, and its tiles stay taken
    CHECK(w.place_building(f.surface, BANNER, { 1, 0 }, BLUE));
    const Settlement* s = w.settlement_of(BLUE);
    CHECK(s && h->settlement_id == s->id() && s->building_count() == 2);
    CHECK(w.check_placement(f.surface, WALL, { 4, 4 }, BLUE) == PlacementResult::Occupied);
}

static void test_spawn_area() {
    std::printf("spawn area\n");
    Fixture f;
    World& w = f.world;
    CHECK(!w.is_spawn_blocked(f.surface, Position2D(0.5f, 0.5f)));
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);

    // Build area -8..8, plus 4: -12..12
    CHECK( w.is_spawn_blocked(f.surface, Position2D(  0.5f,   0.5f)));
    CHECK( w.is_spawn_blocked(f.surface, Position2D( 12.9f, -11.9f)));
    CHECK(!w.is_spawn_blocked(f.surface, Position2D( 13.1f,   0.0f)));
    CHECK(!w.is_spawn_blocked(f.surface, Position2D(  0.0f, -12.1f)));
}

int main() {
    test_founding();
    test_placement();
    test_walls();
    test_disband_and_adopt();
    test_spawn_area();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
