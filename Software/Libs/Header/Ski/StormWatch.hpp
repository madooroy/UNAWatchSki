/**
 ******************************************************************************
 * @file    StormWatch.hpp
 * @brief   Warns of fast-falling air pressure on a ski day, without mistaking
 *          a change of altitude for a change of weather.
 *
 * The problem
 * -----------
 * Air pressure falls about 1.2 hPa for every 10 m you climb, so the usual
 * storm rule (3 hPa in 3 hours) is the same as riding a lift 25 m. Comparing
 * any two readings from a ski day measures your altitude, not the weather,
 * and the barometric altitude cannot be used to correct it: that altitude is
 * itself computed from the pressure.
 *
 * What this does instead
 * ----------------------
 * Pressure is only compared at the same place, which GPS tells us
 * independently of the barometer:
 *
 *  1. A reading is taken only once you have stood still (GPS speed under
 *     0.7 m/s, within 25 m) for 90 s: the median pressure over the last
 *     minute, so a gust or a door slam does not count. Standing longer gives
 *     one more reading every 10 minutes.
 *  2. Each reading is filed under a "spot": a reading within 20 m of a
 *     spot's average position belongs to it (so a spot centres itself on a
 *     lift queue, wherever in the queue you stood).
 *  3. The trend at a spot is the least-squares slope of its readings over the
 *     last 3 hours, once there are three spanning at least two hours. If they
 *     scatter about that line by more than ~0.45 hPa (4 m of height), they
 *     were not all taken at one height -- stops spread along a slope -- and
 *     the spot is not used. The slope is scaled to its sea-level
 *     equivalent (up a mountain the station pressure moves by only p/p0 of
 *     the weather's change, about 80 % at 2000 m; the 3 hPa rule is a
 *     sea-level figure).
 *
 * A ski day supplies these comparisons for free: you queue at the same lift
 * base many times a day. Lunch in the lodge and waiting at the bottom of the
 * nursery slope count too.
 *
 * Levels (rate scaled to hPa per 3 h): Rising >= +1, Steady, Falling <= -1,
 * Falling fast <= -2, Storm <= -threshold (default 3 hPa). The alert fires on
 * entering Storm and repeats at most hourly while it lasts. It re-arms once
 * the fall eases by 1 hPa/3 h.
 *
 * Plain C++, no SDK dependency.
 ******************************************************************************
 */

#ifndef SKI_STORM_WATCH_HPP
#define SKI_STORM_WATCH_HPP

#include <cstdint>

namespace Ski
{

class StormWatch
{
public:
    struct Config {
        float    thresholdHpa   = 3.0f;    ///< alert on this fall per 3 h; 0 = never alert
        float    parkSpeed      = 0.7f;    ///< m/s, standing still
        uint32_t parkMinS       = 90;      ///< s still before a reading
        float    parkRadiusM    = 25.0f;   ///< drift allowed while standing
        float    spotRadiusM    = 20.0f;   ///< readings this close are the same spot
        float    maxScatterPa   = 45.0f;   ///< readings scattered more than this about
                                           ///< their trend were not at one height
        uint32_t readingEveryS  = 600;     ///< extra reading while standing longer
        uint32_t windowS        = 3 * 3600;
        uint32_t minSpanS       = 2 * 3600; ///< readings at a spot must span this
        uint32_t repeatS        = 3600;    ///< repeat the alert at most this often
    };

    enum class Level : uint8_t {
        Learning = 0,   ///< no spot with readings an hour apart yet
        Rising,
        Steady,
        Falling,
        FallingFast,
        Storm,
    };

    struct Reading {
        uint32_t t  = 0;
        float    pa = 0.0f;   ///< station pressure, Pa
    };

    static constexpr uint8_t kMaxSpots        = 12;
    static constexpr uint8_t kReadingsPerSpot = 24;

    struct Spot {
        double   lat = 0.0, lon = 0.0;   ///< average position of its readings
        uint16_t visits = 0;         ///< readings ever filed here (for the average)
        uint8_t  count = 0;          ///< readings held (oldest first)
        Reading  r[kReadingsPerSpot];
        uint32_t lastT = 0;
    };

    struct Trend {
        Level    level      = Level::Learning;
        float    hpaPer3h   = 0.0f;   ///< valid unless Learning
        uint32_t t          = 0;      ///< when it was computed
        uint8_t  spot       = 0xFF;   ///< spot it came from
        uint16_t readings   = 0;      ///< readings taken today
        uint8_t  spots      = 0;      ///< spots in use
        float    pressureHpa = 0.0f;  ///< latest station pressure (any), hPa
    };

    StormWatch();
    explicit StormWatch(const Config& cfg);

    void setThreshold(float hpa) { mCfg.thresholdHpa = hpa; }
    const Config& config() const { return mCfg; }

    void reset();

    /**
     * One sample, about once a second.
     * @param pa     station pressure (Pa), pValid false if none
     * @param speed  GPS ground speed (m/s), speedValid false if none
     * @param fix    lat/lon valid
     */
    void add(uint32_t t, float pa, bool pValid, float speed, bool speedValid,
             double lat, double lon, bool fix);

    const Trend& trend() const { return mTrend; }
    /// True once per alert; the caller plays it.
    bool takeAlert();

    /// The spot with the most readings in the window (for a sparkline), or
    /// nullptr if there is none.
    const Spot* busiestSpot(uint32_t now) const;
    const Spot& spot(uint8_t i) const { return mSpots[i]; }
    uint8_t     spotCount() const { return mSpotCount; }

    static float distanceM(double lat1, double lon1, double lat2, double lon2);

private:
    static constexpr uint8_t kParkBuf = 60;

    Config  mCfg;

    // Standing-still detection
    bool     mParked     = false;    ///< within a still period (any length)
    uint32_t mParkStartT = 0;
    double   mParkLat = 0.0, mParkLon = 0.0;
    uint32_t mLastReadingT = 0;      ///< within this still period, 0 = none yet
    uint8_t  mFastCount  = 0;        ///< consecutive samples over parkSpeed
    float    mPBuf[kParkBuf] = {};
    uint8_t  mPCount = 0, mPPos = 0;

    Spot     mSpots[kMaxSpots];
    uint8_t  mSpotCount = 0;
    Trend    mTrend;

    bool     mArmed       = true;
    bool     mAlertPending = false;
    uint32_t mLastAlertT  = 0;
    bool     mEverAlerted = false;

    void    endPark();
    void    takeReading(uint32_t t);
    uint8_t spotFor(double lat, double lon, uint32_t t);
    bool    slopeAt(const Spot& s, uint32_t now, float& hpaPer3h) const;
    void    evaluate(uint32_t t, uint8_t spot, float rate);
};

const char* levelName(StormWatch::Level l);

} // namespace Ski

#endif // SKI_STORM_WATCH_HPP
