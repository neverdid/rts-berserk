import {
  HARVEST_SECONDS,
  ORE_PER_TRIP,
  RACE_DEFS,
  RACE_POWER_DEFS,
  RUIN_TIDE_PERIOD,
  STARTING_ORE,
  TERROR_RANGE,
  TRAINABLE_BY_BUILDING,
  WARD_RANGE,
  getEntityDef,
} from './catalog'
import { getMapDef, hasTerrainKind, isTerrainBlocked } from './map'
import { findNavigationPath } from './navigation'
import { getStoryMission } from './story'
import { RESEARCH_DEFS, researchForRace } from './technology'
import type {
  AiPersonality,
  BuildingType,
  CommandResult,
  Entity,
  EntityType,
  GameEvent,
  GameMode,
  GameSetup,
  GameState,
  MatchStats,
  MatchRuleId,
  PlayerId,
  RaceId,
  ResearchId,
  ResourceNode,
  UnitStance,
  UnitType,
  Vec2,
} from './types'

const PLAYER_ONE: PlayerId = 1
const PLAYER_TWO: PlayerId = 2

export function distance(a: Vec2, b: Vec2): number {
  return Math.hypot(a.x - b.x, a.y - b.y)
}

export function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value))
}

function emptyStats(): MatchStats {
  return {
    oreMined: 0,
    unitsCreated: 0,
    unitsLost: 0,
    structuresLost: 0,
    damageDealt: 0,
    objectivesCaptured: 0,
  }
}

function firstWaveDelay(personality: AiPersonality): number {
  if (personality === 'aggressive') {
    return 24
  }
  if (personality === 'economic') {
    return 38
  }
  return 44
}

export function createInitialState(
  mode: GameMode,
  playerRace: RaceId = 'compact',
  enemyRace: RaceId = 'ascendancy',
  setup: Partial<GameSetup> = {},
): GameState {
  const missionId = setup.missionId ?? 'bridge-of-names'
  const mission = getStoryMission(missionId)
  const resolvedPlayerRace = mode === 'story' ? mission.playerRace : playerRace
  const resolvedEnemyRace = mode === 'story' ? mission.opponentRace : enemyRace
  const playerOneRace = RACE_DEFS[resolvedPlayerRace]
  const playerTwoRace = RACE_DEFS[resolvedEnemyRace]
  const mapId = mode === 'story' ? mission.mapId : (setup.mapId ?? 'black-iron-ford')
  const map = getMapDef(mapId)
  const aiPersonality = setup.aiPersonality ?? 'aggressive'
  const matchRule = setup.matchRule ?? 'standard'
  const state: GameState = {
    mode,
    mapId,
    missionId,
    aiPersonality,
    matchRule,
    tick: 0,
    time: 0,
    mapSize: { ...map.size },
    activePlayer: PLAYER_ONE,
    players: {
      1: {
        id: PLAYER_ONE,
        race: resolvedPlayerRace,
        name: playerOneRace.name,
        ore: STARTING_ORE,
        resolve: 100,
        color: playerOneRace.color,
        accent: playerOneRace.accent,
        active: true,
        powerCooldown: 0,
        techTier: 1,
        researched: [],
        researchQueue: [],
      },
      2: {
        id: PLAYER_TWO,
        race: resolvedEnemyRace,
        name: playerTwoRace.name,
        ore: mode === 'story' ? 180 : STARTING_ORE,
        resolve: 100,
        color: playerTwoRace.color,
        accent: playerTwoRace.accent,
        active: mode === 'pvp',
        powerCooldown: 0,
        techTier: 1,
        researched: [],
        researchQueue: [],
      },
    },
    entities: [],
    resources: createResources(mapId, matchRule),
    projectiles: [],
    controlPoints: map.controlPoints.map((point) => ({ ...point, position: { ...point.position }, owner: null, influence: 0 })),
    stats: {
      1: emptyStats(),
      2: emptyStats(),
    },
    events: [],
    status: 'playing',
    winner: null,
    ruinTide: 0,
    tideCrestAnnounced: false,
    storyStep: 0,
    aiWaveTimer:
      mode === 'pvp'
        ? Number.POSITIVE_INFINITY
        : mode === 'story' && missionId === 'bridge-of-names'
          ? 32
          : firstWaveDelay(aiPersonality),
    aiDecisionTimer: 0,
    nextEventId: 1,
  }

  addStartingBase(state, PLAYER_ONE, map.startingBases[PLAYER_ONE].origin, map.startingBases[PLAYER_ONE].direction)
  addStartingBase(state, PLAYER_TWO, map.startingBases[PLAYER_TWO].origin, map.startingBases[PLAYER_TWO].direction)
  assignOpeningEconomy(state, PLAYER_ONE)
  assignOpeningEconomy(state, PLAYER_TWO)

  addEvent(
    state,
    mode === 'story'
      ? `${mission.title} begun. ${mission.briefing}`
      : mode === 'skirmish'
        ? `Skirmish begun on ${map.name}. Scout, capture the relic, and break the rival keep.`
        : 'Local PvP sandbox started. Active commander can be switched with Tab.',
    'info',
  )

  return state
}

export function createEntity(
  state: GameState,
  type: EntityType,
  owner: PlayerId,
  position: Vec2,
  underConstruction = false,
): Entity {
  const race = RACE_DEFS[state.players[owner].race]
  const def = getEntityDef(type, race.id)
  const hp = underConstruction ? Math.max(25, Math.round(def.hp * 0.24)) : def.hp
  const unitScale = def.kind === 'unit' ? race.unitScale : 1
  const entity: Entity = {
    id: `${type}-${state.nextEventId}-${Math.round(position.x)}-${Math.round(position.y)}`,
    owner,
    kind: def.kind,
    type,
    label: def.label,
    position: { ...position },
    radius: Math.round(def.radius * unitScale),
    hp,
    maxHp: def.hp,
    speed: def.speed,
    range: def.range,
    damage: def.damage,
    attackCooldown: def.attackCooldown,
    cooldown: 0,
    sight: def.sight,
    armor: def.armor,
    bonusAgainst: def.bonusAgainst,
    bonusDamage: def.bonusDamage,
    terror: def.terror,
    ward: def.ward,
    resolve: 100,
    order: { type: 'idle' },
    carrying: 0,
    queue: [],
    rallyPoint: null,
    stance: 'aggressive',
    guardPosition: { ...position },
    navigationPath: [],
    navigationTarget: null,
    underConstruction,
    constructionProgress: underConstruction ? 0 : 1,
  }
  applyResearchBonusesToEntity(state, entity)
  state.nextEventId += 1
  state.entities.push(entity)
  if (state.time > 0 && entity.kind === 'unit') {
    state.stats[owner].unitsCreated += 1
  }
  return entity
}

export function addEvent(state: GameState, text: string, tone: GameEvent['tone'] = 'info'): void {
  state.events.unshift({
    id: state.nextEventId,
    time: state.time,
    text,
    tone,
  })
  state.nextEventId += 1
  state.events = state.events.slice(0, 8)
}

export function issueMove(state: GameState, ids: string[], target: Vec2, owner = state.activePlayer): CommandResult {
  const units = ids
    .map((id) => getEntity(state, id))
    .filter((entity): entity is Entity => entity !== undefined && entity.owner === owner && entity.kind === 'unit')

  if (units.length === 0) {
    return { ok: false, reason: 'No movable units selected.' }
  }

  const slots = formationSlots(target, units.length)
  units.forEach((unit, index) => {
    const slot = slots[index] ?? target
    unit.order = { type: 'move', target: slot }
    assignNavigation(state, unit, slot)
  })

  return { ok: true }
}

export function issueAttack(state: GameState, ids: string[], targetId: string, owner = state.activePlayer): CommandResult {
  const target = getEntity(state, targetId)
  if (!target || target.owner === owner) {
    return { ok: false, reason: 'No enemy target.' }
  }

  const attackers = ids
    .map((id) => getEntity(state, id))
    .filter((entity): entity is Entity => entity !== undefined && entity.owner === owner && entity.damage > 0)

  if (attackers.length === 0) {
    return { ok: false, reason: 'Selected units cannot attack.' }
  }

  attackers.forEach((entity) => {
    entity.order = { type: 'attack', targetId }
    assignNavigation(state, entity, target.position)
  })

  return { ok: true }
}

