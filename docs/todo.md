# Todo

Next steps and improvements suggested during past work on Nezumi and
Nightmourn, deduplicated and grouped by area. Items that are already done on
`main` have been left out (checked against nezumi `dfe2012` and nightmourn
`11ee012`).

Two longer roadmaps already live in the engine and aren't repeated here.
Items below point to them where they overlap:

- `nezumi/src/Nezumi/Render/Renderer.hpp`, "Future Improvements" (Renderer #N)
- `nezumi/src/Nezumi/World/ROADMAP.md` (ROADMAP §N)

---

## Decisions to make

These change how later items are built, so they come first.

- [ ] **Faction model.** Can an entity belong to several factions, or should
      each entity have one owner `FactionIndex` plus a per-faction ally table
      on the World (Factorio's model)? Decide before pheromones.
- [ ] **Units given their own command.** Today they leave their group for
      good. Alternative: detach, then rejoin the group when the command
      finishes (Age of Empires style).
- [ ] **Group engagement.** Today the whole group engages on first contact.
      Alternative: only members in reach fight while the rest hold formation
      (needs a per-member "engaged" bit in the group plan).
- [ ] **Character health model.** Keep the engine's `uint16_t` health and
      damage phase, or give Nightmourn its own (damage types, resistances,
      mana costs)?
- [ ] **Multiplayer or replays.** If either is planned, turn the
      between-ticks command convention into a real per-tick `PlayerCommand`
      stream keyed by `PlayerID`, applied in the command phase.
- [ ] **Where Unit/UnitGroup live.** Current choice: keep them in the engine.
      Revisit if Nightmourn's units need their own rules (move to the game,
      or split into an optional `nezumi_units` library).

---

## Nezumi (engine)

### Bugs and small fixes

- [ ] `World/Surface/SurfaceID.hpp` forward-declares `class Surface;` at
      global scope. Move it inside `namespace Nezumi`; files with
      `using namespace Nezumi` get "reference to 'Surface' is ambiguous".
- [ ] `Line::width` and `Circle::width` comments say "world pixels", but the
      shaders treat width as world units (tiles).
- [ ] `Core/EventPump.cpp` emits 7 `-Wmissing-field-initializers` warnings
      (designated initializers on events derived from `IEvent`). Build each
      event with its default constructor and assign the fields.
- [ ] `World::create_entity` has two doc comments stacked on each other.
      Keep one.
- [ ] `EntitySearchFilter::friends_of` also matches UnitGroups, because it
      doesn't require the Damageable flag. Add a type or flag requirement
      when something starts calling it.
- [ ] `Engine::get_stats()` should be `const`.
- [ ] A parallel Debug build once failed copying `assets/`, apparently
      because several examples copy into the same folder at once. Investigate
      if it shows up again.
- [ ] ROADMAP §8 "Entity slimming" still says to move health/mana to a
      Damageable mixin. Health is done (`EntityDamageable`); reword it.

### Render

- [ ] **Stop zero-filling the command arena.** `CommandQueue::push_bytes`
      calls `arena_.resize()` per push, which zeroes bytes that `memcpy`
      then overwrites (about 4.8 MB per frame at 100k sprites). Track a
      `used_` size and grow only on overflow. Code is in the renderer
      cleanups review.
- [ ] **Bulk submit.** `draw_many<K>(span, params)` that pushes one
      `DrawCommand` for N instances, for tiles, decals, particles and
      bullets. `BatchBuilder` would take the stride from `KindRegistry`.
- [ ] **Retained static layers** for tiles that rarely change (Renderer #3).
- [ ] **Texture2DArray atlases** so y-sorted sprites from different pages
      still batch (Renderer #1). Until then, keep all unit sprites on one
      atlas page.
- [ ] **Stream buffer stalls.** Orphan the buffer (`glBufferData(..., nullptr,
      GL_DYNAMIC_DRAW)`) before a write at offset 0, or go to persistently
      mapped buffers (Renderer #5). Only after profiling on real hardware.
      Instance data is currently copied four times on its way to the GPU.
- [ ] **Slimmer `Sprite`**: RGBA8 tint plus `Unorm16x4` UVs takes it from
      48 to 32 bytes (Renderer #6). Only once bandwidth-bound.
- [ ] **Grid kind.** A screen-sized quad whose fragment shader draws tile
      and chunk lines from `fract(world_pos)`. One instance instead of 66
      lines per chunk for Nightmourn's tile grid.
- [ ] **Multithreaded submission**: per-thread `CommandQueue`s merged before
      the sort (Renderer #11).
- [ ] `CameraConfig::size` and `CameraConfig::name` are dead: the renderer
      overwrites view camera sizes every frame and `Camera` never stores the
      name. Drop them, or add a per-view "auto-size to backbuffer" flag (also
      useful for a fixed virtual resolution in pixel art).
- [ ] When atlas regions come back, consider
      `draw_sprite(const Atlas&, SpriteID, pos, ...)` to match `draw_text`.
      `Atlas/SpritePixelBox.hpp` and `Atlas/SpriteTexelBox.hpp` are empty
      leftovers from that.
- [ ] Without `GL_ARB_clip_control`, `Camera` should use `glm::orthoNO`
      (check `DeviceInfo::clip_depth_zero_to_one`). Only matters for
      depth-tested 3D; 2D at z = 0 is fine as is.

### Input, Control and Core

- [ ] Report mouse enter/leave from `EventPump` (the cases are commented
      out). Edge pan keeps scrolling when the mouse leaves the window.
- [ ] HiDPI: mouse positions and camera size use different units on
      scaled displays.
- [ ] Render interpolation between ticks: draw at
      `prev + (pos - prev) * alpha` so sprites don't step at 60 Hz. Either
      snapshot visible positions after each tick or add `prev_position` to
      `Entity` (ROADMAP §8). Camera follow benefits too.
- [ ] A `snap_to_8_directions(glm::vec2)` helper in `Core/`, if a second
      user appears.

### World: entities and combat

- [ ] **Unit separation / collision response.** Units stack when they
      fight and overlap when converging on a group's engage position. A
      push-apart steer in phase 5 using `find_colliding` (needs
      `set_collidable()` on units; ROADMAP §8).
- [ ] **Attack wind-up.** `attack_windup` ticks in `UnitConfig` so the hit
      lands on the swing frame once Animation exists. Store "swing started
      at tick" on the unit, not in the animator.
- [ ] **Return to post.** Idle units that auto-engage walk back to where
      they were idling.
- [ ] **`OnEntityDied`** event carrying the killer's handle (kill feeds,
      score).
- [ ] **More unit states:** Dying (corpse/death animation before removal),
      HoldPosition (attack in range, never move), Gathering/Building.
- [ ] **Armor and resistances:** one virtual `mitigate(amount, source)` hook
      on `EntityDamageable`, called from the damage phase. Not on `Entity`.
- [ ] **Pheromones per faction.** Lazily allocated
      `std::array<std::unique_ptr<PheromoneGrid>, MAX_FACTIONS>` per chunk,
      deposits through a deferred command, decay as a tick phase over active
      chunks. Its own task, after the faction decision above.
- [ ] **Animation class** that picks sprites from `Unit::state()`,
      `facing()` and `state_ticks()`.

### World: performance

- [ ] Finer query grid when thousands of units pack into one chunk
      (nearest-enemy becomes O(n²)-ish). Reuse the sector grid or bucket
      units per tick (ROADMAP §5). Profile first.
- [ ] `OnEntityDamaged` is one heap allocation per hit. Batch it per tick
      or keep it opt-in (`WorldConfig::publish_damage_events`; ROADMAP §8).
- [ ] `sizeof(Unit)` is 168 B. 32-bit handles would shrink `UnitCommand`
      and the group fields (ROADMAP §8).
- [ ] `Surface::sweep_empty_sectors_` allocates a vector every sweep. Make
      it a member scratch vector if sweeps get more frequent.

### UnitGroup

- [ ] More formations (Line, Column, Wedge, Staggered) as new
      `UnitFormation` cases, and per-formation bonuses read through the
      group plan.
- [ ] Wheel turns: rotate the facing a few degrees per tick around a front
      corner instead of stopping to reform on sharp turns.
- [ ] Target distribution on engage (nearest-first, capped per enemy) to
      stop overkill and stacking.
- [ ] Stances: Aggressive (current), Defensive, Stand Ground.
- [ ] Stuck members: once terrain blocks movement, a member that stays a
      straggler for N seconds leaves the group.
- [ ] Selection: `set_selectable()` and a selection box from the group's
      `collision_bounds`.
- [ ] Pathfinding: compute one path per group from the anchor into
      `waypoints_`.

### Tests and tooling

- [ ] Pure-CPU tests for `Sort.hpp` (stability, skipped passes), `SortKey`
      (order bias and clamping, `same_state`) and `SlotMap` (stale handles,
      version wrap).
- [ ] CI builds for Nezumi (no `.github/` yet).

---

## Nightmourn (game)

### Build and repo

- [ ] `CMakeLists.txt` fetches Nezumi at `GIT_TAG main`, but its comment
      and the README say it's pinned to a commit SHA. Pin a SHA again, or
      update the docs if tracking `main` is intended.
- [ ] `tests/` only holds an empty `todo` placeholder. Add a first headless
      test (Character and Player command handling are good candidates).
- [ ] CI builds for Nightmourn.

### Draw

- [ ] Replace the if-chain in `WorldDrawer::draw_entity_` with a drawer
      table indexed by `EntityTypeID` once there are more than a handful of
      types.
- [ ] Appearance data: map `EntitySubtypeID` / `CharacterConfig` to an
      appearance table owned by the drawers, so sim configs stay free of
      graphics.
- [ ] Draw the tile grid with the engine grid kind once it exists.
- [ ] Cache each chunk's ground tile instances once surfaces store tiles
      (the biggest draw-side win).
- [ ] Multithreaded draw path: (a) per-thread command queues, (b) split
      visible chunks across workers in `WorldDrawer::draw`, (c) a render
      snapshot so tick N+1 can overlap the draw of tick N.
- [ ] A debug menu to replace the F1 to F6 debug flag keys.

### Gameplay

- [ ] Tune the hero: with 300 HP it lasts about 2 seconds against a full
      blue group.
- [ ] Day/night cycle as a game-side timer on top of World ticks (a
      lighting tint could go into the engine later).
- [ ] Enemy AI controller for characters: a sibling of `Player` that writes
      the same `CharacterCommand` from behavior logic.
- [ ] Settings screen for key bindings (bindings are already data in
      `InputMap`); later a per-player bindings profile and HUD state on
      `Player`.
- [ ] Settlements, buildings that produce unit blocks, and waypoint orders
      from the settlement (the core loop in `docs/nightmourn.md`).
