import { RACE_DEFS, STARTING_ORE, getEntityDef } from '../src/game/catalog'
import {
  activateRacePower,
  createEntity,
  createInitialState,
  getPlayerSupply,
  issueAttack,
  issueAttackMove,
  issueGather,
  issueMove,
  isEntityVisibleTo,
  setRallyPoint,
  startBuilding,
  startProduction,
  startResearch,
  updateSimulation,
} from '../src/game/simulation'
import type {
  BuildingType,
  EntityType,
  GameState,
  PlayerId,
  RaceId,
  ResearchId,
  UnitType,
  Vec2,
} from '../src/game/types'

const TICKS_PER_SECOND = 20
const ENTITY_TYPES: EntityType[] = ['worker', 'vanguard', 'skirmisher', 'command', 'barracks', 'turret']
const FACTIONS: RaceId[] = ['compact', 'ascendancy', 'concord']

interface ScenarioEntity {
  alias: string
  owner: PlayerId
  type: EntityType
  position: Vec2
}

interface ScenarioResource {
  alias: string
  position: Vec2
  amount: number
  radius: number
}

interface ScenarioControlPoint {
  alias: string
  position: Vec2
  radius: number
}

type ScenarioAction =
  | { type: 'move'; player: PlayerId; entities: string[]; target: Vec2 }
  | { type: 'attack'; player: PlayerId; entities: string[]; target: string }
  | { type: 'attack-move'; player: PlayerId; entities: string[]; target: Vec2 }
  | { type: 'gather'; player: PlayerId; entities: string[]; resource: string }
  | { type: 'train'; player: PlayerId; producer: string; unit: UnitType }
  | { type: 'rally'; player: PlayerId; producer: string; target: Vec2 }
  | {
      type: 'build'
      player: PlayerId
      worker: string
      building: BuildingType
      target: Vec2
      alias: string
    }
  | { type: 'research'; player: PlayerId; producer: string; research: ResearchId }
  | { type: 'power'; player: PlayerId }
  | { type: 'run'; ticks: number }

export interface ParityCheckpoint {
  path: string
  tolerance?: number
}

export interface ParityScenario {
  name: string
  factions: Record<PlayerId, RaceId>
  entities: ScenarioEntity[]
  resources: ScenarioResource[]
  controls?: ScenarioControlPoint[]
  actions: ScenarioAction[]
  checkpoints: ParityCheckpoint[]
}

interface EntitySnapshot {
  alive: boolean
  owner?: PlayerId
  type?: EntityType
  xMilli?: number
  yMilli?: number
  hpMilli?: number
  order?: string
  carrying?: number
  resolve?: number
  underConstruction?: boolean
  constructionProgressBasis?: number
  queueCount?: number
  visibleToOne?: boolean
}

export interface ParitySnapshot {
  tick: number
  status: GameState['status']
  winner: PlayerId | null
  results: boolean[]
  players: Record<
    string,
    {
      faction: RaceId
      ore: number
      supplyUsed: number
      supplyCap: number
      resolve: number
      powerCooldownTicks: number
      techTier: number
      researched: ResearchId[]
    }
  >
  entities: Record<string, EntitySnapshot>
  counts: Record<string, Record<EntityType, number>>
  resources: Record<string, { amount: number } | null>
  controls: Record<string, { owner: PlayerId | null; influence: number } | null>
  ruinTide: number
}

export interface CatalogSnapshot {
  faction: RaceId
  factionName: string
  incomeBasisPoints: number
  resolveDrift: number
  type: EntityType
  kind: 'unit' | 'building'
  label: string
  cost: number
  buildTicks: number
  hitPoints: number
  radiusMilli: number
  speedMilliPerTick: number
  rangeMilli: number
  damage: number
  cooldownTicks: number
  sightMilli: number
  terror: number
  ward: number
  armor: string
  bonusAgainst: string | null
  bonusDamage: number
  supplyCost: number
  supplyProvided: number
}

const sharedCheckpoints: ParityCheckpoint[] = [
  { path: 'results' },
  { path: 'status' },
  { path: 'winner' },
  { path: 'players.1.faction' },
  { path: 'players.2.faction' },
]

