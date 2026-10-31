// =============================================================================
// UnitGroup.cpp
// =============================================================================

#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Entity/EntitySearchFilter.hpp"
#include "Nightmourn/World/World.hpp"
#include "Nightmourn/World/WorldContext.hpp"
#include <algorithm> // max, min, sort
#include <cassert>   // assert
#include <cmath>     // ceil, round, sqrt

namespace Nightmourn {

namespace {

// The formation's right-hand side: `facing` turned 90 degrees clockwise
constexpr Position2D right_of(Position2D facing) { return { facing.y, -facing.x }; }

constexpr float dot(Position2D a, Position2D b) { return a.x * b.x + a.y * b.y; }

float length(Position2D v) { return std::sqrt(dot(v, v)); }

} // namespace

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

UnitGroup::UnitGroup(const UnitGroupConfig& config, EntityFactionsMask factions)
    : config_(&config)
{
    this->set_active();
    this->set_tickable();

    factions_mask = factions;
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

void UnitGroup::tick(WorldContext& ctx) {
    const uint32_t cur = static_cast<uint32_t>(ctx.tick & 1u);
    const uint32_t nxt = cur ^ 1u;
    const Plan&    plan = plans_[cur];

    const MemberStats stats = measure_members_(ctx);

    // Nobody holds a slot while Engaged, so lag means nothing then
    const bool in_formation = plan.state != UnitGroupState::Engaged;
    member_count_    = stats.count;
    straggler_count_ = in_formation ? stats.stragglers : 0;
    lag_             = in_formation ? stats.lag        : 0.0f;

    if (stats.count == 0) {
        velocity = {};
        ctx.world->remove_entity(handle);
        return;
    }

    // Not collidable: the box is for selection, overlays and future queries
    collision_bounds = stats.bounds - position;

    // 1. Move the anchor this tick, as planned last tick. While Engaged it
    //    follows the fight, so it is already in place when the group reforms.
    velocity = (plan.state == UnitGroupState::Engaged)
        ? (stats.centroid - position) / ctx.dt
        : plan.velocity;

    const Position2D anchor = position + velocity * ctx.dt; // after this tick

    // 2. Look for enemies around the members
    EntityHandle enemy = {};
    if (engages_() && (ctx.tick + handle.index()) % config_->scan_interval == 0)
        enemy = scan_(ctx, stats);

    if (enemy)                                ticks_since_enemy_ = 0;
    else if (ticks_since_enemy_ < UINT16_MAX) ticks_since_enemy_++;

    // 3. Decide the next tick's plan
    Plan next     = plan;
    next.velocity = {};
    if (enemy) next.engage_position = ctx.world->get_entity(enemy)->position;

    // Faces the route before a fresh layout, so the front rows lead
    const auto face_route = [&] {
        Position2D dest;
        if (!destination_(ctx, dest)) return;
        const Position2D delta = dest - anchor;
        const float      dist  = length(delta);
        if (dist > ARRIVE_DISTANCE) next.facing = delta / dist;
    };

    switch (plan.state) {
        case UnitGroupState::Formed: {
            if (enemy) {
                next.state = UnitGroupState::Engaged;
            } else if (march_(ctx, stats, anchor, next)) {
                next.state    = UnitGroupState::Reforming; // sharp turn: wheel in place
                next.velocity = {};
                layout_dirty_ = true;
            }
            break;
        }

        case UnitGroupState::Engaged: {
            if (!engages_() || ticks_since_enemy_ > config_->disengage_ticks) {
                next.state    = UnitGroupState::Reforming;
                layout_dirty_ = true;
                face_route();
            }
            break;
        }

        case UnitGroupState::Reforming: {
            if (enemy) {
                next.state = UnitGroupState::Engaged;
                break;
            }
            if (layout_dirty_) face_route();

            // Lag is measured against the slots members chase this tick, so
            // a layout still pending means it isn't meaningful yet
            const bool settled = !layout_dirty_ && stats.lag <= config_->reform_lag;
            if (settled || state_ticks_ >= config_->reform_timeout) {
                next.state = UnitGroupState::Formed;
                if (march_(ctx, stats, anchor, next)) {
                    next.state    = UnitGroupState::Reforming;
                    next.velocity = {};
                    layout_dirty_ = true;
                }
            }
            break;
        }
    }

    // 4. Slots for the next tick. Layouts wait while Engaged: nobody holds a
    //    slot then, and members dying would re-sort the group every hit.
    if (layout_dirty_ && next.state != UnitGroupState::Engaged) {
        layout_(ctx, next.facing, offsets_[nxt]);
        layout_dirty_ = false;
    } else {
        offsets_[nxt] = offsets_[cur]; // same size: no allocation
    }

    plans_[nxt] = next;

    if (next.state != state_) {
        state_       = next.state;
        state_ticks_ = 0;
    } else if (state_ticks_ < UINT16_MAX) {
        state_ticks_++;
    }
}

// -----------------------------------------------------------------------------
// Membership
// -----------------------------------------------------------------------------

bool UnitGroup::add_member(Unit& unit) {
    assert(unit.factions_mask == factions_mask && "Members must share the group's factions");
    assert(unit.surface_id == surface_id && "Members must share the group's surface");
    assert(!unit.is_pending_remove() && "Unit is being removed");

    if (unit.group_ == handle && members_[unit.group_slot_] == unit.handle) return true;
    if (member_count_ >= config_->max_members) return false;

    uint16_t slot;
    if (!free_slots_.empty()) {
        slot = free_slots_.back();
        free_slots_.pop_back();
        members_[slot] = unit.handle;
    } else {
        assert(members_.size() < UINT16_MAX && "UnitGroup slot space exhausted");
        slot = static_cast<uint16_t>(members_.size());
        members_.push_back(unit.handle);
        offsets_[0].push_back({});
        offsets_[1].push_back({});
    }

    // Keep the unit where it stands until the next layout (one tick later)
    const Position2D d = unit.position - position;
    for (size_t b = 0; b < 2; b++) {
        const Position2D f = plans_[b].facing;
        offsets_[b][slot] = { dot(d, right_of(f)), dot(d, f) };
    }

    unit.join_group_(handle, slot);
    member_count_++;
    layout_dirty_ = true;
    return true;
}

void UnitGroup::remove_member(Unit& unit) {
    if (unit.group_ != handle) return;
    unit.leave_group_(); // its slot is freed on the group's next tick
}

void UnitGroup::merge_from(World& world, UnitGroup& other) {
    assert(&other != this && "Cannot merge a group into itself");

    for (size_t slot = 0; slot < other.members_.size(); slot++) {
        Unit* unit = world.get_entity_as<Unit>(other.members_[slot]);
        if (!unit || unit->is_pending_remove())                     continue;
        if (unit->group_ != other.handle || unit->group_slot_ != slot) continue;
        if (!add_member(*unit)) break; // full: the rest stay with `other`
    }
}

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

void UnitGroup::issue(const UnitCommand& cmd) {
    assert(cmd.type != UnitCommandType::Group && "Groups don't follow groups");

    command_ = cmd;
    waypoints_.clear();
    next_waypoint_ = 0;

    if (cmd.type == UnitCommandType::MoveTo || cmd.type == UnitCommandType::AttackMove)
        waypoints_.push_back(cmd.position);
}

void UnitGroup::add_waypoint(Position2D position) {
    const bool has_route = command_.type == UnitCommandType::MoveTo ||
                           command_.type == UnitCommandType::AttackMove;

    if (has_route) waypoints_.push_back(position);
    else           attack_move(position);
}

// -----------------------------------------------------------------------------
// Plan
// -----------------------------------------------------------------------------

Position2D UnitGroup::slot_position(Nezumi::Tick tick, uint16_t slot) const {
    const uint32_t   b      = static_cast<uint32_t>(tick & 1u);
    const Position2D facing = plans_[b].facing;
    const Position2D offset = offsets_[b][slot];
    return position + right_of(facing) * offset.x + facing * offset.y;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

UnitGroup::MemberStats UnitGroup::measure_members_(WorldContext& ctx) {
    MemberStats s;
    Position2D  sum      = {};
    float       lag2_sum = 0.0f;

    const float straggler2 = config_->straggler_distance * config_->straggler_distance;

    for (size_t i = 0; i < members_.size(); i++) {
        if (!members_[i]) continue;

        const uint16_t slot = static_cast<uint16_t>(i);
        const Unit*    unit = ctx.world->get_entity_as<Unit>(members_[i]);

        // Died, or left (own command, another group, or re-added to a new slot)
        const bool gone = !unit || unit->is_pending_remove() ||
                          unit->group() != handle || unit->group_slot() != slot;
        if (gone) {
            members_[i] = {};
            free_slots_.push_back(slot);
            layout_dirty_ = true;
            continue;
        }

        const Position2D  p      = unit->position;
        const UnitConfig& config = unit->config();

        if (s.count == 0) {
            s.bounds    = { p, p };
            s.min_speed = config.move_speed;
        } else {
            s.bounds.min = { std::min(s.bounds.min.x, p.x), std::min(s.bounds.min.y, p.y) };
            s.bounds.max = { std::max(s.bounds.max.x, p.x), std::max(s.bounds.max.y, p.y) };
            s.min_speed  = std::min(s.min_speed, config.move_speed);
        }
        s.max_aggro = std::max(s.max_aggro, config.aggro_radius);

        const float lag2 = p.distance2(slot_position(ctx.tick, slot));
        if (lag2 > straggler2) s.stragglers++;

        sum      += p;
        lag2_sum += lag2;
        s.count++;
    }

    if (s.count > 0) {
        s.centroid = sum / static_cast<float>(s.count);
        s.lag      = std::sqrt(lag2_sum / static_cast<float>(s.count));
    }
    return s;
}

bool UnitGroup::destination_(WorldContext& ctx, Position2D& out) {
    switch (command_.type) {
        case UnitCommandType::Stop:
        case UnitCommandType::Group:
            return false;

        case UnitCommandType::MoveTo:
        case UnitCommandType::AttackMove:
            if (next_waypoint_ >= waypoints_.size()) return false;
            out = waypoints_[next_waypoint_];
            return true;

        case UnitCommandType::Attack: {
            const Entity* target = ctx.world->get_entity(command_.target);
            if (!target || !target->is_alive()) {
                command_ = UnitCommand::stop();
                return false;
            }
            out = target->position;
            return true;
        }
    }
    return false;
}

EntityHandle UnitGroup::scan_(WorldContext& ctx, const MemberStats& stats) const {
    const Surface* surface = ctx.world->get_surface(surface_id);
    assert(surface && "UnitGroup is not on a valid surface");

    const Position2D half   = stats.bounds.size() * 0.5f;
    const float      radius = length(half) + stats.max_aggro;

    return surface->find_nearest_entity(stats.bounds.center(), radius,
                                        EntitySearchFilter::enemies_of(*this));
}

bool UnitGroup::march_(WorldContext& ctx, const MemberStats& stats, Position2D anchor, Plan& next) {
    Position2D dest;
    if (!destination_(ctx, dest)) return false; // nowhere to go: stand

    Position2D delta = dest - anchor;
    float      dist  = length(delta);

    // At a waypoint: on to the next one, or the move is done
    while (dist <= ARRIVE_DISTANCE) {
        if (command_.type == UnitCommandType::Attack) return false; // on the target: stand

        if (++next_waypoint_ >= waypoints_.size()) {
            command_ = UnitCommand::stop();
            waypoints_.clear();
            next_waypoint_ = 0;
            return false;
        }
        delta = waypoints_[next_waypoint_] - anchor;
        dist  = length(delta);
    }

    const Position2D dir = delta / dist;

    // Turn toward the route. Small turns rotate the slots and members catch
    // up; sharp ones reform in place. Not when nearly there, where the
    // direction swings wildly.
    if (dist > 2.0f * config_->spacing) {
        const float d = dot(dir, next.facing);
        if (d < REFACE_DOT) {
            next.facing = dir;
            if (d < REFORM_DOT) return true;
        }
    }

    // Straggler slowdown, by how far members are from their slots
    float factor = 1.0f;
    if (stats.lag > config_->slowdown_lag) {
        const float range = std::max(config_->stop_lag - config_->slowdown_lag, 1e-3f);
        const float t     = std::min((stats.lag - config_->slowdown_lag) / range, 1.0f);
        factor = 1.0f - t * (1.0f - config_->min_speed_factor);
    }

    float speed = stats.min_speed * config_->march_speed_factor * factor;
    speed = std::min(speed, dist / ctx.dt); // land exactly on the waypoint

    next.velocity = dir * speed;
    return false;
}

void UnitGroup::layout_(WorldContext& ctx, Position2D facing, std::vector<Position2D>& offsets) {
    const Position2D right = right_of(facing);

    layout_scratch_.clear();
    for (size_t i = 0; i < members_.size(); i++) {
        if (!members_[i]) continue;

        // measure_members_ already dropped dead and departed members
        const Unit*      unit = ctx.world->get_entity_as<Unit>(members_[i]);
        const Position2D d    = unit->position - position;

        layout_scratch_.push_back({
            .slot    = static_cast<uint16_t>(i),
            .range   = config_->ranged_in_back ? unit->config().attack_range : 0.0f,
            .forward = dot(d, facing),
            .lateral = dot(d, right),
            .id      = members_[i].index(),
        });
    }

    const size_t n = layout_scratch_.size();
    if (n == 0) return;

    // Front to back: short range first, then whoever stands furthest forward.
    // Then each row left to right by where its members stand. Members take
    // the slot nearest them, so a reform doesn't send units across the block.
    std::sort(layout_scratch_.begin(), layout_scratch_.end(),
        [](const LayoutEntry& a, const LayoutEntry& b) {
            if (a.range   != b.range)   return a.range   < b.range;
            if (a.forward != b.forward) return a.forward > b.forward;
            return a.id < b.id;
        });

    size_t cols = config_->columns;
    if (cols == 0) cols = static_cast<size_t>(std::round(std::sqrt(static_cast<float>(n) * config_->aspect)));
    cols = std::clamp<size_t>(cols, 1, n);

    const size_t rows    = (n + cols - 1) / cols;
    const float  spacing = config_->spacing;

    for (size_t r = 0; r < rows; r++) {
        const size_t begin = r * cols;
        const size_t end   = std::min(n, begin + cols);
        const size_t count = end - begin;

        std::sort(layout_scratch_.begin() + begin, layout_scratch_.begin() + end,
            [](const LayoutEntry& a, const LayoutEntry& b) {
                if (a.lateral != b.lateral) return a.lateral < b.lateral;
                return a.id < b.id;
            });

        // Row 0 is the front; the block is centered on the anchor
        const float y = (static_cast<float>(rows - 1) * 0.5f - static_cast<float>(r)) * spacing;

        for (size_t c = 0; c < count; c++) {
            const float x = (static_cast<float>(c) - static_cast<float>(count - 1) * 0.5f) * spacing;
            offsets[layout_scratch_[begin + c].slot] = { x, y };
        }
    }
}

} // namespace Nightmourn
