# Black-Iron Ford strategic layout

This document records the design logic for Vowfall's first production battlefield. The map is an
original dark-medieval space: its mood uses ruined feudal horror, hostile nature, and monumental
silhouettes without reproducing protected characters, locations, or iconography.

## Design goal

Black-Iron Ford must support a readable standard match and reward players who scout, rotate, or accept
economic risk. It must not force every player into the map's novelty. The direct lane remains the fastest
route between bases; the mountain and Gravewood flanks trade travel time and exposure for concealment,
resource value, and new attack angles.

The playable world is 4,800 by 2,800 Unreal units. Its simulation is rotationally balanced around the
center even though the northwest and southeast use different visual themes.

## Research translated into rules

- Blizzard's map process starts with gameplay goals, iterates before art lock, and rejects art that
  obscures play. Black-Iron Vale therefore shares one route specification across terrain clearing,
  roads, navigation tests, camera bounds, and the minimap. Source: [How Does Blizzard Make Maps?](https://news.blizzard.com/en-us/article/20984531/how-does-blizzard-make-maps)
- Golden Wall's postmortem recommends preserving a conventional way to play while offering experimental
  options. It also warns against inaccessible siege pockets and praises vulnerable, high-reward economy.
  The central ford is conventional; both outer economies are reachable and punishable. Source:
  [StarCraft II ladder map postmortem: Golden Wall](https://www.gamedeveloper.com/design/starcraft-2-ladder-map-post-mortem-golden-wall)
- Blizzard's mapmaking guidance calls out exploitable pockets, cramped resource backs, and doodads that
  interfere with pathing. Resource clearings and road corridors are kept free of procedural foliage, and
  all visual proxies have collision disabled. Source: [Mastering Mapmaking, Part Two](https://news.blizzard.com/en-gb/article/20521249/mastering-mapmaking-part-two)
- Current Warcraft III mapper feedback repeatedly asks for meaningful early route choices and secret
  passages with commitment, while criticizing maps whose unused half or two-route structure makes play
  one-dimensional. This is anecdotal feedback, not balance data, so it informs hypotheses tested in play
  rather than acting as proof. Source: [Warcraft III community map feedback](https://www.reddit.com/r/WC3/comments/1s430co/3_new_maps_released_for_testing_links_in_comments/)
- Adjacent competitive games show that concealment works only when its approach remains legible and
  defensible geometry does not become oppressive. Gravewood uses a dense canopy as a landmark but keeps
  the navigable road and cache clearings visible. Sources: [Wild Rift 7.0 terrain update](https://wildrift.leagueoflegends.com/en-us/news/game-updates/patch-7-0-aaa-aram-and-rift-updates/),
  [Riot's Dark Star level-design retrospective](https://nexus.leagueoflegends.com/en-us/2017/05/dev-on-dark-star-singularity/)

## Route structure

### Direct front

- Broad, straight, and fastest.
- Crosses the river at the central ritual ford.
- Carries the richest central iron field and the highest likelihood of an early contest.
- Wide enough for formation movement, flanking inside a fight, and retreat without a single-unit choke.

### Widow's Ascent

- Leaves the human side northwest of the main approach, climbs around the mountain's west face, and runs
  behind its crest before crossing the north bridge.
- Two 900-ore hidden mines reward early scouting and worker exposure.
- The massif blocks direct movement but creates no unreachable ledge from which ranged units can attack.
- The route costs more travel time than the center and rejoins readable approaches before either base.

### Gravewood turn

- Rotational counterpart to Widow's Ascent, using forest density, dead trunks, roots, and shrine caches
  instead of rock elevation.
- Two equivalent 900-ore caches preserve competitive value and travel timing.
- The canopy conceals intent at normal camera distance, while a reserved road corridor prevents unit and
  cursor noise.
- The south bridge remains contestable from both banks; the forest is not a private safe half-map.

## Competitive invariants

1. Every base has one broad standard approach and access to both rotations.
2. The two outer routes are rotationally equivalent in navigation obstacles, resource value, and crossing
   distance even though their art differs.
3. No reward sits in an unreachable pocket, and no flank opens directly behind a command structure.
4. River blockers leave exactly three broad portals: north bridge, central ford, and south bridge.
5. Procedural trees, grass, rocks, roots, roads, water, and terrain never own authoritative collision.
6. Hidden resources remain hidden through fog until scouted; concealment does not grant combat immunity.
7. Natural elevation and overscan terrain replace the old perimeter monolith ring.

## Validation

The native regression suite sends one scout through the mountain pass and north crossing while a second
uses Gravewood and the south crossing. It also verifies the center route independently. Unreal automation
checks deterministic boot, navigation and queued orders, the full competitive slice, required materials,
and separation between visual terrain and interaction collision.

Before this layout is art-locked, playtests must record opening contact time, first-resource exposure,
crossing usage, army travel time, retreat success, expansion safety, and camera readability. Heatmaps and
replays should decide whether a route is underused or oppressive; foliage quantity alone must not be used
to solve a topology problem.
