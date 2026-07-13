import './style.css'
import { RACE_DEFS } from './game/catalog'
import { RtsThreeEngine, type ThreeUiSnapshot } from './game/RtsThreeEngine'
import { MAP_DEFS } from './game/map'
import { STORY_MISSIONS } from './game/story'
import type {
  AiPersonality,
  BuildingType,
  GameMode,
  MapId,
  MatchRuleId,
  MissionId,
  RaceId,
  ResearchId,
  UnitStance,
  UnitType,
} from './game/types'

const app = document.querySelector<HTMLDivElement>('#app')
if (!app) {
  throw new Error('Missing #app root')
}

app.innerHTML = `
  <div class="shell">
    <header class="topbar">
      <button id="brand-home" class="brand" type="button" aria-label="Open main menu">
        <span class="brand-mark" aria-hidden="true"></span>
        <span class="brand-copy">
          <strong>Ashen Dominion</strong>
          <small id="mode-label">Main Menu</small>
        </span>
      </button>
      <div class="match-heading">
        <span id="match-clock">00:00</span>
        <strong id="objective-text">The Black-Iron Ford</strong>
      </div>
      <div class="top-actions">
        <button id="pause" type="button">Pause</button>
        <button id="restart" type="button">Restart</button>
        <button id="menu-button" type="button">Menu</button>
      </div>
    </header>

    <main class="game-stage" aria-label="Ashen Dominion game">
      <div id="game-root"></div>
      <div class="battle-vignette" aria-hidden="true"></div>

      <section class="resource-hud" aria-label="Match status">
        <div class="commander-status">
          <span id="active-label">Candlebound</span>
          <strong id="active-player">Candlebound Remnant</strong>
        </div>
        <div class="resource-stat">
          <span>Iron</span>
          <strong id="ore-count">260</strong>
        </div>
        <div class="resource-stat">
          <span>Army</span>
          <strong id="supply-count">5 / 14</strong>
        </div>
        <div class="resource-stat resolve-stat">
          <span>Resolve</span>
          <strong id="resolve-count">100</strong>
        </div>
        <div class="resource-stat ruin-stat">
          <span>Ruin</span>
          <strong id="ruin-count">0</strong>
        </div>
        <div class="resource-stat">
          <span>Age</span>
          <strong id="tier-count">I</strong>
        </div>
        <div class="resource-stat">
          <span>Relics</span>
          <strong id="relic-count">0 / 1</strong>
        </div>
      </section>

      <div id="control-groups" class="control-groups" aria-label="Control groups"></div>

      <button id="panel-toggle" class="panel-toggle" type="button" aria-expanded="false" aria-controls="command-panel">
        War Council
      </button>

      <section id="command-panel" class="command-console" aria-label="Command console">
        <section class="console-selection">
          <div class="panel-heading">
            <div>
              <span>Selected force</span>
              <strong id="race-name">Candlebound Remnant</strong>
            </div>
            <button id="panel-close" class="panel-close" type="button" aria-label="Close war council">Close</button>
          </div>
          <div id="selection-list" class="selection-list">
            <p class="empty">No selection</p>
          </div>
        </section>

        <section class="commands console-orders" aria-label="Unit commands">
          <div class="console-section-heading">
            <span>Orders</span>
            <div class="stance-control" role="group" aria-label="Unit stance">
              <button data-stance="aggressive" type="button">Pursue</button>
              <button data-stance="defensive" type="button">Guard</button>
              <button data-stance="hold" type="button">Hold</button>
            </div>
          </div>
          <div class="command-grid">
            <button data-action="attack-move" type="button"><strong>Attack Move</strong><span>Advance and engage</span></button>
            <button data-action="retreat" type="button"><strong>Retreat</strong><span>Fall back to the keep</span></button>
            <button data-action="race-power" type="button"><strong>Faction Doctrine</strong><span>Invoke war doctrine</span></button>
            <button data-train="worker" type="button">Train Worker</button>
            <button data-train="vanguard" type="button">Train Vanguard</button>
            <button data-train="skirmisher" type="button">Train Ranged</button>
            <button data-build="barracks" type="button">Build Hall</button>
            <button data-build="turret" type="button">Build Ward</button>
            <button id="cancel-build" class="danger-command" type="button" hidden>Cancel Order</button>
          </div>
        </section>

        <section class="console-doctrine">
          <div class="console-tabs" role="tablist" aria-label="Council information">
            <button class="active" data-console-view="research" type="button" role="tab" aria-selected="true">Doctrine</button>
            <button data-console-view="intel" type="button" role="tab" aria-selected="false">Battle Log</button>
          </div>
          <div data-console-panel="research">
            <p id="race-style" class="console-race-style">${RACE_DEFS.candlebound.style}</p>
            <div id="research-list" class="research-list"></div>
          </div>
          <div data-console-panel="intel" hidden>
            <div id="event-log" class="event-log"></div>
          </div>
        </section>
      </section>

      <section id="main-menu" class="main-menu" aria-label="Main menu">
        <div class="menu-topline">
          <span>Bellgrave Theatre</span>
          <span>Black-Iron Chronicle / 01</span>
        </div>

        <div class="menu-primary">
          <p class="eyebrow">A medieval horror real-time strategy</p>
          <h1><span>Ashen</span> Dominion</h1>
          <p class="menu-lead">Command the last war hosts through a kingdom where every victory teaches the dark your name.</p>
          <button id="resume-match" class="resume-match" type="button" hidden>
            <span>Continue current battle</span><strong>Resume</strong>
          </button>
          <nav class="mode-list" aria-label="Choose game mode">
            <button data-launch="story" type="button">
              <span class="mode-index">01</span>
              <span class="mode-copy"><strong>Story Campaign</strong><small id="story-mode-subtitle">The Black-Iron Ford</small></span>
              <span class="mode-arrow" aria-hidden="true">&rsaquo;</span>
            </button>
            <button data-launch="skirmish" type="button">
              <span class="mode-index">02</span>
              <span class="mode-copy"><strong>Skirmish</strong><small>Custom war against a rival host</small></span>
              <span class="mode-arrow" aria-hidden="true">&rsaquo;</span>
            </button>
            <button data-launch="pvp" type="button">
              <span class="mode-index">03</span>
              <span class="mode-copy"><strong>Local PvP</strong><small>Two commanders, one battlefield</small></span>
              <span class="mode-arrow" aria-hidden="true">&rsaquo;</span>
            </button>
          </nav>
        </div>

        <section class="war-table" aria-label="Match setup">
          <header class="war-table-heading">
            <div><span>Campaign dispatch</span><strong id="menu-chapter-act">Act I - The Last Roads</strong></div>
            <span class="chapter-mark" aria-hidden="true">I</span>
          </header>
          <div class="campaign-preview">
            <span id="menu-chapter-location">Western Verge, Bellgrave Valley</span>
            <h2 id="menu-chapter-title">The Black-Iron Ford</h2>
            <p id="menu-chapter-lore">The Remnant reaches Bellgrave before the Hollow Choir can seal the eastern bank.</p>
          </div>

          <div class="war-table-section">
            <span class="section-kicker">Choose your banner</span>
            <div class="faction-list">
              ${Object.values(RACE_DEFS)
                .map(
                  (race) => `
                    <button class="faction-choice ${race.id === 'candlebound' ? 'active' : ''}" data-race="${race.id}" type="button">
                      <span class="faction-sigil" data-sigil="${race.id}" aria-hidden="true"></span>
                      <span><strong>${race.shortName}</strong><small>${race.tagline}</small></span>
                    </button>
                  `,
                )
                .join('')}
            </div>
            <p id="menu-race-style" class="faction-doctrine">${RACE_DEFS.candlebound.style}</p>
          </div>

          <div class="match-setup-grid">
            <label for="mission-select"><span>Campaign chapter</span><select id="mission-select">
              ${STORY_MISSIONS.map((mission) => `<option value="${mission.id}">${mission.title.replace(/^Mission \d+: /, '')}</option>`).join('')}
            </select></label>
            <label for="map-select"><span>Battlefield</span><select id="map-select">
              ${Object.values(MAP_DEFS).map((map) => `<option value="${map.id}">${map.name}</option>`).join('')}
            </select></label>
            <label for="opponent-race"><span>Rival banner</span><select id="opponent-race">
              ${Object.values(RACE_DEFS).map((race) => `<option value="${race.id}" ${race.id === 'hollow' ? 'selected' : ''}>${race.name}</option>`).join('')}
            </select></label>
            <label for="ai-select"><span>Rival strategy</span><select id="ai-select">
              <option value="aggressive">Aggressive</option><option value="economic">Economic</option><option value="fortress">Fortress</option>
            </select></label>
            <label class="rule-select" for="rule-select"><span>Match rules</span><select id="rule-select">
              <option value="standard">Standard</option><option value="fast-ruin">Fast Ruin Tide</option><option value="rich-seams">Rich Iron Seams</option>
            </select></label>
          </div>
        </section>

        <footer class="menu-footer"><span>Original medieval-horror universe</span><span>Build 0.2 / Ashfall</span></footer>
      </section>

      <section id="briefing" class="briefing" hidden>
        <header class="briefing-header">
          <div><span id="briefing-act">Act I - The Last Roads</span><strong id="briefing-location">Western Verge, Bellgrave Valley</strong></div>
          <span class="briefing-seal" aria-hidden="true">I</span>
        </header>
        <p id="briefing-subtitle" class="eyebrow">First Crossing</p>
        <h2 id="briefing-title">Mission 01: The Black-Iron Ford</h2>
        <p id="briefing-body">Take the ford, establish production, survive the first Ruin Tide, and break the rival keep.</p>
        <blockquote id="briefing-quote">We do not need the ford forever. We need it until morning.</blockquote>
        <div class="briefing-intel">
          <div><span>Field intelligence</span><p id="briefing-lore"></p></div>
          <div><span>Primary objectives</span><ol id="briefing-objectives"></ol></div>
        </div>
        <div class="briefing-actions">
          <button id="start-mission" type="button"><span>Deploy war host</span><strong>Begin Mission</strong></button>
          <button id="briefing-menu" type="button">Return to Menu</button>
        </div>
      </section>

      <section id="end-screen" class="end-screen" hidden>
        <p class="eyebrow">The ford remembers</p>
        <h2 id="end-title">Victory</h2>
        <p id="end-copy">The rival keep has fallen.</p>
        <div id="end-stats" class="end-stats"></div>
        <div class="end-actions">
          <button id="replay" type="button">Fight Again</button>
          <button id="end-menu" type="button">Main Menu</button>
        </div>
      </section>

      <div id="pause-indicator" class="pause-indicator" hidden>Paused</div>
      <div id="notice" class="notice" aria-live="polite"></div>
    </main>
  </div>
`

