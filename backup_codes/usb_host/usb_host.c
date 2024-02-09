/**
 * @file
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include "usb_host.h"
#include "hcd/usb_hcd.h"


/**
 * USBホスト初期化
 */
void usb_host_init(void) {

    usb_hcd_init();



    return ;
}
/**
 * USBホスト停止
 */
void usb_host_fini(void) {

    usb_hcd_fini();

    return ;
}
/**
 * USBホスト処理
 */
void usb_host_process(void) {

    return ;
}
