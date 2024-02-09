/**
 * @file アプリケーションのエントリポイント
 */
#include <r_smc_entry.h>

#include <stdio.h>
#include <string.h>
#include <tusb_if.h>

#include "led.h"
#include "utils.h"
#include "command_io.h"

void main(void);

static void init_peripherals(void);

/**
 * @brief アプリケーションのエントリポイント
 */
void main(void)
{
    init_peripherals();
    led_set(false);
    command_io_init();
    tusbif_init();

    printf("started.\n");

    uint32_t proc_tick = get_tick_count();
    while (1) {
        tusbif_proc_usb_event(); // USBホストドライバ処理

        command_io_update(); // コマンドがあったら処理する
        uint32_t now = get_tick_count();
        if ((now - proc_tick) >= 1000) {
            proc_tick = now;
        }
    }

    command_io_fini();

    return ;
}

/**
 * @brief ペリフェラルを初期化する。
 */
static void init_peripherals(void) {
    R_Config_ELC_Start(); // ELC開始
    R_Config_CMTW0_Start(); // タイマーカウンタ開始
    R_Config_TPU0_Start(); // プリスケーラ開始

    return ;
}



