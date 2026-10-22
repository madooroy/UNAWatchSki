/**
 ******************************************************************************
 * @file    RunTracker.hpp
 * @brief   Splits a ski day into lift rides and runs from the barometer and
 *          GPS speed, one sample per second.
 *
 * Plain C++ with no SDK, sensor or GUI dependency, so it is unit-tested on a
 * PC and driven identically by the watch service and the simulated day.
 *
 * How it decides
 * --------------
 * Vertical speed is the least-squares slope of the (median-filtered)
 * barometric altitude over the last 20 s. Each second is then:
 *
 *   UP    climbing faster than 0.3 m/s                      (any lift)
 *   DOWN  dropping faster than 0.25 m/s while GPS shows you
 *         moving (or faster than 0.5 m/s with no GPS speed)  (skiing)
 *   still anything else: queues, walking, flat traverses, a hike up
 *
 * A run is confirmed once you are 25 m below the high point since the last
 * lift (or since the last run ended) with at least 10 DOWN seconds in the last
 * 30. A lift is confirmed once you have climbed 15 m since you last stood
 * level, with 20 UP seconds in the last 30. Neither is reported until
 * confirmed, but both are dated back:
 *
 *   run start   last moment within 2 m of the high point (you pushed off),
 *               or when you set off after a stop of 2 minutes or more
 *   run end     first moment within 2 m of the run's low point (you reached
 *               the bottom; a flat skate to the lift is not run time)
 *   lift start  last moment you were not climbing (you boarded)
 *   lift end    first moment within 2 m of the lift's top (you arrived)
 *
 * so the queue is neither run nor lift time, and the stats cover the whole
 * run, not just the part after it was confirmed. These moments are all
 * found within minutes of each other, so the barometer's drift with the
 * weather (up to ~15 m an hour in a storm) does not move them.
 *
 * A run ends when the next lift is confirmed, after 15 minutes without moving
 * downhill (lunch, a long wait), or when you walk 12 m back up. Shorter stops
 * inside a run (a fall, waiting for a friend) stay in the run. A lift ends
 * after 40 s without climbing; if another lift starts within 5 minutes and no
 * run came between, the two are merged (a gondola that stopped in the wind,
 * or two chained lifts).
 *
 * Without GPS (a gondola cabin) lifts and runs are still found from the
 * barometer; only distance and speed are missing for that stretch.
 ******************************************************************************
 */

#ifndef SKI_RUN_TRACKER_HPP
#define SKI_RUN_TRACKER_HPP

#include <cstdint>

namespace Ski
{

/// What the skier is doing right now.
enum class Motion : uint8_t {
    Idle = 0,   ///< Neither on a confirmed lift nor on a confirmed run
    Lift,
    Run,
};

/// One second of input.
struct Sample {
    uint32_t t          = 0;      ///< Seconds, monotonic (gaps allowed: pauses)
    float    alt        = 0.0f;   ///< Barometric altitude, m
    bool     altValid   = false;
    float    speed      = 0.0f;   ///< Ground speed, m/s
    bool     speedValid = false;
    float    dist       = 0.0f;   ///< Ground distance since the previous sample, m
};

struct RunStats {
    uint16_t number     = 0;      ///< 1-based
    uint32_t tStart     = 0;
    uint32_t tEnd       = 0;      ///< While open: now
    float    topAlt     = 0.0f;
    float    bottomAlt  = 0.0f;   ///< While open: lowest so far
    float    distance   = 0.0f;   ///< m
    float    maxSpeed   = 0.0f;   ///< m/s
    float    movingTime = 0.0f;   ///< s at > 1 m/s

    float    vertical() const { return topAlt > bottomAlt ? topAlt - bottomAlt : 0.0f; }
    uint32_t duration() const { return tEnd > tStart ? tEnd - tStart : 0; }
    /// Distance over moving time (stops inside the run excluded).
    float    avgSpeed() const { return movingTime > 0.5f ? distance / movingTime : 0.0f; }
};

struct LiftStats {
    uint16_t number    = 0;       ///< 1-based
    uint32_t tStart    = 0;
    uint32_t tEnd      = 0;       ///< While open: now
    float    bottomAlt = 0.0f;
    float    topAlt    = 0.0f;    ///< While open: highest so far
    float    distance  = 0.0f;

