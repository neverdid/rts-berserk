# Vowfall beta roadmap

This roadmap orders work by dependency and risk. Milestones are completed one at a time; visual content
does not outrun the gameplay, networking, and validation systems that must support it.

## 0. Product and fiction baseline - complete

- Original Vowfall identity, three-faction world bible, campaign spine, visual language, and source-safe
  inspiration ledger
- Playable TypeScript interaction and balance reference
- Portable deterministic C++ core and playable Unreal foundation

## 1. Cross-runtime parity gate - complete

- Shared Vowfall faction identity and deterministic catalog
- Single-source scenarios executed by TypeScript and C++
- CI failures that name the exact drifting checkpoint
- Local native, web, Unreal, and parity validation

Exit gate: catalog and representative movement, economy, production, combat, supply, and victory
contracts pass locally and in GitHub Actions.

## 2. Complete the competitive gameplay vertical slice - complete

- Building placement and construction
- Research, faction powers, resolve, terror, wards, control points, and Dread Tide
- Fog of war and scouting information rules
- Full command card, production queues, rally behavior, and readable combat feedback
- Headless tests and parity fixtures for every authoritative rule

Exit gate: one complete 1v1 match can be played in Unreal from opening worker split to victory without a
debug-only action or missing core system.

Delivered with deterministic TypeScript/C++ parity fixtures, native and Unreal automation coverage,
build-order-aware enemy construction and tech, responsive 1280x720 and 800x600 command surfaces, delayed
render captures, and world-space feedback for construction, capture pressure, fog, resolve, and damage.

## 3. Build the production battlefield - in progress

Visual foundation delivered:

- Painterly-realism art target, faction shape grammar, and asset intake contract
- Sculpted overscan terrain, layered world materials, water, roads, bridges, forests, faction
  fortifications, ritual landmarks, lighting, fog, and capture automation
- Deterministic interaction collision preserved independently from visual terrain
- Unreal regression coverage for required world assets and collision behavior

Next graphics slice: replace each Engine proxy category with one coherent licensed or custom production
environment kit, then convert the approved composition to an authored Landscape and spline workflow.

- Replace procedural proxy terrain with an authored competitive map using landscape, roads, bridges,
  river water, forests, cliffs, castles, landmarks, and faction-readable bases
- Establish modular environment kits, materials, foliage rules, collision, navigation, occlusion, and
  level-of-detail budgets
- Validate fair spawn geometry, expansion timing, attack lanes, vision blockers, and camera readability
- Add a second map only after the first passes gameplay and performance review

Exit gate: the primary map is visually distinctive, competitively fair, performant, and readable at
normal RTS camera distances on the beta minimum specification.

## 4. Complete the three faction rosters

- Production-quality silhouettes, animation sets, buildings, upgrades, sound signatures, and effects
- Distinct macro mechanics, army compositions, counters, timing windows, and comeback tools
- Data-driven balance values shared by story, skirmish, replay, and PvP

Exit gate: each faction has at least two viable strategic openings, clear counterplay, and no placeholder
unit required for a standard match.

## 5. Raise AI to beta quality

- Scouting, information memory, build-order planning, expansion, harassment, retreat, focus fire, and
  terrain-aware engagements
- Distinct personalities and difficulty levels without hidden resource cheating at standard difficulty
- Deterministic regression scenarios and large headless simulation batches

Exit gate: AI completes legal full matches with all factions, recovers from disruption, and offers stable
difficulty without frame-rate-dependent decisions.

## 6. Deliver the campaign vertical slice

- One polished opening mission with briefing, dialogue, objectives, checkpoints, save/load, failure
  recovery, cinematics, and difficulty selection
- One short playable perspective from each faction to prove the multi-perspective narrative structure
- Mission scripting layered over the same authoritative rules used by PvP

Exit gate: campaign progress survives restart, objectives cannot soft-lock, and story presentation meets
the same performance and accessibility gates as skirmish.

## 7. Implement authoritative online PvP

- Dedicated server, tick-stamped command buffering, validation, snapshots, reconnect, replay, and desync
  diagnostics
- Lobby, party flow, map/faction selection, matchmaking, surrender, rematch, and observer foundations
- Network simulation tests for latency, jitter, packet loss, disconnects, and hostile commands

Exit gate: complete remote matches remain synchronized under the beta network profile and produce a
verifiable replay.

## 8. Finish player experience and presentation

- Vowfall main menu, settings, key rebinding, accessibility, onboarding, loading, pause, results, credits,
  and legal screens
- Final HUD hierarchy, minimap, alerts, selection feedback, command audio, music states, VFX, and camera
  polish
- Localization-ready text and subtitle pipeline

Exit gate: all primary workflows work with mouse and keyboard at supported resolutions without clipped
text, placeholder copy, missing focus states, or inaccessible critical information.

## 9. Closed alpha, balance, and performance

- Crash reporting, performance captures, replay-based bug reproduction, balance telemetry, and privacy
  controls
- External playtests across new, campaign-focused, and competitive RTS players
- CPU, GPU, memory, shader, loading, networking, and server-cost budgets

Exit gate: no known progression blocker, desync, data-loss defect, or repeatable crash; minimum-spec frame
time and server tick budgets hold in worst-case battles.

## 10. Beta release gate

- Signed packaged builds, patching, version compatibility, backend environments, moderation/reporting,
  support runbooks, and rollback procedure
- Store assets, system requirements, known issues, privacy terms, credits, and feedback channels
- Staged invite waves with measurable stability and retention thresholds

Exit gate: the release candidate passes the full acceptance matrix and can be updated or rolled back
without losing player progression or invalidating compatible replays.
