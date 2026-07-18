#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include <iomanip>
#include <iostream>

int main() {
  using namespace ashen::core;

  SimulationConfig config{};
  config.commander_players = {true, true};
  Simulation simulation{config};

  constexpr Tick maximum_match_ticks = 60'000;
  while (simulation.status() == MatchStatus::Playing && simulation.tick() < maximum_match_ticks) {
    simulation.step();
  }

  std::cout << "Vowfall headless bot match\n"
            << "  tick: " << simulation.tick() << '\n'
            << "  entities: " << simulation.entities().size() << '\n'
            << "  player one ore: " << simulation.player(PlayerId::One).ore << '\n'
            << "  player two ore: " << simulation.player(PlayerId::Two).ore << '\n'
            << "  winner: "
            << (simulation.winner().has_value()
                    ? (*simulation.winner() == PlayerId::One ? "player one" : "player two")
                    : "none")
            << '\n'
            << "  state hash: 0x" << std::hex << std::setw(16) << std::setfill('0')
            << simulation.state_hash() << std::dec << '\n';
  return simulation.winner().has_value() ? 0 : 1;
}
