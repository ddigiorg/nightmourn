// =============================================================================
// Crafting/CraftSite.cpp
// =============================================================================

#include "Nightmourn/Crafting/CraftSite.hpp"
#include "Nightmourn/Crafting/CraftQueue.hpp" // ticks_for
#include "Nightmourn/Item/Inventory.hpp"      // give, pay
#include <algorithm> // find_if

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Machines
// -----------------------------------------------------------------------------

void CraftSite::add_machine(EntityHandle building, const BuildingConfig& config) {
    if (!building || config.crafting_categories == 0 || machine(building)) return;

    Machine m;
    m.building   = building;
    m.categories = config.crafting_categories;
    m.speed      = config.crafting_speed;

    // Reuse a free slot, so indices (and the heap's entries) stay small
    auto free = std::find_if(machines_.begin(), machines_.end(), [](const Machine& s) { return !s.building; });
    if (free != machines_.end()) *free = m;
    else                         machines_.push_back(m);
    machine_count_++;
    dirty_ = true;
}

void CraftSite::remove_machine(EntityHandle building) {
    for (Machine& m : machines_) {
        if (m.building != building || !building) continue;
        end_job_(m); // its ingredients are lost with it
        m = Machine{};
        machine_count_--;
        return;
    }
}

bool CraftSite::refund_job(EntityHandle building, Inventory& storage) {
    for (Machine& m : machines_) {
        if (m.building != building || !building) continue;
        if (!m.recipe) return true;
        Inventory* const to[] = { &storage };
        if (!m.recipe->ingredients.empty() && !give(m.recipe->ingredients, to)) return false;
        end_job_(m);
        dirty_ = true;
        return true;
    }
    return true;
}

const Machine* CraftSite::machine(EntityHandle building) const {
    if (!building) return nullptr;
    for (const Machine& m : machines_) if (m.building == building) return &m;
    return nullptr;
}

CraftCategoryMask CraftSite::categories() const {
    CraftCategoryMask mask = 0;
    for (const Machine& m : machines_) mask |= m.categories;
    return mask;
}

// -----------------------------------------------------------------------------
// Rules
// -----------------------------------------------------------------------------

void CraftSite::set_max(const Recipe& recipe, ItemID item, std::uint32_t max) {
    auto rule = std::find_if(rules_.begin(), rules_.end(), [&](const CraftRule& r) { return r.item == item; });
    if (max == 0) {
        if (rule != rules_.end()) rules_.erase(rule); // its running jobs still finish
    } else if (rule != rules_.end()) {
        *rule = { &recipe, item, max };
    } else {
        rules_.push_back({ &recipe, item, max });
    }
    dirty_ = true;
}

void CraftSite::set_min(ItemID item, std::uint32_t min) {
    if (!item || (min == 0 && item.value() >= mins_.size())) return;
    grow_(mins_, item) = min;
    dirty_ = true;
}

void CraftSite::set_priority(ItemID item, std::uint8_t priority) {
    if (!item || (priority == 0 && item.value() >= priorities_.size())) return;
    grow_(priorities_, item) = priority;
    dirty_ = true;
}

CraftLimits CraftSite::limits(ItemID item) const {
    CraftLimits l;
    l.min      = at_(mins_, item);
    l.priority = static_cast<std::uint8_t>(at_(priorities_, item));
    for (const CraftRule& r : rules_) if (r.item == item) l.max = r.max;
    return l;
}

// -----------------------------------------------------------------------------
// Tick
// -----------------------------------------------------------------------------

CraftSite::Report CraftSite::tick(Nezumi::Tick now, Inventory& storage) {
    Report report;
    bool   freed = false;

    // The jobs due now; a stale entry (job refunded, machine removed or
    // reused) has a serial that no longer matches
    while (!due_.empty() && due_.top().finish <= now) {
        const Due d = due_.top();
        due_.pop();
        Machine& m = machines_[d.machine];
        if (m.state != MachineState::Working || m.serial != d.serial) continue;
        if (deliver_(m, storage)) { report.made++; freed = true; }
        else                      m.state = MachineState::OutputFull;
    }

    // Idle machines sleep until something they'd care about changed
    if (!dirty_ && !freed && storage.version() == seen_version_) return report;

    for (std::size_t i = 0; i < machines_.size(); i++) {
        Machine& m = machines_[i];
        if (!m.building) continue;
        if (m.state == MachineState::OutputFull && deliver_(m, storage)) report.made++;
        if (m.state == MachineState::Idle && try_start_(i, now, storage)) report.started++;
    }

    // What it paid and delivered itself doesn't count as news
    seen_version_ = storage.version();
    dirty_        = false;
    return report;
}

bool CraftSite::try_start_(std::size_t index, Nezumi::Tick now, Inventory& storage) {
    Machine& m = machines_[index];
    const std::size_t n = rules_.size();

    // The highest priority it can make now; among equals, the first from
    // its round-robin cursor
    std::size_t best = n;
    std::uint32_t best_priority = 0;
    for (std::size_t k = 0; k < n; k++) {
        const std::size_t i    = (m.next_rule + k) % n;
        const CraftRule&  rule = rules_[i];
        const std::uint32_t priority = at_(priorities_, rule.item);
        if (best < n && priority <= best_priority) continue;
        if (!rule.recipe || !rule.recipe->can_craft_in(m.categories)) continue;
        if (storage.count(rule.item) + in_progress(rule.item) >= rule.max) continue;
        if (!affordable_(*rule.recipe, storage)) continue;
        best          = i;
        best_priority = priority;
    }

    if (best < n) {
        const std::size_t i    = best;
        const CraftRule&  rule = rules_[i];
        Inventory* const from[] = { &storage };
        if (!pay(rule.recipe->ingredients, from)) return false;

        m.state  = MachineState::Working;
        m.recipe = rule.recipe;
        m.start  = now;
        m.finish = now + CraftQueue::ticks_for(*rule.recipe, m.speed);
        m.serial = next_serial_++;
        m.next_rule = static_cast<std::uint16_t>((i + 1) % n);
        for (const ItemAmount& r : rule.recipe->results) grow_(in_progress_, r.item) += r.count;

        due_.push({ m.finish, static_cast<std::uint32_t>(index), m.serial });
        return true;
    }
    return false;
}

bool CraftSite::deliver_(Machine& m, Inventory& storage) {
    // One result (most recipes): room_for says if it fits, without give()'s
    // trial copy of the storage
    const std::span<const ItemAmount> results = m.recipe->results;
    if (results.size() == 1) {
        if (storage.room_for(results[0].item) < results[0].count) return false;
        storage.add(results[0].item, results[0].count);
    } else {
        Inventory* const to[] = { &storage };
        if (!give(results, to)) return false;
    }
    end_job_(m);
    return true;
}

void CraftSite::end_job_(Machine& m) {
    if (m.recipe)
        for (const ItemAmount& r : m.recipe->results) grow_(in_progress_, r.item) -= r.count;
    m.state  = MachineState::Idle;
    m.recipe = nullptr;
    m.start  = 0;
    m.finish = 0;
    m.serial = 0;
}

bool CraftSite::affordable_(const Recipe& recipe, const Inventory& storage) const {
    for (const ItemAmount& in : recipe.ingredients)
        if (storage.count(in.item) < in.count + at_(mins_, in.item)) return false;
    return true;
}

std::uint32_t& CraftSite::grow_(std::vector<std::uint32_t>& v, ItemID item) {
    if (item.value() >= v.size()) v.resize(item.value() + 1, 0);
    return v[item.value()];
}

} // namespace Nightmourn
