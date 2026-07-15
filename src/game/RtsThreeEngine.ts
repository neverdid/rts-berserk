import * as THREE from 'three'
import { RACE_DEFS, RACE_POWER_DEFS, getEntityDef } from './catalog'
import { MAP_SIZE, STARTING_BASES, getMapDef, isInTerrainZone } from './map'
import { getStoryMission } from './story'
import { RESEARCH_DEFS, researchForRace } from './technology'
import {
  canPlaceBuilding,
  activateRacePower,
  createInitialState,
  distance,
  enemyOf,
  getEntity,
  getObjectiveText,
  getPlayerSupply,
  getProductionProfile,
  issueAttack,
  issueAttackMove,
  issueGather,
  issueMove,
  issueRetreat,
  isEntityVisibleTo,
  isPositionVisibleTo,
  setActivePlayer,
  setRallyPoint,
  setUnitStance,
  startBuilding,
  startProduction,
  startResearch,
  updateSimulation,
} from './simulation'
import type {
  AiPersonality,
  BuildingType,
  Entity,
  GameMode,
  GameState,
  MapId,
  MatchRuleId,
  MissionId,
  PlayerId,
  RaceId,
  ResearchId,
  ResourceNode,
  UnitStance,
  UnitType,
  Vec2,
} from './types'

type CommandMode = 'normal' | 'attack-move'

interface EntityVisual {
  root: THREE.Group
  model: THREE.Group
  selection: THREE.Mesh<THREE.RingGeometry, THREE.MeshBasicMaterial>
  ward: THREE.Mesh<THREE.RingGeometry, THREE.MeshBasicMaterial>
  health: THREE.Group
  healthFill: THREE.Mesh<THREE.PlaneGeometry, THREE.MeshBasicMaterial>
  lastPosition: Vec2
  lastCooldown: number
  lastHp: number
  hitUntil: number
}

interface ResourceVisual {
  root: THREE.Group
  crystals: THREE.Group
  glow: THREE.Mesh<THREE.RingGeometry, THREE.MeshBasicMaterial>
}

interface CommandPulse {
  mesh: THREE.Mesh<THREE.RingGeometry, THREE.MeshBasicMaterial>
  age: number
  duration: number
}

interface ControlPointVisual {
  ring: THREE.Mesh<THREE.RingGeometry, THREE.MeshBasicMaterial>
  beam: THREE.Mesh<THREE.CylinderGeometry, THREE.MeshBasicMaterial>
}

interface PointerStart {
  x: number
  y: number
  world: Vec2
}

export interface ThreeUiSnapshot {
  mode: GameMode
  activePlayer: PlayerId
  status: GameState['status']
  players: GameState['players']
  selected: Array<{
    id: string
    label: string
    type: string
    hp: number
    maxHp: number
    resolve: number
    armor: string
    counters: string[]
    underConstruction: boolean
    constructionProgress: number
    queue: Entity['queue']
    rallyPoint: Vec2 | null
    stance: UnitStance
  }>
  events: GameState['events']
  objective: string
  buildPreview: BuildingType | null
  ruinTide: number
  activeRace: RaceId
  races: Array<{ id: RaceId; name: string; tagline: string; style: string }>
  commands: {
    train: Record<UnitType, { label: string; cost: number; supply: number }>
    build: Record<BuildingType, { label: string; cost: number; supply: number }>
  }
  supply: { used: number; queued: number; cap: number }
  commandMode: CommandMode
  controlGroups: Array<{ slot: number; count: number }>
  paused: boolean
  missionStarted: boolean
  matchTime: number
  racePower: { label: string; description: string; cost: number; cooldown: number; maxCooldown: number }
  setupRaces: { player: RaceId; opponent: RaceId }
  setup: { mapId: MapId; missionId: MissionId; aiPersonality: AiPersonality; matchRule: MatchRuleId }
  mapName: string
  mission: ReturnType<typeof getStoryMission>
  research: Array<{
    id: ResearchId
    label: string
    description: string
    cost: number
    time: number
    producer: BuildingType
    available: boolean
    completed: boolean
    queued: boolean
  }>
  controlPoints: GameState['controlPoints']
  stats: GameState['stats']
}

type UiCommand =
  | { type: 'mode'; mode: GameMode }
  | { type: 'start' }
  | { type: 'race'; race: RaceId }
  | { type: 'opponent-race'; race: RaceId }
  | { type: 'map'; mapId: MapId }
  | { type: 'mission'; missionId: MissionId }
  | { type: 'ai'; personality: AiPersonality }
  | { type: 'rule'; rule: MatchRuleId }
  | { type: 'restart' }
  | { type: 'train'; unitType: UnitType }
  | { type: 'build'; buildingType: BuildingType }
  | { type: 'cancel-build' }
  | { type: 'focus-home' }
  | { type: 'attack-move' }
  | { type: 'group'; slot: number }
  | { type: 'race-power' }
  | { type: 'stance'; stance: UnitStance }
  | { type: 'retreat' }
  | { type: 'research'; researchId: ResearchId }
  | { type: 'pause' }

const PLAYER_ONE: PlayerId = 1
const PLAYER_TWO: PlayerId = 2

export class RtsThreeEngine {
  private readonly container: HTMLElement
  private readonly renderer: THREE.WebGLRenderer
  private readonly scene = new THREE.Scene()
  private readonly camera = new THREE.PerspectiveCamera(46, 1, 8, 5200)
  private readonly timer = new THREE.Timer()
  private readonly raycaster = new THREE.Raycaster()
  private readonly pointerNdc = new THREE.Vector2()
  private readonly cameraTarget = new THREE.Vector3(520, 0, 980)
  private readonly worldRoot = new THREE.Group()
  private readonly terrainRoot = new THREE.Group()
  private readonly dynamicRoot = new THREE.Group()
  private readonly effectRoot = new THREE.Group()
  private readonly entityVisuals = new Map<string, EntityVisual>()
  private readonly resourceVisuals = new Map<string, ResourceVisual>()
  private readonly controlGroups = new Map<number, string[]>()
  private readonly heldKeys = new Set<string>()
  private readonly commandPulses: CommandPulse[] = []
  private readonly waterMeshes: Array<THREE.Mesh<THREE.PlaneGeometry, THREE.MeshPhysicalMaterial>> = []
  private readonly projectileVisuals = new Map<string, THREE.Mesh<THREE.SphereGeometry, THREE.MeshBasicMaterial>>()
  private readonly controlPointVisuals = new Map<string, ControlPointVisual>()
  private readonly selectionBox: HTMLDivElement
  private readonly minimap: HTMLCanvasElement
  private readonly minimapContext: CanvasRenderingContext2D
  private readonly resizeObserver: ResizeObserver
  private readonly ground: THREE.Mesh<THREE.PlaneGeometry, THREE.MeshStandardMaterial>
  private readonly fogCanvas = document.createElement('canvas')
  private readonly fogTexture: THREE.CanvasTexture
  private readonly fogPlane: THREE.Mesh<THREE.PlaneGeometry, THREE.MeshBasicMaterial>
  private readonly rain: THREE.Points<THREE.BufferGeometry, THREE.PointsMaterial>

  private state: GameState = createInitialState('story')
  private selectedIds: string[] = []
  private selectedRace: RaceId = 'compact'
  private selectedOpponentRace: RaceId = 'ascendancy'
  private selectedMap: MapId = 'black-iron-ford'
  private selectedMission: MissionId = 'bridge-of-names'
  private selectedAi: AiPersonality = 'aggressive'
  private selectedRule: MatchRuleId = 'standard'
  private missionStarted = false
  private paused = false
  private commandMode: CommandMode = 'normal'
  private buildPreview: BuildingType | null = null
  private placementGhost: THREE.Group | null = null
  private pointerStart: PointerStart | null = null
  private pointerWorld: Vec2 | null = null
  private middleDragging = false
  private cameraDistance = 740
  private lastSnapshot = 0
  private lastFogUpdate = 0
  private lastGroupTap = { slot: 0, time: 0 }
  private simulationAccumulator = 0
  private frameId = 0
  private destroyed = false

