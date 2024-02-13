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

## コンパイラはCC-RX前提のコードになっています。

コピーしてきたソースだと、portable/renesas/rusb2/rusb2_type.h でエラーが出ます。
これはCCRXがターゲットとなっているためです。
そのため、GNU C用に生成されたiodefine.hを元に一部修正をかけました。
__CCRX__ が定義されているかどうかで大きく分岐させましたが、ソースコードが無駄に長くなりました。

* TU_ATTR_PACKED_BEGIN
* TU_ATTR_BIT_FIELD_ORDER_BEGIN
* TU_ATTR_PACKED

を上手く処理してあげるように実装すると良かったように思います。
FITモジュール、CGコンポーネントと組み合わせる場合は、iodefine.hをインクルードするよりも、
platform.hをインクルードした方が良いです。

## WDTを使うとすると一工夫必要

tuh_task()内の処理が普通に秒オーダーでかかることがあり、
こうなると内蔵WDTのカウンタを余裕でタイムアウトさせてしまいます。
そのため、WDTを使う場合には一工夫が必要です。
今回のサンプルでは割愛しました。

## 接続処理中にATTACHイベントが入るとハングアップします。

usbh.cにて、接続処理中にATTACHイベントが入ったとき、
後で処理するためにイベントキューにATTACHイベントを追加します。
tuh_task()内でイベントキューが空になるまで制御を返さないため、ハングアップします。
usbh.cのコメントに「better to have an separated queue for newly attached devices」とあるとおり、
接続処理中に検出したATTCHイベントは別キューに入れるように変更しました。

## (RX-72N)切断処理するとハングアップします。

切断処理、つまりUSBを引っこ抜くと応答が返らなくなります。
これはTU_ATTR_WEAK void osal_task_delay()から戻らなくなるためのようでした。

~~~
TU_ATTR_WEAK void osal_task_delay(uint32_t msec) {
  const uint32_t start = hcd_frame_number(_usbh_controller);
  while ( ( hcd_frame_number(_usbh_controller) - start ) < msec ) {} // ここから出てこない
}
~~~
hcd_frame_number()が1msec毎に更新されることを使用した設計で、RX用ポーティングでは、
FRMNUMのFRNM[10:0]の値を返す実装になっています。
このSRNMはSOF発行タイミングに更新されますが、どうやら未接続では更新されないようで、
結果としてデバイス未接続中は時間計測が正しくできないようです。

usbh.cのコメントに「rework time-related function later」とあるとおり、
osal_task_delayは定義しなおしした方がよいです。
このテストコードでは、TPU0,ELC,CMTW0を使用して、
1ms毎に歩進するハードウェアTickカウンタを用意し、
これを使ってosal_task_delay()の機能を提供するように変更しました。

~~~
uint32_t get_tick_count(void) {
    return CMTW0.CMWCNT;
}
~~~
~~~
void osal_task_delay(uint32_t msec) {
    volatile uint32_t begin = get_tick_count();
    while ((get_tick_count() - begin) < msec) {
        // do nothing.
    }
}
~~~

## ちょっとわかりづらかったマクロ関数

### TU_VERIFY(__VA_ARGS__)

 **__VA_ARGS__**  が偽の時に **false** を返す。
 #define定義のちょっと上にコメントで説明が書いてありました。

~~~
TU_VERIFY(__VA_ARGS__)
｜
｜ #define TU_VERIFY(...)                 _GET_3RD_ARG(__VA_ARGS__, TU_VERIFY_2ARGS, TU_VERIFY_1ARGS, _dummy)(__VA_ARGS__)
↓
_GET_3RD_ARG(__VA_ARGS__, TU_VERIFY_2ARGS, TU_VERIFY_1ARGS, _dummy)(__VA_ARGS__)
｜
｜ #define _GET_3RD_ARG(arg1, arg2, arg3, ...)        arg3
↓
TU_VERIFY_1ARGS(__VA_ARGS__)
｜
｜ #define TU_VERIFY_1ARGS(_cond)         TU_VERIFY_DEFINE(_cond, false)
↓
TU_VERIFY_DEFINE(__VA_ARGS__, false)
｜
｜#define TU_VERIFY_DEFINE(_cond, _ret)    \
｜  do {                                   \
｜    if ( !(_cond) ) { return _ret; }     \
｜  } while(0)
↓
do {
    if (!(__VA_ARGS__) { 
        return false;
    }
} while (0)
~~~


## USBコネクタがデリケート？

RX-72N Envision Kitのホストコネクタの挿抜具合により、
上手くATTACHされる場合と、されない場合があるようでした。
ちょっと斜めになると難しいようです。




