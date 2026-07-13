export interface StorySetting {
  id: string
  title: string
  pitch: string
  factionA: string
  factionB: string
  campaignHook: string
}

export interface StoryMission {
  id: 'black-iron-ford' | 'lantern-vigil' | 'choir-gate'
  title: string
  subtitle: string
  briefing: string
  mapId: 'black-iron-ford' | 'ossuary-crossroads'
  kind: 'assault' | 'survival' | 'domination'
  duration?: number
  act: string
  location: string
  commander: string
  quote: string
  lore: string
  objectives: string[]
}

export const STORY_SETTINGS: StorySetting[] = [
  {
    id: 'ashen-dominion',
    title: 'Ashen Dominion',
    pitch: 'A medieval horror war where candle-keeps, oathbound soldiers, and starving villages endure a spreading ruin.',
    factionA: 'The Candlebound Remnant',
    factionB: 'The Hollow Choir',
    campaignHook:
      'The last human keeps burn sanctified tallow while an old king beneath the earth teaches the dead to sing.',
  },
  {
    id: 'wound-march',
    title: 'The Wound March',
    pitch: 'Border forts crawl across a battlefield where the soil remembers every execution.',
    factionA: 'The Blackroad Companies',
    factionB: 'The Red Chapel',
    campaignHook:
      'A mercenary host discovers that each captured shrine makes its soldiers stronger and less human.',
  },
  {
    id: 'salt-saint',
    title: 'The Salt Saint',
    pitch: 'A drowned principality returns from the sea with rusted armor, mute priests, and miracle-plagues.',
    factionA: 'The Lowland Vow',
    factionB: 'The Brine Host',
    campaignHook:
      'The campaign follows a retreat inland as every defended village buys time for one impossible exorcism.',
  },
]

export const DEFAULT_SETTING = STORY_SETTINGS[0]

export const STORY_MISSIONS: StoryMission[] = [
  {
    id: 'black-iron-ford',
    title: 'Mission 01: The Black-Iron Ford',
    subtitle: 'First Crossing',
    briefing: 'Take the ford, establish production, survive the first Ruin Tide, and break the rival keep.',
    mapId: 'black-iron-ford',
    kind: 'assault',
    act: 'Act I - The Last Roads',
    location: 'Western Verge, Bellgrave Valley',
    commander: 'Marshal Elian Veyr',
    quote: 'We do not need the ford forever. We need it until morning.',
    lore:
      'The Remnant reaches Bellgrave after thirteen days without a friendly fire on the horizon. Beneath the ford lies black iron enough to arm a province, but the Hollow Choir has already raised its listening spires on the eastern bank.',
    objectives: ['Secure eighty black iron', 'Raise an Oath Hall', 'Endure the first Ruin Tide crest', 'Destroy the Choir command spire'],
  },
  {
    id: 'lantern-vigil',
    title: 'Mission 02: Lantern Vigil',
    subtitle: 'Hold Until Dawn',
    briefing: 'The crossroads cannot be abandoned. Hold your command keep through the full vigil while enemy warbands close from the dark.',
    mapId: 'ossuary-crossroads',
    kind: 'survival',
    duration: 150,
    act: 'Act I - The Last Roads',
    location: "Ossuary Crossroads, Saint Orra's Mile",
    commander: 'Lantern-Captain Maelin Rook',
    quote: 'Count the bells, not the dead. When the seventh sounds, dawn is ours.',
    lore:
      "Refugees fill the old ossuary road while the valley's lantern chain fails one shrine at a time. Rook's exhausted rearguard must hold the crossroads until the final column reaches the northern gate.",
    objectives: ['Keep the Candle Keep standing', 'Contest both roadside reliquaries', 'Survive until the seventh bell'],
  },
  {
    id: 'choir-gate',
    title: 'Mission 03: The Choir Gate',
    subtitle: 'Relics of the Crossroads',
    briefing: 'Capture both reliquaries at the Ossuary Crossroads, use their income to reach the Black-Iron Age, then destroy the rival command keep.',
    mapId: 'ossuary-crossroads',
    kind: 'domination',
    act: 'Act II - A Kingdom of Ash',
    location: 'The Choir Gate, Lower Bellgrave',
    commander: 'Marshal Elian Veyr',
    quote: 'Their song has a source. Tonight, we put a sword through it.',
    lore:
      'Two reliquaries seal the road into Lower Bellgrave. Their saints have been silent for a century, yet the Choir fears them enough to bleed for every stone. Rekindle both and the buried forge beneath the gate will answer.',
    objectives: ['Capture both reliquaries', 'Reach the Black-Iron Age', 'Break the Choir Gate'],
  },
]

export function getStoryMission(id: StoryMission['id']): StoryMission {
  return STORY_MISSIONS.find((mission) => mission.id === id) ?? STORY_MISSIONS[0]
}

export const STORY_BRIEFING = {
  title: STORY_MISSIONS[0].title,
  subtitle: 'Ashen Dominion',
  body:
    'The Candlebound Remnant reaches a ruined ford before dusk. Harvest black iron, raise an Oath Hall, hold through the Ruin Tide, then burn the Hollow Choir keep before the valley learns your names.',
  objectives: ['Harvest black iron', 'Raise an Oath Hall', 'Survive the Ruin Tide', 'Destroy the Hollow Candle Keep'],
}
