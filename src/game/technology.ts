import type { BuildingType, RaceId, ResearchId } from './types'

export interface ResearchDef {
  id: ResearchId
  race: RaceId | null
  label: string
  description: string
  cost: number
  time: number
  producer: BuildingType
  requires?: ResearchId
}

export const RESEARCH_DEFS: Record<ResearchId, ResearchDef> = {
  'tier-two': {
    id: 'tier-two',
    race: null,
    label: 'Black-Iron Age',
    description: 'Unlock ranged troops and faction mastery research.',
    cost: 165,
    time: 16,
    producer: 'command',
  },
  'tempered-oaths': {
    id: 'tempered-oaths',
    race: 'compact',
    label: 'Interlocking Drills',
    description: 'Pikeguards gain 15% health and damage through practiced formation relief.',
    cost: 145,
    time: 13,
    producer: 'barracks',
    requires: 'tier-two',
  },
  wardcraft: {
    id: 'wardcraft',
    race: 'compact',
    label: 'Field Charters',
    description: 'Compact wards project more strongly through a standardized relief network.',
    cost: 130,
    time: 12,
    producer: 'command',
    requires: 'tier-two',
  },
  'chorus-of-knives': {
    id: 'chorus-of-knives',
    race: 'ascendancy',
    label: 'Perfected Purpose',
    description: 'Ascendancy combat units move faster and project more terror once committed.',
    cost: 135,
    time: 12,
    producer: 'barracks',
    requires: 'tier-two',
  },
  'pit-broods': {
    id: 'pit-broods',
    race: 'ascendancy',
    label: 'Merciful Shaping',
    description: 'Crowned Bulwarks require less black iron and manifest more quickly.',
    cost: 150,
    time: 14,
    producer: 'barracks',
    requires: 'tier-two',
  },
  'vault-plate': {
    id: 'vault-plate',
    race: 'concord',
    label: 'Living Ramparts',
    description: 'Concord structures and front-line bodies gain additional health.',
    cost: 165,
    time: 15,
    producer: 'command',
    requires: 'tier-two',
  },
  'siege-liturgy': {
    id: 'siege-liturgy',
    race: 'concord',
    label: 'Deep Resonance',
    description: 'Orrun Wardens deal heavier damage to structures through tuned impact.',
    cost: 155,
    time: 14,
    producer: 'barracks',
    requires: 'tier-two',
  },
}

export function researchForRace(race: RaceId): ResearchDef[] {
  return Object.values(RESEARCH_DEFS).filter((research) => research.race === null || research.race === race)
}
