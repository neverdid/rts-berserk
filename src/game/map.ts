import type { MapId, PlayerId, ResourceNode, Vec2 } from './types'

export interface TerrainBlocker {
  id: string
  kind: 'circle' | 'rect'
  position: Vec2
  radius?: number
  size?: Vec2
  color: number
  accent: number
}

export interface TerrainZone {
  id: string
  kind: 'high-ground' | 'forest' | 'ford' | 'cursed'
  shape: 'circle' | 'rect'
  position: Vec2
  radius?: number
  size?: Vec2
}

export interface MapDef {
  id: MapId
  name: string
  size: Vec2
  startingBases: Record<PlayerId, { origin: Vec2; direction: 1 | -1 }>
  resources: ResourceNode[]
  blockers: TerrainBlocker[]
  zones: TerrainZone[]
  controlPoints: Array<{ id: string; position: Vec2; radius: number }>
  lanes: Vec2[][]
}

const SHARED_BASES: Record<PlayerId, { origin: Vec2; direction: 1 | -1 }> = {
  1: { origin: { x: 330, y: 1120 }, direction: 1 },
  2: { origin: { x: 1840, y: 360 }, direction: -1 },
}

const BLACK_IRON_RESOURCES: ResourceNode[] = [
  { id: 'ore-main-1a', position: { x: 255, y: 930 }, radius: 34, amount: 680, maxAmount: 680 },
  { id: 'ore-main-1b', position: { x: 515, y: 1230 }, radius: 34, amount: 680, maxAmount: 680 },
  { id: 'ore-natural-1', position: { x: 690, y: 1010 }, radius: 34, amount: 760, maxAmount: 760 },
  { id: 'ore-center-1', position: { x: 1015, y: 760 }, radius: 34, amount: 940, maxAmount: 940 },
  { id: 'ore-center-2', position: { x: 1215, y: 725 }, radius: 34, amount: 940, maxAmount: 940 },
  { id: 'ore-natural-2', position: { x: 1515, y: 505 }, radius: 34, amount: 760, maxAmount: 760 },
  { id: 'ore-main-2a', position: { x: 1690, y: 255 }, radius: 34, amount: 680, maxAmount: 680 },
  { id: 'ore-main-2b', position: { x: 1940, y: 560 }, radius: 34, amount: 680, maxAmount: 680 },
]

const BLACK_IRON_BLOCKERS: TerrainBlocker[] = [
  {
    id: 'north-bone-wall',
    kind: 'rect',
    position: { x: 855, y: 335 },
    size: { x: 410, y: 132 },
    color: 0x241d1a,
    accent: 0x6f5d48,
  },
  {
    id: 'west-abbey-ruin',
    kind: 'circle',
    position: { x: 735, y: 665 },
    radius: 118,
    color: 0x171b18,
    accent: 0x56614e,
  },
  {
    id: 'maw-west',
    kind: 'circle',
    position: { x: 990, y: 760 },
    radius: 82,
    color: 0x201116,
    accent: 0x95383d,
  },
  {
    id: 'maw-east',
    kind: 'circle',
    position: { x: 1222, y: 740 },
    radius: 82,
    color: 0x201116,
    accent: 0x95383d,
  },
  {
    id: 'south-ossuary',
    kind: 'rect',
    position: { x: 1320, y: 1080 },
    size: { x: 430, y: 138 },
    color: 0x221d18,
    accent: 0x6f5d48,
  },
  {
    id: 'east-fallen-tower',
    kind: 'circle',
    position: { x: 1600, y: 880 },
    radius: 110,
    color: 0x171b18,
    accent: 0x56614e,
  },
]

