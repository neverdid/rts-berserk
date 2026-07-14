#pragma once

#include "ashen/core/Types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ashen::core {

class ASHENCORE_API Simulation final {
 public:
  explicit Simulation(const SimulationConfig& config = {});

  void reset(const SimulationConfig& config = {});
  void enqueue(Command command);
  [[nodiscard]] CommandResult execute_now(Command command);
  void step();
  void run(Tick ticks);

  [[nodiscard]] EntityId spawn_entity(PlayerId owner, EntityType type, Vec2 position);
  [[nodiscard]] ResourceId add_resource(Vec2 position, std::int32_t amount, std::int32_t radius = 24'000);

  [[nodiscard]] Tick tick() const noexcept { return tick_; }
  [[nodiscard]] MatchMode mode() const noexcept { return config_.mode; }
  [[nodiscard]] MatchStatus status() const noexcept { return status_; }
  [[nodiscard]] std::optional<PlayerId> winner() const noexcept { return winner_; }
  [[nodiscard]] const SimulationConfig& config() const noexcept { return config_; }
  [[nodiscard]] const std::array<PlayerState, 2>& players() const noexcept { return players_; }
  [[nodiscard]] const PlayerState& player(PlayerId id) const noexcept;
  [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }
  [[nodiscard]] const std::vector<ResourceNode>& resources() const noexcept { return resources_; }
  [[nodiscard]] const Entity* find_entity(EntityId id) const noexcept;
  [[nodiscard]] const ResourceNode* find_resource(ResourceId id) const noexcept;
  [[nodiscard]] std::uint64_t state_hash() const noexcept;

 private:
  [[nodiscard]] PlayerState& mutable_player(PlayerId id) noexcept;
  [[nodiscard]] Entity* find_entity_mutable(EntityId id) noexcept;
  [[nodiscard]] ResourceNode* find_resource_mutable(ResourceId id) noexcept;
  [[nodiscard]] CommandResult apply_command(const Command& command);
  [[nodiscard]] CommandResult apply_move(const Command& command);
  [[nodiscard]] CommandResult apply_attack(const Command& command);
  [[nodiscard]] CommandResult apply_attack_move(const Command& command);
  [[nodiscard]] CommandResult apply_gather(const Command& command);
  [[nodiscard]] CommandResult apply_train(const Command& command);
  [[nodiscard]] CommandResult apply_stop(const Command& command);
  [[nodiscard]] CommandResult apply_hold(const Command& command);
  [[nodiscard]] CommandResult apply_patrol(const Command& command);
  [[nodiscard]] CommandResult apply_set_rally_point(const Command& command);
  void apply_due_commands();
  void update_production();
  void update_orders();
  void update_gather(Entity& entity);
  void update_attack_move(Entity& entity);
  void update_patrol(Entity& entity);
  void update_hold(Entity& entity);
  void resolve_unit_separation();
  void remove_dead_entities();
  void update_match_status();
  void set_order(Entity& entity, Order order, bool queue);
  void complete_order(Entity& entity);
  void clear_route(Order& order) const noexcept;
  [[nodiscard]] bool move_along_route(Entity& entity, Vec2 target);
  void move_toward(Entity& entity, Vec2 target) const noexcept;
  [[nodiscard]] bool attack_target(Entity& entity, bool chase);
  [[nodiscard]] EntityId nearest_enemy(PlayerId owner, Vec2 position, std::int32_t acquisition_range) const noexcept;
  [[nodiscard]] std::vector<Vec2> formation_targets(const std::vector<EntityId>& entities, Vec2 target) const;
  [[nodiscard]] std::vector<Vec2> find_path(Vec2 start, Vec2 goal, std::int32_t radius) const;
  [[nodiscard]] bool is_navigable(Vec2 position, std::int32_t radius) const noexcept;
  [[nodiscard]] bool segment_is_navigable(Vec2 start, Vec2 end, std::int32_t radius) const noexcept;
  [[nodiscard]] Vec2 nearest_navigable(Vec2 position, std::int32_t radius) const noexcept;
  [[nodiscard]] const Entity* nearest_command(PlayerId owner, Vec2 position) const noexcept;
  [[nodiscard]] std::int32_t queued_supply(PlayerId owner) const noexcept;

  SimulationConfig config_{};
  Tick tick_{};
  MatchStatus status_{MatchStatus::Playing};
  std::optional<PlayerId> winner_{};
  std::array<PlayerState, 2> players_{};
  std::array<bool, 2> command_seen_{};
  std::vector<Entity> entities_{};
  std::vector<ResourceNode> resources_{};
  std::vector<Command> command_queue_{};
  std::uint32_t next_entity_id_{1};
  std::uint32_t next_resource_id_{1};
  std::uint64_t next_sequence_{1};
};

}  // namespace ashen::core