const gameRoot = requiredElement<HTMLElement>('#game-root')
const mainMenu = requiredElement<HTMLElement>('#main-menu')
const briefing = requiredElement<HTMLElement>('#briefing')
const endScreen = requiredElement<HTMLElement>('#end-screen')
const commandPanel = requiredElement<HTMLElement>('#command-panel')
const panelToggle = requiredElement<HTMLButtonElement>('#panel-toggle')
const resumeMatch = requiredElement<HTMLButtonElement>('#resume-match')
const pauseButton = requiredElement<HTMLButtonElement>('#pause')
const notice = requiredElement<HTMLElement>('#notice')
const activePlayer = requiredElement<HTMLElement>('#active-player')
const activeLabel = requiredElement<HTMLElement>('#active-label')
const oreCount = requiredElement<HTMLElement>('#ore-count')
const supplyCount = requiredElement<HTMLElement>('#supply-count')
const resolveCount = requiredElement<HTMLElement>('#resolve-count')
const ruinCount = requiredElement<HTMLElement>('#ruin-count')
const tierCount = requiredElement<HTMLElement>('#tier-count')
const relicCount = requiredElement<HTMLElement>('#relic-count')
const objectiveText = requiredElement<HTMLElement>('#objective-text')
const modeLabel = requiredElement<HTMLElement>('#mode-label')
const matchClock = requiredElement<HTMLElement>('#match-clock')
const selectionList = requiredElement<HTMLElement>('#selection-list')
const eventLog = requiredElement<HTMLElement>('#event-log')
const raceName = requiredElement<HTMLElement>('#race-name')
const raceStyle = requiredElement<HTMLElement>('#race-style')
const menuRaceStyle = requiredElement<HTMLElement>('#menu-race-style')
const briefingBody = requiredElement<HTMLElement>('#briefing-body')
const briefingTitle = requiredElement<HTMLElement>('#briefing-title')
const briefingSubtitle = requiredElement<HTMLElement>('#briefing-subtitle')
const briefingAct = requiredElement<HTMLElement>('#briefing-act')
const briefingLocation = requiredElement<HTMLElement>('#briefing-location')
const briefingQuote = requiredElement<HTMLElement>('#briefing-quote')
const briefingLore = requiredElement<HTMLElement>('#briefing-lore')
const briefingObjectives = requiredElement<HTMLOListElement>('#briefing-objectives')
const menuChapterAct = requiredElement<HTMLElement>('#menu-chapter-act')
const menuChapterLocation = requiredElement<HTMLElement>('#menu-chapter-location')
const menuChapterTitle = requiredElement<HTMLElement>('#menu-chapter-title')
const menuChapterLore = requiredElement<HTMLElement>('#menu-chapter-lore')
const opponentRace = requiredElement<HTMLSelectElement>('#opponent-race')
const missionSelect = requiredElement<HTMLSelectElement>('#mission-select')
const mapSelect = requiredElement<HTMLSelectElement>('#map-select')
const aiSelect = requiredElement<HTMLSelectElement>('#ai-select')
const ruleSelect = requiredElement<HTMLSelectElement>('#rule-select')
const storyModeSubtitle = requiredElement<HTMLElement>('#story-mode-subtitle')
const cancelBuild = requiredElement<HTMLButtonElement>('#cancel-build')
const controlGroups = requiredElement<HTMLElement>('#control-groups')
const researchList = requiredElement<HTMLElement>('#research-list')
const endStats = requiredElement<HTMLElement>('#end-stats')
const pauseIndicator = requiredElement<HTMLElement>('#pause-indicator')

