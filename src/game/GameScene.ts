import Phaser from 'phaser'
import { RACE_DEFS, getEntityDef } from './catalog'
import { RESOURCE_SEAMS, TERRAIN_BLOCKERS, type TerrainBlocker } from './map'
import {
  canPlaceBuilding,
  createInitialState,
  distance,
  enemyOf,
  getEntity,
  issueAttack,
  issueGather,
  issueMove,
  isEntityVisibleTo,
  isPositionVisibleTo,
  setActivePlayer,
  startBuilding,
  startProduction,
  updateSimulation,
} from './simulation'
import { STORY_BRIEFING } from './story'
import type { BuildingType, Entity, GameMode, GameState, PlayerId, RaceId, ResourceNode, UnitType, Vec2 } from './types'

interface EntityView {
  shadow: Phaser.GameObjects.Ellipse
  ward: Phaser.GameObjects.Arc
  body: Phaser.GameObjects.Shape
  bevel: Phaser.GameObjects.Shape
  hpBack: Phaser.GameObjects.Rectangle
  hpFill: Phaser.GameObjects.Rectangle
  selection: Phaser.GameObjects.Shape
  tag: Phaser.GameObjects.Text
}

interface ResourceView {
  shadow: Phaser.GameObjects.Ellipse
  glow: Phaser.GameObjects.Arc
  body: Phaser.GameObjects.Polygon
  label: Phaser.GameObjects.Text
}

interface UiSnapshot {
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
  }>
  events: GameState['events']
  objective: string
  buildPreview: BuildingType | null
  ruinTide: number
  activeRace: RaceId
  races: Array<{ id: RaceId; name: string; tagline: string; style: string }>
  commands: {
    train: Record<UnitType, { label: string; cost: number }>
    build: Record<BuildingType, { label: string; cost: number }>
  }
}

export class GameScene extends Phaser.Scene {
  private state: GameState = createInitialState('story')
  private entityViews = new Map<string, EntityView>()
  private resourceViews = new Map<string, ResourceView>()
  private selectedIds: string[] = []
  private dragStart: Vec2 | null = null
  private selectionRect?: Phaser.GameObjects.Rectangle
  private terrain?: Phaser.GameObjects.Graphics
  private fog?: Phaser.GameObjects.Graphics
  private minimap?: Phaser.GameObjects.Graphics
  private lastSnapshot = 0
  private buildPreview: BuildingType | null = null
  private missionStarted = false
  private selectedRace: RaceId = 'candlebound'
  private placementGhost?: Phaser.GameObjects.Rectangle
  private cursors?: Phaser.Types.Input.Keyboard.CursorKeys
  private keys?: Record<string, Phaser.Input.Keyboard.Key>

  constructor() {
    super('game')
  }

  create(): void {
    this.state = createInitialState('story', this.selectedRace, this.opponentRaceFor(this.selectedRace))
    this.missionStarted = false
    this.cameras.main.setBounds(0, 0, this.state.mapSize.x, this.state.mapSize.y)
    this.cameras.main.setZoom(0.78)
    this.frameStartingBase()

    this.drawTerrain()
    this.createInput()
    this.listenForUi()
    this.publishSnapshot()
  }

  update(_time: number, delta: number): void {
    if (this.missionStarted || this.state.mode === 'pvp') {
      updateSimulation(this.state, delta / 1000)
    }
    this.updateCamera(delta / 1000)
    this.renderResources()
    this.renderEntities()
    this.updateSelectionRectangle()
    this.updatePlacementGhost()
    this.renderFog()
    this.renderMinimap()

    this.lastSnapshot += delta
    if (this.lastSnapshot > 160) {
      this.publishSnapshot()
      this.lastSnapshot = 0
    }
  }

