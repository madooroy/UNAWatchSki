/**
 ******************************************************************************
 * @file    Theme.hpp
 * @brief   Colours shared by the screens, from the SDK's 64-colour palette
 *          (the watch's display has two bits per channel and shows these
 *          exactly).
 *
 * Colour carries meaning: white is skiing (snow), blue is a lift, grey is
 * standing, amber is weather. The UNA apps' button colours are kept: amber
 * act, green confirm, red discard, white back.
 ******************************************************************************
 */

#ifndef THEME_HPP
#define THEME_HPP

#include <cstdint>

#include "SDK/GUI/Color.hpp"

namespace Theme
{
namespace C = SDK::GUI::Color;

constexpr uint32_t kText     = C::WHITE;
constexpr uint32_t kTextDim  = C::GRAY;
constexpr uint32_t kTrack    = C::GRAY_DARK;
constexpr uint32_t kBrand    = C::CYAN_LIGHT;   // ice
constexpr uint32_t kRun      = C::WHITE;        // snow
constexpr uint32_t kLift     = C::CORNFLOWER;   // lift
constexpr uint32_t kLiftDim  = C::STEEL_DARK;
constexpr uint32_t kIdle     = C::GRAY;
constexpr uint32_t kBest     = C::YELLOW;
constexpr uint32_t kGood     = C::LIME_SOFT;
constexpr uint32_t kWarn     = C::YELLOW_DARK;  // amber
constexpr uint32_t kDanger   = C::RED;
constexpr uint32_t kStormBg  = C::SIENNA_DARK;
constexpr uint32_t kStormFg  = C::LEMON;
constexpr uint32_t kStormDim = C::CREAM;
constexpr uint32_t kSelect   = C::GRAY_DARK;

} // namespace Theme

#endif // THEME_HPP
