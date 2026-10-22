#!/usr/bin/env bash
# Run one complete, unskipped workout in the simulator and verify the cue
# timeline from the service log. Usage: Tools/sim_fullrun.sh OUTDIR
set -euo pipefail
OUT=${1:-/tmp/sim-full}; mkdir -p "$OUT"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/Software/Apps/LVGL-GUI/simulator/build/bin"
export DISPLAY=${DISPLAY_NUM:-:97} UNA_SIM_MUTE=1
Xvfb "$DISPLAY" -screen 0 800x600x24 >/dev/null 2>&1 & XPID=$!
sleep 1
rm -f "$ROOT/Output/sevenminute.cfg"
cd "$BIN"; stdbuf -oL -eL ./SevenMinuteSimulator > "$OUT/sim.log" 2>&1 & SPID=$!
trap 'kill $SPID $XPID 2>/dev/null || true' EXIT
sleep 3
W=$(xdotool search --name "7-Minute" | head -1); xdotool windowmove "$W" 0 0; sleep 0.5
xdotool key --window "$W" 3
sleep 485
import -window root -crop 480x480+0+0 "$OUT/summary.png"
xdotool key --window "$W" 4; sleep 1.5
import -window root -crop 480x480+0+0 "$OUT/home_after.png"
xdotool key --window "$W" Escape; sleep 2
python3 "$ROOT/Tools/check_cues.py" "$OUT/sim.log"
