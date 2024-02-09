/**
 * @file USB HCD (Host Controller Driver) implimentatin of RX-72N
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <platform.h>
#include <stdbool.h>
#include <stdint.h>

#include "rx72n_usbdefs.h"
#include "usb_hcd.h"
#include "usb_hcd_rx72n_config.h"


static void setup_pins(void);
static void release_pins(void);
static void set_module_stop(bool is_stop);
static void setup_usb_registers(void);
static uint8_t wait_no_chattaring(void);
static void disable_bus_interrupts(void);

R_BSP_PRAGMA_INTERRUPT(INT_Excep_USB0_D0FIFO0, VECT(USB0,D0FIFO0))
R_BSP_PRAGMA_INTERRUPT(INT_Excep_USB0_D1FIFO0, VECT(USB0,D1FIFO0))
R_BSP_PRAGMA_INTERRUPT(INT_Excep_USB0_USBI0, VECT(USB0,USBI0))
R_BSP_PRAGMA_INTERRUPT(INT_Excep_USB0_USBR0, VECT(USB0,USBR0))

/**
 * ホストコントローラドライバを初期化する。
 */
void usb_hcd_init(void) {
    setup_pins(); // 端子設定
    set_module_stop(false); // USB0ストップ解除

    USB.DPUSR0R.BIT.FIXPHY0 = 0u; // ディープスタンバイ設定
    IPR(USB0, USBR0) = 0u; // Bus Ready割り込み未使用
#if ((USBI_INT_PRIORITY > 0) && (USBI_INT_PRIORITY <= 15))
    IPR(USB0, USBI0) = USBI_INT_PRIORITY; // プライオリティ設定
#else
#error "USBI_INT_PRIORITY must set 1 to 15."
#endif
    R_BSP_InterruptRequestEnable(VECT(USB0,USBI0)); // USBI0割り込み有効


    setup_usb_registers();
#if !SUPPORT_TYPEC
    usb_hcd_set_vbus(true);
#if !SUPPORT_BUS_CHARGE
    R_BSP_SoftwareDelay(100u, BSP_DELAY_MILLISECS);
#endif
#endif

    return ;
}

/**
 * ホストコントローラドライバを停止する。
 */
void usb_hcd_fini(void) {

    USB0.DVSTCTR0.BIT.UACT = 0u; // ダウンポート動作禁止
    while ((USB0.SYSSTS0.BIT.HTACT != 0u) // USB0ホストシーケンサ動作中？
            || (USB0.SYSSTS0.BIT.SOFEA != 0u)) { // SOF出力停止していない？
        // do nothing.
    }

    USB0.SYSCFG.BIT.USBE = 0u; // USB 動作禁止
    while (USB0.SYSCFG.BIT.USBE != 0u) { // 書き込み完了待ち
        // do nothing.
    }
    USB0.SYSCFG.BIT.SCKE = 0u; // USB クロック供給停止
    while (USB0.SYSCFG.BIT.SCKE != 0u) { // 書き込み完了待ち
        // do nothing.
    }

    set_module_stop(true); // USB0ストップ
    release_pins(); // 端子設定解除

    return ;
}

/**
 * 端子機能を設定する。
 */
static void setup_pins(void)
{
    // MCU端子設定
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス許可

    // VBUSEN 出力
#if VBUSEN_PIN == 16
    MPC.P16PFS.BYTE = 0x12u; // USB0_VBUSEN
    PORT1.PMR.BIT.B6 = 1u; // P16 to Peripheral.
#elif VBUSEN_PIN == 24
    MPC.P24PFS.BYTE = 0x13u;
    PORT2.PMR.BIT.B4 = 1u; // P24 to Peripheral.
#elif VBUSEN_PIN == 32
    MPC.P32PFS.BYTE = 0x13u;
    PORT3.PMR.BIT.B2 = 1u; // P32 to Peripheral.
#elif VBUSEN_PIN < 0
    // do nothing.
#else
#error "VBUSEN_PIN unsupported."
#endif
    // OVERCURA 端子設定
#if OVERCURA_PIN == 14
    MPC.P14PFS.BYTE = 0x12u;
    PORT1.PMR.BIT.B4 = 1u; // P14 to Peripheral.
#elif OVERCURA_PIN < 0 // 端子無効？
    // do nothing.
#else
#error "OVERCURA_PIN unsupported."
#endif

    // OVERCURB 端子設定
#if OVERCURB_PIN == 16
    MPC.P16PFS.BYTE = 0x13u; // USB_OVERCURB
    PORT1.PMR.BIT.B6 = 1u; // P16 to Peripheral.
#elif OVERCURB_PIN == 22
    MPC.P22PFS.BYTE = 0x13u; // USB_OVERCURB
    PORT2.PMR.BIT.B2 = 1u; // P22 to Peripheral.
#elif OVERCURB_PIN < 0
    // do nothing.
#else
#error "OVERCURB_PIN unsupported"
#endif

    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス禁止

    return ;
}

