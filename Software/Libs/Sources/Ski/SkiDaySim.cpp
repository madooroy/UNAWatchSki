/**
 ******************************************************************************
 * @file    SkiDaySim.cpp
 * @brief   The demo resort, the scripted day and the sensor model.
 ******************************************************************************
 */

#include "Ski/SkiDaySim.hpp"

#include <algorithm>
#include <cmath>

namespace Ski
{

namespace
{

constexpr float  kPi    = 3.14159265f;
constexpr double kLat0  = 46.0412;   // somewhere in the Alps; the resort is fictional
constexpr double kLon0  = 7.0583;
constexpr double kMPerDegLat = 111320.0;

struct P {
    float x, y, z;   // m east, m north, m altitude
};

// -- The resort -----------------------------------------------------------------
// clang-format off
const P kGondola[]   = { {    0,    0, 1250 }, { 1400, 2300, 1960 } };
const P kChair[]     = { { 1650, 2450, 1945 }, { 2900, 4300, 2420 } };
const P kTbar[]      = { { 1150, 2050, 1945 }, {  900, 2500, 2080 } };

const P kRed[]       = { { 2900, 4300, 2420 }, { 2600, 3900, 2250 }, { 2250, 3400, 2120 },
                         { 1950, 2900, 2010 }, { 1650, 2450, 1945 } };
const P kBlue[]      = { { 2900, 4300, 2420 }, { 3300, 3900, 2330 }, { 3200, 3300, 2210 },
                         { 2700, 2900, 2130 }, { 2200, 2700, 2112 },   // flat traverse
                         { 1650, 2450, 1945 } };
const P kNursery[]   = { {  900, 2500, 2080 }, { 1000, 2300, 2010 }, { 1150, 2050, 1945 } };
const P kValley[]    = { { 1400, 2300, 1960 }, { 1100, 1800, 1830 }, {  800, 1300, 1650 },
                         {  500,  900, 1590 },                          // cat track
                         {  300,  400, 1420 }, {    0,    0, 1250 } };

const P kWalkIn[]    = { { -100,  -60, 1250 }, {    0,    0, 1250 } };
const P kMidToChair[]= { { 1400, 2300, 1960 }, { 1650, 2450, 1945 } };
const P kChairToT[]  = { { 1650, 2450, 1945 }, { 1150, 2050, 1945 } };
const P kTToLodge[]  = { { 1150, 2050, 1945 }, { 1420, 2260, 1958 } };
const P kLodgeToChair[] = { { 1420, 2260, 1958 }, { 1650, 2450, 1945 } };
const P kChairToMid[]= { { 1650, 2450, 1945 }, { 1400, 2300, 1960 } };
const P kWalkOut[]   = { {    0,    0, 1250 }, {   80, -120, 1250 } };
// clang-format on

#define PATH(a) a, static_cast<uint8_t>(sizeof(a) / sizeof(a[0]))

enum class Kind : uint8_t { Stand, Queue, Walk, Skate, Lift, Run };

struct Leg {
    Kind     kind;
    const P* path;
    uint8_t  n;
    float    vMin;      ///< Run: speed on gentle ground; others unused
    float    vMax;      ///< Run: speed on steep ground; Lift/Walk/Skate: speed
    uint16_t durS;      ///< Stand/Queue
    float    stopAt;    ///< Run: fraction of the way down to stop (0 = none)
    uint16_t stopS;
    bool     cabin;     ///< Lift: GPS lost inside
};

Leg stand(uint16_t s)  { return { Kind::Stand, nullptr, 0, 0, 0, s, 0, 0, false }; }
Leg queue(uint16_t s)  { return { Kind::Queue, nullptr, 0, 0, 0, s, 0, 0, false }; }
Leg walk(const P* p, uint8_t n, float v)  { return { Kind::Walk, p, n, 0, v, 0, 0, 0, false }; }
Leg skate(const P* p, uint8_t n, float v) { return { Kind::Skate, p, n, 0, v, 0, 0, 0, false }; }
Leg lift(const P* p, uint8_t n, float v, bool cabin = false)
{
    return { Kind::Lift, p, n, 0, v, 0, 0, 0, cabin };
}
Leg run(const P* p, uint8_t n, float vMin, float vMax, float stopAt = 0, uint16_t stopS = 0)
{
    return { Kind::Run, p, n, vMin, vMax, 0, stopAt, stopS, false };
}

// -- The day ----------------------------------------------------------------------
// clang-format off
const Leg kDay[] = {
    walk(PATH(kWalkIn), 1.2f),
    queue(300),
    lift(PATH(kGondola), 6.0f, true),
    stand(40),
    skate(PATH(kMidToChair), 3.0f),
    // Morning on the chair: six laps
    queue(180), lift(PATH(kChair), 4.6f), stand(60), run(PATH(kRed), 4, 14, 0.50f, 45),  // regroup
    queue(150), lift(PATH(kChair), 4.6f), stand(40), run(PATH(kBlue), 4, 12, 0.35f, 20),  // a fall
    queue(210), lift(PATH(kChair), 4.6f), stand(30), run(PATH(kRed), 4, 18),             // flat out
    queue(120), lift(PATH(kChair), 4.6f), stand(90), run(PATH(kBlue), 4, 12),
    queue(170), lift(PATH(kChair), 4.6f), stand(45), run(PATH(kRed), 4, 14, 0.70f, 30),
    queue(130), lift(PATH(kChair), 4.6f), stand(35), run(PATH(kBlue), 4, 12),
    // The nursery slope with a slow friend: three T-bar laps
    skate(PATH(kChairToT), 3.0f),
    queue(120), lift(PATH(kTbar), 3.0f), stand(30), run(PATH(kNursery), 2.5f, 6, 0.50f, 60),
    queue(90),  lift(PATH(kTbar), 3.0f), stand(20), run(PATH(kNursery), 2.5f, 6),
    queue(100), lift(PATH(kTbar), 3.0f), stand(25), run(PATH(kNursery), 2.5f, 6, 0.60f, 40),
    // Lunch on the lodge terrace
    walk(PATH(kTToLodge), 1.2f),
    stand(3600),
    skate(PATH(kLodgeToChair), 2.5f),
    // Afternoon: seven chair laps while the weather turns
    queue(200), lift(PATH(kChair), 4.6f), stand(45), run(PATH(kRed), 4, 14),
    queue(160), lift(PATH(kChair), 4.6f), stand(35), run(PATH(kBlue), 4, 12, 0.60f, 30),
    queue(140), lift(PATH(kChair), 4.6f), stand(50), run(PATH(kRed), 4, 15),
    queue(150), lift(PATH(kChair), 4.6f), stand(40), run(PATH(kBlue), 4, 12),
    queue(120), lift(PATH(kChair), 4.6f), stand(30), run(PATH(kRed), 4, 15, 0.40f, 25),
    queue(110), lift(PATH(kChair), 4.6f), stand(40), run(PATH(kBlue), 4, 12),
    queue(100), lift(PATH(kChair), 4.6f), stand(60), run(PATH(kRed), 4, 14),
    // Home: the valley run, with a photo stop
    walk(PATH(kChairToMid), 1.4f),
    stand(60),
    run(PATH(kValley), 4, 15, 0.40f, 90),
    walk(PATH(kWalkOut), 1.2f),
    stand(30),
};
// clang-format on
constexpr uint8_t kLegs = static_cast<uint8_t>(sizeof(kDay) / sizeof(kDay[0]));

float pathLength(const Leg& l)
{
    float len = 0.0f;
    for (uint8_t i = 1; i < l.n; ++i) {
        len += std::hypot(l.path[i].x - l.path[i - 1].x, l.path[i].y - l.path[i - 1].y);
    }
    return len;
}

/// Point and local grade (drop per horizontal metre, + downhill) at s metres.
P pathAt(const Leg& l, float s, float& grade, float& dirX, float& dirY)
{
    for (uint8_t i = 1; i < l.n; ++i) {
        const P&    a   = l.path[i - 1];
        const P&    b   = l.path[i];
        const float seg = std::hypot(b.x - a.x, b.y - a.y);
        if (s <= seg || i == l.n - 1) {
            const float f = seg > 0 ? std::min(1.0f, std::max(0.0f, s / seg)) : 1.0f;
            grade         = seg > 0 ? (a.z - b.z) / seg : 0.0f;
            dirX          = seg > 0 ? (b.x - a.x) / seg : 0.0f;
            dirY          = seg > 0 ? (b.y - a.y) / seg : 1.0f;
            return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f };
        }
        s -= seg;
    }
    grade = 0.0f;
    dirX  = 0.0f;
    dirY  = 1.0f;
    return l.path[l.n - 1];
}

float approach(float v, float target, float up, float down)
{
    if (target > v) {
        return std::min(target, v + up);
    }
    return std::max(target, v - down);
}

} // namespace

