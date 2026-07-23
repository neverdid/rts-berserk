#pragma once

#include "ashen/core/Types.hpp"

#include <compare>
#include <cstdint>
#include <string_view>

namespace ashen::core {

enum class AITemperament : std::uint8_t { Steady, Audacious, Watchful };
enum class AIFormationDoctrine : std::uint8_t { ShieldLine, HuntingPack, WardWeb };
enum class AIScoutingDoctrine : std::uint8_t { MeasuredRecon, PredatoryProbe, FarSightScreen };

struct AIDoctrineProfile {
  FactionId faction{FactionId::Compact};
  AITemperament temperament{AITemperament::Steady};
  AIFormationDoctrine formation{AIFormationDoctrine::ShieldLine};
  AIScoutingDoctrine scouting{AIScoutingDoctrine::MeasuredRecon};

  std::int32_t economy_weight_basis_points{10'000};
  std::int32_t fortification_weight_basis_points{10'000};
  std::int32_t technology_weight_basis_points{10'000};
  std::int32_t vanguard_weight_basis_points{10'000};
  std::int32_t skirmisher_weight_basis_points{10'000};
  std::int32_t objective_weight_basis_points{10'000};
  std::int32_t scouting_weight_basis_points{10'000};
  std::int32_t aggression_weight_basis_points{10'000};
  std::int32_t preservation_weight_basis_points{10'000};
  std::int32_t cohesion_weight_basis_points{10'000};
  std::int32_t dread_exploitation_weight_basis_points{10'000};
  std::int32_t terror_resistance_weight_basis_points{10'000};
  std::int32_t ward_affinity_weight_basis_points{10'000};
  std::int32_t power_weight_basis_points{10'000};

  std::int32_t engagement_power_ratio_basis_points{8'000};
  std::int32_t tactical_retreat_health_basis_points{4'500};
  std::int32_t tactical_retreat_resolve{40};
  std::int32_t critical_retreat_health_basis_points{3'000};
  std::int32_t critical_retreat_resolve{25};
  std::int32_t formation_recovery_distance{world(360, 0).x};
  std::int32_t worker_target{5};
  Tick first_fortification_tick{800};
  Tick earliest_scout_tick{300};
  std::int32_t scout_army_reserve{2};
  std::int32_t minimum_assault_units{3};

  auto operator<=>(const AIDoctrineProfile&) const = default;
};

[[nodiscard]] ASHENCORE_API AIDoctrineProfile ai_doctrine_profile(
    FactionId faction, std::uint64_t match_seed, PlayerId player) noexcept;
[[nodiscard]] ASHENCORE_API std::int32_t apply_ai_weight(
    std::int32_t score, std::int32_t weight_basis_points) noexcept;
[[nodiscard]] ASHENCORE_API std::uint64_t ai_doctrine_hash(
    const AIDoctrineProfile& profile) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(AITemperament temperament) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(
    AIFormationDoctrine formation) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(
    AIScoutingDoctrine scouting) noexcept;

}  // namespace ashen::core
