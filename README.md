# raspioled
pythin library for 128 x 64 dot OLED display connected to Raspberry Pi via i2c

<a href="https://github.com/h-nari/raspioled/wiki/images/181217a0.jpg">
<img src="https://github.com/h-nari/raspioled/wiki/images/181217a0.jpg" width="240"></a>

ラズベリーパイに１I2Cで接続された 128 x 64ドットのOLED (コントローラはSSD1306)用の
Pythonのライブラリです。

Adafruit_SSD1306の描画速度が遅いのが不満で作成しました。

## 特徴

- Adafruit_SSD1306と同様に、Pillow（(画像ライブラリ)で描画したimageのデータを転送しOLEDに表示
- Pillow.image.toBytes()をOLED(SSD1306)のフォーマットに変換する処理をC言語で実装しているので高速
- i2cでの描画データの転送処理は、裏スレッドで行うのでPythonのスレッドは待たない
- 転送完了を待つ機能も実装 

## プログラム例

以下にプログラム例を示します。

    #!/usr/bin/python

    from RaspiOled import oled
    from PIL import Image,ImageDraw,ImageFont

    image = Image.new('1',oled.size)  # make 128x64 bitmap image
    draw  = ImageDraw.Draw(image)
    font = ImageFont.truetype(
        font='/usr/share/fonts/truetype/freefont/FreeMono.ttf',
        size=40)
    draw.text((0,0), "Hello", font=font, fill=255)
    oled.begin()
    oled.image(image)


## インストール方法

python2 で使用する場合

    $ sudo python setup.py install
    $ sudo pip install Pillow
python3 で使用する場合

    $ sudo python3 setup.py install
    $ sudo pip3 install Pillow

## 使用例

examples以下にサンプルのプログラムを用意していますので、参考にして下さい。

サンプルプログラム実行時、必要なライブラリがあれば、適宜　pip等でインストールして下さい。

## I2Cの速度を400kHzにする

ラズベリーパイのI2Cの転送速度はデフォルトで100kHzです。
このままでは画像データの転送に約100mSかかり、Adafruit_SSD1306と大差ありません。

I2Cの速度を400kHzに上げると転送速度が約4倍に早くなります。
I2Cの速度はkernelの設定で決まっているようなので、
変更は /boot/config.txt等で行います。

/boot/config.txtに以下の行を追加し、ラズベリーパイを再起動してください。

    dtparam=i2c_baudrate=400000
    
## プロパティ

### **size**: 

oledの画面の幅と高さのタプル、すなわち `(128,64)` を返します。

## メソッド

### **begin(dev="/dev/i2c-1", i2c_addr=0x3c)**

i2cデバイスファイルを開け、描画用サブスレッドを開始します。

### **end()**

サブスレッドを終了し、i2cデバイスをクローズします。

**clear(update=1,sync=0,timeout=0.5,fill=0,area=(0,0,128,64))**

描画バッファをクリアします。

fillで塗りつぶす色(0:黒、1:白)を指定できます。

areaを指定してクリアする領域を指定できます。
指定の仕方は(x,y,w,h)または((x,y),(w,h))です。

update=1であれば、クリア後すぐにoledに転送します。

sync=1とすることで、 転送の終了を待ちます。
デフォルトのsync=0だと、転送の終了を待たずバッファクリア後すぐに
戻ります。

timeoutで転送の終了を待つ最大時間を指定できます。
単位は秒です。timeoutが発生すると Python3では TIMEOUT例外、
Python2では rapioled.error 例外が発生します。

**image(image,dst_area=NULL,src_area=NULL,update=1,sync=0,timeout=0.5)**

Pillowのimageオブジェクトを渡し、
Oledの描画バッファに書き込みます。

imageはPillowのImageオブジェクトです。
形式はビットマップ、すなわちmodeは'1'でなくてはいけません。

dst_areaでoled側の書き込む領域,
src_areaでイメージ側読み出し領域を
を指定できます。
指定しない場合、それぞれ全領域が対象となります。

領域の指定の方法は (x,y), (x,y,w,h), ((x,y),(w,h))のいずれかです。
wとhが指定されない場合、可能な最大幅が指定されたものとされます。

update,sync, timeout引数は clearメソッドと同じです。

**shift(amount=(-1,0),area=(0,0,128,64),fill=0,update=1,sync=0,timeout=0.5)**

oled上の指定された領域を指定された量だけ平行移動させます。  

amountは移動量をx成分とy成分でしていします。
デフォルトは(-1,0)で左に1ドット移動させます。

areaはシフトさせる領域を指定します。
形式は (x,y,w,h)か((x,y)(w,h))のいずれかです。
指定がなければ、画面全体を移動させます。

fillは、移動の結果できた隙間を塗りつぶす色(0 or 1)を指定します。

update, sync, timeoutは clearメソッドと同じです。


**vsync(timeout=0.5)**

clear, image_bytes メソッド後のoledへのデータのデータ転送を待ちます。  
データ転送が行わなれていなければ、すぐに返ります。  
timeoutは clearコマンドと同じです。




    
