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
inline constexpr std::int32_t kAttackMoveAcquisitionRange = 220'000;
inline constexpr std::int32_t kSeparationPadding = 2'000;
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

[[nodiscard]] std::vector<EntityId> sorted_unique_ids(std::vector<EntityId> ids) {
  std::ranges::sort(ids, {}, &EntityId::value);
  const auto duplicate = std::ranges::unique(ids, {}, &EntityId::value);
  ids.erase(duplicate.begin(), duplicate.end());
  return ids;
}

[[nodiscard]] CommandResult validate_units(const Simulation& simulation, const Command& command,
                                           const bool workers_only = false) noexcept {
  if (command.entities.empty()) {
    return failure(CommandError::InvalidEntity, workers_only ? "No workers were selected."
                                                              : "No units were selected.");
  }
  for (const auto id : command.entities) {
    const auto* entity = simulation.find_entity(id);
    if (entity == nullptr || entity->kind != EntityKind::Unit ||
        (workers_only && entity->type != EntityType::Worker)) {
      return failure(CommandError::InvalidEntity, workers_only ? "Only workers can perform that order."
                                                                : "A selected unit does not exist.");
    }
    if (entity->owner != command.player) {
      return failure(CommandError::InvalidOwner, "A selected unit belongs to another player.");
    }
  }
  return success();
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
    case CommandType::AttackMove:
      return apply_attack_move(command);
    case CommandType::Gather:
      return apply_gather(command);
    case CommandType::Train:
      return apply_train(command);
    case CommandType::Stop:
      return apply_stop(command);
    case CommandType::Hold:
      return apply_hold(command);
    case CommandType::Patrol:
      return apply_patrol(command);
    case CommandType::SetRallyPoint:
      return apply_set_rally_point(command);
  }
  return failure(CommandError::InvalidEntity, "Unsupported command.");
}

CommandResult Simulation::apply_move(const Command& command) {
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }

  const auto ids = sorted_unique_ids(command.entities);
  const auto targets = formation_targets(ids, command.target);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (auto* entity = find_entity_mutable(ids[index])) {
      Order order{};
      order.type = OrderType::Move;
      order.target = targets[index];
      set_order(*entity, std::move(order), command.queue);
    }
  }
  return success();
}

CommandResult Simulation::apply_attack(const Command& command) {
  const auto* target = find_entity(command.target_entity);
  if (target == nullptr || target->owner == command.player) {
    return failure(CommandError::InvalidTarget, "The attack target is invalid.");
  }
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }
  for (const auto id : sorted_unique_ids(command.entities)) {
    if (auto* entity = find_entity_mutable(id)) {
      Order order{};
      order.type = OrderType::Attack;
      order.target_entity = command.target_entity;
      set_order(*entity, std::move(order), command.queue);
    }
  }
  return success();
}

CommandResult Simulation::apply_attack_move(const Command& command) {
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }

  const auto ids = sorted_unique_ids(command.entities);
  const auto targets = formation_targets(ids, command.target);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (auto* entity = find_entity_mutable(ids[index])) {
      Order order{};
      order.type = OrderType::AttackMove;
      order.target = targets[index];
      set_order(*entity, std::move(order), command.queue);
    }
  }
  return success();
}

CommandResult Simulation::apply_gather(const Command& command) {
  if (find_resource(command.resource) == nullptr) {
    return failure(CommandError::InvalidTarget, "The resource node does not exist.");
  }
  if (const auto validation = validate_units(*this, command, true); !validation) {
    return validation;
  }
  for (const auto id : sorted_unique_ids(command.entities)) {
    if (auto* entity = find_entity_mutable(id)) {
      Order order{};
      order.type = OrderType::Gather;
      order.resource = command.resource;
      set_order(*entity, std::move(order), command.queue);
    }
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

CommandResult Simulation::apply_stop(const Command& command) {
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }
  for (const auto id : sorted_unique_ids(command.entities)) {
    if (auto* entity = find_entity_mutable(id)) {
      entity->order = {};
      entity->order_queue.clear();
    }
  }
  return success();
}

CommandResult Simulation::apply_hold(const Command& command) {
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }
  for (const auto id : sorted_unique_ids(command.entities)) {
    if (auto* entity = find_entity_mutable(id)) {
      Order order{};
      order.type = OrderType::Hold;
      set_order(*entity, std::move(order), command.queue);
    }
  }
  return success();
}

