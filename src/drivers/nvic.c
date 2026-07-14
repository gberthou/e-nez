#include <stdint.h>

#include "nvic.h"

static volatile uint32_t * const iser = (volatile uint32_t*) 0xe000e100;
static volatile uint32_t * const icer = (volatile uint32_t*) 0xe000e180;
static volatile uint32_t * const icpr = (volatile uint32_t*) 0xe000e280;

void nvic_setup(unsigned index, bool activate)
{
    const uint32_t mask = (0x1u << (index & 0x1f));

    if (activate)
        *iser = mask;
    else
        *icer = mask;
}

void nvic_ack(unsigned index)
{
    *icpr = (0x1u << (index & 0x1f));
}

void nvic_clear_all_pending(void)
{
    *icpr = 0xffffffffu;
}