let lastSnapshot: ThreeUiSnapshot | null = null
let activeMatch = false
let noticeTimer = 0

window.addEventListener('rts:state', (event) => {
  renderUi((event as CustomEvent<ThreeUiSnapshot>).detail)
})

window.addEventListener('rts:notice', (event) => {
  showNotice((event as CustomEvent<string>).detail)
})

const engine = new RtsThreeEngine(gameRoot)
document.body.classList.add('menu-open')

document.querySelectorAll<HTMLButtonElement>('[data-launch]').forEach((button) => {
  button.addEventListener('click', () => launchMode(button.dataset.launch as GameMode))
})

document.querySelectorAll<HTMLButtonElement>('[data-race]').forEach((button) => {
  button.addEventListener('click', () => dispatchCommand({ type: 'race', race: button.dataset.race as RaceId }))
})

opponentRace.addEventListener('change', () => {
  dispatchCommand({ type: 'opponent-race', race: opponentRace.value as RaceId })
})
missionSelect.addEventListener('change', () => {
  dispatchCommand({ type: 'mission', missionId: missionSelect.value as MissionId })
})
mapSelect.addEventListener('change', () => {
  dispatchCommand({ type: 'map', mapId: mapSelect.value as MapId })
})
aiSelect.addEventListener('change', () => {
  dispatchCommand({ type: 'ai', personality: aiSelect.value as AiPersonality })
})
ruleSelect.addEventListener('change', () => {
  dispatchCommand({ type: 'rule', rule: ruleSelect.value as MatchRuleId })
})

