/**
 ******************************************************************************
 * @file    Charts.cpp
 ******************************************************************************
 */

#include "gui/widgets/Charts.hpp"

#include <algorithm>

#include "SDK/GUI/LVGL/Draw.hpp"

#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;

namespace
{
lv_obj_t* makeLine(lv_obj_t* parent, uint32_t color, int32_t width)
{
    lv_obj_t* l = lv_line_create(parent);
    lv_obj_set_style_line_width(l, width, LV_PART_MAIN);
    lv_obj_set_style_line_color(l, Draw::rgb(color), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(l, true, LV_PART_MAIN);
    lv_obj_set_pos(l, 0, 0);
    return l;
}
} // namespace

// -- Profile --------------------------------------------------------------------

ProfileChart::ProfileChart(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : mBox(Draw::container(parent, x, y, w, h)), mW(w), mH(h)
{
    mBase = makeLine(mBox, Theme::kLift, 2);
    Draw::setHidden(mBase, true);
}

void ProfileChart::set(const Ski::DayProfile& p)
{
    Draw::setHidden(mBase, true);
    for (uint8_t i = 0; i < mRunObjs; ++i) {
        Draw::setHidden(mRuns[i], true);
    }
    const uint8_t n = p.count();
    if (n == 0) {
        return;
    }

    int16_t lo = 0, hi = 0;
    p.range(lo, hi);
    if (hi - lo < 60) {   // a flat day still gets a sensible scale
        const int16_t mid = static_cast<int16_t>((lo + hi) / 2);
        lo = static_cast<int16_t>(mid - 30);
        hi = static_cast<int16_t>(mid + 30);
    }
    const float margin = 3.0f;
    auto yOf = [&](int16_t a) {
        return static_cast<float>(mH) - margin -
               (static_cast<float>(a - lo) / static_cast<float>(hi - lo)) * (static_cast<float>(mH) - 2.0f * margin);
    };
    const float dx = static_cast<float>(mW - 4) / static_cast<float>(n);

    // Two points per bucket: down for a run, up for a lift, level otherwise.
    for (uint8_t i = 0; i < n; ++i) {
        const float x0 = 2.0f + dx * static_cast<float>(i);
        const float x1 = x0 + dx;
        float       y0, y1;
        switch (p.motion(i)) {
            case Ski::Motion::Run:  y0 = yOf(p.maxAlt(i)); y1 = yOf(p.minAlt(i)); break;
            case Ski::Motion::Lift: y0 = yOf(p.minAlt(i)); y1 = yOf(p.maxAlt(i)); break;
            default: {
                const float m = yOf(static_cast<int16_t>((p.minAlt(i) + p.maxAlt(i)) / 2));
                y0 = y1 = m;
            } break;
        }
        mPts[2 * i]     = { static_cast<lv_value_precise_t>(x0 + 0.5f), static_cast<lv_value_precise_t>(y0 + 0.5f) };
        mPts[2 * i + 1] = { static_cast<lv_value_precise_t>(x1 + 0.5f), static_cast<lv_value_precise_t>(y1 + 0.5f) };
    }
    lv_line_set_points(mBase, mPts, static_cast<uint32_t>(2 * n));
    Draw::setHidden(mBase, false);

    // Each stretch of run buckets in white over the base, joined to the point
    // before it so the descent starts at the top.
    uint8_t used = 0;
    for (uint8_t i0 = 0; i0 < n && used < kMaxRuns;) {
        if (p.motion(i0) != Ski::Motion::Run) {
            ++i0;
            continue;
        }
        uint8_t i1 = i0;
        while (i1 + 1 < n && p.motion(i1 + 1) == Ski::Motion::Run) {
            ++i1;
        }
        if (used == mRunObjs) {
            mRuns[mRunObjs++] = makeLine(mBox, Theme::kRun, 3);
        }
        lv_obj_t*      l     = mRuns[used++];
        const uint16_t first = static_cast<uint16_t>(2 * i0);
        const uint16_t last  = static_cast<uint16_t>(2 * i1 + 1);
        lv_line_set_points(l, &mPts[first], static_cast<uint32_t>(last - first + 1));
        Draw::setHidden(l, false);
        i0 = static_cast<uint8_t>(i1 + 1);
    }
}

// -- Sparkline ------------------------------------------------------------------

Sparkline::Sparkline(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : mBox(Draw::container(parent, x, y, w, h)), mW(w), mH(h)
{
    mLine = makeLine(mBox, Theme::kWarn, 2);
    mDot  = Draw::dot(mBox, 0, 0, 3, Theme::kWarn);
    Draw::setHidden(mLine, true);
    Draw::setHidden(mDot, true);
}

void Sparkline::set(const int16_t* values, uint8_t count, uint32_t color, int16_t span)
{
    count = std::min<uint8_t>(count, kMax);
    if (count < 2) {
        Draw::setHidden(mLine, true);
        Draw::setHidden(mDot, true);
        return;
    }
    int16_t lo = values[0], hi = values[0];
    for (uint8_t i = 1; i < count; ++i) {
        lo = std::min(lo, values[i]);
        hi = std::max(hi, values[i]);
    }
    if (hi - lo < span) {
        const int16_t mid = static_cast<int16_t>((lo + hi) / 2);
        lo = static_cast<int16_t>(mid - span / 2);
        hi = static_cast<int16_t>(lo + span);
    }
    const float dx = static_cast<float>(mW - 8) / static_cast<float>(count - 1);
    for (uint8_t i = 0; i < count; ++i) {
        const float y = 3.0f + (static_cast<float>(hi - values[i]) / static_cast<float>(hi - lo)) *
                                   (static_cast<float>(mH) - 6.0f);
        mPts[i] = { static_cast<lv_value_precise_t>(2.5f + dx * static_cast<float>(i)),
                    static_cast<lv_value_precise_t>(y + 0.5f) };
    }
    lv_obj_set_style_line_color(mLine, Draw::rgb(color), LV_PART_MAIN);
    lv_line_set_points(mLine, mPts, count);
    Draw::setHidden(mLine, false);

    lv_obj_set_style_bg_color(mDot, Draw::rgb(color), LV_PART_MAIN);
    lv_obj_set_pos(mDot, static_cast<int32_t>(mPts[count - 1].x) - 3, static_cast<int32_t>(mPts[count - 1].y) - 3);
    Draw::setHidden(mDot, false);
}
