/**
 * @file RX-72N用 HCD Driver コンフィギュレーション
 * @author Cosmosweb Co.,Ltd. 2024
 */

#ifndef USB_HOST_HCD_USB_HCD_RX72N_CONFIG_H_
#define USB_HOST_HCD_USB_HCD_RX72N_CONFIG_H_

/**
 * USB0_VBUSEN 出力信号論理
 * 0: Low active
 * 1: High active
 */
#define VBUSEN_LOGIC (1u)

/**
 * TypeCを使うかどうか
 * 0: 使用しない
 * 1: 使用する
 */
#define SUPPORT_TYPEC (0u)

/**
 * BUS Charge機能を使うかどうか
 * 0: 使用しない
 * 1: 使用する
 */
#define SUPPORT_BUS_CHARGE (0u)

/**
 * USBI 割り込みプライオリティ
 * 0:割り込み使用しない
 * 1～15:割り込み。(15が最大優先度,1が優先度低)
 */
#define USBI_INT_PRIORITY (3u)

/**
 * VBUSEN ピン設定
 * <0:未使用
 * 16:P16
 * 24:P24
 * 32:P32
 */
#define VBUSEN_PIN   (16)
/**
 * OVERCURA ピン設定
 * <0:未使用
 * 14:P14
 */
#define OVERCURA_PIN (-1)
/**
 * OVERCURB ピン設定
 */
#define OVERCURB_PIN (-1)


#endif /* USB_HOST_HCD_USB_HCD_RX72N_CONFIG_H_ */
