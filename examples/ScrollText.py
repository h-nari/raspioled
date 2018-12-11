#!/usr/bin/python3
import time
import raspioled as oled;
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

class ScrollText:

    def __init__(self):
        self.dot_per_second = 128.0
        self.area = ((0,0), oled.size)
        self.text = ""
        self.font = ImageFont.truetype(
            font='/usr/share/fonts/trutype/noto/NotoMono-Regular.ttf',
            size=32);
        self.image = Image.new('1',(64,64)) 
        self.draw  = ImageDraw.Draw(self.image)
        self.tw = self.tx = 0
        self.sc_cnt = 0
        self.time_set = 0
                     
    def add_text(self, text):
        self.text += text

    def update(self):

        if self.getCharImage():
            left = self.calcShift()
            while left > 0:
                ((x,y),(w,h)) = self.area
                oled.shift(area=self.area, amount=(-left,0),fill=0)
                ww = min(left, self.tw - self.tx)
                oled.image(self.image,
                           dst_area=(x+w-ww,y,ww,self.th),
                           src_area=(self.tx,0,ww,self.th))
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
            return 1
        else:
            dot = int((time.time() - self.time) * self.dot_per_second);
            self.time += dot / self.dot_per_second
            return dot
        
if __name__ == "__main__":
    oled.begin()
    sc = ScrollText()
    sc.add_text("Hello")
    while sc.update():
        time.sleep(0.001)
    sc.add_text(" World")
    while sc.update():
        time.sleep(0.001)
    sc.scrollOut()    
    while sc.update():
        time.sleep(0.001)
    
    print("main done");
                     
