#include "ashen/core/PlayerObservation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace ashen::core {
namespace {

inline constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
inline constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;

template <typename Value>
void hash_integral(std::uint64_t& hash, const Value value) noexcept {
  auto bits = static_cast<std::uint64_t>(value);
  for (std::size_t index = 0; index < sizeof(Value); ++index) {
    hash ^= bits & 0xffU;
    hash *= kFnvPrime;
    bits >>= 8U;
  }
}

void hash_vec(std::uint64_t& hash, const Vec2 value) noexcept {
  hash_integral(hash, value.x);
  hash_integral(hash, value.y);
}

void hash_order(std::uint64_t& hash, const Order& order) noexcept {
  hash_integral(hash, static_cast<std::uint8_t>(order.type));
  hash_vec(hash, order.target);
  hash_vec(hash, order.secondary_target);
  hash_integral(hash, order.target_entity.value);
  hash_integral(hash, order.resource.value);
  hash_integral(hash, static_cast<std::uint8_t>(order.gather_phase));
  hash_integral(hash, order.phase_ticks);
  hash_vec(hash, order.route_goal);
  hash_integral(hash, order.route_index);
  hash_integral(hash, order.route.size());
  for (const auto waypoint : order.route) {
    hash_vec(hash, waypoint);
  }
}

void hash_owned_entity(std::uint64_t& hash, const Entity& entity) noexcept {
  hash_integral(hash, entity.id.value);
  hash_integral(hash, static_cast<std::uint8_t>(entity.owner));
  hash_integral(hash, static_cast<std::uint8_t>(entity.type));
  hash_integral(hash, static_cast<std::uint8_t>(entity.kind));
  hash_vec(hash, entity.position);
  hash_integral(hash, entity.radius);
  hash_integral(hash, entity.hit_points);
  hash_integral(hash, entity.max_hit_points);
  hash_integral(hash, entity.speed_per_tick);
  hash_integral(hash, entity.attack_range);
  hash_integral(hash, entity.damage);
  hash_integral(hash, entity.attack_cooldown_ticks);
  hash_integral(hash, entity.cooldown_ticks);
  hash_integral(hash, static_cast<std::uint8_t>(entity.armor));
  hash_integral(hash, static_cast<std::uint8_t>(entity.bonus_against));
  hash_integral(hash, entity.has_damage_bonus);
  hash_integral(hash, entity.bonus_damage);
  hash_integral(hash, entity.sight);
  hash_integral(hash, entity.terror);
  hash_integral(hash, entity.ward);
  hash_integral(hash, entity.resolve);
  hash_integral(hash, entity.supply_cost);
  hash_integral(hash, entity.supply_provided);
  hash_integral(hash, entity.carrying);
  hash_order(hash, entity.order);
  hash_integral(hash, entity.order_queue.size());
  for (const auto& queued : entity.order_queue) {
    hash_order(hash, queued);
  }
  hash_vec(hash, entity.rally_point);
  hash_integral(hash, entity.production_queue.size());
  for (const auto& task : entity.production_queue) {
    hash_integral(hash, static_cast<std::uint8_t>(task.type));
    hash_integral(hash, task.remaining_ticks);
    hash_integral(hash, task.total_ticks);
  }
  hash_integral(hash, static_cast<std::uint8_t>(entity.stance));
  hash_vec(hash, entity.guard_position);
  hash_integral(hash, entity.under_construction);
  hash_integral(hash, entity.construction_ticks);
  hash_integral(hash, entity.construction_total_ticks);
}

}  // namespace

PlayerObservation::PlayerObservation(const Tick tick, const PlayerId player, const FactionId opponent_faction,
                                     const MatchStatus status, PlayerState self, const std::int32_t ruin_tide,
                                     const Vec2 map_size, VisibilityGrid explored_map,
                                     std::vector<Entity> owned_entities,
                                     std::vector<ObservedEnemy> known_enemies,
                                     std::vector<ObservedResource> known_resources,
                                     std::vector<ObservedControlPoint> public_objectives,
                                     std::vector<CommandCapability> capabilities)
    : tick_(tick),
      revision_(tick),
      player_(player),
      opponent_faction_(opponent_faction),
      status_(status),
      self_(std::move(self)),
      ruin_tide_(ruin_tide),
      map_size_(map_size),
      explored_map_(std::move(explored_map)),
      owned_entities_(std::move(owned_entities)),
      known_enemies_(std::move(known_enemies)),
      known_resources_(std::move(known_resources)),
      public_objectives_(std::move(public_objectives)),
      capabilities_(std::move(capabilities)) {}