    float    gain() const     { return topAlt > bottomAlt ? topAlt - bottomAlt : 0.0f; }
    uint32_t duration() const { return tEnd > tStart ? tEnd - tStart : 0; }
};

struct DayTotals {
    uint16_t runs         = 0;    ///< Including an open run
    uint16_t lifts        = 0;    ///< Including an open lift
    float    vertical     = 0.0f; ///< Sum of run verticals, m
    float    liftVertical = 0.0f; ///< Sum of lift gains, m
    float    distance     = 0.0f; ///< Distance skied (runs only), m
    float    maxSpeed     = 0.0f; ///< Top speed on runs, m/s
    float    longestRun   = 0.0f; ///< Largest single-run vertical, m
    uint32_t runTime      = 0;    ///< s
    uint32_t liftTime     = 0;    ///< s
    uint32_t totalTime    = 0;    ///< s tracked (pauses excluded)

    uint32_t idleTime() const
    {
        const uint32_t busy = runTime + liftTime;
        return totalTime > busy ? totalTime - busy : 0;
    }
};

class RunTracker
{
public:
    struct Config {
        float    upRate          = 0.30f;  ///< m/s climbing = lift
        float    downRate        = 0.25f;  ///< m/s dropping = skiing (with GPS motion)
        float    downRateNoGps   = 0.50f;  ///< m/s dropping = skiing (no GPS speed)
        float    minRunSpeed     = 2.0f;   ///< m/s GPS speed to count as skiing
        float    moveSpeed       = 1.0f;   ///< m/s counts as moving (moving time, run end)
        float    runConfirmDrop  = 25.0f;  ///< m below the high point
        uint8_t  runConfirmDown  = 10;     ///< DOWN seconds in the last 30
        float    liftConfirmGain = 15.0f;  ///< m above the low point
        uint8_t  liftConfirmUp   = 20;     ///< UP seconds in the last 30
        float    nearTol         = 2.0f;   ///< m, "at" the high or low point
        float    hikeCloseGain   = 12.0f;  ///< m walked back up that ends a run
        uint32_t longStopS       = 120;    ///< a stop this long resets a run's start
        uint32_t runIdleCloseS   = 900;    ///< close a run after this long not skiing
                                            ///< (a break at a hut stays in the run; lunch ends it)
        uint32_t liftEndHoldS    = 40;     ///< close a lift after this long not climbing
        uint32_t liftMergeGapS   = 300;    ///< merge lifts closer than this, no run between
        float    maxValidSpeed   = 45.0f;  ///< m/s; faster samples are GPS glitches
        uint8_t  slopeWindowS    = 20;     ///< vertical-speed regression window
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onRunStarted(const RunStats& run) { (void)run; }
        /// @param bestSpeed  this run set the day's top speed
        /// @param bestVert   this run is the day's biggest vertical so far
        virtual void onRunEnded(const RunStats& run, bool bestSpeed, bool bestVert)
        {
            (void)run; (void)bestSpeed; (void)bestVert;
        }
        virtual void onLiftStarted(const LiftStats& lift) { (void)lift; }
        virtual void onLiftEnded(const LiftStats& lift) { (void)lift; }
    };

    RunTracker();
    explicit RunTracker(const Config& cfg);

    void setListener(Listener* l) { mListener = l; }
    const Config& config() const { return mCfg; }

    void reset();
    void add(const Sample& s);
    /// End of the day: close whatever is open (as of the last sample).
    void finish();

    Motion           motion() const      { return mState; }
    /// Valid while motion() == Run.
    const RunStats&  currentRun() const  { return mRun; }
    /// Valid while motion() == Lift.
    const LiftStats& currentLift() const { return mLift; }
    /// The last closed run (number 0 = none yet).
    const RunStats&  lastRun() const     { return mLastRun; }
    const LiftStats& lastLift() const    { return mLastLift; }
    /// Day totals, including whatever is open right now.
    DayTotals        totals() const;

