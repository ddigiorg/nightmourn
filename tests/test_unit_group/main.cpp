// =============================================================================
// tests/test_unit_group/main.cpp
//
// Headless checks for UnitGroup: marching into formation, MoveTo ignoring
// enemies, AttackMove engaging and reforming, straggler slowdown, empty
// groups removing themselves, and bit-identical reruns.
// =============================================================================

#include "Nezumi/Core/Clock.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/World/World.hpp"
#include <algorithm> // max, min
#include <cmath>    // sqrt
#include <cstdio>   // printf
#include <cstdlib>  // EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>  // memcmp
#include <vector>   // vector

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

static constexpr UnitConfig FOOT = {
    .name         = "foot",
    .move_speed   = 2.0f,
    .aggro_radius = 5.0f,
};

static constexpr UnitConfig ARCHER = {
    .name         = "archer",
    .move_speed   = 2.0f,
    .attack_range = 4.0f,
    .aggro_radius = 5.0f,
};

// Stands still and never fights back
static constexpr UnitConfig DUMMY = {
    .name         = "dummy",
    .max_health   = 1,
    .aggro_radius = 0.0f,
};

static constexpr UnitGroupConfig BLOCK = { .spacing = 1.0f };

struct Sim {
    Clock     clock;
    EventBus  bus;
    World     world{ clock, bus };
    SurfaceID surface = world.create_surface();

    void run(int ticks) {
        for (int i = 0; i < ticks; i++) {
            world.tick();
            bus.flush(EventPhase::TickPost, world.get_tick());
        }
    }

    EntityHandle spawn_group(Position2D at, EntityFactionsMask faction = EntityFactionsMask::Faction00) {
        return world.create_entity<UnitGroup>(surface, at, BLOCK, faction);
    }

    EntityHandle spawn_unit(Position2D at, const UnitConfig& config = FOOT,
                            EntityFactionsMask faction = EntityFactionsMask::Faction00) {
        return world.create_entity<Unit>(surface, at, config, faction);
    }

    UnitGroup* group(EntityHandle h) { return world.get_entity_as<UnitGroup>(h); }
    Unit*      unit (EntityHandle h) { return world.get_entity_as<Unit>(h);      }

    // Group of `count` units on a loose grid around `at`
    EntityHandle spawn_filled_group(Position2D at, int count, std::vector<EntityHandle>* out = nullptr) {
        const EntityHandle g = spawn_group(at);
        for (int i = 0; i < count; i++) {
            const Position2D p = at + Position2D{ static_cast<float>(i % 5) * 1.3f - 2.6f,
                                                  static_cast<float>(i / 5) * 1.3f - 2.0f };
            const EntityHandle u = spawn_unit(p, (i % 4 == 3) ? ARCHER : FOOT);
            group(g)->add_member(*unit(u));
            if (out) out->push_back(u);
        }
        return g;
    }

