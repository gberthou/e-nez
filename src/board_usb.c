#include <string.h>

#include "board.h"
#include "board_config.h"
#include "board_usb.h"
#include "board_usb_descriptors.h"
#include "board_usb_endpoints.h"
#include "interrupt.h"
#include "drivers/nvic.h"
#include "drivers/spi.h"
#include "drivers/usb.h"
#include "drivers/usb_audio.h"
#include "drivers/usb_cdc_acm.h"
#include "drivers/usb_protocol.h"
#include "util/strformat.h"

void board_kprintformat(const char *s, ...);

struct __attribute__((packed, aligned(4))) usb_decision_t
{
    uint16_t size_to_send: 10; // Used if !expect_data_payload
    bool expect_data_payload: 1;
    bool has_failed;
    bool last_packet;
};
static_assert(sizeof(struct usb_decision_t) == sizeof(uint32_t));

struct upload_info_t
{
    const void *buffer;
    size_t n_remaining;
};

enum usb_device_control_state_e
{
    CONTROL_STATE_IDLE = 0,
    CONTROL_STATE_WAIT_TX_ACK = 1,
    CONTROL_STATE_WAIT_RX_ACK = 2,

    CONTROL_STATE_SET_xxx_TX_ACK = 3, // Same as CONTROL_STATE_WAIT_TX_ACK, but
                                      // TX size is 0 and there is no STATUS OUT
    CONTROL_STATE_SET_ADDRESS = 4,
    CONTROL_STATE_SET_CONFIGURATION = 5,
    CONTROL_STATE_CDC_ACM_SET_LINE_CODING = 6,
    CONTROL_STATE_AUDIO_SET_CS_SAM_FREQ = 7,
    CONTROL_STATE_SET_ENUM_BEGIN = CONTROL_STATE_SET_xxx_TX_ACK
};

// Defines
constexpr unsigned usb_endpoint_control = 0;
#ifdef HAS_CDC_ACM
constexpr unsigned usb_endpoint_cdc_notification = 1; // Configuration0.Interface0, EP1
#endif // HAS_CDC_ACM
constexpr unsigned usb_endpoint_cdc_data = 2; // Configuration0.Interface1, EP2
constexpr unsigned usb_endpoint_audio_stream = 3; // Configuration0.Interface3, EP3

#ifdef HAS_CDC_ACM
constexpr uint16_t usb_config0_cdc_acm_control_interface = 0;
constexpr uint16_t usb_config0_audio_control_interface = 2;
constexpr uint16_t usb_config0_audio_stream_interface = 3;
#else
constexpr uint16_t usb_config0_audio_control_interface = 0;
constexpr uint16_t usb_config0_audio_stream_interface = 1;
#endif // HAS_CDC_ACM

constexpr uint16_t usb_audio_stream_alt_play = 1;

// Cf. descriptor
constexpr uint8_t usb_audio_clock_source_id = 1;
constexpr uint8_t usb_audio_input_terminal_id = 2;
constexpr uint8_t usb_audio_output_terminal_id = 3;

constexpr unsigned nvic_usb = 8;
constexpr size_t no_config = 0xffffffff;

// Constants
// Audio 2.0 layout 3 format
static const uint8_t __attribute__((aligned(4))) usb_audio_clock_source_ranges[] = {
    0x01, 0x00, // 1 range
    AUDIO_SAMPLING_FREQUENCY_HZ & 0xff, (AUDIO_SAMPLING_FREQUENCY_HZ >> 8) & 0xff,
    AUDIO_SAMPLING_FREQUENCY_HZ >> 16, 0x00, // range 0 min
    AUDIO_SAMPLING_FREQUENCY_HZ & 0xff, (AUDIO_SAMPLING_FREQUENCY_HZ >> 8) & 0xff,
    AUDIO_SAMPLING_FREQUENCY_HZ >> 16, 0x00, // range 0 max = range 0 min
    0x00, 0x00, 0x00, 0x00, // range 0 step (mandatory for ranges of one element)
};
// Constant because the device only supports one
static const uint32_t usb_audio_clock_source_current = AUDIO_SAMPLING_FREQUENCY_HZ;

// Variables

static struct usb_device_endpoint_t endpoint_control;
static struct usb_device_endpoint_t endpoint_cdc_notif;
static struct usb_device_endpoint_t endpoint_cdc_data;
static struct usb_device_endpoint_t endpoint_audio_stream;
static struct usb_device_endpoint_t * const config0_endpoints[] = {
    &endpoint_cdc_notif,
    &endpoint_cdc_data,
    &endpoint_audio_stream
};

static bool is_suspended = false;
static uint8_t requested_address = 0;
static uint8_t current_address = 0;
static uint16_t requested_configuration = 0;
static auto current_config = no_config;
static enum usb_device_control_state_e state = CONTROL_STATE_IDLE;
static uint32_t nonconst_response_data;
static_assert(sizeof(nonconst_response_data) >= 2); // Largest response: GET_STATUS (2B)

