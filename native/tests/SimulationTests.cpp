#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using namespace ashen::core;

int failures = 0;

#define CHECK(condition)                                                                                 \
  do {                                                                                                   \
    if (!(condition)) {                                                                                  \
      std::cerr << "  check failed at line " << __LINE__ << ": " #condition "\n";                    \
      ++failures;                                                                                        \
    }                                                                                                    \
  } while (false)

template <typename Test>
void run_test(const std::string_view name, Test&& test) {
  const auto before = failures;
  test();
  // cppcheck-suppress knownConditionTrueFalse
  std::cout << (failures == before ? "[pass] " : "[fail] ") << name << '\n';
}

[[nodiscard]] Simulation sandbox(const FactionId one = FactionId::Candlebound,
                                 const FactionId two = FactionId::Hollow) {
  SimulationConfig config{};
  config.player_one_faction = one;
  config.player_two_faction = two;
  config.seed_starting_forces = false;
  return Simulation{config};
}

void deterministic_fixed_step() {
  Simulation first{};
  Simulation second{};
  CHECK(first.state_hash() == second.state_hash());
  first.run(240);
  second.run(240);
  CHECK(first.tick() == 240);
  CHECK(first.state_hash() == second.state_hash());
}

void movement_reaches_an_exact_target() {
  auto simulation = sandbox();
  const auto worker = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(100, 100));
  const auto target = world(160, 100);
  const auto result = simulation.execute_now(
      Command{.player = PlayerId::One, .type = CommandType::Move, .entities = {worker}, .target = target});
  CHECK(result.ok);
  simulation.run(20);
  const auto* entity = simulation.find_entity(worker);
  CHECK(entity != nullptr);
  CHECK(entity != nullptr && entity->position == target);
  CHECK(entity != nullptr && entity->order.type == OrderType::Idle);
}

void workers_complete_repeatable_ore_trips() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(100, 100)));
  const auto worker = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(145, 100));
  const auto resource = simulation.add_resource(world(220, 100), 100);
  const auto starting_ore = simulation.player(PlayerId::One).ore;
  const auto result = simulation.execute_now(Command{.player = PlayerId::One,
                                                      .type = CommandType::Gather,
                                                      .entities = {worker},
                                                      .resource = resource});
  CHECK(result.ok);
  simulation.run(300);
  CHECK(simulation.player(PlayerId::One).ore > starting_ore);
  CHECK(simulation.find_resource(resource)->amount < 100);
}

void production_obeys_cost_supply_and_build_time() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(100, 100)));
  const auto barracks = simulation.spawn_entity(PlayerId::One, EntityType::Barracks, world(180, 100));
  const auto before_ore = simulation.player(PlayerId::One).ore;
  const auto definition = entity_definition(FactionId::Candlebound, EntityType::Vanguard);
  const auto result = simulation.execute_now(Command{.player = PlayerId::One,
                                                      .type = CommandType::Train,
                                                      .producer = barracks,
                                                      .train_type = EntityType::Vanguard});
  CHECK(result.ok);
  CHECK(simulation.player(PlayerId::One).ore == before_ore - definition.cost);
  simulation.run(definition.build_ticks);
  const auto count = std::count_if(simulation.entities().begin(), simulation.entities().end(),
                                   [](const Entity& entity) { return entity.type == EntityType::Vanguard; });
  CHECK(count == 1);
  CHECK(simulation.player(PlayerId::One).supply_used == definition.supply_cost);
}

void factions_keep_meaningful_asymmetry() {
  const auto remnant = entity_definition(FactionId::Candlebound, EntityType::Vanguard);
  const auto choir = entity_definition(FactionId::Hollow, EntityType::Vanguard);
  const auto host = entity_definition(FactionId::Sepulcher, EntityType::Vanguard);
  CHECK(choir.cost < remnant.cost);
  CHECK(choir.speed_per_tick > remnant.speed_per_tick);
  CHECK(host.hit_points > remnant.hit_points);
  CHECK(host.damage > remnant.damage);
  CHECK(faction_definition(FactionId::Hollow).income_basis_points >
        faction_definition(FactionId::Sepulcher).income_basis_points);
}

