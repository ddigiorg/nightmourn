# World Roadmap

Future iterations, one topic at a time. Each item is independent unless a
dependency is listed. Suggested order is at the bottom.

---

## 1. Flat hash maps for lookup

**Now:** `std::unordered_map` for `ChunkID → Chunk*` and `SectorID → Sector*`.

**Change:** swap to an open-addressing map (`boost::unordered_flat_map`,
`ankerl::unordered_dense`, or `absl::flat_hash_map`).

- `ChunkID` packs into a `uint32_t`, so the key can be the packed value with
  the existing fmix32 hash.
- Only `Surface.hpp/.cpp` change: the two map member types.
- Rule that still applies: lookup only. Never iterate these maps in the sim.
  Open-addressing iteration order depends on capacity and erase history.

---

## 2. Fixed-point positions (24.8, Factorio-style)

**Format:** `int32_t` per axis, 8 fractional bits. Range ±8,388,607 tiles at
1/256-tile precision. Covers the full ChunkID range as long as
`CHUNK_SIZE <= 256` tiles.

**Steps**
1. Add `Fixed` (or `MapCoord`) type: `int32_t raw`; `from_tiles(int)`,
   `to_float()` (render only), `+ - ` and `* / ` by integer or by another
   fixed with explicit rounding.
2. `Position2D`, `BoundingBox`, and `velocity` become fixed. Velocity as
   *sub-tiles per tick* (not per second) removes `dt` from the sim entirely.
3. `ChunkID::pos_to_id` becomes an arithmetic shift:
   `raw >> (8 + log2(CHUNK_SIZE))`. Shifts on negative ints are
   floor-division in C++20, so no `std::floor`.
4. Distance: `int64_t` squared distance, no `sqrt` in the sim. If a real
   length is needed, use an integer sqrt.
5. `to_vec2()` stays for the renderer.

**Gotchas:** watch for `int32 * int32` overflow (widen to `int64` first);
keep all rounding explicit and identical everywhere.

**Until this lands:** float sim is deterministic only for the same binary on
the same CPU family. If cross-platform play matters before then, compile the
sim with `-ffp-contract=off` (GCC/Clang) or `/fp:precise` (MSVC) and no
fast-math.

---

## 3. Random

Observations on the current `Nezumi/Core/Random.hpp`:

| Issue | Why it matters |
|---|---|
| `std::uniform_int_distribution`, `uniform_real_distribution`, `bernoulli_distribution`, `discrete_distribution` | Algorithms are implementation-defined. MSVC and libstdc++ return different values from the same engine state. Desync across platforms. |
| `std::shuffle` | Also implementation-defined. |
| `mt19937_64` | ~2.5 KB state, slow to seed, text-based snapshot. Overkill for games. |
| Default constructor seeds from `std::random_device` | Fine for non-sim uses, but a sim RNG should be impossible to seed non-deterministically. |
| `pick_from` throws | Exceptions in the sim hot path. Prefer an assert. |
| Comment says non-copyable, copy is defaulted | Copying a sim RNG silently forks the stream. |

**Change**
1. Engine: small, fast generator: xoshiro256** or PCG32 (8–32 bytes of state,
   trivially serializable as raw integers).
2. Own distributions:
   - Bounded int: Lemire's multiply-shift with rejection.
   - Float in [0,1): top 24 bits × 2⁻²⁴.
   - Bool with probability: compare against an integer threshold.
   - Weighted index: integer cumulative weights + bounded int.
   - Shuffle: Fisher–Yates with the bounded int above.
3. Split types: `SimRandom` (seed required, non-copyable, snapshot/restore as
   raw state) and the existing `Random` for UI/render/audio.
4. For multithreading (see §6): a counter-based helper
   `hash_rng(seed, tick, entity_index, salt)` so results don't depend on call
   order across threads.

World already owns its own seeded RNG (`WorldConfig::seed`), separate from
`EngineContext::random`, so render code can't consume sim rolls.

---

## 4. Desync detection

**Goal:** every N ticks each peer hashes sim state and exchanges the hash.
Mismatch → log tick number, dump state, stop.

**What to hash** (in canonical order):
- `tick_`, RNG state
- Every entity in SlotMap index order: handle, type, flags, position,
  velocity, health/mana, derived-type state (add a virtual
  `hash_state(Hasher&)` to `Entity`)
- Pending command buffers (sizes + contents)
- SlotMap free-list head and slot versions (cheap, catches allocation drift)

**Tips**
- Hash raw bytes of fixed-point values. With floats, hash bit patterns
  (`std::bit_cast<uint32_t>`).
- Hash per surface/chunk too, so a mismatch can be narrowed to a region.
- Same machinery gives you replay verification and save/load round-trip
  tests for free.

---

## 5. Sector model: dense-in-chunk vs per-tick rebuild

Both remove the second sparse hash map and per-entity sector insert/remove.

| | Dense sectors inside chunks | Rebuild collision grid every tick |
|---|---|---|
| Layout | Fixed `Sector[N×N]` array in each Chunk; `CHUNK_SIZE` a multiple of `SECTOR_SIZE` | One flat array per surface, bucketed by cell via counting sort after movement |
| Cost model | O(entities that change sector) per tick | O(collidable entities in active chunks) per tick |
| Best when | Most collidables are static (buildings, resources) | Most collidables move every tick (RTS armies) |
| Inactive/static entities | Free: they stay registered | Must be re-added every tick, or kept in a separate static grid |
| Threading | Partitions with chunks automatically | Embarrassingly parallel bucketing; merge is deterministic if sorted by (cell, handle) |
| Memory | Per loaded chunk, even if empty | Proportional to collidables only |

