#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ashen::core;

constexpr std::array kFactions{FactionId::Compact, FactionId::Ascendancy, FactionId::Concord};
constexpr std::array kEntityTypes{EntityType::Worker, EntityType::Vanguard, EntityType::Skirmisher,
                                  EntityType::Command, EntityType::Barracks, EntityType::Turret};

[[nodiscard]] std::string json_string(const std::string_view value) {
  std::string encoded{"\""};
  for (const char character : value) {
    switch (character) {
      case '\"':
        encoded += "\\\"";
        break;
      case '\\':
        encoded += "\\\\";
        break;
      case '\n':
        encoded += "\\n";
        break;
      case '\r':
        encoded += "\\r";
        break;
      case '\t':
        encoded += "\\t";
        break;
      default:
        encoded += character;
        break;
    }
  }
  encoded += '\"';
  return encoded;
}

[[nodiscard]] std::string_view faction_token(const FactionId faction) {
  switch (faction) {
    case FactionId::Compact:
      return "compact";
    case FactionId::Ascendancy:
      return "ascendancy";
    case FactionId::Concord:
      return "concord";
  }
  throw std::runtime_error{"Unknown faction value."};
}

[[nodiscard]] FactionId parse_faction(const std::string_view value) {
  if (value == "compact") {
    return FactionId::Compact;
  }
  if (value == "ascendancy") {
    return FactionId::Ascendancy;
  }
  if (value == "concord") {
    return FactionId::Concord;
  }
  throw std::runtime_error{"Unknown faction token: " + std::string{value}};
}

[[nodiscard]] std::string_view entity_type_token(const EntityType type) {
  switch (type) {
    case EntityType::Worker:
      return "worker";
    case EntityType::Vanguard:
      return "vanguard";
    case EntityType::Skirmisher:
      return "skirmisher";
    case EntityType::Command:
      return "command";
    case EntityType::Barracks:
      return "barracks";
    case EntityType::Turret:
      return "turret";
  }
  throw std::runtime_error{"Unknown entity type value."};
}

[[nodiscard]] EntityType parse_entity_type(const std::string_view value) {
  for (const auto type : kEntityTypes) {
    if (entity_type_token(type) == value) {
      return type;
    }
  }
  throw std::runtime_error{"Unknown entity type token: " + std::string{value}};
}

[[nodiscard]] std::string_view entity_kind_token(const EntityKind kind) {
  return kind == EntityKind::Unit ? "unit" : "building";
}

[[nodiscard]] std::string_view armor_token(const ArmorClass armor) {
  switch (armor) {
    case ArmorClass::Laborer:
      return "laborer";
    case ArmorClass::Armored:
      return "armored";
    case ArmorClass::Light:
      return "light";
    case ArmorClass::Structure:
      return "structure";
  }
  throw std::runtime_error{"Unknown armor value."};
}

[[nodiscard]] std::string_view order_token(const OrderType order) {
  switch (order) {
    case OrderType::Idle:
      return "idle";
    case OrderType::Move:
      return "move";
    case OrderType::Attack:
      return "attack";
    case OrderType::AttackMove:
      return "attack-move";
    case OrderType::Gather:
      return "gather";
    case OrderType::Build:
      return "build";
    case OrderType::Patrol:
      return "patrol";
    case OrderType::Hold:
      return "hold";
  }
  throw std::runtime_error{"Unknown order value."};
}

[[nodiscard]] std::string_view research_token(const ResearchId research) {
  switch (research) {
    case ResearchId::TierTwo:
      return "tier-two";
    case ResearchId::TemperedOaths:
      return "tempered-oaths";
    case ResearchId::Wardcraft:
      return "wardcraft";
    case ResearchId::ChorusOfKnives:
      return "chorus-of-knives";
    case ResearchId::PitBroods:
      return "pit-broods";
    case ResearchId::VaultPlate:
      return "vault-plate";
    case ResearchId::SiegeLiturgy:
      return "siege-liturgy";
  }
  throw std::runtime_error{"Unknown research value."};
}

[[nodiscard]] ResearchId parse_research(const std::string_view value) {
  constexpr std::array values{
      ResearchId::TierTwo,       ResearchId::TemperedOaths, ResearchId::Wardcraft,
      ResearchId::ChorusOfKnives, ResearchId::PitBroods,     ResearchId::VaultPlate,
      ResearchId::SiegeLiturgy,
  };
  for (const auto research : values) {
    if (research_token(research) == value) {
      return research;
    }
  }
  throw std::runtime_error{"Unknown research token: " + std::string{value}};
}

