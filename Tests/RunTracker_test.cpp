// Run/lift segmentation on hand-built days: one situation per test.

#include "Ski/RunTracker.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace Ski;

namespace
{

/// Builds a day second by second: altitude with barometer noise, GPS speed.
struct Day : RunTracker::Listener {
    RunTracker trk;
    uint32_t   t     = 36000;
    float      alt   = 1500.0f;
    float      noise = 0.3f;    ///< 1-sigma barometer noise, m
    bool       gps   = true;
    float      drift = 0.0f;    ///< barometer drift with the weather, m/s
    uint32_t   rng   = 12345;

    std::vector<RunStats>  runs;
    std::vector<LiftStats> lifts;
    std::vector<bool>      bestSpeed;
    int runStarts  = 0;
    int liftStarts = 0;

    Day() { trk.setListener(this); }

    void onRunStarted(const RunStats&) override { ++runStarts; }
    void onRunEnded(const RunStats& r, bool bs, bool) override
    {
        runs.push_back(r);
        bestSpeed.push_back(bs);
    }
    void onLiftStarted(const LiftStats&) override { ++liftStarts; }
    void onLiftEnded(const LiftStats& l) override { lifts.push_back(l); }

    float gauss()
    {
        // Box-Muller on a xorshift: deterministic, platform independent.
        auto u = [this] {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            return (static_cast<float>(rng >> 8) + 0.5f) / 16777216.0f;
        };
        const float a = u(), b = u();
        return std::sqrt(-2.0f * std::log(a)) * std::cos(6.2831853f * b);
    }

    void tick(float speed, float dz, float spike = 0.0f)
    {
        alt += dz;
        Sample s;
        s.t          = t++;
        s.alt        = alt + drift * static_cast<float>(t - 36000) + noise * gauss() + spike;
        s.altValid   = true;
        s.speedValid = gps;
        s.speed      = gps ? std::fabs(speed + 0.1f * gauss()) : 0.0f;
        s.dist       = gps ? speed : 0.0f;
        trk.add(s);
    }

    void stand(uint32_t sec)
    {
        for (uint32_t i = 0; i < sec; ++i) tick(0.0f, 0.0f);
    }
    void lift(float gain, float vz = 1.0f, float v = 4.5f)
    {
        const auto n = static_cast<uint32_t>(gain / vz);
        for (uint32_t i = 0; i < n; ++i) tick(v, vz);
    }
    void ski(float drop, float vz = 2.0f, float v = 12.0f)
    {
        const auto n = static_cast<uint32_t>(drop / vz);
        for (uint32_t i = 0; i < n; ++i) tick(v, -vz);
    }
    void walk(float dz, uint32_t sec, float v = 1.2f)
    {
        for (uint32_t i = 0; i < sec; ++i) tick(v, dz / static_cast<float>(sec));
    }
    void skate(uint32_t sec, float v = 3.0f)
    {
        for (uint32_t i = 0; i < sec; ++i) tick(v, 0.0f);
    }
    /// queue, lift, strap in at the top
    void lap(float vert, uint32_t queue = 120)
    {
        stand(queue);
        lift(vert);
        stand(60);
    }
};

} // namespace

TEST(RunTracker, OneLapIsOneLiftAndOneRun)
{
    Day d;
    d.stand(120);
    d.lift(400);
    d.stand(60);
    const uint32_t pushOff = d.t;
    d.ski(400);
    const uint32_t bottom = d.t;
    d.stand(120);
    d.lift(400);

    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(d.runs[0].vertical(), 400.0f, 4.0f);
    // Dated back to the push-off, and ended on arrival at the queue.
    EXPECT_NEAR(static_cast<double>(d.runs[0].tStart), pushOff, 5);
    EXPECT_NEAR(static_cast<double>(d.runs[0].tEnd), bottom, 5);
    EXPECT_NEAR(d.runs[0].distance, 200 * 12.0f, 100.0f);
    EXPECT_NEAR(d.runs[0].maxSpeed, 12.0f, 0.5f);
    EXPECT_NEAR(d.runs[0].avgSpeed(), 12.0f, 0.5f);

    ASSERT_EQ(d.lifts.size(), 1u);
    EXPECT_NEAR(d.lifts[0].gain(), 400.0f, 4.0f);
    EXPECT_EQ(d.trk.motion(), Motion::Lift);
    EXPECT_EQ(d.trk.totals().lifts, 2);
    EXPECT_EQ(d.trk.totals().runs, 1);
}

