#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>
#include <utility>
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

void navigation_routes_through_the_visible_bridge() {
  auto simulation = sandbox();
  const auto unit = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(760, 100));
  const auto target = world(1'160, 100);
  CHECK(simulation.execute_now(
            Command{.player = PlayerId::One, .type = CommandType::Move, .entities = {unit}, .target = target})
            .ok);

  std::int32_t highest_y = 0;
  bool crossed_river = false;
  for (int tick = 0; tick < 500; ++tick) {
    simulation.step();
    const auto* entity = simulation.find_entity(unit);
    CHECK(entity != nullptr);
    if (entity == nullptr) {
      break;
    }
    highest_y = std::max(highest_y, entity->position.y);
    if (entity->position.x >= world(875, 0).x && entity->position.x <= world(1'045, 0).x) {
      crossed_river = true;
      CHECK(entity->position.y > world(285, 0).x && entity->position.y < world(395, 0).x);
    }
  }

  CHECK(crossed_river);
  CHECK(highest_y > world(285, 0).x);
  CHECK(simulation.find_entity(unit)->position == target);
}

void formation_orders_assign_distinct_non_overlapping_slots() {
  auto simulation = sandbox();
  std::vector<EntityId> units;
  for (int index = 0; index < 6; ++index) {
    units.push_back(simulation.spawn_entity(PlayerId::One, EntityType::Worker,
                                            world(100 + index * 18, 100)));
  }
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Move,
                                       .entities = units,
                                       .target = world(520, 420)})
            .ok);

  std::set<std::pair<std::int32_t, std::int32_t>> destinations;
  for (const auto id : units) {
    const auto* entity = simulation.find_entity(id);
    CHECK(entity != nullptr);
    if (entity != nullptr) {
      destinations.emplace(entity->order.target.x, entity->order.target.y);
    }
  }
  CHECK(destinations.size() == units.size());

  simulation.run(300);
  for (std::size_t left = 0; left < units.size(); ++left) {
    const auto* left_entity = simulation.find_entity(units[left]);
    CHECK(left_entity != nullptr && left_entity->order.type == OrderType::Idle);
    for (std::size_t right = left + 1; right < units.size(); ++right) {
      const auto* right_entity = simulation.find_entity(units[right]);
      if (left_entity == nullptr || right_entity == nullptr) {
        continue;
      }
      const auto dx = static_cast<std::int64_t>(left_entity->position.x) - right_entity->position.x;
      const auto dy = static_cast<std::int64_t>(left_entity->position.y) - right_entity->position.y;
      const auto required = static_cast<std::int64_t>(left_entity->radius + right_entity->radius);
      CHECK(dx * dx + dy * dy >= required * required);
    }
  }
}

void shift_queued_orders_execute_in_sequence() {
  auto simulation = sandbox();
  const auto unit = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(100, 100));
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Move,
                                       .entities = {unit},
                                       .target = world(180, 100)})
            .ok);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Move,
                                       .entities = {unit},
                                       .target = world(180, 220),
                                       .queue = true})
            .ok);
  CHECK(simulation.find_entity(unit)->order_queue.size() == 1);
  simulation.run(100);
  CHECK(simulation.find_entity(unit)->position == world(180, 220));
  CHECK(simulation.find_entity(unit)->order.type == OrderType::Idle);
}

void attack_move_engages_then_resumes_the_march() {
  auto simulation = sandbox();
  const auto attacker = simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(100, 100));
  const auto enemy = simulation.spawn_entity(PlayerId::Two, EntityType::Worker, world(250, 100));
  const auto destination = world(430, 100);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::AttackMove,
                                       .entities = {attacker},
                                       .target = destination})
            .ok);
  simulation.run(300);
  CHECK(simulation.find_entity(enemy) == nullptr);
  CHECK(simulation.find_entity(attacker)->position == destination);
  CHECK(simulation.find_entity(attacker)->order.type == OrderType::Idle);
}

