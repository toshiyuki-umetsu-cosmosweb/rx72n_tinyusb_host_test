/**
 * @file usbコマンドインタフェース
 * @author Cosmosweb Co.,Ltd. 2024
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <platform.h>

#include <host/usbh.h>

#include "command_table.h"
#include "command_usb.h"

static void cmd_usb_help(int ac, char **av);
static void cmd_usb_bus_state(int ac, char **av);
static void cmd_usb_int_state(int ac, char **av);
static void cmd_usb_list(int ac, char **av);
static void cmd_usb_lang_id(int ac, char **av);
static void cmd_usb_debug(int ac, char **av);
static void cmd_usb_get_desc_device(int ac, char **av);
static void cmd_usb_get_desc_config(int ac, char **av);
static void cmd_usb_get_desc_string(int ac, char **av);
static void cmd_usb_config(int ac, char **av);
static void cmd_usb_interface(int ac, char **av);
static int get_interface_list(uint8_t daddr, uint8_t config_num, tusb_desc_interface_t *list, int max_interface);
static void cmd_usb_info(int ac, char **av);
static void print_string_descriptor(const tusb_desc_string_t *pdesc);
static int utf16_to_utf32(const uint16_t *p, uint32_t *pcode_point);
static int utf32_to_utf8(uint32_t code_point, uint8_t *pbuf);
static void print_device_descriptor(const tusb_desc_device_t *pdesc);
static void print_configuration_descriptor(const tusb_desc_configuration_t *pdesc);
static void print_interface_descriptor(const tusb_desc_interface_t *pdesc);
static void print_endpoint_descriptor(const tusb_desc_endpoint_t *pdesc);
static void print_binary_array(const uint8_t *buf, uint16_t len);

/**
 * @brief USBコマンドテーブル
 */
//@formatter:off
static const struct cmd_entry USBCommandEntries[] = {
    { "list", "Print USB devices.", cmd_usb_list },
    { "info", "Print USB device information.", cmd_usb_info },
    { "config", "Set/get USB device configuration.", cmd_usb_config },
    { "interface", "Set/get USB device interface.", cmd_usb_interface },
    { "bus-state", "Print USB bus state.", cmd_usb_bus_state },
    { "int-state", "Print USB int state.", cmd_usb_int_state },
    { "get-desc-device", "Get DEVICE descriptor.", cmd_usb_get_desc_device },
    { "get-desc-config", "Get CONFIGURATION descriptor.", cmd_usb_get_desc_config },
    { "get-desc-string", "Get STRING descriptor.", cmd_usb_get_desc_string },
    { "lang-id", "Set/get default language Id.", cmd_usb_lang_id },
    { "debug", "Set/get debug mode.", cmd_usb_debug },
    { "help", "Print USB subcommand list.", cmd_usb_help }
};
//@formatter:on
/**
 * @brief USBコマンドエントリ数
 */
static const int USBCommandEntryCount = (int) (sizeof(USBCommandEntries) / sizeof(struct cmd_entry));

/**
 * @brief I/O バッファ(大きいのでスタックに割り付け)
 */
static union
{
    tusb_desc_device_t device_desc; // Device descriptor.
    tusb_desc_configuration_t config_desc; // Configuration descriptor.
    tusb_desc_string_t string_desc; // String descriptor.
    uint8_t ptr[1024];
} IoBuf;

/**
 * Language ID (省略時デフォルト)
 */
static uint16_t DefaultLangId = 0x0409;
/**
 * デバッグモードかどうか
 */
static bool IsDebug = false;

/**
 * 一時取得用インタフェースリスト。
 * データサイズが大きいため、スタックでなく静的領域に割り当てる。
 */
static tusb_desc_interface_t Interfaces[16];

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
static void cmd_usb_int_state(int ac, char **av)
{
    uint16_t intstat0 = USB0.INTSTS0.WORD;
    uint16_t intstat1 = USB0.INTSTS1.WORD;

    printf ("INTSTS0=0x%04x INTSTS1=0x%04x\n", intstat0, intstat1);

    return;
}

