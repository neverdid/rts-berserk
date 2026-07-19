#include "ashen/core/CommanderAI.hpp"
#include "ashen/core/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
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

[[nodiscard]] Simulation sandbox() {
  SimulationConfig config{};
  config.seed_starting_forces = false;
  config.starting_ore = {1'000, 1'000};
  config.map_size = world(1'200, 800);
  config.navigation_obstacles.clear();
  return Simulation{config};
}

void hold(Simulation& simulation, const PlayerId player,
          const std::initializer_list<EntityId> entities) {
  CHECK(simulation.execute_now(
              Command{.player = player, .type = CommandType::Hold, .entities = entities})
            .ok);
}

[[nodiscard]] const AIPlannedDecision* decision_for(const CommanderPlan& plan,
                                                     const AIDecisionLayer layer) noexcept {
  const auto found = std::ranges::find(plan.decisions, layer, &AIPlannedDecision::layer);
  return found == plan.decisions.end() ? nullptr : &*found;
}

[[nodiscard]] bool has_action(const CommanderPlan& plan, const AIAction action) noexcept {
  return std::ranges::any_of(plan.decisions, [action](const AIPlannedDecision& decision) {
    return decision.selected_action == action;
  });
}

[[nodiscard]] bool valid_score_trace(const AIPlannedDecision& decision) noexcept {
  if (decision.candidates.empty() || decision.selected_candidate >= decision.candidates.size()) {
    return false;
  }
  for (const auto& candidate : decision.candidates) {
    std::int64_t total = 0;
    for (const auto& component : candidate.components) {
      total += component.score;
    }
    if (candidate.components.empty() || total != candidate.total_score) {
      return false;
    }
  }

  const auto& selected = decision.candidates[decision.selected_candidate];
  if (selected.action != decision.selected_action ||
      std::ranges::any_of(decision.candidates, [&](const AICandidateScore& candidate) {
        return candidate.total_score > selected.total_score;
      })) {
    return false;
  }

  auto expected_reason = AIUtilityReason::Baseline;
  auto best_component = std::numeric_limits<std::int32_t>::min();
  for (const auto& component : selected.components) {
    if (component.score > best_component ||
        (component.score == best_component && component.reason < expected_reason)) {
      expected_reason = component.reason;
      best_component = component.score;
    }
  }
  return decision.winning_reason == expected_reason;
}

void layer_cadences_are_distinct_and_human_bounded() {
  CHECK(ai_decision_cadence(AIDecisionLayer::Strategic) == 80);
  CHECK(ai_decision_cadence(AIDecisionLayer::Tactical) == 120);
  CHECK(ai_decision_cadence(AIDecisionLayer::Micro) == 12);
  CHECK(ai_decision_due(AIDecisionLayer::Strategic, 1));
  CHECK(ai_decision_due(AIDecisionLayer::Strategic, 160));
  CHECK(ai_decision_due(AIDecisionLayer::Tactical, 30));
  CHECK(ai_decision_due(AIDecisionLayer::Tactical, 150));
  CHECK(ai_decision_due(AIDecisionLayer::Micro, 12));
  CHECK(!ai_decision_due(AIDecisionLayer::Strategic, 12));
  CHECK(!ai_decision_due(AIDecisionLayer::Tactical, 12));
  CHECK(!ai_decision_due(AIDecisionLayer::Micro, 30));
}

void strategic_layer_scores_the_opening_and_worker_allocation() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  for (const auto y : {350, 400, 450}) {
    static_cast<void>(
        simulation.spawn_entity(PlayerId::One, EntityType::Worker, world(250, y)));
  }
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'020, 400)));
  static_cast<void>(simulation.add_resource(world(390, 400), 1'200));
  simulation.step();

  const auto plan = CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  CHECK(plan.decisions.size() == 2);
  CHECK(has_action(plan, AIAction::AssignGatherers));
  CHECK(has_action(plan, AIAction::BuildBarracks));
  CHECK(std::ranges::all_of(plan.decisions, valid_score_trace));
  const auto build = std::ranges::find_if(plan.decisions, [](const AIPlannedDecision& decision) {
    return decision.selected_action == AIAction::BuildBarracks;
  });
  CHECK(build != plan.decisions.end());
  CHECK(build != plan.decisions.end() && build->winning_reason == AIUtilityReason::RequiredOpening);
  CHECK(build != plan.decisions.end() && build->command.type == CommandType::Build);
}

void tactical_layer_retreats_from_visible_superior_force() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  const auto defender =
      simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(360, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'020, 400)));
  const auto enemy_one =
      simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(540, 340));
  const auto enemy_two =
      simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(540, 400));
  const auto enemy_three =
      simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(540, 460));
  hold(simulation, PlayerId::One, {defender});
  hold(simulation, PlayerId::Two, {enemy_one, enemy_two, enemy_three});
  simulation.run(30);

  const auto plan = CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* tactical = decision_for(plan, AIDecisionLayer::Tactical);
  CHECK(tactical != nullptr);
  CHECK(tactical != nullptr && tactical->selected_action == AIAction::Retreat);
  CHECK(tactical != nullptr && tactical->winning_reason == AIUtilityReason::Outnumbered);
  CHECK(tactical != nullptr && tactical->command.type == CommandType::Retreat);
  CHECK(tactical != nullptr && valid_score_trace(*tactical));
}

