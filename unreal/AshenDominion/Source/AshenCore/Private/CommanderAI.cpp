#include "ashen/core/CommanderAI.hpp"

#include "AIPlanningInternal.hpp"

#include <utility>
#include <vector>

namespace ashen::core {

CommanderPlan CommanderAI::plan(const PlayerObservation& observation) const {
  CommanderPlan result{};
  if (observation.player() != player_ || observation.status() != MatchStatus::Playing) {
    return result;
  }

  const ai::PlanningContext context{observation};
  if (ai_decision_due(AIDecisionLayer::Strategic, observation.tick())) {
    auto strategic = ai::evaluate_strategic_layer(context);
    for (auto& decision : strategic) {
      result.decisions.push_back(std::move(decision));
    }
  }
  if (ai_decision_due(AIDecisionLayer::Tactical, observation.tick())) {
    if (auto tactical = ai::evaluate_tactical_layer(context)) {
      result.decisions.push_back(std::move(*tactical));
    }
  }
  if (ai_decision_due(AIDecisionLayer::Micro, observation.tick())) {
    if (auto micro = ai::evaluate_micro_layer(context)) {
      result.decisions.push_back(std::move(*micro));
    }
  }
  return result;
}

std::vector<Command> CommanderAI::decide(const PlayerObservation& observation) const {
  auto planned = plan(observation);
  std::vector<Command> commands;
  commands.reserve(planned.decisions.size());
  for (auto& decision : planned.decisions) {
    commands.push_back(std::move(decision.command));
  }
  return commands;
}

}  // namespace ashen::core
