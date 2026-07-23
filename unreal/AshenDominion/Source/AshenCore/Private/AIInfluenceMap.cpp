#include "ashen/core/AIInfluenceMap.hpp"

#include "ashen/core/Catalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace ashen::core {
namespace {

inline constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
inline constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;
inline constexpr std::int32_t kNavigationClearance = world(16, 0).x;
inline constexpr std::int32_t kMaximumInfluence = 2'000'000;

template <typename Value>
void hash_integral(std::uint64_t& hash, const Value value) noexcept {
  auto bits = static_cast<std::uint64_t>(value);
  for (std::size_t byte = 0; byte < sizeof(Value); ++byte) {
    hash ^= bits & 0xffU;
    hash *= kFnvPrime;
    bits >>= 8U;
  }
}

void saturated_add(std::int32_t& destination, const std::int32_t amount) noexcept {
  const auto total = static_cast<std::int64_t>(destination) + amount;
  destination = static_cast<std::int32_t>(
      std::clamp<std::int64_t>(total, -kMaximumInfluence, kMaximumInfluence));
}

[[nodiscard]] bool point_is_navigable(const PlayerObservation& observation,
                                      const Vec2 point) noexcept {
  if (point.x < kNavigationClearance || point.y < kNavigationClearance ||
      point.x > observation.map_size().x - kNavigationClearance ||
      point.y > observation.map_size().y - kNavigationClearance) {
    return false;
  }
  for (const auto& obstacle : observation.navigation_obstacles()) {
    if (point.x >= obstacle.minimum.x - kNavigationClearance &&
        point.x <= obstacle.maximum.x + kNavigationClearance &&
        point.y >= obstacle.minimum.y - kNavigationClearance &&
        point.y <= obstacle.maximum.y + kNavigationClearance) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::int32_t health_basis(const std::int32_t hit_points,
                                        const std::int32_t maximum) noexcept {
  return maximum <= 0 ? 0 : std::clamp(hit_points * 10'000 / maximum, 0, 10'000);
}

[[nodiscard]] std::int32_t observed_power(const EntityDefinition& definition,
                                          const std::int32_t hit_points, const std::int32_t maximum,
                                          const std::int32_t resolve) noexcept {
  return std::max(1, definition.cost * health_basis(hit_points, maximum) / 10'000 +
                         definition.damage * 4 + resolve / 5);
}

[[nodiscard]] std::int32_t decay(const std::int32_t value, const Tick age,
                                 const Tick lifetime) noexcept {
  if (age >= lifetime) {
    return 0;
  }
  return static_cast<std::int32_t>(static_cast<std::int64_t>(value) *
                                   static_cast<std::int64_t>(lifetime - age) /
                                   static_cast<std::int64_t>(lifetime));
}

}  // namespace

AIInfluenceMap::AIInfluenceMap(const PlayerObservation& observation, const std::int32_t cell_size)
    : map_size_(observation.map_size()),
      cell_size_(std::max(1, cell_size)),
      columns_(std::max(1, (map_size_.x + cell_size_ - 1) / cell_size_)),
      rows_(std::max(1, (map_size_.y + cell_size_ - 1) / cell_size_)),
      cells_(static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_)) {
  for (std::int32_t row = 0; row < rows_; ++row) {
    for (std::int32_t column = 0; column < columns_; ++column) {
      auto& value = cells_[index(column, row)];
      const auto center = cell_center(column, row);
      value.navigable = point_is_navigable(observation, center);
      switch (observation.explored_map().state_at(center)) {
        case VisibilityState::Visible:
          value.uncertainty = 0;
          break;
        case VisibilityState::Explored:
          value.uncertainty = 350;
          break;
        case VisibilityState::Hidden:
          value.uncertainty = 1'000;
          break;
      }
    }
  }

  for (const auto& entity : observation.owned_entities()) {
    if (!entity.alive() || entity.under_construction) {
      continue;
    }
    const auto definition = entity_definition(observation.self().faction, entity.type);
    const auto power =
        observed_power(definition, entity.hit_points, entity.max_hit_points, entity.resolve);
    add_radial(entity.position, entity.kind == EntityKind::Building ? 2 : 3, power,
               &AIInfluenceCell::friendly_power);
    if (entity.terror > 0) {
      add_radial(entity.position, entity.kind == EntityKind::Building ? 3 : 2,
                 entity.terror * 10, &AIInfluenceCell::friendly_terror);
    }
    if (entity.ward > 0) {
      add_radial(entity.position, entity.kind == EntityKind::Building ? 3 : 2,
                 entity.ward * 8, &AIInfluenceCell::friendly_ward);
      add_radial(entity.position, entity.kind == EntityKind::Building ? 3 : 2, -entity.ward * 8,
                 &AIInfluenceCell::terror_pressure);
    }
  }

  for (const auto& enemy : observation.known_enemies()) {
    if (enemy.hit_points <= 0) {
      continue;
    }
    const auto definition = entity_definition(observation.opponent_faction(), enemy.type);
    const auto base_power =
        observed_power(definition, enemy.hit_points, enemy.max_hit_points, enemy.resolve);
    const auto age = observation.tick() >= enemy.last_observed_tick
                         ? observation.tick() - enemy.last_observed_tick
                         : Tick{};
    if (enemy.currently_visible) {
      add_radial(enemy.position, enemy.kind == EntityKind::Building ? 2 : 3, base_power,
                 &AIInfluenceCell::observed_enemy_power);
      add_radial(enemy.position, enemy.kind == EntityKind::Building ? 3 : 2, definition.terror * 10,
                 &AIInfluenceCell::terror_pressure);
      if (enemy.kind == EntityKind::Unit) {
        add_radial(enemy.position, 2, std::max(0, 100 - enemy.resolve) * 10,
                   &AIInfluenceCell::resolve_vulnerability);
      }
      if (enemy.type == EntityType::Turret) {
        const auto radius = std::max(2, definition.attack_range / cell_size_ + 1);
        add_radial(enemy.position, radius, base_power * 2, &AIInfluenceCell::static_danger);
      }
      continue;
    }

    if (enemy.kind == EntityKind::Unit) {
      const auto remembered_power = decay(base_power, age, kMobileObservationMemoryTicks);
      if (remembered_power <= 0) {
        continue;
      }
      const auto spread =
          2 + static_cast<std::int32_t>(std::min<Tick>(5, age * 5 / kMobileObservationMemoryTicks));
      add_radial(enemy.position, spread, remembered_power, &AIInfluenceCell::observed_enemy_power);
      add_radial(enemy.position, spread,
                 decay(definition.terror * 8, age, kMobileObservationMemoryTicks),
                 &AIInfluenceCell::terror_pressure);
      add_radial(enemy.position, spread,
                 decay(std::max(0, 100 - enemy.resolve) * 8, age,
                       kMobileObservationMemoryTicks),
                 &AIInfluenceCell::resolve_vulnerability);
      add_radial(enemy.position, spread, decay(300, age, kMobileObservationMemoryTicks),
                 &AIInfluenceCell::uncertainty);
      continue;
    }

    add_radial(enemy.position, 2, base_power, &AIInfluenceCell::observed_enemy_power);
    if (enemy.type == EntityType::Turret) {
      const auto radius = std::max(2, definition.attack_range / cell_size_ + 1);
      add_radial(enemy.position, radius, base_power * 2, &AIInfluenceCell::static_danger);
    }
    add_radial(enemy.position, 3, definition.terror * 10, &AIInfluenceCell::terror_pressure);
  }

  for (const auto& objective : observation.public_objectives()) {
    auto value = 650;
    if (objective.has_observed_state) {
      if (!objective.last_observed_owner.has_value()) {
        value = 800;
      } else if (*objective.last_observed_owner == observation.player()) {
        value = 100;
      } else {
        value = 1'100;
      }
    }
    add_radial(objective.position, 2, value, &AIInfluenceCell::objective_value);
  }

  for (auto& value : cells_) {
    value.terror_pressure = std::max(0, value.terror_pressure);
    if (!value.navigable) {
      value.travel_cost = kAIUnreachableTravelCost;
    }
  }
  build_travel_cost(observation);
  finalize_hash();
}

const AIInfluenceCell& AIInfluenceMap::cell(const std::int32_t column,
                                            const std::int32_t row) const noexcept {
  return cells_[index(std::clamp(column, 0, columns_ - 1), std::clamp(row, 0, rows_ - 1))];
}

const AIInfluenceCell& AIInfluenceMap::cell_at(Vec2 position) const noexcept {
  position.x = std::clamp(position.x, 0, std::max(0, map_size_.x - 1));
  position.y = std::clamp(position.y, 0, std::max(0, map_size_.y - 1));
  return cell(position.x / cell_size_, position.y / cell_size_);
}

AIInfluenceSample AIInfluenceMap::sample_at(Vec2 position) const noexcept {
  position.x = std::clamp(position.x, 0, std::max(0, map_size_.x - 1));
  position.y = std::clamp(position.y, 0, std::max(0, map_size_.y - 1));
  const auto column = std::clamp(position.x / cell_size_, 0, columns_ - 1);
  const auto row = std::clamp(position.y / cell_size_, 0, rows_ - 1);
  return {column, row, cell_center(column, row), cell(column, row)};
}

Vec2 AIInfluenceMap::cell_center(const std::int32_t column, const std::int32_t row) const noexcept {
  return {std::min(map_size_.x - 1, column * cell_size_ + cell_size_ / 2),
          std::min(map_size_.y - 1, row * cell_size_ + cell_size_ / 2)};
}

bool AIInfluenceMap::is_navigable(const Vec2 position) const noexcept {
  return cell_at(position).navigable;
}

std::size_t AIInfluenceMap::index(const std::int32_t column,
                                  const std::int32_t row) const noexcept {
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns_) +
         static_cast<std::size_t>(column);
}