export function issueAttackMove(state: GameState, ids: string[], target: Vec2, owner = state.activePlayer): CommandResult {
  const attackers = ids
    .map((id) => getEntity(state, id))
    .filter(
      (entity): entity is Entity =>
        entity !== undefined && entity.owner === owner && entity.kind === 'unit' && entity.damage > 0,
    )

  if (attackers.length === 0) {
    return { ok: false, reason: 'Select combat units to attack-move.' }
  }

  const slots = formationSlots(target, attackers.length)
  attackers.forEach((entity, index) => {
    const slot = slots[index] ?? target
    entity.order = { type: 'attack-move', target: slot }
    assignNavigation(state, entity, slot)
  })
  return { ok: true }
}

export function setRallyPoint(
  state: GameState,
  buildingIds: string[],
  target: Vec2,
  owner = state.activePlayer,
): CommandResult {
  const buildings = buildingIds
    .map((id) => getEntity(state, id))
    .filter(
      (entity): entity is Entity =>
        entity !== undefined && entity.owner === owner && entity.kind === 'building' && !entity.underConstruction,
    )

  if (buildings.length === 0) {
    return { ok: false, reason: 'Select a completed production building.' }
  }

  buildings.forEach((building) => {
    building.rallyPoint = { ...target }
  })
  return { ok: true }
}

export function issueGather(state: GameState, ids: string[], resourceId: string, owner = state.activePlayer): CommandResult {
  const node = state.resources.find((resource) => resource.id === resourceId && resource.amount > 0)
  if (!node) {
    return { ok: false, reason: 'That black-iron seam is depleted.' }
  }

  const workers = ids
    .map((id) => getEntity(state, id))
    .filter((entity): entity is Entity => entity !== undefined && entity.owner === owner && entity.type === 'worker')

  if (workers.length === 0) {
    return { ok: false, reason: `Select ${getEntityDef('worker', state.players[owner].race).label}s to harvest black iron.` }
  }

  workers.forEach((worker) => {
    worker.order = { type: 'gather', resourceId, phase: 'to-resource', harvestTimer: 0 }
    assignNavigation(state, worker, node.position)
  })

  return { ok: true }
}

export function startProduction(
  state: GameState,
  buildingId: string,
  unitType: UnitType,
  owner = state.activePlayer,
): CommandResult {
  const building = getEntity(state, buildingId)
  if (!building || building.owner !== owner || building.kind !== 'building' || building.underConstruction) {
    return { ok: false, reason: 'Select a completed production building.' }
  }

  const allowed = TRAINABLE_BY_BUILDING[building.type as BuildingType] ?? []
  if (!allowed.includes(unitType)) {
    return { ok: false, reason: `${building.label} cannot train that unit.` }
  }

  if (building.queue.length >= 5) {
    return { ok: false, reason: 'Production queue is full.' }
  }

  const player = state.players[owner]
  const def = getEntityDef(unitType, player.race)
  if (unitType === 'skirmisher' && !player.researched.includes('tier-two')) {
    return { ok: false, reason: 'Reach the Black-Iron Age before training ranged troops.' }
  }
  const profile = productionProfile(state, owner, unitType)
  if (player.ore < profile.cost) {
    return { ok: false, reason: 'Not enough ore.' }
  }

  const supply = getPlayerSupply(state, owner)
  if (supply.used + supply.queued + def.supplyCost > supply.cap) {
    return { ok: false, reason: 'Army capacity reached. Complete another production hall.' }
  }

  player.ore -= profile.cost
  building.queue.push({ type: unitType, remaining: profile.time, total: profile.time })
  return { ok: true }
}

export function startBuilding(
  state: GameState,
  workerId: string,
  buildingType: BuildingType,
  position: Vec2,
  owner = state.activePlayer,
): CommandResult {
  const worker = getEntity(state, workerId)
  if (!worker || worker.owner !== owner || worker.type !== 'worker') {
    return { ok: false, reason: `Select one ${getEntityDef('worker', state.players[owner].race).label} to build.` }
  }

  const player = state.players[owner]
  const def = getEntityDef(buildingType, player.race)
  if (player.ore < def.cost) {
    return { ok: false, reason: 'Not enough ore.' }
  }

  if (!canPlaceBuilding(state, position, def.radius)) {
    return { ok: false, reason: 'Placement blocked.' }
  }

  player.ore -= def.cost
  const site = createEntity(state, buildingType, owner, position, true)
  worker.order = { type: 'build', buildingType, target: { ...position }, progress: 0, buildingId: site.id }
  assignNavigation(state, worker, position)
  return { ok: true }
}

export function setActivePlayer(state: GameState, playerId: PlayerId): void {
  state.activePlayer = playerId
}

export function setUnitStance(
  state: GameState,
  ids: string[],
  stance: UnitStance,
  owner = state.activePlayer,
): CommandResult {
  const units = ids
    .map((id) => getEntity(state, id))
    .filter((entity): entity is Entity => entity !== undefined && entity.owner === owner && entity.kind === 'unit')
  if (units.length === 0) {
    return { ok: false, reason: 'Select units before changing stance.' }
  }
  units.forEach((unit) => {
    unit.stance = stance
    unit.guardPosition = { ...unit.position }
    if (stance === 'hold' && unit.order.type === 'move') {
      unit.order = { type: 'idle' }
      clearNavigation(unit)
    }
  })
  return { ok: true }
}

export function issueRetreat(state: GameState, ids: string[], owner = state.activePlayer): CommandResult {
  const units = ids
    .map((id) => getEntity(state, id))
    .filter((entity): entity is Entity => entity !== undefined && entity.owner === owner && entity.kind === 'unit')
  const command = nearestOwnedCommand(state, owner, units[0]?.position ?? { x: 0, y: 0 })
  if (units.length === 0 || !command) {
    return { ok: false, reason: 'No retreat route is available.' }
  }
  const slots = formationSlots(command.position, units.length)
  units.forEach((unit, index) => {
    const target = slots[index] ?? command.position
    unit.order = { type: 'move', target }
    unit.resolve = Math.min(100, unit.resolve + 8)
    assignNavigation(state, unit, target)
  })
  return { ok: true }
}

export function startResearch(
  state: GameState,
  buildingId: string,
  researchId: ResearchId,
  owner = state.activePlayer,
): CommandResult {
  const building = getEntity(state, buildingId)
  const player = state.players[owner]
  const research = RESEARCH_DEFS[researchId]
  if (!building || building.owner !== owner || building.type !== research.producer || building.underConstruction) {
    return { ok: false, reason: `Select the correct completed structure for ${research.label}.` }
  }
  if (research.race && research.race !== player.race) {
    return { ok: false, reason: 'That research belongs to another faction.' }
  }
  if (player.researched.includes(researchId) || player.researchQueue.some((task) => task.id === researchId)) {
    return { ok: false, reason: `${research.label} is already known or being researched.` }
  }
  if (research.requires && !player.researched.includes(research.requires)) {
    return { ok: false, reason: `${research.label} requires ${RESEARCH_DEFS[research.requires].label}.` }
  }
  if (player.researchQueue.length > 0) {
    return { ok: false, reason: 'Another doctrine is already being researched.' }
  }
  if (player.ore < research.cost) {
    return { ok: false, reason: 'Not enough ore for research.' }
  }
  player.ore -= research.cost
  player.researchQueue.push({ id: researchId, remaining: research.time, total: research.time })
  return { ok: true }
}