CommandResult Simulation::apply_patrol(const Command& command) {
  if (const auto validation = validate_units(*this, command); !validation) {
    return validation;
  }

  const auto ids = sorted_unique_ids(command.entities);
  const auto targets = formation_targets(ids, command.target);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (auto* entity = find_entity_mutable(ids[index])) {
      Order order{};
      order.type = OrderType::Patrol;
      order.target = targets[index];
      order.secondary_target = entity->position;
      set_order(*entity, std::move(order), command.queue);
    }
  }
  return success();
}

CommandResult Simulation::apply_set_rally_point(const Command& command) {
  auto* producer = find_entity_mutable(command.producer);
  if (producer == nullptr || producer->kind != EntityKind::Building || producer->owner != command.player) {
    return failure(CommandError::InvalidProducer, "The rally-point building is invalid.");
  }
  producer->rally_point = nearest_navigable(command.target, entity_definition(player(command.player).faction,
                                                                               EntityType::Worker).radius);
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
    Vec2 spawn_position;
    Vec2 rally_point;
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
      const auto definition = entity_definition(player(building.owner).faction, task.type);
      const auto direction = building.owner == PlayerId::One ? 1 : -1;
      const auto spawn_offset = building.radius + definition.radius + 8'000;
      const auto spawn_position = nearest_navigable(
          {building.position.x + direction * spawn_offset, building.position.y}, definition.radius);
      spawns.push_back({building.owner, task.type, spawn_position, building.rally_point});
      building.production_queue.erase(building.production_queue.begin());
    }
  }

  for (const auto& spawn : spawns) {
    const auto id = spawn_entity(spawn.owner, spawn.type, spawn.spawn_position);
    if (auto* entity = find_entity_mutable(id); entity != nullptr && entity->position != spawn.rally_point) {
      Order order{};
      order.type = OrderType::Move;
      order.target = nearest_navigable(spawn.rally_point, entity->radius);
      set_order(*entity, std::move(order), false);
    }
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
          complete_order(entity);
        } else {
          static_cast<void>(move_along_route(entity, entity.order.target));
        }
        break;
      case OrderType::Attack:
        if (!attack_target(entity, true)) {
          complete_order(entity);
        }
        break;
      case OrderType::AttackMove:
        update_attack_move(entity);
        break;
      case OrderType::Gather:
        update_gather(entity);
        break;
      case OrderType::Patrol:
        update_patrol(entity);
        break;
      case OrderType::Hold:
        update_hold(entity);
        break;
    }
  }

  resolve_unit_separation();
}

void Simulation::update_gather(Entity& entity) {
  auto* resource = find_resource_mutable(entity.order.resource);
  if (resource == nullptr || (resource->amount <= 0 && entity.carrying == 0)) {
    complete_order(entity);
    return;
  }

  switch (entity.order.gather_phase) {
    case GatherPhase::ToResource:
      if (within_reach(entity, resource->position, resource->radius)) {
        entity.order.gather_phase = GatherPhase::Harvest;
        entity.order.phase_ticks = kHarvestTicks;
        clear_route(entity.order);
      } else {
        static_cast<void>(move_along_route(entity, resource->position));
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
        complete_order(entity);
        return;
      }
      if (within_reach(entity, command->position, command->radius)) {
        const auto income_rate = faction_definition(player(entity.owner).faction).income_basis_points;
        mutable_player(entity.owner).ore += entity.carrying * income_rate / 10'000;
        entity.carrying = 0;
        entity.order.gather_phase = GatherPhase::ToResource;
        clear_route(entity.order);
      } else {
        static_cast<void>(move_along_route(entity, command->position));
      }
      break;
    }
  }
}

