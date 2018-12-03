# raspioled
pythin library for 128 x 64 dot OLED display connected to Raspberry Pi via i2c

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


## インストール方法

python2 で使用する場合

    $ sudo python setup.py install
    
python3 で使用する場合

    $ sudo python3 setup.py install

## I2Cの速度を400kHzにする

ラズベリーパイのI2Cの転送速度はデフォルトで100kHzです。
このままでは画像データの転送に約100mSかかり、Adafruit_SSD1306と大差ありません。

I2Cの速度を400kHzに上げると転送速度が約4倍に早くなります。
I2Cの速度はkernelの設定で決まっているようなので、
変更は /boot/config.txt等で行います。

/boot/config.txtに以下の行を追加し、ラズベリーパイを再起動してください。

    dtparam=i2c_baudrate=400000
    
## プロパティ

**size**: oledの画面の幅と高さのタプル、すなわち `(128,64)` を返します。

## メソッド

**begin(dev="/dev/i2c-1", i2c_addr=0x3c)**:

**end**:

**clear**:

**image_byte**:

**vsync**:


    
