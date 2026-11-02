// =============================================================================
// tests/test_projectiles/main.cpp
//
// Headless checks for Projectiles and attack windups: a Ground arrow hits a
// target that stands still and misses one that walks on, lead aim and
// Tracking guidance hit it anyway, a miss can hit another enemy but never a
// friend, the windup delays the hit, melee whiffs out of reach, and reruns
// are bit-identical.
// =============================================================================

#include "Nezumi/Core/Clock.hpp"
#include "Nezumi/Event/EventBus.hpp"
#include "Nightmourn/Projectile/ProjectileConfig.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/World/World.hpp"
#include <algorithm> // max
#include <cstdio>    // printf
#include <cstdlib>   // EXIT_FAILURE, EXIT_SUCCESS
#include <vector>    // vector

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

static constexpr EntityFactionsMask BLUE = EntityFactionsMask::Faction00;
static constexpr EntityFactionsMask RED  = EntityFactionsMask::Faction01;

static constexpr ProjectileConfig DUMB = {
    .name  = "dumb",
    .speed = 8.0f,
};

static constexpr ProjectileConfig LEADING = {
    .name        = "leading",
    .speed       = 8.0f,
    .lead_target = true,
};

static constexpr ProjectileConfig TRACKING = {
    .name     = "tracking",
    .guidance = ProjectileGuidance::Tracking,
    .speed    = 8.0f,
};

static constexpr UnitConfig make_archer(const ProjectileConfig* projectile, uint16_t windup = 0) {
    return {
        .name            = "archer",
        .move_speed      = 0.0f, // holds its ground
        .attack_damage   = 5,
        .attack_interval = 60,
        .attack_range    = 6.0f,
        .aggro_radius    = 0.0f, // only on command
        .attack_windup   = windup,
        .projectile      = projectile,
    };
}

static constexpr UnitConfig DUMB_ARCHER     = make_archer(&DUMB);
static constexpr UnitConfig LEADING_ARCHER  = make_archer(&LEADING);
static constexpr UnitConfig TRACKING_ARCHER = make_archer(&TRACKING);
static constexpr UnitConfig SLOW_ARCHER     = make_archer(&DUMB, 20);

static constexpr UnitConfig SWORD = {
    .name            = "sword",
    .move_speed      = 0.0f,
    .attack_damage   = 5,
    .attack_interval = 60,
    .attack_range    = 1.0f,
    .aggro_radius    = 0.0f,
    .attack_windup   = 20,
};

// Never fights back
static constexpr UnitConfig DUMMY = {
    .name         = "dummy",
    .max_health   = 100,
    .move_speed   = 2.0f,
    .aggro_radius = 0.0f,
};

// Walks off fast enough to leave a sword's reach during its windup
static constexpr UnitConfig RUNNER = {
    .name         = "runner",
    .max_health   = 100,
    .move_speed   = 6.0f,
    .aggro_radius = 0.0f,
};

struct Sim {
    Clock     clock;
    EventBus  bus;
    World     world{ clock, bus };
    SurfaceID surface = world.create_surface();

    size_t max_in_flight = 0;

    void run(int ticks) {
        for (int i = 0; i < ticks; i++) {
            world.tick();
            bus.flush(EventPhase::TickPost, world.get_tick());
            max_in_flight = std::max(max_in_flight, in_flight());
        }
    }

    EntityHandle spawn(Position2D at, const UnitConfig& config, EntityFactionsMask faction) {
        return world.create_entity<Unit>(surface, at, config, faction);
    }

