#!/usr/bin/env bash
# Drive the simulator through every screen on a virtual display and save
# screenshots. Needs Xvfb, xdotool and ImageMagick. Usage: Tools/sim_tour.sh OUTDIR
set -euo pipefail
OUT=${1:-/tmp/sim-tour}; mkdir -p "$OUT"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/Software/Apps/LVGL-GUI/simulator/build/bin"
export DISPLAY=${DISPLAY_NUM:-:98} UNA_SIM_MUTE=1
Xvfb "$DISPLAY" -screen 0 800x600x24 >/dev/null 2>&1 & XPID=$!
sleep 1
rm -f "$ROOT/Output/sevenminute.cfg"
cd "$BIN"; stdbuf -oL -eL ./SevenMinuteSimulator > "$OUT/sim.log" 2>&1 & SPID=$!
trap 'kill $SPID $XPID 2>/dev/null || true' EXIT
sleep 3
W=$(xdotool search --name "7-Minute" | head -1); xdotool windowmove "$W" 0 0; sleep 0.5
snap() { import -window root -crop 480x480+0+0 "$OUT/$1.png"; }
key()  { xdotool key --window "$W" "$1"; sleep "${2:-0.4}"; }

snap 01_home
key 1; key 2; snap 02_home_settings          # rounds -> 2, alerts -> buzz only
key 1; key 1; key 2; key 2                   # back to 1 round, beep + buzz
key 3 4; snap 03_get_ready
key 2 2; snap 04_work                        # skip lead-in
key 3 1; snap 05_paused
key 3 1                                      # resume
key 4 1; snap 06_confirm_end
key 3 1                                      # back to the workout
key 2 1.5; snap 07_rest
for i in $(seq 1 18); do key 2 0.5; done; sleep 1.5; snap 08_rest_long_name
key 2 2; snap 09_work_long_name
key 2 0.4; key 2 3; snap 10_side_plank_right
sleep 14; snap 11_side_plank_left
sleep 15; snap 12_summary
key 4 1.5; snap 13_home_after