/**
 * @brief usb list コマンドを処理する。
 *
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_list(int ac, char **av)
{
    for (int dev_addr = 0; dev_addr <= 0xFF; dev_addr++)
    {
        uint16_t vid;
        uint16_t pid;
        if (tuh_vid_pid_get ((uint8_t)(dev_addr), &vid, &pid))
        {
            printf ("DEV_ADDR=%d VID=%04xh PID=%04xh\n", dev_addr, vid, pid);
        }
    }
    return;
}
/**
 * @brief usb lang-id コマンドを処理する
 *        usb lang-id [id#]
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_lang_id(int ac, char **av)
{
    if (ac >= 3)
    {
        int id = strtol(av[2], NULL, 0);
        if ((id < 0) || (id > 0xFFFF))
        {
            printf("Invalid language Id.\n");
            return ;
        }
        DefaultLangId = id;
    }
    printf("%04xh\n", DefaultLangId);
    return ;
}
/**
 * @brief usb debug コマンドを処理する
 *        usb debug [mode#]
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_debug(int ac, char **av)
{
    if (ac >= 3)
    {
        IsDebug = (strtol(av[2], NULL, 0) != 0);
    }
    printf("%d\n", (IsDebug ? 1 : 0));
    return ;
}


/**
 * @brief usb get-desc コマンドを処理する。
 *        usb get-desc-device daddr#
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_get_desc_device(int ac, char **av)
{
    if (ac >= 3)
    {
        int daddr = strtol (av[2], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf ("Invalid device addr. %d\n", daddr);
            return;
        }

        uint16_t len = (uint16_t) (sizeof(tusb_desc_device_t));
        memset (IoBuf.ptr, 0, len);
        if (tuh_descriptor_get_device_sync ((uint8_t) (daddr), IoBuf.ptr, len) != XFER_RESULT_SUCCESS)
        {
            printf ("Could not get descriptor. \n");
            return;
        }
        print_device_descriptor (&IoBuf.device_desc);
        if (IsDebug)
        {
            print_binary_array (IoBuf.ptr, len);
        }
    }
    else
    {
        printf ("usage:\n");
        printf ("  usb get-desc-device device-addr#\n");
    }

    return;
}

/**
 * @brief usb get-desc-config コマンドを処理する
 *        usb get-desc-config daddr# config#
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_get_desc_config(int ac, char **av)
{
    if (ac >= 4)
    {
        int daddr = strtol (av[2], NULL, 0);
        int config_num = strtol (av[3], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf ("Invalid device addr. %d\n", daddr);
            return;
        }

        if (tuh_descriptor_get_configuration_sync((uint8_t)(daddr), (uint8_t)(config_num),
                                                  IoBuf.ptr, sizeof(tusb_desc_configuration_t)) != XFER_RESULT_SUCCESS)
        {
            printf("Bus request failure.\n");
            return ;
        }
        uint16_t total_len = IoBuf.config_desc.wTotalLength;
        if (total_len > sizeof(IoBuf))
        {
            printf("Too large descriptor to receive. (>%d).\n", sizeof(IoBuf));
            return ;
        }

        if (tuh_descriptor_get_configuration_sync((uint8_t)(daddr), (uint8_t)(config_num),
                                                  IoBuf.ptr, total_len) != XFER_RESULT_SUCCESS)
        {
            printf("Bus request failure.\n");
            return ;
        }

        uint16_t offset = 0;
        {
            const tusb_desc_configuration_t *pdesc = (const tusb_desc_configuration_t*)(&(IoBuf.ptr[offset]));
            print_configuration_descriptor(pdesc);
            if (IsDebug)
            {
                print_binary_array(&(IoBuf.ptr[offset]), pdesc->bLength);
            }

            offset += pdesc->bLength;
        }
        while (offset < total_len)
        {
            uint16_t desc_len = IoBuf.ptr[offset + 0];
            uint16_t desc_type = IoBuf.ptr[offset + 1];
            if ((offset + desc_len) > total_len)
            {
                // Incorrect.
                printf("-- Unknown descriptor-- (type=%02xh len=%d)\n", desc_type, desc_len);
                print_binary_array(&(IoBuf.ptr[offset]), (total_len - offset));
                printf("...broken end.\n");
                break;
            }

            if (desc_type == 4)
            {
                const tusb_desc_interface_t* pdesc = (const tusb_desc_interface_t*)(&(IoBuf.ptr[offset]));
                printf("-- Interface %d (alt=%d)--\n", pdesc->bInterfaceNumber, pdesc->bAlternateSetting);
                print_interface_descriptor(pdesc);
                if (IsDebug)
                {
                    print_binary_array(&(IoBuf.ptr[offset]), desc_len);
                }
            }
            else if (desc_type == 5)
            {
                const tusb_desc_endpoint_t *pdesc = (const tusb_desc_endpoint_t *)(&(IoBuf.ptr[offset]));
                printf("--- Endpoint %d --\n", pdesc->bEndpointAddress & 0x0F);
                print_endpoint_descriptor(pdesc);
                if (IsDebug)
                {
                    print_binary_array(&(IoBuf.ptr[offset]), desc_len);
                }
            }
            else
            {
                printf("-- Unknown descriptor-- (type=%02xh len=%d)\n", desc_type, desc_len);
                print_binary_array(&(IoBuf.ptr[offset]), desc_len);
            }
            offset += desc_len;
        }

        return ;
    }
    else
    {
        printf("usage:\n");
        printf("  usb get-desc-config daddr# config#\n");
        return ;
    }
}

/**
 * @brief usb get-desc-string コマンドを処理する。
 *        usb get-desc-string daddr# index# lang-id#
 *        usb get-desc-string daddr#
 * @param ac 引数の数
 * @param av 引数
 */
