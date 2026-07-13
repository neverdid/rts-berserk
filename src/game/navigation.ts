import * as PF from 'pathfinding'
import { isTerrainBlocked } from './map'
import type { GameState, Vec2 } from './types'

const CELL_SIZE = 50
const staticMatrices = new Map<string, number[][]>()

export function findNavigationPath(
  state: GameState,
  start: Vec2,
  target: Vec2,
  radius: number,
  movingEntityId?: string,
): Vec2[] {
  const columns = Math.ceil(state.mapSize.x / CELL_SIZE)
  const rows = Math.ceil(state.mapSize.y / CELL_SIZE)
  const matrix = getStaticMatrix(state, radius, columns, rows).map((row) => [...row])

  state.entities.forEach((entity) => {
    if (entity.id === movingEntityId || entity.kind !== 'building' || entity.hp <= 0) {
      return
    }
    const padding = entity.radius + radius + 6
    const minX = Math.max(0, Math.floor((entity.position.x - padding) / CELL_SIZE))
    const maxX = Math.min(columns - 1, Math.floor((entity.position.x + padding) / CELL_SIZE))
    const minY = Math.max(0, Math.floor((entity.position.y - padding) / CELL_SIZE))
    const maxY = Math.min(rows - 1, Math.floor((entity.position.y + padding) / CELL_SIZE))
    for (let y = minY; y <= maxY; y += 1) {
      for (let x = minX; x <= maxX; x += 1) {
        const center = cellCenter(x, y)
        if (Math.hypot(center.x - entity.position.x, center.y - entity.position.y) < padding) {
          matrix[y][x] = 1
        }
      }
    }
  })

  const startCell = toCell(start, columns, rows)
  const targetCell = nearestWalkableCell(matrix, toCell(target, columns, rows))
  matrix[startCell.y][startCell.x] = 0
  matrix[targetCell.y][targetCell.x] = 0

  const baseGrid = new PF.Grid(matrix)
  const finder = new PF.AStarFinder({
    diagonalMovement: PF.DiagonalMovement.OnlyWhenNoObstacles,
    heuristic: PF.Heuristic.octile,
  })
  const raw = finder.findPath(startCell.x, startCell.y, targetCell.x, targetCell.y, baseGrid.clone())
  if (raw.length <= 1) {
    return [{ ...target }]
  }

  const compressed = PF.Util.compressPath(raw)
  const path = compressed.slice(1).map(([x, y]) => cellCenter(x, y))
  const finalTargetBlocked = isTerrainBlocked(target, radius, 2, state.mapId)
  if (!finalTargetBlocked) {
    path[path.length - 1] = { ...target }
  }
  return path
}

export function clearNavigationCache(): void {
  staticMatrices.clear()
}

function getStaticMatrix(state: GameState, radius: number, columns: number, rows: number): number[][] {
  const radiusBucket = Math.ceil(radius / 6) * 6
  const key = `${state.mapId}:${radiusBucket}`
  const cached = staticMatrices.get(key)
  if (cached) {
    return cached
  }
  const matrix: number[][] = []
  for (let y = 0; y < rows; y += 1) {
    const row: number[] = []
    for (let x = 0; x < columns; x += 1) {
      const center = cellCenter(x, y)
      const outside =
        center.x < radius ||
        center.y < radius ||
        center.x > state.mapSize.x - radius ||
        center.y > state.mapSize.y - radius
      row.push(outside || isTerrainBlocked(center, radius, 5, state.mapId) ? 1 : 0)
    }
    matrix.push(row)
  }
  staticMatrices.set(key, matrix)
  return matrix
}

function nearestWalkableCell(matrix: number[][], origin: { x: number; y: number }): { x: number; y: number } {
  if (matrix[origin.y]?.[origin.x] === 0) {
    return origin
  }
  for (let ring = 1; ring <= 6; ring += 1) {
    for (let y = origin.y - ring; y <= origin.y + ring; y += 1) {
      for (let x = origin.x - ring; x <= origin.x + ring; x += 1) {
        if (y < 0 || x < 0 || y >= matrix.length || x >= matrix[0].length) {
          continue
        }
        if ((Math.abs(x - origin.x) === ring || Math.abs(y - origin.y) === ring) && matrix[y][x] === 0) {
          return { x, y }
        }
      }
    }
  }
  return origin
}

function toCell(position: Vec2, columns: number, rows: number): { x: number; y: number } {
  return {
    x: clamp(Math.floor(position.x / CELL_SIZE), 0, columns - 1),
    y: clamp(Math.floor(position.y / CELL_SIZE), 0, rows - 1),
  }
}

function cellCenter(x: number, y: number): Vec2 {
  return { x: x * CELL_SIZE + CELL_SIZE / 2, y: y * CELL_SIZE + CELL_SIZE / 2 }
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value))
}
