#!/usr/bin/python

import RaspiOled.oled as oled;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

image = Image.new('1',oled.size)  # make 128x64 bitmap image
draw  = ImageDraw.Draw(image)
f10 = ImageFont.truetype(
    font='/usr/share/fonts/truetype/freefont/FreeMono.ttf',
    size=12)
f20 = ImageFont.truetype(
    font='/usr/share/fonts/truetype/freefont/FreeSerif.ttf',
    size=20)
f40 = ImageFont.truetype(
    font='/usr/share/fonts/truetype/freefont/FreeSans.ttf',
    size=40)
draw.text((0, 0), "Hello Raspberry Pi", font=f10, fill=255)
draw.text((0,10), "Hello OLED", font=f20, fill=255)
draw.text((0,26), "Hello", font=f40, fill=255)
oled.begin()
oled.image(image)