  private createInput(): void {
    this.input.mouse?.disableContextMenu()
    this.cursors = this.input.keyboard?.createCursorKeys()
    this.keys = this.input.keyboard?.addKeys('W,A,S,D,Q,E,R,TAB,ESC,H') as Record<string, Phaser.Input.Keyboard.Key>

    this.input.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      if (pointer.rightButtonDown()) {
        this.issueContextCommand(pointer)
        return
      }

      const world = this.pointerWorld(pointer)
      if (this.buildPreview) {
        this.placePreviewBuilding(world)
        return
      }

      this.dragStart = world
      this.selectionRect?.destroy()
      this.selectionRect = this.add.rectangle(world.x, world.y, 1, 1, 0xc4a85f, 0.08)
      this.selectionRect.setStrokeStyle(2, 0xe8d28d, 0.88)
      this.selectionRect.setDepth(900)
    })

    this.input.on('pointerup', (pointer: Phaser.Input.Pointer) => {
      if (!this.dragStart || pointer.rightButtonReleased()) {
        return
      }
      const end = this.pointerWorld(pointer)
      const dragDistance = distance(this.dragStart, end)
      const additive = (pointer.event as MouseEvent | undefined)?.shiftKey ?? false
      if (dragDistance < 8) {
        this.selectAt(end, additive)
      } else {
        this.selectInRect(this.dragStart, end, additive)
      }
      this.dragStart = null
      this.selectionRect?.destroy()
      this.selectionRect = undefined
      this.publishSnapshot()
    })

    this.input.on('wheel', (_pointer: Phaser.Input.Pointer, _objects: unknown, _dx: number, dy: number) => {
      const camera = this.cameras.main
      camera.setZoom(Phaser.Math.Clamp(camera.zoom + (dy > 0 ? -0.08 : 0.08), 0.52, 1.24))
    })

    this.input.keyboard?.on('keydown-TAB', (event: KeyboardEvent) => {
      event.preventDefault()
      if (this.state.mode === 'pvp') {
        const next = enemyOf(this.state.activePlayer)
        setActivePlayer(this.state, next)
        this.selectedIds = []
        this.publishSnapshot()
      }
    })

    this.input.keyboard?.on('keydown-ESC', () => {
      this.setBuildPreview(null)
      this.selectedIds = []
      this.publishSnapshot()
    })

    this.input.keyboard?.on('keydown-Q', () => this.trainSelected('worker'))
    this.input.keyboard?.on('keydown-E', () => this.trainSelected('vanguard'))
    this.input.keyboard?.on('keydown-R', () => this.trainSelected('skirmisher'))
    this.input.keyboard?.on('keydown-H', () => this.focusHome())
  }

  private listenForUi(): void {
    window.addEventListener('rts:command', (event) => {
      const detail = (event as CustomEvent).detail as
        | { type: 'mode'; mode: GameMode }
        | { type: 'start' }
        | { type: 'race'; race: RaceId }
        | { type: 'restart' }
        | { type: 'train'; unitType: UnitType }
        | { type: 'build'; buildingType: BuildingType }
        | { type: 'cancel-build' }
        | { type: 'focus-home' }

      if (detail.type === 'start') {
        this.missionStarted = true
        this.publishSnapshot()
      }
      if (detail.type === 'mode') {
        this.restart(detail.mode)
      }
      if (detail.type === 'race') {
        this.selectedRace = detail.race
        this.restart(this.state.mode)
      }
      if (detail.type === 'restart') {
        this.restart(this.state.mode)
      }
      if (detail.type === 'train') {
        this.trainSelected(detail.unitType)
      }
      if (detail.type === 'build') {
        this.setBuildPreview(detail.buildingType)
        this.publishSnapshot()
      }
      if (detail.type === 'cancel-build') {
        this.setBuildPreview(null)
        this.publishSnapshot()
      }
      if (detail.type === 'focus-home') {
        this.focusHome()
      }
    })
  }

  private restart(mode: GameMode): void {
    this.state = createInitialState(mode, this.selectedRace, this.opponentRaceFor(this.selectedRace))
    this.missionStarted = mode !== 'story'
    this.selectedIds = []
    this.setBuildPreview(null)
    this.entityViews.forEach((view) => {
      view.shadow.destroy()
      view.ward.destroy()
      view.body.destroy()
      view.bevel.destroy()
      view.hpBack.destroy()
      view.hpFill.destroy()
      view.selection.destroy()
      view.tag.destroy()
    })
    this.resourceViews.forEach((view) => {
      view.shadow.destroy()
      view.glow.destroy()
      view.body.destroy()
      view.label.destroy()
    })
    this.entityViews.clear()
    this.resourceViews.clear()
    this.fog?.clear()
    this.drawTerrain()
    this.frameStartingBase()
    this.publishSnapshot()
  }

  private drawTerrain(): void {
    this.terrain?.destroy()
    const graphics = this.add.graphics()
    graphics.setDepth(-100)
    graphics.fillStyle(0x0c1110, 1)
    graphics.fillRect(0, 0, this.state.mapSize.x, this.state.mapSize.y)

    this.drawLane(graphics, [
      { x: 250, y: 1090 },
      { x: 670, y: 1010 },
      { x: 990, y: 845 },
      { x: 1320, y: 710 },
      { x: 1810, y: 430 },
    ], 94, 0x19221e, 0.82)
    this.drawLane(graphics, [
      { x: 430, y: 1270 },
      { x: 740, y: 1180 },
      { x: 1030, y: 1015 },
      { x: 1440, y: 960 },
      { x: 1910, y: 585 },
    ], 68, 0x141d1a, 0.72)
    this.drawLane(graphics, [
      { x: 360, y: 950 },
      { x: 620, y: 735 },
      { x: 930, y: 520 },
      { x: 1290, y: 475 },
      { x: 1700, y: 265 },
    ], 68, 0x141d1a, 0.7)

    graphics.lineStyle(1, 0x2c332e, 0.24)
    for (let x = 0; x <= this.state.mapSize.x; x += 80) {
      graphics.lineBetween(x, 0, x, this.state.mapSize.y)
    }
    for (let y = 0; y <= this.state.mapSize.y; y += 80) {
      graphics.lineBetween(0, y, this.state.mapSize.x, y)
    }

    this.drawBaseGround(graphics, { x: 330, y: 1120 }, 0xc4a85f)
    this.drawBaseGround(graphics, { x: 1840, y: 360 }, 0x95383d)
    this.drawGroundTexture(graphics)

    graphics.fillStyle(0x171f1a, 0.82)
    graphics.fillEllipse(1110, 760, 760, 350)
    graphics.fillStyle(0x241217, 0.62)
    graphics.fillEllipse(1110, 750, 390, 172)

    graphics.lineStyle(7, 0x95383d, 0.24)
    graphics.strokeEllipse(1110, 755, 910, 475)
    graphics.lineStyle(3, 0xc4a85f, 0.16)
    graphics.strokeEllipse(1110, 755, 575, 290)

    RESOURCE_SEAMS.forEach((resource) => {
      graphics.fillStyle(0x1c2420, 0.78)
      graphics.fillEllipse(resource.position.x, resource.position.y + 8, 96, 54)
      graphics.lineStyle(2, 0x6f5d48, 0.3)
      graphics.strokeEllipse(resource.position.x, resource.position.y + 8, 108, 64)
    })

    TERRAIN_BLOCKERS.forEach((blocker) => this.drawTerrainBlocker(graphics, blocker))
    this.drawDeadWoods(graphics)
    this.terrain = graphics
  }

  private drawLane(graphics: Phaser.GameObjects.Graphics, points: Vec2[], width: number, color: number, alpha: number): void {
    graphics.lineStyle(width, color, alpha)
    for (let index = 1; index < points.length; index += 1) {
      graphics.lineBetween(points[index - 1].x, points[index - 1].y, points[index].x, points[index].y)
    }
    graphics.lineStyle(Math.max(2, width * 0.06), 0x56614e, alpha * 0.28)
    for (let index = 1; index < points.length; index += 1) {
      graphics.lineBetween(points[index - 1].x, points[index - 1].y, points[index].x, points[index].y)
    }
  }

  private drawBaseGround(graphics: Phaser.GameObjects.Graphics, position: Vec2, accent: number): void {
    graphics.fillStyle(0x18221f, 0.9)
    graphics.fillEllipse(position.x, position.y, 510, 315)
    graphics.lineStyle(4, accent, 0.18)
    graphics.strokeEllipse(position.x, position.y, 560, 350)
    graphics.lineStyle(2, 0x6f5d48, 0.28)
    graphics.strokeEllipse(position.x, position.y, 360, 225)
  }

  private drawTerrainBlocker(graphics: Phaser.GameObjects.Graphics, blocker: TerrainBlocker): void {
    graphics.fillStyle(0x050706, 0.36)
    if (blocker.kind === 'circle') {
      const radius = blocker.radius ?? 0
      graphics.fillEllipse(blocker.position.x + 12, blocker.position.y + 18, radius * 2.05, radius * 1.18)
      graphics.fillStyle(blocker.color, 0.96)
      graphics.fillCircle(blocker.position.x, blocker.position.y, radius)
      graphics.lineStyle(4, blocker.accent, 0.42)
      graphics.strokeCircle(blocker.position.x, blocker.position.y, radius)
      graphics.lineStyle(2, 0x0b0f0e, 0.75)
      graphics.lineBetween(blocker.position.x - radius * 0.55, blocker.position.y, blocker.position.x + radius * 0.5, blocker.position.y - radius * 0.38)
      graphics.lineBetween(blocker.position.x - radius * 0.2, blocker.position.y + radius * 0.52, blocker.position.x + radius * 0.48, blocker.position.y + radius * 0.1)
      return
    }

    const size = blocker.size ?? { x: 0, y: 0 }
    const left = blocker.position.x - size.x / 2
    const top = blocker.position.y - size.y / 2
    graphics.fillRect(left + 14, top + 18, size.x, size.y)
    graphics.fillStyle(blocker.color, 0.98)
    graphics.fillRect(left, top, size.x, size.y)
    graphics.lineStyle(4, blocker.accent, 0.42)
    graphics.strokeRect(left, top, size.x, size.y)
    graphics.lineStyle(3, 0x0a0f0d, 0.75)
    for (let x = left + 42; x < left + size.x; x += 86) {
      graphics.lineBetween(x, top + 12, x + 42, top + size.y - 12)
    }
  }

  private drawDeadWoods(graphics: Phaser.GameObjects.Graphics): void {
    graphics.lineStyle(2, 0x56614e, 0.24)
    for (let index = 0; index < 34; index += 1) {
      const x = 120 + index * 61
      const y = 165 + ((index * 97) % 1120)
      if (x > 625 && x < 1580 && y > 460 && y < 1040) {
        continue
      }
      graphics.lineBetween(x, y, x + 24, y - 54)
      graphics.lineBetween(x, y, x - 18, y - 38)
      graphics.lineBetween(x + 5, y - 28, x + 28, y - 40)
    }
  }

  private drawGroundTexture(graphics: Phaser.GameObjects.Graphics): void {
    for (let index = 0; index < 70; index += 1) {
      const x = 80 + ((index * 173) % (this.state.mapSize.x - 160))
      const y = 90 + ((index * 251) % (this.state.mapSize.y - 180))
      if ((x > 180 && x < 520 && y > 900 && y < 1320) || (x > 1600 && x < 1990 && y > 220 && y < 610)) {
        continue
      }
      const width = 18 + (index % 5) * 7
      const height = 7 + (index % 4) * 3
      graphics.fillStyle(0x050706, 0.18)
      graphics.fillEllipse(x + 4, y + 5, width, height)
      graphics.lineStyle(1, 0x6f5d48, 0.13)
      graphics.lineBetween(x - width * 0.45, y, x + width * 0.35, y - height * 0.3)
      graphics.lineStyle(1, 0x020303, 0.2)
      graphics.lineBetween(x - width * 0.3, y + height * 0.42, x + width * 0.5, y + height * 0.12)
    }
  }

  private renderResources(): void {
    this.state.resources.forEach((resource) => {
      let view = this.resourceViews.get(resource.id)
      if (!view) {
        const shadow = this.add.ellipse(resource.position.x, resource.position.y + 12, 82, 42, 0x050706, 0.34)
        shadow.setDepth(0)
        const glow = this.add.circle(resource.position.x, resource.position.y, 46, 0xc4a85f, 0.08)
        glow.setDepth(1)
        const points = [0, -38, 16, -14, 32, 0, 14, 18, 0, 38, -16, 14, -32, 0, -14, -18]
        const body = this.add.polygon(resource.position.x, resource.position.y, points, 0x9a8c76, 0.86)
        body.setStrokeStyle(2, 0xd8caa7, 0.72)
        body.setDepth(2)
        const label = this.add.text(resource.position.x, resource.position.y + 44, '', {
          fontFamily: 'Segoe UI, sans-serif',
          fontSize: '13px',
          color: '#d8caa7',
        })
        label.setOrigin(0.5)
        label.setDepth(3)
        view = { shadow, glow, body, label }
        this.resourceViews.set(resource.id, view)
      }
      const alpha = Phaser.Math.Clamp(resource.amount / resource.maxAmount, 0.14, 0.9)
      const visible = isPositionVisibleTo(this.state, resource.position, this.state.activePlayer, resource.radius)
      view.shadow.setVisible(visible)
      view.shadow.setAlpha(visible ? 0.32 : 0)
      view.glow.setVisible(visible)
      view.glow.setAlpha(visible ? 0.08 + alpha * 0.12 : 0)
      view.body.setAlpha(visible ? alpha : 0.08)
      view.label.setText(`${Math.round(resource.amount)}`)
      view.label.setVisible(visible)
      view.label.setAlpha(resource.amount > 0 ? 0.86 : 0.25)
    })
  }

  private renderEntities(): void {
    const liveIds = new Set(this.state.entities.map((entity) => entity.id))
    this.entityViews.forEach((view, id) => {
      if (!liveIds.has(id)) {
        view.shadow.destroy()
        view.ward.destroy()
        view.body.destroy()
        view.hpBack.destroy()
        view.hpFill.destroy()
        view.selection.destroy()
        view.tag.destroy()
        this.entityViews.delete(id)
      }
    })

    this.state.entities.forEach((entity) => {
      let view = this.entityViews.get(entity.id)
      if (!view) {
        view = this.createEntityView(entity)
        this.entityViews.set(entity.id, view)
      }

      const isSelected = this.selectedIds.includes(entity.id)
      const player = this.state.players[entity.owner]
      const visible = isEntityVisibleTo(this.state, entity, this.state.activePlayer)
      const alpha = entity.underConstruction ? 0.55 : entity.resolve < 55 && entity.kind === 'unit' ? 0.78 : 1
      view.shadow.setPosition(entity.position.x, entity.position.y + entity.radius * 0.34)
      view.shadow.setVisible(visible)
      view.shadow.setAlpha(entity.kind === 'building' ? 0.38 : 0.28)
      view.ward.setPosition(entity.position.x, entity.position.y)
      view.ward.setVisible(visible && entity.ward > 0)
      view.ward.setStrokeStyle(1, player.accent, entity.owner === this.state.activePlayer ? 0.26 : 0.13)
      view.body.setPosition(entity.position.x, entity.position.y)
      view.body.setVisible(visible)
      view.body.setFillStyle(player.color, alpha)
      view.body.setStrokeStyle(isSelected ? 4 : 2, isSelected ? player.accent : 0x0f1212, isSelected ? 1 : 0.7)
      view.bevel.setPosition(entity.position.x - entity.radius * 0.16, entity.position.y - entity.radius * 0.2)
      view.bevel.setVisible(visible)
      view.bevel.setFillStyle(player.accent, entity.underConstruction ? 0.2 : 0.34)
      view.selection.setPosition(entity.position.x, entity.position.y)
      view.selection.setVisible(isSelected && visible)
      view.hpBack.setPosition(entity.position.x, entity.position.y - entity.radius - 14)
      view.hpBack.setVisible(visible)
      view.hpFill.setPosition(entity.position.x - 22 + 22 * (entity.hp / entity.maxHp), entity.position.y - entity.radius - 14)
      view.hpFill.setVisible(visible)
      view.hpFill.setSize(44 * Phaser.Math.Clamp(entity.hp / entity.maxHp, 0, 1), 5)
      view.hpFill.setFillStyle(entity.kind === 'unit' && entity.resolve < 60 ? 0xd9af57 : 0xa6df6f, 0.92)
      view.tag.setPosition(entity.position.x, entity.position.y + entity.radius + 10)
      view.tag.setVisible(visible)
      view.tag.setText(entity.type === 'worker' && entity.carrying > 0 ? `${entity.label} +${entity.carrying}` : entity.label)
    })
  }

  private renderFog(): void {
    if (!this.fog) {
      this.fog = this.add.graphics()
      this.fog.setDepth(84)
    }

    const tile = 80
    this.fog.clear()
    this.fog.fillStyle(0x050706, 0.5)

    for (let x = 0; x < this.state.mapSize.x; x += tile) {
      for (let y = 0; y < this.state.mapSize.y; y += tile) {
        const center = { x: x + tile / 2, y: y + tile / 2 }
        if (!isPositionVisibleTo(this.state, center, this.state.activePlayer, tile * 0.9)) {
          this.fog.fillRect(x, y, tile + 1, tile + 1)
        }
      }
    }
  }

  private renderMinimap(): void {
    if (!this.minimap) {
      this.minimap = this.add.graphics()
      this.minimap.setScrollFactor(0)
      this.minimap.setDepth(980)
    }

    const width = this.scale.width < 560 ? 116 : 158
    const height = width * (this.state.mapSize.y / this.state.mapSize.x)
    const x = this.scale.width - width - 12
    const y = 12
    const scaleX = width / this.state.mapSize.x
    const scaleY = height / this.state.mapSize.y
    const toMini = (position: Vec2): Vec2 => ({ x: x + position.x * scaleX, y: y + position.y * scaleY })

    this.minimap.clear()
    this.minimap.fillStyle(0x08100f, 0.84)
    this.minimap.fillRect(x, y, width, height)
    this.minimap.lineStyle(1, 0x6f5d48, 0.72)
    this.minimap.strokeRect(x, y, width, height)

    this.minimap.fillStyle(0x241d1a, 0.86)
    TERRAIN_BLOCKERS.forEach((blocker) => {
      const mini = toMini(blocker.position)
      if (blocker.kind === 'circle') {
        this.minimap?.fillCircle(mini.x, mini.y, (blocker.radius ?? 0) * scaleX)
        return
      }
      const size = blocker.size ?? { x: 0, y: 0 }
      this.minimap?.fillRect(mini.x - size.x * scaleX / 2, mini.y - size.y * scaleY / 2, size.x * scaleX, size.y * scaleY)
    })

    this.state.resources.forEach((resource) => {
      const visible = isPositionVisibleTo(this.state, resource.position, this.state.activePlayer, resource.radius)
      const mini = toMini(resource.position)
      this.minimap?.fillStyle(0xc4a85f, visible ? 0.88 : 0.24)
      this.minimap?.fillCircle(mini.x, mini.y, visible ? 2.4 : 1.8)
    })

    this.state.entities.forEach((entity) => {
      if (!isEntityVisibleTo(this.state, entity, this.state.activePlayer)) {
        return
      }
      const mini = toMini(entity.position)
      const player = this.state.players[entity.owner]
      const radius = entity.kind === 'building' ? 3.2 : 2.2
      this.minimap?.fillStyle(player.color, entity.underConstruction ? 0.48 : 0.94)
      this.minimap?.fillCircle(mini.x, mini.y, radius)
      if (this.selectedIds.includes(entity.id)) {
        this.minimap?.lineStyle(1, player.accent, 0.95)
        this.minimap?.strokeCircle(mini.x, mini.y, radius + 2.4)
      }
    })

    const view = this.cameras.main.worldView
    this.minimap.lineStyle(1, 0xf3e6bf, 0.82)
    this.minimap.strokeRect(x + view.x * scaleX, y + view.y * scaleY, view.width * scaleX, view.height * scaleY)
  }

  private createEntityView(entity: Entity): EntityView {
    const player = this.state.players[entity.owner]
    const shadow = this.add.ellipse(
      entity.position.x,
      entity.position.y + entity.radius * 0.34,
      entity.radius * (entity.kind === 'building' ? 2.08 : 1.82),
      entity.radius * 0.78,
      0x020303,
      entity.kind === 'building' ? 0.38 : 0.28,
    )
    shadow.setDepth(entity.kind === 'building' ? 8 : 18)

    const wardRadius = Math.max(entity.radius + 13, Math.min(92, entity.radius + entity.ward * 1.35))
    const ward = this.add.circle(entity.position.x, entity.position.y, wardRadius, 0xffffff, 0)
    ward.setStrokeStyle(1, player.accent, 0.2)
    ward.setVisible(false)
    ward.setDepth(entity.kind === 'building' ? 9 : 19)

    const body = this.createEntityBody(entity, player.color)
    body.setDepth(entity.kind === 'building' ? 10 : 20)
    const bevel = this.createEntityBevel(entity, player.accent)
    bevel.setDepth(entity.kind === 'building' ? 11 : 21)

    const selection =
      entity.kind === 'building'
        ? this.add.rectangle(entity.position.x, entity.position.y, entity.radius * 2.12, entity.radius * 2.12)
        : this.add.circle(entity.position.x, entity.position.y, entity.radius + 7)
    selection.setStrokeStyle(2, player.accent, 0.9)
    selection.setFillStyle(0xffffff, 0)
    selection.setVisible(false)
    selection.setDepth(30)

    const hpBack = this.add.rectangle(entity.position.x, entity.position.y - entity.radius - 14, 48, 7, 0x060808, 0.82)
    hpBack.setDepth(40)
    const hpFill = this.add.rectangle(entity.position.x, entity.position.y - entity.radius - 14, 44, 5, 0xa6df6f, 0.92)
    hpFill.setDepth(41)

    const tag = this.add.text(entity.position.x, entity.position.y + entity.radius + 10, entity.label, {
      fontFamily: 'Segoe UI, sans-serif',
      fontSize: entity.kind === 'building' ? '13px' : '12px',
      color: '#d9f5ed',
      backgroundColor: 'rgba(6, 10, 10, 0.55)',
      padding: { x: 4, y: 2 },
    })
    tag.setOrigin(0.5, 0)
    tag.setDepth(45)

    return { shadow, ward, body, bevel, hpBack, hpFill, selection, tag }
  }

  private createEntityBevel(entity: Entity, color: number): Phaser.GameObjects.Shape {
    if (entity.type === 'command') {
      return this.add.rectangle(entity.position.x, entity.position.y, entity.radius * 0.9, entity.radius * 0.5, color, 0.34)
    }
    if (entity.type === 'barracks') {
      return this.add.polygon(entity.position.x, entity.position.y, [-20, -12, 10, -18, 22, 6, -8, 16, -24, 0], color, 0.34)
    }
    if (entity.type === 'turret') {
      return this.add.polygon(entity.position.x, entity.position.y, [0, -18, 15, 10, 0, 18, -15, 10], color, 0.34)
    }
    if (entity.type === 'vanguard') {
      return this.add.polygon(entity.position.x, entity.position.y, [0, -10, 9, -4, 8, 5, 0, 10, -8, 5, -9, -4], color, 0.34)
    }
    if (entity.type === 'skirmisher') {
      return this.add.triangle(entity.position.x, entity.position.y, 0, -9, 10, 8, -10, 8, color, 0.34)
    }
    return this.add.ellipse(entity.position.x, entity.position.y, entity.radius * 0.95, entity.radius * 0.62, color, 0.34)
  }

  private createEntityBody(entity: Entity, color: number): Phaser.GameObjects.Shape {
    if (entity.type === 'command') {
      return this.add.rectangle(entity.position.x, entity.position.y, entity.radius * 1.72, entity.radius * 1.72, color, 1)
    }
    if (entity.type === 'barracks') {
      return this.add.polygon(
        entity.position.x,
        entity.position.y,
        [-36, -26, 28, -34, 42, 26, -20, 38, -42, 8],
        color,
        1,
      )
    }
    if (entity.type === 'turret') {
      return this.add.polygon(entity.position.x, entity.position.y, [0, -34, 30, 20, 0, 34, -30, 20], color, 1)
    }
    if (entity.type === 'vanguard') {
      return this.add.polygon(entity.position.x, entity.position.y, [0, -17, 15, -8, 15, 8, 0, 17, -15, 8, -15, -8], color, 1)
    }
    if (entity.type === 'skirmisher') {
      return this.add.triangle(entity.position.x, entity.position.y, 0, -17, 18, 15, -18, 15, color, 1)
    }
    return this.add.circle(entity.position.x, entity.position.y, entity.radius, color, 1)
  }

  private updateSelectionRectangle(): void {
    if (!this.dragStart || !this.selectionRect) {
      return
    }
    const pointer = this.input.activePointer
    const world = this.pointerWorld(pointer)
    const x = (this.dragStart.x + world.x) / 2
    const y = (this.dragStart.y + world.y) / 2
    const width = Math.abs(this.dragStart.x - world.x)
    const height = Math.abs(this.dragStart.y - world.y)
    this.selectionRect.setPosition(x, y)
    this.selectionRect.setSize(width, height)
  }

  private selectAt(world: Vec2, additive: boolean): void {
    const entity = this.findEntityAt(world)
    if (!additive) {
      this.selectedIds = []
    }
    if (entity && entity.owner === this.state.activePlayer && isEntityVisibleTo(this.state, entity, this.state.activePlayer)) {
      if (additive && this.selectedIds.includes(entity.id)) {
        this.selectedIds = this.selectedIds.filter((id) => id !== entity.id)
      } else {
        this.selectedIds.push(entity.id)
      }
    }
  }

  private selectInRect(start: Vec2, end: Vec2, additive: boolean): void {
    if (!additive) {
      this.selectedIds = []
    }
    const left = Math.min(start.x, end.x)
    const right = Math.max(start.x, end.x)
    const top = Math.min(start.y, end.y)
    const bottom = Math.max(start.y, end.y)

    const units = this.state.entities.filter(
      (entity) =>
        entity.owner === this.state.activePlayer &&
        isEntityVisibleTo(this.state, entity, this.state.activePlayer) &&
        entity.kind === 'unit' &&
        entity.position.x >= left &&
        entity.position.x <= right &&
        entity.position.y >= top &&
        entity.position.y <= bottom,
    )

    units.forEach((unit) => {
      if (!this.selectedIds.includes(unit.id)) {
        this.selectedIds.push(unit.id)
      }
    })
  }

  private issueContextCommand(pointer: Phaser.Input.Pointer): void {
    const world = this.pointerWorld(pointer)
    const entity = this.findEntityAt(world)
    const resource = this.findResourceAt(world)
    if (entity && entity.owner !== this.state.activePlayer) {
      const result = issueAttack(this.state, this.selectedIds, entity.id)
      if (result.ok) {
        this.pulseCommand(world, 0xe36c62)
      }
      this.flashResult(result.reason)
      return
    }
    if (resource) {
      const result = issueGather(this.state, this.selectedIds, resource.id)
      if (result.ok) {
        this.pulseCommand(resource.position, 0xd9af57)
      }
      this.flashResult(result.reason)
      return
    }
    const result = issueMove(this.state, this.selectedIds, world)
    if (result.ok) {
      this.pulseCommand(world, 0xf3e6bf)
    }
    this.flashResult(result.reason)
  }

  private placePreviewBuilding(world: Vec2): void {
    const worker = this.selectedIds.map((id) => getEntity(this.state, id)).find((entity) => entity?.type === 'worker')
    if (!worker || !this.buildPreview) {
      this.flashResult(`Select a ${getEntityDef('worker', this.state.players[this.state.activePlayer].race).label} first.`)
      return
    }
    const result = startBuilding(this.state, worker.id, this.buildPreview, world)
    this.flashResult(result.reason)
    if (result.ok) {
      this.pulseCommand(world, 0xd9af57)
      this.setBuildPreview(null)
    }
    this.publishSnapshot()
  }

  private trainSelected(unitType: UnitType): void {
    const selectedBuildings = this.selectedIds
      .map((id) => getEntity(this.state, id))
      .filter((entity): entity is Entity => entity !== undefined && entity.kind === 'building')

    const producer = selectedBuildings.find((entity) => {
      const allowed = entity.type === 'command' || entity.type === 'barracks'
      if (!allowed) {
        return false
      }
      return unitType === 'worker' ? entity.type === 'command' : entity.type === 'barracks'
    })

    if (!producer) {
      const producerType = unitType === 'worker' ? 'command' : 'barracks'
      this.flashResult(`Select a ${getEntityDef(producerType, this.state.players[this.state.activePlayer].race).label}.`)
      return
    }
    const result = startProduction(this.state, producer.id, unitType)
    this.flashResult(result.reason)
    this.publishSnapshot()
  }

  private flashResult(reason?: string): void {
    if (!reason) {
      return
    }
    window.dispatchEvent(new CustomEvent('rts:notice', { detail: reason }))
  }

  private pulseCommand(world: Vec2, color: number): void {
    const ring = this.add.circle(world.x, world.y, 14)
    ring.setStrokeStyle(3, color, 0.92)
    ring.setDepth(120)
    this.tweens.add({
      targets: ring,
      radius: 34,
      alpha: 0,
      duration: 420,
      ease: 'Sine.easeOut',
      onComplete: () => ring.destroy(),
    })
  }

  private setBuildPreview(buildingType: BuildingType | null): void {
    this.buildPreview = buildingType
    if (!buildingType) {
      this.placementGhost?.destroy()
      this.placementGhost = undefined
    }
  }

  private updatePlacementGhost(): void {
    if (!this.buildPreview) {
      return
    }

    const def = getEntityDef(this.buildPreview, this.state.players[this.state.activePlayer].race)
    const world = this.pointerWorld(this.input.activePointer)
    const valid = canPlaceBuilding(this.state, world, def.radius)
    const color = valid ? 0xc4a85f : 0x95383d

    if (!this.placementGhost) {
      this.placementGhost = this.add.rectangle(world.x, world.y, def.radius * 1.72, def.radius * 1.72, color, 0.18)
      this.placementGhost.setDepth(130)
    }

    this.placementGhost.setPosition(world.x, world.y)
    this.placementGhost.setSize(def.radius * 1.72, def.radius * 1.72)
    this.placementGhost.setFillStyle(color, 0.18)
    this.placementGhost.setStrokeStyle(2, color, 0.9)
  }

  private findEntityAt(world: Vec2): Entity | undefined {
    return [...this.state.entities]
      .sort((a, b) => b.radius - a.radius)
      .find(
        (entity) =>
          isEntityVisibleTo(this.state, entity, this.state.activePlayer) &&
          distance(entity.position, world) <= entity.radius + 6,
      )
  }

  private findResourceAt(world: Vec2): ResourceNode | undefined {
    return this.state.resources.find(
      (resource) =>
        resource.amount > 0 &&
        isPositionVisibleTo(this.state, resource.position, this.state.activePlayer, resource.radius) &&
        distance(resource.position, world) <= resource.radius + 8,
    )
  }

  private pointerWorld(pointer: Phaser.Input.Pointer): Vec2 {
    return pointer.positionToCamera(this.cameras.main) as Vec2
  }

  private updateCamera(dt: number): void {
    const camera = this.cameras.main
    const speed = 620 * dt / camera.zoom
    const keys = this.keys
    if (!keys || !this.cursors) {
      return
    }
    if (keys.A.isDown || this.cursors.left?.isDown) {
      camera.scrollX -= speed
    }
    if (keys.D.isDown || this.cursors.right?.isDown) {
      camera.scrollX += speed
    }
    if (keys.W.isDown || this.cursors.up?.isDown) {
      camera.scrollY -= speed
    }
    if (keys.S.isDown || this.cursors.down?.isDown) {
      camera.scrollY += speed
    }
  }

  private focusHome(): void {
    const command = this.state.entities.find(
      (entity) => entity.owner === this.state.activePlayer && entity.type === 'command',
    )
    if (command) {
      this.cameras.main.pan(command.position.x, command.position.y, 280, 'Sine.easeOut')
    }
  }

  private frameStartingBase(): void {
    const command = this.state.entities.find((entity) => entity.owner === this.state.activePlayer && entity.type === 'command')
    if (!command) {
      return
    }
    const compact = this.scale.width < 700
    this.cameras.main.centerOn(command.position.x + (compact ? 120 : 420), command.position.y - (compact ? 120 : 250))
  }

  private publishSnapshot(): void {
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
      }))

    const objective =
      this.state.mode === 'story'
        ? STORY_BRIEFING.objectives[Math.min(this.state.storyStep, STORY_BRIEFING.objectives.length - 1)]
        : 'Destroy the opposing Candle Keep'

    const snapshot: UiSnapshot = {
      mode: this.state.mode,
      activePlayer: this.state.activePlayer,
      status: this.state.status,
      players: this.state.players,
      selected,
      events: this.state.events,
      objective,
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
    }
    window.dispatchEvent(new CustomEvent('rts:state', { detail: snapshot }))
  }

  private commandSnapshot(): UiSnapshot['commands'] {
    const race = this.state.players[this.state.activePlayer].race
    return {
      train: {
        worker: this.commandLabel('worker', race),
        vanguard: this.commandLabel('vanguard', race),
        skirmisher: this.commandLabel('skirmisher', race),
      },
      build: {
        command: this.commandLabel('command', race),
        barracks: this.commandLabel('barracks', race),
        turret: this.commandLabel('turret', race),
      },
    }
  }

  private commandLabel(type: UnitType | BuildingType, race: RaceId): { label: string; cost: number } {
    const def = getEntityDef(type, race)
    return { label: def.label, cost: def.cost }
  }

  private opponentRaceFor(race: RaceId): RaceId {
    if (race === 'hollow') {
      return 'sepulcher'
    }
    return 'hollow'
  }

}
