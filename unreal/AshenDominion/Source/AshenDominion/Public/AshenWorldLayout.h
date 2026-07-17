#pragma once

namespace Ashen::WorldLayout
{
inline constexpr float Width = 4'800.0f;
inline constexpr float Height = 2'800.0f;
inline constexpr float CenterX = Width * 0.5f;
inline constexpr float CenterY = Height * 0.5f;
inline constexpr float HumanBaseX = 600.0f;
inline constexpr float MonsterBaseX = Width - HumanBaseX;
inline constexpr float NorthCrossingY = 760.0f;
inline constexpr float CentralCrossingY = CenterY;
inline constexpr float SouthCrossingY = Height - NorthCrossingY;
inline constexpr float CameraMarginX = 320.0f;
inline constexpr float CameraMarginY = 240.0f;
} // namespace Ashen::WorldLayout