// Default is 115200 baud, 1 stop bit, no parity, 8 data bits
static uint32_t cdc_acm_line_coding[2] = {0x0001c200, 0x00080000};
#ifdef HAS_CDC_ACM
static char __attribute__((aligned(4))) cdc_acm_buffer[config0_ep2_tx_size];
static size_t cdc_acm_current_tx_size = 0;
#endif // HAS_CDC_ACM
static bool cdc_acm_wait_for_tx_ack = false;
static size_t cdc_acm_tx_sof_timeout = 0; // To detect when to timeout a flush

static bool audio_streaming_on = false;

// Utils
#define MIN_UNSAFE(a, b) ((a) < (b) ? (a) : (b))

static void signal_and_hang(uint32_t signal)
{
    interrupt_deactivate();

    for (size_t i = 0; i < 3; ++i)
    {
        board_set_led(0, true);
        board_set_led(1, true);
        for (volatile size_t x = (1 << 20); x--;);
        board_set_led(0, false);
        board_set_led(1, false);
        for (volatile size_t x = (1 << 20); x--;);
    }
    board_set_led(0, (signal & 0x2u) != 0);
    board_set_led(1, (signal & 0x1u) != 0);
_hang:
    goto _hang;
}

static inline void signal_fail(void)
{
    signal_and_hang(3);
}

// Assumes 32b alignment and accesses, as specified in RM0490 Section 29.7
// As a result, TX payloads split over several packets must also have a size that
// is multiple of 4
static inline volatile void *vmemcpy(volatile void *dst_, const void *src_, size_t size)
{
    volatile uint32_t *dst = dst_;
    const uint32_t *src = src_;
    size = (size + 3) >> 2;
    while (size--)
        *dst++ = *src++;
    return dst_;
}

// Assumes 32b alignment and accesses, as specified in RM0490 Section 29.7
// As a result, TX payloads split over several packets must also have a size that
// is multiple of 4
static inline void *memcpyv(void *dst_, const volatile void *src_, size_t size)
{
    uint32_t *dst = dst_;
    const volatile uint32_t *src = src_;
    size = (size + 3) >> 2;
    while (size--)
        *dst++ = *src++;
    return dst_;
}

static void usb_prepare_packet(
    struct usb_device_endpoint_t *endpoint,
    const void *src_,
    size_t size,
    struct usb_decision_t *decision,
    struct upload_info_t *upload)
{
    const size_t size_to_send = MIN_UNSAFE(size, endpoint->tx_packet_size);
    decision->size_to_send = size_to_send;

    vmemcpy(endpoint->layout.tx_rx.tx_packet, src_, size_to_send);

    auto const remaining = size - size_to_send;
    if (upload != NULL)
        upload->n_remaining = remaining;
    if (remaining == 0)
        decision->last_packet = true;
    else
    {
        const uint8_t *src = src_;
        if (upload != NULL)
            upload->buffer = src + size_to_send;
        decision->last_packet = false;
    }
}

static void usb_send_packet(
    const struct usb_device_endpoint_t *endpoint,
    struct usb_decision_t decision,
    bool is_reply_to_get_command,
    bool preserve_rx_status)
{
    if (decision.has_failed || decision.expect_data_payload)
    {
        // Aside from a halted enpoint, only endpoint 0 can stall
        const bool must_stall = (decision.has_failed && endpoint->index == 0);
        usb_set_device_endpoint_response(endpoint,
            must_stall ? USB_EP_STATUSENC_STALL : USB_EP_STATUSENC_NAK,
            USB_EP_STATUSENC_VALID, false, preserve_rx_status);
    }
    else
    {
        // Aside from a halted enpoint, only endpoint 0 can stall
        const bool must_stall = (!decision.last_packet && endpoint->index == 0);
        usb_set_count_tx(endpoint->layout.tx_rx.tx_ctrl, decision.size_to_send);
        usb_set_device_endpoint_response(endpoint,
            USB_EP_STATUSENC_VALID,
            // RM0490 page 965: opposite direction is set to NAK
            must_stall ? USB_EP_STATUSENC_STALL : USB_EP_STATUSENC_NAK,
            is_reply_to_get_command && decision.last_packet, preserve_rx_status);
    }
}

// Returns next state
static inline enum usb_device_control_state_e handle_usb_setup(
    const struct up_setup_t *setup,
    struct usb_decision_t *decision,
    struct upload_info_t *upload)
{
    auto next_state = CONTROL_STATE_WAIT_TX_ACK;