document.querySelectorAll<HTMLButtonElement>('[data-train]').forEach((button) => {
  button.addEventListener('click', () => dispatchCommand({ type: 'train', unitType: button.dataset.train as UnitType }))
})

document.querySelectorAll<HTMLButtonElement>('[data-build]').forEach((button) => {
  button.addEventListener('click', () =>
    dispatchCommand({ type: 'build', buildingType: button.dataset.build as BuildingType }),
  )
})

requiredElement<HTMLButtonElement>('[data-action="attack-move"]').addEventListener('click', () =>
  dispatchCommand({ type: 'attack-move' }),
)
requiredElement<HTMLButtonElement>('[data-action="race-power"]').addEventListener('click', () =>
  dispatchCommand({ type: 'race-power' }),
)
requiredElement<HTMLButtonElement>('[data-action="retreat"]').addEventListener('click', () =>
  dispatchCommand({ type: 'retreat' }),
)
document.querySelectorAll<HTMLButtonElement>('[data-stance]').forEach((button) => {
  button.addEventListener('click', () => dispatchCommand({ type: 'stance', stance: button.dataset.stance as UnitStance }))
})
document.querySelectorAll<HTMLButtonElement>('[data-console-view]').forEach((button) => {
  button.addEventListener('click', () => setConsoleView(button.dataset.consoleView ?? 'research'))
})
requiredElement<HTMLButtonElement>('#start-mission').addEventListener('click', () => {
  briefing.hidden = true
  dispatchCommand({ type: 'start' })
})
requiredElement<HTMLButtonElement>('#briefing-menu').addEventListener('click', openMenu)
requiredElement<HTMLButtonElement>('#brand-home').addEventListener('click', openMenu)
requiredElement<HTMLButtonElement>('#menu-button').addEventListener('click', openMenu)
requiredElement<HTMLButtonElement>('#end-menu').addEventListener('click', openMenu)
requiredElement<HTMLButtonElement>('#resume-match').addEventListener('click', resumeBattle)
requiredElement<HTMLButtonElement>('#restart').addEventListener('click', restartMatch)
requiredElement<HTMLButtonElement>('#replay').addEventListener('click', restartMatch)
pauseButton.addEventListener('click', () => dispatchCommand({ type: 'pause' }))
cancelBuild.addEventListener('click', () => dispatchCommand({ type: 'cancel-build' }))
panelToggle.addEventListener('click', () => setPanelOpen(!commandPanel.classList.contains('open')))
requiredElement<HTMLButtonElement>('#panel-close').addEventListener('click', () => setPanelOpen(false))