bool PlayerObservation::permits(const CommandType type, const EntityId actor,
                                const std::optional<EntityType> entity_type,
                                const std::optional<ResearchId> research) const noexcept {
  return std::ranges::any_of(capabilities_, [=](const CommandCapability& capability) {
    return capability.type == type && capability.actor == actor && capability.entity_type == entity_type &&
           capability.research == research;
  });
}

std::uint64_t PlayerObservation::hash() const noexcept {
  auto hash = kFnvOffset;
  hash_integral(hash, tick_);
  hash_integral(hash, revision_);
  hash_integral(hash, static_cast<std::uint8_t>(player_));
  hash_integral(hash, static_cast<std::uint8_t>(opponent_faction_));
  hash_integral(hash, static_cast<std::uint8_t>(status_));
  hash_integral(hash, static_cast<std::uint8_t>(self_.id));
  hash_integral(hash, static_cast<std::uint8_t>(self_.faction));
  hash_integral(hash, self_.ore);
  hash_integral(hash, self_.supply_used);
  hash_integral(hash, self_.supply_cap);
  hash_integral(hash, self_.resolve);
  hash_integral(hash, self_.power_cooldown_ticks);
  hash_integral(hash, self_.tech_tier);
  for (const auto researched : self_.researched) {
    hash_integral(hash, researched);
  }
  for (const auto& task : self_.research_queue) {
    hash_integral(hash, static_cast<std::uint8_t>(task.id));
    hash_integral(hash, task.remaining_ticks);
    hash_integral(hash, task.total_ticks);
  }
  hash_integral(hash, ruin_tide_);
  hash_vec(hash, map_size_);
  hash_integral(hash, explored_map_.cell_size());
  hash_integral(hash, explored_map_.columns());
  hash_integral(hash, explored_map_.rows());
  for (const auto state : explored_map_.cells()) {
    hash_integral(hash, static_cast<std::uint8_t>(state));
  }
  hash_integral(hash, owned_entities_.size());
  for (const auto& entity : owned_entities_) {
    hash_owned_entity(hash, entity);
  }
  hash_integral(hash, known_enemies_.size());
  for (const auto& enemy : known_enemies_) {
    hash_integral(hash, enemy.id.value);
    hash_integral(hash, static_cast<std::uint8_t>(enemy.owner));
    hash_integral(hash, static_cast<std::uint8_t>(enemy.type));
    hash_integral(hash, static_cast<std::uint8_t>(enemy.kind));
    hash_vec(hash, enemy.position);
    hash_integral(hash, enemy.radius);
    hash_integral(hash, enemy.hit_points);
    hash_integral(hash, enemy.max_hit_points);
    hash_integral(hash, enemy.resolve);
    hash_integral(hash, enemy.under_construction);
    hash_integral(hash, enemy.currently_visible);
    hash_integral(hash, enemy.last_observed_tick);
  }
  hash_integral(hash, known_resources_.size());
  for (const auto& resource : known_resources_) {
    hash_integral(hash, resource.id.value);
    hash_vec(hash, resource.position);
    hash_integral(hash, resource.radius);
    hash_integral(hash, resource.last_observed_amount);
    hash_integral(hash, static_cast<std::uint8_t>(resource.visibility));
    hash_integral(hash, resource.last_observed_tick);
  }
  hash_integral(hash, public_objectives_.size());
  for (const auto& objective : public_objectives_) {
    hash_integral(hash, objective.id.value);
    hash_vec(hash, objective.position);
    hash_integral(hash, objective.radius);
    hash_integral(hash, static_cast<std::uint8_t>(objective.visibility));
    hash_integral(hash, objective.has_observed_state);
    hash_integral(hash, objective.last_observed_owner.has_value()
                            ? static_cast<std::uint8_t>(*objective.last_observed_owner) + 1U
                            : 0U);
    hash_integral(hash, objective.last_observed_influence);
    hash_integral(hash, objective.last_observed_tick);
  }
  hash_integral(hash, capabilities_.size());
  for (const auto& capability : capabilities_) {
    hash_integral(hash, static_cast<std::uint8_t>(capability.type));
    hash_integral(hash, capability.actor.value);
    hash_integral(hash, capability.entity_type.has_value()
                            ? static_cast<std::uint8_t>(*capability.entity_type) + 1U
                            : 0U);
    hash_integral(hash, capability.research.has_value()
                            ? static_cast<std::uint8_t>(*capability.research) + 1U
                            : 0U);
  }
  return hash;
}

}  // namespace ashen::core
