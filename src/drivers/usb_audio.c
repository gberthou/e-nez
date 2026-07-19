#include "usb_audio.h"

bool usb_is_audio_request(const struct up_setup_t *setup)
{
    // Recipient check is redundant if the caller already checked it
    if (setup->recipient != UP_SETUP_RECIPIENT_INTERFACE
    || setup->type != UP_SETUP_TYPE_CLASS)
        return false;

    return setup->request == AUDIO_REQ_CUR
        || setup->request == AUDIO_REQ_RANGE;
}
