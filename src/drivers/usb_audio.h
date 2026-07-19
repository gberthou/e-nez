#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "usb_protocol.h"

enum usb_audio_request_e: uint8_t
{
    AUDIO_REQ_CUR = 1,
    AUDIO_REQ_RANGE = 2
};

// Audio 2.0 Table A-17
enum usb_audio_clock_source_control_e: uint8_t
{
    AUDIO_CS_SAM_FREQ_CONTROL = 0x1,
    AUDIO_CS_CLOCK_VALID_CONTROL = 0x2
};

// Audio 2.0 Table A-20
enum usb_audio_terminal_control_e: uint8_t
{
    AUDIO_TE_COPY_PROTECT_CONTROL = 0x1,
    AUDIO_TE_CONNECTOR_CONTROL = 0x2,
    AUDIO_TE_OVERLOAD_CONTROL = 0x3,
    AUDIO_TE_CLUSTER_CONTROL = 0x4,
    AUDIO_TE_UNDERFLOW_CONTROL = 0x5,
    AUDIO_TE_OVERFLOW_CONTROL = 0x6,
    AUDIO_TE_LATENCY_CONTROL = 0x7,
    AUDIO_TE_PHANTOM_POWER_CONTROL = 0x8
};

struct usb_audio_request_t
{
    uint8_t entity_id;
    enum usb_audio_request_e request_type;
    uint8_t requested_field; // Could be any enum value
};

bool usb_is_audio_request(const struct up_setup_t *setup);

#endif // DRIVERS_AUDIO_STREAM_H