static void cmd_usb_get_desc_string(int ac, char **av)
{
    if (ac >= 4)
    {
        int daddr = strtol (av[2], NULL, 0);
        int index = strtol (av[3], NULL, 0);
        int lang_id = (ac >= 5) ? strtol (av[4], NULL, 0) : DefaultLangId;
        if ((daddr < 0) || (daddr > 255))
        {
            printf ("Invalid device address.\n");
            return;
        }
        if ((index < 0) || (index > 255))
        {
            printf ("Invalid descriptor index.\n");
            return;
        }
        if ((lang_id < 1) || (lang_id > 0xFFFF)) {
            printf ("Invalid langage id.\n");
            return ;
        }

        memset(IoBuf.ptr, 0x0, 255);
        if (tuh_descriptor_get_string_sync (daddr, index, lang_id, IoBuf.ptr, 255) != XFER_RESULT_SUCCESS)
        {
            printf("Could not get descriptor.\n");
            return ;
        }

        print_string_descriptor(&(IoBuf.string_desc));
        putchar('\n');
        if (IsDebug)
        {
            print_binary_array(IoBuf.ptr, IoBuf.string_desc.bLength);
        }
    }
    else if (ac == 3)
    {
        // usb get-desc-string daddr#
        // Getting supported language ids.
        int daddr = strtol (av[2], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf ("Invalid device address.\n");
            return;
        }
        memset(IoBuf.ptr, 0x0, 255);
        if (tuh_descriptor_get_string_sync (daddr, 0, 0, IoBuf.ptr, 255) != XFER_RESULT_SUCCESS)
        {
            printf("Could not get descriptor.\n");
            return ;
        }

        if (IoBuf.string_desc.bLength > 0) {
            int lang_count = (IoBuf.string_desc.bLength - 2) / 2;
            printf("Supported language ids :\n");
            for (int i = 0; i < lang_count; i++) {
                printf("  %04xh\n", IoBuf.string_desc.unicode_string[i]);
            }
            if (IsDebug)
            {
                print_binary_array(IoBuf.ptr, IoBuf.string_desc.bLength);
            }
        }
        else
        {
            printf("No supported languages.\n");
        }
    }
    else
    {
        printf ("usage:\n");
        printf ("  usb get-desc-string daddr# index# [ lang-id# ]\n");
        printf ("  usb get-desc-string daddr#\n");
    }

    return;
}