    decision->has_failed = true;
    switch (setup->request)
    {
        // USB 2.0 Section 9.4.5
        case UP_SETUP_REQ_GET_STATUS:
        {
            nonconst_response_data = 0;
            switch (setup->recipient)
            {
                case UP_SETUP_RECIPIENT_DEVICE:
                    // Remote wakeup not supported; keep 0
                    break;

                case UP_SETUP_RECIPIENT_INTERFACE:
                {
                    auto const interface_index = setup->index_offset & 0xfu;
                    if (current_address == 0 && interface_index != 0)
                        goto _handle_usb_setup_error;
                    if (current_config != no_config
                    && interface_index > usb_config0_audio_stream_interface)
                    {
                        goto _handle_usb_setup_error;
                    }

                    // Interface response is RES0
                    break;
                }

                case UP_SETUP_RECIPIENT_ENDPOINT:
                {
                    auto const endpoint_index = setup->index_offset & 0xfu;
                    if (current_address == 0 && endpoint_index != 0)
                        goto _handle_usb_setup_error;
                    if (current_config != no_config)
                    {
                        if (endpoint_index > usb_endpoint_audio_stream
#ifndef HAS_CDC_ACM
                        || (endpoint_index != 0
                            && endpoint_index != usb_endpoint_audio_stream)
#endif // !HAS_CDC_ACM
                        )
                        {
                            goto _handle_usb_setup_error;
                        }
                    }
                    // TODO: Support Halt=1 for STALL / SET_FEATURE(ENDPOINT_HALT)
                    break;
                }

                default:
                    goto _handle_usb_setup_error;
            }
            usb_prepare_packet(&endpoint_control,
                &nonconst_response_data,
                MIN_UNSAFE(sizeof(uint16_t), setup->length),
                decision, upload);
            break;
        }

        case UP_SETUP_REQ_CLEAR_FEATURE:
            if (setup->recipient == UP_SETUP_RECIPIENT_ENDPOINT)
            {
                auto const endpoint_index = setup->index_offset & 0xfu;
                if (setup->value == UP_FEATURE_ENDPOINT_HALT && endpoint_index != 0)
                {
                    // TODO: Change if more configurations are supported
                    usb_set_device_endpoint_response(
                        config0_endpoints[endpoint_index - 1],
                        USB_EP_STATUSENC_NAK,
                        USB_EP_STATUSENC_VALID, // TODO: maybe not ideal for write-only
                                                // endpoints?
                        false, false);

                    decision->last_packet = true;
                    // Keep size_to_send as 0 to send a ZLP
                }
                else
                {
                    board_kprintformat("Unk. endpoint feature to clear");
                    goto _handle_usb_setup_error;
                }
            }
            else
            {
                board_kprintformat("Unsupp. clear feature recipient");
                goto _handle_usb_setup_error;
            }
            break;

        case UP_SETUP_REQ_GET_DESCRIPTOR:
        {
            auto const descriptor_type = (setup->value >> 8);
            const size_t descriptor_index = (setup->value & 0xffu);
            const void * descriptor_ptr = NULL;
            size_t size_offset = 0;
            switch (descriptor_type)
            {
                case UP_DESCRIPTOR_TYPE_DEVICE:
                    // No need to decode the descriptor index as there is only one
                    // device descriptor
                    descriptor_ptr = &board_usb_device_descriptor;
                    break;

                case UP_DESCRIPTOR_TYPE_CONFIGURATION:
                    // TODO: Read index if several configurations
                    descriptor_ptr = &board_usb_configuration0_descriptor;
                    size_offset = 2;
                    break;

                case UP_DESCRIPTOR_TYPE_STRING:
                {
                    constexpr size_t string_descriptors_amount =
                        sizeof(board_usb_string_descriptors)
                            / sizeof(board_usb_string_descriptors[0]);
                    if (descriptor_index >= string_descriptors_amount)
                        goto _handle_usb_setup_error;

                    descriptor_ptr = board_usb_string_descriptors[descriptor_index];
                    break;
                }

                default: // High-speed descriptors and others
                    board_kprintformat("Unk. desc. req.\r\n");
                    goto _handle_usb_setup_error;
            }
            const size_t descriptor_size = ((uint8_t*) descriptor_ptr)[size_offset];
            usb_prepare_packet(&endpoint_control,
                descriptor_ptr,
                MIN_UNSAFE(descriptor_size, setup->length),
                decision, upload);
            break;
        }

        case UP_SETUP_REQ_SET_ADDRESS:
            requested_address = (setup->value & 0x7fu);
            decision->last_packet = true;
            next_state = CONTROL_STATE_SET_ADDRESS;
            // Keep size_to_send as 0 to send a ZLP
            break;

        case UP_SETUP_REQ_SET_CONFIGURATION:
            requested_configuration = setup->index_offset;
            decision->last_packet = true;
            next_state = CONTROL_STATE_SET_CONFIGURATION;
            // Keep size_to_send as 0 to send a ZLP
            break;

        case UP_SETUP_REQ_SET_INTERFACE:
            if (setup->index_offset == usb_config0_audio_stream_interface)
            {
                auto const old_on = audio_streaming_on;
                audio_streaming_on = (setup->value == usb_audio_stream_alt_play);
                if (old_on != audio_streaming_on)
                {
                    // RM0490 29.5.5, STATTX/STATRX for isochronous endpoints may only
                    // be DISABLED or VALID
                    usb_set_device_endpoint_response(&endpoint_audio_stream,
                        audio_streaming_on ? USB_EP_STATUSENC_VALID
                            : USB_EP_STATUSENC_DISABLED,
                        USB_EP_STATUSENC_DISABLED, // No RX
                        false, false);

                    if (audio_streaming_on)
                        board_start_sampling();
                    else
                        board_stop_sampling();

                    *endpoint_audio_stream.layout.dbl_buf.ctrl0 =
                        usb_make_tx_descriptor(config0_ep3_tx_pkt0,
                            endpoint_audio_stream.tx_packet_size);
                    *endpoint_audio_stream.layout.dbl_buf.ctrl1 =
                        usb_make_tx_descriptor(config0_ep3_tx_pkt1,
                            endpoint_audio_stream.tx_packet_size);
                }

                decision->last_packet = true;
                next_state = CONTROL_STATE_SET_xxx_TX_ACK;
            }
            else
            {
                board_kprintformat("Unexp. SET_INTERFACE %d\r\n", setup->index_offset);
                goto _handle_usb_setup_error;
            }
            break;

        default:
            board_kprintformat("Unexp. SETUP packet\r\n");
            goto _handle_usb_setup_error;
    }
    decision->has_failed = false;
    return next_state;

_handle_usb_setup_error:
    return CONTROL_STATE_IDLE;
}

