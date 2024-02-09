/**
 * @file usbコマンドインタフェース
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <stddef.h>
#include <stdio.h>
#include <platform.h>

#include <host/usbh.h>

#include "command_table.h"
#include "command_usb.h"

static void cmd_usb_help(int ac, char **av);
static void cmd_usb_bus_state(int ac, char **av);
static void cmd_usb_int_state(int ac, char **av);
static void cmd_usb_list(int ac, char **av);

/**
 * @brief USBコマンドテーブル
 */
//@formatter:off
static const struct cmd_entry USBCommandEntries[] = {
    { "bus-state", "Print USB bus state.", cmd_usb_bus_state },
    { "int-state", "Print USB int state.", cmd_usb_int_state },
    { "list", "Print USB devices.", cmd_usb_list },
    { "help", "Print USB subcommand list.", cmd_usb_help }
};
//@formatter:on
/**
 * @brief USBコマンドエントリ数
 */
static const int USBCommandEntryCount = (int) (sizeof(USBCommandEntries) / sizeof(struct cmd_entry));

/**
 * @brief usb コマンドを処理する
 * @param ac 引数の数
 * @param av 引数配列
 */
void cmd_usb(int ac, char **av)
{
    if (ac >= 2)
    {
        const struct cmd_entry *pentry = command_table_find_cmd (USBCommandEntries, USBCommandEntryCount, av[1]);
        if (pentry != NULL)
        {
            pentry->cmd_proc (ac, av);
        }
        else
        {
            printf ("unknown usb sub command: %s\n", av[1]);
        }
    }
    else
    {
        cmd_usb_help (0, NULL);
    }

    return;
}

/**
 * @brief usb help コマンドを処理する
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_help(int ac, char **av)
{
    for (int i = 0; i < USBCommandEntryCount; i++)
    {
        const struct cmd_entry *pentry = &(USBCommandEntries[i]);
        if ((pentry->cmd != NULL) && (pentry->desc != NULL))
        {
            printf ("usb %s - %s\n", pentry->cmd, pentry->desc);
        }
    }

    return;
}
/**
 * @brief usb bus-state コマンドを処理する
 */
static void cmd_usb_bus_state(int ac, char **av)
{
    printf ("VBUS=%s ", USB0.DVSTCTR0.BIT.VBUSEN ? "ON" : "OFF");

    uint8_t line_state = USB0.SYSSTS0.BIT.LNST;
    switch (USB0.DVSTCTR0.BIT.RHST)
    {
        case 0: // Unstable.
        {
            printf ("Bus:Unstable %d ", line_state);
            break;
        }
        case 1: // Low speed
        {
            printf ("Bus:Low-Speed(1.5Mbps) %d ", line_state);
            if (line_state == 0)
            {
                printf ("SE0 ");
            }
            else if (line_state == 1)
            {
                printf ("K-State ");
            }
            else if (line_state == 2)
            {
                printf ("J-State ");
            }
            else
            {
                printf ("SE1 ");
            }
            break;
        }
        case 2: // Full speed
        {
            printf ("Bus:Full-Speed(12Mbps) ");
            if (line_state == 0)
            {
                printf ("SE0 ");
            }
            else if (line_state == 1)
            {
                printf ("J-State ");
            }
            else if (line_state == 2)
            {
                printf ("K-State ");
            }
            else
            {
                printf ("SE1 ");
            }
            break;
        }
        default:
        {
            printf ("Bus:Reseting %d ", line_state);
            break;
        }
    }
    printf ("\n");
    return;
}

/**
 * @brief usb int-state コマンドを処理する。
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_int_state(int ac, char **av) {
    uint16_t intstat0 = USB0.INTSTS0.WORD;
    uint16_t intstat1 = USB0.INTSTS1.WORD;

    printf("INTSTS0=0x%04x INTSTS1=0x%04x\n", intstat0, intstat1);

    return ;
}

/**
 * @brief usb list コマンドを処理する。
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_list(int ac, char **av) {
    for (uint8_t dev_addr = 0; dev_addr < 0xFFu; dev_addr++) {
        uint16_t vid;
        uint16_t pid;
        if (tuh_vid_pid_get(dev_addr, &vid, &pid)) {
            printf("DEV_ADDR=%d VID=%04xh PID=%04xh\n", dev_addr, vid, pid);
        }
    }
    return ;
}