/**
 * 端子機能を解放する。
 */
static void release_pins(void)
{
    // MCU端子設定
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス許可

    // VBUSEN 出力
#if VBUSEN_PIN == 16
    MPC.P16PFS.BYTE = 0x0u; // Hi-Z
    PORT1.PDR.BIT.B6 = 1u; // Output
    PORT1.PODR.BIT.B6 = !VBUSEN_LOGIC; // VBUSEN = OFF
    PORT1.PMR.BIT.B6 = 0u; // P16 to IO Port.
#elif VBUSEN_PIN == 24
    MPC.P24PFS.BYTE = 0x0u; // Hi-Z
    PORT2.PDR.BIT.B4 = 1u; // Output
    PORT2.PODR.BIT.B4 = !VBUSEN_LOGIC; // VBUSEN = OFF
    PORT2.PMR.BIT.B4 = 0u; // P24 to IO Port.
#elif VBUSEN_PIN == 32
    MPC.P32PFS.BYTE = 0x0u; // Hi-Z
    PORT3.PDR.BIT.B2 = 1u; // Output
    PORT3.PODR.BIT.B2 = !VBUSEN_LOGIC; // VBUSEN = OFF
    PORT3.PMR.BIT.B2 = 0u; // P24 to IO Port.
#else
#error "VBUSEN_PIN unsupported."
#endif

    // OVERCURA 端子設定
#if OVERCURA_PIN == 14
    MPC.P14PFS.BYTE = 0x00u; // Hi-Z
    PORT1.PMR.BIT.B4 = 0u; // P14 to Input.
#elif OVERCURA_PIN < 0 // 端子無効？
    // do nothing.
#else
#error "OVERCURA_PIN unsupported."
#endif

    // OVERCURB 端子設定
#if OVERCURB_PIN == 16
    MPC.P16PFS.BYTE = 0x00u; // Hi-Z
    PORT1.PMR.BIT.B6 = 0u; // P16 to IO Port
#elif OVERCURB_PIN == 22
    MPC.P22PFS.BYTE = 0x00u; // Hi-Z
    PORT2.PMR.BIT.B2 = 0u; // P22 to IO Port.
#elif OVERCURB_PIN < 0
    // do nothing.
#else
#error "OVERCURB_PIN unsupported"
#endif

    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // MPCレジスタアクセス禁止

    return ;
}
/**
 * USB0のモジュールストップ状態を設定する。
 *
 * @param is_stop モジュールストップさせる場合にはtrue, それ以外はfalse.
 */
static void set_module_stop(bool is_stop) {
    bsp_int_ctrl_t int_ctrl;
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_LPC_CGC_SWR); // MSTPレジスタアクセス許可
    R_BSP_InterruptControl(BSP_INT_SRC_EMPTY, BSP_INT_CMD_FIT_INTERRUPT_DISABLE, &int_ctrl);

    if (is_stop) {
        MSTP(USB0) = 1u;
    } else {
        MSTP(USB0) = 0u;
    }

    R_BSP_InterruptControl(BSP_INT_SRC_EMPTY, BSP_INT_CMD_FIT_INTERRUPT_ENABLE, &int_ctrl);
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_LPC_CGC_SWR); // MSTPレジスタアクセス禁止

    return ;
}

/**
 * USBコントローラのレジスタを設定する。
 */