static inline enum usb_device_control_state_e handle_usb_cdc_acm_request(
    const struct up_setup_t *setup,
    struct usb_decision_t *decision,
    struct upload_info_t *upload)
{
    auto next_state = CONTROL_STATE_WAIT_TX_ACK;

    decision->has_failed = true;
    switch (setup->request)
    {
        case UP_CDC_ACM_REQ_GET_LINE_CODING:
            usb_prepare_packet(&endpoint_control,
                cdc_acm_line_coding,
                MIN_UNSAFE(7, setup->length),
                decision, upload);
            break;

        case UP_CDC_ACM_REQ_SET_LINE_CODING:
            decision->expect_data_payload = true;
            next_state = CONTROL_STATE_CDC_ACM_SET_LINE_CODING;
            break;

        case UP_CDC_ACM_REQ_SET_CONTROL_LINE_STATE:
            next_state = CONTROL_STATE_SET_xxx_TX_ACK;
            // Keep size_to_send as 0 to send a ZLP
            break;

        default:
            board_kprintformat("Unexp. CDC ACM req.\r\n");
            goto _handle_usb_cdc_acm_request_error;
    }
    decision->last_packet = true;
    decision->has_failed = false;
    return next_state;

_handle_usb_cdc_acm_request_error:
    return CONTROL_STATE_IDLE;
}

// Returns next state
static inline enum usb_device_control_state_e handle_usb_audio_request(
    const struct up_setup_t *setup,
    struct usb_decision_t *decision,
    struct upload_info_t *upload)
{
    auto next_state = CONTROL_STATE_WAIT_TX_ACK;
    const uint16_t entity_id = (setup->index_offset >> 8);
    auto const is_cur = (setup->request == AUDIO_REQ_CUR);
    auto const channel_number = (setup->value & 0xffu);
    auto const control_selector = (setup->value >> 8);

    decision->has_failed = true;
    if (channel_number != 0)
    {
        board_kprintformat("Unsupp. Audio CN %d\r\n", channel_number);
        goto _handle_usb_audio_request_error;
    }

    switch (entity_id)
    {
        case 0: // Device
            board_kprintformat("Unexp. Audio Device\r\n");
            goto _handle_usb_audio_request_error;

        case usb_audio_clock_source_id:
            if (control_selector > AUDIO_CS_CLOCK_VALID_CONTROL)
            {
                board_kprintformat("Unexp. Audio CS\r\n");
                goto _handle_usb_audio_request_error;
            }
            if (setup->is_get && control_selector == AUDIO_CS_SAM_FREQ_CONTROL)
            {
                const void * ptr = is_cur ? (void*) &usb_audio_clock_source_current
                    : (void*) usb_audio_clock_source_ranges;
                auto const size = is_cur ? sizeof(usb_audio_clock_source_current)
                    : sizeof(usb_audio_clock_source_ranges);
                usb_prepare_packet(&endpoint_control, ptr,
                    MIN_UNSAFE(size, setup->length),
                    decision, upload);
            }
            else if (!setup->is_get && is_cur
            && control_selector == AUDIO_CS_SAM_FREQ_CONTROL)
            {
                decision->has_failed = false;
                decision->expect_data_payload = true;
                next_state = CONTROL_STATE_AUDIO_SET_CS_SAM_FREQ;
            }
            else
            {
                board_kprintformat("Unsupp. Audio SET\r\n");
                goto _handle_usb_audio_request_error;
            }
            break;

        case usb_audio_input_terminal_id:
        case usb_audio_output_terminal_id:
            if (control_selector > AUDIO_TE_PHANTOM_POWER_CONTROL)
            {
                board_kprintformat("Unexp. Audio TE\r\n");
                goto _handle_usb_audio_request_error;
            }
            else
            {
                board_kprintformat("Unsupp. Audio TE\r\n");
                goto _handle_usb_audio_request_error;
            }
            break;

        default:
            board_kprintformat("Unexp. Audio EntityId %d\r\n", entity_id);
            goto _handle_usb_audio_request_error;
    }

    decision->has_failed = false;
    return next_state;

_handle_usb_audio_request_error:
    return CONTROL_STATE_IDLE;
}

// Returns next state
static inline enum usb_device_control_state_e handle_usb_ep0(
    struct usb_decision_t *decision,
    struct upload_info_t *upload)
{
    static_assert(sizeof(struct up_setup_t) == sizeof(uint64_t));

