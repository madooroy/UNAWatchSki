/**
 ******************************************************************************
 * @file    SkiTypes.cpp
 ******************************************************************************
 */

#include "SkiTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Ski
{

// -- Settings -------------------------------------------------------------------

void Settings::sanitize()
{
    if (sport >= static_cast<uint8_t>(Sport::COUNT)) {
        sport = 0;
    }
    bool okStorm = false;
    for (uint8_t c : kStormChoices) {
        okStorm = okStorm || c == stormHpa;
    }
    if (!okStorm) {
        stormHpa = 3;
    }
    runAlerts = runAlerts ? 1 : 0;
    if (alertMode >= static_cast<uint8_t>(AlertMode::COUNT)) {
        alertMode = 0;
    }
    if (demo >= static_cast<uint8_t>(DemoSpeed::COUNT)) {
        demo = 0;
    }
}

size_t settingsToText(const Settings& s, const Season& season, char* buf, size_t len)
{
    const int n = std::snprintf(
        buf, len,
        "sport=%u\nstorm=%u\nrunalerts=%u\nalert=%u\ndemo=%u\n"
        "days=%u\nruns=%u\nvert=%lu\ntop=%u\nbestday=%u\nlastruns=%u\nlastvert=%u\nlastday=%lu\n",
        s.sport, s.stormHpa, s.runAlerts, s.alertMode, s.demo, season.days, season.runs,
        static_cast<unsigned long>(season.verticalM), season.topSpeedCms, season.bestDayM,
        season.lastRuns, season.lastVertM, static_cast<unsigned long>(season.lastDayUtc));
    return (n > 0 && static_cast<size_t>(n) < len) ? static_cast<size_t>(n) : 0;
}

void settingsFromText(const char* text, Settings& s, Season& season)
{
    // Tiny "key=value" reader (no sscanf: keeps newlib's stdio out of the image).
    auto valueOf = [text](const char* key, unsigned long& out) -> bool {
        const size_t klen = std::strlen(key);
        for (const char* p = text; p && *p;) {
            if ((p == text || p[-1] == '\n') && std::strncmp(p, key, klen) == 0 && p[klen] == '=') {
                const char* v = p + klen + 1;
                if (*v < '0' || *v > '9') {
                    return false;
                }
                unsigned long x = 0;
                while (*v >= '0' && *v <= '9') {
                    x = x * 10u + static_cast<unsigned long>(*v++ - '0');
                }
                out = x;
                return true;
            }
            p = std::strchr(p, '\n');
            if (p) {
                ++p;
            }
        }
        return false;
    };

    unsigned long v = 0;
    if (valueOf("sport", v))     s.sport     = static_cast<uint8_t>(v);
    if (valueOf("storm", v))     s.stormHpa  = static_cast<uint8_t>(v);
    if (valueOf("runalerts", v)) s.runAlerts = static_cast<uint8_t>(v);
    if (valueOf("alert", v))     s.alertMode = static_cast<uint8_t>(v);
    if (valueOf("demo", v))      s.demo      = static_cast<uint8_t>(v);
    s.sanitize();

    if (valueOf("days", v))      season.days        = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("runs", v))      season.runs        = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("vert", v))      season.verticalM   = static_cast<uint32_t>(v);
    if (valueOf("top", v))       season.topSpeedCms = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("bestday", v))   season.bestDayM    = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("lastruns", v))  season.lastRuns    = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("lastvert", v))  season.lastVertM   = static_cast<uint16_t>(std::min(v, 0xFFFFul));
    if (valueOf("lastday", v))   season.lastDayUtc  = static_cast<uint32_t>(v);
}

// -- Profile --------------------------------------------------------------------

void DayProfile::reset(uint32_t t0, uint16_t bucketS)
{
    mT0      = t0;
    mBucketS = bucketS ? bucketS : 30;
    mCount   = 0;
}

void DayProfile::compress()
{
    // Halve the resolution: merge pairs, double the bucket length.
    const uint8_t half = static_cast<uint8_t>(mCount / 2);
    for (uint8_t i = 0; i < half; ++i) {
        const uint8_t a = static_cast<uint8_t>(2 * i), b = static_cast<uint8_t>(2 * i + 1);
        mMin[i]  = std::min(mMin[a], mMin[b]);
        mMax[i]  = std::max(mMax[a], mMax[b]);
        // Run outranks lift outranks idle, so short runs stay visible.
        mKind[i] = std::max(mKind[a], mKind[b]);
    }
    if (mCount % 2) {
        mMin[half]  = mMin[mCount - 1];
        mMax[half]  = mMax[mCount - 1];
        mKind[half] = mKind[mCount - 1];
        mCount      = static_cast<uint8_t>(half + 1);
    } else {
        mCount = half;
    }
    mBucketS = static_cast<uint16_t>(mBucketS * 2u);
}

