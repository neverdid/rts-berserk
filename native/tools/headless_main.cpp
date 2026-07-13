#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
  using namespace ashen::core;

  Simulation simulation{};
  std::array<std::vector<EntityId>, 2> workers;
  for (const auto& entity : simulation.entities()) {
    if (entity.type == EntityType::Worker) {
      workers[player_index(entity.owner)].push_back(entity.id);
    }
  }

  simulation.enqueue(Command{.player = PlayerId::One,
                             .type = CommandType::Gather,
                             .entities = workers[0],
                             .resource = simulation.resources()[0].id});
  simulation.enqueue(Command{.player = PlayerId::Two,
                             .type = CommandType::Gather,
                             .entities = workers[1],
                             .resource = simulation.resources()[1].id});
  simulation.run(1'200);

  std::cout << "Ashen Dominion headless simulation\n"
            << "  tick: " << simulation.tick() << '\n'
            << "  entities: " << simulation.entities().size() << '\n'
            << "  player one ore: " << simulation.player(PlayerId::One).ore << '\n'
            << "  player two ore: " << simulation.player(PlayerId::Two).ore << '\n'
            << "  state hash: 0x" << std::hex << std::setw(16) << std::setfill('0')
            << simulation.state_hash() << std::dec << '\n';
  return 0;
}