/**
 * @brief usb config コマンドを処理する
 *        usb config daddr# config#
 *        usb config daddr#
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_config(int ac, char **av)
{
    if (ac >= 4)
    {
        int daddr = strtol(av[2], NULL, 0);
        int config_num = strtol(av[3], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf("Invalid device address.\n");
            return ;
        }
        if ((config_num < 0) || (config_num > 255))
        {
            printf("Invalid configuration number.\n");
            return ;
        }

        if (tuh_configuration_set_sync(daddr, (uint8_t)(config_num)) != XFER_RESULT_SUCCESS)
        {
            printf("Bus request failure.\n");
            return ;
        }
        uint8_t actual_config_num;
        if (tuh_configuration_get_sync(daddr, &actual_config_num) != XFER_RESULT_SUCCESS)
        {
            return ;
        }
        printf("Set configuration to %d. (Request=%d)\n", actual_config_num, config_num);
        return ;
    }
    if (ac == 3)
    {
        int daddr = strtol(av[2], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf("Invalid device address.\n");
            return ;
        }

        uint8_t config_num;
        if (tuh_configuration_get_sync((uint8_t)(daddr), &config_num) != XFER_RESULT_SUCCESS)
        {
            printf("Bus request failure.\n");
            return ;
        }

        printf("%d\n", config_num);
        return ;
    }
    else
    {
        printf("usage:\n");
        printf("  usb config daddr# config#\n");
        printf("  usb config daddr#\n");
        return ;
    }
}

/**
 * @brief usb interface コマンドを処理する
 *        usb interface daddr# iface# alt#
 *        usb interface daddr# iface#
 *        usb interface daddr#
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_interface(int ac, char **av)
{
    if (ac >= 5)
    {
        int daddr = strtol(av[2], NULL, 0);
        int iface_num = strtol(av[3], NULL, 0);
        int iface_alt = strtol(av[4], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf("Invalid device address.\n");
            return ;
        }
        if ((iface_num < 0) || (iface_num > 255))
        {
            printf("Invalid interface number.\n");
            return ;
        }
        if ((iface_alt < 0) || (iface_alt > 255))
        {
            printf("Invalid interface alt number.\n");
            return ;
        }

        // このインタフェースのALT設定をする
        if (tuh_interface_set_sync(daddr, iface_num, iface_alt) != XFER_RESULT_SUCCESS)
        {
            printf("Set interface request failure.\n");
            return ;
        }

        uint8_t actual_iface_alt;
        if (tuh_interface_get_sync(daddr, iface_num, &actual_iface_alt) != XFER_RESULT_SUCCESS)
        {
            printf("Get interface request failure.\n");
            return ;
        }

        printf("Set interface#%d to %d (requested=%d)\n", iface_num, actual_iface_alt, iface_alt);
    }
    else if (ac == 4)
    {
        int daddr = strtol(av[2], NULL, 0);
        int iface_num = strtol(av[3], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf("Invalid device address.\n");
            return ;
        }
        if ((iface_num < 0) || (iface_num > 255))
        {
            printf("Invalid interface number.\n");
            return ;
        }

        // このUSBデバイスのインタフェースのALT設定を取得して出力する。
        uint8_t iface_alt;
        if (tuh_interface_get_sync(daddr, iface_num, &iface_alt) != XFER_RESULT_SUCCESS)
        {
            printf("Get interface request failure.\n");
            return ;
        }
        printf("Interface#%d Alt=%d\n", iface_num, iface_alt);
    }
    else if (ac == 3)
    {
        int daddr = strtol(av[2], NULL, 0);
        if ((daddr < 0) || (daddr > 255))
        {
            printf("Invalid device address.\n");
            return ;
        }

        // TODO : このUSBデバイスのインタフェース一覧を取得し、それぞれのALT設定を取得して出力する。
        uint8_t config_num;
        if (tuh_configuration_get_sync(daddr, &config_num) != XFER_RESULT_SUCCESS)
        {
            printf("Could not current configuration.\n");
            return ;
        }
        if (config_num == 0)
        {
            printf("Specified USB device not configured. (config=%d)\n", config_num);
            return ;
        }

        int iface_count = get_interface_list(daddr, config_num, Interfaces, sizeof(Interfaces) / sizeof(tusb_desc_interface_t));
        if (iface_count < 0)
        {
            printf("Getting interface list failure.\n");
            return ;
        }
        if (iface_count == 0)
        {
            printf("No interfaces.\n");
            return ;
        }
        for (int i = 0; i < iface_count; i++)
        {
            const tusb_desc_interface_t *pdesc = &(Interfaces[i]);
            bool is_selected = false;
            uint8_t iface_alt;
            if (tuh_interface_get_sync(daddr, pdesc->bInterfaceNumber, &iface_alt) == XFER_RESULT_SUCCESS)
            {
                is_selected = (iface_alt == pdesc->bAlternateSetting);
            }
            printf("%cInterface#%d Alt%d Class=%02xh SubClass=%02xh Protocol=%02xh InterfaceDesc=%d\n",
                    (is_selected ? '*' : ' '), pdesc->bInterfaceNumber, pdesc->bAlternateSetting,
                    pdesc->bInterfaceClass, pdesc->bInterfaceSubClass, pdesc->bInterfaceProtocol,
                    pdesc->iInterface);
        }
    }
    else
    {
        printf("usage:\n");
        printf("  usb interface daddr# iface# alt#\n");
        printf("  usb interface daddr# iface#\n");
        printf("  usb interface daddr# \n");
    }


    return ;
}

/**
 * @brief 指定されたUSBデバイスのインタフェース一覧を取得する。
 *        この関数は IoBuf を使用する。呼び出し元で IoBuf を使用中は使えない。
 * @param daddr デバイスアドレス
 * @param list 取得したインタフェース情報を格納するリスト。tusb_desc_interface_tの配列
 * @param max_interface listの数
 * @return 成功した場合、取得したインタフェースの数を返す。エラーが発生した場合、-1を返す。
 */
