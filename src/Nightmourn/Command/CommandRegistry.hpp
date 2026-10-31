// =============================================================================
// CommandRegistry.hpp
// =============================================================================

#pragma once
#include "Nightmourn/Command/DeferEntityApplyDamage.hpp"
#include "Nightmourn/Command/DeferEntityCreate.hpp"
#include "Nightmourn/Command/DeferEntityMine.hpp"
#include "Nightmourn/Command/DeferEntityRemove.hpp"
#include "Nightmourn/Command/DeferEntitySetActive.hpp"
#include <utility> // exchange
#include <vector>  // vector

namespace Nightmourn {

class CommandRegistry {
public:

    // -------------------------------------------------------------------------
    // Defer
    // -------------------------------------------------------------------------

    void defer_entity_create(const DeferEntityCreate& cmd) {
        entity_creates_.push_back(cmd);
    }

    void defer_entity_remove(const DeferEntityRemove& cmd) {
        entity_removes_.push_back(cmd);
    }

    void defer_entity_set_active(const DeferEntitySetActive& cmd) {
        entity_set_actives_.push_back(cmd);
    }

    void defer_entity_apply_damage(const DeferEntityApplyDamage& cmd) {
        entity_apply_damages_.push_back(cmd);
    }

    void defer_entity_mine(const DeferEntityMine& cmd) {
        entity_mines_.push_back(cmd);
    }

    // -------------------------------------------------------------------------
    // Apply
    //
    // Buffers are swapped out before processing, so commands issued while
    // applying (e.g. from an entity constructor or destructor) land in the next
    // command phase instead of mutating the buffer being walked.
    // -------------------------------------------------------------------------

    template <typename Fn>
    void apply_entity_creates(Fn&& fn) { apply_(entity_creates_, fn); }

    template <typename Fn>
    void apply_entity_removes(Fn&& fn) { apply_(entity_removes_, fn); }

    template <typename Fn>
    void apply_entity_set_actives(Fn&& fn) { apply_(entity_set_actives_, fn); }

    template <typename Fn>
    void apply_entity_apply_damages(Fn&& fn) { apply_(entity_apply_damages_, fn); }

    template <typename Fn>
    void apply_entity_mines(Fn&& fn) { apply_(entity_mines_, fn); }

private:

    // Swap the buffer out, walk it, then hand its capacity back so steady
    // state does no allocation. Commands issued by `fn` land in `buffer`.
    template <typename T, typename Fn>
    static void apply_(std::vector<T>& buffer, Fn& fn) {
        std::vector<T> cmds = std::exchange(buffer, {});
        for (const T& cmd : cmds) fn(cmd);
        if (buffer.empty()) {
            cmds.clear();
            buffer = std::move(cmds);
        }
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    std::vector<DeferEntityCreate>      entity_creates_       = {};
    std::vector<DeferEntityRemove>      entity_removes_       = {};
    std::vector<DeferEntitySetActive>   entity_set_actives_   = {};
    std::vector<DeferEntityApplyDamage> entity_apply_damages_ = {};
    std::vector<DeferEntityMine>        entity_mines_         = {};
};

} // namespace Nightmourn
