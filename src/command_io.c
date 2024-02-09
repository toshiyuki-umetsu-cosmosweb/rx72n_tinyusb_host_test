/**
 * @file コマンドI/O
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <r_sci_rx_if.h>
#include <r_sci_rx_pinset.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"
#include "command_table.h"
#include "command_usb.h"

/**
 * コマンド受信バッファサイズ
 *
 * @note SCI FITモジュールで使用している受信キューとは別。
 */
#define RX_BUFSIZE (256)
/**
 * バックスペース ASCIIコード
 */
#define ASCII_CODE_BS (0x08)
/**
 * LF ASCIIコード
 */
#define ASCII_CODE_LF (0x0a)
/**
 * CR ASCIIコード
 */
#define ASCII_CODE_CR (0x0d)

static void run_command(void);
static void command_proc(char *cmdbuf);
static int32_t make_argv(char *str, char **argv, int32_t max_argc);
static void cmd_args(int ac, char **av);
static void cmd_help(int ac, char **av);
static void cmd_reset(int ac, char **av);
static void sci2_callback(void *arg);

/**
 * SCIハンドル
 */
static sci_hdl_t SciHandle = NULL;

/**
 *  受信コマンドバッファ(末尾ヌル文字)
 */
static char RxBuf[RX_BUFSIZE];
/**
 * 受信データ長
 */
static uint32_t RxDataLength = 0;
/**
 * プロンプト文字列
 */
static const char *PromptStr = "> ";

/**
 * コマンドエントリテーブル
 */
//@formatter:off
static const struct cmd_entry CommandEntries[] = {
    { "args", "Print arguments.", cmd_args },
    { "help", "Print help message.", cmd_help },
    { "reset", "Reset software.", cmd_reset },
    { "usb", "USB control.", cmd_usb }
};
//@formatter:on
/**
 * コマンドエントリ数
 */
static const int CommandEntryCount = (int) (sizeof(CommandEntries) / sizeof(struct cmd_entry));
/**
 * 最後に入力検知した時間
 */
static uint32_t LastInputTick;

/**
 * @brief コマンドI/Oを初期化する。
 */
void command_io_init(void)
{
    RxBuf[0] = '\0';
    RxDataLength = 0;
    LastInputTick = get_tick_count();

    sci_cfg_t cfg;
    memset (&cfg, 0, sizeof(cfg));
    cfg.async.baud_rate = 115200;
    cfg.async.clk_src = SCI_CLK_INT;
    cfg.async.data_size = SCI_DATA_8BIT;
    cfg.async.parity_en = SCI_PARITY_OFF;
    cfg.async.stop_bits = SCI_STOPBITS_1;
    cfg.async.int_priority = 15;
    sci_err_t result = R_SCI_Open (SCI_CH2, SCI_MODE_ASYNC, &cfg, sci2_callback, &SciHandle);
    if (result != SCI_SUCCESS)
    {
        // Error : ignored.
        R_BSP_SoftwareReset ();
    }
    else
    {
        uint8_t priority = BSP_MCU_IPL_MAX;
        R_SCI_Control(SciHandle, SCI_CMD_SET_TXI_PRIORITY, &priority);
        R_SCI_Control(SciHandle, SCI_CMD_SET_RXI_PRIORITY, &priority);
    }

    // ピン設定
    R_SCI_PinSet_SCI2 ();

    printf (PromptStr); // プロンプト出力

    return;
}

/**
 * @brief コマンドI/Oを停止する。
 */
void command_io_fini(void)
{
    if (SciHandle != NULL)
    {
        R_SCI_Close (SciHandle);
        SciHandle = NULL;
    }
    return;
}

/**
 * @brief コマンドI/Oを更新する。
 */
void command_io_update(void)
{
    uint32_t now = get_tick_count();
    uint8_t d;
    while (R_SCI_Receive (SciHandle, &d, 1) == SCI_SUCCESS) // 受信文字がある？
    {
        LastInputTick = now;

        if (d == ASCII_CODE_BS)
        {
            if (RxDataLength > 0)
            {
                RxDataLength--;
                // エコーバック(BSが可能な場合のみ)
                putchar (d);
            }
        }
        else
        {
            if ((d != ASCII_CODE_LF) // LFでない？
                    && (RxDataLength > 0) // バッファに文字がある？
                    && (RxBuf[RxDataLength - 1] == ASCII_CODE_CR)) { // 直前の入力がCR？
               run_command();
            }

            if (RxDataLength < (sizeof(RxBuf) - 2))
            {
                RxBuf[RxDataLength] = (char) (d);
                RxDataLength++;
                // エコーバック
                putchar (d);
            }
        }

        if (d == '\n')
        {
            run_command();
        }
    }

    if (((now - LastInputTick) >= 50u) // 最後に入力されてから50ミリ秒経過？
            && (RxDataLength > 0) // 受信バッファに文字がある？
            && (RxBuf[RxDataLength - 1] == ASCII_CODE_CR)) // 末尾がCR？
    {
        run_command();
    }

    return;
}

/**
 * @brief 現在受信バッファにたまっている文字列をコマンドラインとして処理する。
 * 受信バッファはクリアされる。
 *
 */
