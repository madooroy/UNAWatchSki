/**
 ******************************************************************************
 * @file    SkiDaySim.hpp
 * @brief   A made-up day at a made-up resort, one second at a time, as the
 *          watch's sensors would report it.
 *
 * There is no snow before the competition deadline, so the app ships with a
 * demo day. It drives the real pipeline (run tracker, storm watch, FIT file,
 * screens) in the simulator, for the screenshots and the video, and lets
 * anyone with a watch try the app in October. The host tests use it too,
 * with the ground truth it records, to check the run counting.
 *
 * The resort ("Demo resort", base 1250 m, summit 2420 m)
 * -------------------------------------------------------
 *   Gondola   base village -> mid station, 710 m up, ~7.5 min. GPS is lost
 *             inside the cabin for most of the ride.
 *   Chairlift chair base -> summit, 475 m up, ~8 min.
 *   T-bar     nursery slope, 135 m up, ~3 min.
 *   Runs      Summit red (steep, fast), Summit blue (long, with a flat
 *             traverse), Nursery (gentle), Valley run (710 m home, with a
 *             cat track).
 *
 * The day (09:00 to about 14:30)
 * ------------------------------
 *   gondola up, six chairlift laps (a regroup stop, a fall, one flat-out
 *   red), three T-bar laps on the nursery slope with a slow friend, lunch on
 *   the lodge terrace, seven more chair laps, then the valley run home.
 *   17 runs, 17 lift rides; the storm warning comes after lunch and the
 *   day ends early, about 14:30.
 *
 * Weather: steady until noon, then (storm option) pressure falls 1.9 hPa an
 * hour. The chair-base queue is visited all day, which is what lets the
 * storm watch see it.
 *
 * Sensor model: GPS position noise (white + slowly wandering), GPS speed
 * noise, GPS altitude noise, a cold start, loss in the gondola; pressure
 * noise with gusts on the lifts. Deterministic for a given seed.
 *
 * Plain C++, no SDK dependency; it steps on demand (no big tables), so it is
 * small enough to run on the watch.
 ******************************************************************************
 */

#ifndef SKI_SKI_DAY_SIM_HPP
#define SKI_SKI_DAY_SIM_HPP

#include <cstdint>

#include "Ski/RunTracker.hpp"

namespace Ski
{

class SkiDaySim
{
public:
    struct Options {
        uint32_t seed       = 1;
        bool     storm      = true;   ///< pressure falls in the afternoon
        float    speedScale = 1.0f;   ///< < 1: a slower skier on the same day
        bool     gpsLoss    = true;   ///< lose GPS in the gondola cabin
        bool     moreStops  = false;  ///< extra falls and waits on every run
    };

    /// One second of sensor data.
    struct Output {
        uint32_t t           = 0;      ///< seconds since 00:00 local
        double   lat         = 0.0;
        double   lon         = 0.0;
        float    gpsAlt      = 0.0f;
        bool     fix         = false;
        float    speed       = 0.0f;   ///< GPS ground speed, m/s
        bool     speedValid  = false;
        float    dist        = 0.0f;   ///< GPS distance since last second, m
        float    pressurePa  = 0.0f;   ///< station pressure
        float    seaLevelPa  = 0.0f;   ///< sea-level pressure as the kernel would estimate it
        // Ground truth (tests only)
        float    trueAlt     = 0.0f;
        Motion   truth       = Motion::Idle;
    };

    struct TruthRun {
        uint32_t tStart = 0, tEnd = 0;
        float    vertical = 0.0f, distance = 0.0f, maxSpeed = 0.0f;
    };
    struct TruthLift {
        uint32_t tStart = 0, tEnd = 0;
        float    gain = 0.0f;
    };

    static constexpr uint8_t  kMaxTruth  = 24;
    static constexpr uint32_t kDayStartS = 9 * 3600;

    SkiDaySim();
    explicit SkiDaySim(const Options& opt);

    void reset();
    /// Next second. Returns false once the day is over (out is not written).
    bool step(Output& out);
    bool done() const { return mLeg >= legCount(); }

    /// 0..1000 through the day (for a progress readout).
    uint16_t progressPermille() const;
    uint32_t now() const { return mT; }

    uint8_t          truthRuns() const        { return mTruthRuns; }
    uint8_t          truthLifts() const       { return mTruthLifts; }
    const TruthRun&  truthRun(uint8_t i) const  { return mRuns[i]; }
    const TruthLift& truthLift(uint8_t i) const { return mLifts[i]; }

    static uint8_t  legCount();
    static uint32_t plannedDurationS();

private:
    struct Rng {
        uint32_t s = 1;
        uint32_t next();
        float    uniform();          ///< [0, 1)
        float    gauss();            ///< N(0, 1)
    };

    Options  mOpt;
    Rng      mRng;
    uint32_t mT = 0;

    // Leg state
    uint8_t  mLeg      = 0;
    bool     mLegInit  = false;
    float    mS        = 0.0f;   ///< m along the leg's path (horizontal)
    float    mV        = 0.0f;   ///< m/s progress speed
    uint32_t mLegT     = 0;      ///< s into the leg
    uint32_t mStopLeft = 0;      ///< s left in a stop
    uint8_t  mStopsDone = 0;
    float    mX = 0.0f, mY = 0.0f, mZ = 0.0f;   ///< true position, m (ENU) and altitude
    float    mTurnPhase = 0.0f;

    // GPS error state
    float    mGmX = 0.0f, mGmY = 0.0f, mGmZ = 0.0f;
    float    mLastGpsX = 0.0f, mLastGpsY = 0.0f;

    // Ground truth
    TruthRun  mRuns[kMaxTruth];
    TruthLift mLifts[kMaxTruth];
    uint8_t   mTruthRuns = 0, mTruthLifts = 0;
    float     mSegStartZ = 0.0f;
    float     mPrevX = 0.0f, mPrevY = 0.0f;

    void  beginLeg();
    void  endLeg();
    void  advance(float& trueSpeed);
    float seaLevelPaAt(uint32_t t) const;
};

} // namespace Ski

#endif // SKI_SKI_DAY_SIM_HPP
