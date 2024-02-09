/**
 * @file TinyUSB とアプリケーションを接続するラッパー
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <platform.h>
#include <tusb.h>
#include <host/usbh.h>

#include "utils.h"
#include "led.h"
#include "tusb_if.h"

R_BSP_PRAGMA_INTERRUPT(INT_Excep_USB0_USBI0, VECT(USB0,USBI0))

/**
 * @brief USB0 USBI割り込み
 */
R_BSP_ATTRIB_INTERRUPT void INT_Excep_USB0_USBI0(void) {
    tuh_int_handler(0);
    return ;
}

/**
 * @brief 初期化する
 */
void tusbif_init(void) {
    // MCU端子設定
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス許可

    // VBUSEN端子
    MPC.P16PFS.BYTE = 0x12u; // USB0_VBUSEN
    PORT1.PMR.BIT.B6 = 1u; // P16 to Peripheral.
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス禁止
    PORT1.PMR.BIT.B6 = 1u; // P16 to Peripheral.

    // 割り込みレベル設定
    IPR(USB0, USBI0) = 3;

    // TinyUSB初期化
    tusb_init();

    return ;
}

/**
 * @brief USBイベント処理をする
 */
void tusbif_proc_usb_event(void) {
    if (tuh_task_event_ready()) {
        led_set(true);
        tuh_task(); // USB ホスト処理
        led_set(false);
    }
    return ;
}

/**
 * @brief HIDデータ受信時コールバック(と思われる)
 * @param dev_addr 送信元デバイスのデバイスアドレス
 * @param idx インデックス番号
 * @param report レポートデータ
 * @param len レポートデータのサイズ
 */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
    // do nothng.
    return ;
}

/**
 * @brief 指定時間待つ。
 * @param msec 待機する時間[ミリ秒]
 * @note R_BSP_SoftwareDelay()の方が良いかも。
 */
void osal_task_delay(uint32_t msec) {
    uint32_t begin = get_tick_count();
    while ((get_tick_count() - begin) < msec) {
        // do nothing.
    }
}
