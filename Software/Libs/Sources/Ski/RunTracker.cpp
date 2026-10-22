/**
 ******************************************************************************
 * @file    RunTracker.cpp
 * @brief   Lift/run segmentation. See RunTracker.hpp for the rules.
 *
 * Reset rules for the tracked extremes
 * ------------------------------------
 *   mHi  max altitude since the last lift was confirmed, or since the last
 *        run closed. A run starts from it.
 *   mLo  min altitude since the last run was confirmed, or since the last
 *        lift closed. A lift starts from it, and it is the bottom of a run.
 *
 * So after a lift closes, mHi still holds its top (the next run pushes off
 * from there), and after a run closes, mLo still holds its bottom (the next
 * lift boards there).
 ******************************************************************************
 */

#include "Ski/RunTracker.hpp"

#include <algorithm>

namespace Ski
{

namespace
{
/// Speed that counts as still skiing for the purpose of dating a run's end.
/// Above the ~1 m/s of shuffling forward in a lift queue.
constexpr float kRunEndSpeed = 1.5f;
/// Vertical speed above which the skier is treated as climbing (not skiing).
constexpr float kClimbing = 0.15f;
/// Longest gap between samples counted as tracked time (a pause is longer).
constexpr uint32_t kMaxStepS = 5;
} // namespace

RunTracker::RunTracker()
    : RunTracker(Config{})
{
}

RunTracker::RunTracker(const Config& cfg)
    : mCfg(cfg)
{
}

void RunTracker::reset()
{
    const Config cfg = mCfg;
    Listener*    l   = mListener;
    *this     = RunTracker(cfg);
    mListener = l;
}

uint8_t RunTracker::popcount(uint32_t v)
{
    uint8_t n = 0;
    while (v) {
        v &= v - 1u;
        ++n;
    }
    return n;
}

RunTracker::Mark RunTracker::markNow() const
{
    Mark m;
    m.t        = mT;
    m.alt      = mAltSmooth;
    m.cum      = mCum;
    m.maxSpeed = (mState == Motion::Run) ? mRun.maxSpeed : 0.0f;
    return m;
}

// -- Signal conditioning --------------------------------------------------------

void RunTracker::conditionAltitude(const Sample& s)
{
    // Median of the last five raw readings: removes single-sample spikes
    // (pressure gusts on an exposed lift) without the lag of a long average.
    mRawAlt[mRawPos] = s.alt;
    mRawPos          = static_cast<uint8_t>((mRawPos + 1u) % kMedian);
    if (mRawCount < kMedian) {
        ++mRawCount;
    }
    float tmp[kMedian];
    for (uint8_t i = 0; i < mRawCount; ++i) {
        tmp[i] = mRawAlt[i];
    }
    for (uint8_t i = 1; i < mRawCount; ++i) {   // insertion sort: at most five
        const float v = tmp[i];
        uint8_t     j = i;
        while (j > 0 && tmp[j - 1] > v) {
            tmp[j] = tmp[j - 1];
            --j;
        }
        tmp[j] = v;
    }
    mAltSmooth = tmp[mRawCount / 2];
    mHaveAlt   = true;

    mRingAlt[mRingPos] = mAltSmooth;
    mRingT[mRingPos]   = s.t;
    mRingPos           = static_cast<uint8_t>((mRingPos + 1u) % kRing);
    if (mRingCount < kRing) {
        ++mRingCount;
    }

    // Vertical speed: least-squares slope over the samples of the last
    // slopeWindowS seconds (by timestamp, so a pause does not stretch it).
    double sumT = 0, sumA = 0, sumTT = 0, sumTA = 0;
    uint8_t n = 0;
    uint32_t tMin = s.t, tMax = 0;
    for (uint8_t i = 0; i < mRingCount; ++i) {
        const uint32_t ti = mRingT[i];
        if (s.t - ti >= mCfg.slopeWindowS) {
            continue;
        }
        const double x = static_cast<double>(static_cast<int32_t>(ti - s.t));
        const double y = mRingAlt[i];
        sumT += x;
        sumA += y;
        sumTT += x * x;
        sumTA += x * y;
        tMin = std::min(tMin, ti);
        tMax = std::max(tMax, ti);
        ++n;
    }
    if (n >= 5 && tMax - tMin >= 5) {
        const double den = n * sumTT - sumT * sumT;
        mVs = den > 0 ? static_cast<float>((n * sumTA - sumT * sumA) / den) : 0.0f;
    } else {
        mVs = 0.0f;
    }
}

float RunTracker::filteredSpeed(const Sample& s)
{
    // Median of the last three valid speeds: one GPS spike cannot set a top
    // speed. Fewer than three: the lowest, for the same reason.
    if (!s.speedValid || s.speed < 0.0f || s.speed > mCfg.maxValidSpeed) {
        return 0.0f;
    }
    mSpd[mSpdPos] = s.speed;
    mSpdPos       = static_cast<uint8_t>((mSpdPos + 1u) % 3u);
    if (mSpdCount < 3) {
        ++mSpdCount;
    }
    if (mSpdCount < 3) {
        float m = mSpd[0];
        for (uint8_t i = 1; i < mSpdCount; ++i) {
            m = std::min(m, mSpd[i]);
        }
        return m;
    }
    const float a = mSpd[0], b = mSpd[1], c = mSpd[2];
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

// -- Trails -------------------------------------------------------------------------

void RunTracker::Trail::reset(const Mark& now)
{
    count   = 1;
    pos     = 1;
    m[0]    = now;
    lastAlt = now.alt;
}

void RunTracker::Trail::update(const Mark& now, bool up)
{
    const bool step = up ? (now.alt >= lastAlt + kStep) : (now.alt <= lastAlt - kStep);
    if (!step) {
        return;
    }
    m[pos]  = now;
    pos     = static_cast<uint8_t>((pos + 1u) % kSteps);
    if (count < kSteps) {
        ++count;
    }
    lastAlt = now.alt;
}

const RunTracker::Mark& RunTracker::Trail::first(float extreme, float tol, bool up) const
{
    const uint8_t oldest = static_cast<uint8_t>((pos + kSteps - count) % kSteps);
    for (uint8_t k = 0; k < count; ++k) {
        const Mark& e = m[(oldest + k) % kSteps];
        if (up ? (e.alt >= extreme - tol) : (e.alt <= extreme + tol)) {
            return e;
        }
    }
    return m[(pos + kSteps - 1u) % kSteps];
}

// -- Extremes -------------------------------------------------------------------

void RunTracker::resetHi()
{
    mHi         = mAltSmooth;
    mHiLast     = markNow();
    mHiTrail.reset(mHiLast);
    mMaxSinceHi = 0.0f;
}

void RunTracker::resetLo()
{
    mLo = mAltSmooth;
    mLoTrail.reset(markNow());
}

void RunTracker::updateExtremes(float speed, bool movingDown, bool still)
{
    const float alt = mAltSmooth;

    if (alt > mHi) {
        mHi = alt;
    }
    if (alt >= mHi - mCfg.nearTol) {
        mHiLast     = markNow();   // still at the top: the run has not begun
        mMaxSinceHi = 0.0f;
    }
    mMaxSinceHi = std::max(mMaxSinceHi, speed);
    mHiTrail.update(markNow(), true);

    if (alt < mLo) {
        mLo = alt;
    }
    mLoTrail.update(markNow(), false);

    if (mVs <= 0.1f) {
        mClimbStart = markNow();       // not climbing (yet): not on a lift
    }
    if (movingDown) {
        mLastMoveDown = markNow();
    }

    if (still) {
        ++mStillS;
    } else {
        if (mStillS >= mCfg.longStopS) {
            mResume = markNow();       // setting off after a long stop
        }
        mStillS = 0;
    }
}

// -- Main step ------------------------------------------------------------------

void RunTracker::add(const Sample& s)
{
    const uint32_t dt = mStarted ? std::min<uint32_t>(s.t - mT, kMaxStepS) : 0u;

    if (!s.altValid) {
        // No barometer this second: count the time and distance, decide nothing.
        if (mStarted) {
            mTotalTime += dt;
            mCum.distance += s.dist;
            mT = s.t;
        }
        return;
    }

    mT = s.t;
    conditionAltitude(s);
    const float speedF = filteredSpeed(s);

    const bool moving = s.speedValid ? (s.speed > mCfg.moveSpeed) : (mVs < -mCfg.downRateNoGps);
    if (mStarted) {
        mTotalTime += dt;
        mCum.distance += s.dist;
        if (moving) {
            mCum.movingTime += static_cast<float>(dt);
        }
    }

    const bool up   = mVs > mCfg.upRate;
    const bool down = s.speedValid ? (mVs < -mCfg.downRate && s.speed > mCfg.minRunSpeed)
                                   : (mVs < -mCfg.downRateNoGps);
    const uint32_t keep = (1u << kVote) - 1u;
    mUpBits   = ((mUpBits << 1) | (up ? 1u : 0u)) & keep;
    mDownBits = ((mDownBits << 1) | (down ? 1u : 0u)) & keep;

    const bool movingDown = mVs < kClimbing &&
                            (s.speedValid ? s.speed > kRunEndSpeed : mVs < -0.2f);
    const bool still = s.speedValid ? (s.speed < mCfg.moveSpeed) : (mVs > -0.1f && mVs < 0.1f);

    if (!mStarted) {
        mStarted = true;
        resetHi();
        resetLo();
        mClimbStart   = markNow();
        mLastMoveDown = markNow();
        mResume       = markNow();
        return;
    }

    if (mState == Motion::Run) {
        mRun.maxSpeed = std::max(mRun.maxSpeed, speedF);
    }
    updateExtremes(speedF, movingDown, still);
    if (up) {
        mLiftLastUpT = mT;
    }

    if (mLiftEndPending && mState == Motion::Idle && mT - mLastLiftEndT > mCfg.liftMergeGapS) {
        flushLiftEnd();
    }

    const float alt = mAltSmooth;
    if (mState != Motion::Run && mHi - alt >= mCfg.runConfirmDrop &&
        popcount(mDownBits) >= mCfg.runConfirmDown) {
        if (mState == Motion::Lift) {
            closeLift();
        }
        openRun();
    } else if (mState == Motion::Run &&
               (alt - mLo >= mCfg.hikeCloseGain ||                 // walked/rode back up
                mT - mLastMoveDown.t > mCfg.runIdleCloseS)) {     // stopped for good
        closeRun();
    } else if (mState != Motion::Lift && alt - mClimbStart.alt >= mCfg.liftConfirmGain &&
               popcount(mUpBits) >= mCfg.liftConfirmUp) {
        openLift();
    } else if (mState == Motion::Lift && mT - mLiftLastUpT > mCfg.liftEndHoldS) {
        closeLift();
    }

    // Live view of whatever is open.
    if (mState == Motion::Run) {
        mRun.tEnd       = mT;
        mRun.bottomAlt  = mLo;
        mRun.distance   = mCum.distance - mSegStartCum.distance;
        mRun.movingTime = mCum.movingTime - mSegStartCum.movingTime;
    } else if (mState == Motion::Lift) {
        mLift.tEnd     = mT;
        mLift.topAlt   = mHi;
        mLift.distance = mCum.distance - mSegStartCum.distance;
    }
}

void RunTracker::finish()
{
    if (mState == Motion::Run) {
        closeRun();
    } else if (mState == Motion::Lift) {
        closeLift();
    }
    flushLiftEnd();
}

// -- Segments -------------------------------------------------------------------

void RunTracker::openRun()
{
    // Pushed off from the top -- or, after a long stop below the top (lunch
    // in a lodge under the lift station), when the skier set off again.
    const Mark& start = (mResume.t > mHiLast.t) ? mResume : mHiLast;
    flushLiftEnd();   // a run between means no merge

    mRun            = RunStats{};
    mRun.number     = static_cast<uint16_t>(mRuns + 1u);
    mRun.tStart     = start.t;
    mRun.tEnd       = mT;
    mRun.topAlt     = (&start == &mHiLast) ? mHi : start.alt;
    mRun.bottomAlt  = mAltSmooth;
    mRun.maxSpeed   = mMaxSinceHi;
    mSegStartCum    = start.cum;
    mRun.distance   = mCum.distance - mSegStartCum.distance;
    mRun.movingTime = mCum.movingTime - mSegStartCum.movingTime;

    mState            = Motion::Run;
    mRunSinceLastLift = true;
    resetLo();
    mLastMoveDown = markNow();

    if (mListener) {
        mListener->onRunStarted(mRun);
    }
}

void RunTracker::closeRun()
{
    // The run ended on arrival at its low point.
    const Mark& end = mLoTrail.first(mLo, mCfg.nearTol, false);

    RunStats r   = mRun;
    r.tEnd       = std::max(end.t, r.tStart);
    r.bottomAlt  = std::min(mLo, r.topAlt);
    r.distance   = std::max(0.0f, end.cum.distance - mSegStartCum.distance);
    r.movingTime = std::max(0.0f, end.cum.movingTime - mSegStartCum.movingTime);
    r.maxSpeed   = end.maxSpeed;   // the run's top speed as of its end

    // A best needs a clear margin: the same run skied twice is not a record.
    const bool bestSpeed = r.maxSpeed > mMaxSpeed + 0.3f && r.maxSpeed > 0.0f;
    const bool bestVert  = r.vertical() > mLongestRun * 1.05f + 5.0f;

    ++mRuns;
    mVertical += r.vertical();
    mDistance += r.distance;
    mMaxSpeed   = std::max(mMaxSpeed, r.maxSpeed);
    mLongestRun = std::max(mLongestRun, r.vertical());
    mRunTime += r.duration();

    mLastRun = r;
    mState   = Motion::Idle;
    resetHi();

    if (mListener) {
        mListener->onRunEnded(r, bestSpeed, bestVert);
    }
}

void RunTracker::openLift()
{
    const bool merge = !mRunSinceLastLift && mLastLift.number != 0 &&
                       mClimbStart.t - mLastLiftEndT <= mCfg.liftMergeGapS;
    if (merge) {
        // A hold on the lift, or two chained lifts: carry on with the last one.
        mLiftEndPending = false;
        --mLifts;
        mLiftVertical -= mLastLift.gain();
        mLiftTime -= mLastLift.duration();
        mLift        = mLastLift;
        mSegStartCum = mLastLiftStartCum;
        mLastLift    = mPrevLift;
    } else {
        flushLiftEnd();
        mLift           = LiftStats{};
        mLift.number    = static_cast<uint16_t>(mLifts + 1u);
        mLift.tStart    = mClimbStart.t;
        mLift.bottomAlt = mClimbStart.alt;
        mSegStartCum    = mClimbStart.cum;
    }
    mLift.tEnd   = mT;
    mLift.topAlt = mAltSmooth;

    mState            = Motion::Lift;
    mRunSinceLastLift = false;
    mLiftLastUpT      = mT;
    resetHi();

    if (!merge && mListener) {
        mListener->onLiftStarted(mLift);
    }
}

void RunTracker::closeLift()
{
    LiftStats l = mLift;
    const Mark& arrive = mHiTrail.first(mHi, mCfg.nearTol, true);
    l.tEnd      = std::max(arrive.t, l.tStart);
    l.topAlt    = std::max(mHi, l.bottomAlt);
    l.distance  = std::max(0.0f, arrive.cum.distance - mSegStartCum.distance);

    ++mLifts;
    mLiftVertical += l.gain();
    mLiftTime += l.duration();

    mPrevLift         = mLastLift;
    mLastLift         = l;
    mLastLiftStartCum = mSegStartCum;
    mLastLiftEndT     = l.tEnd;
    mState            = Motion::Idle;
    resetLo();

    // Announced once it can no longer be merged with a lift that follows.
    mLiftEndPending = true;
}

void RunTracker::flushLiftEnd()
{
    if (!mLiftEndPending) {
        return;
    }
    mLiftEndPending = false;
    if (mListener) {
        mListener->onLiftEnded(mLastLift);
    }
}

// -- Totals ---------------------------------------------------------------------

DayTotals RunTracker::totals() const
{
    DayTotals t;
    t.runs         = mRuns;
    t.lifts        = mLifts;
    t.vertical     = mVertical;
    t.liftVertical = mLiftVertical;
    t.distance     = mDistance;
    t.maxSpeed     = mMaxSpeed;
    t.longestRun   = mLongestRun;
    t.runTime      = mRunTime;
    t.liftTime     = mLiftTime;
    t.totalTime    = mTotalTime;

    if (mState == Motion::Run) {
        ++t.runs;
        t.vertical += mRun.vertical();
        t.distance += mRun.distance;
        t.maxSpeed   = std::max(t.maxSpeed, mRun.maxSpeed);
        t.longestRun = std::max(t.longestRun, mRun.vertical());
        t.runTime += mRun.duration();
    } else if (mState == Motion::Lift) {
        ++t.lifts;
        t.liftVertical += mLift.gain();
        t.liftTime += mLift.duration();
    }
    return t;
}

} // namespace Ski
