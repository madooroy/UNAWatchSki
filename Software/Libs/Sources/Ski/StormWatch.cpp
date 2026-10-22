/**
 ******************************************************************************
 * @file    StormWatch.cpp
 * @brief   Like-for-like pressure trend. See StormWatch.hpp.
 ******************************************************************************
 */

#include "Ski/StormWatch.hpp"

#include <algorithm>
#include <cmath>

namespace Ski
{

namespace
{
constexpr double kEarthR = 6371000.0;
constexpr double kDeg    = 3.14159265358979323846 / 180.0;
/// A brief speed-up (shuffling forward in a queue) does not end a still period.
constexpr uint8_t kFastToMove = 3;
/// Readings needed in the median before one can be taken.
constexpr uint8_t kMinMedian = 30;
} // namespace

const char* levelName(StormWatch::Level l)
{
    switch (l) {
        case StormWatch::Level::Learning:    return "Learning";
        case StormWatch::Level::Rising:      return "Rising";
        case StormWatch::Level::Steady:      return "Steady";
        case StormWatch::Level::Falling:     return "Falling";
        case StormWatch::Level::FallingFast: return "Falling fast";
        case StormWatch::Level::Storm:       return "Storm";
    }
    return "?";
}

StormWatch::StormWatch()
    : StormWatch(Config{})
{
}

StormWatch::StormWatch(const Config& cfg)
    : mCfg(cfg)
{
}

void StormWatch::reset()
{
    const Config cfg = mCfg;
    *this = StormWatch(cfg);
}

float StormWatch::distanceM(double lat1, double lon1, double lat2, double lon2)
{
    // Equirectangular: exact enough over the few hundred metres compared here.
    const double x = (lon2 - lon1) * kDeg * std::cos(0.5 * (lat1 + lat2) * kDeg);
    const double y = (lat2 - lat1) * kDeg;
    return static_cast<float>(std::sqrt(x * x + y * y) * kEarthR);
}

void StormWatch::endPark()
{
    mParked    = false;
    mFastCount = 0;
    mPCount    = 0;
    mPPos      = 0;
}

void StormWatch::add(uint32_t t, float pa, bool pValid, float speed, bool speedValid,
                     double lat, double lon, bool fix)
{
    if (pValid) {
        mTrend.pressureHpa = pa / 100.0f;
    }

    // Standing still is only trusted with a position: without one we cannot
    // know this is the same place as before.
    if (!fix || !speedValid) {
        endPark();
        return;
    }

    if (speed >= mCfg.parkSpeed) {
        if (++mFastCount >= kFastToMove) {
            endPark();
        }
        return;
    }
    mFastCount = 0;

    if (!mParked || distanceM(mParkLat, mParkLon, lat, lon) > mCfg.parkRadiusM) {
        endPark();
        mParked       = true;
        mParkStartT   = t;
        mParkLat      = lat;
        mParkLon      = lon;
        mLastReadingT = 0;
    }

    if (pValid) {
        mPBuf[mPPos] = pa;
        mPPos        = static_cast<uint8_t>((mPPos + 1u) % kParkBuf);
        if (mPCount < kParkBuf) {
            ++mPCount;
        }
    }

    if (t - mParkStartT >= mCfg.parkMinS && mPCount >= kMinMedian &&
        (mLastReadingT == 0 || t - mLastReadingT >= mCfg.readingEveryS)) {
        takeReading(t);
    }
}

void StormWatch::takeReading(uint32_t t)
{
    float tmp[kParkBuf];
    for (uint8_t i = 0; i < mPCount; ++i) {
        tmp[i] = mPBuf[i];
    }
    std::nth_element(tmp, tmp + mPCount / 2, tmp + mPCount);
    const float pa = tmp[mPCount / 2];

    const uint8_t idx = spotFor(mParkLat, mParkLon, t);
    Spot& s = mSpots[idx];
    // Move the spot's centre to the average of its readings (capped, so it
    // can still follow a queue that moves through the day).
    const double w = 1.0 / static_cast<double>(std::min<uint16_t>(++s.visits, 8));
    s.lat += (mParkLat - s.lat) * w;
    s.lon += (mParkLon - s.lon) * w;
    if (s.count == kReadingsPerSpot) {
        for (uint8_t i = 1; i < kReadingsPerSpot; ++i) {
            s.r[i - 1] = s.r[i];
        }
        --s.count;
    }
    s.r[s.count].t  = t;
    s.r[s.count].pa = pa;
    ++s.count;
    s.lastT = t;

    mLastReadingT = t;
    if (mTrend.readings < 0xFFFF) {
        ++mTrend.readings;
    }
    mTrend.spots = mSpotCount;

    float rate = 0.0f;
    if (slopeAt(s, t, rate)) {
        evaluate(t, idx, rate);
    }
}

uint8_t StormWatch::spotFor(double lat, double lon, uint32_t t)
{
    int   best  = -1;
    float bestD = mCfg.spotRadiusM;
    for (uint8_t i = 0; i < mSpotCount; ++i) {
        const float d = distanceM(mSpots[i].lat, mSpots[i].lon, lat, lon);
        if (d <= bestD) {
            bestD = d;
            best  = i;
        }
    }
    if (best >= 0) {
        return static_cast<uint8_t>(best);
    }

    uint8_t idx;
    if (mSpotCount < kMaxSpots) {
        idx = mSpotCount++;
    } else {
        // Full: reuse the spot visited longest ago.
        idx = 0;
        for (uint8_t i = 1; i < kMaxSpots; ++i) {
            if (mSpots[i].lastT < mSpots[idx].lastT) {
                idx = i;
            }
        }
    }
    mSpots[idx]       = Spot{};
    mSpots[idx].lat   = lat;
    mSpots[idx].lon   = lon;
    mSpots[idx].lastT = t;
    return idx;
}

bool StormWatch::slopeAt(const Spot& s, uint32_t now, float& hpaPer3h) const
{
    double sumX = 0, sumY = 0, sumXX = 0, sumXY = 0;
    uint8_t  n    = 0;
    uint32_t tMin = now, tMax = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        if (now - s.r[i].t > mCfg.windowS) {
            continue;
        }
        const double x = static_cast<double>(static_cast<int32_t>(s.r[i].t - now));
        const double y = s.r[i].pa;
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
        tMin = std::min(tMin, s.r[i].t);
        tMax = std::max(tMax, s.r[i].t);
        ++n;
    }
    if (n < 3 || tMax - tMin < mCfg.minSpanS) {
        return false;
    }
    const double den = n * sumXX - sumX * sumX;
    if (den <= 0) {
        return false;
    }
    const double paPerS = (n * sumXY - sumX * sumY) / den;

