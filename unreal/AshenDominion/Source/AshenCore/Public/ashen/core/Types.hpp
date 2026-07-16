#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#ifndef ASHENCORE_API
#define ASHENCORE_API
#endif

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

struct ControlPointId {
  std::uint32_t value{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
  auto operator<=>(const ControlPointId&) const = default;
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
enum class FactionId : std::uint8_t { Compact, Ascendancy, Concord };
enum class MatchMode : std::uint8_t { Story, Skirmish, PvP };
enum class MatchStatus : std::uint8_t { Playing, Won, Lost };
enum class EntityKind : std::uint8_t { Unit, Building };
enum class EntityType : std::uint8_t { Worker, Vanguard, Skirmisher, Command, Barracks, Turret };
enum class ArmorClass : std::uint8_t { Laborer, Armored, Light, Structure };
enum class ResearchId : std::uint8_t {
  TierTwo,
  TemperedOaths,
  Wardcraft,
  ChorusOfKnives,
  PitBroods,
  VaultPlate,
  SiegeLiturgy,
};
enum class UnitStance : std::uint8_t { Aggressive, Defensive, Hold };
enum class OrderType : std::uint8_t { Idle, Move, Attack, AttackMove, Gather, Build, Patrol, Hold };
enum class GatherPhase : std::uint8_t { ToResource, Harvest, Return };
enum class CommandType : std::uint8_t {
  Move,
  Attack,
  AttackMove,
  Gather,
  Train,
  Stop,
  Hold,
  Patrol,
  SetRallyPoint,
  Build,
  Research,
  ActivatePower,
  Retreat,
  SetStance,
};
enum class CommandError : std::uint8_t {
  None,
  InvalidOwner,
  InvalidEntity,
  InvalidTarget,
  InvalidProducer,
  InvalidUnitType,
  InsufficientOre,
  SupplyBlocked,
  PlacementBlocked,
  UnderConstruction,
  QueueFull,
  PrerequisiteMissing,
  AlreadyResearched,
  ResearchBusy,
  PowerCooldown,
};

inline constexpr std::size_t kResearchCount = 7;

[[nodiscard]] constexpr std::size_t research_index(const ResearchId research) noexcept {
  return static_cast<std::size_t>(research);
}

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
  std::int32_t sight{};
  ArmorClass armor{ArmorClass::Laborer};
  ArmorClass bonus_against{ArmorClass::Laborer};
  bool has_damage_bonus{};
  std::int32_t bonus_damage{};
  std::int32_t terror{};
  std::int32_t ward{};
  std::int32_t supply_cost{};
  std::int32_t supply_provided{};
};

struct FactionDefinition {
  FactionId id{FactionId::Compact};
  std::string_view name{};
  std::int32_t income_basis_points{10'000};
  std::int32_t resolve_drift{};
};

struct ResearchDefinition {
  ResearchId id{ResearchId::TierTwo};
  std::optional<FactionId> faction{};
  std::string_view label{};
  std::int32_t cost{};
  Tick research_ticks{};
  EntityType producer{EntityType::Command};
  std::optional<ResearchId> prerequisite{};
};

struct PowerDefinition {
  FactionId faction{FactionId::Compact};
  std::string_view label{};
  std::int32_t cost{};
  Tick cooldown_ticks{};
};

struct Order {
  OrderType type{OrderType::Idle};
  Vec2 target{};
  Vec2 secondary_target{};
  EntityId target_entity{};
  ResourceId resource{};
  GatherPhase gather_phase{GatherPhase::ToResource};
  Tick phase_ticks{};
  Vec2 route_goal{};
  std::vector<Vec2> route{};
  std::size_t route_index{};
};

struct ProductionTask {
  EntityType type{EntityType::Worker};
  Tick remaining_ticks{};
  Tick total_ticks{};
};

struct ResearchTask {
  ResearchId id{ResearchId::TierTwo};
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
  std::int32_t sight{};
  std::int32_t terror{};
  std::int32_t ward{};
  std::int32_t resolve{100};
  std::int32_t supply_cost{};
  std::int32_t supply_provided{};
  std::int32_t carrying{};
  Order order{};
  std::vector<Order> order_queue{};
  Vec2 rally_point{};
  std::vector<ProductionTask> production_queue{};
  UnitStance stance{UnitStance::Aggressive};
  Vec2 guard_position{};
  bool under_construction{};
  Tick construction_ticks{};
  Tick construction_total_ticks{};

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
  FactionId faction{FactionId::Compact};
  std::int32_t ore{260};
  std::int32_t supply_used{};
  std::int32_t supply_cap{};
  std::int32_t resolve{100};
  Tick power_cooldown_ticks{};
  std::uint8_t tech_tier{1};
  std::array<bool, kResearchCount> researched{};
  std::vector<ResearchTask> research_queue{};
};

struct ControlPoint {
  ControlPointId id{};
  Vec2 position{};
  std::int32_t radius{};
  std::optional<PlayerId> owner{};
  std::int32_t influence{};
  std::int32_t income_progress{};
};

struct NavigationObstacle {
  Vec2 minimum{};
  Vec2 maximum{};
};

struct SimulationConfig {
  MatchMode mode{MatchMode::Skirmish};
  FactionId player_one_faction{FactionId::Compact};
  FactionId player_two_faction{FactionId::Ascendancy};
  Vec2 map_size{world(1'920, 1'080)};
  std::int32_t navigation_cell_size{world(36, 0).x};
  std::vector<NavigationObstacle> navigation_obstacles{
      {world(875, 0), world(1'045, 285)},
      {world(875, 395), world(1'045, 500)},
      {world(875, 580), world(1'045, 685)},
      {world(875, 795), world(1'045, 1'080)},
  };
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
  EntityType building_type{EntityType::Barracks};
  ResearchId research{ResearchId::TierTwo};
  UnitStance stance{UnitStance::Aggressive};
  bool queue{};
};

struct CommandResult {
  bool ok{};
  CommandError error{CommandError::None};
  std::string_view reason{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok; }
};

}  // namespace ashen::core
