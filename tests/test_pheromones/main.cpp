// =============================================================================
// tests/test_pheromones/main.cpp
//
// Headless checks for chunk pheromones: the integer decay curve, lazy decay
// and saturation, neighbour linking, deposits by units, the gradient, and
// bit-identical reruns.
// =============================================================================

#include "Nezumi/Core/Clock.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nightmourn/Chunk/Pheromones.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/World/World.hpp"
#include <cstdio>  // printf
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS
#include <vector>  // vector

using namespace Nezumi;
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

// Stands still and never fights
static constexpr UnitConfig POST = {
    .name         = "post",
    .aggro_radius = 0.0f,
};

static constexpr UnitConfig WALKER = {
    .name         = "walker",
    .move_speed   = 2.0f,
    .aggro_radius = 0.0f,
};

static constexpr UnitGroupConfig BLOCK = { .spacing = 1.0f };

static constexpr FactionIndex RED = 3;

struct Sim {
    explicit Sim(const WorldConfig& config = {}) : world(clock, bus, config) {}

    Clock     clock;
    EventBus  bus;
    World     world;
    SurfaceID surface = world.create_surface();

    Nightmourn::Surface& surf() { return *world.get_surface(surface); }

    void run(int ticks) {
        for (int i = 0; i < ticks; i++) {
            world.tick();
            bus.flush(EventPhase::TickPost, world.get_tick());
        }
    }

    EntityHandle spawn_unit(Position2D at, const UnitConfig& config = POST, FactionIndex faction = RED) {
        return world.create_entity<Unit>(surface, at, config, faction_bit(faction));
    }
};

