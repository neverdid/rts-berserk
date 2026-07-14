# Ashen Dominion

Ashen Dominion is an original dark-medieval-horror real-time strategy game. It combines asymmetric
factions, economy and production, positional combat, story missions, and competitive matches without
using copyrighted settings, characters, names, or assets from other games or manga.

## Project status

The repository now has three cooperating layers:

- `unreal/AshenDominion/` is the playable Unreal Engine 5.8 C++ client. It currently provides an RTS
  camera, edge scrolling and zoom, click and drag-box selection, contextual move/attack/gather commands,
  production hotkeys, a deployment menu, tactical minimap, a skirmish AI commander, two distinct
  multi-part faction silhouettes, and a procedural dark-medieval battlefield with castles, forests,
  roads, bridges, a contested island, and shader-driven river water.
- `unreal/AshenDominion/Source/AshenCore/` is the portable C++20 authoritative simulation. CMake and
  Unreal compile these exact same sources, so gameplay rules do not fork between clients.
- `src/` is the playable TypeScript/Three.js reference slice. It remains available for rapid interaction,
  balance, story, and visual experiments while Unreal reaches feature parity.

The Unreal skirmish foundation is playable, but it is not being presented as a finished game. Story
mission objectives, campaign presentation, advanced AI personalities, construction, fog of war, matchmaking, and
authoritative online PvP are later milestones. The deterministic core already supports story, skirmish,
and PvP match modes so those features can share one ruleset.

## Unreal client

Requirements:

- Unreal Engine 5.8
- Visual Studio with Game development with C++, MSVC, Windows SDK, and Visual Studio Tools for Unreal

Open `unreal/AshenDominion/AshenDominion.uproject`. Let Unreal build the modules if prompted, then press
Play. A first launch can spend extra time compiling shaders. The match remains frozen on the deployment
screen; choose **Begin Skirmish** or press Enter/Space when ready. Opening workers automatically begin
harvesting so the first minute can be spent learning the camera and production flow.

Current controls:

- Left mouse: select; drag to box-select; Shift adds to the selection
- Right mouse: move, attack an enemy, or gather a resource based on the target
- WASD or screen edges: pan the war camera
- Mouse wheel: smooth zoom
- 1 and 2: train the primary or secondary unit from a selected producer
- Enter or Space: begin the skirmish from the deployment screen
- Escape: pause the simulation and return to the deployment screen

Build the editor target directly from PowerShell when `UE_ROOT` points to the Unreal installation:

```powershell
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
  AshenDominionEditor Win64 Development `
  "-Project=$PWD\unreal\AshenDominion\AshenDominion.uproject" `
  -WaitMutex -NoHotReload
```

Run the Unreal automation test:

```powershell
& "$env:UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\unreal\AshenDominion\AshenDominion.uproject" `
  -unattended -nop4 -nullrhi -nosplash `
  '-ExecCmds=Automation RunTests Ashen' `
  '-TestExit=Automation Test Queue Empty' -log
```

## Portable core

Requirements: a C++20 compiler and CMake 3.24 or newer.

```bash
cmake --preset dev
cmake --build --preset dev --config Debug
ctest --preset dev -C Debug
```

The generated `ashen_headless` executable advances a match without graphics and prints its final tick,
economy, result, and deterministic state hash.

## Web reference

```bash
npm ci
npm run dev
```

Tests and production build:

```bash
npm test -- --run
npm run build
```

See [docs/cpp-migration.md](docs/cpp-migration.md) for ownership boundaries and the remaining migration
stages, and [docs/research-brief.md](docs/research-brief.md) for the genre and market findings behind the
design direction.