void Simulation::update_attack_move(Entity& entity) {
  if (entity.order.target_entity && attack_target(entity, true)) {
    return;
  }

  entity.order.target_entity = nearest_enemy(
      entity.owner, entity.position, std::max(kAttackMoveAcquisitionRange, entity.radius + entity.attack_range));
  if (entity.order.target_entity && attack_target(entity, true)) {
    return;
  }

  entity.order.target_entity = {};
  if (within_reach(entity, entity.order.target, 0)) {
    entity.position = entity.order.target;
    complete_order(entity);
    return;
  }
  static_cast<void>(move_along_route(entity, entity.order.target));
}

void Simulation::update_patrol(Entity& entity) {
  if (entity.order.target_entity && attack_target(entity, true)) {
    return;
  }

  entity.order.target_entity = nearest_enemy(
      entity.owner, entity.position, std::max(kAttackMoveAcquisitionRange, entity.radius + entity.attack_range));
  if (entity.order.target_entity && attack_target(entity, true)) {
    return;
  }

  entity.order.target_entity = {};
  if (within_reach(entity, entity.order.target, 0)) {
    entity.position = entity.order.target;
    std::swap(entity.order.target, entity.order.secondary_target);
    clear_route(entity.order);
    return;
  }
  static_cast<void>(move_along_route(entity, entity.order.target));
}

void Simulation::update_hold(Entity& entity) {
  if (entity.order.target_entity && attack_target(entity, false)) {
    return;
  }

  entity.order.target_entity = nearest_enemy(entity.owner, entity.position, entity.radius + entity.attack_range);
  if (entity.order.target_entity) {
    static_cast<void>(attack_target(entity, false));
  }
}

void Simulation::set_order(Entity& entity, Order order, const bool queue) {
  if (!queue) {
    entity.order_queue.clear();
    entity.order = std::move(order);
    return;
  }

  if (entity.order.type == OrderType::Idle) {
    entity.order = std::move(order);
  } else if (entity.order_queue.size() < 32) {
    entity.order_queue.push_back(std::move(order));
  }
}

void Simulation::complete_order(Entity& entity) {
  if (entity.order_queue.empty()) {
    entity.order = {};
    return;
  }

  entity.order = std::move(entity.order_queue.front());
  entity.order_queue.erase(entity.order_queue.begin());
}

void Simulation::clear_route(Order& order) const noexcept {
  order.route.clear();
  order.route_index = 0;
  order.route_goal = {};
}

bool Simulation::move_along_route(Entity& entity, Vec2 target) {
  target = nearest_navigable(target, entity.radius);
  auto& order = entity.order;
  const auto repath_distance = std::max(1, config_.navigation_cell_size / 2);
  const auto repath_distance_squared = static_cast<std::uint64_t>(repath_distance) * repath_distance;
  const bool target_moved = squared_distance(order.route_goal, target) > repath_distance_squared;
  if (order.route.empty() || order.route_index >= order.route.size() || target_moved) {
    order.route = find_path(entity.position, target, entity.radius);
    order.route_index = 0;
    order.route_goal = target;
  }

  while (order.route_index < order.route.size() &&
         within_reach(entity, order.route[order.route_index], 0)) {
    entity.position = order.route[order.route_index];
    ++order.route_index;
  }
  if (order.route_index >= order.route.size()) {
    return within_reach(entity, target, 0);
  }

  move_toward(entity, order.route[order.route_index]);
  return false;
}

