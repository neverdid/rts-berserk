#include "ashen/core/AIInfluenceMap.hpp"
#include "ashen/core/Simulation.hpp"

#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string_view>

namespace {

using namespace ashen::core;

int failures = 0;

#define CHECK(condition)                                                          \
  do {                                                                            \
    if (!(condition)) {                                                           \
      std::cerr << "  check failed at line " << __LINE__ << ": " #condition "\n"; \
      ++failures;                                                                 \
    }                                                                             \
  } while (false)

template <typename Test>
void run_test(const std::string_view name, Test&& test) {
  const auto before = failures;
  test();
  std::cout << (failures == before ? "[pass] " : "[fail] ") << name << '\n';
}

[[nodiscard]] SimulationConfig open_config() {
  SimulationConfig config{};
  config.seed_starting_forces = false;
  config.map_size = world(1'600, 800);
  config.navigation_obstacles.clear();
  return config;
}

void influence_grid_is_deterministic_and_navigation_aware() {
  auto config = open_config();
  config.navigation_obstacles.push_back({world(720, 180), world(880, 620)});
  Simulation simulation{config};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(220, 400)));
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(360, 400)));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(520, 400)));
  static_cast<void>(simulation.add_control_point(world(1'250, 400)));
  simulation.step();

  const auto observation = simulation.observe(PlayerId::One);
  const AIInfluenceMap first{observation};
  const AIInfluenceMap second{observation};

  CHECK(first.hash() != 0);
  CHECK(first.hash() == second.hash());
  CHECK(first.cells() == second.cells());
  CHECK(observation.navigation_obstacles() == config.navigation_obstacles);
  CHECK(!first.cell_at(world(800, 400)).navigable);
  CHECK(first.cell_at(world(800, 400)).travel_cost == kAIUnreachableTravelCost);
  CHECK(first.cell_at(world(220, 400)).friendly_power > 0);
  CHECK(first.cell_at(world(520, 400)).observed_enemy_power > 0);
  CHECK(first.cell_at(world(1'250, 400)).objective_value > 0);
  CHECK(first.cell_at(world(1'500, 80)).uncertainty > first.cell_at(world(220, 400)).uncertainty);
}

void visible_static_defenses_project_danger_and_terror() {
  auto config = open_config();
  config.player_two_faction = FactionId::Ascendancy;
  Simulation simulation{config};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(300, 400)));
  const auto turret = simulation.spawn_entity(PlayerId::Two, EntityType::Turret, world(570, 400));
  simulation.step();

  const auto observation = simulation.observe(PlayerId::One);
  const auto sighting = std::ranges::find(observation.known_enemies(), turret, &ObservedEnemy::id);
  CHECK(sighting != observation.known_enemies().end());
  CHECK(sighting != observation.known_enemies().end() && sighting->currently_visible);

  const AIInfluenceMap influence{observation};
  const auto& threatened = influence.cell_at(world(570, 400));
  CHECK(threatened.observed_enemy_power > 0);
  CHECK(threatened.static_danger > 0);
  CHECK(threatened.terror_pressure > 0);
}

void hidden_live_positions_cannot_change_an_influence_map() {
  auto config = open_config();
  Simulation first{config};
  Simulation second{config};
  static_cast<void>(first.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  static_cast<void>(second.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  static_cast<void>(first.spawn_entity(PlayerId::Two, EntityType::Command, world(1'480, 400)));
  static_cast<void>(second.spawn_entity(PlayerId::Two, EntityType::Command, world(1'480, 400)));
  static_cast<void>(first.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(1'220, 250)));
  static_cast<void>(second.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(1'350, 650)));
  first.step();
  second.step();

  const auto first_observation = first.observe(PlayerId::One);
  const auto second_observation = second.observe(PlayerId::One);
  CHECK(first.state_hash() != second.state_hash());
  CHECK(first_observation.known_enemies().empty());
  CHECK(second_observation.known_enemies().empty());
  CHECK(first_observation.hash() == second_observation.hash());
  CHECK(AIInfluenceMap{first_observation}.hash() == AIInfluenceMap{second_observation}.hash());
}

void visibly_cleared_mobile_sighting_is_removed() {
  auto config = open_config();
  Simulation simulation{config};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(260, 400)));
  const auto enemy = simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(520, 400));
  simulation.step();
  const auto visible = simulation.observe(PlayerId::One);
  CHECK(std::ranges::find(visible.known_enemies(), enemy, &ObservedEnemy::id) !=
        visible.known_enemies().end());
  CHECK(simulation
            .execute_now(Command{.player = PlayerId::Two,
                                 .type = CommandType::Move,
                                 .entities = {enemy},
                                 .target = world(1'480, 400)})
            .ok);

  PlayerObservation observation = simulation.observe(PlayerId::One);
  for (std::int32_t step = 0; step < 300; ++step) {
    simulation.step();
    observation = simulation.observe(PlayerId::One);
    if (std::ranges::find(observation.known_enemies(), enemy, &ObservedEnemy::id) ==
        observation.known_enemies().end()) {
      break;
    }
  }
  CHECK(std::ranges::find(observation.known_enemies(), enemy, &ObservedEnemy::id) ==
        observation.known_enemies().end());
}

void unseen_mobile_influence_decays_from_its_last_known_position() {
  auto config = open_config();
  Simulation simulation{config};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(120, 400)));
  const auto scout =
      simulation.spawn_entity(PlayerId::One, EntityType::Skirmisher, world(560, 400));
  const auto enemy = simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(700, 400));
  simulation.step();
  CHECK(simulation
            .execute_now(Command{.player = PlayerId::One,
                                 .type = CommandType::Move,
                                 .entities = {scout},
                                 .target = world(120, 400)})
            .ok);
  CHECK(simulation
            .execute_now(Command{.player = PlayerId::Two,
                                 .type = CommandType::Move,
                                 .entities = {enemy},
                                 .target = world(1'480, 400)})
            .ok);

  PlayerObservation hidden = simulation.observe(PlayerId::One);
  for (std::int32_t step = 0; step < 300; ++step) {
    simulation.step();
    hidden = simulation.observe(PlayerId::One);
    const auto remembered = std::ranges::find(hidden.known_enemies(), enemy, &ObservedEnemy::id);
    if (remembered != hidden.known_enemies().end() && !remembered->currently_visible) {
      break;
    }
  }

  const auto remembered = std::ranges::find(hidden.known_enemies(), enemy, &ObservedEnemy::id);
  CHECK(remembered != hidden.known_enemies().end());
  CHECK(remembered != hidden.known_enemies().end() && !remembered->currently_visible);
  if (remembered == hidden.known_enemies().end()) {
    return;
  }
  const auto last_known_position = remembered->position;
  const AIInfluenceMap recent{hidden};
  const auto recent_power = recent.cell_at(last_known_position).observed_enemy_power;
  const auto recent_uncertainty = recent.cell_at(last_known_position).uncertainty;
  CHECK(recent_power > 0);

  simulation.run(600);
  const auto older_observation = simulation.observe(PlayerId::One);
  const auto older_memory =
      std::ranges::find(older_observation.known_enemies(), enemy, &ObservedEnemy::id);
  CHECK(older_memory != older_observation.known_enemies().end());
  CHECK(older_memory != older_observation.known_enemies().end() &&
        older_memory->position == last_known_position);
  const AIInfluenceMap older{older_observation};
  CHECK(older.cell_at(last_known_position).observed_enemy_power < recent_power);
  CHECK(older.cell_at(last_known_position).uncertainty <= recent_uncertainty);

  simulation.run(kMobileObservationMemoryTicks);
  const auto expired = simulation.observe(PlayerId::One);
  CHECK(std::ranges::find(expired.known_enemies(), enemy, &ObservedEnemy::id) ==
        expired.known_enemies().end());
}

void unseen_static_influence_persists_until_the_location_is_cleared() {
  auto config = open_config();
  Simulation simulation{config};
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(120, 400)));
  const auto scout =
      simulation.spawn_entity(PlayerId::One, EntityType::Skirmisher, world(580, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'480, 400)));
  const auto turret = simulation.spawn_entity(PlayerId::Two, EntityType::Turret, world(760, 400));
  simulation.step();
  const auto visible = simulation.observe(PlayerId::One);
  CHECK(std::ranges::find(visible.known_enemies(), turret, &ObservedEnemy::id) !=
        visible.known_enemies().end());
  CHECK(simulation
            .execute_now(Command{.player = PlayerId::One,
                                 .type = CommandType::Move,
                                 .entities = {scout},
                                 .target = world(120, 400)})
            .ok);

  PlayerObservation hidden = simulation.observe(PlayerId::One);
  for (std::int32_t step = 0; step < 300; ++step) {
    simulation.step();
    hidden = simulation.observe(PlayerId::One);
    const auto remembered = std::ranges::find(hidden.known_enemies(), turret, &ObservedEnemy::id);
    if (remembered != hidden.known_enemies().end() && !remembered->currently_visible) {
      break;
    }
  }

  const auto remembered = std::ranges::find(hidden.known_enemies(), turret, &ObservedEnemy::id);
  CHECK(remembered != hidden.known_enemies().end());
  CHECK(remembered != hidden.known_enemies().end() && !remembered->currently_visible);
  if (remembered == hidden.known_enemies().end()) {
    return;
  }
  const auto last_known_position = remembered->position;
  const auto recent_danger = AIInfluenceMap{hidden}.cell_at(last_known_position).static_danger;
  CHECK(recent_danger > 0);

  simulation.run(kMobileObservationMemoryTicks + 600);
  const auto persistent = simulation.observe(PlayerId::One);
  const auto persistent_memory =
      std::ranges::find(persistent.known_enemies(), turret, &ObservedEnemy::id);
  CHECK(persistent_memory != persistent.known_enemies().end());
  CHECK(persistent_memory != persistent.known_enemies().end() &&
        !persistent_memory->currently_visible);
  CHECK(AIInfluenceMap{persistent}.cell_at(last_known_position).static_danger == recent_danger);
}

}  // namespace

int main() {
  run_test("influence grid is deterministic and navigation aware",
           influence_grid_is_deterministic_and_navigation_aware);
  run_test("visible static defenses project danger and terror",
           visible_static_defenses_project_danger_and_terror);
  run_test("hidden live positions cannot change an influence map",
           hidden_live_positions_cannot_change_an_influence_map);
  run_test("visibly cleared mobile sightings are removed",
           visibly_cleared_mobile_sighting_is_removed);
  run_test("unseen mobile influence decays from its last known position",
           unseen_mobile_influence_decays_from_its_last_known_position);
  run_test("unseen static influence persists until its location is cleared",
           unseen_static_influence_persists_until_the_location_is_cleared);

  if (failures != 0) {
    std::cerr << failures << " influence-map check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All influence-map checks passed.\n";
  return EXIT_SUCCESS;
}