[[nodiscard]] std::string_view status_token(const MatchStatus status) {
  switch (status) {
    case MatchStatus::Playing:
      return "playing";
    case MatchStatus::Won:
      return "won";
    case MatchStatus::Lost:
      return "lost";
  }
  throw std::runtime_error{"Unknown match status value."};
}

[[nodiscard]] PlayerId parse_player(const int value) {
  if (value == 1) {
    return PlayerId::One;
  }
  if (value == 2) {
    return PlayerId::Two;
  }
  throw std::runtime_error{"Player must be 1 or 2."};
}

[[nodiscard]] int player_number(const PlayerId player) {
  return player == PlayerId::One ? 1 : 2;
}

void require_end(std::istringstream& input) {
  std::string extra;
  if (input >> extra) {
    throw std::runtime_error{"Unexpected trailing token: " + extra};
  }
}

template <typename Value>
[[nodiscard]] Value read(std::istringstream& input, const std::string_view label) {
  Value value{};
  if (!(input >> value)) {
    throw std::runtime_error{"Missing or invalid " + std::string{label} + '.'};
  }
  return value;
}

[[nodiscard]] std::vector<std::string> split_aliases(const std::string& value) {
  std::vector<std::string> aliases;
  std::istringstream input{value};
  std::string alias;
  while (std::getline(input, alias, ',')) {
    if (alias.empty()) {
      throw std::runtime_error{"Entity alias lists cannot contain empty entries."};
    }
    aliases.push_back(alias);
  }
  if (aliases.empty()) {
    throw std::runtime_error{"At least one entity alias is required."};
  }
  return aliases;
}

class ParityRunner final {
 public:
  void apply_line(const std::string& line, const std::size_t line_number) {
    try {
      std::istringstream input{line};
      const auto instruction = read<std::string>(input, "instruction");
      if (instruction.starts_with('#')) {
        return;
      }
      if (instruction == "version") {
        const auto version = read<int>(input, "protocol version");
        require_end(input);
        if (version != 1 || version_seen_) {
          throw std::runtime_error{"Expected the protocol declaration 'version 1' exactly once."};
        }
        version_seen_ = true;
        return;
      }
      if (!version_seen_) {
        throw std::runtime_error{"The first instruction must be 'version 1'."};
      }
      if (instruction == "config") {
        apply_config(input);
      } else if (instruction == "entity") {
        apply_entity(input);
      } else if (instruction == "resource") {
        apply_resource(input);
      } else if (instruction == "control") {
        apply_control(input);
      } else if (instruction == "move") {
        apply_move(input);
      } else if (instruction == "attack") {
        apply_attack(input);
      } else if (instruction == "attack-move") {
        apply_attack_move(input);
      } else if (instruction == "gather") {
        apply_gather(input);
      } else if (instruction == "train") {
        apply_train(input);
      } else if (instruction == "rally") {
        apply_rally(input);
      } else if (instruction == "build") {
        apply_build(input);
      } else if (instruction == "research") {
        apply_research(input);
      } else if (instruction == "power") {
        apply_power(input);
      } else if (instruction == "run") {
        apply_run(input);
      } else {
        throw std::runtime_error{"Unknown instruction: " + instruction};
      }
    } catch (const std::exception& exception) {
      throw std::runtime_error{"line " + std::to_string(line_number) + ": " + exception.what()};
    }
  }

