#pragma once

#include "ashen/core/Types.hpp"

#include <string_view>

namespace ashen::core {

[[nodiscard]] ASHENCORE_API FactionDefinition faction_definition(FactionId faction) noexcept;
[[nodiscard]] ASHENCORE_API EntityDefinition entity_definition(FactionId faction, EntityType type) noexcept;
[[nodiscard]] ASHENCORE_API ResearchDefinition research_definition(ResearchId research) noexcept;
[[nodiscard]] ASHENCORE_API PowerDefinition power_definition(FactionId faction) noexcept;
[[nodiscard]] ASHENCORE_API bool can_train(EntityType producer, EntityType unit) noexcept;
[[nodiscard]] ASHENCORE_API bool is_unit(EntityType type) noexcept;
[[nodiscard]] ASHENCORE_API bool is_building(EntityType type) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(FactionId faction) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(EntityType type) noexcept;
[[nodiscard]] ASHENCORE_API std::string_view to_string(ResearchId research) noexcept;

}  // namespace ashen::core