// -- RNG ----------------------------------------------------------------------------

uint32_t SkiDaySim::Rng::next()
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

float SkiDaySim::Rng::uniform()
{
    return static_cast<float>(next() >> 8) / 16777216.0f;
}

float SkiDaySim::Rng::gauss()
{
    const float u1 = std::max(uniform(), 1e-7f);
    const float u2 = uniform();
    return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * kPi * u2);
}

// -- Public -------------------------------------------------------------------------

SkiDaySim::SkiDaySim()
    : SkiDaySim(Options{})
{
}

SkiDaySim::SkiDaySim(const Options& opt)
    : mOpt(opt)
{
    reset();
}

uint8_t SkiDaySim::legCount()
{
    return kLegs;
}

void SkiDaySim::reset()
{
    mRng.s = mOpt.seed ? mOpt.seed * 2654435761u + 1u : 0x9E3779B9u;
    mT        = kDayStartS;
    mLeg      = 0;
    mLegInit  = false;
    mS = mV   = 0.0f;
    mLegT     = 0;
    mStopLeft = 0;
    mStopsDone = 0;
    mX = kDay[0].path[0].x;
    mY = kDay[0].path[0].y;
    mZ = kDay[0].path[0].z;
    mTurnPhase = 0.0f;
    mGmX = mGmY = mGmZ = 0.0f;
    mLastGpsX = mX;
    mLastGpsY = mY;
    mTruthRuns = mTruthLifts = 0;
    mSegStartZ = mZ;
    mPrevX     = mX;
    mPrevY     = mY;
}