const OSSUARY_RESOURCES: ResourceNode[] = [
  { id: 'cross-main-1a', position: { x: 220, y: 960 }, radius: 34, amount: 720, maxAmount: 720 },
  { id: 'cross-main-1b', position: { x: 500, y: 1270 }, radius: 34, amount: 720, maxAmount: 720 },
  { id: 'cross-west', position: { x: 700, y: 790 }, radius: 34, amount: 820, maxAmount: 820 },
  { id: 'cross-north', position: { x: 1040, y: 405 }, radius: 34, amount: 900, maxAmount: 900 },
  { id: 'cross-south', position: { x: 1160, y: 1110 }, radius: 34, amount: 900, maxAmount: 900 },
  { id: 'cross-east', position: { x: 1510, y: 700 }, radius: 34, amount: 820, maxAmount: 820 },
  { id: 'cross-main-2a', position: { x: 1690, y: 220 }, radius: 34, amount: 720, maxAmount: 720 },
  { id: 'cross-main-2b', position: { x: 1970, y: 550 }, radius: 34, amount: 720, maxAmount: 720 },
]

const OSSUARY_BLOCKERS: TerrainBlocker[] = [
  {
    id: 'cross-west-wall',
    kind: 'rect',
    position: { x: 690, y: 1040 },
    size: { x: 300, y: 105 },
    color: 0x211d19,
    accent: 0x756753,
  },
  {
    id: 'cross-east-wall',
    kind: 'rect',
    position: { x: 1510, y: 470 },
    size: { x: 300, y: 105 },
    color: 0x211d19,
    accent: 0x756753,
  },
  {
    id: 'cross-center-tomb',
    kind: 'rect',
    position: { x: 1100, y: 750 },
    size: { x: 270, y: 185 },
    color: 0x281f1c,
    accent: 0x8b6d56,
  },
  {
    id: 'cross-north-cairn',
    kind: 'circle',
    position: { x: 1035, y: 235 },
    radius: 92,
    color: 0x1c211d,
    accent: 0x657061,
  },
  {
    id: 'cross-south-cairn',
    kind: 'circle',
    position: { x: 1170, y: 1265 },
    radius: 92,
    color: 0x1c211d,
    accent: 0x657061,
  },
  {
    id: 'cross-west-chapel',
    kind: 'circle',
    position: { x: 530, y: 520 },
    radius: 102,
    color: 0x191e1a,
    accent: 0x59645a,
  },
  {
    id: 'cross-east-chapel',
    kind: 'circle',
    position: { x: 1670, y: 980 },
    radius: 102,
    color: 0x191e1a,
    accent: 0x59645a,
  },
]

export const MAP_DEFS: Record<MapId, MapDef> = {
  'black-iron-ford': {
    id: 'black-iron-ford',
    name: 'The Black-Iron Ford',
    size: { x: 2200, y: 1500 },
    startingBases: SHARED_BASES,
    resources: BLACK_IRON_RESOURCES,
    blockers: BLACK_IRON_BLOCKERS,
    zones: [
      { id: 'ford', kind: 'ford', shape: 'rect', position: { x: 1110, y: 755 }, size: { x: 690, y: 235 } },
      { id: 'north-rise', kind: 'high-ground', shape: 'circle', position: { x: 1230, y: 430 }, radius: 205 },
      { id: 'south-rise', kind: 'high-ground', shape: 'circle', position: { x: 900, y: 1110 }, radius: 190 },
      { id: 'west-deadwood', kind: 'forest', shape: 'circle', position: { x: 520, y: 560 }, radius: 210 },
      { id: 'east-deadwood', kind: 'forest', shape: 'circle', position: { x: 1710, y: 790 }, radius: 220 },
      { id: 'maw-stain', kind: 'cursed', shape: 'circle', position: { x: 1100, y: 750 }, radius: 255 },
    ],
    controlPoints: [{ id: 'ford-shrine', position: { x: 1110, y: 750 }, radius: 92 }],
    lanes: [
      [
        { x: 250, y: 1090 },
        { x: 670, y: 1010 },
        { x: 990, y: 845 },
        { x: 1320, y: 710 },
        { x: 1810, y: 430 },
      ],
      [
        { x: 430, y: 1270 },
        { x: 740, y: 1180 },
        { x: 1030, y: 1015 },
        { x: 1440, y: 960 },
        { x: 1910, y: 585 },
      ],
      [
        { x: 360, y: 950 },
        { x: 620, y: 735 },
        { x: 930, y: 520 },
        { x: 1290, y: 475 },
        { x: 1700, y: 265 },
      ],
    ],
  },
  'ossuary-crossroads': {
    id: 'ossuary-crossroads',
    name: 'Ossuary Crossroads',
    size: { x: 2200, y: 1500 },
    startingBases: SHARED_BASES,
    resources: OSSUARY_RESOURCES,
    blockers: OSSUARY_BLOCKERS,
    zones: [
      { id: 'north-road', kind: 'high-ground', shape: 'rect', position: { x: 1110, y: 430 }, size: { x: 620, y: 180 } },
      { id: 'south-road', kind: 'high-ground', shape: 'rect', position: { x: 1090, y: 1080 }, size: { x: 620, y: 180 } },
      { id: 'west-grove', kind: 'forest', shape: 'circle', position: { x: 700, y: 760 }, radius: 190 },
      { id: 'east-grove', kind: 'forest', shape: 'circle', position: { x: 1500, y: 760 }, radius: 190 },
      { id: 'crossroads-curse', kind: 'cursed', shape: 'circle', position: { x: 1100, y: 750 }, radius: 300 },
    ],
    controlPoints: [
      { id: 'north-reliquary', position: { x: 1090, y: 430 }, radius: 86 },
      { id: 'south-reliquary', position: { x: 1110, y: 1070 }, radius: 86 },
    ],
    lanes: [
      [
        { x: 300, y: 1120 },
        { x: 720, y: 790 },
        { x: 1100, y: 620 },
        { x: 1500, y: 540 },
        { x: 1840, y: 360 },
      ],
      [
        { x: 330, y: 1180 },
        { x: 760, y: 1110 },
        { x: 1100, y: 900 },
        { x: 1430, y: 750 },
        { x: 1880, y: 420 },
      ],
      [
        { x: 420, y: 980 },
        { x: 760, y: 560 },
        { x: 1100, y: 430 },
        { x: 1510, y: 500 },
        { x: 1780, y: 300 },
      ],
    ],
  },
}