export const PARITY_SCENARIOS: ParityScenario[] = [
  {
    name: 'formation movement reaches its tactical destination',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 120, y: 120 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
      { alias: 'scout', owner: 1, type: 'worker', position: { x: 220, y: 220 } },
    ],
    resources: [],
    actions: [
      { type: 'move', player: 1, entities: ['scout'], target: { x: 360, y: 220 } },
      { type: 'run', ticks: 60 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'tick' },
      { path: 'entities.scout.alive' },
      { path: 'entities.scout.order' },
      { path: 'entities.scout.xMilli', tolerance: 6_000 },
      { path: 'entities.scout.yMilli', tolerance: 1_000 },
      { path: 'players.1.supplyUsed' },
      { path: 'players.1.supplyCap' },
      { path: 'counts.1.worker' },
    ],
  },
  {
    name: 'worker completes one black-iron delivery',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 120, y: 180 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
      { alias: 'worker', owner: 1, type: 'worker', position: { x: 220, y: 180 } },
    ],
    resources: [{ alias: 'seam', position: { x: 300, y: 180 }, amount: 100, radius: 24 }],
    actions: [
      { type: 'gather', player: 1, entities: ['worker'], resource: 'seam' },
      { type: 'run', ticks: 90 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'tick' },
      { path: 'players.1.ore' },
      { path: 'resources.seam.amount' },
      { path: 'entities.worker.alive' },
      { path: 'counts.1.worker' },
    ],
  },
  {
    name: 'production spends ore and respects army capacity',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 120, y: 120 } },
      { alias: 'assembly_hall', owner: 1, type: 'barracks', position: { x: 260, y: 180 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
    ],
    resources: [],
    actions: [
      { type: 'train', player: 1, producer: 'assembly_hall', unit: 'vanguard' },
      { type: 'run', ticks: 180 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'tick' },
      { path: 'players.1.ore' },
      { path: 'players.1.supplyUsed' },
      { path: 'players.1.supplyCap' },
      { path: 'counts.1.vanguard' },
      { path: 'counts.1.barracks' },
    ],
  },
  {
    name: 'focused melee removes the authored target',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 120, y: 120 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
      { alias: 'attacker', owner: 1, type: 'vanguard', position: { x: 360, y: 300 } },
      { alias: 'target', owner: 2, type: 'worker', position: { x: 410, y: 300 } },
    ],
    resources: [],
    actions: [
      { type: 'attack', player: 1, entities: ['attacker'], target: 'target' },
      { type: 'run', ticks: 200 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'entities.attacker.alive' },
      { path: 'entities.target.alive' },
      { path: 'counts.2.worker' },
      { path: 'counts.2.command' },
    ],
  },
  {
    name: 'destroying the command structure resolves victory',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 400, y: 300 } },
      { alias: 'attacker', owner: 1, type: 'vanguard', position: { x: 470, y: 300 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 540, y: 300 } },
    ],
    resources: [],
    actions: [
      { type: 'attack', player: 1, entities: ['attacker'], target: 'quiet_house' },
      { type: 'run', ticks: 2_000 },
    ],
    checkpoints: [
      { path: 'results' },
      { path: 'status' },
      { path: 'winner' },
      { path: 'entities.compact_keep.alive' },
      { path: 'entities.quiet_house.alive' },
      { path: 'counts.1.command' },
      { path: 'counts.2.command' },
    ],
  },
  {
    name: 'worker construction completes a supply structure',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 100, y: 100 } },
      { alias: 'builder', owner: 1, type: 'worker', position: { x: 155, y: 100 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
    ],
    resources: [],
    actions: [
      {
        type: 'build',
        player: 1,
        worker: 'builder',
        building: 'barracks',
        target: { x: 310, y: 180 },
        alias: 'assembly_hall',
      },
      { type: 'run', ticks: 380 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'players.1.ore' },
      { path: 'players.1.supplyCap' },
      { path: 'entities.assembly_hall.alive' },
      { path: 'entities.assembly_hall.underConstruction' },
      { path: 'entities.assembly_hall.constructionProgressBasis' },
      { path: 'counts.1.barracks' },
    ],
  },
  {
    name: 'research unlocks ranged production',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 100, y: 100 } },
      { alias: 'assembly_hall', owner: 1, type: 'barracks', position: { x: 230, y: 100 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
    ],
    resources: [],
    actions: [
      { type: 'research', player: 1, producer: 'compact_keep', research: 'tier-two' },
      { type: 'run', ticks: 321 },
      { type: 'train', player: 1, producer: 'assembly_hall', unit: 'skirmisher' },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'players.1.ore' },
      { path: 'players.1.techTier' },
      { path: 'players.1.researched' },
      { path: 'entities.assembly_hall.queueCount' },
    ],
  },
  {
    name: 'ascendancy doctrine manifests a capped combat body',
    factions: { 1: 'ascendancy', 2: 'compact' },
    entities: [
      { alias: 'quiet_house', owner: 1, type: 'command', position: { x: 120, y: 120 } },
      { alias: 'compact_keep', owner: 2, type: 'command', position: { x: 1050, y: 680 } },
    ],
    resources: [],
    actions: [{ type: 'power', player: 1 }],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'players.1.ore' },
      { path: 'players.1.powerCooldownTicks' },
      { path: 'players.1.supplyUsed' },
      { path: 'counts.1.vanguard' },
    ],
  },
  {
    name: 'relic capture and dread resolve share fixed outcomes',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 100, y: 100 } },
      { alias: 'line', owner: 1, type: 'vanguard', position: { x: 300, y: 300 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 430, y: 300 } },
    ],
    resources: [],
    controls: [{ alias: 'ford_relic', position: { x: 300, y: 300 }, radius: 90 }],
    actions: [{ type: 'run', ticks: 150 }],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'controls.ford_relic.owner' },
      { path: 'controls.ford_relic.influence' },
      { path: 'players.1.resolve', tolerance: 3 },
      { path: 'entities.line.resolve', tolerance: 3 },
      { path: 'ruinTide', tolerance: 3 },
    ],
  },
  {
    name: 'scouting reveals an enemy through authoritative sight',
    factions: { 1: 'compact', 2: 'ascendancy' },
    entities: [
      { alias: 'compact_keep', owner: 1, type: 'command', position: { x: 100, y: 100 } },
      { alias: 'scout', owner: 1, type: 'worker', position: { x: 150, y: 300 } },
      { alias: 'quiet_house', owner: 2, type: 'command', position: { x: 1050, y: 300 } },
    ],
    resources: [],
    actions: [
      { type: 'move', player: 1, entities: ['scout'], target: { x: 850, y: 300 } },
      { type: 'run', ticks: 200 },
    ],
    checkpoints: [
      ...sharedCheckpoints,
      { path: 'entities.quiet_house.visibleToOne' },
    ],
  },
]