    float max_slot_error(EntityHandle g) {
        const UnitGroup* grp = group(g);
        float worst = 0.0f;
        for (size_t i = 0; i < grp->members().size(); i++) {
            const Unit* u = unit(grp->members()[i]);
            if (!u) continue;
            const Position2D slot = grp->slot_position(world.get_tick(), static_cast<uint16_t>(i));
            worst = std::max(worst, std::sqrt(u->position.distance2(slot)));
        }
        return worst;
    }
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_march_into_formation() {
    std::printf("march into formation\n");
    Sim sim;
    std::vector<EntityHandle> units;
    const EntityHandle g = sim.spawn_filled_group({ 0.0f, 0.0f }, 12, &units);
    sim.group(g)->move_to({ 30.0f, 0.0f });

    sim.run(60 * 30);

    const UnitGroup* grp = sim.group(g);
    CHECK(grp != nullptr);
    CHECK(grp->state() == UnitGroupState::Formed);
    CHECK(grp->command().type == UnitCommandType::Stop);
    CHECK(grp->position.distance2({ 30.0f, 0.0f }) < 0.01f);
    CHECK(sim.max_slot_error(g) < 0.1f);
    CHECK(grp->collision_bounds.size().x > 0.0f); // tracks the members

    // Facing +x: archers (every 4th unit) stand behind the foot soldiers
    float foot_x = 1e9f, archer_x = -1e9f;
    for (EntityHandle h : units) {
        const Unit* u = sim.unit(h);
        CHECK(u->state() == UnitState::Idle);
        CHECK(u->group() == g);
        if (&u->config() == &ARCHER) archer_x = std::max(archer_x, u->position.x);
        else                         foot_x   = std::min(foot_x,   u->position.x);
    }
    CHECK(archer_x <= foot_x + 1e-3f); // a mixed row may hold both
}

static void test_move_to_ignores_enemies() {
    std::printf("move_to ignores enemies\n");
    Sim sim;
    const EntityHandle g = sim.spawn_filled_group({ 0.0f, 0.0f }, 8);
    sim.spawn_unit({ 15.0f, 8.0f }, DUMMY, EntityFactionsMask::Faction01);
    sim.group(g)->move_to({ 30.0f, 0.0f });

    bool engaged = false;
    for (int i = 0; i < 60 * 25; i++) {
        sim.run(1);
        engaged |= sim.group(g)->state() == UnitGroupState::Engaged;
    }
    CHECK(!engaged);
    CHECK(sim.group(g)->position.distance2({ 30.0f, 0.0f }) < 0.01f);
}

static void test_attack_move_engages_and_reforms() {
    std::printf("attack_move engages, reforms, marches on\n");
    Sim sim;
    const EntityHandle g     = sim.spawn_filled_group({ 0.0f, 0.0f }, 8);
    const EntityHandle dummy = sim.spawn_unit({ 15.0f, 3.0f }, DUMMY, EntityFactionsMask::Faction01);
    sim.group(g)->attack_move({ 30.0f, 0.0f });

    bool engaged = false, reformed_after = false;
    for (int i = 0; i < 60 * 40; i++) {
        sim.run(1);
        const UnitGroupState s = sim.group(g)->state();
        if (s == UnitGroupState::Engaged) engaged = true;
        if (engaged && s == UnitGroupState::Reforming) reformed_after = true;
    }
    CHECK(engaged);
    CHECK(reformed_after);
    CHECK(sim.world.get_entity(dummy) == nullptr); // killed
    CHECK(sim.group(g)->state() == UnitGroupState::Formed);
    CHECK(sim.group(g)->position.distance2({ 30.0f, 0.0f }) < 0.01f);
    CHECK(sim.max_slot_error(g) < 0.1f);
}

static void test_straggler_slowdown() {
    std::printf("straggler slowdown and catch-up\n");
    Sim sim;
    std::vector<EntityHandle> units;
    const EntityHandle g = sim.spawn_filled_group({ 0.0f, 0.0f }, 8, &units);
    sim.run(60 * 6); // assemble
    CHECK(sim.group(g)->state() == UnitGroupState::Formed);

    // March, then leave half the members far behind (as if blocked): the
    // mean lag jumps and the anchor slows toward min_speed_factor
    sim.group(g)->move_to({ 60.0f, 0.0f });
    sim.run(60 * 3);
    const float full = sim.group(g)->velocity.x;
    CHECK(full > 0.0f);

    // Half the members leave (their own command) and fall back; re-adding
    // them makes them stragglers without breaking any rule
    std::vector<EntityHandle> laggards;
    for (size_t i = 0; i < units.size(); i += 2) laggards.push_back(units[i]);
    for (EntityHandle h : laggards) {
        Unit* u = sim.unit(h);
        u->move_to(u->position - Position2D{ 8.0f, 0.0f });
    }
    sim.run(60 * 3);
    for (EntityHandle h : laggards) sim.group(g)->add_member(*sim.unit(h));
    sim.run(3);

    const float slowed = sim.group(g)->velocity.x;
    CHECK(sim.group(g)->straggler_count() > 0);
    CHECK(slowed < full * 0.5f);

    // They catch up and the march recovers
    sim.run(60 * 8);
    CHECK(sim.group(g)->straggler_count() == 0);
    CHECK(sim.group(g)->velocity.x > full * 0.95f);
}

static void test_empty_group_removes_itself() {
    std::printf("empty group removes itself\n");
    Sim sim;
    std::vector<EntityHandle> units;
    const EntityHandle g = sim.spawn_filled_group({ 0.0f, 0.0f }, 4, &units);
    sim.run(2);

    for (EntityHandle h : units) sim.unit(h)->stop(); // own command: leaves
    sim.run(2);
    CHECK(sim.world.get_entity(g) == nullptr);
    for (EntityHandle h : units) CHECK(sim.unit(h)->group() == EntityHandle{});
}

static void test_merge() {
    std::printf("merge\n");
    Sim sim;
    const EntityHandle a = sim.spawn_filled_group({ 0.0f, 0.0f }, 6);
    const EntityHandle b = sim.spawn_filled_group({ 0.0f, 10.0f }, 5);
    sim.run(2);

    sim.group(a)->merge_from(sim.world, *sim.group(b));
    sim.run(2);
    CHECK(sim.world.get_entity(b) == nullptr);
    CHECK(sim.group(a)->member_count() == 11);
}

// Two identical runs end bit-identical
static std::vector<float> battle_snapshot() {
    Sim sim;
    const EntityHandle red  = sim.spawn_filled_group({ -10.0f, 0.0f }, 15);
    const EntityHandle blue = sim.spawn_group({ 10.0f, 0.0f }, EntityFactionsMask::Faction01);
    for (int i = 0; i < 15; i++) {
        const EntityHandle u = sim.spawn_unit({ 10.0f + (i % 5), -2.0f + (i / 5) }, FOOT,
                                              EntityFactionsMask::Faction01);
        sim.group(blue)->add_member(*sim.unit(u));
    }
    sim.group(red )->attack_move({  10.0f, 0.0f });
    sim.group(blue)->attack_move({ -10.0f, 0.0f });
    sim.run(60 * 20);

    std::vector<float> out;
    sim.world.each_entity([&](const Entity& e) {
        out.push_back(e.position.x);
        out.push_back(e.position.y);
    });
    return out;
}

static void test_deterministic() {
    std::printf("deterministic reruns\n");
    const std::vector<float> a = battle_snapshot();
    const std::vector<float> b = battle_snapshot();
    CHECK(a.size() == b.size());
    CHECK(!a.empty() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    test_march_into_formation();
    test_move_to_ignores_enemies();
    test_attack_move_engages_and_reforms();
    test_straggler_slowdown();
    test_empty_group_removes_itself();
    test_merge();
    test_deterministic();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