void AIInfluenceMap::add_radial(const Vec2 center, const std::int32_t radius_cells,
                                const std::int32_t amount,
                                std::int32_t AIInfluenceCell::* channel) noexcept {
  if (amount == 0) {
    return;
  }
  const auto center_column = std::clamp(center.x / cell_size_, 0, columns_ - 1);
  const auto center_row = std::clamp(center.y / cell_size_, 0, rows_ - 1);
  const auto radius = std::max(0, radius_cells);
  for (auto row = std::max(0, center_row - radius); row <= std::min(rows_ - 1, center_row + radius);
       ++row) {
    for (auto column = std::max(0, center_column - radius);
         column <= std::min(columns_ - 1, center_column + radius); ++column) {
      const auto distance = std::max(std::abs(column - center_column), std::abs(row - center_row));
      const auto weighted = static_cast<std::int32_t>(static_cast<std::int64_t>(amount) *
                                                      (radius + 1 - distance) / (radius + 1));
      saturated_add(cells_[index(column, row)].*channel, weighted);
    }
  }
}

void AIInfluenceMap::build_travel_cost(const PlayerObservation& observation) noexcept {
  const Entity* origin_entity = nullptr;
  for (const auto& entity : observation.owned_entities()) {
    if (!entity.alive() || entity.under_construction) {
      continue;
    }
    const auto is_command = entity.type == EntityType::Command;
    const auto origin_is_command =
        origin_entity != nullptr && origin_entity->type == EntityType::Command;
    if (origin_entity == nullptr || (is_command && !origin_is_command) ||
        (is_command == origin_is_command && entity.id.value < origin_entity->id.value)) {
      origin_entity = &entity;
    }
  }
  if (origin_entity == nullptr) {
    return;
  }

  const auto source = sample_at(origin_entity->position);
  if (!source.cell.navigable) {
    return;
  }
  const auto source_index = index(source.column, source.row);
  std::vector<bool> open(cells_.size(), false);
  std::vector<bool> closed(cells_.size(), false);
  cells_[source_index].travel_cost = 0;
  open[source_index] = true;
  constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> directions{
      std::pair{1, 0}, std::pair{0, 1},  std::pair{-1, 0},  std::pair{0, -1},
      std::pair{1, 1}, std::pair{-1, 1}, std::pair{-1, -1}, std::pair{1, -1},
  };

  while (true) {
    auto current = cells_.size();
    auto current_cost = kAIUnreachableTravelCost;
    for (std::size_t candidate = 0; candidate < cells_.size(); ++candidate) {
      if (!open[candidate] || closed[candidate]) {
        continue;
      }
      if (cells_[candidate].travel_cost < current_cost ||
          (cells_[candidate].travel_cost == current_cost && candidate < current)) {
        current = candidate;
        current_cost = cells_[candidate].travel_cost;
      }
    }
    if (current == cells_.size()) {
      break;
    }
    open[current] = false;
    closed[current] = true;
    const auto current_column =
        static_cast<std::int32_t>(current % static_cast<std::size_t>(columns_));
    const auto current_row =
        static_cast<std::int32_t>(current / static_cast<std::size_t>(columns_));
    for (const auto [column_step, row_step] : directions) {
      const auto column = current_column + column_step;
      const auto row = current_row + row_step;
      if (column < 0 || row < 0 || column >= columns_ || row >= rows_) {
        continue;
      }
      const auto next = index(column, row);
      if (closed[next] || !cells_[next].navigable) {
        continue;
      }
      if (column_step != 0 && row_step != 0 &&
          (!cell(current_column + column_step, current_row).navigable ||
           !cell(current_column, current_row + row_step).navigable)) {
        continue;
      }
      const auto movement = column_step == 0 || row_step == 0 ? 10 : 14;
      const auto next_cost = current_cost + movement;
      if (next_cost < cells_[next].travel_cost) {
        cells_[next].travel_cost = next_cost;
        open[next] = true;
      }
    }
  }
}

void AIInfluenceMap::finalize_hash() noexcept {
  auto value = kFnvOffset;
  hash_integral(value, map_size_.x);
  hash_integral(value, map_size_.y);
  hash_integral(value, cell_size_);
  hash_integral(value, columns_);
  hash_integral(value, rows_);
  for (const auto& cell_value : cells_) {
    hash_integral(value, cell_value.friendly_power);
    hash_integral(value, cell_value.observed_enemy_power);
    hash_integral(value, cell_value.static_danger);
    hash_integral(value, cell_value.objective_value);
    hash_integral(value, cell_value.travel_cost);
    hash_integral(value, cell_value.terror_pressure);
    hash_integral(value, cell_value.friendly_terror);
    hash_integral(value, cell_value.friendly_ward);
    hash_integral(value, cell_value.resolve_vulnerability);
    hash_integral(value, cell_value.uncertainty);
    hash_integral(value, cell_value.navigable);
  }
  hash_ = value;
}

}  // namespace ashen::core