export function serializeScenario(scenario: ParityScenario): string {
  const lines = ['version 1', `config ${scenario.factions[1]} ${scenario.factions[2]}`]
  scenario.entities.forEach((entity) => {
    lines.push(`entity ${entity.alias} ${entity.owner} ${entity.type} ${entity.position.x} ${entity.position.y}`)
  })
  scenario.resources.forEach((resource) => {
    lines.push(
      `resource ${resource.alias} ${resource.position.x} ${resource.position.y} ${resource.amount} ${resource.radius}`,
    )
  })
  scenario.controls?.forEach((control) => {
    lines.push(`control ${control.alias} ${control.position.x} ${control.position.y} ${control.radius}`)
  })
  scenario.actions.forEach((action) => {
    switch (action.type) {
      case 'move':
      case 'attack-move':
        lines.push(
          `${action.type} ${action.player} ${action.entities.join(',')} ${action.target.x} ${action.target.y}`,
        )
        break
      case 'attack':
        lines.push(`attack ${action.player} ${action.entities.join(',')} ${action.target}`)
        break
      case 'gather':
        lines.push(`gather ${action.player} ${action.entities.join(',')} ${action.resource}`)
        break
      case 'train':
        lines.push(`train ${action.player} ${action.producer} ${action.unit}`)
        break
      case 'rally':
        lines.push(`rally ${action.player} ${action.producer} ${action.target.x} ${action.target.y}`)
        break
      case 'build':
        lines.push(
          `build ${action.player} ${action.worker} ${action.building} ${action.target.x} ${action.target.y} ${action.alias}`,
        )
        break
      case 'research':
        lines.push(`research ${action.player} ${action.producer} ${action.research}`)
        break
      case 'power':
        lines.push(`power ${action.player}`)
        break
      case 'run':
        lines.push(`run ${action.ticks}`)
        break
    }
  })
  return `${lines.join('\n')}\n`
}