**Recommendation:** dense-in-chunk, plus a small per-tick list for fast
movers if profiling shows churn. It matches your factory/RTS hybrid (many
static entities, chunk-based activation), keeps sleeping entities free, and
lines up with chunk-partitioned threading. Decide with a profile of your
real entity mix: if >70% of collidables move every tick, the rebuild wins.

---

## 6. Multithreading

The phase structure already provides the seams. Steps when you get there:

1. **Parallel entity tick (phase 3):** split `active_chunks_` into ranges.
   Entities already write only themselves and issue commands.
2. **Per-chunk command buffers:** each chunk gets its own spawn / destroy /
   activate / damage vectors. At the phase barrier, concatenate in
   `active_chunks_` order. Never merge in thread-completion order.
3. **Spawning off-thread:** `SlotMap::create` isn't thread-safe. Store spawn
   *requests* (type + args, e.g. via a type-erased factory) per chunk and
   construct during the command phase instead.
4. **RNG:** counter-based (see §3), so a roll doesn't depend on which thread
   ran first.
5. **Movement (phase 5):** integration parallelizes per chunk; spatial
   updates (chunk/sector moves) stay serial, or use checkerboard (2×2
   coloring) so neighboring chunks never update concurrently.
6. **Reads:** queries during phase 3 read state from the start of the tick,
   which no one writes during that phase, so they need no locks.

---

## 7. Per-type entity pools (SlotMap per entity type)

**Goal:** contiguous memory per type and batched, non-virtual ticking.

**Shape**
- Keep one global **handle table** (index → `{Entity*, version, next_free}`),
  so `EntityHandle` stays polymorphic and stable.
- Add `Pool<T>` per concrete type: fixed-size pages (e.g. 1024 objects),
  never reallocated, with its own free list. Addresses stay stable.
- Registration uses the `TYPE_ID` already in every derived class:

```cpp
// EntityTypes.hpp (game code): single source of truth for type IDs
enum class EntityTypes : uint8_t { Unit = 1, Tree = 2, Building = 3 };

// World
template <EntityType T> Pool<T>& pool();        // indexed by T::TYPE_ID
template <EntityType T> void register_type();   // creates the pool
```

- Type IDs index a fixed array of `std::unique_ptr<IPool>`.
- Batched tick: for each registered type, walk its pool's live, active
  entities in index order and call `T::tick` non-virtually. Chunk activity
  still gates *which* entities are active (keep a per-entity active bit).

**Prerequisite decision:** whether chunk lists or per-type pools drive the
tick order. Both are deterministic; pools are faster, chunks give spatial
locality for neighbor queries.

---

## 8. Smaller items noted along the way

- **Chunk loading/unloading:** player-radius activation, chunk unload with
  entity serialization. Remember: all peers must load the same chunks.
- **Cross-surface entity transfer:** a deferred `transfer_entity(handle,
  surface, position)` command (remove from source, insert into destination in
  the command phase). Needed before surfaces can be removed.
- **Collision response:** `find_colliding` exists; resolution (push-apart,
  blocking) belongs in phase 5 after integration.
- **Render interpolation:** snapshot positions of visible entities after each
  tick on the render side, or add `prev_position` (8 B; there is 24 B free).
- **Save/load:** serialize SlotMap slots *including* versions and free-list
  order, RNG state, `tick_`, pending command buffers. Active chunk order is
  already canonical (sorted), so it needs no special handling.
- **32-bit handles:** 20/12 or 22/10 split with slot retirement on version
  saturation (see conversation notes).
- **Entity slimming:** move health/mana to a `Damageable` mixin; target a
  64-byte base.
- **Query range clamping:** `span_from_box` asserts if a query box leaves the
  ChunkID range. Clamp query boxes to the surface limits.
- **EventBus volume:** each enqueued event is a heap allocation plus a
  `std::map` lookup. `OnEntityDamaged` can be high-volume in large battles;
  consider batching it (one event per tick with a span of records) or making
  it opt-in.

---

## 9. Chunk data: pheromones, smoke, decorations

Per-chunk data is baked into `Chunk`, not a layer system.

- **Pheromones (done):** `Chunk/Pheromones.hpp`. 16 `uint16_t` strengths
  per chunk with integer lazy decay; deposits in tick phase 7; reads through
  `Surface::pheromone_at` and `pheromone_gradient`. Still to do: include
  `values` and `epoch` in save/load and in the desync checksum (§4), and
  have unit AI follow the gradient.
- **Smoke and decorations:** add a flat array to `Chunk` when a feature
  needs one (Factorio keeps smoke and decoratives per chunk the same way).
  Neither is planned yet.

---

## Suggested order

1. Random (§3): small, self-contained, fixes a real desync source.
2. Desync detection (§4): turns every later change into a testable one.
3. Fixed-point (§2)
4. Sector model (§5)
5. Flat maps (§1): trivial, do it whenever.
6. Per-type pools (§7)
7. Multithreading (§6): needs §3, §4, and ideally §7.
