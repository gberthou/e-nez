#include "usb_protocol.h"

// https://www.beyondlogic.org/usbnutshell/usb6.shtml
bool up_verify_setup(const struct up_setup_t *setup)
{
    // Reject reserved recipients and types
    if (setup->recipient > UP_SETUP_RECIPIENT_OTHER)
        return false;
    if (setup->type > UP_SETUP_TYPE_VENDOR)
        return false;
    return true;
}

bool up_is_standard(const struct up_setup_t *setup)
{
    // Reject non-standard requests
    if (setup->type != UP_SETUP_TYPE_STANDARD)
        return false;

    // Reject reserved requests
    auto const request = setup->request;
    if (request > UP_SETUP_REQ_SYNCH_FRAME || request == 2 || request == 4)
        return false;

    return true;
}
