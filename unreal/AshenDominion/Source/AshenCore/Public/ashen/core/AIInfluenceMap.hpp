#pragma once

#include "ashen/core/PlayerObservation.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ashen::core {

inline constexpr std::int32_t kAIInfluenceCellSize = world(96, 0).x;
inline constexpr std::int32_t kAIUnreachableTravelCost = 1'000'000;

struct AIInfluenceCell {
  std::int32_t friendly_power{};
  std::int32_t observed_enemy_power{};
  std::int32_t static_danger{};
  std::int32_t objective_value{};
  std::int32_t travel_cost{kAIUnreachableTravelCost};
  std::int32_t terror_pressure{};
  std::int32_t uncertainty{};
  bool navigable{};

  auto operator<=>(const AIInfluenceCell&) const = default;
};

struct AIInfluenceSample {
  std::int32_t column{};
  std::int32_t row{};
  Vec2 center{};
  AIInfluenceCell cell{};

  auto operator<=>(const AIInfluenceSample&) const = default;
};

// A deterministic tactical field built exclusively from one player's
// observation.
class ASHENCORE_API AIInfluenceMap final {
 public:
  explicit AIInfluenceMap(const PlayerObservation& observation,
                          std::int32_t cell_size = kAIInfluenceCellSize);

  [[nodiscard]] Vec2 map_size() const noexcept { return map_size_; }
  [[nodiscard]] std::int32_t cell_size() const noexcept { return cell_size_; }
  [[nodiscard]] std::int32_t columns() const noexcept { return columns_; }
  [[nodiscard]] std::int32_t rows() const noexcept { return rows_; }
  [[nodiscard]] const std::vector<AIInfluenceCell>& cells() const noexcept { return cells_; }
  [[nodiscard]] const AIInfluenceCell& cell(std::int32_t column, std::int32_t row) const noexcept;
  [[nodiscard]] const AIInfluenceCell& cell_at(Vec2 position) const noexcept;
  [[nodiscard]] AIInfluenceSample sample_at(Vec2 position) const noexcept;
  [[nodiscard]] Vec2 cell_center(std::int32_t column, std::int32_t row) const noexcept;
  [[nodiscard]] bool is_navigable(Vec2 position) const noexcept;
  [[nodiscard]] std::uint64_t hash() const noexcept { return hash_; }

 private:
  [[nodiscard]] std::size_t index(std::int32_t column, std::int32_t row) const noexcept;
  void add_radial(Vec2 center, std::int32_t radius_cells, std::int32_t amount,
                  std::int32_t AIInfluenceCell::* channel) noexcept;
  void build_travel_cost(const PlayerObservation& observation) noexcept;
  void finalize_hash() noexcept;

  Vec2 map_size_{};
  std::int32_t cell_size_{1};
  std::int32_t columns_{1};
  std::int32_t rows_{1};
  std::vector<AIInfluenceCell> cells_{};
  std::uint64_t hash_{};
};

}  // namespace ashen::core
