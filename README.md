# Nightmourn

A 2D action-adventure and survival game built using the [Nezumi Engine](https://github.com/ddigiorg/nezumi).

## Build and Run Instructions

```sh
git clone https://github.com/ddigiorg/nightmourn.git
cd nightmourn
tools/build.sh
./.build/debug/nightmourn
```

Demos are built next to the game:

```sh
./.build/debug/demos/bin/00-hello-world
```

## Layout

| Path | What lives there |
| --- | --- |
| `src/main.cpp` | The game executable |
| `src/Nightmourn/` | Game code shared by the game and the demos, included as `<Nightmourn/...>`. The simulation (`World/`, `Surface/`, `Chunk/`, `Sector/`, `Entity/`, `Unit/`, `Resource/`, ...) builds as the headless `nightmourn_world` library; drawing, GUI, input and `Player` build as `nightmourn_game` on top |
| `demos/` | One small prototype per feature, the game's version of Nezumi's `examples/` |
| `data/` | Game data, Factorio style: `base/graphics/` (PNGs) and `base/prototypes/` (JSON naming them, see `src/Nightmourn/Data/Prototypes.hpp`). Copied next to each executable; sprite sheets are packed from it when the game starts. Adding a unit look, tree or terrain is a PNG plus a few lines of JSON |
| `tools/` | Generators for the placeholder art in `data/` (`gen_trees.py`, `gen_meadow.py`, `gen_ui.py`) |
| `tests/` | Headless simulation tests (link `nightmourn_world` only); run with `ctest --test-dir .build/debug` |

## Engine Version

Nezumi is fetched by CMake from its `main` branch (`GIT_TAG` in
`CMakeLists.txt`). To freeze the engine at a known version, set `GIT_TAG` to a
commit SHA instead.

## Developing the Engine and Game Together

To build against a local Nezumi checkout instead of the fetched one, clone
both repositories side by side:

```
nezumi/
nightmourn/
```

and create `CMakeUserPresets.json` (gitignored) in `nightmourn/`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "dev-local",
      "displayName": "Debug (local Nezumi)",
      "inherits": "debug",
      "binaryDir": "${sourceDir}/.build/dev-local",
      "cacheVariables": {
        "FETCHCONTENT_SOURCE_DIR_NEZUMI": "${sourceDir}/../nezumi"
      }
    }
  ],
  "buildPresets": [
    { "name": "dev-local", "configurePreset": "dev-local" }
  ]
}
```

Then:

```sh
cmake --preset dev-local
cmake --build --preset dev-local
```

Engine edits in `../nezumi` are picked up on the next build.
