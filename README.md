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
| `src/Nightmourn/` | Game code shared by the game and the demos, included as `<Nightmourn/...>` |
| `demos/` | One small prototype per feature, the game's version of Nezumi's `examples/` |
| `assets/` | Game assets, copied over the engine's assets next to each executable |
| `tests/` | Tests (none yet) |

## Engine Version

Nezumi is fetched by CMake and pinned to a commit SHA (`GIT_TAG` in
`CMakeLists.txt`). To update the engine, change that SHA.

## Developing the Engine and Game Together

To build against a local Nezumi checkout instead of the pinned commit, clone
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