static void setup_usb_registers(void) {

    USB0.SYSCFG.BIT.SCKE = 1u; // USBへのクロック供給許可
    while (USB0.SYSCFG.BIT.SCKE == 0u) { // SCKEが1になるまで待つ
        // do nothing.
    }

    USB0.PHYSLEW.LONG = 0x5u; // 0101b 固定

    USB0.SYSCFG.BIT.DPRPU = 0u; // D+プルアップ禁止
    while (USB0.SYSCFG.BIT.DPRPU != 0) { // DPRPU書き替え待ち
        // do nothing.
    }
    USB0.SYSCFG.BIT.DRPD = 0u; // D+/D- プルダウン禁止
    while (USB0.SYSCFG.BIT.DRPD != 0) { // DRPD書き替え待ち
        // do nothing.
    }

    USB0.SYSCFG.BIT.DCFM = 1u; // ホストコントローラ機能選択

    USB0.SYSCFG.BIT.DRPD = 1u; // D+/D-ラインプルダウン許可
    uint8_t line_state = wait_no_chattaring();

    USB0.SYSCFG.BIT.USBE = 1u; // USB 動作許可

    USB0.CFIFOSEL.BIT.MBW = 1u; // CFIFOアクセス幅は16bit
    USB0.D0FIFOSEL.BIT.MBW = 1u; // D0FIFOアクセス幅は16bit
    USB0.D1FIFOSEL.BIT.MBW = 1u; // D1FIFOアクセス幅は16bit

    switch (line_state) {
        case LINE_STATE_FS_JSTATE:
        case LINE_STATE_LS_JSTATE:
        {
            USB0.DVSTCTR0.BIT.USBRST = 1u; // USBバスリセット送信開始
            R_BSP_SoftwareDelay(50u, BSP_DELAY_MILLISECS);
            USB0.DVSTCTR0.BIT.USBRST = 0u; // USBバスリセット送信停止
            while (USB0.DVSTCTR0.BIT.UACT != 0) { // USB ダウンポート許可の間待つ
                // do nothing.
            }
            if (USB0.DVSTCTR0.BIT.RHST == RHST_LOW_SPEED) { // Low Speed接続中？
                USB0.SOFCFG.BIT.TRNENSEL = 1u; // Low Speed対応
            }
            USB0.DVSTCTR0.BIT.UACT = 1; // ダウンポート動作許可
            break;
        }
        case LINE_STATE_SE0: // USBデバイス未接続
        {
            USB0.INTENB1.BIT.ATTCHE = 1u; // USB接続割り込み許可
            break;
        }
        default:
        {
            // do nothing.
            break;
        }
    }

    // OVRCRと、未使用割り込みフラグをクリア
    USB0.INTSTS1.WORD &= ((~INTSTS1_OVRCR) & INTSTS1_MASK);

    // 割り込み有効化
    USB0.INTENB0.WORD = INTSTS0_BEMP | INTSTS0_NRDY | INTSTS0_BRDY;
    USB0.INTENB1.WORD = INTSTS1_OVRCR | INTSTS1_ATTCH;

    return ;
}

/**
 * D+/D- ラインのチャタリングがとれるまで待つ
 * @return D+/D-ラインステータス値。
 *         0: SE0
 *         1: K-State(LS時) / J-State(FS時)
 *         2: J-State(LS時) / K-State(FS時)
 *         3: SE1
 */
static uint8_t wait_no_chattaring(void) {
    uint8_t line_states[4];

    /* WAIT_LOOP */
    line_states[0] = USB0.SYSSTS0.BIT.LNST;
    R_BSP_SoftwareDelay(1u, BSP_DELAY_MILLISECS); // 1ms wait
    line_states[1] = USB0.SYSSTS0.BIT.LNST;
    while (1)
    {
        R_BSP_SoftwareDelay(1u, BSP_DELAY_MILLISECS); // 1ms wait
        line_states[2] = USB0.SYSSTS0.BIT.LNST;
        if ((line_states[0] == line_states[1]) && (line_states[0] == line_states[2]))
        {
            break;
        }

        // Retry.
        line_states[0] = line_states[1];
        line_states[1] = line_states[2];
    }
    return line_states[0];
}


/**
 * VBUS出力を設定する。
 *
 * @param is_on VBUS供給ONの場合にはtrue, 供給OFFの場合にはfalse.
 */
void usb_hcd_set_vbus(bool is_on) {
#if VBUSEN_LOGIC
    USB0.DVSTCTR0.BIT.VBUSEN = (is_on) ? 1u : 0u;
#else
    USB0.DVSTCTR0.BIT.VBUSEN = (is_on) ? 0u : 1u;
#endif

}

/**
 * VBUS出力が供給されているかどうかを得る。
 *
 * @return 供給されている場合にはtrue, 供給されていない場合にはfalse
 */
bool usb_hcd_is_vbus(void) {
#if VBUSEN_LOGIC
    return USB0.DVSTCTR0.BIT.VBUSEN != 0u;
#else
    return USB0.DVSTCTR0.BIT.VBUSEN == 0u;
#endif
}


/**
 * USB0 D0FIFO0割り込み
 */
R_BSP_ATTRIB_INTERRUPT void INT_Excep_USB0_D0FIFO0(void) {
    // do nothing.

    return ;
}

/**
 * USB0 D1FIFO0割り込み
 */
R_BSP_ATTRIB_INTERRUPT void INT_Excep_USB0_D1FIFO0(void) {
    // do nothing.

    return ;
}


