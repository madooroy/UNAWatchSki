// Replays the demo ski day through the run tracker and the storm watch, the
// way the watch service does, and prints what they found next to the truth.
//
//   g++ -std=c++17 -O2 -ISoftware/Libs/Header Tools/replay_day.cpp
//       Software/Libs/Sources/Ski/*.cpp -o build/replay_day
//   build/replay_day [seed] [storm 0|1] [speedScale] [moreStops 0|1] [gpsLoss 0|1]

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Ski/RunTracker.hpp"
#include "Ski/SkiDaySim.hpp"
#include "Ski/StormWatch.hpp"

using namespace Ski;

static void hhmm(char* b, uint32_t t)
{
    std::snprintf(b, 12, "%02u:%02u", t / 3600, (t / 60) % 60);
}

struct Printer : RunTracker::Listener {
    void onRunEnded(const RunStats& r, bool bs, bool bv) override
    {
        char a[12], b[12];
        hhmm(a, r.tStart);
        hhmm(b, r.tEnd);
        std::printf("  RUN  %2u %s-%s %4u s  vert %5.0f m  dist %5.0f m  top %4.1f km/h  avg %4.1f km/h%s%s\n",
                    r.number, a, b, r.duration(), r.vertical(), r.distance, r.maxSpeed * 3.6f,
                    r.avgSpeed() * 3.6f, bs ? "  *speed" : "", bv ? "  *vert" : "");
    }
    void onLiftEnded(const LiftStats& l) override
    {
        char a[12], b[12];
        hhmm(a, l.tStart);
        hhmm(b, l.tEnd);
        std::printf("  LIFT %2u %s-%s %4u s  gain %5.0f m\n", l.number, a, b, l.duration(), l.gain());
    }
};

int main(int argc, char** argv)
{
    SkiDaySim::Options o;
    if (argc > 1) o.seed = static_cast<uint32_t>(std::atoi(argv[1]));
    if (argc > 2) o.storm = std::atoi(argv[2]) != 0;
    if (argc > 3) o.speedScale = static_cast<float>(std::atof(argv[3]));
    if (argc > 4) o.moreStops = std::atoi(argv[4]) != 0;
    if (argc > 5) o.gpsLoss = std::atoi(argv[5]) != 0;

    SkiDaySim  sim(o);
    RunTracker trk;
    StormWatch storm;
    Printer    pr;
    trk.setListener(&pr);

    SkiDaySim::Output s;
    float    p0 = 0;
    uint32_t agree = 0, total = 0;
    while (sim.step(s)) {
        if (p0 == 0) p0 = s.seaLevelPa;
        const float alt = 44330.77f * (1.0f - std::pow(s.pressurePa / p0, 1.0f / 5.25588f));
        Sample in;
        in.t = s.t; in.alt = alt; in.altValid = true;
        in.speed = s.speed; in.speedValid = s.speedValid; in.dist = s.dist;
        trk.add(in);
        storm.add(s.t, s.pressurePa, true, s.speed, s.speedValid, s.lat, s.lon, s.fix);
        if (storm.takeAlert()) {
            char a[12];
            hhmm(a, s.t);
            std::printf("  STORM ALERT %s  %.1f hPa/3h\n", a, storm.trend().hpaPer3h);
        }
        ++total;
        if (trk.motion() == s.truth) ++agree;
    }
    trk.finish();

    std::printf("\nTruth: %u runs, %u lifts\n", sim.truthRuns(), sim.truthLifts());
    for (uint8_t i = 0; i < sim.truthRuns(); ++i) {
        const auto& r = sim.truthRun(i);
        char a[12], b[12];
        hhmm(a, r.tStart);
        hhmm(b, r.tEnd);
        std::printf("  run  %2u %s-%s %4u s  vert %5.0f m  dist %5.0f m  top %4.1f km/h\n", i + 1, a, b,
                    r.tEnd - r.tStart, r.vertical, r.distance, r.maxSpeed * 3.6f);
    }
    const DayTotals d = trk.totals();
    std::printf("\nTracker: %u runs, %u lifts, vertical %.0f m, dist %.1f km, top %.1f km/h\n", d.runs, d.lifts,
                d.vertical, d.distance / 1000, d.maxSpeed * 3.6f);
    std::printf("  time: ski %u min, lift %u min, idle %u min, total %u min\n", d.runTime / 60, d.liftTime / 60,
                d.idleTime() / 60, d.totalTime / 60);
    std::printf("  per-second agreement with truth: %.1f %%\n", 100.0 * agree / total);
    const auto& tr = storm.trend();
    std::printf("Storm: level %s, %.2f hPa/3h, %u readings, %u spots\n", levelName(tr.level), tr.hpaPer3h,
                tr.readings, tr.spots);
    return 0;
}
