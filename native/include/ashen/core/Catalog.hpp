#pragma once

#include "ashen/core/Types.hpp"

#include <string_view>

namespace ashen::core {

[[nodiscard]] FactionDefinition faction_definition(FactionId faction) noexcept;
[[nodiscard]] EntityDefinition entity_definition(FactionId faction, EntityType type) noexcept;
[[nodiscard]] bool can_train(EntityType producer, EntityType unit) noexcept;
[[nodiscard]] bool is_unit(EntityType type) noexcept;
[[nodiscard]] std::string_view to_string(FactionId faction) noexcept;
[[nodiscard]] std::string_view to_string(EntityType type) noexcept;

}  // namespace ashen::core