void micro_layer_focuses_the_high_value_visible_target() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  const auto vanguard =
      simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(300, 380));
  const auto skirmisher =
      simulation.spawn_entity(PlayerId::One, EntityType::Skirmisher, world(330, 420));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'020, 400)));
  const auto target =
      simulation.spawn_entity(PlayerId::Two, EntityType::Skirmisher, world(520, 400));
  hold(simulation, PlayerId::One, {vanguard, skirmisher});
  hold(simulation, PlayerId::Two, {target});
  simulation.run(12);

  const auto plan = CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* micro = decision_for(plan, AIDecisionLayer::Micro);
  CHECK(micro != nullptr);
  CHECK(micro != nullptr && micro->selected_action == AIAction::FocusFire);
  CHECK(micro != nullptr && micro->command.type == CommandType::Attack);
  CHECK(micro != nullptr && micro->command.target_entity == target);
  CHECK(micro != nullptr && valid_score_trace(*micro));
}

void micro_layer_kites_while_a_ranged_weapon_cools() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  const auto skirmisher =
      simulation.spawn_entity(PlayerId::One, EntityType::Skirmisher, world(400, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'020, 400)));
  const auto pursuer =
      simulation.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(520, 400));
  hold(simulation, PlayerId::Two, {pursuer});
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Attack,
                                       .entities = {skirmisher},
                                       .target_entity = pursuer})
            .ok);
  simulation.run(12);

  const auto plan = CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* micro = decision_for(plan, AIDecisionLayer::Micro);
  CHECK(micro != nullptr);
  CHECK(micro != nullptr && micro->selected_action == AIAction::Kite);
  CHECK(micro != nullptr && micro->command.type == CommandType::Move);
  CHECK(micro != nullptr && micro->command.entities == std::vector<EntityId>{skirmisher});
  CHECK(micro != nullptr && valid_score_trace(*micro));
}

void sheltered_wounded_units_do_not_repeat_retreat_orders() {
  auto simulation = sandbox();
  static_cast<void>(simulation.spawn_entity(PlayerId::One, EntityType::Command, world(180, 400)));
  const auto survivor =
      simulation.spawn_entity(PlayerId::One, EntityType::Vanguard, world(450, 400));
  static_cast<void>(simulation.spawn_entity(PlayerId::Two, EntityType::Command, world(1'020, 400)));
  const auto attacker =
      simulation.spawn_entity(PlayerId::Two, EntityType::Skirmisher, world(600, 400));
  hold(simulation, PlayerId::One, {survivor});
  CHECK(simulation.execute_now(Command{.player = PlayerId::Two,
                                       .type = CommandType::Attack,
                                       .entities = {attacker},
                                       .target_entity = survivor})
            .ok);

  while (simulation.tick() < 200) {
    const auto* unit = simulation.find_entity(survivor);
    if (unit == nullptr || unit->hit_points * 10 <= unit->max_hit_points * 3) {
      break;
    }
    simulation.step();
  }
  const auto* wounded = simulation.find_entity(survivor);
  CHECK(wounded != nullptr && wounded->alive());
  CHECK(wounded != nullptr && wounded->hit_points * 10 <= wounded->max_hit_points * 3);
  CHECK(simulation.execute_now(
              Command{.player = PlayerId::Two, .type = CommandType::Stop, .entities = {attacker}})
            .ok);
  CHECK(simulation.execute_now(Command{.player = PlayerId::Two,
                                       .type = CommandType::Move,
                                       .entities = {attacker},
                                       .target = world(980, 400)})
            .ok);
  CHECK(simulation.execute_now(Command{.player = PlayerId::One,
                                       .type = CommandType::Retreat,
                                       .entities = {survivor}})
            .ok);
  simulation.run(100);
  while (simulation.tick() % kMicroDecisionCadence != 0) {
    simulation.step();
  }

  const auto* sheltered = simulation.find_entity(survivor);
  CHECK(sheltered != nullptr && sheltered->alive());
  CHECK(sheltered != nullptr &&
        (sheltered->order.type == OrderType::Idle || sheltered->order.type == OrderType::Hold));
  const auto plan = CommanderAI{PlayerId::One}.plan(simulation.observe(PlayerId::One));
  const auto* micro = decision_for(plan, AIDecisionLayer::Micro);
  CHECK(micro == nullptr || micro->selected_action != AIAction::Retreat);
}

}  // namespace

int main() {
  run_test("layer cadences are distinct and human bounded",
           layer_cadences_are_distinct_and_human_bounded);
  run_test("strategic layer scores the opening and worker allocation",
           strategic_layer_scores_the_opening_and_worker_allocation);
  run_test("tactical layer retreats from visible superior force",
           tactical_layer_retreats_from_visible_superior_force);
  run_test("micro layer focuses the high-value visible target",
           micro_layer_focuses_the_high_value_visible_target);
  run_test("micro layer kites while a ranged weapon cools",
           micro_layer_kites_while_a_ranged_weapon_cools);
  run_test("sheltered wounded units do not repeat retreat orders",
           sheltered_wounded_units_do_not_repeat_retreat_orders);

  if (failures != 0) {
    std::cerr << failures << " commander AI check(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All commander AI checks passed.\n";
  return EXIT_SUCCESS;
}
