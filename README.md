# Ashen Dominion

Ashen Dominion is an original dark-medieval-horror real-time strategy game. It combines asymmetric
factions, economy and production, positional combat, story missions, and competitive matches without
using copyrighted settings, characters, or assets from other games or manga.

## Project status

The repository contains two complementary implementations:

- `native/` is the new portable C++20 simulation core. It owns authoritative rules, fixed-step updates,
  ordered commands, faction data, economy, production, combat, win state, and deterministic state hashes.
- `src/` is the playable TypeScript/Three.js vertical slice. It remains the interaction, balance, UI, and
  visual reference while systems migrate into C++.

The migration is incremental. The web build is not discarded until the native client reaches feature
parity.

## Native build

Requirements: a C++20 compiler and CMake 3.24 or newer.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the headless simulation from the generated build directory to inspect its final tick, economy, and
deterministic state hash.

## Web prototype

```bash
npm ci
npm run dev
```

Tests and production build:

```bash
npm test -- --run
npm run build
```

## Architecture

The native game is split deliberately:

- `ashen_core`: engine-independent authoritative simulation with no renderer or Unreal dependency.
- Headless tools and tests: fast balance, replay, determinism, and server validation.
- Future Unreal bridge: input, camera, UI, audio, assets, animation, and visual representation.
- Future dedicated server: owns `ashen_core`; clients submit commands and receive verified snapshots.

See [docs/cpp-migration.md](docs/cpp-migration.md) for the migration plan and ownership rules.
