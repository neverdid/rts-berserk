#include "ashen/core/AIDoctrine.hpp"
#include "ashen/core/CommanderAI.hpp"
#include "ashen/core/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string_view>

namespace {

using namespace ashen::core;

int failures = 0;

#define CHECK(condition)                                                                            \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      std::cerr << "  check failed at line " << __LINE__ << ": " #condition "\n";               \
      ++failures;                                                                                   \
    }                                                                                               \
  } while (false)

template <typename Test>
void run_test(const std::string_view name, Test&& test) {
  const auto before = failures;
  test();
  std::cout << (failures == before ? "[pass] " : "[fail] ") << name << '\n';
}

[[nodiscard]] SimulationConfig faction_config(const FactionId faction,
                                              const std::uint64_t seed = 91) {
  SimulationConfig config{};
  config.player_one_faction = faction;
  config.player_two_faction = FactionId::Compact;
  config.seed_starting_forces = false;
  config.starting_ore = {1'000, 1'000};
  config.map_size = world(1'600, 800);
  config.navigation_obstacles.clear();
  config.match_seed = seed;
  return config;
}

void hold(Simulation& simulation, const PlayerId player,
          const std::initializer_list<EntityId> entities) {
  CHECK(simulation
            .execute_now(Command{.player = player,
                                 .type = CommandType::Hold,
                                 .entities = entities})
            .ok);
}

[[nodiscard]] const AIPlannedDecision* decision_for(
    const CommanderPlan& plan, const AIDecisionLayer layer) noexcept {
  const auto found = std::ranges::find(plan.decisions, layer,
                                       &AIPlannedDecision::layer);
  return found == plan.decisions.end() ? nullptr : &*found;
}

[[nodiscard]] AIAction engagement_action(const FactionId faction,
                                         const std::uint64_t seed) {
  Simulation simulation{faction_config(faction, seed)};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command,
                                            world(180, 400)));
  const auto defender = simulation.spawn_entity(
      PlayerId::One, EntityType::Vanguard, world(360, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command,
                                            world(1'420, 400)));
  const auto first = simulation.spawn_entity(
      PlayerId::Two, EntityType::Vanguard, world(540, 350));
  const auto second = simulation.spawn_entity(
      PlayerId::Two, EntityType::Vanguard, world(540, 450));
  hold(simulation, PlayerId::One, {defender});
  hold(simulation, PlayerId::Two, {first, second});
  simulation.run(kTacticalDecisionPhase);

  const auto plan =
      CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* tactical = decision_for(plan, AIDecisionLayer::Tactical);
  return tactical == nullptr ? AIAction::AssignGatherers
                             : tactical->selected_action;
}

[[nodiscard]] bool scouts_with_one_combat_unit(const FactionId faction,
                                               const std::uint64_t seed) {
  Simulation simulation{faction_config(faction, seed)};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command,
                                            world(160, 400)));
  const auto scout = simulation.spawn_entity(
      PlayerId::One, EntityType::Skirmisher, world(320, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command,
                                            world(1'520, 400)));
  hold(simulation, PlayerId::One, {scout});
  simulation.run(270);

  const auto plan =
      CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* tactical = decision_for(plan, AIDecisionLayer::Tactical);
  return tactical != nullptr && tactical->selected_action == AIAction::Scout;
}

[[nodiscard]] bool reforms_a_tight_line(const FactionId faction,
                                        const std::uint64_t seed) {
  Simulation simulation{faction_config(faction, seed)};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command,
                                            world(160, 400)));
  const auto first = simulation.spawn_entity(
      PlayerId::One, EntityType::Vanguard, world(300, 360));
  const auto second = simulation.spawn_entity(
      PlayerId::One, EntityType::Vanguard, world(300, 400));
  const auto straggler = simulation.spawn_entity(
      PlayerId::One, EntityType::Vanguard, world(660, 380));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command,
                                            world(1'520, 400)));
  hold(simulation, PlayerId::One, {first, second, straggler});
  simulation.run(kMicroDecisionCadence);

  const auto plan =
      CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* micro = decision_for(plan, AIDecisionLayer::Micro);
  return micro != nullptr &&
         micro->selected_action == AIAction::RejoinFormation;
}

[[nodiscard]] std::uint8_t behavior_signature(const FactionId faction,
                                              const std::uint64_t seed) {
  std::uint8_t signature = 0;
  signature |= scouts_with_one_combat_unit(faction, seed) ? 1U : 0U;
  signature |=
      engagement_action(faction, seed) == AIAction::EngageForce ? 2U : 0U;
  signature |= reforms_a_tight_line(faction, seed) ? 4U : 0U;
  return signature;
}

void doctrine_identity_dominates_personality_variance() {
  for (std::uint64_t seed = 1; seed <= 128; ++seed) {
    const auto compact =
        ai_doctrine_profile(FactionId::Compact, seed, PlayerId::One);
    const auto ascendancy =
        ai_doctrine_profile(FactionId::Ascendancy, seed, PlayerId::One);
    const auto concord =
        ai_doctrine_profile(FactionId::Concord, seed, PlayerId::One);

    CHECK(compact.economy_weight_basis_points >
          ascendancy.economy_weight_basis_points);
    CHECK(ascendancy.aggression_weight_basis_points >
          compact.aggression_weight_basis_points);
    CHECK(ascendancy.aggression_weight_basis_points >
          concord.aggression_weight_basis_points);
    CHECK(concord.objective_weight_basis_points >
          compact.objective_weight_basis_points);
    CHECK(concord.ward_affinity_weight_basis_points >
          compact.ward_affinity_weight_basis_points);
    CHECK(compact.engagement_power_ratio_basis_points >
          ascendancy.engagement_power_ratio_basis_points);
    CHECK(concord.engagement_power_ratio_basis_points >
          compact.engagement_power_ratio_basis_points);
  }
}

void doctrine_profiles_are_deterministic_and_auditable() {
  for (const auto faction :
       {FactionId::Compact, FactionId::Ascendancy, FactionId::Concord}) {
    const auto first = ai_doctrine_profile(faction, 77, PlayerId::Two);
    const auto second = ai_doctrine_profile(faction, 77, PlayerId::Two);
    CHECK(first == second);
    CHECK(ai_doctrine_hash(first) != 0);
    CHECK(ai_doctrine_hash(first) == ai_doctrine_hash(second));
  }

  const auto compact =
      ai_doctrine_profile(FactionId::Compact, 77, PlayerId::Two);
  const auto ascendancy =
      ai_doctrine_profile(FactionId::Ascendancy, 77, PlayerId::Two);
  const auto concord =
      ai_doctrine_profile(FactionId::Concord, 77, PlayerId::Two);
  CHECK(ai_doctrine_hash(compact) != ai_doctrine_hash(ascendancy));
  CHECK(ai_doctrine_hash(compact) != ai_doctrine_hash(concord));
  CHECK(ai_doctrine_hash(ascendancy) != ai_doctrine_hash(concord));
}

void blind_decision_signatures_distinguish_all_factions() {
  for (std::uint64_t seed = 1; seed <= 32; ++seed) {
    const std::array signatures{
        behavior_signature(FactionId::Compact, seed),
        behavior_signature(FactionId::Ascendancy, seed),
        behavior_signature(FactionId::Concord, seed),
    };
    CHECK(signatures[0] == 0U);
    CHECK(signatures[1] == 3U);
    CHECK(signatures[2] == 5U);
    CHECK(signatures[0] != signatures[1]);
    CHECK(signatures[0] != signatures[2]);
    CHECK(signatures[1] != signatures[2]);
  }
}

void decisions_retain_the_applied_doctrine_fingerprint() {
  Simulation simulation{faction_config(FactionId::Ascendancy)};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command,
                                            world(160, 400)));
  const auto scout = simulation.spawn_entity(
      PlayerId::One, EntityType::Skirmisher, world(320, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command,
                                            world(1'520, 400)));
  hold(simulation, PlayerId::One, {scout});
  simulation.run(270);

  const auto observation = simulation.observe(PlayerId::One);
  const auto expected = ai_doctrine_profile(
      FactionId::Ascendancy, observation.match_seed(), PlayerId::One);
  const auto plan = CommanderAI{PlayerId::One}.plan(observation);
  CHECK(!plan.decisions.empty());
  for (const auto& decision : plan.decisions) {
    CHECK(decision.doctrine_faction == FactionId::Ascendancy);
    CHECK(decision.temperament == expected.temperament);
    CHECK(decision.doctrine_hash == ai_doctrine_hash(expected));
  }
}

void ascendancy_focuses_low_resolve_prey() {
  Simulation simulation{faction_config(FactionId::Ascendancy)};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command,
                                            world(160, 400)));
  const auto vanguard = simulation.spawn_entity(
      PlayerId::One, EntityType::Vanguard, world(300, 380));
  const auto seer = simulation.spawn_entity(
      PlayerId::One, EntityType::Skirmisher, world(320, 420));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command,
                                            world(1'520, 400)));
  const auto wavering = simulation.spawn_entity(
      PlayerId::Two, EntityType::Skirmisher, world(470, 380));
  const auto steady = simulation.spawn_entity(
      PlayerId::Two, EntityType::Skirmisher, world(590, 420));
  hold(simulation, PlayerId::One, {vanguard, seer});
  hold(simulation, PlayerId::Two, {wavering, steady});
  simulation.run(kMicroDecisionCadence);

  const auto observation = simulation.observe(PlayerId::One);
  const auto wavering_enemy = std::ranges::find(
      observation.known_enemies(), wavering, &ObservedEnemy::id);
  const auto steady_enemy = std::ranges::find(
      observation.known_enemies(), steady, &ObservedEnemy::id);
  CHECK(wavering_enemy != observation.known_enemies().end());
  CHECK(steady_enemy != observation.known_enemies().end());
  CHECK(wavering_enemy != observation.known_enemies().end() &&
        steady_enemy != observation.known_enemies().end() &&
        wavering_enemy->resolve < steady_enemy->resolve);

  const auto plan = CommanderAI{PlayerId::One}.plan(observation);
  const auto* micro = decision_for(plan, AIDecisionLayer::Micro);
  CHECK(micro != nullptr);
  CHECK(micro != nullptr &&
        micro->selected_action == AIAction::FocusFire);
  CHECK(micro != nullptr && micro->command.target_entity == wavering);
  if (micro != nullptr) {
    const auto& selected =
        micro->candidates[micro->selected_candidate];
    CHECK(std::ranges::any_of(
        selected.components, [](const AIUtilityComponent& component) {
          return component.reason == AIUtilityReason::DreadExploitation &&
                 component.score > 0;
        }));
  }
}

}  // namespace

int main() {
  run_test("doctrine identity dominates personality variance",
           doctrine_identity_dominates_personality_variance);
  run_test("doctrine profiles are deterministic and auditable",
           doctrine_profiles_are_deterministic_and_auditable);
  run_test("blind decision signatures distinguish all factions",
           blind_decision_signatures_distinguish_all_factions);
  run_test("decisions retain the applied doctrine fingerprint",
           decisions_retain_the_applied_doctrine_fingerprint);
  run_test("Ascendancy focuses wavering prey to exploit dread",
           ascendancy_focuses_low_resolve_prey);

  if (failures != 0) {
    std::cerr << failures << " faction AI check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All faction AI checks passed.\n";
  return EXIT_SUCCESS;
}