  void write_snapshot(std::ostream& output) const {
    const auto& current = simulation();
    output << "{\"tick\":" << current.tick() << ",\"status\":" << json_string(status_token(current.status()))
           << ",\"winner\":";
    if (current.winner().has_value()) {
      output << player_number(*current.winner());
    } else {
      output << "null";
    }

    output << ",\"results\":[";
    for (std::size_t index = 0; index < results_.size(); ++index) {
      output << (index == 0 ? "" : ",") << (results_[index] ? "true" : "false");
    }
    output << "],\"players\":{";
    for (const auto player : {PlayerId::One, PlayerId::Two}) {
      const auto& state = current.player(player);
      output << (player == PlayerId::One ? "" : ",") << json_string(std::to_string(player_number(player)))
             << ":{\"faction\":" << json_string(faction_token(state.faction)) << ",\"ore\":" << state.ore
             << ",\"supplyUsed\":" << state.supply_used << ",\"supplyCap\":" << state.supply_cap
             << ",\"resolve\":" << state.resolve << ",\"powerCooldownTicks\":"
             << state.power_cooldown_ticks << ",\"techTier\":" << static_cast<int>(state.tech_tier)
             << ",\"researched\":[";
      bool first_research = true;
      for (std::size_t index = 0; index < kResearchCount; ++index) {
        if (!state.researched[index]) {
          continue;
        }
        output << (first_research ? "" : ",")
               << json_string(research_token(static_cast<ResearchId>(index)));
        first_research = false;
      }
      output << "]}";
    }

    output << "},\"entities\":{";
    bool first = true;
    for (const auto& [alias, id] : entity_aliases_) {
      output << (first ? "" : ",") << json_string(alias) << ':';
      first = false;
      const auto* entity = current.find_entity(id);
      if (entity == nullptr) {
        output << "{\"alive\":false}";
        continue;
      }
      output << "{\"alive\":true,\"owner\":" << player_number(entity->owner) << ",\"type\":"
             << json_string(entity_type_token(entity->type)) << ",\"xMilli\":" << entity->position.x
             << ",\"yMilli\":" << entity->position.y << ",\"hpMilli\":"
             << static_cast<std::int64_t>(entity->hit_points) * kWorldScale << ",\"order\":"
             << json_string(order_token(entity->order.type)) << ",\"carrying\":" << entity->carrying
             << ",\"resolve\":" << entity->resolve << ",\"underConstruction\":"
             << (entity->under_construction ? "true" : "false") << ",\"constructionProgressBasis\":"
             << (entity->under_construction && entity->construction_total_ticks > 0
                     ? entity->construction_ticks * 10'000 / entity->construction_total_ticks
                     : 10'000)
             << ",\"queueCount\":" << entity->production_queue.size() << ",\"visibleToOne\":"
             << (current.is_entity_visible_to(*entity, PlayerId::One) ? "true" : "false") << '}';
    }

    output << "},\"counts\":{";
    for (const auto player : {PlayerId::One, PlayerId::Two}) {
      output << (player == PlayerId::One ? "" : ",") << json_string(std::to_string(player_number(player))) << ":{";
      for (std::size_t type_index = 0; type_index < kEntityTypes.size(); ++type_index) {
        const auto type = kEntityTypes[type_index];
        std::size_t count = 0;
        for (const auto& entity : current.entities()) {
          if (entity.alive() && entity.owner == player && entity.type == type) {
            ++count;
          }
        }
        output << (type_index == 0 ? "" : ",") << json_string(entity_type_token(type)) << ':' << count;
      }
      output << '}';
    }

    output << "},\"resources\":{";
    first = true;
    for (const auto& [alias, id] : resource_aliases_) {
      output << (first ? "" : ",") << json_string(alias) << ':';
      first = false;
      const auto* resource = current.find_resource(id);
      if (resource == nullptr) {
        output << "null";
      } else {
        output << "{\"amount\":" << resource->amount << '}';
      }
    }
    output << "},\"controls\":{";
    first = true;
    for (const auto& [alias, id] : control_aliases_) {
      output << (first ? "" : ",") << json_string(alias) << ':';
      first = false;
      const auto* point = current.find_control_point(id);
      if (point == nullptr) {
        output << "null";
        continue;
      }
      output << "{\"owner\":";
      if (point->owner.has_value()) {
        output << player_number(*point->owner);
      } else {
        output << "null";
      }
      output << ",\"influence\":" << point->influence << '}';
    }
    output << "},\"ruinTide\":" << current.ruin_tide() << "}\n";
  }

 private:
  void apply_config(std::istringstream& input) {
    if (simulation_.has_value()) {
      throw std::runtime_error{"A scenario can only contain one config instruction."};
    }
    const auto one = parse_faction(read<std::string>(input, "player-one faction"));
    const auto two = parse_faction(read<std::string>(input, "player-two faction"));
    require_end(input);
    SimulationConfig config{};
    config.mode = MatchMode::PvP;
    config.player_one_faction = one;
    config.player_two_faction = two;
    config.map_size = world(1'200, 800);
    config.navigation_obstacles.clear();
    config.seed_starting_forces = false;
    simulation_.emplace(config);
  }

  void apply_entity(std::istringstream& input) {
    const auto alias = read<std::string>(input, "entity alias");
    const auto player = parse_player(read<int>(input, "owner"));
    const auto type = parse_entity_type(read<std::string>(input, "entity type"));
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    require_end(input);
    if (alias_exists(alias)) {
      throw std::runtime_error{"Duplicate alias: " + alias};
    }
    entity_aliases_.emplace(alias, simulation().spawn_entity(player, type, world(x, y)));
  }

  void apply_resource(std::istringstream& input) {
    const auto alias = read<std::string>(input, "resource alias");
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto amount = read<std::int32_t>(input, "amount");
    const auto radius = read<std::int32_t>(input, "radius");
    require_end(input);
    if (alias_exists(alias)) {
      throw std::runtime_error{"Duplicate alias: " + alias};
    }
    resource_aliases_.emplace(alias, simulation().add_resource(world(x, y), amount, world(radius, 0).x));
  }

  void apply_control(std::istringstream& input) {
    const auto alias = read<std::string>(input, "control-point alias");
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto radius = read<std::int32_t>(input, "radius");
    require_end(input);
    if (alias_exists(alias)) {
      throw std::runtime_error{"Duplicate alias: " + alias};
    }
    control_aliases_.emplace(alias, simulation().add_control_point(world(x, y), world(radius, 0).x));
  }

  void apply_move(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto entities = entity_ids(read<std::string>(input, "entity aliases"));
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto target = world(x, y);
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::Move, .entities = entities, .target = target}));
  }

  void apply_attack(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto entities = entity_ids(read<std::string>(input, "entity aliases"));
    const auto target = entity_id(read<std::string>(input, "target alias"));
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::Attack, .entities = entities, .target_entity = target}));
  }

  void apply_attack_move(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto entities = entity_ids(read<std::string>(input, "entity aliases"));
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto target = world(x, y);
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::AttackMove, .entities = entities, .target = target}));
  }

  void apply_gather(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto entities = entity_ids(read<std::string>(input, "entity aliases"));
    const auto resource = resource_id(read<std::string>(input, "resource alias"));
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::Gather, .entities = entities, .resource = resource}));
  }

  void apply_train(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto producer = entity_id(read<std::string>(input, "producer alias"));
    const auto type = parse_entity_type(read<std::string>(input, "unit type"));
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::Train, .producer = producer, .train_type = type}));
  }

  void apply_rally(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto producer = entity_id(read<std::string>(input, "producer alias"));
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto target = world(x, y);
    require_end(input);
    record(simulation().execute_now(
        Command{.player = player, .type = CommandType::SetRallyPoint, .target = target, .producer = producer}));
  }

  void apply_build(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto worker = entity_id(read<std::string>(input, "worker alias"));
    const auto type = parse_entity_type(read<std::string>(input, "building type"));
    const auto x = read<std::int32_t>(input, "x coordinate");
    const auto y = read<std::int32_t>(input, "y coordinate");
    const auto alias = read<std::string>(input, "new building alias");
    require_end(input);
    if (alias_exists(alias)) {
      throw std::runtime_error{"Duplicate alias: " + alias};
    }
    const auto result = simulation().execute_now(Command{.player = player,
                                                          .type = CommandType::Build,
                                                          .entities = {worker},
                                                          .target = world(x, y),
                                                          .building_type = type});
    record(result);
    if (!result.ok) {
      return;
    }
    const auto newest = std::ranges::max_element(simulation().entities(), {}, &Entity::id);
    if (newest == simulation().entities().end()) {
      throw std::runtime_error{"A successful build command did not create a construction site."};
    }
    entity_aliases_.emplace(alias, newest->id);
  }

  void apply_research(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    const auto producer = entity_id(read<std::string>(input, "producer alias"));
    const auto research = parse_research(read<std::string>(input, "research"));
    require_end(input);
    record(simulation().execute_now(Command{.player = player,
                                             .type = CommandType::Research,
                                             .producer = producer,
                                             .research = research}));
  }

  void apply_power(std::istringstream& input) {
    const auto player = parse_player(read<int>(input, "player"));
    require_end(input);
    record(simulation().execute_now(Command{.player = player, .type = CommandType::ActivatePower}));
  }

  void apply_run(std::istringstream& input) {
    const auto ticks = read<Tick>(input, "tick count");
    require_end(input);
    simulation().run(ticks);
  }

  [[nodiscard]] Simulation& simulation() {
    if (!simulation_.has_value()) {
      throw std::runtime_error{"A config instruction is required before scenario instructions."};
    }
    return *simulation_;
  }

  [[nodiscard]] const Simulation& simulation() const {
    if (!simulation_.has_value()) {
      throw std::runtime_error{"A config instruction is required before writing a snapshot."};
    }
    return *simulation_;
  }

  [[nodiscard]] EntityId entity_id(const std::string& alias) const {
    const auto found = entity_aliases_.find(alias);
    if (found == entity_aliases_.end()) {
      throw std::runtime_error{"Unknown entity alias: " + alias};
    }
    return found->second;
  }

  [[nodiscard]] std::vector<EntityId> entity_ids(const std::string& aliases) const {
    std::vector<EntityId> ids;
    for (const auto& alias : split_aliases(aliases)) {
      ids.push_back(entity_id(alias));
    }
    return ids;
  }

  [[nodiscard]] ResourceId resource_id(const std::string& alias) const {
    const auto found = resource_aliases_.find(alias);
    if (found == resource_aliases_.end()) {
      throw std::runtime_error{"Unknown resource alias: " + alias};
    }
    return found->second;
  }

  [[nodiscard]] bool alias_exists(const std::string& alias) const {
    return entity_aliases_.contains(alias) || resource_aliases_.contains(alias) || control_aliases_.contains(alias);
  }

  void record(const CommandResult result) { results_.push_back(result.ok); }

  bool version_seen_{};
  std::optional<Simulation> simulation_{};
  std::map<std::string, EntityId> entity_aliases_{};
  std::map<std::string, ResourceId> resource_aliases_{};
  std::map<std::string, ControlPointId> control_aliases_{};
  std::vector<bool> results_{};
};