export function activateRacePower(state: GameState, owner = state.activePlayer): CommandResult {
  const player = state.players[owner]
  const power = RACE_POWER_DEFS[player.race]
  if (player.powerCooldown > 0) {
    return { ok: false, reason: `${power.label} is recovering.` }
  }
  if (player.ore < power.cost) {
    return { ok: false, reason: 'Not enough ore for the faction doctrine.' }
  }

  const owned = livingByOwner(state, owner)
  if (player.race === 'ascendancy') {
    const supply = getPlayerSupply(state, owner)
    const def = getEntityDef('vanguard', player.race)
    const command = owned.find((entity) => entity.type === 'command' && !entity.underConstruction)
    if (!command) {
      return { ok: false, reason: 'The Ascendancy needs a House of Quiet.' }
    }
    if (supply.used + supply.queued + def.supplyCost > supply.cap) {
      return { ok: false, reason: 'Army capacity reached. Complete another Chrysalis Court.' }
    }
    const spawn = findSpawnPoint(state, command)
    const unit = createEntity(state, 'vanguard', owner, spawn)
    const target = offset(spawn, 110, owner === PLAYER_ONE ? 1 : -1)
    unit.order = { type: 'attack-move', target }
    assignNavigation(state, unit, target)
  } else if (player.race === 'compact') {
    owned.forEach((entity) => {
      if (entity.kind === 'unit') {
        entity.resolve = 100
        entity.hp = Math.min(entity.maxHp, entity.hp + entity.maxHp * 0.12)
      }
    })
  } else {
    owned.forEach((entity) => {
      if (entity.kind === 'building') {
        entity.hp = Math.min(entity.maxHp, entity.hp + entity.maxHp * 0.24)
      } else {
        entity.resolve = Math.min(100, entity.resolve + 18)
      }
    })
  }

  player.ore -= power.cost
  player.powerCooldown = power.cooldown
  addEvent(state, `${player.name} invoked ${power.label}.`, 'success')
  return { ok: true }
}

export function updateSimulation(state: GameState, dt: number): void {
  if (state.status !== 'playing') {
    return
  }

  const step = clamp(dt, 0, 0.05)
  state.tick += 1
  state.time += step
  Object.values(state.players).forEach((player) => {
    player.powerCooldown = Math.max(0, player.powerCooldown - step)
  })
  updateRuinTide(state)
  updateResearch(state, step)
  updateProduction(state, step)
  updateControlPoints(state, step)
  updateResolve(state)
  updateOrders(state, step)
  updateAutoAggro(state)
  updateCombat(state, step)
  updateProjectiles(state, step)
  separateUnits(state)
  updateAI(state, step)
  removeDeadEntities(state)
  updateStoryObjectives(state)
  updateWinState(state)
}

export function resolveMultiplier(entity: Entity): number {
  if (entity.kind === 'building') {
    return 1
  }
  return clamp(0.68 + entity.resolve / 100 * 0.32, 0.68, 1)
}

export function calculateDamage(attacker: Entity, target: Entity, state?: GameState): number {
  const counterBonus = attacker.bonusAgainst.includes(target.armor) ? attacker.bonusDamage : 0
  let terrainMultiplier = 1
  if (state) {
    const attackerHigh = hasTerrainKind(attacker.position, 'high-ground', state.mapId)
    const targetHigh = hasTerrainKind(target.position, 'high-ground', state.mapId)
    if (attackerHigh && !targetHigh) {
      terrainMultiplier *= 1.12
    }
    if (attacker.range >= 70 && hasTerrainKind(target.position, 'forest', state.mapId)) {
      terrainMultiplier *= 0.82
    }
  }
  return Math.max(1, (attacker.damage + counterBonus) * resolveMultiplier(attacker) * terrainMultiplier)
}

export function isPositionVisibleTo(state: GameState, position: Vec2, owner: PlayerId, buffer = 0): boolean {
  return state.entities.some(
    (entity) =>
      entity.owner === owner &&
      entity.hp > 0 &&
      !entity.underConstruction &&
      distance(entity.position, position) <=
        entity.sight + buffer + (hasTerrainKind(entity.position, 'high-ground', state.mapId) ? 38 : 0),
  )
}

export function isEntityVisibleTo(state: GameState, entity: Entity, owner: PlayerId): boolean {
  if (entity.owner === owner) {
    return true
  }
  return isPositionVisibleTo(state, entity.position, owner, entity.radius)
}

export function getEntity(state: GameState, id: string): Entity | undefined {
  return state.entities.find((entity) => entity.id === id)
}

export function livingByOwner(state: GameState, owner: PlayerId): Entity[] {
  return state.entities.filter((entity) => entity.owner === owner && entity.hp > 0)
}

export function getPlayerSupply(state: GameState, owner: PlayerId): { used: number; queued: number; cap: number } {
  const race = state.players[owner].race
  const owned = livingByOwner(state, owner)
  const used = owned.reduce((total, entity) => total + getEntityDef(entity.type, race).supplyCost, 0)
  const queued = owned.reduce(
    (total, entity) =>
      total + entity.queue.reduce((queueTotal, task) => queueTotal + getEntityDef(task.type, race).supplyCost, 0),
    0,
  )
  const cap = owned.reduce(
    (total, entity) =>
      total +
      (entity.kind === 'building' && !entity.underConstruction
        ? getEntityDef(entity.type, race).supplyProvided
        : 0),
    0,
  )
  return { used, queued, cap }
}

export function getProductionProfile(
  state: GameState,
  owner: PlayerId,
  type: UnitType,
): { cost: number; time: number } {
  return productionProfile(state, owner, type)
}

export function getObjectiveText(state: GameState): string {
  if (state.mode !== 'story') {
    const owned = state.controlPoints.filter((point) => point.owner === state.activePlayer).length
    return `Destroy the rival command keep. Relics controlled: ${owned}/${state.controlPoints.length}.`
  }

  if (state.missionId === 'mercy-for-the-uncounted') {
    const duration = getStoryMission(state.missionId).duration ?? 150
    return `Hold the command keep until dawn. ${Math.max(0, Math.ceil(duration - state.time))} seconds remain.`
  }
  if (state.missionId === 'where-roots-remember') {
    if (state.storyStep === 0) {
      const owned = state.controlPoints.filter((point) => point.owner === PLAYER_ONE).length
      return `Capture both reliquaries. ${owned}/${state.controlPoints.length} secured.`
    }
    if (state.storyStep === 1) {
      return 'Reach the Black-Iron Age at your command keep.'
    }
    return 'Destroy the rival command keep.'
  }

  if (state.storyStep === 0) {
    return 'Harvest 80 black iron.'
  }
  if (state.storyStep === 1) {
    return `Raise ${getEntityDef('barracks', state.players[PLAYER_ONE].race).label}.`
  }
  if (state.storyStep === 2) {
    return 'Keep the column together through the first Dread Tide crest.'
  }
  return 'Destroy the rival command keep.'
}

export function enemyOf(owner: PlayerId): PlayerId {
  return owner === PLAYER_ONE ? PLAYER_TWO : PLAYER_ONE
}

export function canPlaceBuilding(state: GameState, position: Vec2, radius: number): boolean {
  if (
    position.x < radius + 24 ||
    position.y < radius + 24 ||
    position.x > state.mapSize.x - radius - 24 ||
    position.y > state.mapSize.y - radius - 24
  ) {
    return false
  }

  const blockedByEntity = state.entities.some(
    (entity) => distance(entity.position, position) < entity.radius + radius + 18,
  )
  const blockedByResource = state.resources.some(
    (resource) => resource.amount > 0 && distance(resource.position, position) < resource.radius + radius + 22,
  )
  return !blockedByEntity && !blockedByResource && !isTerrainBlocked(position, radius, 12, state.mapId)
}

function createResources(mapId: GameState['mapId'], matchRule: MatchRuleId): ResourceNode[] {
  const multiplier = matchRule === 'rich-seams' ? 1.55 : 1
  return getMapDef(mapId).resources.map((resource) => ({
    ...resource,
    amount: Math.round(resource.amount * multiplier),
    maxAmount: Math.round(resource.maxAmount * multiplier),
    position: { ...resource.position },
  }))
}

function updateRuinTide(state: GameState): void {
  const period = state.matchRule === 'fast-ruin' ? RUIN_TIDE_PERIOD * 0.62 : RUIN_TIDE_PERIOD
  const wave = Math.sin((state.time / period) * Math.PI * 2 - Math.PI / 2)
  state.ruinTide = Math.round(52 + wave * 48)

  if (state.ruinTide >= 84 && !state.tideCrestAnnounced) {
    addEvent(state, 'The Dread Tide crests. Keep troops near wards or the Quiet will narrow their will.', 'danger')
    state.tideCrestAnnounced = true
  }

  if (state.ruinTide < 45) {
    state.tideCrestAnnounced = false
  }
}

