// Storm watch: only like-for-like pressure comparisons may raise an alert.

#include "Ski/StormWatch.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace Ski;

namespace
{

constexpr double kLat = 46.0;
constexpr double kLon = 7.0;
constexpr double kMPerDegLat = 111320.0;

/// ISA station pressure for a sea-level pressure and an altitude.
float stationPa(float p0, float alt)
{
    return p0 * std::pow(1.0f - alt / 44330.77f, 5.25588f);
}

struct Feed {
    StormWatch sw;
    uint32_t   t = 9 * 3600;
    int        alerts = 0;
    uint32_t   firstAlertT = 0;

    /// Sea-level pressure at time t (Pa): the weather.
    float (*weather)(uint32_t) = [](uint32_t) { return 101500.0f; };

    void at(double northM, double eastM, float alt, uint32_t sec, float speed)
    {
        for (uint32_t i = 0; i < sec; ++i, ++t) {
            const double lat = kLat + northM / kMPerDegLat;
            const double lon = kLon + eastM / (kMPerDegLat * std::cos(kLat * 3.14159265 / 180.0));
            sw.add(t, stationPa(weather(t), alt), true, speed, true, lat, lon, true);
            if (sw.takeAlert()) {
                if (alerts++ == 0) firstAlertT = t;
            }
        }
    }
    void standAt(double n, double e, float alt, uint32_t sec) { at(n, e, alt, sec, 0.1f); }
    void move(uint32_t sec) { at(5000, 5000, 1800, sec, 8.0f); }   // somewhere else, moving
};

float falling2(uint32_t t)   // -2 hPa/h from 10:00 (6 hPa / 3 h)
{
    return t < 36000 ? 101500.0f : 101500.0f - 200.0f * static_cast<float>(t - 36000) / 3600.0f;
}

float steady(uint32_t) { return 101500.0f; }

} // namespace

TEST(StormWatch, LodgeAllAfternoonWithFallingPressureAlerts)
{
    Feed f;
    f.weather = falling2;
    f.standAt(0, 0, 1900, 6 * 3600);
    EXPECT_GE(f.alerts, 1);
    EXPECT_EQ(f.sw.trend().level, StormWatch::Level::Storm);
    EXPECT_NEAR(f.sw.trend().hpaPer3h, -6.0f, 0.3f);
    // Not before an hour of readings, and well within two hours of the fall.
    EXPECT_GE(f.firstAlertT, 36000u + 3600u);
    EXPECT_LE(f.firstAlertT, 36000u + 2 * 3600u);
}

TEST(StormWatch, ChangingAltitudeIsNotWeather)
{
    // Steady weather, but every stop is at a different height: 25 m apart is
    // already 3 hPa. Nothing may be compared across them.
    Feed f;
    f.weather = steady;
    for (int lap = 0; lap < 20; ++lap) {
        f.standAt(lap * 300.0, 0, 1500.0f + 100.0f * static_cast<float>(lap % 7), 300);
        f.move(600);
    }
    EXPECT_EQ(f.alerts, 0);
    EXPECT_EQ(f.sw.trend().level, StormWatch::Level::Learning);
}

TEST(StormWatch, RepeatedLiftQueueReadingsSeeTheStorm)
{
    // The normal ski day: back at the same chair base every 20 minutes,
    // skiing and riding (moving, at other heights) in between.
    Feed f;
    f.weather = falling2;
    for (int lap = 0; lap < 18; ++lap) {
        f.standAt(10.0 * (lap % 3), 5.0 * (lap % 2), 1945, 180);   // the queue, a few metres apart
        f.move(1020);
    }
    EXPECT_GE(f.alerts, 1);
    EXPECT_LE(f.firstAlertT, 36000u + 2 * 3600u + 1200u);
    EXPECT_EQ(f.sw.trend().spots, 1);   // only the queue: moving never counts
}

TEST(StormWatch, SteadyQueueReadingsStaySteady)
{
    Feed f;
    f.weather = steady;
    for (int lap = 0; lap < 18; ++lap) {
        f.standAt(0, 0, 1945, 180);
        f.move(1020);
    }
    EXPECT_EQ(f.alerts, 0);
    EXPECT_EQ(f.sw.trend().level, StormWatch::Level::Steady);
    EXPECT_NEAR(f.sw.trend().hpaPer3h, 0.0f, 0.3f);
}

TEST(StormWatch, ShortStopsTakeNoReadings)
{
    Feed f;
    for (int i = 0; i < 50; ++i) {
        f.standAt(0, 0, 1945, 60);   // under the 90 s needed
        f.move(60);
    }
    EXPECT_EQ(f.sw.trend().readings, 0);
}

TEST(StormWatch, NoGpsNoReadings)
{
    StormWatch sw;
    for (uint32_t t = 0; t < 7200; ++t) {
        sw.add(t, 80000.0f, true, 0.0f, false, 0, 0, false);
    }
    EXPECT_EQ(sw.trend().readings, 0);
    EXPECT_EQ(sw.trend().level, StormWatch::Level::Learning);
}

TEST(StormWatch, AlertRepeatsHourlyWhileItLasts)
{
    Feed f;
    f.weather = falling2;
    f.standAt(0, 0, 1900, 6 * 3600);   // readings every 10 min, falling all along
    // First alert ~1.2 h in, then hourly: 10:00 + 6 h covers about four.
    EXPECT_GE(f.alerts, 3);
    EXPECT_LE(f.alerts, 5);
}

TEST(StormWatch, ThresholdZeroNeverAlerts)
{
    Feed f;
    f.sw.setThreshold(0.0f);
    f.weather = falling2;
    f.standAt(0, 0, 1900, 5 * 3600);
    EXPECT_EQ(f.alerts, 0);
    EXPECT_EQ(f.sw.trend().level, StormWatch::Level::FallingFast);
}

TEST(StormWatch, GustsInTheMedianDoNotCount)
{
    Feed f;
    f.weather = steady;
    // A gust of +-50 Pa (4 m of altitude) every 7 s while standing.
    for (int lap = 0; lap < 12; ++lap) {
        for (uint32_t i = 0; i < 200; ++i, ++f.t) {
            const float gust = (i % 7 == 0) ? ((i % 14 == 0) ? 50.0f : -50.0f) : 0.0f;
            f.sw.add(f.t, stationPa(101500.0f, 1945) + gust, true, 0.1f, true, kLat, kLon, true);
        }
        f.move(1000);
    }
    EXPECT_EQ(f.alerts, 0);
    EXPECT_NEAR(f.sw.trend().hpaPer3h, 0.0f, 0.2f);
}

TEST(StormWatch, ShufflingInTheQueueKeepsTheStop)
{
    // Standing at 0.1 m/s with a 1 m/s shuffle forward every 20 s.
    StormWatch sw;
    for (uint32_t t = 0; t < 200; ++t) {
        const float v = (t % 20 == 0) ? 1.0f : 0.1f;
        sw.add(t, 80000.0f, true, v, true, kLat, kLon, true);
    }
    EXPECT_EQ(sw.trend().readings, 1);
}