void DayProfile::add(uint32_t t, float alt, Motion m)
{
    if (t < mT0) {
        return;
    }
    uint32_t idx = (t - mT0) / mBucketS;
    while (idx >= kBuckets) {
        compress();
        idx = (t - mT0) / mBucketS;
    }
    const int16_t a    = static_cast<int16_t>(std::lround(std::max(-1000.0f, std::min(9000.0f, alt))));
    const uint8_t kind = static_cast<uint8_t>(m == Motion::Run ? 2 : (m == Motion::Lift ? 1 : 0));
    // Fill any skipped buckets (a pause) by holding the last altitude.
    while (mCount <= idx) {
        const int16_t fill = mCount ? mMax[mCount - 1] : a;
        mMin[mCount]  = mCount < idx ? fill : a;
        mMax[mCount]  = mCount < idx ? fill : a;
        mKind[mCount] = 0;
        ++mCount;
    }
    mMin[idx]  = std::min(mMin[idx], a);
    mMax[idx]  = std::max(mMax[idx], a);
    mKind[idx] = std::max(mKind[idx], kind);
}

void DayProfile::range(int16_t& lo, int16_t& hi) const
{
    lo = 0;
    hi = 0;
    for (uint8_t i = 0; i < mCount; ++i) {
        lo = (i == 0) ? mMin[i] : std::min(lo, mMin[i]);
        hi = (i == 0) ? mMax[i] : std::max(hi, mMax[i]);
    }
}

void DayLog::reset()
{
    *this = DayLog{};
}

void DayLog::addRun(const RunInfo& r)
{
    if (count < kMaxRuns) {
        runs[count++] = r;
    }
}

// -- Conversions ----------------------------------------------------------------

uint16_t toCms(float mps)
{
    const float c = mps * 100.0f;
    return static_cast<uint16_t>(c <= 0.0f ? 0.0f : (c >= 65535.0f ? 65535.0f : c + 0.5f));
}

namespace
{
uint16_t clamp16(float v)
{
    return static_cast<uint16_t>(v <= 0.0f ? 0.0f : (v >= 65535.0f ? 65535.0f : v + 0.5f));
}
} // namespace

RunInfo toInfo(const RunStats& r, uint32_t localMidnightUtc)
{
    RunInfo i;
    i.number      = r.number;
    i.verticalM   = clamp16(r.vertical());
    i.distanceM   = static_cast<uint32_t>(std::max(0.0f, r.distance) + 0.5f);
    i.maxSpeedCms = toCms(r.maxSpeed);
    i.avgSpeedCms = toCms(r.avgSpeed());
    i.durationS   = static_cast<uint16_t>(std::min<uint32_t>(r.duration(), 0xFFFF));
    i.startMin    = r.tStart >= localMidnightUtc
                        ? static_cast<uint16_t>(((r.tStart - localMidnightUtc) / 60u) % 1440u)
                        : 0;
    return i;
}

LiftInfo toInfo(const LiftStats& l)
{
    LiftInfo i;
    i.number    = l.number;
    i.gainM     = clamp16(l.gain());
    i.durationS = static_cast<uint16_t>(std::min<uint32_t>(l.duration(), 0xFFFF));
    i.topAltM   = clamp16(l.topAlt);
    return i;
}

TotalsInfo toInfo(const DayTotals& d)
{
    TotalsInfo i;
    i.runs        = d.runs;
    i.lifts       = d.lifts;
    i.verticalM   = static_cast<uint32_t>(std::max(0.0f, d.vertical) + 0.5f);
    i.liftVertM   = static_cast<uint32_t>(std::max(0.0f, d.liftVertical) + 0.5f);
    i.distanceM   = static_cast<uint32_t>(std::max(0.0f, d.distance) + 0.5f);
    i.maxSpeedCms = toCms(d.maxSpeed);
    i.longestRunM = clamp16(d.longestRun);
    i.runTimeS    = d.runTime;
    i.liftTimeS   = d.liftTime;
    i.totalTimeS  = d.totalTime;
    return i;
}

} // namespace Ski
