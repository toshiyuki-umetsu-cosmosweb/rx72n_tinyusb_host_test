/**
 * @file ユーザーLEDドライバ
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <r_smc_entry.h>
#include "led.h"

/**
 * @brief ユーザーLEDの点灯状態を設定する。
 *
 * @param is_on 点灯させる場合にはtrue, 消灯させる場合にはfalse。
 */
void led_set(bool is_on)
{
    PORT4.PODR.BIT.B0 = (is_on) ? 0u : 1u; // Active Low
}

/**
 * @brief ユーザーLEDが点灯しているかどうかを取得する。
 *
 * @return ユーザーLEDが点灯している場合にはtrue, 消灯している場合にはfalse。
 */
bool led_is_set(void) {
    return (PORT4.PODR.BIT.B0 == 0u);
}