function updateResolve(state: GameState): void {
  const playerResolve: Record<PlayerId, number[]> = {
    1: [],
    2: [],
  }

  state.entities.forEach((entity) => {
    if (entity.kind === 'building') {
      entity.resolve = 100
      return
    }

    const ambientDread = state.ruinTide * 0.18
    const enemyDread = state.entities.reduce((total, other) => {
      if (other.owner === entity.owner || other.hp <= 0 || other.terror <= 0) {
        return total
      }
      const gap = distance(entity.position, other.position)
      if (gap > TERROR_RANGE) {
        return total
      }
      return total + other.terror * (1 - gap / TERROR_RANGE)
    }, 0)

    const warding = state.entities.reduce((total, other) => {
      if (other.owner !== entity.owner || other.hp <= 0 || other.ward <= 0) {
        return total
      }
      const gap = distance(entity.position, other.position)
      if (gap > WARD_RANGE) {
        return total
      }
      return total + other.ward * (1 - gap / WARD_RANGE)
    }, 0)

    const raceResolve = RACE_DEFS[state.players[entity.owner].race].resolveDrift
    const cursedDread = hasTerrainKind(entity.position, 'cursed', state.mapId) ? 10 : 0
    const relicWard = state.controlPoints.some(
      (point) => point.owner === entity.owner && distance(point.position, entity.position) <= point.radius + 130,
    )
      ? 8
      : 0
    const dread = ambientDread + enemyDread + cursedDread - warding - relicWard - raceResolve
    entity.resolve = Math.round(clamp(100 - dread, 38, 100))
    playerResolve[entity.owner].push(entity.resolve)
  })

  ;([PLAYER_ONE, PLAYER_TWO] as PlayerId[]).forEach((playerId) => {
    const samples = playerResolve[playerId]
    state.players[playerId].resolve =
      samples.length === 0 ? 100 : Math.round(samples.reduce((total, value) => total + value, 0) / samples.length)
  })
}

function addStartingBase(state: GameState, owner: PlayerId, origin: Vec2, direction: 1 | -1): void {
  createEntity(state, 'command', owner, origin)
  createEntity(state, 'worker', owner, { x: origin.x + 78 * direction, y: origin.y - 24 * direction })
  createEntity(state, 'worker', owner, { x: origin.x + 38 * direction, y: origin.y + 70 * direction })
  createEntity(state, 'worker', owner, { x: origin.x + 118 * direction, y: origin.y + 52 * direction })
  createEntity(state, 'vanguard', owner, { x: origin.x + 152 * direction, y: origin.y - 74 * direction })

  if (owner === PLAYER_TWO && state.mode === 'story') {
    createEntity(state, 'barracks', owner, { x: origin.x - 145, y: origin.y + 80 })
    createEntity(state, 'turret', owner, { x: origin.x - 48, y: origin.y + 150 })
    if (state.missionId === 'mercy-for-the-uncounted' || state.aiPersonality === 'aggressive') {
      createEntity(state, 'vanguard', owner, { x: origin.x - 185, y: origin.y - 30 })
    }
    if (state.aiPersonality === 'fortress') {
      createEntity(state, 'turret', owner, { x: origin.x - 170, y: origin.y - 120 })
    }
  }
}

function assignOpeningEconomy(state: GameState, owner: PlayerId): void {
  const workers = livingByOwner(state, owner).filter((entity) => entity.type === 'worker')
  const command = livingByOwner(state, owner).find((entity) => entity.type === 'command')
  const nearbySeams = state.resources
    .filter((resource) => resource.amount > 0)
    .sort(
      (a, b) =>
        distance(command?.position ?? workers[0]?.position ?? a.position, a.position) -
        distance(command?.position ?? workers[0]?.position ?? b.position, b.position),
    )
    .slice(0, 2)
  workers.forEach((worker, index) => {
    const seam = nearbySeams[index % nearbySeams.length]
    if (seam) {
      issueGather(state, [worker.id], seam.id, owner)
    }
  })
}

function updateResearch(state: GameState, dt: number): void {
  ;([PLAYER_ONE, PLAYER_TWO] as PlayerId[]).forEach((owner) => {
    const player = state.players[owner]
    const task = player.researchQueue[0]
    if (!task) {
      return
    }
    task.remaining -= dt
    if (task.remaining > 0) {
      return
    }
    player.researchQueue.shift()
    player.researched.push(task.id)
    if (task.id === 'tier-two') {
      player.techTier = 2
    }
    livingByOwner(state, owner).forEach((entity) => applyResearchBonusesToEntity(state, entity, true))
    addEvent(state, `${player.name} completed ${RESEARCH_DEFS[task.id].label}.`, 'success')
  })
}

function updateControlPoints(state: GameState, dt: number): void {
  state.controlPoints.forEach((point) => {
    const presence: Record<PlayerId, number> = { 1: 0, 2: 0 }
    state.entities.forEach((entity) => {
      if (entity.kind !== 'unit' || entity.hp <= 0 || distance(entity.position, point.position) > point.radius) {
        return
      }
      presence[entity.owner] += entity.type === 'worker' ? 0.5 : 1
    })
    const previousOwner = point.owner
    if (presence[1] > 0 && presence[2] === 0) {
      point.influence = Math.min(100, point.influence + dt * (13 + presence[1] * 3))
    } else if (presence[2] > 0 && presence[1] === 0) {
      point.influence = Math.max(-100, point.influence - dt * (13 + presence[2] * 3))
    }
    if (point.owner === PLAYER_ONE && point.influence <= 0) {
      point.owner = null
    } else if (point.owner === PLAYER_TWO && point.influence >= 0) {
      point.owner = null
    }
    if (point.influence >= 100) {
      point.owner = PLAYER_ONE
    } else if (point.influence <= -100) {
      point.owner = PLAYER_TWO
    }
    if (previousOwner !== point.owner && point.owner) {
      state.stats[point.owner].objectivesCaptured += 1
      addEvent(state, `${state.players[point.owner].name} captured ${point.id.replaceAll('-', ' ')}.`, 'success')
    }
    if (point.owner) {
      state.players[point.owner].ore += 1.55 * dt
    }
  })
}

function updateProduction(state: GameState, dt: number): void {
  state.entities.forEach((entity) => {
    if (entity.queue.length === 0 || entity.underConstruction) {
      return
    }
    const task = entity.queue[0]
    task.remaining -= dt
    if (task.remaining <= 0) {
      entity.queue.shift()
      const spawn = findSpawnPoint(state, entity)
      const unit = createEntity(state, task.type, entity.owner, spawn)
      if (task.type === 'worker' && !entity.rallyPoint) {
        const seam = nearestResource(state, spawn)
        if (seam) {
          issueGather(state, [unit.id], seam.id, entity.owner)
          return
        }
      }
      const target = entity.rallyPoint ?? offset(spawn, 52, entity.owner === PLAYER_ONE ? 1 : -1)
      unit.order = { type: 'move', target }
      assignNavigation(state, unit, target)
    }
  })
}

function updateOrders(state: GameState, dt: number): void {
  state.entities.forEach((entity) => {
    entity.cooldown = Math.max(0, entity.cooldown - dt)

    if (entity.underConstruction && entity.order.type !== 'idle') {
      return
    }

    switch (entity.order.type) {
      case 'move':
        moveToward(state, entity, entity.order.target, dt)
        if (distance(entity.position, entity.order.target) <= Math.max(5, entity.radius * 0.45)) {
          entity.order = { type: 'idle' }
          entity.guardPosition = { ...entity.position }
          clearNavigation(entity)
        }
        break
      case 'attack':
        updateAttackOrder(state, entity, dt)
        break
      case 'attack-move':
        updateAttackMoveOrder(state, entity, dt)
        break
      case 'gather':
        updateGatherOrder(state, entity, dt)
        break
      case 'build':
        updateBuildOrder(state, entity, dt)
        break
      case 'idle':
        break
    }
  })
}

