#include <string.h>

#include "usb.h"

struct __attribute__((packed)) usb_t
{
    uint32_t chepr[8];
    uint32_t _reserved0[8];
    uint32_t cntr;
    uint32_t istr;
    uint32_t fnr;
    uint32_t daddr;
    uint32_t _reverved1;
    uint32_t lpmcsr;
    uint32_t bcdr;
};

static volatile struct usb_t * const usb = (volatile struct usb_t*) 0x40005c00;
static volatile uint32_t * const usbsram_base = (volatile uint32_t*) 0x40009800;

constexpr uint32_t cntr_usbrst = (0x1u << 0);
constexpr uint32_t cntr_pdwn = (0x1u << 1);
constexpr uint32_t cntr_suspen = (0x1u << 3);
constexpr size_t cntr_l1reqm_shift = 7;

static_assert(sizeof(struct usb_interrupt_mask_t) == sizeof(uint32_t));
void usb_init(bool host, struct usb_interrupt_mask_t interrupt_mask, uint32_t raw_cntr)
{
    uint32_t interrupt_mask_u32 = 0;
    memcpy(&interrupt_mask_u32, &interrupt_mask, sizeof(interrupt_mask_u32));

    const uint32_t cntr = (host ? (0x1u << 31) : 0x0u)
        | (interrupt_mask_u32 << cntr_l1reqm_shift)
        | (raw_cntr & 0x38u);
    usb->cntr = cntr;
}

void usb_activate_interrupts(struct usb_interrupt_mask_t interrupt_mask)
{
    uint32_t interrupt_mask_u32 = 0;
    memcpy(&interrupt_mask_u32, &interrupt_mask, sizeof(interrupt_mask_u32));

    usb->cntr |= (interrupt_mask_u32 << cntr_l1reqm_shift);
    usb_interrupt_ack(interrupt_mask);
}

void usb_deactivate_interrupts(struct usb_interrupt_mask_t interrupt_mask)
{
    uint32_t interrupt_mask_u32 = 0;
    memcpy(&interrupt_mask_u32, &interrupt_mask, sizeof(interrupt_mask_u32));

    usb->cntr &= ~(interrupt_mask_u32 << cntr_l1reqm_shift);
}

void usb_interrupt_ack(struct usb_interrupt_mask_t interrupt_mask)
{
    uint32_t interrupt_mask_u32 = 0;
    memcpy(&interrupt_mask_u32, &interrupt_mask, sizeof(interrupt_mask_u32));

    usb->istr = ~(interrupt_mask_u32 << cntr_l1reqm_shift);
}

void usb_reset(void)
{
    usb->cntr |= cntr_pdwn | cntr_usbrst;
}

void usb_suspend(void)
{
    // RM0490 page 972
    usb->cntr |= cntr_suspen;
}

void usb_wakeup(void)
{
    // RM0490 page 972
    usb->cntr &= ~cntr_suspen;
}

void usb_start(void)
{
    // TODO: make sure t_startup = 1us has passed
    // (cf. STM32C071x Table 67)
    for (volatile unsigned x = 30; x--; );

    usb->cntr &= ~(cntr_pdwn | cntr_usbrst);
}

void usb_configure_device(bool activate, uint32_t address)
{
    if (!activate)
        usb->daddr = 0x0u;
    else
    {
        address &= 0x7fu;
        const uint32_t daddr = (0x1u << 7) // EF
                             | address
                             ;
        usb->daddr = daddr;
        usb->bcdr |= (0x1u << 15); // DPPU_DPD
    }
}

uint32_t usb_get_device_address(void)
{
    // TODO: Check EF maybe?
    return usb->daddr & 0x7fu;
}

void usb_setup_device_endpoint(
    unsigned index,
    const struct usb_device_endpoint_config_t *desc,
    struct usb_device_endpoint_t *endpoint)
{
    constexpr uint32_t togglable_mask = ((0x7u << 4) | (0x7u << 12));

    auto const type = desc->type;
    auto const is_bulk_double_buf = (type == USB_EP_TYPE_BULK_DBL_BUF);
    auto const is_control = (type == USB_EP_TYPE_CONTROL || type == USB_EP_TYPE_CONTROL_STATUS_OUT);

    // The DTOG* bits matter for isochronous endpoints with EPKIND=0, however the
    // application doesn't need full configurability of the initial state if it assumes
    // that the initial values are 0 (guaranteed by this function)
    const uint32_t dtogtx = (is_bulk_double_buf || is_control ? 1: 0);
    const uint32_t dtogrx = 0;