void write_catalog(std::ostream& output) {
  output << '[';
  bool first = true;
  for (const auto faction : kFactions) {
    const auto faction_definition_value = faction_definition(faction);
    for (const auto type : kEntityTypes) {
      const auto definition = entity_definition(faction, type);
      output << (first ? "" : ",") << "{\"faction\":" << json_string(faction_token(faction))
             << ",\"factionName\":" << json_string(faction_definition_value.name)
             << ",\"incomeBasisPoints\":" << faction_definition_value.income_basis_points
             << ",\"resolveDrift\":" << faction_definition_value.resolve_drift
             << ",\"type\":" << json_string(entity_type_token(type)) << ",\"kind\":"
             << json_string(entity_kind_token(definition.kind)) << ",\"label\":" << json_string(definition.label)
             << ",\"cost\":" << definition.cost << ",\"buildTicks\":" << definition.build_ticks
             << ",\"hitPoints\":" << definition.hit_points << ",\"radiusMilli\":" << definition.radius
             << ",\"speedMilliPerTick\":" << definition.speed_per_tick << ",\"rangeMilli\":"
             << definition.attack_range << ",\"damage\":" << definition.damage << ",\"cooldownTicks\":"
             << definition.attack_cooldown_ticks << ",\"sightMilli\":" << definition.sight
             << ",\"terror\":" << definition.terror << ",\"ward\":" << definition.ward
             << ",\"armor\":" << json_string(armor_token(definition.armor))
             << ",\"bonusAgainst\":";
      if (definition.has_damage_bonus) {
        output << json_string(armor_token(definition.bonus_against));
      } else {
        output << "null";
      }
      output << ",\"bonusDamage\":" << definition.bonus_damage << ",\"supplyCost\":"
             << definition.supply_cost << ",\"supplyProvided\":" << definition.supply_provided << '}';
      first = false;
    }
  }
  output << "]\n";
}

}  // namespace

int main(const int argument_count, const char* const* arguments) {
  try {
    if (argument_count == 2 && std::string_view{arguments[1]} == "--catalog") {
      write_catalog(std::cout);
      return EXIT_SUCCESS;
    }
    if (argument_count != 1) {
      throw std::runtime_error{"Usage: ashen_parity_runner [--catalog]"};
    }

    ParityRunner runner;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(std::cin, line)) {
      ++line_number;
      if (line.empty()) {
        continue;
      }
      runner.apply_line(line, line_number);
    }
    runner.write_snapshot(std::cout);
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "Parity runner failed: " << exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
