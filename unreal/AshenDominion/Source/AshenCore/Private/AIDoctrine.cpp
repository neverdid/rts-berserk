#include "ashen/core/AIDoctrine.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace ashen::core {
namespace {

inline constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
inline constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;

[[nodiscard]] std::uint64_t mixed_seed(std::uint64_t seed, const FactionId faction,
                                       const PlayerId player) noexcept {
  seed ^= player == PlayerId::One ? 0x9e3779b97f4a7c15ULL : 0xd1b54a32d192ed03ULL;
  seed ^= (static_cast<std::uint64_t>(faction) + 1U) * 0x94d049bb133111ebULL;
  seed ^= seed >> 30U;
  seed *= 0xbf58476d1ce4e5b9ULL;
  seed ^= seed >> 27U;
  seed *= 0x94d049bb133111ebULL;
  return seed ^ (seed >> 31U);
}

[[nodiscard]] AIDoctrineProfile base_profile(const FactionId faction) noexcept {
  AIDoctrineProfile profile{};
  profile.faction = faction;
  switch (faction) {
    case FactionId::Compact:
      profile.formation = AIFormationDoctrine::ShieldLine;
      profile.scouting = AIScoutingDoctrine::MeasuredRecon;
      profile.economy_weight_basis_points = 12'000;
      profile.fortification_weight_basis_points = 12'500;
      profile.technology_weight_basis_points = 10'500;
      profile.vanguard_weight_basis_points = 11'000;
      profile.skirmisher_weight_basis_points = 10'500;
      profile.objective_weight_basis_points = 10'000;
      profile.scouting_weight_basis_points = 9'000;
      profile.aggression_weight_basis_points = 9'000;
      profile.preservation_weight_basis_points = 12'500;
      profile.cohesion_weight_basis_points = 13'500;
      profile.dread_exploitation_weight_basis_points = 8'000;
      profile.terror_resistance_weight_basis_points = 12'000;
      profile.ward_affinity_weight_basis_points = 11'500;
      profile.power_weight_basis_points = 12'500;
      profile.engagement_power_ratio_basis_points = 9'000;
      profile.tactical_retreat_health_basis_points = 5'000;
      profile.tactical_retreat_resolve = 48;
      profile.critical_retreat_health_basis_points = 3'200;
      profile.critical_retreat_resolve = 30;
      profile.formation_recovery_distance = world(260, 0).x;
      profile.worker_target = 6;
      profile.first_fortification_tick = 640;
      profile.earliest_scout_tick = 360;
      profile.scout_army_reserve = 2;
      profile.minimum_assault_units = 3;
      break;
    case FactionId::Ascendancy:
      profile.formation = AIFormationDoctrine::HuntingPack;
      profile.scouting = AIScoutingDoctrine::PredatoryProbe;
      profile.economy_weight_basis_points = 9'000;
      profile.fortification_weight_basis_points = 7'500;
      profile.technology_weight_basis_points = 11'000;
      profile.vanguard_weight_basis_points = 12'500;
      profile.skirmisher_weight_basis_points = 9'000;
      profile.objective_weight_basis_points = 9'500;
      profile.scouting_weight_basis_points = 11'000;
      profile.aggression_weight_basis_points = 13'500;
      profile.preservation_weight_basis_points = 6'500;
      profile.cohesion_weight_basis_points = 7'000;
      profile.dread_exploitation_weight_basis_points = 16'000;
      profile.terror_resistance_weight_basis_points = 6'500;
      profile.ward_affinity_weight_basis_points = 6'000;
      profile.power_weight_basis_points = 12'000;
      profile.engagement_power_ratio_basis_points = 6'800;
      profile.tactical_retreat_health_basis_points = 3'200;
      profile.tactical_retreat_resolve = 30;
      profile.critical_retreat_health_basis_points = 2'000;
      profile.critical_retreat_resolve = 18;
      profile.formation_recovery_distance = world(520, 0).x;
      profile.worker_target = 5;
      profile.first_fortification_tick = 1'200;
      profile.earliest_scout_tick = 240;
      profile.scout_army_reserve = 1;
      profile.minimum_assault_units = 2;
      break;
    case FactionId::Concord:
      profile.formation = AIFormationDoctrine::WardWeb;
      profile.scouting = AIScoutingDoctrine::FarSightScreen;
      profile.economy_weight_basis_points = 10'500;
      profile.fortification_weight_basis_points = 14'000;
      profile.technology_weight_basis_points = 11'500;
      profile.vanguard_weight_basis_points = 9'000;
      profile.skirmisher_weight_basis_points = 12'500;
      profile.objective_weight_basis_points = 14'000;
      profile.scouting_weight_basis_points = 14'000;
      profile.aggression_weight_basis_points = 9'500;
      profile.preservation_weight_basis_points = 13'500;
      profile.cohesion_weight_basis_points = 15'000;
      profile.dread_exploitation_weight_basis_points = 6'000;
      profile.terror_resistance_weight_basis_points = 15'000;
      profile.ward_affinity_weight_basis_points = 16'000;
      profile.power_weight_basis_points = 13'000;
      profile.engagement_power_ratio_basis_points = 10'000;
      profile.tactical_retreat_health_basis_points = 5'600;
      profile.tactical_retreat_resolve = 56;
      profile.critical_retreat_health_basis_points = 3'600;
      profile.critical_retreat_resolve = 38;
      profile.formation_recovery_distance = world(220, 0).x;
      profile.worker_target = 5;
      profile.first_fortification_tick = 560;
      profile.earliest_scout_tick = 200;
      profile.scout_army_reserve = 2;
      profile.minimum_assault_units = 3;
      break;
  }
  return profile;
}

void apply_temperament(AIDoctrineProfile& profile) noexcept {
  switch (profile.temperament) {
    case AITemperament::Steady:
      break;
    case AITemperament::Audacious:
      profile.aggression_weight_basis_points += 400;
      profile.scouting_weight_basis_points += 100;
      profile.preservation_weight_basis_points -= 300;
      profile.engagement_power_ratio_basis_points -= 200;
      profile.tactical_retreat_health_basis_points -= 150;
      profile.tactical_retreat_resolve -= 2;
      break;
    case AITemperament::Watchful:
      profile.scouting_weight_basis_points += 400;
      profile.terror_resistance_weight_basis_points += 300;
      profile.cohesion_weight_basis_points += 200;
      profile.aggression_weight_basis_points -= 150;
      profile.engagement_power_ratio_basis_points += 150;
      profile.tactical_retreat_health_basis_points += 100;
      profile.tactical_retreat_resolve += 1;
      break;
  }
}

template <typename Value>
void hash_integral(std::uint64_t& hash, const Value value) noexcept {
  auto bits = static_cast<std::uint64_t>(value);
  for (std::size_t byte = 0; byte < sizeof(Value); ++byte) {
    hash ^= bits & 0xffU;
    hash *= kFnvPrime;
    bits >>= 8U;
  }
}

}  // namespace

