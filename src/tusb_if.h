/**
 * @file TinyUSB とアプリケーションを接続するラッパー
 * @author Cosmosweb Co.,Ltd. 2024
 */

#ifndef TUSB_IF_H_
#define TUSB_IF_H_

#include <stdint.h>

void tusbif_init(void);

void tusbif_proc_usb_event(void);

#endif /* TUSB_IF_H_ */
