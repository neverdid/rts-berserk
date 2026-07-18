#include "ashen/core/VisibilityGrid.hpp"

#include <algorithm>
#include <cstdint>

namespace ashen::core {

void VisibilityGrid::reset(const Vec2 map_size, const std::int32_t cell_size) {
  map_size_ = {std::max(0, map_size.x), std::max(0, map_size.y)};
  cell_size_ = std::max(1, cell_size);
  columns_ = std::max(1, (map_size_.x + cell_size_ - 1) / cell_size_);
  rows_ = std::max(1, (map_size_.y + cell_size_ - 1) / cell_size_);
  cells_.assign(static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_), VisibilityState::Hidden);
}

void VisibilityGrid::begin_update() noexcept {
  for (auto& state : cells_) {
    if (state == VisibilityState::Visible) {
      state = VisibilityState::Explored;
    }
  }
}

void VisibilityGrid::reveal(const Vec2 center, const std::int32_t radius) noexcept {
  const auto reveal_radius = std::max(0, radius);
  const auto raw_minimum_x = static_cast<std::int64_t>(center.x) - reveal_radius;
  const auto raw_maximum_x = static_cast<std::int64_t>(center.x) + reveal_radius;
  const auto raw_minimum_y = static_cast<std::int64_t>(center.y) - reveal_radius;
  const auto raw_maximum_y = static_cast<std::int64_t>(center.y) + reveal_radius;
  if (raw_maximum_x < 0 || raw_maximum_y < 0 || raw_minimum_x > map_size_.x || raw_minimum_y > map_size_.y) {
    return;
  }
  const auto minimum_x = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_minimum_x, 0, map_size_.x));
  const auto maximum_x = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_maximum_x, 0, map_size_.x));
  const auto minimum_y = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_minimum_y, 0, map_size_.y));
  const auto maximum_y = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_maximum_y, 0, map_size_.y));
  const auto minimum_column = std::min(columns_ - 1, minimum_x / cell_size_);
  const auto maximum_column = std::min(columns_ - 1, maximum_x / cell_size_);
  const auto minimum_row = std::min(rows_ - 1, minimum_y / cell_size_);
  const auto maximum_row = std::min(rows_ - 1, maximum_y / cell_size_);

  for (auto row = minimum_row; row <= maximum_row; ++row) {
    for (auto column = minimum_column; column <= maximum_column; ++column) {
      if (cell_intersects_circle(column, row, center, reveal_radius)) {
        cells_[index(column, row)] = VisibilityState::Visible;
      }
    }
  }
}

VisibilityState VisibilityGrid::state_at(const Vec2 position) const noexcept {
  if (position.x < 0 || position.y < 0 || position.x > map_size_.x || position.y > map_size_.y) {
    return VisibilityState::Hidden;
  }
  const auto column = std::min(columns_ - 1, position.x / cell_size_);
  const auto row = std::min(rows_ - 1, position.y / cell_size_);
  return cells_[index(column, row)];
}

bool VisibilityGrid::overlaps_visible(const Vec2 center, const std::int32_t radius) const noexcept {
  const auto query_radius = std::max(0, radius);
  const auto raw_minimum_x = static_cast<std::int64_t>(center.x) - query_radius;
  const auto raw_maximum_x = static_cast<std::int64_t>(center.x) + query_radius;
  const auto raw_minimum_y = static_cast<std::int64_t>(center.y) - query_radius;
  const auto raw_maximum_y = static_cast<std::int64_t>(center.y) + query_radius;
  if (raw_maximum_x < 0 || raw_maximum_y < 0 || raw_minimum_x > map_size_.x || raw_minimum_y > map_size_.y) {
    return false;
  }
  const auto minimum_x = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_minimum_x, 0, map_size_.x));
  const auto maximum_x = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_maximum_x, 0, map_size_.x));
  const auto minimum_y = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_minimum_y, 0, map_size_.y));
  const auto maximum_y = static_cast<std::int32_t>(std::clamp<std::int64_t>(raw_maximum_y, 0, map_size_.y));
  const auto minimum_column = std::min(columns_ - 1, minimum_x / cell_size_);
  const auto maximum_column = std::min(columns_ - 1, maximum_x / cell_size_);
  const auto minimum_row = std::min(rows_ - 1, minimum_y / cell_size_);
  const auto maximum_row = std::min(rows_ - 1, maximum_y / cell_size_);

  for (auto row = minimum_row; row <= maximum_row; ++row) {
    for (auto column = minimum_column; column <= maximum_column; ++column) {
      if (cells_[index(column, row)] == VisibilityState::Visible &&
          cell_intersects_circle(column, row, center, query_radius)) {
        return true;
      }
    }
  }
  return false;
}

std::size_t VisibilityGrid::index(const std::int32_t column, const std::int32_t row) const noexcept {
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns_) + static_cast<std::size_t>(column);
}

bool VisibilityGrid::cell_intersects_circle(const std::int32_t column, const std::int32_t row, const Vec2 center,
                                            const std::int32_t radius) const noexcept {
  const auto minimum_x = column * cell_size_;
  const auto maximum_x = std::min(map_size_.x, minimum_x + cell_size_);
  const auto minimum_y = row * cell_size_;
  const auto maximum_y = std::min(map_size_.y, minimum_y + cell_size_);
  const auto closest_x = std::clamp(center.x, minimum_x, maximum_x);
  const auto closest_y = std::clamp(center.y, minimum_y, maximum_y);
  const auto delta_x = static_cast<std::int64_t>(center.x) - closest_x;
  const auto delta_y = static_cast<std::int64_t>(center.y) - closest_y;
  const auto squared_radius = static_cast<std::int64_t>(radius) * radius;
  return delta_x * delta_x + delta_y * delta_y <= squared_radius;
}

}  // namespace ashen::core
