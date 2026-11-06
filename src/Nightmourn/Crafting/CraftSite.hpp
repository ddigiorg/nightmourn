// =============================================================================
// Crafting/CraftSite.hpp
//
// Where crafting runs on its own: a settlement (later the Spirit Realm). A
// site has machines (its crafting buildings, a workshop) and rules the
// player sets, and makes things from its storage (the settlement's
// inventory) into its storage, with nobody standing there.
//
// Limits per item, not per-machine recipes (crafting-design/DESIGN.md §3,
// and the Settlement menu's Crafting tab):
//
// - max: "keep 100 wooden arrows". A free machine makes the item while the
//   storage holds fewer than its max, counting what machines are already
//   making, so several machines don't all pile on (it can end up one batch
//   over). 0: not made.
// - min: "never use the last 20 wood". A job only starts if paying it
//   leaves every ingredient at or above its min.
// - priority (0..255): a free machine takes the highest-priority item it
//   can make now; among equals it goes round robin from where it last left
//   off, so one cheap item can't starve the rest.
// - Each machine runs one job at a time (more workshops, more in
//   parallel), and only takes items whose recipe category it offers.
//
// Jobs are paid when they start and delivered when they finish, all or
// nothing; with no room for the results the machine waits (OutputFull) and
// delivers as soon as there's room. A machine that's destroyed loses its
// job; one deconstructed gets refunded first (refund_job).
//
// Cheap when nothing happens:
//
// - Running jobs sit in a heap by finish tick: a tick only touches the
//   jobs due now, never every machine.
// - Idle machines sleep. They look for work again only when the storage
//   changed (Inventory::version), the rules changed, or a machine just
//   finished. A site with nothing to do costs a couple of compares a tick.
//
// The World ticks every settlement's site in its crafting phase, in
// SettlementID order, so it's deterministic; buildings join and leave it as
// they're placed and removed (World::place_building, removal).
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nightmourn/Building/BuildingConfig.hpp"
#include "Nightmourn/Crafting/Recipe.hpp"
#include "Nightmourn/Entity/EntityID.hpp"
#include "Nightmourn/Item/ItemID.hpp"
#include <cstddef>    // size_t
#include <cstdint>    // uint8_t, uint16_t, uint32_t
#include <functional> // greater
#include <queue>      // priority_queue
#include <span>       // span
#include <vector>     // vector

namespace Nightmourn {

class Inventory;

enum class MachineState : std::uint8_t {
    Idle,       // nothing to do (or asleep until something changes)
    Working,    // a job under way
    OutputFull, // done, waiting for room in the storage
};

struct Machine {
    EntityHandle      building   = {}; // null: a free slot
    CraftCategoryMask categories = 0;
    float             speed      = 1.0f;
    MachineState      state      = MachineState::Idle;

    // The job (Working, OutputFull)
    const Recipe* recipe = nullptr;
    Nezumi::Tick  start  = 0;
    Nezumi::Tick  finish = 0;
    std::uint32_t serial = 0; // which job, so a stale heap entry is known

    std::uint16_t next_rule = 0; // round-robin cursor

    // How far along the job is at tick `now`, 0..1 (1 when OutputFull)
    float progress(Nezumi::Tick now) const {
        if (state == MachineState::OutputFull) return 1.0f;
        if (state != MachineState::Working || finish <= start) return 0.0f;
        const float p = static_cast<float>(now - start) / static_cast<float>(finish - start);
        return p < 0.0f ? 0.0f : p > 1.0f ? 1.0f : p;
    }
};

// What a site does about one item
struct CraftLimits {
    std::uint32_t min      = 0; // never used as an ingredient below this
    std::uint32_t max      = 0; // made while there are fewer (0: not made)
    std::uint8_t  priority = 0; // higher first
};

// Keep `max` of `item` in store, made by `recipe`
struct CraftRule {
    const Recipe* recipe = nullptr;
    ItemID        item   = {};
    std::uint32_t max    = 0;
};

class CraftSite {
public:

    // -------------------------------------------------------------------------
    // Machines (World)
    // -------------------------------------------------------------------------

    // A crafting building joins: it makes `config`'s crafting categories
    void add_machine(EntityHandle building, const BuildingConfig& config);

    // It leaves; a job under way is lost
    void remove_machine(EntityHandle building);

    // Puts the ingredients of `building`'s job back into `storage` and
    // makes it idle (deconstructing it). False, with the job kept, if they
    // don't all fit. True if there was no job.
    bool refund_job(EntityHandle building, Inventory& storage);

    // The machine for `building`, or null
    const Machine* machine(EntityHandle building) const;

    // Every slot; free ones have a null building
    std::span<const Machine> machines() const { return machines_; }
    std::size_t              machine_count() const { return machine_count_; }

    // What its machines make between them
    CraftCategoryMask categories() const;

    // -------------------------------------------------------------------------
    // Rules
    // -------------------------------------------------------------------------

    // Keep `max` of `item`, made by `recipe` (0: stop making it)
    void set_max(const Recipe& recipe, ItemID item, std::uint32_t max);

    // Never use `item` below `min` (0: use it all)
    void set_min(ItemID item, std::uint32_t min);

    // Higher goes first when a machine picks what to make
    void set_priority(ItemID item, std::uint8_t priority);

    CraftLimits limits(ItemID item) const;

    // The items with a max, in the order they were first given one
    std::span<const CraftRule> rules() const { return rules_; }

    // How many of `item` its running jobs will make
    std::uint32_t in_progress(ItemID item) const { return at_(in_progress_, item); }

    // -------------------------------------------------------------------------
    // Tick
    // -------------------------------------------------------------------------

    struct Report {
        std::uint32_t started = 0;
        std::uint32_t made    = 0;
    };

    // Delivers the jobs due at `now`, then gives idle machines work from
    // the rules if anything changed. `storage` pays and receives.
    Report tick(Nezumi::Tick now, Inventory& storage);

private:
    struct Due {
        Nezumi::Tick  finish;
        std::uint32_t machine;
        std::uint32_t serial;
        bool operator>(const Due& o) const {
            return finish != o.finish ? finish > o.finish : machine > o.machine;
        }
    };

    bool try_start_(std::size_t index, Nezumi::Tick now, Inventory& storage);
    bool deliver_  (Machine& m, Inventory& storage);
    void end_job_  (Machine& m);
    bool affordable_(const Recipe& recipe, const Inventory& storage) const;

    static std::uint32_t at_(const std::vector<std::uint32_t>& v, ItemID item) {
        return item && item.value() < v.size() ? v[item.value()] : 0;
    }
    static std::uint32_t& grow_(std::vector<std::uint32_t>& v, ItemID item);

    std::vector<Machine>   machines_      = {};
    std::size_t            machine_count_ = 0;
    std::vector<CraftRule> rules_         = {};

    std::vector<std::uint32_t> mins_        = {}; // by ItemID
    std::vector<std::uint32_t> priorities_  = {}; // by ItemID
    std::vector<std::uint32_t> in_progress_ = {}; // by ItemID

    std::priority_queue<Due, std::vector<Due>, std::greater<Due>> due_ = {};
    std::uint32_t next_serial_  = 1;
    std::uint32_t seen_version_ = 0;
    bool          dirty_        = true; // rules or machines changed: idle ones look again
};

} // namespace Nightmourn
