export type PlayerId = 1 | 2
export type GameMode = 'story' | 'skirmish' | 'pvp'
export type RaceId = 'compact' | 'ascendancy' | 'concord'
export type MapId = 'black-iron-ford' | 'ossuary-crossroads'
export type MissionId = 'bridge-of-names' | 'mercy-for-the-uncounted' | 'where-roots-remember'
export type AiPersonality = 'aggressive' | 'economic' | 'fortress'
export type MatchRuleId = 'standard' | 'fast-ruin' | 'rich-seams'
export type UnitStance = 'aggressive' | 'defensive' | 'hold'
export type ResearchId =
  | 'tier-two'
  | 'tempered-oaths'
  | 'wardcraft'
  | 'chorus-of-knives'
  | 'pit-broods'
  | 'vault-plate'
  | 'siege-liturgy'
export type EntityKind = 'unit' | 'building'
export type UnitType = 'worker' | 'vanguard' | 'skirmisher'
export type BuildingType = 'command' | 'barracks' | 'turret'
export type EntityType = UnitType | BuildingType
export type ArmorClass = 'laborer' | 'armored' | 'light' | 'structure'

export interface Vec2 {
  x: number
  y: number
}

export interface EntityDef {
  type: EntityType
  kind: EntityKind
  label: string
  description: string
  cost: number
  buildTime: number
  hp: number
  radius: number
  speed: number
  range: number
  damage: number
  attackCooldown: number
  sight: number
  armor: ArmorClass
  bonusAgainst: ArmorClass[]
  bonusDamage: number
  terror: number
  ward: number
  supplyCost: number
  supplyProvided: number
}

export interface RaceDef {
  id: RaceId
  name: string
  shortName: string
  tagline: string
  style: string
  protagonist: string
  ethos: string
  cost: string
  silhouette: string
  color: number
  accent: number
  unitScale: number
  incomeRate: number
  resolveDrift: number
  overrides: Partial<Record<EntityType, Partial<Omit<EntityDef, 'type' | 'kind'>>>>
}

export type Order =
  | { type: 'idle' }
  | { type: 'move'; target: Vec2 }
  | { type: 'attack'; targetId: string }
  | { type: 'attack-move'; target: Vec2; targetId?: string }
  | { type: 'gather'; resourceId: string; phase: 'to-resource' | 'harvest' | 'return'; harvestTimer: number }
  | { type: 'build'; buildingType: BuildingType; target: Vec2; progress: number; buildingId: string }

export interface ProductionTask {
  type: UnitType
  remaining: number
  total: number
}

export interface ResearchTask {
  id: ResearchId
  remaining: number
  total: number
}

export interface Entity {
  id: string
  owner: PlayerId
  kind: EntityKind
  type: EntityType
  label: string
  position: Vec2
  radius: number
  hp: number
  maxHp: number
  speed: number
  range: number
  damage: number
  attackCooldown: number
  cooldown: number
  sight: number
  armor: ArmorClass
  bonusAgainst: ArmorClass[]
  bonusDamage: number
  terror: number
  ward: number
  resolve: number
  order: Order
  carrying: number
  queue: ProductionTask[]
  rallyPoint: Vec2 | null
  stance: UnitStance
  guardPosition: Vec2
  navigationPath: Vec2[]
  navigationTarget: Vec2 | null
  underConstruction: boolean
  constructionProgress: number
}

export interface ResourceNode {
  id: string
  position: Vec2
  radius: number
  amount: number
  maxAmount: number
}

export interface PlayerState {
  id: PlayerId
  race: RaceId
  name: string
  ore: number
  resolve: number
  color: number
  accent: number
  active: boolean
  powerCooldown: number
  techTier: number
  researched: ResearchId[]
  researchQueue: ResearchTask[]
}

export interface CombatProjectile {
  id: string
  owner: PlayerId
  sourceId: string
  targetId: string
  position: Vec2
  speed: number
  damage: number
  radius: number
}

export interface ControlPoint {
  id: string
  position: Vec2
  radius: number
  owner: PlayerId | null
  influence: number
}

export interface MatchStats {
  oreMined: number
  unitsCreated: number
  unitsLost: number
  structuresLost: number
  damageDealt: number
  objectivesCaptured: number
}

export interface GameSetup {
  mapId: MapId
  missionId: MissionId
  aiPersonality: AiPersonality
  matchRule: MatchRuleId
}

export interface GameEvent {
  id: number
  time: number
  text: string
  tone: 'info' | 'success' | 'danger'
}

export interface GameState {
  mode: GameMode
  mapId: MapId
  missionId: MissionId
  aiPersonality: AiPersonality
  matchRule: MatchRuleId
  tick: number
  time: number
  mapSize: Vec2
  players: Record<PlayerId, PlayerState>
  activePlayer: PlayerId
  entities: Entity[]
  resources: ResourceNode[]
  projectiles: CombatProjectile[]
  controlPoints: ControlPoint[]
  stats: Record<PlayerId, MatchStats>
  events: GameEvent[]
  status: 'playing' | 'won' | 'lost'
  winner: PlayerId | null
  ruinTide: number
  tideCrestAnnounced: boolean
  storyStep: number
  aiWaveTimer: number
  aiDecisionTimer: number
  nextEventId: number
}

export interface CommandResult {
  ok: boolean
  reason?: string
}