bool Simulation::attack_target(Entity& entity, const bool chase) {
  auto* target = find_entity_mutable(entity.order.target_entity);
  if (target == nullptr || target->owner == entity.owner) {
    entity.order.target_entity = {};
    clear_route(entity.order);
    return false;
  }

  if (!within_reach(entity, target->position, target->radius, entity.attack_range)) {
    if (!chase) {
      entity.order.target_entity = {};
      clear_route(entity.order);
      return false;
    }
    static_cast<void>(move_along_route(entity, target->position));
    return true;
  }

  clear_route(entity.order);
  if (entity.cooldown_ticks == 0) {
    target->hit_points -= damage_against(entity, *target);
    entity.cooldown_ticks = entity.attack_cooldown_ticks;
  }
  return true;
}

EntityId Simulation::nearest_enemy(const PlayerId owner, const Vec2 position,
                                   const std::int32_t acquisition_range) const noexcept {
  EntityId result{};
  auto best_distance = std::numeric_limits<std::uint64_t>::max();
  for (const auto& candidate : entities_) {
    if (!candidate.alive() || candidate.owner == owner) {
      continue;
    }
    const auto reach = static_cast<std::int64_t>(acquisition_range) + candidate.radius;
    const auto distance = squared_distance(position, candidate.position);
    if (distance <= static_cast<std::uint64_t>(reach * reach) &&
        (distance < best_distance || (distance == best_distance && candidate.id.value < result.value))) {
      result = candidate.id;
      best_distance = distance;
    }
  }
  return result;
}

void Simulation::resolve_unit_separation() {
  for (int pass = 0; pass < 2; ++pass) {
    for (std::size_t left_index = 0; left_index < entities_.size(); ++left_index) {
      auto& left = entities_[left_index];
      if (!left.alive() || left.kind != EntityKind::Unit) {
        continue;
      }
      for (std::size_t right_index = left_index + 1; right_index < entities_.size(); ++right_index) {
        auto& right = entities_[right_index];
        if (!right.alive() || right.kind != EntityKind::Unit) {
          continue;
        }

        auto dx = static_cast<std::int64_t>(right.position.x) - left.position.x;
        auto dy = static_cast<std::int64_t>(right.position.y) - left.position.y;
        auto distance = integer_sqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
        const auto required = static_cast<std::uint64_t>(left.radius + right.radius + kSeparationPadding);
        if (distance >= required) {
          continue;
        }
        if (distance == 0) {
          dx = left.id.value < right.id.value ? 1 : -1;
          dy = 0;
          distance = 1;
        }

        const auto overlap = static_cast<std::int64_t>(required - distance);
        const auto left_push = overlap / 2;
        const auto right_push = overlap - left_push;
        Vec2 left_candidate{
            left.position.x - static_cast<std::int32_t>(dx * left_push / static_cast<std::int64_t>(distance)),
            left.position.y - static_cast<std::int32_t>(dy * left_push / static_cast<std::int64_t>(distance)),
        };
        Vec2 right_candidate{
            right.position.x + static_cast<std::int32_t>(dx * right_push / static_cast<std::int64_t>(distance)),
            right.position.y + static_cast<std::int32_t>(dy * right_push / static_cast<std::int64_t>(distance)),
        };
        left_candidate.x = std::clamp(left_candidate.x, left.radius, config_.map_size.x - left.radius);
        left_candidate.y = std::clamp(left_candidate.y, left.radius, config_.map_size.y - left.radius);
        right_candidate.x = std::clamp(right_candidate.x, right.radius, config_.map_size.x - right.radius);
        right_candidate.y = std::clamp(right_candidate.y, right.radius, config_.map_size.y - right.radius);
        if (is_navigable(left_candidate, left.radius)) {
          left.position = left_candidate;
        }
        if (is_navigable(right_candidate, right.radius)) {
          right.position = right_candidate;
        }
      }
    }
  }
}

