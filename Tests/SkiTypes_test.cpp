// Settings file, day profile, conversions and number formatting.

#include "Ski/Format.hpp"
#include "SkiTypes.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

using namespace Ski;

static std::string f(void (*fn)(char*, size_t, uint16_t, bool), uint16_t v, bool imp)
{
    char b[24];
    fn(b, sizeof(b), v, imp);
    return b;
}

TEST(Format, Speed)
{
    EXPECT_EQ(f(Fmt::speed, 1444, false), "52");   // 14.44 m/s = 52.0 km/h
    EXPECT_EQ(f(Fmt::speed, 1444, true), "32");    // 32.3 mph
    EXPECT_EQ(f(Fmt::speed, 0, false), "0");
}

TEST(Format, VerticalWithThousands)
{
    char b[24];
    Fmt::vertical(b, sizeof(b), 2940, false);
    EXPECT_STREQ(b, "2,940");
    Fmt::vertical(b, sizeof(b), 2940, true);
    EXPECT_STREQ(b, "9,646");
    Fmt::vertical(b, sizeof(b), -286, false);
    EXPECT_STREQ(b, "-286");
    Fmt::vertical(b, sizeof(b), 1234567, false);
    EXPECT_STREQ(b, "1,234,567");
    Fmt::vertical(b, sizeof(b), 999, false);
    EXPECT_STREQ(b, "999");
}

TEST(Format, Distance)
{
    char b[24];
    Fmt::distance(b, sizeof(b), 18240, false);
    EXPECT_STREQ(b, "18.2");
    Fmt::distance(b, sizeof(b), 18240, true);
    EXPECT_STREQ(b, "11.3");
    Fmt::distance(b, sizeof(b), 850, false);
    EXPECT_STREQ(b, "0.9");
}

TEST(Format, Times)
{
    char b[24];
    Fmt::duration(b, sizeof(b), 151);
    EXPECT_STREQ(b, "2:31");
    Fmt::duration(b, sizeof(b), 3751);
    EXPECT_STREQ(b, "1:02:31");
    Fmt::hoursMinutes(b, sizeof(b), 5 * 3600 + 42 * 60 + 59);
    EXPECT_STREQ(b, "5:42");
    Fmt::clock(b, sizeof(b), 13 * 60 + 44, false);
    EXPECT_STREQ(b, "13:44");
    Fmt::clock(b, sizeof(b), 13 * 60 + 44, true);
    EXPECT_STREQ(b, "1:44");
    Fmt::clock(b, sizeof(b), 0 * 60 + 5, true);
    EXPECT_STREQ(b, "12:05");
}

TEST(Format, Pressure)
{
    char b[24];
    Fmt::pressure(b, sizeof(b), 10164, false);
    EXPECT_STREQ(b, "1016.4");
    Fmt::pressure(b, sizeof(b), 10164, true);
    EXPECT_STREQ(b, "30.01");
    Fmt::pressureRate(b, sizeof(b), -340, false);
    EXPECT_STREQ(b, "-3.4");
    Fmt::pressureRate(b, sizeof(b), 120, false);
    EXPECT_STREQ(b, "+1.2");
    Fmt::pressureRate(b, sizeof(b), -340, true);
    EXPECT_STREQ(b, "-0.10");
    Fmt::verticalSpeed(b, sizeof(b), 210, false);
    EXPECT_STREQ(b, "+2.1");
    Fmt::verticalSpeed(b, sizeof(b), -210, true);
    EXPECT_STREQ(b, "-413");
}

TEST(SkiTypes, SettingsRoundTrip)
{
    Settings s;
    s.sport     = static_cast<uint8_t>(Sport::Snowboard);
    s.stormHpa  = 2;
    s.runAlerts = 0;
    s.alertMode = static_cast<uint8_t>(AlertMode::BuzzOnly);
    s.demo      = static_cast<uint8_t>(DemoSpeed::X60);
    Season season;
    season.days       = 12;
    season.runs       = 140;
    season.verticalM  = 61234;
    season.lastDayUtc = 1792000000u;
    char buf[256];
    ASSERT_GT(settingsToText(s, season, buf, sizeof(buf)), 0u);

    Settings s2;
    Season   season2;
    settingsFromText(buf, s2, season2);
    EXPECT_EQ(std::memcmp(&s, &s2, sizeof(s)), 0);
    EXPECT_EQ(season2.days, 12);
    EXPECT_EQ(season2.runs, 140);
    EXPECT_EQ(season2.verticalM, 61234u);
    EXPECT_EQ(season2.lastDayUtc, 1792000000u);
}

TEST(SkiTypes, BadSettingsFallBackToDefaults)
{
    Settings s;
    Season   season;
    settingsFromText("sport=9\nstorm=7\nalert=5\ndemo=8\nrunalerts=3\nxruns=4\n", s, season);
    EXPECT_EQ(s.sport, 0);
    EXPECT_EQ(s.stormHpa, 3);
    EXPECT_EQ(s.alertMode, 0);
    EXPECT_EQ(s.demo, 0);
    EXPECT_EQ(s.runAlerts, 1);
    EXPECT_EQ(season.runs, 0);   // "xruns" is not "runs"
}

TEST(SkiTypes, ProfileFitsAnyDay)
{
    DayProfile p;
    p.reset(1000, 30);
    // An 8-hour day of 400 m laps: 30 s buckets would need 960.
    for (uint32_t t = 0; t < 8 * 3600; ++t) {
        const uint32_t phase = t % 1200;
        const float alt = phase < 900 ? 1500.0f + 400.0f * phase / 900.0f
                                      : 1900.0f - 400.0f * (phase - 900) / 300.0f;
        p.add(1000 + t, alt, phase < 900 ? Motion::Lift : Motion::Run);
    }
    EXPECT_LE(p.count(), DayProfile::kBuckets);
    EXPECT_GE(p.count(), DayProfile::kBuckets / 2);
    EXPECT_EQ(p.bucketSeconds(), 240);
    int16_t lo = 0, hi = 0;
    p.range(lo, hi);
    EXPECT_NEAR(lo, 1500, 2);
    EXPECT_NEAR(hi, 1900, 2);
    int runs = 0;
    for (uint8_t i = 0; i < p.count(); ++i) {
        runs += p.motion(i) == Motion::Run;
    }
    EXPECT_GT(runs, 20);   // every run still visible after compressing
}

TEST(SkiTypes, RunInfoConversion)
{
    RunStats r;
    r.number     = 3;
    r.tStart     = 1000 + 9 * 3600 + 30 * 60;
    r.tEnd       = r.tStart + 151;
    r.topAlt     = 2420.4f;
    r.bottomAlt  = 1945.0f;
    r.distance   = 2716.4f;
    r.maxSpeed   = 19.05f;
    r.movingTime = 140.0f;
    const RunInfo i = toInfo(r, 1000);
    EXPECT_EQ(i.number, 3);
    EXPECT_EQ(i.verticalM, 475);
    EXPECT_EQ(i.distanceM, 2716u);
    EXPECT_EQ(i.maxSpeedCms, 1905);
    EXPECT_EQ(i.durationS, 151);
    EXPECT_EQ(i.startMin, 9 * 60 + 30);
}