void command_destruction_ends_the_match() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(100, 100)));
  const auto enemy_command = simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(400, 100));
  const auto attacker = simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(300, 100));
  const auto result = simulation.execute_now(Command{.player = PlayerId::One,
                                                      .type = CommandType::Attack,
                                                      .entities = {attacker},
                                                      .target_entity = enemy_command});
  CHECK(result.ok);
  simulation.run(1'200);
  CHECK(simulation.status() == MatchStatus::Won);
  CHECK(simulation.winner() == PlayerId::One);
  CHECK(simulation.find_entity(enemy_command) == nullptr);
}

void queued_commands_replay_to_the_same_hash() {
  Simulation first{};
  Simulation second{};
  const auto first_worker = std::find_if(first.entities().begin(), first.entities().end(), [](const Entity& entity) {
    return entity.owner == PlayerId::One && entity.type == EntityType::Worker;
  })->id;
  const auto second_worker = std::find_if(second.entities().begin(), second.entities().end(), [](const Entity& entity) {
    return entity.owner == PlayerId::One && entity.type == EntityType::Worker;
  })->id;
  CHECK(first_worker == second_worker);

  Command first_command{.execute_tick = 10,
                        .sequence = 7,
                        .player = PlayerId::One,
                        .type = CommandType::Move,
                        .entities = {first_worker},
                        .target = world(700, 450)};
  auto second_command = first_command;
  second_command.entities = {second_worker};
  first.enqueue(first_command);
  second.enqueue(second_command);
  first.run(400);
  second.run(400);
  CHECK(first.state_hash() == second.state_hash());
}

void same_tick_commands_have_stable_sequence_order() {
  auto simulation = sandbox();
  const auto worker = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(100, 100));
  simulation.enqueue(Command{.execute_tick = 2,
                             .sequence = 1,
                             .player = PlayerId::One,
                             .type = CommandType::Move,
                             .entities = {worker},
                             .target = world(300, 100)});
  simulation.enqueue(Command{.execute_tick = 2,
                             .sequence = 2,
                             .player = PlayerId::One,
                             .type = CommandType::Move,
                             .entities = {worker},
                             .target = world(100, 300)});
  simulation.run(80);
  CHECK(simulation.find_entity(worker)->position == world(100, 300));
}

void state_hash_covers_queued_command_payloads() {
  Simulation first{};
  Simulation second{};
  const auto worker = std::find_if(first.entities().begin(), first.entities().end(), [](const Entity& entity) {
    return entity.owner == PlayerId::One && entity.type == EntityType::Worker;
  })->id;
  first.enqueue(Command{.execute_tick = 50,
                        .sequence = 1,
                        .player = PlayerId::One,
                        .type = CommandType::Move,
                        .entities = {worker},
                        .target = world(600, 400)});
  second.enqueue(Command{.execute_tick = 50,
                         .sequence = 1,
                         .player = PlayerId::One,
                         .type = CommandType::Move,
                         .entities = {worker},
                         .target = world(700, 400)});
  CHECK(first.state_hash() != second.state_hash());
}

}  // namespace

int main() {
  run_test("deterministic fixed step", deterministic_fixed_step);
  run_test("movement reaches an exact target", movement_reaches_an_exact_target);
  run_test("workers complete repeatable ore trips", workers_complete_repeatable_ore_trips);
  run_test("production obeys cost, supply, and build time", production_obeys_cost_supply_and_build_time);
  run_test("factions keep meaningful asymmetry", factions_keep_meaningful_asymmetry);
  run_test("command destruction ends the match", command_destruction_ends_the_match);
  run_test("queued commands replay to the same hash", queued_commands_replay_to_the_same_hash);
  run_test("same-tick commands have stable sequence order", same_tick_commands_have_stable_sequence_order);
  run_test("state hash covers queued command payloads", state_hash_covers_queued_command_payloads);

  if (failures != 0) {
    std::cerr << failures << " native simulation check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All native simulation checks passed.\n";
  return EXIT_SUCCESS;
}