    index &= 0x7u;
    auto const initial_chepr = usb->chepr[index];
    auto const togglable_value =
        (initial_chepr ^
            ((desc->tx_status << 4)
            | (dtogtx << 6)
            | (desc->rx_status << 12)
            | (dtogrx << 14)))
        & togglable_mask;
    // Replace/clear all rw/w0 fields
    const uint32_t chepr = desc->address
                         | togglable_value
                         | (type << 8) // UTYPE:EPKIND
                         ;
    usb->chepr[index] = chepr;

    auto sram_base = &usbsram_base[2 * index];
    // TODO: Consider whether enpoint is IN or OUT + DTOG
    if (type == USB_EP_TYPE_ISO)
    {
        endpoint->layout.dbl_buf.ctrl0 = &sram_base[0];
        endpoint->layout.dbl_buf.ctrl1 = &sram_base[1];
    }
    else
    {
        endpoint->layout.tx_rx.tx_ctrl = &sram_base[0];
        endpoint->layout.tx_rx.rx_ctrl = &sram_base[1];
    }
    endpoint->index = index;
}

void usb_set_device_endpoint_response(
    const struct usb_device_endpoint_t *endpoint,
    enum usb_endpoint_statusenc_e tx_status,
    enum usb_endpoint_statusenc_e rx_status,
    bool epkind,
    bool preserve_rx_status)
{
    // VTTX, VTRX
    // (host-only?) NAK, ERR_TX, ERR_TX, THREE_ERR_TX, THREE_ERR_RX
    constexpr uint32_t w0_preserve = 0x7e808080u;

    // DTOGTX, DTGOGR
    constexpr uint32_t togglable_preserve = (0x1u << 6) | (0x1u << 14);

    // STATTX, STATRX
    const uint32_t stattx_statrx_mask = (0x3u << 4)
                                      | (preserve_rx_status ? 0 : (0x3u << 12));

    auto const chepr_ptr = &usb->chepr[endpoint->index];
    auto const initial_chepr = *chepr_ptr;
    auto chepr = initial_chepr;
    // Clear STATTX, STATRX, EPKIND
    // Clear togglable bits to preserve their values
    // Preserve rw fields except EPKIND
    chepr &= ~((0x1u << 8) | stattx_statrx_mask | togglable_preserve);

    auto const toggled_stattx_statrx =
        (initial_chepr ^ ((tx_status << 4) | (rx_status << 12))) & stattx_statrx_mask;

    chepr |= toggled_stattx_statrx
          | (epkind ? (0x1u << 8) : 0x0u)
          // Set w0 bits to preserve their values
          | w0_preserve
          ;

    // Keep VTTX and VTRX values. Use usb_endpoint_ack to clear them
    *chepr_ptr = chepr;
}

static_assert(sizeof(struct usb_device_endpoint_info_t) == sizeof(uint32_t));
struct usb_device_endpoint_info_t usb_get_device_endpoint_info(
    const struct usb_device_endpoint_t *endpoint)
{
    auto const chepr = usb->chepr[endpoint->index];
    struct usb_device_endpoint_info_t ret;
    memcpy(&ret, &chepr, sizeof(ret));
    return ret;
}

void usb_endpoint_ack(const struct usb_device_endpoint_t *endpoint, bool tx_ack,
    bool rx_ack)
{
    constexpr uint32_t preserve_mask = 0x807f070fu;
    constexpr uint32_t vttx_mask = (0x1u << 7);
    constexpr uint32_t vtrx_mask = (0x1u << 15);

    const uint32_t w0_mask = ((tx_ack ? 0x0u : vttx_mask)
        | (rx_ack ? 0x0u : vtrx_mask)
        | (0x1u << 23)
        | (0x3fu << 25)
    );

    auto const chepr_ptr = &usb->chepr[endpoint->index];
    auto chepr = *chepr_ptr;
    chepr &= preserve_mask;
    chepr |= w0_mask;
    *chepr_ptr = chepr;
}

volatile void *usb_endpoint_get_iso_buffer(const struct usb_device_endpoint_t *endpoint,
    bool is_tx)
{
    auto const chepr = usb->chepr[endpoint->index];

    // Select DTGOTX or DTGORX
    const size_t shift = (is_tx ? 6 : 14);
    if (chepr & (0x1u << shift))
        return endpoint->layout.dbl_buf.packet0;
    return endpoint->layout.dbl_buf.packet1;
}

void usb_get_interrupt_info(struct usb_interrupt_info_t *info)
{
    constexpr uint32_t interrupt_flags_mask = 0x6003ff80u;

    auto const istr = usb->istr;
    info->pending_interrupts = (istr & interrupt_flags_mask);
    info->endpoint = (istr & 0xfu);
}