void hold_position_fights_without_chasing() {
  auto simulation = sandbox();
  const auto defender = simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(100, 100));
  const auto enemy = simulation.spawn_entity(PlayerId::Two, EntityType::Worker, world(145, 100));
  const auto starting_position = simulation.find_entity(defender)->position;
  CHECK(simulation.execute_now(
            Command{.player = PlayerId::One, .type = CommandType::Hold, .entities = {defender}})
            .ok);
  simulation.run(120);
  CHECK(simulation.find_entity(defender)->position == starting_position);
  CHECK(simulation.find_entity(defender)->order.type == OrderType::Hold);
  CHECK(simulation.find_entity(enemy) == nullptr);
}

void stop_clears_movement_and_queued_orders() {
  auto simulation = sandbox();
  const auto unit = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(100, 100));
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Move,
                                       .entities = {unit},
                                       .target = world(600, 100)})
            .ok);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Move,
                                       .entities = {unit},
                                       .target = world(600, 300),
                                       .queue = true})
            .ok);
  simulation.run(5);
  CHECK(simulation.execute_now(
            Command{.player = PlayerId::One, .type = CommandType::Stop, .entities = {unit}})
            .ok);
  const auto stopped_at = simulation.find_entity(unit)->position;
  simulation.run(100);
  CHECK(simulation.find_entity(unit)->position == stopped_at);
  CHECK(simulation.find_entity(unit)->order.type == OrderType::Idle);
  CHECK(simulation.find_entity(unit)->order_queue.empty());
}

void patrol_reverses_and_remains_active() {
  auto simulation = sandbox();
  const auto unit = simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(100, 100));
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Patrol,
                                       .entities = {unit},
                                       .target = world(220, 100)})
            .ok);

  bool moved_back = false;
  auto previous_x = simulation.find_entity(unit)->position.x;
  for (int tick = 0; tick < 100; ++tick) {
    simulation.step();
    const auto current_x = simulation.find_entity(unit)->position.x;
    moved_back = moved_back || current_x < previous_x;
    previous_x = current_x;
  }
  CHECK(moved_back);
  CHECK(simulation.find_entity(unit)->order.type == OrderType::Patrol);
}

void rally_point_controls_completed_production() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(100, 100)));
  const auto barracks = simulation.spawn_entity(PlayerId::One, EntityType::Barracks, world(180, 100));
  const auto rally = world(320, 180);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::SetRallyPoint,
                                       .target = rally,
                                       .producer = barracks})
            .ok);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Train,
                                       .producer = barracks,
                                       .train_type = EntityType::Vanguard})
            .ok);
  simulation.run(entity_definition(FactionId::Candlebound, EntityType::Vanguard).build_ticks);
  const auto trained = std::find_if(simulation.entities().begin(), simulation.entities().end(),
                                    [](const Entity& entity) { return entity.type == EntityType::Vanguard; });
  CHECK(trained != simulation.entities().end());
  CHECK(trained != simulation.entities().end() && trained->order.type == OrderType::Move);
  simulation.run(100);
  const auto arrived = std::find_if(simulation.entities().begin(), simulation.entities().end(),
                                    [](const Entity& entity) { return entity.type == EntityType::Vanguard; });
  CHECK(arrived != simulation.entities().end() && arrived->position == rally);
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
  run_test("navigation routes through the visible bridge", navigation_routes_through_the_visible_bridge);
  run_test("formation orders assign distinct non-overlapping slots",
           formation_orders_assign_distinct_non_overlapping_slots);
  run_test("shift-queued orders execute in sequence", shift_queued_orders_execute_in_sequence);
  run_test("attack-move engages then resumes the march", attack_move_engages_then_resumes_the_march);
  run_test("hold position fights without chasing", hold_position_fights_without_chasing);
  run_test("stop clears movement and queued orders", stop_clears_movement_and_queued_orders);
  run_test("patrol reverses and remains active", patrol_reverses_and_remains_active);
  run_test("rally point controls completed production", rally_point_controls_completed_production);

  if (failures != 0) {
    std::cerr << failures << " native simulation check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All native simulation checks passed.\n";
  return EXIT_SUCCESS;
}