window.addEventListener('beforeunload', () => engine.destroy())

function launchMode(mode: GameMode): void {
  activeMatch = true
  document.body.classList.remove('menu-open')
  mainMenu.hidden = true
  endScreen.hidden = true
  dispatchCommand({ type: 'mode', mode })
  briefing.hidden = mode !== 'story'
  setPanelOpen(false)
}

function openMenu(): void {
  document.body.classList.add('menu-open')
  briefing.hidden = true
  endScreen.hidden = true
  mainMenu.hidden = false
  resumeMatch.hidden = !activeMatch
  if (activeMatch && lastSnapshot && !lastSnapshot.paused) {
    dispatchCommand({ type: 'pause' })
  }
  setPanelOpen(false)
}

function resumeBattle(): void {
  document.body.classList.remove('menu-open')
  mainMenu.hidden = true
  if (lastSnapshot?.mode === 'story' && !lastSnapshot.missionStarted) {
    briefing.hidden = false
    return
  }
  if (lastSnapshot?.paused) {
    dispatchCommand({ type: 'pause' })
  }
}

function restartMatch(): void {
  document.body.classList.remove('menu-open')
  endScreen.hidden = true
  dispatchCommand({ type: 'restart' })
  briefing.hidden = lastSnapshot?.mode !== 'story'
  mainMenu.hidden = true
}

