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

## Isometric

Phases 0 and 1 of the isometric plan are done: `Render::Projection`, the
`Quad` kind, every drawer in view space, screen-relative WASD, tile and
figure picking, and a `use-top-down-view` debug toggle. Decided: projection
on the CPU in the drawers, square textures projected onto diamonds, flat
world, footprint depth for buildings, 3D pre-rendered art, 64x32 tiles.

Ground is world-space (AoE2: DE style): one seamless 1024² meadow covers
8x8 tiles (`tools/gen_meadow.py`). Research and the reasoning behind the
terrain roadmap below: `isometric-plan/TILE-RESEARCH.md` in the project
files.

- [ ] **Atlas frames with pivots** (JSON sidecar): per-frame size and feet
      pivot, replacing the fixed `FIGURE_SPRITE_BOUNDS` box. Comes out of
      the packer below.
- [ ] **Player-color mask** in a sprite shader variant, replacing one atlas
      row per faction.
- [ ] **First pre-rendered unit** (Blender: orthographic, 30° elevation,
      45° yaw), 8 directions (5 stored + 3 mirrored), walk + attack. Frames
      are picked with `Projection::dir_to_heading`.
- [ ] **Footprint depth for buildings**: a depth attachment on the world
      pass, and a sprite variant that writes the depth of its footprint's
      front edge per pixel column. Then occluded-unit silhouettes and a
      placement ghost snapped to `tile_at`.
- [ ] **Darkness**: light map pass (offscreen target + multiply), hero light
      radius, fog of war per chunk, day/night ambient.
- [ ] Drag-box selection: query the world bounds of the screen box, keep
      figures whose projected feet fall inside it.

### Asset pipeline

Decisions for the rest of the roadmap to build on.

- **Sprite packer: yes.** Factorio ships each entity's sprites as separate
  PNG sheets and packs them into atlas pages when the game loads. AoE2: DE
  ships separate files too: one sprite file per unit graphic, one `.dds` per
  terrain. Loose files are what an artist (or Blender) produces. Atlases are
  what the GPU wants, so something has to pack them.
  - Pack **at build time**, not at startup: a `tools/` packer run by a CMake
    custom command that writes atlas pages plus a JSON of frames
    (`{ page, tex_box, size, pivot }`). It's simpler in the engine, and
    there's no packing cost at load. Move it to startup (Factorio's way)
    only if mods need to add sprites. Then cache the result on disk.
  - Group sprites drawn together onto the same page (units with units,
    terrain with terrain). Leave padding of at least `2^(mips-1)` px between
    sprites, so mips don't blend neighbors.
- **File format: PNG for source art; a GPU container for shipping, later.**
  - `.dds` is a container, not a pixel format. The AoE2: DE wiki lists its
    terrain `.dds` files as linear RGBA 8-bit, uncompressed. DDS (or KTX2)
    usually earns its keep with GPU block compression (BC1/BC3/BC7: 4-8x
    less VRAM, no decode) and a mip chain stored in the file.
  - Raw RGBA8 files would load without a decode step but take about 2x
    PNG's disk space (1024² is 4 MB raw vs 1.8 MB for the meadow PNG),
    and no image editor opens them. Not worth it.
  - So: keep PNGs in `assets/` as the source of truth. When load time or
    VRAM matters, have the packer write KTX2/DDS (BC7, with mips) next to
    them, and have the engine load those. Until then the engine loads
    PNGs.
