#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys,time
from RaspiOled import oled,ScrollText;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

image = Image.new('1',oled.size)  # make 128x64 bitmap image
draw  = ImageDraw.Draw(image)

oled.begin()
sc = ScrollText.new()
sc.add_text("Hello World");
sc.scrollOut();
while sc.update():
    time.sleep(0.005)

    
