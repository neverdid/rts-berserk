#include "AIPlanningInternal.hpp"

#include "ashen/core/Catalog.hpp"

#include <algorithm>
#include <limits>
#include <ranges>
#include <tuple>
#include <utility>

namespace ashen::core::ai {
namespace {

[[nodiscard]] std::uint64_t strategy_variant(std::uint64_t seed, const PlayerId player) noexcept {
  seed ^= player == PlayerId::One ? 0x9e3779b97f4a7c15ULL : 0xd1b54a32d192ed03ULL;
  seed ^= seed >> 30U;
  seed *= 0xbf58476d1ce4e5b9ULL;
  seed ^= seed >> 27U;
  seed *= 0x94d049bb133111ebULL;
  return seed ^ (seed >> 31U);
}

[[nodiscard]] auto candidate_key(const ScoredCommand& candidate) noexcept {
  const auto& score = candidate.candidate;
  return std::tuple{static_cast<std::uint8_t>(score.action), score.target_entity.value,
                    score.target_objective.value, score.target_position.x, score.target_position.y,
                    score.entity_type.has_value() ? static_cast<std::uint16_t>(*score.entity_type) + 1U : 0U,
                    score.research.has_value() ? static_cast<std::uint16_t>(*score.research) + 1U : 0U};
}

[[nodiscard]] AIUtilityReason winning_reason(const AICandidateScore& candidate) noexcept {
  auto reason = AIUtilityReason::Baseline;
  auto best_score = std::numeric_limits<std::int32_t>::min();
  for (const auto& component : candidate.components) {
    if (component.score > best_score ||
        (component.score == best_score && component.reason < reason)) {
      reason = component.reason;
      best_score = component.score;
    }
  }
  return reason;
}

}  // namespace

CandidateBuilder::CandidateBuilder(const AIAction action, Command command) {
  value_.candidate.action = action;
  value_.command = std::move(command);
}

CandidateBuilder& CandidateBuilder::add(const AIUtilityReason reason, const std::int32_t score) {
  value_.candidate.components.push_back({reason, score});
  value_.candidate.total_score += score;
  return *this;
}

CandidateBuilder& CandidateBuilder::target(const EntityId entity) {
  value_.candidate.target_entity = entity;
  return *this;
}

CandidateBuilder& CandidateBuilder::objective(const ControlPointId objective_value) {
  value_.candidate.target_objective = objective_value;
  return *this;
}

CandidateBuilder& CandidateBuilder::position(const Vec2 position_value) {
  value_.candidate.target_position = position_value;
  return *this;
}

CandidateBuilder& CandidateBuilder::entity_type(const EntityType type) {
  value_.candidate.entity_type = type;
  return *this;
}

CandidateBuilder& CandidateBuilder::research(const ResearchId research_value) {
  value_.candidate.research = research_value;
  return *this;
}

CandidateBuilder& CandidateBuilder::influence(const AIInfluenceMap& map,
                                              const Vec2 position_value) {
  value_.candidate.influence_map_hash = map.hash();
  value_.candidate.influence_sample = map.sample_at(position_value);
  return *this;
}

ScoredCommand CandidateBuilder::finish() && { return std::move(value_); }

PlanningContext::PlanningContext(const PlayerObservation& observation_value,
                                 const bool build_tactical_map)
    : observation(observation_value),
      doctrine(ai_doctrine_profile(observation_value.self().faction,
                                   observation_value.match_seed(),
                                   observation_value.player())),
      strategy(strategy_variant(observation_value.match_seed(), observation_value.player())) {
  if (build_tactical_map) {
    tactical_map_.emplace(observation_value);
  }
  for (const auto& entity : observation.owned_entities()) {
    if (!entity.alive() || entity.under_construction) {
      continue;
    }
    if (entity.type == EntityType::Command && command_building == nullptr) {
      command_building = &entity;
    }
    if (entity.type == EntityType::Worker) {
      workers.push_back(&entity);
    }
    if (!is_army_unit(entity.type)) {
      continue;
    }
    army.push_back(&entity);
    const auto power = entity_power(entity, observation.self().faction);
    friendly_power += power;
    friendly_terror += entity.terror;
    friendly_ward += entity.ward;
    const auto health_basis = entity.max_hit_points <= 0
                                  ? 0
                                  : std::clamp(entity.hit_points * 10'000 / entity.max_hit_points,
                                               0, 10'000);
    if (health_basis > doctrine.critical_retreat_health_basis_points &&
        entity.resolve > doctrine.critical_retreat_resolve) {
      ready_army.push_back(&entity);
      ready_power += power;
    }
    if (entity.type == EntityType::Vanguard) {
      vanguards.push_back(&entity);
    } else {
      skirmishers.push_back(&entity);
    }
  }
  if (!army.empty()) {
    std::int64_t resolve_total = 0;
    for (const auto* unit : army) {
      resolve_total += unit->resolve;
    }
    average_army_resolve =
        static_cast<std::int32_t>(resolve_total / static_cast<std::int64_t>(army.size()));
  }

  std::int64_t enemy_resolve_total = 0;
  std::int32_t visible_enemy_units = 0;
  for (const auto& enemy : observation.known_enemies()) {
    if (!enemy.currently_visible || enemy.hit_points <= 0) {
      continue;
    }
    visible_enemies.push_back(&enemy);
    const auto definition = entity_definition(observation.opponent_faction(), enemy.type);
    visible_enemy_terror += definition.terror;
    visible_enemy_ward += definition.ward;
    if (is_army_unit(enemy.type)) {
      visible_enemy_power += enemy_power(enemy, observation.opponent_faction());
      enemy_resolve_total += enemy.resolve;
      ++visible_enemy_units;
    }
  }
  if (visible_enemy_units > 0) {
    visible_enemy_average_resolve =
        static_cast<std::int32_t>(enemy_resolve_total / visible_enemy_units);
  }
  attrition_commitment = observation.tick() >= kAttritionCommitmentTick &&
                         !army.empty() && ready_army.empty();
  const auto visible_combat_enemy =
      std::ranges::any_of(visible_enemies, [](const ObservedEnemy* enemy) {
        return is_army_unit(enemy->type);
      });
  search_commitment = attrition_commitment ||
                      (observation.tick() >= kLateSearchCommitmentTick &&
                       !army.empty() && !visible_combat_enemy);
}

const Entity* PlanningContext::owned(const EntityId id) const noexcept {
  const auto found = std::ranges::find(observation.owned_entities(), id, &Entity::id);
  return found == observation.owned_entities().end() ? nullptr : &*found;
}

bool is_army_unit(const EntityType type) noexcept {
  return type == EntityType::Vanguard || type == EntityType::Skirmisher;
}

std::uint64_t squared_distance(const Vec2 left, const Vec2 right) noexcept {
  const auto dx = static_cast<std::int64_t>(right.x) - left.x;
  const auto dy = static_cast<std::int64_t>(right.y) - left.y;
  return static_cast<std::uint64_t>(dx * dx + dy * dy);
}

std::uint64_t integer_sqrt(const std::uint64_t value) noexcept {
  auto remaining = value;
  std::uint64_t result = 0;
  std::uint64_t bit = std::uint64_t{1} << 62U;
  while (bit > remaining) {
    bit >>= 2U;
  }
  while (bit != 0) {
    if (remaining >= result + bit) {
      remaining -= result + bit;
      result = (result >> 1U) + bit;
    } else {
      result >>= 1U;
    }
    bit >>= 2U;
  }
  return result;
}

Vec2 centroid(const std::vector<const Entity*>& entities) noexcept {
  if (entities.empty()) {
    return {};
  }
  std::int64_t x = 0;
  std::int64_t y = 0;
  for (const auto* entity : entities) {
    x += entity->position.x;
    y += entity->position.y;
  }
  return {static_cast<std::int32_t>(x / static_cast<std::int64_t>(entities.size())),
          static_cast<std::int32_t>(y / static_cast<std::int64_t>(entities.size()))};
}

Vec2 enemy_centroid(const std::vector<const ObservedEnemy*>& enemies) noexcept {
  if (enemies.empty()) {
    return {};
  }
  std::int64_t x = 0;
  std::int64_t y = 0;
  for (const auto* enemy : enemies) {
    x += enemy->position.x;
    y += enemy->position.y;
  }
  return {static_cast<std::int32_t>(x / static_cast<std::int64_t>(enemies.size())),
          static_cast<std::int32_t>(y / static_cast<std::int64_t>(enemies.size()))};
}

Vec2 clamped_position(const PlayerObservation& observation, Vec2 position,
                      const std::int32_t margin) noexcept {
  const auto safe_margin = std::max(0, margin);
  position.x = std::clamp(position.x, safe_margin,
                          std::max(safe_margin, observation.map_size().x - safe_margin));
  position.y = std::clamp(position.y, safe_margin,
                          std::max(safe_margin, observation.map_size().y - safe_margin));
  return position;
}

Vec2 position_away_from(const PlayerObservation& observation, const Vec2 origin,
                        const Vec2 danger, const std::int32_t distance) noexcept {
  auto dx = static_cast<std::int64_t>(origin.x) - danger.x;
  auto dy = static_cast<std::int64_t>(origin.y) - danger.y;
  auto length = integer_sqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
  if (length == 0) {
    dx = observation.player() == PlayerId::One ? -1 : 1;
    dy = 0;
    length = 1;
  }
  return clamped_position(
      observation,
      {origin.x + static_cast<std::int32_t>(dx * distance / static_cast<std::int64_t>(length)),
       origin.y + static_cast<std::int32_t>(dy * distance / static_cast<std::int64_t>(length))});
}

std::int32_t entity_power(const Entity& entity, const FactionId faction) noexcept {
  const auto definition = entity_definition(faction, entity.type);
  const auto health_basis = entity.max_hit_points <= 0
                                ? 0
                                : std::clamp(entity.hit_points * 10'000 / entity.max_hit_points, 0, 10'000);
  return std::max(1, definition.cost * health_basis / 10'000 + definition.damage * 4 +
                         entity.resolve / 5);
}

std::int32_t enemy_power(const ObservedEnemy& enemy, const FactionId faction) noexcept {
  const auto definition = entity_definition(faction, enemy.type);
  const auto health_basis = enemy.max_hit_points <= 0
                                ? 0
                                : std::clamp(enemy.hit_points * 10'000 / enemy.max_hit_points, 0, 10'000);
  return std::max(1, definition.cost * health_basis / 10'000 + definition.damage * 4 +
                         enemy.resolve / 5);
}

Command command_for(const PlayerId player, const CommandType type) noexcept {
  Command command{};
  command.player = player;
  command.type = type;
  return command;
}

const CommandCapability* first_capability(const PlayerObservation& observation,
                                          const CommandType type,
                                          const std::optional<EntityType> entity_type,
                                          const std::optional<ResearchId> research) noexcept {
  const auto found = std::ranges::find_if(
      observation.capabilities(), [&](const CommandCapability& capability) {
        return capability.type == type && capability.entity_type == entity_type &&
               capability.research == research;
      });
  return found == observation.capabilities().end() ? nullptr : &*found;
}

const ObservedResource* nearest_usable_resource(const PlayerObservation& observation,
                                                const Vec2 origin) noexcept {
  const ObservedResource* nearest = nullptr;
  auto nearest_distance = std::numeric_limits<std::uint64_t>::max();
  for (const auto& resource : observation.known_resources()) {
    if (resource.last_observed_amount <= 0) {
      continue;
    }
    const auto distance = squared_distance(origin, resource.position);
    if (distance < nearest_distance ||
        (distance == nearest_distance && nearest != nullptr && resource.id.value < nearest->id.value)) {
      nearest = &resource;
      nearest_distance = distance;
    }
  }
  return nearest;
}

std::optional<ResearchId> faction_doctrine(const FactionId faction) noexcept {
  switch (faction) {
    case FactionId::Compact:
      return ResearchId::TemperedOaths;
    case FactionId::Ascendancy:
      return ResearchId::ChorusOfKnives;
    case FactionId::Concord:
      return ResearchId::VaultPlate;
  }
  return std::nullopt;
}

std::optional<AIPlannedDecision> select_decision(const AIDecisionLayer layer,
                                                 const Tick cadence,
                                                 const AIDoctrineProfile& doctrine,
                                                 std::vector<ScoredCommand> candidates) {
  if (candidates.empty()) {
    return std::nullopt;
  }
  std::ranges::sort(candidates, [](const ScoredCommand& left, const ScoredCommand& right) {
    return candidate_key(left) < candidate_key(right);
  });

  std::size_t selected = 0;
  for (std::size_t index = 1; index < candidates.size(); ++index) {
    if (candidates[index].candidate.total_score > candidates[selected].candidate.total_score) {
      selected = index;
    }
  }
  if (candidates[selected].candidate.total_score <= 0) {
    return std::nullopt;
  }

  AIPlannedDecision decision{};
  decision.layer = layer;
  decision.cadence_ticks = cadence;
  decision.doctrine_faction = doctrine.faction;
  decision.temperament = doctrine.temperament;
  decision.doctrine_hash = ai_doctrine_hash(doctrine);
  decision.selected_candidate = selected;
  decision.selected_action = candidates[selected].candidate.action;
  decision.winning_reason = winning_reason(candidates[selected].candidate);
  decision.command = std::move(candidates[selected].command);
  decision.candidates.reserve(candidates.size());
  for (auto& candidate : candidates) {
    decision.candidates.push_back(std::move(candidate.candidate));
  }
  return decision;
}

}  // namespace ashen::core::ai