TEST(RunTracker, LiftIsDatedFromBoardingToArrival)
{
    Day d;
    d.stand(200);
    const uint32_t board = d.t;
    d.lift(300);
    const uint32_t arrive = d.t;
    d.stand(120);

    // Known at once, but announced only when it can no longer be merged.
    EXPECT_EQ(d.trk.lastLift().number, 1);
    EXPECT_EQ(d.lifts.size(), 0u);
    d.trk.finish();
    ASSERT_EQ(d.lifts.size(), 1u);
    EXPECT_NEAR(static_cast<double>(d.lifts[0].tStart), board, 8);
    EXPECT_NEAR(static_cast<double>(d.lifts[0].tEnd), arrive, 5);
    EXPECT_EQ(d.trk.motion(), Motion::Idle);
}

TEST(RunTracker, ShortStopsStayInTheRun)
{
    Day d;
    d.lap(500);
    d.ski(150);
    d.stand(25);    // a fall
    d.ski(150);
    d.stand(90);    // waiting for a friend
    d.ski(200);
    d.lap(500);
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(d.runs[0].vertical(), 500.0f, 5.0f);
    // Moving time leaves the stops out; the run's time keeps them.
    EXPECT_NEAR(d.runs[0].movingTime, 250.0f, 10.0f);
    EXPECT_NEAR(static_cast<double>(d.runs[0].duration()), 250 + 25 + 90, 12);
}

TEST(RunTracker, FourMinuteWaitMidRunStaysInTheRun)
{
    // Waiting for a slower friend half way down is part of the same run.
    Day d;
    d.lap(500);
    d.ski(250);
    d.stand(240);
    d.ski(250);
    d.lap(500);
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(d.runs[0].vertical(), 500.0f, 5.0f);
}

TEST(RunTracker, BreakThenSlowRunOutStaysOneRun)
{
    // From a real day at Jay Peak: 70 m down, an 11-minute break, then 34 m
    // down a gentle run-out at 0.2 m/s to the lift. Garmin counts one run of
    // 107 m; so must we.
    Day d;
    d.lap(300);
    d.ski(70);
    d.stand(690);
    d.walk(-34.0f, 180);
    d.stand(60);
    d.lap(300);
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(d.runs[0].vertical(), 104.0f, 4.0f);
}

TEST(RunTracker, LongStopEndsTheRun)
{
    Day d;
    d.lap(400);
    d.ski(200);
    d.stand(1800);  // lunch half way down
    d.ski(200);
    d.stand(60);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 2u);
    EXPECT_NEAR(d.runs[0].vertical(), 200.0f, 4.0f);
    EXPECT_NEAR(d.runs[1].vertical(), 200.0f, 4.0f);
}

TEST(RunTracker, WalkingBackUpEndsTheRun)
{
    Day d;
    d.lap(500);
    d.ski(300);
    d.walk(+15.0f, 200);   // walk up to the other lift station
    d.stand(60);
    d.ski(300);
    d.stand(60);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 2u);
    EXPECT_NEAR(d.runs[0].vertical(), 300.0f, 4.0f);
    EXPECT_NEAR(d.runs[1].vertical(), 300.0f, 4.0f);
    EXPECT_TRUE(d.lifts.size() == 1u);   // the walk was not a lift
}

