// Whole simulated days through the run tracker and storm watch, compared
// with the simulator's ground truth -- the same pipeline the watch runs.

#include "Ski/RunTracker.hpp"
#include "Ski/SkiDaySim.hpp"
#include "Ski/StormWatch.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace Ski;

namespace
{

struct Result : RunTracker::Listener {
    std::vector<RunStats>  runs;
    std::vector<LiftStats> lifts;
    std::vector<uint32_t>  alerts;
    DayTotals              totals;
    SkiDaySim              sim;

    explicit Result(const SkiDaySim::Options& o)
        : sim(o)
    {}

    void onRunEnded(const RunStats& r, bool, bool) override { runs.push_back(r); }
    void onLiftEnded(const LiftStats& l) override { lifts.push_back(l); }

    void play()
    {
        RunTracker trk;
        StormWatch storm;
        trk.setListener(this);
        SkiDaySim::Output s;
        float p0 = 0;
        while (sim.step(s)) {
            if (p0 == 0) {
                p0 = s.seaLevelPa;   // as the service does: fixed for the day
            }
            Sample in;
            in.t          = s.t;
            in.alt        = 44330.77f * (1.0f - std::pow(s.pressurePa / p0, 1.0f / 5.25588f));
            in.altValid   = true;
            in.speed      = s.speed;
            in.speedValid = s.speedValid;
            in.dist       = s.dist;
            trk.add(in);
            storm.add(s.t, s.pressurePa, true, s.speed, s.speedValid, s.lat, s.lon, s.fix);
            if (storm.takeAlert()) {
                alerts.push_back(s.t);
            }
        }
        trk.finish();
        totals = trk.totals();
    }
};

void expectMatchesTruth(const Result& r)
{
    ASSERT_EQ(r.runs.size(), r.sim.truthRuns());
    ASSERT_EQ(r.lifts.size(), r.sim.truthLifts());
    for (size_t i = 0; i < r.runs.size(); ++i) {
        const auto& got  = r.runs[i];
        const auto& want = r.sim.truthRun(static_cast<uint8_t>(i));
        SCOPED_TRACE(testing::Message() << "run " << i + 1);
        EXPECT_NEAR(got.vertical(), want.vertical, 0.02f * want.vertical + 3.0f);
        EXPECT_NEAR(static_cast<double>(got.tStart), want.tStart, 15);
        EXPECT_NEAR(static_cast<double>(got.tEnd), want.tEnd, 15);
        EXPECT_NEAR(got.distance, want.distance, 0.03f * want.distance + 20.0f);
        // A peak lasting a second or two reads low through the spike filter
        // (deliberately: one GPS glitch must not set a top speed), never high.
        EXPECT_GT(got.maxSpeed, 0.85f * want.maxSpeed);
        EXPECT_LT(got.maxSpeed, 1.05f * want.maxSpeed);
    }
    for (size_t i = 0; i < r.lifts.size(); ++i) {
        const auto& got  = r.lifts[i];
        const auto& want = r.sim.truthLift(static_cast<uint8_t>(i));
        SCOPED_TRACE(testing::Message() << "lift " << i + 1);
        EXPECT_NEAR(got.gain(), want.gain, 0.02f * want.gain + 3.0f);
        EXPECT_NEAR(static_cast<double>(got.tStart), want.tStart, 15);
        EXPECT_NEAR(static_cast<double>(got.tEnd), want.tEnd, 15);
    }
}

} // namespace

TEST(SkiDay, DemoDayMatchesTheTruth)
{
    Result r(SkiDaySim::Options{});
    r.play();
    expectMatchesTruth(r);
    EXPECT_EQ(r.sim.truthRuns(), 17);
    EXPECT_EQ(r.totals.runTime + r.totals.liftTime + r.totals.idleTime(), r.totals.totalTime);
}

TEST(SkiDay, DemoDayStormIsWarnedAfterLunch)
{
    Result r(SkiDaySim::Options{});
    r.play();
    ASSERT_FALSE(r.alerts.empty());
    // The fall starts at noon; the chair-base queue is next visited after lunch.
    EXPECT_GT(r.alerts.front(), 13u * 3600u);
    EXPECT_LT(r.alerts.front(), 14u * 3600u + 1800u);
}

TEST(SkiDay, FairWeatherDayHasNoStorm)
{
    for (uint32_t seed = 1; seed <= 5; ++seed) {
        SkiDaySim::Options o;
        o.seed  = seed;
        o.storm = false;
        Result r(o);
        r.play();
        EXPECT_TRUE(r.alerts.empty()) << "seed " << seed;
        expectMatchesTruth(r);
    }
}

TEST(SkiDay, SlowSkierWithExtraStops)
{
    SkiDaySim::Options o;
    o.seed       = 7;
    o.speedScale = 0.5f;
    o.moreStops  = true;
    Result r(o);
    r.play();
    expectMatchesTruth(r);
}

TEST(SkiDay, FastSkier)
{
    SkiDaySim::Options o;
    o.seed       = 8;
    o.speedScale = 1.3f;
    Result r(o);
    r.play();
    expectMatchesTruth(r);
}

TEST(SkiDay, WithAndWithoutGpsInTheGondola)
{
    for (bool loss : { true, false }) {
        SkiDaySim::Options o;
        o.seed    = 11;
        o.gpsLoss = loss;
        Result r(o);
        r.play();
        expectMatchesTruth(r);
    }
}

TEST(SkiDay, SimulatorIsDeterministic)
{
    SkiDaySim a, b;
    SkiDaySim::Output x, y;
    while (a.step(x)) {
        ASSERT_TRUE(b.step(y));
        ASSERT_EQ(x.t, y.t);
        ASSERT_EQ(x.pressurePa, y.pressurePa);
        ASSERT_EQ(x.lat, y.lat);
    }
    EXPECT_FALSE(b.step(y));
    EXPECT_EQ(a.progressPermille(), 1000);
}