static int get_interface_list(uint8_t daddr, uint8_t config_num, tusb_desc_interface_t *list, int max_interface)
{
    if (tuh_descriptor_get_configuration_sync(daddr, config_num, IoBuf.ptr, sizeof(tusb_desc_configuration_t)) != XFER_RESULT_SUCCESS)
    {
        return -1;
    }
    uint16_t total_len = IoBuf.config_desc.wTotalLength;
    if (total_len > sizeof(IoBuf))
    {
        return -1;
    }

    if (tuh_descriptor_get_configuration_sync(daddr, config_num, IoBuf.ptr, total_len) != XFER_RESULT_SUCCESS)
    {
        return -1;
    }

    int iface_count = 0;
    uint16_t offset = 0;
    {
        // Configuration descriptor.
        const tusb_desc_configuration_t *pdesc = (const tusb_desc_configuration_t*)(&(IoBuf.ptr[offset]));
        offset += pdesc->bLength;
    }
    while (offset < total_len)
    {
        uint16_t desc_len = IoBuf.ptr[offset + 0];
        uint16_t desc_type = IoBuf.ptr[offset + 1];
        if ((offset + desc_len) > total_len)
        {
            // Incorrect.
            return -1;
        }

        if (desc_type == 4)
        {
            const tusb_desc_interface_t* pdesc = (const tusb_desc_interface_t*)(&(IoBuf.ptr[offset]));

            if (iface_count < max_interface)
            {
                memcpy(&(list[iface_count]), pdesc, sizeof(tusb_desc_interface_t));
                iface_count++;
            }
            else
            {
                // 空きなし
                return -1;
            }
        }
        else
        {
            // do nothing.
        }
        offset += desc_len;
    }

    return iface_count;
}




/**
 * @brief usb info コマンドを処理する。
 *        usb info daddr#
 * @param ac 引数の数
 * @param av 引数配列
 */
