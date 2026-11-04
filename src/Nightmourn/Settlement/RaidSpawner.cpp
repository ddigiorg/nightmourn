// =============================================================================
// Settlement/RaidSpawner.cpp
// =============================================================================

#include "Nightmourn/Settlement/RaidSpawner.hpp"
#include "Nightmourn/Chunk/ChunkID.hpp"
#include "Nightmourn/Settlement/Settlement.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/World/World.hpp"
#include <algorithm> // count_if, erase_if
#include <cassert>   // assert

namespace Nightmourn {

// Random spots tried per spawn before giving up until the next interval
static constexpr int SPOT_TRIES = 12;

RaidSpawner::RaidSpawner(const RaidConfig& config, EntityFactionsMask raiders, std::uint64_t seed)
    : config_(&config)
    , raiders_(raiders)
    , random_(seed)
{
    assert(config_->unit && "RaidSpawner: the raid config has no unit");
}

std::size_t RaidSpawner::alive() const { return alive_.size(); }

float& RaidSpawner::timer_(SettlementID settlement) {
    for (Timer& t : timers_) if (t.settlement == settlement) return t.seconds;
    timers_.push_back({ settlement, config_->interval * 0.5f });
    return timers_.back().seconds;
}

std::optional<Position2D> RaidSpawner::pick_spot(const World& world, const Settlement& settlement) {
    const Surface* surface = world.get_surface(settlement.surface());
    if (!surface) return std::nullopt;

    // The spawn area as a world box, then a ring `d` tiles out from it
    const TileRect area = settlement.spawn_area();
    const Area2D   unit = config_->unit->collision_bounds;
    std::vector<EntityHandle> hits;

    for (int i = 0; i < SPOT_TRIES; i++) {
        const float d  = random_.get<float>(config_->min_distance, config_->max_distance);
        const float x0 = static_cast<float>(area.min.x) - d, x1 = static_cast<float>(area.max.x + 1) + d;
        const float y0 = static_cast<float>(area.min.y) - d, y1 = static_cast<float>(area.max.y + 1) + d;
        const float w  = x1 - x0, h = y1 - y0;

        // A point on the ring's perimeter, evenly by length
        float t = random_.get<float>(0.0f, 2.0f * (w + h));
        Position2D at;
        if      (t < w)          at = { x0 + t, y0 };
        else if ((t -= w) < h)   at = { x1, y0 + t };
        else if ((t -= h) < w)   at = { x1 - t, y1 };
        else                     at = { x0, y1 - (t - w) };

        if (!surface->get_chunk(ChunkID::pos_to_id(at))) continue;      // off the map
        if (world.is_spawn_blocked(settlement.surface(), at)) continue; // another settlement's area

        hits.clear();
        surface->find_colliding(unit + at, {}, hits);
        if (!hits.empty()) continue; // a tree or building in the way

        return at;
    }
    return std::nullopt;
}

void RaidSpawner::update(World& world, SurfaceID surface, float darkness, float dt) {
    // Forget the dead
    std::erase_if(alive_, [&](const Raider& r) {
        const Entity* e = world.get_entity(r.unit);
        return !e || e->is_pending_remove();
    });

    // Daylight: the next night starts its count afresh
    if (darkness < config_->min_darkness) {
        timers_.clear();
        return;
    }

    std::vector<const Settlement*> targets;
    world.each_settlement([&](const Settlement& s) {
        if (s.surface() == surface && !has_any(s.factions(), raiders_)) targets.push_back(&s);
    });

    for (const Settlement* s : targets) {
        float& timer = timer_(s->id());
        timer -= dt;
        if (timer > 0.0f) continue;
        timer += config_->interval;

        const auto count = std::count_if(alive_.begin(), alive_.end(),
                                         [&](const Raider& r) { return r.settlement == s->id(); });
        if (count >= config_->max_alive) continue;

        const Entity* banner = world.get_entity(s->banner());
        if (!banner) continue;

        const std::optional<Position2D> at = pick_spot(world, *s);
        if (!at) continue;

        const EntityHandle h = world.create_entity<Unit>(surface, *at, *config_->unit, raiders_);
        if (Unit* u = world.get_entity_as<Unit>(h)) u->attack_move(banner->position);
        alive_.push_back({ s->id(), h });
    }
}

} // namespace Nightmourn
