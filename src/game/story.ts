import type { MapId, MissionId, RaceId } from './types'

export interface StorySetting {
  id: string
  title: string
  pitch: string
  factions: string[]
  campaignHook: string
}

export interface StoryMission {
  id: MissionId
  title: string
  subtitle: string
  briefing: string
  mapId: MapId
  playerRace: RaceId
  opponentRace: RaceId
  kind: 'assault' | 'survival' | 'domination'
  duration?: number
  act: string
  location: string
  commander: string
  perspective: string
  dilemma: string
  quote: string
  lore: string
  objectives: string[]
}

export const STORY_SETTINGS: StorySetting[] = [
  {
    id: 'vowfall',
    title: 'Vowfall',
    pitch:
      'Three civilizations fight over black iron that can feed human cities, restore the memory of the world, or end the pain of choice itself.',
    factions: ['Cinder Compact', 'Gloam Ascendancy', 'Elder Concord'],
    campaignHook:
      'A human marshal, a transformed physician, and a dwarven war speaker reach Bellgrave carrying three honest answers to suffering that cannot coexist.',
  },
]

export const DEFAULT_SETTING = STORY_SETTINGS[0]

export const STORY_MISSIONS: StoryMission[] = [
  {
    id: 'bridge-of-names',
    title: 'Prologue I: The Bridge of Names',
    subtitle: 'No One Left Uncounted',
    briefing:
      'Open the refugee road, establish a field assembly, endure the first Dread Tide crest, and break the Ascendancy stronghold across the ford.',
    mapId: 'black-iron-ford',
    playerRace: 'compact',
    opponentRace: 'ascendancy',
    kind: 'assault',
    act: 'Prologue - Three Honest Beginnings',
    location: 'Greywake Ford, Bellgrave Valley',
    commander: 'Marshal Mara Veyr',
    perspective: 'Cinder Compact',
    dilemma: 'A road can save a city and still abandon everyone standing on the wrong side of it.',
    quote: 'Write down every name before you count the distance.',
    lore:
      'Mara reaches the ford with six thousand refugees behind her and winter stores for only four thousand. Black iron beneath the river can keep the road open, but the House of Quiet on the eastern bank shelters civilians the Compact once left without medicine.',
    objectives: [
      'Secure eighty black iron for the refugee road',
      'Raise an Assembly Hall',
      'Keep the column together through the Dread Tide crest',
      'Break the House of Quiet without losing the March Keep',
    ],
  },
  {
    id: 'mercy-for-the-uncounted',
    title: 'Prologue II: Mercy for the Uncounted',
    subtitle: 'The Last Contradiction',
    briefing:
      'Hold the free clinic and its evacuation road until dawn while Compact companies close around the valley.',
    mapId: 'ossuary-crossroads',
    playerRace: 'ascendancy',
    opponentRace: 'compact',
    kind: 'survival',
    duration: 150,
    act: 'Prologue - Three Honest Beginnings',
    location: 'Saint Orra Clinic, Lower Bellgrave',
    commander: 'Aurel Sorn, the First Crowned',
    perspective: 'Gloam Ascendancy',
    dilemma: 'A choice offered after hunger is still a choice, but it is not an innocent one.',
    quote: 'Feed them first. Ask what they believe when their hands stop shaking.',
    lore:
      'The clinic abolished debt and treated a plague the Compact could only quarantine. Now its patients are called collaborators. Aurel must defend their evacuation while Ione Vale argues that no wounded volunteer should accept Absolution before sunrise.',
    objectives: [
      'Keep the House of Quiet standing',
      'Contest the two roadside memory stones',
      'Protect the evacuation until dawn',
    ],
  },
  {
    id: 'where-roots-remember',
    title: 'Prologue III: Where Roots Remember',
    subtitle: 'A History With Two Voices',
    briefing:
      'Reclaim both memory stones, awaken the buried resonance forge, and drive the Ascendancy from the watershed.',
    mapId: 'ossuary-crossroads',
    playerRace: 'concord',
    opponentRace: 'ascendancy',
    kind: 'domination',
    act: 'Prologue - Three Honest Beginnings',
    location: 'Nine-Reeds Watershed, Bellgrave',
    commander: 'Tavra Nine-Reeds',
    perspective: 'Elder Concord',
    dilemma: 'Restoring an ancient river can heal the world and drown the people who arrived after the map was drawn.',
    quote: 'The stone remembers our law. It also remembers who was forbidden to speak it.',
    lore:
      'Two memory stones seal the oldest surviving account of the First Vow. Tavra can wake them, but their water once fed a valley now occupied by forty thousand humans. The Ascendancy offers those settlers painless passage into the Quiet.',
    objectives: [
      'Reclaim both memory stones',
      'Reach the Black-Iron Age at the Meeting Stone',
      'Break the House of Quiet at the watershed gate',
    ],
  },
]

export function getStoryMission(id: MissionId): StoryMission {
  return STORY_MISSIONS.find((mission) => mission.id === id) ?? STORY_MISSIONS[0]
}

export const STORY_BRIEFING = {
  title: STORY_MISSIONS[0].title,
  subtitle: 'Vowfall',
  body: STORY_MISSIONS[0].briefing,
  objectives: STORY_MISSIONS[0].objectives,
}