function updateAttackMoveOrder(state: GameState, entity: Entity, dt: number): void {
  if (entity.order.type !== 'attack-move') {
    return
  }

  const order = entity.order
  const currentTarget = order.targetId ? getEntity(state, order.targetId) : undefined
  if (currentTarget && currentTarget.hp > 0 && currentTarget.owner !== entity.owner) {
    const reach = entity.range + entity.radius + currentTarget.radius
    if (distance(entity.position, currentTarget.position) > reach) {
      moveToward(state, entity, currentTarget.position, dt)
    }
    return
  }

  const nextTarget = nearestEnemy(state, entity, entity.sight * 0.92)
  if (nextTarget) {
    entity.order = { ...order, targetId: nextTarget.id }
    assignNavigation(state, entity, nextTarget.position)
    return
  }

  entity.order = { ...order, targetId: undefined }
  moveToward(state, entity, order.target, dt)
  if (distance(entity.position, order.target) <= Math.max(5, entity.radius * 0.45)) {
    entity.order = { type: 'idle' }
    entity.guardPosition = { ...entity.position }
    clearNavigation(entity)
  }
}

function updateAttackOrder(state: GameState, entity: Entity, dt: number): void {
  const targetId = entity.order.type === 'attack' ? entity.order.targetId : ''
  const target = getEntity(state, targetId)
  if (!target || target.hp <= 0) {
    if (entity.stance === 'defensive' && distance(entity.position, entity.guardPosition) > 24) {
      entity.order = { type: 'move', target: { ...entity.guardPosition } }
      assignNavigation(state, entity, entity.guardPosition)
    } else {
      entity.order = { type: 'idle' }
      clearNavigation(entity)
    }
    return
  }

  const reach = entity.range + entity.radius + target.radius
  const targetDistance = distance(entity.position, target.position)
  if (entity.stance === 'hold' && targetDistance > reach) {
    entity.order = { type: 'idle' }
    clearNavigation(entity)
    return
  }
  if (entity.stance === 'defensive' && distance(target.position, entity.guardPosition) > 235) {
    entity.order = { type: 'move', target: { ...entity.guardPosition } }
    assignNavigation(state, entity, entity.guardPosition)
    return
  }
  if (targetDistance > reach) {
    moveToward(state, entity, target.position, dt)
  }
}

function updateGatherOrder(state: GameState, entity: Entity, dt: number): void {
  if (entity.type !== 'worker' || entity.order.type !== 'gather') {
    return
  }

  const order = entity.order
  const node = state.resources.find((resource) => resource.id === order.resourceId)
  if (!node || node.amount <= 0) {
    entity.order = { type: 'idle' }
    clearNavigation(entity)
    return
  }

  if (order.phase === 'to-resource') {
    moveToward(state, entity, node.position, dt)
    if (distance(entity.position, node.position) < entity.radius + node.radius + 8) {
      entity.order = { ...order, phase: 'harvest', harvestTimer: HARVEST_SECONDS }
      clearNavigation(entity)
    }
    return
  }

  if (order.phase === 'harvest') {
    const nextTimer = order.harvestTimer - dt
    if (nextTimer > 0) {
      entity.order = { ...order, harvestTimer: nextTimer }
      return
    }
    const mined = Math.min(ORE_PER_TRIP, node.amount)
    node.amount -= mined
    entity.carrying = mined
    entity.order = { ...order, phase: 'return', harvestTimer: 0 }
    const command = nearestOwnedCommand(state, entity.owner, entity.position)
    if (command) {
      assignNavigation(state, entity, command.position)
    }
    return
  }

  const command = nearestOwnedCommand(state, entity.owner, entity.position)
  if (!command) {
    entity.order = { type: 'idle' }
    clearNavigation(entity)
    return
  }

  moveToward(state, entity, command.position, dt)
  if (distance(entity.position, command.position) < entity.radius + command.radius + 8) {
    state.stats[entity.owner].oreMined += entity.carrying
    state.players[entity.owner].ore += entity.carrying * RACE_DEFS[state.players[entity.owner].race].incomeRate
    entity.carrying = 0
    entity.order = { ...order, phase: 'to-resource', harvestTimer: 0 }
    assignNavigation(state, entity, node.position)
  }
}

function updateBuildOrder(state: GameState, entity: Entity, dt: number): void {
  if (entity.type !== 'worker' || entity.order.type !== 'build') {
    return
  }

  const order = entity.order
  const def = getEntityDef(order.buildingType, state.players[entity.owner].race)
  const building = getEntity(state, order.buildingId)
  if (!building || building.hp <= 0) {
    entity.order = { type: 'idle' }
    clearNavigation(entity)
    return
  }

  const reach = entity.radius + building.radius + 12
  if (distance(entity.position, building.position) > reach) {
    moveToward(state, entity, building.position, dt)
    return
  }

  const progress = order.progress + dt
  const progressRatio = clamp(progress / def.buildTime, 0, 1)
  building.constructionProgress = progressRatio
  building.hp = Math.max(building.hp, Math.round(building.maxHp * (0.24 + progressRatio * 0.76)))

  if (progress < def.buildTime) {
    entity.order = { ...order, progress }
    return
  }

  building.underConstruction = false
  building.constructionProgress = 1
  building.hp = building.maxHp
  entity.order = { type: 'idle' }
  entity.guardPosition = { ...entity.position }
  clearNavigation(entity)
  addEvent(state, `${state.players[entity.owner].name} completed ${building.label}.`, 'success')
}

function updateAutoAggro(state: GameState): void {
  state.entities.forEach((entity) => {
    const canAcquire = entity.kind === 'building' || entity.order.type === 'idle'
    if (entity.damage <= 0 || !canAcquire) {
      return
    }

    const acquisitionRange =
      entity.kind === 'building'
        ? entity.range + 90
        : entity.stance === 'hold'
          ? entity.range + entity.radius + 42
          : entity.stance === 'defensive'
            ? entity.sight * 0.72
            : entity.sight
    const nearest = nearestEnemy(state, entity, acquisitionRange)
    if (nearest) {
      entity.order = { type: 'attack', targetId: nearest.id }
      if (entity.kind === 'unit' && entity.stance !== 'hold') {
        assignNavigation(state, entity, nearest.position)
      }
    }
  })
}

function updateCombat(state: GameState, _dt: number): void {
  state.entities.forEach((entity) => {
    if (entity.damage <= 0 || entity.cooldown > 0) {
      return
    }

    const target =
      entity.order.type === 'attack'
        ? getEntity(state, entity.order.targetId)
        : entity.order.type === 'attack-move' && entity.order.targetId
          ? getEntity(state, entity.order.targetId)
        : nearestEnemy(state, entity, entity.range + entity.radius + 80)

    if (!target || target.owner === entity.owner || target.hp <= 0) {
      return
    }

    const reach = entity.range + entity.radius + target.radius
    if (distance(entity.position, target.position) <= reach) {
      const damage = calculateDamage(entity, target, state)
      if (entity.type === 'skirmisher' || entity.type === 'turret') {
        state.projectiles.push({
          id: `projectile-${state.nextEventId}`,
          owner: entity.owner,
          sourceId: entity.id,
          targetId: target.id,
          position: { ...entity.position },
          speed: entity.type === 'turret' ? 690 : 520,
          damage,
          radius: entity.type === 'turret' ? 7 : 5,
        })
        state.nextEventId += 1
      } else {
        applyDamage(state, entity.owner, target, damage)
      }
      entity.cooldown = entity.attackCooldown
    }
  })
}

function updateProjectiles(state: GameState, dt: number): void {
  state.projectiles = state.projectiles.filter((projectile) => {
    const target = getEntity(state, projectile.targetId)
    if (!target || target.hp <= 0) {
      return false
    }
    const gap = distance(projectile.position, target.position)
    const impactDistance = target.radius + projectile.radius
    if (gap <= impactDistance) {
      applyDamage(state, projectile.owner, target, projectile.damage)
      return false
    }
    const step = Math.min(gap, projectile.speed * dt)
    projectile.position.x += ((target.position.x - projectile.position.x) / gap) * step
    projectile.position.y += ((target.position.y - projectile.position.y) / gap) * step
    return true
  })
}

function applyDamage(state: GameState, owner: PlayerId, target: Entity, damage: number): void {
  const dealt = Math.min(target.hp, damage)
  target.hp -= damage
  state.stats[owner].damageDealt += dealt
}

