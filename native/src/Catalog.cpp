#include "ashen/core/Catalog.hpp"

namespace ashen::core {
namespace {

[[nodiscard]] EntityDefinition base_definition(const EntityType type) noexcept {
  switch (type) {
    case EntityType::Worker:
      return {type, EntityKind::Unit, "Grave Serf", 45, 120, 42, 13'000, 4'800, 26'000, 3, 22,
              ArmorClass::Laborer, ArmorClass::Laborer, false, 0, 1, 0};
    case EntityType::Vanguard:
      return {type, EntityKind::Unit, "Oathbound", 70, 160, 78, 15'000, 4'400, 28'000, 11, 19,
              ArmorClass::Armored, ArmorClass::Structure, true, 4, 2, 0};
    case EntityType::Skirmisher:
      return {type, EntityKind::Unit, "Ashen Arbalest", 95, 200, 58, 14'000, 6'000, 142'000, 8, 14,
              ArmorClass::Light, ArmorClass::Armored, true, 6, 2, 0};
    case EntityType::Command:
      return {type, EntityKind::Building, "Candle Keep", 0, 0, 720, 46'000, 0, 0, 0, 0,
              ArmorClass::Structure, ArmorClass::Laborer, false, 0, 0, 14};
    case EntityType::Barracks:
      return {type, EntityKind::Building, "Oath Hall", 160, 280, 430, 36'000, 0, 0, 0, 0,
              ArmorClass::Structure, ArmorClass::Laborer, false, 0, 0, 8};
    case EntityType::Turret:
      return {type, EntityKind::Building, "Warding Pyre", 120, 220, 260, 27'000, 0, 172'000, 13, 16,
              ArmorClass::Structure, ArmorClass::Light, true, 5, 0, 0};
  }
  return {};
}

void apply_hollow_overrides(EntityDefinition& definition) noexcept {
  switch (definition.type) {
    case EntityType::Worker:
      definition.label = "Mute Thrall";
      definition.cost = 36;
      definition.build_ticks = 96;
      definition.hit_points = 34;
      definition.speed_per_tick = 5'400;
      break;
    case EntityType::Vanguard:
      definition.label = "Choir Husk";
      definition.cost = 54;
      definition.build_ticks = 130;
      definition.hit_points = 56;
      definition.speed_per_tick = 5'300;
      definition.damage = 9;
      definition.attack_cooldown_ticks = 16;
      definition.supply_cost = 1;
      break;
    case EntityType::Skirmisher:
      definition.label = "Bellwraith";
      definition.cost = 82;
      definition.build_ticks = 168;
      definition.hit_points = 44;
      definition.speed_per_tick = 6'800;
      definition.attack_range = 150'000;
      definition.damage = 7;
      break;
    case EntityType::Command:
      definition.label = "Choir Spire";
      definition.hit_points = 630;
      break;
    case EntityType::Barracks:
      definition.label = "Vesper Pit";
      definition.cost = 138;
      definition.build_ticks = 230;
      definition.hit_points = 350;
      definition.supply_provided = 10;
      break;
    case EntityType::Turret:
      definition.label = "Tolling Stake";
      definition.cost = 105;
      definition.build_ticks = 180;
      definition.hit_points = 205;
      definition.damage = 11;
      definition.attack_cooldown_ticks = 12;
      break;
  }
}

void apply_sepulcher_overrides(EntityDefinition& definition) noexcept {
  switch (definition.type) {
    case EntityType::Worker:
      definition.label = "Reliquary Mason";
      definition.cost = 55;
      definition.build_ticks = 144;
      definition.hit_points = 58;
      definition.speed_per_tick = 4'100;
      break;
    case EntityType::Vanguard:
      definition.label = "Iron Penitent";
      definition.cost = 92;
      definition.build_ticks = 210;
      definition.hit_points = 108;
      definition.speed_per_tick = 3'700;
      definition.damage = 15;
      definition.attack_cooldown_ticks = 22;
      definition.bonus_damage = 7;
      definition.supply_cost = 3;
      break;
    case EntityType::Skirmisher:
      definition.label = "Vault Arbalist";
      definition.cost = 116;
      definition.build_ticks = 240;
      definition.hit_points = 72;
      definition.speed_per_tick = 4'800;
      definition.attack_range = 154'000;
      definition.damage = 11;
      definition.attack_cooldown_ticks = 18;
      definition.supply_cost = 3;
      break;
    case EntityType::Command:
      definition.label = "Sepulcher Gate";
      definition.hit_points = 860;
      break;
    case EntityType::Barracks:
      definition.label = "Reliquary Foundry";
      definition.cost = 190;
      definition.build_ticks = 340;
      definition.hit_points = 560;
      definition.supply_provided = 6;
      break;
    case EntityType::Turret:
      definition.label = "Grave Lantern";
      definition.cost = 145;
      definition.build_ticks = 260;
      definition.hit_points = 340;
      definition.damage = 17;
      definition.attack_cooldown_ticks = 20;
      break;
  }
}

}  // namespace

FactionDefinition faction_definition(const FactionId faction) noexcept {
  switch (faction) {
    case FactionId::Candlebound:
      return {faction, "Candlebound Remnant", 10'000};
    case FactionId::Hollow:
      return {faction, "Hollow Choir", 10'800};
    case FactionId::Sepulcher:
      return {faction, "Sepulcher Host", 9'400};
  }
  return {};
}

EntityDefinition entity_definition(const FactionId faction, const EntityType type) noexcept {
  auto definition = base_definition(type);
  if (faction == FactionId::Hollow) {
    apply_hollow_overrides(definition);
  } else if (faction == FactionId::Sepulcher) {
    apply_sepulcher_overrides(definition);
  }
  return definition;
}

bool can_train(const EntityType producer, const EntityType unit) noexcept {
  return (producer == EntityType::Command && unit == EntityType::Worker) ||
         (producer == EntityType::Barracks &&
          (unit == EntityType::Vanguard || unit == EntityType::Skirmisher));
}

bool is_unit(const EntityType type) noexcept {
  return type == EntityType::Worker || type == EntityType::Vanguard || type == EntityType::Skirmisher;
}

std::string_view to_string(const FactionId faction) noexcept {
  return faction_definition(faction).name;
}

std::string_view to_string(const EntityType type) noexcept {
  return base_definition(type).label;
}

}  // namespace ashen::core
