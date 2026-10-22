#!/usr/bin/env python3
"""Check the cue timeline in a simulator log against the protocol
(10 s lead-in, 12 x 30 s work, 10 s transitions, 3-2-1 countdowns)."""
import re, sys
cues = [(m.group(1), int(m.group(2)), int(m.group(3)))
        for m in re.finditer(r"CUE (\w+) (\d+) \(t=(\d+)\)", open(sys.argv[1]).read())]
assert cues, "no cues in log"
t0 = cues[0][2] - 7000          # first cue is COUNTDOWN 3 at lead-in t=7 s
expect = []
t = 0
phases = [("READY", 10000)] + sum([[("WORK", 30000)] + ([("REST", 10000)] if i < 11 else []) for i in range(12)], [])
for i, (name, dur) in enumerate(phases):
    for s in (3, 2, 1):
        expect.append(("COUNTDOWN", s, t + dur - s * 1000))
    if name == "WORK" and i == len(phases) - 1:
        expect.append(("SWITCH_SIDES", 0, t + 15000))
    t += dur
    if i + 1 < len(phases):
        expect.append(("GO" if phases[i + 1][0] == "WORK" else "REST", 0, t))
expect.append(("FINISHED", 0, t))
expect.sort(key=lambda e: (e[2], e[0] != "SWITCH_SIDES"))
got = [(c, a, tt - t0) for c, a, tt in cues]
worst = 0; bad = 0
for (ec, ea, et), (gc, ga, gt) in zip(expect, got):
    err = gt - et; worst = max(worst, abs(err))
    if ec != gc or ea != ga or abs(err) > 50:
        bad += 1; print(f"MISMATCH expected {ec} {ea} @{et} got {gc} {ga} @{gt}")
print(f"{len(got)} cues logged, {len(expect)} expected, worst timing error {worst} ms")
print(f"total workout {got[-1][2]/1000:.1f} s")
sys.exit(1 if bad or len(got) != len(expect) else 0)