TEST(RunTracker, FlatSkateToTheLiftIsNotRunTime)
{
    Day d;
    d.lap(300);
    d.ski(300);
    const uint32_t bottom = d.t;
    d.skate(200);   // 600 m across the flat to the next lift
    d.stand(60);
    d.lift(300);
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(static_cast<double>(d.runs[0].tEnd), bottom, 6);
    EXPECT_NEAR(d.runs[0].distance, 150 * 12.0f, 80.0f);
}

TEST(RunTracker, GondolaStoppedInTheWindIsOneLift)
{
    Day d;
    d.stand(120);
    d.lift(300, 2.0f, 6.0f);
    d.stand(150);   // hold
    d.lift(300, 2.0f, 6.0f);
    d.stand(60);
    d.ski(500);
    d.stand(60);
    d.trk.finish();
    ASSERT_EQ(d.lifts.size(), 1u);
    EXPECT_NEAR(d.lifts[0].gain(), 600.0f, 6.0f);
    EXPECT_EQ(d.liftStarts, 1);
    ASSERT_EQ(d.runs.size(), 1u);
}

TEST(RunTracker, TwoLiftsWithARunBetweenAreNotMerged)
{
    Day d;
    d.lap(200, 60);
    d.ski(60);       // a short run to the next lift
    d.stand(30);
    d.lift(200);
    d.stand(60);
    d.trk.finish();
    EXPECT_EQ(d.lifts.size(), 2u);
    EXPECT_EQ(d.runs.size(), 1u);
}

TEST(RunTracker, WithoutGpsRunsAreStillCounted)
{
    Day d;
    d.gps = false;
    for (int i = 0; i < 3; ++i) {
        d.lap(400);
        d.ski(400);
    }
    d.stand(240);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 3u);
    ASSERT_EQ(d.lifts.size(), 3u);
    for (const auto& r : d.runs) {
        EXPECT_NEAR(r.vertical(), 400.0f, 5.0f);
        EXPECT_EQ(r.distance, 0.0f);
        EXPECT_EQ(r.maxSpeed, 0.0f);
    }
}

TEST(RunTracker, BarometerSpikesAreIgnored)
{
    Day d;
    d.noise = 0.8f;
    for (uint32_t i = 0; i < 1800; ++i) {
        // A door slam or gust: one sample tens of metres off, now and then.
        const float spike = (i % 97 == 0) ? -30.0f : (i % 151 == 0 ? 25.0f : 0.0f);
        d.tick(0.0f, 0.0f, spike);
    }
    d.trk.finish();
    EXPECT_EQ(d.runs.size(), 0u);
    EXPECT_EQ(d.lifts.size(), 0u);
    EXPECT_EQ(d.runStarts + d.liftStarts, 0);
}

TEST(RunTracker, SmallDropsAreNotRuns)
{
    Day d;
    d.lap(300);
    d.ski(20);   // 20 m down to another lift
    d.stand(60);
    d.trk.finish();
    EXPECT_EQ(d.runs.size(), 0u);
}

TEST(RunTracker, HikingUpIsNotALift)
{
    Day d;
    d.stand(60);
    d.walk(+60.0f, 400);   // boot-packing a ridge, 0.15 m/s
    d.stand(60);
    d.trk.finish();
    EXPECT_EQ(d.lifts.size(), 0u);
    EXPECT_EQ(d.liftStarts, 0);
}

TEST(RunTracker, TbarAndBeginnerRunsCount)
{
    Day d;
    d.stand(60);
    d.lift(120, 0.7f, 3.0f);   // T-bar
    d.stand(30);
    d.ski(100, 0.4f, 4.0f);    // a slow beginner
    d.stand(90);
    d.lift(120, 0.7f, 3.0f);
    d.trk.finish();
    ASSERT_EQ(d.lifts.size(), 2u);
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(d.runs[0].vertical(), 100.0f, 4.0f);
}