/**
 * USB0 USBI割り込み
 */
R_BSP_ATTRIB_INTERRUPT void INT_Excep_USB0_USBI0(void) {

    uint16_t intsts0 = USB0.INTSTS0.WORD & USB0.INTENB0.WORD;
    uint16_t intsts1 = USB0.INTSTS1.WORD & USB0.INTENB1.WORD;

    if ((intsts1 & INTSTS1_SACK) != 0) // セットアップトランザクション正常応答
    {
        USB0.INTSTS1.BIT.SACK = 0;
        USB0.INTENB1.WORD &= (uint16_t)(~(INTSTS1_SACK | INTSTS1_SIGN)); // SACK/SIGN 割り込み通知解除

        // TODO : 通知
    }
    else if ((intsts1 & INTSTS1_SIGN) != 0) // セットアップトランザクションエラー
    {
        USB0.INTSTS1.BIT.SIGN = 0u;
        USB0.INTENB1.WORD &= (uint16_t)(~(INTSTS1_SACK | INTSTS1_SIGN)); // SACK/SIGN 割り込み通知解除

        // TODO : 通知
    }
    else if ((intsts0 & INTSTS0_BRDY) != 0) // バッファレディ
    {
        uint16_t brdysts = USB0.BRDYSTS.WORD & USB0.BRDYENB.WORD;
        USB0.BRDYSTS.WORD = (uint16_t)((~brdysts) & BRDYSTS_MASK); // 割り込みフラグクリア

        // TODO : 通知 (brdystsがバッファレディのビット)
    }
    else if ((intsts0 & INTSTS0_BEMP) != 0) // バッファエンプティ
    {
        uint16_t bempsts = USB0.BEMPSTS.WORD & USB0.BEMPENB.WORD;
        USB0.BEMPSTS.WORD = (uint16_t)((~bempsts) & BEMPSTS_MASK); // 割り込みフラグクリア

        // TODO : 通知 bempstsがバッファエンプティのビット
    }
    else if ((intsts0 & INTSTS0_NRDY)) // バッファノットレディ
    {
        uint16_t nrdysts = USB0.NRDYSTS.WORD & USB0.NRDYENB.WORD;
        USB0.NRDYSTS.WORD = (uint16_t)((~nrdysts) & NRDYSTS_MASK);

        // TODO : 通知 nrdysts がバッファノットレディのビット

    }
    else if ((intsts1 & INTSTS1_OVRCR) != 0) // オーバーカレント割り込み
    {
        USB0.INTSTS1.BIT.OVRCR = 0u;

        // TODO : 通知
    }
    else if ((intsts1 & INTSTS1_ATTCH) != 0) // 接続割り込み
    {
        disable_bus_interrupts();

        // TODO : 通知

    }
    else if ((intsts1 & INTSTS1_EOFERR) != 0) // EOFエラー検出割り込み
    {
        USB0.INTSTS1.BIT.EOFERR = 0;

        // TODO : 通知
    }
    else if ((intsts1 & INTSTS1_BCHG) != 0) // バス状態変化
    {
        USB0.INTSTS1.BIT.BCHG = 0u;
        USB0.INTENB1.BIT.BCHGE = 0u;

        // TODO : 通知

    }
    else if ((intsts1 & INTSTS1_DTCH) != 0) // 切断
    {
        disable_bus_interrupts();

        // TODO : 通知
    }
    else if ((intsts0 & INTSTS0_VBINT) != 0) // VBUS状態変化
    {
        USB0.INTSTS0.BIT.VBINT = 0u;

        // TODO : 通知
    }
    else if ((intsts0 & INTSTS0_SOFR) != 0) // フレーム番号更新
    {
        USB0.INTSTS0.BIT.SOFR = 0u;

        // TODO : 通知
    }







}

/**
 * USB0 USBR0割り込み
 */
R_BSP_ATTRIB_INTERRUPT void INT_Excep_USB0_USBR0(void) {

}

/**
 * バス割り込みを無効にする。
 *
 * @note
 * ATTCH, DTCH, BCHG 割り込みステータスをクリアし、
 * 該当割り込みを禁止に設定する。
 */
static void disable_bus_interrupts(void) {
    USB0.INTSTS1.BIT.ATTCH = 0u;
    USB0.INTENB1.BIT.ATTCHE = 0u;
    USB0.INTSTS1.BIT.DTCH = 0u;
    USB0.INTENB1.BIT.DTCHE = 0u;
    USB0.INTSTS1.BIT.BCHG = 0u;
    USB0.INTENB1.BIT.BCHGE = 0u;

    return ;
}

