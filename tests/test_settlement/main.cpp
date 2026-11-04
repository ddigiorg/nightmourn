// =============================================================================
// tests/test_settlement/main.cpp
//
// Headless checks for buildings and settlements (World::place_building and
// friends): a banner founds a settlement and its build area; other buildings
// go only inside it, on free tiles, clear of trees; walls join up and come
// apart; removing a building frees its tiles; a fallen banner disbands its
// settlement and a new one adopts what's left; spawning is blocked in the
// build area and its margin; figures stop at building footprints and slide
// along them; a gate lets its own faction through; units hit walls; the
// hero hits enemies; night raids spawn only after dark, outside the spawn
// area, a few at a time.
// =============================================================================

#include "Nezumi/Core/Clock.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nightmourn/Building/Building.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/Character/Character.hpp"
#include "Nightmourn/Settlement/RaidSpawner.hpp"
#include "Nightmourn/Unit/Unit.hpp"
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

static const BuildingConfig GATE    = { .name = "gate",    .type = BuildingType::Gate,    .index = 4, .footprint = { 1, 1 }, .max_health = 60,  .item = {}, .connects = true,  .settlement = nullptr };

static const ResourceConfig TREE = { .name = "tree", .subtype = {}, .variety = {}, .item = {}, .max_amount = 5,
                                     .collision_bounds = { -0.4f, -0.4f, 0.4f, 0.4f } };

static constexpr UnitConfig WALKER = { .name = "walker", .move_speed = 6.0f, .aggro_radius = 0.0f };

static const ResourceConfig BARE_TREE = { .name = "bare-tree", .subtype = {}, .variety = {}, .item = {}, .max_amount = 5,
                                          .collision_bounds = {} };
static const BuildingConfig TORCH   = { .name = "torch",   .type = BuildingType::Torch,   .index = 5, .footprint = { 1, 1 }, .max_health = 20,  .item = {}, .connects = false, .solid = false, .settlement = nullptr };

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

