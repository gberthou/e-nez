#include "usb_cdc_acm.h"

bool usb_is_cdc_acm_request(const struct up_setup_t *setup)
{
    // Recipient check is redundant if the caller already checked it
    if (setup->recipient != UP_SETUP_RECIPIENT_INTERFACE
    || setup->type != UP_SETUP_TYPE_CLASS)
        return false;

    return setup->request >= UP_CDC_ACM_REQ_SET_LINE_CODING
        && setup->request <= UP_CDC_ACM_REQ_SET_CONTROL_LINE_STATE;
}
