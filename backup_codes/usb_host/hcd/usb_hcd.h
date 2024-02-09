/**
 * @file USB HCD (Host Controller Driver)
 * @author Cosmosweb Co.,Ltd. 2024
 */

#ifndef USB_HOST_HCD_USB_HCD_H_
#define USB_HOST_HCD_USB_HCD_H_

#include <stdbool.h>

void usb_hcd_init(void);
void usb_hcd_fini(void);

void usb_hcd_set_vbus(bool is_on);
bool usb_hcd_is_vbus(void);



#endif /* USB_HOST_HCD_USB_HCD_H_ */