    auto const data_size = usb_rx_get_byte_count(
        *endpoint_control.layout.tx_rx.rx_ctrl);
    if (data_size != 8)
    {
        board_kprintformat("SETUP with %d bytes\r\n", data_size);
        goto _handle_usb_ep0_error;
    }

    struct up_setup_t setup;
    memcpyv(&setup, ep0_rx_pkt, sizeof(setup));
    usb_endpoint_ack(&endpoint_control, false, true);

    // In case of audio streaming start, printing would take too much time, compared to
    // the 1ms cadence of full-speed isochronous transmission
    if (!(setup.request == UP_SETUP_REQ_SET_INTERFACE
    && setup.index_offset == usb_config0_audio_stream_interface))
    {
        board_kprintformat("S %8b\r\n", &setup);
    }

    if (!up_verify_setup(&setup))
    {
        board_kprintformat("Bad setup\r\n");
        goto _handle_usb_ep0_error;
    }
    if (up_is_standard(&setup))
        return handle_usb_setup(&setup, decision, upload);

    // Configs might extend the list of supported SETUP packets
    if (board_usb_cdc_acm_is_active())
    {
        if (setup.recipient == UP_SETUP_RECIPIENT_INTERFACE
        && setup.type == UP_SETUP_TYPE_CLASS)
        {
            switch (setup.index_offset & 0xfu)
            {
#ifdef HAS_CDC_ACM
                case usb_config0_cdc_acm_control_interface:
                    if (usb_is_cdc_acm_request(&setup))
                        return handle_usb_cdc_acm_request(&setup, decision, upload);
                    break;
#endif // HAS_CDC_ACM

                case usb_config0_audio_control_interface:
                    if (usb_is_audio_request(&setup))
                        return handle_usb_audio_request(&setup, decision, upload);
                    break;

                default:
                    board_kprintformat("Unexp. dest. interface %d\r\n",
                        setup.index_offset);
                    break;
            }
        }
    }
    board_kprintformat("No packet listener matched\r\n");

_handle_usb_ep0_error:
    return CONTROL_STATE_IDLE;
}

static inline void board_usb_suspend(void)
{
    usb_suspend();
    // TODO: Switch off peripherals on the board
}

void board_usb_init_ep0(void)
{
    constexpr struct usb_device_endpoint_config_t endpoint_config = {
        .address = usb_endpoint_control,
        .type = USB_EP_TYPE_CONTROL,
        .tx_status = USB_EP_STATUSENC_NAK,
        .rx_status = USB_EP_STATUSENC_VALID // Allow RX
    };

    state = CONTROL_STATE_IDLE;

    usb_setup_device_endpoint(endpoint_config.address, &endpoint_config,
        &endpoint_control);
    endpoint_control.layout.tx_rx.tx_packet = ep0_tx_pkt;
    endpoint_control.layout.tx_rx.rx_packet = ep0_rx_pkt;
    endpoint_control.tx_packet_size = ep0_tx_size;
    *endpoint_control.layout.tx_rx.tx_ctrl = usb_make_tx_descriptor(ep0_tx_pkt,
        0);
    *endpoint_control.layout.tx_rx.rx_ctrl = usb_make_rx_descriptor(ep0_rx_pkt,
        ep0_rx_size);
}