TEST(RunTracker, TopSpeedIgnoresGpsGlitches)
{
    Day d;
    d.lap(300);
    for (int i = 0; i < 150; ++i) {
        float v = 12.0f;
        if (i == 50) v = 60.0f;   // impossible: discarded outright
        if (i == 90) v = 30.0f;   // a single spike: the median removes it
        d.tick(v, -2.0f);
    }
    d.stand(120);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_LT(d.runs[0].maxSpeed, 13.0f);
}

TEST(RunTracker, LunchBelowTheTopStartsTheRunAfterLunch)
{
    Day d;
    d.lap(400);
    d.walk(-8.0f, 60);   // down to the summit restaurant
    d.drift = -0.003f;   // pressure rising over lunch: the barometer reads lower
    d.stand(3600);
    d.drift = 0.0f;
    const uint32_t leave = d.t;
    d.ski(390);
    d.stand(100);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 1u);
    EXPECT_NEAR(static_cast<double>(d.runs[0].tStart), leave, 10);
    EXPECT_LT(d.runs[0].duration(), 260u);
}

TEST(RunTracker, AfterLunchTheLiftStartsAtBoardingDespiteDrift)
{
    Day d;
    d.lap(300);
    d.ski(300);
    d.drift = 0.003f;   // pressure falling: the barometer reads ~11 m higher an hour on
    d.stand(3600);
    const uint32_t board = d.t;
    d.lift(300);
    d.stand(60);
    d.trk.finish();
    ASSERT_EQ(d.lifts.size(), 2u);
    EXPECT_NEAR(static_cast<double>(d.lifts[1].tStart), board, 8);
}

TEST(RunTracker, TimeSplitAddsUp)
{
    Day d;
    for (int i = 0; i < 4; ++i) {
        d.lap(300);
        d.ski(300);
    }
    d.stand(200);
    const DayTotals live = d.trk.totals();
    EXPECT_EQ(live.runTime + live.liftTime + live.idleTime(), live.totalTime);
    d.trk.finish();
    const DayTotals t = d.trk.totals();
    EXPECT_EQ(t.runs, 4);
    EXPECT_EQ(t.lifts, 4);
    EXPECT_NEAR(t.vertical, 1200.0f, 12.0f);
    EXPECT_NEAR(static_cast<double>(t.runTime), 4 * 150, 30);
    EXPECT_NEAR(static_cast<double>(t.liftTime), 4 * 300, 40);
    EXPECT_EQ(t.runTime + t.liftTime + t.idleTime(), t.totalTime);
}

TEST(RunTracker, PausesAreNotTrackedTime)
{
    Day d;
    d.stand(600);
    d.t += 3600;   // paused for an hour
    d.stand(600);
    EXPECT_NEAR(static_cast<double>(d.trk.totals().totalTime), 1200, 10);
}

TEST(RunTracker, BestSpeedIsFlagged)
{
    Day d;
    d.lap(300);
    d.ski(300, 2.0f, 10.0f);
    d.lap(300);
    d.ski(300, 2.5f, 15.0f);
    d.lap(300);
    d.ski(300, 2.0f, 11.0f);
    d.stand(200);
    d.trk.finish();
    ASSERT_EQ(d.runs.size(), 3u);
    EXPECT_TRUE(d.bestSpeed[0]);
    EXPECT_TRUE(d.bestSpeed[1]);
    EXPECT_FALSE(d.bestSpeed[2]);
    EXPECT_NEAR(d.trk.totals().maxSpeed, 15.0f, 0.5f);
}

TEST(RunTracker, LiveRunIsVisibleBeforeItEnds)
{
    Day d;
    d.lap(400);
    d.ski(200);
    ASSERT_EQ(d.trk.motion(), Motion::Run);
    EXPECT_EQ(d.trk.currentRun().number, 1);
    EXPECT_NEAR(d.trk.currentRun().vertical(), 200.0f, 5.0f);
    EXPECT_EQ(d.trk.totals().runs, 1);
    EXPECT_NEAR(d.trk.totals().vertical, 200.0f, 5.0f);
}
