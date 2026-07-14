#ifndef DRIVERS_USB_CDC_ACM_H
#define DRIVERS_USB_CDC_ACM_H

#include <stdbool.h>
#include <stdint.h>

#include "usb_protocol.h"

enum usb_cdc_acm_management_request_e: uint16_t
{
    UP_CDC_ACM_REQ_SET_LINE_CODING = 0x20,
    UP_CDC_ACM_REQ_GET_LINE_CODING = 0x21,
    UP_CDC_ACM_REQ_SET_CONTROL_LINE_STATE = 0x22
};

bool usb_is_cdc_acm_request(const struct up_setup_t *setup);

#endif // DRIVERS_USB_CDC_ACM_H