function updateAI(state: GameState, dt: number): void {
  if (state.mode === 'pvp' || state.status !== 'playing') {
    return
  }

  state.aiDecisionTimer -= dt
  if (state.aiDecisionTimer <= 0) {
    runAiMacro(state)
    state.aiDecisionTimer = 0.72
  }

  const aiPlayer = state.players[PLAYER_TWO]
  const aiPower = RACE_POWER_DEFS[aiPlayer.race]
  const aiCommand = livingByOwner(state, PLAYER_TWO).find((entity) => entity.type === 'command')
  if (
    aiPlayer.powerCooldown <= 0 &&
    aiPlayer.ore >= aiPower.cost &&
    (state.ruinTide >= 72 || (aiCommand ? aiCommand.hp < aiCommand.maxHp * 0.72 : false))
  ) {
    activateRacePower(state, PLAYER_TWO)
  }

  const openingTruce = state.mode === 'story' && state.missionId === 'bridge-of-names' && state.storyStep === 0
  if (openingTruce) {
    return
  }

  state.aiWaveTimer -= dt
  if (state.aiWaveTimer <= 0) {
    const army = livingByOwner(state, PLAYER_TWO).filter(
      (entity) => entity.kind === 'unit' && entity.type !== 'worker' && entity.damage > 0,
    )
    const minimumArmy = state.aiPersonality === 'aggressive' ? 2 : state.aiPersonality === 'economic' ? 4 : 6
    const openRelic = state.controlPoints.find((point) => point.owner !== PLAYER_TWO)
    const target = chooseAiAttackTarget(state)
    if (army.length >= minimumArmy && openRelic) {
      issueAttackMove(state, army.map((unit) => unit.id), openRelic.position, PLAYER_TWO)
      addEvent(state, `${state.players[PLAYER_TWO].name} is marching on a relic.`, 'danger')
    } else if (target && army.length >= minimumArmy) {
      issueAttack(state, army.map((unit) => unit.id), target.id, PLAYER_TWO)
      addEvent(state, `${state.players[PLAYER_TWO].name} warband hunting your ${target.label}.`, 'danger')
    }
    const baseDelay = state.aiPersonality === 'aggressive' ? 22 : state.aiPersonality === 'economic' ? 31 : 38
    state.aiWaveTimer = state.ruinTide >= 75 ? baseDelay * 0.76 : baseDelay
  }
}

function runAiMacro(state: GameState): void {
  const owned = livingByOwner(state, PLAYER_TWO)
  const player = state.players[PLAYER_TWO]
  const command = owned.find((entity) => entity.type === 'command' && !entity.underConstruction)
  const workers = owned.filter((entity) => entity.type === 'worker')

  workers
    .filter((worker) => worker.order.type === 'idle')
    .forEach((worker) => {
      const seam = nearestResource(state, worker.position)
      if (seam) {
        issueGather(state, [worker.id], seam.id, PLAYER_TWO)
      }
    })

  if (command) {
    const queuedWorkers = command.queue.filter((task) => task.type === 'worker').length
    const workerTarget = state.aiPersonality === 'economic' ? 7 : state.aiPersonality === 'fortress' ? 6 : 5
    const workerProfile = productionProfile(state, PLAYER_TWO, 'worker')
    if (workers.length + queuedWorkers < workerTarget && command.queue.length < 2 && player.ore >= workerProfile.cost) {
      startProduction(state, command.id, 'worker', PLAYER_TWO)
    }
  }

  maintainAiInfrastructure(state, command, workers)
  updateAiResearch(state)
  trainAiArmy(state)
  respondToAiThreats(state)
}

function maintainAiInfrastructure(state: GameState, command: Entity | undefined, workers: Entity[]): void {
  if (!command) {
    return
  }
  const owned = livingByOwner(state, PLAYER_TWO)
  const halls = owned.filter((entity) => entity.type === 'barracks')
  const turrets = owned.filter((entity) => entity.type === 'turret')
  const supply = getPlayerSupply(state, PLAYER_TWO)
  const hallTarget = supply.cap - supply.used - supply.queued <= 4 ? 2 : 1
  const desiredTurrets = state.aiPersonality === 'fortress' ? 2 : state.time >= 70 ? 1 : 0
  const builder = workers.find((worker) => worker.order.type !== 'build')

  if (halls.length < hallTarget && builder) {
    tryAiBuilding(state, builder, command, 'barracks', halls.length)
    return
  }
  if (turrets.length < desiredTurrets && builder) {
    tryAiBuilding(state, builder, command, 'turret', turrets.length)
  }
}

function tryAiBuilding(
  state: GameState,
  worker: Entity,
  command: Entity,
  buildingType: BuildingType,
  index: number,
): boolean {
  const def = getEntityDef(buildingType, state.players[PLAYER_TWO].race)
  if (state.players[PLAYER_TWO].ore < def.cost) {
    return false
  }
  const inward = command.position.x > state.mapSize.x / 2 ? -1 : 1
  const offsets =
    buildingType === 'barracks'
      ? [
          { x: 168 * inward, y: 92 },
          { x: 225 * inward, y: -105 },
          { x: 112 * inward, y: -205 },
          { x: 270 * inward, y: 185 },
        ]
      : [
          { x: 76 * inward, y: 190 },
          { x: 188 * inward, y: -176 },
          { x: 255 * inward, y: 45 },
          { x: 72 * inward, y: -240 },
        ]
  for (let offsetIndex = 0; offsetIndex < offsets.length; offsetIndex += 1) {
    const offsetValue = offsets[(offsetIndex + index) % offsets.length]
    const position = { x: command.position.x + offsetValue.x, y: command.position.y + offsetValue.y }
    if (canPlaceBuilding(state, position, def.radius)) {
      return startBuilding(state, worker.id, buildingType, position, PLAYER_TWO).ok
    }
  }
  return false
}

function trainAiArmy(state: GameState): void {
  const player = state.players[PLAYER_TWO]
  const barracks = livingByOwner(state, PLAYER_TWO).filter(
    (entity) => entity.type === 'barracks' && !entity.underConstruction,
  )
  barracks.forEach((hall) => {
    if (hall.queue.length >= 2) {
      return
    }
    const preferred = chooseAiUnitType(state)
    const profile = productionProfile(state, PLAYER_TWO, preferred)
    if (player.ore >= profile.cost) {
      startProduction(state, hall.id, preferred, PLAYER_TWO)
    }
  })
}

function chooseAiUnitType(state: GameState): UnitType {
  const player = state.players[PLAYER_TWO]
  if (!player.researched.includes('tier-two')) {
    return 'vanguard'
  }
  const visibleEnemies = livingByOwner(state, PLAYER_ONE).filter(
    (entity) => entity.kind === 'unit' && isEntityVisibleTo(state, entity, PLAYER_TWO),
  )
  const enemyVanguards = visibleEnemies.filter((entity) => entity.type === 'vanguard').length
  const enemySkirmishers = visibleEnemies.filter((entity) => entity.type === 'skirmisher').length
  const ownArmy = livingByOwner(state, PLAYER_TWO).filter(
    (entity) => entity.kind === 'unit' && entity.type !== 'worker' && entity.damage > 0,
  )
  const ownVanguards = ownArmy.filter((entity) => entity.type === 'vanguard').length
  const ownSkirmishers = ownArmy.filter((entity) => entity.type === 'skirmisher').length

  if (enemyVanguards > enemySkirmishers || state.aiPersonality === 'fortress') {
    return ownSkirmishers <= ownVanguards ? 'skirmisher' : 'vanguard'
  }
  if (enemySkirmishers > enemyVanguards) {
    return ownVanguards <= ownSkirmishers + 1 ? 'vanguard' : 'skirmisher'
  }
  return state.ruinTide >= 70 || ownVanguards > ownSkirmishers + 1 ? 'skirmisher' : 'vanguard'
}