    float    altitude() const       { return mAltSmooth; }
    bool     altitudeValid() const  { return mHaveAlt; }
    float    verticalSpeed() const  { return mVs; }   ///< m/s, + up
    uint32_t stillSeconds() const   { return mStillS; }
    uint32_t now() const            { return mT; }

private:
    /// Cumulative day counters, snapshotted at the moments a segment may be
    /// dated back to, so its stats can be cut out exactly afterwards.
    struct Cum {
        float distance   = 0.0f;
        float movingTime = 0.0f;
    };
    struct Mark {
        uint32_t t = 0;
        float    alt = 0.0f;
        Cum      cum;
        float    maxSpeed = 0.0f;   ///< Run max speed as of this moment
    };

    /// The last few half-metre steps towards an extreme, so the first moment
    /// within nearTol of it can be found once it is known.
    struct Trail {
        static constexpr uint8_t kSteps = 16;
        static constexpr float   kStep  = 0.5f;
        Mark    m[kSteps];
        uint8_t count = 0, pos = 0;
        float   lastAlt = 0.0f;
        void  reset(const Mark& now);
        /// @param up  true: tracking a top (climbing), false: a bottom
        void  update(const Mark& now, bool up);
        /// First step within tol of the extreme (the newest step if none).
        const Mark& first(float extreme, float tol, bool up) const;
    };

    static constexpr uint8_t kRing   = 32;   ///< altitude history (>= slope window)
    static constexpr uint8_t kVote   = 30;   ///< classification history, seconds
    static constexpr uint8_t kMedian = 5;

    Config    mCfg;
    Listener* mListener = nullptr;

    // Signal conditioning
    float    mRawAlt[kMedian] = {};
    uint8_t  mRawCount = 0;
    uint8_t  mRawPos   = 0;
    float    mRingAlt[kRing] = {};
    uint32_t mRingT[kRing]   = {};
    uint8_t  mRingCount = 0;
    uint8_t  mRingPos   = 0;
    float    mAltSmooth = 0.0f;
    bool     mHaveAlt   = false;
    float    mVs        = 0.0f;
    float    mSpd[3]    = {};
    uint8_t  mSpdCount  = 0;
    uint8_t  mSpdPos    = 0;

    // Classification votes over the last kVote seconds (bit per second)
    uint32_t mUpBits   = 0;
    uint32_t mDownBits = 0;

    // Time
    uint32_t mT         = 0;
    bool     mStarted   = false;
    uint32_t mTotalTime = 0;
    Cum      mCum;

    // Extremes since the last reset point (see the .cpp for the reset rules)
    float mHi = 0.0f;
    Mark  mHiLast;          ///< last time within nearTol of mHi
    Trail mHiTrail;         ///< steps up to mHi (lift arrival)
    float mLo = 0.0f;
    Trail mLoTrail;         ///< steps down to mLo (run end)
    Mark  mClimbStart;      ///< last second not climbing (lift start)
    Mark  mLastMoveDown;    ///< last second moving and not climbing (idle close)
    uint32_t mStillS = 0;   ///< consecutive seconds standing still
    Mark  mResume;          ///< when the skier set off after a long stop
    float mMaxSinceHi   = 0.0f; ///< top speed since mHiLast (a run's speed before confirmation)

    // Segments
    Motion    mState = Motion::Idle;
    RunStats  mRun;
    LiftStats mLift;
    Cum       mSegStartCum;
    RunStats  mLastRun;
    LiftStats mLastLift;
    LiftStats mPrevLift;          ///< the one before mLastLift (restored on a merge)
    Cum       mLastLiftStartCum;
    bool      mRunSinceLastLift = true;
    uint32_t  mLastLiftEndT     = 0;
    bool      mLiftEndPending   = false;   ///< closed, but could still be merged
    uint32_t  mLiftLastUpT      = 0;

    // Closed-segment aggregates
    uint16_t mRuns = 0, mLifts = 0;
    float    mVertical = 0.0f, mLiftVertical = 0.0f, mDistance = 0.0f;
    float    mMaxSpeed = 0.0f, mLongestRun = 0.0f;
    uint32_t mRunTime = 0, mLiftTime = 0;

    void  conditionAltitude(const Sample& s);
    float filteredSpeed(const Sample& s);
    void  updateExtremes(float speed, bool movingDown, bool still);
    void  resetHi();
    void  resetLo();
    Mark  markNow() const;

    void openRun();
    void closeRun();
    void openLift();
    void closeLift();
    void flushLiftEnd();

    static uint8_t popcount(uint32_t v);
};

} // namespace Ski

#endif // SKI_RUN_TRACKER_HPP
