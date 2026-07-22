#include "AIPlanningInternal.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace ashen::core::ai {
namespace {

struct DestinationChoice {
  Vec2 position{};
  AIInfluenceSample sample{};
  std::int64_t score{std::numeric_limits<std::int64_t>::min()};
};

[[nodiscard]] std::vector<EntityId> entity_ids(const std::vector<const Entity*>& entities) {
  std::vector<EntityId> result;
  result.reserve(entities.size());
  for (const auto* entity : entities) {
    result.push_back(entity->id);
  }
  return result;
}

[[nodiscard]] std::vector<const Entity*> tactical_field_force(const PlanningContext& context) {
  if (context.search_commitment) {
    return context.army;
  }
  if (!context.ready_army.empty()) {
    return context.ready_army;
  }
  return {};
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

[[nodiscard]] std::int32_t average_resolve(const std::vector<const Entity*>& entities) noexcept {
  if (entities.empty()) {
    return 100;
  }
  std::int64_t total = 0;
  for (const auto* entity : entities) {
    total += entity->resolve;
  }
  return static_cast<std::int32_t>(total / static_cast<std::int64_t>(entities.size()));
}

[[nodiscard]] std::int32_t cell_distance(const AIInfluenceMap& map, const Vec2 left,
                                         const Vec2 right) noexcept {
  return static_cast<std::int32_t>(integer_sqrt(squared_distance(left, right)) /
                                   static_cast<std::uint64_t>(map.cell_size()));
}

[[nodiscard]] std::int32_t danger_value(const AIInfluenceCell& cell) noexcept {
  const auto value = static_cast<std::int64_t>(cell.observed_enemy_power) * 6 +
                     static_cast<std::int64_t>(cell.static_danger) * 8 +
                     static_cast<std::int64_t>(cell.terror_pressure) * 2;
  return static_cast<std::int32_t>(std::min<std::int64_t>(50'000, value));
}

[[nodiscard]] std::int32_t corridor_danger(const AIInfluenceMap& map, const Vec2 start,
                                           const Vec2 end) noexcept {
  const auto dx = static_cast<std::int64_t>(end.x) - start.x;
  const auto dy = static_cast<std::int64_t>(end.y) - start.y;
  const auto distance = integer_sqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
  const auto samples =
      std::max<std::uint64_t>(1, distance / static_cast<std::uint64_t>(map.cell_size()));
  std::int64_t total = 0;
  for (std::uint64_t index = 0; index <= samples; ++index) {
    const Vec2 point{
        start.x + static_cast<std::int32_t>(dx * static_cast<std::int64_t>(index) /
                                            static_cast<std::int64_t>(samples)),
        start.y + static_cast<std::int32_t>(dy * static_cast<std::int64_t>(index) /
                                            static_cast<std::int64_t>(samples)),
    };
    total += danger_value(map.cell_at(point));
  }
  return static_cast<std::int32_t>(
      std::min<std::int64_t>(50'000, total / static_cast<std::int64_t>(samples + 1)));
}

[[nodiscard]] bool usable_cell(const PlanningContext& context,
                               const AIInfluenceCell& cell) noexcept {
  return cell.navigable &&
         (context.command_building == nullptr || cell.travel_cost < kAIUnreachableTravelCost);
}

[[nodiscard]] std::int32_t stable_tie_bonus(const PlanningContext& context,
                                            const AIInfluenceSample& sample) noexcept {
  auto value = context.strategy ^ (static_cast<std::uint64_t>(sample.row) << 32U) ^
               static_cast<std::uint32_t>(sample.column);
  value ^= value >> 33U;
  value *= 0xff51afd7ed558ccdULL;
  value ^= value >> 33U;
  return static_cast<std::int32_t>(value & 31U);
}

[[nodiscard]] DestinationChoice safe_approach(const PlanningContext& context, const Vec2 origin,
                                              const Vec2 goal) noexcept {
  const auto& map = context.tactical_map();
  const auto goal_sample = map.sample_at(goal);
  DestinationChoice best{goal_sample.center, goal_sample};
  constexpr std::int32_t radius = 2;
  for (auto row = std::max(0, goal_sample.row - radius);
       row <= std::min(map.rows() - 1, goal_sample.row + radius); ++row) {
    for (auto column = std::max(0, goal_sample.column - radius);
         column <= std::min(map.columns() - 1, goal_sample.column + radius); ++column) {
      const auto position = map.cell_center(column, row);
      const auto sample = map.sample_at(position);
      if (!usable_cell(context, sample.cell)) {
        continue;
      }
      const auto goal_steps = cell_distance(map, position, goal);
      const auto travel_steps = cell_distance(map, origin, position);
      const auto danger = danger_value(sample.cell);
      const auto route_danger = corridor_danger(map, origin, position);
      const auto score = -static_cast<std::int64_t>(std::abs(goal_steps - 1)) * 1'200 -
                         static_cast<std::int64_t>(travel_steps) * 45 -
                         static_cast<std::int64_t>(danger) * 5 -
                         static_cast<std::int64_t>(route_danger) * 2 +
                         static_cast<std::int64_t>(sample.cell.friendly_power) * 4 -
                         sample.cell.uncertainty + stable_tie_bonus(context, sample);
      if (score > best.score || (score == best.score && position < best.position)) {
        best = {position, sample, score};
      }
    }
  }
  return best;
}

[[nodiscard]] DestinationChoice safe_advance(const PlanningContext& context, const Vec2 origin,
                                             const Vec2 goal, const bool scouting) noexcept {
  const auto& map = context.tactical_map();
  const auto origin_to_goal = cell_distance(map, origin, goal);
  const auto goal_sample = map.sample_at(goal);
  if (origin_to_goal <= 2 && usable_cell(context, goal_sample.cell)) {
    return {goal, goal_sample, std::numeric_limits<std::int64_t>::max()};
  }
  DestinationChoice best{};
  constexpr std::int32_t maximum_leg = 6;
  for (std::int32_t row = 0; row < map.rows(); ++row) {
    for (std::int32_t column = 0; column < map.columns(); ++column) {
      const auto position = map.cell_center(column, row);
      const auto sample = map.sample_at(position);
      if (!usable_cell(context, sample.cell)) {
        continue;
      }
      const auto leg = cell_distance(map, origin, position);
      const auto remaining = cell_distance(map, position, goal);
      const auto progress = origin_to_goal - remaining;
      if (leg == 0 || leg > maximum_leg || progress <= 0) {
        continue;
      }
      const auto danger = danger_value(sample.cell);
      const auto route_danger = corridor_danger(map, origin, position);
      const auto information = scouting ? sample.cell.uncertainty * 3 : -sample.cell.uncertainty;
      const auto score =
          static_cast<std::int64_t>(progress) * 3'000 - static_cast<std::int64_t>(leg) * 90 -
          static_cast<std::int64_t>(danger) * 3 - static_cast<std::int64_t>(route_danger) * 2 +
          static_cast<std::int64_t>(sample.cell.friendly_power) * 3 + information +
          stable_tie_bonus(context, sample);
      if (score > best.score || (score == best.score && position < best.position)) {
        best = {position, sample, score};
      }
    }
  }
  return best.score == std::numeric_limits<std::int64_t>::min()
             ? safe_approach(context, origin, goal)
             : best;
}

[[nodiscard]] DestinationChoice safest_shelter(const PlanningContext& context,
                                               const Vec2 origin) noexcept {
  const auto& map = context.tactical_map();
  const auto shelter =
      context.command_building == nullptr ? origin : context.command_building->position;
  const auto shelter_sample = map.sample_at(shelter);
  DestinationChoice best{shelter_sample.center, shelter_sample};
  constexpr std::int32_t radius = 4;
  for (auto row = std::max(0, shelter_sample.row - radius);
       row <= std::min(map.rows() - 1, shelter_sample.row + radius); ++row) {
    for (auto column = std::max(0, shelter_sample.column - radius);
         column <= std::min(map.columns() - 1, shelter_sample.column + radius); ++column) {
      const auto position = map.cell_center(column, row);
      const auto sample = map.sample_at(position);
      if (!usable_cell(context, sample.cell)) {
        continue;
      }
      const auto danger = danger_value(sample.cell);
      const auto route_danger = corridor_danger(map, origin, position);
      const auto shelter_distance = cell_distance(map, shelter, position);
      const auto score = static_cast<std::int64_t>(sample.cell.friendly_power) * 10 -
                         static_cast<std::int64_t>(danger) * 8 -
                         static_cast<std::int64_t>(route_danger) * 4 -
                         static_cast<std::int64_t>(shelter_distance) * 250 -
                         static_cast<std::int64_t>(sample.cell.uncertainty) * 2 +
                         stable_tie_bonus(context, sample);
      if (score > best.score || (score == best.score && position < best.position)) {
        best = {position, sample, score};
      }
    }
  }
  return best;
}

[[nodiscard]] DestinationChoice reinforcement_staging(const PlanningContext& context,
                                                      const Vec2 origin,
                                                      const Vec2 front) noexcept {
  const auto& map = context.tactical_map();
  const auto front_sample = map.sample_at(front);
  DestinationChoice best{front_sample.center, front_sample};
  constexpr std::int32_t radius = 2;
  for (auto row = std::max(0, front_sample.row - radius);
       row <= std::min(map.rows() - 1, front_sample.row + radius); ++row) {
    for (auto column = std::max(0, front_sample.column - radius);
         column <= std::min(map.columns() - 1, front_sample.column + radius); ++column) {
      const auto position = map.cell_center(column, row);
      const auto sample = map.sample_at(position);
      if (!usable_cell(context, sample.cell)) {
        continue;
      }
      const auto danger = danger_value(sample.cell);
      const auto route_danger = corridor_danger(map, origin, position);
      const auto front_distance = cell_distance(map, front, position);
      const auto score = static_cast<std::int64_t>(sample.cell.friendly_power) * 9 -
                         static_cast<std::int64_t>(danger) * 7 -
                         static_cast<std::int64_t>(route_danger) * 3 -
                         static_cast<std::int64_t>(front_distance) * 300 - sample.cell.uncertainty +
                         stable_tie_bonus(context, sample);
      if (score > best.score || (score == best.score && position < best.position)) {
        best = {position, sample, score};
      }
    }
  }
  return best;
}

[[nodiscard]] Vec2 scouting_goal(const PlanningContext& context, const Vec2 origin) noexcept {
  const auto& map = context.tactical_map();
  DestinationChoice best{};
  for (std::int32_t row = 0; row < map.rows(); ++row) {
    for (std::int32_t column = 0; column < map.columns(); ++column) {
      const auto position = map.cell_center(column, row);
      const auto sample = map.sample_at(position);
      if (!usable_cell(context, sample.cell)) {
        continue;
      }
      const auto distance = cell_distance(map, origin, position);
      if (distance < 2) {
        continue;
      }
      const auto enemy_depth =
          context.observation.player() == PlayerId::One ? column : map.columns() - 1 - column;
      const auto score = static_cast<std::int64_t>(sample.cell.uncertainty) * 8 +
                         static_cast<std::int64_t>(sample.cell.objective_value) * 2 -
                         static_cast<std::int64_t>(danger_value(sample.cell)) * 8 -
                         static_cast<std::int64_t>(distance) * 70 -
                         static_cast<std::int64_t>(sample.cell.travel_cost) * 2 +
                         static_cast<std::int64_t>(enemy_depth) * 220 +
                         stable_tie_bonus(context, sample);
      if (score > best.score || (score == best.score && position < best.position)) {
        best = {position, sample, score};
      }
    }
  }
  return best.score == std::numeric_limits<std::int64_t>::min() ? origin : best.position;
}

void add_influence_utility(CandidateBuilder& candidate, const AIInfluenceSample& sample,
                           const bool scouting = false, const bool flanking = false) {
  const auto danger = danger_value(sample.cell);
  candidate.add(AIUtilityReason::DangerAvoidance, std::clamp(1'800 - danger / 2, -3'000, 1'800));
  candidate.add(AIUtilityReason::FriendlySupport, std::min(1'500, sample.cell.friendly_power * 4));
  candidate.add(AIUtilityReason::TravelEfficiency,
                sample.cell.travel_cost >= kAIUnreachableTravelCost
                    ? -3'000
                    : std::max(0, 1'200 - sample.cell.travel_cost * 3));
  candidate.add(AIUtilityReason::TerrorAvoidance,
                std::clamp(700 - sample.cell.terror_pressure * 3, -1'500, 700));
  if (scouting) {
    candidate.add(AIUtilityReason::UncertaintyReduction,
                  std::min(2'000, sample.cell.uncertainty * 2));
  }
  if (flanking) {
    candidate.add(AIUtilityReason::FlankSafety,
                  std::clamp(1'200 - sample.cell.static_danger * 2, -2'000, 1'200));
  }
}

[[nodiscard]] const ObservedEnemy* latest_known_command(const PlanningContext& context) noexcept {
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

void add_retreat_candidate(const PlanningContext& context, std::vector<ScoredCommand>& candidates) {
  if (context.army.empty() || context.command_building == nullptr ||
      context.visible_enemy_power <= 0 || context.attrition_commitment) {
    return;
  }
  std::vector<const Entity*> exposed;
  auto exposed_power = std::int32_t{};
  for (const auto* unit : context.army) {
    const auto shelter_radius =
        static_cast<std::int64_t>(context.command_building->radius) + 110'000;
    const auto sheltered = squared_distance(unit->position, context.command_building->position) <=
                           static_cast<std::uint64_t>(shelter_radius * shelter_radius);
    const auto settled = unit->order.type == OrderType::Idle || unit->order.type == OrderType::Hold;
    const auto retreating =
        unit->stance == UnitStance::Defensive && unit->order.type == OrderType::Move;
    if ((sheltered && settled) || retreating) {
      continue;
    }
    const auto threatened =
        std::ranges::any_of(context.visible_enemies, [unit](const ObservedEnemy* enemy) {
          constexpr auto danger_radius = std::uint64_t{360'000};
          return squared_distance(unit->position, enemy->position) <= danger_radius * danger_radius;
        });
    const auto committed = unit->order.type == OrderType::Attack ||
                           unit->order.type == OrderType::AttackMove ||
                           unit->order.type == OrderType::Patrol;
    if (!threatened && !committed) {
      continue;
    }
    exposed.push_back(unit);
    exposed_power += entity_power(*unit, context.observation.self().faction);
  }
  if (exposed.empty()) {
    return;
  }
  const auto health = average_health_basis(exposed);
  const auto resolve = average_resolve(exposed);
  const auto outnumbered = static_cast<std::int64_t>(context.visible_enemy_power) * 100 >
                           static_cast<std::int64_t>(exposed_power) * 125;
  const auto damaged = health < 4'500;
  const auto wavering = resolve < 40;
  if (!outnumbered && !damaged && !wavering) {
    return;
  }

  auto retreat = command_for(context.observation.player(), CommandType::Retreat);
  retreat.entities = entity_ids(exposed);
  const auto destination = safest_shelter(context, centroid(exposed));
  retreat.target = destination.position;
  auto candidate = CandidateBuilder{AIAction::Retreat, std::move(retreat)};
  candidate.add(AIUtilityReason::Baseline, 1'000);
  if (outnumbered) {
    const auto deficit = std::max(0, context.visible_enemy_power - exposed_power);
    candidate.add(AIUtilityReason::Outnumbered, 13'000 + std::min(4'000, deficit * 10));
  }
  if (damaged) {
    candidate.add(AIUtilityReason::CriticalHealth, 8'000 + (4'500 - health));
  }
  if (wavering) {
    candidate.add(AIUtilityReason::LowResolve, 7'000 + (40 - resolve) * 80);
  }
  candidate.position(destination.position).influence(context.tactical_map(), destination.position);
  add_influence_utility(candidate, destination.sample);
  candidates.push_back(std::move(candidate).finish());
}

void add_power_candidate(const PlanningContext& context, std::vector<ScoredCommand>& candidates) {
  if (context.ready_army.size() < 3 || context.visible_enemies.empty() ||
      !context.observation.permits(CommandType::ActivatePower)) {
    return;
  }
  auto power = command_for(context.observation.player(), CommandType::ActivatePower);
  const auto position = centroid(context.ready_army);
  const auto sample = context.tactical_map().sample_at(position);
  auto candidate = CandidateBuilder{AIAction::ActivatePower, std::move(power)};
  candidate.add(AIUtilityReason::Baseline, 2'500).add(AIUtilityReason::AbilityOpportunity, 5'500);
  if (context.visible_enemy_power > context.friendly_power) {
    candidate.add(AIUtilityReason::Outnumbered, 2'000);
  }
  candidate.position(position).influence(context.tactical_map(), position);
  add_influence_utility(candidate, sample);
  candidates.push_back(std::move(candidate).finish());
}

void add_engagement_candidates(const PlanningContext& context,
                               std::vector<ScoredCommand>& candidates) {
  const auto assault_force = tactical_field_force(context);
  if (assault_force.empty() || context.visible_enemies.empty()) {
    return;
  }
  const auto* known_command = latest_known_command(context);
  const auto minimum_assault_force =
      context.observation.tick() >= 2'400 ? std::size_t{1} : std::size_t{3};
  if (known_command != nullptr && known_command->currently_visible &&
      assault_force.size() >= minimum_assault_force) {
    const auto army_center = centroid(assault_force);
    const auto approach = safe_approach(context, army_center, known_command->position);
    const auto in_commit_range =
        integer_sqrt(squared_distance(army_center, known_command->position)) <=
        static_cast<std::uint64_t>(kAIInfluenceCellSize * 2);
    auto assault = command_for(context.observation.player(),
                               in_commit_range ? CommandType::Attack : CommandType::AttackMove);
    assault.entities = entity_ids(assault_force);
    if (in_commit_range) {
      assault.target_entity = known_command->id;
    } else {
      assault.target = approach.position;
    }
    const auto destination = in_commit_range ? known_command->position : approach.position;
    auto candidate = CandidateBuilder{AIAction::AssaultCommand, std::move(assault)};
    candidate.add(AIUtilityReason::Baseline, 3'000)
        .add(AIUtilityReason::EnemyCommandExposed, 11'000)
        .target(known_command->id)
        .position(destination)
        .influence(context.tactical_map(), destination);
    add_influence_utility(
        candidate,
        in_commit_range ? context.tactical_map().sample_at(destination) : approach.sample, false,
        !in_commit_range);
    candidates.push_back(std::move(candidate).finish());
  }

  if (context.ready_army.empty() && !context.attrition_commitment) {
    return;
  }

  std::vector<const ObservedEnemy*> combat_enemies;
  for (const auto* enemy : context.visible_enemies) {
    if (is_army_unit(enemy->type)) {
      combat_enemies.push_back(enemy);
    }
  }
  auto combat_power = std::int32_t{};
  for (const auto* unit : assault_force) {
    combat_power += entity_power(*unit, context.observation.self().faction);
  }
  if (combat_enemies.empty() || (!context.attrition_commitment &&
                                 static_cast<std::int64_t>(combat_power) * 100 <
                                     static_cast<std::int64_t>(context.visible_enemy_power) * 80)) {
    return;
  }
  const auto enemy_center = enemy_centroid(combat_enemies);
  const auto approach = safe_approach(context, centroid(assault_force), enemy_center);
  const auto target = approach.position;
  auto engage = command_for(context.observation.player(), CommandType::AttackMove);
  engage.entities = entity_ids(assault_force);
  engage.target = target;
  const auto advantage = std::max(0, combat_power - context.visible_enemy_power);
  auto candidate = CandidateBuilder{AIAction::EngageForce, std::move(engage)};
  candidate.add(AIUtilityReason::Baseline, 3'000)
      .add(AIUtilityReason::FavorableEngagement, 4'500 + std::min(4'000, advantage * 8))
      .position(target)
      .influence(context.tactical_map(), target);
  add_influence_utility(candidate, approach.sample, false, true);
  candidates.push_back(std::move(candidate).finish());
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
  const auto target = reinforcement_staging(context, centroid(idle), centroid(active));
  auto reinforce = command_for(context.observation.player(), CommandType::AttackMove);
  reinforce.entities = entity_ids(idle);
  reinforce.target = target.position;
  auto candidate = CandidateBuilder{AIAction::ReinforceFront, std::move(reinforce)};
  candidate.add(AIUtilityReason::Baseline, 2'000)
      .add(AIUtilityReason::ReinforcementReady,
           4'500 + static_cast<std::int32_t>(idle.size()) * 300)
      .position(target.position)
      .influence(context.tactical_map(), target.position);
  add_influence_utility(candidate, target.sample);
  candidates.push_back(std::move(candidate).finish());
}

void add_objective_candidates(const PlanningContext& context,
                              std::vector<ScoredCommand>& candidates) {
  const auto visible_combat_enemy =
      std::ranges::any_of(context.visible_enemies,
                          [](const ObservedEnemy* enemy) { return is_army_unit(enemy->type); });
  if (context.ready_army.size() < 3 || visible_combat_enemy) {
    return;
  }
  const auto origin = centroid(context.ready_army);
  for (const auto& objective : context.observation.public_objectives()) {
    if (objective.has_observed_state &&
        objective.last_observed_owner == context.observation.player()) {
      continue;
    }
    const auto destination = safe_advance(context, origin, objective.position, false);
    auto capture = command_for(context.observation.player(), CommandType::AttackMove);
    capture.entities = entity_ids(context.ready_army);
    capture.target = destination.position;
    const auto distance_world =
        static_cast<std::int32_t>(integer_sqrt(squared_distance(origin, objective.position)) /
                                  static_cast<std::uint64_t>(kWorldScale));
    auto candidate = CandidateBuilder{AIAction::CaptureObjective, std::move(capture)};
    candidate.add(AIUtilityReason::Baseline, 2'000)
        .add(AIUtilityReason::ObjectiveAvailable, 5'000 - std::min(2'500, distance_world))
        .objective(objective.id)
        .position(destination.position)
        .influence(context.tactical_map(), destination.position);
    add_influence_utility(candidate, destination.sample);
    candidates.push_back(std::move(candidate).finish());
  }
}

void add_search_candidate(const PlanningContext& context, std::vector<ScoredCommand>& candidates) {
  if (!context.visible_enemies.empty()) {
    return;
  }
  const auto search_force = tactical_field_force(context);
  const auto* known_command = latest_known_command(context);
  const auto minimum_search_force =
      context.observation.tick() >= 2'400 ? std::size_t{1} : std::size_t{3};
  if (known_command != nullptr && !known_command->currently_visible &&
      search_force.size() >= minimum_search_force) {
    const auto destination =
        context.search_commitment
            ? DestinationChoice{known_command->position,
                                context.tactical_map().sample_at(known_command->position),
                                std::numeric_limits<std::int64_t>::max()}
            : safe_advance(context, centroid(search_force), known_command->position, false);
    auto search = command_for(context.observation.player(), CommandType::AttackMove);
    search.entities = entity_ids(search_force);
    search.target = destination.position;
    const auto age = context.observation.tick() - known_command->last_observed_tick;
    auto candidate = CandidateBuilder{AIAction::SearchEnemyCommand, std::move(search)};
    candidate.add(AIUtilityReason::Baseline, 2'000)
        .add(AIUtilityReason::LastKnownCommand,
             6'500 - static_cast<std::int32_t>(std::min<Tick>(2'500, age)))
        .target(known_command->id)
        .position(destination.position)
        .influence(context.tactical_map(), destination.position);
    add_influence_utility(candidate, destination.sample);
    candidates.push_back(std::move(candidate).finish());
    return;
  }

  if (context.observation.tick() >= 2'400 && !search_force.empty() &&
      context.command_building != nullptr) {
    const auto goal = Vec2{context.observation.map_size().x - context.command_building->position.x,
                           context.command_building->position.y};
    const auto destination = context.search_commitment
                                 ? DestinationChoice{goal, context.tactical_map().sample_at(goal),
                                                     std::numeric_limits<std::int64_t>::max()}
                                 : safe_advance(context, centroid(search_force), goal, false);
    auto search = command_for(context.observation.player(), CommandType::AttackMove);
    search.entities = entity_ids(search_force);
    search.target = destination.position;
    auto candidate = CandidateBuilder{AIAction::SearchEnemyCommand, std::move(search)};
    candidate.add(AIUtilityReason::Baseline, 2'000)
        .add(AIUtilityReason::InformationNeed, 6'200)
        .position(destination.position)
        .influence(context.tactical_map(), destination.position);
    add_influence_utility(candidate, destination.sample);
    candidates.push_back(std::move(candidate).finish());
  }
}

void add_scout_candidate(const PlanningContext& context, std::vector<ScoredCommand>& candidates) {
  const auto field_force = tactical_field_force(context);
  if (!context.visible_enemies.empty() || context.observation.tick() < 300 ||
      context.command_building == nullptr ||
      (context.observation.tick() >= 2'400 && !field_force.empty())) {
    return;
  }

  const Entity* scout = nullptr;
  const auto ready_skirmisher = std::ranges::find_if(context.ready_army, [](const Entity* entity) {
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
  const auto goal = scouting_goal(context, scout->position);
  const auto destination = safe_advance(context, scout->position, goal, true);
  const auto target = destination.position;
  if ((scout->order.type == OrderType::Move || scout->order.type == OrderType::AttackMove) &&
      squared_distance(scout->order.target, target) <=
          static_cast<std::uint64_t>(80'000) * 80'000U) {
    return;
  }

  auto command = command_for(context.observation.player(), CommandType::AttackMove);
  command.entities = {scout->id};
  command.target = target;
  auto candidate = CandidateBuilder{AIAction::Scout, std::move(command)};
  candidate.add(AIUtilityReason::Baseline, 1'500)
      .add(AIUtilityReason::InformationNeed, 3'800)
      .target(scout->id)
      .position(target)
      .influence(context.tactical_map(), target);
  add_influence_utility(candidate, destination.sample, true);
  candidates.push_back(std::move(candidate).finish());
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
