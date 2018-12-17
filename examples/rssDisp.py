#!/usr/bin/python
# -*- coding: utf-8 -*-
import time,sys
from copy import copy
from RaspiOled import oled, ScrollText, log
from PIL import Image,ImageDraw,ImageFont
import feedparser                   # pip install feedparser

rg_datetime = (( 0, 0), (128,16))   # 日時表示領域
rg_source   = (( 0,16), ( 88,16))   # ソース表示領域
rg_num      = ((88,16), ( 40,16))   # 記事番号表示領域
rg_title    = (( 0,32), (128,32))   # タイトル表示領域
sp_source   =  64  # ソース・スクロール速度
sp_title    = 128  # タイトル・スクロール速度 [dot per sec]
rss_list = [
    "https://news.google.com/_/rss/topics/CAAqIQgKIhtDQkFTRGdvSUwyMHZNRE5mTTJRU0FtcGhLQUFQAQ?hl=ja&gl=JP&ceid=JP:ja",
    "https://news.yahoo.co.jp/pickup/world/rss.xml",
    "https://news.yahoo.co.jp/pickup/sports/rss.xml",
    "https://news.yahoo.co.jp/pickup/science/rss.xml",
    "https://news.yahoo.co.jp/pickup/computer/rss.xml",
    "https://headlines.yahoo.co.jp/rss/kyodonews-c_int.xml",
    "https://headlines.yahoo.co.jp/rss/san-dom.xml",
    "https://pc.watch.impress.co.jp/data/rss/1.0/pcw/feed.rdf",
    "http://www.jiji.com/rss/ranking.rdf",
    "http://feeds.feedburner.com/make_jp",
    "https://news.yahoo.co.jp/pickup/rss.xml",
    "http://numbers2007.blog123.fc2.com/?xml",
    "https://headlines.yahoo.co.jp/rss/afpbbnewsv-c_int.xml",
    "http://rss.asahi.com/rss/asahi/newsheadlines.rdf"
    "https://blog.adafruit.com/feed",
    "https://hackaday.com/feed",
]

font_path = '/usr/share/fonts/truetype/fonts-japanese-gothic.ttf'

class DatetimeDisp:
    def __init__ (self, area ):
        self.area = area
        self.img = Image.new('1', area[1])
        self.drw = ImageDraw.Draw(self.img)
        self.font = ImageFont.truetype(font=font_path, size=16)
        self.minute  = 0

    def update(self):
        if self.minute == int(time.time()/60):
            return
        self.minute = int(time.time()/60)
        s = time.strftime("%m/%d(%a) %H:%M")
        ((x,y),(w,h)) = self.area
        self.drw.rectangle((x,y,x+w,y+h),fill=0)
        self.drw.text((0,0),s,fill=1,font=self.font)
        oled.image(self.img,dst_area=self.area)

class RssDisp:
    def __init__(self, title_area, num_area, source_area):
        self.num_area    = num_area
        self.st_title = ScrollText.new(area=title_area,
                                       font=font_path, size=30,
                                       speed=96)
        self.st_source = ScrollText.new(area=source_area,
                                        font=font_path, size=14,
                                        speed=32)
        self.urls = [] 
        self.entries = []
        self.title = ""
        self.num_img = Image.new('1', num_area[1])
        self.num_drw = ImageDraw.Draw(self.num_img)
        self.num_font = ImageFont.truetype(font=font_path, size=12)
        self.mode = 'next'

    def draw_num(self, n):
        ((x,y),(w,h)) = self.num_area
        self.num_drw.rectangle((0,0,w,h),fill=0)
        str = "[%2d/%2d]" % (n, self.noe)
        self.num_drw.text((0,0),str,fill=1,font=self.num_font)
        oled.image(self.num_img, dst_area=self.num_area)
        
    def update(self):
        if not self.st_source.update():
            if self.mode == 'disp':
                self.st_source.add_text("... " + self.title + " ...")
            else:
                self.mode = 'next'
            
        if self.st_title.update():
            return

        if len(self.entries) > 0:
            e = self.entries.pop(0)
            self.st_title.add_text(e['title'])
            self.st_title.scrollOut()
            self.draw_num(self.noe - len(self.entries))   
            return

        if self.mode == 'disp':
            self.st_source.scrollOut()
            self.mode = 'wait'
            return
        
        if self.mode == 'wait':
            return
        
        if len(self.urls) > 0:
            url = self.urls.pop(0)
            if self.st_source.update():
                return
            f = feedparser.parse(url)
            if 'title' in f['feed']:
                self.title = f['feed']['title']
                self.entries = f['entries'][:20]
                self.noe = len(self.entries)
                self.mode = 'disp'
                print("%s - %3d entries,%s" %(time.strftime("%H:%M:%S"),
                                           self.noe, url))
            else :
                self.title = ""
                print("failed ",url)
        else:
            self.urls = copy(rss_list)
         
if __name__ == '__main__':
    datetime_disp = DatetimeDisp(rg_datetime)
    rss_disp = RssDisp(rg_title, rg_num, rg_source)
    oled.begin()
    while 1:
        datetime_disp.update()
        rss_disp.update()
        time.sleep(0.01)
    
    
