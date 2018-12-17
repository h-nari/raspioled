#!/usr/bin/python3
# -*- coding: utf-8 -*-

import time,sys,math
from PIL import Image,ImageDraw,ImageFont
from RaspiOled import oled;

usage = "Usage:%s [-fps num][-v][-q] image_files ..."
fps = 30.0
verbose = 0

i=1
while i < len(sys.argv) and sys.argv[i][0] == '-':
    if sys.argv[i] == '-fps':
        fps = float(sys.argv[i+1])
        i += 1
    elif sys.argv[i] == '-v':
        verbose += 1
    elif sys.argv[i] == '-q':
        verbose -= 1
    else:
        print("option %s not defined." % sys.argv[i])
        print(usage)
        sys.exit(1)
    i += 1

if i >= len(sys.argv):
    print(usage)
    sys.exit(1)
    
ftime = 1 / float(fps);

oled.begin()
oled.clear()
im = Image.new('1',oled.size)

frames = sys.argv[i:]

f = 0;
t_start = t = time.time()
while f < len(frames):
    im2 = Image.open(frames[f])
    im.paste(im2)
    oled.image(im,sync=1)

    # 進めるフレーム数を計算
    # t + ftime * x > time.time() を満たす最小整数のx
    # x > (time.time() - t) / ftime
    now = time.time()
    x = math.ceil((now  - t) / ftime)
    t_next = t + x * ftime;
    t_sleep = t_next - now

    if (f != len(frames)-1) and (f + x >= len(frames)):
        f = len(frames)-1   # 最後のフレームは必ず表示
    else:    
        f += x;
    t2 = time.time()
    if verbose:
        print("%d %6.4f %6.4f %6d fps:%6.2f" % (x, t_sleep, t2 - t,
                                                f, f / (t2 - t_start)))
    t = t_next
    time.sleep(t_sleep)
    


