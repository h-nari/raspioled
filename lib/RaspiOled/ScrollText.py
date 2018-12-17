#!/usr/bin/python3
import time
from RaspiOled import oled,log
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

class new:

    def __init__(self,
                 area=((0,0),(128,64)),
                 font='/usr/share/fonts/trutype/noto/NotoMono-Regular.ttf',
                 size=16,
                 speed = 128
    ):
        self.dot_per_second = speed
        self.area = area
        self.text = ""
        self.font_size = size
        self.font_path = font
        self.font=ImageFont.truetype(font=self.font_path,size=self.font_size);
        self.image = Image.new('1',(64,64)) 
        self.draw  = ImageDraw.Draw(self.image)
        self.tw = self.tx = 0
        self.sc_cnt = 0
        self.time_set = 0
                     
    def add_text(self, text):
        self.text += text

    def set_font(self, path, size=-1):
        self.font_path = path;
        if(size >= 0):
            self.font_size = size
        self.font=ImageFont.truetype(font=self.font_path,size=self.font_size);

    def set_font_size(self, size):
        self.font_size = size;
        self.font=ImageFont.truetype(font=self.font_path,size=self.font_size);

    def set_speed(self, dps):
        self.dot_per_second = dps;

    def update(self):
        if self.getCharImage():
            ((x,y),(w,h)) = self.area
            left = self.calcShift()
            oled.shift(area=self.area, amount=(-left,0),fill=0)
            wx = x + w - left
            while left > 0:
                ww = min(left, self.tw - self.tx)
                oled.image(self.image,
                           dst_area=(wx,y,ww,self.th),
                           src_area=(self.tx,0,ww,self.th))
                wx += ww
                left -= ww
                self.tx += ww
                if not self.getCharImage():
                    break
            return 1
        elif self.sc_cnt > 0:
            shift = self.calcShift()
            ww = min(shift, self.sc_cnt)
            self.sc_cnt -= ww
            oled.shift(amount=(-ww,0),area=self.area)
            return 1
        else:
            self.time_set = 0
            return 0
            
    def getCharImage(self):
        if self.tx >= self.tw:
            if len(self.text) > 0:
                c = self.text[0]
                self.text = self.text[1:]
                (w,h) = self.draw.textsize(c, font=self.font)
                self.tw = w;
                self.th = h; 
                self.tx = 0;
                self.draw.rectangle((0,0,w,h),fill=0)
                self.draw.text((0,0),c,font=self.font,fill=1)
            else:
                return 0
        return 1

    def scrollOut(self, w=-1):
        if(w < 0):
            w = self.area[1][0]
        self.sc_cnt = w
           
    def calcShift(self):
        if(not self.time_set):
            self.time = time.time()
            self.time_set = 1
            return 0
        else:
            dot = int((time.time() - self.time) * self.dot_per_second);
            self.time += float(dot) / self.dot_per_second
            return dot
        
