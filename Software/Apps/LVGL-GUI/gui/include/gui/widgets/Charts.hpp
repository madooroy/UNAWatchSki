/**
 ******************************************************************************
 * @file    Charts.hpp
 * @brief   The two small charts: the day's altitude profile (runs white,
 *          lifts blue, standing grey) and the storm watch's pressure
 *          sparkline.
 ******************************************************************************
 */

#ifndef CHARTS_HPP
#define CHARTS_HPP

#include <cstdint>

#include "lvgl.h"

#include "SkiTypes.hpp"

/// Sawtooth of the day: each bucket is drawn as a short segment, down for a
/// run, up for a lift, flat when standing. The whole day is one blue line;
/// each run is drawn over it in white (line objects made only as needed, so
/// a long day costs a few dozen objects).
class ProfileChart
{
public:
    ProfileChart(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    void set(const Ski::DayProfile& p);

private:
    static constexpr uint16_t kMaxPoints = 2 * Ski::DayProfile::kBuckets + 2;
    static constexpr uint8_t  kMaxRuns   = 48;

    lv_obj_t*          mBox;
    int32_t            mW, mH;
    lv_point_precise_t mPts[kMaxPoints] = {};
    lv_obj_t*          mBase = nullptr;
    lv_obj_t*          mRuns[kMaxRuns] = {};
    uint8_t            mRunObjs = 0;   ///< run line objects made so far
};

/// A few readings joined by a line, with a dot on the latest.
class Sparkline
{
public:
    Sparkline(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    /// @param values hPa x 10 relative to the latest; @param span: the
    ///        vertical range drawn (at least), hPa x 10.
    void set(const int16_t* values, uint8_t count, uint32_t color, int16_t span = 10);

private:
    static constexpr uint8_t kMax = Ski::StormInfo::kSpark;
    lv_obj_t*          mBox;
    int32_t            mW, mH;
    lv_point_precise_t mPts[kMax] = {};
    lv_obj_t*          mLine = nullptr;
    lv_obj_t*          mDot  = nullptr;
};

#endif // CHARTS_HPP
