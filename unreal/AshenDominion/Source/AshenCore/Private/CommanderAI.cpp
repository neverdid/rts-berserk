#include "ashen/core/CommanderAI.hpp"

#include "ashen/core/Catalog.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace ashen::core {
namespace {

[[nodiscard]] std::uint64_t squared_distance(const Vec2 left, const Vec2 right) noexcept {
  const auto dx = static_cast<std::int64_t>(right.x) - left.x;
  const auto dy = static_cast<std::int64_t>(right.y) - left.y;
  return static_cast<std::uint64_t>(dx * dx + dy * dy);
}

[[nodiscard]] std::uint64_t strategy_variant(std::uint64_t seed, const PlayerId player) noexcept {
  seed ^= player == PlayerId::One ? 0x9e3779b97f4a7c15ULL : 0xd1b54a32d192ed03ULL;
  seed ^= seed >> 30U;
  seed *= 0xbf58476d1ce4e5b9ULL;
  seed ^= seed >> 27U;
  seed *= 0x94d049bb133111ebULL;
  return seed ^ (seed >> 31U);
}

[[nodiscard]] const Entity* first_owned(const PlayerObservation& observation, const EntityType type,
                                        const bool completed_only = false) noexcept {
  const auto found = std::ranges::find_if(observation.owned_entities(), [=](const Entity& entity) {
    return entity.type == type && (!completed_only || !entity.under_construction);
  });
  return found == observation.owned_entities().end() ? nullptr : &*found;
}

[[nodiscard]] const Entity* first_orphaned_site(const PlayerObservation& observation,
                                                const EntityType type) noexcept {
  for (const auto& building : observation.owned_entities()) {
    if (building.type != type || !building.alive() || !building.under_construction) {
      continue;
    }
    const auto staffed = std::ranges::any_of(
        observation.owned_entities(), [site = building.id](const Entity& worker) {
          return worker.type == EntityType::Worker && worker.alive() &&
                 worker.order.type == OrderType::Build && worker.order.target_entity == site;
        });
    if (!staffed) {
      return &building;
    }
  }
  return nullptr;
}

[[nodiscard]] std::vector<EntityId> owned_units(const PlayerObservation& observation,
                                                const EntityType type) {
  std::vector<EntityId> result;
  for (const auto& entity : observation.owned_entities()) {
    if (entity.type == type && entity.alive() && !entity.under_construction) {
      result.push_back(entity.id);
    }
  }
  return result;
}

[[nodiscard]] std::vector<EntityId> army_units(const PlayerObservation& observation) {
  std::vector<EntityId> result;
  for (const auto& entity : observation.owned_entities()) {
    if ((entity.type == EntityType::Vanguard || entity.type == EntityType::Skirmisher) && entity.alive() &&
        !entity.under_construction) {
      result.push_back(entity.id);
    }
  }
  return result;
}

[[nodiscard]] const ObservedResource* nearest_usable_resource(const PlayerObservation& observation,
                                                              const Vec2 origin) noexcept {
  const ObservedResource* nearest = nullptr;
  auto nearest_distance = std::numeric_limits<std::uint64_t>::max();
  for (const auto& resource : observation.known_resources()) {
    if (resource.last_observed_amount <= 0) {
      continue;
    }
    const auto distance = squared_distance(origin, resource.position);
    if (distance < nearest_distance ||
        (distance == nearest_distance && nearest != nullptr && resource.id.value < nearest->id.value)) {
      nearest = &resource;
      nearest_distance = distance;
    }
  }
  return nearest;
}

[[nodiscard]] std::optional<ResearchId> faction_doctrine(const FactionId faction) noexcept {
  switch (faction) {
    case FactionId::Compact:
      return ResearchId::TemperedOaths;
    case FactionId::Ascendancy:
      return ResearchId::ChorusOfKnives;
    case FactionId::Concord:
      return ResearchId::VaultPlate;
  }
  return std::nullopt;
}

[[nodiscard]] const CommandCapability* first_capability(const PlayerObservation& observation,
                                                        const CommandType type,
                                                        const std::optional<EntityType> entity_type = std::nullopt,
                                                        const std::optional<ResearchId> research = std::nullopt) {
  const auto found = std::ranges::find_if(observation.capabilities(), [&](const CommandCapability& capability) {
    return capability.type == type && capability.entity_type == entity_type && capability.research == research;
  });
  return found == observation.capabilities().end() ? nullptr : &*found;
}

[[nodiscard]] Command command_for(const PlayerId player, const CommandType type) noexcept {
  Command command{};
  command.player = player;
  command.type = type;
  return command;
}

}  // namespace