function respondToAiThreats(state: GameState): void {
  const owned = livingByOwner(state, PLAYER_TWO)
  const buildings = owned.filter((entity) => entity.kind === 'building')
  const army = owned.filter((entity) => entity.kind === 'unit' && entity.type !== 'worker' && entity.damage > 0)
  if (army.length === 0 || buildings.length === 0) {
    return
  }
  const threat = livingByOwner(state, PLAYER_ONE)
    .filter((entity) => entity.kind === 'unit')
    .filter((entity) => buildings.some((building) => distance(entity.position, building.position) <= 420))
    .sort((a, b) => distance(a.position, buildings[0].position) - distance(b.position, buildings[0].position))[0]
  if (threat) {
    const defenders = army.filter((unit) => unit.hp / unit.maxHp >= 0.34)
    if (defenders.length > 0) {
      issueAttack(state, defenders.map((unit) => unit.id), threat.id, PLAYER_TWO)
    }
    return
  }

  const averageHealth = army.reduce((total, unit) => total + unit.hp / unit.maxHp, 0) / army.length
  const averageResolve = army.reduce((total, unit) => total + unit.resolve, 0) / army.length
  if (army.length >= 3 && (averageHealth < 0.46 || averageResolve < 50)) {
    issueRetreat(state, army.map((unit) => unit.id), PLAYER_TWO)
  }
}

function nearestResource(state: GameState, position: Vec2): ResourceNode | undefined {
  return state.resources
    .filter((resource) => resource.amount > 0)
    .sort((a, b) => distance(position, a.position) - distance(position, b.position))[0]
}

function updateAiResearch(state: GameState): void {
  const player = state.players[PLAYER_TWO]
  if (player.researchQueue.length > 0) {
    return
  }
  const command = livingByOwner(state, PLAYER_TWO).find((entity) => entity.type === 'command' && !entity.underConstruction)
  const barracks = livingByOwner(state, PLAYER_TWO).find((entity) => entity.type === 'barracks' && !entity.underConstruction)
  if (!player.researched.includes('tier-two')) {
    const buffer = state.aiPersonality === 'aggressive' ? 105 : 55
    if (command && state.time >= 24 && player.ore >= RESEARCH_DEFS['tier-two'].cost + buffer) {
      startResearch(state, command.id, 'tier-two', PLAYER_TWO)
    }
    return
  }

  const preferences: Record<AiPersonality, ResearchId[]> = {
    aggressive: ['tempered-oaths', 'chorus-of-knives', 'siege-liturgy'],
    economic: ['wardcraft', 'pit-broods', 'vault-plate'],
    fortress: ['wardcraft', 'chorus-of-knives', 'vault-plate'],
  }
  const available = researchForRace(player.race).filter(
    (research) =>
      research.id !== 'tier-two' &&
      !player.researched.includes(research.id) &&
      (!research.requires || player.researched.includes(research.requires)),
  )
  const priority = (id: ResearchId) => {
    const index = preferences[state.aiPersonality].indexOf(id)
    return index < 0 ? 99 : index
  }
  available.sort((a, b) => priority(a.id) - priority(b.id))
  const next = available[0]
  const producer = next?.producer === 'command' ? command : barracks
  if (next && producer && player.ore >= next.cost + 70) {
    startResearch(state, producer.id, next.id, PLAYER_TWO)
  }
}

function updateStoryObjectives(state: GameState): void {
  if (state.mode !== 'story') {
    return
  }

  if (state.missionId === 'mercy-for-the-uncounted') {
    if (state.storyStep === 0 && state.time >= 45) {
      state.storyStep = 1
      addEvent(state, 'The first clinic column is clear. Compact companies are closing on the road.', 'danger')
    }
    if (state.storyStep === 1 && state.time >= 95) {
      state.storyStep = 2
      addEvent(state, 'The final patients are moving. Hold the House of Quiet until the eastern clouds pale.', 'success')
    }
    return
  }

  if (state.missionId === 'where-roots-remember') {
    if (
      state.storyStep === 0 &&
      state.controlPoints.length > 0 &&
      state.controlPoints.every((point) => point.owner === PLAYER_ONE)
    ) {
      state.storyStep = 1
      addEvent(state, 'Both memory stones answer Tavra. Reach the Black-Iron Age.', 'success')
    }
    if (state.storyStep === 1 && state.players[PLAYER_ONE].researched.includes('tier-two')) {
      state.storyStep = 2
      addEvent(state, 'The resonance forge wakes. Break the House of Quiet.', 'success')
    }
    return
  }

  if (state.storyStep === 0 && state.players[PLAYER_ONE].ore >= STARTING_ORE + 80) {
    state.storyStep = 1
    addEvent(
      state,
      `Black iron secured. Raise ${getEntityDef('barracks', state.players[PLAYER_ONE].race).label}.`,
      'success',
    )
  }

  if (
    state.storyStep === 1 &&
    state.entities.some((entity) => entity.owner === PLAYER_ONE && entity.type === 'barracks' && !entity.underConstruction)
  ) {
    state.storyStep = 2
    addEvent(
      state,
      `${getEntityDef('barracks', state.players[PLAYER_ONE].race).label} raised. Hold the road through the next Dread Tide crest.`,
      'success',
    )
  }

  if (state.storyStep === 2 && state.ruinTide >= 84) {
    state.storyStep = 3
    addEvent(
      state,
      `The first crest breaks. Train a warband and destroy the ${getEntityDef('command', state.players[PLAYER_TWO].race).label}.`,
      'success',
    )
  }
}

function chooseAiAttackTarget(state: GameState): Entity | undefined {
  const playerAssets = livingByOwner(state, PLAYER_ONE).filter((entity) => entity.kind === 'building')
  return (
    playerAssets.find((entity) => entity.type === 'turret') ??
    playerAssets.find((entity) => entity.type === 'barracks') ??
    playerAssets.find((entity) => entity.type === 'command')
  )
}

function removeDeadEntities(state: GameState): void {
  const dead = state.entities.filter((entity) => entity.hp <= 0)
  dead.forEach((entity) => {
    if (entity.kind === 'unit') {
      state.stats[entity.owner].unitsLost += 1
    } else {
      state.stats[entity.owner].structuresLost += 1
    }
    if (entity.type === 'command') {
      addEvent(state, `${entity.label} of ${state.players[entity.owner].name} destroyed.`, 'danger')
    }
  })
  state.entities = state.entities.filter((entity) => entity.hp > 0)
}

function updateWinState(state: GameState): void {
  const p1Command = state.entities.some((entity) => entity.owner === PLAYER_ONE && entity.type === 'command')
  const p2Command = state.entities.some((entity) => entity.owner === PLAYER_TWO && entity.type === 'command')

  if (state.mode === 'story' && state.missionId === 'mercy-for-the-uncounted') {
    const duration = getStoryMission(state.missionId).duration ?? 150
    if (!p1Command || state.time >= duration) {
      state.status = p1Command ? 'won' : 'lost'
      state.winner = p1Command ? PLAYER_ONE : PLAYER_TWO
      addEvent(
        state,
        p1Command ? 'Victory. Dawn finds the final clinic column beyond the valley.' : 'Defeat. The evacuation road is closed.',
        p1Command ? 'success' : 'danger',
      )
    }
    return
  }

  if (!p1Command || !p2Command) {
    state.status = p1Command ? 'won' : 'lost'
    state.winner = p1Command ? PLAYER_ONE : PLAYER_TWO
    addEvent(
      state,
      p1Command ? 'Victory. Your answer to Bellgrave survives another morning.' : 'Defeat. Another promise is written by the victor.',
      p1Command ? 'success' : 'danger',
    )
  }
}

function productionProfile(state: GameState, owner: PlayerId, type: UnitType): { cost: number; time: number } {
  const player = state.players[owner]
  const def = getEntityDef(type, player.race)
  const pitBroods = player.race === 'ascendancy' && player.researched.includes('pit-broods') && type === 'vanguard'
  return {
    cost: Math.round(def.cost * (pitBroods ? 0.82 : 1)),
    time: def.buildTime * (pitBroods ? 0.82 : 1),
  }
}

