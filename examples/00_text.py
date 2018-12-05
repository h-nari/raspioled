#!/usr/bin/python

import raspioled as oled;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

image = Image.new('1',oled.size)  # make 128x64 bitmap image
draw  = ImageDraw.Draw(image)
font = ImageFont.truetype(
    font='/usr/share/fonts/truetype/freefont/FreeMono.ttf',
    size=40)
draw.text((0,0), "Hello", font=font, fill=255)
oled.begin()
oled.image_bytes(image.tobytes())