// Center of chunk (x, y)
static Position2D chunk_center(int x, int y) {
    return { (static_cast<float>(x) + 0.5f) * CHUNK_SIZE.x, (static_cast<float>(y) + 0.5f) * CHUNK_SIZE.y };
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_decay_curve() {
    std::printf("decay curve\n");
    const PheromoneDecay decay(60, 60);

    CHECK(decay.epoch(0)   == 0);
    CHECK(decay.epoch(59)  == 0);
    CHECK(decay.epoch(60)  == 1);
    CHECK(decay.apply(60000, 0) == 60000);

    // Halves every half life (rounding down at each step costs a little)
    const PheromoneValue half    = decay.apply(60000,  60);
    const PheromoneValue quarter = decay.apply(60000, 120);
    std::printf("  60000 -> %u after one half life, %u after two\n", half, quarter);
    CHECK(half    >= 29700 && half    <= 30000);
    CHECK(quarter >= 14700 && quarter <= 15000);

    // Never grows, and reaches 0 exactly at the end of the table
    PheromoneValue prev = PHEROMONE_MAX;
    bool monotonic = true;
    for (PheromoneEpoch k = 0; k <= decay.lifetime(); k++) {
        const PheromoneValue v = decay.apply(PHEROMONE_MAX, k);
        if (v > prev) monotonic = false;
        prev = v;
    }
    CHECK(monotonic);
    CHECK(decay.apply(PHEROMONE_MAX, decay.lifetime() - 1) > 0);
    CHECK(decay.apply(PHEROMONE_MAX, decay.lifetime())     == 0);
    CHECK(decay.apply(PHEROMONE_MAX, 1000000)              == 0);
}

static void test_lazy_decay_and_saturation() {
    std::printf("lazy decay and saturation\n");
    const PheromoneDecay decay(60, 60);

    Pheromones p;
    p.deposit(RED, 1000, 0, decay);
    CHECK(p.get(RED, 0, decay)  == 1000);
    CHECK(p.get(0,   0, decay)  == 0);
    CHECK(p.get(RED, 60, decay) == decay.apply(1000, 60));

    // A read never writes; a deposit writes every faction back first
    CHECK(p.epoch == 0);
    p.deposit(0, 10, 60, decay);
    CHECK(p.epoch == 60);
    CHECK(p.values[RED] == decay.apply(1000, 60));

    // Saturates instead of wrapping
    p.deposit(RED, 65000, 60, decay);
    p.deposit(RED, 65000, 60, decay);
    CHECK(p.get(RED, 60, decay) == PHEROMONE_MAX);
}

static void test_neighbour_links() {
    std::printf("neighbour links\n");
    Sim sim;
    Nightmourn::Surface& s = sim.surf();

    // Out of order, with a gap, so links are made from both sides
    Chunk* a = s.create_chunk({ 0, 0 });
    Chunk* c = s.create_chunk({ 2, 0 });
    Chunk* b = s.create_chunk({ 1, 0 });
    Chunk* d = s.create_chunk({ 1, 1 });

    CHECK(b->neighbour(3) == a); // west
    CHECK(a->neighbour(4) == b); // east
    CHECK(b->neighbour(4) == c);
    CHECK(c->neighbour(3) == b);
    CHECK(b->neighbour(6) == d); // north (+y)
    CHECK(d->neighbour(1) == b);
    CHECK(a->neighbour(7) == d); // north-east
    CHECK(d->neighbour(0) == a);
    CHECK(c->neighbour(5) == d); // north-west
    CHECK(d->neighbour(2) == c);
    CHECK(a->neighbour(0) == nullptr);
    CHECK(a->neighbour(3) == nullptr);

    // Every link is two-way
    bool symmetric = true;
    for (const Chunk* chunk : { a, b, c, d })
        for (std::size_t i = 0; i < NEIGHBOUR_COUNT; i++)
            if (const Chunk* n = chunk->neighbour(i); n && n->neighbour(opposite_neighbour(i)) != chunk)
                symmetric = false;
    CHECK(symmetric);
}

static void test_units_deposit() {
    std::printf("units deposit\n");
    Sim sim;
    sim.surf().create_chunk({ 1, 0 });
    const EntityHandle unit = sim.spawn_unit(chunk_center(0, 0));

    sim.run(600); // 10 s: 20 deposits of 64, less a little decay

    const PheromoneValue here = sim.surf().pheromone_at(chunk_center(0, 0), RED);
    std::printf("  one unit, 10 s: %u\n", here);
    CHECK(here > 1100 && here <= 1280);
    CHECK(sim.surf().pheromone_at(chunk_center(0, 0), 0) == 0); // other factions
    CHECK(sim.surf().pheromone_at(chunk_center(1, 0), RED) == 0); // neighbours
    CHECK(sim.surf().pheromone_at(chunk_center(5, 5), RED) == 0); // no chunk

    // The gradient points from the empty neighbour toward the unit, and is
    // flat at the peak
    const ChunkOffset toward = sim.surf().pheromone_gradient(chunk_center(1, 0), RED);
    const ChunkOffset peak   = sim.surf().pheromone_gradient(chunk_center(0, 0), RED);
    CHECK(toward.x == -1 && toward.y == 0);
    CHECK(peak.x   ==  0 && peak.y   == 0);

    // Without the unit it decays: halved after one half life (60 s)
    sim.world.remove_entity(unit);
    sim.run(1);
    const PheromoneValue before = sim.surf().pheromone_at(chunk_center(0, 0), RED);
    sim.run(3600);
    const PheromoneValue after = sim.surf().pheromone_at(chunk_center(0, 0), RED);
    std::printf("  %u -> %u over one half life\n", before, after);
    CHECK(after * 2 >= before - 40 && after * 2 <= before + 2);
}

static void test_deposits_disabled() {
    std::printf("deposits disabled\n");
    Sim sim(WorldConfig{ .pheromone_deposit_interval = 0 });
    sim.spawn_unit(chunk_center(0, 0));
    sim.run(600);
    CHECK(sim.surf().pheromone_at(chunk_center(0, 0), RED) == 0);
}

// A group marches east; returns its trail, chunk (0..4, 0)
static std::vector<PheromoneValue> march(ChunkOffset* gradient_out) {
    Sim sim;
    const EntityHandle g = sim.world.create_entity<UnitGroup>(sim.surface, Position2D{ 4.0f, 8.0f }, BLOCK, faction_bit(RED));
    for (int i = 0; i < 12; i++) {
        const Position2D p = { 2.0f + static_cast<float>(i % 4) * 1.3f, 6.0f + static_cast<float>(i / 4) * 1.3f };
        const EntityHandle u = sim.spawn_unit(p, WALKER);
        sim.world.get_entity_as<UnitGroup>(g)->add_member(*sim.world.get_entity_as<Unit>(u));
    }
    sim.world.get_entity_as<UnitGroup>(g)->move_to({ 150.0f, 8.0f });
    sim.run(60 * 90);

    std::vector<PheromoneValue> trail;
    for (int x = 0; x <= 4; x++) trail.push_back(sim.surf().pheromone_at(chunk_center(x, 0), RED));
    if (gradient_out) *gradient_out = sim.surf().pheromone_gradient(chunk_center(1, 0), RED);
    return trail;
}

static void test_trail_and_rerun() {
    std::printf("trail and rerun\n");
    ChunkOffset gradient;
    const std::vector<PheromoneValue> first  = march(&gradient);
    const std::vector<PheromoneValue> second = march(nullptr);

    std::printf("  trail:");
    for (PheromoneValue v : first) std::printf(" %u", v);
    std::printf("  gradient at chunk 1: (%d, %d)\n", gradient.x, gradient.y);

    bool laid = true;
    for (PheromoneValue v : first) if (v == 0) laid = false;
    CHECK(laid);
    CHECK(first[0] < first[4]); // older trail has decayed more
    CHECK(gradient.x == 1);     // follow it toward the group
    CHECK(first == second);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    test_decay_curve();
    test_lazy_decay_and_saturation();
    test_neighbour_links();
    test_units_deposit();
    test_deposits_disabled();
    test_trail_and_rerun();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all checks passed\n");
    return EXIT_SUCCESS;
}
