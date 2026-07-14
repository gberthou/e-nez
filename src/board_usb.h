#ifndef BOARD_USB_H
#define BOARD_USB_H

#include <stdbool.h>

void board_usb_init_ep0(void);
bool board_usb_cdc_acm_is_active(void);

// Assumes board_usb_cdc_acm_is_active() == true.
// Warning: blocking call when it's the call that triggers the flush condition. This
// function may only be called by the main thread. The main thread does not do anything
// useful since the firmware/application is interrupt-based. So it's acceptable to block
// for a few ms here.
void board_usb_cdc_acm_putc(char c);

#endif // BOARD_USB_H