static inline void board_usb_set_configuration(size_t configuration_index)
{
    // TODO: use configuration_index if several configurations are supported
    current_config = configuration_index;

    // Endpoint 0 - Setup + CDC ACM control; already configured
    // Endpoint 1 - CDC ACM notification
#ifdef HAS_CDC_ACM
    {
        constexpr struct usb_device_endpoint_config_t endpoint_config = {
            .address = usb_endpoint_cdc_notification,
            .type = USB_EP_TYPE_INTERRUPT, // Cf. configuration
            .tx_status = USB_EP_STATUSENC_NAK,
            .rx_status = USB_EP_STATUSENC_DISABLED // TX-only endpoint
        };
        usb_setup_device_endpoint(endpoint_config.address, &endpoint_config,
            &endpoint_cdc_notif);
        endpoint_cdc_notif.layout.tx_rx.tx_packet = config0_ep1_tx_pkt;
        endpoint_cdc_notif.layout.tx_rx.rx_packet = NULL;
        endpoint_cdc_notif.tx_packet_size = config0_ep1_tx_size;
        *endpoint_cdc_notif.layout.tx_rx.tx_ctrl = usb_make_tx_descriptor(
            config0_ep1_tx_pkt, 0);
        *endpoint_cdc_notif.layout.tx_rx.rx_ctrl = 0x0u;
    }

    // Endpoint 2 - CDC ACM data
    {
        constexpr struct usb_device_endpoint_config_t endpoint_config = {
            .address = usb_endpoint_cdc_data,
            .type = USB_EP_TYPE_BULK, // Cf. configuration
            .tx_status = USB_EP_STATUSENC_NAK,
            .rx_status = USB_EP_STATUSENC_VALID // Allow RX
        };
        usb_setup_device_endpoint(endpoint_config.address, &endpoint_config,
            &endpoint_cdc_data);
        endpoint_cdc_data.layout.tx_rx.tx_packet = config0_ep2_tx_pkt;
        endpoint_cdc_data.layout.tx_rx.rx_packet = config0_ep2_rx_pkt;
        endpoint_cdc_data.tx_packet_size = config0_ep2_tx_size;
        *endpoint_cdc_data.layout.tx_rx.tx_ctrl = usb_make_tx_descriptor(config0_ep2_tx_pkt, 0);
        *endpoint_cdc_data.layout.tx_rx.rx_ctrl = usb_make_rx_descriptor(config0_ep2_rx_pkt,
            config0_ep2_rx_size);
    }
#endif // HAS_CDC_ACM

    // Endpoint 3 - Audio stream
    {
        // RM0490 29.5.5, STATTX/STATRX for isochronous endpoints may only
        // be DISABLED or VALID
        constexpr struct usb_device_endpoint_config_t endpoint_config = {
            .address = usb_endpoint_audio_stream,
            .type = USB_EP_TYPE_ISO, // Cf. configuration
            .tx_status = USB_EP_STATUSENC_DISABLED, // No TX yet since alt0 has no
                                                    // endpoint
            .rx_status = USB_EP_STATUSENC_DISABLED // TX-only endpoint
        };
        usb_setup_device_endpoint(endpoint_config.address, &endpoint_config,
            &endpoint_audio_stream);
        // Since double-buffering is on for isochronous endpoints, RX is actually the
        // "other" TX
        endpoint_audio_stream.layout.dbl_buf.packet0 = config0_ep3_tx_pkt0;
        endpoint_audio_stream.layout.dbl_buf.packet1 = config0_ep3_tx_pkt1;
        endpoint_audio_stream.tx_packet_size = config0_ep3_tx_size - 8; // Jitter
        *endpoint_audio_stream.layout.dbl_buf.ctrl0 = usb_make_tx_descriptor(
            config0_ep3_tx_pkt0, 0);
        *endpoint_audio_stream.layout.dbl_buf.ctrl1 = usb_make_tx_descriptor(
            config0_ep3_tx_pkt1, 0);
    }
}