static void run_command(void) {
    RxBuf[RxDataLength] = '\0'; // コマンド処理前にヌル文字終端させる。

    // コマンド処理
    command_proc (RxBuf);

    printf (PromptStr); // プロンプト出力

    // 受信コマンドバッファリセット
    RxBuf[0] = '\0';
    RxDataLength = 0;

    return ;
}

/**
 * @brief cmdbufで渡された入力を元に処理する。
 *
 * @param cmdbuf コマンドバッファ
 */
static void command_proc(char *cmdbuf)
{
    char *argv[32];
    int argc = make_argv (cmdbuf, argv, sizeof(argv) / sizeof(char*));
    if (argc > 0)
    {
        const struct cmd_entry *pentry = command_table_find_cmd (CommandEntries, CommandEntryCount, argv[0]);
        if (pentry != NULL)
        {
            pentry->cmd_proc (argc, argv);
        }
        else
        {
            printf ("Unknown command: %s\n", argv[0]);
        }

    }
    return;
}

/**
 * @brief strをデリミタで区切って文字列を得る。
 *
 * @param str 文字列が格納されたアドレス。ヌル文字終端されている必要があります。
 * @param argv トークンを格納する配列
 * @param max_argc 配列の最大数
 * @return トークン数が返ります。
 */
static int32_t make_argv(char *str, char **argv, int32_t max_argc)
{
    char *p = str;
    int32_t argc = 0L;
    const char *delim = " \t\r\n";

    while ((*p != '\0') && (argc < max_argc))
    {
        while ((*p != '\0') && (strchr (delim, *p) != NULL))
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }

        if ((*p == '\'') || (*p == '\"'))
        {
            char *end = strchr (p + 1, *p);
            if (end != NULL)
            {
                char *token = p + 1;
                argv[argc] = token;
                argc++;
                *end = '\0';
                p = end + 1;
            }
            else
            {
                char *token = p;
                argv[argc] = token;
                argc++;
                p = token + strlen (token);
            }
        }
        else
        {
            char *begin = p;
            while ((*p != '\0') && (strchr (delim, *p) == NULL) && (argc < max_argc))
            {
                p++;
            }
            if (*p != '\0')
            {
                *p = '\0';
                p++;
            }
            argv[argc] = begin;
            argc++;
        }
    }

    return argc;
}

/**
 * @brief args コマンドを処理する
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_args(int ac, char **av)
{
    for (int i = 0; i < ac; i++)
    {
        printf ("args[%d]:%s\n", i, av[i]);
    }

    return;
}

/**
 * @brief help コマンドを処理する。
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_help(int ac, char **av)
{
    for (uint32_t i = 0u; i < CommandEntryCount; i++)
    {
        const struct cmd_entry *pentry = &(CommandEntries[i]);
        if ((pentry->cmd != NULL) && (pentry->desc != NULL))
        {
            printf ("%s - %s\n", pentry->cmd, pentry->desc);
        }
    }

    return;
}

/**
 * @brief reset コマンドを処理する
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_reset(int ac, char **av)
{
    R_BSP_SoftwareReset ();

    return;
}

/**
 * @brief FIT SCIモジュールからのコールバックを受け取る。
 *
 * @param arg 引数
 */
static void sci2_callback(void *arg)
{
    sci_cb_args_t *args = (sci_cb_args_t*) (arg);
    switch (args->event)
    {
        case SCI_EVT_RX_CHAR: // 1文字受信
        {
            // Note : 有効データ args.byte
            // do nothing.
            break;
        }
        case SCI_EVT_RX_CHAR_MATCH: // 指定した文字と一致
        {
            // do nothing.
            break;
        }
#if SCI_CFG_TEI_INCLUDED
        case SCI_EVT_TEI:
        {
            break;
        }
#endif
        case SCI_EVT_RXBUF_OVFL:
        {
            // do nothing.
            break;
        }
        case SCI_EVT_OVFL_ERR:
        {
            // do nothing.
            break;
        }
        case SCI_EVT_FRAMING_ERR:
        {
            // do nothing.
            break;
        }
        case SCI_EVT_PARITY_ERR:
        {
            // do nothing.
            break;
        }
        case SCI_EVT_RX_DONE:
        {
            // Next request.
            break;
        }
        default:
        {
            // do nothing.
            break;
        }
    }

    return;
}

/**
 * @brief 1文字読み出す。
 *
 * @note 1文字受信するまで、呼び出し元をブロックします。
 * @note 標準入出力の入力処理になります。
 *
 * @return 受信した文字
 */
char my_charget(void)
{
    if (SciHandle != NULL)
    {
        uint8_t d;
        while (R_SCI_Receive (SciHandle, &d, 1) != SCI_SUCCESS)
        {
            ; // do nothing.
        }
        return (char) (d);
    }
    else
    {
        return '\0';
    }
}

/**
 * @brief 1文字送信する。
 *
 * @note 送信バッファに書き出せるまで、呼び出し元をブロックする。
 * @note 標準出力処理になります。
 *
 * @param c 文字
 */
void my_charput(char c)
{

    if (SciHandle != NULL)
    {
        while (R_SCI_Send (SciHandle, &c, 1) != SCI_SUCCESS)
        {
            ; // do nothing.
        }
    }
}