std::vector<Command> CommanderAI::decide(const PlayerObservation& observation) const {
  std::vector<Command> commands;
  if (observation.player() != player_ || observation.status() != MatchStatus::Playing) {
    return commands;
  }

  const auto tick = observation.tick();
  const auto strategy = strategy_variant(observation.match_seed(), player_);
  const auto* command_building = first_owned(observation, EntityType::Command, true);
  const auto* barracks = first_owned(observation, EntityType::Barracks);
  const auto* completed_barracks = first_owned(observation, EntityType::Barracks, true);
  const auto* turret = first_owned(observation, EntityType::Turret);
  const auto* orphaned_barracks = first_orphaned_site(observation, EntityType::Barracks);
  const auto* orphaned_turret = first_orphaned_site(observation, EntityType::Turret);
  const auto workers = owned_units(observation, EntityType::Worker);
  const auto army = army_units(observation);

  if ((tick == 1 || tick % 420 == 0) && !workers.empty() && command_building != nullptr) {
    if (const auto* resource = nearest_usable_resource(observation, command_building->position)) {
      auto gather = command_for(player_, CommandType::Gather);
      for (const auto& worker : observation.owned_entities()) {
        if (worker.type == EntityType::Worker && worker.alive() && !worker.under_construction &&
            worker.order.type != OrderType::Build &&
            observation.permits(CommandType::Gather, worker.id)) {
          gather.entities.push_back(worker.id);
        }
      }
      gather.resource = resource->id;
      if (!gather.entities.empty()) {
        commands.push_back(std::move(gather));
      }
    }
  }

  if ((barracks == nullptr || orphaned_barracks != nullptr) && command_building != nullptr &&
      !workers.empty() &&
      (tick == 1 || tick % 180 == 0)) {
    const auto* capability = first_capability(observation, CommandType::Build, EntityType::Barracks);
    if (capability != nullptr) {
      const auto direction = command_building->position.x < observation.map_size().x / 2 ? 1 : -1;
      constexpr std::array<Vec2, 4> offsets = {
          Vec2{190 * kWorldScale, -135 * kWorldScale},
          Vec2{190 * kWorldScale, 135 * kWorldScale},
          Vec2{260 * kWorldScale, -95 * kWorldScale},
          Vec2{260 * kWorldScale, 95 * kWorldScale},
      };
      const auto attempt = static_cast<std::size_t>(tick / 180) % offsets.size();
      auto build = command_for(player_, CommandType::Build);
      build.entities = {capability->actor};
      if (orphaned_barracks != nullptr) {
        build.target = orphaned_barracks->position;
        build.target_entity = orphaned_barracks->id;
      } else {
        build.target = {command_building->position.x + direction * offsets[attempt].x,
                        command_building->position.y + offsets[attempt].y};
      }
      build.building_type = EntityType::Barracks;
      commands.push_back(std::move(build));
      return commands;
    }
  }

  if ((turret == nullptr || orphaned_turret != nullptr) && command_building != nullptr &&
      completed_barracks != nullptr && !workers.empty() && tick >= 900 && tick % 240 == 0) {
    const auto* capability = first_capability(observation, CommandType::Build, EntityType::Turret);
    if (capability != nullptr) {
      const auto direction = command_building->position.x < observation.map_size().x / 2 ? 1 : -1;
      auto build = command_for(player_, CommandType::Build);
      build.entities = {capability->actor};
      if (orphaned_turret != nullptr) {
        build.target = orphaned_turret->position;
        build.target_entity = orphaned_turret->id;
      } else {
        build.target = {command_building->position.x + direction * 330 * kWorldScale,
                        command_building->position.y + direction * 175 * kWorldScale};
      }
      build.building_type = EntityType::Turret;
      commands.push_back(std::move(build));
      return commands;
    }
  }

  if (barracks != nullptr && tick > 0 && tick % 160 == 0) {
    if (const auto* capability =
            first_capability(observation, CommandType::Research, std::nullopt, ResearchId::TierTwo)) {
      auto research = command_for(player_, CommandType::Research);
      research.producer = capability->actor;
      research.research = ResearchId::TierTwo;
      commands.push_back(std::move(research));
      return commands;
    }

    if (const auto doctrine = faction_doctrine(observation.self().faction)) {
      if (const auto* capability =
              first_capability(observation, CommandType::Research, std::nullopt, doctrine)) {
        auto research = command_for(player_, CommandType::Research);
        research.producer = capability->actor;
        research.research = *doctrine;
        commands.push_back(std::move(research));
        return commands;
      }
    }

    if (barracks != nullptr && workers.size() < 5) {
      if (const auto* capability = first_capability(observation, CommandType::Train, EntityType::Worker)) {
        auto train = command_for(player_, CommandType::Train);
        train.producer = capability->actor;
        train.train_type = EntityType::Worker;
        commands.push_back(std::move(train));
        return commands;
      }
    }

    const auto army_type = (tick / 160 + strategy) % 3 == 0 ? EntityType::Skirmisher
                                                            : EntityType::Vanguard;
    if (const auto* capability = first_capability(observation, CommandType::Train, army_type)) {
      auto train = command_for(player_, CommandType::Train);
      train.producer = capability->actor;
      train.train_type = army_type;
      commands.push_back(std::move(train));
      return commands;
    }
  }

  if (tick >= 900 && tick % 480 == 0 && army.size() >= 4 && !observation.public_objectives().empty()) {
    const ObservedControlPoint* target = nullptr;
    for (const auto& objective : observation.public_objectives()) {
      if (objective.has_observed_state && objective.last_observed_owner != player_) {
        target = &objective;
        break;
      }
    }
    if (target == nullptr) {
      const auto index = static_cast<std::size_t>(tick / 480 + strategy) %
                         observation.public_objectives().size();
      target = &observation.public_objectives()[index];
    }
    auto capture = command_for(player_, CommandType::AttackMove);
    capture.entities = army;
    capture.target = target->position;
    commands.push_back(std::move(capture));
  }

  if (tick >= 1'200 && tick % 240 == 0 && army.size() >= 3 &&
      observation.permits(CommandType::ActivatePower)) {
    commands.push_back(command_for(player_, CommandType::ActivatePower));
    return commands;
  }

  if (tick >= 2'400 && (tick - 2'400) % 640 == 0 && army.size() >= 3) {
    const ObservedEnemy* known_command = nullptr;
    for (const auto& enemy : observation.known_enemies()) {
      if (enemy.type == EntityType::Command &&
          (known_command == nullptr || (enemy.currently_visible && !known_command->currently_visible) ||
           (enemy.currently_visible == known_command->currently_visible &&
            enemy.last_observed_tick > known_command->last_observed_tick))) {
        known_command = &enemy;
      }
    }

    if (known_command != nullptr && known_command->currently_visible) {
      auto assault = command_for(player_, CommandType::Attack);
      assault.entities = army;
      assault.target_entity = known_command->id;
      commands.push_back(std::move(assault));
    } else {
      auto assault = command_for(player_, CommandType::AttackMove);
      assault.entities = army;
      if (known_command != nullptr) {
        assault.target = known_command->position;
      } else if (command_building != nullptr) {
        assault.target = {observation.map_size().x - command_building->position.x,
                          command_building->position.y};
      } else {
        assault.target = {observation.map_size().x / 2, observation.map_size().y / 2};
      }
      commands.push_back(std::move(assault));
    }
  }

  return commands;
}

}  // namespace ashen::core