void board_usb_handler(void)
{
    static struct upload_info_t upload;

    nvic_ack(nvic_usb);

    struct usb_interrupt_info_t info;
    usb_get_interrupt_info(&info);

    const bool was_suspended = is_suspended;
    if (info.pending_interrupts & USB_INT_WKUP)
    {
        constexpr struct usb_interrupt_mask_t interrupt_mask = {
            .l1reqm = false,
            .esofm = true,
            .sofm = false,
            .rst_dconm = false,
            .suspm = true,
            .wkupm = true,
            .errm = false,
            .pmaovrm = false,
            .ctrm = false,
            .thr512m = false,
            .ddiscm = false
        };
        usb_interrupt_ack(interrupt_mask);
        // TODO: Read RXDP, RXDM
        if (was_suspended)
        {
            usb_wakeup();
            is_suspended = false;
        }
    }
    else if (info.pending_interrupts & USB_INT_SUSP)
    {
        constexpr struct usb_interrupt_mask_t interrupt_mask = {
            .l1reqm = false,
            .esofm = true,
            .sofm = false,
            .rst_dconm = false,
            .suspm = true,
            .wkupm = true,
            .errm = false,
            .pmaovrm = false,
            .ctrm = false,
            .thr512m = false,
            .ddiscm = false
        };
        usb_interrupt_ack(interrupt_mask);
        if (!was_suspended)
        {
            board_usb_suspend();
            is_suspended = true;
        }
    }
    if (was_suspended != is_suspended)
    {
        state = CONTROL_STATE_IDLE;
        usb_set_device_endpoint_response(&endpoint_control,
            USB_EP_STATUSENC_NAK, USB_EP_STATUSENC_VALID, false, false);
        return;
    }

    if (info.pending_interrupts & USB_INT_RST_DCON)
    {
        constexpr struct usb_interrupt_mask_t interrupt_mask = {
            .l1reqm = false,
            .esofm = true,
            .sofm = false,
            .rst_dconm = true,
            .suspm = false,
            .wkupm = false,
            .errm = false,
            .pmaovrm = false,
            .ctrm = false,
            .thr512m = false,
            .ddiscm = false
        };
        usb_interrupt_ack(interrupt_mask);
        state = CONTROL_STATE_IDLE;
        current_address = 0;
        current_config = no_config;
        board_usb_reset(0x0);
        return;
    }

    if (info.pending_interrupts & USB_INT_SOF)
    {
        constexpr struct usb_interrupt_mask_t interrupt_mask = {
            .l1reqm = false,
            .esofm = false,
            .sofm = true,
            .rst_dconm = false,
            .suspm = false,
            .wkupm = false,
            .errm = false,
            .pmaovrm = false,
            .ctrm = false,
            .thr512m = false,
            .ddiscm = false
        };
        usb_interrupt_ack(interrupt_mask);

        if (board_usb_cdc_acm_is_active() && cdc_acm_tx_sof_timeout > 0)
            --cdc_acm_tx_sof_timeout;
    }

    if (info.pending_interrupts & USB_INT_CTR)
    {
        auto const address = usb_get_device_address();
        if (address != current_address)
        {
            // TODO: probably reset?
            board_kprintformat("Unexp. address %d vs. %d\r\n", address, current_address);
            signal_fail();
        }

        // RM0490 p964
        if (info.endpoint == usb_endpoint_control)
        {
            auto const volatile endpoint_info = usb_get_device_endpoint_info(
                &endpoint_control);
            if (endpoint_info.setup && endpoint_info.valid_rx)
            {
                struct usb_decision_t decision = {
                    .size_to_send = 0,
                    .expect_data_payload = false
                };

                if (endpoint_info.valid_tx)
                {
                    // TODO: probably reset?
                    board_kprintformat("Unexp. VTTX on SETUP VTRX\r\n");
                    signal_fail();
                }

                state = handle_usb_ep0(&decision, &upload);
                usb_send_packet(&endpoint_control, decision,
                    state < CONTROL_STATE_SET_ENUM_BEGIN, false);
            }
            else if(endpoint_info.setup)
            {
                // TODO: probably reset?
                board_kprintformat("Unexp. SETUP with VTRX=0\r\n");
                signal_fail();
            }
            else if (endpoint_info.valid_tx)
            {
                usb_endpoint_ack(&endpoint_control, true, false);
                switch (state)
                {
                    case CONTROL_STATE_SET_xxx_TX_ACK:
                        state = CONTROL_STATE_IDLE;
                        break;

                    case CONTROL_STATE_SET_ADDRESS:
                        current_address = requested_address;
                        state = CONTROL_STATE_IDLE;
                        usb_configure_device(true, requested_address);
                        break;

                    case CONTROL_STATE_SET_CONFIGURATION:
                        board_usb_set_configuration(requested_configuration);
                        state = CONTROL_STATE_IDLE;
                        break;

                    case CONTROL_STATE_WAIT_TX_ACK:
                        // DATA IN
                        // Stay in the same state if there are still packets to send
                        if (upload.n_remaining != 0)
                        {
                            struct usb_decision_t decision = {
                                .size_to_send = 0,
                                .expect_data_payload = false,
                                .last_packet = true,
                                .has_failed = false,
                            };

                            usb_prepare_packet(&endpoint_control, upload.buffer,
                                upload.n_remaining, &decision, &upload);
                            usb_send_packet(&endpoint_control, decision, true, false);
                            return;
                        }
                        state = CONTROL_STATE_WAIT_RX_ACK;
                        break;

                    // Fallback to stall
                    default:
                        goto _board_usb_handler_stall;
                }
                usb_set_device_endpoint_response(&endpoint_control,
                    USB_EP_STATUSENC_NAK, USB_EP_STATUSENC_VALID,
                    state < CONTROL_STATE_SET_ENUM_BEGIN, false);
            }
            else if (endpoint_info.valid_rx)
            {
                switch (state)
                {
                    case CONTROL_STATE_CDC_ACM_SET_LINE_CODING:
                    {
                        // Data size is 7, no more than one data packet is required
                        auto const data_size = usb_rx_get_byte_count(
                            *endpoint_control.layout.tx_rx.rx_ctrl);
                        memcpyv(cdc_acm_line_coding, ep0_rx_pkt,
                            MIN_UNSAFE(data_size, sizeof(cdc_acm_line_coding)));
                        break;
                    }

                    case CONTROL_STATE_AUDIO_SET_CS_SAM_FREQ:
                    {
                        // Data size is 4, no more than one data packet is required
                        auto const data_size = usb_rx_get_byte_count(
                            *endpoint_control.layout.tx_rx.rx_ctrl);
                        uint32_t requested_sampling_frequency = 0;
                        memcpyv(&requested_sampling_frequency, ep0_rx_pkt,
                            MIN_UNSAFE(data_size,
                                sizeof(requested_sampling_frequency)));

                        // Since the sampling frequency is constant for now, just reject
                        // any different value
                        if (requested_sampling_frequency !=
                            usb_audio_clock_source_current)
                        {
                            goto _board_usb_handler_stall;
                        }
                        break;
                    }

                    case CONTROL_STATE_WAIT_RX_ACK:
                        break;

                    // Fallback to stall
                    default:
                        usb_endpoint_ack(&endpoint_control, false, true);
                        goto _board_usb_handler_stall;
                }

                usb_endpoint_ack(&endpoint_control, false, true);
                state = CONTROL_STATE_IDLE;
                usb_set_device_endpoint_response(&endpoint_control,
                    USB_EP_STATUSENC_NAK, USB_EP_STATUSENC_VALID, false, false);
            }
            else
                goto _board_usb_handler_stall;
        }
        else if (current_config != no_config)
        {
            // TODO: Change if more than one config is supported
            auto const endpoint = config0_endpoints[info.endpoint - 1];
            auto const volatile endpoint_info = usb_get_device_endpoint_info(endpoint);
            if (endpoint_info.setup)
            {
                // TODO: Probably reset? Since the device may not stall a setup packet
                board_kprintformat("Unexp. SETUP on nonzero EP\r\n");
                signal_fail();
            }
            else if (endpoint_info.valid_tx)
            {
                usb_endpoint_ack(endpoint, true, false);

                if (board_usb_cdc_acm_is_active()
                && info.endpoint == usb_endpoint_cdc_data)
                {
                    usb_set_device_endpoint_response(endpoint,
                        USB_EP_STATUSENC_NAK,
                        USB_EP_STATUSENC_NAK, // Unused
                        false, true);
                    cdc_acm_wait_for_tx_ack = false;
                }
                else if (board_usb_audio_is_active()
                && info.endpoint == usb_endpoint_audio_stream)
                {
                    volatile void * const new_buffer =
                        usb_endpoint_get_iso_buffer(&endpoint_audio_stream, true);
                    auto const actual_buffer_size =
                        board_on_audio_buffers_swapped(new_buffer);

                    if (new_buffer == config0_ep3_tx_pkt0)
                    {
                        *endpoint_audio_stream.layout.dbl_buf.ctrl0 =
                            usb_make_tx_descriptor(config0_ep3_tx_pkt0,
                                actual_buffer_size);
                    }
                    else
                    {
                        *endpoint_audio_stream.layout.dbl_buf.ctrl1 =
                            usb_make_tx_descriptor(config0_ep3_tx_pkt1,
                                actual_buffer_size);
                    }
                }
            }
            else if (endpoint_info.valid_rx)
            {
                if (board_usb_cdc_acm_is_active()
                && info.endpoint < 1 + sizeof(config0_endpoints) / sizeof(config0_endpoints[0]))
                {
                    // Code is left here for the example, but the app simply ignores
                    // input data on CDC ACM
#if 0
                    // -1 because EP0 is always endpoint_control, so it's not present
                    // inside the array
                    auto endpoint = config0_endpoints[info.endpoint - 1];

                    // Should be 1, but host might buffer input?
                    auto const data_size = usb_rx_get_byte_count(*endpoint->rx_ctrl);
                    for (size_t copied = 0; copied < data_size; copied += sizeof(uint32_t))
                    {
                        uint32_t data = 0;
                        memcpyv(&data, endpoint->layout.tx_rx.rx_packet, sizeof(data));
                        // TODO: do something with packet contents
                    }
#endif
                    usb_endpoint_ack(endpoint, false, true);
                    usb_set_device_endpoint_response(endpoint,
                        USB_EP_STATUSENC_NAK, USB_EP_STATUSENC_VALID, false, false);
                }
                else
                {
                    usb_endpoint_ack(endpoint, false, true);
                    goto _board_usb_handler_stall;
                }
            }
            else
                goto _board_usb_handler_stall;
        }
    }
    /* TODO
    USB_INT_PMAOVR
    */
    return; // Skip stall

_board_usb_handler_stall:
    state = CONTROL_STATE_IDLE;
    usb_set_device_endpoint_response(&endpoint_control,
        USB_EP_STATUSENC_STALL, USB_EP_STATUSENC_STALL, false, false);
}

