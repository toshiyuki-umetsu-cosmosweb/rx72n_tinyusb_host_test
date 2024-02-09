This source codes are copied from https://github.com/hathach/tinyusb in February 1 2004.

# オンラインドキュメント

https://docs.tinyusb.org/en/latest/



# メモ

Getting Startedの和訳に補足したもの。

1. git submodule でリポジトリを追加するか、tinyusbの全ソースコードをプロジェクトにコピーします。

2. tinyusb/src内の全ての.cをプロジェクトに追加します。

3. tinyusb/srcをインクルードパスに追加します。
   あわせて、インクルードパスにtusb_config.hが含まれていることを確認します。

4. 必要なマクロがすべて tusb_config.h で適切に定義されていることを確認します。
CFG_TUSB_MCU、CFG_TUSB_OS などのいくつかのマクロを追加する必要があります。
CFG_TUSB_MCUは tusb_option.h内のOPT_MCU_xxxx を定義します。例えば    
「#define CFG_TUSB_MCU OPT_MCU_RX72N」などとします。

デバイススタックを使用する場合は、USB ディスクリプタを作成/変更します。
最終的にはスタックが機能するために、全ての tud ディスクリプタコールバックを実装する必要があります。

5. tusb_init()呼び出しをリセット初期化コードに追加します。

6. USB IRQ ハンドラーで tud_int_handler() (デバイス時) および/または tuh_int_handler() (ホスト時) を呼び出します。

7. 有効なクラスのコールバックを全て実装します。

8. RTOSを全く使用しない場合は、tud_task()(デバイス時) / tuh_task() (ホスト時)関数を継続的または定期的に呼び出す必要があります。全てのコールバックと昨日は、そのタスクランナーの呼び出し内で処理され、呼び出されます。


# RX-72N (GCC for Renesas RX)ポーティングメモ

コピーしてきたソースだと、portable/renesas/rusb2/rusb2_type.h でエラーが出ます。
これはCCRXがターゲットとなっているためです。
そのため、GNU C用に生成されたiodefine.hを元に一部修正をかけました。
__CCRX__ が定義されているかどうかで大きく分岐させましたが、ソースコードの無駄でした。

* TU_ATTR_PACKED_BEGIN
* TU_ATTR_BIT_FIELD_ORDER_BEGIN
* TU_ATTR_PACKED

を上手く処理してあげるように実装すると良かったように思います。
FITモジュール、CGコンポーネントと組み合わせる場合は、iodefine.hをインクルードするよりも、
platform.hをインクルードした方が良いです。