export const MAP_SIZE: Vec2 = MAP_DEFS['black-iron-ford'].size
export const STARTING_BASES = MAP_DEFS['black-iron-ford'].startingBases
export const RESOURCE_SEAMS = MAP_DEFS['black-iron-ford'].resources
export const TERRAIN_BLOCKERS = MAP_DEFS['black-iron-ford'].blockers

export function getMapDef(mapId: MapId): MapDef {
  return MAP_DEFS[mapId]
}

export function isTerrainBlocked(
  position: Vec2,
  radius: number,
  padding = 0,
  mapId: MapId = 'black-iron-ford',
): boolean {
  return getMapDef(mapId).blockers.some((blocker) => circleHitsBlocker(position, radius + padding, blocker))
}

export function circleHitsBlocker(position: Vec2, radius: number, blocker: TerrainBlocker): boolean {
  if (blocker.kind === 'circle') {
    const blockerRadius = blocker.radius ?? 0
    return Math.hypot(position.x - blocker.position.x, position.y - blocker.position.y) < radius + blockerRadius
  }

  const size = blocker.size ?? { x: 0, y: 0 }
  const halfX = size.x / 2
  const halfY = size.y / 2
  const closestX = clamp(position.x, blocker.position.x - halfX, blocker.position.x + halfX)
  const closestY = clamp(position.y, blocker.position.y - halfY, blocker.position.y + halfY)
  return Math.hypot(position.x - closestX, position.y - closestY) < radius
}

export function isInTerrainZone(position: Vec2, zone: TerrainZone): boolean {
  if (zone.shape === 'circle') {
    return Math.hypot(position.x - zone.position.x, position.y - zone.position.y) <= (zone.radius ?? 0)
  }
  const size = zone.size ?? { x: 0, y: 0 }
  return (
    Math.abs(position.x - zone.position.x) <= size.x / 2 &&
    Math.abs(position.y - zone.position.y) <= size.y / 2
  )
}

export function hasTerrainKind(position: Vec2, kind: TerrainZone['kind'], mapId: MapId): boolean {
  return getMapDef(mapId).zones.some((zone) => zone.kind === kind && isInTerrainZone(position, zone))
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value))
}