static void cmd_usb_info(int ac, char **av)
{
    if (ac < 3)
    {
        printf("usage:\n");
        printf("  usb info daddr#\n");
        return ;
    }

    int daddr = strtol(av[2], NULL, 0);
    if ((daddr < 0) || (daddr > 255))
    {
        printf("Invalid device address.\n");
        return ;
    }

    tusb_desc_device_t device_desc;

    if (tuh_descriptor_get_device_sync((uint8_t)(daddr), &device_desc, sizeof(device_desc)) != XFER_RESULT_SUCCESS)
    {
        printf("Could not get device descriptor.\n");
        return ;
    }
    printf("DeviceAddr = %xh\n", daddr);
    printf("USBVersion = %xh\n", device_desc.bcdUSB);
    printf("VendorId = %04xh\n", device_desc.idVendor);
    printf("ProductId = %04xh\n", device_desc.idProduct);
    printf("Device : Class=%xh SubClass=%xh Protocol=%xh\n",
           device_desc.bDeviceClass, device_desc.bDeviceSubClass, device_desc.bDeviceProtocol);
    printf("          See https://www.usb.org/defined-class-codes \n");
    uint8_t config_num;
    if (tuh_configuration_get_sync((uint8_t)(daddr), &config_num) == XFER_RESULT_SUCCESS)
    {
        printf("Configuration = %d/%d\n", config_num, device_desc.bNumConfigurations);
    }


}





/**
 * @brief String Descriptorを出力する
 * @param pdesc String Descriptorデータ
 */
static void print_string_descriptor(const tusb_desc_string_t *pdesc)
{
    const uint16_t *str = pdesc->unicode_string;
    int length = (pdesc->bLength - 2) / 2;

    int i = 0;
    while (i < length)
    {
        uint32_t code_point;
        int d = utf16_to_utf32(&(str[i]), &code_point);
        if (d > 0) // コードポイント取得成功?
        {
            uint8_t buf[4u];
            int n = utf32_to_utf8(code_point, buf);
            if (n > 0)
            {
                for (int j = 0; j < n; j++)
                {
                    putchar(buf[j]);
                }
            }
            i += d;
        }
        else
        {
            i++;
        }
    }

    return ;
}

/**
 * @brief UTF-16 から UTF-32 に1文字分変換する
 * @param p UTF-16データのアドレス
 * @param pcode_point コードポイントを格納する変数
 * @return 使用したUTF-16のデータ数を返す。不正な文字列だった場合には-1を返す。
 */
static int utf16_to_utf32(const uint16_t *p, uint32_t *pcode_point)
{
    uint32_t code_point;
    uint16_t c = p[0];
    int retval;

    if ((c >= 0xD800u) && (c <= 0xDBFFu)) // サロゲートペアの先頭バイト？
    {
        uint16_t cl = *p;
        if ((cl >= 0xDC00u) && (cl <= 0xDFFFu)) // サロゲートペアの次バイト？
        {
            code_point = ((uint32_t)(c - 0xD800u) << 10u) + (uint32_t)(cl - 0xDC00u) + 0x00010000uL;
            retval = 2;
        }
        else
        {
            code_point = 0x00; // 不正
            retval = -1;
        }
    }
    else if ((c >= 0xDC00u) && (c <= 0xDFFFu))  // サロゲートペアの次バイト？
    {
        code_point = 0x00; // 不正
        retval = -1;
    }
    else
    {
        code_point = c;
        retval = 1;
    }
    (*pcode_point) = code_point;

    return retval;
}

/**
 * @brief UTF-32をUTF-8に1文字変換する。
 *        終端コードを入れたりはしないので注意。
 * @param code_point コードポイント
 * @param pbuf UTF-8を格納するバッファ(4バイト以上のサイズを用意すること)
 * @return pbufに書き込んだバイト数。code_pointがUTF-8で表現できない値の場合には-1を返す。
 */