uint32_t SkiDaySim::plannedDurationS()
{
    // Not exact (runs depend on speed), but close enough for a progress bar.
    uint32_t total = 0;
    for (uint8_t i = 0; i < kLegs; ++i) {
        const Leg& l = kDay[i];
        switch (l.kind) {
            case Kind::Stand:
            case Kind::Queue: total += l.durS; break;
            case Kind::Run:   total += static_cast<uint32_t>(pathLength(l) / (0.6f * l.vMax)) + l.stopS; break;
            default:          total += static_cast<uint32_t>(pathLength(l) / l.vMax); break;
        }
    }
    return total;
}

uint16_t SkiDaySim::progressPermille() const
{
    const uint32_t planned = plannedDurationS();
    const uint32_t done    = mT - kDayStartS;
    if (this->done() || planned == 0) {
        return 1000;
    }
    return static_cast<uint16_t>(std::min<uint32_t>(999, done * 1000u / planned));
}

float SkiDaySim::seaLevelPaAt(uint32_t t) const
{
    constexpr float  kP0     = 101700.0f;
    constexpr uint32_t kTurn = 12 * 3600;
    if (!mOpt.storm) {
        // Fair weather: a slow rise of 0.3 hPa in 3 h.
        return kP0 + 10.0f * static_cast<float>(t - kDayStartS) / 3600.0f;
    }
    if (t <= kTurn) {
        return kP0;
    }
    return kP0 - 190.0f * static_cast<float>(t - kTurn) / 3600.0f;   // -1.9 hPa/h
}

