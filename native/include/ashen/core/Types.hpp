#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ashen::core {

using Tick = std::uint64_t;

inline constexpr std::int32_t kWorldScale = 1'000;
inline constexpr std::int32_t kTicksPerSecond = 20;

struct EntityId {
  std::uint32_t value{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
  auto operator<=>(const EntityId&) const = default;
};

struct ResourceId {
  std::uint32_t value{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
  auto operator<=>(const ResourceId&) const = default;
};

struct Vec2 {
  std::int32_t x{};
  std::int32_t y{};

  auto operator<=>(const Vec2&) const = default;
};

[[nodiscard]] constexpr Vec2 world(const std::int32_t x, const std::int32_t y) noexcept {
  return {x * kWorldScale, y * kWorldScale};
}

enum class PlayerId : std::uint8_t { One, Two };
enum class FactionId : std::uint8_t { Candlebound, Hollow, Sepulcher };
enum class MatchMode : std::uint8_t { Story, Skirmish, PvP };
enum class MatchStatus : std::uint8_t { Playing, Won, Lost };
enum class EntityKind : std::uint8_t { Unit, Building };
enum class EntityType : std::uint8_t { Worker, Vanguard, Skirmisher, Command, Barracks, Turret };
enum class ArmorClass : std::uint8_t { Laborer, Armored, Light, Structure };
enum class OrderType : std::uint8_t { Idle, Move, Attack, Gather };
enum class GatherPhase : std::uint8_t { ToResource, Harvest, Return };
enum class CommandType : std::uint8_t { Move, Attack, Gather, Train };
enum class CommandError : std::uint8_t {
  None,
  InvalidOwner,
  InvalidEntity,
  InvalidTarget,
  InvalidProducer,
  InvalidUnitType,
  InsufficientOre,
  SupplyBlocked,
};

[[nodiscard]] constexpr std::size_t player_index(const PlayerId player) noexcept {
  return static_cast<std::size_t>(player);
}

[[nodiscard]] constexpr PlayerId enemy_of(const PlayerId player) noexcept {
  return player == PlayerId::One ? PlayerId::Two : PlayerId::One;
}

struct EntityDefinition {
  EntityType type{EntityType::Worker};
  EntityKind kind{EntityKind::Unit};
  std::string_view label{};
  std::int32_t cost{};
  Tick build_ticks{};
  std::int32_t hit_points{};
  std::int32_t radius{};
  std::int32_t speed_per_tick{};
  std::int32_t attack_range{};
  std::int32_t damage{};
  Tick attack_cooldown_ticks{};
  ArmorClass armor{ArmorClass::Laborer};
  ArmorClass bonus_against{ArmorClass::Laborer};
  bool has_damage_bonus{};
  std::int32_t bonus_damage{};
  std::int32_t supply_cost{};
  std::int32_t supply_provided{};
};

struct FactionDefinition {
  FactionId id{FactionId::Candlebound};
  std::string_view name{};
  std::int32_t income_basis_points{10'000};
};

struct Order {
  OrderType type{OrderType::Idle};
  Vec2 target{};
  EntityId target_entity{};
  ResourceId resource{};
  GatherPhase gather_phase{GatherPhase::ToResource};
  Tick phase_ticks{};
};

struct ProductionTask {
  EntityType type{EntityType::Worker};
  Tick remaining_ticks{};
  Tick total_ticks{};
};

struct Entity {
  EntityId id{};
  PlayerId owner{PlayerId::One};
  EntityType type{EntityType::Worker};
  EntityKind kind{EntityKind::Unit};
  Vec2 position{};
  std::int32_t radius{};
  std::int32_t hit_points{};
  std::int32_t max_hit_points{};
  std::int32_t speed_per_tick{};
  std::int32_t attack_range{};
  std::int32_t damage{};
  Tick attack_cooldown_ticks{};
  Tick cooldown_ticks{};
  ArmorClass armor{ArmorClass::Laborer};
  ArmorClass bonus_against{ArmorClass::Laborer};
  bool has_damage_bonus{};
  std::int32_t bonus_damage{};
  std::int32_t supply_cost{};
  std::int32_t supply_provided{};
  std::int32_t carrying{};
  Order order{};
  Vec2 rally_point{};
  std::vector<ProductionTask> production_queue{};

  [[nodiscard]] bool alive() const noexcept { return hit_points > 0; }
};

struct ResourceNode {
  ResourceId id{};
  Vec2 position{};
  std::int32_t radius{};
  std::int32_t amount{};
};

struct PlayerState {
  PlayerId id{PlayerId::One};
  FactionId faction{FactionId::Candlebound};
  std::int32_t ore{260};
  std::int32_t supply_used{};
  std::int32_t supply_cap{};
};

struct SimulationConfig {
  MatchMode mode{MatchMode::Skirmish};
  FactionId player_one_faction{FactionId::Candlebound};
  FactionId player_two_faction{FactionId::Hollow};
  Vec2 map_size{world(1'920, 1'080)};
  bool seed_starting_forces{true};
};

struct Command {
  Tick execute_tick{};
  std::uint64_t sequence{};
  PlayerId player{PlayerId::One};
  CommandType type{CommandType::Move};
  std::vector<EntityId> entities{};
  Vec2 target{};
  EntityId target_entity{};
  ResourceId resource{};
  EntityId producer{};
  EntityType train_type{EntityType::Worker};
};

struct CommandResult {
  bool ok{};
  CommandError error{CommandError::None};
  std::string_view reason{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok; }
};

}  // namespace ashen::core
