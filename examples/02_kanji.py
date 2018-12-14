#!/usr/bin/python
# -*- coding: utf-8 -*-

import RaspiOled.oled as oled;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

image = Image.new('1',oled.size)  # make 128x64 bitmap image
draw  = ImageDraw.Draw(image)


f1 = ImageFont.truetype(
    font='/usr/share/fonts/truetype/fonts-japanese-gothic.ttf',
    size=60)
draw.text((0,  0), u"漢字", font=f1, fill=255)
oled.begin()
oled.image(image)