    // Readings taken at one height lie close to a smooth line (sensor noise
    // is a few Pa). If they scatter more than mCfg.maxScatterPa about the
    // fit, they were taken at different heights (stops spread along a slope,
    // within the spot radius) and the trend means nothing.
    const double intercept = (sumY - paPerS * sumX) / n;
    double ss = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        if (now - s.r[i].t > mCfg.windowS) {
            continue;
        }
        const double x = static_cast<double>(static_cast<int32_t>(s.r[i].t - now));
        const double e = s.r[i].pa - (intercept + paPerS * x);
        ss += e * e;
    }
    if (std::sqrt(ss / n) > mCfg.maxScatterPa) {
        return false;
    }
    // Scaled to sea level: up a mountain the station pressure moves by only
    // p/p0 of the weather's change (about 80 % at 2000 m), and the 3 hPa
    // rule is a sea-level figure.
    const double meanPa = sumY / n;
    const double toSea  = meanPa > 1000.0 ? 101325.0 / meanPa : 1.0;
    hpaPer3h = static_cast<float>(paPerS * toSea * 10800.0 / 100.0);
    return true;
}

void StormWatch::evaluate(uint32_t t, uint8_t spot, float rate)
{
    const float thr   = mCfg.thresholdHpa;
    const bool  storm = thr > 0.0f && rate <= -thr;

    Level lvl;
    if (storm) {
        lvl = Level::Storm;
    } else if (rate <= -2.0f) {
        lvl = Level::FallingFast;
    } else if (rate <= -1.0f) {
        lvl = Level::Falling;
    } else if (rate >= 1.0f) {
        lvl = Level::Rising;
    } else {
        lvl = Level::Steady;
    }

    mTrend.level    = lvl;
    mTrend.hpaPer3h = rate;
    mTrend.t        = t;
    mTrend.spot     = spot;

    if (storm) {
        if (mArmed || (mEverAlerted && t - mLastAlertT >= mCfg.repeatS)) {
            mAlertPending = true;
            mLastAlertT   = t;
            mEverAlerted  = true;
            mArmed        = false;
        }
    } else if (thr > 0.0f && rate > -thr + 1.0f) {
        mArmed = true;
    }
}

bool StormWatch::takeAlert()
{
    const bool a  = mAlertPending;
    mAlertPending = false;
    return a;
}

const StormWatch::Spot* StormWatch::busiestSpot(uint32_t now) const
{
    const Spot* best  = nullptr;
    uint8_t     bestN = 1;
    for (uint8_t i = 0; i < mSpotCount; ++i) {
        uint8_t n = 0;
        for (uint8_t k = 0; k < mSpots[i].count; ++k) {
            if (now - mSpots[i].r[k].t <= mCfg.windowS) {
                ++n;
            }
        }
        if (n > bestN || (n == bestN && best && mSpots[i].lastT > best->lastT)) {
            bestN = n;
            best  = &mSpots[i];
        }
    }
    return best;
}

} // namespace Ski