export function executeTypeScriptScenario(scenario: ParityScenario): ParitySnapshot {
  const state = createParityState(scenario.factions)
  const entityIds = new Map<string, string>()
  scenario.entities.forEach((entry) => {
    const entity = createEntity(state, entry.type, entry.owner, entry.position)
    entityIds.set(entry.alias, entity.id)
  })
  scenario.resources.forEach((entry) => {
    state.resources.push({
      id: entry.alias,
      position: { ...entry.position },
      radius: entry.radius,
      amount: entry.amount,
      maxAmount: entry.amount,
    })
  })
  scenario.controls?.forEach((entry) => {
    state.controlPoints.push({
      id: entry.alias,
      position: { ...entry.position },
      radius: entry.radius,
      owner: null,
      influence: 0,
    })
  })

  const results: boolean[] = []
  const ids = (aliases: string[]): string[] => aliases.map((alias) => requiredAlias(entityIds, alias))
  scenario.actions.forEach((action) => {
    switch (action.type) {
      case 'move':
        results.push(issueMove(state, ids(action.entities), action.target, action.player).ok)
        break
      case 'attack':
        results.push(issueAttack(state, ids(action.entities), requiredAlias(entityIds, action.target), action.player).ok)
        break
      case 'attack-move':
        results.push(issueAttackMove(state, ids(action.entities), action.target, action.player).ok)
        break
      case 'gather':
        results.push(issueGather(state, ids(action.entities), action.resource, action.player).ok)
        break
      case 'train':
        results.push(startProduction(state, requiredAlias(entityIds, action.producer), action.unit, action.player).ok)
        break
      case 'rally':
        results.push(setRallyPoint(state, [requiredAlias(entityIds, action.producer)], action.target, action.player).ok)
        break
      case 'build': {
        const result = startBuilding(
          state,
          requiredAlias(entityIds, action.worker),
          action.building,
          action.target,
          action.player,
        )
        results.push(result.ok)
        if (result.ok) {
          const site = state.entities.at(-1)
          if (!site || site.type !== action.building) {
            throw new Error(`A successful build action did not create ${action.building}.`)
          }
          entityIds.set(action.alias, site.id)
        }
        break
      }
      case 'research':
        results.push(
          startResearch(
            state,
            requiredAlias(entityIds, action.producer),
            action.research,
            action.player,
          ).ok,
        )
        break
      case 'power':
        results.push(activateRacePower(state, action.player).ok)
        break
      case 'run':
        for (let tick = 0; tick < action.ticks; tick += 1) {
          updateSimulation(state, 1 / TICKS_PER_SECOND)
        }
        break
    }
  })

  return snapshotState(state, scenario, entityIds, results)
}

export function typeScriptCatalog(): CatalogSnapshot[] {
  return FACTIONS.flatMap((faction) => {
    const race = RACE_DEFS[faction]
    return ENTITY_TYPES.map((type) => {
      const definition = getEntityDef(type, faction)
      const effectiveRadius = definition.kind === 'unit' ? Math.round(definition.radius * race.unitScale) : definition.radius
      return {
        faction,
        factionName: race.name,
        incomeBasisPoints: Math.round(race.incomeRate * 10_000),
        resolveDrift: race.resolveDrift,
        type,
        kind: definition.kind,
        label: definition.label,
        cost: definition.cost,
        buildTicks: Math.round(definition.buildTime * TICKS_PER_SECOND),
        hitPoints: definition.hp,
        radiusMilli: effectiveRadius * 1_000,
        speedMilliPerTick: Math.round((definition.speed / TICKS_PER_SECOND) * 1_000),
        rangeMilli: definition.range * 1_000,
        damage: definition.damage,
        cooldownTicks: Math.round(definition.attackCooldown * TICKS_PER_SECOND),
        sightMilli: definition.sight * 1_000,
        terror: definition.terror,
        ward: definition.ward,
        armor: definition.armor,
        bonusAgainst: definition.bonusAgainst[0] ?? null,
        bonusDamage: definition.bonusDamage,
        supplyCost: definition.supplyCost,
        supplyProvided: definition.supplyProvided,
      }
    })
  })
}

