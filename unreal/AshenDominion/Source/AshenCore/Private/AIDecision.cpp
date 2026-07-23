#include "ashen/core/AIDecision.hpp"

namespace ashen::core {

std::string_view to_string(const AIDecisionLayer layer) noexcept {
  switch (layer) {
    case AIDecisionLayer::Strategic:
      return "strategic";
    case AIDecisionLayer::Tactical:
      return "tactical";
    case AIDecisionLayer::Micro:
      return "micro";
  }
  return "unknown";
}

std::string_view to_string(const AIAction action) noexcept {
  switch (action) {
    case AIAction::AssignGatherers:
      return "assign_gatherers";
    case AIAction::ResumeBarracks:
      return "resume_barracks";
    case AIAction::BuildBarracks:
      return "build_barracks";
    case AIAction::ResumeTurret:
      return "resume_turret";
    case AIAction::BuildTurret:
      return "build_turret";
    case AIAction::TrainWorker:
      return "train_worker";
    case AIAction::TrainVanguard:
      return "train_vanguard";
    case AIAction::TrainSkirmisher:
      return "train_skirmisher";
    case AIAction::ResearchTierTwo:
      return "research_tier_two";
    case AIAction::ResearchDoctrine:
      return "research_doctrine";
    case AIAction::Scout:
      return "scout";
    case AIAction::CaptureObjective:
      return "capture_objective";
    case AIAction::ReinforceFront:
      return "reinforce_front";
    case AIAction::EngageForce:
      return "engage_force";
    case AIAction::AssaultCommand:
      return "assault_command";
    case AIAction::SearchEnemyCommand:
      return "search_enemy_command";
    case AIAction::ActivatePower:
      return "activate_power";
    case AIAction::Retreat:
      return "retreat";
    case AIAction::Kite:
      return "kite";
    case AIAction::FocusFire:
      return "focus_fire";
    case AIAction::ScreenRanged:
      return "screen_ranged";
    case AIAction::RejoinFormation:
      return "rejoin_formation";
  }
  return "unknown";
}

std::string_view to_string(const AIUtilityReason reason) noexcept {
  switch (reason) {
    case AIUtilityReason::Baseline:
      return "baseline";
    case AIUtilityReason::IdleWorkers:
      return "idle_workers";
    case AIUtilityReason::RequiredOpening:
      return "required_opening";
    case AIUtilityReason::OrphanedConstruction:
      return "orphaned_construction";
    case AIUtilityReason::SupplyPressure:
      return "supply_pressure";
    case AIUtilityReason::EconomyTarget:
      return "economy_target";
    case AIUtilityReason::ProductionCapacity:
      return "production_capacity";
    case AIUtilityReason::TechnologyTiming:
      return "technology_timing";
    case AIUtilityReason::FactionDoctrine:
      return "faction_doctrine";
    case AIUtilityReason::CounterArmored:
      return "counter_armored";
    case AIUtilityReason::PressureStructures:
      return "pressure_structures";
    case AIUtilityReason::CompositionBalance:
      return "composition_balance";
    case AIUtilityReason::InformationNeed:
      return "information_need";
    case AIUtilityReason::ObjectiveAvailable:
      return "objective_available";
    case AIUtilityReason::ReinforcementReady:
      return "reinforcement_ready";
    case AIUtilityReason::FavorableEngagement:
      return "favorable_engagement";
    case AIUtilityReason::EnemyCommandExposed:
      return "enemy_command_exposed";
    case AIUtilityReason::LastKnownCommand:
      return "last_known_command";
    case AIUtilityReason::AbilityOpportunity:
      return "ability_opportunity";
    case AIUtilityReason::Outnumbered:
      return "outnumbered";
    case AIUtilityReason::CriticalHealth:
      return "critical_health";
    case AIUtilityReason::LowResolve:
      return "low_resolve";
    case AIUtilityReason::ResolvePreservation:
      return "resolve_preservation";
    case AIUtilityReason::DreadExploitation:
      return "dread_exploitation";
    case AIUtilityReason::AcceptableLosses:
      return "acceptable_losses";
    case AIUtilityReason::WeaponCoolingDown:
      return "weapon_cooling_down";
    case AIUtilityReason::MeleePressure:
      return "melee_pressure";
    case AIUtilityReason::VulnerableTarget:
      return "vulnerable_target";
    case AIUtilityReason::HighThreatTarget:
      return "high_threat_target";
    case AIUtilityReason::RangedLineThreatened:
      return "ranged_line_threatened";
    case AIUtilityReason::FormationSpread:
      return "formation_spread";
    case AIUtilityReason::FormationDoctrine:
      return "formation_doctrine";
    case AIUtilityReason::WardSupport:
      return "ward_support";
    case AIUtilityReason::ScoutingDoctrine:
      return "scouting_doctrine";
    case AIUtilityReason::FlankSafety:
      return "flank_safety";
    case AIUtilityReason::DangerAvoidance:
      return "danger_avoidance";
    case AIUtilityReason::FriendlySupport:
      return "friendly_support";
    case AIUtilityReason::TravelEfficiency:
      return "travel_efficiency";
    case AIUtilityReason::TerrorAvoidance:
      return "terror_avoidance";
    case AIUtilityReason::UncertaintyReduction:
      return "uncertainty_reduction";
    case AIUtilityReason::CombatRecovery:
      return "combat_recovery";
  }
  return "unknown";
}

std::string_view to_string(const AICommandStatus status) noexcept {
  switch (status) {
    case AICommandStatus::Queued:
      return "queued";
    case AICommandStatus::Accepted:
      return "accepted";
    case AICommandStatus::Rejected:
      return "rejected";
  }
  return "unknown";
}

}  // namespace ashen::core
