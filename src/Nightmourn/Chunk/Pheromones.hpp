// =============================================================================
// Chunk/Pheromones.hpp
//
// Per-chunk pheromone field: one decaying strength per faction, like the
// trail ants leave. Units that carry EntityFlagsMask::EmitsPheromone deposit
// into the chunk they stand in (World tick phase 7); AI reads a chunk's
// strength and compares it with its neighbours' to follow the gradient
// (Chunk::strongest_neighbour, Surface::pheromone_gradient).
//
// Everything is integer so lockstep peers stay bit-identical:
//
//   - A strength is a uint16_t (0 to 65535); deposits saturate.
//   - Time is counted in decay epochs of `period` ticks (epoch = tick /
//     period). Each epoch multiplies a strength by a fixed factor f, with
//     f^half_life == 1/2.
//   - Decay is lazy. A chunk stores the epoch its values were last brought up
//     to, and a read applies f^(now - epoch) from a table of Q32 factors, so
//     idle chunks cost nothing per tick. Only a deposit writes the decayed
//     values back. Reads never write, so the order of reads cannot change
//     the result.
//
// The factors are found with an integer binary search, not std::pow, so the
// table is the same on every platform and compiler.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include "Nightmourn/Entity/EntityFactionsMask.hpp"
#include <array>   // array
#include <cstdint> // uint16_t, uint32_t, uint64_t
#include <vector>  // vector

namespace Nightmourn {

using PheromoneValue = std::uint16_t;
using PheromoneEpoch = std::uint32_t;

inline constexpr PheromoneValue PHEROMONE_MAX = 0xFFFFu;

// -----------------------------------------------------------------------------
// PheromoneDecay
//
// The decay curve, shared by every chunk of a World (WorldConfig sets it).
// -----------------------------------------------------------------------------

class PheromoneDecay {
public:

    // `period` ticks per epoch (> 0); a strength halves every
    // `half_life_epochs` epochs (> 0).
    PheromoneDecay(Nezumi::Tick period, std::uint32_t half_life_epochs);

    PheromoneEpoch epoch(Nezumi::Tick tick) const {
        return static_cast<PheromoneEpoch>(tick / period_);
    }

    // `value` after `elapsed` epochs of decay. Rounds down, and reaches 0
    // once even PHEROMONE_MAX would have decayed below 1.
    PheromoneValue apply(PheromoneValue value, PheromoneEpoch elapsed) const {
        if (elapsed >= factors_.size()) return 0;
        return static_cast<PheromoneValue>((static_cast<std::uint64_t>(value) * factors_[elapsed]) >> 32);
    }

    Nezumi::Tick  period()           const { return period_;           }
    std::uint32_t half_life_epochs() const { return half_life_epochs_; }

    // Epochs until any strength has decayed to 0
    PheromoneEpoch lifetime() const { return static_cast<PheromoneEpoch>(factors_.size()); }

private:
    Nezumi::Tick               period_           = 1;
    std::uint32_t              half_life_epochs_ = 1;
    std::vector<std::uint64_t> factors_          = {}; // f^k in Q32, factors_[0] == 1.0
};

// -----------------------------------------------------------------------------
// Pheromones
//
// One chunk's field (36 B).
// -----------------------------------------------------------------------------

struct Pheromones {

    // Strength of `faction` at epoch `now` (>= the last deposit's epoch)
    PheromoneValue get(FactionIndex faction, PheromoneEpoch now, const PheromoneDecay& decay) const {
        return decay.apply(values[faction], now - epoch);
    }

    // Decay every faction to `now`, then add `amount` to `faction`, capped
    // at PHEROMONE_MAX. Saturating adds commute, so deposits in the same
    // epoch give the same result in any order.
    void deposit(FactionIndex faction, PheromoneValue amount, PheromoneEpoch now, const PheromoneDecay& decay) {
        decay_to(now, decay);
        const std::uint32_t sum = static_cast<std::uint32_t>(values[faction]) + amount;
        values[faction] = sum > PHEROMONE_MAX ? PHEROMONE_MAX : static_cast<PheromoneValue>(sum);
    }

    // Write the decayed values back, as of epoch `now`
    void decay_to(PheromoneEpoch now, const PheromoneDecay& decay) {
        if (now == epoch) return;
        for (PheromoneValue& value : values) value = decay.apply(value, now - epoch);
        epoch = now;
    }

    std::array<PheromoneValue, MAX_FACTIONS> values = {}; // 32 B, as of `epoch`
    PheromoneEpoch                           epoch  = 0;  //  4 B
};

} // namespace Nightmourn
