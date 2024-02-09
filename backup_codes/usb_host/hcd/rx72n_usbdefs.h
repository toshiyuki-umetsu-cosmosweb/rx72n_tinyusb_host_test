/**
 * @file RX-72N用USB定義
 * @author Cosmosweb Co.,Ltd. 2024
 */

#ifndef USB_HOST_HCD_RX72N_USBDEFS_H_
#define USB_HOST_HCD_RX72N_USBDEFS_H_

/**
 * LNST SE0 state
 */
#define LINE_STATE_SE0 (0u)
/**
 * LNST J-State at Full Speed
 */
#define LINE_STATE_FS_JSTATE (1u)
/**
 * LNST K-State at Full Speed
 */
#define LINE_STATE_FS_KSTATE (2u)
/**
 * LNST K-State at Low Speed
 */
#define LINE_STATE_LS_KSTATE (1u)
/**
 * LNST J-State at Low Speed
 */
#define LINE_STATE_LS_JSTATE (2u)
/**
 * LNST SE1
 */
#define LINE_STATE_SE1 (3u)

/**
 * RHST Unstable (不定状態)
 */
#define RHST_UNSTABLE (0u)
/**
 * RHST Low Speed
 */
#define RHST_LOW_SPEED (1u)
/**
 * RHST Full Speed
 */
#define RHST_FULL_SPEED (2u)

/**
 * バッファレディ
 */
#define INTSTS0_BRDY (1u << 8u)
/**
 * バッファノットレディ
 */
#define INTSTS0_NRDY (1u << 9u)
/**
 * バッファエンプティ
 */
#define INTSTS0_BEMP (1u << 10u)
/**
 * コントロール転送ステージ遷移
 */
#define INTSTS0_CTRT (1u << 11u)
/**
 * フレーム番号更新
 */
#define INTSTS0_SOFR (1u << 13u)
/**
 * VBUS割り込みステータス
 */
#define INTSTS0_VBINT (1u << 15u)

/**
 * INTSTS1 セットアップトランザクション正常応答
 */
#define INTSTS1_SACK (1u << 4u)
/**
 * INTSTS1 セットアップトランザクションエラー
 */
#define INTSTS1_SIGN (1u << 5u)
/**
 * EOFエラー検出
 */
#define INTSTS1_EOFERR (1u << 6u)
/**
 * 接続検知
 */
#define INTSTS1_ATTCH (1u << 11u)
/**
 * 切断検知
 */
#define INTSTS1_DTCH (1u << 12u)
/**
 * バス変化
 */
#define INTSTS1_BCHG (1u << 14u)
/**
 * オーバーカレント入力変化
 */
#define INTSTS1_OVRCR (1u << 15u)

/**
 * INTSTS1 ホストで処理する必要がある割り込みフラグマスク
 */
#define INTSTS1_MASK ( INTSTS1_OVRCR | INTSTS1_BCHG | INTSTS1_DTCH | INTSTS1_ATTCH | INTSTS1_EOFERR | INTSTS1_SIGN | INTSTS1_SACK)


/**
 * BRDYSTS バッファレディビットの有効値だけ取り出すマスク
 */
#define BRDYSTS_MASK (0x03FFu)
/**
 * BEMPSTS バッファエンプティの有効値だけ取り出すマスク
 */
#define BEMPSTS_MASK (0x03FFu)
/**
 * NRDYSTS バッファノットレディの有効値だけ取り出すマスク
 */
#define NRDYSTS_MASK (0x03FFu)


#endif /* USB_HOST_HCD_RX72N_USBDEFS_H_ */
