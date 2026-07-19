#include "AIPlanningInternal.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace ashen::core::ai {
namespace {

[[nodiscard]] std::vector<EntityId> entity_ids(
    const std::vector<const Entity*>& entities) {
  std::vector<EntityId> result;
  result.reserve(entities.size());
  for (const auto* entity : entities) {
    result.push_back(entity->id);
  }
  return result;
}

[[nodiscard]] std::int32_t average_health_basis(
    const std::vector<const Entity*>& entities) noexcept {
  if (entities.empty()) {
    return 10'000;
  }
  std::int64_t total = 0;
  for (const auto* entity : entities) {
    total += entity->max_hit_points <= 0
                 ? 0
                 : std::clamp(entity->hit_points * 10'000 / entity->max_hit_points, 0, 10'000);
  }
  return static_cast<std::int32_t>(total / static_cast<std::int64_t>(entities.size()));
}

[[nodiscard]] std::int32_t average_resolve(
    const std::vector<const Entity*>& entities) noexcept {
  if (entities.empty()) {
    return 100;
  }
  std::int64_t total = 0;
  for (const auto* entity : entities) {
    total += entity->resolve;
  }
  return static_cast<std::int32_t>(total / static_cast<std::int64_t>(entities.size()));
}

[[nodiscard]] const ObservedEnemy* latest_known_command(
    const PlanningContext& context) noexcept {
  const ObservedEnemy* result = nullptr;
  for (const auto& enemy : context.observation.known_enemies()) {
    if (enemy.type != EntityType::Command || enemy.hit_points <= 0) {
      continue;
    }
    if (result == nullptr || (enemy.currently_visible && !result->currently_visible) ||
        (enemy.currently_visible == result->currently_visible &&
         enemy.last_observed_tick > result->last_observed_tick) ||
        (enemy.currently_visible == result->currently_visible &&
         enemy.last_observed_tick == result->last_observed_tick &&
         enemy.id.value < result->id.value)) {
      result = &enemy;
    }
  }
  return result;
}

void add_retreat_candidate(const PlanningContext& context,
                           std::vector<ScoredCommand>& candidates) {
  if (context.army.empty() || context.command_building == nullptr ||
      context.visible_enemy_power <= 0) {
    return;
  }
  const auto health = average_health_basis(context.army);
  const auto resolve = average_resolve(context.army);
  const auto outnumbered = static_cast<std::int64_t>(context.visible_enemy_power) * 100 >
                           static_cast<std::int64_t>(context.friendly_power) * 125;
  const auto damaged = health < 4'500;
  const auto wavering = resolve < 40;
  if (!outnumbered && !damaged && !wavering) {
    return;
  }

  auto retreat = command_for(context.observation.player(), CommandType::Retreat);
  retreat.entities = entity_ids(context.army);
  auto candidate = CandidateBuilder{AIAction::Retreat, std::move(retreat)};
  candidate.add(AIUtilityReason::Baseline, 1'000);
  if (outnumbered) {
    const auto deficit = std::max(0, context.visible_enemy_power - context.friendly_power);
    candidate.add(AIUtilityReason::Outnumbered, 13'000 + std::min(4'000, deficit * 10));
  }
  if (damaged) {
    candidate.add(AIUtilityReason::CriticalHealth, 8'000 + (4'500 - health));
  }
  if (wavering) {
    candidate.add(AIUtilityReason::LowResolve, 7'000 + (40 - resolve) * 80);
  }
  candidate.position(context.command_building->position);
  candidates.push_back(std::move(candidate).finish());
}

void add_power_candidate(const PlanningContext& context,
                         std::vector<ScoredCommand>& candidates) {
  if (context.ready_army.size() < 3 || context.visible_enemies.empty() ||
      !context.observation.permits(CommandType::ActivatePower)) {
    return;
  }
  auto power = command_for(context.observation.player(), CommandType::ActivatePower);
  auto candidate = CandidateBuilder{AIAction::ActivatePower, std::move(power)};
  candidate.add(AIUtilityReason::Baseline, 2'500)
      .add(AIUtilityReason::AbilityOpportunity, 5'500);
  if (context.visible_enemy_power > context.friendly_power) {
    candidate.add(AIUtilityReason::Outnumbered, 2'000);
  }
  candidates.push_back(std::move(candidate).finish());
}

void add_engagement_candidates(const PlanningContext& context,
                               std::vector<ScoredCommand>& candidates) {
  if (context.ready_army.empty() || context.visible_enemies.empty()) {
    return;
  }
  const auto* known_command = latest_known_command(context);
  if (known_command != nullptr && known_command->currently_visible &&
      context.ready_army.size() >= 3) {
    auto assault = command_for(context.observation.player(), CommandType::Attack);
    assault.entities = entity_ids(context.ready_army);
    assault.target_entity = known_command->id;
    candidates.push_back(std::move(CandidateBuilder{AIAction::AssaultCommand, std::move(assault)}
                                       .add(AIUtilityReason::Baseline, 3'000)
                                       .add(AIUtilityReason::EnemyCommandExposed, 11'000)
                                       .target(known_command->id)
                                       .position(known_command->position))
                             .finish());
  }

  std::vector<const ObservedEnemy*> combat_enemies;
  for (const auto* enemy : context.visible_enemies) {
    if (is_army_unit(enemy->type)) {
      combat_enemies.push_back(enemy);
    }
  }
  if (combat_enemies.empty() ||
      static_cast<std::int64_t>(context.ready_power) * 100 <
          static_cast<std::int64_t>(context.visible_enemy_power) * 80) {
    return;
  }
  const auto target = enemy_centroid(combat_enemies);
  auto engage = command_for(context.observation.player(), CommandType::AttackMove);
  engage.entities = entity_ids(context.ready_army);
  engage.target = target;
  const auto advantage = std::max(0, context.ready_power - context.visible_enemy_power);
  candidates.push_back(std::move(CandidateBuilder{AIAction::EngageForce, std::move(engage)}
                                     .add(AIUtilityReason::Baseline, 3'000)
                                     .add(AIUtilityReason::FavorableEngagement,
                                          4'500 + std::min(4'000, advantage * 8))
                                     .position(target))
                           .finish());
}

void add_reinforcement_candidate(const PlanningContext& context,
                                 std::vector<ScoredCommand>& candidates) {
  std::vector<const Entity*> active;
  std::vector<const Entity*> idle;
  for (const auto* unit : context.ready_army) {
    if (unit->order.type == OrderType::Attack || unit->order.type == OrderType::AttackMove) {
      active.push_back(unit);
    } else if (unit->order.type == OrderType::Idle || unit->order.type == OrderType::Hold) {
      idle.push_back(unit);
    }
  }
  if (active.size() < 2 || idle.empty()) {
    return;
  }
  const auto target = centroid(active);
  auto reinforce = command_for(context.observation.player(), CommandType::AttackMove);
  reinforce.entities = entity_ids(idle);
  reinforce.target = target;
  candidates.push_back(std::move(CandidateBuilder{AIAction::ReinforceFront, std::move(reinforce)}
                                     .add(AIUtilityReason::Baseline, 2'000)
                                     .add(AIUtilityReason::ReinforcementReady,
                                          4'500 + static_cast<std::int32_t>(idle.size()) * 300)
                                     .position(target))
                           .finish());
}

void add_objective_candidates(const PlanningContext& context,
                              std::vector<ScoredCommand>& candidates) {
  if (context.ready_army.size() < 3 || !context.visible_enemies.empty()) {
    return;
  }
  const auto origin = centroid(context.ready_army);
  for (const auto& objective : context.observation.public_objectives()) {
    if (objective.has_observed_state && objective.last_observed_owner == context.observation.player()) {
      continue;
    }
    auto capture = command_for(context.observation.player(), CommandType::AttackMove);
    capture.entities = entity_ids(context.ready_army);
    capture.target = objective.position;
    const auto distance_world = static_cast<std::int32_t>(
        integer_sqrt(squared_distance(origin, objective.position)) /
        static_cast<std::uint64_t>(kWorldScale));
    candidates.push_back(std::move(CandidateBuilder{AIAction::CaptureObjective, std::move(capture)}
                                       .add(AIUtilityReason::Baseline, 2'000)
                                       .add(AIUtilityReason::ObjectiveAvailable,
                                            5'000 - std::min(2'500, distance_world))
                                       .objective(objective.id)
                                       .position(objective.position))
                             .finish());
  }
}

void add_search_candidate(const PlanningContext& context,
                          std::vector<ScoredCommand>& candidates) {
  if (!context.visible_enemies.empty()) {
    return;
  }
  const auto* known_command = latest_known_command(context);
  if (known_command != nullptr && !known_command->currently_visible &&
      context.ready_army.size() >= 3) {
    auto search = command_for(context.observation.player(), CommandType::AttackMove);
    search.entities = entity_ids(context.ready_army);
    search.target = known_command->position;
    const auto age = context.observation.tick() - known_command->last_observed_tick;
    candidates.push_back(std::move(CandidateBuilder{AIAction::SearchEnemyCommand, std::move(search)}
                                       .add(AIUtilityReason::Baseline, 2'000)
                                       .add(AIUtilityReason::LastKnownCommand,
                                            6'500 - static_cast<std::int32_t>(std::min<Tick>(2'500, age)))
                                       .target(known_command->id)
                                       .position(known_command->position))
                             .finish());
    return;
  }

  if (context.observation.tick() >= 2'400 && context.ready_army.size() >= 3 &&
      context.command_building != nullptr) {
    const auto target = Vec2{context.observation.map_size().x - context.command_building->position.x,
                             context.command_building->position.y};
    auto search = command_for(context.observation.player(), CommandType::AttackMove);
    search.entities = entity_ids(context.ready_army);
    search.target = target;
    candidates.push_back(std::move(CandidateBuilder{AIAction::SearchEnemyCommand, std::move(search)}
                                       .add(AIUtilityReason::Baseline, 2'000)
                                       .add(AIUtilityReason::InformationNeed, 4'600)
                                       .position(target))
                             .finish());
  }
}

void add_scout_candidate(const PlanningContext& context,
                         std::vector<ScoredCommand>& candidates) {
  if (!context.visible_enemies.empty() || context.observation.tick() < 300 ||
      context.command_building == nullptr) {
    return;
  }

  const Entity* scout = nullptr;
  const auto ready_skirmisher = std::ranges::find_if(
      context.ready_army, [](const Entity* entity) {
        return entity->type == EntityType::Skirmisher;
      });
  if (ready_skirmisher != context.ready_army.end()) {
    scout = *ready_skirmisher;
  } else if (context.ready_army.size() >= 2) {
    scout = context.ready_army.front();
  } else if (context.workers.size() > 4) {
    scout = context.workers.back();
  }
  if (scout == nullptr) {
    return;
  }
  const auto target = Vec2{context.observation.map_size().x - context.command_building->position.x,
                           context.command_building->position.y};
  if ((scout->order.type == OrderType::Move || scout->order.type == OrderType::AttackMove) &&
      squared_distance(scout->order.target, target) <=
          static_cast<std::uint64_t>(80'000) * 80'000U) {
    return;
  }

  auto command = command_for(context.observation.player(), CommandType::AttackMove);
  command.entities = {scout->id};
  command.target = target;
  candidates.push_back(std::move(CandidateBuilder{AIAction::Scout, std::move(command)}
                                     .add(AIUtilityReason::Baseline, 1'500)
                                     .add(AIUtilityReason::InformationNeed, 3'800)
                                     .target(scout->id)
                                     .position(target))
                           .finish());
}

}  // namespace

std::optional<AIPlannedDecision> evaluate_tactical_layer(const PlanningContext& context) {
  std::vector<ScoredCommand> candidates;
  add_retreat_candidate(context, candidates);
  add_power_candidate(context, candidates);
  add_engagement_candidates(context, candidates);
  add_reinforcement_candidate(context, candidates);
  add_objective_candidates(context, candidates);
  add_search_candidate(context, candidates);
  add_scout_candidate(context, candidates);
  return select_decision(AIDecisionLayer::Tactical, kTacticalDecisionCadence,
                         std::move(candidates));
}

}  // namespace ashen::core::ai
