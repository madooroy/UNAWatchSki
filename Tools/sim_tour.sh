#!/usr/bin/env bash
# Plays the whole demo day in the simulator on a virtual display and saves a
# screenshot of every screen. Each shot is taken when the matching event shows
# up in the simulator's log, so a banner is caught on screen, not guessed at.
# Optionally records the whole thing as a video. At the end the day's FIT file
# is copied to OUTDIR too.
#
# Needs Xvfb, xdotool, ImageMagick (and ffmpeg for --video).
# Usage: Tools/sim_tour.sh OUTDIR [--video]
set -euo pipefail
OUT=${1:-/tmp/ski-tour}; mkdir -p "$OUT"
VIDEO=${2:-}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/Software/Apps/LVGL-GUI/simulator/build/bin"
FS="$ROOT/Software/Output"            # the simulator's watch file system
LOG="$OUT/sim.log"
export DISPLAY=${DISPLAY_NUM:-:98} UNA_SIM_MUTE=1
# UNA_SIM_IMPERIAL=1 Tools/sim_tour.sh ... shows the imperial screens.
Xvfb "$DISPLAY" -screen 0 640x480x24 >/dev/null 2>&1 & XPID=$!
sleep 1
# Start clean: no settings, no interrupted day to recover, no old activities.
rm -rf "$FS/skiday.cfg" "$FS/Activity"
cd "$BIN"; stdbuf -oL -eL ./SkiDaySimulator > "$LOG" 2>&1 & SPID=$!
VPID=""
trap 'kill $SPID $XPID $VPID 2>/dev/null || true' EXIT
sleep 3
W=$(xdotool search --name "Ski Day" | head -1); xdotool windowmove "$W" 0 0; sleep 0.5

if [ "$VIDEO" = "--video" ]; then
    ffmpeg -loglevel error -y -f x11grab -framerate 25 -video_size 480x480 -i "$DISPLAY+0,0" \
        -c:v libx264 -pix_fmt yuv420p -crf 20 "$OUT/demo.mp4" & VPID=$!
fi

snap() { import -window root -crop 480x480+0+0 "$OUT/$1.png"; echo "  $1"; }
key()  { xdotool key --window "$W" "$1"; sleep "${2:-0.5}"; }
count() { grep -c -E -- "$1" "$LOG" || true; }
# Wait until the log has PATTERN at least N times (default 1), then SETTLE s.
waitlog() {
    local pat=$1 n=${2:-1} settle=${3:-0.3}
    for _ in $(seq 1 2400); do
        [ "$(count "$pat")" -ge "$n" ] && { sleep "$settle"; return 0; }
        sleep 0.25
    done
    echo "timeout waiting for '$pat' x$n" >&2; return 1
}
# A key press while a run banner is up only dismisses the banner (by design).
# Before navigating, wait for a moment with no banner: 8 s without a run ending.
quiet() {
    for _ in $(seq 1 60); do
        local n; n=$(count "Run [0-9]+:")
        sleep 8
        [ "$(count "Run [0-9]+:")" -eq "$n" ] && return 0
    done
}

sleep 3; snap 01_home                        # GPS fix after a few seconds
key 2 0.8; snap 02_settings                  # Sport
key 2; snap 03_settings_storm                # Storm alert
key 1; key 1; key 3; key 3; key 3 0.6        # Demo day: Off -> 1x -> 10x -> 60x
snap 04_settings_demo
key 4 0.8; snap 05_home_demo
key 3 1.5; snap 06_ready                     # Start the demo day

waitlog "Lift 1 started" 1 6;   snap 07_first_lift
waitlog "Run 1 started" 1 0.8;  snap 08_skiing
waitlog "Run 1:" 1 0.8;         snap 09_run_banner
sleep 8;                        snap 10_on_lift
waitlog "Run 3 started" 1 1.5;  snap 11_skiing_fast
quiet
key 2 0.8; snap 12_day
key 2 0.8; snap 13_storm_learning
key 2 0.8; snap 14_clock
key 4 0.8                                    # back to the live face
quiet
key 3 0.8; snap 15_menu                      # R1: menu
key 3 1;   snap 16_paused                    # Pause (no banners while paused)
key 3 0.8; key 3 1                           # menu, Resume
waitlog "Run 10 started" 1 1
quiet
key 2 0.6; key 2 0.8; snap 17_storm_falling  # storm face as the weather turns
key 4 0.6
waitlog "STORM" 1 1.5;          snap 18_storm_alert
key 3 0.8
quiet
key 2 0.6; key 2 0.8; snap 19_storm_storm
key 4 0.6
waitlog "Day ended" 1 2;        snap 20_summary
key 2 0.8; snap 21_profile
key 2 0.8; snap 22_runs
for _ in $(seq 1 16); do key 2 0.15; done
sleep 0.5; snap 23_runs_end
key 3 1;   snap 24_home_after

[ -n "$VPID" ] && { kill -INT $VPID; wait $VPID 2>/dev/null || true; VPID=""; }
cp "$FS"/Activity/*/*.fit "$OUT/" 2>/dev/null || true
grep -E "Day ended|STORM|Run [0-9]+:" "$LOG" | sed 's/.*: //' > "$OUT/events.txt"
echo "done: $OUT"
