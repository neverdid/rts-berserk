import { describe, expect, it } from 'vitest'
import {
  activateRacePower,
  canPlaceBuilding,
  createEntity,
  createInitialState,
  calculateDamage,
  getPlayerSupply,
  getEntity,
  isEntityVisibleTo,
  issueAttackMove,
  issueGather,
  issueMove,
  resolveMultiplier,
  setRallyPoint,
  setUnitStance,
  startBuilding,
  startProduction,
  startResearch,
  updateSimulation,
} from './simulation'
import { TERRAIN_BLOCKERS, isTerrainBlocked } from './map'

function runFor(seconds: number, step: number, tick: () => void): void {
  for (let elapsed = 0; elapsed < seconds; elapsed += step) {
    tick()
  }
}

describe('RTS simulation', () => {
  it('starts local PvP with symmetric infrastructure', () => {
    const state = createInitialState('pvp')
    const buildingsOne = state.entities.filter((entity) => entity.owner === 1 && entity.kind === 'building')
    const buildingsTwo = state.entities.filter((entity) => entity.owner === 2 && entity.kind === 'building')
    expect(buildingsOne.map((entity) => entity.type)).toEqual(buildingsTwo.map((entity) => entity.type))
    expect(getPlayerSupply(state, 1).cap).toBe(getPlayerSupply(state, 2).cap)
  })

  it('lets workers gather and deposit ore', () => {
    const state = createInitialState('pvp')
    const worker = state.entities.find((entity) => entity.owner === 1 && entity.type === 'worker')
    const ore = state.resources[0]
    expect(worker).toBeDefined()

    const startingOre = state.players[1].ore
    issueGather(state, [worker!.id], ore.id, 1)
    runFor(18, 0.05, () => updateSimulation(state, 0.05))

    expect(state.players[1].ore).toBeGreaterThan(startingOre)
    expect(ore.amount).toBeLessThan(ore.maxAmount)
  })

  it('spends ore and spawns trained units from a queue', () => {
    const state = createInitialState('pvp')
    const command = state.entities.find((entity) => entity.owner === 1 && entity.type === 'command')
    expect(command).toBeDefined()

    const result = startProduction(state, command!.id, 'worker', 1)
    expect(result.ok).toBe(true)
    expect(state.players[1].ore).toBe(215)

    runFor(7, 0.05, () => updateSimulation(state, 0.05))
    const workers = state.entities.filter((entity) => entity.owner === 1 && entity.type === 'worker')
    expect(workers.length).toBe(4)
  })

  it('counts queued army capacity and blocks over-cap production', () => {
    const state = createInitialState('pvp')
    const command = state.entities.find((entity) => entity.owner === 1 && entity.type === 'command')
    expect(command).toBeDefined()
    expect(getPlayerSupply(state, 1)).toEqual({ used: 5, queued: 0, cap: 14 })

    for (let index = 0; index < 9; index += 1) {
      createEntity(state, 'worker', 1, { x: 520 + index * 24, y: 1320 })
    }

    const result = startProduction(state, command!.id, 'worker', 1)
    expect(result.ok).toBe(false)
    expect(result.reason).toContain('capacity')
  })

  it('sends trained units to a persistent rally point', () => {
    const state = createInitialState('pvp')
    const command = state.entities.find((entity) => entity.owner === 1 && entity.type === 'command')
    expect(command).toBeDefined()
    const rally = { x: 760, y: 980 }
    expect(setRallyPoint(state, [command!.id], rally, 1).ok).toBe(true)
    expect(startProduction(state, command!.id, 'worker', 1).ok).toBe(true)

    runFor(6.2, 0.05, () => updateSimulation(state, 0.05))
    const newWorker = state.entities.filter((entity) => entity.owner === 1 && entity.type === 'worker').at(-1)
    expect(newWorker?.order.type).toBe('move')
    if (newWorker?.order.type === 'move') {
      expect(newWorker.order.target).toEqual(rally)
    }
  })

  it('attack-moves through a target and resumes the advance', () => {
    const state = createInitialState('pvp')
    const attacker = createEntity(state, 'vanguard', 1, { x: 800, y: 900 })
    const defender = createEntity(state, 'worker', 2, { x: 855, y: 900 })
    defender.hp = 1
    const destination = { x: 1100, y: 900 }

    expect(issueAttackMove(state, [attacker.id], destination, 1).ok).toBe(true)
    runFor(0.4, 0.05, () => updateSimulation(state, 0.05))
    expect(getEntity(state, defender.id)).toBeUndefined()

    runFor(1, 0.05, () => updateSimulation(state, 0.05))
    expect(attacker.position.x).toBeGreaterThan(850)
    expect(attacker.order.type).toBe('attack-move')
  })

  it('applies race-specific names and production costs', () => {
    const state = createInitialState('pvp', 'hollow', 'sepulcher')
    const command = state.entities.find((entity) => entity.owner === 1 && entity.type === 'command')
    expect(command).toBeDefined()
    expect(state.players[1].name).toBe('Hollow Choir')
    expect(command!.label).toBe('Choir Spire')

    const result = startProduction(state, command!.id, 'worker', 1)
    expect(result.ok).toBe(true)
    expect(state.players[1].ore).toBe(224)

    runFor(5.5, 0.05, () => updateSimulation(state, 0.05))
    const workers = state.entities.filter((entity) => entity.owner === 1 && entity.type === 'worker')
    expect(workers.some((entity) => entity.label === 'Mute Thrall')).toBe(true)
  })

  it('gives each faction a distinct recovery doctrine', () => {
    const remnant = createInitialState('pvp', 'candlebound', 'hollow')
    const remnantSoldier = remnant.entities.find((entity) => entity.owner === 1 && entity.type === 'vanguard')!
    remnantSoldier.hp = 30
    remnantSoldier.resolve = 20
    expect(activateRacePower(remnant, 1).ok).toBe(true)
    expect(remnantSoldier.hp).toBeGreaterThan(30)
    expect(remnantSoldier.resolve).toBe(100)
    expect(activateRacePower(remnant, 1).ok).toBe(false)

    const choir = createInitialState('pvp', 'hollow', 'sepulcher')
    const choirCount = choir.entities.filter((entity) => entity.owner === 1 && entity.type === 'vanguard').length
    expect(activateRacePower(choir, 1).ok).toBe(true)
    expect(choir.entities.filter((entity) => entity.owner === 1 && entity.type === 'vanguard')).toHaveLength(choirCount + 1)

    const host = createInitialState('pvp', 'sepulcher', 'hollow')
    const hostKeep = host.entities.find((entity) => entity.owner === 1 && entity.type === 'command')!
    hostKeep.hp = 300
    expect(activateRacePower(host, 1).ok).toBe(true)
    expect(hostKeep.hp).toBeGreaterThan(300)
  })

  it('creates a building through worker construction', () => {
    const state = createInitialState('pvp')
    const worker = state.entities.find((entity) => entity.owner === 1 && entity.type === 'worker')
    expect(worker).toBeDefined()

    const result = startBuilding(state, worker!.id, 'barracks', { x: 560, y: 1120 }, 1)
    expect(result.ok).toBe(true)

    const site = state.entities.find((entity) => entity.owner === 1 && entity.type === 'barracks')
    expect(site).toBeDefined()
    expect(site!.underConstruction).toBe(true)
    expect(site!.hp).toBeLessThan(site!.maxHp)

    runFor(8, 0.05, () => updateSimulation(state, 0.05))
    expect(site!.hp).toBeGreaterThan(100)
    expect(site!.constructionProgress).toBeGreaterThan(0)

    runFor(18, 0.05, () => updateSimulation(state, 0.05))
    const barracks = state.entities.find((entity) => entity.owner === 1 && entity.type === 'barracks')
    expect(barracks).toBeDefined()
    expect(barracks!.underConstruction).toBe(false)
  })

  it('resolves victory when a command spire is destroyed', () => {
    const state = createInitialState('pvp')
    const enemyCommand = state.entities.find((entity) => entity.owner === 2 && entity.type === 'command')
    expect(enemyCommand).toBeDefined()

    getEntity(state, enemyCommand!.id)!.hp = 0
    updateSimulation(state, 0.05)

    expect(state.status).toBe('won')
    expect(state.winner).toBe(1)
  })

  it('applies explicit counter damage bonuses', () => {
    const state = createInitialState('pvp')
    const arbalest = createEntity(state, 'skirmisher', 1, { x: 900, y: 900 })
    const oathbound = createEntity(state, 'vanguard', 2, { x: 930, y: 900 })
    const pyre = createEntity(state, 'turret', 2, { x: 960, y: 900 })

    expect(calculateDamage(arbalest, oathbound)).toBeGreaterThan(arbalest.damage)
    expect(calculateDamage(pyre, arbalest)).toBeGreaterThan(pyre.damage)
  })

  it('hides distant enemies until scouted', () => {
    const state = createInitialState('pvp')
    const enemyCommand = state.entities.find((entity) => entity.owner === 2 && entity.type === 'command')
    expect(enemyCommand).toBeDefined()
    expect(isEntityVisibleTo(state, enemyCommand!, 1)).toBe(false)

    createEntity(state, 'worker', 1, { x: enemyCommand!.position.x - 80, y: enemyCommand!.position.y })

    expect(isEntityVisibleTo(state, enemyCommand!, 1)).toBe(true)
  })

  it('raises and lowers the ruin tide over time', () => {
    const state = createInitialState('pvp')
    updateSimulation(state, 0.05)
    const openingTide = state.ruinTide

    runFor(24, 0.05, () => updateSimulation(state, 0.05))

    expect(state.ruinTide).toBeGreaterThan(openingTide)
  })

  it('drops resolve near enemy terror and restores it near wards', () => {
    const state = createInitialState('pvp')
    const soldier = createEntity(state, 'vanguard', 1, { x: 980, y: 760 })
    createEntity(state, 'turret', 1, { x: 980, y: 818 })
    createEntity(state, 'turret', 2, { x: 1015, y: 760 })

    runFor(1, 0.05, () => updateSimulation(state, 0.05))
    const wardedResolve = soldier.resolve

    soldier.position = { x: 1015, y: 760 }
    state.entities = state.entities.filter((entity) => !(entity.owner === 1 && entity.type === 'turret'))
    runFor(1, 0.05, () => updateSimulation(state, 0.05))

    expect(soldier.resolve).toBeLessThan(wardedResolve)
    expect(resolveMultiplier(soldier)).toBeLessThan(1)
  })

  it('adds a survival beat before the final story assault objective', () => {
    const state = createInitialState('story')
    state.players[1].ore = 400
    updateSimulation(state, 0.05)
    expect(state.storyStep).toBe(1)

    createEntity(state, 'barracks', 1, { x: 620, y: 1120 })
    updateSimulation(state, 0.05)
    expect(state.storyStep).toBe(2)

    runFor(40, 0.05, () => updateSimulation(state, 0.05))

    expect(state.storyStep).toBe(3)
  })

  it('keeps story AI dormant until the first economy objective advances', () => {
    const state = createInitialState('story')
    const enemyOre = state.players[2].ore

    runFor(45, 0.05, () => updateSimulation(state, 0.05))

    expect(state.players[2].ore).toBe(enemyOre)
    expect(state.events.some((event) => event.text.includes('warband'))).toBe(false)

    state.players[1].ore = 400
    updateSimulation(state, 0.05)

    expect(state.storyStep).toBe(1)
  })

  it('uses map blockers for building placement and unit steering', () => {
    const state = createInitialState('pvp')
    const ruin = TERRAIN_BLOCKERS.find((blocker) => blocker.id === 'west-abbey-ruin')
    expect(ruin).toBeDefined()
    expect(canPlaceBuilding(state, ruin!.position, 36)).toBe(false)

    const scout = createEntity(state, 'worker', 1, { x: 600, y: 665 })
    issueMove(state, [scout.id], { x: 900, y: 665 }, 1)
    runFor(6, 0.05, () => updateSimulation(state, 0.05))

    expect(isTerrainBlocked(scout.position, scout.radius)).toBe(false)
    expect(scout.position.x).toBeGreaterThan(600)
  })

  it('completes tech research and safely upgrades existing troops', () => {
    const state = createInitialState('pvp')
    state.players[1].ore = 1000
    const command = state.entities.find((entity) => entity.owner === 1 && entity.type === 'command')!
    const soldier = state.entities.find((entity) => entity.owner === 1 && entity.type === 'vanguard')!
    const originalHp = soldier.maxHp
    const originalDamage = soldier.damage

    expect(startResearch(state, command.id, 'tier-two', 1).ok).toBe(true)
    runFor(17, 0.05, () => updateSimulation(state, 0.05))
    expect(state.players[1].techTier).toBe(2)

    const hall = createEntity(state, 'barracks', 1, { x: 600, y: 1240 })
    expect(startResearch(state, hall.id, 'tempered-oaths', 1).ok).toBe(true)
    runFor(14, 0.05, () => updateSimulation(state, 0.05))

    expect(soldier.maxHp).toBeGreaterThan(originalHp)
    expect(soldier.damage).toBeGreaterThan(originalDamage)
  })

  it('uses traveling projectiles for ranged attacks', () => {
    const state = createInitialState('pvp')
    state.entities = state.entities.filter((entity) => entity.type === 'command')
    const arbalest = createEntity(state, 'skirmisher', 1, { x: 500, y: 500 })
    const target = createEntity(state, 'vanguard', 2, { x: 620, y: 500 })
    const startingHp = target.hp

    expect(issueAttackMove(state, [arbalest.id], target.position, 1).ok).toBe(true)
    updateSimulation(state, 0.05)
    expect(state.projectiles.length).toBeGreaterThan(0)
    expect(target.hp).toBe(startingHp)

    runFor(0.5, 0.05, () => updateSimulation(state, 0.05))
    expect(target.hp).toBeLessThan(startingHp)
    expect(state.stats[1].damageDealt).toBeGreaterThan(0)
  })

  it('captures relics and turns them into strategic income', () => {
    const state = createInitialState('pvp')
    const relic = state.controlPoints[0]
    state.entities = state.entities.filter((entity) => entity.type === 'command')
    createEntity(state, 'vanguard', 1, { ...relic.position })
    const openingOre = state.players[1].ore

    runFor(7, 0.05, () => updateSimulation(state, 0.05))

    expect(relic.owner).toBe(1)
    expect(state.stats[1].objectivesCaptured).toBe(1)
    expect(state.players[1].ore).toBeGreaterThan(openingOre)
  })

  it('wins Lantern Vigil by holding through the survival timer', () => {
    const state = createInitialState('story', 'candlebound', 'hollow', { missionId: 'lantern-vigil' })
    state.entities = state.entities.filter((entity) => entity.owner === 1)

    runFor(151, 0.05, () => updateSimulation(state, 0.05))

    expect(state.status).toBe('won')
    expect(state.winner).toBe(1)
  })

  it('gives AI personalities distinct opening forces', () => {
    const aggressive = createInitialState('skirmish', 'candlebound', 'hollow', { aiPersonality: 'aggressive' })
    const economic = createInitialState('skirmish', 'candlebound', 'hollow', { aiPersonality: 'economic' })
    const fortress = createInitialState('skirmish', 'candlebound', 'hollow', { aiPersonality: 'fortress' })

    expect(aggressive.entities.filter((entity) => entity.owner === 2 && entity.type === 'vanguard')).toHaveLength(2)
    expect(economic.entities.filter((entity) => entity.owner === 2 && entity.type === 'worker')).toHaveLength(4)
    expect(fortress.entities.filter((entity) => entity.owner === 2 && entity.type === 'turret')).toHaveLength(2)
  })

  it('keeps hold-position troops from chasing beyond weapon reach', () => {
    const state = createInitialState('pvp')
    state.entities = state.entities.filter((entity) => entity.type === 'command')
    const guard = createEntity(state, 'vanguard', 1, { x: 500, y: 500 })
    createEntity(state, 'vanguard', 2, { x: 580, y: 500 })
    const opening = { ...guard.position }
    expect(setUnitStance(state, [guard.id], 'hold', 1).ok).toBe(true)

    runFor(2, 0.05, () => updateSimulation(state, 0.05))

    expect(guard.position).toEqual(opening)
  })
})