void SkiDaySim::beginLeg()
{
    const Leg& l = kDay[mLeg];
    mLegInit   = true;
    mS         = 0.0f;
    mV         = 0.0f;
    mLegT      = 0;
    mStopLeft  = 0;
    mStopsDone = 0;
    mSegStartZ = mZ;
    if (l.kind == Kind::Run && mTruthRuns < kMaxTruth) {
        TruthRun& r = mRuns[mTruthRuns];
        r           = TruthRun{};
        r.tStart    = mT;
    } else if (l.kind == Kind::Lift && mTruthLifts < kMaxTruth) {
        TruthLift& f = mLifts[mTruthLifts];
        f            = TruthLift{};
        f.tStart     = mT;
    }
}

void SkiDaySim::endLeg()
{
    const Leg& l = kDay[mLeg];
    if (l.kind == Kind::Run && mTruthRuns < kMaxTruth) {
        mRuns[mTruthRuns].tEnd     = mT;
        mRuns[mTruthRuns].vertical = mSegStartZ - mZ;
        ++mTruthRuns;
    } else if (l.kind == Kind::Lift && mTruthLifts < kMaxTruth) {
        mLifts[mTruthLifts].tEnd = mT;
        mLifts[mTruthLifts].gain = mZ - mSegStartZ;
        ++mTruthLifts;
    }
    ++mLeg;
    mLegInit = false;
}

void SkiDaySim::advance(float& trueSpeed)
{
    const Leg& l = kDay[mLeg];
    trueSpeed    = 0.0f;

    if (l.kind == Kind::Stand || l.kind == Kind::Queue) {
        if (l.kind == Kind::Queue && mLegT % 25 >= 22) {
            // Shuffle forward a little in the queue.
            mX += 0.5f;
            mY += 0.3f;
        }
        if (++mLegT >= l.durS) {
            endLeg();
        }
        return;
    }

    const float len = pathLength(l);
    float grade = 0.0f, dx = 0.0f, dy = 1.0f;
    pathAt(l, mS, grade, dx, dy);

    float target = l.vMax;
    if (l.kind == Kind::Run) {
        const float scale = mOpt.speedScale;
        const float f     = std::min(1.0f, std::max(0.0f, (grade - 0.05f) / 0.25f));
        target = scale * (l.vMin + (l.vMax - l.vMin) * f);
        target *= 1.0f + 0.12f * std::sin(mTurnPhase);
        target *= 1.0f + 0.04f * mRng.gauss();
        mTurnPhase += 2.0f * kPi / 6.0f;

        // Planned stops (and extra ones on a "more stops" day).
        const float frac = len > 0 ? mS / len : 1.0f;
        float    stopAt  = 0.0f;
        uint16_t stopS   = 0;
        if (mStopsDone == 0 && l.stopAt > 0.0f) {
            stopAt = l.stopAt;
            stopS  = l.stopS;
        } else if (mOpt.moreStops && mStopsDone < 2) {
            stopAt = mStopsDone == 0 ? 0.25f : 0.80f;
            stopS  = mStopsDone == 0 ? 35 : 70;
            if (l.stopAt > 0.0f && mStopsDone == 1) {
                stopAt = 0.85f;
            }
        }
        if (mStopLeft > 0) {
            target = 0.0f;
            if (mV < 0.1f && --mStopLeft == 0) {
                ++mStopsDone;
            }
        } else if (stopS > 0 && frac >= stopAt) {
            mStopLeft = stopS;
            target    = 0.0f;
        }
    } else if (l.kind == Kind::Skate) {
        target = l.vMax * (1.0f + 0.1f * mRng.gauss());
    }

    // Slow down for the end of the leg (a run ends in the queue).
    const float left = len - mS;
    target = std::min(target, std::sqrt(std::max(0.0f, 2.0f * 1.5f * left)) + 0.3f);
    if (l.kind == Kind::Lift) {
        mV = approach(mV, target, 1.0f, 10.0f);
    } else {
        mV = approach(mV, std::max(0.0f, target), 2.0f, 3.0f);
    }

    mS = std::min(len, mS + mV);
    P p = pathAt(l, mS, grade, dx, dy);
    if (l.kind == Kind::Run) {
        // Turns: swing across the slope.
        const float off = 4.0f * std::sin(2.0f * kPi * mS / 60.0f);
        p.x += -dy * off;
        p.y += dx * off;
    }
    trueSpeed = std::hypot(p.x - mX, p.y - mY);
    mX = p.x;
    mY = p.y;
    mZ = p.z;

    ++mLegT;
    if (mS >= len - 0.01f && mStopLeft == 0) {
        endLeg();
    }
}