function renderUi(snapshot: ThreeUiSnapshot): void {
  lastSnapshot = snapshot
  const player = snapshot.players[snapshot.activePlayer]
  const race = RACE_DEFS[player.race]
  const setupRace = RACE_DEFS[snapshot.setupRaces.player]
  activePlayer.textContent = player.name
  activeLabel.textContent = snapshot.mode === 'pvp' ? `Commander ${snapshot.activePlayer}` : race.shortName
  oreCount.textContent = Math.floor(player.ore).toString()
  supplyCount.textContent = `${snapshot.supply.used + snapshot.supply.queued} / ${snapshot.supply.cap}`
  supplyCount.classList.toggle('warning', snapshot.supply.cap > 0 && snapshot.supply.used + snapshot.supply.queued >= snapshot.supply.cap)
  resolveCount.textContent = player.resolve.toString()
  ruinCount.textContent = snapshot.ruinTide.toString()
  tierCount.textContent = player.techTier >= 2 ? 'II' : 'I'
  relicCount.textContent = `${snapshot.controlPoints.filter((point) => point.owner === snapshot.activePlayer).length} / ${snapshot.controlPoints.length}`
  objectiveText.textContent = snapshot.status === 'playing' ? snapshot.objective : snapshot.status === 'won' ? 'Victory' : 'Defeat'
  matchClock.textContent = formatTime(snapshot.matchTime)
  modeLabel.textContent = formatMode(snapshot.mode)
  raceName.textContent = race.name
  raceStyle.textContent = race.style
  menuRaceStyle.textContent = setupRace.style
  briefingTitle.textContent = snapshot.mission.title
  briefingSubtitle.textContent = snapshot.mission.subtitle
  briefingAct.textContent = snapshot.mission.act
  briefingLocation.textContent = snapshot.mission.location
  briefingBody.textContent = briefingForRace(snapshot.setupRaces.player, snapshot.setupRaces.opponent, snapshot.mission.briefing)
  briefingQuote.textContent = `"${snapshot.mission.quote}" - ${snapshot.mission.commander}`
  briefingLore.textContent = snapshot.mission.lore
  briefingObjectives.innerHTML = snapshot.mission.objectives.map((objective) => `<li>${objective}</li>`).join('')
  storyModeSubtitle.textContent = snapshot.mission.title.replace(/^Mission \d+: /, '')
  menuChapterAct.textContent = snapshot.mission.act
  menuChapterLocation.textContent = snapshot.mission.location
  menuChapterTitle.textContent = snapshot.mission.title.replace(/^Mission \d+: /, '')
  menuChapterLore.textContent = snapshot.mission.lore
  opponentRace.value = snapshot.setupRaces.opponent
  missionSelect.value = snapshot.setup.missionId
  mapSelect.value = snapshot.setup.mapId
  aiSelect.value = snapshot.setup.aiPersonality
  ruleSelect.value = snapshot.setup.matchRule
  document.documentElement.style.setProperty('--faction-color', `#${player.color.toString(16).padStart(6, '0')}`)
  document.documentElement.style.setProperty('--faction-accent', `#${player.accent.toString(16).padStart(6, '0')}`)
  document.querySelectorAll<HTMLButtonElement>('[data-race]').forEach((button) => {
    button.classList.toggle('active', button.dataset.race === snapshot.setupRaces.player)
  })

  renderSelection(snapshot)
  renderCommands(snapshot)
  renderEvents(snapshot)
  renderControlGroups(snapshot)

  pauseButton.textContent = snapshot.paused ? 'Resume' : 'Pause'
  pauseIndicator.hidden = !snapshot.paused || !mainMenu.hidden
  cancelBuild.hidden = !snapshot.buildPreview && snapshot.commandMode === 'normal'
  document.body.classList.toggle('targeting', snapshot.commandMode === 'attack-move')
  document.body.classList.toggle('placing', Boolean(snapshot.buildPreview))

  if (snapshot.status !== 'playing') {
    endScreen.hidden = false
    requiredElement<HTMLElement>('#end-title').textContent = snapshot.status === 'won' ? 'Victory' : 'Defeat'
    requiredElement<HTMLElement>('#end-copy').textContent =
      snapshot.status === 'won' ? `${snapshot.mapName} belongs to your banner.` : 'The dark has claimed another banner.'
    renderEndStats(snapshot)
  }
}

