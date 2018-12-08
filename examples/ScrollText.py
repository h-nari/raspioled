#!/usr/bin/python3
import time
import threading
import raspioled as oled

class ScrollText:

    def __init__(self):
        self.dot_per_second = 32.0
        self.area = ((0,0), oled.size)
        self.text = ""
                     
    def add_text(self, text):
        self.text += text

    def start(self):
        self.thread = threading.Thread(target=self.runner)
        self.quit = 0
        self.thread.start()             
    
    def runner(self):
        self.wait = 1.0 / self.dot_per_second
        print("wait:", self.wait)
        while(not self.quit):             
            print("run")
            time.sleep(self.wait)
        print("thread quit");

    def end(self):
        sc.quit = 1
        self.thread.join()

if __name__ == "__main__":
    oled.begin()
    sc = ScrollText()
    sc.start()
    
    time.sleep(5)
    sc.end()
    print("main done");
                     