function applyResearchBonusesToEntity(state: GameState, entity: Entity, refresh = false): void {
  const player = state.players[entity.owner]
  const def = getEntityDef(entity.type, player.race)
  const healthRatio = refresh && entity.maxHp > 0 ? entity.hp / entity.maxHp : entity.hp / def.hp
  let maxHp = def.hp
  let speed = def.speed
  let damage = def.damage
  let bonusDamage = def.bonusDamage
  let terror = def.terror
  let ward = def.ward

  if (player.researched.includes('tempered-oaths') && entity.type === 'vanguard') {
    maxHp *= 1.15
    damage *= 1.15
  }
  if (player.researched.includes('wardcraft')) {
    ward *= 1.35
  }
  if (
    player.researched.includes('chorus-of-knives') &&
    entity.kind === 'unit' &&
    entity.type !== 'worker'
  ) {
    speed *= 1.1
    terror += 4
  }
  if (
    player.researched.includes('vault-plate') &&
    (entity.kind === 'building' || entity.type !== 'worker')
  ) {
    maxHp *= 1.2
  }
  if (player.researched.includes('siege-liturgy') && entity.type === 'vanguard') {
    bonusDamage += 8
  }

  entity.maxHp = Math.round(maxHp)
  entity.hp = clamp(Math.round(entity.maxHp * healthRatio), 1, entity.maxHp)
  entity.speed = speed
  entity.range = def.range
  entity.damage = Math.round(damage * 10) / 10
  entity.attackCooldown = def.attackCooldown
  entity.sight = def.sight
  entity.armor = def.armor
  entity.bonusAgainst = [...def.bonusAgainst]
  entity.bonusDamage = bonusDamage
  entity.terror = terror
  entity.ward = ward
}

function assignNavigation(state: GameState, entity: Entity, target: Vec2): void {
  entity.navigationTarget = { ...target }
  entity.navigationPath = findNavigationPath(state, entity.position, target, entity.radius, entity.id)
}

function clearNavigation(entity: Entity): void {
  entity.navigationPath = []
  entity.navigationTarget = null
}

function moveToward(state: GameState, entity: Entity, target: Vec2, dt: number): void {
  if (entity.speed <= 0) {
    return
  }
  if (!entity.navigationTarget || distance(entity.navigationTarget, target) > 90) {
    assignNavigation(state, entity, target)
  }
  while (
    entity.navigationPath.length > 0 &&
    distance(entity.position, entity.navigationPath[0]) <= Math.max(8, entity.radius * 0.7)
  ) {
    entity.navigationPath.shift()
  }
  const steeringTarget = entity.navigationPath[0] ?? target
  const dx = steeringTarget.x - entity.position.x
  const dy = steeringTarget.y - entity.position.y
  const dist = Math.hypot(dx, dy)
  if (dist < 0.001) {
    return
  }
  const terrainSpeed = hasTerrainKind(entity.position, 'ford', state.mapId)
    ? 0.78
    : hasTerrainKind(entity.position, 'forest', state.mapId)
      ? 0.88
      : 1
  const step = Math.min(dist, entity.speed * resolveMultiplier(entity) * terrainSpeed * dt)
  const angle = Math.atan2(dy, dx)
  const candidates = [0, Math.PI / 8, -Math.PI / 8, Math.PI / 4, -Math.PI / 4, Math.PI / 2, -Math.PI / 2]
    .map((offsetAngle) => ({
      x: entity.position.x + Math.cos(angle + offsetAngle) * step,
      y: entity.position.y + Math.sin(angle + offsetAngle) * step,
    }))
    .filter((position) => canOccupyUnitTerrain(state, entity, position))
    .sort((a, b) => distance(a, steeringTarget) - distance(b, steeringTarget))

  const next = candidates[0]
  if (next) {
    entity.position.x = next.x
    entity.position.y = next.y
  }
}

function canOccupyUnitTerrain(state: GameState, entity: Entity, position: Vec2): boolean {
  if (
    position.x < entity.radius ||
    position.y < entity.radius ||
    position.x > state.mapSize.x - entity.radius ||
    position.y > state.mapSize.y - entity.radius
  ) {
    return false
  }

  return !isTerrainBlocked(position, entity.radius, 3, state.mapId)
}

function separateUnits(state: GameState): void {
  const movers = state.entities.filter((entity) => entity.kind === 'unit')
  for (let i = 0; i < movers.length; i += 1) {
    for (let j = i + 1; j < movers.length; j += 1) {
      const a = movers[i]
      const b = movers[j]
      const minDist = a.radius + b.radius + 7
      const dist = distance(a.position, b.position)
      if (dist > 0 && dist < minDist) {
        const push = (minDist - dist) * 0.5
        const nx = (a.position.x - b.position.x) / dist
        const ny = (a.position.y - b.position.y) / dist
        const nextA = { x: a.position.x + nx * push, y: a.position.y + ny * push }
        const nextB = { x: b.position.x - nx * push, y: b.position.y - ny * push }
        if (canOccupyUnitTerrain(state, a, nextA)) {
          a.position = nextA
        }
        if (canOccupyUnitTerrain(state, b, nextB)) {
          b.position = nextB
        }
      }
    }
  }

  movers.forEach((unit) => {
    state.entities
      .filter((entity) => entity.kind === 'building')
      .forEach((building) => {
        const minDist = unit.radius + building.radius + 4
        const dist = distance(unit.position, building.position)
        if (dist > 0 && dist < minDist) {
          const push = minDist - dist
          const next = {
            x: unit.position.x + ((unit.position.x - building.position.x) / dist) * push,
            y: unit.position.y + ((unit.position.y - building.position.y) / dist) * push,
          }
          if (canOccupyUnitTerrain(state, unit, next)) {
            unit.position = next
          }
        }
      })
  })
}

function nearestEnemy(state: GameState, entity: Entity, maxRange = Number.POSITIVE_INFINITY): Entity | undefined {
  let best: Entity | undefined
  let bestScore = Number.POSITIVE_INFINITY
  state.entities.forEach((candidate) => {
    if (candidate.owner === entity.owner || candidate.hp <= 0) {
      return
    }
    const candidateDistance = distance(entity.position, candidate.position)
    if (candidateDistance > maxRange) {
      return
    }
    const priority =
      candidate.kind === 'unit' && candidate.damage > 0
        ? -62
        : candidate.type === 'turret'
          ? -32
          : candidate.type === 'worker'
            ? 24
            : 46
    const score = candidateDistance + priority
    if (score < bestScore) {
      best = candidate
      bestScore = score
    }
  })
  return best
}

function nearestOwnedCommand(state: GameState, owner: PlayerId, position: Vec2): Entity | undefined {
  let best: Entity | undefined
  let bestDistance = Number.POSITIVE_INFINITY
  state.entities.forEach((entity) => {
    if (entity.owner !== owner || entity.type !== 'command') {
      return
    }
    const commandDistance = distance(position, entity.position)
    if (commandDistance < bestDistance) {
      best = entity
      bestDistance = commandDistance
    }
  })
  return best
}

function findSpawnPoint(state: GameState, building: Entity): Vec2 {
  const angleStep = Math.PI / 4
  for (let index = 0; index < 8; index += 1) {
    const angle = angleStep * index
    const position = {
      x: building.position.x + Math.cos(angle) * (building.radius + 46),
      y: building.position.y + Math.sin(angle) * (building.radius + 46),
    }
    if (canPlaceUnit(state, position, 16)) {
      return position
    }
  }
  return { x: building.position.x + building.radius + 48, y: building.position.y }
}

function canPlaceUnit(state: GameState, position: Vec2, radius: number): boolean {
  return (
    !isTerrainBlocked(position, radius, 3, state.mapId) &&
    !state.entities.some((entity) => distance(entity.position, position) < entity.radius + radius)
  )
}

function formationSlots(center: Vec2, count: number): Vec2[] {
  const slots: Vec2[] = []
  const columns = Math.ceil(Math.sqrt(count))
  const spacing = 44
  for (let index = 0; index < count; index += 1) {
    const col = index % columns
    const row = Math.floor(index / columns)
    slots.push({
      x: center.x + (col - (columns - 1) / 2) * spacing,
      y: center.y + (row - Math.floor(count / columns) / 2) * spacing,
    })
  }
  return slots
}

function offset(position: Vec2, amount: number, direction: 1 | -1): Vec2 {
  return {
    x: position.x + amount * direction,
    y: position.y + amount * 0.35 * direction,
  }
}