function renderSelection(snapshot: ThreeUiSnapshot): void {
  if (snapshot.selected.length === 0) {
    selectionList.innerHTML = '<p class="empty">No selection</p>'
    return
  }
  const shown = snapshot.selected.slice(0, 6)
  selectionList.innerHTML = shown
    .map((entity) => {
      const queue = entity.queue[0]
      const details = entity.underConstruction
        ? `Raising ${Math.round(entity.constructionProgress * 100)}%`
        : queue
          ? `${snapshot.commands.train[queue.type].label} ${Math.ceil(queue.remaining)}s`
          : entity.rallyPoint
            ? 'Rally set'
            : `${entity.hp}/${entity.maxHp} health / ${formatTerm(entity.stance)}`
      return `
        <article>
          <div><strong>${entity.label}</strong><small>${formatTerm(entity.armor)}</small></div>
          <span>${details}</span>
          <meter min="0" max="${entity.maxHp}" value="${entity.hp}">${entity.hp}/${entity.maxHp}</meter>
        </article>
      `
    })
    .join('')
  if (snapshot.selected.length > shown.length) {
    selectionList.insertAdjacentHTML('beforeend', `<p class="selection-more">+${snapshot.selected.length - shown.length} more</p>`)
  }
}

function renderCommands(snapshot: ThreeUiSnapshot): void {
  document.querySelectorAll<HTMLButtonElement>('[data-train]').forEach((button) => {
    const type = button.dataset.train as UnitType
    const command = snapshot.commands.train[type]
    button.innerHTML = `<strong>${command.label}</strong><span>${command.cost} iron / ${command.supply} army</span>`
    const requiredBuilding = type === 'worker' ? 'command' : 'barracks'
    const hasProducer = snapshot.selected.some((entity) => entity.type === requiredBuilding && !entity.underConstruction)
    const technologyLocked = type === 'skirmisher' && snapshot.players[snapshot.activePlayer].techTier < 2
    button.disabled = !hasProducer || technologyLocked || snapshot.players[snapshot.activePlayer].ore < command.cost
  })
  document.querySelectorAll<HTMLButtonElement>('[data-build]').forEach((button) => {
    const type = button.dataset.build as BuildingType
    const command = snapshot.commands.build[type]
    button.innerHTML = `<strong>${command.label}</strong><span>${command.cost} iron${command.supply ? ` / +${command.supply} army` : ''}</span>`
    button.disabled = !snapshot.selected.some((entity) => entity.type === 'worker') || snapshot.players[snapshot.activePlayer].ore < command.cost
  })
  const attackMove = requiredElement<HTMLButtonElement>('[data-action="attack-move"]')
  attackMove.classList.toggle('active', snapshot.commandMode === 'attack-move')
  attackMove.disabled = !snapshot.selected.some((entity) => entity.type !== 'command' && entity.type !== 'barracks' && entity.type !== 'turret')
  const hasUnits = snapshot.selected.some((entity) => ['worker', 'vanguard', 'skirmisher'].includes(entity.type))
  requiredElement<HTMLButtonElement>('[data-action="retreat"]').disabled = !hasUnits
  document.querySelectorAll<HTMLButtonElement>('[data-stance]').forEach((button) => {
    const stance = button.dataset.stance as UnitStance
    button.disabled = !hasUnits
    button.classList.toggle('active', hasUnits && snapshot.selected.every((entity) => !['worker', 'vanguard', 'skirmisher'].includes(entity.type) || entity.stance === stance))
  })
  const racePower = requiredElement<HTMLButtonElement>('[data-action="race-power"]')
  const cooldown = Math.ceil(snapshot.racePower.cooldown)
  racePower.innerHTML = `<strong>${snapshot.racePower.label}</strong><span>${cooldown > 0 ? `${cooldown}s recovery` : `${snapshot.racePower.cost} iron`}</span>`
  racePower.title = snapshot.racePower.description
  racePower.disabled = cooldown > 0 || snapshot.players[snapshot.activePlayer].ore < snapshot.racePower.cost
  renderResearch(snapshot)
}

function renderResearch(snapshot: ThreeUiSnapshot): void {
  const queue = snapshot.players[snapshot.activePlayer].researchQueue[0]
  researchList.innerHTML = snapshot.research
    .map((research) => {
      const state = research.completed
        ? 'Known'
        : research.queued
          ? `${Math.ceil(queue?.remaining ?? 0)}s`
          : `${research.cost} iron`
      return `
        <button data-research="${research.id}" type="button" ${research.available ? '' : 'disabled'}>
          <strong>${research.label}</strong>
          <span>${state}</span>
          <small>${research.description}</small>
        </button>
      `
    })
    .join('')
  researchList.querySelectorAll<HTMLButtonElement>('[data-research]').forEach((button) => {
    button.addEventListener('click', () =>
      dispatchCommand({ type: 'research', researchId: button.dataset.research as ResearchId }),
    )
  })
}

