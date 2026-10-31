// =============================================================================
// Chunk/Pheromones.cpp
// =============================================================================

#include "Nightmourn/Chunk/Pheromones.hpp"
#include <cassert> // assert

namespace Nightmourn {

static constexpr std::uint64_t Q32_ONE = 1ull << 32;

// f^n in Q32, rounding down at each step. f < 2^32 and the running product
// stays <= 2^32, so every multiply fits in 64 bits.
static std::uint64_t pow_q32(std::uint64_t f, std::uint32_t n) {
    std::uint64_t r = Q32_ONE;
    for (std::uint32_t i = 0; i < n && r; i++) r = (r * f) >> 32;
    return r;
}

PheromoneDecay::PheromoneDecay(Nezumi::Tick period, std::uint32_t half_life_epochs)
    : period_          (period          )
    , half_life_epochs_(half_life_epochs)
{
    assert(period           > 0 && "PheromoneDecay: period must be > 0");
    assert(half_life_epochs > 0 && "PheromoneDecay: half life must be > 0");

    // Largest per-epoch factor f (Q32) with f^half_life <= 1/2. f^n is
    // monotonic in f, so a binary search over [0, 1) finds it exactly.
    std::uint64_t lo = 0;           // f^h <= 1/2 holds
    std::uint64_t hi = Q32_ONE - 1; // search ceiling
    while (lo < hi) {
        const std::uint64_t mid = lo + (hi - lo + 1) / 2;
        if (pow_q32(mid, half_life_epochs_) <= Q32_ONE / 2) lo = mid;
        else                                                hi = mid - 1;
    }
    const std::uint64_t f = lo;

    // f^k until even the largest strength rounds down to 0
    factors_.push_back(Q32_ONE);
    for (;;) {
        const std::uint64_t next = (factors_.back() * f) >> 32;
        if (((static_cast<std::uint64_t>(PHEROMONE_MAX) * next) >> 32) == 0) break;
        factors_.push_back(next);
    }
}

} // namespace Nightmourn
