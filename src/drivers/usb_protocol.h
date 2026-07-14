#ifndef DRIVERS_USB_PROTOCOL_H
#define DRIVERS_USB_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

enum up_setup_type_e: uint8_t
{
    UP_SETUP_TYPE_STANDARD = 0,
    UP_SETUP_TYPE_CLASS = 1,
    UP_SETUP_TYPE_VENDOR = 2
};

enum up_setup_recipient_e: uint8_t
{
    UP_SETUP_RECIPIENT_DEVICE = 0,
    UP_SETUP_RECIPIENT_INTERFACE = 1,
    UP_SETUP_RECIPIENT_ENDPOINT = 2,
    UP_SETUP_RECIPIENT_OTHER = 3
};

// USB 2.0 Table 9-4
enum up_setup_request_e: uint8_t
{
    UP_SETUP_REQ_GET_STATUS = 0x0,
    UP_SETUP_REQ_CLEAR_FEATURE = 0x1,
    UP_SETUP_REQ_SET_FEATURE = 0x3,
    UP_SETUP_REQ_SET_ADDRESS = 0x5,
    UP_SETUP_REQ_GET_DESCRIPTOR = 0x6,
    UP_SETUP_REQ_SET_DESCRIPTOR = 0x7,
    UP_SETUP_REQ_GET_CONFIGURATION= 0x8,
    UP_SETUP_REQ_SET_CONFIGURATION = 0x9,
    UP_SETUP_REQ_GET_INTERFACE = 0xa,
    UP_SETUP_REQ_SET_INTERFACE = 0xb,
    UP_SETUP_REQ_SYNCH_FRAME = 0xc
};

// USB 2.0 Table 9-5
enum up_descriptor_type_e: uint8_t
{
    UP_DESCRIPTOR_TYPE_DEVICE = 1,
    UP_DESCRIPTOR_TYPE_CONFIGURATION = 2,
    UP_DESCRIPTOR_TYPE_STRING = 3
};

// USB 2.0 Table 9-6
enum up_feature_e: uint16_t
{
    UP_FEATURE_ENDPOINT_HALT = 0,
    UP_FEATURE_DEVICE_REMOTE_WAKEUP = 1,
    UP_FEATURE_TEST_MODE = 2
};

struct __attribute__((packed, aligned(8))) up_setup_t
{
    enum up_setup_recipient_e recipient: 5;
    enum up_setup_type_e type: 2;
    bool is_get: 1;
    uint8_t request; // Cannot be a single enum, since the enum would depend on
                     // type + recipient + index_offset
    uint16_t value;
    uint16_t index_offset;
    uint16_t length;
};
static_assert(sizeof(struct up_setup_t) == 8);

bool up_verify_setup(const struct up_setup_t *setup);
bool up_is_standard(const struct up_setup_t *setup);

#endif // DRIVERS_USB_PROTOCOL_H
