#include <stdint.h>

#include "usart.h"

struct __attribute__((packed)) usart_t
{
    uint32_t cr1;
    uint32_t cr2;
    uint32_t cr3;
    uint32_t brr;
    uint32_t gtpr;
    uint32_t rtor;
    uint32_t rqr;
    uint32_t isr;
    uint32_t icr;
    uint32_t rdr;
    uint32_t tdr;
    uint32_t presc;
};

static volatile struct usart_t * const usart1 = (volatile struct usart_t*) 0x40013800;
static volatile struct usart_t * const usart2 = (volatile struct usart_t*) 0x40004400;

constexpr uint32_t cr1_ue = 0x1u;

constexpr uint32_t isr_txfnf = (0x1u << 7);

static inline volatile struct usart_t * get_usart(unsigned index)
{
    // 1 -> usart1
    // 2 -> usart2
    return (index & 0x1u) ? usart1 : usart2;
}

void usart_config_uart(unsigned index, unsigned baudrate)
{
    constexpr uint32_t source_freq = 48000000u;
    /* TODO: Don't assume f_PCLK = 48MHz.
     * In practice, if f_HSIUSB = 48MHz, RCC_CR.SYSDIV=0, RCC_CFGR.HPRE=0 and
     * RCC_CFGR.PPRE=0 (reset values), then f_PCLK = 48MHz
     */
    const uint32_t ideal_div = source_freq / baudrate;
    uint32_t brr = 0u;
    if (ideal_div <= 0xffffu)
        brr = ideal_div;
    else
    {
        // TODO: In practice, no prescaler is needed if baudrate > baud Hz with a source
        // clock of 48MHz
    }

    auto const usart = get_usart(index);
    // BRR can be programmed only when CR1.UE=0
    usart->cr1 &= ~cr1_ue;
    usart->brr = brr;
    /* Prescaler: input clock divided by 1 if PRESCALER == 0
     * otherwise, divided by (2 * PRESCALER) if PRESCALER <= 6
     * otherwise, divided by (1 << (PRESCALER - 3)) if PRESCALER <= 11
     * PRESCALER may not be > 11
     */
    usart->presc = 0u; // TODO: other values, but no prescaler is needed
    usart->cr1 = (0x1u << 2) // RE
        | (0x1u << 3) // TE
        | (0x1u << 29) // FIFOEN
        ;
    usart->cr2 = 0x0u;
    usart->cr1 |= cr1_ue;
}

void usart_uart_putc(unsigned index, char c)
{
    auto const usart = get_usart(index);
    // Wait until TX fifo is non-full
    while (!(usart->isr & isr_txfnf));
    usart->tdr = c;
}
