#pragma once

#include "ashen/core/AIDecision.hpp"
#include "ashen/core/PlayerObservation.hpp"

#include <vector>

namespace ashen::core {

// CommanderAI deliberately has no Simulation dependency. Its only input is a sanitized observation and
// its only output is the same Command value a human player submits.
class ASHENCORE_API CommanderAI final {
 public:
  explicit constexpr CommanderAI(const PlayerId player) noexcept : player_(player) {}

  [[nodiscard]] PlayerId player() const noexcept { return player_; }
  [[nodiscard]] CommanderPlan plan(const PlayerObservation& observation) const;
  [[nodiscard]] std::vector<Command> decide(const PlayerObservation& observation) const;

 private:
  PlayerId player_{PlayerId::One};
};

}  // namespace ashen::core