AIDoctrineProfile ai_doctrine_profile(const FactionId faction,
                                       const std::uint64_t match_seed,
                                       const PlayerId player) noexcept {
  auto profile = base_profile(faction);
  profile.temperament =
      static_cast<AITemperament>(mixed_seed(match_seed, faction, player) % 3U);
  apply_temperament(profile);
  return profile;
}

std::int32_t apply_ai_weight(const std::int32_t score,
                             const std::int32_t weight_basis_points) noexcept {
  const auto weighted = static_cast<std::int64_t>(score) * weight_basis_points / 10'000;
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(weighted, std::numeric_limits<std::int32_t>::min(),
                               std::numeric_limits<std::int32_t>::max()));
}

std::uint64_t ai_doctrine_hash(const AIDoctrineProfile& profile) noexcept {
  auto hash = kFnvOffset;
  hash_integral(hash, static_cast<std::uint8_t>(profile.faction));
  hash_integral(hash, static_cast<std::uint8_t>(profile.temperament));
  hash_integral(hash, static_cast<std::uint8_t>(profile.formation));
  hash_integral(hash, static_cast<std::uint8_t>(profile.scouting));
  hash_integral(hash, profile.economy_weight_basis_points);
  hash_integral(hash, profile.fortification_weight_basis_points);
  hash_integral(hash, profile.technology_weight_basis_points);
  hash_integral(hash, profile.vanguard_weight_basis_points);
  hash_integral(hash, profile.skirmisher_weight_basis_points);
  hash_integral(hash, profile.objective_weight_basis_points);
  hash_integral(hash, profile.scouting_weight_basis_points);
  hash_integral(hash, profile.aggression_weight_basis_points);
  hash_integral(hash, profile.preservation_weight_basis_points);
  hash_integral(hash, profile.cohesion_weight_basis_points);
  hash_integral(hash, profile.dread_exploitation_weight_basis_points);
  hash_integral(hash, profile.terror_resistance_weight_basis_points);
  hash_integral(hash, profile.ward_affinity_weight_basis_points);
  hash_integral(hash, profile.power_weight_basis_points);
  hash_integral(hash, profile.engagement_power_ratio_basis_points);
  hash_integral(hash, profile.tactical_retreat_health_basis_points);
  hash_integral(hash, profile.tactical_retreat_resolve);
  hash_integral(hash, profile.critical_retreat_health_basis_points);
  hash_integral(hash, profile.critical_retreat_resolve);
  hash_integral(hash, profile.formation_recovery_distance);
  hash_integral(hash, profile.worker_target);
  hash_integral(hash, profile.first_fortification_tick);
  hash_integral(hash, profile.earliest_scout_tick);
  hash_integral(hash, profile.scout_army_reserve);
  hash_integral(hash, profile.minimum_assault_units);
  return hash;
}

std::string_view to_string(const AITemperament temperament) noexcept {
  switch (temperament) {
    case AITemperament::Steady:
      return "steady";
    case AITemperament::Audacious:
      return "audacious";
    case AITemperament::Watchful:
      return "watchful";
  }
  return "unknown";
}

std::string_view to_string(const AIFormationDoctrine formation) noexcept {
  switch (formation) {
    case AIFormationDoctrine::ShieldLine:
      return "shield_line";
    case AIFormationDoctrine::HuntingPack:
      return "hunting_pack";
    case AIFormationDoctrine::WardWeb:
      return "ward_web";
  }
  return "unknown";
}

std::string_view to_string(const AIScoutingDoctrine scouting) noexcept {
  switch (scouting) {
    case AIScoutingDoctrine::MeasuredRecon:
      return "measured_recon";
    case AIScoutingDoctrine::PredatoryProbe:
      return "predatory_probe";
    case AIScoutingDoctrine::FarSightScreen:
      return "far_sight_screen";
  }
  return "unknown";
}

}  // namespace ashen::core
