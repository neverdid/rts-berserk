# C++ migration plan

## Decision

Ashen Dominion uses an Unreal-hosted C++ client while keeping authoritative RTS rules in a portable
C++20 library. The browser prototype remains playable during migration and acts as a behavior, balance,
story, and interaction reference.

This is not a line-by-line TypeScript translation. Rendering and platform behavior are deliberately
separated from deterministic game rules.

## Ownership boundaries

### `AshenCore`

Owns deterministic, headless game rules:

- fixed 20 Hz simulation ticks
- strong entity and resource IDs
- ordered player commands
- integer world coordinates
- faction and unit definitions
- movement, gathering, production, combat, supply, and victory
- state hashing for replay and desync detection

It has no dependency on Unreal types, rendering, audio, operating-system APIs, or wall-clock time. CMake
and Unreal compile the same files from `unreal/AshenDominion/Source/AshenCore/`.

### Unreal client

Owns presentation and platform behavior:

- RTS camera, screen-edge movement, smooth zoom, and input mapping
- click, additive, and drag-box selection feedback
- contextual command dispatch to authoritative entity IDs
- terrain dressing, faction meshes, materials, lighting, fog, menu/HUD, and visual entity synchronization
- future animation, VFX, audio, mission presentation, and localization
- future lobby, account, matchmaking, and platform services

Actors are visual proxies. They do not decide damage, income, production completion, or victory.

### Dedicated server

The planned server will run `AshenCore` without graphics. Clients submit tick-stamped commands; the
server validates ownership and resources, advances the authoritative simulation, and periodically sends
state hashes and snapshots. Per-unit Actor replication is intentionally avoided.

## Migration stages

1. **Native foundation - complete**: fixed-step core, commands, economy, combat, tests, and CI.
2. **Unreal foundation - complete**: UE 5.8 project, module bridge, camera, selection, visual registry,
   contextual commands, HUD, and a procedural battlefield.
3. **Parity fixtures - next**: export representative TypeScript scenarios and assert equivalent C++
   outcomes.
4. **Feature parity**: pathfinding, construction, research, faction powers, control points, fog, advanced
   AI, and story objectives.
5. **Competitive networking**: authoritative server, command buffering, reconnect snapshots, replays,
   matchmaking, and desync diagnostics.
6. **Content pipeline**: project-owned terrain materials, production meshes, maps, missions, animation,
   audio, cinematics, and localization.
7. **Native transition**: retire the browser runtime only after story and PvP acceptance tests pass.

## Quality gates

- Every rule change requires a headless test.
- Simulation iteration order must be explicit and stable.
- No gameplay decision may depend on frame rate or renderer state.
- Commands are validated atomically before state mutation.
- PvP and story use the same simulation systems; missions add data and scripted objectives.
- Replays store setup, commands, and version metadata rather than rendered state.
- CI builds and tests the native core on Windows and Linux and keeps the web prototype green.
- Unreal changes must pass UHT, an editor build, the `Ashen` automation suite, a game launch, and a
  nonblank render inspection before merge.

## Current parity

Implemented in portable C++:

- three asymmetric factions
- six entity archetypes and faction overrides
- fixed-step movement
- worker harvesting and faction income modifiers
- production queues, costs, build time, and supply
- targeted combat and armor bonuses
- command-structure victory
- ordered command replay and deterministic state hashing

Implemented in the Unreal client:

- fixed-step subsystem integration with bounded catch-up
- entity and resource visual proxy lifecycle
- contextual movement, attack, gathering, and training
- player camera, edge scroll, smooth zoom, single/additive/box selection
- paused deployment menu, responsive resource/selection HUD, outcome presentation, and tactical minimap
- automatic opening economy and a deterministic local skirmish commander with production and attack waves
- distinct multi-part human and monster units, castles, barracks, resources, selection rings, and health bars
- lit battlefield with fortified territories, roads, two bridges, water shader and ripples, ritual island,
  grass, living/dead trees, rocks, fog, post-processing, and instanced boundary dressing
- Blueprint-safe command/state API
- in-engine deterministic automation coverage

Still using the TypeScript prototype as a reference:

- grid pathfinding, collision separation, and formations
- building placement and construction
- stances, attack-move, retreat, research, and faction powers
- projectiles, resolve, terror, wards, control points, and Ruin Tide
- advanced AI personalities, campaign objectives, dialogue, and cinematics
- fog of war, full command card, animation, VFX, audio, and production assets
- online lobby, authoritative server, reconnect, replays, and ranked PvP