    Unit*  unit     (EntityHandle h) { return world.get_entity_as<Unit>(h); }
    int    health   (EntityHandle h) { const Unit* u = unit(h); return u ? u->health : 0; }
    size_t in_flight()               { return world.get_surface(surface)->projectiles().size(); }
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_ground_hits_still_target() {
    std::printf("ground arrow hits a target that stands still\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, DUMB_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,       RED);
    sim.unit(archer)->attack(target);

    // Released on tick 0; 4 tiles at 8 tiles/s is 30 ticks of flight
    sim.run(1);
    CHECK(sim.in_flight() == 1);
    CHECK(sim.health(target) == 100);

    const Projectile& p = sim.world.get_surface(sim.surface)->projectiles().front();
    CHECK(p.flight_ticks == 30);
    CHECK(p.height_at(0.0f) == DUMB.launch_height);
    CHECK(p.height_at(0.5f) > p.height_at(0.25f));
    CHECK(p.height_at(1.0f) == 0.0f);

    sim.run(29); // lands on tick 30
    CHECK(sim.health(target) == 100);
    sim.run(1);
    CHECK(sim.in_flight() == 0);
    CHECK(sim.health(target) == 95);
}

static void test_ground_misses_walking_target() {
    std::printf("ground arrow misses a target that walks on\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, DUMB_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,       RED);
    sim.unit(target)->move_to({ 4.0f, 100.0f }); // walks across the line of fire
    sim.unit(archer)->attack(target);

    sim.run(150); // three shots
    CHECK(sim.max_in_flight >= 1);
    CHECK(sim.health(target) == 100);
}

static void test_leading_hits_walking_target() {
    std::printf("leading arrow hits a target that walks on\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, LEADING_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,          RED);
    sim.unit(target)->move_to({ 4.0f, 100.0f });
    sim.unit(archer)->attack(target);

    sim.run(1);  // the target starts walking on tick 0, so the first shot
    sim.run(150); // may lead nothing; the next ones see its velocity
    CHECK(sim.health(target) < 100);
}

static void test_tracking_hits_walking_target() {
    std::printf("tracking arrow hits a target that walks on\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, TRACKING_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,           RED);
    sim.unit(target)->move_to({ 4.0f, 100.0f });
    sim.unit(archer)->attack(target);

    sim.run(150); // shots on ticks 0, 60, 120, the last still in the air
    CHECK(sim.health(target) == 90);
}

static void test_miss_hits_enemy_not_friend() {
    std::printf("a miss hits an enemy standing there, never a friend\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, DUMB_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,       RED);
    sim.unit(archer)->attack(target);
    sim.run(1); // released at (4, 0)

    // The target steps aside; a friend and another enemy step in
    sim.unit(target)->move_to({ 4.0f, 50.0f });
    const EntityHandle friend_ = sim.spawn({ 4.0f, 0.0f }, DUMMY, BLUE);
    const EntityHandle bystander = sim.spawn({ 4.05f, 0.0f }, DUMMY, RED);

    sim.run(30);
    CHECK(sim.health(target)    == 100);
    CHECK(sim.health(friend_)   == 100);
    CHECK(sim.health(bystander) == 95);
}

static void test_windup_delays_release() {
    std::printf("windup delays the release\n");
    Sim sim;
    const EntityHandle archer = sim.spawn({ 0.0f, 0.0f }, SLOW_ARCHER, BLUE);
    const EntityHandle target = sim.spawn({ 4.0f, 0.0f }, DUMMY,       RED);
    sim.unit(archer)->attack(target);

    sim.run(1); // attack starts on tick 0
    CHECK(sim.unit(archer)->state() == UnitState::Attacking);
    CHECK(sim.unit(archer)->windup_ticks() == 20);
    CHECK(sim.in_flight() == 0);

    sim.run(19); // released on tick 20
    CHECK(sim.in_flight() == 0);
    sim.run(1);
    CHECK(sim.in_flight() == 1);
    CHECK(sim.unit(archer)->windup_ticks() == 0);

    sim.run(30); // lands on tick 50
    CHECK(sim.health(target) == 95);
}

static void test_melee_windup_and_whiff() {
    std::printf("melee hits after its windup, and whiffs out of reach\n");
    {
        Sim sim;
        const EntityHandle sword  = sim.spawn({ 0.0f, 0.0f }, SWORD, BLUE);
        const EntityHandle target = sim.spawn({ 0.8f, 0.0f }, DUMMY, RED);
        sim.unit(sword)->attack(target);

        sim.run(20);
        CHECK(sim.health(target) == 100);
        sim.run(1); // lands on tick 20
        CHECK(sim.health(target) == 95);
    }
    {
        Sim sim;
        const EntityHandle sword  = sim.spawn({ 0.0f, 0.0f }, SWORD, BLUE);
        const EntityHandle target = sim.spawn({ 0.8f, 0.0f }, RUNNER, RED);
        sim.unit(sword)->attack(target);

        sim.run(1); // swing starts, then the target walks off
        sim.unit(target)->move_to({ 100.0f, 0.0f });
        sim.run(20);
        CHECK(sim.health(target) == 100);
    }
}

static void test_reruns_identical() {
    std::printf("reruns are bit-identical\n");

    const auto battle = []() {
        Sim sim;
        std::vector<EntityHandle> blues, reds;
        for (int i = 0; i < 10; i++) {
            blues.push_back(sim.spawn({ 0.0f, static_cast<float>(i) }, DUMB_ARCHER, BLUE));
            reds .push_back(sim.spawn({ 5.0f, static_cast<float>(i) }, DUMMY,       RED));
        }
        for (int i = 0; i < 10; i++) {
            sim.unit(blues[i])->attack(reds[(i * 3) % 10]);
            sim.unit(reds[i])->move_to({ 5.0f + static_cast<float>(i % 3), 20.0f });
        }
        sim.run(300);

        std::vector<int> health;
        for (EntityHandle h : reds) health.push_back(sim.health(h));
        return health;
    };

    CHECK(battle() == battle());
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    test_ground_hits_still_target();
    test_ground_misses_walking_target();
    test_leading_hits_walking_target();
    test_tracking_hits_walking_target();
    test_miss_hits_enemy_not_friend();
    test_windup_delays_release();
    test_melee_windup_and_whiff();
    test_reruns_identical();

    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("all passed\n");
    return EXIT_SUCCESS;
}