function renderEndStats(snapshot: ThreeUiSnapshot): void {
  const rows: Array<[string, keyof ThreeUiSnapshot['stats'][1]]> = [
    ['Iron mined', 'oreMined'],
    ['Army raised', 'unitsCreated'],
    ['Army lost', 'unitsLost'],
    ['Damage dealt', 'damageDealt'],
    ['Relics taken', 'objectivesCaptured'],
  ]
  endStats.innerHTML = `
    <div class="end-stats-heading"><span>${snapshot.players[1].name}</span><span>${snapshot.players[2].name}</span></div>
    ${rows
      .map(
        ([label, key]) =>
          `<div class="end-stat-row"><strong>${Math.round(snapshot.stats[1][key])}</strong><span>${label}</span><strong>${Math.round(snapshot.stats[2][key])}</strong></div>`,
      )
      .join('')}
  `
}

function renderEvents(snapshot: ThreeUiSnapshot): void {
  eventLog.innerHTML = snapshot.events
    .slice(0, 4)
    .map((item) => `<p class="${item.tone}"><time>${formatTime(item.time)}</time>${item.text}</p>`)
    .join('')
}

function renderControlGroups(snapshot: ThreeUiSnapshot): void {
  controlGroups.innerHTML = snapshot.controlGroups
    .map((group) => `<button data-group="${group.slot}" type="button"><strong>${group.slot}</strong><span>${group.count}</span></button>`)
    .join('')
  controlGroups.querySelectorAll<HTMLButtonElement>('[data-group]').forEach((button) => {
    button.addEventListener('click', () => dispatchCommand({ type: 'group', slot: Number(button.dataset.group) }))
  })
}

function setPanelOpen(open: boolean): void {
  commandPanel.classList.toggle('open', open)
  document.body.classList.toggle('council-open', open)
  panelToggle.setAttribute('aria-expanded', String(open))
}

function setConsoleView(view: string): void {
  document.querySelectorAll<HTMLButtonElement>('[data-console-view]').forEach((button) => {
    const active = button.dataset.consoleView === view
    button.classList.toggle('active', active)
    button.setAttribute('aria-selected', String(active))
  })
  document.querySelectorAll<HTMLElement>('[data-console-panel]').forEach((panel) => {
    panel.hidden = panel.dataset.consolePanel !== view
  })
}

function showNotice(message: string): void {
  notice.textContent = message
  notice.classList.add('visible')
  window.clearTimeout(noticeTimer)
  noticeTimer = window.setTimeout(() => notice.classList.remove('visible'), 2100)
}

function dispatchCommand(detail: object): void {
  window.dispatchEvent(new CustomEvent('rts:command', { detail }))
}

function requiredElement<T extends Element>(selector: string): T {
  const element = document.querySelector<T>(selector)
  if (!element) {
    throw new Error(`Missing ${selector}`)
  }
  return element
}

function formatTime(seconds: number): string {
  const minutes = Math.floor(seconds / 60)
  const remainder = Math.floor(seconds % 60)
  return `${minutes.toString().padStart(2, '0')}:${remainder.toString().padStart(2, '0')}`
}

function formatMode(mode: GameMode): string {
  if (mode === 'story') return 'Story Campaign'
  if (mode === 'skirmish') return 'Skirmish'
  return 'Local PvP'
}

function briefingForRace(race: RaceId, opponent: RaceId, missionBriefing: string): string {
  const playerName = RACE_DEFS[race].name
  const enemyName = RACE_DEFS[opponent].name
  return `${missionBriefing} ${playerName} marches against ${enemyName}.`
}

function formatTerm(value: string): string {
  return value.charAt(0).toUpperCase() + value.slice(1)
}
