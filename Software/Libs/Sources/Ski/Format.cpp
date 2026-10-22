/**
 ******************************************************************************
 * @file    Format.cpp
 * @brief   Integer-only formatting (no %f: the watch's newlib-nano printf
 *          does not print floats unless it is linked in specially).
 ******************************************************************************
 */

#include "Ski/Format.hpp"

#include <cstdio>
#include <cstdlib>

namespace Ski
{
namespace Fmt
{

namespace
{
constexpr uint32_t kFtPerKm = 3281;    // ft per 1000 m
constexpr uint32_t kMiMilli = 621;     // milli-miles per km

/// Thousands separators for a non-negative integer.
void grouped(char* b, size_t n, uint32_t v)
{
    char raw[16];
    const int len = std::snprintf(raw, sizeof(raw), "%lu", static_cast<unsigned long>(v));
    size_t o = 0;
    for (int i = 0; i < len && o + 1 < n; ++i) {
        b[o++] = raw[i];
        const int left = len - 1 - i;
        if (left > 0 && left % 3 == 0 && o + 1 < n) {
            b[o++] = ',';
        }
    }
    b[o] = '\0';
}

/// value/scale with one decimal, rounded: tenths(1234, 100) = "12.3".
void oneDecimal(char* b, size_t n, uint32_t value, uint32_t scale)
{
    const uint32_t tenths = (value * 10u + scale / 2u) / scale;
    std::snprintf(b, n, "%lu.%lu", static_cast<unsigned long>(tenths / 10u),
                  static_cast<unsigned long>(tenths % 10u));
}
} // namespace

void speed(char* b, size_t n, uint16_t cms, bool imperial)
{
    // km/h = cm/s * 0.036; mph = cm/s * 0.0223694
    const uint32_t v = imperial ? (static_cast<uint32_t>(cms) * 224u + 5000u) / 10000u
                                : (static_cast<uint32_t>(cms) * 36u + 500u) / 1000u;
    std::snprintf(b, n, "%lu", static_cast<unsigned long>(v));
}

const char* speedUnit(bool imperial)
{
    return imperial ? "mph" : "km/h";
}

void vertical(char* b, size_t n, int32_t metres, bool imperial)
{
    const bool     neg = metres < 0;
    const uint32_t m   = static_cast<uint32_t>(neg ? -metres : metres);
    const uint32_t v   = imperial ? (m * kFtPerKm + 500u) / 1000u : m;
    if (neg && n > 1) {
        b[0] = '-';
        grouped(b + 1, n - 1, v);
    } else {
        grouped(b, n, v);
    }
}

const char* verticalUnit(bool imperial)
{
    return imperial ? "ft" : "m";
}

void distance(char* b, size_t n, uint32_t metres, bool imperial)
{
    if (imperial) {
        // miles x 1000 = m * 0.621371
        const uint32_t milli = static_cast<uint32_t>((static_cast<uint64_t>(metres) * kMiMilli + 500u) / 1000u);
        oneDecimal(b, n, milli, 1000u);
    } else {
        oneDecimal(b, n, metres, 1000u);
    }
}

const char* distanceUnit(bool imperial)
{
    return imperial ? "mi" : "km";
}

void duration(char* b, size_t n, uint32_t s)
{
    if (s >= 3600u) {
        std::snprintf(b, n, "%lu:%02lu:%02lu", static_cast<unsigned long>(s / 3600u),
                      static_cast<unsigned long>((s / 60u) % 60u), static_cast<unsigned long>(s % 60u));
    } else {
        std::snprintf(b, n, "%lu:%02lu", static_cast<unsigned long>(s / 60u),
                      static_cast<unsigned long>(s % 60u));
    }
}

void hoursMinutes(char* b, size_t n, uint32_t s)
{
    std::snprintf(b, n, "%lu:%02lu", static_cast<unsigned long>(s / 3600u),
                  static_cast<unsigned long>((s / 60u) % 60u));
}

void clock(char* b, size_t n, uint16_t minuteOfDay, bool h12)
{
    unsigned h = (minuteOfDay / 60u) % 24u;
    const unsigned m = minuteOfDay % 60u;
    if (h12) {
        h = h % 12u;
        if (h == 0) {
            h = 12;
        }
    }
    std::snprintf(b, n, h12 ? "%u:%02u" : "%02u:%02u", h, m);
}

void pressure(char* b, size_t n, uint16_t hpaDeci, bool imperial)
{
    if (imperial) {
        // inHg x 100 = hPa x 10 * 0.2953
        const uint32_t c = (static_cast<uint32_t>(hpaDeci) * 2953u + 5000u) / 10000u;
        std::snprintf(b, n, "%lu.%02lu", static_cast<unsigned long>(c / 100u), static_cast<unsigned long>(c % 100u));
    } else {
        std::snprintf(b, n, "%u.%u", hpaDeci / 10u, hpaDeci % 10u);
    }
}

const char* pressureUnit(bool imperial)
{
    return imperial ? "inHg" : "hPa";
}

void pressureRate(char* b, size_t n, int16_t hpaCenti, bool imperial)
{
    const bool neg = hpaCenti < 0;
    const uint32_t a = static_cast<uint32_t>(neg ? -hpaCenti : hpaCenti);
    if (imperial) {
        // inHg x 100 = hPa x 100 * 0.02953
        const uint32_t c = (a * 2953u + 50000u) / 100000u;
        std::snprintf(b, n, "%c%lu.%02lu", neg ? '-' : '+', static_cast<unsigned long>(c / 100u),
                      static_cast<unsigned long>(c % 100u));
    } else {
        const uint32_t d = (a + 5u) / 10u;   // tenths
        std::snprintf(b, n, "%c%lu.%lu", neg ? '-' : '+', static_cast<unsigned long>(d / 10u),
                      static_cast<unsigned long>(d % 10u));
    }
}

void verticalSpeed(char* b, size_t n, int16_t cms, bool imperial)
{
    const bool     neg = cms < 0;
    const uint32_t a   = static_cast<uint32_t>(neg ? -cms : cms);
    if (imperial) {
        // ft/min = cm/s * 1.9685
        const uint32_t f = (a * 19685u + 5000u) / 10000u;
        std::snprintf(b, n, "%c%lu", neg ? '-' : '+', static_cast<unsigned long>(f));
    } else {
        const uint32_t d = (a + 5u) / 10u;   // dm/s
        std::snprintf(b, n, "%c%lu.%lu", neg ? '-' : '+', static_cast<unsigned long>(d / 10u),
                      static_cast<unsigned long>(d % 10u));
    }
}

const char* verticalSpeedUnit(bool imperial)
{
    return imperial ? "ft/min" : "m/s";
}

} // namespace Fmt
} // namespace Ski