static int utf32_to_utf8(uint32_t code_point, uint8_t *pbuf)
{
    if (code_point < 0x00000080uL) // 7bit
    {
        pbuf[0] = (uint8_t)(code_point & 0xFFu);
        return 1;
    }
    else if (code_point <= 0x00000800uL) // 11bit
    {
        pbuf[0] = (uint8_t)((code_point >> 6u) & 0x1Fu) | 0xC0u; // 0b110xxxxx (bit11-7)
        pbuf[1] = (uint8_t)(code_point & 0x3Fu) | 0x80u; // 0b10xxxxxx (bit6-0)
        return 2;
    }
    else if (code_point <= 0x00010000uL) // 1
    {
        pbuf[0] = (uint8_t)((code_point >> 12u) & 0x0Fu) | 0xE0u; // 0b1110xxxx (bit16-13)
        pbuf[1] = (uint8_t)((code_point >> 6u) & 0x3Fu) | 0x80u; // 0b10xxxxxx  (bit-12-7)
        pbuf[2] = (uint8_t)(code_point & 0x3Fu) | 0x80u; // 0b10xxxxxx (bit6-0)
        return 3;
    }
    else if (code_point <= 0x0010FFFFuL)
    {
        pbuf[0] = (uint8_t)((code_point >> 18u) & 0x0Fu) | 0xF0u; // 0b11110xxx (bit21-19)
        pbuf[1] = (uint8_t)((code_point >> 12u) & 0x3Fu) | 0x80u; // 0b10xxxxxx (bit18-13)
        pbuf[2] = (uint8_t)((code_point >> 6u) & 0x3Fu) | 0x80u; // 0b10xxxxxx (bit-12-7)
        pbuf[3] = (uint8_t)(code_point & 0x3Fu) | 0x80u; // 0b10xxxxxx (bit6-0)
        return 4;
    }
    else
    {
        return -1;
    }
}



/**
 * @brief Device Descriptorを出力する
 * @param pdesc Device Descriptorデータ
 */
static void print_device_descriptor(const tusb_desc_device_t *pdesc)
{
    printf("bLength = %u\n", pdesc->bLength);
    printf("bDescriptorType = %u\n", pdesc->bDescriptorType);
    printf("bcdUSB = %04xh\n", pdesc->bcdUSB);
    printf("bDeviceClass = %d\n", pdesc->bDeviceClass);
    printf("bDeviceSubClass = %d\n", pdesc->bDeviceSubClass);
    printf("bDeviceProtocol = %d\n", pdesc->bDeviceProtocol);
    printf("bMaxPackSize0 = %d\n", pdesc->bMaxPacketSize0);
    printf("idVendor = %04xh\n", pdesc->idVendor);
    printf("idProduct = %04xh\n", pdesc->idProduct);
    printf("bcdDevice = %04xh\n", pdesc->bcdDevice);
    printf("iManufacturer = %d (desc#)\n", pdesc->iManufacturer);
    printf("iProduct = %d (desc#)\n", pdesc->iProduct);
    printf("iSerialnumber = %d (desc#)\n", pdesc->iSerialNumber);
    printf("bNumConfigurations = %d\n", pdesc->bNumConfigurations);

    return ;
}

/**
 * @brief Configuration Descriptorを出力する。
 *        USB規格では、GET_CONFIGURATIONリクエストを発行すると、
 *        Configuration Descriptorに続けていろんなディスクリプタを受け取る。（合計wTotalLengthバイト)
 *        本関数では純粋にConfiguration Descriptorのフィールドだけを出力する。
 * @param pdesc Configuration Descriptor
 */
static void print_configuration_descriptor(const tusb_desc_configuration_t *pdesc)
{
    printf("bLength = %u\n", pdesc->bLength);
    printf("bDescriptorType = %u\n", pdesc->bDescriptorType);
    printf("wTotalLength = %d\n", pdesc->wTotalLength);
    printf("bNumInterfaces = %d\n", pdesc->bNumInterfaces);
    printf("bConfigurationValue = %d\n", pdesc->bConfigurationValue);
    printf("iConfiguration = %d (desc#)\n", pdesc->iConfiguration);
    printf("bmAttributes = %02xh ", pdesc->bmAttributes);
    if (pdesc->bmAttributes & 0x40u)
    {
        printf("[Self-powered] ");
    }
    if (pdesc->bmAttributes & 0x20u)
    {
        printf("[Remote wakeup] ");
    }
    printf("\n");
    printf("bMaxPower = %d (%dmA)\n", pdesc->bMaxPower, (int)(pdesc->bMaxPower) * 2);

    return ;
}
/**
 * @brief Interface Descriptorを出力する。
 * @param pdesc Interface Descriptor.
 */
