# Vowfall AI Architecture

## Direction

Vowfall's production game is the Unreal client backed by the deterministic C++20 `AshenCore`.
The archived TypeScript prototype is not an AI runtime, parity target, or implementation destination.

AI is three cooperating systems, never one update function:

1. **Commander AI** chooses economy, technology, production, scouting, objectives, and attack timing.
2. **Combat AI** executes tactical positioning and unit micro after the commander commits forces.
3. **Ambient AI** drives wildlife, horrors, civilians, and scenario actors for mood and world response.

The first two systems may share observations and influence data, but they have separate schedules,
budgets, tests, and tuning surfaces. Ambient behavior cannot issue player commands or inspect hidden
player state.

## Non-negotiable rules

- AI runs in `AshenCore`, not in an Unreal-only player controller or subsystem.
- AI receives the same authoritative observation a human player can receive.
- Hidden entities cannot be targeted, pursued, counted, counter-built against, or used in utility scores.
- Known map geometry and previously explored terrain are valid knowledge; live hidden state is not.
- Difficulty comes from bounded reaction time, planning depth, risk tolerance, and execution quality.
- No difficulty grants free resources, impossible actions, hidden vision, or zero-latency reactions.
- Decisions are deterministic for a match seed and command stream.
- Designer-authored utility scoring, behavior trees/state machines, and influence maps come before ML.

## Ordered delivery

### Step 0: Authoritative fog of war

Status: implemented in `AshenCore`.

- Per-player cells have `Hidden`, `Explored`, or `Visible` state.
- Current visibility is rebuilt from living, completed friendly observers after each fixed simulation tick.
- Explored memory persists when current vision leaves an area.
- Direct attacks reject unseen targets with a generic invalid-target response.
- Existing attack orders stop when all contact is lost.
- Attack-move, patrol, hold position, idle aggression, and defenses acquire visible targets only.
- Deterministic hashes include visibility configuration and cell state.
- Unreal hides unseen enemies, discovers resources through fog, remembers last-observed relic ownership,
  and renders hidden versus explored terrain on the tactical map.

Exit gate:

- An enemy outside vision cannot be targeted or reacted to, even when its entity ID is known.

### Step 1: Core-owned AI controller

Status: implemented in `AshenCore`.

- `PlayerObservation` is a value snapshot containing owned state, sanitized visible enemies, explored map
  cells, remembered sightings, discovered resources, last-observed objective state, and legal capabilities.
- Visible enemy snapshots omit production queues, orders, routes, rally points, cooldowns, and other
  information a player cannot observe.
- `CommanderAI` includes no `Simulation` dependency. Its only input is a const observation and its only
  output is an ordinary `Command` value.
- `Simulation` owns both-player controller scheduling. It takes both observations before queuing commands
  for the next fixed tick, preventing immediate mutation and host-specific first reactions.
- The Unreal subsystem only enables the Player Two commander; it contains no opponent decision logic.
- The native headless runner enables both commanders and finishes a complete deterministic match without
  Unreal or rendering.

Exit gate:

- Passed: two bots finish a headless match, duplicate matches produce identical hashes, and perturbing a
  hidden enemy order changes raw simulation state without changing the observer hash or commander decision.

### Step 2: Self-play and measurement

Status: implemented in the native C++ benchmark layer.

- Six ordered full-match scenarios cover every non-mirror faction pairing with every faction on both spawns.
- Nine targeted fixtures per seed exercise economy-deficit recovery, blocked-opening recovery, and early-rush
  survival for every faction. Each fixture exposes named checks and final diagnostic state in the report.
- Match seeds are part of simulation state and the sanitized observation. They select deterministic commander
  variants without exposing hidden state or granting resources.
- Every applied command records source, issue and application ticks, the exact observation hash, complete
  payload, acceptance, and validation error. External and commander commands share the same trace contract.