bool SkiDaySim::step(Output& out)
{
    if (mLeg >= kLegs) {
        return false;
    }
    if (!mLegInit) {
        beginLeg();
    }

    const Leg   legNow  = kDay[mLeg];
    const float sBefore = mS;
    const float len     = legNow.path ? pathLength(legNow) : 0.0f;
    float       trueSpeed = 0.0f;
    const uint8_t legBefore = mLeg;
    advance(trueSpeed);
    ++mT;

    // Ground truth for this second.
    out.trueAlt = mZ;
    out.truth   = legNow.kind == Kind::Run ? Motion::Run
                : legNow.kind == Kind::Lift ? Motion::Lift : Motion::Idle;
    if (legNow.kind == Kind::Run) {
        const uint8_t idx = (mLeg != legBefore) ? static_cast<uint8_t>(mTruthRuns - 1) : mTruthRuns;
        if (idx < kMaxTruth) {
            mRuns[idx].distance += trueSpeed;
            mRuns[idx].maxSpeed = std::max(mRuns[idx].maxSpeed, trueSpeed);
        }
    }

    // GPS: cold start, then lost inside the gondola cabin.
    bool fix = (mT - kDayStartS) > 20;
    if (legNow.kind == Kind::Lift && legNow.cabin && mOpt.gpsLoss && len > 0) {
        const float f = sBefore / len;
        if (f > 0.15f && f < 0.85f) {
            fix = false;
        }
    }

    // Position error: white plus a slowly wandering part (Gauss-Markov, 60 s).
    const float a = std::exp(-1.0f / 60.0f);
    const float b = std::sqrt(1.0f - a * a);
    mGmX = a * mGmX + b * 2.5f * mRng.gauss();
    mGmY = a * mGmY + b * 2.5f * mRng.gauss();
    mGmZ = a * mGmZ + b * 5.0f * mRng.gauss();
    const float gx = mX + mGmX + 1.5f * mRng.gauss();
    const float gy = mY + mGmY + 1.5f * mRng.gauss();

    out.t          = mT;
    out.fix        = fix;
    out.lat        = kLat0 + gy / kMPerDegLat;
    out.lon        = kLon0 + gx / (kMPerDegLat * std::cos(kLat0 * kPi / 180.0));
    out.gpsAlt     = mZ + mGmZ + 3.0f * mRng.gauss();
    out.speedValid = fix;
    out.speed      = fix ? std::fabs(trueSpeed + 0.12f * mRng.gauss()) : 0.0f;
    out.dist       = (fix && trueSpeed > 0.3f) ? trueSpeed : 0.0f;
    mLastGpsX      = gx;
    mLastGpsY      = gy;

    // Pressure: ISA at the true altitude for today's sea-level pressure, plus
    // sensor noise, plus gusts on the lifts.
    const float p0    = seaLevelPaAt(mT);
    const float ratio = std::pow(1.0f - mZ / 44330.77f, 5.25588f);
    float       noise = 3.0f * mRng.gauss();
    if (legNow.kind == Kind::Lift) {
        noise += 10.0f * mRng.gauss();
    }
    out.pressurePa = p0 * ratio + noise;
    out.seaLevelPa = p0 + 20.0f;   // the kernel's own estimate is never exact
    return true;
}

} // namespace Ski