static void print_interface_descriptor(const tusb_desc_interface_t *pdesc)
{
    printf("bLength = %d\n", pdesc->bLength);
    printf("bDescriptorType = %d\n", pdesc->bDescriptorType);
    printf("bInterfaceNumber = %d\n", pdesc->bInterfaceNumber);
    printf("bAlternateSetting = %d\n", pdesc->bAlternateSetting);
    printf("bNumEndpoints = %d\n", pdesc->bNumEndpoints);
    printf("bInterfaceClass = %d\n", pdesc->bInterfaceClass);
    printf("bInterfaceSubClass = %d\n", pdesc->bInterfaceSubClass);
    printf("bInterfaceProtocol = %d\n", pdesc->bInterfaceProtocol);
    printf("iInterface = %d (desc#)\n", pdesc->iInterface);

    return ;
}

/**
 * @brief Endpoint Descriptorを出力する。
 * @param pdesc Endpoint Descriptor.
 */
static void print_endpoint_descriptor(const tusb_desc_endpoint_t *pdesc)
{
    printf("bLength = %d\n", pdesc->bLength);
    printf("bDescriptorType = %d\n", pdesc->bDescriptorType);
    uint8_t ep_addr = pdesc->bEndpointAddress;
    printf("bEndpointAddress = %02xh No=%d(%s)\n", ep_addr, (ep_addr & 0x0F),
            ((ep_addr & 0x80) ? "In" : "Out"));
    printf("bmAttributes = %02xh ", *((uint8_t*)(&(pdesc->bmAttributes))));
    switch (pdesc->bmAttributes.xfer)
    {
        case 3: // Interrupt.
        {
            uint8_t usage = pdesc->bmAttributes.usage;
            static const char *usage_text[4] = { "Periodic", "Notification", "Reserved", "Reserved" };
            printf("Interrupt, %s", usage_text[usage]);
            break;
        }
        case 2: // Bulk
        {
            printf("Bulk\n");
            break;
        }
        case 1: // Isochronous
        {
            uint8_t sync_type = pdesc->bmAttributes.sync;
            uint8_t usage = pdesc->bmAttributes.usage;
            static const char *sync_text[4] = { "No sSynchronization", "Asynchronous", "Adaptive", "Synchronous" };
            static const char *usage_text[4] = { "Data endpoint", "Feedback endpoint", "Implicit feedback Data endpoint", "Reserved" };
            printf("Isochronous, %s, %s\n", sync_text[sync_type], usage_text[usage]);
            break;
        }
        case 0: // Control
        default:
        {
            printf("Control\n");
            break;
        }
    }
    printf("wMaxPacketSize = %d\n", pdesc->wMaxPacketSize);
    printf("bInterval = %d\n", pdesc->bInterval);

    return ;
}


/**
 * @brief バイナリ配列を出力する。
 * @param buf バッファアドレス
 * @param len 出力する長さ
 */
static void print_binary_array(const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    while (i < len)
    {
        if ((i & 0x0F) == 0x00)
        {
            printf ("%02x: %02x ", i, buf[i]);
        }
        else if ((i & 0x0F) == 0x0F)
        {
            printf ("%02x\n", buf[i]);
        }
        else if ((i & 0x0F) == 0x7)
        {
            printf ("%02x  ", buf[i]);
        }
        else
        {
            printf ("%02x ", buf[i]);
        }
        i++;
    }
    if ((i & 0x0F) != 0)
    {
        printf ("\n");
    }

    return;
}

