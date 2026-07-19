#pragma once

#include "ashen/core/Types.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace ashen::core {

enum class AIDecisionLayer : std::uint8_t { Strategic, Tactical, Micro };

enum class AIAction : std::uint8_t {
  AssignGatherers,
  ResumeBarracks,
  BuildBarracks,
  ResumeTurret,
  BuildTurret,
  TrainWorker,
  TrainVanguard,
  TrainSkirmisher,
  ResearchTierTwo,
  ResearchDoctrine,
  Scout,
  CaptureObjective,
  ReinforceFront,
  EngageForce,
  AssaultCommand,
  SearchEnemyCommand,
  ActivatePower,
  Retreat,
  Kite,
  FocusFire,
  ScreenRanged,
  RejoinFormation,
};

enum class AIUtilityReason : std::uint8_t {
  Baseline,
  IdleWorkers,
  RequiredOpening,
  OrphanedConstruction,
  SupplyPressure,
  EconomyTarget,
  ProductionCapacity,
  TechnologyTiming,
  FactionDoctrine,
  CounterArmored,
  PressureStructures,
  CompositionBalance,
  InformationNeed,
  ObjectiveAvailable,
  ReinforcementReady,
  FavorableEngagement,
  EnemyCommandExposed,
  LastKnownCommand,
  AbilityOpportunity,
  Outnumbered,
  CriticalHealth,
  LowResolve,
  WeaponCoolingDown,
  MeleePressure,
  VulnerableTarget,
  HighThreatTarget,
  RangedLineThreatened,
  FormationSpread,
};

enum class AICommandStatus : std::uint8_t { Queued, Accepted, Rejected };

inline constexpr Tick kStrategicDecisionCadence = 80;
inline constexpr Tick kTacticalDecisionCadence = 120;
inline constexpr Tick kTacticalDecisionPhase = 30;
inline constexpr Tick kMicroDecisionCadence = 12;

[[nodiscard]] constexpr Tick ai_decision_cadence(const AIDecisionLayer layer) noexcept {
  switch (layer) {
    case AIDecisionLayer::Strategic:
      return kStrategicDecisionCadence;
    case AIDecisionLayer::Tactical:
      return kTacticalDecisionCadence;
    case AIDecisionLayer::Micro:
      return kMicroDecisionCadence;
  }
  return 1;
}

[[nodiscard]] constexpr bool ai_decision_due(const AIDecisionLayer layer, const Tick tick) noexcept {
  switch (layer) {
    case AIDecisionLayer::Strategic:
      return tick == 1 || (tick > 0 && tick % kStrategicDecisionCadence == 0);
    case AIDecisionLayer::Tactical:
      return tick >= kTacticalDecisionPhase &&
             (tick - kTacticalDecisionPhase) % kTacticalDecisionCadence == 0;
    case AIDecisionLayer::Micro:
      return tick >= kMicroDecisionCadence && tick % kMicroDecisionCadence == 0;
  }
  return false;
}

struct AIUtilityComponent {
  AIUtilityReason reason{AIUtilityReason::Baseline};
  std::int32_t score{};

  auto operator<=>(const AIUtilityComponent&) const = default;
};

struct AICandidateScore {
  AIAction action{AIAction::AssignGatherers};
  EntityId target_entity{};
  ControlPointId target_objective{};
  Vec2 target_position{};
  std::optional<EntityType> entity_type{};
  std::optional<ResearchId> research{};
  std::int32_t total_score{};
  std::vector<AIUtilityComponent> components{};

  auto operator<=>(const AICandidateScore&) const = default;
};

struct AIPlannedDecision {
  AIDecisionLayer layer{AIDecisionLayer::Strategic};
  Tick cadence_ticks{kStrategicDecisionCadence};
  std::vector<AICandidateScore> candidates{};
  std::size_t selected_candidate{};
  AIAction selected_action{AIAction::AssignGatherers};
  AIUtilityReason winning_reason{AIUtilityReason::Baseline};
  Command command{};

  auto operator<=>(const AIPlannedDecision&) const = default;
};

struct CommanderPlan {
  std::vector<AIPlannedDecision> decisions{};

  auto operator<=>(const CommanderPlan&) const = default;
};

struct AIDecisionRecord {
  std::uint64_t id{};
  Tick observation_tick{};
  std::uint64_t observation_hash{};
  PlayerId player{PlayerId::One};
  AIDecisionLayer layer{AIDecisionLayer::Strategic};
  Tick cadence_ticks{kStrategicDecisionCadence};
  std::vector<AICandidateScore> candidates{};
  std::size_t selected_candidate{};
  AIAction selected_action{AIAction::AssignGatherers};
  AIUtilityReason winning_reason{AIUtilityReason::Baseline};
  Command command{};
  std::uint64_t command_sequence{};
  Tick applied_tick{};
  AICommandStatus command_status{AICommandStatus::Queued};
  CommandError command_error{CommandError::None};

  auto operator<=>(const AIDecisionRecord&) const = default;
};

[[nodiscard]] ASHENCORE_API std::string_view to_string(AIDecisionLayer layer) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(AIAction action) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(AIUtilityReason reason) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(AICommandStatus status) noexcept;

}  // namespace ashen::core