std::vector<Vec2> Simulation::formation_targets(const std::vector<EntityId>& entities, const Vec2 target) const {
  if (entities.empty()) {
    return {};
  }

  std::int64_t center_x = 0;
  std::int64_t center_y = 0;
  std::int32_t max_radius = 0;
  for (const auto id : entities) {
    if (const auto* entity = find_entity(id)) {
      center_x += entity->position.x;
      center_y += entity->position.y;
      max_radius = std::max(max_radius, entity->radius);
    }
  }
  center_x /= static_cast<std::int64_t>(entities.size());
  center_y /= static_cast<std::int64_t>(entities.size());

  auto direction_x = static_cast<std::int64_t>(target.x) - center_x;
  auto direction_y = static_cast<std::int64_t>(target.y) - center_y;
  auto direction_length = integer_sqrt(
      static_cast<std::uint64_t>(direction_x * direction_x + direction_y * direction_y));
  if (direction_length == 0) {
    direction_x = 1;
    direction_y = 0;
    direction_length = 1;
  }

  const auto columns = static_cast<std::size_t>(integer_sqrt(entities.size() - 1) + 1);
  const auto rows = (entities.size() + columns - 1) / columns;
  const auto spacing = static_cast<std::int64_t>(std::max(max_radius * 2 + 8'000, 36'000));
  std::vector<Vec2> result;
  result.reserve(entities.size());
  for (std::size_t index = 0; index < entities.size(); ++index) {
    const auto column = index % columns;
    const auto row = index / columns;
    const auto right_offset =
        (static_cast<std::int64_t>(column * 2) - static_cast<std::int64_t>(columns - 1)) * spacing / 2;
    const auto forward_offset =
        (static_cast<std::int64_t>(rows - 1) - static_cast<std::int64_t>(row * 2)) * spacing / 2;
    Vec2 slot{
        target.x + static_cast<std::int32_t>((-direction_y * right_offset + direction_x * forward_offset) /
                                             static_cast<std::int64_t>(direction_length)),
        target.y + static_cast<std::int32_t>((direction_x * right_offset + direction_y * forward_offset) /
                                             static_cast<std::int64_t>(direction_length)),
    };
    const auto* entity = find_entity(entities[index]);
    result.push_back(nearest_navigable(slot, entity == nullptr ? max_radius : entity->radius));
  }
  return result;
}

