/**
 * @file ユーティリティ関数定義
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <r_smc_entry.h>
#include "utils.h"

/**
 * @brief 1msec毎に歩進するTickカウンタの値を得る。
 *
 * @note
 * スマートコンフィギュレータにて、
 * TPU0コンペアマッチ(1ms) ---(ELC)---> CMTW0カウントアップ
 * として動作させている。
 * こうすると、割り込み遅延が発生してもズレないハードウェアTickカウンタになる。
 * ARM MCUだと1つのタイマ内にプリスケーラがあるが、Renesasはそうなっていないので、
 * プリスケーラ用のタイマを用意して、ELCでカウントするようにした。
 *
 * @return Tickカウンタの値。
 */
uint32_t get_tick_count(void) {
    return CMTW0.CMWCNT;
}


