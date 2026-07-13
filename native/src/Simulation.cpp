#include "ashen/core/Simulation.hpp"

#include "ashen/core/Catalog.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

namespace ashen::core {
namespace {

inline constexpr Tick kHarvestTicks = 22;
inline constexpr std::int32_t kOrePerTrip = 10;
inline constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
inline constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;

[[nodiscard]] CommandResult success() noexcept {
  return {true, CommandError::None, {}};
}

[[nodiscard]] CommandResult failure(const CommandError error, const std::string_view reason) noexcept {
  return {false, error, reason};
}

[[nodiscard]] std::uint64_t squared_distance(const Vec2 a, const Vec2 b) noexcept {
  const auto dx = static_cast<std::int64_t>(b.x) - static_cast<std::int64_t>(a.x);
  const auto dy = static_cast<std::int64_t>(b.y) - static_cast<std::int64_t>(a.y);
  return static_cast<std::uint64_t>(dx * dx + dy * dy);
}

[[nodiscard]] std::uint64_t integer_sqrt(const std::uint64_t value) noexcept {
  if (value == 0) {
    return 0;
  }

  auto estimate = value;
  auto next = (estimate + 1) / 2;
  while (next < estimate) {
    estimate = next;
    next = (estimate + value / estimate) / 2;
  }
  return estimate;
}

[[nodiscard]] bool within_reach(const Entity& entity, const Vec2 target, const std::int32_t target_radius,
                                const std::int32_t extra_range = 0) noexcept {
  const auto reach = static_cast<std::int64_t>(entity.radius) + target_radius + extra_range;
  return squared_distance(entity.position, target) <= static_cast<std::uint64_t>(reach * reach);
}

[[nodiscard]] std::int32_t damage_against(const Entity& attacker, const Entity& target) noexcept {
  const auto bonus = attacker.has_damage_bonus && attacker.bonus_against == target.armor
                         ? attacker.bonus_damage
                         : 0;
  return std::max(1, attacker.damage + bonus);
}

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

}  // namespace

Simulation::Simulation(const SimulationConfig& config) {
  reset(config);
}

void Simulation::reset(const SimulationConfig& config) {
  config_ = config;
  tick_ = 0;
  status_ = MatchStatus::Playing;
  winner_.reset();
  players_ = {PlayerState{PlayerId::One, config.player_one_faction},
              PlayerState{PlayerId::Two, config.player_two_faction}};
  command_seen_.fill(false);
  entities_.clear();
  resources_.clear();
  command_queue_.clear();
  next_entity_id_ = 1;
  next_resource_id_ = 1;
  next_sequence_ = 1;

  if (!config.seed_starting_forces) {
    return;
  }

  const auto middle_y = config.map_size.y / 2;
  const auto left_x = world(250, 0).x;
  const auto right_x = config.map_size.x - left_x;

  for (const auto& [player_id, base_x, direction] :
       std::array{std::tuple{PlayerId::One, left_x, 1}, std::tuple{PlayerId::Two, right_x, -1}}) {
    static_cast<void>(spawn_entity(player_id, EntityType::Command, {base_x, middle_y}));
    static_cast<void>(spawn_entity(player_id, EntityType::Barracks,
                                   {base_x + direction * world(95, 0).x, middle_y - world(90, 0).x}));
    for (std::int32_t index = 0; index < 3; ++index) {
      static_cast<void>(spawn_entity(player_id, EntityType::Worker,
                                     {base_x + direction * world(70 + index * 20, 0).x,
                                      middle_y + world(-30 + index * 30, 0).x}));
    }
    static_cast<void>(spawn_entity(player_id, EntityType::Vanguard,
                                   {base_x + direction * world(125, 0).x, middle_y + world(80, 0).x}));
  }

  static_cast<void>(add_resource({left_x + world(210, 0).x, middle_y - world(150, 0).x}, 1'200));
  static_cast<void>(add_resource({right_x - world(210, 0).x, middle_y + world(150, 0).x}, 1'200));
  static_cast<void>(add_resource({config.map_size.x / 2, middle_y}, 2'400, 32'000));
}

void Simulation::enqueue(Command command) {
  if (command.sequence == 0) {
    command.sequence = next_sequence_++;
  } else {
    next_sequence_ = std::max(next_sequence_, command.sequence + 1);
  }
  command_queue_.push_back(std::move(command));
  std::stable_sort(command_queue_.begin(), command_queue_.end(), [](const Command& left, const Command& right) {
    return std::tuple{left.execute_tick, left.sequence, player_index(left.player)} <
           std::tuple{right.execute_tick, right.sequence, player_index(right.player)};
  });
}

CommandResult Simulation::execute_now(Command command) {
  command.execute_tick = tick_;
  if (command.sequence == 0) {
    command.sequence = next_sequence_++;
  }
  return apply_command(command);
}

void Simulation::step() {
  if (status_ != MatchStatus::Playing) {
    return;
  }

  apply_due_commands();

  for (auto& entity : entities_) {
    if (entity.cooldown_ticks > 0) {
      --entity.cooldown_ticks;
    }
  }

  update_production();
  update_orders();
  remove_dead_entities();
  update_match_status();
  ++tick_;
}

void Simulation::run(const Tick ticks) {
  for (Tick count = 0; count < ticks && status_ == MatchStatus::Playing; ++count) {
    step();
  }
}

EntityId Simulation::spawn_entity(const PlayerId owner, const EntityType type, const Vec2 position) {
  const auto definition = entity_definition(player(owner).faction, type);
  Entity entity{};
  entity.id = EntityId{next_entity_id_++};
  entity.owner = owner;
  entity.type = type;
  entity.kind = definition.kind;
  entity.position = position;
  entity.radius = definition.radius;
  entity.hit_points = definition.hit_points;
  entity.max_hit_points = definition.hit_points;
  entity.speed_per_tick = definition.speed_per_tick;
  entity.attack_range = definition.attack_range;
  entity.damage = definition.damage;
  entity.attack_cooldown_ticks = definition.attack_cooldown_ticks;
  entity.armor = definition.armor;
  entity.bonus_against = definition.bonus_against;
  entity.has_damage_bonus = definition.has_damage_bonus;
  entity.bonus_damage = definition.bonus_damage;
  entity.supply_cost = definition.supply_cost;
  entity.supply_provided = definition.supply_provided;
  entity.rally_point = {position.x + (owner == PlayerId::One ? world(55, 0).x : -world(55, 0).x), position.y};

  auto& owner_state = mutable_player(owner);
  owner_state.supply_used += definition.supply_cost;
  owner_state.supply_cap += definition.supply_provided;
  if (type == EntityType::Command) {
    command_seen_[player_index(owner)] = true;
  }

  entities_.push_back(std::move(entity));
  return entities_.back().id;
}

ResourceId Simulation::add_resource(const Vec2 position, const std::int32_t amount, const std::int32_t radius) {
  const auto id = ResourceId{next_resource_id_++};
  resources_.push_back(ResourceNode{id, position, radius, std::max(0, amount)});
  return id;
}

const PlayerState& Simulation::player(const PlayerId id) const noexcept {
  return players_[player_index(id)];
}

PlayerState& Simulation::mutable_player(const PlayerId id) noexcept {
  return players_[player_index(id)];
}

const Entity* Simulation::find_entity(const EntityId id) const noexcept {
  const auto found = std::find_if(entities_.begin(), entities_.end(),
                                  [id](const Entity& entity) { return entity.id == id && entity.alive(); });
  return found == entities_.end() ? nullptr : &*found;
}

Entity* Simulation::find_entity_mutable(const EntityId id) noexcept {
  const auto found = std::find_if(entities_.begin(), entities_.end(),
                                  [id](const Entity& entity) { return entity.id == id && entity.alive(); });
  return found == entities_.end() ? nullptr : &*found;
}

const ResourceNode* Simulation::find_resource(const ResourceId id) const noexcept {
  const auto found = std::find_if(resources_.begin(), resources_.end(),
                                  [id](const ResourceNode& node) { return node.id == id; });
  return found == resources_.end() ? nullptr : &*found;
}

ResourceNode* Simulation::find_resource_mutable(const ResourceId id) noexcept {
  const auto found = std::find_if(resources_.begin(), resources_.end(),
                                  [id](const ResourceNode& node) { return node.id == id; });
  return found == resources_.end() ? nullptr : &*found;
}

CommandResult Simulation::apply_command(const Command& command) {
  if (status_ != MatchStatus::Playing) {
    return failure(CommandError::InvalidTarget, "The match has already ended.");
  }

  switch (command.type) {
    case CommandType::Move:
      return apply_move(command);
    case CommandType::Attack:
      return apply_attack(command);
    case CommandType::Gather:
      return apply_gather(command);
    case CommandType::Train:
      return apply_train(command);
  }
  return failure(CommandError::InvalidEntity, "Unsupported command.");
}

CommandResult Simulation::apply_move(const Command& command) {
  if (command.entities.empty()) {
    return failure(CommandError::InvalidEntity, "No units were selected.");
  }
  for (const auto id : command.entities) {
    const auto* entity = find_entity(id);
    if (entity == nullptr || entity->kind != EntityKind::Unit) {
      return failure(CommandError::InvalidEntity, "A selected unit does not exist.");
    }
    if (entity->owner != command.player) {
      return failure(CommandError::InvalidOwner, "A selected unit belongs to another player.");
    }
  }
  for (const auto id : command.entities) {
    auto* entity = find_entity_mutable(id);
    entity->order = Order{OrderType::Move, command.target};
  }
  return success();
}

CommandResult Simulation::apply_attack(const Command& command) {
  const auto* target = find_entity(command.target_entity);
  if (target == nullptr || target->owner == command.player) {
    return failure(CommandError::InvalidTarget, "The attack target is invalid.");
  }
  if (command.entities.empty()) {
    return failure(CommandError::InvalidEntity, "No attackers were selected.");
  }
  for (const auto id : command.entities) {
    const auto* entity = find_entity(id);
    if (entity == nullptr || entity->kind != EntityKind::Unit) {
      return failure(CommandError::InvalidEntity, "An attacker does not exist.");
    }
    if (entity->owner != command.player) {
      return failure(CommandError::InvalidOwner, "An attacker belongs to another player.");
    }
  }
  for (const auto id : command.entities) {
    auto* entity = find_entity_mutable(id);
    entity->order = Order{OrderType::Attack, {}, command.target_entity};
  }
  return success();
}

CommandResult Simulation::apply_gather(const Command& command) {
  if (find_resource(command.resource) == nullptr) {
    return failure(CommandError::InvalidTarget, "The resource node does not exist.");
  }
  if (command.entities.empty()) {
    return failure(CommandError::InvalidEntity, "No workers were selected.");
  }
  for (const auto id : command.entities) {
    const auto* entity = find_entity(id);
    if (entity == nullptr || entity->type != EntityType::Worker) {
      return failure(CommandError::InvalidEntity, "Only workers can gather ore.");
    }
    if (entity->owner != command.player) {
      return failure(CommandError::InvalidOwner, "A worker belongs to another player.");
    }
  }
  for (const auto id : command.entities) {
    auto* entity = find_entity_mutable(id);
    entity->order = Order{OrderType::Gather, {}, {}, command.resource, GatherPhase::ToResource, 0};
  }
  return success();
}

CommandResult Simulation::apply_train(const Command& command) {
  auto* producer = find_entity_mutable(command.producer);
  if (producer == nullptr || producer->kind != EntityKind::Building || producer->owner != command.player) {
    return failure(CommandError::InvalidProducer, "The production building is invalid.");
  }
  if (!is_unit(command.train_type) || !can_train(producer->type, command.train_type)) {
    return failure(CommandError::InvalidUnitType, "That building cannot train the requested unit.");
  }

  const auto definition = entity_definition(player(command.player).faction, command.train_type);
  auto& owner = mutable_player(command.player);
  if (owner.ore < definition.cost) {
    return failure(CommandError::InsufficientOre, "Not enough ore.");
  }
  if (owner.supply_used + queued_supply(command.player) + definition.supply_cost > owner.supply_cap) {
    return failure(CommandError::SupplyBlocked, "Supply cap reached.");
  }

  owner.ore -= definition.cost;
  producer->production_queue.push_back({command.train_type, definition.build_ticks, definition.build_ticks});
  return success();
}

void Simulation::apply_due_commands() {
  while (!command_queue_.empty() && command_queue_.front().execute_tick <= tick_) {
    const auto command = std::move(command_queue_.front());
    command_queue_.erase(command_queue_.begin());
    static_cast<void>(apply_command(command));
  }
}

void Simulation::update_production() {
  struct SpawnRequest {
    PlayerId owner;
    EntityType type;
    Vec2 position;
  };
  std::vector<SpawnRequest> spawns;

  for (auto& building : entities_) {
    if (!building.alive() || building.production_queue.empty()) {
      continue;
    }
    auto& task = building.production_queue.front();
    if (task.remaining_ticks > 0) {
      --task.remaining_ticks;
    }
    if (task.remaining_ticks == 0) {
      spawns.push_back({building.owner, task.type, building.rally_point});
      building.production_queue.erase(building.production_queue.begin());
    }
  }

  for (const auto& spawn : spawns) {
    static_cast<void>(spawn_entity(spawn.owner, spawn.type, spawn.position));
  }
}

void Simulation::update_orders() {
  for (auto& entity : entities_) {
    if (!entity.alive() || entity.kind != EntityKind::Unit) {
      continue;
    }

    switch (entity.order.type) {
      case OrderType::Idle:
        break;
      case OrderType::Move:
        if (within_reach(entity, entity.order.target, 0)) {
          entity.position = entity.order.target;
          entity.order = {};
        } else {
          move_toward(entity, entity.order.target);
        }
        break;
      case OrderType::Attack: {
        auto* target = find_entity_mutable(entity.order.target_entity);
        if (target == nullptr || target->owner == entity.owner) {
          entity.order = {};
          break;
        }
        if (!within_reach(entity, target->position, target->radius, entity.attack_range)) {
          move_toward(entity, target->position);
          break;
        }
        if (entity.cooldown_ticks == 0) {
          target->hit_points -= damage_against(entity, *target);
          entity.cooldown_ticks = entity.attack_cooldown_ticks;
        }
        break;
      }
      case OrderType::Gather:
        update_gather(entity);
        break;
    }
  }
}

void Simulation::update_gather(Entity& entity) {
  auto* resource = find_resource_mutable(entity.order.resource);
  if (resource == nullptr || (resource->amount <= 0 && entity.carrying == 0)) {
    entity.order = {};
    return;
  }

  switch (entity.order.gather_phase) {
    case GatherPhase::ToResource:
      if (within_reach(entity, resource->position, resource->radius)) {
        entity.order.gather_phase = GatherPhase::Harvest;
        entity.order.phase_ticks = kHarvestTicks;
      } else {
        move_toward(entity, resource->position);
      }
      break;
    case GatherPhase::Harvest:
      if (entity.order.phase_ticks > 0) {
        --entity.order.phase_ticks;
      }
      if (entity.order.phase_ticks == 0) {
        const auto gathered = std::min(kOrePerTrip, resource->amount);
        resource->amount -= gathered;
        entity.carrying = gathered;
        entity.order.gather_phase = GatherPhase::Return;
      }
      break;
    case GatherPhase::Return: {
      const auto* command = nearest_command(entity.owner, entity.position);
      if (command == nullptr) {
        entity.order = {};
        return;
      }
      if (within_reach(entity, command->position, command->radius)) {
        const auto income_rate = faction_definition(player(entity.owner).faction).income_basis_points;
        mutable_player(entity.owner).ore += entity.carrying * income_rate / 10'000;
        entity.carrying = 0;
        entity.order.gather_phase = GatherPhase::ToResource;
      } else {
        move_toward(entity, command->position);
      }
      break;
    }
  }
}

void Simulation::remove_dead_entities() {
  for (const auto& entity : entities_) {
    if (entity.alive()) {
      continue;
    }
    auto& owner = mutable_player(entity.owner);
    owner.supply_used = std::max(0, owner.supply_used - entity.supply_cost);
    owner.supply_cap = std::max(0, owner.supply_cap - entity.supply_provided);
  }
  std::erase_if(entities_, [](const Entity& entity) { return !entity.alive(); });
}

void Simulation::update_match_status() {
  if (!command_seen_[0] || !command_seen_[1]) {
    return;
  }
  std::array<bool, 2> command_alive{};
  for (const auto& entity : entities_) {
    if (entity.type == EntityType::Command && entity.alive()) {
      command_alive[player_index(entity.owner)] = true;
    }
  }
  if (command_alive[0] == command_alive[1]) {
    return;
  }
  winner_ = command_alive[0] ? PlayerId::One : PlayerId::Two;
  status_ = MatchStatus::Won;
}

void Simulation::move_toward(Entity& entity, const Vec2 target) const noexcept {
  const auto dx = static_cast<std::int64_t>(target.x) - entity.position.x;
  const auto dy = static_cast<std::int64_t>(target.y) - entity.position.y;
  const auto distance = integer_sqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
  if (distance == 0 || distance <= static_cast<std::uint64_t>(entity.speed_per_tick)) {
    entity.position = target;
    return;
  }

  entity.position.x += static_cast<std::int32_t>(dx * entity.speed_per_tick / static_cast<std::int64_t>(distance));
  entity.position.y += static_cast<std::int32_t>(dy * entity.speed_per_tick / static_cast<std::int64_t>(distance));
  entity.position.x = std::clamp(entity.position.x, 0, config_.map_size.x);
  entity.position.y = std::clamp(entity.position.y, 0, config_.map_size.y);
}

const Entity* Simulation::nearest_command(const PlayerId owner, const Vec2 position) const noexcept {
  const Entity* nearest = nullptr;
  auto nearest_distance = std::numeric_limits<std::uint64_t>::max();
  for (auto& entity : entities_) {
    if (entity.owner != owner || entity.type != EntityType::Command || !entity.alive()) {
      continue;
    }
    const auto distance = squared_distance(position, entity.position);
    if (distance < nearest_distance) {
      nearest = &entity;
      nearest_distance = distance;
    }
  }
  return nearest;
}

std::int32_t Simulation::queued_supply(const PlayerId owner) const noexcept {
  std::int32_t total = 0;
  for (const auto& entity : entities_) {
    if (entity.owner != owner) {
      continue;
    }
    total = std::accumulate(entity.production_queue.begin(), entity.production_queue.end(), total,
                            [this, owner](const std::int32_t subtotal, const ProductionTask& task) {
                              return subtotal + entity_definition(player(owner).faction, task.type).supply_cost;
                            });
  }
  return total;
}

std::uint64_t Simulation::state_hash() const noexcept {
  auto hash = kFnvOffset;
  hash_integral(hash, tick_);
  hash_integral(hash, static_cast<std::uint8_t>(config_.mode));
  hash_integral(hash, static_cast<std::uint8_t>(config_.player_one_faction));
  hash_integral(hash, static_cast<std::uint8_t>(config_.player_two_faction));
  hash_vec(hash, config_.map_size);
  hash_integral(hash, static_cast<std::uint8_t>(status_));
  hash_integral(hash, winner_.has_value() ? static_cast<std::uint8_t>(*winner_) + 1U : 0U);
  hash_integral(hash, next_entity_id_);
  hash_integral(hash, next_resource_id_);
  hash_integral(hash, next_sequence_);
  for (const auto seen : command_seen_) {
    hash_integral(hash, seen);
  }

  for (const auto& player_state : players_) {
    hash_integral(hash, static_cast<std::uint8_t>(player_state.id));
    hash_integral(hash, static_cast<std::uint8_t>(player_state.faction));
    hash_integral(hash, player_state.ore);
    hash_integral(hash, player_state.supply_used);
    hash_integral(hash, player_state.supply_cap);
  }

  for (const auto& entity : entities_) {
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
    hash_integral(hash, entity.supply_cost);
    hash_integral(hash, entity.supply_provided);
    hash_integral(hash, entity.carrying);
    hash_integral(hash, static_cast<std::uint8_t>(entity.order.type));
    hash_vec(hash, entity.order.target);
    hash_integral(hash, entity.order.target_entity.value);
    hash_integral(hash, entity.order.resource.value);
    hash_integral(hash, static_cast<std::uint8_t>(entity.order.gather_phase));
    hash_integral(hash, entity.order.phase_ticks);
    hash_vec(hash, entity.rally_point);
    for (const auto& task : entity.production_queue) {
      hash_integral(hash, static_cast<std::uint8_t>(task.type));
      hash_integral(hash, task.remaining_ticks);
      hash_integral(hash, task.total_ticks);
    }
  }

  for (const auto& resource : resources_) {
    hash_integral(hash, resource.id.value);
    hash_vec(hash, resource.position);
    hash_integral(hash, resource.radius);
    hash_integral(hash, resource.amount);
  }

  for (const auto& command : command_queue_) {
    hash_integral(hash, command.execute_tick);
    hash_integral(hash, command.sequence);
    hash_integral(hash, static_cast<std::uint8_t>(command.player));
    hash_integral(hash, static_cast<std::uint8_t>(command.type));
    for (const auto entity : command.entities) {
      hash_integral(hash, entity.value);
    }
    hash_vec(hash, command.target);
    hash_integral(hash, command.target_entity.value);
    hash_integral(hash, command.resource.value);
    hash_integral(hash, command.producer.value);
    hash_integral(hash, static_cast<std::uint8_t>(command.train_type));
  }
  return hash;
}

}  // namespace ashen::core
