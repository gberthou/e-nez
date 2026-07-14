#include <stdint.h>

#include "rcc.h"

struct __attribute__((packed)) rcc_t
{
    uint32_t cr;
    uint32_t icscr;
    uint32_t cfgr;
    uint32_t _reserved0[2];
    uint32_t crrcr;
    uint32_t cier;
    uint32_t cifr;
    uint32_t cicr;
    uint32_t ioprstr;
    uint32_t ahbrstr;
    uint32_t apbrstr1;
    uint32_t apbrstr2;
    uint32_t iopenr;
    uint32_t ahbenr;
    uint32_t apbenr1;
    uint32_t apbenr2;
    uint32_t iopsmenr;
    uint32_t ahbsmenr;
    uint32_t apbsmenr1;
    uint32_t apbsmenr2;
    uint32_t ccipr;
    uint32_t ccipr2;
    uint32_t csr1;
    uint32_t csr2;
};

static volatile struct rcc_t * const rcc = (volatile struct rcc_t*) 0x40021000;

void rcc_select_sysclk_source()
{
    constexpr uint32_t sw_shift = 0;
    auto cfgr = rcc->cfgr;
    cfgr &= ~(0x7u << sw_shift);
    cfgr |= (0x2u << sw_shift); // HSIUSB48
    rcc->cfgr = cfgr;
}

void rcc_set_clock(enum rcc_clock_e clock, bool activate)
{
    // TODO: Other buses than APB1
    const uint32_t mask = (0x1u << clock);
    if (activate)
        rcc->apbenr1 |= mask;
    else
        rcc->apbenr1 &= ~mask;
}

void rcc_io_set(enum rcc_io_e io, bool activate)
{
    auto const mask = 0x1u << io;
    if (activate)
        rcc->iopenr |= mask;
    else
        rcc->iopenr &= ~mask;
}


void rcc_hsiusb_set(bool activate)
{
    constexpr auto mask = 0x1u << 22;
    if (activate)
        rcc->cr |= mask;
    else
        rcc->cr &= ~mask;
}

void rcc_hsiusb_wait(void)
{
    // TODO: Evaluate the need for a timeout?

    constexpr auto mask = 0x1u << 23;
    while (!(rcc->cr & mask));
}
