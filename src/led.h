/**
 * @file ユーザーLEDドライバ
 * @author Cosmosweb Co.,Ltd. 2024
 */

#ifndef LED_H_
#define LED_H_

#include <stdbool.h>

void led_set(bool is_on);
bool led_is_set(void);

#endif /* LED_H_ */