- Headless reports record faction and spawn win rates, opening and technology timings, fifth-worker and
  first-reinforcement timings, legal idle producer-ticks, first contact and ore at contact, peak and final army
  value, command rate and rejection reasons, retreat survival, duration, checkpoints, and final hashes.
- The fixed CI suite runs each match twice. Nondeterminism, timeout, missing contact, broken openings, missing
  observation provenance, empty traces, and excessive rejected commands are correctness failures.
- Faction or spawn win-rate skew creates a non-blocking balance alert. Tuning evidence remains visible without
  weakening deterministic and behavioral gates.
- Windows and Linux upload the stable JSON report, and CI requires the files to be byte-identical.

Exit gate:

- Passed: the same seed and build produce the same telemetry, command trace, checkpoints, outcome, and state
  hash; the default two-seed suite completes 12 full matches and 18 targeted fixtures with all 78 behavior
  checks passing.

### Step 3: Strategic, tactical, and micro layers

- Strategic layer: utility-scored economy, supply, technology, production, expansion, and army composition.
- Tactical layer: scouting, objective selection, force allocation, engagement, reinforcement, and retreat.
- Micro layer: focus fire, spacing, formation maintenance, kiting, screening, ability timing, and path recovery.
- Each layer runs at its own human-plausible cadence and exposes a decision trace for debugging.

Exit gate:

- Every chosen action can report its candidate scores, winning reason, observation tick, and command result.

### Step 4: Influence maps

- Build deterministic grids for friendly power, observed enemy power, static danger, objective value,
  travel cost, terror pressure, and uncertainty.
- Unseen mobile enemies contribute only decaying last-known influence, never live positions.
- Tactical movement uses influence plus navigation, not influence as a replacement for pathfinding.

Exit gate:

- Fixed tactical scenarios verify flank choice, danger avoidance, reinforcement, and retreat destinations.

### Step 5: Faction, resolve, and dread behavior

- Each faction has distinct utility weights, acceptable losses, formation preferences, scouting doctrine,
  terror use, and retreat thresholds.
- Resolve and dread alter commitment, cohesion, target choice, rally behavior, and exploitation decisions.
- Personality changes weights and tolerances without bypassing faction identity or game rules.

Exit gate:

- Blind scenario classification can distinguish the three factions by decisions, not labels or bonuses.

### Step 6: Honest difficulty

- Difficulty profiles define observation delay, decision cadence, command precision, planning horizon,
  mistake rate, remembered-information decay, and utility search breadth.
- Reaction latency is applied to newly observed facts, not to the simulation itself.
- Competitive difficulty remains bounded by actions a skilled human could perform.

Exit gate:

- Difficulty audits prove equal resources and vision while showing measurable decision-quality differences.

### Step 7: Optional narrow ML

ML is permitted only after deterministic self-play data exists. Suitable uses include offline tuning,
build-order evaluation, opponent-style classification, or training-data analysis. Runtime command legality,
visibility, combat execution, and the deterministic simulation never depend on an opaque model.

## Required test families

- **Visibility contracts:** hidden target rejection, reveal, loss of contact, explored memory, observer symmetry.
- **Command legality:** AI and human commands pass through the same validation path.
- **Determinism:** duplicate simulations, queued commands, self-play traces, and final hashes match.
- **Tactical fixtures:** engage, disengage, flank, defend, reinforce, focus fire, kite, and blocked-path recovery.
- **Strategic fixtures:** worker saturation, supply prevention, tech timing, counter-composition, and expansion risk.
- **Information audits:** perturbing hidden enemy state cannot change an AI decision until that state is observed.
- **Faction fixtures:** identical observations produce intentionally different, documented faction choices.

## Debugging contract

Every AI decision record must contain the simulation tick, observer player, observation revision, layer,
candidate actions, utility components, selected action, rejected command reason, and relevant influence cells.
Shipping builds may discard these records, but tests and development builds must be able to reproduce them.