std::vector<Vec2> Simulation::find_path(Vec2 start, Vec2 goal, const std::int32_t radius) const {
  start = nearest_navigable(start, radius);
  goal = nearest_navigable(goal, radius);
  if (segment_is_navigable(start, goal, radius)) {
    return {goal};
  }

  const auto cell_size = std::max(1, config_.navigation_cell_size);
  const auto width = std::max(1, (config_.map_size.x + cell_size - 1) / cell_size);
  const auto height = std::max(1, (config_.map_size.y + cell_size - 1) / cell_size);
  const auto node_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const auto index_of = [width](const std::int32_t x, const std::int32_t y) {
    return y * width + x;
  };
  const auto point_of = [this, cell_size, radius](const std::int32_t x, const std::int32_t y) {
    return Vec2{
        std::clamp(x * cell_size + cell_size / 2, radius, config_.map_size.x - radius),
        std::clamp(y * cell_size + cell_size / 2, radius, config_.map_size.y - radius),
    };
  };

  const auto nearest_cell = [&](const Vec2 point) {
    auto best_index = -1;
    auto best_distance = std::numeric_limits<std::uint64_t>::max();
    for (std::int32_t y = 0; y < height; ++y) {
      for (std::int32_t x = 0; x < width; ++x) {
        const auto candidate = point_of(x, y);
        if (!is_navigable(candidate, radius) || !segment_is_navigable(point, candidate, radius)) {
          continue;
        }
        const auto distance = squared_distance(point, candidate);
        const auto index = index_of(x, y);
        if (distance < best_distance || (distance == best_distance && index < best_index)) {
          best_index = index;
          best_distance = distance;
        }
      }
    }
    return best_index;
  };

  const auto start_index = nearest_cell(start);
  const auto goal_index = nearest_cell(goal);
  if (start_index < 0 || goal_index < 0) {
    return {};
  }

  constexpr auto unreachable = std::numeric_limits<std::int32_t>::max() / 4;
  std::vector<std::int32_t> cost(node_count, unreachable);
  std::vector<std::int32_t> parent(node_count, -1);
  std::vector<bool> open(node_count, false);
  std::vector<bool> closed(node_count, false);
  cost[static_cast<std::size_t>(start_index)] = 0;
  open[static_cast<std::size_t>(start_index)] = true;

  const auto goal_x = goal_index % width;
  const auto goal_y = goal_index / width;
  const auto heuristic = [goal_x, goal_y](const std::int32_t x, const std::int32_t y) {
    const auto dx = std::abs(goal_x - x);
    const auto dy = std::abs(goal_y - y);
    const auto diagonal = std::min(dx, dy);
    return diagonal * 14 + (std::max(dx, dy) - diagonal) * 10;
  };
  constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> directions{
      std::pair{1, 0}, std::pair{0, 1}, std::pair{-1, 0}, std::pair{0, -1},
      std::pair{1, 1}, std::pair{-1, 1}, std::pair{-1, -1}, std::pair{1, -1},
  };

  while (true) {
    auto current = -1;
    auto current_score = unreachable;
    auto current_heuristic = unreachable;
    for (std::size_t index = 0; index < node_count; ++index) {
      if (!open[index] || closed[index]) {
        continue;
      }
      const auto x = static_cast<std::int32_t>(index) % width;
      const auto y = static_cast<std::int32_t>(index) / width;
      const auto estimate = heuristic(x, y);
      const auto score = cost[index] + estimate;
      if (score < current_score ||
          (score == current_score && (estimate < current_heuristic ||
                                      (estimate == current_heuristic && static_cast<int>(index) < current)))) {
        current = static_cast<int>(index);
        current_score = score;
        current_heuristic = estimate;
      }
    }
    if (current < 0 || current == goal_index) {
      break;
    }

    open[static_cast<std::size_t>(current)] = false;
    closed[static_cast<std::size_t>(current)] = true;
    const auto current_x = current % width;
    const auto current_y = current / width;
    for (const auto [step_x, step_y] : directions) {
      const auto next_x = current_x + step_x;
      const auto next_y = current_y + step_y;
      if (next_x < 0 || next_y < 0 || next_x >= width || next_y >= height) {
        continue;
      }
      const auto next = index_of(next_x, next_y);
      if (closed[static_cast<std::size_t>(next)] || !is_navigable(point_of(next_x, next_y), radius)) {
        continue;
      }
      if (step_x != 0 && step_y != 0 &&
          (!is_navigable(point_of(current_x + step_x, current_y), radius) ||
           !is_navigable(point_of(current_x, current_y + step_y), radius))) {
        continue;
      }

      const auto next_cost = cost[static_cast<std::size_t>(current)] + (step_x == 0 || step_y == 0 ? 10 : 14);
      if (next_cost < cost[static_cast<std::size_t>(next)]) {
        cost[static_cast<std::size_t>(next)] = next_cost;
        parent[static_cast<std::size_t>(next)] = current;
        open[static_cast<std::size_t>(next)] = true;
      }
    }
  }

  if (start_index != goal_index && parent[static_cast<std::size_t>(goal_index)] < 0) {
    return {};
  }

  std::vector<Vec2> raw_path;
  for (auto current = goal_index; current != start_index && current >= 0;
       current = parent[static_cast<std::size_t>(current)]) {
    raw_path.push_back(point_of(current % width, current / width));
  }
  std::ranges::reverse(raw_path);
  if (raw_path.empty() || raw_path.back() != goal) {
    raw_path.push_back(goal);
  }

  std::vector<Vec2> smoothed;
  auto anchor = start;
  std::size_t cursor = 0;
  while (cursor < raw_path.size()) {
    auto best = cursor;
    auto found = false;
    for (auto candidate = raw_path.size(); candidate-- > cursor;) {
      if (segment_is_navigable(anchor, raw_path[candidate], radius)) {
        best = candidate;
        found = true;
        break;
      }
    }
    if (!found) {
      return {};
    }
    smoothed.push_back(raw_path[best]);
    anchor = raw_path[best];
    cursor = best + 1;
  }
  return smoothed;
}

