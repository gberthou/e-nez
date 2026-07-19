#ifndef DRIVERS_USB_H
#define DRIVERS_USB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct __attribute__((packed, aligned(4))) usb_interrupt_mask_t
{
    bool l1reqm: 1;
    bool esofm: 1;
    bool sofm: 1;
    bool rst_dconm: 1;
    bool suspm: 1;
    bool wkupm: 1;
    bool errm: 1;
    bool pmaovrm: 1;
    bool ctrm: 1;
    bool thr512m: 1;
    bool ddiscm: 1;
    uint32_t _pad1: 21;
};

enum usb_endpoint_statusenc_e: uint32_t
{
    USB_EP_STATUSENC_DISABLED = 0,
    USB_EP_STATUSENC_STALL = 1,
    USB_EP_STATUSENC_NAK = 2,
    USB_EP_STATUSENC_VALID = 3
};

enum usb_endpoint_type_e: uint32_t
{
    USB_EP_TYPE_BULK = 0,
    USB_EP_TYPE_BULK_DBL_BUF = 1,
    USB_EP_TYPE_CONTROL = 2,
    USB_EP_TYPE_CONTROL_STATUS_OUT = 3,
    USB_EP_TYPE_ISO = 4,
    USB_EP_TYPE_ISO_SINGLE_BUF = 5,
    USB_EP_TYPE_INTERRUPT = 6
};

// For device (re-)configuration purposes
struct usb_device_endpoint_config_t
{
    uint32_t address; // 0-15
    enum usb_endpoint_type_e type;
    enum usb_endpoint_statusenc_e tx_status;
    enum usb_endpoint_statusenc_e rx_status;
};

struct usb_device_endpoint_t
{
    union {
        struct { // General case
            volatile uint32_t *tx_ctrl;
            volatile uint32_t *rx_ctrl;
            volatile uint32_t *tx_packet;
            const volatile uint32_t *rx_packet;
        } tx_rx;
        struct { // Isochronous double-buffered endpoints
            volatile uint32_t *ctrl0;
            volatile uint32_t *ctrl1;
            volatile uint32_t *packet0;
            volatile uint32_t *packet1;
        } dbl_buf;
    } layout;
    uint16_t tx_packet_size;
    uint8_t index;
};

// To decode the hardware-updated fields when a packet is received
struct __attribute__((packed, aligned(4))) usb_device_endpoint_info_t
{
    uint32_t _pad0: 4;
    enum usb_endpoint_statusenc_e tx_status: 2;
    bool data_toggle_tx: 1;
    bool valid_tx: 1;
    bool ep_kind: 1;
    uint8_t _pad2: 2;
    bool setup: 1;
    enum usb_endpoint_statusenc_e rx_status: 2;
    bool data_toggle_rx: 1;
    bool valid_rx: 1;
    uint16_t _pad3: 16;
};

enum usb_interrupt_e: uint32_t
{
    USB_INT_L1REQ = (0x1u << 7),
    USB_INT_ESOF = (0x1u << 8),
    USB_INT_SOF = (0x1u << 9),
    USB_INT_RST_DCON = (0x1u << 10),
    USB_INT_SUSP = (0x1u << 11),
    USB_INT_WKUP = (0x1u << 12),
    USB_INT_ERR = (0x1u << 13),
    USB_INT_PMAOVR = (0x1u << 14),
    USB_INT_CTR = (0x1u << 15),
    USB_INT_THR512 = (0x1u << 16),
    USB_INT_DDISC = (0x1u << 17),
    USB_INT_DCON_STAT = (0x1u << 29),
    USB_INT_LS_DCON = (0x1u << 30)
};

struct usb_interrupt_info_t
{
    uint32_t pending_interrupts;
    uint32_t endpoint;
};

void usb_init(bool host, struct usb_interrupt_mask_t interrupt_mask, uint32_t raw_cntr);
void usb_activate_interrupts(struct usb_interrupt_mask_t interrupt_mask);
void usb_deactivate_interrupts(struct usb_interrupt_mask_t interrupt_mask);
void usb_interrupt_ack(struct usb_interrupt_mask_t interrupt_mask);
void usb_reset(void);
void usb_suspend(void);
void usb_wakeup(void);
void usb_start(void);
void usb_configure_device(bool activate, uint32_t address);
uint32_t usb_get_device_address(void);
void usb_setup_device_endpoint(unsigned index,
    const struct usb_device_endpoint_config_t *desc,
    struct usb_device_endpoint_t *endpoint);

// Set preserve_rx_status to keep the STATRX value (ignores rx_status)
void usb_set_device_endpoint_response(const struct usb_device_endpoint_t *endpoint,
    enum usb_endpoint_statusenc_e tx_status, enum usb_endpoint_statusenc_e rx_status,
    bool epkind, bool preserve_rx_status);

struct usb_device_endpoint_info_t usb_get_device_endpoint_info(
    const struct usb_device_endpoint_t *endpoint);
void usb_endpoint_ack(const struct usb_device_endpoint_t *endpoint, bool tx_ack,
    bool rx_ack);

// Assume a double-buffered isochronous endpoint.
// Return buffer under an application-centered perspective: when DTOG* is cleared,
// hardware is using the "normal" buffer so application must use the alternate buffer
// and vice-versa.
volatile void *usb_endpoint_get_iso_buffer(const struct usb_device_endpoint_t *endpoint,
    bool is_tx);

void usb_get_interrupt_info(struct usb_interrupt_info_t *info);

// Utils
static inline uint32_t usb_make_tx_descriptor(volatile const void *ptr, uint32_t count)
{
    const uint32_t address = ((uint32_t) ptr) & 0x7ffu; // USBSRAM is 2kB-0x40B
    count &= 0x3ffu;
    return address | (count << 16);
}

static inline void usb_set_count_tx(volatile uint32_t *descriptor, uint32_t count)
{
    constexpr uint32_t count_mask = 0x3ff;
    constexpr size_t count_shift = 16;

    auto value = *descriptor;
    count &= count_mask;
    value &= ~(count_mask << count_shift);
    *descriptor = value | (count << count_shift);
}

static inline uint32_t usb_make_rx_descriptor(volatile const void *ptr, uint32_t size)
{
    const uint32_t address = ((uint32_t) ptr) & 0x7ffu; // USBSRAM is 2kB-0x40B

    // RM0490 Table 168
    if (size > 1023)
        size = 1023;
    else if (size < 2)
        size = 2;
    // Quirk: BLSIZE=0 also supports 32B (NUM_BLOCK=16)
    // But maybe 1 block of 32B is better than 16 blocks of 2B
    const bool blsize = (size == 32 || size > 62);
    // If the size is not directly supported (odd when BLSIZE=0 or not a multiple of 32
    // when BLSIZE=1), allocate the largest buffer below requested size, so that the
    // hardware USB controller will not overflow onto other packets.
    const uint32_t n_block = (blsize
        ? (size >> 5) - 1
        : (size >> 1)
    );
    // Count = 0 because it's only meant to be written by hardware
    return address | (n_block << 26)
        | (blsize ? (0x1u << 31) : 0x0u);
}

static inline size_t usb_rx_get_byte_count(uint32_t usbsram_endpoint_entry)
{
    return (usbsram_endpoint_entry >> 16) & 0x3ffu;
}

#endif // DRIVERS_USB_H
