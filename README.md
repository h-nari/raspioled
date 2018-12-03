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

## 使用例

## インストール方法