bool Simulation::is_navigable(const Vec2 position, const std::int32_t radius) const noexcept {
  if (position.x < radius || position.y < radius || position.x > config_.map_size.x - radius ||
      position.y > config_.map_size.y - radius) {
    return false;
  }

  for (const auto& obstacle : config_.navigation_obstacles) {
    if (position.x >= obstacle.minimum.x - radius && position.x <= obstacle.maximum.x + radius &&
        position.y >= obstacle.minimum.y - radius && position.y <= obstacle.maximum.y + radius) {
      return false;
    }
  }
  return true;
}

bool Simulation::segment_is_navigable(const Vec2 start, const Vec2 end, const std::int32_t radius) const noexcept {
  const auto dx = static_cast<std::int64_t>(end.x) - start.x;
  const auto dy = static_cast<std::int64_t>(end.y) - start.y;
  const auto distance = integer_sqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
  const auto sample_stride = static_cast<std::uint64_t>(std::max(1, config_.navigation_cell_size / 3));
  const auto samples = std::max<std::uint64_t>(1, (distance + sample_stride - 1) / sample_stride);
  for (std::uint64_t index = 0; index <= samples; ++index) {
    const Vec2 point{
        start.x + static_cast<std::int32_t>(dx * static_cast<std::int64_t>(index) /
                                            static_cast<std::int64_t>(samples)),
        start.y + static_cast<std::int32_t>(dy * static_cast<std::int64_t>(index) /
                                            static_cast<std::int64_t>(samples)),
    };
    if (!is_navigable(point, radius)) {
      return false;
    }
  }
  return true;
}

Vec2 Simulation::nearest_navigable(Vec2 position, const std::int32_t radius) const noexcept {
  position.x = std::clamp(position.x, radius, config_.map_size.x - radius);
  position.y = std::clamp(position.y, radius, config_.map_size.y - radius);
  if (is_navigable(position, radius)) {
    return position;
  }

  const auto cell_size = std::max(1, config_.navigation_cell_size);
  const auto width = std::max(1, (config_.map_size.x + cell_size - 1) / cell_size);
  const auto height = std::max(1, (config_.map_size.y + cell_size - 1) / cell_size);
  auto result = position;
  auto best_distance = std::numeric_limits<std::uint64_t>::max();
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      const Vec2 candidate{
          std::clamp(x * cell_size + cell_size / 2, radius, config_.map_size.x - radius),
          std::clamp(y * cell_size + cell_size / 2, radius, config_.map_size.y - radius),
      };
      if (!is_navigable(candidate, radius)) {
        continue;
      }
      const auto distance = squared_distance(position, candidate);
      if (distance < best_distance || (distance == best_distance && candidate < result)) {
        result = candidate;
        best_distance = distance;
      }
    }
  }
  return result;
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
  hash_integral(hash, config_.navigation_cell_size);
  hash_integral(hash, config_.navigation_obstacles.size());
  for (const auto& obstacle : config_.navigation_obstacles) {
    hash_vec(hash, obstacle.minimum);
    hash_vec(hash, obstacle.maximum);
  }
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
    hash_order(hash, entity.order);
    hash_integral(hash, entity.order_queue.size());
    for (const auto& order : entity.order_queue) {
      hash_order(hash, order);
    }
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
    hash_integral(hash, command.queue);
  }
  return hash;
}

}  // namespace ashen::core
