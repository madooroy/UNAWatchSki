/**
 ******************************************************************************
 * @file    Format.hpp
 * @brief   Numbers as the screens show them, metric or imperial (the watch's
 *          own setting decides). Plain C++, tested on the host.
 *
 * Each function writes the number only; the unit comes from the matching
 * *Unit() function, so the screens can set it in a smaller face.
 ******************************************************************************
 */

#ifndef SKI_FORMAT_HPP
#define SKI_FORMAT_HPP

#include <cstddef>
#include <cstdint>

namespace Ski
{
namespace Fmt
{

/// "52" km/h or mph, rounded.
void        speed(char* b, size_t n, uint16_t cms, bool imperial);
const char* speedUnit(bool imperial);

/// "2,940" m or ft, with a thousands separator.
void        vertical(char* b, size_t n, int32_t metres, bool imperial);
const char* verticalUnit(bool imperial);

/// "18.2" km or mi; "0.8" below one.
void        distance(char* b, size_t n, uint32_t metres, bool imperial);
const char* distanceUnit(bool imperial);

/// "2:31" (m:ss) under an hour, "1:02:31" above.
void        duration(char* b, size_t n, uint32_t seconds);
/// "5:42" (h:mm), for the day's times.
void        hoursMinutes(char* b, size_t n, uint32_t seconds);

/// Local time of day: "13:44", or "1:44" on a 12-hour watch.
void        clock(char* b, size_t n, uint16_t minuteOfDay, bool h12);

/// Pressure "1016.4" hPa or "30.01" inHg (from hPa x 10).
void        pressure(char* b, size_t n, uint16_t hpaDeci, bool imperial);
const char* pressureUnit(bool imperial);
/// Trend "-3.4" hPa or "-0.10" inHg (from hPa per 3 h x 100), always signed.
/// Storm alert threshold, a whole number of hPa: "3 hPa" or "0.09 inHg".
void        stormThreshold(char* b, size_t n, uint8_t hpa, bool imperial);
void        pressureRate(char* b, size_t n, int16_t hpaCenti, bool imperial);

/// Vertical speed "+2.1" m/s or "+410" ft/min (from cm/s).
void        verticalSpeed(char* b, size_t n, int16_t cms, bool imperial);
const char* verticalSpeedUnit(bool imperial);

} // namespace Fmt
} // namespace Ski

#endif // SKI_FORMAT_HPP