static void test_collision() {
    std::printf("collision\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    for (int y = -1; y <= 1; y++) w.place_building(f.surface, WALL, { 3, y }, BLUE);

    const auto run = [&](int ticks) {
        for (int i = 0; i < ticks; i++) {
            w.tick();
            f.bus.flush(EventPhase::TickPost, w.get_tick());
        }
    };

    // Walking diagonally into the wall (tiles x 3, y -1..1): x stops at the
    // wall's face, y keeps going, so it slides up and around the end
    const EntityHandle h = w.create_entity<Unit>(f.surface, Position2D(1.5f, 0.5f), WALKER, BLUE);
    run(1);
    w.get_entity_as<Unit>(h)->move_to({ 6.0f, 4.0f });
    run(20);
    const Unit* u = w.get_entity_as<Unit>(h);
    CHECK(u && u->position.x <= 3.0f - 0.25f && u->position.y > 1.0f);
    run(120);
    u = w.get_entity_as<Unit>(h);
    CHECK(u && u->position.x > 5.0f && u->position.y > 3.0f);

    // A house placed on top of a figure lets it walk out
    const EntityHandle g = w.create_entity<Unit>(f.surface, Position2D(-2.5f, 0.5f), WALKER, BLUE);
    run(1);
    CHECK(w.place_building(f.surface, HOUSE, { -3, 0 }, BLUE));
    run(1);
    w.get_entity_as<Unit>(g)->move_to({ -6.5f, 0.5f });
    run(60);
    const Unit* v = w.get_entity_as<Unit>(g);
    CHECK(v && v->position.x < -5.0f);
}

static void run_ticks(Fixture& f, int ticks) {
    for (int i = 0; i < ticks; i++) {
        f.world.tick();
        f.bus.flush(EventPhase::TickPost, f.world.get_tick());
    }
}

static void test_gate() {
    std::printf("gate\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    // A wall line along x = 3 with a gate in the middle
    w.place_building(f.surface, WALL, { 3, -1 }, BLUE);
    const EntityHandle gate = w.place_building(f.surface, GATE, { 3, 0 }, BLUE);
    w.place_building(f.surface, WALL, { 3, 1 }, BLUE);
    run_ticks(f, 1);
    const Building* g = w.get_entity_as<Building>(gate);
    CHECK(g && g->connections == (1 | 4)); // joins the walls north and south

    // Blue walks through it, red stops at it
    const EntityHandle blue = w.create_entity<Unit>(f.surface, Position2D(1.5f, 0.5f), WALKER, BLUE);
    const EntityHandle red  = w.create_entity<Unit>(f.surface, Position2D(1.5f, 0.5f), WALKER, RED);
    run_ticks(f, 1);
    w.get_entity_as<Unit>(blue)->move_to({ 6.5f, 0.5f });
    w.get_entity_as<Unit>(red)->move_to({ 6.5f, 0.5f });
    run_ticks(f, 90);
    CHECK(w.get_entity_as<Unit>(blue)->position.x > 6.0f);
    CHECK(w.get_entity_as<Unit>(red)->position.x <= 3.0f - 0.25f);
}

static void test_units_hit_walls() {
    std::printf("units hit walls\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    const EntityHandle wall = w.place_building(f.surface, WALL, { 3, 0 }, BLUE);
    static constexpr UnitConfig BRUTE = { .name = "brute", .move_speed = 4.0f, .attack_damage = 10,
                                          .attack_interval = 10, .attack_range = 0.75f, .aggro_radius = 0.0f };
    const EntityHandle u = w.create_entity<Unit>(f.surface, Position2D(6.5f, 0.5f), BRUTE, RED);
    run_ticks(f, 1);
    w.get_entity_as<Unit>(u)->attack(wall); // its center is out of reach from outside the box
    run_ticks(f, 120);
    const Building* b = w.get_entity_as<Building>(wall);
    CHECK(!b || b->health < WALL.max_health);

    // Attack-moving on the banner, a raider walled off from it hits the wall
    static constexpr UnitConfig RAIDER = { .name = "raider", .move_speed = 4.0f, .attack_damage = 10,
                                           .attack_interval = 10, .attack_range = 0.75f, .aggro_radius = 3.0f };
    const EntityHandle ring = w.place_building(f.surface, WALL, { 0, 3 }, BLUE);
    for (int x = -1; x <= 1; x += 2) w.place_building(f.surface, WALL, { x, 3 }, BLUE);
    const EntityHandle r = w.create_entity<Unit>(f.surface, Position2D(0.5f, 7.5f), RAIDER, RED);
    run_ticks(f, 1);
    w.get_entity_as<Unit>(r)->attack_move({ 0.5f, 0.5f });
    run_ticks(f, 240);
    const Building* rb = w.get_entity_as<Building>(ring);
    CHECK(!rb || rb->health < WALL.max_health);
}

static void test_hero_attack() {
    std::printf("hero attack\n");
    Fixture f;
    World& w = f.world;
    static constexpr CharacterConfig HERO = { .name = "hero", .attack_interval = 10, .attack_damage = 25,
                                              .attack_reach = 1.25f };
    static constexpr UnitConfig DUMMY = { .name = "dummy", .max_health = 100, .move_speed = 0.0f, .aggro_radius = 0.0f };
    const EntityHandle hero  = w.create_entity<Character>(f.surface, Position2D(0.0f, 0.0f), HERO, BLUE);
    const EntityHandle enemy = w.create_entity<Unit>(f.surface, Position2D(1.0f, 0.0f), DUMMY, RED);
    const EntityHandle ally  = w.create_entity<Unit>(f.surface, Position2D(-1.0f, 0.0f), DUMMY, BLUE);
    const EntityHandle far   = w.create_entity<Unit>(f.surface, Position2D(0.0f, 5.0f), DUMMY, RED);
    run_ticks(f, 1);

    const auto hit = [&](EntityHandle target, int ticks) {
        w.get_entity_as<Character>(hero)->issue(CharacterCommand{ .attack_target = target });
        run_ticks(f, ticks);
    };
    hit(ally, 30);
    hit(far, 30);
    CHECK(w.get_entity_as<Unit>(ally)->health == 100);  // not an enemy
    CHECK(w.get_entity_as<Unit>(far)->health == 100);   // out of reach
    hit(enemy, 25);                                     // 3 blows: at 0, 10 and 20 ticks
    const Unit* e = w.get_entity_as<Unit>(enemy);
    CHECK(e && e->health == 25);
    hit(enemy, 20);
    CHECK(!w.get_entity_as<Unit>(enemy) || w.get_entity_as<Unit>(enemy)->is_pending_remove());
}

static void test_raids() {
    std::printf("raids\n");
    Fixture f;
    World& w = f.world;
    // Raiders only spawn on loaded chunks: load 4 x 4 around the banner
    f.surface = w.create_surface(SurfaceConfig{ .name = "raids", .initial_chunks = ChunkSpan::span_from_box({ -64.0f, -64.0f, 64.0f, 64.0f }) });
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);
    run_ticks(f, 1);

    static constexpr UnitConfig RAIDER = { .name = "raider", .move_speed = 0.0f, .aggro_radius = 0.0f };
    const RaidConfig raid = { .name = "night", .unit = &RAIDER, .interval = 1.0f, .max_alive = 2,
                              .min_distance = 2.0f, .max_distance = 6.0f, .min_darkness = 0.5f };
    RaidSpawner spawner(raid, RED, 3);
    const float dt = 1.0f / 60.0f;

    // Daylight: nothing
    for (int i = 0; i < 300; i++) { spawner.update(w, f.surface, 0.2f, dt); run_ticks(f, 1); }
    CHECK(spawner.alive() == 0);

    // Dark: the first after half an interval, then one a second, two at most
    for (int i = 0; i < 25; i++) { spawner.update(w, f.surface, 1.0f, dt); run_ticks(f, 1); }
    CHECK(spawner.alive() == 0);
    for (int i = 0; i < 10; i++) { spawner.update(w, f.surface, 1.0f, dt); run_ticks(f, 1); }
    CHECK(spawner.alive() == 1);
    for (int i = 0; i < 300; i++) { spawner.update(w, f.surface, 1.0f, dt); run_ticks(f, 1); }
    CHECK(spawner.alive() == 2);

    // All outside the spawn area, red, and attack-moving on the banner
    const Settlement* s = w.settlement_of(BLUE);
    std::size_t raiders = 0;
    w.each_entity([&](const Entity& e) {
        if (!e.is_type<Unit>()) return;
        raiders++;
        CHECK(!s->spawn_area().contains(TileCoord::from_position(e.position)));
        CHECK(!w.is_spawn_blocked(f.surface, e.position));
        CHECK(e.factions_mask == RED);
        CHECK(static_cast<const Unit&>(e).command().type == UnitCommandType::AttackMove);
    });
    CHECK(raiders == 2);
}

static void test_not_solid() {
    std::printf("not solid\n");
    Fixture f;
    World& w = f.world;
    w.place_building(f.surface, BANNER, { 0, 0 }, BLUE);

    // A tree with no collision box isn't collidable, but still keeps
    // buildings off the tile its trunk stands on
    const EntityHandle tree = w.create_entity<Resource>(f.surface, Position2D(4.5f, 0.5f), BARE_TREE);
    run_ticks(f, 1);
    CHECK(!w.get_entity(tree)->is_collidable());
    CHECK(w.check_placement(f.surface, WALL, { 4, 0 }, BLUE) == PlacementResult::Blocked);
    CHECK(w.check_placement(f.surface, WALL, { 4, 1 }, BLUE) == PlacementResult::Ok);

    // A torch isn't solid either, but takes its tile
    const EntityHandle torch = w.place_building(f.surface, TORCH, { 2, 0 }, BLUE);
    run_ticks(f, 1);
    CHECK(torch && !w.get_entity(torch)->is_collidable());
    CHECK(w.check_placement(f.surface, WALL, { 2, 0 }, BLUE) == PlacementResult::Occupied);

    // A figure walks straight through both
    const EntityHandle u = w.create_entity<Unit>(f.surface, Position2D(0.5f, 0.5f), WALKER, RED);
    run_ticks(f, 1);
    w.get_entity_as<Unit>(u)->move_to({ 7.5f, 0.5f });
    run_ticks(f, 90);
    CHECK(w.get_entity_as<Unit>(u)->position.x > 7.0f);
}

int main() {
    test_founding();
    test_placement();
    test_walls();
    test_disband_and_adopt();
    test_spawn_area();
    test_collision();
    test_gate();
    test_units_hit_walls();
    test_hero_attack();
    test_raids();
    test_not_solid();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