  constructor(container: HTMLElement) {
    this.container = container
    this.container.classList.add('three-stage')
    this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.8))
    this.renderer.shadowMap.enabled = true
    this.renderer.shadowMap.type = THREE.PCFShadowMap
    this.renderer.outputColorSpace = THREE.SRGBColorSpace
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping
    this.renderer.toneMappingExposure = 1.42
    this.renderer.domElement.className = 'three-canvas'
    this.renderer.domElement.setAttribute('aria-label', 'Vowfall 3D battlefield')
    this.renderer.domElement.tabIndex = 0
    this.container.append(this.renderer.domElement)
    this.timer.connect(document)

    this.selectionBox = document.createElement('div')
    this.selectionBox.className = 'selection-box'
    this.container.append(this.selectionBox)

    this.minimap = document.createElement('canvas')
    this.minimap.className = 'minimap'
    this.minimap.width = 240
    this.minimap.height = 150
    this.minimap.setAttribute('aria-label', 'Tactical minimap')
    this.container.append(this.minimap)
    const minimapContext = this.minimap.getContext('2d')
    if (!minimapContext) {
      throw new Error('Unable to create minimap context')
    }
    this.minimapContext = minimapContext

    this.scene.background = new THREE.Color(0x0b1512)
    this.scene.fog = new THREE.FogExp2(0x101a17, 0.00044)
    this.camera.up.set(0, 1, 0)
    this.scene.add(this.worldRoot, this.dynamicRoot, this.effectRoot)
    this.addLighting()

    this.ground = this.createGround()
    this.worldRoot.add(this.ground, this.terrainRoot)
    this.createMapGeometry()
    this.fogTexture = this.createFogTexture()
    this.fogPlane = this.createFogPlane()
    this.worldRoot.add(this.fogPlane)
    this.rain = this.createRain()
    this.effectRoot.add(this.rain)
    this.createInput()
    this.listenForUi()

    this.resizeObserver = new ResizeObserver(() => this.resize())
    this.resizeObserver.observe(this.container)
    this.resize()
    this.frameHome(true)
    this.publishSnapshot()
    this.animate()
  }

  destroy(): void {
    this.destroyed = true
    cancelAnimationFrame(this.frameId)
    this.resizeObserver.disconnect()
    this.timer.dispose()
    this.renderer.dispose()
    this.container.replaceChildren()
  }

  private addLighting(): void {
    const hemisphere = new THREE.HemisphereLight(0xb4c5ba, 0x17120e, 2.08)
    this.scene.add(hemisphere)

    const moon = new THREE.DirectionalLight(0xb8c8c2, 3.2)
    moon.position.set(420, 980, 280)
    moon.castShadow = true
    moon.shadow.mapSize.set(2048, 2048)
    moon.shadow.camera.left = -1100
    moon.shadow.camera.right = 1100
    moon.shadow.camera.top = 850
    moon.shadow.camera.bottom = -850
    moon.shadow.camera.near = 80
    moon.shadow.camera.far = 2400
    moon.shadow.bias = -0.0004
    this.scene.add(moon)

    const woundLight = new THREE.DirectionalLight(0x9a3137, 1.2)
    woundLight.position.set(1900, 520, 160)
    this.scene.add(woundLight)

    const homeLight = new THREE.PointLight(0xe1b55c, 860, 430, 1.55)
    homeLight.position.set(STARTING_BASES[PLAYER_ONE].origin.x, 92, STARTING_BASES[PLAYER_ONE].origin.y)
    this.scene.add(homeLight)

    const enemyLight = new THREE.PointLight(0xa5363e, 700, 390, 1.7)
    enemyLight.position.set(STARTING_BASES[PLAYER_TWO].origin.x, 80, STARTING_BASES[PLAYER_TWO].origin.y)
    this.scene.add(enemyLight)
  }

  private createGround(): THREE.Mesh<THREE.PlaneGeometry, THREE.MeshStandardMaterial> {
    const geometry = new THREE.PlaneGeometry(MAP_SIZE.x, MAP_SIZE.y, 48, 34)
    const positions = geometry.attributes.position
    for (let index = 0; index < positions.count; index += 1) {
      const x = positions.getX(index)
      const y = positions.getY(index)
      const edge = Math.min(MAP_SIZE.x / 2 - Math.abs(x), MAP_SIZE.y / 2 - Math.abs(y))
      const undulation = Math.sin(x * 0.012) * 5 + Math.cos(y * 0.016) * 4 + Math.sin((x + y) * 0.006) * 3
      positions.setZ(index, undulation - Math.max(0, 75 - edge) * 0.12)
    }
    geometry.computeVertexNormals()
    const groundTexture = this.createGroundTexture()
    const material = new THREE.MeshStandardMaterial({
      color: 0x46564b,
      map: groundTexture,
      bumpMap: groundTexture,
      bumpScale: 2.8,
      roughness: 0.96,
      metalness: 0.02,
    })
    const ground = new THREE.Mesh(geometry, material)
    ground.rotation.x = -Math.PI / 2
    ground.position.set(MAP_SIZE.x / 2, -4, MAP_SIZE.y / 2)
    ground.receiveShadow = true
    ground.userData.ground = true
    return ground
  }

  private createGroundTexture(): THREE.CanvasTexture {
    const canvas = document.createElement('canvas')
    canvas.width = 512
    canvas.height = 512
    const context = canvas.getContext('2d')
    if (!context) {
      throw new Error('Unable to create terrain texture')
    }
    context.fillStyle = '#a8afa9'
    context.fillRect(0, 0, canvas.width, canvas.height)
    for (let index = 0; index < 2600; index += 1) {
      const x = (index * 193) % canvas.width
      const y = (index * 317) % canvas.height
      const shade = 82 + ((index * 47) % 86)
      context.fillStyle = `rgba(${shade}, ${shade + 7}, ${shade}, ${0.05 + (index % 5) * 0.018})`
      const radius = 0.4 + (index % 4) * 0.45
      context.beginPath()
      context.arc(x, y, radius, 0, Math.PI * 2)
      context.fill()
    }
    context.strokeStyle = 'rgba(42, 48, 43, .16)'
    context.lineWidth = 1
    for (let index = 0; index < 36; index += 1) {
      const x = (index * 137) % 512
      const y = (index * 223) % 512
      context.beginPath()
      context.moveTo(x, y)
      context.lineTo((x + 18 + (index % 5) * 8) % 512, (y + 9 + (index % 4) * 7) % 512)
      context.stroke()
    }
    const texture = new THREE.CanvasTexture(canvas)
    texture.wrapS = THREE.RepeatWrapping
    texture.wrapT = THREE.RepeatWrapping
    texture.repeat.set(7, 5)
    texture.colorSpace = THREE.SRGBColorSpace
    texture.anisotropy = Math.min(8, this.renderer.capabilities.getMaxAnisotropy())
    return texture
  }

  private createMapGeometry(): void {
    this.terrainRoot.children.forEach((child) => this.disposeObject(child))
    this.terrainRoot.clear()
    this.controlPointVisuals.clear()
    this.waterMeshes.length = 0
    const map = getMapDef(this.state.mapId)
    const roadMaterial = new THREE.MeshStandardMaterial({ color: 0x514d42, roughness: 1, metalness: 0 })
    const narrowRoadMaterial = new THREE.MeshStandardMaterial({ color: 0x403f37, roughness: 1, metalness: 0 })
    map.lanes.forEach((lane, index) => this.addRoad(lane, index === 0 ? 92 : 62, index === 0 ? roadMaterial : narrowRoadMaterial))

    map.zones.forEach((zone) => {
      const size = zone.size ?? { x: (zone.radius ?? 100) * 2, y: (zone.radius ?? 100) * 2 }
      const geometry =
        zone.shape === 'circle'
          ? new THREE.CylinderGeometry(zone.radius ?? 100, zone.radius ?? 100, zone.kind === 'high-ground' ? 8 : 3, 52)
          : new THREE.BoxGeometry(size.x, zone.kind === 'high-ground' ? 8 : 3, size.y)
      const colors = { 'high-ground': 0x39483f, forest: 0x1c2c24, ford: 0x172724, cursed: 0x32171d }
      const patch = new THREE.Mesh(
        geometry,
        new THREE.MeshStandardMaterial({
          color: colors[zone.kind],
          roughness: zone.kind === 'ford' ? 0.3 : 0.92,
          metalness: zone.kind === 'ford' ? 0.25 : 0.02,
          transparent: zone.kind === 'cursed',
          opacity: zone.kind === 'cursed' ? 0.62 : 1,
        }),
      )
      patch.position.set(zone.position.x, zone.kind === 'high-ground' ? -1 : -2.2, zone.position.y)
      patch.receiveShadow = true
      this.terrainRoot.add(patch)
      if (zone.kind === 'ford') {
        const water = new THREE.Mesh(
          new THREE.PlaneGeometry(size.x * 0.98, size.y * 0.92, 28, 10),
          new THREE.MeshPhysicalMaterial({
            color: 0x24575c,
            emissive: 0x0a2729,
            emissiveIntensity: 0.48,
            roughness: 0.22,
            metalness: 0.16,
            clearcoat: 0.72,
            clearcoatRoughness: 0.2,
            transparent: true,
            opacity: 0.72,
            depthWrite: false,
          }),
        )
        water.rotation.x = -Math.PI / 2
        water.position.set(zone.position.x, 0.7, zone.position.y)
        water.receiveShadow = true
        this.terrainRoot.add(water)
        this.waterMeshes.push(water)
      }
    })

    this.addBaseDais(map.startingBases[PLAYER_ONE].origin, this.state.players[PLAYER_ONE].accent)
    this.addBaseDais(map.startingBases[PLAYER_TWO].origin, this.state.players[PLAYER_TWO].accent)
    map.blockers.forEach((blocker, index) => {
      if (blocker.kind === 'circle') {
        const radius = blocker.radius ?? 60
        const rock = new THREE.Mesh(
          new THREE.DodecahedronGeometry(radius, 1),
          new THREE.MeshStandardMaterial({ color: blocker.color, roughness: 0.88, metalness: 0.08 }),
        )
        rock.scale.y = 0.32 + (index % 3) * 0.07
        rock.position.set(blocker.position.x, radius * rock.scale.y * 0.55, blocker.position.y)
        rock.rotation.set(index * 0.7, index * 0.46, index * 0.21)
        rock.castShadow = true
        rock.receiveShadow = true
        this.terrainRoot.add(rock)
        this.addRuinColumns(blocker.position, radius, blocker.accent, 3 + (index % 3))
      } else {
        const size = blocker.size ?? { x: 180, y: 90 }
        const ruin = new THREE.Mesh(
          new THREE.BoxGeometry(size.x, 24, size.y),
          new THREE.MeshStandardMaterial({ color: blocker.color, roughness: 0.9, metalness: 0.04 }),
        )
        ruin.position.set(blocker.position.x, 10, blocker.position.y)
        ruin.castShadow = true
        ruin.receiveShadow = true
        this.terrainRoot.add(ruin)
        this.addRuinColumns(blocker.position, Math.min(size.x, size.y), blocker.accent, 5)
      }
    })

    map.controlPoints.forEach((point) => {
      const plinth = new THREE.Mesh(
        new THREE.CylinderGeometry(28, 36, 10, 10),
        new THREE.MeshStandardMaterial({ color: 0x4e5149, roughness: 0.8, metalness: 0.16 }),
      )
      plinth.position.set(point.position.x, 5, point.position.y)
      plinth.castShadow = true
      this.terrainRoot.add(plinth)
      const obelisk = new THREE.Mesh(
        new THREE.ConeGeometry(12, 54, 5),
        new THREE.MeshStandardMaterial({ color: 0x9b8e75, roughness: 0.5, metalness: 0.32 }),
      )
      obelisk.position.set(point.position.x, 36, point.position.y)
      obelisk.castShadow = true
      this.terrainRoot.add(obelisk)
      const ring = new THREE.Mesh(
        new THREE.RingGeometry(point.radius - 4, point.radius, 56),
        new THREE.MeshBasicMaterial({ color: 0xb9aa85, transparent: true, opacity: 0.42, side: THREE.DoubleSide }),
      )
      ring.rotation.x = -Math.PI / 2
      ring.position.set(point.position.x, 3, point.position.y)
      this.terrainRoot.add(ring)
      const beam = new THREE.Mesh(
        new THREE.CylinderGeometry(2, 8, 66, 10, 1, true),
        new THREE.MeshBasicMaterial({ color: 0xb9aa85, transparent: true, opacity: 0.12, side: THREE.DoubleSide }),
      )
      beam.position.set(point.position.x, 52, point.position.y)
      this.terrainRoot.add(beam)
      this.controlPointVisuals.set(point.id, { ring, beam })
    })
    if (map.zones.some((zone) => zone.kind === 'ford')) {
      this.addFordBridge({ x: 948, y: 836 }, { x: 1292, y: 690 })
    }
    this.addGroundCover()
    this.addForestCanopies()
    this.addDeadForest()
  }

  private addRoad(points: Vec2[], width: number, material: THREE.Material): void {
    for (let index = 1; index < points.length; index += 1) {
      const start = points[index - 1]
      const end = points[index]
      const length = distance(start, end)
      const segment = new THREE.Mesh(new THREE.BoxGeometry(length, 2.5, width), material)
      segment.position.set((start.x + end.x) / 2, -0.4, (start.y + end.y) / 2)
      segment.rotation.y = -Math.atan2(end.y - start.y, end.x - start.x)
      segment.receiveShadow = true
      this.terrainRoot.add(segment)
    }
  }

  private addBaseDais(position: Vec2, accent: number): void {
    const stone = new THREE.Mesh(
      new THREE.CylinderGeometry(220, 245, 10, 48),
      new THREE.MeshStandardMaterial({ color: 0x202b26, roughness: 0.86, metalness: 0.05 }),
    )
    stone.position.set(position.x, 1, position.y)
    stone.receiveShadow = true
    this.terrainRoot.add(stone)
    const ring = new THREE.Mesh(
      new THREE.RingGeometry(178, 184, 64),
      new THREE.MeshBasicMaterial({ color: accent, transparent: true, opacity: 0.28, side: THREE.DoubleSide }),
    )
    ring.rotation.x = -Math.PI / 2
    ring.position.set(position.x, 7, position.y)
    this.terrainRoot.add(ring)
  }

  private addRuinColumns(position: Vec2, radius: number, accent: number, count: number): void {
    const material = new THREE.MeshStandardMaterial({ color: accent, roughness: 0.88, metalness: 0.02 })
    for (let index = 0; index < count; index += 1) {
      const angle = (Math.PI * 2 * index) / count + count * 0.17
      const height = 28 + ((index * 19 + count * 7) % 48)
      const column = new THREE.Mesh(new THREE.CylinderGeometry(6, 8, height, 7), material)
      column.position.set(
        position.x + Math.cos(angle) * radius * 0.54,
        height / 2 + 4,
        position.y + Math.sin(angle) * radius * 0.46,
      )
      column.rotation.z = (index % 2 === 0 ? 1 : -1) * 0.06
      column.castShadow = true
      column.receiveShadow = true
      this.terrainRoot.add(column)
    }
  }

  private addDeadForest(): void {
    const trunkMaterial = new THREE.MeshStandardMaterial({ color: 0x1b1815, roughness: 1 })
    for (let index = 0; index < 52; index += 1) {
      const x = 70 + ((index * 367) % 2070)
      const z = 85 + ((index * 521) % 1320)
      if ((x > 150 && x < 590 && z > 860 && z < 1370) || (x > 1490 && x < 2050 && z > 120 && z < 670)) {
        continue
      }
      const height = 30 + (index % 6) * 9
      const trunk = new THREE.Mesh(new THREE.CylinderGeometry(1.8, 3.6, height, 5), trunkMaterial)
      trunk.position.set(x, height / 2, z)
      trunk.rotation.z = ((index % 5) - 2) * 0.035
      trunk.castShadow = true
      this.terrainRoot.add(trunk)
      const branch = new THREE.Mesh(new THREE.CylinderGeometry(0.8, 1.5, height * 0.48, 5), trunkMaterial)
      branch.position.set(x + 6, height * 0.72, z)
      branch.rotation.z = -0.72
      branch.castShadow = true
      this.terrainRoot.add(branch)
    }
  }

  private addFordBridge(start: Vec2, end: Vec2): void {
    const plankMaterial = new THREE.MeshStandardMaterial({ color: 0x4e4032, roughness: 0.94, metalness: 0.01 })
    const railMaterial = new THREE.MeshStandardMaterial({ color: 0x25231f, roughness: 0.88, metalness: 0.08 })
    const length = distance(start, end)
    const heading = -Math.atan2(end.y - start.y, end.x - start.x)
    const planks = 24
    for (let index = 0; index < planks; index += 1) {
      const ratio = (index + 0.5) / planks
      const plank = new THREE.Mesh(new THREE.BoxGeometry(length / planks * 0.94, 5, 52), plankMaterial)
      plank.position.set(
        THREE.MathUtils.lerp(start.x, end.x, ratio),
        4 + Math.sin(ratio * Math.PI) * 2.4,
        THREE.MathUtils.lerp(start.y, end.y, ratio),
      )
      plank.rotation.y = heading
      plank.rotation.z = Math.sin(index * 1.9) * 0.012
      plank.castShadow = true
      plank.receiveShadow = true
      this.terrainRoot.add(plank)
    }
    const center = { x: (start.x + end.x) / 2, y: (start.y + end.y) / 2 }
    ;[-31, 31].forEach((side) => {
      const rail = new THREE.Mesh(new THREE.BoxGeometry(length, 5, 5), railMaterial)
      rail.position.set(
        center.x + Math.sin(heading) * side,
        12,
        center.y + Math.cos(heading) * side,
      )
      rail.rotation.y = heading
      rail.castShadow = true
      this.terrainRoot.add(rail)
    })
  }

  private addGroundCover(): void {
    const map = getMapDef(this.state.mapId)
    const grass = new THREE.InstancedMesh(
      new THREE.ConeGeometry(1.2, 9, 3),
      new THREE.MeshStandardMaterial({ color: 0x52634d, roughness: 1, side: THREE.DoubleSide }),
      420,
    )
    const dummy = new THREE.Object3D()
    let used = 0
    for (let index = 0; index < 520 && used < 420; index += 1) {
      const position = {
        x: 42 + ((index * 379 + (index % 7) * 71) % (this.state.mapSize.x - 84)),
        y: 42 + ((index * 613 + (index % 11) * 43) % (this.state.mapSize.y - 84)),
      }
      const nearBase = Object.values(map.startingBases).some((base) => distance(position, base.origin) < 245)
      const inHarshZone = map.zones.some(
        (zone) => (zone.kind === 'ford' || zone.kind === 'cursed') && isInTerrainZone(position, zone),
      )
      const nearRoad = map.lanes.some((lane) =>
        lane.slice(1).some((point, laneIndex) => pointToSegmentDistance(position, lane[laneIndex], point) < 54),
      )
      if (nearBase || inHarshZone || nearRoad) {
        continue
      }
      dummy.position.set(position.x, 4, position.y)
      dummy.rotation.set(0, index * 1.73, ((index % 5) - 2) * 0.025)
      const scale = 0.58 + (index % 7) * 0.08
      dummy.scale.set(scale, scale, scale)
      dummy.updateMatrix()
      grass.setMatrixAt(used, dummy.matrix)
      used += 1
    }
    grass.count = used
    grass.instanceMatrix.needsUpdate = true
    grass.receiveShadow = true
    this.terrainRoot.add(grass)
  }

  private addForestCanopies(): void {
    const forestZones = getMapDef(this.state.mapId).zones.filter((zone) => zone.kind === 'forest')
    const capacity = forestZones.length * 28
    if (capacity === 0) {
      return
    }
    const trunks = new THREE.InstancedMesh(
      new THREE.CylinderGeometry(2.4, 4.2, 30, 6),
      new THREE.MeshStandardMaterial({ color: 0x29251f, roughness: 1 }),
      capacity,
    )
    const crowns = new THREE.InstancedMesh(
      new THREE.ConeGeometry(15, 38, 7),
      new THREE.MeshStandardMaterial({ color: 0x20372a, roughness: 0.96 }),
      capacity,
    )
    const dummy = new THREE.Object3D()
    let used = 0
    forestZones.forEach((zone, zoneIndex) => {
      const radius = zone.radius ?? Math.min(zone.size?.x ?? 200, zone.size?.y ?? 200) / 2
      for (let index = 0; index < 28; index += 1) {
        const angle = index * 2.399 + zoneIndex * 0.91
        const radial = radius * (0.2 + ((index * 37) % 73) / 100)
        const x = zone.position.x + Math.cos(angle) * radial
        const z = zone.position.y + Math.sin(angle) * radial
        const scale = 0.7 + (index % 6) * 0.09
        dummy.position.set(x, 15 * scale, z)
        dummy.rotation.set(0, angle, 0)
        dummy.scale.set(scale, scale, scale)
        dummy.updateMatrix()
        trunks.setMatrixAt(used, dummy.matrix)
        dummy.position.y = 39 * scale
        dummy.rotation.y = angle + 0.4
        dummy.updateMatrix()
        crowns.setMatrixAt(used, dummy.matrix)
        used += 1
      }
    })
    trunks.count = used
    crowns.count = used
    trunks.instanceMatrix.needsUpdate = true
    crowns.instanceMatrix.needsUpdate = true
    trunks.castShadow = true
    crowns.castShadow = true
    this.terrainRoot.add(trunks, crowns)
  }

  private createFogTexture(): THREE.CanvasTexture {
    this.fogCanvas.width = 128
    this.fogCanvas.height = 88
    const texture = new THREE.CanvasTexture(this.fogCanvas)
    texture.colorSpace = THREE.SRGBColorSpace
    texture.minFilter = THREE.LinearFilter
    texture.magFilter = THREE.LinearFilter
    return texture
  }

  private createFogPlane(): THREE.Mesh<THREE.PlaneGeometry, THREE.MeshBasicMaterial> {
    const material = new THREE.MeshBasicMaterial({
      map: this.fogTexture,
      transparent: true,
      opacity: 0.7,
      depthWrite: false,
      side: THREE.DoubleSide,
    })
    const plane = new THREE.Mesh(new THREE.PlaneGeometry(MAP_SIZE.x, MAP_SIZE.y), material)
    plane.rotation.x = -Math.PI / 2
    plane.position.set(MAP_SIZE.x / 2, 2.2, MAP_SIZE.y / 2)
    plane.renderOrder = 2
    return plane
  }

  private createRain(): THREE.Points<THREE.BufferGeometry, THREE.PointsMaterial> {
    const count = 520
    const positions = new Float32Array(count * 3)
    for (let index = 0; index < count; index += 1) {
      positions[index * 3] = (index * 977) % MAP_SIZE.x
      positions[index * 3 + 1] = 70 + ((index * 613) % 780)
      positions[index * 3 + 2] = (index * 431) % MAP_SIZE.y
    }
    const geometry = new THREE.BufferGeometry()
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3))
    const material = new THREE.PointsMaterial({
      color: 0xb7c8c1,
      size: 2.2,
      transparent: true,
      opacity: 0.24,
      depthWrite: false,
    })
    return new THREE.Points(geometry, material)
  }

  private renderEntities(time: number): void {
    const liveIds = new Set(this.state.entities.map((entity) => entity.id))
    this.entityVisuals.forEach((visual, id) => {
      if (!liveIds.has(id)) {
        this.disposeObject(visual.root)
        this.dynamicRoot.remove(visual.root)
        this.entityVisuals.delete(id)
      }
    })

    this.state.entities.forEach((entity) => {
      let visual = this.entityVisuals.get(entity.id)
      if (!visual) {
        visual = this.createEntityVisual(entity)
        this.entityVisuals.set(entity.id, visual)
        this.dynamicRoot.add(visual.root)
      }

      const visible = isEntityVisibleTo(this.state, entity, this.state.activePlayer)
      visual.root.visible = visible
      if (!visible) {
        return
      }

      const moved = distance(visual.lastPosition, entity.position) > 0.35
      if (entity.hp < visual.lastHp - 0.01) {
        visual.hitUntil = time + 145
        this.pulseImpact(entity.position, this.state.players[entity.owner].accent)
      }
      const bob = entity.kind === 'unit' && moved ? Math.sin(time * 0.012 + entity.position.x * 0.02) * 1.25 : 0
      visual.root.position.set(entity.position.x, entity.underConstruction ? -5 : bob, entity.position.y)
      const modelScale = entity.kind === 'unit' ? 1.14 : 1
      visual.model.scale.set(
        modelScale,
        modelScale * (entity.underConstruction ? Math.max(0.28, entity.constructionProgress) : 1),
        modelScale,
      )
      if (moved && entity.kind === 'unit') {
        const dx = entity.position.x - visual.lastPosition.x
        const dz = entity.position.y - visual.lastPosition.y
        const targetHeading = Math.atan2(dx, dz)
        visual.model.rotation.y = lerpAngle(visual.model.rotation.y, targetHeading, 0.2)
      }
      const attackMotion =
        entity.kind === 'unit' && entity.attackCooldown > 0 && entity.cooldown > entity.attackCooldown * 0.72
      const attackOffset = entity.type === 'skirmisher' ? -entity.radius * 0.11 : entity.radius * 0.16
      visual.model.position.z = THREE.MathUtils.lerp(visual.model.position.z, attackMotion ? attackOffset : 0, 0.28)
      visual.model.traverse((object) => {
        if (!(object instanceof THREE.Mesh)) {
          return
        }
        const materials = Array.isArray(object.material) ? object.material : [object.material]
        materials.forEach((material) => {
          if ('opacity' in material) {
            material.transparent = entity.underConstruction
            material.opacity = entity.underConstruction ? 0.54 : 1
          }
          if (material instanceof THREE.MeshStandardMaterial) {
            const hit = time < visual.hitUntil
            material.emissive.setHex(hit ? 0xffd7ad : 0x000000)
            material.emissiveIntensity = hit ? 1.65 : 0
          }
        })
      })

      const selected = this.selectedIds.includes(entity.id)
      visual.selection.visible = selected
      visual.selection.material.opacity = selected ? 0.92 : 0
      visual.selection.scale.setScalar(selected ? 1 + Math.sin(time * 0.0045 + entity.position.x) * 0.025 : 1)
      visual.ward.visible = entity.ward > 0 && (selected || entity.kind === 'building')
      visual.ward.material.opacity = selected ? 0.34 : 0.1
      visual.health.visible = selected || entity.hp < entity.maxHp
      visual.health.position.y = entity.kind === 'building' ? entity.radius * 2.05 : entity.radius * 2.25
      visual.health.quaternion.copy(this.camera.quaternion)
      const hpRatio = THREE.MathUtils.clamp(entity.hp / entity.maxHp, 0, 1)
      visual.healthFill.scale.x = Math.max(0.001, hpRatio)
      visual.healthFill.position.x = (hpRatio - 1) * 20
      visual.healthFill.material.color.set(entity.resolve < 55 ? 0xd9af57 : hpRatio < 0.34 ? 0xc84a4f : 0x8ecf77)

      visual.lastCooldown = entity.cooldown
      visual.lastHp = entity.hp
      visual.lastPosition = { ...entity.position }
    })
  }

  private createEntityVisual(entity: Entity): EntityVisual {
    const root = new THREE.Group()
    root.userData.entityId = entity.id
    const model = this.createEntityModel(entity)
    root.add(model)

    const player = this.state.players[entity.owner]
    const selection = new THREE.Mesh(
      new THREE.RingGeometry(entity.radius * 1.15, entity.radius * 1.34, 40),
      new THREE.MeshBasicMaterial({ color: player.accent, transparent: true, opacity: 0, side: THREE.DoubleSide }),
    )
    selection.rotation.x = -Math.PI / 2
    selection.position.y = 2.6
    selection.visible = false
    root.add(selection)

    const ward = new THREE.Mesh(
      new THREE.RingGeometry(entity.radius * 1.62, entity.radius * 1.69, 48),
      new THREE.MeshBasicMaterial({ color: player.accent, transparent: true, opacity: 0.12, side: THREE.DoubleSide }),
    )
    ward.rotation.x = -Math.PI / 2
    ward.position.y = 1.7
    ward.visible = entity.ward > 0
    root.add(ward)

    const health = new THREE.Group()
    const healthBack = new THREE.Mesh(
      new THREE.PlaneGeometry(44, 5.4),
      new THREE.MeshBasicMaterial({ color: 0x050706, transparent: true, opacity: 0.9, depthTest: false }),
    )
    const healthFill = new THREE.Mesh(
      new THREE.PlaneGeometry(40, 3),
      new THREE.MeshBasicMaterial({ color: 0x8ecf77, depthTest: false }),
    )
    healthFill.position.z = 0.2
    health.add(healthBack, healthFill)
    health.visible = false
    root.add(health)

    root.traverse((object) => {
      object.userData.entityId = entity.id
    })
    return {
      root,
      model,
      selection,
      ward,
      health,
      healthFill,
      lastPosition: { ...entity.position },
      lastCooldown: entity.cooldown,
      lastHp: entity.hp,
      hitUntil: 0,
    }
  }

  private createEntityModel(entity: Entity): THREE.Group {
    const group = new THREE.Group()
    const player = this.state.players[entity.owner]
    const race = RACE_DEFS[player.race]
    const primary = new THREE.MeshStandardMaterial({ color: player.color, roughness: 0.68, metalness: 0.2 })
    const accent = new THREE.MeshStandardMaterial({ color: player.accent, roughness: 0.54, metalness: 0.34 })
    const dark = new THREE.MeshStandardMaterial({ color: 0x1b2420, roughness: 0.86, metalness: 0.1 })
    const stone = new THREE.MeshStandardMaterial({ color: 0x465149, roughness: 0.9, metalness: 0.04 })
    const flesh = new THREE.MeshStandardMaterial({ color: 0xb9977f, roughness: 0.92, metalness: 0 })
    const porcelain = new THREE.MeshStandardMaterial({ color: 0xd8cec0, roughness: 0.38, metalness: 0.08 })
    const r = entity.radius

    const add = (geometry: THREE.BufferGeometry, material: THREE.Material, position: [number, number, number]) => {
      const mesh = new THREE.Mesh(geometry, material)
      mesh.position.set(...position)
      mesh.castShadow = true
      mesh.receiveShadow = true
      group.add(mesh)
      return mesh
    }

    if (entity.type === 'worker') {
      add(new THREE.CylinderGeometry(r * 0.42, r * 0.58, r * 1.24, 7), primary, [0, r * 0.68, 0])
      add(new THREE.SphereGeometry(r * 0.31, 12, 8), accent, [0, r * 1.46, 0])
      const tool = add(new THREE.BoxGeometry(r * 0.12, r * 1.5, r * 0.1), dark, [r * 0.66, r * 0.86, 0])
      tool.rotation.z = -0.36
      const blade = add(new THREE.BoxGeometry(r * 0.62, r * 0.13, r * 0.12), accent, [r * 0.86, r * 1.49, 0])
      blade.rotation.z = -0.36
    } else if (entity.type === 'vanguard') {
      add(new THREE.CylinderGeometry(r * 0.46, r * 0.62, r * 1.46, 8), primary, [0, r * 0.8, 0])
      add(new THREE.SphereGeometry(r * 0.35, 12, 9), accent, [0, r * 1.72, 0])
      add(new THREE.BoxGeometry(r * 0.18, r * 1.52, r * 0.15), dark, [r * 0.72, r * 1.12, 0])
      const shield = add(new THREE.CylinderGeometry(r * 0.56, r * 0.56, r * 0.14, 8), accent, [-r * 0.62, r * 0.88, 0])
      shield.rotation.z = Math.PI / 2
    } else if (entity.type === 'skirmisher') {
      add(new THREE.ConeGeometry(r * 0.68, r * 1.72, 7), primary, [0, r * 0.86, 0])
      add(new THREE.SphereGeometry(r * 0.29, 12, 8), accent, [0, r * 1.74, 0])
      const bow = add(new THREE.BoxGeometry(r * 1.75, r * 0.13, r * 0.16), dark, [0, r * 1.12, r * 0.46])
      bow.rotation.y = 0.08
      add(new THREE.BoxGeometry(r * 0.1, r * 0.1, r * 1.1), accent, [0, r * 1.12, r * 0.78])
    } else if (entity.type === 'command') {
      add(new THREE.CylinderGeometry(r * 0.9, r, r * 0.42, 10), stone, [0, r * 0.21, 0])
      add(new THREE.BoxGeometry(r * 0.96, r * 1.45, r * 0.96), primary, [0, r * 0.98, 0])
      ;[-1, 1].forEach((sx) =>
        [-1, 1].forEach((sz) => {
          add(new THREE.CylinderGeometry(r * 0.19, r * 0.23, r * 1.62, 8), stone, [sx * r * 0.57, r, sz * r * 0.57])
          add(new THREE.ConeGeometry(r * 0.28, r * 0.54, 8), accent, [sx * r * 0.57, r * 2.02, sz * r * 0.57])
        }),
      )
      add(new THREE.ConeGeometry(r * 0.58, r * 0.78, 4), dark, [0, r * 2.06, 0]).rotation.y = Math.PI / 4
    } else if (entity.type === 'barracks') {
      add(new THREE.BoxGeometry(r * 1.78, r * 0.74, r * 1.34), stone, [0, r * 0.37, 0])
      add(new THREE.BoxGeometry(r * 1.42, r * 0.82, r * 1.12), primary, [0, r * 1.02, 0])
      const roof = add(new THREE.ConeGeometry(r * 1.12, r * 0.78, 4), dark, [0, r * 1.8, 0])
      roof.rotation.y = Math.PI / 4
      add(new THREE.BoxGeometry(r * 0.42, r * 1.1, r * 0.26), accent, [0, r * 0.72, r * 0.7])
    } else {
      add(new THREE.CylinderGeometry(r * 0.82, r, r * 0.62, 10), stone, [0, r * 0.31, 0])
      add(new THREE.CylinderGeometry(r * 0.26, r * 0.38, r * 1.56, 8), primary, [0, r * 1.18, 0])
      add(new THREE.ConeGeometry(r * 0.34, r * 0.86, 7), accent, [0, r * 2.18, 0])
    }

    if (race.id === 'compact' && entity.kind === 'unit') {
      const faceHeight = entity.type === 'worker' ? r * 1.47 : r * 1.74
      add(new THREE.BoxGeometry(r * 0.34, r * 0.25, r * 0.09), flesh, [0, faceHeight, r * 0.28])
      const shoulderCloth = add(
        new THREE.BoxGeometry(r * 0.92, r * 0.14, r * 0.52),
        primary,
        [0, r * 1.3, -r * 0.05],
      )
      shoulderCloth.rotation.z = entity.type === 'worker' ? 0.08 : 0
    }
    if (race.id === 'ascendancy') {
      for (let index = -1; index <= 1; index += 1) {
        const spike = add(new THREE.ConeGeometry(r * 0.12, r * 0.72, 5), dark, [index * r * 0.32, r * 1.34, -r * 0.45])
        spike.rotation.x = -0.38
      }
      if (entity.kind === 'unit') {
        const maskHeight = entity.type === 'worker' ? r * 1.48 : r * 1.78
        const mask = add(
          new THREE.BoxGeometry(r * 0.48, r * 0.5, r * 0.11),
          porcelain,
          [r * 0.07, maskHeight, r * 0.3],
        )
        mask.rotation.z = -0.08
        add(new THREE.ConeGeometry(r * 0.28, r * 0.7, 5), accent, [r * 0.62, r * 1.35, -r * 0.1]).rotation.z = -0.58
      } else {
        ;[-1, 1].forEach((side) => {
          const rib = add(
            new THREE.ConeGeometry(r * 0.13, r * 1.35, 5),
            porcelain,
            [side * r * 0.66, r * 1.2, -r * 0.12],
          )
          rib.rotation.z = side * 0.34
        })
      }
    }
    if (race.id === 'concord') {
      if (entity.kind === 'unit') {
        add(new THREE.BoxGeometry(r * 1.12, r * 0.22, r * 0.74), stone, [0, r * 1.14, 0])
        if (entity.type === 'skirmisher') {
          ;[-1, 1].forEach((side) => {
            const branch = add(
              new THREE.ConeGeometry(r * 0.06, r * 0.56, 5),
              dark,
              [side * r * 0.2, r * 2.02, -r * 0.02],
            )
            branch.rotation.z = side * 0.34
          })
        }
      } else {
        for (let index = 0; index < 5; index += 1) {
          const angle = index / 5 * Math.PI * 2
          const root = add(
            new THREE.ConeGeometry(r * 0.13, r * 0.9, 6),
            dark,
            [Math.cos(angle) * r * 0.74, r * 0.35, Math.sin(angle) * r * 0.74],
          )
          root.rotation.z = Math.cos(angle) * 0.7
          root.rotation.x = Math.sin(angle) * 0.7
        }
      }
    }
    if (entity.kind === 'unit') {
      if (race.id === 'ascendancy') {
        group.scale.set(1.08, 1.28, 1.08)
      } else if (race.id === 'concord' && entity.type === 'worker') {
        group.scale.set(1.28, 0.9, 1.28)
      } else if (race.id === 'concord' && entity.type === 'vanguard') {
        group.scale.set(1.32, 1.48, 1.32)
      } else if (race.id === 'concord') {
        group.scale.set(0.98, 1.26, 0.98)
      } else {
        group.scale.setScalar(1.14)
      }
    }
    return group
  }

  private renderResources(time: number): void {
    this.state.resources.forEach((resource, resourceIndex) => {
      let visual = this.resourceVisuals.get(resource.id)
      if (!visual) {
        visual = this.createResourceVisual(resource, resourceIndex)
        this.resourceVisuals.set(resource.id, visual)
        this.dynamicRoot.add(visual.root)
      }
      const visible = isPositionVisibleTo(this.state, resource.position, this.state.activePlayer, resource.radius)
      visual.root.visible = visible
      const ratio = THREE.MathUtils.clamp(resource.amount / resource.maxAmount, 0, 1)
      visual.crystals.scale.setScalar(0.48 + ratio * 0.52)
      visual.glow.material.opacity = 0.18 + Math.sin(time * 0.002 + resourceIndex) * 0.05
    })
  }

  private renderProjectiles(): void {
    const liveIds = new Set(this.state.projectiles.map((projectile) => projectile.id))
    this.projectileVisuals.forEach((mesh, id) => {
      if (!liveIds.has(id)) {
        this.effectRoot.remove(mesh)
        mesh.geometry.dispose()
        mesh.material.dispose()
        this.projectileVisuals.delete(id)
      }
    })
    this.state.projectiles.forEach((projectile) => {
      let mesh = this.projectileVisuals.get(projectile.id)
      if (!mesh) {
        mesh = new THREE.Mesh(
          new THREE.SphereGeometry(projectile.radius, 10, 8),
          new THREE.MeshBasicMaterial({ color: this.state.players[projectile.owner].accent }),
        )
        this.projectileVisuals.set(projectile.id, mesh)
        this.effectRoot.add(mesh)
      }
      mesh.position.set(projectile.position.x, 18, projectile.position.y)
    })
  }

  private renderControlPoints(time: number): void {
    this.state.controlPoints.forEach((point) => {
      const visual = this.controlPointVisuals.get(point.id)
      if (!visual) {
        return
      }
      const color = point.owner
        ? this.state.players[point.owner].accent
        : point.influence > 0
          ? this.state.players[PLAYER_ONE].accent
          : point.influence < 0
            ? this.state.players[PLAYER_TWO].accent
            : 0xb9aa85
      visual.ring.material.color.set(color)
      visual.beam.material.color.set(color)
      visual.ring.material.opacity = 0.32 + Math.abs(point.influence) / 100 * 0.52
      visual.beam.material.opacity = point.owner ? 0.19 + Math.sin(time * 0.002) * 0.04 : 0.08
    })
  }

  private createResourceVisual(resource: ResourceNode, index: number): ResourceVisual {
    const root = new THREE.Group()
    root.position.set(resource.position.x, 0, resource.position.y)
    root.userData.resourceId = resource.id
    const crystals = new THREE.Group()
    const crystalMaterial = new THREE.MeshStandardMaterial({
      color: 0xc5b78c,
      emissive: 0x51451d,
      emissiveIntensity: 0.65,
      roughness: 0.38,
      metalness: 0.36,
    })
    for (let shard = 0; shard < 5; shard += 1) {
      const height = 22 + ((index * 17 + shard * 11) % 30)
      const crystal = new THREE.Mesh(new THREE.OctahedronGeometry(10 + (shard % 3) * 3, 0), crystalMaterial)
      crystal.scale.y = height / 18
      crystal.position.set((shard - 2) * 11, height * 0.52, ((shard * 13) % 19) - 9)
      crystal.rotation.y = shard * 0.71
      crystal.castShadow = true
      crystal.userData.resourceId = resource.id
      crystals.add(crystal)
    }
    root.add(crystals)
    const glow = new THREE.Mesh(
      new THREE.RingGeometry(resource.radius * 0.92, resource.radius * 1.16, 40),
      new THREE.MeshBasicMaterial({ color: 0xd9af57, transparent: true, opacity: 0.2, side: THREE.DoubleSide }),
    )
    glow.rotation.x = -Math.PI / 2
    glow.position.y = 1.8
    glow.userData.resourceId = resource.id
    root.add(glow)
    return { root, crystals, glow }
  }

  private createInput(): void {
    const canvas = this.renderer.domElement
    canvas.addEventListener('pointerdown', (event) => {
      canvas.focus({ preventScroll: true })
      this.pointerWorld = this.groundPosition(event)
      if (event.button === 1) {
        this.middleDragging = true
        canvas.setPointerCapture(event.pointerId)
        return
      }
      if (event.button !== 0 || !this.pointerWorld) {
        return
      }
      this.pointerStart = { x: event.clientX, y: event.clientY, world: this.pointerWorld }
      this.selectionBox.classList.remove('visible')
    })

    canvas.addEventListener('pointermove', (event) => {
      this.pointerWorld = this.groundPosition(event)
      if (this.middleDragging) {
        const scale = this.cameraDistance / Math.max(420, this.container.clientHeight)
        this.cameraTarget.x -= event.movementX * scale
        this.cameraTarget.z -= event.movementY * scale
        this.clampCameraTarget()
        return
      }
      if (this.pointerStart) {
        const dx = event.clientX - this.pointerStart.x
        const dy = event.clientY - this.pointerStart.y
        if (Math.hypot(dx, dy) > 6 && !this.buildPreview && this.commandMode === 'normal') {
          const rect = canvas.getBoundingClientRect()
          this.selectionBox.classList.add('visible')
          this.selectionBox.style.left = `${Math.min(this.pointerStart.x, event.clientX) - rect.left}px`
          this.selectionBox.style.top = `${Math.min(this.pointerStart.y, event.clientY) - rect.top}px`
          this.selectionBox.style.width = `${Math.abs(dx)}px`
          this.selectionBox.style.height = `${Math.abs(dy)}px`
        }
      }
      if (this.buildPreview && this.pointerWorld) {
        this.updatePlacementGhost(this.pointerWorld)
      }
    })

    canvas.addEventListener('pointerup', (event) => {
      if (event.button === 1) {
        this.middleDragging = false
        return
      }
      if (event.button !== 0 || !this.pointerStart) {
        return
      }
      const start = this.pointerStart
      const currentWorld = this.groundPosition(event)
      this.pointerStart = null
      this.selectionBox.classList.remove('visible')
      if (!currentWorld) {
        return
      }
      if (this.buildPreview) {
        this.placePreviewBuilding(currentWorld)
        return
      }
      if (this.commandMode === 'attack-move') {
        const result = issueAttackMove(this.state, this.selectedIds, currentWorld)
        this.commandMode = 'normal'
        this.renderer.domElement.classList.remove('targeting')
        if (result.ok) {
          this.pulseCommand(currentWorld, 0xc84a4f)
          this.playTone(128, 0.08, 'square')
        }
        this.flashResult(result.reason)
        this.publishSnapshot()
        return
      }
      const dragDistance = Math.hypot(event.clientX - start.x, event.clientY - start.y)
      const additive = event.shiftKey
      if (dragDistance > 7) {
        this.selectInScreenRect(start.x, start.y, event.clientX, event.clientY, additive)
      } else {
        this.selectAt(event, additive)
      }
      this.publishSnapshot()
    })

    canvas.addEventListener('pointercancel', () => {
      this.pointerStart = null
      this.middleDragging = false
      this.selectionBox.classList.remove('visible')
    })

    canvas.addEventListener('contextmenu', (event) => {
      event.preventDefault()
      const world = this.groundPosition(event)
      if (!world) {
        return
      }
      this.issueContextCommand(event, world)
    })

    canvas.addEventListener('dblclick', (event) => {
      const entity = this.pickEntity(event)
      if (!entity || entity.owner !== this.state.activePlayer) {
        return
      }
      this.selectedIds = this.state.entities
        .filter(
          (candidate) =>
            candidate.owner === this.state.activePlayer &&
            candidate.type === entity.type &&
            isEntityVisibleTo(this.state, candidate, this.state.activePlayer),
        )
        .map((candidate) => candidate.id)
      this.playTone(280, 0.07, 'sine')
      this.publishSnapshot()
    })

    canvas.addEventListener(
      'wheel',
      (event) => {
        event.preventDefault()
        this.cameraDistance = THREE.MathUtils.clamp(this.cameraDistance + event.deltaY * 0.64, 420, 1380)
      },
      { passive: false },
    )

    window.addEventListener('keydown', (event) => this.handleKeyDown(event))
    window.addEventListener('keyup', (event) => this.heldKeys.delete(event.code))

    this.minimap.addEventListener('pointerdown', (event) => {
      const rect = this.minimap.getBoundingClientRect()
      this.cameraTarget.x = ((event.clientX - rect.left) / rect.width) * this.state.mapSize.x
      this.cameraTarget.z = ((event.clientY - rect.top) / rect.height) * this.state.mapSize.y
      this.clampCameraTarget()
      this.playTone(220, 0.045, 'sine')
    })
  }

  private handleKeyDown(event: KeyboardEvent): void {
    this.heldKeys.add(event.code)
    const digit = Number(event.key)
    if (Number.isInteger(digit) && digit >= 1 && digit <= 9) {
      event.preventDefault()
      if (event.ctrlKey || event.metaKey) {
        this.controlGroups.set(digit, [...this.selectedIds])
        this.flashResult(`Control group ${digit} set: ${this.selectedIds.length} selected.`)
        this.playTone(310 + digit * 12, 0.06, 'sine')
      } else if (event.shiftKey) {
        const existing = this.controlGroups.get(digit) ?? []
        this.controlGroups.set(digit, [...new Set([...existing, ...this.selectedIds])])
        this.flashResult(`Selection added to control group ${digit}.`)
      } else {
        this.recallControlGroup(digit)
      }
      this.publishSnapshot()
      return
    }

    if (event.repeat) {
      return
    }
    if (event.code === 'KeyA') {
      this.armAttackMove()
    } else if (event.code === 'Escape') {
      this.cancelCommand()
      this.selectedIds = []
      this.publishSnapshot()
    } else if (event.code === 'KeyQ') {
      this.trainSelected('worker')
    } else if (event.code === 'KeyE') {
      this.trainSelected('vanguard')
    } else if (event.code === 'KeyR') {
      this.trainSelected('skirmisher')
    } else if (event.code === 'KeyB') {
      this.setBuildPreview('barracks')
    } else if (event.code === 'KeyP') {
      this.setBuildPreview('turret')
    } else if (event.code === 'KeyH') {
      this.frameHome(false)
    } else if (event.code === 'KeyF') {
      this.useRacePower()
    } else if (event.code === 'KeyZ') {
      this.changeStance('aggressive')
    } else if (event.code === 'KeyX') {
      this.changeStance('defensive')
    } else if (event.code === 'KeyC') {
      this.changeStance('hold')
    } else if (event.code === 'KeyV') {
      this.retreatSelection()
    } else if (event.code === 'Space') {
      event.preventDefault()
      this.paused = !this.paused
      this.publishSnapshot()
    } else if (event.code === 'Tab' && this.state.mode === 'pvp') {
      event.preventDefault()
      setActivePlayer(this.state, enemyOf(this.state.activePlayer))
      this.selectedIds = []
      this.frameHome(false)
      this.publishSnapshot()
    }
  }

  private selectAt(event: PointerEvent, additive: boolean): void {
    const entity = this.pickEntity(event)
    if (!additive) {
      this.selectedIds = []
    }
    if (!entity || entity.owner !== this.state.activePlayer) {
      return
    }
    if (additive && this.selectedIds.includes(entity.id)) {
      this.selectedIds = this.selectedIds.filter((id) => id !== entity.id)
    } else if (!this.selectedIds.includes(entity.id)) {
      this.selectedIds.push(entity.id)
    }
    this.playTone(entity.kind === 'building' ? 170 : 245, 0.045, 'sine')
  }

  private selectInScreenRect(startX: number, startY: number, endX: number, endY: number, additive: boolean): void {
    const rect = this.renderer.domElement.getBoundingClientRect()
    const left = Math.min(startX, endX) - rect.left
    const right = Math.max(startX, endX) - rect.left
    const top = Math.min(startY, endY) - rect.top
    const bottom = Math.max(startY, endY) - rect.top
    if (!additive) {
      this.selectedIds = []
    }
    this.state.entities.forEach((entity) => {
      if (entity.owner !== this.state.activePlayer || entity.kind !== 'unit') {
        return
      }
      const screen = this.worldToScreen(entity.position)
      if (screen.x >= left && screen.x <= right && screen.y >= top && screen.y <= bottom) {
        if (!this.selectedIds.includes(entity.id)) {
          this.selectedIds.push(entity.id)
        }
      }
    })
    if (this.selectedIds.length > 0) {
      this.playTone(260, 0.055, 'sine')
    }
  }

  private issueContextCommand(event: MouseEvent, world: Vec2): void {
    const entity = this.pickEntity(event)
    const resource = this.pickResource(event)
    if (entity && entity.owner !== this.state.activePlayer) {
      const result = issueAttack(this.state, this.selectedIds, entity.id)
      if (result.ok) {
        this.pulseCommand(entity.position, 0xc84a4f)
        this.playTone(112, 0.08, 'sawtooth')
      }
      this.flashResult(result.reason)
      return
    }
    if (resource) {
      const result = issueGather(this.state, this.selectedIds, resource.id)
      if (result.ok) {
        this.pulseCommand(resource.position, 0xd9af57)
        this.playTone(320, 0.06, 'triangle')
      }
      this.flashResult(result.reason)
      return
    }

    const selected = this.selectedIds.map((id) => getEntity(this.state, id)).filter((item): item is Entity => Boolean(item))
    const hasUnits = selected.some((item) => item.kind === 'unit')
    const result = hasUnits
      ? issueMove(this.state, this.selectedIds, world)
      : setRallyPoint(this.state, this.selectedIds, world)
    if (result.ok) {
      this.pulseCommand(world, hasUnits ? 0xe9dfbd : 0xd9af57)
      this.playTone(hasUnits ? 210 : 285, 0.055, 'sine')
    }
    this.flashResult(result.reason)
    this.publishSnapshot()
  }

  private pickEntity(event: MouseEvent | PointerEvent): Entity | undefined {
    this.setPointerNdc(event)
    this.raycaster.setFromCamera(this.pointerNdc, this.camera)
    const roots = [...this.entityVisuals.values()].filter((visual) => visual.root.visible).map((visual) => visual.root)
    const hit = this.raycaster.intersectObjects(roots, true).find((intersection) => intersection.object.userData.entityId)
    return hit ? getEntity(this.state, hit.object.userData.entityId as string) : undefined
  }

  private pickResource(event: MouseEvent | PointerEvent): ResourceNode | undefined {
    this.setPointerNdc(event)
    this.raycaster.setFromCamera(this.pointerNdc, this.camera)
    const roots = [...this.resourceVisuals.values()].filter((visual) => visual.root.visible).map((visual) => visual.root)
    const hit = this.raycaster.intersectObjects(roots, true).find((intersection) => intersection.object.userData.resourceId)
    return hit ? this.state.resources.find((resource) => resource.id === hit.object.userData.resourceId) : undefined
  }

  private groundPosition(event: MouseEvent | PointerEvent): Vec2 | null {
    this.setPointerNdc(event)
    this.raycaster.setFromCamera(this.pointerNdc, this.camera)
    const hit = this.raycaster.intersectObject(this.ground, false)[0]
    if (!hit) {
      return null
    }
    return {
      x: THREE.MathUtils.clamp(hit.point.x, 0, MAP_SIZE.x),
      y: THREE.MathUtils.clamp(hit.point.z, 0, MAP_SIZE.y),
    }
  }

  private setPointerNdc(event: MouseEvent | PointerEvent): void {
    const rect = this.renderer.domElement.getBoundingClientRect()
    this.pointerNdc.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1)
  }

  private worldToScreen(position: Vec2): Vec2 {
    const projected = new THREE.Vector3(position.x, 18, position.y).project(this.camera)
    return {
      x: ((projected.x + 1) / 2) * this.container.clientWidth,
      y: ((1 - projected.y) / 2) * this.container.clientHeight,
    }
  }

  private listenForUi(): void {
    window.addEventListener('rts:command', (event) => {
      const detail = (event as CustomEvent<UiCommand>).detail
      if (detail.type === 'start') {
        this.missionStarted = true
        this.paused = false
        this.playTone(96, 0.16, 'sawtooth')
      } else if (detail.type === 'mode') {
        this.restart(detail.mode)
      } else if (detail.type === 'race') {
        this.selectedRace = detail.race
        this.restart(this.state.mode)
      } else if (detail.type === 'opponent-race') {
        this.selectedOpponentRace = detail.race
        this.restart(this.state.mode)
      } else if (detail.type === 'map') {
        this.selectedMap = detail.mapId
        this.restart(this.state.mode)
      } else if (detail.type === 'mission') {
        this.selectedMission = detail.missionId
        const mission = getStoryMission(detail.missionId)
        this.selectedMap = mission.mapId
        this.selectedRace = mission.playerRace
        this.selectedOpponentRace = mission.opponentRace
        this.restart('story')
      } else if (detail.type === 'ai') {
        this.selectedAi = detail.personality
        this.restart(this.state.mode)
      } else if (detail.type === 'rule') {
        this.selectedRule = detail.rule
        this.restart(this.state.mode)
      } else if (detail.type === 'restart') {
        this.restart(this.state.mode)
      } else if (detail.type === 'train') {
        this.trainSelected(detail.unitType)
      } else if (detail.type === 'build') {
        this.setBuildPreview(detail.buildingType)
      } else if (detail.type === 'cancel-build') {
        this.cancelCommand()
      } else if (detail.type === 'focus-home') {
        this.frameHome(false)
      } else if (detail.type === 'attack-move') {
        this.armAttackMove()
      } else if (detail.type === 'group') {
        this.recallControlGroup(detail.slot)
      } else if (detail.type === 'race-power') {
        this.useRacePower()
      } else if (detail.type === 'stance') {
        this.changeStance(detail.stance)
      } else if (detail.type === 'retreat') {
        this.retreatSelection()
      } else if (detail.type === 'research') {
        this.researchSelected(detail.researchId)
      } else if (detail.type === 'pause') {
        this.paused = !this.paused
      }
      this.publishSnapshot()
    })
  }

  private restart(mode: GameMode): void {
    if (mode === 'story') {
      const mission = getStoryMission(this.selectedMission)
      this.selectedMap = mission.mapId
      this.selectedRace = mission.playerRace
      this.selectedOpponentRace = mission.opponentRace
    }
    this.state = createInitialState(mode, this.selectedRace, this.selectedOpponentRace, {
      mapId: this.selectedMap,
      missionId: this.selectedMission,
      aiPersonality: this.selectedAi,
      matchRule: this.selectedRule,
    })
    this.missionStarted = mode !== 'story'
    this.paused = false
    this.selectedIds = []
    this.controlGroups.clear()
    this.cancelCommand()
    this.entityVisuals.forEach((visual) => this.disposeObject(visual.root))
    this.resourceVisuals.forEach((visual) => this.disposeObject(visual.root))
    this.entityVisuals.clear()
    this.resourceVisuals.clear()
    this.dynamicRoot.clear()
    this.projectileVisuals.forEach((mesh) => {
      this.effectRoot.remove(mesh)
      mesh.geometry.dispose()
      mesh.material.dispose()
    })
    this.projectileVisuals.clear()
    this.commandPulses.splice(0).forEach((pulse) => this.effectRoot.remove(pulse.mesh))
    this.simulationAccumulator = 0
    this.createMapGeometry()
    this.frameHome(true)
    this.publishSnapshot()
  }

  private trainSelected(unitType: UnitType): void {
    const selectedBuildings = this.selectedIds
      .map((id) => getEntity(this.state, id))
      .filter((entity): entity is Entity => entity !== undefined && entity.kind === 'building')
    const producer = selectedBuildings.find((entity) =>
      unitType === 'worker' ? entity.type === 'command' : entity.type === 'barracks',
    )
    if (!producer) {
      const producerType = unitType === 'worker' ? 'command' : 'barracks'
      this.flashResult(`Select a ${getEntityDef(producerType, this.state.players[this.state.activePlayer].race).label}.`)
      this.playTone(84, 0.08, 'square')
      return
    }
    const result = startProduction(this.state, producer.id, unitType)
    if (result.ok) {
      this.playTone(340, 0.06, 'triangle')
    } else {
      this.playTone(84, 0.08, 'square')
    }
    this.flashResult(result.reason)
    this.publishSnapshot()
  }

  private useRacePower(): void {
    const result = activateRacePower(this.state)
    if (result.ok) {
      this.playTone(118, 0.16, 'sawtooth')
      const command = this.state.entities.find(
        (entity) => entity.owner === this.state.activePlayer && entity.type === 'command',
      )
      if (command) {
        this.pulseCommand(command.position, this.state.players[this.state.activePlayer].accent)
      }
    } else {
      this.playTone(82, 0.08, 'square')
    }
    this.flashResult(result.reason)
    this.publishSnapshot()
  }

  private changeStance(stance: UnitStance): void {
    const result = setUnitStance(this.state, this.selectedIds, stance)
    this.flashResult(result.reason)
    this.playTone(result.ok ? 260 : 84, 0.06, result.ok ? 'triangle' : 'square')
    this.publishSnapshot()
  }

  private retreatSelection(): void {
    const result = issueRetreat(this.state, this.selectedIds)
    this.flashResult(result.reason)
    this.playTone(result.ok ? 150 : 84, 0.08, result.ok ? 'sawtooth' : 'square')
    this.publishSnapshot()
  }

  private researchSelected(researchId: ResearchId): void {
    const research = RESEARCH_DEFS[researchId]
    const producer = this.selectedIds
      .map((id) => getEntity(this.state, id))
      .find((entity) => entity?.type === research.producer)
    if (!producer) {
      this.flashResult(`Select ${getEntityDef(research.producer, this.state.players[this.state.activePlayer].race).label}.`)
      this.playTone(84, 0.08, 'square')
      return
    }
    const result = startResearch(this.state, producer.id, researchId)
    this.flashResult(result.reason)
    this.playTone(result.ok ? 310 : 84, 0.08, result.ok ? 'triangle' : 'square')
    this.publishSnapshot()
  }

  private setBuildPreview(buildingType: BuildingType | null): void {
    this.commandMode = 'normal'
    this.renderer.domElement.classList.remove('targeting')
    this.buildPreview = buildingType
    if (this.placementGhost) {
      this.disposeObject(this.placementGhost)
      this.effectRoot.remove(this.placementGhost)
      this.placementGhost = null
    }
    if (!buildingType) {
      this.renderer.domElement.classList.remove('placing')
      return
    }
    this.renderer.domElement.classList.add('placing')
    this.placementGhost = this.createPlacementGhost(buildingType)
    this.effectRoot.add(this.placementGhost)
    if (this.pointerWorld) {
      this.updatePlacementGhost(this.pointerWorld)
    }
    this.publishSnapshot()
  }

  private createPlacementGhost(buildingType: BuildingType): THREE.Group {
    const group = new THREE.Group()
    const def = getEntityDef(buildingType, this.state.players[this.state.activePlayer].race)
    const material = new THREE.MeshStandardMaterial({
      color: 0x8ecf77,
      transparent: true,
      opacity: 0.38,
      roughness: 0.72,
      depthWrite: false,
    })
    const body = new THREE.Mesh(
      buildingType === 'turret'
        ? new THREE.CylinderGeometry(def.radius * 0.72, def.radius, def.radius * 1.8, 10)
        : new THREE.BoxGeometry(def.radius * 1.7, def.radius * 1.2, def.radius * 1.45),
      material,
    )
    body.position.y = def.radius * 0.65
    group.add(body)
    const ring = new THREE.Mesh(
      new THREE.RingGeometry(def.radius * 1.02, def.radius * 1.12, 40),
      new THREE.MeshBasicMaterial({ color: 0x8ecf77, transparent: true, opacity: 0.72, side: THREE.DoubleSide }),
    )
    ring.rotation.x = -Math.PI / 2
    ring.position.y = 1.5
    group.add(ring)
    group.userData.bodyMaterial = material
    group.userData.ringMaterial = ring.material
    return group
  }

  private updatePlacementGhost(world: Vec2): void {
    if (!this.placementGhost || !this.buildPreview) {
      return
    }
    const def = getEntityDef(this.buildPreview, this.state.players[this.state.activePlayer].race)
    const valid = canPlaceBuilding(this.state, world, def.radius)
    const color = valid ? 0x8ecf77 : 0xc84a4f
    this.placementGhost.position.set(world.x, 0, world.y)
    ;(this.placementGhost.userData.bodyMaterial as THREE.MeshStandardMaterial).color.set(color)
    ;(this.placementGhost.userData.ringMaterial as THREE.MeshBasicMaterial).color.set(color)
  }

  private placePreviewBuilding(world: Vec2): void {
    const worker = this.selectedIds.map((id) => getEntity(this.state, id)).find((entity) => entity?.type === 'worker')
    if (!worker || !this.buildPreview) {
      this.flashResult(`Select a ${getEntityDef('worker', this.state.players[this.state.activePlayer].race).label} first.`)
      this.playTone(82, 0.08, 'square')
      return
    }
    const result = startBuilding(this.state, worker.id, this.buildPreview, world)
    if (result.ok) {
      this.pulseCommand(world, 0xd9af57)
      this.playTone(154, 0.1, 'triangle')
      this.setBuildPreview(null)
    } else {
      this.playTone(82, 0.08, 'square')
    }
    this.flashResult(result.reason)
    this.publishSnapshot()
  }

  private armAttackMove(): void {
    this.setBuildPreview(null)
    const combatUnits = this.selectedIds
      .map((id) => getEntity(this.state, id))
      .filter((entity) => entity?.kind === 'unit' && entity.damage > 0)
    if (combatUnits.length === 0) {
      this.flashResult('Select combat units before attack-moving.')
      this.playTone(82, 0.08, 'square')
      return
    }
    this.commandMode = 'attack-move'
    this.renderer.domElement.classList.add('targeting')
    this.flashResult('Attack-move armed. Choose ground to advance through.')
    this.publishSnapshot()
  }

  private cancelCommand(): void {
    this.commandMode = 'normal'
    this.renderer.domElement.classList.remove('targeting')
    if (this.buildPreview) {
      this.setBuildPreview(null)
    }
  }

  private recallControlGroup(slot: number): void {
    const validIds = (this.controlGroups.get(slot) ?? []).filter((id) => Boolean(getEntity(this.state, id)))
    this.controlGroups.set(slot, validIds)
    if (validIds.length === 0) {
      this.flashResult(`Control group ${slot} is empty.`)
      return
    }
    this.selectedIds = validIds
    const now = performance.now()
    if (this.lastGroupTap.slot === slot && now - this.lastGroupTap.time < 380) {
      this.focusSelection()
    }
    this.lastGroupTap = { slot, time: now }
    this.playTone(240 + slot * 8, 0.045, 'sine')
  }

  private focusSelection(): void {
    const selected = this.selectedIds.map((id) => getEntity(this.state, id)).filter((entity): entity is Entity => Boolean(entity))
    if (selected.length === 0) {
      return
    }
    this.cameraTarget.x = selected.reduce((sum, entity) => sum + entity.position.x, 0) / selected.length
    this.cameraTarget.z = selected.reduce((sum, entity) => sum + entity.position.y, 0) / selected.length
    this.clampCameraTarget()
  }

  private animate = (frameTime = performance.now()): void => {
    if (this.destroyed) {
      return
    }
    this.frameId = requestAnimationFrame(this.animate)
    this.timer.update(frameTime)
    const dt = Math.min(this.timer.getDelta(), 0.1)
    const time = frameTime
    if ((this.missionStarted || this.state.mode !== 'story') && !this.paused) {
      this.simulationAccumulator = Math.min(0.25, this.simulationAccumulator + dt)
      while (this.simulationAccumulator >= 0.05) {
        updateSimulation(this.state, 0.05)
        this.simulationAccumulator -= 0.05
      }
    }
    this.updateCamera(dt)
    this.renderEntities(time)
    this.renderResources(time)
    this.renderProjectiles()
    this.renderControlPoints(time)
    this.updateWater(time)
    this.updateRain(dt)
    this.updateEffects(dt)
    this.lastFogUpdate += dt
    if (this.lastFogUpdate >= 0.18) {
      this.updateFog()
      this.lastFogUpdate = 0
    }
    this.renderMinimap()
    this.renderer.render(this.scene, this.camera)
    this.lastSnapshot += dt
    if (this.lastSnapshot >= 0.14) {
      this.publishSnapshot()
      this.lastSnapshot = 0
    }
  }

  private updateCamera(dt: number): void {
    const speed = this.cameraDistance * 0.74 * dt
    if (this.heldKeys.has('ArrowLeft')) {
      this.cameraTarget.x -= speed
    }
    if (this.heldKeys.has('ArrowRight')) {
      this.cameraTarget.x += speed
    }
    if (this.heldKeys.has('ArrowUp')) {
      this.cameraTarget.z -= speed
    }
    if (this.heldKeys.has('ArrowDown')) {
      this.cameraTarget.z += speed
    }
    this.clampCameraTarget()
    this.camera.position.set(
      this.cameraTarget.x,
      this.cameraDistance * 0.78,
      this.cameraTarget.z + this.cameraDistance * 0.69,
    )
    this.camera.lookAt(this.cameraTarget.x, 0, this.cameraTarget.z)
  }

  private clampCameraTarget(): void {
    const margin = Math.min(250, this.cameraDistance * 0.24)
    this.cameraTarget.x = THREE.MathUtils.clamp(this.cameraTarget.x, margin, this.state.mapSize.x - margin)
    this.cameraTarget.z = THREE.MathUtils.clamp(this.cameraTarget.z, margin, this.state.mapSize.y - margin)
  }

  private frameHome(immediate: boolean): void {
    const command = this.state.entities.find(
      (entity) => entity.owner === this.state.activePlayer && entity.type === 'command',
    )
    if (!command) {
      return
    }
    const direction = this.state.activePlayer === PLAYER_ONE ? 1 : -1
    const narrow = this.container.clientWidth < 640
    this.cameraTarget.set(
      command.position.x + (narrow ? 45 : 170) * direction,
      0,
      command.position.y - (narrow ? 35 : 105) * direction,
    )
    this.cameraDistance = immediate ? (narrow ? 850 : 720) : Math.min(this.cameraDistance, narrow ? 880 : 820)
    this.clampCameraTarget()
  }

  private resize(): void {
    const width = Math.max(1, this.container.clientWidth)
    const height = Math.max(1, this.container.clientHeight)
    this.camera.aspect = width / height
    this.camera.updateProjectionMatrix()
    this.renderer.setSize(width, height, false)
  }

  private updateRain(dt: number): void {
    const attribute = this.rain.geometry.getAttribute('position') as THREE.BufferAttribute
    for (let index = 0; index < attribute.count; index += 1) {
      let y = attribute.getY(index) - 430 * dt
      let x = attribute.getX(index) + 24 * dt
      if (y < 8) {
        y = 690 + ((index * 31) % 160)
        x = (index * 977 + performance.now() * 0.02) % this.state.mapSize.x
      }
      attribute.setXYZ(index, x, y, attribute.getZ(index))
    }
    attribute.needsUpdate = true
  }

  private updateWater(time: number): void {
    this.waterMeshes.forEach((water, index) => {
      water.position.y = 0.7 + Math.sin(time * 0.0014 + index) * 0.42
      water.material.opacity = 0.68 + Math.sin(time * 0.0008 + index * 0.7) * 0.05
      water.material.emissiveIntensity = 0.32 + Math.sin(time * 0.0011 + index) * 0.08
    })
  }

  private updateFog(): void {
    const context = this.fogCanvas.getContext('2d')
    if (!context) {
      return
    }
    context.globalCompositeOperation = 'source-over'
    context.clearRect(0, 0, this.fogCanvas.width, this.fogCanvas.height)
    context.fillStyle = 'rgba(1, 5, 4, 0.9)'
    context.fillRect(0, 0, this.fogCanvas.width, this.fogCanvas.height)
    context.globalCompositeOperation = 'destination-out'
    this.state.entities
      .filter((entity) => entity.owner === this.state.activePlayer && !entity.underConstruction)
      .forEach((entity) => {
        const x = (entity.position.x / this.state.mapSize.x) * this.fogCanvas.width
        const y = (entity.position.y / this.state.mapSize.y) * this.fogCanvas.height
        const radius = (entity.sight / this.state.mapSize.x) * this.fogCanvas.width
        const gradient = context.createRadialGradient(x, y, radius * 0.48, x, y, radius)
        gradient.addColorStop(0, 'rgba(0, 0, 0, 1)')
        gradient.addColorStop(1, 'rgba(0, 0, 0, 0)')
        context.fillStyle = gradient
        context.beginPath()
        context.arc(x, y, radius, 0, Math.PI * 2)
        context.fill()
      })
    context.globalCompositeOperation = 'source-over'
    this.fogTexture.needsUpdate = true
  }

  private renderMinimap(): void {
    const context = this.minimapContext
    const width = this.minimap.width
    const height = this.minimap.height
    const sx = width / this.state.mapSize.x
    const sy = height / this.state.mapSize.y
    const map = getMapDef(this.state.mapId)
    context.clearRect(0, 0, width, height)
    context.fillStyle = '#07100d'
    context.fillRect(0, 0, width, height)
    context.strokeStyle = 'rgba(132, 151, 134, .24)'
    context.lineWidth = 7
    map.lanes.forEach((lane) => {
      context.beginPath()
      lane.forEach((point, index) => {
        if (index === 0) context.moveTo(point.x * sx, point.y * sy)
        else context.lineTo(point.x * sx, point.y * sy)
      })
      context.stroke()
    })
    map.blockers.forEach((blocker) => {
      context.fillStyle = '#252b27'
      if (blocker.kind === 'circle') {
        context.beginPath()
        context.arc(blocker.position.x * sx, blocker.position.y * sy, Math.max(3, (blocker.radius ?? 40) * sx), 0, Math.PI * 2)
        context.fill()
      } else {
        const size = blocker.size ?? { x: 100, y: 50 }
        context.fillRect((blocker.position.x - size.x / 2) * sx, (blocker.position.y - size.y / 2) * sy, size.x * sx, size.y * sy)
      }
    })
    this.state.controlPoints.forEach((point) => {
      context.strokeStyle = point.owner
        ? `#${this.state.players[point.owner].accent.toString(16).padStart(6, '0')}`
        : '#b9aa85'
      context.lineWidth = 2
      context.beginPath()
      context.arc(point.position.x * sx, point.position.y * sy, Math.max(4, point.radius * sx), 0, Math.PI * 2)
      context.stroke()
    })
    this.state.resources.forEach((resource) => {
      if (!isPositionVisibleTo(this.state, resource.position, this.state.activePlayer, resource.radius)) {
        return
      }
      context.fillStyle = '#c8b77f'
      context.fillRect(resource.position.x * sx - 1.5, resource.position.y * sy - 1.5, 3, 3)
    })
    this.state.entities.forEach((entity) => {
      if (!isEntityVisibleTo(this.state, entity, this.state.activePlayer)) {
        return
      }
      const player = this.state.players[entity.owner]
      context.fillStyle = `#${player.color.toString(16).padStart(6, '0')}`
      const size = entity.kind === 'building' ? 5 : 2.6
      context.fillRect(entity.position.x * sx - size / 2, entity.position.y * sy - size / 2, size, size)
    })
    const viewWidth = Math.min(width, this.cameraDistance * 0.55 * sx)
    const viewHeight = Math.min(height, this.cameraDistance * 0.38 * sy)
    context.strokeStyle = 'rgba(239, 226, 188, .8)'
    context.lineWidth = 1.5
    context.strokeRect(this.cameraTarget.x * sx - viewWidth / 2, this.cameraTarget.z * sy - viewHeight / 2, viewWidth, viewHeight)
  }

  private pulseCommand(world: Vec2, color: number): void {
    const material = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.9, side: THREE.DoubleSide })
    const mesh = new THREE.Mesh(new THREE.RingGeometry(12, 17, 36), material)
    mesh.rotation.x = -Math.PI / 2
    mesh.position.set(world.x, 4, world.y)
    this.effectRoot.add(mesh)
    this.commandPulses.push({ mesh, age: 0, duration: 0.46 })
  }

  private pulseImpact(world: Vec2, color: number): void {
    const material = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.78,
      side: THREE.DoubleSide,
      depthWrite: false,
    })
    const mesh = new THREE.Mesh(new THREE.RingGeometry(4, 9, 24), material)
    mesh.rotation.x = -Math.PI / 2
    mesh.position.set(world.x, 5, world.y)
    this.effectRoot.add(mesh)
    this.commandPulses.push({ mesh, age: 0, duration: 0.24 })
  }

  private updateEffects(dt: number): void {
    for (let index = this.commandPulses.length - 1; index >= 0; index -= 1) {
      const pulse = this.commandPulses[index]
      pulse.age += dt
      const progress = pulse.age / pulse.duration
      pulse.mesh.scale.setScalar(1 + progress * 2.2)
      pulse.mesh.material.opacity = Math.max(0, 0.9 * (1 - progress))
      if (progress >= 1) {
        this.effectRoot.remove(pulse.mesh)
        pulse.mesh.geometry.dispose()
        pulse.mesh.material.dispose()
        this.commandPulses.splice(index, 1)
      }
    }

  }

  private publishSnapshot(): void {
    this.selectedIds = this.selectedIds.filter((id) => Boolean(getEntity(this.state, id)))
    const selected = this.selectedIds
      .map((id) => getEntity(this.state, id))
      .filter((entity): entity is Entity => Boolean(entity))
      .map((entity) => ({
        id: entity.id,
        label: entity.label,
        type: entity.type,
        hp: Math.max(0, Math.round(entity.hp)),
        maxHp: entity.maxHp,
        resolve: entity.resolve,
        armor: entity.armor,
        counters: entity.bonusAgainst,
        underConstruction: entity.underConstruction,
        constructionProgress: entity.constructionProgress,
        queue: entity.queue,
        rallyPoint: entity.rallyPoint,
        stance: entity.stance,
      }))
    const activePlayer = this.state.players[this.state.activePlayer]
    const selectedTypes = new Set(selected.map((entity) => entity.type))
    const research = researchForRace(activePlayer.race).map((definition) => {
      const completed = activePlayer.researched.includes(definition.id)
      const queued = activePlayer.researchQueue.some((task) => task.id === definition.id)
      return {
        ...definition,
        available:
          !completed &&
          !queued &&
          activePlayer.researchQueue.length === 0 &&
          (!definition.requires || activePlayer.researched.includes(definition.requires)) &&
          activePlayer.ore >= definition.cost &&
          selectedTypes.has(definition.producer),
        completed,
        queued,
      }
    })
    const snapshot: ThreeUiSnapshot = {
      mode: this.state.mode,
      activePlayer: this.state.activePlayer,
      status: this.state.status,
      players: this.state.players,
      selected,
      events: this.state.events,
      objective: getObjectiveText(this.state),
      buildPreview: this.buildPreview,
      ruinTide: this.state.ruinTide,
      activeRace: this.state.players[this.state.activePlayer].race,
      races: Object.values(RACE_DEFS).map((race) => ({
        id: race.id,
        name: race.name,
        tagline: race.tagline,
        style: race.style,
      })),
      commands: this.commandSnapshot(),
      supply: getPlayerSupply(this.state, this.state.activePlayer),
      commandMode: this.commandMode,
      controlGroups: [...this.controlGroups.entries()]
        .map(([slot, ids]) => ({ slot, count: ids.filter((id) => Boolean(getEntity(this.state, id))).length }))
        .filter((group) => group.count > 0),
      paused: this.paused,
      missionStarted: this.missionStarted,
      matchTime: this.state.time,
      racePower: {
        ...RACE_POWER_DEFS[this.state.players[this.state.activePlayer].race],
        cooldown: this.state.players[this.state.activePlayer].powerCooldown,
        maxCooldown: RACE_POWER_DEFS[this.state.players[this.state.activePlayer].race].cooldown,
      },
      setupRaces: { player: this.selectedRace, opponent: this.selectedOpponentRace },
      setup: {
        mapId: this.selectedMap,
        missionId: this.selectedMission,
        aiPersonality: this.selectedAi,
        matchRule: this.selectedRule,
      },
      mapName: getMapDef(this.state.mapId).name,
      mission: getStoryMission(this.selectedMission),
      research,
      controlPoints: this.state.controlPoints.map((point) => ({ ...point, position: { ...point.position } })),
      stats: this.state.stats,
    }
    window.dispatchEvent(new CustomEvent('rts:state', { detail: snapshot }))
  }

  private commandSnapshot(): ThreeUiSnapshot['commands'] {
    const race = this.state.players[this.state.activePlayer].race
    return {
      train: {
        worker: this.commandLabel('worker', race, getProductionProfile(this.state, this.state.activePlayer, 'worker').cost),
        vanguard: this.commandLabel('vanguard', race, getProductionProfile(this.state, this.state.activePlayer, 'vanguard').cost),
        skirmisher: this.commandLabel('skirmisher', race, getProductionProfile(this.state, this.state.activePlayer, 'skirmisher').cost),
      },
      build: {
        command: this.commandLabel('command', race),
        barracks: this.commandLabel('barracks', race),
        turret: this.commandLabel('turret', race),
      },
    }
  }

  private commandLabel(type: UnitType | BuildingType, race: RaceId, costOverride?: number): { label: string; cost: number; supply: number } {
    const def = getEntityDef(type, race)
    return { label: def.label, cost: costOverride ?? def.cost, supply: def.kind === 'unit' ? def.supplyCost : def.supplyProvided }
  }

  private flashResult(reason?: string): void {
    if (reason) {
      window.dispatchEvent(new CustomEvent('rts:notice', { detail: reason }))
    }
  }

  private playTone(frequency: number, duration: number, type: OscillatorType): void {
    const AudioContextClass = window.AudioContext ?? window.webkitAudioContext
    if (!AudioContextClass) {
      return
    }
    const context = RtsThreeEngine.audioContext ?? new AudioContextClass()
    RtsThreeEngine.audioContext = context
    if (context.state === 'suspended') {
      void context.resume()
    }
    const oscillator = context.createOscillator()
    const gain = context.createGain()
    oscillator.type = type
    oscillator.frequency.setValueAtTime(frequency, context.currentTime)
    gain.gain.setValueAtTime(0.018, context.currentTime)
    gain.gain.exponentialRampToValueAtTime(0.0001, context.currentTime + duration)
    oscillator.connect(gain)
    gain.connect(context.destination)
    oscillator.start()
    oscillator.stop(context.currentTime + duration)
  }

  private disposeObject(object: THREE.Object3D): void {
    object.traverse((child) => {
      if (!(child instanceof THREE.Mesh)) {
        return
      }
      child.geometry.dispose()
      const materials = Array.isArray(child.material) ? child.material : [child.material]
      materials.forEach((material) => material.dispose())
    })
  }

  private static audioContext: AudioContext | null = null
}

function lerpAngle(current: number, target: number, amount: number): number {
  const delta = Math.atan2(Math.sin(target - current), Math.cos(target - current))
  return current + delta * amount
}

function pointToSegmentDistance(point: Vec2, start: Vec2, end: Vec2): number {
  const dx = end.x - start.x
  const dy = end.y - start.y
  const lengthSquared = dx * dx + dy * dy
  if (lengthSquared === 0) {
    return distance(point, start)
  }
  const ratio = THREE.MathUtils.clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared, 0, 1)
  return distance(point, { x: start.x + dx * ratio, y: start.y + dy * ratio })
}

declare global {
  interface Window {
    webkitAudioContext?: typeof AudioContext
  }
}