function createParityState(factions: Record<PlayerId, RaceId>): GameState {
  const state = createInitialState('pvp', factions[1], factions[2])
  state.tick = 0
  state.time = 0
  state.mapSize = { x: 1_200, y: 800 }
  state.entities = []
  state.resources = []
  state.projectiles = []
  state.controlPoints = []
  state.events = []
  state.status = 'playing'
  state.winner = null
  state.ruinTide = 0
  state.tideCrestAnnounced = false
  state.storyStep = 0
  state.aiWaveTimer = Number.POSITIVE_INFINITY
  state.aiDecisionTimer = Number.POSITIVE_INFINITY
  state.nextEventId = 1
  for (const player of [1, 2] as PlayerId[]) {
    state.players[player].ore = STARTING_ORE
    state.players[player].resolve = 100
    state.players[player].powerCooldown = 0
    state.players[player].techTier = 1
    state.players[player].researched = []
    state.players[player].researchQueue = []
    state.stats[player] = {
      oreMined: 0,
      unitsCreated: 0,
      unitsLost: 0,
      structuresLost: 0,
      damageDealt: 0,
      objectivesCaptured: 0,
    }
  }
  return state
}

function snapshotState(
  state: GameState,
  scenario: ParityScenario,
  entityIds: Map<string, string>,
  results: boolean[],
): ParitySnapshot {
  const entities: Record<string, EntitySnapshot> = {}
  const trackedAliases = [
    ...scenario.entities.map((entry) => entry.alias),
    ...scenario.actions.filter((action) => action.type === 'build').map((action) => action.alias),
  ]
  trackedAliases.forEach((alias) => {
    const id = requiredAlias(entityIds, alias)
    const entity = state.entities.find((candidate) => candidate.id === id)
    entities[alias] = entity
      ? {
          alive: true,
          owner: entity.owner,
          type: entity.type,
          xMilli: Math.round(entity.position.x * 1_000),
          yMilli: Math.round(entity.position.y * 1_000),
          hpMilli: Math.round(entity.hp * 1_000),
          order: entity.order.type,
          carrying: entity.carrying,
          resolve: entity.resolve,
          underConstruction: entity.underConstruction,
          constructionProgressBasis: entity.underConstruction
            ? Math.round(entity.constructionProgress * 10_000)
            : 10_000,
          queueCount: entity.queue.length,
          visibleToOne: isEntityVisibleTo(state, entity, 1),
        }
      : { alive: false }
  })

  const counts = Object.fromEntries(
    ([1, 2] as PlayerId[]).map((player) => [
      String(player),
      Object.fromEntries(
        ENTITY_TYPES.map((type) => [
          type,
          state.entities.filter((entity) => entity.owner === player && entity.type === type && entity.hp > 0).length,
        ]),
      ) as Record<EntityType, number>,
    ]),
  ) as Record<string, Record<EntityType, number>>

  const players = Object.fromEntries(
    ([1, 2] as PlayerId[]).map((player) => {
      const supply = getPlayerSupply(state, player)
      return [
        String(player),
        {
          faction: state.players[player].race,
          ore: state.players[player].ore,
          supplyUsed: supply.used,
          supplyCap: supply.cap,
          resolve: state.players[player].resolve,
          powerCooldownTicks: Math.round(state.players[player].powerCooldown * TICKS_PER_SECOND),
          techTier: state.players[player].techTier,
          researched: [...state.players[player].researched],
        },
      ]
    }),
  ) as ParitySnapshot['players']

  const resources = Object.fromEntries(
    scenario.resources.map((entry) => {
      const resource = state.resources.find((candidate) => candidate.id === entry.alias)
      return [entry.alias, resource ? { amount: resource.amount } : null]
    }),
  )

  const controls = Object.fromEntries(
    (scenario.controls ?? []).map((entry) => {
      const point = state.controlPoints.find((candidate) => candidate.id === entry.alias)
      return [
        entry.alias,
        point ? { owner: point.owner, influence: Math.round(point.influence * 100) } : null,
      ]
    }),
  )

  return {
    tick: state.tick,
    status: state.status,
    winner: state.winner,
    results,
    players,
    entities,
    counts,
    resources,
    controls,
    ruinTide: state.ruinTide,
  }
}

function requiredAlias(aliases: Map<string, string>, alias: string): string {
  const value = aliases.get(alias)
  if (!value) {
    throw new Error(`Unknown parity entity alias: ${alias}`)
  }
  return value
}
