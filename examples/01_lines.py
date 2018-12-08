#!/usr/bin/python

import raspioled as oled;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont
import time
import random

image = Image.new('1',oled.size)  # make 128x64 bitmap image
draw  = ImageDraw.Draw(image)
font = ImageFont.truetype(
    font='/usr/share/fonts/truetype/freefont/FreeMono.ttf',
    size=40)

oled.begin()
random.seed()

(w,h) = oled.size;

for i in range(50):
    x1 = random.randrange(w)
    y1 = random.randrange(h)
    x2 = random.randrange(w)
    y2 = random.randrange(h)
    draw.line([x1,y1,x2,y2],fill= 1)
    oled.image(image)
print("done")
oled.end()



