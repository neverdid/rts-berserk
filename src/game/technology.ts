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
    race: 'candlebound',
    label: 'Tempered Oaths',
    description: 'Oathbound gain 15% health and damage.',
    cost: 145,
    time: 13,
    producer: 'barracks',
    requires: 'tier-two',
  },
  wardcraft: {
    id: 'wardcraft',
    race: 'candlebound',
    label: 'Deep Wardcraft',
    description: 'All Candlebound wards project more strongly.',
    cost: 130,
    time: 12,
    producer: 'command',
    requires: 'tier-two',
  },
  'chorus-of-knives': {
    id: 'chorus-of-knives',
    race: 'hollow',
    label: 'Chorus of Knives',
    description: 'Choir combat units move faster and project more terror.',
    cost: 135,
    time: 12,
    producer: 'barracks',
    requires: 'tier-two',
  },
  'pit-broods': {
    id: 'pit-broods',
    race: 'hollow',
    label: 'Pit Broods',
    description: 'Choir Husks cost less and train more quickly.',
    cost: 150,
    time: 14,
    producer: 'barracks',
    requires: 'tier-two',
  },
  'vault-plate': {
    id: 'vault-plate',
    race: 'sepulcher',
    label: 'Vault Plate',
    description: 'Host structures and infantry gain additional health.',
    cost: 165,
    time: 15,
    producer: 'command',
    requires: 'tier-two',
  },
  'siege-liturgy': {
    id: 'siege-liturgy',
    race: 'sepulcher',
    label: 'Siege Liturgy',
    description: 'Iron Penitents deal heavier damage to structures.',
    cost: 155,
    time: 14,
    producer: 'barracks',
    requires: 'tier-two',
  },
}

export function researchForRace(race: RaceId): ResearchDef[] {
  return Object.values(RESEARCH_DEFS).filter((research) => research.race === null || research.race === race)
}