- **Mips: one source image, generated chain.** Author one full-size image
  and generate the levels (box filter in premultiplied alpha, see
  `AtlasConfig::mip_levels`'s TODO), either at load or in the packer. A
  DDS/KTX2 file stores the whole chain in one file, so "one file per level"
  only makes sense if someone hand-paints the small levels, which nobody
  does for terrain.

### Terrain: per-chunk ground kind (TILE-RESEARCH step 5)

The end state: one instance per visible chunk, terrains blended in the
shader. It replaces `ChunkDrawer`'s per-tile Quads. Prerequisites first,
each useful on its own:

- [ ] **Mipmaps** (Renderer #7, `AtlasConfig::mip_levels` TODO): CPU chain or
      `glGenerateMipmap`, a `mip_filter` on the sampler. Needed now as well:
      below about zoom 0.5 the meadow aliases into a shimmer, because a 128
      px tile cell lands on a few screen pixels with no mips.
- [ ] **Texture2DArray** in the RHI and the renderer (Renderer #1): array
      textures, `sampler2DArray`, and atlas pages as layers with the page
      index per instance. Units batch across pages, and terrains get one
      layer each.
- [ ] **Materials with more than one texture**: today a material is one
      texture + sampler bind group. The ground kind needs the terrain array
      and a tile-type map.
- [ ] **Tile storage in chunks** (engine World): a 32x32 `uint8_t` terrain id
      per chunk, `Surface::get_tile` / `set_tile` (deferred like the other
      world changes), and a per-chunk "tiles changed" version the draw side
      can compare against. Pathing costs and buildability read the same
      array.
- [ ] **Terrain definitions**: id → texture layer, tiles per texture (N),
      blend priority. The meadow is terrain 0.
- [ ] **Ground kind** (engine): instance = the chunk's 4 projected corners
      plus its tile-map layer. The vertex shader interpolates world
      position next to view position. The fragment shader reads the tile id
      with `texelFetch` at `floor(world)`, samples the terrain layer at
      `fract(world / N)`, and blends with the 3x3 neighbors by priority,
      through a shared noise/mask texture (blendomatic's rule, per pixel).
- [ ] **Tile-map layers**: an R8 `Texture2DArray` of 32x32 layers, one per
      visible chunk, handed out from a pool (least recently drawn reused).
      Re-upload a layer only when its chunk's tile version changes.
- [ ] **Switch `ChunkDrawer`** to the ground kind. The grid and top-down view
      keep working, since the kind takes projected corners like `Quad`.
- [ ] **Grid kind** taking the inverse projection, so the world grid draws
      in the same pass (todo above).
- [ ] Later: a **detail layer** of decals (rocks, flowers, cracks) over the
      material, Factorio-style. That's where 2x2/4x4 variants belong.

### Terrain: cached ground (TILE-RESEARCH step 6, only if profiled)

Factorio keeps terrain in a buffer texture and redraws only the strips a
scroll reveals (FFF #333), because its transitions make the ground expensive
per pixel. Do this only if the ground kind shows up in a GPU profile
(Renderer #4, GPU timing, comes first).

- [ ] **GPU timing** (Renderer #4): `GL_TIME_ELAPSED` per pass, so the
      decision is measured.
- [ ] **Offscreen color targets** in the renderer: a world-ground target a
      little larger than the screen, drawn before the world pass and
      composited as one quad.
- [ ] **Scroll reuse**: keep the target's view offset, shift by whole pixels
      as the camera moves, and redraw only the revealed strips (a scissored
      ground pass each).
- [ ] **Invalidation**: full redraw on zoom, projection switch or resize. A
      chunk's region on a tile change.
- [ ] The light map pass (Darkness) needs the same offscreen-target support,
      so build that support once.

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

### UI

From the first UI pass (immediate-mode `UI::Context`, the `UIQuad` kind).

- [x] **Escape while typing.** The demo no longer uses
      `EngineConfig::escape_exits`: Escape is `Action::ToggleMainMenu`, an
      InputMap action, so a focused text field keeps it. (The engine flag
      still listens to raw keys, for examples.)
- [ ] **Keyboard and gamepad navigation**: Tab between widgets, arrow keys
      in dropdowns, a focus ring. Needed before a console port or a
      controller-only menu.
- [x] **Tooltips** on hover (`UI::Context::tooltip`, after
      `Style::tooltip_delay`).
- [ ] **Text field editing**: selection, clipboard (SDL has it), Ctrl+arrow
      word jumps, caret blink, UTF-8 once the font covers more than ASCII.
- [ ] **Scrollbar dragging.** Panels scroll with the wheel and show a thumb,
      but the thumb can't be dragged yet.
- [x] **Images and icons** in widgets (`UI::Image`, any atlas; the UI bumps
      the draw order per texture run).
- [ ] **Skinning**: `UI::NineSlice` exists and the action bar uses it, but
      panels and the built-in buttons still draw `Style` boxes. Add
      optional frames to `Style`.
- [ ] **Layout helpers**: columns / tables, word-wrapped text, and
      right-aligned or centered rows.
- [ ] **HiDPI**: the UI maps window points to backbuffer pixels, but sizes
      aren't scaled. Add `Style::scale` from the display's content scale.
- [ ] **Persist panel positions** (where the player dragged the debug menu)
      with the rest of the settings once there's serialization.

### Action bar

From the first pass (`GUI/ActionBar`, `UI::Context::slot_grid`).

- [ ] **Rebinding**: a WoW-style bind mode (hover a slot, press a key) in
      the Options menu, and saving bindings with the settings.
- [ ] **More slots**: the upgrade system that grows the bar
      (`Action::MAX_ACTION_SLOTS` already reserves the ids); a second row,
      or Shift+1..0 once `InputSource` has modifiers.
- [ ] **Items that do something** when used, and cooldowns (`UI::Slot`
      has a cooldown shade; a clock sweep like WoW's needs a shader shape).
- [ ] **Inventory** as a slot grid that drags into the bar with
      `ActionBar::DRAG_TYPE`, and drops items on the ground from
      `take_unclaimed_drop`.
- [ ] **Hand-drawn art** to replace `tools/gen_ui.py`'s placeholders at the
      same pixel boxes (`GUI/UIAtlas.hpp`).

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

### Resources and trees

Trees are `Nezumi::Resource`s (an `EntityResource`: they never tick; mining
goes through `World::mine_entity` and `OnEntityMined`). `type_id` is
Resource, `subtype_id` what it is (`ResourceType::Tree`), `variety_id` which
one (`TreeVariety::Oak`, `Pine`), and `item` the `Nezumi::ItemID` mining
yields (`Items::Wood`). Decorative for now.

- [ ] Storage system (Nezumi `World/Item/`), then a gatherer: find a
      resource with `EntitySearchFilter::resources(Resource::TYPE_ID)` (and
      the subtype it wants), mine it on an interval, add
      `OnEntityMined::amount` of `OnEntityMined::item` to its storage.
- [ ] Collision response, so figures walk around trunks (trees are already
      collidable, with a 0.8 tile box).
- [ ] Figures hidden behind a canopy: AoE2 draws an outline of them on top.
      A silhouette pass (sprites drawn again, flat color, behind-depth only)
      or fading canopies near the hero.
- [ ] A stump (and a felled-tree frame) once trees can be cut, instead of
      vanishing at 0 wood.
- [ ] More subtypes and varieties (palm and dead trees, berry bushes, stone
      and gold mines) as `ResourceConfig`s plus atlas looks. Once the atlas has a sidecar with
      pivots, a kind's looks can differ in size.
- [ ] Map generation: today the demo scatters trees with value noise; a
      real map generator should own forests, clearings and resource spots.

