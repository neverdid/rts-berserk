# C++ migration plan

## Decision

Ashen Dominion is moving toward an Unreal-hosted C++ client while keeping its authoritative RTS rules in
a portable C++20 library. The browser prototype remains playable during migration and acts as a behavior
and balance reference.

This is not a line-by-line TypeScript translation. The migration separates responsibilities that were
previously concentrated in `simulation.ts` and `RtsThreeEngine.ts`.

## Ownership boundaries

### `ashen_core`

Owns deterministic, headless game rules:

- fixed 20 Hz simulation ticks
- strong entity and resource IDs
- ordered player commands
- integer world coordinates
- faction and unit definitions
- movement, gathering, production, combat, supply, and victory
- state hashing for replay and desync detection

It must not depend on Unreal types, rendering, audio, operating-system APIs, or wall-clock time.

### Unreal bridge

Will own presentation and platform behavior:

- camera and input mapping
- selection feedback and command previews
- terrain, meshes, animation, VFX, audio, and UI
- mapping authoritative entity IDs to visual representations
- interpolation between fixed simulation snapshots
- lobby, account, matchmaking, and platform services

Mass Entity and Mass Gameplay are candidates for visual representation and LOD at high unit counts. They
are not the source of truth for competitive rules.

### Dedicated server

Will run the same `ashen_core` library without graphics. Clients submit tick-stamped commands; the server
validates ownership and resources, advances the authoritative simulation, and periodically sends state
hashes and snapshots. Per-unit Actor replication is intentionally avoided.

## Migration stages

1. **Native foundation**: fixed-step core, commands, economy, combat, tests, and CI.
2. **Parity fixtures**: export representative TypeScript scenarios and assert equivalent C++ outcomes.
3. **Unreal shell**: camera, selection, terrain, visual entity registry, and command bridge.
4. **Feature parity**: construction, research, race powers, control points, fog, AI, and story objectives.
5. **Competitive networking**: authoritative server, command buffering, reconnect snapshots, replays, and
   desync diagnostics.
6. **Content pipeline**: data-driven units, maps, missions, animation, audio, and localization.
7. **Native transition**: retire the browser runtime only after story and PvP acceptance tests pass.

## Quality gates

- Every rule change requires a headless test.
- Simulation iteration order must be explicit and stable.
- No gameplay decision may depend on frame rate or renderer state.
- Commands are validated atomically before state mutation.
- PvP and story use the same simulation systems; missions add data and scripted objectives.
- Replays store setup, commands, and version metadata rather than rendered state.
- CI builds and tests the native core on Windows and Linux and keeps the web prototype green.

## Current parity

Implemented in C++:

- three asymmetric factions
- six entity archetypes and faction overrides
- fixed-step movement
- worker harvesting and faction income modifiers
- production queues, costs, build time, and supply
- targeted combat and armor bonuses
- command-structure victory
- ordered command replay and deterministic state hashing

Still using the TypeScript prototype as the reference:

- pathfinding and collision separation
- building placement and construction
- stances, attack-move, retreat, research, and race powers
- projectiles, resolve, terror, wards, control points, and Ruin Tide
- AI personalities and story objectives
- fog of war, navigation mesh integration, UI, rendering, and audio
