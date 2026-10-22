#!/usr/bin/env python3
"""Cut the demo-day recording from `Tools/sim_tour.sh OUTDIR --video` into a
~90 s captioned video: the watch in a round bezel, a caption underneath.
Quiet stretches are sped up; screens shown only briefly in the recording
are held as stills.

usage: Tools/make_video.py OUTDIR      (writes OUTDIR/SkiDay_demo.mp4)
Needs ffmpeg and Pillow.
"""
import os, subprocess, sys
from PIL import Image, ImageDraw

OUT = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'Software/Apps/LVGL-GUI/assets/ttf/Poppins-Medium.ttf')
VIDEO = os.path.join(OUT, 'demo.mp4')
W, H, S = 720, 800, 600
X, Y = (W - S) // 2, 40
BG = (18, 18, 18)

# When each screenshot was taken, relative to the start of the recording.
# The recording ends just after the last screenshot.
def mtime(p):
    return os.stat(p).st_mtime
dur = float(subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', VIDEO],
                           capture_output=True, text=True, check=True).stdout)
start = mtime(VIDEO) - dur
shots = {f[:-4]: mtime(os.path.join(OUT, f)) - start for f in os.listdir(OUT) if f[:2].isdigit() and f.endswith('.png')}
def at(name, d=0.0):
    return shots[name] + d

# (kind, from, to-or-None, speed-or-seconds, caption)
SEGS = [
    ('clip', at('01_home', -2.0), at('06_ready'), 1, 'Buttons only: sport, storm alert, demo day'),
    ('clip', at('06_ready'), at('09_run_banner', -0.5), 2.5, 'Start, and just go ski'),
    ('clip', at('09_run_banner', -0.5), at('09_run_banner', 7.0), 1, 'Each run, told on the next lift'),
    ('clip', at('09_run_banner', 7.0), at('12_day', -0.8), 4, 'Live: speed and the run so far'),
    ('png', '12_day', None, 2.6, 'The day so far'),
    ('png', '13_storm_learning', None, 2.2, 'Storm watch learns while you queue'),
    ('png', '14_clock', None, 1.8, 'Clock, altitude, battery'),
    ('png', '15_menu', None, 1.8, 'Pause, end or discard'),
    ('clip', at('16_paused', 1.0), at('17_storm_falling', -2.2), 12, 'Morning, lunch, afternoon (demo day at 60x)'),
    ('png', '17_storm_falling', None, 2.6, 'Pressure starts to fall'),
    ('clip', at('17_storm_falling', 0.3), at('18_storm_alert', -1.7), 8, 'Pressure starts to fall'),
    ('png', '18_storm_alert', None, 3.5, 'Storm warning: 3 hPa in 3 hours'),
    ('png', '19_storm_storm', None, 2.2, 'The storm face keeps the trend'),
    ('clip', at('19_storm_storm', 0.5), at('20_summary', -0.8), 10, 'Last runs of the day'),
    ('png', '20_summary', None, 2.8, 'The day: runs, vertical, top speed'),
    ('png', '21_profile', None, 2.8, 'Altitude profile: runs and lifts'),
    ('clip', at('22_runs', -0.3), at('23_runs_end', 0.6), 1, 'Every run, with its vertical and top speed'),
    ('png', '24_home_after', None, 2.0, 'Saved as a FIT file'),
]

mask = Image.new('RGBA', (W, H), BG + (255,))
d = ImageDraw.Draw(mask)
d.ellipse([X - 22, Y - 22, X + S + 21, Y + S + 21], fill=(44, 44, 44, 255))
d.ellipse([X, Y, X + S - 1, Y + S - 1], fill=(0, 0, 0, 0))
mask_png = os.path.join(OUT, 'mask.png')
mask.save(mask_png)

def esc(c):
    return c.replace(':', '\\:').replace("'", '’').replace(',', '\\,')

parts = []
for i, (kind, a, b, sp, cap) in enumerate(SEGS):
    seg = os.path.join(OUT, f'seg{i:02d}.mp4')
    parts.append(seg)
    if kind == 'clip':
        inp = ['-ss', f'{a:.2f}', '-t', f'{b - a:.2f}', '-i', VIDEO]
        pre = f'setpts=PTS/{sp},fps=25,'
    else:
        inp = ['-loop', '1', '-t', f'{sp}', '-i', os.path.join(OUT, a + '.png')]
        pre = 'fps=25,'
    fc = (f'[0:v]{pre}scale={S}:{S}[v];color=c=0x121212:s={W}x{H}:r=25[bg];'
          f'[bg][v]overlay={X}:{Y}:shortest=1[o];[o][1:v]overlay=0:0,'
          f"drawtext=fontfile={FONT}:text='{esc(cap)}':fontsize=30:fontcolor=white:"
          f'x=(w-tw)/2:y={Y + S + 60},format=yuv420p')
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', *inp, '-i', mask_png, '-filter_complex', fc,
                    '-an', '-c:v', 'libx264', '-crf', '20', '-r', '25', seg], check=True)

lst = os.path.join(OUT, 'segments.txt')
with open(lst, 'w') as f:
    f.writelines(f"file '{p}'\n" for p in parts)
final = os.path.join(OUT, 'SkiDay_demo.mp4')
subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', lst,
                '-c', 'copy', '-movflags', '+faststart', final], check=True)
for p in parts + [lst, mask_png]:
    os.remove(p)
print(final)