bool board_usb_cdc_acm_is_active(void)
{
    return current_config == 0;
}

bool board_usb_audio_is_active(void)
{
    return current_config == 0 && audio_streaming_on;
}

static inline void board_usb_cdc_acm_flush(void)
{
#ifdef HAS_CDC_ACM
    if (cdc_acm_current_tx_size == 0)
        return;

    struct usb_decision_t decision = {
        .size_to_send = 0,
        .expect_data_payload = false,
        .last_packet = true,
        .has_failed = false,
    };
    usb_prepare_packet(&endpoint_cdc_data, cdc_acm_buffer,
        cdc_acm_current_tx_size, &decision, NULL);
    usb_send_packet(&endpoint_cdc_data, decision, false, true);

    cdc_acm_wait_for_tx_ack = true;
    cdc_acm_current_tx_size = 0;
    cdc_acm_tx_sof_timeout = 5; // Timeout = approx. 5ms

    // Temporarily activate SOFM interrupts to get a 1kHz clock for timeout management
    constexpr struct usb_interrupt_mask_t interrupt_mask = {
        .l1reqm = false,
        .esofm = false,
        .sofm = true,
        .rst_dconm = false,
        .suspm = false,
        .wkupm = false,
        .errm = false,
        .pmaovrm = false,
        .ctrm = false,
        .thr512m = false,
        .ddiscm = false
    };
    usb_activate_interrupts(interrupt_mask);

    // Warning: waiting for interrupts require that USB CTR interrupt is on, including
    // NVIC.
    // If cdc_acm_tx_sof_timeout reaches 0, then consider the packet lost.
    while (cdc_acm_wait_for_tx_ack && cdc_acm_tx_sof_timeout > 0)
        __asm__("wfi");

    usb_deactivate_interrupts(interrupt_mask);
#endif // HAS_CDC_ACM
}

void board_usb_cdc_acm_putc(char c)
{
#ifdef HAS_CDC_ACM
    cdc_acm_buffer[cdc_acm_current_tx_size++] = c;
    if (c == '\n' || cdc_acm_current_tx_size >= sizeof(cdc_acm_buffer))
        board_usb_cdc_acm_flush();
#else
    (void) c;
#endif // HAS_CDC_ACM
}
